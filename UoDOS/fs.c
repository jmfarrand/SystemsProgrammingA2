#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "bpb.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

#define isascii(c)	((unsigned)(c) <= 0x7F)
#define toascii(c)	((unsigned)(c) & 0x7F)
#define isupper(c)	(c >= 'A' && c <= 'Z')
#define islower(c)	(c >= 'a' && c <= 'z')
#define tolower(c)	(isupper(c) ? c + 'a' - 'A' : c)
#define toupper(c)	(islower(c) ? c + 'A' - 'a' : c)

BootSector bootSector;
MountInfo  mountInfo;

// File Allocation Table 
unsigned char fat[512 * MAXFATSIZE];

void fsFat12Initialise(void)
{
	DiskBuffer * bpb = diskBufferRead(0, 0);
	memmove(&bootSector, bpb->Data, sizeof(BootSector));
	diskBufferRelease(bpb);
	// Store mount info
	if (bootSector.Bpb.BytesPerSector != 512)
	{
		panic("Sector size != 512");
	}
	mountInfo.NumSectors = bootSector.Bpb.NumSectors;
	mountInfo.FatOffset = bootSector.Bpb.ReservedSectors;
	mountInfo.FatSize = bootSector.Bpb.SectorsPerFat;
	mountInfo.NumRootEntries = bootSector.Bpb.NumDirEntries;
	mountInfo.RootOffset = (bootSector.Bpb.NumberOfFats * bootSector.Bpb.SectorsPerFat) + bootSector.Bpb.ReservedSectors;
	mountInfo.RootSize = (bootSector.Bpb.NumDirEntries * 32) / bootSector.Bpb.BytesPerSector;
	mountInfo.ClusterSize = bootSector.Bpb.SectorsPerCluster * bootSector.Bpb.BytesPerSector;
	// Read the FAT into memory
	if (mountInfo.FatSize > MAXFATSIZE)
	{
		panic("FAT too large");
	}
	uint32_t fatOffset = 0;
	for (int fatSector = 0; fatSector < mountInfo.FatSize; fatSector++)
	{
		DiskBuffer * fatContents = diskBufferRead(0, fatSector + mountInfo.FatOffset);
		memmove(&fat + fatOffset, fatContents->Data, 512);
		diskBufferRelease(fatContents);
		fatOffset += 512;
	}
}

// Helper function. Converts filename to DOS 8.3 file format

void toDosFileName(const char* filename, char* convertedFileName, unsigned int filenameLength)
{
	unsigned int i = 0;

	if (filenameLength > 11)
	{
		return;
	}
	if (!convertedFileName || !filename)
	{
		return;
	}
	// Set all characters in output name to spaces
	memset(convertedFileName, ' ', filenameLength);

	// Deal with the special cases of '.' and '..'
	if (strlen(filename) == 1 && filename[0] == '.')
	{
		convertedFileName[0] = '.';
		return;
	}
	if (strlen(filename) == 2 && filename[0] == '.' && filename[1] == '.')
	{
		convertedFileName[0] = '.';
		convertedFileName[1] = '.';
		return;
	}

	// Get first part of the filename
	for (i = 0; i < strlen(filename) && i < filenameLength; i++)
	{
		if (filename[i] == '.' || i == 8)
		{
			break;
		}
		// Capitalise character and copy it over 
		convertedFileName[i] = toupper(filename[i]);
	}
	// Add extension if needed
	if (filename[i] == '.')
	{
		// Note: cannot just copy over (extension might not be 3 chars)
		for (int k = 0; k<3; k++)
		{
			i++;
			if (filename[i])
			{
				convertedFileName[k + 8] = toupper(filename[i]);
			}
		}
	}
}

uint32_t fsFat12ReadCluster(uint32_t deviceNumber, uint32_t clusterNumber, unsigned char * buffer, uint32_t offset, uint32_t size)
{
	DiskBuffer * sectorContents;
	uint32_t sectorContentsSize;
	uint32_t readSize;

	if (size > mountInfo.ClusterSize)
	{
		size = mountInfo.ClusterSize;
	}
	if (offset >= mountInfo.ClusterSize)
	{
		offset = 0;
	}
	if (offset + size > mountInfo.ClusterSize)
	{
		size = mountInfo.ClusterSize - offset;
	}
	uint32_t sector = mountInfo.RootOffset + mountInfo.RootSize + ((clusterNumber - 2) * bootSector.Bpb.SectorsPerCluster);
	uint32_t sectorOffset = 0;
	if (offset > 0)
	{
		sectorOffset = offset / bootSector.Bpb.BytesPerSector;
		sector += sectorOffset;
		offset = offset % bootSector.Bpb.BytesPerSector;
		if (size > mountInfo.ClusterSize - (sectorOffset * bootSector.Bpb.BytesPerSector + offset))
		{
			size = mountInfo.ClusterSize - (sectorOffset * bootSector.Bpb.BytesPerSector + offset);
		}
	}
	readSize = size;
	while (size > 0)
	{
		sectorContents = diskBufferRead(0, sector);
		sectorContentsSize = bootSector.Bpb.BytesPerSector;
		if (size < bootSector.Bpb.BytesPerSector)
		{
			sectorContentsSize = size;
		}
		memmove(buffer, (const void *)(&sectorContents->Data[0] + offset), sectorContentsSize);
		diskBufferRelease(sectorContents);
		offset = 0;
		buffer += sectorContentsSize;
		size -= sectorContentsSize;
		sector++;
	}
	return readSize;
}

