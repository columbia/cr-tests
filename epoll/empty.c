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

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* waitpid() and W* status macros */
#include <sys/wait.h>

/* epoll syscalls */
#include <sys/epoll.h>

#include "libcrtest/libcrtest.h"

#define LOG_FILE	"log.empty"
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

int main (int argc, char **argv)
{
	int efd;

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

	efd = epoll_create(1);
	if (efd < 0) {
		perror("epoll_create(1)");
		fclose(logfp);
		exit(EXIT_FAILURE);
	}
	set_checkpoint_ready();
	while (!test_checkpoint_done())
		usleep(10000);
	if (close(efd) < 0) {
		perror("close()");
		fclose(logfp);
		exit(EXIT_FAILURE);
	}
	fclose(logfp);
	exit(EXIT_SUCCESS);
}
