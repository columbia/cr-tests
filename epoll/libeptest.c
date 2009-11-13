#include "libeptest.h"

FILE *logfp = NULL;

#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif
/* Print EPOLL flag */
#define peflag(flag) \
do { \
	if (!(events & flag))  \
		break; \
	len = snprintf(p, sz, "%s%s", sep, stringify(flag)); \
	if (len > 0) { \
		sz -= len; \
		p += len; \
		sep = "|"; \
	} else \
		abort(); \
} while (0)

/* WARNING: Non-reentrant!! 
 *
 * Print out the epoll events
 */
const char * eflags(unsigned int events)
{
	static char buffer[256];
	char *sep = "";
	char *p = buffer;
	size_t sz = 256;
	int len;

	peflag(EPOLLIN);
	peflag(EPOLLPRI);
	peflag(EPOLLOUT);
	peflag(EPOLLERR);
	peflag(EPOLLHUP);
	peflag(EPOLLRDHUP);
	peflag(EPOLLET);
	peflag(EPOLLONESHOT);

	return buffer;
}
#undef peflag

/* Signal ready for and await the checkpoint */
void do_ckpt(void)
{
	set_checkpoint_ready();
	while (!test_checkpoint_done())
		usleep(10000);

}
