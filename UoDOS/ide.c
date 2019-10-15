// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

#define SECTOR_SIZE		512

// IDE Status codes

#define IDE_BUSY				0x80  // Indicates the drive is preparing to send/receive data (wait for it to clear). 
#define IDE_READY				0x40  // Bit is clear when drive is spun down, or after an error. Set otherwise. 
#define IDE_DRIVEFAULT			0x20  // Drive Fault Error (does not set ERROR). 
#define IDE_ERROR				0x01  // Indicates an error occurred. Send a new command to clear it 

// IDE Commands 

#define IDE_CMD_READ			0x20
#define IDE_CMD_WRITE			0x30
#define IDE_CMD_READMULTIPLE	0xc4
#define IDE_CMD_WRITEMULTIPLE	0xc5

// idequeue points to the DiskBuffer now being read/written to the disk.
// idequeue->QueueNext points to the next DiskBuffer to be processed.
// You must hold idelock while manipulating queue.

static Spinlock			idelock;
static DiskBuffer *		idequeue;

static int havedisk1;

static void ideStartRequest(DiskBuffer*);

// Wait for IDE disk to become ready. 
//
// checkForError should be set to true if we should check if an error occurred, false otherwise

static int ideWait(int checkForError)
{
	int status;

	while(((status = inputByteFromPort(0x1f7)) & (IDE_BUSY | IDE_READY)) != IDE_READY)
		;
	if (checkForError && (status & (IDE_DRIVEFAULT | IDE_ERROR)) != 0)
	{
		return -1;
	}
	return 0;
}

void ideInitialise(void)
{
	int i;

	spinlockInitialise(&idelock, "ide");
	ioApicEnable(IRQ_IDE, ncpu - 1);
	ideWait(0);

	// Check if disk 1 is present
	outputByteToPort(0x1f6, 0xe0 | (1<<4));
	for(i=0; i<1000; i++)
	{
		if(inputByteFromPort(0x1f7) != 0)
		{
			havedisk1 = 1;
			break;
		}
	}

	// Switch back to disk 0.
	outputByteToPort(0x1f6, 0xe0 | (0<<4));
}

// Start the request for b.  Caller must hold idelock.

static void ideStartRequest(DiskBuffer *b)
{
	if (b == 0)
	{
		panic("ideStartRequest");
	}
	//if (b->SectorNumber >= FSSIZE)
	//{
	//	panic("incorrect sectorNumber");
	//}
	int sector_per_block =  BSIZE/SECTOR_SIZE;
	int sector = b->SectorNumber * sector_per_block;
	int readCmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_READMULTIPLE;
	int writeCmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRITEMULTIPLE;

	if (sector_per_block > 7)
	{
		panic("ideStartRequest");
	}
	ideWait(0);
	outputByteToPort(0x3f6, 0);  // generate interrupt
	outputByteToPort(0x1f2, sector_per_block);  // number of sectors
	outputByteToPort(0x1f3, sector & 0xff);
	outputByteToPort(0x1f4, (sector >> 8) & 0xff);
	outputByteToPort(0x1f5, (sector >> 16) & 0xff);
	outputByteToPort(0x1f6, 0xe0 | ((b->Device&1)<<4) | ((sector>>24)&0x0f));
	if(b->Flags & B_DIRTY)
	{
		outputByteToPort(0x1f7, writeCmd);
		outputSequenceToPort(0x1f0, b->Data, BSIZE/4);
	} 
	else 
	{
		outputByteToPort(0x1f7, readCmd);
	}
}

// Interrupt handler.

void ideInterruptHandler(void)
{
	DiskBuffer *b;

	// First queued buffer is the active request.
	spinlockAcquire(&idelock);

	// If no queued buffers, simply return
	if ((b = idequeue) == 0)
	{
		spinlockRelease(&idelock);
		return;
	}
	idequeue = b->QueueNext;

	// Read data if needed.
	if (!(b->Flags & B_DIRTY) && ideWait(1) >= 0)
	{
		inputSequenceFromPort(0x1f0, b->Data, BSIZE / 4);
	}

	// Wake process waiting for this DiskBuffer.
	b->Flags |= B_VALID;
	b->Flags &= ~B_DIRTY;
	wakeup(b);

	// Start disk on next DiskBuffer in queue.
	if (idequeue != 0)
	{
		ideStartRequest(idequeue);
	}
	spinlockRelease(&idelock);
}

// Sync DiskBuffer with disk.
// If B_DIRTY is set, write DiskBuffer to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read DiskBuffer from disk, set B_VALID.

void ideReadWrite(DiskBuffer *b)
{
	DiskBuffer **pp;

	if (!isHoldingSleeplock(&b->Lock))
	{
		panic("ideReadWrite: DiskBuffer not locked");
	}
	if ((b->Flags & (B_VALID | B_DIRTY)) == B_VALID)
	{
		panic("ideReadWrite: nothing to do");
	}
	if (b->Device != 0 && !havedisk1)
	{
		panic("ideReadWrite: ide disk 1 not present");
	}

	spinlockAcquire(&idelock);  

	// Append b to idequeue.
	b->QueueNext = 0;
	for(pp = &idequeue; *pp; pp = &(*pp)->QueueNext)  
		;
	*pp = b;

	// Start disk if necessary.
	if (idequeue == b)
	{
		ideStartRequest(b);
	}

	// Wait for request to finish.
	while((b->Flags & (B_VALID | B_DIRTY)) != B_VALID)
	{
		sleep(b, &idelock);
	}

	  spinlockRelease(&idelock);
}
