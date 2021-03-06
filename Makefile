# Executables
progs       :=

# Source files that are explicitly compiled to .o
sources     :=

# Libraries/archives
libs        :=

# Other build outputs that aren't automatically collected into $(progs),
# $(objects), or $(libs)
extra_clean :=

# Test results and outputs
test_clean  :=

objects = $(subst .c,.o,$(sources))

modules := $(subst /module.mk,,$(shell find . -name module.mk))

INSTALL_LOC ?= /tmp/crtest

# allow user to supplement CFLAGS (e.g. -m64), but always build with -Wall.
override CFLAGS += -Wall

all:

# Prefix CC, AS, LD, AR for cross compilation
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AS = $(CROSS_COMPILE)as
AR = $(CROSS_COMPILE)ar

include $(addsuffix /module.mk,$(modules))

progs += mysu

.PHONY: all
all: $(progs)

.PHONY: libs
libs: $(libs)

.PHONY: clean
clean:
	$(RM) $(objects) $(progs) $(libs) $(extra_clean)

.PHONY: testclean
testclean:
	$(RM) -r $(test_clean)

.PHONY: install
install:
	@echo "Copying files to: $(INSTALL_LOC)"
	@find $(modules) -executable -type f -exec cp --parents {} $(INSTALL_LOC) \;
	@cp common.sh $(INSTALL_LOC)
	@cp runall.sh $(INSTALL_LOC)
	@tar -cf cr-test.tar -C $(INSTALL_LOC) .
	@echo "Done"
