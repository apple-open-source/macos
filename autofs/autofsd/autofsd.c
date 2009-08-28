/*
 * Copyright (c) 2007-2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <spawn.h>
#include <syslog.h>
#include <err.h>

#include <notify.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
/* The following constant was stolen from <DirectoryService/DirServicesPriv.h> */
#ifndef kDSStdNotifySearchPolicyChanged
#define	kDSStdNotifySearchPolicyChanged		"com.apple.DirectoryService.NotifyTypeStandard:SearchPolicyChanged"
#endif

static char automount_path[] = "/usr/sbin/automount";

static void sc_callback(SCDynamicStoreRef, CFArrayRef, void *);
static void debounce_callback(CFRunLoopTimerRef, void *);
static void volume_unmounted_callback(CFMachPortRef, void *, CFIndex, void *);
static void cancel_debounce_timer(void);
static void setup_mounts(void);

static CFRunLoopTimerRef debounce_timer;

int
main(__unused int argc, __unused char **argv)
{
	CFRunLoopSourceRef rls;
	SCDynamicStoreRef store;
	CFMutableArrayRef keys, patterns;
	CFStringRef key, pattern;
	uint32_t status;
	int volume_unmount_token;
	mach_port_t port;
	CFMachPortRef mach_port_ref;
	CFMachPortContext context = { 0, NULL, NULL, NULL, NULL };

	/* 
	 * If launchd is redirecting these two files they'll be block-
	 * buffered, as they'll be pipes, or some other such non-tty,
	 * sending data to launchd. Probably not what you want.
	 */
	setlinebuf(stdout);
	setlinebuf(stderr);

	openlog("autofsd", LOG_PID, LOG_DAEMON);
	(void) setlocale(LC_ALL, "");
	(void) umask(0);

	/*
	 * Register for notifications from the System Configuration framework.
	 * We want to re-evaluate the autofs mounts to be done whenever
	 * we get a network change or Directory Service search policy change
	 * notification, as those might indicate that the automounter maps
	 * and fstab entries we would get have changed.
	 */
	store = SCDynamicStoreCreate(NULL, CFSTR("autofsd"), sc_callback, NULL);
	if (!store) {
		syslog(LOG_ERR, "Couldn't open session with configd: %s",
		    SCErrorString(SCError()));
		exit(EXIT_FAILURE);
	}

	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/*
	 * Establish dynamic store keys and patterns for notifications from
	 * network change events.
	 *
	 * We do this because a host might have a hardwired binding to
	 * a particular set of NIS/LDAP/Active Directory/etc. directory
	 * servers, and might move between "able to get information from
	 * those servers" and "unable to get information from those
	 * servers" when it moves from one network to another, rather
	 * than, for example, getting its directory server bindings from
	 * DHCP, in which case we won't get a Directory Service search policy
	 * change notification when we change networks, even though that
	 * will change what automounter maps and/or fstab entries we'll
	 * get, but we will, at least, get a network change event.
	 */

	/*
	 * Look for changes to any IPv4 information...
	 */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
		kSCDynamicStoreDomainState, kSCEntNetIPv4);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/*
	 * ...on any interface.
	 */
	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
		kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/*
	 * Look for changes to any IPv6 information...
	 */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
		kSCDynamicStoreDomainState, kSCEntNetIPv6);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/*
	 * ...on any interface.
	 */
	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
		kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv6);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/*
	 * Establish and register dynamic store key to watch directory
	 * services search policy changes.
	 */
	key = SCDynamicStoreKeyCreate(NULL,
	    CFSTR(kDSStdNotifySearchPolicyChanged));
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	if (!SCDynamicStoreSetNotificationKeys(store, keys, patterns)) {
		syslog(LOG_ERR, "Couldn't register notification keys: %s",
		    SCErrorString(SCError()));
		CFRelease(store);
		CFRelease(keys);
		CFRelease(patterns);
		exit(EXIT_FAILURE);
	}
	CFRelease(keys);
	CFRelease(patterns);

	/* add a callback */
	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (rls == NULL) {
		syslog(LOG_ERR, "Couldn't create run loop source: %s",
		    SCErrorString(SCError()));
		CFRelease(store);
		exit(EXIT_FAILURE);
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

	/*
	 * Also watch for volume unmounts.  If, for example, you
	 * disconnect from a network, and some mount is no longer
	 * accessible, but it's mounted atop an autofs mount, and
	 * that autofs mount should be removed (because we're no
	 * longer getting that fstab or map entry from Directory
	 * Services), it can't be removed at the time we get the
	 * network change notification.  However, if the user later
	 * gets a "server not responding" dialog and asks to disconnect
	 * from the server, the mount will be forcibly unmounted,
	 * which might allow us to unmount the autofs mount.
	 */
	status = notify_register_mach_port("com.apple.system.kernel.unmount",
	    &port, 0, &volume_unmount_token);
	if (status != NOTIFY_STATUS_OK) {
		syslog(LOG_ERR, "Couldn't get Mach port for volume unmount notifications: %u",
		    status);
		exit(EXIT_FAILURE);
	}
	
	mach_port_ref = CFMachPortCreateWithPort(NULL, port,
	    volume_unmounted_callback, &context, NULL);
	if (mach_port_ref == NULL) {
		syslog(LOG_ERR, "Couldn't create CFMachPort for volume unmount notifications");
		exit(EXIT_FAILURE);
	}

	rls = CFMachPortCreateRunLoopSource(NULL, mach_port_ref, 0);
	if (rls == NULL) {
		syslog(LOG_ERR, "Couldn't create CFRunLoopSource for volume unmount notifications");
		exit(EXIT_FAILURE);
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

	/*
	 * Set up the initial set of mounts.
	 */
	setup_mounts();

	/*
	 * Now wait to be told to update the mounts.
	 */
	CFRunLoopRun();

	syslog(LOG_ERR, "Run loop exited");

	return 0;
}

#define STABLE_DELAY	5.0	// sec to delay after receiving notification

/*
 * Network notifications, in particular, can come thick and fast.
 * To avoid doing a mount check on each one, we wait STABLE_DELAY
 * seconds for a stable network status.
 * Each network notification restarts the debounce timer.
 * It calls its callback function only if it has been running
 * uninterrupted for STABLE_DELAY seconds.
 *
 * XXX - do we want to debounce DS policy change notifications, or do we
 * want to respond to those immediately, canceling any running debounce
 * timer?
 */
static void
sc_callback(__unused SCDynamicStoreRef store, __unused CFArrayRef changedKeys,
    __unused void *info)
{
	if (debounce_timer) {
		/* Already running - cancel the timer */
		cancel_debounce_timer();
	}

	debounce_timer = CFRunLoopTimerCreate(NULL,
		CFAbsoluteTimeGetCurrent() + STABLE_DELAY,
		0.0, 0, 0, debounce_callback, NULL);
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), debounce_timer,
		kCFRunLoopDefaultMode);
}

