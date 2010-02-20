/*
 * Copyright 2009 IBM Corp.
 * Author: Matt Helsley <matthltc@us.ibm.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h> /* also for dnotify definitions like DN_* */
#include <signal.h>
#include <dirent.h> /* for dirfd */
#include <assert.h>
#include <getopt.h>
#include <poll.h> /* POLL* definitions */
#include <string.h>
#include <errno.h>

#include <sys/stat.h> /* for mkdir */
#include <sys/types.h>

#include "libcrtest/libcrtest.h"
#include "libcrtest/labels.h"

#define LOG_FILE	"log.dnotify"
extern FILE *logfp;

static void usage(FILE *pout)
{
	fprintf(pout, "\ndnotify [-L] [-N] [-h|--help] [-l LABEL] [-n NUM] [-f DIR]\n"
"Create and monitor a directory with dnotify.\n"
"\n"
"\t-L\tPrint the valid LABELs in order and exit.\n"
"\t-l\tWait for checkpoint at LABEL.\n"
"\t-N\tPrint the maximum label number and exit.\n"
"\t-n\tWait for checkpoint at NUM.\n"
"\t-f\tUse cgroup specified by DIR to freeze.\n"
"\n"
"You may only specify one LABEL or NUM and you may not specify both.\n"
"Label numbers are integers in the range 0-%d\n"
"Valid label numbers and their corresponding LABELs are:\n", num_labels - 1);
	print_labels(pout);
}

const struct option long_options[] = {
	{ "print-labels",	0, 0, 'L'},
	{ "print-max-label-no",	0, 0, 'N'},
	{ "help",		0, 0, 'h'},
	{ "label",		1, 0, 'l'},
	{ "num",		1, 0, 'n'},
	{ "freezer",		1, 0, 'f'},
	{0, 0, 0, 0},
};

static char *freezer = "1";

static void parse_args(int argc, char **argv)
{
	ckpt_op_num = num_labels - 1;
	ckpt_label = labels[ckpt_op_num];
	while (1) {
		int c;
		c = getopt_long(argc, argv, "f:LNhl:n:", long_options, NULL);
		if (c == -1)
			break;
		switch(c) {
		case 'f':
			freezer = optarg;
			printf("Will enter freezer cgroup %s\n", freezer);
			break;
		case 'L':
			print_labels(stdout);
			exit(EXIT_SUCCESS);
			break;
		case 'N':
			printf("%d\n", num_labels - 1);
			exit(EXIT_SUCCESS);
			break;
		case 'h':
			usage(stdout);
			exit(EXIT_SUCCESS);
			break;
		case 'l':
			ckpt_label = optarg;
			break;
		case 'n':
			if ((sscanf(optarg, "%d", &ckpt_op_num) < 1) ||
			    (ckpt_op_num < 0) ||
			    (ckpt_op_num >= num_labels)) {
				fprintf(stderr, "Option -n requires an argument in the range 0-%d. Got %d\n", num_labels - 1, ckpt_op_num);
				usage(stderr);
				exit(EXIT_FAILURE);
			}
			break;
		default: /* unknown option */
			break;
		}
	}
}

#define eassert(cond) do { \
	typeof(cond) ____cond = (cond); \
	if (!____cond) \
		perror(stringify(cond)); \
	assert(!!____cond); \
} while(0)

#define DN_ALL (DN_CREATE|DN_DELETE|DN_RENAME|DN_ATTRIB|DN_MODIFY|DN_ACCESS)
static int watchfd = -1;

static void dnotify_sigio(int signo, siginfo_t *info, void *ucontext)
{
	assert(signo == SIGIO);
	assert(info->si_signo == SIGIO);
/*	assert(info->si_fd == watchfd);
	assert(info->si_code == POLLMSG);
	assert(info->si_band == (POLLIN|POLLRDNORM|POLLMSG));*/
	/* TODO find a way to double-check dn_events ?? siginfo doesn't give enough */ \
}

#define dn_expect_events(dn_events) do { \
	struct timespec __timeout = { \
		.tv_sec = 1, \
		.tv_nsec = 0 \
	}; \
	siginfo_t __info; \
