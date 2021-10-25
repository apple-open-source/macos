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
 * Portions Copyright 2007-2017 Apple Inc.
 */

#include <sys/syslog.h>
#pragma ident	"@(#)autod_main.c	1.69	05/06/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/resource.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <locale.h>
#include <vproc.h>
#include <assert.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <servers/bootstrap.h>
#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#include <os/transaction_private.h>

#include "automount.h"
#include "automountd.h"
#include "autofs_protServer.h"
#include <arpa/inet.h>
#include "deflt.h"
#include <strings.h>
#include "sysctl_fsid.h"

#include <os/log.h>
#define MAXVAL(a, b)	((a) > (b) ? (a) : (b))
#define AUTOFS_MAX_MSG_SIZE \
	MAXVAL(sizeof (union __RequestUnion__autofs_subsystem), \
	    sizeof (union __ReplyUnion__autofs_subsystem))

static void usage(void);
static void compute_new_timeout(struct timespec *);
static void *shutdown_thread(void *);
static void *timeout_thread(void *);
static void *wait_for_flush_indication_thread(void *);
static int get_audit_token_pid(audit_token_t *atoken);
static int do_mount_subtrigger(autofs_pathname, autofs_pathname,
    autofs_pathname, autofs_opts, autofs_pathname, autofs_pathname,
    autofs_component, uint32_t, uint32_t, int32_t, fsid_t *, boolean_t *);

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
#define MIG_MAX_REPLY_SIZE 65536 /* Largest reply is returned from autofs_readdir() */

os_log_t automountd_logger;
static int numthreads;
static pthread_attr_t attr;	/* To create detached threads */
static time_t timeout = TIMEOUT; /* Seconds to wait before exiting */
static int bye = 0;		/* Force clean shutdown flag. */

static sigset_t waitset;	/* Signals that we wait for */
static sigset_t contset;	/* Signals that we don't exit from */

static mach_port_t service_port_receive_right;
static dispatch_mach_t public_mach_channel;

static int autofs_fd;

#define	RESOURCE_FACTOR 8

struct autodir *dir_head;
struct autodir *dir_tail;

time_t timenow;
int verbose = 0;
int trace = 0;
int automountd_nobrowse = 0;
int automountd_nosuid = TRUE;
char *automountd_defopts = NULL;

/* dispatch related */
#define QUEUE_NAME_PREFIX "com.apple.automountd.wq_"
#define QUEUE_NAME_LEN 30
dispatch_queue_t *work_queues = NULL;
static int current_worker_queue = 0;
int num_cpus;

void
parse_config_file()
{
	int defflags;
	char *defval;

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
			trace = (int)strtol(defval, (char **)NULL, 10);
			if (errno != 0)
				trace = 0;
		}
		if ((defval = defread("AUTOMOUNTD_ENV=")) != NULL) {
			char *tmp = strdup(defval);
			if (putenv(tmp)) {
				free(tmp);
			}
			/* if putenv() is successfull, we lost ownership of the string */
			tmp = NULL;

			defflags = defcntl(DC_GETFLAGS, 0);
			TURNON(defflags, DC_NOREWIND);
			defflags = defcntl(DC_SETFLAGS, defflags);
			while ((defval = defread("AUTOMOUNTD_ENV=")) != NULL) {
				tmp = strdup(defval);
				if (putenv(tmp)) {
					free(tmp);
				}
				/* if putenv() is successfull, we lost ownership of the string */
				tmp = NULL;
			}
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
		if ((defval = defread("AUTOMOUNTD_NOSUID=")) != NULL) {
			if (strncasecmp("true", defval, 4) == 0)
				automountd_nosuid = TRUE;
			else
				automountd_nosuid = FALSE;
		}

		/* close defaults file */
		defopen(NULL);
	}
}

void
parse_args(int argc, char **argv)
{
	int c;

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
}

void
determine_platform()
{
	/*
	 * This is platform-dependent; for now, we just say
	 * "macintosh".
	 * XXX Unclear what we should do for iOS.
	 */
	if (getenv("ARCH") == NULL)
		(void) putenv("ARCH=macintosh");
	if (getenv("CPU") == NULL) {
#if defined(__i386__) || defined(__x86_64__)
		/*
		 * At least on Solaris, "CPU" appears to be the
		 * narrowest ISA the machine supports, with
		 * "NATISA" being the widest, so "CPU" is "i386"
		 * even on x86-64.
		 */
		(void) putenv("CPU=i386");
#elif defined(__arm64__)
		(void) putenv("CPU=arm");
#else
#error "can't determine processor type");
#endif
	}
}

