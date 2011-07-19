
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <stdbool.h>
#include <unistd.h>

#include "autofs.h"

static char usage[] = "usage: %s [-d] mount_point opts map subdir key";

static char gKextLoadCommand[] = "/sbin/kextload";
static char gKextLoadPath[] = "/System/Library/Extensions/autofs.kext";

static int LoadAutoFS(void);

int
main(int argc, char **argv)
{
	int ch;
	int error;
	bool retried;
	struct autofs_args mnt_args;
	char mount_path[PATH_MAX];
	char *path;
	int32_t direct = 0;

	while ((ch = getopt(argc, argv, "dt:")) != EOF) {
		switch (ch) {

		case 'd':
			direct = 1;
			break;

		default:
			fprintf(stderr, "%s: unknown option '-%c'.\n", getprogname(), ch);
			fprintf(stderr, usage, getprogname());
			return (EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
    
	if (argc < 5)
		errx(1, usage, getprogname());
	
	path = realpath(argv[0], mount_path);
	if (path == NULL) {
		err(1, "couldn't resolve mount path: %s", strerror(errno));
		/* NOT REACHED */
		return 1;
	};

	mnt_args.version = AUTOFS_ARGSVERSION;
	mnt_args.path = path;
	mnt_args.opts = argv[1];
	mnt_args.map = argv[2];
	mnt_args.subdir = argv[3];
	mnt_args.key = argv[4];
	mnt_args.direct = direct;
	mnt_args.mount_type = MOUNT_TYPE_MAP;	/* top-level autofs mount */
		
	retried = false;
	while (true) {
		error = mount("autofs", mount_path,
		    MNT_DONTBROWSE|MNT_AUTOMOUNTED, &mnt_args);
		if (error == 0) {
			break;
		} else if (retried) {
			perror("mount");
			break;
		} else {
			retried = true;
			(void)LoadAutoFS();
		}
	}
	
	return (error == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int
LoadAutoFS(void)
{
	pid_t pid, terminated_pid;
	int result;
	union wait status;
    
	pid = fork();
	if (pid == 0) {
		result = execl(gKextLoadCommand, gKextLoadCommand, "-q", gKextLoadPath, NULL);
		/* IF WE ARE HERE, WE WERE UNSUCCESSFUL */
		return result ? result : ECHILD;
	}

	if (pid == -1) {
		result = -1;
		goto Err_Exit;
	}

	/* Success! Wait for completion in-line here */
	while ( (terminated_pid = wait4(pid, (int *)&status, 0, NULL)) < 0 ) {
		/* retry if EINTR, else break out with error */
		if ( errno != EINTR ) {
			break;
		}
	}
    
	if ((terminated_pid == pid) && (WIFEXITED(status))) {
		result = WEXITSTATUS(status);
	} else {
		result = -1;
	}
#if DEBUG_TRACE
	syslog(LOG_INFO, "LoadAutoFS: result of fork / exec = %d.\n", result);
#endif

Err_Exit:
	return result;
}
