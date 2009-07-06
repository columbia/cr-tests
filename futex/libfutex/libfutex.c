#include <stdlib.h>
#include <sys/mman.h>
#include "libfutex.h"

void *alloc_futex_mem(size_t sz)
{
	void *p;
	size_t pagesize = sysconf(_SC_PAGE_SIZE);
	int rc;

	if (pagesize == -1)
		return NULL;

	rc = posix_memalign(&p, pagesize, sz);
	if (rc != 0) {
		errno = rc;
		return NULL;
	}

	rc = mprotect(p, sz, PROT_READ|PROT_WRITE|PROT_SEM);
	if (rc == 0)
		return p;
	free(p);
	return NULL;
}
