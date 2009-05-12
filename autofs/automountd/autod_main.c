/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2009 Apple Inc.
 */

#pragma ident	"@(#)autod_main.c	1.69	05/06/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sys/resource.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <locale.h>
#include <assert.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>

#include "automount.h"
#include "automountd.h"
#include "autofs_protServer.h"
#include <arpa/inet.h>
#include "deflt.h"
#include <strings.h>
#include "sysctl_fsid.h"

#define MAXVAL(a, b)	((a) > (b) ? (a) : (b))
#define AUTOFS_MAX_MSG_SIZE \
	MAXVAL(sizeof (union __RequestUnion__autofs_subsystem), \
	    sizeof (union __ReplyUnion__autofs_subsystem))

static void usage(void);
static void *automount_thread(void *);
static void new_worker_thread(void);
static void compute_new_timeout(struct timespec *);
static void *shutdown_thread(void *);
static void *timeout_thread(void *);
static void *wait_for_flush_indication_thread(void *);
static int do_mount_trigger(autofs_pathname, autofs_pathname,
    autofs_pathname, autofs_opts, autofs_pathname, autofs_pathname,
    autofs_component, uint32_t, uint32_t, int32_t, int32_t, int32_t,
    int32_t *, int32_t *, boolean_t *);

#define	CTIME_BUF_LEN 26

/*
 * XXX - this limit is the same as Solaris, although we don't have the
 * System V binary compatibility problem that limits their standard
 * I/O library to a maximum of 256 FILE *'s.  (Now why that affects
 * their automounter is another question....)
 */
#define	MAXTHREADS 64
#define	SHUTDOWN_TIMEOUT  2     /* timeout gets set to this after TERM signal */
#define	TIMEOUT RDDIR_CACHE_TIME  /* Hang around if caches might be valid */
#define	APPLE_PREFIX  "com.apple." /* Bootstrap name prefix */
#define	MAXLABEL	256	/* Max bootstrap name */

static pthread_mutex_t numthreads_lock;
static pthread_cond_t numthreads_cv;
static int numthreads;
static pthread_attr_t attr;	/* To create detached threads */
static time_t timeout = TIMEOUT; /* Seconds to wait before exiting */
static int bye = 0;		/* Force clean shutdown flag. */

static sigset_t waitset;	/* Signals that we wait for */
static sigset_t contset;	/* Signals that we don't exit from */

static mach_port_t service_port_receive_right;

#define	RESOURCE_FACTOR 8

struct autodir *dir_head;
struct autodir *dir_tail;

time_t timenow;
int verbose = 0;
int trace = 0;
int automountd_nobrowse = 0;
char *automountd_defopts = NULL;

/*
 * This daemon is to be started by launchd, as such it follows the following
 * launchd rules:
 *	We don't:
 *		call daemon(3)
 *		call fork and having the parent process exit
 *		change uids or gids.
 *		set up the current working directory or chroot.
 *		set the session id
 * 		change stdio to /dev/null.
 *		call setrusage(2)
 *		call setpriority(2)
 *		Ignore SIGTERM.
 *	We are launched on demand
 *		and we catch SIGTERM to exit cleanly.
 *
 * In practice daemonizing in the classic unix sense would probably be ok
 * since we get invoked by traffic on a task_special_port, but we will play
 * by the rules; it's even easier, to boot.
 */


int
main(argc, argv)
	int argc;
	char *argv[];

