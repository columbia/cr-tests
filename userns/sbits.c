/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 * securebits test
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <libcrtest.h>

#ifndef PR_GET_SECUREBITS
#define PR_GET_SECUREBITS 27
#endif

#ifndef PR_SET_SECUREBITS
#define PR_SET_SECUREBITS 28
#endif

#ifndef SECURE_NOROOT
#define SECURE_NOROOT                   (1 << 0)
#define SECURE_NOROOT_LOCKED            (1 << 1)  /* make bit-0 immutable */
#define SECURE_NO_SETUID_FIXUP          (1 << 2)
#define SECURE_NO_SETUID_FIXUP_LOCKED   (1 << 3)  /* make bit-2 immutable */
#endif
#define NOROOT (SECURE_NOROOT | SECURE_NO_SETUID_FIXUP)
#define NOROOT_LOCK (SECURE_NOROOT_LOCKED | SECURE_NO_SETUID_FIXUP_LOCKED)

void usage(char *me)
{
	printf("Usage: %s [-f name] [-k] [-r] [-l]\n", me);
	printf("  -k: set keepcaps\n");
	printf("  -r: set SECURE_NOROOT and SECURE_NO_SETUID_FIXUP\n");
	printf("  -l: lock the -r state\n");
	printf("  -f <name>: freezer cgroup to enter ('1' by default)\n");
	exit(1);
}

void wait_on_checkpoint(void)
{
	struct stat statbuf;
	int ret;

	while (1) {
		ret = stat("checkpointed", &statbuf);
		if (ret == 0)
			return;
	}
}

/*
 * call with -k to set pr_keepcaps
 * call with -r to set secure_noroot and secure_nosetuid_fixup
 * call with no args to set none of those
 */
int main(int argc, char *argv[])
{
	int keepcaps = 0, noroot = 0, locked = 0;
	int bits;
	char *freezer = "1";
	FILE *fout;
	int c;

	while ((c = getopt(argc, argv, "krlf:")) != -1) {
		switch(c) {
		case 'k': keepcaps = 1; break;
		case 'r': noroot = 1; break;
		case 'l': locked = 1; break;
		case 'f': freezer = optarg; break;
		default:
			usage(argv[0]);
		}
	}

	if (!move_to_cgroup("freezer", freezer, getpid())) {
		printf("Failed to move myself to cgroup /1\n");
		exit(1);
	}

	bits = prctl(PR_GET_SECUREBITS);
	fout = fopen("outfile", "a");
	fprintf(fout, "(%d) Before setting securebits: %d\n", getpid(), bits);
	fflush(fout);
	fclose(fout);
	if (keepcaps) {
		if (prctl(PR_SET_KEEPCAPS, 1) == -1) {
			perror("prctl PR_SET_KEEPCAPS");
			usage(argv[0]);
		}
	}
	if (noroot) {
		bits = NOROOT;
		if (locked) bits |= NOROOT_LOCK;
		if (prctl(PR_SET_SECUREBITS, bits) == -1) {
			perror("prctl PR_SET_SECUREBITS");
			usage(argv[0]);
		}
	}

	close(0);
	close(1);
	close(2);
	close(3);

	bits = prctl(PR_GET_SECUREBITS);
	fout = fopen("outfile", "a");
	fprintf(fout, "(%d) Before checkpoint: %d\n", getpid(), bits);
	fflush(fout);
	fclose(fout);

	creat("started", 0755);
	wait_on_checkpoint();

	/* sleep once to let restarted task be queried */
	bits = prctl(PR_GET_SECUREBITS);
	fout = fopen("outfile", "a");
	fprintf(fout, "(%d) After checkpoint: %d\n", getpid(), bits);
	fflush(fout);
	fclose(fout);

	creat("finished", 0755);
	exit(0);
}
