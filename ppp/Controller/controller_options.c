/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCDPlugin.h>

#include "scnc_main.h"
#include "controller_options.h"

extern TAILQ_HEAD(, service) 	service_head;

typedef struct controlleroptions_context {
	SCPreferencesRef prefs;
} ControlerOptionsContext;

// default time to suspend VOD after authentication cancellation is 10 seconds.
// As long as the timer is valid, the pause state will be "UNTIL_REBOOT"
// After the timer expires, the pause state will go into "UNTIL_NETWORK_CHANGE"
#define kOnDemandPauseIntervalOnAuthCancelDefault		10

// default for ondemand-pause-until-netchange is to include wakeup event check.
#define kOnDemandPauseUntilNetChangeCheckWakeupDefault		TRUE

// default for ondemand-pause-until-netchange is to include network signature change check.
#define kOnDemandPauseUntilNetChangeCheckSignatureDefault	TRUE

// default for automaticaly pausing ondemand upon SCNetworkConnectionStop() is ON.
#define kOnDemandAutoPauseUponDisconnectDefault			TRUE

// default for applying Disconnect OnDemandRules to VOD-disbled services is OFF.
#define kUseVODDisconnectRulesWhenVODDisabledDefault		FALSE

/* If onDemandPauseIntervalOnAuthCancel < 0, ondemand pause on authen cancel is OFF.
 * If it's 0, pause is on and indefinite.
 * If it's > 0, pauase is on for the duration in seconds.
 */
static int32_t onDemandPauseIntervalOnAuthCancel = kOnDemandPauseIntervalOnAuthCancelDefault;

static Boolean onDemandPauseUntilNetChangeCheckWakeup = kOnDemandPauseUntilNetChangeCheckWakeupDefault;

static Boolean onDemandPauseUntilNetChangeCheckSignature = kOnDemandPauseUntilNetChangeCheckSignatureDefault;

static Boolean onDemandAutoPauseUponDisconnect = kOnDemandAutoPauseUponDisconnectDefault;

static Boolean useVODDisconnectRulesWhenVODDisabled = kUseVODDisconnectRulesWhenVODDisabledDefault;

static CFArrayRef onDemandBlacklistedProcesses = NULL;

static void controler_options_update_services(void);

#define kVPNControllerOptionsPlistFile				CFSTR("com.apple.pppcontroller-options.plist")
#define kVPNControllerOptionDebug				CFSTR("Debug")
#define kVPNControllerOptionVODPauseIntervalOnAuthCancel	CFSTR("OnDemandPauseIntervalOnAuthCancel")
#define kVPNControllerOptionPauseUntilNetChangeCheckWakeup	CFSTR("PauseUntilNetChangeCheckWakeup")
#define kVPNControllerOptionPauseUntilNetChangeCheckNetSignature	CFSTR("PauseUntilNetChangeCheckNetSignature")
#define kVPNControllerOptionOnDemandAutoPauseUponDisconnect	CFSTR("OnDemandAutoPauseUponDisconnect")
#define kVPNControllerOptionOnDemandBlacklistedProcesses	CFSTR("OnDemandBlacklistedProcesses")
#define kVPNControllerOptionUseVODDisconnectRulesWhenVODDisabled	CFSTR("UseVODDisconnectRulesWhenVODDisabled")

int32_t controller_options_get_onDemandPauseIntervalOnAuthCancel(void)
{
	return onDemandPauseIntervalOnAuthCancel;
}

Boolean controller_options_is_onDemandPauseUntilNetChangeCheckWakeup(void)
{
	return onDemandPauseUntilNetChangeCheckWakeup;
}

Boolean controller_options_is_onDemandPauseUntilNetChangeCheckSignature(void)
{
	return onDemandPauseUntilNetChangeCheckSignature;
}

Boolean controller_options_is_onDemandAutoPauseUponDisconnect(void)
{
	return onDemandAutoPauseUponDisconnect;
}

Boolean controller_options_is_useVODDisconnectRulesWhenVODDisabled(void)
{
	return useVODDisconnectRulesWhenVODDisabled;
}

CFArrayRef controller_options_get_onDemandBlacklistedProcesses(void)
{
	return onDemandBlacklistedProcesses;
}

