/*
 * Copyright (c) 2007-2011 Apple Inc. All rights reserved.
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
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <spawn.h>
#include <syslog.h>
#include <err.h>

#include <dispatch/dispatch.h>

#include <notify.h>

#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>

static char automount_path[] = "/usr/sbin/automount";

static int logging_debug = 0;	// 1 if logging debugging messages, 0 if not

static void run_automount(char *);

int
main(__unused int argc, __unused char **argv)
{
#define NUM_RECORD_TYPE_NAMES	3
	CFTypeRef record_type_names[NUM_RECORD_TYPE_NAMES];
	CFArrayRef record_types;
	uint32_t status;
	int volume_unmount_token;
	dispatch_source_t usr1_source;

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
	 * Listen for changes to the OD search nodes; that will tell us
	 * if a search node was removed (which means any auto_master map
	 * entries or mount records we got from it earlier aren't valid),
	 * a search node (server) went offline (which we view as similar
	 * to that search node being removed), or a search node (server)
	 * came online (which means we might have new map entries or mount
	 * records to pick up from it).  We don't worry about search nodes
	 * being added; they're not interesting until they're online, as
	 * until then we won't get anything new from them, and a
	 * notification will be delivered if they do come online.
	 */
	ODTriggerCreateForSearch(kCFAllocatorDefault,
	    kODTriggerSearchDelete|kODTriggerSearchOffline|kODTriggerSearchOnline,
	    NULL, dispatch_get_main_queue(),
	    ^(__unused ODTriggerRef trigger, __unused CFStringRef node)
		{
			if (logging_debug)
				syslog(LOG_ERR, "Got an OD search node change notification");
			run_automount("-c");
		});

	/*
	 * Listen for changes to automounter map entries, auto_master
	 * map entries, and mount records.
	 */
	record_type_names[0] = kODRecordTypeAutomount;
	record_type_names[1] = kODRecordTypeAutomountMap;
	record_type_names[2] = kODRecordTypeMounts;
	record_types = CFArrayCreate(kCFAllocatorDefault, record_type_names,
	    NUM_RECORD_TYPE_NAMES, &kCFTypeArrayCallBacks);
	if (record_types == NULL) {
		syslog(LOG_ERR, "Couldn't create array of OD record types");
		exit(EXIT_FAILURE);
	}
	ODTriggerCreateForRecords(kCFAllocatorDefault,
	    kODTriggerRecordEventAdd|kODTriggerRecordEventDelete|kODTriggerRecordEventModify,
	    NULL, record_types, NULL, dispatch_get_main_queue(),
	    ^(__unused ODTriggerRef trigger, __unused CFStringRef node,
	      __unused CFStringRef type, __unused CFStringRef name)
		{
			if (logging_debug)
				syslog(LOG_ERR, "Got an OD record change notification");
			run_automount("-c");
		});

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
	 *
	 * In this case, we don't have any reason to believe that
	 * any cached information in automountd is out of date -
	 * nothing in OD or networking changed - so don't flush
	 * automounter caches.
	 */
	status = notify_register_dispatch("com.apple.system.kernel.unmount",
	    &volume_unmount_token, dispatch_get_main_queue(),
	    ^(__unused int t)
		{
			if (logging_debug)
				syslog(LOG_ERR, "Got an unmount notification");
			run_automount(NULL);
		});
	if (status != NOTIFY_STATUS_OK) {
		syslog(LOG_ERR, "Couldn't add volume unmount notifications to the main dispatch queue: %u",
		    status);
		exit(EXIT_FAILURE);
	}

	/*
	 * Register for SIGUSR1 notifications and, when we get one,
	 * toggle the state of debug logging.
	 *
	 * (Yes, you have to ignore the signal.  See the
	 * dispatch_source_create() man page.  Making it a dispatch
	 * source doesn't mean it doesn't get delivered to us as
	 * a regular signal, and ignoring it doesn't mean it won't
	 * get delivered to us as a regular signal.  EVFILT_SIGNAL
	 * kevents are your friend....)
	 */
	signal(SIGUSR1, SIG_IGN);
	usr1_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL,
	    SIGUSR1, 0, dispatch_get_main_queue());
	if (usr1_source == NULL) {
		syslog(LOG_ERR, "Couldn't create a dispatch source for SIGUSR1");
		exit(EXIT_FAILURE);
	}
	dispatch_source_set_event_handler(usr1_source,
	    ^(void) { logging_debug = !logging_debug; });
	dispatch_resume(usr1_source);

	/*
	 * Set up the initial set of mounts.
	 */
	run_automount("-c");

	/*
	 * Now wait to be told to update the mounts.
	 */
	dispatch_main();

	syslog(LOG_ERR, "Run loop exited");

	return 0;
}

/*
 * Run the automount command with a given flag.
 * Either:
 *
 *	"-c" to re-evaluate what triggers are to be mounted, try to mount
 *	new triggers and unmount triggers no longer desired, and to
 *	flush automounter caches;
 *
 *	"-u" to unmount automounted filesystems;
 *
 *	NULL to just re-evaluate what triggers are to be mounted and try
 *	to mount new triggers and unmount triggers no longer desired
 *	without flushing automounter caches.
 */
static void
run_automount(char *flag)
{
	int error;
	char *args[3];
	int i;
	pid_t child;
	pid_t pid;
	int status;
	extern char **environ;
	static struct timeval last_run_automount;	// last run_automount time
	struct timeval now;

	if (flag != NULL && strcmp(flag, "-u") == 0) {
		/*
		 * Network change events usually come in a flurry.
		 * Don't unmount automounts more than once in
		 * 5 seconds.
		 */
		(void) gettimeofday(&now, NULL);
		if (now.tv_sec < last_run_automount.tv_sec + 5)
			return;
	}
	last_run_automount = now;

	i = 0;
	args[i++] = automount_path;
	if (flag != NULL)
		args[i++] = flag;
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
