
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <c.h>
#include <unistd.h>

#include "autofs.h"

static char usage[] = "usage: %s [-f device-string] node";

static char gKextLoadCommand[] = "/sbin/kextload";
static char gKextLoadPath[] = "/System/Library/Extensions/autofs.kext";

static int LoadAutoFS(void);

int
main(int argc, char **argv)
{
	int ch;
	int error;
	bool retried;
	const char *devicename = NULL;
	autofs_mnt_args *mnt_args = NULL;
	char mount_path[PATH_MAX];
	char *path;

	while ((ch = getopt(argc, argv, "f:")) != EOF) {
		switch (ch) {
			case 'f':
				devicename = optarg;
				break;

			default:
				fprintf(stderr, "%s: unknown option '-%c'.\n", getprogname(), ch);
				fprintf(stderr, usage, getprogname());
				exit(1);
		};
	};
	argc -= optind;
	argv += optind;
    
	if (argc < 1)
		errx(1, usage, getprogname());
	
	path = realpath(argv[0], mount_path);
	if (path == NULL) {
		err(1, "couldn't resolve mount path", strerror(errno));
		/* NOT REACHED */
		return 1;
	};

	if (devicename) {
		mnt_args = calloc(1, sizeof(*mnt_args));
		if (mnt_args == NULL) {
			err(ENOMEM, "Couldn't allocate mount argument structure");
		};
		/* Avoid buffer overflows regardless of command-line input: */
		strncpy(mnt_args->devicename, devicename, sizeof(mnt_args->devicename)-1);
		mnt_args->devicename[sizeof(mnt_args->devicename)-1] = (char)0;
		mnt_args->hdr.mnt_args_size = sizeof(autofs_mnt_args_hdr) + strlen(mnt_args->devicename) + 1;
	};
		
	retried = false;
	while (true) {
		error = mount("autofs", mount_path, MNT_DONTBROWSE, mnt_args);
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
