targets = ckpt wrap

all: $(targets)

ckpt: ckpt.c ../cr.h
	$(CC) -o ckpt ckpt.c

clean:
	rm -rf $(targets) out context cr-test.out cr-test-module restart wrap
