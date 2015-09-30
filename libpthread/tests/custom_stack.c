#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <os/assumes.h>

void *function(void *arg) {
	// Use the stack...
	char buffer[BUFSIZ];
	strlcpy(buffer, arg, sizeof(buffer));
	strlcat(buffer, arg, sizeof(buffer));

	printf("%s", buffer);
	sleep(30);
	return (void *)(uintptr_t)strlen(buffer);
}

int main(int argc, char *argv[]) {
	char *arg = "This is a test and only a test of the pthread stackaddr system.\n";
	size_t stacksize = 4096 * 5;
	uintptr_t stackaddr = (uintptr_t)valloc(stacksize);
	stackaddr += stacksize; // address starts at top of stack.

	pthread_t thread;
	pthread_attr_t attr;

	os_assumes_zero(pthread_attr_init(&attr));
	os_assumes_zero(pthread_attr_setstacksize(&attr, stacksize));
	os_assumes_zero(pthread_attr_setstackaddr(&attr, (void *)stackaddr));

	os_assumes_zero(pthread_create(&thread, &attr, function, arg));

	void *result;
	os_assumes_zero(pthread_join(thread, &result));
	os_assumes((uintptr_t)result == (uintptr_t)strlen(arg)*2);

	return 0;
}
