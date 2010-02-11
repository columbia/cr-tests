/*
 * Open a pipe and test whether epoll retrieves events related to IO on the
 * pipe.
 *
 * Usage:
 *         epoll_pipe [-L|-l LABEL] [--help] [-n NUM]
 * -L - Print the valid LABELs in order and exit.
 * -l - Wait for checkpoint at LABEL.
 * -N - Print the maximum label number and exit.
 * -n - Wait for checkpoint at NUM.
 *
 *  You may only specify one LABEL or NUM and you may not specify both.
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

#define LOG_FILE	"log.pipe"

void usage(FILE *pout)
{
	fprintf(pout, "\npipe [-L] [-N] [-h|--help] [-l LABEL] [-n NUM]\n"
"Create an epoll set and use it to wait for IO on a pipe.\n"
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

int main(int argc, char **argv)
{
	struct epoll_event ev[2] = {
		{ .events = EPOLLIN,  },
		{ .events = EPOLLOUT, },
	};
	int op_num = 0;
	int tube[2];
	int efd;
	int ec = EXIT_FAILURE;
	int ret;
	char rbuf[128];

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

label(create,
	efd, epoll_create(1));

	ret = pipe(tube);
	if (ret < 0)
		goto out;
	ev[0].data.fd = tube[0];
	ev[1].data.fd = tube[1];

label(ctl_add_wfd,
	ret, epoll_ctl(efd, EPOLL_CTL_ADD, tube[1], &ev[1]));

label(wait_write,
	ret, epoll_wait(efd, &ev[1], 1, 1000));
	if (ret != 1) {
		log_error("Expected epoll_wait() to return one event.\n");
		goto out;
	}
	if (!(ev[1].events & EPOLLOUT)) {
		log("FAIL", "Expected EPOLLOUT (0x%X) flag, got %s (0x%X)\n", 
			  EPOLLOUT, eflags(ev[1].events), ev[1].events);
		goto out;
	}
	if (tube[1] != ev[1].data.fd) {
		log("FAIL", "Expected fd %d, got %d\n", tube[1], ev[1].data.fd);
		goto out;
	}

label(do_write,
	ret, write(tube[1], HELLO, strlen(HELLO) + 1));
	if (ret < (strlen(HELLO) + 1)) {
		log("FAIL", "Unable to write all %zu bytes of \"%s\"\n",
			 strlen(HELLO) + 1, HELLO);
		goto out;
	}

label(ctl_add_rfd,
	ret, epoll_ctl(efd, EPOLL_CTL_ADD, tube[0], &ev[0]));

label(ctl_rm_wfd,
	ret, epoll_ctl(efd, EPOLL_CTL_DEL, tube[1], &ev[1]));

label(wait_read,
	ret, epoll_wait(efd, &ev[0], 1, 5000));
	if (ret != 1) {
		log_error("Expected epoll_wait() to return one event.\n");
		goto out;
	}
	if (!(ev[0].events & EPOLLIN)) {
		log("FAIL", "Expected EPOLLIN (0x%X) flag, got %s (0x%X)\n", 
			  EPOLLIN, eflags(ev[0].events), ev[0].events);
		goto out;
	}
	if (tube[0] != ev[0].data.fd) {
		log("FAIL", "Expected fd %d, got %d\n", tube[0], ev[0].data.fd);
		goto out;
	}

label(do_read,
	ret, read(tube[0], rbuf, strlen(HELLO) + 1));
	if (ret < (strlen(HELLO) + 1)) {
		log("FAIL", "Unable to read all %zu bytes of \"%s\"\n",
			 strlen(HELLO) + 1, HELLO);
		goto out;
	}
	log("INFO", "read len ok\n");
	if (strcmp(HELLO, rbuf)) {
		log("FAIL", "Pipe buffer was corrupted. Expected: \"%s\" Got: \"%s\"\n",
			 HELLO, rbuf);
		goto out;
	}
	log("INFO", "read buffer contents ok\n");
	ec = EXIT_SUCCESS;
	op_num = INT_MAX;

out:
	if ((efd >= 0) && close(efd) < 0) {
		log_error("close()");
		efd = -1;
		goto out;
	}
	if (op_num != INT_MAX) {
		log("FAIL", "error at label %s (op num: %d)\n",
			  labels[op_num], op_num);
	}
	close(tube[0]);
	close(tube[1]);
	fflush(logfp);
	fclose(logfp);
	exit(ec);
}
