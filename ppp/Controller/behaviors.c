/*
 * Copyright (c) 2012 Apple Inc.
 * All rights reserved.
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCDPlugin.h>

#include "scnc_main.h"
#include "scnc_utils.h"
#include "behaviors.h"
#include "sbslauncher.h"
#include "behaviors_defines.h"

#define kVPNBehaviorsAssetCheckSmallDelay		120.0		/* 2 minutes */

#define kVPNBehaviorsVODAlwaysMeansOnRetryDefault	kCFBooleanTrue

typedef struct behaviors_context {
	SCPreferencesRef prefs;
	BehaviorsUpdatedBlock cb_block;
	pid_t sbslauncher_pid;
} BehaviorsContext;

static CFRunLoopTimerRef g_timer = NULL;
static void behaviors_schedule_asset_check(BehaviorsContext *context);

static void
handle_sbslauncher_exit(pid_t pid, int status, struct rusage *rusage, void *info)
{
	BehaviorsContext *context = (BehaviorsContext *)info;

	context->sbslauncher_pid = -1;

	if (WIFSIGNALED(status)) {
		SCLog(TRUE, LOG_ERR, CFSTR("sbslauncher exited with signal %d"), WTERMSIG(status));
		/* sbslauncher was interrupted before it could complete its task. Re-schedule the check */
		behaviors_schedule_asset_check(context);
		SCPreferencesSynchronize(context->prefs);
	} else if (WIFEXITED(status)) {
		uint8_t exit_status = WEXITSTATUS(status);
		if (exit_status) {
			SCLog(TRUE, LOG_NOTICE, CFSTR("sbslauncher exited with non-zero status %d"), exit_status);
		}
	}
}

static void
behaviors_asset_check_timer_callback(CFRunLoopTimerRef timer, void *info)
{
#pragma unused(timer)
	BehaviorsContext *context = (BehaviorsContext *)info;
	
	CFRelease(g_timer);
	g_timer = NULL;

	SCLog(TRUE, LOG_NOTICE, CFSTR("Starting VPN behaviors asset check"));
	context->sbslauncher_pid = SCNCExecSBSLauncherCommandWithArguments(SBSLAUNCHER_TYPE_BEHAVIOR_ASSET_CHECK, NULL, handle_sbslauncher_exit, context, NULL);
	if (context->sbslauncher_pid <= 0) {
		SCLog(LOG_ERR, TRUE, CFSTR("behaviors_query_timer_callback: failed to run sbslauncher"));
	}
}

void
behaviors_schedule_asset_check(BehaviorsContext *context)
{
	CFNumberRef check_time;
	CFBooleanRef reset;
	CFAbsoluteTime timer_date;
	CFRunLoopTimerContext timer_ctx;
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
	CFDateFormatterRef formatter;
	CFStringRef date_str;

	behaviors_cancel_asset_check();

	reset = SCPreferencesGetValue(context->prefs, kVPNBehaviorsReset);
	if (isA_CFBoolean(reset) && CFBooleanGetValue(reset)) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("Behavior reset switch is on, will not query for behaviors asset"));
		return; /* Don't check */
	}

	check_time = SCPreferencesGetValue(context->prefs, kVPNBehaviorsNextAssetCheckTime);
	if (!isA_CFNumber(check_time)) {
		/* No date available, do the check in a few minutes */
		timer_date = now + kVPNBehaviorsAssetCheckSmallDelay;
	} else {
		if (CFNumberGetValue(check_time, kCFNumberDoubleType, &timer_date)) {
			if (timer_date <= now) {
				/* Date is in the past, do the check in a few minutes */
				timer_date = now + kVPNBehaviorsAssetCheckSmallDelay;
			}
		} else {
			/* Failed to get the date, do the check after waiting for the default interval */
			timer_date = now + kVPNBehaviorsAssetCheckErrorInterval;
		}
	}

	formatter = CFDateFormatterCreate(kCFAllocatorDefault, NULL, kCFDateFormatterMediumStyle, kCFDateFormatterLongStyle);
	date_str = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, formatter, timer_date);
	SCLog(TRUE, LOG_NOTICE, CFSTR("Scheduling next VPN behaviors asset check for %@"), date_str);
	CFRelease(date_str);
	CFRelease(formatter);

	memset(&timer_ctx, 0, sizeof(timer_ctx));
	timer_ctx.info = context;
	g_timer = CFRunLoopTimerCreate(kCFAllocatorDefault, timer_date, 0, 0, 0, behaviors_asset_check_timer_callback, &timer_ctx);
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), g_timer, kCFRunLoopDefaultMode);
}

