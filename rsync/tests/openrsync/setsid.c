#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	pid_t pid;

	if (argc <= 1) {
		fprintf(stderr, "usage: %s command [...]\n", getprogname());
		return (1);
	}

	argc--;
	argv++;

	/*
	 * If we're already the session leader, we'll fork off to ensure that we
	 * have a fresh session.
	 */
	pid = getpid();
	if (getsid(pid) == pid) {
		pid = fork();
		if (pid == -1)
			err(1, "fork");
		if (pid != 0)
			return (0);	/* All done here */
	}

	if (setsid() == -1)
		err(1, "setsid");

	execvp(argv[0], argv);
	err(1, "execv");
}
