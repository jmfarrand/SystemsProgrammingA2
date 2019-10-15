struct _DiskBuffer;
struct _Context;
struct _File;
struct _Device;
struct _Pipe;
struct _Process;
struct _RtcDate;
struct _Spinlock;
struct _Sleeplock;
struct _Stat;
struct _DirectoryEntry;
struct _MountInfo;
struct _Cpu;

typedef struct _DiskBuffer		DiskBuffer;
typedef struct _Context			Context;
typedef struct _File			File;
typedef struct _Device			Device;
typedef struct _Pipe			Pipe;
typedef struct _Process			Process;
typedef struct _RtcDate			RtcDate;
typedef struct _Spinlock		Spinlock;
typedef struct _Sleeplock		Sleeplock;
typedef struct _Stat			Stat;
typedef struct _DirectoryEntry	DirectoryEntry;
typedef struct _MountInfo		MountInfo;
typedef struct _Cpu				Cpu;

// bio.c
void						diskBufferCacheInitialise(void);
DiskBuffer*					diskBufferRead(uint32_t, uint32_t);
void						diskBufferRelease(DiskBuffer*);
void						diskBufferWrite(DiskBuffer*);

// console.c
void						consoleInitialise(void);
void						cprintf(char*, ...);
void						consoleInterrupt(int(*)(void));
void						panic(char*) __attribute__((noreturn));

// exec.c
int							exec(char*, char**);

// file.c
File*						allocateFileStructure(void);
void						fileClose(File*);
File*						fileDup(File*);
void						filesInitialise(void);
int							fileRead(File*, char*, int n);
int							fileStat(File*, Stat*);
int							fileWrite(File*, char*, int n);

// fs.c
void						fsFat12Initialise(void);
uint32_t					fsFat12Read(File *, unsigned char *, unsigned int);
void						fsFat12Close(File *);
File	*					fsFat12Open(const char *, const char *, int);

// ide.c
void						ideInitialise(void);
void						ideInterruptHandler(void);
void						ideReadWrite(DiskBuffer*);

// ioApic.c
void						ioApicEnable(int irq, int cpu);
extern uint8_t				ioapicid;
void						ioApicInitialise(void);

// kalloc.c
char*						allocatePhysicalMemoryPage(void);
void						freePhysicalMemoryPage(char*);
void						initialiseLowerkernelMemory(void*, void*);
void						initialiseRestOfkernelMemory(void*, void*);

// kbd.c
void						keyboardInterrupt(void);

// lapic.c
void						cmosTime(RtcDate *r);
int							localApicId(void);
extern volatile uint32_t*	localApic;
void						localApicEndOfInterrupt(void);
void						localApicInitialise(void);
void						localApicStartup(uint8_t, uint32_t);
void						microDelay(int);

// mp.c
extern int					ismp;
void						mpinit(void);

// picirq.c
void						picInitialise(void);

// pipe.c
int							pipealloc(File**, File**);
void						pipeclose(Pipe*, int);
int							piperead(Pipe*, char*, int);
int							pipewrite(Pipe*, char*, int);

// Process.c
int							cpuId(void);
void						exit(void);
int							fork(void);
int							growProcess(int);
int							kill(int);
Cpu*						myCpu(void);
Process*					myProcess();
void						processTableInitialise(void);
void						processDump(void);
void						scheduler(void) __attribute__((noreturn));
void						sched(void);
void						sleep(void*, Spinlock*);
void						initialiseFirstUserProcess(void);
int							wait(void);
void						wakeup(void*);
void						yield(void);

// swtch.asm
void						swtch(Context**, Context*);

// spinlock.c
void						spinlockAcquire(Spinlock*);
void						getProcessCallStack(void*, uint32_t*);
int							isHolding(Spinlock*);
void						spinlockInitialise(Spinlock*, char*);
void						spinlockRelease(Spinlock*);
void						pushCli(void);
void						popCli(void);

// sleeplock.c
void						sleeplockAcquire(Sleeplock*);
void						sleeplockRelease(Sleeplock*);
int							isHoldingSleeplock(Sleeplock*);
void						sleeplockInitialise(Sleeplock*, char*);

// string.c
int							memcmp(const void*, const void*, uint32_t);
void*						memmove(void*, const void*, uint32_t);
void*						memset(void*, int, uint32_t);
char*						safestrcpy(char*, const char*, int);
int							strlen(const char*);
int							strncmp(const char*, const char*, uint32_t);
char*						strncpy(char*, const char*, int);
int							strcmp(const char*, const char*);
char *						strcpy(char *, const char *);
char*						strchr(char *, int);

// syscall.c
int							argint(int, int*);
int							argptr(int, char**, int);
int							argstr(int, char**);
int							fetchint(uint32_t, int*);
int							fetchstr(uint32_t, char**);
void						syscall(void);

// timer.c
void						timerinit(void);

// trap.c
void						interruptDescriptorTableInitialise(void);
extern uint32_t				ticks;
void						trapVectorsInitialise(void);
extern Spinlock				tickslock;

// uart.c
void						uartinit(void);
void						uartintr(void);
void						uartputc(int);

// vm.c
void						initialiseGDT(void);
void						allocateKernelVirtualMemory(void);
pde_t*						setupKernelVirtualMemory(void);
char*						mapVirtualAddressToKernelAddress(pde_t*, char*);
int							allocateMemoryAndPageTables(pde_t*, uint32_t, uint32_t);
int							releaseUserPages(pde_t*, uint32_t, uint32_t);
void						freeMemoryAndPageTable(pde_t*);
void						initialiseUserVirtualMemory(pde_t*, char*, uint32_t);
int							loadProgramSegmentIntoPageTable(pde_t*, char*, File*, uint32_t, uint32_t);
pde_t*						copyProcessPageTable(pde_t*, uint32_t);
void						switchToUserVirtualMemory(Process*);
void						switchToKernelVirtualMemory(void);
int							copyToUserVirtualMemory(pde_t*, uint32_t, void*, uint32_t);
void						clearPTEU(pde_t *pgdir, char *uva);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))
