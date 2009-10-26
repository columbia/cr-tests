LTPROOT ?= ../ltp

LIBS := ../libcrtest/libcrtest.a ./libeptest.a $(LTPROOT)/lib/libltp.a
CFLAGS := -Wall $(ARCHOPTS) -I../ -I../libcrtest -I$(LTPROOT)/include

PROGS=eventfd01

.PHONY: all clean

all: $(PROGS)

../libcrtest/libcrtest.a: ../libcrtest/libcrtest.h ../libcrtest/common.c
	$(MAKE) -C ../libcrtest libcrtest.a

libeptest.a: libeptest.o libeptest.h
	ar cr libeptest.a libeptest.o

$(PROGS): %: %.c $(LIBS)
	gcc -Wall $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o libeptest.a log.* checkpoint-* $(PROGS)
