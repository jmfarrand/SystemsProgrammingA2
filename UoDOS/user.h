struct _Stat;

// System calls.  If you add any new system calls to UoDOS, the signature of the calls for
// user programs should be added here, as well as adding them to syscalls.pl.
//
// This section is important since these declarations are used by the C compiler when
// generating calls to system calls to ensure that parameters are placed on the stack 
// correctly.

int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char*, int);
int fstat(int fd, struct _Stat*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
//stage 1 systems calls
int chdir(char *directory);
int getcwd(char *currentDirectory, int sizeOfBuffer);

// The following are C standard library functions implemented in our
// equivalent of the C run-time library

// ulib.c
int stat(char*, struct _Stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
char* gets(char*, int max);
uint32_t strlen(char*);
void* memset(void*, int, uint32_t);
void* malloc(uint32_t);
void free(void*);
int atoi(const char*);

// printf.c
void printf(char*, ...);
