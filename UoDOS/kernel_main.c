#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

// This is a dummy __main.  For some reason, gcc puts in a call to 
// __main from main, so we just include a dummy.
 
void __main() {}

static void mpmain(void)  __attribute__((noreturn));

// kernelEnd is defined in the linker script file (kernel.ld).  It is the 
// first address immediately after the memory occupied by the kernel executable

extern char * kernelEnd; 

void main() 
{
	initialiseLowerkernelMemory((char *)&kernelEnd, P2V(4 * 1024 * 1024));	// phys page allocator
	allocateKernelVirtualMemory();						// kernel page table
	mpinit();											// detect other processors
	localApicInitialise();								// interrupt controller
	initialiseGDT();									// segment descriptors
	picInitialise();									// disable pic
	ioApicInitialise();									// another interrupt controller
	consoleInitialise();								// console hardware
	//uartinit();										// serial port
	processTableInitialise();							// process table
	trapVectorsInitialise();							// trap vectors
	diskBufferCacheInitialise();						// buffer cache
	filesInitialise();									// file table
	ideInitialise();									// disk 
	initialiseRestOfkernelMemory(P2V(4 * 1024 * 1024), P2V(PHYSTOP));			// must come after startothers()
	initialiseFirstUserProcess();						// first user process
	mpmain();											// finish this processor's setup
}

// Common CPU setup code.
static void mpmain(void)
{
	cprintf("cpu%d: starting %d\n", cpuId(), cpuId());
	interruptDescriptorTableInitialise();       
	atomicExchange(&(myCpu()->Started), 1); // tell startothers() we're up
	scheduler();    
}