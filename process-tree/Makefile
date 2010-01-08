
targets		= ptree1 pthread1 pthread2 pthread3 pthread4

INCLUDE		= ../libcrtest
LIBCRTEST	= ../libcrtest/common.o

CFLAGS		= -I $(INCLUDE)
LDFLAGS		= $(LIBCRTEST) -lpthread

all: $(LIBCRTEST) $(targets)

clean:
	rm -rf *.o $(targets) cr_ptree* cr_pthread*
