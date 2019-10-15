#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint32_t vectors[];  // in vectors.S: array of 256 entry pointers
Spinlock tickslock;
uint32_t ticks;

void trapVectorsInitialise(void)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
	}
	SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);
	spinlockInitialise(&tickslock, "time");
}

void interruptDescriptorTableInitialise(void)
{
	loadInterruptDescriptorTable(idt, sizeof(idt));
}

void trap(struct Trapframe *tf)
{
	if (tf->trapno == T_SYSCALL) 
	{
		if (myProcess()->IsKilled)
		{
			exit();
		}
		myProcess()->Trapframe = tf;
		syscall();
		if (myProcess()->IsKilled)
		{
			exit();
		}
		return;
	}

	switch (tf->trapno) 
	{
		case T_IRQ0 + IRQ_TIMER:
			if (myCpu() == 0) 
			{
				spinlockAcquire(&tickslock);
				ticks++;
				wakeup(&ticks);
				spinlockRelease(&tickslock);
			}
			localApicEndOfInterrupt();
			break;

		case T_IRQ0 + IRQ_IDE:
			ideInterruptHandler();
			localApicEndOfInterrupt();
			break;

		case T_IRQ0 + IRQ_IDE + 1:
			// Bochs generates spurious IDE1 interrupts.
			break;

		case T_IRQ0 + IRQ_KBD:
			keyboardInterrupt();
			localApicEndOfInterrupt();
			break;

		case T_IRQ0 + IRQ_COM1:
			uartintr();
			localApicEndOfInterrupt();
			break;

		case T_IRQ0 + 7:
		case T_IRQ0 + IRQ_SPURIOUS:
			cprintf("cpu%d: spurious interrupt at %x:%x\n",	cpuId(), tf->cs, tf->eip);
			localApicEndOfInterrupt();
			break;

		default:
			if (myProcess() == 0 || (tf->cs & 3) == 0) 
			{
				// In kernel, it must be our mistake.
				cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n", tf->trapno, cpuId(), tf->eip, readControlRegister2());
				panic("trap");
			}
			// In user space, assume process misbehaved.
			cprintf("pid %d %s: trap %d err %d on cpu %d eip 0x%x addr 0x%x--kill proc\n",
					myProcess()->ProcessId, myProcess()->Name, tf->trapno,
					tf->err, cpuId(), tf->eip, readControlRegister2());
			myProcess()->IsKilled = 1;
	}

	// Force process exit if it has been killed and is in user space.
	// (If it is still executing in the kernel, let it keep running
	// until it gets to the regular system call return.)
	if (myProcess() && myProcess()->IsKilled && (tf->cs & 3) == DPL_USER)
	{
		exit();
	}

	// Force process to give up CPU on clock tick.
	// If interrupts were on while locks held, would need to check nlock.
	if (myProcess() && myProcess()->State == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER)
	{
		yield();
	}
	// Check if the process has been killed since we yielded
	if (myProcess() && myProcess()->IsKilled && (tf->cs & 3) == DPL_USER)
	{
		exit();
	}
}
