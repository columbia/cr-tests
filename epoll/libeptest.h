/* epoll syscalls */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/epoll.h>

#include "libcrtest/libcrtest.h"
#include "libcrtest/labels.h"

extern FILE *logfp;

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

/* Non-reentrant!! */
const char * eflags(unsigned int events);

#define HELLO "Hello world!\n"
