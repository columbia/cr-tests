/*
 * Copyright 2008 IBM Corp.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "clone.h"

extern pid_t getpgid(pid_t pid);
extern pid_t getsid(pid_t pid);

static const char* procname;

static void usage(const char *name)
{
	printf("usage: %s [-h] [-c] [-muip]"
			"[command [arg ..]]\n", name);
	printf("\n");
	printf("  -h		this message\n");
	printf("\n");
	printf("  -c		use 'clone' rather than 'unshare' system call\n");
	printf("  -m		mount namespace\n");
	printf("  -u		utsname namespace\n");
	printf("  -i		ipc namespace\n");
	printf("  -p		pid namespace\n");
	printf("  -f <flag>	extra clone flags\n");
	printf("\n");
	printf("(C) Copyright IBM Corp. 2006\n");
	printf("\n");
	exit(1);
}

static void print_my_info(const char *procname, char *ttyname)
{
	printf("procname %s, ttyname %s, pid %d, ppid %d, pgid %d, sid %d\n",
			procname, ttyname, getpid(), getppid(), getpgid(0),
			getsid(0));
}

static int string_to_ul(const char *str, unsigned long int *res)
{
	char *tail;
	long long int r;

	if (!*str)
		return -1;

	errno = 0;

	r = strtol(str, &tail, 16);

	/*
	 * according to strtol(3), if errno is set or tail does no point
	 * to the ending '\0', the conversion failed.
	 */
	if (errno || *tail)
		return -1;

	*res = r;
	return 0;
}

/*
 * Copied following opentty() from Fedora's util-linux rpm
 * I just changed the "FATAL" message below from syslog()
 * to printf
 */
static void
opentty(const char * tty) {
        int i, fd, flags;

        fd = open(tty, O_RDWR | O_NONBLOCK);
        if (fd == -1) {
		printf("FATAL: can't reopen tty: %s", strerror(errno));
                sleep(1);
                exit(1);
        }

        flags = fcntl(fd, F_GETFL);
        flags &= ~O_NONBLOCK;
        fcntl(fd, F_SETFL, flags);

        for (i = 0; i < fd; i++)
                close(i);
        for (i = 0; i < 3; i++)
                if (fd != i)
                        dup2(fd, i);
        if (fd >= 3)
                close(fd);
}
// Code copy end

int do_child(void *vargv)
{
	char **argv = (char **)vargv;
	execve(argv[0], argv, __environ);
	perror("execve");
	return 1;
}

int main(int argc, char *argv[])
{	
	int c;
	unsigned long flags = 0, eflags = 0;
	char ttyname[256];
	int status;
	int ret, use_clone = 0;
	int pid;

	procname = basename(argv[0]);

	memset(ttyname, '\0', sizeof(ttyname));
	readlink("/proc/self/fd/0", ttyname, sizeof(ttyname));

	while ((c = getopt(argc, argv, "+muUiphcnf:")) != EOF) {
		switch (c) {
		case 'm': flags |= CLONE_NEWNS;  break;
		case 'c': use_clone = 1; break;
		case 'u': flags |= CLONE_NEWUTS;		break;
		case 'i': flags |= CLONE_NEWIPC;		break;
		case 'U': flags |= CLONE_NEWUSER;		break;
		case 'n': flags |= CLONE_NEWNET;		break;
		case 'p': flags |= CLONE_NEWNS|CLONE_NEWPID;	break;
		case 'f': if (!string_to_ul(optarg, &eflags)) {
				flags |= eflags;
				break;
			}
		case 'h':
		default:
			usage(procname);
		}
	};

	argv = &argv[optind];
	argc = argc - optind;	
	
	if (use_clone) {
		int stacksize = 4*getpagesize();
		void *childstack, *stack = malloc(stacksize);

		if (!stack) {
			perror("malloc");
			return -1;
		}
		childstack = stack + stacksize;

		printf("about to clone with %lx\n", flags);
		pid = clone(do_child, childstack, flags, (void *)argv);
		if (pid == -1) {
			perror("clone");
			return -1;
		}
	} else {
		if ((pid = fork()) == 0) {
			// Child.
			//print_my_info(procname, ttyname);

			opentty(ttyname);

			printf("about to unshare with %lx\n", flags);
			ret = unshare(flags);
			if (ret < 0) {
				perror("unshare");
				return 1;
			}		
			
			return do_child((void*)argv);
		}

	}
	if ((ret = waitpid(pid, &status, __WALL)) < 0)
		printf("waitpid() returns %d, errno %d\n", ret, errno);

	exit(0);
}
