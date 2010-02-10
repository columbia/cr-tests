local_dir := futex
local_progs := $(addprefix $(local_dir)/,plain robust pi)

MACHINE := $(shell gcc -dumpmachine)
ifeq ($(MACHINE:i386-%=i386),i386)
ARCHOPTS := -march=i486
endif

progs += $(local_progs)
test_clean += $(addprefix $(local_dir)/,*.o cr_futex*)

$(local_progs): CFLAGS += $(ARCHOPTS)
$(local_progs): CPPFLAGS += -I . -I $(local_dir)/libfutex
$(local_progs): libcrtest/libcrtest.a $(local_dir)/libfutex/libfutex.a

