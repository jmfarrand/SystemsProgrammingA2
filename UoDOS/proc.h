// Per-CPU state
struct _Cpu
{
  uint8_t				Apicid;             // Local APIC ID
  Context *				Scheduler;			// swtch() here to enter Scheduler
  struct Taskstate		Taskstate;			// Used by x86 to find stack for interrupt
  struct Segdesc		Gdt[NSEGS];			// x86 global descriptor table
  volatile uint32_t		Started;			// Has the CPU Started?
  int					CliDepth;           // Depth of pushCli nesting.
  int					InterruptsEnabled;  // Were interrupts enabled before pushCli?
  Process *				Process;			// The process running on this cpu or null
};

extern Cpu cpus[NCPU];
extern int ncpu;

// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocateProcess() manipulates it.

struct _Context 
{
	uint32_t edi;
	uint32_t esi;
	uint32_t ebx;
	uint32_t ebp;
	uint32_t eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct _Process 
{
	uint32_t			MemorySize;         // Size of process memory (bytes)
	pde_t*				PageTable;          // Page table
	char *				KernelStack;        // Bottom of kernel stack for this process
	enum procstate		State;				// Process state
	int					ProcessId;          // Process ID
	Process *			Parent;				// Parent process
	struct Trapframe *	Trapframe;			// Trap frame for current syscall
	Context *			Context;			// swtch() here to run process
	void *				Chan;               // If non-zero, sleeping on chan
	int					IsKilled;           // If non-zero, have been killed
	File *				OpenFile[NOFILE];	// Open files
	char				Cwd[MAXCWDSIZE];	// Current directory
	char				Name[16];		    // Process name (debugging)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
