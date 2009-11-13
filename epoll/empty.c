/*
 * Make sure epoll sets stay empty across c/r.
 *
 * epoll create
 * checkpoint
 * close epoll
 */
/* pretty standard stuff really */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* waitpid() and W* status macros */
#include <sys/wait.h>

#include "libeptest.h"

#define LOG_FILE	"log.empty"

void usage(FILE *pout)
{
	fprintf(pout, "\nempty [-L] [-N] [-h|--help] [-l LABEL] [-n NUM]\n"
"Create an empty epoll set.\n"
"\n"
"\t-L\tPrint the valid LABELs in order and exit.\n"
"\t-l\tWait for checkpoint at LABEL.\n"
"\t-N\tPrint the maximum label number and exit.\n"
"\t-n\tWait for checkpoint at NUM.\n"
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

char *freezer = "1";

void parse_args(int argc, char **argv)
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

int main (int argc, char **argv)
{
	int efd, ret = 0;
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
		log_error("dup2(logfp, 1)");
		goto out;
	}
	if (dup2(fileno(logfp), 2) < 0) {
		log_error("dup2(logfp, 2)");
		goto out;
	}
	if (!move_to_cgroup("freezer", freezer, getpid())) {
		log_error("move_to_cgroup");
		exit(2);
	}
	printf("entered cgroup %s\n", freezer);

label(create,
	efd, epoll_create(1));
	if (efd < 0) {
		perror("epoll_create(1)");
		fclose(logfp);
		exit(EXIT_FAILURE);
	}
label(do_nothing, ret, ret + 0);
out:
	if (close(efd) < 0) {
		perror("close()");
		fclose(logfp);
		exit(EXIT_FAILURE);
	}
	fclose(logfp);
	exit(EXIT_SUCCESS);
}
