#ifndef LIBCRTEST_LOG_H
#define LIBCRTEST_LOG_H

extern FILE *logfp;

#ifdef HAVE_LOG_LOCK
/* Thread-safe logging */
extern atomic_t log_lock; /* initialize to = { 0 }; !! */
#endif

#ifdef HAVE_LOG_LOCK
#define lock_log() do { \
	while (atomic_cmpxchg(&log_lock, 0, __tid) != 0) {} \
} while(0)

#define unlock_log() do { \
	while (atomic_cmpxchg(&log_lock, __tid, 0) != __tid) {} \
} while(0)
#else
#define lock_log() do {} while (0)
#define unlock_log() do {} while (0)
#endif

#ifndef HAVE_GETTID
#define gettid getpid
#endif

/*
 * Log output with a tag (INFO, WARN, FAIL, PASS) and a format.
 * Adds information about the thread originating the message.
 *
 * Flush the log after every write to make sure we get consistent, and
 * complete logs.
 */
#define log(tag, fmt, ...) \
do { \
	pid_t __tid = gettid(); \
	lock_log(); \
	fprintf(logfp, ("%s: thread %d: " fmt), (tag), __tid, ##__VA_ARGS__ ); \
	fflush(logfp); \
	fsync(fileno(logfp)); \
	unlock_log(); \
} while(0)

/* like perror() except to the log */
#define log_error(s) log("FAIL", "%s: %s\n", (s), strerror(errno))

#endif /* LIBCRTEST_LOG_H */
