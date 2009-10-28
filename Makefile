SUBDIRS = libcrtest counterloop fileio simple userns ipc sleep \
	  process-tree futex epoll

targets = ns_exec

all: $(targets)
	for s in $(SUBDIRS) ; do \
		$(MAKE) -C $$s ; \
	done

clean:
	rm -f $(targets)
	for s in $(SUBDIRS) ; do \
		$(MAKE) -C $$s $@ ; \
	done
