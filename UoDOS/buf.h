#define BSIZE 512  // block size

struct _DiskBuffer
{
	int				Flags;
	uint32_t		Device;
	uint32_t		SectorNumber;
	Sleeplock		Lock;
	uint32_t		ReferenceCount;
	DiskBuffer *	Previous;
	DiskBuffer *	Next;
	DiskBuffer *	QueueNext; // disk queue
	uint8_t			Data[BSIZE];
};

#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk

