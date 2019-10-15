// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void spinlockInitialise(Spinlock *lk, char *name)
{
	lk->Name = name;
	lk->Locked = 0;
	lk->Cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// holding a lock for a long time may cause
// other CPUs to waste time spinning to spinlockAcquire it.

void spinlockAcquire(Spinlock *lk)
{
	pushCli(); // disable interrupts to avoid deadlock.
	if (isHolding(lk))
	{
		panic("spinlockAcquire");
	}
	// The xchg is atomic.
	while (atomicExchange(&lk->Locked, 1) != 0)
		;

	// Tell the C compiler and the processor to not move loads or stores
	// past this point, to ensure that the critical section's memory
	// references happen after the lock is acquired.
	__sync_synchronize();

	// Record info about lock acquisition for debugging.
	lk->Cpu = myCpu();
	getProcessCallStack(&lk, lk->Pcs);
}

// Release the lock.
void spinlockRelease(Spinlock *lk)
{
	if (!isHolding(lk))
	{
		panic("spinlockRelease");
	}
	lk->Pcs[0] = 0;
	lk->Cpu = 0;

	// Tell the C compiler and the processor to not move loads or stores
	// past this point, to ensure that all the stores in the critical
	// section are visible to other cores before the lock is released.
	// Both the C compiler and the hardware may re-order loads and
	// stores; __sync_synchronize() tells them both not to.
	__sync_synchronize();

	// spinlockRelease the lock, equivalent to lk->Locked = 0.
	// This code can't use a C assignment, since it might
	// not be atomic. A real OS would use C atomics here.
	asm volatile("movl $0, %0" : "+m" (lk->Locked) : );

	popCli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void getProcessCallStack(void *v, uint32_t pcs[])
{
	uint32_t *ebp;
	int i;

	ebp = (uint32_t*)v - 2;
	for (i = 0; i < 10; i++) 
	{
		if (ebp == 0 || ebp < (uint32_t*)KERNBASE || ebp == (uint32_t*)0xffffffff)
		{
			break;
		}
		pcs[i] = ebp[1];     // saved %eip
		ebp = (uint32_t*)ebp[0]; // saved %ebp
	}
	for (; i < 10; i++)
	{
		pcs[i] = 0;
	}
}

// Check whether this cpu is holding the lock.
int isHolding(Spinlock *lock)
{
	return lock->Locked && lock->Cpu == myCpu();
}


// pushCli/popCli are like cli/sti except that they are matched:
// it takes two popCli to undo two pushCli.  Also, if interrupts
// are off, then pushCli, popCli leaves them off.

void pushCli(void)
{
	int eflags;

	eflags = readExtendedFlags();
	disableInterrupts();
	if (myCpu()->CliDepth == 0)
	{
		myCpu()->InterruptsEnabled = eflags & FL_IF;
	}
	myCpu()->CliDepth += 1;
}

void popCli(void)
{
	if (readExtendedFlags()&FL_IF)
	{
		panic("popCli - interruptible");
	}
	if (--myCpu()->CliDepth < 0)
	{
		panic("popCli");
	}
	if (myCpu()->CliDepth == 0 && myCpu()->InterruptsEnabled)
	{
		enableInterrupts();
	}
}

