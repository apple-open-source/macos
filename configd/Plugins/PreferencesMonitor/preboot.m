/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

/*
 * Modification History
 *
 * May 12, 2021		Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

/*
 * Threading: Main thread of configd calls syncNetworkConfigurationToPrebootVolume(),
 * which loads the DM/DA work on the runloop of a new pthread. Upon completion of the
 * work and the receipt of the async callback, the main thread pthread_joins. There is
 * a 1:1 mapping between a thread and a DM/DA session, therefore the whole updater
 * session and its associated objects are not shared across different activations of
 * the updater runloop. This approach provides the following solutions:
 * - Control of the update_in_progress/sync_needed logic stays with the main thread
 * - The extra pthread isn't kept around needlessly
 * - Emulates a "dispatched-on-demand" version of the DM/DA APIs (currently only support runloop)
 * Note: The target product for this file does NOT build with ARC.
 */

#import "preboot.h"
#import "prefsmon_log.h"
#import <os/boot_mode_private.h>
#import <limits.h>
#import <uuid/uuid.h>
#import <sys/types.h>
#import <sys/sysctl.h>
#import <string.h>
#import <pthread.h>
#import <AssertMacros.h>

#import <SystemConfiguration/SystemConfiguration.h>
#import <SystemConfiguration/SCPrivate.h>
#import <SystemConfiguration/SCValidation.h>
#import <DiskArbitration/DiskArbitration.h>
#import <DiskManagement/DMAPFS.h>
#import <DiskManagement/DMManager.h>
#import <DiskManagement/DMManagerInfo.h>

@interface PrebootUpdater : NSObject<DMAsyncDelegate>
@property (strong,nonatomic)DMManager *	manager;
@property (nonatomic)DADiskRef		disk;
@property (strong, nonatomic)DMAPFS *	dmapfs;
@property (nonatomic) DASessionRef	session;
@end

#if defined(TEST_PREBOOT)
static bool S_retry;
static bool S_exit_on_completion;
#endif
static bool update_in_progress;
static bool sync_needed;
static CFRunLoopRef G_main_runloop;
static pthread_t G_sync_thread;

static int syncThreadCreate(void);
static void cleanupUpdater(PrebootUpdater *);
static void * syncToPreboot(void * info);
static void syncComplete(void);

#define _LOG_PREFIX		"syncNetworkConfigurationToPrebootVolume"

@implementation PrebootUpdater

- (instancetype)initWithManager:(DMManager *)manager disk:(DADiskRef)disk
			 DMAPFS:(DMAPFS *)dmapfs session:(DASessionRef)session
{
	self = [super init];
	_manager = manager;
	_disk = disk;
	_dmapfs = dmapfs;
	_session = session;
	[manager setDelegate:self];
	return (self);
}