{
	int c, error;
	int autofs_fd;
	kern_return_t ret;
	pthread_t thread, timeout_thr, shutdown_thr;
	char *defval;
	int defflags;
	char bname[MAXLABEL] = { APPLE_PREFIX };
	char *myname;

	/* 
	 * If launchd is redirecting these two files they'll be block-
	 * buffered, as they'll be pipes, or some other such non-tty,
	 * sending data to launchd. Probably not what you want.
	 */
	setlinebuf(stdout);
	setlinebuf(stderr);

	/* Figure out our bootstrap name based on what we are called. */

	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	strlcat(bname, myname, sizeof(bname));

	/*
	 * Read in the values from config file first before we check
	 * commandline options so the options override the file.
	 */
	if ((defopen(AUTOFSADMIN)) == 0) {
		if ((defval = defread("AUTOMOUNTD_VERBOSE=")) != NULL) {
			if (strncasecmp("true", defval, 4) == 0)
				verbose = TRUE;
			else
				verbose = FALSE;
		}
		if ((defval = defread("AUTOMOUNTD_NOBROWSE=")) != NULL) {
			if (strncasecmp("true", defval, 4) == 0)
				automountd_nobrowse = TRUE;
			else
				automountd_nobrowse = FALSE;
		}
		if ((defval = defread("AUTOMOUNTD_TRACE=")) != NULL) {
			errno = 0;
			trace = strtol(defval, (char **)NULL, 10);
			if (errno != 0)
				trace = 0;
		}
		if ((defval = defread("AUTOMOUNTD_ENV=")) != NULL) {
			(void) putenv(strdup(defval));
			defflags = defcntl(DC_GETFLAGS, 0);
			TURNON(defflags, DC_NOREWIND);
			defflags = defcntl(DC_SETFLAGS, defflags);
			while ((defval = defread("AUTOMOUNTD_ENV=")) != NULL)
				(void) putenv(strdup(defval));
			(void) defcntl(DC_SETFLAGS, defflags);
		}
		if ((defval = defread("AUTOMOUNTD_MNTOPTS=")) != NULL
		    && *defval != '\0') {
		    	automountd_defopts = strdup(defval);
			if (automountd_defopts == NULL) {
				syslog(LOG_ERR, "Memory allocation failed: %m");
				exit(2);
			}
		}

		/* close defaults file */
		defopen(NULL);
	}

	while ((c = getopt(argc, argv, "vnTo:D:")) != EOF) {
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'n':
			automountd_nobrowse++;
			break;
		case 'T':
			trace++;
			break;
		case 'o':
			if (automountd_defopts != NULL)
				free(automountd_defopts);
			automountd_defopts = strdup(optarg);
			break;
		case 'D':
			(void) putenv(optarg);
			break;
		default:
			usage();
		}
	}

	openlog(myname, LOG_PID, LOG_DAEMON);
	(void) setlocale(LC_ALL, "");

	/*
	 * This is platform-dependent; for now, we just say
	 * "macintosh" - I guess we could do something to
	 * distinguish x86 from PowerPC.
	 */
	if (getenv("ARCH") == NULL)
		(void) putenv("ARCH=macintosh");
	if (getenv("CPU") == NULL) {
#if defined(__ppc__)
		(void) putenv("CPU=powerpc");
#elif defined(__i386__)
		(void) putenv("CPU=i386");
#else
		syslog(LOG_ERR, "can't determine processor type");
#endif
	}

	/*
	 * We catch
	 *
	 *	SIGINT - if we get one, we just drive on;
	 *	SIGHUP - if we get one, we log a message
	 *		 so the user knows that it doesn't
	 *		 do anything useful, and drive on;
	 *	SIGTERM - we quit.
	 */
	sigemptyset(&waitset);
	sigaddset(&waitset, SIGINT);
	sigaddset(&waitset, SIGHUP);
	contset = waitset;
	sigaddset(&waitset, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &waitset, NULL);
	
	/*
	 * We create most threads as detached threads, so any Mach
	 * resources they have are reclaimed when they terminate.
	 */
	(void) pthread_attr_init(&attr);
	(void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	(void) pthread_mutex_init(&numthreads_lock, NULL);
	(void) pthread_cond_init(&numthreads_cv, NULL);
	(void) pthread_rwlock_init(&cache_lock, NULL);
	(void) pthread_rwlock_init(&rddir_cache_lock, NULL);
	(void) pthread_mutex_init(&gssd_port_lock, NULL);

	/*
	 * initialize the name services, use NULL arguments to ensure
	 * we don't initialize the stack of files used in file service
	 *
	 * XXX - do we need to do something equivalent here?
	 */
#if 0
	(void) ns_setup(NULL, NULL);
#endif

	/*
	 * Attempt to open the autofs device to establish ourselves
	 * as an automounter;
	 *
	 * We hold it open as long as we're running; if we exit,
	 * that'll close it, so autofs will forget that we were
	 * the automounter.
	 *
	 * This device also delivers notifications that we should
	 * flush our caches.
	 */
	autofs_fd = open("/dev/" AUTOFS_DEVICE, O_RDONLY);
	if (autofs_fd == -1) {
		syslog(LOG_ERR, "Can't open /dev/autofs: %s",
		    strerror(errno));
		exit(2);
	}

	/*
	 * Check in with launchd to get the receive right.
	 * N.B. Since we're using a task special port, if launchd
	 * does not have the receive right we can't get it.
	 * And since we should always be started by launchd
	 * this should always succeed.
	 */
	ret = bootstrap_check_in(bootstrap_port, bname, &service_port_receive_right);
	if (ret != BOOTSTRAP_SUCCESS) {
		syslog(LOG_ERR, "Could not get receive right for %s: %s (%d)\n",
				bname, bootstrap_strerror(ret), ret);
		exit(EXIT_FAILURE);
	}

	/* Create signal handling thread */
	error = pthread_create(&shutdown_thr, &attr, shutdown_thread, NULL);
	if (error) {
		syslog(LOG_ERR, "unable to create shutdown thread: %s", strerror(error));
		exit(EXIT_FAILURE);
	}

	/* Create time out thread */
	error = pthread_create(&timeout_thr, NULL, timeout_thread, NULL);
	if (error) {
		syslog(LOG_ERR, "unable to create time out thread: %s", strerror(error));
		exit(EXIT_FAILURE);
	}

	/*
	 * Create cache_cleanup thread
	 */
	error = pthread_create(&thread, &attr, cache_cleanup, NULL);
	if (error) {
		syslog(LOG_ERR, "unable to create cache_cleanup thread: %s",
		    strerror(error));
		exit(1);
	}

	/*
	 * Create wait-for-cache-flush-indication thread.
	 */
	error = pthread_create(&thread, &attr, wait_for_flush_indication_thread,
	    &autofs_fd);
	if (error) {
		syslog(LOG_ERR, "unable to create wait-for-flush-indication thread: %s",
		    strerror(error));
		exit(1);
	}

	/*
	 * Start a worker thread to listen for a message.
	 * Then just spin.
	 *
	 * This is a bit ugly.  If there were a version of mach_msg_server()
	 * that spun off a thread before calling the dispatch routine,
	 * and had that thread handle the message, we could have this
	 * thread run that routine, with workers created as messages arrive.
	 */
	new_worker_thread();

	/* Wait for time out */
	pthread_join(timeout_thr, NULL);

	pthread_attr_destroy(&attr);

	/*
	 * Wake up the "wait for flush indication" thread.
	 */
	if (ioctl(autofs_fd, AUTOFS_NOTIFYCHANGE, 0) == -1)
		pr_msg("AUTOFS_NOTIFYCHANGE failed: %m");
	
	return (EXIT_SUCCESS);
}

