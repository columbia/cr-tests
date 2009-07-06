SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=%.o)

CFLAGS += -I./ # LTP -I../../../../include

TARGET := libfutex.a

all: $(TARGET)

libfutex.a: $(OBJS)
	$(AR) -cr $@ libfutex.o

clean:
	rm -f $(TARGET) $(OBJS)

install:
