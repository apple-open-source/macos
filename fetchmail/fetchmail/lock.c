/*
 * lock.c -- cross-platform concurrency locking for fetchmail
 *
 * For license terms, see the file COPYING in this directory.
 */
#include "config.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h> /* strcat() */
#endif
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <fcntl.h>
#include <signal.h>

#include "fetchmail.h"
#include "i18n.h"

static char *lockfile;		/* name of lockfile */
static int lock_acquired;	/* have we acquired a lock */

void lock_setup(void)
/* set up the global lockfile name */
{
    /* set up to do lock protocol */
#define	FETCHMAIL_PIDFILE	"fetchmail.pid"
    if (getuid() == ROOT_UID) {
	lockfile = (char *)xmalloc(
		sizeof(PID_DIR) + sizeof(FETCHMAIL_PIDFILE) + 1);
	sprintf(lockfile, "%s/%s", PID_DIR, FETCHMAIL_PIDFILE);
    } else {
	lockfile = (char *)xmalloc(strlen(fmhome)+sizeof(FETCHMAIL_PIDFILE)+2);
	strcpy(lockfile, fmhome);
	strcat(lockfile, "/");
        if (fmhome == home)
 	   strcat(lockfile, ".");
	strcat(lockfile, FETCHMAIL_PIDFILE);
    }
#undef FETCHMAIL_PIDFILE
}

#ifdef HAVE_ON_EXIT
static void unlockit(int n, void *p)
#else
static void unlockit(void)
#endif
/* must-do actions for exit (but we can't count on being able to do malloc) */
{
    if (lockfile && lock_acquired)
	unlink(lockfile);
}

void lock_dispose(void)
/* arrange for a lock to be removed on process exit */
{
#ifdef HAVE_ATEXIT
    atexit(unlockit);
#endif
#ifdef HAVE_ON_EXIT
    on_exit(unlockit, (char *)NULL);
#endif
}

int lock_state(void)
{
    int 	pid, st;
    FILE	*lockfp;
    int		bkgd = FALSE;

    pid = 0;
    if ((lockfp = fopen(lockfile, "r")) != NULL )
    {
	bkgd = (fscanf(lockfp, "%d %d", &pid, &st) == 2);

	if (pid == 0 || kill(pid, 0) == -1) {
	    fprintf(stderr,GT_("fetchmail: removing stale lockfile\n"));
	    pid = 0;
	    unlink(lockfile);
	}
	fclose(lockfp);	/* not checking should be safe, file mode was "r" */
    }

    return(bkgd ? -pid : pid);
}

void lock_assert(void)
/* assert that we already posess a lock */
{
    lock_acquired = TRUE;
}

void lock_or_die(void)
/* get a lock on a given host or exit */
{
    int fd;
    char	tmpbuf[20];

#ifndef O_SYNC
#define O_SYNC	0	/* use it if we have it */
#endif
    if (!lock_acquired)
    {
      if ((fd = open(lockfile, O_WRONLY|O_CREAT|O_EXCL|O_SYNC, 0666)) != -1)
      {
	  sprintf(tmpbuf,"%d", getpid());
	  write(fd, tmpbuf, strlen(tmpbuf));
	  if (run.poll_interval)
	  {
	      sprintf(tmpbuf," %d", run.poll_interval);
	      write(fd, tmpbuf, strlen(tmpbuf));
	  }
	  close(fd);	/* should be safe, fd was opened with O_SYNC */
	  lock_acquired = TRUE;
      }
      else
      {
	  fprintf(stderr,	GT_("fetchmail: lock creation failed.\n"));
	  exit(PS_EXCLUDE);
      }
    }
}

void lock_do_release(void)
/* release a lock on a given host */
{
    unlink(lockfile);
}

/* lock.c ends here */