static void
usage(void)
{
	(void) fprintf(stderr, "Usage: automountd\n"
		"\t[-o opts]\t\t(default mount options)\n"
		"\t[-T]\t\t(trace requests)\n"
		"\t[-v]\t\t(verbose error msgs)\n"
		"\t[-D n=s]\t(define env variable)\n");
	exit(1);
	/* NOTREACHED */
}

/*
 * Receive one message and process it.
 */
static void *
automount_thread(__unused void *arg)
{
	kern_return_t ret;

	ret = mach_msg_server_once(autofs_server, AUTOFS_MAX_MSG_SIZE,
	    service_port_receive_right,
	    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_SENDER) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	if (ret != KERN_SUCCESS) {
		syslog(LOG_ERR, "automounter mach_msg_server_once failed: %s",
		    mach_error_string(ret));
	}
	return NULL;
}

/*
 * Wait until we have fewer than the maximum number of worker threads,
 * and then create one running automount_thread().
 *
 * Called by the dispatch routines just before processing a message,
 * so we're listening for messages even while processing a message,
 * as long as we aren't out of threads.
 */
static void
new_worker_thread(void)
{
	pthread_t thread;
	int error;

	(void) pthread_mutex_lock(&numthreads_lock);
	while (bye == 0 && numthreads >= MAXTHREADS)
		(void) pthread_cond_wait(&numthreads_cv, &numthreads_lock);
	if (bye)
		goto out;
	numthreads++;
	error = pthread_create(&thread, &attr, automount_thread, NULL);
	if (error) {
		syslog(LOG_ERR, "unable to create worker thread: %s",
		    strerror(error));
		numthreads--;
	}
out:	
	(void) pthread_mutex_unlock(&numthreads_lock);
}

