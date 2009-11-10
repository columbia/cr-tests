targets = create-sem create-shm check-mq

SLIBS = ../libcrtest/libcrtest.a
LIBCMD = -I../libcrtest -L../libcrtest $(SLIBS)

all: $(targets)

create-sem: create-sem.c ../clone.h $(SLIBS)
	gcc -o create-sem create-sem.c $(LIBCMD)

create-shm: create-shm.c ../clone.h $(SLIBS)
	gcc -o create-shm create-shm.c $(LIBCMD)

check-mq: check-mq.c ../clone.h $(SLIBS)
	gcc -o check-mq check-mq.c $(LIBCMD)

clean:
	rm -rf $(targets) cr_sem* cr_mq* cr_shm*
