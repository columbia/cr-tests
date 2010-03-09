local_dir := epoll
local_progs := $(addprefix $(local_dir)/,empty pipe sk10k cycle scm)

sources += $(addprefix $(local_dir)/,libeptest.c)
progs += $(local_progs)
test_clean += $(local_dir)/cr_epoll*

# epoll tests include libcrtest/libcrtest.h
$(local_progs): CPPFLAGS += -I .
$(local_progs): LDFLAGS += -Xlinker -dT -Xlinker libcrtest/labels.lds
$(local_progs): $(local_dir)/libeptest.o libcrtest/libcrtest.a

extra_clean += $(local_dir)/libeptest.o $(local_dir)/libeptest.a
