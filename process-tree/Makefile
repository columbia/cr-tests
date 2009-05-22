
targets		= ptree1

INCLUDE		= ../libcrtest
LIBCRTEST	= ../libcrtest/common.o

CFLAGS		= -I $(INCLUDE)
LDFLAGS		= $(LIBCRTEST) -lpthread

all: $(LIBCRTEST) $(targets)

clean:
	rm -f *.o $(targets)
