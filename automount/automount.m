/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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
#import <sys/stat.h>
#import <sys/wait.h>
#import <sys/time.h>
#import <sys/resource.h>
#import "automount.h"
#import "AMVnode.h"
#import "Controller.h"
#import "AMString.h"
#import "log.h"
#import "AMVersion.h"
#import "systhread.h"
#import "NSLMap.h"
#import "NSLVnode.h"
#ifndef __APPLE__
#import <libc.h>
extern int getppid(void);
#endif

#define forever for(;;)

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
CFRunLoopRef gMainRunLoop;
CFRunLoopTimerRef gRunLoopTimer;
int runloop_prepared = 0;


int gWakeupFDs[2] = { -1, -1 };

static BOOL gReinitializationSuspended = FALSE;
static BOOL gReinitDeferred = FALSE;
static BOOL gNetworkChangeDeferred = FALSE;
static BOOL gUserLogoutDeferred = FALSE;

static struct timeval last_timeout;

int osType;

BOOL doServerMounts = YES;

NSLMap *GlobalTargetNSLMap;

MountProgressRecord_List gMountsInProgress = LIST_HEAD_INITIALIZER(&gMountsInProgress);

BOOL gForkedMountInProgress = NO;
BOOL gForkedMount = NO;
BOOL gBlockedMountDependency = NO;
unsigned long gBlockingMountTransactionID;
int gMountResult;
int gVolumeUnmountToken;
BOOL gTerminating = FALSE;

BOOL gUserLoggedIn = NO;

NSLMapList gNSLMapList = LIST_HEAD_INITIALIZER(&gNSLMapList);

#define DefaultMountDir "/private/var/automount"

static char gForkedExecutionFlag[] = "-f";
static char gAutomounterPath[] = "/usr/sbin/automount";

struct debug_fdset
{
	unsigned int i[8];
};

