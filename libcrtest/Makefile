
all: libcrtest.a

common.o: libcrtest.h common.c
	cc $(CFLAGS) -c common.c

libcrtest.a: common.o
	ar cr libcrtest.a common.o

clean:
	rm -f common.o libcrtest.a
