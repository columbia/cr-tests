#ifndef LIBCRTEST_LABELS_H
#define LIBCRTEST_LABELS_H 1
#include <stdio.h>

/*
 * A LABEL is a point in the program we can goto where it's interesting to
 * checkpoint. These enable us to have a set of labels that can be specified
 * on the commandline.
 */
extern const char *labels[];
extern const char *___labels_end[];

extern int op_num; /* current operation count */

/* The spot (LABEL or label number) where we should test checkpoint/restart */
extern char const *ckpt_label; /* label to checkpoint at */
extern int ckpt_op_num; /* op_num to checkpoint at. -1 -> all */

/*#define num_labels ((&last_label - &first_label) - 1)*/
#define num_labels ((int)(___labels_end - labels))

/* Print the labels that this program has to pout */
static inline void print_labels(FILE *pout)
{
	int i;

	if (num_labels > 0)
		fprintf(pout, "\tNUM\tLABEL\n");
	for (i = 0; i < num_labels; i++)
		fprintf(pout, "\t%d\t%s\n", i, labels[i]);
}

/* Signal ready for and await the checkpoint. */
extern void do_ckpt(void);

#define stringify(expr) #expr

/* Label a spot in the code. TODO: Find a nicer way to do "out" */
#define label(lbl, ret, action) \
do { \
	static char __attribute__((section(".LABELs"))) *___ ##lbl## _l = stringify(lbl); \
	goto lbl ; \
lbl: \
\
        fprintf(logfp, "INFO: label: %s: \"%s\"\n", \
		    labels[op_num], stringify(action)); \
\
	ret = action ; \
\
	if ((ckpt_op_num == op_num) || (ckpt_op_num == -1) || \
	    (strcmp(ckpt_label, ___ ##lbl## _l) == 0)) \
		do_ckpt(); \
	if (ret < 0) { \
		fprintf(logfp, "FAIL: %d\t%s: %s\n", \
		    op_num, ___ ##lbl## _l, stringify(action) ); \
		goto out ; \
	} \
	op_num++; \
} while(0)

#endif /* LIBCRTEST_LABELS_H */
