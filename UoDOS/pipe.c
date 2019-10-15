#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

#define PIPESIZE 512

struct _Pipe 
{
	Spinlock	Lock;
	char		Data[PIPESIZE];
	uint32_t	ReadCount;     // number of bytes read
	uint32_t	WriteCount;    // number of bytes written
	int			ReadOpen;   // read fd is still open
	int			WriteOpen;  // write fd is still open
};

void freepiperesources(Pipe *p, File **f0, File **f1)
{
	if (p)
	{
		freePhysicalMemoryPage((char*)p);
	}
	if (*f0)
	{
		fileClose(*f0);
	}
	if (*f1)
	{
		fileClose(*f1);
	}
}

int pipealloc(File **f0, File **f1)
{
	Pipe *p;

	p = 0;
	*f0 = *f1 = 0;
	if ((*f0 = allocateFileStructure()) == 0 || (*f1 = allocateFileStructure()) == 0)
	{
		freepiperesources(p, f0, f1);
		return -1;
	}
	if ((p = (Pipe*)allocatePhysicalMemoryPage()) == 0)
	{
		freepiperesources(p, f0, f1);
		return -1;
	}
	p->ReadOpen = 1;
	p->WriteOpen = 1;
	p->WriteCount = 0;
	p->ReadCount = 0;
	spinlockInitialise(&p->Lock, "Pipe");
	(*f0)->Type = FD_PIPE;
	(*f0)->Readable = 1;
	(*f0)->Writable = 0;
	(*f0)->Pipe = p;
	(*f1)->Type = FD_PIPE;
	(*f1)->Readable = 0;
	(*f1)->Writable = 1;
	(*f1)->Pipe = p;
	return 0;
}

void pipeclose(Pipe *p, int writable)
{
	spinlockAcquire(&p->Lock);
	if (writable) 
	{
		p->WriteOpen = 0;
		wakeup(&p->ReadCount);
	}
	else 
	{
		p->ReadOpen = 0;
		wakeup(&p->WriteCount);
	}
	if (p->ReadOpen == 0 && p->WriteOpen == 0) 
	{
		spinlockRelease(&p->Lock);
		freePhysicalMemoryPage((char*)p);
	}
	else
	{
		spinlockRelease(&p->Lock);
	}
}

int pipewrite(Pipe *p, char *addr, int n)
{
	int i;

	spinlockAcquire(&p->Lock);
	for (i = 0; i < n; i++) 
	{
		while (p->WriteCount == p->ReadCount + PIPESIZE) 
		{ 
			if (p->ReadOpen == 0 || myProcess()->IsKilled) 
			{
				spinlockRelease(&p->Lock);
				return -1;
			}
			wakeup(&p->ReadCount);
			sleep(&p->WriteCount, &p->Lock);  //DOC: pipewrite-sleep
		}
		p->Data[p->WriteCount++ % PIPESIZE] = addr[i];
	}
	wakeup(&p->ReadCount); 
	spinlockRelease(&p->Lock);
	return n;
}

int piperead(Pipe *p, char *addr, int n)
{
	int i;

	spinlockAcquire(&p->Lock);
	while (p->ReadCount == p->WriteCount && p->WriteOpen) 
	{ 
		if (myProcess()->IsKilled) 
		{
			spinlockRelease(&p->Lock);
			return -1;
		}
		sleep(&p->ReadCount, &p->Lock); 
	}
	for (i = 0; i < n; i++) 
	{  
		if (p->ReadCount == p->WriteCount)
		{
			break;
		}
		addr[i] = p->Data[p->ReadCount++ % PIPESIZE];
	}
	wakeup(&p->WriteCount); 
	spinlockRelease(&p->Lock);
	return i;
}
