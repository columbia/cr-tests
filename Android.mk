#
# Android makefile for Checkpoint/Restart test suite
#

# a note for the future: the 'LOCAL_PATH' variable should _not_ be used
# directly in explicit rule directives or dependencies. I found that this
# variable was being overwritten at invocation of the rule, and assigning a
# different variable (such as ckpt_extract_headers below) fixed the problem.
# However, the variable assignment _must_ be done with ':=' to prevent
# the recursive expansion of variable in the assignment... wonderful really...

LOCAL_PATH := $(call my-dir)

#bionic_fixup_hdr := $(LOCAL_PATH)/include/bionic_libc.h

local_c_includes := $(LOCAL_PATH)/libcrtest $(LOCAL_PATH)/../user
local_cflags := -Wall -Wstrict-prototypes -Wno-trigraphs \
		-DARCH_HAS_ECLONE
local_ldlibs := -lm

ckpt_header_path := $(LOCAL_PATH)/include
ckpt_headers := $(ckpt_header_path)/linux/checkpoint.h \
		$(ckpt_header_path)/linux/checkpoint_hdr.h \
		$(ckpt_header_path)/asm/checkpoint_hdr.h

local_output_subdir := crtests

#
# libcrtest
#
include $(CLEAR_VARS)
LOCAL_MODULE := libcrtest
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := libcrtest/common.c libcrtest/labels.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_CFLAGS += $(local_cflags)
LOCAL_LDLIBS := $(local_ldlibs)
include $(BUILD_STATIC_LIBRARY)

#
# counterloop
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/crcounter
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := counterloop/crcounter.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)

#
# eclone
#
define eclone-test
  $(foreach file,$(1), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_MODULE := $(local_output_subdir)/$(file)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_SRC_FILES := eclone/$(file).c) \
    $(eval LOCAL_C_INCLUDES := $(local_c_includes)) \
    $(eval LOCAL_LDLIBS := $(local_ldlibs)) \
    $(eval LOCAL_STATIC_LIBRARIES := libcrtest libeclone) \
    $(eval include $(BUILD_EXECUTABLE)) \
  )
endef
eclone_tests := eclone-1 eclone-2 eclone-3 eclone-4 eclone-5
$(call eclone-test, $(eclone_tests))

#
# epoll
#
define epoll-test
  $(foreach file,$(1), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_MODULE := $(local_output_subdir)/$(file)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_SRC_FILES := epoll/libeptest.c epoll/$(file).c) \
    $(eval LOCAL_C_INCLUDES := $(local_c_includes)) \
    $(eval LOCAL_LDLIBS := $(local_ldlibs)) \
    $(eval LOCAL_STATIC_LIBRARIES := libcrtest libeclone) \
    $(eval LOCAL_CFFLAGS += -I $(LOCAL_PATH)/epoll) \
    $(eval LOCAL_LDFLAGS += -Xlinker -T -Xlinker $(LOCAL_PATH)/libcrtest/labels.lds) \
    $(eval include $(BUILD_EXECUTABLE)) \
  )
endef
epoll_tests := empty pipe sk10k cycle scm
$(call epoll-test, $(epoll_tests))

#
# eventfd
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/eventfd_rw
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := eventfd/rw.c eventfd/libeptest.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_LDFLAGS += -Xlinker -T -Xlinker $(LOCAL_PATH)/libcrtest/labels.lds
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)

#
# fileio
#
define fileio-test
  $(foreach file,$(1), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_MODULE := $(local_output_subdir)/$(file)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_SRC_FILES := fileio/$(file).c) \
    $(eval LOCAL_C_INCLUDES := $(local_c_includes)) \
    $(eval LOCAL_LDLIBS := $(local_ldlibs)) \
    $(eval LOCAL_STATIC_LIBRARIES := libcrtest libeclone) \
    $(eval include $(BUILD_EXECUTABLE)) \
  )
endef
fileio_tests := fileio1 filelease1 filelease2 filelock1 filelock2 fsetown1
$(call fileio-test, $(fileio_tests))

#
# fs
#
define fs-test
  $(foreach file,$(1), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_MODULE := $(local_output_subdir)/$(file)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_SRC_FILES := fs/libfstest.c fs/$(file).c) \
    $(eval LOCAL_C_INCLUDES := $(local_c_includes)) \
    $(eval LOCAL_LDLIBS := $(local_ldlibs)) \
    $(eval LOCAL_STATIC_LIBRARIES := libcrtest libeclone) \
    $(eval LOCAL_CFFLAGS += -I $(LOCAL_PATH)/fs) \
    $(eval LOCAL_LDFLAGS += -Xlinker -T -Xlinker $(LOCAL_PATH)/libcrtest/labels.lds) \
    $(eval include $(BUILD_EXECUTABLE)) \
  )