static void
controller_options_prefs_process(SCPreferencesRef prefs)
{
	CFBooleanRef booleanRef;
	CFNumberRef onDemandPauseIntervalRef;
	Boolean controllerDebug = FALSE;
	CFArrayRef blacklistedProcesses = NULL;
	Boolean old_flag;

	SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option processing"));
    
	/* set PPPController debug flag, default is OFF */
	controllerDebug = FALSE;
	booleanRef = SCPreferencesGetValue(prefs, kVPNControllerOptionDebug);
	if (isA_CFBoolean(booleanRef)) {
		controllerDebug = CFBooleanGetValue(booleanRef);
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: Debug %d\n"), controllerDebug);
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: Debug option %s\n"), (booleanRef ? "value invalid" : "not present, use default"));
	}
	gSCNCVerbose = _sc_verbose | controllerDebug;
	gSCNCDebug = controllerDebug;

	/* set VPN on-demand pause interval upon authen cancellation, default is kOnDemandPauseIntervalOnAuthCancelDefault */
	onDemandPauseIntervalOnAuthCancel = kOnDemandPauseIntervalOnAuthCancelDefault;
	onDemandPauseIntervalRef = SCPreferencesGetValue(prefs, kVPNControllerOptionVODPauseIntervalOnAuthCancel);
	if (isA_CFNumber(onDemandPauseIntervalRef)) {
		CFNumberGetValue(onDemandPauseIntervalRef, kCFNumberSInt32Type, &onDemandPauseIntervalOnAuthCancel);
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: OnDemandPauseIntervalOnAuthCancel interval %d\n"), onDemandPauseIntervalOnAuthCancel);
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: OnDemandPauseIntervalOnAuthCancel option %s\n"), (onDemandPauseIntervalRef ? "value invalid" : "not present, use default"));
	}

	/* set whether VPN on-demand paused-until-network-change should include power wakeup event, default is kOnDemandPauseUntilNetChangeCheckWakeupDefault. */
	onDemandPauseUntilNetChangeCheckWakeup = kOnDemandPauseUntilNetChangeCheckWakeupDefault;
	booleanRef = SCPreferencesGetValue(prefs, kVPNControllerOptionPauseUntilNetChangeCheckWakeup);
	if (isA_CFBoolean(booleanRef)) {
		onDemandPauseUntilNetChangeCheckWakeup = CFBooleanGetValue(booleanRef);
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: PauseUntilNetChangeCheckWakeup %d\n"), onDemandPauseUntilNetChangeCheckWakeup);
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: PauseUntilNetChangeCheckWakeup option %s\n"), (booleanRef ? "value invalid" : "not present, use default"));
	}

	/* set whether VPN on-demand paused-until-network-change should check network signature change, default is kOnDemandPauseUntilNetChangeCheckSignatureDefault. */
	onDemandPauseUntilNetChangeCheckSignature = kOnDemandPauseUntilNetChangeCheckSignatureDefault;
	booleanRef = SCPreferencesGetValue(prefs, kVPNControllerOptionPauseUntilNetChangeCheckNetSignature);
	if (isA_CFBoolean(booleanRef)) {
		onDemandPauseUntilNetChangeCheckSignature = CFBooleanGetValue(booleanRef);
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: PauseUntilNetChangeCheckNetSignature %d\n"), onDemandPauseUntilNetChangeCheckSignature);
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: PauseUntilNetChangeCheckNetSignature option %s\n"), (booleanRef ? "value invalid" : "not present, use default"));
	}

	/* set whether VPN on-demand paused-until-network-change should automatically be turned on/off upon disconnect/connect, default is kOnDemandAutoPauseUponDisconnectDefault. */
	onDemandAutoPauseUponDisconnect = kOnDemandAutoPauseUponDisconnectDefault;
	booleanRef = SCPreferencesGetValue(prefs, kVPNControllerOptionOnDemandAutoPauseUponDisconnect);
	if (isA_CFBoolean(booleanRef)) {
		onDemandAutoPauseUponDisconnect = CFBooleanGetValue(booleanRef);
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: OnDemandAutoPauseUponDisconnect %d\n"), onDemandAutoPauseUponDisconnect);
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: OnDemandAutoPauseUponDisconnect option %s\n"), (booleanRef ? "value invalid" : "not present, use default"));
	}
	
    /* set list of process names that are not allowed to trigger VPN on-demand */
	if (onDemandBlacklistedProcesses) {
		CFRelease(onDemandBlacklistedProcesses);
	}
	onDemandBlacklistedProcesses = NULL;
	blacklistedProcesses = SCPreferencesGetValue(prefs, kVPNControllerOptionOnDemandBlacklistedProcesses);
	if (isA_CFArray(blacklistedProcesses)) {
		Boolean success = TRUE;
		CFIndex i;
		CFIndex numApps = CFArrayGetCount(blacklistedProcesses);
		for (i = 0; i < numApps; i++) {
			CFStringRef app = CFArrayGetValueAtIndex(blacklistedProcesses, i);
			if (!isA_CFString(app)) {
				success = FALSE;
				break;
			}
		}
		
		if (success) {
			onDemandBlacklistedProcesses = CFArrayCreateCopy(kCFAllocatorDefault, blacklistedProcesses);
		}
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: OnDemandBlacklistedProcesses option %s\n"), (blacklistedProcesses ? "value invalid" : "not present, use default"));
	}
	
	/* set whether VPN on-demand disconnect rules should be applied to VOD-disabled services, default is kUseVODDisconnectRulesWhenVODDisabledDefault. */
	old_flag = useVODDisconnectRulesWhenVODDisabled;
	useVODDisconnectRulesWhenVODDisabled = kUseVODDisconnectRulesWhenVODDisabledDefault;
	booleanRef = SCPreferencesGetValue(prefs, kVPNControllerOptionUseVODDisconnectRulesWhenVODDisabled);
	if (isA_CFBoolean(booleanRef)) {
		useVODDisconnectRulesWhenVODDisabled = CFBooleanGetValue(booleanRef);
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: UseVODDisconnectRulesWhenVODDisabled %d\n"), useVODDisconnectRulesWhenVODDisabled);
	} else {
		SCLog(TRUE, LOG_DEBUG, CFSTR("PPPController option: UseVODDisconnectRulesWhenVODDisabled option %s\n"), (booleanRef ? "value invalid" : "not present, use default"));
	}
	if (useVODDisconnectRulesWhenVODDisabled != old_flag) {
		controler_options_update_services();
	}

}

