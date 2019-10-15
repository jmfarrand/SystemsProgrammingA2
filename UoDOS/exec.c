#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "fs.h"
#include "file.h"
#include "pe.h"

IMAGE_NT_HEADERS imageFileHeader;

void cleanupExec(pde_t * pageTable, File * exeFile)
{
	if (pageTable)
	{
		freeMemoryAndPageTable(pageTable);
	}
	fileClose(exeFile);
}

int exec(char *path, char **argv)
{
	char *s;
	char *last;
	uint32_t argc;
	uint32_t sp;
	uint32_t ustack[3 + MAXARG + 1];
	uint32_t memorySize;
	pde_t *pgdir;
	pde_t *oldpgdir;
 	Process *curproc = myProcess();
	int oldFilePosition;
		
	File * exeFile = fsFat12Open(curproc->Cwd, path, 0);
	if (!exeFile)
	{
		return -1;
	}
	exeFile->Readable = 1;
	exeFile->Writable = 0;
	// Get the IMAGE_FILE_HEADER structure (we skip the DOS Header)
	exeFile->Position = 0x80;
	int count = fileRead(exeFile, (char *)&imageFileHeader, sizeof(IMAGE_FILE_HEADER) + 4);
	if (count != sizeof(IMAGE_FILE_HEADER) + 4)
	{
		fileClose(exeFile);
		return -1;
	}
	if (imageFileHeader.Signature[0] != 'P' || imageFileHeader.Signature[1] != 'E')
	{
		fileClose(exeFile);
		return -1;
	}
	exeFile->Position = 0x80 + sizeof(IMAGE_FILE_HEADER) + 4;
	count = fileRead(exeFile, (char *)&imageFileHeader.OptionalHeader, sizeof(IMAGE_OPTIONAL_HEADER));
	if (count != sizeof(IMAGE_OPTIONAL_HEADER))
	{
		fileClose(exeFile);
		return -1;
	}
	if ((pgdir = setupKernelVirtualMemory()) == 0)
	{
		fileClose(exeFile);
		return -1;
	}
	oldFilePosition = 0x80 + sizeof(IMAGE_FILE_HEADER) + 4 + imageFileHeader.FileHeader.SizeOfOptionalHeader;
	memorySize = 0;
	for (int i = 0; i < imageFileHeader.FileHeader.NumberOfSections; i++)
	{
		IMAGE_SECTION_HEADER sectionHeader;
		exeFile->Position = oldFilePosition;
		count = fileRead(exeFile, (char *)&sectionHeader, sizeof(IMAGE_SECTION_HEADER));
		if (count != sizeof(IMAGE_SECTION_HEADER))
		{
			fileClose(exeFile);
			return -1;
		}
		oldFilePosition = exeFile->Position;
		if ((memorySize = allocateMemoryAndPageTables(pgdir, memorySize, sectionHeader.VirtualAddress + sectionHeader.ActualSize)) == 0)
		{
			cleanupExec(pgdir, exeFile);
			return -1;
		}
		if (loadProgramSegmentIntoPageTable(pgdir, (char*)sectionHeader.VirtualAddress, exeFile, sectionHeader.OffsetInExeFile, sectionHeader.ActualSize) < 0)
		{
			cleanupExec(pgdir, exeFile);
			return -1;
		}
	}
	fileClose(exeFile);
 
	// Allocate two pages at the next page boundary.
	// Make the first inaccessible.  Use the second as the user stack.
	memorySize = PGROUNDUP(memorySize);
	if ((memorySize = allocateMemoryAndPageTables(pgdir, memorySize, memorySize + 2 * PGSIZE)) == 0)
	{
		cleanupExec(pgdir, exeFile);
		return -1;
	}
	clearPTEU(pgdir, (char*)(memorySize - 2 * PGSIZE));
    sp = memorySize;

	// Push argument strings, prepare rest of stack in ustack.
    for(argc = 0; argv[argc]; argc++) 
	{
		if (argc >= MAXARG)
		{
			cleanupExec(pgdir, exeFile);
			return -1;
		}
		sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
        if (copyToUserVirtualMemory(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
		{
			cleanupExec(pgdir, exeFile);
			return -1;
		}
	    ustack[3+argc] = sp;
	}
	ustack[3+argc] = 0;

    ustack[0] = 0xffffffff;  // fake return PC
	ustack[1] = argc;
	ustack[2] = sp - (argc+1) * 4;  // argv pointer

    sp -= (3+argc+1) * 4;
    if (copyToUserVirtualMemory(pgdir, sp, ustack, (3+argc+1)*4) < 0)
	{
		cleanupExec(pgdir, exeFile);
		return -1;
	}

	// Save program name for debugging.
	for (last = s = path; *s; s++)
	{
		if (*s == '/' || *s == '\\')
		{
			last = s + 1;
		}
	}
	safestrcpy(curproc->Name, last, sizeof(curproc->Name));

	// Commit to the user image.
	oldpgdir = curproc->PageTable;
	curproc->PageTable = pgdir;
	curproc->MemorySize = memorySize;
	curproc->Trapframe->eip = imageFileHeader.OptionalHeader.AddressOfEntryPoint;
	curproc->Trapframe->esp = sp;
    switchToUserVirtualMemory(curproc);
    freeMemoryAndPageTable(oldpgdir);
	return 0;
}
