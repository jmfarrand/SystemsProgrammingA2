// Sleeping locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

void sleeplockInitialise(Sleeplock *lk, char *name)
{
	spinlockInitialise(&lk->Spinlock, "sleep lock");
	lk->Name = name;
	lk->Locked = 0;
	lk->Pid = 0;
}

void sleeplockAcquire(Sleeplock *lk)
{
	spinlockAcquire(&lk->Spinlock);
	while (lk->Locked) 
	{
		sleep(lk, &lk->Spinlock);
	}
	lk->Locked = 1;
	lk->Pid = myProcess()->ProcessId;
	spinlockRelease(&lk->Spinlock);
}

void sleeplockRelease(Sleeplock *lk)
{
	spinlockAcquire(&lk->Spinlock);
	lk->Locked = 0;
	lk->Pid = 0;
	wakeup(lk);
	spinlockRelease(&lk->Spinlock);
}

int isHoldingSleeplock(Sleeplock *lk)
{
	int r;

	spinlockAcquire(&lk->Spinlock);
	r = lk->Locked;
	spinlockRelease(&lk->Spinlock);
	return r;
}



