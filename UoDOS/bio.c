// Buffer cache.
//
// The buffer cache is a linked list of DiskBuffer structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call diskBufferRead.
// * After changing buffer data, call diskBufferWrite to write it to disk.
// * When done with the buffer, call diskBufferRelease.
// * Do not use the buffer after calling diskBufferRelease.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//
// The implementation uses two state flags internally:
// * B_VALID: the buffer data has been read from the disk.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

struct 
{
	Spinlock		Lock;
	DiskBuffer		DiskBuffer[NBUF];

	// Linked list of all buffers, through prev/next.
	// head.Next is most recently used.
	DiskBuffer		Head;
} diskBufferCache;

void diskBufferCacheInitialise(void)
{
	DiskBuffer *b;

	spinlockInitialise(&diskBufferCache.Lock, "diskBufferCache");

	  // Create linked list of buffers
	diskBufferCache.Head.Previous = &diskBufferCache.Head;
	diskBufferCache.Head.Next = &diskBufferCache.Head;
	for (b = diskBufferCache.DiskBuffer; b < diskBufferCache.DiskBuffer + NBUF; b++) 
	{
		b->Next = diskBufferCache.Head.Next;
		b->Previous = &diskBufferCache.Head;
		sleeplockInitialise(&b->Lock, "buffer");
		diskBufferCache.Head.Next->Previous = b;
		diskBufferCache.Head.Next = b;
	}
}

// Look through buffer cache for sector on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.

static DiskBuffer* diskBufferGet(uint32_t dev, uint32_t sectorNumber)
{
	DiskBuffer *b;

	spinlockAcquire(&diskBufferCache.Lock);

	// Is the block already cached?
	for (b = diskBufferCache.Head.Next; b != &diskBufferCache.Head; b = b->Next) 
	{
		if (b->Device == dev && b->SectorNumber == sectorNumber) 
		{
			b->ReferenceCount++;
			spinlockRelease(&diskBufferCache.Lock);
			sleeplockAcquire(&b->Lock);
			return b;
		}
	}

	// Not cached; recycle an unused buffer.
	for (b = diskBufferCache.Head.Previous; b != &diskBufferCache.Head; b = b->Previous) 
	{
		if (b->ReferenceCount == 0 && (b->Flags & B_DIRTY) == 0) 
		{
			b->Device = dev;
			b->SectorNumber = sectorNumber;
			b->Flags = 0;
			b->ReferenceCount = 1;
			spinlockRelease(&diskBufferCache.Lock);
			sleeplockAcquire(&b->Lock);
			return b;
		}
	}
	panic("diskBufferGet: no buffers");
}

// Return a locked DiskBuffer with the contents of the indicated sector.
DiskBuffer *	diskBufferRead(uint32_t dev, uint32_t sectorNumber)
{
	DiskBuffer *b;

	b = diskBufferGet(dev, sectorNumber);
	if ((b->Flags & B_VALID) == 0) 
	{
		ideReadWrite(b);
	}
	return b;
}

// Write b's contents to disk.  Must be locked.
void diskBufferWrite(DiskBuffer *b)
{
	if (!isHoldingSleeplock(&b->Lock))
	{
		panic("diskBufferWrite");
	}
	b->Flags |= B_DIRTY;
	ideReadWrite(b);
}

// Release a locked buffer.
// Move to the head of the MRU list.

void diskBufferRelease(DiskBuffer *b)
{
	if (!isHoldingSleeplock(&b->Lock))
	{
		panic("diskBufferRelease");
	}
	sleeplockRelease(&b->Lock);

	spinlockAcquire(&diskBufferCache.Lock);
	b->ReferenceCount--;
	if (b->ReferenceCount == 0) 
	{
		// no one is waiting for it.
		b->Next->Previous = b->Previous;
		b->Previous->Next = b->Next;
		b->Next = diskBufferCache.Head.Next;
		b->Previous = &diskBufferCache.Head;
		diskBufferCache.Head.Next->Previous = b;
		diskBufferCache.Head.Next = b;
	}
	spinlockRelease(&diskBufferCache.Lock);
}

