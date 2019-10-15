#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"
#include "syscalltable.h"

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.

int fetchint(uint32_t addr, int *ip)
{
	Process *curproc = myProcess();

	if (addr >= curproc->MemorySize || addr + 4 > curproc->MemorySize)
	{
		return -1;
	}
	*ip = *(int*)(addr);
	return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.

int fetchstr(uint32_t addr, char **pp)
{
	char *s, *ep;
	Process *curproc = myProcess();

	if (addr >= curproc->MemorySize)
	{
		return -1;
	}
	*pp = (char*)addr;
	ep = (char*)curproc->MemorySize;
	for (s = *pp; s < ep; s++) 
	{
		if (*s == 0)
		{
			return s - *pp;
		}
	}
	return -1;
}

// Fetch the nth parameter to the system call as an int

int argint(int n, int *ip)
{
	return fetchint((myProcess()->Trapframe->esp) + 4 + 4 * n, ip);
}

// Fetch the nth parameter to the system call as a pointer
// to a block of memory of size bytes.  Check that the pointer
// lies within the process address space.

int argptr(int n, char **pp, int size)
{
	int i;
	Process *curproc = myProcess();

	if (argint(n, &i) < 0)
	{
		return -1;
	}
	if (size < 0 || (uint32_t)i >= curproc->MemorySize || (uint32_t)i + size > curproc->MemorySize)
	{
		return -1;
	}
	*pp = (char*)i;
	return 0;
}

// Fetch the nth parameter to the system call as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)

int argstr(int n, char **pp)
{
	int addr;
	if (argint(n, &addr) < 0)
	{
		return -1;
	}
	return fetchstr(addr, pp);
}

// Execute the system call whose index is specified in AX.

void syscall(void)
{
	int num;
	Process *curproc = myProcess();

	num = curproc->Trapframe->eax;
	if (num > 0 && num < NELEM(syscalls) && syscalls[num]) 
	{
		curproc->Trapframe->eax = syscalls[num]();
	}
	else 
	{
		cprintf("%d %s: unknown sys call %d\n",
		curproc->ProcessId, curproc->Name, num);
		curproc->Trapframe->eax = -1;
	}
}
