/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <notify.h>
#import <stdio.h>
#import <fcntl.h>
#import <unistd.h>
#import <stdlib.h>
#import <syslog.h>
#import <signal.h>
#import <string.h>
#import <errno.h>
#import <err.h>
#import <sys/types.h>
#import <sys/attr.h>
#import <sys/event.h>
#import <sys/stat.h>
#import <sys/wait.h>
#import <sys/time.h>
#import <sys/resource.h>
#import <sys/vnode.h>
#import "automount.h"
#import "AMVnode.h"
#import "Controller.h"
#import "AMString.h"
#import "log.h"
#import "AMVersion.h"
#import "systhread.h"
#import "NSLMap.h"
#import "NSLVnode.h"
#import "vfs_sysctl.h"
#ifndef __APPLE__
#import <libc.h>
extern int getppid(void);
#endif

#import <CoreServices/CoreServices.h>

#define forever for(;;)

#define SEPARATE_VFSEVENT_THREAD 1

int debug_select = 0;
int debug_mount = DEBUG_SYSLOG;
int debug_proc = 0;
int debug_options = 0;
int debug_nsl = 0;
int debug;
int doing_timeout = 0;

int protocol_1 = IPPROTO_UDP;
int protocol_2 = IPPROTO_TCP;

Controller *controller;
String *dot;
String *dotdot;

unsigned int GlobalMountTimeout = 20;
unsigned int GlobalTimeToLive = 10000;

int run_select_loop = 0;
int running_select_loop = 0;
#if SEPARATE_VFSEVENT_THREAD
int run_vfseventwait_loop = 0;
int running_vfseventwait_loop = 0;
#endif
CFRunLoopRef gMainRunLoop;
CFRunLoopTimerRef gRunLoopTimer;
int runloop_prepared = 0;

int gWakeupFDs[2] = { -1, -1 };

int gVFSEventKQueue = -1;

static BOOL gReinitializationSuspended = FALSE;
static BOOL gReinitDeferred = FALSE;
static BOOL gNetworkChangeDeferred = FALSE;
static BOOL gUserLogoutDeferred = FALSE;

static struct timeval last_timeout;

int osType;

BOOL doServerMounts = YES;

BOOL gUIAllowed = NO;

NSLMap *GlobalTargetNSLMap;

MountProgressRecord_List gMountsInProgress = LIST_HEAD_INITIALIZER(gMountsInProgress);

BOOL gForkedMountInProgress = NO;
pid_t gForkedMountPID; 
BOOL gForkedMount = NO;
BOOL gSubMounter = NO;
BOOL gBlockedMountDependency = NO;
unsigned long gBlockingMountTransactionID;
int gMountResult;
int gVolumeUnmountToken;
BOOL gTerminating = FALSE;

BOOL gUserLoggedIn = NO;

NSLMapList gNSLMapList = LIST_HEAD_INITIALIZER(gNSLMapList);

AMIMsgList gAMIMsgList = STAILQ_HEAD_INITIALIZER(gAMIMsgList);

#define AUTOMOUNTDIRECTORYPATH "/automount"

#define DefaultMountDir "/private/var/automount"

static char gForkedExecutionFlag[] = "-f";
static char gAutomounterPath[] = "/usr/sbin/automount";

struct debug_fdset
{
	unsigned int i[8];
};

static void child_exit(void);
static void shutdown_server(void);

struct AMIQueueEntry {
	struct AMIMsgListEntry AMIListEntry;
	char msgcopy[0];
};

struct directory_finder_info {
    struct FolderInfo di;
    struct ExtendedFolderInfo xdi;
};

struct catalog_finder_info {
	struct directory_finder_info d;
    unsigned long flags;
};

struct catalog_object_info {
    fsobj_type_t objtype;
};

struct cataloginfo_attribute_buffer {
    unsigned long length;
	union {
		struct catalog_finder_info cfi;
		struct catalog_object_info coi;
	} i;
};

#if SEPARATE_VFSEVENT_THREAD
void handle_vfsevent(int kq);
#endif

#ifndef __APPLE__ 
int
setsid(void)
{
	return 0;
}
#endif

#ifdef __APPLE__ 
#define PID_FILE "/var/run/automount.pid"
#else
#define PID_FILE "/etc/automount.pid"
#endif

char gNoLookupTarget[] = "[ other source ]";
char *gLookupTarget = gNoLookupTarget;

