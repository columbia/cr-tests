
#define CKPT_READY		"checkpoint-ready"
#define CKPT_DONE		"checkpoint-done"
#define TEST_DONE		"test-done"

extern FILE *logfp;

struct record {
	int id;
	char data[256];
};

extern void do_exit(int status);
extern int test_done(void);
extern int test_checkpoint_done();
extern void set_checkpoint_ready(void);
extern int do_wait(int num_children);
extern void copy_data(char *srcfile, char *destfile);

extern char *freezer_mountpoint(void);
/* right now, subsys must always be "freezer" */
extern int move_to_cgroup(char *subsys, char *grp, int pid);
