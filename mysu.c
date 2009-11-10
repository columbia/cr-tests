#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

int main(int argc, char *argv[])
{
	char *newarg, **newargv;
	int ret, newuid;
	struct passwd *pe;

	if (argc < 3) {
		printf("Usage: %s <username> <command> ...\n", argv[0]);
		return(1);
	}
	pe = getpwnam(argv[1]);
	if (!pe) {
		perror("getpwnam");
		return(1);
	}
	newuid = pe->pw_uid;
	ret = setuid(newuid);
	if (ret) {
		perror("setuid");
		return(1);
	}
	if (getuid() != newuid) {
		printf("Error doing setuid to %d\n", newuid);
		return(1);
	}
	newarg = argv[2];
	newargv = &argv[2];
	return execvp(newarg, newargv);
}
