/*
 * Open a pipe and an epoll set. Pass them across a unix socket to an
 * "unrelated" task and test whether epoll retrieves events related to IO
 * on the pipe.
 *
 * By varying the container or subtree checkpointed we can attempt to
 * introduce and detect checkpoint leaks.
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

/* Stuff needed to use SCM_RIGHTS via a UNIX socket */
#include <sys/socket.h>

#include <sys/prctl.h>

#include "libeptest.h"

#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0x40000000
#endif

#define LOG_FILE	"log.scm"

void usage(FILE *pout)
{
	fprintf(pout, "\nscm [-L] [-N] [-h|--help] [-l LABEL] [-n NUM]\n"
"Open a pipe and an epoll set. Pass them across a unix socket to an\n"
"'unrelated' task and test whether epoll retrieves events related to IO\n"
"on the pipe.\n"
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
	{ "cgroup2",		2, 0, 'c'},
	{ "freezer",		1, 0, 'f'},
	{0, 0, 0, 0},
};

char *cgroup2 = NULL;
char *freezer = "1";

void parse_args(int argc, char **argv)
{
	ckpt_op_num = num_labels - 1;
	ckpt_label = labels[ckpt_op_num];
	while (1) {
		int c;
		c = getopt_long(argc, argv, "f:LNhl:n:c::", long_options, NULL);
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
			case 'c':
				if (optarg && (strlen(optarg) > 0))
					cgroup2 = strdup(optarg);
				else
					cgroup2 = "2";
				break;
			case '?':
				/* unkown option in optopt */
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
int main(int argc, char **argv)
{
	struct epoll_event ev[2] = {
		{ .events = EPOLLIN,  },
		{ .events = EPOLLOUT, },
	};
	pid_t kid;
	int op_num = 0;
	int tube[2];
	int efd, sk[2];
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

	/*
	 * Now we have efd and the pipe set up. Time to pass file descriptors
	 * between two processes.
	 */
	socketpair(PF_UNIX, SOCK_DGRAM, 0, sk);
	kid = fork();
	if (kid) {
		char *msg_bytes = "efd,tube[0],tube[1]";
		int status;
		int *fdp;
		struct msghdr msg = {0};
		struct cmsghdr *cmsg;
		struct iovec iobase;
		char cbuf[CMSG_SPACE(sizeof(int)*3)];

		iobase.iov_base = msg_bytes;
		iobase.iov_len = strlen(msg_bytes) + 1;
		msg.msg_iov = &iobase;
		msg.msg_iovlen = 1;
		msg.msg_flags = 0;
		msg.msg_control = cbuf;
		msg.msg_controllen = sizeof(cbuf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int)*3);
		fdp = (int*)CMSG_DATA(cmsg);
		fdp[0] = efd;
		fdp[1] = tube[0];
		fdp[2] = tube[1];
		msg.msg_controllen = cmsg->cmsg_len;
		close(sk[1]);
		sendmsg(sk[0], &msg, 0);
		close(sk[0]);

		/* 
		 * Now the child and the parent share the open file description
		 * (aka handle) of efd, tube[0], and tube[1]. For more info
		 * see fork(2) and epoll(7).
		 */

		waitpid(kid, &status, 0);
		exit(status);
	} else {
		char msg_bytes[1024];
		struct msghdr msg = {0};
		struct cmsghdr *cmsg;
		struct iovec iobase;
		int *fdp;

		close(sk[0]);
		close(efd);
		close(tube[0]);
		close(tube[1]);
		efd = tube[0] = tube[1] = -1;

		/* Ensure that if the parent dies the child does too. */
		prctl(PR_SET_PDEATHSIG, SIGINT);

		/* Otherwise, distance ourself from parent */
		setsid();
		if (cgroup2 && !move_to_cgroup("freezer", cgroup2, getpid())) {
			log_error("move_to_cgroup [kid]");
			exit(2);
		}

		iobase.iov_base = msg_bytes;
		iobase.iov_len = 1024;

		msg.msg_iov = &iobase;
		msg.msg_iovlen = 1;
		msg.msg_flags = 0;

		recvmsg(sk[1], &msg, MSG_CMSG_CLOEXEC|MSG_WAITALL);
		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
		     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			if ((cmsg->cmsg_level != SOL_SOCKET) ||
			    (cmsg->cmsg_type != SCM_RIGHTS) ||
			    (cmsg->cmsg_len != CMSG_LEN(sizeof(int)*3)))
				continue;
			fdp = (int*)CMSG_DATA(cmsg);
			efd = fdp[0];
			tube[0] = fdp[1];
			tube[1] = fdp[2];
		}
		close(sk[1]);
	}

	if (efd == -1 ||
	    tube[0] == -1 ||
	    tube[1] == -1) {
		/* Failed to recv fds via SCM_RIGHTS */
		log_error("failed to pass fds with SCM_RIGHTS");
		exit(2);
	}

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
		log("FAIL", "Unable to write all %d bytes of \"%s\"\n",
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
		log("FAIL", "Unable to read all %d bytes of \"%s\"\n",
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
