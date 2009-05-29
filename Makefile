SUBDIRS = libcrtest counterloop fileio simple cr-ipc-test userns ipc \
	  sleep process-tree

targets = ns_exec

all: $(targets)
	for s in $(SUBDIRS) ; do \
		( cd $$s ; make ) ; \
	done

clean:
	rm -f $(targets)
	for s in $(SUBDIRS) ; do \
		( cd $$s ; make clean ) ; \
	done