static void
behaviors_prefs_changed(SCPreferencesRef prefs, SCPreferencesNotification notification_type, void *info)
{
#pragma unused(prefs)
	if (notification_type == kSCPreferencesNotificationCommit) {
		BehaviorsContext *context = (BehaviorsContext *)info;
		context->cb_block();
		/*
		 * We rely on cb_block (eventually) triggering a call to behaviors_modify_ondemand(), which will re-schedule
		 * the asset check.
		 */
	}
}

void
behaviors_modify_ondemand(CFMutableDictionaryRef trigger_dict, BehaviorsUpdatedBlock cb_block)
{
	CFBooleanRef always_means_on_retry;
	static BehaviorsContext *context = NULL;
	static dispatch_once_t predicate = 0;
	__block Boolean success = TRUE;

	dispatch_once(&predicate, ^{
		SCPreferencesContext prefs_ctx;

		context = (BehaviorsContext *)CFAllocatorAllocate(kCFAllocatorDefault, sizeof(*context), 0);

		context->prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("PPPController"), kVPNBehaviorsPlistFile);
		if (context->prefs == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCPreferencesCreate failed: %s"), SCErrorString(SCError()));
			success = FALSE;
			goto done;
		}

		context->sbslauncher_pid = -1;

		memset(&prefs_ctx, 0, sizeof(prefs_ctx));
		prefs_ctx.info = context;
		if (!SCPreferencesSetCallback(context->prefs, behaviors_prefs_changed, &prefs_ctx)) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCPreferencesSetCallback failed: %s"), SCErrorString(SCError()));
			success = FALSE;
			goto done;
		}

		if (!SCPreferencesScheduleWithRunLoop(context->prefs, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode)) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCPreferencesSetDispatchQueue failed: %s"), SCErrorString(SCError()));
			success = FALSE;
			goto done;
		}

done:
		if (!success) {
		   	if (context->prefs != NULL) {
				CFRelease(context->prefs);
			}
			CFAllocatorDeallocate(kCFAllocatorDefault, context);
			context = NULL;
		} else {
			context->cb_block = Block_copy(cb_block);
		}
	});

	if (!success) {
		return;
	}

#if TARGET_OS_EMBEDDED
	if (g_timer == NULL && context->sbslauncher_pid == -1) {
		behaviors_schedule_asset_check(context);
	}
#endif

	always_means_on_retry = SCPreferencesGetValue(context->prefs, kVPNBehaviorsVODAlwaysMeansOnRetry);
	if (!isA_CFBoolean(always_means_on_retry)) {
		always_means_on_retry = kVPNBehaviorsVODAlwaysMeansOnRetryDefault;
	}

	if (CFBooleanGetValue(always_means_on_retry)) {
		CFArrayRef always_domains = CFDictionaryGetValue(trigger_dict, kSCNetworkConnectionOnDemandMatchDomainsAlways);
		if (isA_CFArray(always_domains)) {
			CFMutableArrayRef new_on_retry_domains;
			CFArrayRef on_retry_domains = CFDictionaryGetValue(trigger_dict, kSCNetworkConnectionOnDemandMatchDomainsOnRetry);

			new_on_retry_domains = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, always_domains);

			if (isA_CFArray(on_retry_domains)) {
				CFIndex on_retry_count = CFArrayGetCount(on_retry_domains);
				if (on_retry_count > 0) {
					CFArrayAppendArray(new_on_retry_domains, on_retry_domains, CFRangeMake(0, on_retry_count));
				}
			}

			CFDictionaryRemoveValue(trigger_dict, kSCNetworkConnectionOnDemandMatchDomainsAlways);
			CFDictionarySetValue(trigger_dict, kSCNetworkConnectionOnDemandMatchDomainsOnRetry, new_on_retry_domains);

			CFRelease(new_on_retry_domains);
		}
	}

	SCPreferencesSynchronize(context->prefs);
}

void
behaviors_cancel_asset_check(void)
{
	if (g_timer != NULL) {
		CFRunLoopTimerInvalidate(g_timer);
		CFRelease(g_timer);
		g_timer = NULL;
	}
}

