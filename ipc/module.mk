local_dir := ipc

local_progs := $(addprefix $(local_dir)/,create-sem create-shm check-mq)

progs += $(local_progs)
test_clean += $(addprefix $(local_dir)/,cr_sem* cr_mq* cr_shm*)

$(local_progs): CPPFLAGS += -I libcrtest
$(local_progs): LDFLAGS +=  -L libcrtest -lcrtest
$(local_progs): libcrtest/libcrtest.a
