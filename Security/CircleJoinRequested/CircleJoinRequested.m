/*
 * Copyright (c) 2013-2017 Apple Inc. All Rights Reserved.
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

#import <Accounts/Accounts.h>
#import <Accounts/ACAccountStore_Private.h>
#import <Accounts/ACAccountType_Private.h>
#import <AppSupport/AppSupportUtils.h>
#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#import <CloudServices/SecureBackup.h>
#import <CoreFoundation/CFUserNotification.h>
#import <Foundation/Foundation.h>
#import <ManagedConfiguration/ManagedConfiguration.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <MobileCoreServices/LSApplicationWorkspace.h>
#import <MobileGestalt.h>
#import <os/transaction_private.h>
#import <ProtectedCloudStorage/CloudIdentity.h>
#import <Security/SecFrameworkStrings.h>
#import <SpringBoardServices/SBSCFUserNotificationKeys.h>
#include <dispatch/dispatch.h>
#include "keychain/SecureObjectSync/SOSCloudCircle.h"
#include "keychain/SecureObjectSync/SOSCloudCircleInternal.h"
#include "keychain/SecureObjectSync/SOSPeerInfo.h"
#include "keychain/SecureObjectSync/SOSInternal.h"
#include <notify.h>
#include <sysexits.h>
#import "Applicant.h"
#import "NSArray+map.h"
#import "PersistentState.h"
#include <xpc/private.h>
#include <sys/time.h>
#include <xpc/activity.h>
#include <xpc/private.h>
#import "os/activity.h"
#import <syslog.h>
#include "utilities/SecCFRelease.h"
#include "utilities/debugging.h"
#include "utilities/SecAKSWrappers.h"
#include "utilities/SecCFWrappers.h"
#include <utilities/SecXPCError.h>
#import <os/variant_private.h>

#import "CoreCDP/CDPFollowUpController.h"
#import "CoreCDP/CDPFollowUpContext.h"
#import <CoreCDP/CDPAccount.h>

// As long as we are logging the failure use exit code of zero to make launchd happy
#define EXIT_LOGGED_FAILURE(code) exit(0)

const char *kLaunchLaterXPCName = "com.apple.security.CircleJoinRequestedTick";
CFRunLoopSourceRef currentAlertSource = NULL;
CFUserNotificationRef currentAlert = NULL;
bool currentAlertIsForApplicants = true;
bool currentAlertIsForKickOut = false;
NSMutableDictionary *applicants = nil;
volatile NSString *debugState = @"main?";
dispatch_block_t doOnceInMainBlockChain = NULL;
bool _hasPostedFollowupAndStillInError = false;
bool _isAccountICDP = false;
bool _executeProcessEventsOnce = false;

NSString *castleKeychainUrl = @"prefs:root=APPLE_ACCOUNT&path=ICLOUD_SERVICE/com.apple.Dataclass.KeychainSync/ADVANCED";
NSString *rejoinICDPUrl     = @"prefs:root=APPLE_ACCOUNT&aaaction=CDP&command=rejoin";

BOOL processRequests(CFErrorRef *error);

static BOOL isErrorFromXPC(CFErrorRef error)
{
    // Error due to XPC failure does not provide information about the circle.
    if (error && (CFEqual(sSecXPCErrorDomain, CFErrorGetDomain(error)))) {
        secnotice("cjr", "XPC error while checking circle status: \"%@\", not processing events", error);
        return YES;
    }
    return NO;
}

static void PSKeychainSyncIsUsingICDP(void)
{
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
    ACAccountStore *accountStore = [ACAccountStore defaultStore];
    ACAccount *account = [accountStore aa_primaryAppleAccount];
    NSString *dsid = account.accountProperties[@"personID"];
    BOOL isICDPEnabled = NO;
    if (dsid) {
        isICDPEnabled = [CDPAccount isICDPEnabledForDSID:dsid];
        NSLog(@"iCDP: PSKeychainSyncIsUsingICDP returning %{bool}d", isICDPEnabled);
    } else {
        NSLog(@"iCDP: no primary account");
    }
    
    _isAccountICDP = isICDPEnabled;
    secnotice("cjr", "account is icdp: %{bool}d", _isAccountICDP);
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


static NSString *appleIDAccountName(void)
{
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return nil;
    }
    
    ACAccountStore *accountStore   = [ACAccountStore defaultStore];
    ACAccount *primaryAppleAccount = [accountStore aa_primaryAppleAccount];
    return primaryAppleAccount.username;
}


static CFOptionFlags flagsForAsk(Applicant *applicant)
{
	return kCFUserNotificationPlainAlertLevel | CFUserNotificationSecureTextField(0);
}


// NOTE: gives precedence to OnScreen
static Applicant *firstApplicantWaitingOrOnScreen(void)
{
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return nil;
    }
    
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
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return [[NSMutableArray alloc] init];
    }
    
    NSMutableArray *results = [NSMutableArray new];
    for (Applicant *applicant in [applicants objectEnumerator]) {
        if (applicant.applicantUIState == state) {
            [results addObject:applicant];
        }
    }
    
    return results;
}


BOOL processRequests(CFErrorRef *error) {
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return NO;
    }
    
	NSMutableArray *toAccept = [[applicantsInState(ApplicantAccepted) mapWithBlock:^id(id obj) {return (id)[obj rawPeerInfo];}] mutableCopy];
	NSMutableArray *toReject = [[applicantsInState(ApplicantRejected) mapWithBlock:^id(id obj) {return (id)[obj rawPeerInfo];}] mutableCopy];
	bool			ok = true;

	if ([toAccept count]) {
		secnotice("cjr", "Process accept: %@", toAccept);
		ok = ok && SOSCCAcceptApplicants((__bridge CFArrayRef) toAccept, error);
	}

	if ([toReject count]) {
		secnotice("cjr", "Process reject: %@", toReject);
		ok = ok && SOSCCRejectApplicants((__bridge CFArrayRef) toReject, error);
	}

	return ok;
}


static void cancelCurrentAlert(bool stopRunLoop) {
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
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
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
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


static void passwordFailurePrompt(void)
{
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
	NSString	 *pwIncorrect = [NSString stringWithFormat:(NSString *)CFBridgingRelease(SecCopyCKString(SEC_CK_PASSWORD_INCORRECT)), appleIDAccountName()];
#pragma clang diagnostic pop
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
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return nil;
    }
    
	if(!applicantToAskAbout || !applicantToAskAbout.name || !applicantToAskAbout.deviceType)
		return NULL;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
	NSString *header = [NSString stringWithFormat: (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_APPROVAL_TITLE), applicantToAskAbout.name];
	NSString *body   = [NSString stringWithFormat: getLocalizedApprovalBody(applicantToAskAbout.deviceType), appleIDAccountName()];
#pragma clang diagnostic pop

	return @{
		(id) kCFUserNotificationAlertHeaderKey		   : header,
		(id) kCFUserNotificationAlertMessageKey		   : body,
		(id) kCFUserNotificationDefaultButtonTitleKey  : (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_APPROVE),
		(id) kCFUserNotificationAlternateButtonTitleKey: (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_DECLINE),
		(id) kCFUserNotificationTextFieldTitlesKey	   : (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_ICLOUD_PASSWORD),
		(id) kCFUserNotificationAlertTopMostKey		   : @YES,				//  get us onto the lock screen
		(__bridge_transfer id) SBUserNotificationDontDismissOnUnlock: @YES,
		(__bridge_transfer id) SBUserNotificationDismissOnLock		: @NO,
    };
}


static void askAboutAll(bool passwordFailure)
{
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    xpc_object_t options = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_DELAY, alertInterval);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_GRACE_PERIOD, XPC_ACTIVITY_INTERVAL_1_MIN);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_REPEATING, false);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_ALLOW_BATTERY, true);
    xpc_dictionary_set_string(options, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_UTILITY);
    
    xpc_activity_register(kLaunchLaterXPCName, options, ^(xpc_activity_t activity) {
        secnotice("cjr", "activity handler fired");
    });
#pragma clang diagnostic pop
}


static void reminderChoice(CFUserNotificationRef userNotification, CFOptionFlags responseFlags) {
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
    if (responseFlags == kCFUserNotificationAlternateResponse || responseFlags == kCFUserNotificationDefaultResponse) {
        PersistentState *state = [PersistentState loadFromStorage];
        NSDate *nowish = [NSDate new];
        state.pendingApplicationReminder = [nowish dateByAddingTimeInterval: state.pendingApplicationReminderAlertInterval];
        scheduleActivity(state.pendingApplicationReminderAlertInterval);
        [state writeToStorage];
        if (responseFlags == kCFUserNotificationAlternateResponse) {
			// Use security code
            BOOL ok = [[LSApplicationWorkspace defaultWorkspace] openSensitiveURL:[NSURL URLWithString:castleKeychainUrl] withOptions:nil];
			secnotice("cjr", "%s iCSC: opening %@ ok=%{BOOL}d", __FUNCTION__, castleKeychainUrl, ok);
        }
    }

    cancelCurrentAlert(true);
}


static bool iCloudResetAvailable(void) {
    SecureBackup *backupd = [[SecureBackup alloc] initWithUserActivityLabel:@"iCloudResetAvailable"];
    NSDictionary *backupdResults;
    NSError		 *error = [backupd getAccountInfoWithInfo:nil results:&backupdResults];
    secnotice("cjr", "SecureBackup e=%@ r=%@", error, backupdResults);
    return (error == nil && [backupdResults[kSecureBackupIsEnabledKey] isEqualToNumber:@YES]);
}


static NSString *getLocalizedApplicationReminder(void) {
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

static bool isSOSInternalDevice(void) {
    static dispatch_once_t onceToken;
    static BOOL internal = NO;
    dispatch_once(&onceToken, ^{
        internal = os_variant_has_internal_diagnostics("com.apple.security");
    });
    return internal;
}

static void postApplicationReminderAlert(NSDate *nowish, PersistentState *state, unsigned int alertInterval)
{
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
    NSString *body		= getLocalizedApplicationReminder();
	bool      has_iCSC	= iCloudResetAvailable();

	if (isSOSInternalDevice() &&
		state.defaultPendingApplicationReminderAlertInterval != state.pendingApplicationReminderAlertInterval) {
#ifdef DEBUG
		body = [body stringByAppendingFormat: @"〖debug interval %u; wait time %@〗",
					state.pendingApplicationReminderAlertInterval,
                             [[[NSDateComponentsFormatter alloc] init] stringFromTimeInterval:[nowish timeIntervalSinceDate:state.applicationDate]]];
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
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
	secnotice("cjr", "kOC %@ %lu", userNotification, responseFlags);
    
    //default response: continue -> settings pref pane advanced keychain sync page
	if (responseFlags == kCFUserNotificationDefaultResponse) {
		// We need to let things unwind to main for the new state to get saved
		doOnceInMain(^{
			NSURL    		  *url		= [NSURL URLWithString: _isAccountICDP ? rejoinICDPUrl : castleKeychainUrl];
			BOOL 			  ok		= [[LSApplicationWorkspace defaultWorkspace] openSensitiveURL:url withOptions:nil];
            secnotice("cjr","kickOutChoice account is iCDP: %{bool}d", _isAccountICDP);
            secnotice("cjr", "ok=%{bool}d opening %@", ok, url);
		});
	}
    //alternate response: later -> call CD
    else if (responseFlags == kCFUserNotificationAlternateResponse) {
        // We need to let things unwind to main for the new state to get saved
        doOnceInMain(^{
            if(_isAccountICDP){
                CDPFollowUpController *cdpd = [[CDPFollowUpController alloc] init];
                NSError *localError = nil;
                
                CDPFollowUpContext *context = nil;
                if (SOSCompatibilityModeEnabled()) {
                    context = [CDPFollowUpContext contextForSOSCompatibilityMode];
                } else {
                    context = [CDPFollowUpContext contextForStateRepair];
                }
                
                if (SOSCompatibilityModeEnabled()) {
                    secnotice("followup", "Posting a follow up (for SOS) of type SOS Compatibility Mode");
                } else {
                    secnotice("followup", "Posting a follow up (for SOS) of type repair");
                }
                [cdpd postFollowUpWithContext:context error:&localError ];
                if(localError){
                    secnotice("cjr", "request to CoreCDP to follow up failed: %@", localError);
                }
                else{
                    secnotice("cjr", "CoreCDP handling follow up");
                    _hasPostedFollowupAndStillInError = true;
                }
            }
        });
    }
    cancelCurrentAlert(true);
}

static void postKickedOutAlert(enum DepartureReason reason)
{
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
	NSString	*header  = nil;
	NSString	*message = nil;

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

	if (isSOSInternalDevice()) {
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
		NSString *reason_str = [NSString stringWithFormat:(__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CR_REASON_INTERNAL),
								departureReasonStrings[idx]];
#pragma clang diagnostic pop
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

static void askForCDPFollowup(void) {
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return;
    }
    
    doOnceInMain(^{
        NSError *localError = nil;
        CDPFollowUpController *cdpd = [[CDPFollowUpController alloc] init];
       
        CDPFollowUpContext *context = nil;
        if (SOSCompatibilityModeEnabled()) {
            context = [CDPFollowUpContext contextForSOSCompatibilityMode];
        } else {
            context = [CDPFollowUpContext contextForStateRepair];
        }

        if (SOSCompatibilityModeEnabled()) {
            secnotice("followup", "Posting a follow up (for SOS) of type SOS Compatibility Mode");
        } else {
            secnotice("followup", "Posting a follow up (for SOS) of type repair");
        }
        
        [cdpd postFollowUpWithContext:context error:&localError ];
        if(localError){
            secnotice("cjr", "request to CoreCDP to follow up failed: %@", localError);
        }
        else{
            secnotice("cjr", "CoreCDP handling follow up");
            _hasPostedFollowupAndStillInError = true;
        }
    });
}

static bool processEvents(void)
{
    if (!SOSCCIsSOSTrustAndSyncingEnabled()) {
        return false;
    }
	debugState = @"processEvents A";

    CFErrorRef			error			 = NULL;
    CFErrorRef			departError		 = NULL;
    SOSCCStatus			circleStatus	 = SOSCCThisDeviceIsInCircleNonCached(&error);
    enum DepartureReason departureReason = SOSCCGetLastDepartureReason(&departError);

    BOOL abortFromError = isErrorFromXPC(error);
    if(abortFromError && circleStatus == kSOSCCError) {
        secnotice("cjr", "returning from processEvents due to error returned from securityd: %@", error);
        return true;
    }
    if (departureReason == kSOSDepartureReasonError && departError && (CFEqual(sSecXPCErrorDomain, CFErrorGetDomain(departError)))) {
        secnotice("cjr", "XPC error while checking last departure reason: \"%@\", not processing events", departError);
        return true;
    }
    
    NSDate				*nowish			 = [NSDate date];
    PersistentState 	*state     		 = [PersistentState loadFromStorage];
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
    
    PSKeychainSyncIsUsingICDP();

    // Refresh because sometimes we're fixed elsewhere before we get here.
    CFReleaseNull(error);
    circleStatus = SOSCCThisDeviceIsInCircleNonCached(&error);
    abortFromError = isErrorFromXPC(error);
    if(abortFromError && circleStatus == kSOSCCError) {
        secnotice("cjr", "returning from processEvents due to error returned from securityd: %@", error);
        return true;
    }

    if(_isAccountICDP){
        state.lastCircleStatus = circleStatus;
        [state writeToStorage];
        if(_hasPostedFollowupAndStillInError == true) {
            secnotice("cjr", "followup not resolved");
            _executeProcessEventsOnce = true;
            return false;
        }

        switch(circleStatus) {
        case kSOSCCInCircle:
            secnotice("cjr", "follow up should be resolved");
            _executeProcessEventsOnce = true;
            _hasPostedFollowupAndStillInError = false;
            break;
        case kSOSCCError:
            secnotice("cjr", "error from SOSCCThisDeviceIsInCircle: %@", error);
            askForCDPFollowup();
            _executeProcessEventsOnce = true;
            return false;
        case kSOSCCCircleAbsent:
        case kSOSCCNotInCircle:
            /*
             You would think we could count on not being iCDP if the account was signed out.  Evidently that's wrong.
             So we'll go based on the artifact that when the account object is reset (like by signing out) the
             departureReason will be set to kSOSDepartureReasonError.  So we won't push to get back into a circle if that's
             the current reason.  I've checked code for other ways we could be out.  If we boot and can't load the account
             we'll end up with kSOSDepartureReasonError.  Then too if we end up in kSOSDepartureReasonError and reboot we end up
             in the same place.  Leave it to cdpd to decide whether the user needs to sign in to an account.
             */
            if(departureReason != kSOSDepartureReasonError) {
                secnotice("cjr", "iCDP: We need to get back into the circle");
                askForCDPFollowup();
            } else {
                secnotice("cjr", "iCDP: We appear to not be associated with an iCloud account");
            }
            _executeProcessEventsOnce = true;
            return false;
        case kSOSCCRequestPending:
            break;
        default:
            secnotice("cjr", "Unknown circle status %d", circleStatus);
            return false;
        }
    } else if(circleStatus == kSOSCCError && state.lastCircleStatus != kSOSCCError && (departureReason == kSOSNeverLeftCircle)) {
        secnotice("cjr", "SA: error from SOSCCThisDeviceIsInCircle: %@", error);
        CFIndex errorCode = CFErrorGetCode(error);
        if(errorCode == kSOSErrorPublicKeyAbsent){
            secnotice("cjr", "SA: We need the password to re-validate ourselves - it's changed on another device");
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
            if(!_isAccountICDP){
                secnotice("cjr", "posting revocation notification!");
                postKickedOutAlert(departureReason);
            }
            else if(_isAccountICDP && _hasPostedFollowupAndStillInError == false){
                NSError *localError = nil;
                CDPFollowUpController *cdpd = [[CDPFollowUpController alloc] init];
                
                CDPFollowUpContext *context = nil;
                if (SOSCompatibilityModeEnabled()) {
                    context = [CDPFollowUpContext contextForSOSCompatibilityMode];
                } else {
                    context = [CDPFollowUpContext contextForStateRepair];
                }

                if (SOSCompatibilityModeEnabled()) {
                    secnotice("followup", "Posting a follow up (for SOS) of type SOS Compatibility Mode");
                } else {
                    secnotice("followup", "Posting a follow up (for SOS) of type repair");
                }

                [cdpd postFollowUpWithContext:context error:&localError ];
                if(localError){
                    secnotice("cjr", "request to CoreCDP to follow up failed: %@", localError);
                }
                else{
                    secnotice("cjr", "CoreCDP handling follow up");
                    _hasPostedFollowupAndStillInError = true;
                }
            }
            else{
                secnotice("cjr", "still waiting for followup to resolve");
            }
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
					secnotice("cjr", "-- CC after original alert gone (currentAlertIsForApplicants %{bool}d, pA %p, cA %p -- %@)",
                              currentAlertIsForApplicants, postedAlert, currentAlert, currentAlert);
                    notify_cancel(token);
                } else {
                    CFErrorRef localError = NULL;
                    SOSCCStatus newCircleStatus = SOSCCThisDeviceIsInCircle(&localError);
                    BOOL xpcError = isErrorFromXPC(localError);
                    if(xpcError && newCircleStatus == kSOSCCError) {
                        secnotice("cjr", "returning from processEvents due to error returned from securityd: %@", localError);
                        return;
                    }
                    
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
			secnotice("cjr", "NOTE: currentAlertIsForApplicants %{bool}d, token %d", currentAlertIsForApplicants, notifyToken);
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
        CFErrorRef circleError = NULL;
        SOSCCStatus currentCircleStatus = SOSCCThisDeviceIsInCircle(&circleError);
        BOOL xpcError = isErrorFromXPC(circleError);
        if(xpcError && currentCircleStatus == kSOSCCError) {
            secnotice("cjr", "returning early due to error returned from securityd: %@", circleError);
            return;
        }
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

    if (!OctagonPlatformSupportsSOS()) {
        secnotice("nosos", "CJR triggered even though SOS is turned off for this platform");
        return 0;
    }

    os_transaction_t txion = os_transaction_create("com.apple.security.circle-join-requested");

	@autoreleasepool {

        // NOTE: DISPATCH_QUEUE_PRIORITY_LOW will not actually manage to drain events in a lot of cases (like circleStatus != kSOSCCInCircle)
        xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(xpc_object_t object) {
            if (SOSCCIsSOSTrustAndSyncingEnabled()) {
                char *event_description = xpc_copy_description(object);
                const char *notificationName = xpc_dictionary_get_string(object, "Notification");
                secnotice("cjr", "notification arrived: %s", notificationName);
                secnotice("cjr", "notifyd event: %s\nAlert (%p) %s %s\ndebugState: %@", event_description, currentAlert,
                          currentAlertIsForApplicants ? "for applicants" : "!applicants",
                          currentAlertIsForKickOut ? "KO" : "!KO", debugState);
                free(event_description);
            }
        });
        
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		xpc_activity_register(kLaunchLaterXPCName, XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        });
#pragma clang diagnostic pop
        
		int falseInARow = 0;
		while (falseInARow < 2 && !_executeProcessEventsOnce) {
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
    (void) txion; // But we really do want this around, compiler...
    txion = nil;
	return(0);
}
