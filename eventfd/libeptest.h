/* epoll syscalls */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/epoll.h>

#include "libcrtest/libcrtest.h"

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

/*
 * A LABEL is a point in the program we can goto where it's interesting to
 * checkpoint. These enable us to have a set of labels that can be specified
 * on the commandline.
 */
extern const char __attribute__((__section__(".LABELs"))) *first_label;
extern const char __attribute__((__section__(".LABELs"))) *last_label;

#define num_labels ((&last_label - &first_label) - 1)

static inline const char * labels(int i)
{
	return (&first_label)[num_labels - i];
}

/* Print the labels that this program has to pout */
void print_labels(FILE *pout);

/* Signal ready for and await the checkpoint */
void do_ckpt(void);

/* The spot (LABEL or label number) where we should test checkpoint/restart */
extern char const *ckpt_label;
extern int ckpt_op_num;

#define stringify(expr) #expr

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
	if ((ckpt_op_num == op_num) || (ckpt_op_num == -1) || \
	    (strcmp(ckpt_label, ___ ##lbl## _l) == 0)) \
		do_ckpt(); \
	op_num++; \
} while(0)