/*
 * This worker thread is terminating; reduce the count of worker threads,
 * and, if it's dropped below the maximum, wake up anybody waiting for
 * it to drop below the maximum.
 *
 * Called by the dispatch routines just before returning.
 */
static void
end_worker_thread(void)
{
	(void) pthread_mutex_lock(&numthreads_lock);
	numthreads--;
	if (numthreads < MAXTHREADS)
		pthread_cond_signal(&numthreads_cv);
	(void) pthread_mutex_unlock(&numthreads_lock);
}

/*
 * Thread that handles signals for us and will tell the timeout thread to
 * shut us down if we get a signal that we don't continue for. We set a global
 * variable bye and the timeout value to SHUTDOWN_TIMEOUT and wake every
 * body up. Threads blocked in new_worker_thread will see bye is set and exit.
 * We set timeout to SHUTDOWN_TIMEOUT for the timeout thread, so that threads
 * executing dispatch routines have an opportunity to finish.
 */
static void*
shutdown_thread(__unused void *arg)
{
	int sig;

	do {
		sigwait(&waitset, &sig);
		switch (sig) {
		case SIGHUP:
			/*
			 * The old automounter supported a SIGHUP
			 * to allow it to resynchronize internal
			 * state with the /etc/mnttab.
			 * This is no longer relevant, but we
			 * need to catch the signal and warn
			 * the user.
			 */

			syslog(LOG_ERR, "SIGHUP received: ignored");
			break;
		}
	} while (sigismember(&contset, sig));

	pthread_mutex_lock(&numthreads_lock);
	bye = 1;
	/*
 	 * Wait a little bit for dispatch threads to complete.
	 */
	timeout = SHUTDOWN_TIMEOUT;
	/*
	 * Force the timeout_thread and all the rest to to wake up and exit.
	 */
	pthread_cond_broadcast(&numthreads_cv);
	pthread_mutex_unlock(&numthreads_lock);

	return (NULL);
}

static void
compute_new_timeout(struct timespec *new)
{
	struct timeval current;

	gettimeofday(&current, NULL);
	new->tv_sec = current.tv_sec + timeout;
	new->tv_nsec = 1000 * current.tv_usec;
}

static void*
timeout_thread(__unused void *arg)
{
	int rv = 0;
	struct timespec exittime;

	(void) pthread_mutex_lock(&numthreads_lock);

	/*
	 * Wait until the timer given to pthread_cond_timedwait()
	 * expires (which causes it to return ETIMEDOUT rather
	 * than 0) and we have no worker threads running, or
	 * until bye was set to 1 by the shutdown thread (telling
	 * us to terminate) and the timer expires (we shut down
	 * after SHUTDOWN_TIMEOUT in that case).
	 */
	while ((rv == 0 || numthreads > 1) && bye < 2) {
		compute_new_timeout(&exittime);
		/*
		 * If the shutdown thread has told us to exit (bye == 1),
		 * then increment bye so that we will exit after
		 * SHUTDOWN_TIMEOUT.
		 */
		if (bye)
			bye++;
		rv = pthread_cond_timedwait(&numthreads_cv,
					&numthreads_lock, &exittime);
	}

	(void) pthread_mutex_unlock(&numthreads_lock);

	return (NULL);
}

