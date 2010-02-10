local_dir := counterloop
local_src := $(addprefix $(local_dir)/,crcounter.c)

progs += $(local_dir)/crcounter
test_clean += $(addprefix $(local_dir)/,cr_once* cr_par* cr_ser*)
