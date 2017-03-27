//
//  CircleJoinRequested.m
//  CircleJoinRequested
//
//  Created by J Osborne on 3/5/13.
//  Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
//
#import <Accounts/Accounts.h>
#import <Accounts/ACAccountStore_Private.h>
#import <Accounts/ACAccountType_Private.h>
#import <AggregateDictionary/ADClient.h>
#import <AppSupport/AppSupportUtils.h>
#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#import <CloudServices/SecureBackup.h>
#import <CoreFoundation/CFUserNotification.h>
#import <Foundation/Foundation.h>
#import <ManagedConfiguration/MCProfileConnection.h>
#import <ManagedConfiguration/MCFeatures.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <MobileCoreServices/LSApplicationWorkspace.h>
#import <MobileGestalt.h>
#import <ProtectedCloudStorage/CloudIdentity.h>
#import <Security/SecFrameworkStrings.h>
#import <SpringBoardServices/SBSCFUserNotificationKeys.h>
#include <dispatch/dispatch.h>
#include "SecureObjectSync/SOSCloudCircle.h"
#include "SecureObjectSync/SOSCloudCircleInternal.h"
#include "SecureObjectSync/SOSPeerInfo.h"
#include "SecureObjectSync/SOSInternal.h"
#include <notify.h>
#include <sysexits.h>
#import "Applicant.h"
#import "NSArray+map.h"
#import "PersistentState.h"
#include <xpc/private.h>
#include <sys/time.h>
#import "NSDate+TimeIntervalDescription.h"
#include <xpc/activity.h>
#include <xpc/private.h>
#import "os/activity.h"
#import <syslog.h>
#include "utilities/SecCFRelease.h"
#include "utilities/debugging.h"
#include "utilities/SecAKSWrappers.h"
#include "utilities/SecCFWrappers.h"

#import "CoreCDP/CDPFollowUpController.h"
#import "CoreCDP/CDPFollowUpContext.h"

// As long as we are logging the failure use exit code of zero to make launchd happy
#define EXIT_LOGGED_FAILURE(code)  xpc_transaction_end();  exit(0)

const char *kLaunchLaterXPCName = "com.apple.security.CircleJoinRequestedTick";
CFRunLoopSourceRef currentAlertSource = NULL;
CFUserNotificationRef currentAlert = NULL;
bool currentAlertIsForApplicants = true;
bool currentAlertIsForKickOut = false;
NSMutableDictionary *applicants = nil;
volatile NSString *debugState = @"main?";
dispatch_block_t doOnceInMainBlockChain = NULL;
bool _isLocked = true;
bool processApplicantsAfterUnlock = false;
bool _unlockedSinceBoot = false;

NSString *castleKeychainUrl = @"prefs:root=CASTLE&path=Keychain/ADVANCED";
NSString *rejoinICDPUrl     = @"prefs:root=CASTLE&aaaction=CDP&command=rejoin";

BOOL processRequests(CFErrorRef *error);


static void keybagDidLock()
{
    secnotice("cjr", "keybagDidLock");
}

static void keybagDidUnlock()
{
    secnotice("cjr", "keybagDidUnlock");
    
    CFErrorRef error = NULL;
    
    if(processApplicantsAfterUnlock){
        processRequests(&error);
        processApplicantsAfterUnlock = false;
    }
    
}

static bool updateIsLocked ()
{
    CFErrorRef aksError = NULL;
    if (!SecAKSGetIsLocked(&_isLocked, &aksError)) {
        _isLocked = YES;
        secerror("Got error querying lock state: %@", aksError);
        CFReleaseSafe(aksError);
        return NO;
    }
    if (!_isLocked)
        _unlockedSinceBoot = YES;
    return YES;
}

static void keybagStateChange ()
{
    secerror("osactivity initiated");
    os_activity_initiate("keybagStateChanged", OS_ACTIVITY_FLAG_DEFAULT, ^{
        BOOL wasLocked = _isLocked;
        if ( updateIsLocked()) {
            if (wasLocked == _isLocked)
                secerror("still %s ignoring", _isLocked ? "locked" : "unlocked");
            else if (_isLocked)
                keybagDidLock();
            else
                keybagDidUnlock();
        }
    });
}

static void doOnceInMain(dispatch_block_t block)
{
    if (doOnceInMainBlockChain) {
        doOnceInMainBlockChain = ^{
            doOnceInMainBlockChain();
            block();
        };
    } else {
        doOnceInMainBlockChain = block;
    }
}


static NSString *appleIDAccountName()
{
    ACAccountStore *accountStore   = [[ACAccountStore alloc] init];
    ACAccount *primaryAppleAccount = [accountStore aa_primaryAppleAccount];
    return primaryAppleAccount.username;
}


static CFOptionFlags flagsForAsk(Applicant *applicant)
{
	return kCFUserNotificationPlainAlertLevel | CFUserNotificationSecureTextField(0);
}


