local_dir := sleep

local_progs := $(addprefix $(local_dir)/,sleeptest)

progs += $(local_progs)
test_clean += $(addprefix $(local_dir)/,cr_sleep* o.*)

$(local_progs): CPPFLAGS += -I libcrtest
$(local_progs): LDFLAGS +=  -L libcrtest -lcrtest
$(local_progs): libcrtest/libcrtest.a
