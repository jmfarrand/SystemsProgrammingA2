// Mutual exclusion lock.
struct _Spinlock
{
	uint32_t			Locked;			// Is the lock held?

	// For debugging:
	char *				Name;			// Name of lock.
	Cpu *		Cpu;			// The cpu holding the lock.
	uint32_t			Pcs[10];		// The call stack (an array of program counters)
										// that locked the lock.
};