// NOTE: gives precedence to OnScreen
static Applicant *firstApplicantWaitingOrOnScreen()
{
    Applicant *waiting = nil;
    for (Applicant *applicant in [applicants objectEnumerator]) {
        if (applicant.applicantUIState == ApplicantOnScreen) {
            return applicant;
        } else if (applicant.applicantUIState == ApplicantWaiting) {
            waiting = applicant;
        }
    }
    
    return waiting;
}


static NSMutableArray *applicantsInState(ApplicantUIState state)
{
    NSMutableArray *results = [NSMutableArray new];
    for (Applicant *applicant in [applicants objectEnumerator]) {
        if (applicant.applicantUIState == state) {
            [results addObject:applicant];
        }
    }
    
    return results;
}


BOOL processRequests(CFErrorRef *error) {
	NSMutableArray *toAccept = [[applicantsInState(ApplicantAccepted) mapWithBlock:^id(id obj) {return (id)[obj rawPeerInfo];}] mutableCopy];
	NSMutableArray *toReject = [[applicantsInState(ApplicantRejected) mapWithBlock:^id(id obj) {return (id)[obj rawPeerInfo];}] mutableCopy];
	bool			ok = true;

	if ([toAccept count]) {
		secnotice("cjr", "Process accept: %@", toAccept);
		ok = ok && SOSCCAcceptApplicants((__bridge CFArrayRef) toAccept, error);
		if (ok) {
			secnotice("cjr", "kSOSCCHoldLockForInitialSync");
			notify_post(kSOSCCHoldLockForInitialSync);
		}
	}

	if ([toReject count]) {
		secnotice("cjr", "Process reject: %@", toReject);
		ok = ok && SOSCCRejectApplicants((__bridge CFArrayRef) toReject, error);
	}

	return ok;
}


static void cancelCurrentAlert(bool stopRunLoop) {
	if (currentAlertSource) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), currentAlertSource, kCFRunLoopDefaultMode);
		CFReleaseNull(currentAlertSource);
	}
	if (currentAlert) {
		CFUserNotificationCancel(currentAlert);
		CFReleaseNull(currentAlert);
	}
	if (stopRunLoop) {
		CFRunLoopStop(CFRunLoopGetCurrent());
	}
	currentAlertIsForKickOut = currentAlertIsForApplicants = false;
}


static void askAboutAll(bool passwordFailure);


static void applicantChoice(CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
	ApplicantUIState choice;

	if (kCFUserNotificationAlternateResponse == responseFlags) {
		choice = ApplicantRejected;
	} else if (kCFUserNotificationDefaultResponse == responseFlags) {
		choice = ApplicantAccepted;
	} else {
		secnotice("cjr", "Unexpected response %lu", responseFlags);
		choice = ApplicantRejected;
	}

	BOOL		processed = NO;
	CFErrorRef	error     = NULL;
	NSArray		*onScreen = applicantsInState(ApplicantOnScreen);

	[onScreen enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
		Applicant* applicant = (Applicant *) obj;
		applicant.applicantUIState = choice;
	}];

	if (choice == ApplicantRejected) {
		// If this device has ever set up the public key this should work without the password...
		processed = processRequests(&error);
		if (processed) {
			secnotice("cjr", "Didn't need password to process %@", onScreen);
			cancelCurrentAlert(true);
			return;
		} else {
			// ...however if the public key gets lost we should "just" fall through to the validate
			// password path.
			secnotice("cjr", "Couldn't process reject without password (e=%@) for %@ (will try with password next)", error, onScreen);

            if(CFErrorIsMalfunctioningKeybagError(error)){
                secnotice("cjr", "system is locked, dismiss the notification");
                processApplicantsAfterUnlock = true;
                return;
            }
		}
		CFReleaseNull(error);
	}

	NSString *password = (__bridge NSString *) CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, 0);
	if (!password) {
		secnotice("cjr", "No password given, retry");
		askAboutAll(true);
		return;
	}
	const char  *passwordUTF8  = [password UTF8String];
	NSData		*passwordBytes = [NSData dataWithBytes:passwordUTF8 length:strlen(passwordUTF8)];

	// Sometimes securityd crashes between SOSCCRegisterUserCredentials and processRequests
	// (which results in a process error -- I think this is 13355140), as a workaround we retry
	// failure a few times before we give up.
	for (int try = 0; try < 5 && !processed; try++) {
		if (!SOSCCTryUserCredentials(CFSTR(""), (__bridge CFDataRef) passwordBytes, &error)) {
			secnotice("cjr", "Try user credentials failed %@", error);
			if ((error == NULL) ||
				(CFEqual(kSOSErrorDomain, CFErrorGetDomain(error)) && kSOSErrorWrongPassword == CFErrorGetCode(error))) {
				secnotice("cjr", "Calling askAboutAll again...");
				[onScreen enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
					Applicant *applicant = (Applicant*) obj;
					applicant.applicantUIState = ApplicantWaiting;
				}];
				askAboutAll(true);
				CFReleaseNull(error);
				return;
			}
			EXIT_LOGGED_FAILURE(EX_DATAERR);
		}

		processed = processRequests(&error);
		if (!processed) {
			secnotice("cjr", "Can't processRequests: %@ for %@", error, onScreen);
		}
		CFReleaseNull(error);
	}

	if (processed && firstApplicantWaitingOrOnScreen()) {
		cancelCurrentAlert(false);
		askAboutAll(false);
	} else {
		cancelCurrentAlert(true);
	}
}


