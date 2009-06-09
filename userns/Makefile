targets = userns_ckptme usertask sbits userns_deep simple_deep

SLIBS = ../libcrtest/libcrtest.a
LIBCMD = -I../libcrtest -L../libcrtest $(SLIBS)

all: $(targets)

userns_ckptme: userns_ckptme.c ../clone.h $(SLIBS)
	gcc -o userns_ckptme userns_ckptme.c $(LIBCMD)

usertask: usertask.c $(SLIBS)
	gcc -o usertask usertask.c $(LIBCMD)

sbits: sbits.c $(SLIBS)
	gcc -o sbits sbits.c $(LIBCMD)

userns_deep: userns_deep.c $(SLIBS)
	gcc -o userns_deep userns_deep.c $(LIBCMD)

simple_deep: simple_deep.c $(SLIBS)
	gcc -o simple_deep simple_deep.c $(LIBCMD)

clean:
	rm -rf $(targets) o.* psout* sandbox ckpt.out out.* outfile
	rm -rf checkpointed finished started pidfile
