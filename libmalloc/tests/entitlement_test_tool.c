#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, const char *argv[])
{
	assert(argc >= 1);
	printf("ran %s (pid %d)\n", argv[0], getpid());
	pause();
	return 0;
}
