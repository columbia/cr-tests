#ifndef LIBCRTEST_H
#define LIBCRTEST_H 1
#include <unistd.h>
#include <sys/syscall.h>

#define CKPT_READY		"checkpoint-ready"
#define CKPT_DONE		"checkpoint-done"
#define CKPT_DRY_RUN		"checkpoint-skip"
#define TEST_DONE		"test-done"

extern FILE *logfp;

struct record {
	int id;
	char data[256];
};

typedef unsigned long long u64;

extern void close_all_fds(void);
extern void do_exit(int status);
extern int test_done(void);
extern int test_checkpoint_done();
extern void set_checkpoint_ready(void);
extern void do_ckpt(void);
extern int do_wait(int num_children);
extern void copy_data(char *srcfile, char *destfile);
extern void print_exit_status(int pid, int status);

extern char *freezer_mountpoint(void);
/* right now, subsys must always be "freezer" */
extern int move_to_cgroup(char *subsys, char *grp, int pid);

extern void notify_one_event(int efd);
extern void wait_for_events(int efd, u64 total);
extern int setup_notification();

static inline pid_t gettid(void)
{
	return syscall(SYS_gettid);
}
#define HAVE_GETTID 1

#endif /* LIBCRTEST_H */