static void *
wait_for_flush_indication_thread(void *arg)
{
	int *autofs_fdp = arg;

	/*
	 * This thread waits for an indication that we should flush
	 * our caches.  It quits if bye >= 1, meaning we're shutting
	 * down.
	 */
	for (;;) {
		/*
		 * Check whether we're shutting down.
		 */
		pthread_mutex_lock(&numthreads_lock);
		if (bye >= 1) {
			pthread_mutex_unlock(&numthreads_lock);
			break;
		}
		pthread_mutex_unlock(&numthreads_lock);

		if (ioctl(*autofs_fdp, AUTOFS_WAITFORFLUSH, 0) == -1) {
			if (errno != EINTR) {
				syslog(LOG_ERR,
				    "AUTOFS_WAITFORFLUSH failed: %s",
				    strerror(errno));
			}
		} else
			flush_caches();
	}
	return (NULL);
}

kern_return_t
autofs_readdir(__unused mach_port_t server, autofs_pathname rda_map,
    uint64_t rda_offset, uint32_t rda_count, int *status, uint64_t *rddir_offset,
    boolean_t *rddir_eof, byte_buffer *rddir_entries,
    mach_msg_type_number_t *rddir_entriesCnt, security_token_t token)
{
	new_worker_thread();

	/*
	 * Reject this if the sender wasn't root
	 * (all messages from the kernel will be from root).
	 */
	if (token.val[0] != 0) {
		*status  = EPERM;
		end_worker_thread();
		return KERN_SUCCESS;
	}

	if (trace > 0)
		trace_prt(1, "READDIR REQUEST   : %s @ %llu\n",
                rda_map, rda_offset);

	*status = do_readdir(rda_map, rda_offset, rda_count, rddir_offset,
	    rddir_eof, rddir_entries, rddir_entriesCnt);

	if (trace > 0)
		trace_prt(1, "READDIR REPLY	: status=%d\n", *status);

	end_worker_thread();

	return KERN_SUCCESS;
}

kern_return_t
autofs_unmount(__unused mach_port_t server, int32_t fsid_val0,
    int32_t fsid_val1, autofs_pathname mntresource,
    autofs_pathname mntpnt, autofs_component fstype, autofs_opts mntopts,
    int *status, security_token_t token)
{
	new_worker_thread();

	/*
	 * Reject this if the sender wasn't root
	 * (all messages from the kernel will be from root).
	 */
	if (token.val[0] != 0) {
		*status  = EPERM;
		end_worker_thread();
		return KERN_SUCCESS;
	}

	if (trace > 0) {
		char ctime_buf[CTIME_BUF_LEN];
		if (ctime_r(&timenow, ctime_buf) == NULL)
			ctime_buf[0] = '\0';

		trace_prt(1, "UNMOUNT REQUEST: %s", ctime_buf);
		trace_prt(1, " resource=%s fstype=%s mntpnt=%s"
			" mntopts=%s\n",
			mntresource,
			fstype,
			mntpnt,
			mntopts);
	}

	*status = do_unmount1(fsid_val0, fsid_val1, mntresource,
	    mntpnt, fstype, mntopts);

	if (trace > 0)
		trace_prt(1, "UNMOUNT REPLY: status=%d\n", *status);

	end_worker_thread();

	return KERN_SUCCESS;
}

