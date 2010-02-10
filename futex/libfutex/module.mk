local_dir := futex/libfutex
local_lib := $(local_dir)/libfutex.a
local_src := $(addprefix $(local_dir)/,libfutex.c)
local_objs := $(subst .c,.o,$(local_src))

libs += $(local_lib)
sources += $(local_src)

$(local_lib): $(local_objs)
	$(AR) cr $@ $^

