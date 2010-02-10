local_dir := taskfs
local_progs := $(addprefix $(local_dir)/,cwdsleep chrootsleep)

progs += $(local_progs)
test_clean += $(local_dir)/cr_taskfs*

$(local_progs): CFLAGS += -static
$(local_progs): CPPFLAGS += -I libcrtest
$(local_progs): libcrtest/libcrtest.a
