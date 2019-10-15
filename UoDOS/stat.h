#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

struct _Stat 
{
  short		type;	// Type of file
  int		dev;    // File system's disk device
  uint32_t	ino;    // Inode number
  short		nlink;	// Number of links to file
  uint32_t	size;   // Size of file in bytes
};
