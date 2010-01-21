targets = fileio1 filelock1 filelease1

INCLUDE   = ../libcrtest
LIBCRTEST = ../libcrtest/common.o
CFLAGS    = -I $(INCLUDE)
LDFLAGS   = $(LIBCRTEST)

all: $(LIBCRTEST) $(targets)

clean:
	rm -f $(targets)
	rm -rf cr_fileio* cr_filelock1* cr_filelease1*
