local_dir := pidns
local_progs := $(local_dir)/pidns

progs += $(local_progs)
test_clean += $(addprefix $(local_dir)/,*.o checkpoint* mypid* pidns outpid)

