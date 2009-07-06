#if __i386__

#ifndef __NR_checkpoint
#define __NR_checkpoint 335
#endif
#ifndef __NR_restart
#define __NR_restart    336
#endif

#elif __s390x__

#ifndef __NR_checkpoint
#define __NR_checkpoint 332
#endif
#ifndef __NR_restart
#define __NR_restart    333
#endif

#elif __powerpc__

#ifndef __NR_checkpoint
#define __NR_checkpoint 322
#endif
#ifndef __NR_restart
#define __NR_restart    323
#endif
#endif
