#include <errno.h>

#include "libeptest.h"

FILE *logfp = NULL;

void print_labels(FILE *pout)
{
	int i;

	if (num_labels > 0)
		fprintf(pout, "\tNUM\tLABEL\n");
	for (i = 0; i < num_labels; i++)
		fprintf(pout, "\t%d\t%s\n", i, labels(i));
}

/* Signal ready for and await the checkpoint */
void do_ckpt(void)
{
	int rc;

	set_checkpoint_ready();

	rc = access("./checkpoint-skip", F_OK);
	if (rc == 0)
		return;
	else if (errno != ENOENT)
		exit(EXIT_FAILURE);

	while (!test_checkpoint_done())
		usleep(10000);
	if (unlink("./checkpoint-done") == -1) {
		/* perror("unlink(\"./checkpoint-done\")"); */
	}
}

/* The spot (LABEL or label number) where we should test checkpoint/restart */
char const *ckpt_label;
int ckpt_op_num = 0;
