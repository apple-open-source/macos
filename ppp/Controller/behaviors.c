/*
 * Copyright (c) 2012 Apple Inc.
 * All rights reserved.
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

#include "scnc_main.h"
#include "scnc_utils.h"
#include "behaviors.h"

#define kVPNBehaviorsPlistFile						CFSTR("com.apple.pppcontroller-vpn-behaviors.plist")
#define kVPNBehaviorsVODAlwaysMeansOnRetry			CFSTR("VODAlwaysMeansOnRetry")

#define kVPNBehaviorsVODAlwaysMeansOnRetryDefault	kCFBooleanTrue

typedef struct behaviors_context {
	SCPreferencesRef prefs;
	BehaviorsUpdatedBlock cb_block;
} BehaviorsContext;

static void
behaviors_prefs_changed(SCPreferencesRef prefs, SCPreferencesNotification notification_type, void *info)
{
#pragma unused(prefs)
	if (notification_type == kSCPreferencesNotificationCommit) {
		BehaviorsContext *context = (BehaviorsContext *)info;
		context->cb_block();
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
