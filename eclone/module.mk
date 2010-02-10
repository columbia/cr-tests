local_dir := eclone
local_progs := $(addprefix $(local_dir)/,eclone-1 eclone-2 eclone-3 eclone-4 eclone-5)

progs += $(local_progs)

USER_CR_DIR ?= ../user-cr

$(local_progs): CPPFLAGS += -I $(USER_CR_DIR)
$(local_progs): $(USER_CR_DIR)/libeclone.a
