targets = ptyloop

all: $(targets)

SLIBS = ../libcrtest/libcrtest.a
LIBCMD = -I../libcrtest -L../libcrtest $(SLIBS)

ptyloop: ptyloop.c ../clone.h $(SLIBS)
	$(CC) -o ptyloop ptyloop.c $(LIBCMD)

clean:
	rm -f $(targets) out* ckpt-out read-ok read-bad
