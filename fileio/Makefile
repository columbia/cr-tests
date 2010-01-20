targets = fileio1 filelock1

INCLUDE   = ../libcrtest
LIBCRTEST = ../libcrtest/common.o
CFLAGS    = -I $(INCLUDE)
LDFLAGS   = $(LIBCRTEST)

all: $(LIBCRTEST) $(targets)

clean:
	rm -f $(targets)
	rm -rf cr_fileio*
