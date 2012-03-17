local_dir := process-tree
local_progs := $(addprefix $(local_dir)/,ptree1 pthread1 pthread2 pthread3 pthread4)

progs += $(local_progs)
test_clean += $(addprefix $(local_dir)/,*.o cr_ptree* cr_pthread*)

$(local_progs): CPPFLAGS += -I libcrtest
$(local_progs): LDFLAGS +=  -L libcrtest -lcrtest -pthread
$(local_progs): libcrtest/libcrtest.a
