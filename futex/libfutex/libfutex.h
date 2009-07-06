#ifndef __LIBFUTEX_H
#define __LIBFUTEX_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>
#include <signal.h>
#include <linux/futex.h>
#include <sys/time.h>

#include <atomic.h>

#ifndef SYS_futex
#ifdef __NR_futex
#define SYS_futex __NR_futex
#elif __i386__
#define SYS_futex 240
#elif __ia64__
#define SYS_futex 1230
#elif __x86_64__
#define SYS_futex 202
#elif __s390x__ || __s390__
#define SYS_futex 238
#elif __powerpc__
#define SYS_futex 221
#else
#error "libfutex not supported on this architecure yet. If your arch and kernel support futexes then it is just syscall glue plus some basic atomic operations. So a patch would be fairly easy and welcome upstream."
#endif
#endif

#ifndef __NR_futex
#define __NR_futex SYS_futex
#endif

#ifndef PROT_SEM
#define PROT_SEM 0x08
#endif

static inline long futex(volatile int *uaddr, int op, int val,
			const struct timespec *timeout,
			int *uaddr2, int val2)
{
	return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val2);
}

static inline long set_robust_list(struct robust_list_head *rlist, size_t len)
{
	return syscall(__NR_set_robust_list, rlist, len);
}

static inline long get_robust_list(pid_t pid, struct robust_list_head **rlist,
				  size_t *len)
{

	return syscall(__NR_get_robust_list, pid, rlist, len);
}

static inline pid_t gettid(void)
{
	return syscall(SYS_gettid);
}

static inline long tgkill(pid_t tgid, pid_t tid, int sig)
{
	return syscall(SYS_tgkill, tgid, tid, sig);
}

/* Allocate memory suitable for use as a futex */
extern void *alloc_futex_mem(size_t sz);


/* Thread-safe logging */
extern FILE *logfp;
extern atomic_t log_lock; /* initialize to = { 0 }; !! */

/*
 * Log output with a tag (INFO, WARN, FAIL, PASS) and a format.
 * Adds information about the thread originating the message.
 *
 * Flush the log after every write to make sure we get consistent, and
 * complete logs.
 */
#define log(tag, fmt, ...) \
do { \
	int __tid = gettid(); \
	while (atomic_cmpxchg(&log_lock, 0, __tid) != 0) {} \
	fprintf(logfp, ("%s: thread %d: " fmt), (tag), __tid, ##__VA_ARGS__ ); \
	fflush(logfp); \
	fsync(fileno(logfp)); \
	while (atomic_cmpxchg(&log_lock, __tid, 0) != __tid) {} \
} while(0)

/* like perror() except to the log */
#define log_error(s) log("FAIL", "%s: %s\n", (s), strerror(errno))

#endif /* __LIBFUTEX_H */
