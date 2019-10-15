enum FileType 
{ 
	FD_NONE, 
	FD_PIPE, 
	FD_FILE, 
	FD_DIR, 
	FD_DEVICE 
};

struct _File 
{
  enum FileType			 Type;
  int					 ReferenceCount; 
  char					 Readable;
  char					 Writable;
  Pipe *				 Pipe;
  DirectoryEntry		 DirectoryEntry;
  char					 Name[256];
  uint32_t				 Eof;
  uint32_t				 Position;
  uint32_t				 Size;
  uint32_t				 DeviceID;
};

struct _Device
{
	int(*read)(File *, char*, int);
	int(*write)(File *, char*, int);
};

#define CONSOLE 0
