#
# libeclone.a is built/installed from user-cr git-tree. Point USER_CR_DIR
# to the directory containing genstack.h
#
USER_CR_DIR ?= ../../user-cr

CFLAGS = -Wall -I $(USER_CR_DIR)

LDFLAGS = -L $(USER_CR_DIR)

LIB_ECLONE = -leclone

PROGS = eclone-1 eclone-2 eclone-3 eclone-4 eclone-5

all: $(PROGS)

$(PROGS): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LIB_ECLONE) $(LDFLAGS)

clean:
	rm -f *.o $(PROGS)
