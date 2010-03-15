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

#include <sys/eventfd.h>
#include <sys/time.h> /* itimers */

#include "libeptest.h"

#define LOG_FILE	"log.rw"

void usage(FILE *pout, const char *prog)
{
	fprintf(pout, "\n%s [-L] [-N] [-h|--help] [-l LABEL] [-n NUM]\n"
"Create an empty epoll set.\n"
"\n"
"\t-L\tPrint the valid LABELs in order and exit.\n"
"\t-l\tWait for checkpoint at LABEL.\n"
"\t-N\tPrint the maximum label number and exit.\n"
"\t-n\tWait for checkpoint at NUM.\n"
"\n"
"You may only specify one LABEL or NUM and you may not specify both.\n"
"Label numbers are integers in the range 0-%d\n"
"Valid label numbers and their corresponding LABELs are:\n", prog,
		num_labels - 1);
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

void parse_label_args(int argc, char **argv)
{
	ckpt_op_num = -1;
	ckpt_label = NULL;
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

void setup_test_output(const char *log_filename)
{
	/* FIXME eventually stdio streams should be harmless */
	close(0);
	logfp = fopen(log_filename, "w+");
	if (!logfp) {
		perror("could not open logfile");
		exit(1);
	}
	/* redirect stdout and stderr to the log file */
	if (dup2(fileno(logfp), 1) < 0) {
		log_error("dup2(logfp, 1)");
		exit(2);
	}
	if (dup2(fileno(logfp), 2) < 0) {
		log_error("dup2(logfp, 2)");
		exit(3);
	}
}

static int evfd;

void handler(int signo)
{
	u64 v;

	/*
	 * We don't actually do anything here -- just handle EINTR errnos
	 * synchronously.
	 */
	log("INFO", "Received SIGLARM\n");
	v = 1;
	write(evfd, &v, sizeof(v));
}

int main (int argc, char **argv)
{
	struct itimerval one_second_timeout = {
		.it_value = { .tv_sec = 1, .tv_usec = 0 },
		.it_interval = { .tv_sec = 0, .tv_usec = 0 }
	};
	u64 v;
	int ret = 0;
	int op_num = 0;

	parse_label_args(argc, argv);
	setup_test_output(LOG_FILE);

	if (!move_to_cgroup("freezer", freezer, getpid())) {
		log_error("move_to_cgroup");
		exit(4);
	}
	log("INFO", "entered cgroup %s\n", freezer);

label(create_eventfd,
	evfd, eventfd(0, EFD_CLOEXEC));
	if (evfd < 0) {
		log_error("eventfd(EFD_CLOEXEC)");
		fclose(logfp);
		exit(EXIT_FAILURE);
	}

	/* Set up a timer to bring us out of a blocking read */
	signal(SIGALRM, handler);
label(prepare_blocking_read_timeout,
      ret, setitimer(ITIMER_REAL, &one_second_timeout, NULL));

	/* Do the blocking read. */
	log("INFO", "Doing blocking read. Quick! Somebody checkpoint me!\n");
	ret = read(evfd, &v, sizeof(v));
	if (v != 1) {
		log("FAIL", "Expected event value %lld, got %lld\n",
		    1ULL, v);
		fclose(logfp);
		exit(EXIT_FAILURE);
	}

	/* Write to the event counter and get the result. */
	v = 31337;
label(write_event1, ret, write(evfd, &v, sizeof(v)));
	v = 0;
label(read_event1, ret, read(evfd, &v, sizeof(v)));
	if (v != 31337) {
		log("FAIL", "Expected event value %lld, got %lld\n",
		    31337ULL, v);
		fclose(logfp);
		exit(EXIT_FAILURE);
	}

	/* Now test non-blocking operation */
label(set_nonblocking, ret, fcntl(evfd, F_SETFL, O_NONBLOCK));
	ret = read(evfd, &v, sizeof(v));
	if ((ret != -1) || (errno != EAGAIN)) {
		log("FAIL", "Expected EAGAIN from non-blocking read.\n");
		fclose(logfp);
		exit(EXIT_FAILURE);
	}

	v = 1;
label(write_event2, ret, write(evfd, &v, sizeof(v)));
	v = 0;
label(read_event2, ret, read(evfd, &v, sizeof(v)));
	if (v != 1) {
		log("FAIL", "Expected event value %lld, got %lld\n",
		    1ULL, v);
		fclose(logfp);
		exit(EXIT_FAILURE);
	}
	ret = EXIT_SUCCESS;

out:
	if (close(evfd) < 0) {
		perror("close()");
		fclose(logfp);
		exit(EXIT_FAILURE);
	}
	fclose(logfp);
	exit(ret);
}
