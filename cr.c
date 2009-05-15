/* 
 * Copyright (C) 2008 Oren Laadan
 * Changelog:
 *	Early 2009: Serge: tweak cmdline a bit
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cr.h"

int main(int argc, char *argv[])
{
       pid_t pid;
       int ret;
       int outf;

       if (argc != 3) {
               printf("usage: cr PID outfile\n");
               exit(1);
       }
  
       pid = atoi(argv[1]);
       if (pid <= 0) {
               printf("invalid pid\n");
               exit(1);
       }

	unlink(argv[2]);
	outf = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
	if (outf == -1) {
		perror("open");
		exit(1);
	}

       ret = syscall(__NR_checkpoint, pid, outf, 4);

       if (ret < 0)
               perror("checkpoint");
       else
               printf("checkpoint id %d\n", ret);

       return (ret > 0 ? 0 : 1);
}
