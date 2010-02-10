local_dir := libcrtest
local_lib := $(local_dir)/libcrtest.a
local_src := $(addprefix $(local_dir)/,common.c labels.c)
local_objs := $(subst .c,.o,$(local_src))

libs += $(local_lib)
sources += $(local_src)

$(local_lib): $(local_objs)
	$(AR) cr $@ $^