static void child_exit(void);
static void shutdown_server(void);

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
enqueue_reinit_request(void) {
	char request_code[1] ={  REQ_REINIT };

	if (gReinitializationSuspended) {
		sys_msg(debug, LOG_ERR, "deferring re-init while init is in progress...");
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
		sys_msg(debug, LOG_ERR, "deferring network change notification while init is in progress...");
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

	sys_msg(debug, LOG_ERR, "logout notification received.");
	if (gReinitializationSuspended) {
		sys_msg(debug, LOG_ERR, "deferring user logout notification while init is in progress...");
		gUserLogoutDeferred = TRUE;
	} else {
		if (gWakeupFDs[1] != -1) {
			sys_msg(debug, LOG_ERR, "requesting logout processing.");
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
		sys_msg(debug, LOG_ERR, "reposting deferred logout notification.");
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
		sys_msg(debug, LOG_ERR, "handle_deferred_requests: user logged out.");
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

	  case REQ_USR2:
		sys_msg(debug, LOG_DEBUG, "handle_deferred_requests: printing mount tree on SIGUSR2.");
		[controller printTree];
		break;
		
	  default:
	    sys_msg(debug, LOG_DEBUG, "handle_enqueued_requests: Unknown request code '%c'?!", request_code);
		break;
	};
};

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

	if (tv) {
		sys_msg(debug_select, LOG_DEBUG, "select timeout %d %d", tv->tv_sec, tv->tv_usec);
	};

	cleanuptimeout(tv);
	n = select(FD_SETSIZE, &x, NULL, NULL, tv);

	if (n != 0)
		sys_msg(debug_select, LOG_DEBUG, "select(%s, %d) --> %d",
			fdtoc(&x), tv->tv_sec, n);
	if (n < 0)
		sys_msg(debug_select, LOG_DEBUG, "select: %s", strerror(errno));

	if (n > 0) {
		if ((gWakeupFDs[0] != -1) && FD_ISSET(gWakeupFDs[0], &x)) {
			char request_code[1];
			
			(void)read(gWakeupFDs[0], request_code, sizeof(request_code));
			handle_enqueued_requests(request_code[0]);
			return 0;
		};

		svc_getreqset(&x);
	};

	if (gForkedMount) exit((gMountResult < 128) ? gMountResult : ECANCELED);
	
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

void
usage(void)
{
	fprintf(stderr, "usage: automount [options]...\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -V            ");
	fprintf(stderr, "Print version and host information, then quit.\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  -a dir        ");
	fprintf(stderr, "Set mount directory to dir.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "Default value is \"%s\".\n", DefaultMountDir);
	fprintf(stderr, "\n");

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

	fprintf(stderr, "  -m dir map [-mnt dir]");
	fprintf(stderr, "Mount map on directory dir.\n");
	fprintf(stderr, "Optionally followed by specification of private mount dir.\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "map may be a file (must be an absolute path),\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "a NetInfo mountmap name,\n");
	fprintf(stderr, "                ");
	fprintf(stderr, "\"-fstab\", or \"-null\".\n");
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
	sys_msg(debug, LOG_ERR, "Shutting down.");

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
		if ((pid != 0) && (pid != -1)) {
			[controller completeMountInProgressBy:pid exitStatus:result];
		};
	};
}

static void
child_exit_sighandler(int x)
{
	if (gWakeupFDs[1] != -1) {
		char request_code[1] = { REQ_MOUNTCOMPLETE };
		
		(void)write(gWakeupFDs[1], request_code, sizeof(request_code));
	} else {
		sys_msg(debug, LOG_ERR, "child_exit_sighandler: gWakeupFDs[1] uninitialized.");
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
				sys_msg(debug, LOG_DEBUG, "the console user is logged in\n");
				gUserLoggedIn = YES;		/* One-shot, never cleared */
				if (gUserLogoutDeferred) {
					sys_msg(debug, LOG_ERR, "canceling deferred logout notification...");
					gUserLogoutDeferred = NO;
				};
				CFRelease(user);
			} else {
				sys_msg(debug, LOG_DEBUG, "the console user has logged out\n");
				enqueue_userlogout_notification();
			}
			continue;
		}

	    if (CFStringCompare(key, netinfoChangedKey, 0) == kCFCompareEqualTo) {
		sys_msg(debug, LOG_DEBUG, "the netinfo configuration has changed\n");
		networkChanged = YES;
		continue;
	    }
	    
	    if (CFStringCompare(key, searchPolicyChangedKey, 0) == kCFCompareEqualTo) {
		sys_msg(debug, LOG_DEBUG, "directory services search policy changed\n");
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
		sys_msg(debug, LOG_ERR, "could not open session with configd\n");
		sys_msg(debug, LOG_ERR, "error = %s\n", SCErrorString(SCError()));
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
		sys_msg(debug, LOG_ERR, "could not register notification keys\n");
		sys_msg(debug, LOG_ERR, "error = %s\n", SCErrorString(SCError()));
		CFRelease(store);
		CFRelease(keys);
		return;
	}
	CFRelease(keys);

	/* add a callback */
	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!rls) {
		sys_msg(debug, LOG_ERR, "could not create runloop source\n");
		sys_msg(debug, LOG_ERR, "error = %s\n", SCErrorString(SCError()));
		CFRelease(store);
		return;
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
}

void VolumeUnmounted(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	char request_code[1];
	sys_msg(debug, LOG_DEBUG, "Volume unmounted notification");

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
	String *mountDir;
	int pid, i, nmaps, result;
	struct timeval timeout;
	BOOL becomeDaemon, forkedExecution, printVers, staticMode;
	struct rlimit rlim;
	systhread *rpcLoop, *runLoop;
	struct stat sb;
	
	if ((argc > 1) && (strcmp(argv[1], gForkedExecutionFlag))) {
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
	sys_msg(debug, LOG_ERR, "automount version %s", version);

	result = pipe(gWakeupFDs);
	if (result) {
		sys_msg(debug, LOG_ERR, "Couldn't open internal wakeup pipe: %s?!", strerror(errno));
		gWakeupFDs[0] = -1;
		gWakeupFDs[1] = -1;
	};
	
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
				mountDir = [String uniqueString:argv[i+2]];
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
	
	sys_msg(debug, LOG_DEBUG, "Starting service");
	if (becomeDaemon || forkedExecution) kill(getppid(), SIGTERM);

	if (staticMode) auto_run_no_timeout(NULL);
	else auto_run(&timeout);

	shutdown_server();
	exit(0);
}