- (void)dealloc
{
	if (_dmapfs != nil) {
		[_dmapfs release];
		_dmapfs = nil;
	}
	if (_manager != nil) {
		[_manager done];
		[_manager release];
		_manager = nil;
	}
	if (_disk != NULL) {
		CFRelease(_disk);
		_disk = NULL;
	}
	if (_session != NULL) {
		DASessionUnscheduleFromRunLoop (_session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		CFRelease(_session);
		_session = NULL;
	}
	[super dealloc];
	return;
}

- (void)dmAsyncStartedForDisk:(nullable DADiskRef)inDisk
{
#if defined(TEST_PREBOOT)
	SC_log(LOG_DEBUG, "%s: %@", _LOG_PREFIX, NSStringFromSelector(_cmd));
#endif
}

- (void)dmAsyncProgressForDisk:(nullable DADiskRef)inDisk
		    barberPole:(BOOL)inBarberPole percent:(float)inPercent
{
#if defined(TEST_PREBOOT)
	SC_log(LOG_DEBUG, "%s: %f%% complete", _LOG_PREFIX, inPercent);
	if (S_retry && inPercent > 25) {
		S_retry = false;
		SC_log(LOG_NOTICE, "Test: scheduling sync again");
		CFRunLoopPerformBlock(G_main_runloop, kCFRunLoopDefaultMode, ^{
			syncNetworkConfigurationToPrebootVolume();
		});
		CFRunLoopWakeUp(G_main_runloop);
	}
#endif
}

- (void)dmAsyncMessageForDisk:(nullable DADiskRef)inDisk
		       string:(NSString *)inString
		   dictionary:(nullable NSDictionary *)inDictionary
{
}

- (void)dmAsyncFinishedForDisk:(nullable DADiskRef)inDisk
		     mainError:(DMDiskErrorType)inMainError
		   detailError:(DMDiskErrorType)inDetailError
		    dictionary:(nullable NSDictionary *)inDictionary
{
	CFRunLoopPerformBlock(G_main_runloop, kCFRunLoopDefaultMode, ^{
		syncComplete();
	});
	CFRunLoopWakeUp(G_main_runloop);
	CFRunLoopPerformBlock(CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, ^{
		cleanupUpdater(self);
	});
}

@end

static void
cleanupUpdater(PrebootUpdater * updater)
{
	if (updater != nil) {
		[updater release];
		updater = nil;
	}
	CFRunLoopStop(CFRunLoopGetCurrent());
}

static void
syncComplete(void)
{
	bool sync_started = false;
	int res = 0;

	SC_log(LOG_NOTICE, "%s: sync complete", _LOG_PREFIX);
	res = pthread_join(G_sync_thread, NULL);
	require_noerr_quiet(res, done);
	G_sync_thread = NULL;
	if (!sync_needed) {
		update_in_progress = false;
	} else {
		SC_log(LOG_NOTICE, "%s: starting sync for queued request",
		       _LOG_PREFIX);
		sync_needed = false;
		sync_started = true;
		res = syncThreadCreate();
		require_noerr_quiet(res, done);
	}
#if defined(TEST_PREBOOT)
	if (!sync_started && S_exit_on_completion) {
		SC_log(LOG_NOTICE, "%s: All done, exiting",
		       _LOG_PREFIX);
		exit(0);
	}
#endif
	return;
done:
	SC_log(LOG_NOTICE, "%s: Sync failed to complete gracefully with err %d",
	       _LOG_PREFIX, res);
	return;
}

static PrebootUpdater *
allocateUpdater(void)
{
	DADiskRef		disk = NULL;
	DMAPFS *		dmapfs = nil;
	DMDiskErrorType		error;
	DMManager *		manager = nil;
	DASessionRef		session = NULL;
	PrebootUpdater * 	updater = nil;

	session = DASessionCreate(kCFAllocatorDefault);
	if (session == NULL) {
		SC_log(LOG_NOTICE, "%s: DASessionCreate failed", _LOG_PREFIX);
		goto failed;
	}
	DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	manager = [[DMManager alloc] init];
	[manager setDefaultDASession:session];
	disk = [manager copyRootDisk:&error];
	if (disk == NULL) {
		SC_log(LOG_NOTICE, "%s: Failed to copy root disk, %d",
		       _LOG_PREFIX, error);
		goto failed;
	}
	dmapfs = [[DMAPFS alloc] initWithManager:manager];
	updater = [[PrebootUpdater alloc]
		   initWithManager:manager
		   disk:disk
		   DMAPFS:dmapfs
		   session:session];
	
	return (updater);

 failed:
	if (dmapfs != nil) {
		[dmapfs release];
	}
	if (manager != nil) {
		[manager done];
		[manager release];
	}
	if (disk != NULL) {
		CFRelease(disk);
	}
	if (session != NULL) {
		DASessionUnscheduleFromRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		CFRelease(session);
	}
	return (nil);
}

static const char *
get_boot_mode(void)
{
	const char *	mode = NULL;
	bool		success;

	success = os_boot_mode_query(&mode);
	if (!success) {
		SC_log(LOG_NOTICE, "os_boot_mode_query failed");
		return (NULL);
	}
	return (mode);
}

static void *
syncToPreboot(void * info)
{
#pragma unused(info)
	@autoreleasepool {
		DMDiskErrorType error = 0;
		PrebootUpdater *updater = allocateUpdater();
		require_action_quiet(updater != nil, done,
				     SC_log(LOG_NOTICE,
					    "%s: Failed to create <DMAsyncDelegate> instance",
					    _LOG_PREFIX));
		error = [[updater dmapfs] updatePrebootForVolume:[updater disk] options:nil];
		require_noerr_action_quiet(error, done,
					   SC_log(LOG_NOTICE,
						  "%s: [DMAPFS updatePrebootForVolume] failed %d",
						  _LOG_PREFIX, error));
		SC_log(LOG_NOTICE, "%s: sync started", _LOG_PREFIX);
		CFRunLoopRun();
		return nil;
	}
done:
	CFRunLoopPerformBlock(G_main_runloop, kCFRunLoopDefaultMode, ^{
		syncComplete();
	});
	CFRunLoopWakeUp(G_main_runloop);
	return nil;
}

static int
syncThreadCreate(void)
{
	int ret = 0;
	pthread_attr_t threadAttrs = {0};

	ret = pthread_attr_init(&threadAttrs);
	require_noerr_quiet(ret, done);
	ret = pthread_attr_setdetachstate(&threadAttrs, PTHREAD_CREATE_JOINABLE);
	require_noerr_quiet(ret, done);
	ret = pthread_create(&G_sync_thread, &threadAttrs, syncToPreboot, NULL);
	require_noerr_quiet(ret, done);
	ret = pthread_attr_destroy(&threadAttrs);
	require_noerr_quiet(ret, done);
	return ret;
done:
	SC_log(LOG_NOTICE, "%s: Failed to bring up sync thread with err %d", __func__, ret);
	return ret;
}

bool
syncNetworkConfigurationToPrebootVolume(void)
{
	const char *	boot_mode;
	bool		success = false;

	boot_mode = get_boot_mode();
	if (boot_mode != NULL) {
		SC_log(LOG_NOTICE,
		       "%s: boot mode is %s, not syncing",
		       __func__, boot_mode);
		goto done;
	}
	if (G_main_runloop == NULL) {
		G_main_runloop = CFRunLoopGetCurrent();
	}
	if (update_in_progress) {
		sync_needed = true;
	} else {
		update_in_progress = true;
		require_noerr_quiet(syncThreadCreate(), done);
	}
	success = true;
 done:
	return (success);
}

#if defined(TEST_PREBOOT)

#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>

static void
timer_fired(CFRunLoopTimerRef timer, void *info)
{
#pragma unused(timer)
#pragma unused(info)
	SC_log(LOG_NOTICE, "timer fired");
	syncNetworkConfigurationToPrebootVolume();
}

static void
schedule_retry_timer(void)
{
	CFRunLoopTimerRef	timer;

	timer = CFRunLoopTimerCreate(NULL,
				     0,
				     1.0,
				     0,
				     0,
				     timer_fired,
				     NULL);
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
}

int
main(int argc, char *argv[])
{
	int		ch;
	bool		retry_on_timer = false;

	while ((ch = getopt(argc, argv, "ert")) != EOF) {
		switch (ch) {
		case 'e':
			S_exit_on_completion = true;
			break;
		case 'r':
			S_retry = true;
			break;
		case 't':
			retry_on_timer = true;
			break;
		default:
			break;
		}
	}
	if (syncNetworkConfigurationToPrebootVolume()) {
		printf("Started...\n");
		if (retry_on_timer) {
			schedule_retry_timer();
		}
		CFRunLoopRun();
	}
	exit(0);
	return (0);
}

#endif /* TEST_PREBOOT */