static void passwordFailurePrompt()
{
	NSString	 *pwIncorrect = [NSString stringWithFormat:(NSString *)CFBridgingRelease(SecCopyCKString(SEC_CK_PASSWORD_INCORRECT)), appleIDAccountName()];
	NSString 	 *tryAgain    = CFBridgingRelease(SecCopyCKString(SEC_CK_TRY_AGAIN));
	NSDictionary *noteAttributes = @{
		(id) kCFUserNotificationAlertHeaderKey			   : pwIncorrect,
		(id) kCFUserNotificationDefaultButtonTitleKey	   : tryAgain,
		(id) kCFUserNotificationAlertTopMostKey      	   : @YES,			// get us onto the lock screen
		(__bridge id) SBUserNotificationDontDismissOnUnlock: @YES,
		(__bridge id) SBUserNotificationDismissOnLock	   : @NO,
	};
	CFOptionFlags		  flags = kCFUserNotificationPlainAlertLevel;
	SInt32		  		  err;
	CFUserNotificationRef note = CFUserNotificationCreate(NULL, 0.0, flags, &err, (__bridge CFDictionaryRef) noteAttributes);

	if (note) {
		CFUserNotificationReceiveResponse(note, 0.0, &flags);
		CFRelease(note);
	}
}


static NSString *getLocalizedApprovalBody(NSString *deviceType) {
	CFStringRef applicationReminder = NULL;

	if ([deviceType isEqualToString:@"iPhone"])
		applicationReminder = SecCopyCKString(SEC_CK_APPROVAL_BODY_IOS_IPHONE);
	else if ([deviceType isEqualToString:@"iPod"])
		applicationReminder = SecCopyCKString(SEC_CK_APPROVAL_BODY_IOS_IPOD);
	else if ([deviceType isEqualToString:@"iPad"])
		applicationReminder = SecCopyCKString(SEC_CK_APPROVAL_BODY_IOS_IPAD);
	else if ([deviceType isEqualToString:@"Mac"])
		applicationReminder = SecCopyCKString(SEC_CK_APPROVAL_BODY_IOS_MAC);
	else
		applicationReminder = SecCopyCKString(SEC_CK_APPROVAL_BODY_IOS_GENERIC);

	return (__bridge_transfer NSString *) applicationReminder;
}


static NSDictionary *createNote(Applicant *applicantToAskAbout)
{
	if(!applicantToAskAbout || !applicantToAskAbout.name || !applicantToAskAbout.deviceType)
		return NULL;

	NSString *header = [NSString stringWithFormat: (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_APPROVAL_TITLE_IOS), applicantToAskAbout.name];
	NSString *body   = [NSString stringWithFormat: getLocalizedApprovalBody(applicantToAskAbout.deviceType), appleIDAccountName()];

	return @{
		(id) kCFUserNotificationAlertHeaderKey		   : header,
		(id) kCFUserNotificationAlertMessageKey		   : body,
		(id) kCFUserNotificationDefaultButtonTitleKey  : (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_ALLOW),
		(id) kCFUserNotificationAlternateButtonTitleKey: (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_DONT_ALLOW),
		(id) kCFUserNotificationTextFieldTitlesKey	   : (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_ICLOUD_PASSWORD),
		(id) kCFUserNotificationAlertTopMostKey		   : @YES,				//  get us onto the lock screen
		(__bridge_transfer id) SBUserNotificationDontDismissOnUnlock: @YES,
		(__bridge_transfer id) SBUserNotificationDismissOnLock		: @NO,
    };
}


static void askAboutAll(bool passwordFailure)
{
	if ([[MCProfileConnection sharedConnection] effectiveBoolValueForSetting: MCFeatureAccountModificationAllowed] == MCRestrictedBoolExplicitNo) {
		secnotice("cjr", "Account modifications not allowed.");
		return;
	}

	if (passwordFailure) {
		passwordFailurePrompt();
	}

	if ((passwordFailure || !currentAlertIsForApplicants) && currentAlert) {
		if (!currentAlertIsForApplicants) {
			CFUserNotificationCancel(currentAlert);
		}
		// after password failure we need to remove the existing alert and supporting objects
		// because we can't reuse them.
		CFReleaseNull(currentAlert);
		CFReleaseNull(currentAlertSource);
	}
	currentAlertIsForApplicants = true;

	Applicant *applicantToAskAbout = firstApplicantWaitingOrOnScreen();
	secnotice("cjr", "Asking about: %@ (of: %@)", applicantToAskAbout, applicants);

	NSDictionary *noteAttributes = createNote(applicantToAskAbout);
	if(!noteAttributes) {
		secnotice("cjr", "NULL data for %@", applicantToAskAbout);
		cancelCurrentAlert(true);
		return;
	}

	CFOptionFlags flags = flagsForAsk(applicantToAskAbout);

	if (currentAlert) {
		SInt32 err = CFUserNotificationUpdate(currentAlert, 0, flags, (__bridge CFDictionaryRef) noteAttributes);
		if (err) {
			secnotice("cjr", "CFUserNotificationUpdate err=%d", (int)err);
			EXIT_LOGGED_FAILURE(EX_SOFTWARE);
		}
	} else {
		SInt32 err = 0;
		currentAlert = CFUserNotificationCreate(NULL, 0.0, flags, &err, (__bridge CFDictionaryRef) noteAttributes);
		if (err) {
			secnotice("cjr", "Can't make notification for %@ err=%x", applicantToAskAbout, (int)err);
			EXIT_LOGGED_FAILURE(EX_SOFTWARE);
		}

		currentAlertSource = CFUserNotificationCreateRunLoopSource(NULL, currentAlert, applicantChoice, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), currentAlertSource, kCFRunLoopDefaultMode);
	}

	applicantToAskAbout.applicantUIState = ApplicantOnScreen;
}


