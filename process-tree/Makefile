
targets		= ptree1 pthread1

INCLUDE		= ../libcrtest
LIBCRTEST	= ../libcrtest/common.o

CFLAGS		= -I $(INCLUDE)
LDFLAGS		= $(LIBCRTEST) -lpthread

all: $(LIBCRTEST) $(targets)

clean:
	rm -rf *.o $(targets) checkpoint-* input.data log* pid* snap* test-done data.d
