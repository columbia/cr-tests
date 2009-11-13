TARGETS := libcrtest.a
all: $(TARGETS)

common.o: libcrtest.h
labels.o: labels.h

libcrtest.a: common.o labels.o
	ar cr $@ $^

clean:
	rm -f *.o $(TARGETS)
