.PHONY: clean all

LIBS := ../libcrtest/libcrtest.a ./libeptest.a
CFLAGS := -Wall $(ARCHOPTS) -I../ -I../libcrtest

# Properly generate special symbols for the labels ELF section
LDFLAGS :=  -Xlinker -dT -Xlinker ../libcrtest/labels.lds

PROGS=empty pipe sk10k cycle scm

.PHONY: all clean

all: $(PROGS)

../libcrtest/libcrtest.a: ../libcrtest/libcrtest.h ../libcrtest/common.c
	$(MAKE) -C ../libcrtest libcrtest.a

libeptest.o: libeptest.h

libeptest.a: libeptest.o
	ar cr $@ $<

$(PROGS): %: %.c $(LIBS)
	gcc -Wall $(CFLAGS) -o $@ $< $(LIBS) $(LDFLAGS)

clean:
	rm -f $(PROGS) libeptest.[ao]
	$(MAKE) -C ../libcrtest clean
	rm -rf cr_epoll*
