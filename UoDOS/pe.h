// PE File Structures

typedef struct _IMAGE_DOS_HEADER 
{  
	uint16_t	Magic;				// must contain "MZ"
	uint16_t	Cblp;				// number of bytes on the last page of the file
	uint16_t	Cp;					// number of pages in file
	uint16_t	Crlc;				// relocations
	uint16_t	Cparhdr;			// size of the header in paragraphs
	uint16_t	Minalloc;			// minimum and maximum paragraphs to allocate
	uint16_t	Maxalloc;
	uint16_t	InitialSS;			// initial SS:SP to set by Loader
	uint16_t	InitialSP;
	uint16_t	Checksum;			// checksum
	uint16_t	InitialIP;			// initial CS:IP
	uint16_t	InitialCS;
	uint16_t	RelocationTable;	// address of relocation table
	uint16_t	Ovno;				// overlay number
	uint16_t	reserved1[4];		// resevered
	uint16_t	Oemid;				// OEM id
	uint16_t	Oeminfo;			// OEM info
	uint16_t	reserved2[10];		// reserved
	uint32_t	NewExeHeader;		// address of new EXE header
} IMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER 
{
	uint16_t	Machine;
	uint16_t	NumberOfSections;			// Number of sections in section table
	uint32_t	TimeDateStamp;				// Date and time of program link
	uint32_t	PointerToSymbolTable;		// RVA of symbol table
	uint32_t	NumberOfSymbols;			// Number of symbols in table
	uint16_t	SizeOfOptionalHeader;		// Size of IMAGE_OPTIONAL_HEADER in bytes
	uint16_t	Characteristics;
} IMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY 
{
	uint32_t	VirtualAddress;				// RVA of table
	uint32_t	Size;						// size of table
} IMAGE_DATA_DIRECTORY;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 10

typedef struct _IMAGE_OPTIONAL_HEADER 
{
	uint16_t	Magic;						// not-so-magical number
	uint8_t		MajorLinkerVersion;			// linker version
	uint8_t		MinorLinkerVersion;
	uint32_t	SizeOfCode;					// size of .text in bytes
	uint32_t	SizeOfInitializedData;		// size of .bss (and others) in bytes
	uint32_t	SizeOfUninitializedData;	// size of .data,.sdata etc in bytes
	uint32_t	AddressOfEntryPoint;		// RVA of entry point
	uint32_t	BaseOfCode;					// base of .text
	uint32_t	BaseOfData;					// base of .data
	uint32_t	ImageBase;					// image base VA
	uint32_t	SectionAlignment;			// file section alignment
	uint32_t	FileAlignment;				// file alignment
	uint16_t	MajorOperatingSystemVersion;
	uint16_t	MinorOperatingSystemVersion;
	uint16_t	MajorImageVersion;			// version of program
	uint16_t	MinorImageVersion;
	uint16_t	MajorSubsystemVersion;		// Windows specific. Version of SubSystem
	uint16_t	MinorSubsystemVersion;
	uint32_t	Reserved1;
	uint32_t	SizeOfImage;				// size of image in bytes
	uint32_t	SizeOfHeaders;				// size of headers (and stub program) in bytes
	uint32_t	CheckSum;					// checksum
	uint16_t	Subsystem;					// Windows specific. subsystem type
	uint16_t	DllCharacteristics;			// DLL properties
	uint32_t	SizeOfStackReserve;			// size of stack, in bytes
	uint32_t	SizeOfStackCommit;			// size of stack to commit
	uint32_t	SizeOfHeapReserve;			// size of heap, in bytes
	uint32_t	SizeOfHeapCommit;			// size of heap to commit
	uint32_t	LoaderFlags;				// no longer used
	uint32_t	NumberOfRvaAndSizes;		// number of DataDirectory entries
	//IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER;

typedef struct _SECTION_HEADER
{
	char		SectionName[8];
	uint32_t	ActualSize;
	uint32_t	VirtualAddress;
	uint32_t	RoundedUpSize;
	uint32_t	OffsetInExeFile;
	uint32_t	PointerToRelocations;
	uint32_t	PointerToLineNumbers;
	uint16_t	NumberOfRelocations;
	uint16_t	NumberOfLineNumbers;
	uint32_t	Characteristics;
} IMAGE_SECTION_HEADER;

typedef struct _IMAGE_NT_HEADERS 
{
	char                  Signature[4];
	IMAGE_FILE_HEADER     FileHeader;
	IMAGE_OPTIONAL_HEADER OptionalHeader;
} IMAGE_NT_HEADERS;

