#include "libfstest.h"

FILE *logfp = NULL;

/* Signal ready for and await the checkpoint */
void do_ckpt(void)
{
	set_checkpoint_ready();
	while (!test_checkpoint_done())
		usleep(10000);

}