static void scheduleActivity(int alertInterval)
{
    xpc_object_t options = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_DELAY, alertInterval);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_GRACE_PERIOD, XPC_ACTIVITY_INTERVAL_1_MIN);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_REPEATING, false);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_ALLOW_BATTERY, true);
    xpc_dictionary_set_string(options, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_UTILITY);
    
    xpc_activity_register(kLaunchLaterXPCName, options, ^(xpc_activity_t activity) {
        secnotice("cjr", "activity handler fired");
    });
}


static void reminderChoice(CFUserNotificationRef userNotification, CFOptionFlags responseFlags) {
    if (responseFlags == kCFUserNotificationAlternateResponse || responseFlags == kCFUserNotificationDefaultResponse) {
        PersistentState *state = [PersistentState loadFromStorage];
        NSDate *nowish = [NSDate new];
        state.pendingApplicationReminder = [nowish dateByAddingTimeInterval: state.pendingApplicationReminderAlertInterval];
        scheduleActivity(state.pendingApplicationReminderAlertInterval);
        [state writeToStorage];
        if (responseFlags == kCFUserNotificationAlternateResponse) {
			// Use security code
            BOOL ok = [[LSApplicationWorkspace defaultWorkspace] openSensitiveURL:[NSURL URLWithString:castleKeychainUrl] withOptions:nil];
			secnotice("cjr", "%s iCSC: opening %@ ok=%d", __FUNCTION__, castleKeychainUrl, ok);
        }
    }

    cancelCurrentAlert(true);
}


static bool iCloudResetAvailable() {
	SecureBackup *backupd = [SecureBackup new];
	NSDictionary *backupdResults;
	NSError		 *error = [backupd getAccountInfoWithInfo:nil results:&backupdResults];
	secnotice("cjr", "SecureBackup e=%@ r=%@", error, backupdResults);
	return (error == nil && [backupdResults[kSecureBackupIsEnabledKey] isEqualToNumber:@YES]);
}


static NSString *getLocalizedApplicationReminder() {
	CFStringRef applicationReminder = NULL;
	switch (MGGetSInt32Answer(kMGQDeviceClassNumber, MGDeviceClassInvalid)) {
	case MGDeviceClassiPhone:
		applicationReminder = SecCopyCKString(SEC_CK_REMINDER_BODY_IOS_IPHONE);
		break;
	case MGDeviceClassiPod:
		applicationReminder = SecCopyCKString(SEC_CK_REMINDER_BODY_IOS_IPOD);
		break;
	case MGDeviceClassiPad:
		applicationReminder = SecCopyCKString(SEC_CK_REMINDER_BODY_IOS_IPAD);
		break;
	default:
		applicationReminder = SecCopyCKString(SEC_CK_REMINDER_BODY_IOS_GENERIC);
		break;
	}
	return (__bridge_transfer NSString *) applicationReminder;
}


static void postApplicationReminderAlert(NSDate *nowish, PersistentState *state, unsigned int alertInterval)
{
	NSString *body		= getLocalizedApplicationReminder();
	bool      has_iCSC	= iCloudResetAvailable();

	if (CPIsInternalDevice() &&
		state.defaultPendingApplicationReminderAlertInterval != state.pendingApplicationReminderAlertInterval) {
#ifdef DEBUG
		body = [body stringByAppendingFormat: @"〖debug interval %u; wait time %@〗",
					state.pendingApplicationReminderAlertInterval,
					[nowish copyDescriptionOfIntervalSince:state.applicationDate]];
#endif
    }

    NSDictionary *pendingAttributes = @{
		(id) kCFUserNotificationAlertHeaderKey		   : CFBridgingRelease(SecCopyCKString(SEC_CK_REMINDER_TITLE_IOS)),
		(id) kCFUserNotificationAlertMessageKey		   : body,
		(id) kCFUserNotificationDefaultButtonTitleKey  : CFBridgingRelease(SecCopyCKString(SEC_CK_REMINDER_BUTTON_OK)),
		(id) kCFUserNotificationAlternateButtonTitleKey: has_iCSC ? CFBridgingRelease(SecCopyCKString(SEC_CK_REMINDER_BUTTON_ICSC)) : @"",
		(id) kCFUserNotificationAlertTopMostKey				: @YES,
		(__bridge id) SBUserNotificationDontDismissOnUnlock	: @YES,
		(__bridge id) SBUserNotificationDismissOnLock		: @NO,
	};
    SInt32 err = 0;
    currentAlert = CFUserNotificationCreate(NULL, 0.0, kCFUserNotificationPlainAlertLevel, &err, (__bridge CFDictionaryRef) pendingAttributes);

	if (err) {
		secnotice("cjr", "Can't make pending notification err=%x", (int)err);
	} else {
		currentAlertIsForApplicants = false;
		currentAlertSource = CFUserNotificationCreateRunLoopSource(NULL, currentAlert, reminderChoice, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), currentAlertSource, kCFRunLoopDefaultMode);
	}
}


