#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "spinlock.h"

struct 
{
	Spinlock		Lock;
	Process			Process[NPROC];
} processTable;

static Process *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void processTableInitialise(void)
{
	spinlockInitialise(&processTable.Lock, "processTable");
}

// Must be called with interrupts disabled
int cpuId() 
{
	return myCpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading localApicId and running through the loop.

Cpu* myCpu(void)
{
	int apicid, i;

	if (readExtendedFlags() & FL_IF)
	{
		panic("myCpu called with interrupts enabled\n");
	}
	apicid = localApicId();
	// APIC IDs are not guaranteed to be contiguous. Maybe we should have
	// a reverse map, or reserve a register to store &cpus[i].
	for (i = 0; i < ncpu; ++i) 
	{
		if (cpus[i].Apicid == apicid)
		{
			return &cpus[i];
		}
	}
	panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading Process from the cpu structure

Process* myProcess(void) 
{
	Cpu *c;
	Process *p;

	pushCli();
	c = myCpu();
	p = c->Process;
	popCli();
	return p;
}

// Look in the process table for an UNUSED Process.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.

static Process* allocateProcess(void)
{
	Process *p;
	char *sp;
	int found = 0;

	spinlockAcquire(&processTable.Lock);
	p = processTable.Process;
	while (p < &processTable.Process[NPROC] && !found)
	{
		if (p->State == UNUSED)
		{
			found = 1;
		}
		else
		{
			p++;
		}
	}
	if (!found)
	{
		spinlockRelease(&processTable.Lock);
		return 0;
	}
	p->State = EMBRYO;
	p->ProcessId = nextpid++;

	spinlockRelease(&processTable.Lock);

	// Allocate kernel stack.
	if ((p->KernelStack = allocatePhysicalMemoryPage()) == 0) 
	{
		p->State = UNUSED;
		return 0;
	}
	sp = p->KernelStack + KSTACKSIZE;

	// Leave room for trap frame.
	sp -= sizeof *p->Trapframe;
	p->Trapframe = (struct Trapframe*)sp;

	// Set up new context to start executing at forkret,
	// which returns to trapret.
	sp -= 4;
	*(uint32_t*)sp = (uint32_t)trapret;

	sp -= sizeof *p->Context;
	p->Context = (Context*)sp;
	memset(p->Context, 0, sizeof *p->Context);
	p->Context->eip = (uint32_t)forkret;

	// Allocate the stdin, stdout and stderr devices

	File * consoleDevice = allocateFileStructure();
	consoleDevice->Type = FD_DEVICE;
	consoleDevice->DeviceID = CONSOLE;
	consoleDevice->Readable = 1;
	consoleDevice->Writable = 1;
	p->OpenFile[0] = consoleDevice;
	p->OpenFile[1] = fileDup(consoleDevice);
	p->OpenFile[2] = fileDup(consoleDevice);
 	return p;
}

// Set up first user process.

void initialiseFirstUserProcess(void)
{
	Process *p;
	extern char initcode_start[], initcode_end[];

	p = allocateProcess();

	initproc = p;
	if ((p->PageTable = setupKernelVirtualMemory()) == 0)
	{
		panic("initialiseFirstUserProcess: out of memory?");
	}
	initialiseUserVirtualMemory(p->PageTable, initcode_start, (int)(initcode_end - initcode_start));
	p->MemorySize = PGSIZE;
	memset(p->Trapframe, 0, sizeof(*p->Trapframe));
	p->Trapframe->cs = (SEG_UCODE << 3) | DPL_USER;
	p->Trapframe->ds = (SEG_UDATA << 3) | DPL_USER;
	p->Trapframe->es = p->Trapframe->ds;
	p->Trapframe->ss = p->Trapframe->ds;
	p->Trapframe->eflags = FL_IF;
	p->Trapframe->esp = PGSIZE;
	p->Trapframe->eip = 0;  // beginning of initcode.asm

	safestrcpy(p->Name, "initcode", sizeof(p->Name));
	safestrcpy(p->Cwd, "/", MAXCWDSIZE);

	// this assignment to p->state lets other cores
	// run this process. the spinlockAcquire forces the above
	// writes to be visible, and the Lock is also needed
	// because the assignment might not be atomic.
	spinlockAcquire(&processTable.Lock);

	p->State = RUNNABLE;

	spinlockRelease(&processTable.Lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.

int growProcess(int n)
{
	uint32_t memorySize;
	Process *curproc = myProcess();

	memorySize = curproc->MemorySize;
	if (n > 0) 
	{
		if ((memorySize = allocateMemoryAndPageTables(curproc->PageTable, memorySize, memorySize + n)) == 0)
		{
			return -1;
		}
	}
	else if (n < 0) 
	{
		if ((memorySize = releaseUserPages(curproc->PageTable, memorySize, memorySize + n)) == 0)
		{
			return -1;
		}
	}
	curproc->MemorySize = memorySize;
	switchToUserVirtualMemory(curproc);
	return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned Process to RUNNABLE.

int fork(void)
{
	int i, pid;
	Process *np;
	Process *curproc = myProcess();

	// Allocate process.
	if ((np = allocateProcess()) == 0) 
	{
		return -1;
	}

	// Copy process state from Process.
	if ((np->PageTable = copyProcessPageTable(curproc->PageTable, curproc->MemorySize)) == 0) 
	{
		freePhysicalMemoryPage(np->KernelStack);
		np->KernelStack = 0;
		np->State = UNUSED;
		return -1;
	}
	np->MemorySize = curproc->MemorySize;
	np->Parent = curproc;
	*np->Trapframe = *curproc->Trapframe;

	// Clear %eax so that fork returns 0 in the child.
	np->Trapframe->eax = 0;

	for (i = 0; i < NOFILE; i++)
	{
		if (curproc->OpenFile[i])
		{
			np->OpenFile[i] = fileDup(curproc->OpenFile[i]);
		}
	}
	safestrcpy(np->Cwd, curproc->Cwd, MAXCWDSIZE);
	safestrcpy(np->Name, curproc->Name, sizeof(curproc->Name));
	pid = np->ProcessId;
	spinlockAcquire(&processTable.Lock);
	np->State = RUNNABLE;
	spinlockRelease(&processTable.Lock);

	return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.

void exit(void)
{
	Process *curproc = myProcess();
	Process *p;
	int fd;

	if (curproc == initproc)
	{
		panic("init exiting");
	}
	// Close all open files.
	for (fd = 0; fd < NOFILE; fd++) 
	{
		if (curproc->OpenFile[fd]) 
		{
			fileClose(curproc->OpenFile[fd]);
			curproc->OpenFile[fd] = 0;
		}
	}

	safestrcpy(curproc->Cwd, "", MAXCWDSIZE);

	spinlockAcquire(&processTable.Lock);

	// Parent might be sleeping in wait().
	wakeup1(curproc->Parent);

	// Pass abandoned children to init.
	for (p = processTable.Process; p < &processTable.Process[NPROC]; p++) 
	{
		if (p->Parent == curproc) 
		{
			p->Parent = initproc;
			if (p->State == ZOMBIE)
			{
				wakeup1(initproc);
			}
		}
	}

	// Jump into the Scheduler, never to return.
	curproc->State = ZOMBIE;
	sched();
	panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.

int wait(void)
{
	Process *p;
	int havekids, pid;
	Process *curproc = myProcess();

	spinlockAcquire(&processTable.Lock);
	for (;;) 
	{
		// Scan through table looking for exited children.
		havekids = 0;
		for (p = processTable.Process; p < &processTable.Process[NPROC]; p++) 
		{
			if (p->Parent != curproc)
			{
				continue;
			}
			havekids = 1;
			if (p->State == ZOMBIE) 
			{
				// Found one.
				pid = p->ProcessId;
				freePhysicalMemoryPage(p->KernelStack);
				p->KernelStack = 0;
				freeMemoryAndPageTable(p->PageTable);
				p->ProcessId = 0;
				p->Parent = 0;
				p->Name[0] = 0;
				p->IsKilled = 0;
				p->State = UNUSED;
				spinlockRelease(&processTable.Lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if (!havekids || curproc->IsKilled) 
		{
			spinlockRelease(&processTable.Lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(curproc, &processTable.Lock);  //DOC: wait-sleep
	}
}

// Per-CPU process Scheduler.
// Each CPU calls Scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the Scheduler.

void scheduler(void)
{
	Process *p;
	Cpu *c = myCpu();
	c->Process = 0;
	for (;;) 
	{
		// Enable interrupts on this processor.
		enableInterrupts();

		// Loop over process table looking for process to run.
		spinlockAcquire(&processTable.Lock);

		for (p = processTable.Process; p < &processTable.Process[NPROC]; p++) 
		{
			if (p->State != RUNNABLE)
			{
				continue;
			}
			// Switch to chosen process.  It is the process's job
			// to spinlockRelease processTable.Lock and then reacquire it
			// before jumping back to us.
			c->Process = p;
			switchToUserVirtualMemory(p);
			p->State = RUNNING;

			swtch(&(c->Scheduler), p->Context);
			switchToKernelVirtualMemory();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->Process = 0;
		}
		spinlockRelease(&processTable.Lock);
	}
}

// Enter Scheduler.  Must hold only processTable.Lock
// and have changed Process->state. Saves and restores
// InterruptsEnabled because InterruptsEnabled is a property of this
// kernel thread, not this CPU. It should
// be Process->InterruptsEnabled and Process->CliDepth, but that would
// break in the few places where a Lock is held but
// there's no process.

void sched(void)
{
	int intena;
	Process *p = myProcess();

	if (!isHolding(&processTable.Lock))
	{
		panic("sched processTable.Lock");
	}
	if (myCpu()->CliDepth != 1)
	{
		panic("sched locks");
	}
	if (p->State == RUNNING)
	{
		panic("sched running");
	}
	if (readExtendedFlags()&FL_IF)
	{
		panic("sched interruptible");
	}
	intena = myCpu()->InterruptsEnabled;
	swtch(&p->Context, myCpu()->Scheduler);
	myCpu()->InterruptsEnabled = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
	spinlockAcquire(&processTable.Lock);
	myProcess()->State = RUNNABLE;
	sched();
	spinlockRelease(&processTable.Lock);
}

// A fork child's very first scheduling by Scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
	static int first = 1;
	// Still holding processTable.Lock from Scheduler.
	spinlockRelease(&processTable.Lock);

	if (first) 
	{
		// Some initialization functions must be run in the context
		// of a regular process (e.g., they call sleep), and thus cannot
		// be run from main().
		fsFat12Initialise();
		first = 0;
	}

	// Return to "caller", actually trapret (see allocateProcess).
}

// Atomically spinlockRelease Lock and sleep on chan.
// Reacquires Lock when awakened.
void sleep(void *chan, Spinlock *lk)
{
	Process *p = myProcess();

	if (p == 0)
	{
		panic("sleep");
	}
	if (lk == 0)
	{
		panic("sleep without lk");
	}
	// Must spinlockAcquire processTable.Lock in order to
	// change p->state and then call sched.
	// Once we hold processTable.Lock, we can be
	// guaranteed that we won't miss any wakeup
	// (wakeup runs with processTable.Lock locked),
	// so it's okay to spinlockRelease lk.
	if (lk != &processTable.Lock) 
	{
		spinlockAcquire(&processTable.Lock);  
		spinlockRelease(lk);
	}
	// Go to sleep.
	p->Chan = chan;
	p->State = SLEEPING;

	sched();

	// Tidy up.
	p->Chan = 0;

	// Reacquire original Lock.
	if (lk != &processTable.Lock) 
	{
		spinlockRelease(&processTable.Lock);
		spinlockAcquire(lk);
	}
}

// Wake up all processes sleeping on chan.
// The processTable Lock must be held.

static void wakeup1(void *chan)
{
	Process *p;

	for (p = processTable.Process; p < &processTable.Process[NPROC]; p++)
	{
		if (p->State == SLEEPING && p->Chan == chan)
		{
			p->State = RUNNABLE;
		}
	}
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
	spinlockAcquire(&processTable.Lock);
	wakeup1(chan);
	spinlockRelease(&processTable.Lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).

int kill(int pid)
{
	Process *p;

	spinlockAcquire(&processTable.Lock);
	for (p = processTable.Process; p < &processTable.Process[NPROC]; p++) 
	{
		if (p->ProcessId == pid) 
		{
			p->IsKilled = 1;
			// Wake process from sleep if necessary.
			if (p->State == SLEEPING)
			{
				p->State = RUNNABLE;
			}
			spinlockRelease(&processTable.Lock);
			return 0;
		}
	}
	spinlockRelease(&processTable.Lock);
	return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No Lock to avoid wedging a stuck machine further.

void processDump(void)
{
	static char *states[] = 
	{
		[UNUSED]    "unused",
		[EMBRYO]    "embryo",
		[SLEEPING]  "sleep ",
		[RUNNABLE]  "Runnable",
		[RUNNING]   "Running",
		[ZOMBIE]    "zombie"
	};
	int i;
	Process *p;
	char *state;
	uint32_t pc[10];

	cprintf("\n");
	for (p = processTable.Process; p < &processTable.Process[NPROC]; p++) 
	{
		if (p->State == UNUSED)
		{
			continue;
		}
		if (p->State >= 0 && p->State < NELEM(states) && states[p->State])
		{
			state = states[p->State];
		}
		else
		{
			state = "???";
		}
		cprintf("%d %s %s", p->ProcessId, state, p->Name);
		if (p->State == SLEEPING) 
		{
			getProcessCallStack((uint32_t*)p->Context->ebp + 2, pc);
			for (i = 0; i < 10 && pc[i] != 0; i++)
			{
				cprintf(" %p", pc[i]);
			}
		}
		cprintf("\n");
	}
}
