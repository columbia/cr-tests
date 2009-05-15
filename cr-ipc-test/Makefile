CFLAGS += -Wall

all: checkpoint-self restart-self sysv-sem clone-newipc checkpoint-shm checkpoint-sem tst_ipcshm_multi

checkpoint-self: util.h checkpoint.h syscalls.o util.o checkpoint-self.c
	$(CC) $(CFLAGS) checkpoint-self.c util.o syscalls.o -o checkpoint-self

restart-self: util.h checkpoint.h syscalls.o util.o restart-self.c
	$(CC) $(CFLAGS) restart-self.c util.o syscalls.o -o restart-self

sysv-sem: util.h checkpoint.h syscalls.o util.o sysv-sem.c
	$(CC) $(CFLAGS) sysv-sem.c util.o syscalls.o -o sysv-sem

clone-newipc: util.h checkpoint.h syscalls.o util.o clone-newipc.c
	$(CC) $(CFLAGS) clone-newipc.c util.o syscalls.o -o clone-newipc

checkpoint-shm: util.h checkpoint.h syscalls.o util.o checkpoint-shm.c
	$(CC) $(CFLAGS) checkpoint-shm.c util.o syscalls.o -o checkpoint-shm

checkpoint-sem: util.h checkpoint.h syscalls.o util.o checkpoint-sem.c
	$(CC) $(CFLAGS) checkpoint-sem.c util.o syscalls.o -o checkpoint-sem

tst_ipcshm_multi: tst_ipcshm_multi.c
	$(CC) $(CFLAGS) tst_ipcshm_multi.c -o tst_ipcshm_multi

.PHONY: clean
clean:
	rm -f tst_ipcshm_multi checkpoint-self restart-self sysv-sem clone-newipc checkpoint-shm checkpoint-sem *.o

.PHONY: archive
archive:
	git archive --prefix=cr-test/ --format=tar HEAD | gzip > cr-test.tar.gz