static void
controller_options_prefs_changed(SCPreferencesRef prefs, SCPreferencesNotification notification_type, void *info)
{
	if (notification_type != kSCPreferencesNotificationCommit) {
		return;
	}

	SCLog(TRUE, LOG_DEBUG, CFSTR("controller option commmit notification: update the config"));
    
	controller_options_prefs_process(prefs);
	SCPreferencesSynchronize(prefs);	// ensure that the "next" read will be in sync with any changes
}

void
controller_options_modify_ondemand(void)
{
	static ControlerOptionsContext *context = NULL;
	static dispatch_once_t predicate = 0;
	__block Boolean success = TRUE;
	
	dispatch_once(&predicate, ^{
		SCPreferencesContext prefs_ctx;

		context = (ControlerOptionsContext *)CFAllocatorAllocate(kCFAllocatorDefault, sizeof(*context), 0);

		context->prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("PPPController"), kVPNControllerOptionsPlistFile);
		if (context->prefs == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCPreferencesCreate failed: %s"), SCErrorString(SCError()));
			success = FALSE;
			goto done;
		}

		memset(&prefs_ctx, 0, sizeof(prefs_ctx));
		prefs_ctx.info = context;
		if (!SCPreferencesSetCallback(context->prefs, controller_options_prefs_changed, &prefs_ctx)) {
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
		}
	});

	if (!success) {
		return;
	}

	controller_options_prefs_process(context->prefs);
	SCPreferencesSynchronize(context->prefs);   	// ensure that the "next" read will be in sync with any changes
}

static void
controler_options_update_services(void)
{
	struct service      *serv;
    struct service      *serv_tmp;
	
	TAILQ_FOREACH_SAFE(serv, &service_head, next, serv_tmp) {
		if (!(serv->flags & FLAG_SETUP_ONDEMAND)) {
			if (useVODDisconnectRulesWhenVODDisabled) {
				if (CFDictionaryGetValue(serv->systemprefs, kSCPropNetVPNOnDemandRules))
					serv->flags |= FLAG_SETUP_NETWORKDETECTION;
			} else
				serv->flags &= ~FLAG_SETUP_NETWORKDETECTION;
		}
	}
}

