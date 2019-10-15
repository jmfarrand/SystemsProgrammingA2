// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"

char *argv[] = { "sh.exe", 0 };

int
main(void)
{
	int pid, wpid;

	for (;;) 
	{
		printf("init: starting sh.exe\n");
		pid = fork();
		if (pid < 0) 
		{
			printf("init: fork failed\n");
			exit();
		}
		if (pid == 0) 
		{
			exec("/usrbin/sh.exe", argv);
			printf("init: exec sh.exe failed\n");
			exit();
		}
		while ((wpid = wait()) >= 0 && wpid != pid)
		{
			printf("zombie!\n");
		}
	}
}
