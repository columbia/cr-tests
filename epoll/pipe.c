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

/* epoll syscalls */
#include <sys/epoll.h>

#include "libcrtest/libcrtest.h"

#define LOG_FILE	"log.pipe"
FILE *logfp = NULL;

/*
 * Log output with a tag (INFO, WARN, FAIL, PASS) and a format.
 * Adds information about the thread originating the message.
 *
 * Flush the log after every write to make sure we get consistent, and
 * complete logs.
 */
#define log(tag, fmt, ...) \
do { \
	pid_t __tid = getpid(); \
	fprintf(logfp, ("%s: thread %d: " fmt), (tag), __tid, ##__VA_ARGS__ ); \
	fflush(logfp); \
	fsync(fileno(logfp)); \
} while(0)

/* like perror() except to the log */
#define log_error(s) log("FAIL", "%s: %s\n", (s), strerror(errno))

#define stringify(expr) #expr

/* Print EPOLL flag */
#define peflag(flag) \
do { \
	if (!events & flag)  \
		break; \
	len = snprintf(p, sz, stringify(flag)); \
	if (len > 0) { \
		sz -= len; \
		p += len; \
	} else \
		abort(); \
} while (0)

const char * eflags(unsigned int events)
{
	static char buffer[256];
	char *p = buffer;
	size_t sz = 256;
	int len;

	peflag(EPOLLIN);
	peflag(EPOLLPRI);
	peflag(EPOLLOUT);
	peflag(EPOLLERR);
	peflag(EPOLLHUP);
	peflag(EPOLLRDHUP);
	peflag(EPOLLET);
	peflag(EPOLLONESHOT);

	return buffer;
}
#undef peflag

/*
 * A LABEL is a point in the program we can goto where it's interesting to
 * checkpoint. These enable us to have a set of labels that can be specified
 * on the commandline.
 */
const char __attribute__((__section__(".LABELs"))) *first_label = "<start>";
const char __attribute__((__section__(".LABELs"))) *last_label;

#define num_labels ((&last_label - &first_label) - 1)

static inline const char * labels(int i)
{
	return (&first_label)[num_labels - i];
}

void print_labels(FILE *pout)
{
	int i;

	if (num_labels > 0)
		fprintf(pout, "\tNUM\tLABEL\n");
	for (i = 0; i < num_labels; i++)
		fprintf(pout, "\t%d\t%s\n", i, labels(i));
}

void usage(FILE *pout)
{
	fprintf(pout, "\nepoll_pipe [-L] [-N] [-h|--help] [-l LABEL] [-n NUM]\n"
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
	{0, 0, 0, 0},
};

/* The spot (LABEL or label number) where we should test checkpoint/restart */
char const *ckpt_label;
int ckpt_op_num = 0;

void parse_args(int argc, char **argv)
{
	ckpt_label = last_label;
	ckpt_op_num = num_labels;
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

/* Signal ready for and await the checkpoint */
void do_ckpt(void)
{
	set_checkpoint_ready();
	while (!test_checkpoint_done())
		usleep(10000);

}

/* Label a spot in the code... */
#define label(lbl, ret, action) \
do { \
	static char __attribute__((__section__(".LABELs"))) *___ ##lbl## _l = stringify(lbl); \
	goto lbl ; \
lbl: \
\
        log("INFO", "label: %s: \"%s\"\n", \
		    labels(op_num), stringify(action)); \
\
	ret = action ; \
\
	if ((ckpt_op_num == op_num) || \
	    (strcmp(ckpt_label, ___ ##lbl## _l) == 0)) \
		do_ckpt(); \
	if (ret < 0) { \
		log("FAIL", "%d\t%s: %s\n", \
		    op_num, ___ ##lbl## _l, stringify(action) ); \
		goto out; \
	} \
	op_num++; \
} while(0)

#define HELLO "Hello world!\n"
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
	if (!move_to_cgroup("freezer", "1", getpid())) {
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
			  labels(op_num), op_num);
	}
	close(tube[0]);
	close(tube[1]);
	fflush(logfp);
	fclose(logfp);
	exit(ec);
}

const char __attribute__((__section__(".LABELs"))) *last_label  = "<end>";
