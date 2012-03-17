/*
 * Open a number of epoll sets, link them into a cycle, add a pipe to one,
 * write sample content to the pipe, and verify that the write events
 * arrived.
 *
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

#define LOG_FILE "log.cycle"

void usage(FILE *pout)
{
	fprintf(pout, "\ncycle [-L] [-N] [-h|--help] [-l LABEL] [-n NUM] [-c NUM]\n\n"
"Open several epoll sets and link them in a cycle.\n"
"This means that each successive epoll fd waits for events from another epoll\n"
"file descriptor. To test event propagation we also use a pipe in one epoll\n"
"set and do some IO with the pipe.\n"
"\n"
"\t-L\tPrint the valid LABELs in order and exit.\n"
"\t-l\tWait for checkpoint at LABEL.\n"
"\t-N\tPrint the maximum label number and exit.\n"
"\t-n\tWait for checkpoint at NUM.\n"
"\t-c\tCicumference of the epoll set cycle in NUM epoll fds.\n"
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
	{ "circumference",	1, 0, 'c'},
	{ "freezer",		1, 0, 'f'},
	{0, 0, 0, 0},
};

int num_efd = 3;
char *freezer = "1";

void parse_args(int argc, char **argv)
{
	ckpt_op_num = num_labels - 1;
	ckpt_label = labels[ckpt_op_num];
	while (1) {
		int c;
		c = getopt_long(argc, argv, "f:LNhl:n:s:c:", long_options, NULL);
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
					fprintf(stderr, "Option -%c requires an argument in the range 0-%d. Got %d\n", c, num_labels - 1, ckpt_op_num);
					usage(stderr);
					exit(EXIT_FAILURE);
				}
				break;
			case 'c':
				if (sscanf(optarg, "%u", &num_efd) < 1) {
					fprintf(stderr, "Option -%c requires an argument in the range 1-INT_MAX. Got %d\n", c, num_efd);
					usage(stderr);
					exit(EXIT_FAILURE);
				}

				{
					/* rlimit restricts max fd */
					struct rlimit lim;
					getrlimit(RLIMIT_NOFILE, &lim);
					fprintf(stdout, "INFO: RLIMIT_NOFILE: soft (cur): %ld hard (max): %ld\n", lim.rlim_cur, lim.rlim_max);
					if ((unsigned int)num_efd >= lim.rlim_cur) {
						fprintf(stderr, "WARN: process is restricted from opening %d sockets. Opening %ld instead.\n", num_efd, lim.rlim_cur);
						num_efd = lim.rlim_cur;
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
	struct epoll_event ev;
	int op_num = 0;
	int pfd[2];
	int efd[3];
	int ec = EXIT_FAILURE;
	int ret, i, j;

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

	ret = pipe(pfd);
	if (ret < 0)
		goto out;
	log("INFO", "pipe read fd: %d, pipe write fd: %d\n",
	    pfd[0], pfd[1]);

label(create_efd, ret, ret + 0);
	ev.events = EPOLLOUT|EPOLLIN|EPOLLET;
	for (i = 0; i < num_efd; i++) {
		efd[i] = epoll_create(4);
		if (efd[i] < 0) {
			log("FAIL", "efd[i] = epoll_create(3);");
			ret = efd[i];
			goto out;
		}
		if (i == 0)
			continue;
		ev.data.fd = efd[i - 1];
		ret = epoll_ctl(efd[i], EPOLL_CTL_ADD, ev.data.fd, &ev);
		if (ret < 0) {
			log("FAIL", "epoll_ctl(efd[i] (%d), EPOLL_CTL_ADD, ev.data.fd (%d), &ev);", efd[i], ev.data.fd);
			goto out;
		}
	}

	/* Close the cycle */
	ev.data.fd = efd[num_efd - 1];
	ret = epoll_ctl(efd[0], EPOLL_CTL_ADD, ev.data.fd, &ev);
	if (ret < 0) {
		log("FAIL",
		"epoll_ctl(efd[num_efd - 1], EPOLL_CTL_ADD, ev.data.fd, &ev);");
		goto out;
	}

label(link_pipe, ret, ret + 0);
	/*
	 * Now put the pipe fds "last" set of the cycle. For example:
	 *
	 * /---------------------------------\
	 * |                                 |
	 * \- efd[0] <-- efd[1] <-- efd[2] <-/
	 *                            | |
	 *                            | \--> pfd[0]
	 *                            \----> pfd[1]
	 *
	 * Where foo --> bar means that foo has bar in its set.
	 */
	ev.events = EPOLLIN;
	ev.data.fd = pfd[0];
	ret = epoll_ctl(efd[num_efd - 1], EPOLL_CTL_ADD, ev.data.fd, &ev);
	if (ret < 0) {
		log("FAIL",
		"epoll_ctl(efd[num_efd - 1], EPOLL_CTL_ADD, pfd[0], &ev);");
		goto out;
	}
	ev.events = EPOLLOUT;
	ev.data.fd = pfd[1];
	ret = epoll_ctl(efd[num_efd - 1], EPOLL_CTL_ADD, ev.data.fd, &ev);
	if (ret < 0) {
		log("FAIL",
		"epoll_ctl(efd[num_efd - 1], EPOLL_CTL_ADD, pfd[1], &ev);");
		goto out;
	}

label(wait_write, ret, ret + 0);
	/*
	 * Since it's a cycle of epoll sets, we have to wait on the
	 * other epoll sets to get the event that triggered EPOLLIN
	 * on this set. Start with the epoll fd which will take us the
	 * long way around the cycle: efd[num_efd - 2].
	 */

	/* The index of the previous epoll fd in the cycle */
	j = num_efd - 1;
	for (i = num_efd - 2; i > -1; i--) {
		/* The index of the previous epoll fd in the cycle */
		j = (unsigned int)(i - 1) % ~(num_efd - 1);
		log("INFO", "Waiting on %d for EPOLLIN on %d\n", efd[i], efd[j]);
		ret = epoll_wait(efd[i], &ev, 1, 1000);
		if (ret != 1) {
			log_error("Expected epoll_wait() to return an event.\n");
			goto out;
		}
		log("INFO", "Got event: fd: %d eflags: %s\n", ev.data.fd, eflags(ev.events));
		if ((ev.data.fd != efd[j]) || !(ev.events & EPOLLIN))
			goto out;
	}

	/*
	 * Now we expect the actual event indicating it's ok to write
	 * output.
	 */
	log("INFO", "Waiting on %d for EPOLLOUT on %d\n", efd[j], pfd[1]);
	ret = epoll_wait(efd[j], &ev, 1, 1000);
	if (ret != 1) {
		log_error("Expected epoll_wait() to return an event.\n");
		goto out;
	}
	log("INFO", "Got event: fd: %d eflags: %s\n", ev.data.fd, eflags(ev.events));
	if ((ev.data.fd != pfd[1]) || !(ev.events & EPOLLOUT))
		goto out;
label(do_write,
	ret, write(pfd[1], HELLO, strlen(HELLO) + 1));
	if (ret < (int)(strlen(HELLO) + 1)) {
		log("FAIL", "Unable to write all %zu bytes of \"%s\" to %d\n",
			 strlen(HELLO) + 1, HELLO, pfd[0]);
		goto out;
	}

label(wait_read, ret, ret + 0);
	/* The index of the previous epoll fd in the cycle */
	j = num_efd - 1;
	for (i = num_efd - 2; i > -1; i--) {
		/* The index of the previous epoll fd in the cycle */
		j = (unsigned int)(i - 1) % ~(num_efd - 1);
		log("INFO", "Waiting on %d for EPOLLIN on %d\n", efd[i], efd[j]);
		ret = epoll_wait(efd[i], &ev, 1, 1000);
		if (ret != 1) {
			log_error("Expected epoll_wait() to return an event.\n");
			goto out;
		}
		log("INFO", "Got event: fd: %d eflags: %s\n", ev.data.fd, eflags(ev.events));
		if ((ev.data.fd != efd[j]) || !(ev.events & EPOLLIN))
			goto out;
	}
	log("INFO", "Waiting on %d for EPOLLIN on %d\n", efd[j], pfd[0]);
	ret = epoll_wait(efd[j], &ev, 1, 1000);
	if (ret != 1) {
		log_error("Expected epoll_wait() to return an event.\n");
		goto out;
	}
	log("INFO", "Got event: fd: %d eflags: %s\n", ev.data.fd, eflags(ev.events));
	if ((ev.data.fd != pfd[0]) || !(ev.events & EPOLLIN))
		goto out;

label(do_read, ret, ret + 0);
	ret = read(pfd[0], rbuf, strlen(HELLO) + 1);
	if (ret < (int)(strlen(HELLO) + 1)) {
		log("FAIL", "Unable to read all %zu bytes of \"%s\"\n",
			 strlen(HELLO) + 1, HELLO);
		goto out;
	}
	if (strcmp(HELLO, rbuf)) {
		log("FAIL", "File was corrupted. Expected: \"%s\" Got: \"%s\"\n",
			 HELLO, rbuf);
		goto out;
	}
	log("INFO", "read len ok\n");
	log("INFO", "read pipe contents ok\n");
	ec = EXIT_SUCCESS;
	op_num = INT_MAX;

out:
	if (op_num != INT_MAX) {
		log("FAIL", "error at label %s (op num: %d)\n",
			  labels[op_num], op_num);
	}
	for (i = 0; i < num_efd; i++) {
		ret = close(efd[i]);
		efd[i] = -1;
		if (ret < 0)
			log_error("close(efd[i])");
	}
	if (pfd[0]) {
		close(pfd[0]);
		close(pfd[1]);
	}
	fflush(logfp);
	fclose(logfp);
	exit(ec);
}