static void kickOutChoice(CFUserNotificationRef userNotification, CFOptionFlags responseFlags) {
	secnotice("cjr", "kOC %@ %lu", userNotification, responseFlags);
    
    //default response: continue -> settings pref pane advanced keychain sync page
	if (responseFlags == kCFUserNotificationDefaultResponse) {
		// We need to let things unwind to main for the new state to get saved
		doOnceInMain(^{
			ACAccountStore	  *store	= [ACAccountStore new];
			ACAccount		  *primary  = [store aa_primaryAppleAccount];
			NSString		  *dsid 	= [primary aa_personID];
			bool			  localICDP = false;
			if (dsid) {
				NSDictionary	  *options = @{ (__bridge id) kPCSSetupDSID : dsid, };
				PCSIdentitySetRef identity = PCSIdentitySetCreate((__bridge CFDictionaryRef) options, NULL, NULL);

				if (identity) {
					localICDP = PCSIdentitySetIsICDP(identity, NULL);
					CFRelease(identity);
				}
			}
			NSURL    		  *url		= [NSURL URLWithString: localICDP ? rejoinICDPUrl : castleKeychainUrl];
			BOOL 			  ok		= [[LSApplicationWorkspace defaultWorkspace] openSensitiveURL:url withOptions:nil];
            secnotice("cjr", "ok=%d opening %@", ok, url);
		});
	}
    //alternate response: later -> call CD
    else if (responseFlags == kCFUserNotificationAlternateResponse) {
        // We need to let things unwind to main for the new state to get saved
        doOnceInMain(^{
            CDPFollowUpController *cdpd = [[CDPFollowUpController alloc] init];
            ACAccountStore	  *store	= [ACAccountStore new];
            ACAccount		  *primary  = [store aa_primaryAppleAccount];
            NSString		  *dsid 	= [primary aa_personID];
            bool			  localICDP = false;
            if (dsid) {
                NSDictionary	  *options = @{ (__bridge id) kPCSSetupDSID : dsid, };
                PCSIdentitySetRef identity = PCSIdentitySetCreate((__bridge CFDictionaryRef) options, NULL, NULL);
                
                if (identity) {
                    localICDP = PCSIdentitySetIsICDP(identity, NULL);
                    CFRelease(identity);
                }
            }
            if(localICDP){
                NSError *localError = nil;
                CDPFollowUpContext *context = [CDPFollowUpContext contextForStateRepair];
                [cdpd postFollowUpWithContext:context error:&localError ];
                if(localError){
                    secnotice("cjr", "request to CoreCDP to follow up failed: %@", localError);
                }
                else
                    secnotice("cjr", "CoreCDP handling follow up");
            }
        });

    }
	cancelCurrentAlert(true);
}


CFStringRef const CJRAggdDepartureReasonKey  = CFSTR("com.apple.security.circlejoinrequested.departurereason");
CFStringRef const CJRAggdNumCircleDevicesKey = CFSTR("com.apple.security.circlejoinrequested.numcircledevices");

