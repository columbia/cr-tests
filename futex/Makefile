.PHONY: clean all

LIBS := libfutex/libfutex.a ../libcrtest/libcrtest.a
PROGS := plain robust pi

MACHINE=$(shell gcc -dumpmachine)
ifeq ($(MACHINE:i386-%=i386),i386)
ARCHOPTS := -march=i486
endif

# PowerPC doesn't seem to require any particular target machine specification
# # to get the __sync_* GCC builtins.
# Neither should x86_64.
#
# If you see output like:
#	/tmp/ccqoSnmA.o: In function `atomic_cmpxchg':
#	plain.c:(.text+0x69b): undefined reference to `__sync_val_compare_and_swap_4'
#
# then you probably need a -mFOO option to gcc set in ARCHOPT.

CFLAGS := -Wall $(ARCHOPTS) -I./libfutex -I../

all: $(PROGS)

../libcrtest/libcrtest.a: ../libcrtest/libcrtest.h ../libcrtest/common.c
	$(MAKE) -C ../libcrtest libcrtest.a

libfutex/libfutex.a: libfutex/libfutex.c libfutex/libfutex.h
	$(MAKE) -C libfutex libfutex.a

plain: plain.c $(LIBS) Makefile
	gcc $(CFLAGS) -o $@ $< $(LIBS)

robust: robust.c $(LIBS) Makefile
	gcc $(CFLAGS) -o $@ $< $(LIBS)

pi: pi.c $(LIBS) Makefile
	gcc $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o $(PROGS)
	rm -rf log.* checkpoint-ready checkpoint-done
	$(MAKE) -C libfutex clean
	$(MAKE) -C ../libcrtest clean