label(dn_events ## _sigio_wait, ret, sigtimedwait(&sigio, &__info, &__timeout)); \
	dnotify_sigio(ret, &__info, NULL); \
} while (0)

int main (int argc, char **argv)
{
	const char *writ_contents = "hello world";
	char contents[16];
	sigset_t sigio;
	DIR *dir;
	int ret = 0, fd = -1;
	int op_num = 0;

	parse_args(argc, argv);

	/* FIXME eventually stdio streams should be harmless */
	close(0);
	logfp = fopen(LOG_FILE, "w+");
	if (!logfp) {
		perror("could not open logfile");
		exit(1);
	}
	/* redirect stdout and stderr to the log file */
	if (dup2(fileno(logfp), 1) < 0) {
		fprintf(logfp, "dup2(logfp, 1)\n");
		goto out;
	}
	if (dup2(fileno(logfp), 2) < 0) {
		fprintf(logfp, "dup2(logfp, 2)\n");
		goto out;
	}
	if (!move_to_cgroup("freezer", freezer, getpid())) {
		fprintf(logfp, "move_to_cgroup\n");
		goto out;
	}
	printf("entered cgroup %s\n", freezer);
	ret = mkdir("./dwatch", S_IRWXU);
	eassert(ret == 0);
	dir = opendir("./dwatch");
	eassert(dir);
	watchfd = dirfd(dir);
	eassert(watchfd >= 0);

	/* Block SIGIO -- handle it synchronously with dn_expect_events() */
	ret = sigemptyset(&sigio);
	eassert(ret == 0);
	ret = sigaddset(&sigio, SIGIO);
	eassert(ret == 0);
	ret = sigprocmask(SIG_BLOCK, &sigio, NULL);
	eassert(ret == 0);

label(watch_create, ret, fcntl(watchfd, F_NOTIFY, DN_CREATE));
label(do_create, ret, open("./dwatch/create", O_CREAT|O_EXCL|O_RDWR));
	fd = ret;
	assert(fd != watchfd);
	assert(fd >= 0);
	dn_expect_events(DN_CREATE);

label(watch_modify, ret, fcntl(watchfd, F_NOTIFY, DN_MODIFY));
label(do_modify, ret, write(fd, writ_contents, strlen(writ_contents) + 1));
	assert(ret == (strlen(writ_contents) + 1));
	dn_expect_events(DN_MODIFY);

	ret = lseek(fd, 0, SEEK_SET);
	assert(ret == 0);
label(watch_access, ret, fcntl(watchfd, F_NOTIFY, DN_ACCESS));
label(do_access, ret, read(fd, contents, strlen(writ_contents) + 1));
	assert(ret == (strlen(writ_contents) + 1));
	assert(strcmp(contents, writ_contents) == 0);
	dn_expect_events(DN_ACCESS);

label(watch_attrib, ret, fcntl(watchfd, F_NOTIFY, DN_ATTRIB));
label(do_attrib, ret, fchmod(fd, S_IRUSR));
	assert(ret == 0);
	ret = close(fd);
	assert(ret == 0);
	fd = 0;
	dn_expect_events(DN_ATTRIB);

label(watch_rename, ret, fcntl(watchfd, F_NOTIFY, DN_RENAME));
label(do_rename, ret, rename("./dwatch/create", "./dwatch/rename"));
	assert(ret == 0);
	dn_expect_events(DN_RENAME);

label(watch_delete, ret, fcntl(watchfd, F_NOTIFY, DN_DELETE|DN_MULTISHOT));
label(do_delete, ret, unlink("./dwatch/rename"));
	assert(ret == 0);
	dn_expect_events(DN_DELETE);
label(do_rmdir, ret, rmdir("./dwatch"));
	assert(ret == 0);
	ret = EXIT_SUCCESS;
out:
	fprintf(logfp, "\nerrno: %s\n", strerror(errno));
	if (fd >= 0)
		close(fd);
	closedir(dir);
	fclose(logfp);
	if (ret != EXIT_SUCCESS)
		ret = EXIT_FAILURE;
	exit(ret);
}

