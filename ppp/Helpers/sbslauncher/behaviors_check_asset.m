/*
 * Copyright (c) 2012 Apple Inc.
 * All rights reserved.
 */

#include <sys/socket.h>
#include <netinet/in.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <MobileAsset/MobileAsset.h>
#include <AppSupport/AppSupportUtils.h>

#include "behaviors_defines.h"
#include "behaviors_check_asset.h"

static SCPreferencesRef g_prefs = NULL;

static SCNetworkReachabilityRef g_inet_reach_target = NULL;
static SCNetworkReachabilityRef g_inet6_reach_target = NULL;

static void
process_finish(SCPreferencesRef prefs, NSError *error)
{
	CFTimeInterval interval = 0;
	CFAbsoluteTime next_check_date;
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
	CFNumberRef cf_abs_time = NULL;

	if (error != nil) {
		if ([error code] == ASErrorNetwork || [error code] == ASErrorNetworkNoConnection ||
		    [error code] == ASErrorNetworkUnexpectedResponse)
		{
			interval = kVPNBehaviorsAssetCheckErrorInterval;
		} else {
			interval = kVPNBehaviorsAssetCheckDefaultInterval;
		}
	}
   
	if (interval == 0) {
		/* If the prefs contain an interval, check again that many seconds in the future */
		CFNumberRef asset_check_interval = SCPreferencesGetValue(prefs, kVPNBehaviorsAssetCheckInterval);
		if (isA_CFNumber(asset_check_interval)) {
			CFTimeInterval prefs_interval;
			if (CFNumberGetValue(asset_check_interval, kCFNumberDoubleType, &prefs_interval) &&
			    prefs_interval >= kVPNBehaviorsAssetCheckMinInterval &&
			    prefs_interval <= kVPNBehaviorsAssetCheckMaxInterval)
			{
				interval = prefs_interval * kVPNBehaviorsTimeMultiplier;
			}
		}
	}

	if (interval == 0) {
		/* Use the default interval */
		interval = kVPNBehaviorsAssetCheckDefaultInterval;
	}

	SCLog(TRUE, LOG_NOTICE, CFSTR("Finished asset check, setting next asset check time to %lf seconds from now"), interval);

	next_check_date = now + interval;

	cf_abs_time = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &next_check_date);
	if (!SCPreferencesSetValue(prefs, kVPNBehaviorsNextAssetCheckTime, cf_abs_time)) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCPreferencesSetValue failed when setting the next check date: %s"),
		      SCErrorString(SCError()));
	}
	CFRelease(cf_abs_time);

	/* This will notify PPPController to re-configure VOD and schedule the next asset check */
	if (!SCPreferencesCommitChanges(prefs)) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCPreferencesCommitChanges failed"), SCErrorString(SCError()));
	}
}

static void
process_asset_query_results(NSArray *results, NSError *error, SCPreferencesRef prefs)
{
	if (results != nil && [results count] > 0) {
		NSNumber *max_version = nil;
		ASAsset *most_recent_asset = nil;

		/* Find which result is the newest */
		for (ASAsset *asset in results) {
			NSNumber *version = [asset.attributes objectForKey:(id)kVPNBehaviorsAssetVersion];
			if (version != nil) {
				if (max_version == nil || [version compare:max_version] == NSOrderedDescending) {
					max_version = version;
					most_recent_asset = asset;
				}
			}
		}

		if (most_recent_asset != nil) {
			CFBooleanRef always_means_onretry;
			CFArrayRef asset_regions;
			CFNumberRef asset_version;
			CFNumberRef interval;
			CFStringRef region_code;
			CFBooleanRef reset;

			always_means_onretry = (CFBooleanRef)[most_recent_asset.attributes objectForKey:(id)kVPNBehaviorsVODAlwaysMeansOnRetry];
			asset_regions = (CFArrayRef)[most_recent_asset.attributes objectForKey:(id)kVPNBehaviorsAssetRegions];
			region_code = CPGetDeviceRegionCode();
			interval = (CFNumberRef)[most_recent_asset.attributes objectForKey:(id)kVPNBehaviorsAssetCheckInterval];
			asset_version = (CFNumberRef)[most_recent_asset.attributes objectForKey:(id)kVPNBehaviorsAssetVersion];
			reset = (CFBooleanRef)[most_recent_asset.attributes objectForKey:(id)kVPNBehaviorsReset];

			SCLog(TRUE, LOG_NOTICE, CFSTR("Got asset version %@, applicable regions = %@, current region = %@, check interval = %@, reset = %@, always means on retry = %@"), asset_version, asset_regions, region_code, interval, reset, always_means_onretry);

			if (isA_CFArray(asset_regions)) {
				if (!isA_CFString(region_code) ||
				    !CFArrayContainsValue(asset_regions, CFRangeMake(0, CFArrayGetCount(asset_regions)), region_code))
				{
					always_means_onretry = NULL;
				}
			}

			if (isA_CFBoolean(always_means_onretry)) {
				SCPreferencesSetValue(prefs, kVPNBehaviorsVODAlwaysMeansOnRetry, always_means_onretry);
			} else {
				SCPreferencesRemoveValue(prefs, kVPNBehaviorsVODAlwaysMeansOnRetry);
			}

			if (isA_CFNumber(interval)) {
				SCPreferencesSetValue(prefs, kVPNBehaviorsAssetCheckInterval, interval);
			} else {
				SCPreferencesRemoveValue(prefs, kVPNBehaviorsAssetCheckInterval);
			}

			if (isA_CFNumber(asset_version)) {
				SCPreferencesSetValue(prefs, kVPNBehaviorsAssetVersion, asset_version);
			}

			if (isA_CFBoolean(reset) && CFBooleanGetValue(reset)) {
				SCPreferencesSetValue(prefs, kVPNBehaviorsVODAlwaysMeansOnRetry, kCFBooleanFalse);
				SCPreferencesSetValue(prefs, kVPNBehaviorsReset, reset);
			}
		}
	} else {
		if (error != nil) {
			SCLog(TRUE, LOG_ERR, CFSTR("asset query failed: %@"), error);
		} else {
			CFNumberRef curr_version = SCPreferencesGetValue(prefs, kVPNBehaviorsAssetVersion);
			SCLog(TRUE, LOG_NOTICE, CFSTR("No asset found with version > %@"), curr_version);
		}
	}

	process_finish(prefs, error);
}

