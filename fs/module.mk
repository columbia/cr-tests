local_dir := fs
local_progs := $(addprefix $(local_dir)/, file dir do_ckpt fifo)

sources += $(addprefix $(local_dir)/,libeptest.c)
progs += $(local_progs)
test_clean += $(local_dir)/cr_fs*

# fs tests include libcrtest/libcrtest.h
$(local_progs): CFLAGS += -D_GNU_SOURCE=1
$(local_progs): CPPFLAGS += -I .
$(local_progs): LDFLAGS += -Xlinker -dT -Xlinker libcrtest/labels.lds
$(local_progs): $(local_dir)/libfstest.o libcrtest/libcrtest.a
