/*
 * Test checkpoint/restart of unlinked files.
 * Currently expected to fail.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/syscall.h> /* SYS_getdents */

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* waitpid() and W* status macros */
#include <sys/wait.h>

#define LOG_FILE	"log.unlinked.dir"
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
	{ "label",		1, 0, 'l'},
	{ "num",		1, 0, 'n'},
	{0, 0, 0, 0},
};

void parse_args(int argc, char **argv)
{
	ckpt_op_num = num_labels - 1;
	ckpt_label = labels[ckpt_op_num];
	while (1) {
		char c;
		c = getopt_long(argc, argv, "LNhl:n:", long_options, NULL);
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

/*
 * A LABEL is a point in the program we can goto where it's interesting to
 * checkpoint. These enable us to have a set of labels that can be specified
 * on the commandline.
 */
int main (int argc, char **argv)
{
	const char *pathname = "trash";
	const char buffer[] = "hello world!\n";
	char fdcontents[sizeof(buffer)];
	int fd, ret, op_num = 0;

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

label(mkdir,  ret, mkdir(pathname, S_IRWXU));
label(open,    fd, open(pathname, O_RDONLY|O_DIRECTORY));
label(rmdir,  ret, rmdir(pathname));
label(getdent,ret, syscall(SYS_getdents, fd, buffer, sizeof(buffer)));
label(close,  ret, close(fd));

	if (strcmp(buffer, fdcontents) != 0) {
		log("FAIL", "contents don't match.");
		ret = EXIT_FAILURE;
	} else
		ret = EXIT_SUCCESS;
out:
	close(fd);
	fclose(logfp);
	exit(ret);
}
