#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"

extern char data[];  			// defined by kernel.ld
pde_t *kernelPageDirectory;  	// for use in Scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.

void initialiseGDT(void)
{
	Cpu *c;

	// Map "logical" addresses to virtual addresses using identity map.
	// Cannot share a CODE descriptor for both kernel and user
	// because it would have to have DPL_USR, but the CPU forbids
	// an interrupt from CPL=0 to DPL=3.
	c = &cpus[cpuId()];
	c->Gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, 0);
	c->Gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
	c->Gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
	c->Gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
	loadGlobalDescriptorTable(c->Gdt, sizeof(c->Gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.

static pte_t * getPageTableEntry(pde_t *pgdir, const void *va, int alloc)
{
	pde_t *pde;
	pte_t *pgtab;

	pde = &pgdir[PDX(va)];
	if (*pde & PTE_P) 
	{
		pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
	}
	else 
	{
		if (!alloc || (pgtab = (pte_t*)allocatePhysicalMemoryPage()) == 0)
		{
			return 0;
		}
		// Make sure all those PTE_P bits are zero.
		memset(pgtab, 0, PGSIZE);
		// The permissions here are overly generous, but they can
		// be further restricted by the permissions in the page table
		// entries, if necessary.
		*pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
	}
	return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.

static int createPageTableEntries(pde_t *pgdir, void *va, uint32_t size, uint32_t pa, int perm)
{
	char *a, *last;
	pte_t *pte;

	a = (char*)PGROUNDDOWN((uint32_t)va);
	last = (char*)PGROUNDDOWN(((uint32_t)va) + size - 1);
	for (;;) 
	{
		if ((pte = getPageTableEntry(pgdir, a, 1)) == 0)
		{
			return -1;
		}
		if (*pte & PTE_P)
		{
			panic("remap");
		}
		*pte = pa | perm | PTE_P;
		if (a == last)
		{
			break;
		}
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kernelPageDirectory). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupKernelVirtualMemory() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioApic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.

static struct kernelMemoryMap 
{
	void *virt;
	uint32_t phys_start;
	uint32_t phys_end;
	int perm;
} kernelMemoryMap[] = 
{
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.

pde_t* setupKernelVirtualMemory(void)
{
	pde_t *pgdir;
	struct kernelMemoryMap *k;

	if ((pgdir = (pde_t*)allocatePhysicalMemoryPage()) == 0)
	{
		return 0;
	}
	memset(pgdir, 0, PGSIZE);
	if (P2V(PHYSTOP) > (void*)DEVSPACE)
	{
		panic("PHYSTOP too high");
	}
	for (k = kernelMemoryMap; k < &kernelMemoryMap[NELEM(kernelMemoryMap)]; k++)
	{
		if (createPageTableEntries(pgdir, k->virt, k->phys_end - k->phys_start, (uint32_t)k->phys_start, k->perm) < 0)
		{
			freeMemoryAndPageTable(pgdir);
			return 0;
		}
	}
	return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for Scheduler processes.

void allocateKernelVirtualMemory(void)
{
	kernelPageDirectory = setupKernelVirtualMemory();
	switchToKernelVirtualMemory();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.

void switchToKernelVirtualMemory(void)
{
	loadControlRegister3(V2P(kernelPageDirectory));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.

void switchToUserVirtualMemory(Process *p)
{
	if (p == 0)
	{
		panic("switchToUserVirtualMemory: no process");
	}
	if (p->KernelStack == 0)
	{
		panic("switchToUserVirtualMemory: no kstack");
	}
	if (p->PageTable == 0)
	{
		panic("switchToUserVirtualMemory: no pgdir");
	}
	pushCli();
	myCpu()->Gdt[SEG_TSS] = SEG16(STS_T32A, &myCpu()->Taskstate, sizeof(myCpu()->Taskstate) - 1, 0);
	myCpu()->Gdt[SEG_TSS].s = 0;
	myCpu()->Taskstate.ss0 = SEG_KDATA << 3;
	myCpu()->Taskstate.esp0 = (uint32_t)p->KernelStack + KSTACKSIZE;
	// setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
	// forbids I/O instructions (e.g., inb and outb) from user space
	myCpu()->Taskstate.iomb = (uint16_t)0xFFFF;
	loadTaskRegister(SEG_TSS << 3);
	loadControlRegister3(V2P(p->PageTable));  // switch to process's address space
	popCli();
}

// Load the initcode into address 0 of pgdir.
// memorySize must be less than a page.

void initialiseUserVirtualMemory(pde_t *pgdir, char *init, uint32_t memorySize)
{
	char *mem;

	if (memorySize >= PGSIZE)
	{
		panic("initialiseUserVirtualMemory: more than a page");
	}
	mem = allocatePhysicalMemoryPage();
	memset(mem, 0, PGSIZE);
	createPageTableEntries(pgdir, 0, PGSIZE, V2P(mem), PTE_W | PTE_U);
	memmove(mem, init, memorySize);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.

int loadProgramSegmentIntoPageTable(pde_t *pgdir, char *addr, File *f, uint32_t offset, uint32_t memorySize)
{
	uint32_t i, pa, n;
	pte_t *pte;

	if ((uint32_t)addr % PGSIZE != 0)
	{
		panic("loadProgramSegmentIntoPageTable: addr must be page aligned");
	}
	for (i = 0; i < memorySize; i += PGSIZE)
	{
		if ((pte = getPageTableEntry(pgdir, addr + i, 0)) == 0)
		{
			panic("loadProgramSegmentIntoPageTable: address should exist");
		}
		pa = PTE_ADDR(*pte);
		if (memorySize - i < PGSIZE)
		{
			n = memorySize - i;
		}
		else
		{
			n = PGSIZE;
		}
		if (offset == 0)
		{
			// No file offset specified in the section header, so just initialise the 
			// memory to 0 (for example, this happens in the .bss section).
			memset((char *)P2V(pa), 0, n);
		}
		else
		{
			f->Position = offset + i;
			if (fileRead(f, P2V(pa), n) != n)
			{
				return -1;
			}
		}
	}
	return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.

int allocateMemoryAndPageTables(pde_t *pgdir, uint32_t oldsz, uint32_t newsz)
{
	char *mem;
	uint32_t a;

	if (newsz >= KERNBASE)
	{
		return 0;
	}
	if (newsz < oldsz)
	{
		return oldsz;
	}
	a = PGROUNDUP(oldsz);
	for (; a < newsz; a += PGSIZE) 
	{
		mem = allocatePhysicalMemoryPage();
		if (mem == 0) 
		{
			cprintf("allocateMemoryAndPageTables out of memory\n");
			releaseUserPages(pgdir, newsz, oldsz);
			return 0;
		}
		memset(mem, 0, PGSIZE);
		if (createPageTableEntries(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) 
		{
			cprintf("allocateMemoryAndPageTables out of memory (2)\n");
			releaseUserPages(pgdir, newsz, oldsz);
			freePhysicalMemoryPage(mem);
			return 0;
		}
	}
	return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.

int releaseUserPages(pde_t *pgdir, uint32_t oldsz, uint32_t newsz)
{
	pte_t *pte;
	uint32_t a, pa;

	if (newsz >= oldsz)
	{
		return oldsz;
	}
	a = PGROUNDUP(newsz);
	for (; a < oldsz; a += PGSIZE) 
	{
		pte = getPageTableEntry(pgdir, (char*)a, 0);
		if (!pte)
		{
			a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
		}
		else if ((*pte & PTE_P) != 0) 
		{
			pa = PTE_ADDR(*pte);
			if (pa == 0)
			{
				panic("freePhysicalMemoryPage");
			}
			char *v = P2V(pa);
			freePhysicalMemoryPage(v);
			*pte = 0;
		}
	}
	return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.

void freeMemoryAndPageTable(pde_t *pgdir)
{
	uint32_t i;

	if (pgdir == 0)
	{
		panic("freeMemoryAndPageTable: no pgdir");
	}
	releaseUserPages(pgdir, KERNBASE, 0);
	for (i = 0; i < NPDENTRIES; i++) 
	{
		if (pgdir[i] & PTE_P) 
		{
			char * v = P2V(PTE_ADDR(pgdir[i]));
			freePhysicalMemoryPage(v);
		}
	}
	freePhysicalMemoryPage((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.

void clearPTEU(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = getPageTableEntry(pgdir, uva, 0);
	if (pte == 0)
	{
		panic("clearPTEU");
	}
	*pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.

pde_t* copyProcessPageTable(pde_t *pgdir, uint32_t MemorySize)
{
	pde_t *d;
	pte_t *pte;
	uint32_t pa, i, flags;
	char *mem;

	if ((d = setupKernelVirtualMemory()) == 0)
	{
		return 0;
	}
	for (i = 0; i < MemorySize; i += PGSIZE) 
	{
		if ((pte = getPageTableEntry(pgdir, (void *)i, 0)) == 0)
		{
			panic("copyProcessPageTable: pte should exist");
		}
		if (!(*pte & PTE_P))
		{
			panic("copyProcessPageTable: page not present");
		}
		pa = PTE_ADDR(*pte);
		flags = PTE_FLAGS(*pte);
		if ((mem = allocatePhysicalMemoryPage()) == 0)
		{
			freeMemoryAndPageTable(d);
			return 0;
		}
		memmove(mem, (char*)P2V(pa), PGSIZE);
		if (createPageTableEntries(d, (void*)i, PGSIZE, V2P(mem), flags) < 0)
		{
			freeMemoryAndPageTable(d);
			return 0;
		}
	}
	return d;
}

// Map user virtual address to kernel address.

char* mapVirtualAddressToKernelAddress(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = getPageTableEntry(pgdir, uva, 0);
	if ((*pte & PTE_P) == 0)
	{
		return 0;
	}
	if ((*pte & PTE_U) == 0)
	{
		return 0;
	}
	return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// mapVirtualAddressToKernelAddress ensures this only works for PTE_U pages.

int copyToUserVirtualMemory(pde_t *pgdir, uint32_t va, void *p, uint32_t len)
{
	char *buf, *pa0;
	uint32_t n, va0;

	buf = (char*)p;
	while (len > 0) 
	{
		va0 = (uint32_t)PGROUNDDOWN(va);
		pa0 = mapVirtualAddressToKernelAddress(pgdir, (char*)va0);
		if (pa0 == 0)
		{
			return -1;
		}
		n = PGSIZE - (va - va0);
		if (n > len)
		{
			n = len;
		}
		memmove(pa0 + (va - va0), buf, n);
		len -= n;
		buf += n;
		va = va0 + PGSIZE;
	}
	return 0;
}