kern_return_t
autofs_lookup(__unused mach_port_t server, autofs_pathname map,
    autofs_pathname path, autofs_component name, mach_msg_type_number_t nameCnt,
    autofs_pathname subdir, autofs_opts opts, boolean_t isdirect,
    uint32_t sendereuid, int *err, int *lu_action, boolean_t *lu_verbose,
    security_token_t token)
{
	char *key;

	new_worker_thread();

	/*
	 * Reject this if the sender wasn't root
	 * (all messages from the kernel will be from root).
	 */
	if (token.val[0] != 0) {
		*err = EPERM;
		*lu_verbose = 0;
		end_worker_thread();
		return KERN_SUCCESS;
	}

	/*
	 * The name component is a counted string; make a
	 * null-terminated string out of it.
	 */
	if (nameCnt < 1 || nameCnt > MAXNAMLEN) {
		*err = ENOENT;
		*lu_verbose = 0;
		end_worker_thread();
		return KERN_SUCCESS;
	}
	key = malloc(nameCnt + 1);
	if (key == NULL) {
		*err = ENOMEM;
		*lu_verbose = 0;
		end_worker_thread();
		return KERN_SUCCESS;
	}
	memcpy(key, name, nameCnt);
	key[nameCnt] = '\0';

	if (trace > 0) {
		char ctime_buf[CTIME_BUF_LEN];
		if (ctime_r(&timenow, ctime_buf) == NULL)
			ctime_buf[0] = '\0';

		trace_prt(1, "LOOKUP REQUEST: %s", ctime_buf);
		trace_prt(1, "  name=%s[%s] map=%s opts=%s path=%s direct=%d\n",
			key, subdir, map, opts, path, isdirect);
	}

	*err = do_lookup1(map, key, subdir, opts, isdirect, sendereuid,
	    lu_action);
	*lu_verbose = verbose;
	free(key);

	if (trace > 0)
		trace_prt(1, "LOOKUP REPLY    : status=%d\n", *err);

	end_worker_thread();

	return KERN_SUCCESS;
}

kern_return_t
autofs_mount(__unused mach_port_t server, autofs_pathname map,
    autofs_pathname path, autofs_component name, mach_msg_type_number_t nameCnt,
    autofs_pathname subdir, autofs_opts opts, boolean_t isdirect,
    uint32_t sendereuid, mach_port_t gssd_port, int *mr_type,
    byte_buffer *actions, mach_msg_type_number_t *actionsCnt,
    int *err, boolean_t *mr_verbose, security_token_t token)
{
	char *key;
	int status;

	new_worker_thread();

	/*
	 * Reject this if the sender wasn't root
	 * (all messages from the kernel will be from root).
	 */
	if (token.val[0] != 0) {
		*mr_type = AUTOFS_DONE;
		*err  = EPERM;
		*mr_verbose = 0;
		end_worker_thread();
		return KERN_SUCCESS;
	}

	/*
	 * The name component is a counted string; make a
	 * null-terminated string out of it.
	 */
	if (nameCnt < 1 || nameCnt > MAXNAMLEN) {
		*mr_type = AUTOFS_DONE;
		*err = ENOENT;
		*mr_verbose = 0;
		end_worker_thread();
		return KERN_SUCCESS;
	}
	key = malloc(nameCnt + 1);
	if (key == NULL) {
		*mr_type = AUTOFS_DONE;
		*err = ENOMEM;
		*mr_verbose = 0;
		end_worker_thread();
		return KERN_SUCCESS;
	}
	memcpy(key, name, nameCnt);
	key[nameCnt] = '\0';

	if (trace > 0) {
		char ctime_buf[CTIME_BUF_LEN];
		if (ctime_r(&timenow, ctime_buf) == NULL)
			ctime_buf[0] = '\0';

		trace_prt(1, "MOUNT REQUEST:   %s", ctime_buf);
		trace_prt(1, "  name=%s[%s] map=%s opts=%s path=%s direct=%d\n",
			key, subdir, map, opts, path, isdirect);
	}

	status = do_mount1(map, key, subdir, opts, path, isdirect, sendereuid,
	    gssd_port, actions, actionsCnt);
	if (status == 0 && *actionsCnt != 0)
		*mr_type = AUTOFS_ACTION;
	else
		*mr_type = AUTOFS_DONE;

	*err = status;
	*mr_verbose = verbose;

	if (trace > 0) {
		switch (*mr_type) {
		case AUTOFS_ACTION:
			trace_prt(1,
				"MOUNT REPLY    : status=%d, AUTOFS_ACTION\n",
				status);
			break;
		case AUTOFS_DONE:
			trace_prt(1,
				"MOUNT REPLY    : status=%d, AUTOFS_DONE\n",
				status);
			break;
		default:
			trace_prt(1, "MOUNT REPLY    : status=%d, UNKNOWN\n",
				status);
		}
	}

	if (status && verbose) {
		if (isdirect) {
			/* direct mount */
			syslog(LOG_ERR, "mount of %s failed", path);
		} else {
			/* indirect mount */
			syslog(LOG_ERR,
				"mount of %s/%s failed", path, key);
		}
	}
	free(key);

	end_worker_thread();

	return KERN_SUCCESS;
}

