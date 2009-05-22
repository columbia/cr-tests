
#define CKPT_READY		"checkpoint-ready"
#define CKPT_DONE		"checkpoint-done"
#define TEST_DONE		"test-done"

extern FILE *logfp;

struct record {
	int id;
	char data[256];
};

