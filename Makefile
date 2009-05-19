SUBDIRS = counterloop fileio simple cr-ipc-test userns sleep

targets = cr ns_exec rstr

all: $(targets)
	for s in $(SUBDIRS) ; do \
		( cd $$s ; make ) ; \
	done
	$(CC) -o rstr rstr.c
# I do NOT understand why make refuses to make rstr without
# specifying it above

cr: cr.c cr.h

rstr: rstr.c cr.h
	
clean:
	rm -f $(targets)
	for s in $(SUBDIRS) ; do \
		( cd $$s ; make clean ) ; \
	done