File * fsFat12CreateFileStructure(DirectoryEntry * directoryEntry, const char * filename)
{
	File * file = allocateFileStructure();
	if (file == 0)
	{
		return 0;
	}
	strcpy(file->Name, filename);
	memmove(&file->DirectoryEntry, directoryEntry, sizeof(DirectoryEntry));
	file->Size = directoryEntry->FileSize;
	file->Position = 0;
	file->Eof = 0;
	file->Type = FD_FILE;
	return file;
}


// Locates file or directory in root directory

bool fsFat12FindInRootDirectory(const char* nameToFind, DirectoryEntry * foundDirectoryEntry)
{
	DiskBuffer * buf;
	DirectoryEntry * directoryEntry;
	// Get 8.3 directory name
	char dosFileName[12];
	toDosFileName(nameToFind, dosFileName, 11);
	dosFileName[11] = 0;

	int rootDirectorySectorCount = mountInfo.NumRootEntries / 16;
	for (int sector = 0; sector < rootDirectorySectorCount; sector++)
	{
		// Read in sector of root directory
		buf = diskBufferRead(0, mountInfo.RootOffset + sector);
		// get directory entry info
		directoryEntry = (DirectoryEntry *)buf->Data;

		// 16 entries per sector
		for (int i = 0; i<16; i++)
		{
			if (*(directoryEntry->Filename) != 0)
			{
				// get filename from directory entry
				char name[12];
				memmove(name, directoryEntry->Filename, 11);
				name[11] = 0;

				// Is there a match?
				if (strcmp(dosFileName, name) == 0)
				{
					// Found it
					memmove((char *)foundDirectoryEntry, (char *)directoryEntry, sizeof(DirectoryEntry));
					diskBufferRelease(buf);
					return 1;
				}
			}
			//! go to next directory
			directoryEntry++;
		}
		diskBufferRelease(buf);
	}
	return 0;
}

// Locate a file or folder in subdirectory. 
//

bool fsFat12FindInSubDirectory(const char* nameToFind, DirectoryEntry * foundDirectoryEntry)
{
	unsigned char buf[512];

	// Take a copy of the directory entry for the sub-directory we are going to search
	DirectoryEntry currentDirectory;
	memmove((char *)&currentDirectory, (char *)foundDirectoryEntry, sizeof(DirectoryEntry));
	
	// Create a file structure for it - we don' bother with a name
	File * subDirectory = fsFat12CreateFileStructure(&currentDirectory, "");

	// Get 8.3 name for sub-directory we are searching for
	char dosFileName[12];
	toDosFileName(nameToFind, dosFileName, 11);
	dosFileName[11] = 0;

	//! read directory
	while (!subDirectory->Eof)
	{
		// Read directory
		fsFat12Read(subDirectory, buf, 512);

		DirectoryEntry * subDirectoryEntry = (DirectoryEntry *)buf;

		// 16 entries in buffer
		for (unsigned int i = 0; i < 16; i++)
		{
			// Get current filename
			char name[12];
			memmove(name, subDirectoryEntry->Filename, 11);
			name[11] = 0;
			// Is there a match?
			if (strcmp(name, dosFileName) == 0)
			{
				memmove((char *)foundDirectoryEntry, (char *)subDirectoryEntry, sizeof(DirectoryEntry));
				fileClose(subDirectory);
				return 1;
			}
			// go to next entry
			subDirectoryEntry++;
		}
	}

	// unable to find file
	fileClose(subDirectory);
	return 0;
}

