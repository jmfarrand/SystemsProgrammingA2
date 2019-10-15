// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freeMemoryRange(void *vstart, void *vend);
extern char kernelEnd[]; // first address after kernel loaded from ELF file
						 // defined by the kernel linker script in kernel.ld

struct MemoryPage 
{
	struct MemoryPage *		Next;
};

struct 
{
	Spinlock				Lock;
	int						UseLock;
	struct MemoryPage *		FreeList;
} kernelMemory;

// Initialization happens in two phases.
// 1. main() calls initialiseLowerkernelMemory() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls initialiseRestOfkernelMemory() with the rest of the physical pages
// after installing a full page table that maps them on all cores.

void initialiseLowerkernelMemory(void *vstart, void *vend)
{
	spinlockInitialise(&kernelMemory.Lock, "kernelMemory");
	kernelMemory.UseLock = 0;
	freeMemoryRange(vstart, vend);
}

void initialiseRestOfkernelMemory(void *vstart, void *vend)
{
	freeMemoryRange(vstart, vend);
	kernelMemory.UseLock = 1;
}

void freeMemoryRange(void *vstart, void *vend)
{
	char *p;
	p = (char*)PGROUNDUP((uint32_t)vstart);
	for (; p + PGSIZE <= (char*)vend; p += PGSIZE)
	{
		freePhysicalMemoryPage(p);
	}
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to allocatePhysicalMemoryPage().  (The exception is when
// initializing the allocator; see kinit above.)

void freePhysicalMemoryPage(char *v)
{
	struct MemoryPage *r;

	if ((uint32_t)v % PGSIZE || v < (char *)&kernelEnd || V2P(v) >= PHYSTOP)
	{
		panic("freePhysicalMemoryPage");
	}
	// Fill with junk to catch dangling refs.
	memset(v, 1, PGSIZE);

	if (kernelMemory.UseLock)
	{
		spinlockAcquire(&kernelMemory.Lock);
	}
	r = (struct MemoryPage*)v;
	r->Next = kernelMemory.FreeList;
	kernelMemory.FreeList = r;
	if (kernelMemory.UseLock)
	{
		spinlockRelease(&kernelMemory.Lock);
	}
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

char* allocatePhysicalMemoryPage(void)
{
	struct MemoryPage *r;

	if (kernelMemory.UseLock)
	{
		spinlockAcquire(&kernelMemory.Lock);
	}
	r = kernelMemory.FreeList;
	if (r)
	{
		kernelMemory.FreeList = r->Next;
	}
	if (kernelMemory.UseLock)
	{
		spinlockRelease(&kernelMemory.Lock);
	}
	return (char*)r;
}