static void
writepid(void)
{
	FILE *fp;

	fp = fopen(PID_FILE, "w");
	if (fp != NULL)
	{
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
}

char *
fdtoc(fd_set *f)
{
	static char str[32];
	struct debug_fdset *df;

	df = (struct debug_fdset *)f;

	sprintf(str, "[%x %x %x %x %x %x %x %x]",
		df->i[0], df->i[1], df->i[2], df->i[3],
		df->i[4], df->i[5], df->i[6], df->i[7]);
	return str;
}

void
enqueue_AMInfoServiceRequest(void)
{
	char request_code[1];

	request_code[0] = REQ_AMINFOREQ;
	if (gWakeupFDs[1] != -1) {
		(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
	} else {
		sys_msg(debug, LOG_ERR, "EnqueueAMInfoServiceRequest: gWakeupFDs[1] uninitialized.");
	}
}

int
post_AMInfoServiceRequest(mach_port_t port, Map *map, mach_msg_header_t *msg, size_t size) {
	struct AMIQueueEntry *entry;
	size_t entrySize = sizeof(struct AMIQueueEntry) + size;
	
	entry = (struct AMIQueueEntry *)calloc(1, entrySize);
	if (entry == NULL) return ENOMEM;
	
	mach_msg_header_t *msgcopy = (mach_msg_header_t *)(&entry->msgcopy);
	bcopy(msg, msgcopy, size);
	
	entry->AMIListEntry.iml_port = port;
	entry->AMIListEntry.iml_map = [map retain];
	entry->AMIListEntry.iml_size = size;
	entry->AMIListEntry.iml_msg = msgcopy;
	
#if 0
	sys_msg(debug, LOG_DEBUG, "post_AMInfoServiceRequest: entry->port = 0x%08lx", (unsigned long)entry->AMIListEntry.iml_port);
	sys_msg(debug, LOG_DEBUG, "post_AMInfoServiceRequest: entry->msg->remote_port = 0x%08lx",
								(unsigned long)entry->AMIListEntry.iml_msg->msgh_remote_port);
	sys_msg(debug, LOG_DEBUG, "post_AMInfoServiceRequest: entry->msg->local_port = 0x%08lx",
								(unsigned long)entry->AMIListEntry.iml_msg->msgh_local_port);
	sys_msg(debug, LOG_DEBUG, "post_AMInfoServiceRequest: entry->msg->bits = 0x%08lx",
								(unsigned long)entry->AMIListEntry.iml_msg->msgh_bits);
#endif
	STAILQ_INSERT_TAIL(&gAMIMsgList, &entry->AMIListEntry, iml_link);
	
	enqueue_AMInfoServiceRequest();
	
	return 0;
}

void
process_AMInfoServiceRequests(void) {
	struct AMIMsgListEntry *entry;

	while (entry = STAILQ_FIRST(&gAMIMsgList)) {
#if 0
		sys_msg(debug, LOG_DEBUG, "\tDispatching %ld-byte message for %s...", entry->iml_size, [[entry->iml_map name] value]);
#endif
		[entry->iml_map handleAMInfoRequest:entry->iml_msg ofSize:entry->iml_size onPort:entry->iml_port];
		[entry->iml_map release];
		STAILQ_REMOVE_HEAD(&gAMIMsgList, iml_link);
		free(entry);
	};
};

#if SEPARATE_VFSEVENT_THREAD
void
enqueue_vfsevent_notification(void) {
	char request_code[1] ={  REQ_VFSEVENT };

	if (gWakeupFDs[1] != -1) {
		(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
	} else {
		sys_msg(debug, LOG_ERR, "enqueue_vfsevent_notification: gWakeupFDs[1] uninitialized.");
	};

}
#endif

void
enqueue_reinit_request(void) {
	char request_code[1] ={  REQ_REINIT };

	if (gReinitializationSuspended) {
		sys_msg(debug, LOG_NOTICE, "deferring re-initialization while initialization is in progress...");
		gReinitDeferred = TRUE;
	} else {
		if (gWakeupFDs[1] != -1) {
			(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
		} else {
			sys_msg(debug, LOG_ERR, "enqueue_reinit_request: gWakeupFDs[1] uninitialized.");
		};
	};
}

void
enqueue_networkchange_notification(void) {
	char request_code[1] ={  REQ_NETWORKCHANGE };

	if (gReinitializationSuspended) {
		sys_msg(debug, LOG_NOTICE, "deferring network change notification while initialization is in progress...");
		gNetworkChangeDeferred = TRUE;
	} else {
		if (gWakeupFDs[1] != -1) {
			(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
		} else {
			sys_msg(debug, LOG_ERR, "enqueue_networkchange_notification: gWakeupFDs[1] uninitialized.");
		};
	};
}

void
enqueue_userlogout_notification(void) {
	char request_code[1] ={  REQ_USERLOGOUT };

	sys_msg(debug, LOG_INFO, "logout notification received.");
	if (gReinitializationSuspended) {
		sys_msg(debug, LOG_NOTICE, "deferring user logout notification while init is in progress...");
		gUserLogoutDeferred = TRUE;
	} else {
		if (gWakeupFDs[1] != -1) {
			sys_msg(debug, LOG_INFO, "requesting logout processing.");
			(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
		} else {
			sys_msg(debug, LOG_ERR, "enqueue_userlogout_notification: gWakeupFDs[1] uninitialized.");
		};
	};
}

void
suspend_reinitialization(void) {
	/* Stop the actual enqueueing of requests that can interfere with an initialization in progress: */
	gReinitializationSuspended = TRUE;
}

void
reenable_reinitialization(void) {
	/* Open the floodgates for incoming re-initialization requests: */
	gReinitializationSuspended = FALSE;
	
	/* Deferred SIGHUP or other reinitialization request? */
	if (gReinitDeferred) {
		enqueue_reinit_request();
		gReinitDeferred = FALSE;
	};
	
	/* Deferred network change notification? */
	if (gNetworkChangeDeferred) {
		enqueue_networkchange_notification();
		gNetworkChangeDeferred = FALSE;
	};
	
	/* Deferred user logout? */
	if (gUserLogoutDeferred) {
		sys_msg(debug, LOG_NOTICE, "reposting deferred logout notification.");
		enqueue_userlogout_notification();
		gUserLogoutDeferred = FALSE;
	};
}

void
handle_enqueued_requests(char request_code) {
	struct NSLMapListEntry *mapListEntry;

	switch (request_code) {
	  case REQ_MOUNTCOMPLETE:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: completing forked mount.");
        child_exit();
		break;
		
	  case REQ_REINIT:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: re-initializing automounter.");
		[controller reInit];
		break;
		
	  case REQ_SHUTDOWN:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: shutting down automounter service.");
        shutdown_server();
        exit(0);
		
		/* NOT REACHED */
		break;

	  case REQ_NETWORKCHANGE:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: network changed.");
		// validate maps
		if (gTerminating) {
			sys_msg(debug_nsl, LOG_DEBUG, "handle_deferred_requests: ignoring network change at shutdown.");
		} else {
			[controller validate];
		};
		break;
	  
	  case REQ_USERLOGOUT:
		sys_msg(debug, LOG_INFO, "handle_deferred_requests: user logged out.");
	    // unmount (no force)
	    [controller unmountAutomounts:0];
		break;
	  
	  case REQ_PROCESS_RESULTS:
		sys_msg(debug_nsl, LOG_DEBUG, "handle_deferred_requests: new search results to be processed.");
		if (gTerminating) {
			sys_msg(debug_nsl, LOG_DEBUG, "handle_deferred_requests: ignoring new search at shutdown.");
		} else {
			LIST_FOREACH(mapListEntry, &gNSLMapList, mle_link) {
				[mapListEntry->mle_map processNewSearchResults];
			};
		};
		break;
	  
	  case REQ_ALARM:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: triggering deferred notifications...");
		LIST_FOREACH(mapListEntry, &gNSLMapList, mle_link) {
			[mapListEntry->mle_map triggerDeferredNotifications];
		};
		break;

	  case REQ_UNMOUNT:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: volume unmounted.");
		[controller checkForUnmounts];
		break;

      case REQ_AMINFOREQ:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: processing pending AM info requests...");
		process_AMInfoServiceRequests();
		break;
      
#if SEPARATE_VFSEVENT_THREAD
	  case REQ_VFSEVENT:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: processing pending VFS Event...");
		handle_vfsevent(gVFSEventKQueue);
		break;
#endif

	  case REQ_USR2:
#if 0
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: triggering timeout on SIGUSR2.");
		doing_timeout = 1;
		[controller timeout];
		doing_timeout = 0;
#else
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: printing mount tree on SIGUSR2.");
		[controller printTree];
#endif
		break;
		
	  default:
	    sys_msg(debug, LOG_DEBUG, "handle_enqueued_requests: Unknown request code '%c'?!", request_code);
		break;
	};
}

void handle_vfsevent(int kq) {
	int result;
	struct timespec to;
	struct kevent vfsevent;
	int vers = AUTOFS_PROTOVERS;
	fsid_t *fsid_list;
	int fs_count, fs, req;
	size_t reqlen;
	int reqcnt;
	struct vfsquery vq;
	struct autofs_userreq *reqs;

	to.tv_sec = 0;
	to.tv_nsec = 0;
	do {
		result = kevent(kq, NULL, 0, &vfsevent, 1, &to);
	} while ((result == -1) && (result == EINTR));
	
#if 0
	if (result) {
		sys_msg(debug_select, LOG_DEBUG, "handle_vfsevent: kevent returned %d (errno = %d, %s)?", result, errno, strerror(errno));
	} else {
		if (vfsevent.fflags & VQ_ASSIST) {
#endif
			result = create_vfslist(&fsid_list, &fs_count);
			if (fs_count > 0) {
				for (fs = 0; fs < fs_count; ++fs) {
					bzero(&vq, sizeof(vq));
					result = sysctl_queryfs(&fsid_list[fs], &vq);
					if (result == 0) {
						if ((vq.vq_flags & VQ_ASSIST) == 0) continue;
						
						sys_msg(debug_select, LOG_DEBUG, "checking for autofs reqs for fs #%d...", fs);
						reqlen = 0;
						result = sysctl_fsid(AUTOFS_CTL_GETREQS, &fsid_list[fs], NULL, &reqlen, &vers, sizeof(vers));
						if (result == -1) continue;
						sys_msg(debug_select, LOG_DEBUG, "request size = %ld.", reqlen);

						reqs = malloc(reqlen);
						if (reqs == NULL) continue;
						result = sysctl_fsid(AUTOFS_CTL_GETREQS, &fsid_list[fs], reqs, &reqlen, &vers, sizeof(vers));
						if (result == -1) {
							free(reqs);
							continue;
						}
						reqcnt = reqlen / sizeof(*reqs);
						sys_msg(debug_select, LOG_DEBUG, "processing %d requests...", reqcnt);
						for (req = 0; req < reqcnt; req++) {
							sys_msg(debug_select, LOG_DEBUG, "Request #%d: mount node %ld (%s) for process %d (uid=%d)...",
										req + 1, reqs[req].au_ino, reqs[req].au_name, reqs[req].au_pid, reqs[req].au_uid);
							result = [controller dispatch_autofsreq:&reqs[req] forFSID:&fsid_list[fs]];
							if (result == -1)
								continue;		/* Request was not for one of our maps; ignore it */
							reqs[req].au_errno = result;
							reqs[req].au_flags = 1;
							if (gForkedMountInProgress || gBlockedMountDependency) {
								/* Leave it to the child process to coordinate with autofs (AUTOFS_CTL_SERVREQ) */
								gForkedMountInProgress = NO;
								gBlockedMountDependency = NO;
								continue;
							} else {
								/* This process has been running as the child of a forked mount (gForkedMount or gSubMounter must be set) */
								sys_msg(debug_select, LOG_DEBUG, "request #%d: errno = %d ('%s')...", req + 1, reqs[req].au_errno, strerror(reqs[req].au_errno));
								result = sysctl_fsid(AUTOFS_CTL_SERVREQ, &fsid_list[fs], NULL, NULL, &reqs[req], sizeof(reqs[req]));
								if (result) {
									sys_msg(debug_select, LOG_DEBUG, "request #%d: AUTOFS_CTL_SERVREQ sysctl returned error %d ('%s')...", req + 1, errno, strerror(errno));
								};
								if (gForkedMount || gSubMounter) {
									/* Only the parent process should continue in the loop: */
									exit((gMountResult < 128) ? gMountResult : ECANCELED);
								};
							};
						}
						free(reqs);
					}
				}
			}
			free_vfslist(fsid_list);
#if 0
		}
	}
#endif
}

/*
 * Check that a proposed value to load into the .tv_sec or
 * .tv_usec part of an interval timer is acceptable, and fix
 * it to have at least minimal value (i.e. absent direct access
 * to 'tick', if it is less than 0.1 Sec, round it up to 0.1 Sec.)
 */
#define TIMEQUANTUM (100*1000) /* 100mSec. (10Hz) - reasonable default */
#define ONEMILLION (1000*1000)
#define TIMEOUTLIMIT (604800) /* A week, in seconds */
void
cleanuptimeout(tv)
	struct timeval *tv;
{
	if (tv == NULL) return;
	
	if (tv->tv_sec < 0) tv->tv_sec = 0;
	if (tv->tv_usec != 0) {
		if (tv->tv_usec < TIMEQUANTUM) tv->tv_usec = TIMEQUANTUM;
		if (tv->tv_usec >= ONEMILLION) {
			tv->tv_sec += tv->tv_usec / ONEMILLION;
			tv->tv_usec = tv->tv_usec % ONEMILLION;
		};
	};
	if (tv->tv_sec > TIMEOUTLIMIT) tv->tv_sec = TIMEOUTLIMIT;
}

int
do_select(struct timeval *tv)
{
	int n;
	fd_set x;

	x = svc_fdset;
	if (gWakeupFDs[0] != -1) {
		FD_SET(gWakeupFDs[0], &x);	/* This allows writing to gWakeupFDs[1] to wake up the select() */
	};
#if ! SEPARATE_VFSEVENT_THREAD
	if (gVFSEventKQueue != -1) {
		FD_SET(gVFSEventKQueue, &x);	/* This allows incoming VFS events to wake up the select() */
	};
#endif

	if (tv) {
		sys_msg(debug_select, LOG_DEBUG, "select timeout %d %d", tv->tv_sec, tv->tv_usec);
	};

	cleanuptimeout(tv);
	n = select(FD_SETSIZE, &x, NULL, NULL, tv);

	if (n != 0)
		sys_msg(debug_select, LOG_DEBUG, "select(%s, %d) --> %d",
			fdtoc(&x), tv ? tv->tv_sec : 0, n);
	if (n < 0)
		sys_msg(debug_select, LOG_DEBUG, "select: %s", strerror(errno));

	if (n > 0) {
		if ((gWakeupFDs[0] != -1) && FD_ISSET(gWakeupFDs[0], &x)) {
			char request_code[1];
			
			(void)read(gWakeupFDs[0], request_code, sizeof(request_code));
			handle_enqueued_requests(request_code[0]);
			return 0;
		};
		
#if ! SEPARATE_VFSEVENT_THREAD
		if ((gVFSEventKQueue != -1) && FD_ISSET(gVFSEventKQueue, &x)) {
			handle_vfsevent(gVFSEventKQueue);
			return 0;
		};
#endif

		svc_getreqset(&x);
	};

	if (gForkedMount || gSubMounter) exit((gMountResult < 128) ? gMountResult : ECANCELED);
	
	return n;
}

void
select_loop(void *x)
{
	struct timeval tv;
	running_select_loop = 1;

	sys_msg(debug_select, LOG_DEBUG, "--> select loop");

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (run_select_loop != 0)
	{
		do_select(&tv);
		systhread_yield();
	}

	sys_msg(debug_select, LOG_DEBUG, "<-- select loop");

	running_select_loop = 0;
	systhread_exit();
}

void
auto_run_no_timeout(void *x)
{
	forever do_select(NULL);
}

void
auto_run(struct timeval *t)
{
	int n;
	struct timeval tv, now, delta;

	gettimeofday(&last_timeout, (struct timezone *)0);

	tv.tv_usec = 0;
	tv.tv_sec = t->tv_sec;
	cleanuptimeout(&tv);

	delta.tv_sec = tv.tv_sec;
	delta.tv_usec = 0;

	forever
	{
		n = do_select(&delta);

		gettimeofday(&now, (struct timezone *)0);
		if (now.tv_sec >= (last_timeout.tv_sec + tv.tv_sec))
		{
			if (t->tv_sec > 0)
			{
				doing_timeout = 1;
				[controller timeout];
				doing_timeout = 0;
			}

			last_timeout = now;
            delta.tv_sec = tv.tv_sec;
		}
		else
		{
			delta.tv_sec = tv.tv_sec - (now.tv_sec - last_timeout.tv_sec);
			if (delta.tv_sec <= 0) delta.tv_sec = 1;
		}
		cleanuptimeout(&delta);
	}
}


#if SEPARATE_VFSEVENT_THREAD
void
vfseventwait_loop(void *x)
{
	u_int waitret;

	running_vfseventwait_loop = 1;
	sys_msg(debug_select, LOG_DEBUG, "--> vfseventwait loop");

	while (run_vfseventwait_loop != 0)
	{
		waitret = vfsevent_wait(gVFSEventKQueue, 0);
		if (waitret) enqueue_vfsevent_notification();
		systhread_yield();
	}

	sys_msg(debug_select, LOG_DEBUG, "<-- vfseventwait loop");
	running_vfseventwait_loop = 0;
	systhread_exit();
}
#endif

void
usage(void)
{
	fprintf(stderr, "usage: automount [options]...\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -help         ");
	fprintf(stderr, "Print this message.\n");
	fprintf(stderr, "\n");
	
	fprintf(stderr, "  -V            ");
	fprintf(stderr, "Print version and host information, then quit.\n");
	fprintf(stderr, "\n");

#if 0
	fprintf(stderr, "  -a dir        ");
	fprintf(stderr, "Set mount directory to dir.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "Default value is \"%s\".\n", DefaultMountDir);
	fprintf(stderr, "\n");
#endif

	fprintf(stderr, "  -tm n         ");
	fprintf(stderr, "Set default mount timeout to n seconds.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "mnttimeo=n mount option overrides this default.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "Default value is %d seconds.\n", GlobalMountTimeout);
	fprintf(stderr, "\n");

	fprintf(stderr, "  -tl n         ");
	fprintf(stderr, "Set default mount time-to-live to n seconds.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "ttl=n mount option overrides this default.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "Zero value sets infinite time-to-live.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "Default value is %d seconds.\n", GlobalTimeToLive);
	fprintf(stderr, "\n");

	fprintf(stderr, "  -1            ");
	fprintf(stderr, "Modifies the \"-fstab\" map.  Mounts are done \"one at a time\",\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "when an actual mount point is traversed, rather than forcing\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "all mounts from a server at its top-level directory.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -s            ");
	fprintf(stderr, "All mounts are forced at startup, and never expire.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -d            ");
	fprintf(stderr, "Run in debug mode.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -D type       ");
	fprintf(stderr, "Log debug messages for type.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "type may be \"mount\", \"proc\", \"select\"\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "\"options\", \"nsl\", or \"all\".\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "Multiple -D options may be specified.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -tcp          ");
	fprintf(stderr, "Mount NFS servers using TCP if possible, otherwise UDP.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "(the default is to try UDP first, then TCP).\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "'-T', 'TCP', or 'tcp' mount option has the same effect\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "as specifying -tcp; '-U', 'UDP', or 'udp' mount option\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "forces the default behavior despite -tcp.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -m dir <map> -mnt dir\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "Mount <map> on directory dir.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "Followed by specification of private mount dir.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "<map> may be a file (must be an absolute path),\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "\"-fstab\", \"-static\", \"-nsl\", or \"-null\".\n");
	fprintf(stderr, "\n");
}

static void
alarm_sighandler(int x)
{
	if (gWakeupFDs[1] != -1) {
		char request_code[1] = { REQ_ALARM };
		
		(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
	};
}

void
parentexit(int x)
{
	_exit(0);
}

static void
shutdown_server(void)
{
	sys_msg(debug, LOG_NOTICE, "Shutting down.");

	gTerminating = TRUE;

	notify_cancel(gVolumeUnmountToken);
	
	[controller release];
	[dot release];
	[dotdot release];
}

static void
shutdown_server_sighandler(int x)
{
	if (gWakeupFDs[1] != -1) {
		char request_code[1] = { REQ_SHUTDOWN };
		
		(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
	} else {
		sys_msg(debug, LOG_ERR, "shutdown_server_sighandler: gWakeupFDs[1] uninitialized.");
	};
}

static void
child_exit(void)
{
	int result;
	pid_t pid;

	while ((((pid = wait4((pid_t)-1, &result, WNOHANG, NULL)) != 0) && (pid != -1)) ||
           ((pid == -1) && (errno == EINTR))) {
		sys_msg(debug_mount, LOG_DEBUG, "child_exit: wait4() returned pid = %d, status = 0x%08x, errno = %d...", pid, result, errno);
		if ((pid != 0) && (pid != -1)) {
			[controller completeMountInProgressBy:pid exitStatus:result];
		};
	};
}

static void
child_exit_sighandler(int x)
{
	if (!gSubMounter) {
		if (gWakeupFDs[1] != -1) {
			char request_code[1] = { REQ_MOUNTCOMPLETE };
			
			(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
		} else {
			sys_msg(debug, LOG_ERR, "child_exit_sighandler: gWakeupFDs[1] uninitialized.");
		};
	};
}

static void
reinit_sighandler(int x)
{
	enqueue_reinit_request();
}

void
usr1_signhandler(int x)
{
    enqueue_networkchange_notification();
}

static void
usr2_sighandler(int x)
{
	if (gWakeupFDs[1] != -1) {
		char request_code[1] ={  REQ_USR2 };
		
		(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
	} else {
		sys_msg(debug, LOG_ERR, "usr2_sighandler: gWakeupFDs[1] uninitialized.");
	};
}

static void
print_host_info(void)
{
	char banner[1024];

	sprintf(banner, "Host Info: %s", [[controller hostName] value]);
	if ([controller hostDNSDomain] != nil)
	{
		strcat(banner, ".");
		strcat(banner, [[controller hostDNSDomain] value]);
	}

	strcat(banner, " ");
	strcat(banner, [[controller hostOS] value]);
	strcat(banner, " ");
	strcat(banner, [[controller hostOSVersion] value]);
	strcat(banner, " ");
	strcat(banner, [[controller hostArchitecture] value]);
	strcat(banner, " (");
	strcat(banner, [[controller hostByteOrder] value]);
	strcat(banner, " endian)");
	sys_msg(debug, LOG_DEBUG, banner);
}

// *********************************************************************
// 			look for network and user changes
// *********************************************************************

#include <SystemConfiguration/SystemConfiguration.h>

static SCDynamicStoreRef	store = NULL;
static CFRunLoopSourceRef	rls;
static CFStringRef		userChangedKey;
static CFStringRef		netinfoChangedKey;
static CFStringRef		searchPolicyChangedKey;

/* The following constant was stolen from <DirectoryService/DirServicesPriv.h> */
#ifndef		kDSStdNotifySearchPolicyChanged
#define		kDSStdNotifySearchPolicyChanged		"com.apple.DirectoryService.NotifyTypeStandard:SearchPolicyChanged"
#endif

void
systemConfigHasChanged(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
	int i, count;
	CFStringRef key, user;
	BOOL networkChanged = NO;
    
	count = CFArrayGetCount(changedKeys);
	for (i=0; i < count; i++) {
	    key = CFArrayGetValueAtIndex(changedKeys, i);
	    
	    if (CFStringCompare(key, userChangedKey, 0) == kCFCompareEqualTo) {
			user = SCDynamicStoreCopyConsoleUser(store, NULL, NULL);
			if (user) {
				sys_msg(debug, LOG_DEBUG, "the console user is logged in");
				gUserLoggedIn = YES;		/* One-shot, never cleared */
				if (gUserLogoutDeferred) {
					sys_msg(debug, LOG_NOTICE, "canceling deferred logout notification...");
					gUserLogoutDeferred = NO;
				};
				CFRelease(user);
			} else {
				sys_msg(debug, LOG_DEBUG, "the console user has logged out");
				enqueue_userlogout_notification();
			}
			continue;
		}

	    if (CFStringCompare(key, netinfoChangedKey, 0) == kCFCompareEqualTo) {
		sys_msg(debug, LOG_DEBUG, "the netinfo configuration has changed");
		networkChanged = YES;
		continue;
	    }
	    
	    if (CFStringCompare(key, searchPolicyChangedKey, 0) == kCFCompareEqualTo) {
		sys_msg(debug, LOG_DEBUG, "directory services search policy changed");
		networkChanged = YES;
		continue;
	    }
	}
	
	if (networkChanged) {
		/* Reschedule the next validation for 1 second from now. */
		CFRunLoopTimerSetNextFireDate(gRunLoopTimer, CFAbsoluteTimeGetCurrent()+1.0);
	}
}

void
watchForSystemConfigChanges()
{
	CFMutableArrayRef	keys;

	store = SCDynamicStoreCreate(NULL, CFSTR("automount"), systemConfigHasChanged, NULL);
	if (!store) {
		sys_msg(debug, LOG_ERR, "could not open session with configd");
		sys_msg(debug, LOG_ERR, "error = %s", SCErrorString(SCError()));
		return;
	}

	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/*
	 * establish and register dynamic store keys to watch
	 *   - netinfo configuration changes
	 *	 - directory services search policy changes
	 */
	netinfoChangedKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							   kSCDynamicStoreDomainState,
							   kSCEntNetNetInfo);
	CFArrayAppendValue(keys, netinfoChangedKey);
	
	searchPolicyChangedKey = SCDynamicStoreKeyCreate(NULL,
								CFSTR(kDSStdNotifySearchPolicyChanged));
	CFArrayAppendValue(keys, searchPolicyChangedKey);

	/*
	 * establish and register dynamic store keys to watch console user login/logouts
	 */
	userChangedKey = SCDynamicStoreKeyCreateConsoleUser(NULL);
	CFArrayAppendValue(keys, userChangedKey);

	if (!SCDynamicStoreSetNotificationKeys(store, keys, NULL)) {
		sys_msg(debug, LOG_ERR, "could not register notification keys");
		sys_msg(debug, LOG_ERR, "error = %s", SCErrorString(SCError()));
		CFRelease(store);
		CFRelease(keys);
		return;
	}
	CFRelease(keys);

	/* add a callback */
	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!rls) {
		sys_msg(debug, LOG_ERR, "could not create runloop source");
		sys_msg(debug, LOG_ERR, "error = %s", SCErrorString(SCError()));
		CFRelease(store);
		return;
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
}

void VolumeUnmounted(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	char request_code[1];
	sys_msg(debug, LOG_DEBUG, "Volume unmounted notification");

	invalidate_fsstat_array();
	
	request_code[0] = REQ_UNMOUNT;
	if (gWakeupFDs[1] != -1) {
		(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
	} else {
		sys_msg(debug, LOG_ERR, "VolumeUnmounted: gWakeupFDs[1] uninitialized.");
	}
}

void WatchForVolumeUnmounts()
{
	mach_port_t port = MACH_PORT_NULL;
	CFMachPortRef machPortRef = NULL;
	CFRunLoopSourceRef notifySource = NULL;
	CFMachPortContext context = { 0, NULL, NULL, NULL, NULL };
	
	if (notify_register_mach_port(
		"com.apple.system.kernel.unmount",
		&port,
		0,
		&gVolumeUnmountToken) != NOTIFY_STATUS_OK)
	{
		sys_msg(debug, LOG_ERR, "WatchForVolumeUnmounts: could not register");
		return;
	}
	
	machPortRef = CFMachPortCreateWithPort(NULL, port, VolumeUnmounted, &context, NULL);
	if (machPortRef == NULL)
	{
		sys_msg(debug, LOG_ERR, "WatchForVolumeUnmounts: could not create CFMachPort");
		goto cleanup;
	}

	notifySource = CFMachPortCreateRunLoopSource(NULL, machPortRef, 0);
	if (notifySource == NULL)
	{
		sys_msg(debug, LOG_ERR, "WatchForVolumeUnmounts: could not create CFRunLoopSource");
		goto cleanup;
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), notifySource, kCFRunLoopDefaultMode);
	port = MACH_PORT_NULL;		/* The port will be reclaimed on process exit */

cleanup:
	if (notifySource != NULL)
		CFRelease(notifySource);
	if (machPortRef != NULL)
		CFRelease(machPortRef);
}

static void
TimerExpired(CFRunLoopTimerRef timer, void *info)
{
	sys_msg(debug, LOG_DEBUG, "TimerExpired: requesting network change");
	enqueue_networkchange_notification();
}

static int
MarkDirectoryInvisible(const char *target)
{
    int result;
    struct attrlist alist;
    struct cataloginfo_attribute_buffer abuf;

	/* Prepare a minimal attribute list: */
    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_OBJTYPE;
    alist.volattr = 0;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;
    
	/* Verify that the target is a directory: */
    result = getattrlist(target, &alist, &abuf, sizeof(abuf), 0);
    if (result != 0) return ((result == -1) ? (errno ? errno : 1) : result);
	if (abuf.i.coi.objtype != VDIR) {
		sys_msg(debug, LOG_ERR, "MarkDirectoryInvisible: '%s' is not a directory.", target);
		return ENOTDIR;
	}
	
	/* Get the basic catalog information for the specified directory: */
    alist.commonattr = ATTR_CMN_FNDRINFO | ATTR_CMN_FLAGS;
    result = getattrlist(target, &alist, &abuf, sizeof(abuf), 0);
    if (result != 0) return ((result == -1) ? (errno ? errno : 1) : result);
    
	/* Initialize the Finder info if it hasn't been already: */
	if ((abuf.i.cfi.d.di.finderFlags & kHasBeenInited) == 0) {
		sys_msg(debug, LOG_DEBUG, "MarkDirectoryInvisible: initializing Finder Info for '%s'...", target);
		bzero(&abuf.i.cfi.d.di, sizeof(abuf.i.cfi.d.di));
		bzero(&abuf.i.cfi.d.xdi, sizeof(abuf.i.cfi.d.xdi));
		abuf.i.cfi.d.di.finderFlags |= kHasBeenInited;
	};
	
	/* Set the 'invisible' bit if necessary: */
	if (abuf.i.cfi.d.di.finderFlags & kIsInvisible) {
		sys_msg(debug, LOG_DEBUG, "MarkDirectoryInvisible: '%s' is already marked invisible.", target);
		return 0;
	}
	sys_msg(debug, LOG_DEBUG, "MarkDirectoryInvisible: marking '%s' as invisible...", target);
	abuf.i.cfi.d.di.finderFlags |= kIsInvisible;
	
	/* Update the Finder info: */
	result = setattrlist(target, &alist, &abuf.i.cfi, sizeof(abuf.i.cfi), 0);
	if (result) {
		sys_msg(debug, LOG_ERR, "MarkDirectoryInvisible: setattrlist('%s', ... ) failed: %s", target, strerror(errno));
	}
    return ((result == -1) ? (errno ? errno : 1) : result);
}

static void
mainRunLoop() {
	CFTimeInterval longTime = 10000000000.0;	// About 300 years, in seconds
	CFRunLoopTimerContext context = { 0, NULL, NULL, NULL, NULL };
	
	watchForSystemConfigChanges();
	WatchForVolumeUnmounts();
	gMainRunLoop = CFRunLoopGetCurrent();
	gRunLoopTimer = CFRunLoopTimerCreate(
		kCFAllocatorDefault,
		longTime,
		longTime,
		0,
		0,
		TimerExpired,
		&context);
	CFRunLoopAddTimer(gMainRunLoop, gRunLoopTimer, kCFRunLoopCommonModes);
	/* Request Carbon do its DiskArb notification processing on this thread's CFRunLoop: */
	_FSScheduleFileVolumeOperations(gMainRunLoop, kCFRunLoopDefaultMode);
	runloop_prepared = 1;
	CFRunLoopRun();
}

int
main(int argc, char *argv[])
{
	int daemon_argc;
	char **daemon_argv = NULL;
	String *mapName;
	String *mapDir;
	char *mntdir;
	char mountPathPrefix[PATH_MAX];
	size_t mountPathPrefixLength;
	String *mountDir;
	int pid, i, nmaps, result;
	struct timeval timeout;
	BOOL becomeDaemon, forkedExecution, printVers, staticMode;
	struct rlimit rlim;
	systhread *rpcLoop, *runLoop;
#if SEPARATE_VFSEVENT_THREAD
	systhread *vfseventLoop;
#endif
	struct stat sb;
	
	if ((argc <= 1) || (strcmp(argv[1], gForkedExecutionFlag) != 0)) {
		daemon_argc = argc + 1;
		daemon_argv = malloc((daemon_argc + 1) * sizeof(char *));
		if (daemon_argv) {
			daemon_argv[0] = argv[0];
			daemon_argv[1] = gForkedExecutionFlag;
			for (i = 2; i < daemon_argc; ++i) {
				daemon_argv[i] = argv[i - 1];
			};
			daemon_argv[daemon_argc] = NULL;
		}
	};
	
	mntdir = DefaultMountDir;

	nmaps = 0;
	becomeDaemon = YES;
	forkedExecution = NO;
	printVers = NO;
	staticMode = NO;
	debug = DEBUG_SYSLOG;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-help"))
		{
			usage();
			exit(0);
		}
		else if (!strcmp(argv[i], gForkedExecutionFlag))
		{
			forkedExecution = YES;
			becomeDaemon = NO;
		}
		else if (!strcmp(argv[i], "-d"))
		{
			becomeDaemon = NO;
			debug = DEBUG_STDERR;
		}
		else if (!strcmp(argv[i], "-V"))
		{
			printVers = YES;
		}
		else if (!strcmp(argv[i], "-1"))
		{
			doServerMounts = NO;
		}
		else if (!strcmp(argv[i], "-s"))
		{
			staticMode = YES;
		}
		else if (!strcmp(argv[i], "-tcp"))
		{
			protocol_1 = IPPROTO_TCP;
			protocol_2 = IPPROTO_UDP;
		}
		else if (!strcmp(argv[i], "-u"))
		{
			gUIAllowed = YES;
		}
		else if (!strcmp(argv[i], "-D"))
		{
			if ((argc - (i + 1)) < 1)
			{
				usage();
				exit(1);
			}
			i++;

			if ((!strcmp(argv[i], "proc")) || (!strcmp(argv[i], "all")))
				debug_proc = DEBUG_SYSLOG;
			if ((!strcmp(argv[i], "mount")) || (!strcmp(argv[i], "all")))
				debug_mount = DEBUG_SYSLOG;
			if ((!strcmp(argv[i], "select")) || (!strcmp(argv[i], "all")))
				debug_select = DEBUG_SYSLOG;
			if ((!strcmp(argv[i], "options")) || (!strcmp(argv[i], "all")))
				debug_options = DEBUG_SYSLOG;
			if ((!strcmp(argv[i], "nsl")) || (!strcmp(argv[i], "all")))
				debug_nsl = DEBUG_SYSLOG;
		}
		else if (!strcmp(argv[i], "-m"))
		{
			if ((argc - (i + 1)) < 2)
			{
				usage();
				exit(1);
			}
			i += 2;
			if (!strcmp(argv[i], "-mnt"))
			{
				if ((argc - (i + 1)) < 2)
				{
					usage();
					exit(1);
				}
				i += 2;
			};
			nmaps++;
		}			
		else if (!strcmp(argv[i], "-a"))
		{
			if ((argc - (i + 1)) < 1)
			{
				usage();
				exit(1);
			}
			mntdir = argv[++i];
		}
		else if (!strcmp(argv[i], "-tl"))
		{
			if ((argc - (i + 1)) < 1)
			{
				usage();
				exit(1);
			}
			GlobalTimeToLive = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-tm"))
		{
			if ((argc - (i + 1)) < 1)
			{
				usage();
				exit(1);
			}
			GlobalMountTimeout = atoi(argv[++i]);
		}
	}

	if (printVers)
	{
		debug = DEBUG_STDERR;
		controller = [[Controller alloc] init:mntdir];
		sys_msg(debug, LOG_DEBUG, "automount version %s", version);
		print_host_info();
		[controller release];
		exit(0);
	}

	if (geteuid() != 0)
	{
		fprintf(stderr, "Must be root to run automount\n");
		exit(1);
	}

	if (debug == DEBUG_STDERR)
	{
		if (debug_proc != 0) debug_proc = DEBUG_STDERR;
		if (debug_mount != 0) debug_mount = DEBUG_STDERR;
		if (debug_select != 0) debug_select = DEBUG_STDERR;
		if (debug_options != 0) debug_options = DEBUG_STDERR;
		if (debug_nsl != 0) debug_nsl = DEBUG_STDERR;
	}

	timeout.tv_sec = GlobalTimeToLive;
	timeout.tv_usec = 0;

	dot = [String uniqueString:"."];
	dotdot = [String uniqueString:".."];

	if (becomeDaemon)
	{
		signal(SIGTERM, parentexit);
		signal(SIGCHLD, parentexit);

		pid = fork();
		if (pid < 0)
		{
			sys_msg(debug, LOG_ERR, "fork() failed: %s", strerror(errno));
			exit(1);
		}
		else if (pid > 0)
		{
			/* Parent waits for child's signal */
			forever pause();
		}

		/* detach from controlling tty and start a new process group */
		if (setsid() < 0)
		{
			sys_msg(debug, LOG_ERR, "setsid() failed: %s", strerror(errno));
		}

		writepid();
		
		if ((stat(daemon_argv[0], &sb) != 0) || ((sb.st_mode & S_IFMT) != S_IFREG)) {
			daemon_argv[0] = gAutomounterPath;
		};
		result = execv(daemon_argv[0], daemon_argv);
		err(1, "execv() failed");
	}

	if (daemon_argv) free(daemon_argv);
	
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);

	sys_openlog("automount", LOG_NDELAY | LOG_PID, LOG_DAEMON);
	sys_msg(debug, LOG_INFO, "automount version %s", version);

	/* Disable the Carbon File Manager's use of DiskArb,
	   since the automounter never actually runs the main thread's CFRunLoop: */
	(void)_FSDisableAutoDiskArbHandling(0);

	MarkDirectoryInvisible(AUTOMOUNTDIRECTORYPATH);
	
	result = pipe(gWakeupFDs);
	if (result) {
		sys_msg(debug, LOG_ERR, "Couldn't open internal wakeup pipe: %s?!", strerror(errno));
		gWakeupFDs[0] = -1;
		gWakeupFDs[1] = -1;
	};
	
	gVFSEventKQueue = vfsevent_init();			/* Set up to wait for VFS events */
	
	signal(SIGHUP, reinit_sighandler);
	signal(SIGUSR1, usr1_signhandler);
	signal(SIGUSR2, usr2_sighandler);
	signal(SIGINT, shutdown_server_sighandler);
	signal(SIGQUIT, shutdown_server_sighandler);
	signal(SIGTERM, shutdown_server_sighandler);
	signal(SIGCHLD, child_exit_sighandler);    /* Depends on 'controller' being set up... */
	signal(SIGALRM, alarm_sighandler);

	/*
	 * Replace stdin, which might be a TTY, with /dev/null to prevent
	 * file systems like SMB from trying to use the terminal during mount:
	 */
	fclose(stdin);
	(void)open("/dev/null", 0, 0);
	
	controller = [[Controller alloc] init:mntdir];
	if (controller == nil)
	{
		sys_msg(debug, LOG_ERR, "Initialization failed!");
		exit(1);
	}

	print_host_info();

	// kick off the "main" cf run loop on it's own thread
	runLoop = systhread_new();
	systhread_run(runLoop, mainRunLoop, NULL);
	systhread_yield();
	
	run_select_loop = 1;
	rpcLoop = systhread_new();
	systhread_run(rpcLoop, select_loop, NULL);
	systhread_yield();

#if SEPARATE_VFSEVENT_THREAD
	run_vfseventwait_loop = 1;
	vfseventLoop = systhread_new();
	systhread_run(vfseventLoop, vfseventwait_loop, NULL);
	systhread_yield();
#endif

	/* Wait to make sure the main event loop is prepared in case some maps
	   use CFRunLoop event sources, like NSLMap */
	while (!runloop_prepared) {
		usleep(100*1000);			/* Sleep for 0.1 Sec. */
	};
	
	if (nmaps == 0)
	{
		fprintf(stderr, "No maps to mount!\n");
		if (becomeDaemon || forkedExecution) kill(getppid(), SIGTERM);
		else usage();
		exit(0);
	}

	/*
	   Hold of running re-inits in do_select on separate select thread (rpcLoop)
	   until initialization of all maps is complete and the re-init code won't
	   race along in parallel with the main thread initializing the maps:
	 */
	suspend_reinitialization();
	
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-tl")) i++;
		else if (!strcmp(argv[i], "-tm")) i++;
		else if (!strcmp(argv[i], "-m"))
		{
			mapDir = [String uniqueString:argv[i+1]];
			mapName = [String uniqueString:argv[i+2]];
			i += 2;
			if ((argc > (i+1)) && !strcmp(argv[i+1], "-mnt"))
			{
				strncpy(mountPathPrefix, argv[i+2], sizeof(mountPathPrefix));	/* Careful of possible overruns */
				mountPathPrefix[sizeof(mountPathPrefix)-1] = '\000';			/* Ensure proper termination */
				mountPathPrefixLength = strlen(mountPathPrefix);
				while ((mountPathPrefixLength > 1) && (mountPathPrefix[mountPathPrefixLength-1] == '/')) {
					mountPathPrefix[mountPathPrefixLength-1] = '\000';	/* Strip off trailing slash */
					--mountPathPrefixLength;
				};
				mountDir = [String uniqueString:mountPathPrefix];
				i += 2;
			} else {
				mountDir = nil;
			};
			[controller mountmap:mapName directory:mapDir mountdirectory:mountDir];
			if (mountDir) [mountDir release];
			[mapName release];
			[mapDir release];
		}
	}

	if (staticMode)
		[[controller rootMap] mount:[[controller rootMap] root] withUid:0];
 	
	run_select_loop = 0;
	while (running_select_loop)
	{
		usleep(1000*100);
	}

	/*
	   It's now OK to enqueue reinitialization requests, since they'll be
	   handled on this main thread: */
	reenable_reinitialization();
	
	MarkDirectoryInvisible(AUTOMOUNTDIRECTORYPATH);

	sys_msg(debug, LOG_DEBUG, "Starting service");
	if (becomeDaemon || forkedExecution) kill(getppid(), SIGTERM);

	if (staticMode || (GlobalTimeToLive == 0)) auto_run_no_timeout(NULL);
	else auto_run(&timeout);

	shutdown_server();
	
	exit(0);
}
