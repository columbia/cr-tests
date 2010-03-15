local_dir := eventfd
local_progs := $(addprefix $(local_dir)/,rw)

sources += $(addprefix $(local_dir)/,libeptest.c)
progs += $(local_progs)
test_clean += $(addprefix $(local_dir)/,*.o cr_eventfd*)

$(local_progs): CFLAGS += $(ARCHOPTS)
$(local_progs): CPPFLAGS += -I .
$(local_progs): LDFLAGS += -Xlinker -dT -Xlinker libcrtest/labels.lds
$(local_progs): libcrtest/libcrtest.a $(local_dir)/libeptest.o

extra_clean += $(local_dir)/libeptest.o $(local_dir)/libeptest.a
