/*
 * Test checkpoint/restart of unlinked fifos. fifos are an interesting
 * case for unlinked file support because they offer limited buffer space
 * but are not seekable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* set inode flags */
#include <sys/ioctl.h>
#include <linux/fs.h>

/* waitpid() and W* status macros */
#include <sys/wait.h>

#define LOG_FILE	"log.unlinked.file"
#include "libfstest.h"

const char *descr = "Create an unlinked file to checkpoint/restore.";

void usage(FILE *pout, const char *prog)
{
	fprintf(pout, "\n%s [-L] [-N] [-h|--help] [-l LABEL] [-n NUM]\n"
"%s\n"
"\n"
"\t-L\tPrint the valid LABELs in order and exit.\n"
"\t-l\tWait for checkpoint at LABEL.\n"
"\t-N\tPrint the maximum label number and exit.\n"
"\t-n\tWait for checkpoint at NUM.\n"
"\t-a\tSet the append-only flag (extX filesystems, needs cap_linux_immutable).\n"
"\t-i\tSet the immutable flag (extX filesystems, needs cap_linux_immutable).\n"
"\t-u\tSet the undeleteable (recoverable) flag (extX filesystems).\n"
"\n"
"You may only specify one LABEL or NUM and you may not specify both.\n"
"Label numbers are integers in the range 0-%d\n"
"Valid label numbers and their corresponding LABELs are:\n", prog,
		descr, num_labels - 1);
	print_labels(pout);
}

const struct option long_options[] = {
	{ "print-labels",	0, 0, 'L'},
	{ "print-max-label-no",	0, 0, 'N'},
	{ "help",		0, 0, 'h'},
	{ "append",		0, 0, 'a'}, /* Need CAP_LINUX_IMMUTABLE */
	{ "immutable",		0, 0, 'i'}, /* Need CAP_LINUX_IMMUTABLE */
	{ "undelete",		0, 0, 'u'}, /* unsupported */
	{ "label",		1, 0, 'l'},
	{ "num",		1, 0, 'n'},
	{0, 0, 0, 0},
};

static int inode_flags = 0;
static int oflags = O_RDWR; /* rely on Linux-specific behavior (man 7 fifo) */

void parse_args(int argc, char **argv)
{
	ckpt_op_num = num_labels - 1;
	ckpt_label = labels[ckpt_op_num];
	while (1) {
		char c;
		c = getopt_long(argc, argv, "LNhl:n:aiu", long_options, NULL);
		if (c == -1)
			break;
		switch(c) {
			case 'L':
				print_labels(stdout);
				exit(EXIT_SUCCESS);
				break;
			case 'N':
				printf("%d\n", num_labels - 1);
				exit(EXIT_SUCCESS);
				break;
			case 'h':
				usage(stdout, argv[0]);
				exit(EXIT_SUCCESS);
				break;
			case 'a':
				oflags |= O_APPEND;
				inode_flags |= FS_APPEND_FL;
				break;
			case 'i':
				inode_flags |= FS_IMMUTABLE_FL;
				break;
			case 'u':
				inode_flags |= FS_UNRM_FL;
				break;
			case 'l':
				ckpt_label = optarg;
				break;
			case 'n':
				if ((sscanf(optarg, "%d", &ckpt_op_num) < 1) ||
				    (ckpt_op_num < 0) ||
				    (ckpt_op_num >= num_labels)) {
					fprintf(stderr, "Option -n requires an argument in the range 0-%d. Got %d\n", num_labels - 1, ckpt_op_num);
					usage(stderr, argv[0]);
					exit(EXIT_FAILURE);
				}
				break;
			default: /* unknown option */
				break;
		}
	}
}

static int setflags(int fd)
{
	int flags = inode_flags;

	if (!flags)
		return 0;
#ifdef FS_IOC_SETFLAGS
	return ioctl(fd, FS_IOC_SETFLAGS, &flags);
#else
	errno = ENOTSUP;
	return -1;
#endif
}

int main (int argc, char **argv)
{
	const char *pathname = "trash";
	const char buffer1[] = "hello world!\n";
	const char buffer2[] = "goodbye old world, hello new world!\n";
	char fdcontents1[sizeof(buffer1)];
	char fdcontents2[sizeof(buffer2)];
	int fifo1 = -1, fifo2 = -1, ret, op_num = 0;

	parse_args(argc, argv);
	/* FIXME eventually stdio streams should be harmless */
	close(0);
	logfp = fopen(LOG_FILE, "w");
	if (!logfp) {
		perror("could not open logfile");
		exit(1);
	}
	dup2(fileno(logfp), 1); /* redirect stdout and stderr to the log file */
	dup2(fileno(logfp), 2);
	if (!move_to_cgroup("freezer", "1", getpid())) {
		log_error("move_to_cgroup");
		exit(2);
	}

/* Open a first fifo, write to it, and unlink it */
label(mkfifo1,   ret, mkfifo(pathname, S_IRUSR|S_IWUSR));
label(open1,   fifo1, open(pathname, oflags));
label(setflags1, ret, setflags(fifo1));
label(write1,    ret, write(fifo1, buffer1, sizeof(buffer1)));
label(unlink,    ret, unlink(pathname));

/* Open a second fifo, write to it */
label(mkfifo2,   ret, mkfifo(pathname, S_IRUSR|S_IWUSR));
label(open2,   fifo2, open(pathname, oflags));
label(setflags2, ret, setflags(fifo2));
label(write2,    ret, write(fifo2, buffer2, sizeof(buffer2)));

/* Check fifo contents */
label(read1,     ret, read(fifo1, fdcontents1, sizeof(fdcontents1)));
	if (strcmp(buffer1, fdcontents1) != 0) {
		log("FAIL", "original fifo contents don't match.");
		ret = EXIT_FAILURE;
		goto out;
	}
label(read2,     ret, read(fifo2, fdcontents2, sizeof(fdcontents2)));
	if (strcmp(buffer2, fdcontents2) != 0) {
		log("FAIL", "new fifo contents don't match.");
		ret = EXIT_FAILURE;
		goto out;
	}
	ret = EXIT_SUCCESS;
out:
	if (ret != EXIT_SUCCESS)
		perror("ERROR");
	if (fifo1 > -1)
		close(fifo1);
	if (fifo2 > -1)
		close(fifo2);
	unlink(pathname);
	fclose(logfp);
	exit(ret);
}