static void
start_query(void)
{
	CFNumberRef current_version;
	ASAssetQuery *query;
	SCPreferencesRef prefs;

	prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("sbslauncher"), kVPNBehaviorsPlistFile);
	if (prefs == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCPreferencesCreate failed for %@: %s"), kVPNBehaviorsPlistFile, SCErrorString(SCError()));
		exit(1);
	}

	current_version = SCPreferencesGetValue(prefs, kVPNBehaviorsAssetVersion);
	
	query = [[ASAssetQuery alloc] initWithAssetType:@"com.apple.MobileAsset.VPNBehaviors"];

	if (isA_CFNumber(current_version)) {
		query.predicate = [NSPredicate predicateWithFormat:@"%K > %@", kVPNBehaviorsAssetVersion, current_version];
	}
	
	/* Send the query to the server */
	query.queriesLocalAssetInformationOnly = NO;

	[query startQuery:^(NSArray *results, NSError *error) {
		process_asset_query_results(results, error, prefs);
		[query release];
		CFRelease(prefs);
		if (error != nil) {
			exit([error code]);
		} else {
			exit(0);
		}
	}];
}

static void
reachability_callback(SCNetworkReachabilityRef target, SCNetworkReachabilityFlags flags, void *info)
{
#pragma unused(target, info)
	if (flags & kSCNetworkReachabilityFlagsReachable) {
		SCNetworkReachabilityUnscheduleFromRunLoop(g_inet_reach_target, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		CFRelease(g_inet_reach_target);
		g_inet_reach_target = NULL;

		SCNetworkReachabilityUnscheduleFromRunLoop(g_inet6_reach_target, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		CFRelease(g_inet6_reach_target);
		g_inet6_reach_target = NULL;

		start_query();
	}
}

static Boolean
check_reachability(void)
{
	struct sockaddr_storage ss;
	SCNetworkReachabilityContext reach_ctx;
	SCNetworkReachabilityFlags flags = 0;
	SCNetworkReachabilityRef inet_target;
	SCNetworkReachabilityRef inet6_target;

	/*
	 * We should not start the check until some network connection is available, start
	 * a reachability check for both IPv4 and IPv6.
	 */

	memset(&ss, 0, sizeof(ss));
	ss.ss_family = AF_INET;
	ss.ss_len = sizeof(struct sockaddr_in);

	inet_target = SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, (struct sockaddr *)&ss);
	if (SCNetworkReachabilityGetFlags(inet_target, &flags)) {
		if (flags & kSCNetworkReachabilityFlagsReachable) {
			CFRelease(inet_target);
			return TRUE;
		}
	}

	memset(&ss, 0, sizeof(ss));
	ss.ss_family = AF_INET6;
	ss.ss_len = sizeof(struct sockaddr_in6);
	flags = 0;

	inet6_target = SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, (struct sockaddr *)&ss);
	if (SCNetworkReachabilityGetFlags(inet6_target, &flags)) {
		if (flags & kSCNetworkReachabilityFlagsReachable) {
			CFRelease(inet_target);
			CFRelease(inet6_target);
			return TRUE;
		}
	}

	memset(&reach_ctx, 0, sizeof(reach_ctx));

	SCNetworkReachabilitySetCallback(inet_target, reachability_callback, &reach_ctx);
	SCNetworkReachabilityScheduleWithRunLoop(inet_target, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	SCNetworkReachabilitySetCallback(inet6_target, reachability_callback, &reach_ctx);
	SCNetworkReachabilityScheduleWithRunLoop(inet6_target, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	g_inet_reach_target = inet_target;
	g_inet6_reach_target = inet6_target;

	return FALSE;
}


int
behaviors_start_asset_check(int argc, const char *argv[])
{
#pragma unused(argc, argv)
	if (check_reachability()) {
		start_query();
	} /* else wait for reachability notifications */

	CFRunLoopRun();

	return 0;
}
