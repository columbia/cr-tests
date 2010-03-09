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

/* The spot (LABEL or label number) where we should test checkpoint/restart */
char const *ckpt_label;
int ckpt_op_num = 0;
