#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

Device devices[NDEV];

struct
{
	Spinlock		Lock;
	File			File[NFILE];
} FileTable;

void filesInitialise(void)
{
	File *f;

	spinlockInitialise(&FileTable.Lock, "FileTable");
	spinlockAcquire(&FileTable.Lock);
	for (f = FileTable.File; f < FileTable.File + NFILE; f++)
	{
		f->ReferenceCount = 0;
	}
	spinlockRelease(&FileTable.Lock);
}

// Allocate a file structure.
File* allocateFileStructure(void)
{
	File *f;

	spinlockAcquire(&FileTable.Lock);
	for (f = FileTable.File; f < FileTable.File + NFILE; f++) 
	{
		if (f->ReferenceCount == 0) 
		{
			f->ReferenceCount = 1;
			spinlockRelease(&FileTable.Lock);
			return f;
		}
	}
	spinlockRelease(&FileTable.Lock);
	return 0;
}

// Increment ref count for file f.
File* fileDup(File *f)
{
	spinlockAcquire(&FileTable.Lock);
	if (f->ReferenceCount < 1)
	{
		panic("fileDup");
	}
	f->ReferenceCount++;
	spinlockRelease(&FileTable.Lock);
	return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void fileClose(File *f)
{
	File ff;

	spinlockAcquire(&FileTable.Lock);
	if (f->ReferenceCount < 1)
	{
		panic("fileClose");
	}
	if (--f->ReferenceCount > 0) 
	{
		spinlockRelease(&FileTable.Lock);
		return;
	}
	ff = *f;
	f->ReferenceCount = 0;
	f->Type = FD_NONE;
	spinlockRelease(&FileTable.Lock);
	
	if (ff.Type == FD_PIPE)
	{
		pipeclose(ff.Pipe, ff.Writable);
	}
	else if (ff.Type == FD_FILE || ff.Type == FD_DIR) 
	{
		fsFat12Close(&ff);
	}

}

// Get metadata about file f.
int fileStat(File *f, Stat *st)
{
	if (f->Type == FD_FILE || f->Type == FD_DIR) 
	{
		//    ilock(f->ip);
		//    stati(f->ip, st);
		//    iunlock(f->ip);
		return 0;
	}
	return -1;
}

// Read from file f.
int fileRead(File *f, char *addr, int n)
{
	int r;

	if (f->Readable == 0)
	{
		return -1;
	}
	if (f->Type == FD_DEVICE)
	{
		return devices[f->DeviceID].read(f, addr, n);
	}
	else if (f->Type == FD_PIPE)
	{
		return piperead(f->Pipe, addr, n);
	}
	else if (f->Type == FD_FILE || f->Type == FD_DIR)
	{
		r = fsFat12Read(f, (unsigned char *)addr, n);
		return r;
	}
	panic("fileRead");
}

// Write to file f.
int fileWrite(File *f, char *addr, int n)
{

	if (f->Writable == 0)
	{
		return -1;
	}
	if (f->Type == FD_DEVICE)
	{
		return devices[f->DeviceID].write(f, addr, n);
	}
	else if (f->Type == FD_PIPE)
	{
		return pipewrite(f->Pipe, addr, n);
	}
	// Code to write to a disk file needs to be added
	panic("fileWrite");
}