static void
debounce_callback(__unused CFRunLoopTimerRef timer, __unused void *info)
{
	cancel_debounce_timer();

	setup_mounts();
}

static void
volume_unmounted_callback(__unused CFMachPortRef port, __unused void *msg,
    __unused CFIndex size, __unused void *info)
{
	if (debounce_timer) {
		/*
		 * The debounce timer is running; wait for it to fire
		 * before running automount.
		 */
		return;
	}

	/*
	 * Run automount now, to try to get rid of any out-of-date triggers
	 * ASAP.
	 */
	setup_mounts();
}

static void
cancel_debounce_timer(void)
{
	CFRunLoopTimerInvalidate(debounce_timer);
	CFRelease(debounce_timer);
	debounce_timer = NULL;
}

static void
setup_mounts(void)
{
	int error;
	char *args[3];
	int i;
	pid_t child;
	pid_t pid;
	int status;
	extern char **environ;

	i = 0;
	args[i++] = automount_path;
	args[i++] = "-c";	/* tell automountd to clear its caches */
	args[i] = NULL;
	error = posix_spawn(&child, automount_path, NULL, NULL, args,
	    environ);
	if (error != 0) {
		syslog(LOG_ERR, "Can't run %s: %s", automount_path,
		    strerror(error));
		return;
	}

	/*
	 * Wait for the child to complete.
	 */
	for (;;) {
		pid = waitpid(child, &status, 0);
		if (pid == child)
			break;
		if (pid == -1 && errno != EINTR) {
			syslog(LOG_ERR, "Error %m while waiting for %s",
			    automount_path);
			return;
		}
	}
	if (WIFSIGNALED(status)) {
		syslog(LOG_ERR, "%s terminated with signal %s%s",
		    automount_path, strsignal(WTERMSIG(status)),
		    WCOREDUMP(status) ? "- core dumped" : "");
	}
}