uint32_t fsFat12GetNextCluster(uint32_t cluster)
{
	unsigned int fatOffset = cluster + (cluster / 2); //multiply by 1.5
	uint16_t nextCluster = *(uint16_t*)&fat[fatOffset];

	// Test if entry is odd or even
	if (cluster & 0x0001)
	{
		// Get high 12 bits
		nextCluster >>= 4;
	}
	else
	{
		nextCluster &= 0x0FFF;
	}
	// Test for end of file
	if (nextCluster >= 0xff8)
	{
		return 0;
	}
	// Test for file corruption
	if (nextCluster == 0)
	{
		return 0;
	}
	return nextCluster;
}

// Read from a file

uint32_t fsFat12Read(File * file, unsigned char* buffer, unsigned int length)
{
	uint32_t readLength = 0;
	uint32_t totalRead = 0;

	if (file && (file->Type == FD_FILE || file->Type == FD_DIR) && file->Eof == 0)
	{
		// Calculate starting cluster
		uint32_t clusterHops = file->Position / mountInfo.ClusterSize;
		uint32_t clusterOffset = file->Position % mountInfo.ClusterSize;
		uint32_t currentCluster = file->DirectoryEntry.FirstCluster;
		while (clusterHops > 0)
		{
			// Follow the cluster chain to get to the cluster we want to start with
			currentCluster = fsFat12GetNextCluster(currentCluster);
			clusterHops--;
		}
		while (length > 0)
		{
			readLength = fsFat12ReadCluster(0, currentCluster, buffer, clusterOffset, length);
			buffer += readLength;
			length -= readLength;
			totalRead += readLength;
			file->Position += readLength;
			if (file->Position >= file->Size && file->Type == FD_FILE)
			{
				file->Eof = 1;
				return totalRead;
			}
			if (clusterOffset + readLength == mountInfo.ClusterSize)
			{
				currentCluster = fsFat12GetNextCluster(currentCluster);
			}
			clusterOffset = 0;
			if (currentCluster == 0)
			{
				file->Eof = 1;
				return totalRead;
			}
		}
	}
	return totalRead;
}

//	Closes file

void fsFat12Close(File * file)
{
	if (file)
	{
		file->Type = FD_NONE;
	}
}

// Parse path to get next directory or filename on path. 
// Returns 0 if we are returning the filename in pathPart or 
// the length of the directory name if it is a directory name

int fsGetPathPart(char * path, char * pathPart)
{
	char * p = path;
	while (*p != 0 && *p != '\\' && *p != '/')
	{
		p++;
	}
	int partLength = (int)(p - path);
	strcpy(pathPart, path);
	*(pathPart + partLength) = 0;	
	if (*p == 0)
	{
		return 0;
	}
	else
	{
		return partLength;
	}
}

//  Open a file
//
//  cwd = 		The current working directory
//  filename  = The path of the file to open.  If it does not begin with a '\' or '/'. we prepend cwd to the filename
//  directory = 1 if we are opening a sub-directory, 0 otherwise

File * fsFat12Open(const char * cwd, const char* filename, int directory)
{
	DirectoryEntry currentDirectoryEntry;
	char * p = 0;
	bool rootDirectory = true;
	char path[255];
	char pathPart[20];
	int partLength;
	
	if (*filename == '\\' || *filename == '/')
	{
		safestrcpy(path, filename, MAXCWDSIZE);
	}
	else
	{
		safestrcpy(path, cwd, MAXCWDSIZE);
		int cwdLen = strlen(cwd);
		safestrcpy(path + cwdLen, filename, MAXCWDSIZE - cwdLen);
	}
	// Move past first '/' or '\'
	p = path + 1;
	while (p)
	{
		partLength = fsGetPathPart(p, pathPart);
		if (rootDirectory)
		{
			// Search root directory 
			if (fsFat12FindInRootDirectory(pathPart, &currentDirectoryEntry) == 0)
			{
				// This part of the path was not found in the directory
				return 0;
			}
			rootDirectory = false;
		}
		else
		{
			// Search subdirectory
			if (fsFat12FindInSubDirectory(pathPart, &currentDirectoryEntry) == 0)
			{
				// This part of the path was not found in the directory
				return 0;
			}
		}	
		// If we got here, we do have a match for this part of the path
		if (partLength == 0)
		{
			// If we are trying to open a directory, but we have not found a directory, return error
			if (directory == 1 && currentDirectoryEntry.Attrib != 0x10)
			{
				return 0;
			}
			// This is the filename component, so just return a file structure
			File * fileStructure = fsFat12CreateFileStructure(&currentDirectoryEntry, filename);			
			if (directory == 1)
			{
				fileStructure->Type = FD_DIR;			
			} 
			return fileStructure;
		}
		// Check to see if we found a directory.  If not, then we cannot continue
		if (currentDirectoryEntry.Attrib != 0x10)
		{
			return 0;
		}
		p = p + partLength + 1;
	}
	return 0;
}
