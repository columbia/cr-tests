targets = cwdsleep chrootsleep

all: $(targets)

SLIBS = ../libcrtest/libcrtest.a
LIBCMD = -I../libcrtest -L../libcrtest $(SLIBS)

cwdsleep: cwdsleep.c $(SLIBS)
	gcc -static -o cwdsleep cwdsleep.c $(LIBCMD)

chrootsleep: chrootsleep.c $(SLIBS)
	gcc -static -o chrootsleep chrootsleep.c $(LIBCMD)

clean:
	rm -f $(targets)
	rm -rf cr_taskfs*