kern_return_t
autofs_mount_trigger(__unused mach_port_t server,
    autofs_pathname mntpt, autofs_pathname submntpt,
    autofs_pathname path, autofs_opts opts,
    autofs_pathname map, autofs_pathname subdir,
    autofs_component key, uint32_t flags, uint32_t mntflags,
    int32_t mount_to, int32_t mach_to, int32_t direct,
    int32_t *fsid_val0, int32_t *fsid_val1, boolean_t *top_level,
    int *err, security_token_t token)
{
	new_worker_thread();

	/*
	 * Reject this if the sender wasn't root
	 * (all messages from the kernel will be from root).
	 */
	if (token.val[0] != 0) {
		*err = EPERM;
		end_worker_thread();
		return KERN_SUCCESS;
	}

	*err = do_mount_trigger(mntpt, submntpt, path, opts, map, subdir, key,
	    flags, mntflags, mount_to, mach_to, direct, fsid_val0, fsid_val1,
	    top_level);

	if (*err)
		syslog(LOG_ERR, "trigger mount on %s failed: %s", mntpt,
		    strerror(*err));

	end_worker_thread();

	return KERN_SUCCESS;
}

static int
do_mount_trigger(autofs_pathname mntpt, autofs_pathname submntpt,
    autofs_pathname path, autofs_opts opts,
    autofs_pathname map, autofs_pathname subdir,
    autofs_component key, uint32_t flags, uint32_t mntflags,
    int32_t mount_to, int32_t mach_to, int32_t direct,
    int32_t *fsid_val0, int32_t *fsid_val1, boolean_t *top_level)
{
	struct stat statb;
	struct autofs_args mnt_args;
	struct statfs buf;
	int err;

	/*
	 * Check whether this is a symlink; Solaris does this entirely
	 * in the kernel, and looks up the trigger mount point with
	 * a no-follow lookup, but we can't do that, so we have to
	 * check this ourselves.
	 *
	 * (We don't want to be tricked by sneaky servers into mounting
	 * a trigger on top of, for example, "/etc".)
	 */
	if (lstat(mntpt, &statb) == 0) {
		if (S_ISLNK(statb.st_mode)) {
			syslog(LOG_ERR, "%s symbolic link: not a valid mountpoint - mount failed",
			    mntpt);
			return (ENOENT);
		}
	}

	mnt_args.version = AUTOFS_ARGSVERSION;
	mnt_args.path = path;
	mnt_args.opts = opts;
	mnt_args.map = map;
	mnt_args.subdir = subdir;
	mnt_args.key = key;
	mnt_args.mntflags = mntflags;
	mnt_args.mount_to = mount_to;
	mnt_args.mach_to = mach_to;
	mnt_args.direct = direct;
	mnt_args.trigger = 1;		/* special trigger submount */

	if (mount(MNTTYPE_AUTOFS, mntpt, flags|MNT_AUTOMOUNTED|MNT_DONTBROWSE,
	    &mnt_args) == -1)
		return (errno);
	/*
	 * XXX - what if somebody unmounts the mount out from under us?
	 */
	if (statfs(mntpt, &buf) == -1) {
		err = errno;
		/*
		 * XXX - if the unmount fails, we're screwed - we can't
		 * succeed, and we can't undo the mount.
		 * It "shouldn't happen", though - but it could, if,
		 * for example, we're mounting on a soft-mounted NFS
		 * mount and it times out.
		 */
		unmount(mntpt, MNT_FORCE);
		return (err);
	}
	*fsid_val0 = buf.f_fsid.val[0];
	*fsid_val1 = buf.f_fsid.val[1];
	*top_level = (strcmp(submntpt, ".") == 0);
	return (0);
}

