local_dir := userns
local_progs := $(addprefix $(local_dir)/,userns_ckptme usertask sbits userns_deep simple_deep)

progs += $(local_progs)
test_clean += $(addprefix $(local_dir)/,o.* psout* sandbox ckpt.out out.* outfile cr_depth* cr_sbits* cr_simple* cr_userns* cr_usertask* checkpointed finished started pidfile)

$(local_progs): CPPFLAGS += -I libcrtest
$(local_progs): LDFLAGS +=  -L libcrtest -lcrtest
$(local_progs): libcrtest/libcrtest.a
