.PHONY: clean all

LIBS := ../libcrtest/libcrtest.a
CFLAGS := -Wall $(ARCHOPTS) -I../ -I../libcrtest

PROGS=empty pipe

.PHONY: all clean

all: $(PROGS)

../libcrtest/libcrtest.a: ../libcrtest/libcrtest.h ../libcrtest/common.c
	$(MAKE) -C ../libcrtest libcrtest.a

$(PROGS): %: %.c $(LIBS)
	gcc -Wall $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(PROGS)
	rm -rf log.* checkpoint-*
	$(MAKE) -C ../libcrtest clean
