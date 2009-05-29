targets = create-sem create-shm

SLIBS = ../libcrtest/libcrtest.a
LIBCMD = -I../libcrtest -L../libcrtest $(SLIBS)

all: $(targets)

create-sem: create-sem.c ../clone.h $(SLIBS)
	gcc -o create-sem create-sem.c $(LIBCMD)

create-shm: create-shm.c ../clone.h $(SLIBS)
	gcc -o create-shm create-shm.c $(LIBCMD)

clean:
	rm -rf $(targets) o.* ckpt.* sandbox
