/*
 * Open a number of sockets, write sample content, seek to the beginning,
 * and verify that the writes completed. This test is meant to run with
 * thousands or more sockets open.
 *
 * Usage:
 *         sk10k [-L|-l LABEL] [--help] [-n NUM]
 * -L - Print the valid LABELs in order and exit.
 * -l - Wait for checkpoint at LABEL.
 * -N - Print the maximum label number and exit.
 * -n - Wait for checkpoint at NUM.
 * -h - help
 * -s - num socket pairs
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
#include <dirent.h> /* scandir() */

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* socketpair AF_UNIX */
#include <sys/socket.h>
#include <sys/un.h>

/* waitpid() and W* status macros */
#include <sys/wait.h>

#include "libeptest.h"

#define LOG_FILE	"log.sk10k"

void usage(FILE *pout)
{
	fprintf(pout, "\nsk10k [-L] [-N] [-h|--help] [-l LABEL] [-n NUM]\n"
"Create a large number of sockets, add them to an epoll set, and use epoll to\n"
"wait for IO on the sockets.\n"
"\n"
"\t-L\tPrint the valid LABELs in order and exit.\n"
"\t-l\tWait for checkpoint at LABEL.\n"
"\t-N\tPrint the maximum label number and exit.\n"
"\t-n\tWait for checkpoint at NUM.\n"
"\t-s\tNUM socket pairs to create. [Default: up to half of ulimit -n]\n"
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
	{ "num-skp",		1, 0, 's'},
	{ "freezer",		1, 0, 'f'},
	{0, 0, 0, 0},
};

int num_sk = 400;

void set_default_num_sk(void)
{
	struct rlimit lim;
	int num_fds_open = 0;
	struct dirent **dents;

	/*
	 * Get num_sk from hard rlimit. The goal of this default is to open
	 * enough sockets that we can't kmalloc() enough space for all the
	 * checkpointed epoll items at one time.
	 */
	getrlimit(RLIMIT_NOFILE, &lim);
	num_sk = lim.rlim_cur/2;

	num_fds_open = scandir("/proc/self/fd", &dents, 0, alphasort);
	if (num_fds_open < 0)
		perror("scandir");
	else {
		free(dents);
		num_fds_open -= 2;
		num_sk -= (num_fds_open + 1)/2;
	}
	num_sk &= ~1; /* round down to nearest multiple of 2 */

	/*
	 * Of course if we're running as root then we may have an
	 * insanely high rlimit -- so high that the test will never pass.
	 * Keep it reasonable enough to pass.
	 */
	if (num_sk > 1000000)
		num_sk = 1000000;
}

char *freezer = "1";

void parse_args(int argc, char **argv)
{
	ckpt_op_num = num_labels - 1;
	ckpt_label = labels[ckpt_op_num];

	set_default_num_sk();

	while (1) {
		int c;
		c = getopt_long(argc, argv, "f:LNhl:n:s:", long_options, NULL);
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
			case 's':
				if (sscanf(optarg, "%u", &num_sk) < 1) {
					fprintf(stderr, "Option -f requires an argument in the range 1-INT_MAX. Got %d\n", num_sk);
					usage(stderr);
					exit(EXIT_FAILURE);
				}
				num_sk *= 2; /* pairs of sockets */

				{
					/* rlimit restricts max sk */
					struct rlimit lim;
					getrlimit(RLIMIT_NOFILE, &lim);
					fprintf(stdout, "INFO: RLIMIT_NOFILE: soft (cur): %ld hard (max): %ld\n", lim.rlim_cur, lim.rlim_max);
					if (num_sk >= lim.rlim_cur) {
						fprintf(stderr, "WARN: process is restricted from opening %d sockets. Opening %ld instead.\n", num_sk, lim.rlim_cur);
						num_sk = lim.rlim_cur;
					}
				}
				break;
			default: /* unknown option */
				break;
		}
	}
}

