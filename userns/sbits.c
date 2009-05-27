/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 * securebits test
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

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
	printf("Usage: %s [-k] [-r] [-l]\n", me);
	printf("  -k: set keepcaps\n");
	printf("  -r: set SECURE_NOROOT and SECURE_NO_SETUID_FIXUP\n");
	printf("  -l: lock the -r state\n");
	exit(1);
}

int move_to_cgroup_1(void)
{
	FILE *fout = fopen("/cgroup/1/tasks", "w");
	if (!fout)
		return 1;
	fprintf(fout, "%d\n", getpid());
	fclose(fout);
	return 0;
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
	FILE *fout;
	int c;

	if (move_to_cgroup_1()) {
		printf("Failed to move myself to cgroup /1\n");
		exit(1);
	}

	while ((c = getopt(argc, argv, "krl")) != -1) {
		switch(c) {
		case 'k': keepcaps = 1; break;
		case 'r': noroot = 1; break;
		case 'l': locked = 1; break;
		default:
			usage(argv[0]);
		}
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

	/* sleep once to let us be queried and checkpointed */
	sleep(3);

	/* sleep once to let restarted task be queried */
	bits = prctl(PR_GET_SECUREBITS);
	fout = fopen("outfile", "a");
	fprintf(fout, "(%d) After checkpoint: %d\n", getpid(), bits);
	fflush(fout);
	fclose(fout);

	sleep(6);
	exit(0);
}
