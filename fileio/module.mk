local_dir := fileio

local_progs := $(addprefix $(local_dir)/,fileio1 filelease1 filelease2 filelock1 filelock2 fsetown1)

progs += $(local_progs)
test_clean += $(addprefix $(local_dir)/,cr_fileio* cr_filelock1* cr_filelease[12]* cr_fsetown1*)

$(local_progs): CPPFLAGS += -I libcrtest
$(local_progs): LDFLAGS +=  -L libcrtest -lcrtest
$(local_progs): libcrtest/libcrtest.a