kern_return_t
autofs_check_thishost(__unused mach_port_t server, autofs_component name,
    mach_msg_type_number_t nameCnt, boolean_t *is_us, security_token_t token)
{
	new_worker_thread();

	/*
	 * Reject this if the sender wasn't root
	 * (all messages from the kernel will be from root).
	 */
	if (token.val[0] != 0) {
		*is_us = 0;
		end_worker_thread();
		return KERN_SUCCESS;
	}

	*is_us = host_is_us(name, nameCnt);
	end_worker_thread();
	return KERN_SUCCESS;
}

kern_return_t
autofs_check_trigger(__unused mach_port_t server, autofs_pathname map,
    autofs_pathname path, autofs_component name, mach_msg_type_number_t nameCnt,
    autofs_pathname subdir, autofs_opts opts, boolean_t isdirect,
    int *err, boolean_t *istrigger, security_token_t token)
{
	char *key;
	int status;

	new_worker_thread();

	/*
	 * Reject this if the sender wasn't root
	 * (all messages from the kernel will be from root).
	 */
	if (token.val[0] != 0) {
		*err  = EPERM;
		end_worker_thread();
		return KERN_SUCCESS;
	}

	/*
	 * The name component is a counted string; make a
	 * null-terminated string out of it.
	 */
	if (nameCnt < 1 || nameCnt > MAXNAMLEN) {
		*err = ENOENT;
		end_worker_thread();
		return KERN_SUCCESS;
	}
	key = malloc(nameCnt + 1);
	if (key == NULL) {
		*err = ENOMEM;
		end_worker_thread();
		return KERN_SUCCESS;
	}
	memcpy(key, name, nameCnt);
	key[nameCnt] = '\0';

	if (trace > 0) {
		char ctime_buf[CTIME_BUF_LEN];
		if (ctime_r(&timenow, ctime_buf) == NULL)
			ctime_buf[0] = '\0';

		trace_prt(1, "CHECK TRIGGER REQUEST:   %s", ctime_buf);
		trace_prt(1, "  name=%s[%s] map=%s opts=%s path=%s direct=%d\n",
			key, subdir, map, opts, path, isdirect);
	}

	status = do_check_trigger(map, key, subdir, opts, path, isdirect, istrigger);

	*err = status;

	if (trace > 0) {
		trace_prt(1, "CHECK TRIGGER REPLY    : status=%d, istrigger = %d\n",
			status, istrigger);
	}

	if (status && verbose) {
		if (isdirect) {
			/* direct mount */
			syslog(LOG_ERR, "check trigger of %s failed", path);
		} else {
			/* indirect mount */
			syslog(LOG_ERR,
				"check trigger of %s/%s failed", path, key);
		}
	}
	free(key);

	end_worker_thread();

	return KERN_SUCCESS;
}

/*
 * Used for reporting messages from code
 * shared with automount command.
 * Formats message into a buffer and
 * calls syslog.
 *
 * Print an error.
 * Works like printf (fmt string and variable args)
 * except that it will subsititute an error message
 * for a "%m" string (like syslog).
 */
void
pr_msg(const char *fmt, ...)
{
	va_list ap;
	char fmtbuff[BUFSIZ], buff[BUFSIZ];
	const char *p1;
	char *p2;

	p2 = fmtbuff;
#if 0
	fmt = gettext(fmt);
#endif

	for (p1 = fmt; *p1; p1++) {
		if (*p1 == '%' && *(p1+1) == 'm') {
			(void) strcpy(p2, strerror(errno));
			p2 += strlen(p2);
			p1++;
		} else {
			*p2++ = *p1;
		}
	}
	if (p2 > fmtbuff && *(p2-1) != '\n')
		*p2++ = '\n';
	*p2 = '\0';

	va_start(ap, fmt);
	(void) vsprintf(buff, fmtbuff, ap);
	va_end(ap);
	syslog(LOG_ERR, buff);
}
