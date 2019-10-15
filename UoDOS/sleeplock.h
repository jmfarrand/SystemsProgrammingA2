// Long-term locks for processes
struct _Sleeplock 
{
  uint32_t	Locked;		// Is the lock held?
  Spinlock	Spinlock;	// spinlock protecting this sleep lock
  
  // For debugging:
  char *	Name;		// Name of lock.
  int		Pid;		// Process holding lock
};

