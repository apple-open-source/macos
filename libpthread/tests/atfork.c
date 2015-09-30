#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <os/assumes.h>
#include <sys/wait.h>

#define DECL_ATFORK(x) \
static void prepare_##x(void) { \
	printf("%d: %s\n", getpid(), __FUNCTION__); \
} \
static void parent_##x(void) { \
	printf("%d: %s\n", getpid(), __FUNCTION__); \
} \
static void child_##x(void) { \
	printf("%d: %s\n", getpid(), __FUNCTION__); \
}

#define ATFORK(x) \
os_assumes_zero(pthread_atfork(prepare_##x, parent_##x, child_##x));

DECL_ATFORK(1);
DECL_ATFORK(2);
DECL_ATFORK(3);
DECL_ATFORK(4);
DECL_ATFORK(5);
DECL_ATFORK(6);
DECL_ATFORK(7);
DECL_ATFORK(8);
DECL_ATFORK(9);
DECL_ATFORK(10);
DECL_ATFORK(11);
DECL_ATFORK(12);
DECL_ATFORK(13);
DECL_ATFORK(14);
DECL_ATFORK(15);
DECL_ATFORK(16);
DECL_ATFORK(17);
DECL_ATFORK(18);
DECL_ATFORK(19);

int main(int argc, char *argv[]) {
	ATFORK(1);
	ATFORK(2);
	ATFORK(3);
	ATFORK(4);
	ATFORK(5);
	ATFORK(6);
	ATFORK(7);
	ATFORK(8);
	ATFORK(9);
	ATFORK(10);
	ATFORK(11);
	ATFORK(12);
	ATFORK(13);
	ATFORK(14);
	ATFORK(15);
	ATFORK(16);
	ATFORK(17);
	ATFORK(18);
	ATFORK(19);

	pid_t pid = fork();
	if (pid == 0) {
		pid = fork(); 
	}
	if (pid == -1) {
		posix_assumes_zero(pid);
	} else if (pid > 0) {
		int status;
		posix_assumes_zero(waitpid(pid, &status, 0));
		posix_assumes_zero(WEXITSTATUS(status));
	}
	return 0;
}