void
open_autofs_fd()
{
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
}

void
signal_handler(void *context)
{
	bye = 2;

	/*
	 * Cancel the mach channel - prevent further messages from being received.
	 * Port deallocation will happen in the channel's handler when
	 * DISPATCH_MACH_DISCONNECTED is received.
	 */
	dispatch_mach_cancel(public_mach_channel);
}

struct worker_context {
	void *context;
	dispatch_mach_reason_t reason;
	dispatch_mach_msg_t message;
	mach_error_t error;
};

static void
mach_channel_handler(void *context, dispatch_mach_reason_t reason,
	dispatch_mach_msg_t message, mach_error_t error)
{
    kern_return_t kr;
	/*
	 * Check if we got a SIGTERM. If so, deallocate mach port and we're done.
	 * Else, dispatch the work to one of the worker queues in a round-robin.
	 */
	if (reason == DISPATCH_MACH_CANCELED) {
		/*
		 * Wake up the "wait for flush indication" thread.
		 */
		if (ioctl(autofs_fd, AUTOFS_NOTIFYCHANGE, 0) == -1)
			pr_msg(LOG_ERR, "AUTOFS_NOTIFYCHANGE failed: %m");
        kr = mach_port_mod_refs(mach_task_self(), service_port_receive_right, MACH_PORT_RIGHT_RECEIVE, -1);
        if (!kr) {
            syslog(LOG_ERR, "mach_port_mod_refs failed (%d)\n", kr);
        }
		return;
	}
	if (message) {
		dispatch_retain((dispatch_object_t)message);
	}
	dispatch_async(work_queues[current_worker_queue], ^ {
		static const struct mig_subsystem *subsystems[] = {
			(mig_subsystem_t)&autofs_subsystem,
		};
		os_transaction_t trans;

		switch (reason) {
		case DISPATCH_MACH_MESSAGE_RECEIVED:
			trans = os_transaction_create("com.apple.automountd.transaction");
			if (!dispatch_mach_mig_demux(context, subsystems, 1, message)) {
				syslog(LOG_ERR, "dispatch_mach_mig_demux failed, %m");
				mach_msg_destroy(dispatch_mach_msg_get_msg(message, NULL));
			} else {
			}
			os_release(trans);
			break;
		default:
			syslog(LOG_ERR, "Unsupported message type %lu", reason);
			break;
		}
		if (message) {
			dispatch_release(message);
		}
		});
	current_worker_queue = (current_worker_queue + 1) % num_cpus; 
}

/*
 * This daemon is to be started by launchd, as such it follows the following
 * launchd rules:
 *	We don't:
 *		call daemon(3)
 *		call fork and having the parent process exit
 *		change uids or gids.
 *		set up the current working directory or chroot.
 *		set the session id
 *		change stdio to /dev/null.
 *		call setrusage(2)
 *		call setpriority(2)
 *		Ignore SIGTERM.
 *	We are launched on demand
 *		and we catch SIGTERM and use vproc_transactions to exit cleanly.
 *
 * In practice daemonizing in the classic unix sense would probably be ok
 * since we get invoked by traffic on a task_special_port, but we will play
 * by the rules; it's even easier, to boot.
 */

