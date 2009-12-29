SUBDIRS = libcrtest counterloop fileio simple userns ipc sleep \
	  process-tree futex epoll taskfs

targets = ns_exec mysu

all: $(targets)
	for s in $(SUBDIRS) ; do \
		$(MAKE) -C $$s ; \
	done

clean:
	rm -f $(targets)
	for s in $(SUBDIRS) ; do \
		$(MAKE) -C $$s $@ ; \
	done
	rm -rf bashckpt/cr_bash*