endef
fs_tests := file dir do_ckpt fifo
$(call fs-test, $(fs_tests))

#
# fs notify
# (no sigtimedwait in bionic - skipping for now)
#
#include $(CLEAR_VARS)
#LOCAL_MODULE := $(local_output_subdir)/dnotify
#LOCAL_MODULE_TAGS := optional
#LOCAL_SRC_FILES := fs/libfstest.c fs/notify/dnotify.c
#LOCAL_C_INCLUDES := $(local_c_includes)
#LOCAL_LDLIBS := $(local_ldlibs)
#LOCAL_LDFLAGS += -Xlinker -T -Xlinker $(LOCAL_PATH)/libcrtest/labels.lds
#LOCAL_STATIC_LIBRARIES := libcrtest libeclone
#include $(BUILD_EXECUTABLE)


#
# libfutex
#
include $(CLEAR_VARS)
LOCAL_MODULE := libfutex
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := futex/libfutex/libfutex.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_CFLAGS += $(local_cflags)
LOCAL_LDLIBS := $(local_ldlibs)
include $(BUILD_STATIC_LIBRARY)

#
# futex
#
    #$(eval LOCAL_SRC_FILES := futex/libfutex/libfutex.c futex/$(file).c)
define futex-test
  $(foreach file,$(1), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_MODULE := $(local_output_subdir)/$(file)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_SRC_FILES := futex/$(file).c) \
    $(eval LOCAL_C_INCLUDES := $(local_c_includes)) \
    $(eval LOCAL_LDLIBS := $(local_ldlibs)) \
    $(eval LOCAL_STATIC_LIBRARIES := libcrtest libeclone libfutex) \
    $(eval LOCAL_CFFLAGS += -I $(LOCAL_PATH)/futex -I $(LOCAL_PATH)/futex/libfutex) \
    $(eval include $(BUILD_EXECUTABLE)) \
  )
endef
futex_tests := plain robust pi
$(call futex-test, $(futex_tests))

#
# ipc
# (not used, because Android has custom IPC mechanisms)
#
define ipc-test
  $(foreach file,$(1), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_MODULE := $(local_output_subdir)/$(file)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_SRC_FILES := ipc/$(file).c) \
    $(eval LOCAL_C_INCLUDES := $(local_c_includes)) \
    $(eval LOCAL_LDLIBS := $(local_ldlibs)) \
    $(eval LOCAL_STATIC_LIBRARIES := libcrtest libeclone) \
    $(eval include $(BUILD_EXECUTABLE)) \
  )
endef
#ipc_tests := create-sem create-shm check-mq
#$(call ipc-test, $(ipc_tests))

#
# pidns
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/pidns
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := pidns/pidns.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)

#
# process tree
#
define process-test
  $(foreach file,$(1), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_MODULE := $(local_output_subdir)/$(file)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_SRC_FILES := process-tree/$(file).c) \
    $(eval LOCAL_C_INCLUDES := $(local_c_includes)) \
    $(eval LOCAL_LDLIBS := $(local_ldlibs)) \
    $(eval LOCAL_STATIC_LIBRARIES := libcrtest libeclone) \
    $(eval LOCAL_LDFLAGS += -pthread) \
    $(eval include $(BUILD_EXECUTABLE)) \
  )
endef
#process_tests := ptree1 pthread1 pthread2 pthread3 pthread4
process_tests := ptree1 pthread1
$(call process-test, $(process_tests))

#
# ptyloop
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/ptyloop
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := pty/ptyloop.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)

#
# simple
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/simple
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := simple/ckpt.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)

#
# sleep
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/sleeptest
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := sleep/sleeptest.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)

#
# smack
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/smack
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := smack/ckpt.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)

#
# cwd
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/cwdsleep
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := taskfs/cwdsleep.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)

#
# chroot
#
include $(CLEAR_VARS)
LOCAL_MODULE := $(local_output_subdir)/chrootsleep
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := taskfs/chrootsleep.c
LOCAL_C_INCLUDES := $(local_c_includes)
LOCAL_LDLIBS := $(local_ldlibs)
LOCAL_STATIC_LIBRARIES := libcrtest libeclone
include $(BUILD_EXECUTABLE)
