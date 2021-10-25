#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int ac, char *av[]) {
	char *av_[] = { "/usr/local/libexec/sshd-keygen-wrapper", 0, 0 };
	for (int i = 0; i < ac; i++) {
		if (0 == strcmp(av[i], "-s")) {
			av_[1] = "-s";
			break;
		}
	}
	execv(av_[0], &av_[0]);
	abort();
}