static void postKickedOutAlert(enum DepartureReason reason)
{
	NSString	*header  = nil;
	NSString	*message = nil;

	// <rdar://problem/20358568> iCDP telemetry: ☂ Statistics on circle reset and drop out events
	ADClientSetValueForScalarKey(CJRAggdDepartureReasonKey, reason);

	int64_t    num_peers = 0;
	CFArrayRef peerList  = SOSCCCopyPeerPeerInfo(NULL);
	if (peerList) {
		num_peers = CFArrayGetCount(peerList);
		if (num_peers > 99) {
			// Round down # peers to 2 significant digits
			int factor;
			for (factor = 10; num_peers >= 100*factor; factor *= 10) ;
			num_peers = (num_peers / factor) * factor;
		}
		CFRelease(peerList);
	}
	ADClientSetValueForScalarKey(CJRAggdNumCircleDevicesKey, num_peers);

	debugState = @"pKOA A";
	secnotice("cjr", "DepartureReason %d", reason);
	switch (reason) {
	case kSOSDiscoveredRetirement:
	case kSOSLostPrivateKey:
	case kSOSWithdrewMembership:
		// Was: SEC_CK_CR_BODY_WITHDREW
		// "... if you turn off a switch you have some idea why the light is off" - Murf
		return;

	case kSOSNeverAppliedToCircle:
		// We didn't get kicked out, we were never here. This should only happen if we changed iCloud accounts
		// (and we had sync on in the previous one, and never had it on in the new one). As this is explicit
		// user action alot of the "Light switch" argument (above) applies.
		return;

    case kSOSPasswordChanged:
	case kSOSNeverLeftCircle:
	case kSOSMembershipRevoked:
	case kSOSLeftUntrustedCircle:
	default:
		header  = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_PWD_REQUIRED_TITLE);
		message = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_PWD_REQUIRED_BODY_IOS);
		break;
	}

	if (CPIsInternalDevice()) {
		static const char *departureReasonStrings[] = {
			"kSOSDepartureReasonError",
			"kSOSNeverLeftCircle",
			"kSOSWithdrewMembership",
			"kSOSMembershipRevoked",
			"kSOSLeftUntrustedCircle",
			"kSOSNeverAppliedToCircle",
			"kSOSDiscoveredRetirement",
			"kSOSLostPrivateKey",
            "kSOSPasswordChanged",
			"unknown reason"
		};
		int idx = (kSOSDepartureReasonError <= reason && reason <= kSOSLostPrivateKey) ? reason : (kSOSLostPrivateKey + 1);
		NSString *reason_str = [NSString stringWithFormat:(__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CR_REASON_INTERNAL),
								departureReasonStrings[idx]];
		message = [message stringByAppendingString: reason_str];
	}

    NSDictionary *kickedAttributes = @{
		(id) kCFUserNotificationAlertHeaderKey         : header,
		(id) kCFUserNotificationAlertMessageKey        : message,
		(id) kCFUserNotificationDefaultButtonTitleKey  : (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CONTINUE),
		(id) kCFUserNotificationAlternateButtonTitleKey: (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_NOT_NOW),
		(id) kCFUserNotificationAlertTopMostKey		   : @YES,
		(__bridge id) SBUserNotificationDismissOnLock		: @NO,
		(__bridge id) SBUserNotificationDontDismissOnUnlock	: @YES,
	};
    SInt32 err = 0;

    if (currentAlertIsForKickOut) {
        debugState = @"pKOA B";
        secnotice("cjr", "Updating existing alert %@ with %@", currentAlert, kickedAttributes);
        CFUserNotificationUpdate(currentAlert, 0, kCFUserNotificationPlainAlertLevel, (__bridge CFDictionaryRef) kickedAttributes);
    } else {
        debugState = @"pKOA C";

        CFUserNotificationRef note = CFUserNotificationCreate(NULL, 0.0, kCFUserNotificationPlainAlertLevel, &err, (__bridge CFDictionaryRef) kickedAttributes);
		assert((note == NULL) == (err != 0));
		if (err) {
			secnotice("cjr", "Can't make kicked out notification err=%x", (int)err);
            CFReleaseNull(note);
		} else {
            currentAlertIsForApplicants = false;
            currentAlertIsForKickOut = true;
            
            currentAlert = note;
            secnotice("cjr", "New ko alert %@ a=%@", currentAlert, kickedAttributes);
            currentAlertSource = CFUserNotificationCreateRunLoopSource(NULL, currentAlert, kickOutChoice, 0);
            CFRunLoopAddSource(CFRunLoopGetCurrent(), currentAlertSource, kCFRunLoopDefaultMode);
            int backupStateChangeToken;
            notify_register_dispatch("com.apple.EscrowSecurityAlert.reset", &backupStateChangeToken, dispatch_get_main_queue(), ^(int token) {
                if (currentAlert == note) {
                    secnotice("cjr", "Backup state might have changed (dS=%@)", debugState);
                    postKickedOutAlert(reason);
                } else {
                    secnotice("cjr", "Backup state may have changed, but we don't care anymore (dS=%@)", debugState);
                }
            });
            
            debugState = @"pKOA D";
            CFRunLoopRun();
            debugState = @"pKOA E";
            notify_cancel(backupStateChangeToken);
        }
    }
    debugState = @"pKOA Z";
}


