
struct _DirectoryEntry
{
	uint8_t   Filename[8];
	uint8_t   Ext[3];
	uint8_t   Attrib;
	uint8_t   Reserved;
	uint8_t   TimeCreatedMs;
	uint16_t  TimeCreated;
	uint16_t  DateCreated;
	uint16_t  DateLastAccessed;
	uint16_t  FirstClusterHiBytes;
	uint16_t  LastModTime;
	uint16_t  LastModDate;
	uint16_t  FirstCluster;
	uint32_t  FileSize;
};

// Filesystem mount information

struct _MountInfo
{
	uint32_t NumSectors;
	uint32_t FatOffset;
	uint32_t NumRootEntries;
	uint32_t RootOffset;
	uint32_t RootSize;
	uint32_t FatSize;
	uint32_t ClusterSize;
};

