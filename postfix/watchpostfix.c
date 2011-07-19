#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <syslog.h>
#include <mach/boolean.h>

/*
 * Monitor whether or not something has entered postfix's maildrop and, when
 * it does, start postfix.  When last entry goes away, stop postfix.
 */

#define POSTFIX_DROP	"/Library/Server/Mail/Data/spool/maildrop"	/* APPLE */
#define POSTFIX_FIFO	"/Library/Server/Mail/Data/spool/public/pickup"	/* APPLE */

static int postfix_on = FALSE;

#define START 1
#define STOP 2
static void postfix_control(int);
static int directory_numfiles(char *);

int
main(int argc, char *argv[])
{
	int i, kq, stop_pending = FALSE;
	struct kevent kin, kout;
	struct timespec timeout;

	/* properly daemonize ourselves */
	daemon(0, 0);

	/* Create the submission fifo if it doesn't exist */
	if (access(POSTFIX_FIFO, W_OK))
		if (!mkfifo(POSTFIX_FIFO, 0622))
			(void)chmod(POSTFIX_FIFO, 0622);

	/* If there are files left over from some previous invocation, start postfix*/
	if (directory_numfiles(POSTFIX_DROP))
		postfix_control(START);

	/* Initialize our kevent structures to zero */
	bzero(&kin, sizeof(struct kevent));
	bzero(&kout, sizeof(struct kevent));

	/*
	 * Allocate a kevent for monitoring the directory.  ident is a
	 * file descriptor, filter is a vnode monitor set to watch for
	 * writes.
	 */
	kin.ident = open(POSTFIX_DROP, O_EVTONLY, 0);
	if (kin.ident < 0) {
		syslog(LOG_ERR, "Unable to open postfix drop: %s\n", strerror(errno));
		exit(1);
	}
	kin.filter = EVFILT_VNODE;
	kin.flags = EV_ADD | EV_CLEAR;
	kin.fflags = NOTE_DELETE | NOTE_WRITE;
	kin.data = NULL;
	kin.udata = POSTFIX_DROP;

	/* Create kqueue. */
	kq = kqueue();
	if (kq == -1) {
		syslog(LOG_ERR, "Unable to create kqueue: %s\n", strerror(errno));
		exit(1);
	}

	/*
	 * Input our list of file descriptors to watch.
	 */
	kevent(kq, &kin, 1, NULL, 0, NULL);

	/*
	 * We'll wait for events for 3600 seconds at a time.  One hour should
	 * be a very reasonable "large timeout".
	 */
	timeout.tv_sec = 3600;
	timeout.tv_nsec = 0;

	/*
	 * Big loop to monitor for events.
	 */
	for (;;) {
		/*
		 * Get events.
		 */
		i = kevent(kq, NULL, 0, &kout, 1, &timeout);
		if (i == 0) {
			/* We've timed out after an hour's wait so check status */
			if (stop_pending) {
				/* Did anything sneak in? */
				if (!directory_numfiles(POSTFIX_DROP))
					postfix_control(STOP);
				stop_pending = FALSE;
			}
			continue;
		}
		else if (i == -1) {
			syslog(LOG_ERR, "kevent returned error: %s\n", strerror(errno));
			goto cleanup;
		}

		/* Do appropriate things with file modifications. */
		if (kout.fflags & NOTE_WRITE) {
			i = directory_numfiles(POSTFIX_DROP);
			if (i == 0 && postfix_on == TRUE) {
				/* Time to shut down, so set it up to happen an hour from now */
				stop_pending = TRUE;
			}
			else if (i > 0 && postfix_on == FALSE)
				postfix_control(START);
		}
		else if (kout.fflags & NOTE_DELETE) {
			syslog(LOG_ERR, "Postfix maildrop was deleted! Stopping postfix permanently.\n");
			if (postfix_on == TRUE)
				postfix_control(STOP);
			goto cleanup;
		}
	}

cleanup:
	close(kq);
	exit(1);
}

static void
postfix_control(int what)
{
	pid_t pid;
	int status;

	if (what == START) {
		pid = fork();
		if (pid == 0) {
			execl("/sbin/service", "service", "smtp", "start", NULL);
			syslog(LOG_ERR, "Unable to invoke /sbin/service!\n");
			_exit(1);
		} else {
			/* Should probably do something meaningful with the return status someday */
			(void)waitpid(pid, &status, 0);
			postfix_on = TRUE;
		}
	}
	else if (what == STOP) {
		pid = fork();
		if (pid == 0) {
			execl("/sbin/service", "service", "smtp", "stop", NULL);
			syslog(LOG_ERR, "Unable to invoke /sbin/service!\n");
			_exit(1);
		} else {
			/* Should probably do something meaningful with the return status someday */
			(void)waitpid(pid, &status, 0);
			postfix_on = FALSE;
		}
	}
}

static int
directory_numfiles(char *dirname)
{
	int num = 0;
	DIR *dp;
	struct dirent *de;

	dp = opendir(dirname);
	if (!dp) {
		perror("opendir");
		return 0;
	}
	while ((de = readdir(dp)) != 0) {
		/* A better heuristic for actually determining what postfix
		 * queue files look like should probably go here, just to
		 * eliminate false positives.
		 */
		if (de->d_name[0] != '.')
			++num;
	}
	closedir(dp);
	return num;
}
