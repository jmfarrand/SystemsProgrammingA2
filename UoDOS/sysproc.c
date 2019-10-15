#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sys_fork(void)
{
	return fork();
}

int sys_exit(void)
{
	exit();
	return 0;  // not reached
}

int
sys_wait(void)
{
	return wait();
}

int sys_kill(void)
{
	int pid;

	if (argint(0, &pid) < 0)
	{
		return -1;
	}
	return kill(pid);
}

int sys_getpid(void)
{
	return myProcess()->ProcessId;
}

int sys_sbrk(void)
{
	int addr;
	int n;

	if (argint(0, &n) < 0)
	{
		return -1;
	}
	addr = myProcess()->MemorySize;
	if (growProcess(n) < 0)
	{
		return -1;
	}
	return addr;
}

int sys_sleep(void)
{
	int n;
	uint32_t ticks0;

	if (argint(0, &n) < 0)
	{
		return -1;
	}
	spinlockAcquire(&tickslock);
	ticks0 = ticks;
	while (ticks - ticks0 < n) 
	{
		if (myProcess()->IsKilled) 
		{
			spinlockRelease(&tickslock);
			return -1;
		}
		sleep(&ticks, &tickslock);
	}
	spinlockRelease(&tickslock);
	return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
	uint32_t xticks;

	spinlockAcquire(&tickslock);
	xticks = ticks;
	spinlockRelease(&tickslock);
	return xticks;
}