int main(int argc, char **argv)
{
	char rbuf[128];
	struct epoll_event *evs = NULL;
	int *sk = NULL;
	int op_num = 0;
	int efd;
	int ec = EXIT_FAILURE;
	int ret = 0;
	int i;

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
	sk = malloc(num_sk*sizeof(sk[0]));
	if (!sk) {
		log_error("sk = malloc()");
		goto out;
	}
	evs = malloc(num_sk*sizeof(evs[0]));
	if (!evs) {
		log_error("evs = malloc()");
		goto out;
	}

	if (!move_to_cgroup("freezer", freezer, getpid())) {
		log_error("move_to_cgroup");
		exit(2);
	}

label(create,
	efd, epoll_create(num_sk));

label(open, ret, ret + 0);
	for (i = 0; i < num_sk; i+=2) {
		ret = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, &sk[i]);
		if (ret) {
			log_error("socketpair");
			goto out;
		}
		evs[i].data.fd = sk[i];
		evs[i].events = EPOLLOUT;
		evs[i + 1].data.fd = sk[i + 1];
		evs[i + 1].events = EPOLLOUT;
		ret = epoll_ctl(efd, EPOLL_CTL_ADD, sk[i], &evs[i]);
		if (ret < 0) {
			log("FAIL", "epoll_ctl(ADD, sk[%d])\n", i);
			goto out;
		}
		ret = epoll_ctl(efd, EPOLL_CTL_ADD, sk[i + 1], &evs[i + 1]);
		if (ret < 0) {
			log("FAIL", "epoll_ctl(ADD, sk[%d])\n", i);
			goto out;
		}
	}

label(wait_write,
	ret, epoll_wait(efd, evs, num_sk, 1000));
	if (ret != num_sk) {
		log_error("Expected epoll_wait() to return num_sk events.\n");
		goto out;
	}
	for (i = 0; i < num_sk; i++) {
		if (!(evs[i].events & EPOLLOUT)) {
			log("FAIL", "Expected EPOLLOUT (0x%X) flag, got %s (0x%X)\n", 
				  EPOLLOUT, eflags(evs[i].events), evs[i].events);
			goto out;
		}
	}
	for (i = 0; i < num_sk; i++) {
		ret = write(sk[i], HELLO, strlen(HELLO) + 1);
		if (ret < (strlen(HELLO) + 1)) {
			log("FAIL", "Unable to write all %zu bytes of \"%s\" to sk[%d] (%d)\n",
				 strlen(HELLO) + 1, HELLO, i, sk[i]);
			goto out;
		}
	}

label(mod, ret, ret + 0);
	for (i = 0; i < num_sk; i++) {
		evs[i].events = EPOLLIN;
		ret = lseek(sk[i], 0, SEEK_SET);
		ret = epoll_ctl(efd, EPOLL_CTL_MOD, sk[i], &evs[i]);
	}

label(wait_read,
	ret, epoll_wait(efd, evs, num_sk, 5000));
	if (ret != num_sk) {
		log_error("Expected epoll_wait() to return num_sk events.\n");
		goto out;
	}
	for (i = 0; i < num_sk; i++) {
		if (!(evs[i].events & EPOLLIN)) {
			log("FAIL", "Expected EPOLLIN (0x%X) flag, got %s (0x%X)\n", 
				  EPOLLIN, eflags(evs[i].events), evs[i].events);
			goto out;
		}
	}

label(do_read, ret, ret + 0);
	for (i = 0; i < num_sk; i++) {
		ret = read(sk[i], rbuf, strlen(HELLO) + 1);
		if (ret < (strlen(HELLO) + 1)) {
			log("FAIL", "Unable to read all %zu bytes of \"%s\"\n",
				 strlen(HELLO) + 1, HELLO);
			goto out;
		}
		if (strcmp(HELLO, rbuf)) {
			log("FAIL", "File was corrupted. Expected: \"%s\" Got: \"%s\"\n",
				 HELLO, rbuf);
			goto out;
		}
	}
	log("INFO", "read len ok\n");
	log("INFO", "read socket contents ok\n");
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
	if (sk) {
		for (i = 0; i < num_sk; i++) {
			if (sk[i] > 0) {
				close(sk[i]);
				sk[i] = -1;
			}
		}
		free(sk);
	}
	if (evs)
		free(evs);
	fflush(logfp);
	fclose(logfp);
	exit(ec);
}
