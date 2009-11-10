targets = sleeptest

SLIBS = ../libcrtest/libcrtest.a
LIBCMD = -I../libcrtest -L../libcrtest $(SLIBS)

sleeptest: sleeptest.c $(SLIBS)
	gcc -o sleeptest sleeptest.c $(LIBCMD)

all: $(targets)

clean:
	rm -f $(targets) o.*
	rm -rf cr_sleep*