static bool processEvents()
{
	debugState = @"processEvents A";

	CFErrorRef			error			 = NULL;
	CFErrorRef			departError		 = NULL;
	SOSCCStatus			circleStatus	 = SOSCCThisDeviceIsInCircle(&error);
	NSDate				*nowish			 = [NSDate date];
	PersistentState 	*state     		 = [PersistentState loadFromStorage];
	enum DepartureReason departureReason = SOSCCGetLastDepartureReason(&departError);
	secnotice("cjr", "CircleStatus %d -> %d{%d} (s=%p)", state.lastCircleStatus, circleStatus, departureReason, state);

	// Pending application reminder
	NSTimeInterval timeUntilApplicationAlert = [state.pendingApplicationReminder timeIntervalSinceDate:nowish];
	secnotice("cjr", "Time until pendingApplicationReminder (%@) %f", [state.pendingApplicationReminder debugDescription], timeUntilApplicationAlert);
	if (circleStatus == kSOSCCRequestPending) {
		if (timeUntilApplicationAlert <= 0) {
			debugState = @"reminderAlert";
			postApplicationReminderAlert(nowish, state, state.pendingApplicationReminderAlertInterval);
		} else {
			scheduleActivity(ceil(timeUntilApplicationAlert));
		}
	}

    if(circleStatus == kSOSCCError && state.lastCircleStatus != kSOSCCError && (departureReason == kSOSNeverLeftCircle)) {
        secnotice("cjr", "error from SOSCCThisDeviceIsInCircle: %@", error);
        CFIndex errorCode = CFErrorGetCode(error);
        if(errorCode == kSOSErrorPublicKeyAbsent){
            secnotice("cjr", "We need the password to re-validate ourselves - it's changed on another device");
            postKickedOutAlert(kSOSPasswordChanged);
            state.lastCircleStatus = kSOSCCError;
            [state writeToStorage];
            return true;
        }
    }
    
	// No longer in circle?
	if ((state.lastCircleStatus == kSOSCCInCircle     && (circleStatus == kSOSCCNotInCircle || circleStatus == kSOSCCCircleAbsent)) ||
		(state.lastCircleStatus == kSOSCCCircleAbsent && circleStatus == kSOSCCNotInCircle && state.absentCircleWithNoReason) ||
		state.debugShowLeftReason) {
		// Used to be in the circle, now we aren't - tell the user why
		debugState = @"processEvents B";
    
        if (state.debugShowLeftReason) {
			secnotice("cjr", "debugShowLeftReason: %@", state.debugShowLeftReason);
			departureReason = [state.debugShowLeftReason intValue];
			state.debugShowLeftReason = nil;
			CFReleaseNull(departError);
			[state writeToStorage];
		}

		if (departureReason != kSOSDepartureReasonError) {
			state.absentCircleWithNoReason = (circleStatus == kSOSCCCircleAbsent && departureReason == kSOSNeverLeftCircle);
			secnotice("cjr", "Depature reason %d", departureReason);
			postKickedOutAlert(departureReason);
			secnotice("cjr", "pKOA returned (cS %d lCS %d)", circleStatus, state.lastCircleStatus);
		} else {
			secnotice("cjr", "Couldn't get last departure reason: %@", departError);
		}
       
    }

	// Circle applications: pending request(s) started / completed
	debugState = @"processEvents C";
	if (circleStatus != state.lastCircleStatus) {
		SOSCCStatus lastCircleStatus = state.lastCircleStatus;
		state.lastCircleStatus = circleStatus;
		
		if (lastCircleStatus != kSOSCCRequestPending && circleStatus == kSOSCCRequestPending) {
			secnotice("cjr", "Pending request started");
			state.applicationDate			 = nowish;
			state.pendingApplicationReminder = [nowish dateByAddingTimeInterval: state.pendingApplicationReminderAlertInterval];
			scheduleActivity(state.pendingApplicationReminderAlertInterval);
		}
		if (lastCircleStatus == kSOSCCRequestPending && circleStatus != kSOSCCRequestPending) {
			secnotice("cjr", "Pending request completed");
			state.applicationDate			 = [NSDate distantPast];
			state.pendingApplicationReminder = [NSDate distantFuture];
		}
		
		[state writeToStorage];
	}

	if (circleStatus != kSOSCCInCircle) {
		if (circleStatus == kSOSCCRequestPending && currentAlert) {
			int notifyToken = 0;
			CFUserNotificationRef postedAlert = currentAlert;
			
			debugState = @"processEvents D1";
			notify_register_dispatch(kSOSCCCircleChangedNotification, &notifyToken, dispatch_get_main_queue(), ^(int token) {
				if (postedAlert != currentAlert) {
					secnotice("cjr", "-- CC after original alert gone (currentAlertIsForApplicants %d, pA %p, cA %p -- %@)",
						  currentAlertIsForApplicants, postedAlert, currentAlert, currentAlert);
					notify_cancel(token);
				} else {
                    CFErrorRef localError = NULL;
					SOSCCStatus newCircleStatus = SOSCCThisDeviceIsInCircle(&localError);
					if (newCircleStatus != kSOSCCRequestPending) {
						if (newCircleStatus == kSOSCCError)
							secnotice("cjr", "No longer pending (nCS=%d, alert=%@) error: %@", newCircleStatus, currentAlert, localError);
						else
							secnotice("cjr", "No longer pending (nCS=%d, alert=%@)", newCircleStatus, currentAlert);
						cancelCurrentAlert(true);
					} else {
						secnotice("cjr", "Still pending...");
					}
					CFReleaseNull(localError);
				}
			});
			debugState = @"processEvents D2";
			secnotice("cjr", "NOTE: currentAlertIsForApplicants %d, token %d", currentAlertIsForApplicants, notifyToken);
			CFRunLoopRun();
			return true;
		}
        debugState = @"processEvents D4";
		secnotice("cjr", "SOSCCThisDeviceIsInCircle status %d, not checking applicants", circleStatus);
		return false;
	}


	// Applicants
	debugState = @"processEvents E";
	applicants = [NSMutableDictionary new];
	for (id applicantInfo in (__bridge_transfer NSArray *) SOSCCCopyApplicantPeerInfo(&error)) {
		Applicant *applicant = [[Applicant alloc] initWithPeerInfo:(__bridge SOSPeerInfoRef) applicantInfo];
		applicants[applicant.idString] = applicant;
	}

	// Log error from SOSCCCopyApplicantPeerInfo() above?
	CFReleaseNull(error);

	int notify_token = -42;
	debugState = @"processEvents F";
	int notify_register_status = notify_register_dispatch(kSOSCCCircleChangedNotification, &notify_token, dispatch_get_main_queue(), ^(int token) {
		secnotice("cjr", "Notified: %s", kSOSCCCircleChangedNotification);
		CFErrorRef circleStatusError = NULL;
		
		bool needsUpdate = false;
		CFErrorRef copyPeerError = NULL;
		NSMutableSet *newIds = [NSMutableSet new];
		for (id applicantInfo in (__bridge_transfer NSArray *) SOSCCCopyApplicantPeerInfo(&copyPeerError)) {
			Applicant *newApplicant = [[Applicant alloc] initWithPeerInfo:(__bridge SOSPeerInfoRef) applicantInfo];
			[newIds addObject:newApplicant.idString];
			Applicant *existingApplicant = applicants[newApplicant.idString];
			if (existingApplicant) {
				switch (existingApplicant.applicantUIState) {
				case ApplicantWaiting:
					applicants[newApplicant.idString] = newApplicant;
					break;
					
				case ApplicantOnScreen:
					newApplicant.applicantUIState = ApplicantOnScreen;
					applicants[newApplicant.idString] = newApplicant;
					break;
					
				default:
					secnotice("cjr", "Update to %@ >> %@ with pending order, should work out ok though", existingApplicant, newApplicant);
					break;
				}
			} else {
				needsUpdate = true;
				applicants[newApplicant.idString] = newApplicant;
			}
		}
		if (copyPeerError) {
			secnotice("cjr", "Could not update peer info array: %@", copyPeerError);
			CFRelease(copyPeerError);
			return;
		}
		
		NSMutableArray *idsToRemoveFromApplicants = [NSMutableArray new];
		for (NSString *exisitngId in [applicants keyEnumerator]) {
			if (![newIds containsObject:exisitngId]) {
				[idsToRemoveFromApplicants addObject:exisitngId];
				needsUpdate = true;
			}
		}
		[applicants removeObjectsForKeys:idsToRemoveFromApplicants];
		
		if (newIds.count == 0) {
			secnotice("cjr", "All applicants were handled elsewhere");
			cancelCurrentAlert(true);
		}
		SOSCCStatus currentCircleStatus = SOSCCThisDeviceIsInCircle(&circleStatusError);
		if (kSOSCCInCircle != currentCircleStatus) {
			secnotice("cjr", "Left circle (%d), not handling remaining %lu applicants", currentCircleStatus, (unsigned long)newIds.count);
			cancelCurrentAlert(true);
		}
		if (needsUpdate) {
			askAboutAll(false);
		} else {
			secnotice("cjr", "needsUpdate false, not updating alert");
		}
		// Log circleStatusError?
		CFReleaseNull(circleStatusError);
	});
	secnotice("cjr", "ACC token %d, status %d", notify_token, notify_register_status);
	debugState = @"processEvents F2";

	if (applicants.count == 0) {
		secnotice("cjr", "No applicants");
	} else {
		debugState = @"processEvents F3";
		askAboutAll(false);
		debugState = @"processEvents F4";
		if (currentAlert) {
			debugState = @"processEvents F5";
			CFRunLoopRun();
		}
	}

	debugState = @"processEvents F6";
	notify_cancel(notify_token);
	debugState = @"processEvents DONE";

	return false;
}