int
main(int argc, char *argv[])
{
	char bname[MAXLABEL] = { APPLE_PREFIX };
	dispatch_workloop_t main_workloop;
	dispatch_source_t signal_source;
	kern_return_t ret;
	pthread_t thread;
	size_t num_len;
	char *myname;
	int error;
	int i;

	automountd_logger = os_log_create("com.apple.filesystem.automountd", "automountd");

	/* Figure out our bootstrap name based on what we are called. */
	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	strlcat(bname, myname, sizeof(bname));

	/*
	 * Read in the values from config file first before we check
	 * commandline options so the options override the file.
	 */
	parse_config_file();
	parse_args(argc, argv);

	openlog(myname, LOG_PID, LOG_DAEMON);
	(void) setlocale(LC_ALL, "");

	if (trace > 0)
		trace_prt(1, "%s running", myname);

	determine_platform();

	/*
	 * We create most threads as detached threads, so any Mach
	 * resources they have are reclaimed when they terminate.
	 */
	(void) pthread_attr_init(&attr);
	(void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	(void) pthread_rwlock_init(&cache_lock, NULL);
	(void) pthread_rwlock_init(&rddir_cache_lock, NULL);

	open_autofs_fd();

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
	    NULL);
	if (error) {
		syslog(LOG_ERR, "unable to create wait-for-flush-indication thread: %s",
		    strerror(error));
		exit(1);
	}

	/*
	 * Get number of CPUs. We'll create numcpus serial queues to execute work
	 * during upcalls to get maximum concurrency.
	 */
	num_len = sizeof(num_cpus);
	error = sysctlbyname("hw.logicalcpu", &num_cpus, &num_len, NULL, 0);

	if (error == -1) {
		syslog(LOG_ERR, "Failed to retrieve number of CPUs %m, setting to 4");
		num_cpus = 4;
	}

	if (trace > 2) {
		trace_prt(1, "Number of logical CPUs is %d", num_cpus);
	}

	/*
	 * Create the main queue. Not using the daemon's main queue because it's
	 * considered an anti-pattern.
	 * Using a workloop (subclass of a queue) to allow priority ordering, so
	 * a signal is handled before mach messages but mach messages are still
	 * handled FIFO.
	 */
	main_workloop = dispatch_workloop_create("com.apple.automountd.main_workloop");
	if (main_workloop == NULL) {
		syslog(LOG_ERR, "Failed to create main main workloop\n");
		exit(1);
	}

	/* Create mach channel to receive upcalls */
	public_mach_channel = dispatch_mach_create_f("com.apple.automountd.upcall_channel",
		main_workloop, NULL, mach_channel_handler);
	dispatch_set_qos_class_fallback(public_mach_channel, QOS_CLASS_DEFAULT);

	/* Create a SOURCE_SIGNAL to catch signals */
	signal_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM,
		0, main_workloop);
	/* Set handler for the signal source */
	dispatch_source_set_event_handler_f(signal_source, signal_handler);
	dispatch_resume(signal_source);

	/*
	 * Create the array of serial queues to dispatch kernel upcalls to demux
	 * the messages
	 */
	work_queues = (dispatch_queue_t*)calloc(num_cpus, sizeof(dispatch_queue_t));
	if (work_queues == NULL) {
		syslog(LOG_ERR, "Failed to allocate work queues array %m");
		exit(1);
	}
	/* Create each work queue */
	for (i = 0; i < num_cpus; i++) {
		char q_name[QUEUE_NAME_LEN];
		snprintf(q_name, QUEUE_NAME_LEN, "%s%d", QUEUE_NAME_PREFIX, i);
		work_queues[i] = dispatch_queue_create(q_name, DISPATCH_QUEUE_SERIAL);
	}

	/* Connect the mach channel to start receiving messages */
	dispatch_mach_connect(public_mach_channel, service_port_receive_right,
		MACH_PORT_NULL, NULL);
	dispatch_main();

	/* We should never reach this */
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

static void
compute_new_timeout(struct timespec *new)
{
	struct timeval current;

	gettimeofday(&current, NULL);
	new->tv_sec = current.tv_sec + timeout;
	new->tv_nsec = 1000 * current.tv_usec;
}