int main (int argc, const char * argv[]) {
    
	xpc_transaction_begin();
    
	@autoreleasepool {

        // NOTE: DISPATCH_QUEUE_PRIORITY_LOW will not actually manage to drain events in a lot of cases (like circleStatus != kSOSCCInCircle)
        xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(xpc_object_t object) {
            char *event_description = xpc_copy_description(object);
            const char *notificationName = xpc_dictionary_get_string(object, "Notification");

            if (notificationName && strcmp(notificationName, kUserKeybagStateChangeNotification)==0) {
                secnotice("cjr", "keybag changed!");
                keybagStateChange();
            }

            secnotice("cjr", "notifyd event: %s\nAlert (%p) %s %s\ndebugState: %@", event_description, currentAlert,
                      currentAlertIsForApplicants ? "for applicants" : "!applicants",
                      currentAlertIsForKickOut ? "KO" : "!KO", debugState);
            free(event_description);
        });
        
		xpc_activity_register(kLaunchLaterXPCName, XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        });
        
		int falseInARow = 0;
		while (falseInARow < 2) {
			if (processEvents()) {
                secnotice("cjr", "Processed events!!!");
				falseInARow = 0;
			} else {
				falseInARow++;
			}
			cancelCurrentAlert(false);
			if (doOnceInMainBlockChain) {
				doOnceInMainBlockChain();
				doOnceInMainBlockChain = NULL;
			}
		}
	}
    
	secnotice("cjr", "Done");
	xpc_transaction_end();
	return(0);
}