static void *
wait_for_flush_indication_thread(__unused void *arg)
{
	/*
	 * This thread waits for an indication that we should flush
	 * our caches.  It quits if bye >= 1, meaning we're shutting
	 * down.
	 */
	pthread_setname_np("wait for flush indication");
	for (;;) {
		/*
		 * Check whether we're shutting down.
		 */
		if (bye >= 1) {
			break;
		}

		if (ioctl(autofs_fd, AUTOFS_WAITFORFLUSH, 0) == -1) {
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

static pid_t
get_audit_token_pid(audit_token_t *atoken)
{
	if (atoken) {
		pid_t remote_pid = -1;
		audit_token_to_au32(*atoken, /* audit UID */ NULL, /* euid */ NULL, /* egid */ NULL, /* ruid */ NULL, /* rgid */ NULL, &remote_pid, /* au_asid_t */ NULL, /* au_tid_t */ NULL);
		return remote_pid;
	}
	return -1;
}

kern_return_t
autofs_readdir(__unused mach_port_t server,
	       autofs_pathname rda_map,				/* IN */
	       int64_t rda_offset,				/* IN */
	       uint32_t rda_count,				/* IN */
	       int *status,					/* OUT */
	       int64_t *rddir_offset,				/* OUT */
	       boolean_t *rddir_eof,				/* OUT */
	       byte_buffer *rddir_entries,			/* OUT */
	       mach_msg_type_number_t *rddir_entriesCnt,	/* OUT */
	       audit_token_t atoken)
{
	/* Sanitize our OUT parameters */
	*status = EPERM;
	*rddir_offset = 0;
	*rddir_eof = TRUE;
	*rddir_entries = NULL;
	*rddir_entriesCnt = 0;

	/*
	 * Reject this if the sender wasn't a kernel process
	 * (all messages from the kernel must have zero pid in audit_token_t).
	 */
	if (get_audit_token_pid(&atoken) != 0) {
		goto out;
	}

	if (trace > 0) {
		trace_prt(1, "READDIR REQUEST   : %.1024s @ %llu\n", rda_map, rda_offset);
	}

	*status = do_readdir(rda_map,
			     rda_offset,
			     rda_count,
			     rddir_offset,
			     rddir_eof,
			     rddir_entries,
			     rddir_entriesCnt);

	if (trace > 0) {
		trace_prt(1, "READDIR REPLY	: status=%d\n", *status);
	}

out:
	return KERN_SUCCESS;
}

kern_return_t
autofs_readsubdir(__unused mach_port_t server,
		  autofs_pathname rda_map,			/* IN */
		  autofs_component rda_name,			/* IN */
		  mach_msg_type_number_t rda_nameCnt,		/* IN */
		  autofs_pathname rda_subdir,			/* IN */
		  autofs_opts rda_mntopts,			/* IN */
		  uint32_t rda_parentino,			/* IN */
		  int64_t rda_offset,				/* IN */
		  uint32_t rda_count,				/* IN */
		  int *status,					/* OUT */
		  int64_t *rddir_offset,			/* OUT */
		  boolean_t *rddir_eof,				/* OUT */
		  byte_buffer *rddir_entries,			/* OUT */
		  mach_msg_type_number_t *rddir_entriesCnt,	/* OUT */
		  audit_token_t atoken)
{
	char *key;

	/* Sanitize our OUT parameters */
	*status = EPERM;
	*rddir_offset = 0;
	*rddir_eof = TRUE;
	*rddir_entries = NULL;
	*rddir_entriesCnt = 0;

	/*
	 * Reject this if the sender wasn't a kernel process
	 * (all messages from the kernel must have zero pid in audit_token_t).
	 */
	if (get_audit_token_pid(&atoken) != 0) {
		goto out;
	}

	/*
	 * The name component is a counted string; make a
	 * null-terminated string out of it.
	 */
	if (rda_nameCnt < 1 || rda_nameCnt > MAXNAMLEN) {
		*status = ENOENT;
		goto out;
	}
	key = malloc(rda_nameCnt + 1);
	if (key == NULL) {
		*status = ENOMEM;
		goto out;
	}
	memcpy(key, rda_name, rda_nameCnt);
	key[rda_nameCnt] = '\0';

	if (trace > 0)
		trace_prt(1, "READSUBDIR REQUEST : name=%s[%s] map=%s @ %llu\n",
		key, rda_subdir, rda_map, rda_offset);

	*status = do_readsubdir(rda_map,
				key,
				rda_subdir,
				rda_mntopts,
				rda_parentino,
				rda_offset,
				rda_count,
				rddir_offset,
				rddir_eof,
				rddir_entries,
				rddir_entriesCnt);
	free(key);

	if (trace > 0)
		trace_prt(1, "READSUBDIR REPLY   : status=%d\n", *status);

out:
	return KERN_SUCCESS;
}

kern_return_t
autofs_unmount(__unused mach_port_t server,
	       fsid_t mntpnt_fsid,				/* IN */
	       autofs_pathname mntresource,			/* IN */
	       autofs_pathname mntpnt,				/* IN */
	       autofs_component fstype,				/* IN */
	       autofs_opts mntopts,				/* IN */
	       int *status,					/* OUT */
	       audit_token_t atoken)
{
	/* Sanitize our OUT parameters */
	*status = EPERM;

	/*
	 * Reject this if the sender wasn't a kernel process
	 * (all messages from the kernel must have zero pid in audit_token_t).
	 */
	if (get_audit_token_pid(&atoken) != 0) {
		goto out;
	}

	if (trace > 0) {
		trace_prt(1, "UNMOUNT REQUEST: resource=%s fstype=%s mntpnt=%s mntopts=%s\n",
			mntresource, fstype, mntpnt, mntopts);
	}

	*status = do_unmount1(mntpnt_fsid,
			      mntresource,
			      mntpnt,
			      fstype,
			      mntopts);

	if (trace > 0)
		trace_prt(1, "UNMOUNT REPLY: status=%d\n", *status);

out:
	return KERN_SUCCESS;
}

kern_return_t
autofs_lookup(__unused mach_port_t server,
	      autofs_pathname map,				/* IN */
	      autofs_pathname path,				/* IN */
	      autofs_component name,				/* IN */
	      mach_msg_type_number_t nameCnt,			/* IN */
	      autofs_pathname subdir,				/* IN */
	      autofs_opts opts,					/* IN */
	      boolean_t isdirect,				/* IN */
	      uint32_t sendereuid,				/* IN */
	      int *err,						/* OUT */
	      int *node_type,					/* OUT */
	      boolean_t *lu_verbose,				/* OUT */
	      audit_token_t atoken)
{
	char *key;

	/* Sanitize our OUT parameters */
	*err = EPERM;
	*node_type = 0;
	*lu_verbose = 0;

	/*
	 * Reject this if the sender wasn't a kernel process
	 * (all messages from the kernel must have zero pid in audit_token_t).
	 */
	if (get_audit_token_pid(&atoken) != 0) {
		goto out;
	}

	/*
	 * The name component is a counted string; make a
	 * null-terminated string out of it.
	 */
	if (nameCnt < 1 || nameCnt > MAXNAMLEN) {
		*err = ENOENT;
		goto out;
	}
	key = malloc(nameCnt + 1);
	if (key == NULL) {
		*err = ENOMEM;
		goto out;
	}
	memcpy(key, name, nameCnt);
	key[nameCnt] = '\0';

	if (trace > 0) {
		trace_prt(1, "LOOKUP REQUEST: name=%s[%s] map=%s opts=%s path=%s direct=%d uid=%u\n",
			key, subdir, map, opts, path, isdirect, sendereuid);
	}

	*err = do_lookup1(map, key, subdir, opts, isdirect, sendereuid,
	    node_type);
	*lu_verbose = verbose;
	free(key);

	if (trace > 0)
		trace_prt(1, "LOOKUP REPLY  : status=%d\n", *err);

out:
	return KERN_SUCCESS;
}

kern_return_t
autofs_mount(__unused mach_port_t server,
	     autofs_pathname map,				/* IN */
	     autofs_pathname path,				/* IN */
	     autofs_component name,				/* IN */
	     mach_msg_type_number_t nameCnt,			/* IN */
	     autofs_pathname subdir,				/* IN */
	     autofs_opts opts,					/* IN */
	     boolean_t isdirect,				/* IN */
	     boolean_t issubtrigger,				/* IN */
	     fsid_t mntpnt_fsid,				/* IN */
	     uint32_t sendereuid,				/* IN */
	     int32_t asid,					/* IN */
	     int *mr_type,					/* OUT */
	     fsid_t *fsidp,					/* OUT */
	     uint32_t *retflags,				/* OUT */
	     byte_buffer *actions,				/* OUT */
	     mach_msg_type_number_t *actionsCnt,		/* OUT */
	     int *err,						/* OUT */
	     boolean_t *mr_verbose,				/* OUT */
	     audit_token_t atoken)
{
	char *key;
	int status;
	static time_t prevmsg = 0;

	os_log_debug(automountd_logger, "autofs_mount started");
	/* Sanitize our OUT parameters */
	*err = EPERM;
	*mr_type = AUTOFS_DONE;
	memset(fsidp, 0, sizeof(*fsidp));
	*retflags = 0;	/* what we call sets retflags as needed */
	*actions = NULL;
	*actionsCnt = 0;
	*mr_verbose = FALSE;

	/*
	 * Reject this if the sender wasn't a kernel process
	 * (all messages from the kernel must have zero pid in audit_token_t).
	 */
	if (get_audit_token_pid(&atoken) != 0) {
		goto out;
	}

	/*
	 * The name component is a counted string; make a
	 * null-terminated string out of it.
	 */
	if (nameCnt < 1 || nameCnt > MAXNAMLEN) {
		*err = ENOENT;
		goto out;
	}
	key = malloc(nameCnt + 1);
	if (key == NULL) {
		*err = ENOMEM;
		goto out;
	}
	memcpy(key, name, nameCnt);
	key[nameCnt] = '\0';

	if (trace > 0) {
		trace_prt(1, "MOUNT  REQUEST: name=%s [%s] map=%s opts=%s path=%s direct=%d\n",
			key, subdir, map, opts, path, isdirect);
	}

	status = do_mount1(map,
			   key,
			   subdir,
			   opts,
			   path,
			   isdirect,
			   issubtrigger,
			   mntpnt_fsid,
			   sendereuid,
			   asid,
			   fsidp,
			   retflags,
			   actions,
			   actionsCnt);

	if (status == 0 && *actionsCnt != 0)
		*mr_type = AUTOFS_ACTION;
	else
		*mr_type = AUTOFS_DONE;

	*err = status;
	*mr_verbose = verbose;

	if (trace > 0) {
		switch (*mr_type) {
		case AUTOFS_ACTION:
			trace_prt(1, "MOUNT  REPLY  : status=%d, AUTOFS_ACTION\n", status);
			break;
		case AUTOFS_DONE:
			trace_prt(1, "MOUNT  REPLY  : status=%d, AUTOFS_DONE\n", status);
			break;
		default:
			trace_prt(1, "MOUNT  REPLY  : status=%d, UNKNOWN\n", status);
		}
	}

	/*
	 * Report a failed mount.
	 * Failed mounts can come in bursts of dozens of
	 * these messages - so limit to one in 5 sec interval.
	 */
	if (status && prevmsg < time((time_t *) NULL)) {
		prevmsg = time((time_t *) NULL) + 5;
		if (isdirect) {
			/* direct mount */
			syslog(LOG_ERR, "mount of %s%s failed: %s", path,
				issubtrigger ? "" : subdir, strerror(status));
		} else {
			/* indirect mount */
			syslog(LOG_ERR,
				"mount of %s/%s%s failed: %s", path, key,
				issubtrigger ? "" : subdir, strerror(status));
		}
	}
	free(key);

out:
	return KERN_SUCCESS;
}

kern_return_t
autofs_mount_subtrigger(__unused mach_port_t server,
			autofs_pathname mntpt,			/* IN */
			autofs_pathname submntpt,		/* IN */
			autofs_pathname path,			/* IN */
			autofs_opts opts,			/* IN */
			autofs_pathname map,			/* IN */
			autofs_pathname subdir,			/* IN */
			autofs_component key,			/* IN */
			uint32_t flags,				/* IN */
			uint32_t mntflags,			/* IN */
			int32_t direct,				/* IN */
			fsid_t *fsidp,				/* OUT */
			boolean_t *top_level,			/* OUT */
			int *err,				/* OUT */
			audit_token_t atoken)
{
	/* Sanitize our OUT parameters */
	*err = EPERM;
	memset(fsidp, 0, sizeof(*fsidp));
	*top_level = FALSE;

	/*
	 * Reject this if the sender wasn't a kernel process
	 * (all messages from the kernel must have zero pid in audit_token_t).
	 */
	if (get_audit_token_pid(&atoken) != 0) {
		goto out;
	}

	*err = do_mount_subtrigger(mntpt,
				   submntpt,
				   path,
				   opts,
				   map,
				   subdir,
				   key,
				   flags,
				   mntflags,
				   direct,
				   fsidp,
				   top_level);

	if (*err)
		syslog(LOG_ERR, "subtrigger mount on %s failed: %s", mntpt,
		    strerror(*err));

out:
	return KERN_SUCCESS;
}

static int
do_mount_subtrigger(autofs_pathname mntpt,
		    autofs_pathname submntpt,
		    autofs_pathname path,
		    autofs_opts opts,
		    autofs_pathname map,
		    autofs_pathname subdir,
		    autofs_component key,
		    uint32_t flags,
		    uint32_t mntflags,
		    int32_t direct,
		    fsid_t *fsidp,
		    boolean_t *top_level)
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
	 *
	 * XXX - will this still happen?  These mounts will only be
	 * done as a result of a planted trigger; will vfs_addtrigger(),
	 * which takes a relative path as an argument and won't cross
	 * mount points, handle that?  Should vfs_addtrigger() not follow
	 * symlinks?  Or is it sufficient that it won't leave the file
	 * system that was just mounted?
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
	mnt_args.direct = direct;
	mnt_args.mount_type = MOUNT_TYPE_SUBTRIGGER;		/* special trigger submount */
	/*
	 * XXX - subtriggers are always direct maps, right?
	 */
	if (direct)
		mnt_args.node_type = NT_TRIGGER;
	else
		mnt_args.node_type = 0;	/* not a trigger */

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
	*fsidp = buf.f_fsid;
	*top_level = (strcmp(submntpt, ".") == 0);
	return (0);
}

kern_return_t
autofs_mount_url(__unused mach_port_t server,
		 autofs_pathname url,				/* IN */
		 autofs_pathname mountpoint,			/* IN */
		 autofs_opts opts,				/* IN */
		 fsid_t mntpnt_fsid,				/* IN */
		 uint32_t sendereuid,				/* IN */
		 int32_t asid,					/* IN */
		 fsid_t *fsidp,					/* OUT */
		 uint32_t *retflags,				/* OUT */
		 int *err,					/* OUT */
		 audit_token_t atoken)
{
	int status;

	/* Sanitize our OUT parameters */
	*err = EPERM;
	memset(fsidp, 0, sizeof(*fsidp));
	*retflags = 0;	/* what we call sets retflags as needed */

	/*
	 * Reject this if the sender wasn't a kernel process
	 * (all messages from the kernel must have zero pid in audit_token_t).
	 */
	if (get_audit_token_pid(&atoken) != 0) {
		goto out;
	}

	if (trace > 0) {
		trace_prt(1, "MOUNT_URL REQUEST: url=%s mountpoint=%s\n", url, mountpoint);
	}

	status = mount_generic(url, "url", opts, 0, mountpoint, FALSE,
	    TRUE, FALSE, mntpnt_fsid, sendereuid, asid, fsidp, retflags);

	*err = status;

	if (trace > 0) {
		trace_prt(1, "MOUNT_URL REPLY    : status=%d\n", status);
	}

	if (status && verbose)
		syslog(LOG_ERR, "mount of %s on %s failed", url, mountpoint);

out:
	return KERN_SUCCESS;
}

#define SMBREMOUNTSERVER_PATH	"/System/Library/Extensions/autofs.kext/Contents/Resources/smbremountserver"

kern_return_t
autofs_smb_remount_server(__unused mach_port_t server,
			  byte_buffer blob,			/* IN */
			  mach_msg_type_number_t blobCnt,	/* IN */
			  au_asid_t asid,			/* IN */
			  audit_token_t atoken)
{
	int pipefds[2];
	int child_pid;
	int res;
	uint32_t byte_count;
	ssize_t bytes_written;
	int stat_loc;

	/*
	 * Reject this if the sender wasn't a kernel process
	 * (all messages from the kernel must have zero pid in audit_token_t).
	 */
	if (get_audit_token_pid(&atoken) != 0) {
		goto out;
	}

	if (trace > 0) {
		trace_prt(1, "SMB_REMOUNT_SERVER REQUEST:\n");
	}

	/*
	 * This is a bit ugly; we have to do the SMBRemountServer()
	 * call in a subprocess, for reasons listed in the comment
	 * in smbremountserver.c.
	 */

	/*
	 * Set up a pipe over which we send the blob to the subprocess.
	 */
	if (pipe(pipefds) == -1) {
		syslog(LOG_ERR, "Can't create pipe to smbremountserver: %m");
		goto done;
	}


	switch ((child_pid = fork())) {
	case -1:
		/*
		 * Fork failure.  Close the pipe,
		 * log an error, and quit.
		 */
		close(pipefds[0]);
		close(pipefds[1]);
		syslog(LOG_ERR, "Cannot fork: %m");
		break;

	case 0:
		/*
		 * Child.
		 *
		 * We make the read side of the pipe our standard
		 * input.
		 *
		 * We leave the rest of our environment as it is; we assume
		 * that launchd has made the right thing happen for us,
		 * and that this is also the right thing for the processes
		 * we run.
		 *
		 * Join the passed in audit session so we will have access to credentials
		 */
		if (join_session(asid)) {
			_exit(EPERM);
		}

		if (dup2(pipefds[0], 0) == -1) {
			res = errno;
			syslog(LOG_ERR, "Cannot dup2: %m");
			_exit(res);
		}
		close(pipefds[0]);
		close(pipefds[1]);
		(void) execl(SMBREMOUNTSERVER_PATH, SMBREMOUNTSERVER_PATH,
			     NULL);
		res = errno;
		syslog(LOG_ERR, "exec %s: %m", SMBREMOUNTSERVER_PATH);
		_exit(res);

	default:
		/*
		 * Parent.
		 *
		 * Close the read side of the pipe.
		 */
		close(pipefds[0]);

		/*
		 * Send the size of the blob down the pipe, in host
		 * byte order.
		 */
		byte_count = blobCnt;
		bytes_written = write(pipefds[1], &byte_count,
				      sizeof byte_count);
		if (bytes_written == -1) {
			syslog(LOG_ERR, "Write of byte count to pipe failed: %m");
			close(pipefds[1]);
			goto done;
		}
		if ((size_t)bytes_written != sizeof byte_count) {
			syslog(LOG_ERR, "Write of byte count to pipe wrote only %zd of %zu bytes",
			       bytes_written, sizeof byte_count);
			close(pipefds[1]);
			goto done;
		}

		/*
		 * Send the blob itself.
		 */
		bytes_written = write(pipefds[1], blob, byte_count);
		if (bytes_written == -1) {
			syslog(LOG_ERR, "Write of blob to pipe failed: %m");
			close(pipefds[1]);
			goto done;
		}
		if (bytes_written != (ssize_t)byte_count) {
			syslog(LOG_ERR, "Write of blob to pipe wrote only %zd of %u bytes",
			       bytes_written, byte_count);
			close(pipefds[1]);
			goto done;
		}

		/*
		 * Close the pipe, so the subprocess knows there's nothing
		 * more to read.
		 */
		close(pipefds[1]);

		/*
		 * Now wait for the child to finish.
		 */
		while (waitpid(child_pid, &stat_loc, WUNTRACED) < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "waitpid %d failed - error %d", child_pid, errno);
			goto done;
		}

		if (WIFSIGNALED(stat_loc)) {
			syslog(LOG_ERR, "SMBRemountServer subprocess terminated with %s",
			       strsignal(WTERMSIG(stat_loc)));
		} else if (WIFSTOPPED(stat_loc)) {
			syslog(LOG_ERR, "SMBRemountServer subprocess stopped with %s",
			       strsignal(WSTOPSIG(stat_loc)));
		} else if (!WIFEXITED(stat_loc)) {
			syslog(LOG_ERR, "SMBRemountServer subprocess got unknown status 0x%08x",
			       stat_loc);
		}
		break;
	}

done:

	if (trace > 0) {
		trace_prt(1, "SMB_REMOUNT_SERVER REPLY\n");
	}

out:
	return KERN_SUCCESS;
}

/*
 * Used for reporting messages from code
 * shared with automount command.
 * Calls vsyslog to log the message.
 *
 * Print an error.
 * Works like printf (fmt string and variable args)
 * except that it will subsititute an error message
 * for a "%m" string (like syslog).
 */
void
pr_msg(int priority, const char *fmt, ...)
{
	va_list ap;

#if 0
	fmt = gettext(fmt);
#endif

	va_start(ap, fmt);
	(void) vsyslog(priority, fmt, ap);
	va_end(ap);
}
