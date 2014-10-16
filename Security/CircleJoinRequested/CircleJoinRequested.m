//
//  CircleJoinRequested.m
//  CircleJoinRequested
//
//  Created by J Osborne on 3/5/13.
//  Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
//
#import <Accounts/Accounts.h>
#import <Accounts/ACAccountStore_Private.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnewline-eof"
#import <AppleAccount/AppleAccount.h>
#pragma clang diagnostic pop
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#import <Accounts/ACAccountType_Private.h>
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include "SecureObjectSync/SOSCloudCircle.h"
#include "SecureObjectSync/SOSPeerInfo.h"
#import <CoreFoundation/CFUserNotification.h>
#import <SpringBoardServices/SBSCFUserNotificationKeys.h>
#include <notify.h>
#include <sysexits.h>
#import "Applicant.h"
#import "NSArray+map.h"
#import <ManagedConfiguration/MCProfileConnection.h>
#import <ManagedConfiguration/MCFeatures.h>
#import <Security/SecFrameworkStrings.h>
#import "PersistantState.h"
#include <xpc/private.h>
#include <sys/time.h>
#import "NSDate+TimeIntervalDescription.h"
#include <MobileGestalt.h>
#include <xpc/activity.h>
#include <xpc/private.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <MobileCoreServices/LSApplicationWorkspace.h>
#import <CloudServices/SecureBackup.h>
#import <syslog.h>

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

NSString *castleKeychainUrl = @"prefs:root=CASTLE&path=Keychain/ADVANCED";

#if 0
// For use with:      __attribute__((cleanup(CFReleaseSafeIndirect))) CFType auto_var;
static void CFReleaseSafeIndirect(void *o_)
{
    void **o = o_;
    if (o && *o) {
        CFRelease(*o);
    }
}
#endif

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
    ACAccountStore *accountStore = [[ACAccountStore alloc] init];
    ACAccount *primaryAppleAccount = [accountStore aa_primaryAppleAccount];
    return primaryAppleAccount.username;
}

static CFOptionFlags flagsForAsk(Applicant *applicant)
{
	return kCFUserNotificationPlainAlertLevel|CFUserNotificationSecureTextField(0);
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

static BOOL processRequests(CFErrorRef *error)
{
    bool ok = true;
    NSMutableArray *toAccept = [[applicantsInState(ApplicantAccepted) mapWithBlock:^id(id obj) {
        return (id)[obj rawPeerInfo];
    }] mutableCopy];
    NSMutableArray *toReject = [[applicantsInState(ApplicantRejected) mapWithBlock:^id(id obj) {
        return (id)[obj rawPeerInfo];
    }] mutableCopy];
    
    NSLog(@"Process accept: %@", toAccept);
    NSLog(@"Process reject: %@", toReject);
    
    if ([toAccept count]) {
        ok = ok && SOSCCAcceptApplicants((__bridge CFArrayRef)(toAccept), error);
    }
    if ([toReject count]) {
        ok = ok && SOSCCRejectApplicants((__bridge CFArrayRef)(toReject), error);
    }
    
    return ok;
}

static void cancelCurrentAlert(bool stopRunLoop)
{
    if (currentAlertSource) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), currentAlertSource, kCFRunLoopDefaultMode);
        CFRelease(currentAlertSource);
        currentAlertSource = NULL;
    }
    if (currentAlert) {
        CFUserNotificationCancel(currentAlert);
        CFRelease(currentAlert);
        currentAlert = NULL;
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
        NSLog(@"Unexpected response %lu", responseFlags);
        choice = ApplicantRejected;
    }
    
    BOOL processed = NO;
    CFErrorRef error = NULL;
    
    NSArray *onScreen = applicantsInState(ApplicantOnScreen);
    
    [onScreen enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        Applicant* applicant = (Applicant*) obj;
        
        applicant.applicantUIState = choice;
    }];
    
    if (choice == ApplicantRejected) {
        // If this device has ever set up the public key this should work without the password...
        processed = processRequests(&error);
        if (processed) {
            NSLog(@"Didn't need password to process %@", onScreen);
            cancelCurrentAlert(true);
            return;
        } else {
            // ...however if the public key gets lost we should "just" fall through to the validate
            // password path.
            NSLog(@"Couldn't process reject without password (e=%@) for %@ (will try with password next)", error, onScreen);
        }
    }
    
    NSString *password = (__bridge NSString *)(CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, 0));
    if (!password) {
        NSLog(@"No password given, retry");
        askAboutAll(true);
        return;
    }
    const char *passwordUTF8 = [password UTF8String];
    NSData *passwordBytes = [NSData dataWithBytes:passwordUTF8 length:strlen(passwordUTF8)];
    
    // Sometimes securityd crashes between the SOSCCRegisterUserCredentials and the processRequests,
    // (which results in a process error -- I think this is 13355140); as a workaround we retry
    // failure a few times before we give up.
    for (int try = 0; try < 5 && !processed; try++) {
        if (!SOSCCTryUserCredentials(CFSTR(""), (__bridge CFDataRef)(passwordBytes), &error)) {
            NSLog(@"Try user credentials failed %@", error);
            if ((error==NULL) || (CFEqual(kSOSErrorDomain, CFErrorGetDomain(error)) && kSOSErrorWrongPassword == CFErrorGetCode(error))) {
                NSLog(@"Calling askAboutAll again...");
                
                [onScreen enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
                    Applicant* applicant = (Applicant*) obj;
                    
                    applicant.applicantUIState = ApplicantWaiting;
                }];
                askAboutAll(true);
                return;
            }
            EXIT_LOGGED_FAILURE(EX_DATAERR);
        }
        
        processed = processRequests(&error);
        if (!processed) {
            NSLog(@"Can't processRequests: %@ for %@", error, onScreen);
        }
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
    //CFBridgingRelease
    NSString *pwIncorrect = [NSString stringWithFormat:(NSString *)CFBridgingRelease(SecCopyCKString(SEC_CK_PASSWORD_INCORRECT)), appleIDAccountName()];
    NSString *tryAgain = CFBridgingRelease(SecCopyCKString(SEC_CK_TRY_AGAIN));
    NSDictionary *noteAttributes = @{
                                     (id)kCFUserNotificationAlertHeaderKey: pwIncorrect,
                                     (id)kCFUserNotificationDefaultButtonTitleKey: tryAgain,
                                     // TopMost gets us onto the lock screen
                                     (id)kCFUserNotificationAlertTopMostKey: (id)kCFBooleanTrue,
                                     (__bridge id)SBUserNotificationDontDismissOnUnlock: @YES,
                                     (__bridge id)SBUserNotificationDismissOnLock: @NO,
                                     };
    CFOptionFlags flags = kCFUserNotificationPlainAlertLevel;
    SInt32 err;
    CFUserNotificationRef note = CFUserNotificationCreate(NULL, 0.0, flags, &err, (__bridge CFDictionaryRef)noteAttributes);
    CFUserNotificationReceiveResponse(note, 0.0, &flags);
    CFRelease(note);
}

static NSDictionary *createNote(Applicant *applicantToAskAbout) {
    if(!applicantToAskAbout) return NULL;
    NSString *appName = applicantToAskAbout.name;
    if(!appName) return NULL;
    NSString *devType = applicantToAskAbout.deviceType;
    if(!devType) return NULL;
    return @{
        (id)kCFUserNotificationAlertHeaderKey: [NSString stringWithFormat:(__bridge_transfer NSString*)SecCopyCKString(SEC_CK_JOIN_TITLE), appName],
        (id)kCFUserNotificationAlertMessageKey: [NSString stringWithFormat:(__bridge_transfer NSString*)SecCopyCKString(SEC_CK_JOIN_PROMPT), appleIDAccountName(), devType],
        (id)kCFUserNotificationDefaultButtonTitleKey: (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_ALLOW),
        (id)kCFUserNotificationAlternateButtonTitleKey: (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_DONT_ALLOW),
        (id)kCFUserNotificationTextFieldTitlesKey: (__bridge_transfer NSString*)SecCopyCKString(SEC_CK_ICLOUD_PASSWORD),
        // TopMost gets us onto the lock screen
        (id)kCFUserNotificationAlertTopMostKey: (id)kCFBooleanTrue,
        (__bridge_transfer id)SBUserNotificationDontDismissOnUnlock: @YES,
        (__bridge_transfer id)SBUserNotificationDismissOnLock: @NO,
    };
}

static void askAboutAll(bool passwordFailure)
{
    if ([[MCProfileConnection sharedConnection] effectiveBoolValueForSetting: MCFeatureAccountModificationAllowed] == MCRestrictedBoolExplicitNo) {
        NSLog(@"Account modifications not allowed.");
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
        CFRelease(currentAlert);
        currentAlert = NULL;
        if (currentAlertSource) {
            CFRelease(currentAlertSource);
            currentAlertSource = NULL;
        }
    }
    currentAlertIsForApplicants = true;
    
    Applicant *applicantToAskAbout = firstApplicantWaitingOrOnScreen();
    NSLog(@"Asking about: %@ (of: %@)", applicantToAskAbout, applicants);
    
    NSDictionary *noteAttributes = createNote(applicantToAskAbout);
    if(!noteAttributes) {
        NSLog(@"NULL data for %@", applicantToAskAbout);
        cancelCurrentAlert(true);
        return;
    }
    
    CFOptionFlags flags = flagsForAsk(applicantToAskAbout);
    
    if (currentAlert) {
        SInt32 err = CFUserNotificationUpdate(currentAlert, 0, flags, (__bridge CFDictionaryRef)noteAttributes);
        if (err) {
            NSLog(@"CFUserNotificationUpdate err=%d", (int)err);
            EXIT_LOGGED_FAILURE(EX_SOFTWARE);
        }
    } else {
        SInt32 err = 0;
        currentAlert = CFUserNotificationCreate(NULL, 0.0, flags, &err, (__bridge CFDictionaryRef)(noteAttributes));
        if (err) {
            NSLog(@"Can't make notification for %@ err=%x", applicantToAskAbout, (int)err);
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
        NSLog(@"activity handler fired");
    });
}

static void reminderChoice(CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
    if (kCFUserNotificationAlternateResponse == responseFlags || kCFUserNotificationDefaultResponse == responseFlags) {
        PersistantState *state = [PersistantState loadFromStorage];
        NSDate *nowish = [NSDate new];
        state.pendingApplicationReminder = [nowish dateByAddingTimeInterval: state.pendingApplicationReminderAlertInterval];
        scheduleActivity(state.pendingApplicationReminderAlertInterval);
        [state writeToStorage];
        if (kCFUserNotificationAlternateResponse == responseFlags) {
            BOOL ok = [[LSApplicationWorkspace defaultWorkspace] openSensitiveURL:[NSURL URLWithString:castleKeychainUrl] withOptions:nil];
            NSLog(@"ok=%d opening %@", ok, [NSURL URLWithString:castleKeychainUrl]);
        }
    }

    cancelCurrentAlert(true);
}


static NSString* getLocalizedDeviceClass(void) {
    NSString *deviceType = NULL;
    switch (MGGetSInt32Answer(kMGQDeviceClassNumber, MGDeviceClassInvalid)) {
        case MGDeviceClassiPhone:
            deviceType = (__bridge NSString*)SecCopyCKString(SEC_CK_THIS_IPHONE);
            break;
        case MGDeviceClassiPod:
            deviceType = (__bridge NSString*)SecCopyCKString(SEC_CK_THIS_IPOD);
            break;
        case MGDeviceClassiPad:
            deviceType = (__bridge NSString*)SecCopyCKString(SEC_CK_THIS_IPAD);
            break;
        default:
            deviceType = (__bridge NSString*)SecCopyCKString(SEC_CK_THIS_DEVICE);
            break;
    }
    return deviceType;
}

static bool iCloudResetAvailable()
{
    SecureBackup *backupd = [SecureBackup new];
    NSDictionary *backupdResults;
    NSError *error = [backupd getAccountInfoWithInfo:nil results:&backupdResults];
    NSLog(@"SecureBackup e=%@ r=%@", error, backupdResults);
    return (nil == error && [backupdResults[kSecureBackupIsEnabledKey] isEqualToNumber:@YES]);
}

static void postApplicationReminderAlert(NSDate *nowish, PersistantState *state, unsigned int alertInterval)
{

    NSString *deviceType = getLocalizedDeviceClass();
    
    bool has_iCSC = iCloudResetAvailable();

    NSString *alertMessage = [NSString stringWithFormat:(__bridge NSString*)SecCopyCKString(has_iCSC ? SEC_CK_ARS1_BODY : SEC_CK_ARS0_BODY), deviceType];
    if (state.defaultPendingApplicationReminderAlertInterval != state.pendingApplicationReminderAlertInterval) {
        alertMessage = [NSString stringWithFormat:@"%@  〖debug interval %u; wait time %@〗",
                        alertMessage,
                        state.pendingApplicationReminderAlertInterval,
                        [nowish copyDescriptionOfIntervalSince:state.applcationDate]];
    }
    
    NSDictionary *pendingAttributes = @{
                                        (id)kCFUserNotificationAlertHeaderKey: (__bridge NSString*)SecCopyCKString(has_iCSC ? SEC_CK_ARS1_TITLE : SEC_CK_ARS0_TITLE),
                                        (id)kCFUserNotificationAlertMessageKey: alertMessage,
                                        (id)kCFUserNotificationDefaultButtonTitleKey: (__bridge NSString*)SecCopyCKString(SEC_CK_AR_APPROVE_OTHER),
                                        (id)kCFUserNotificationAlternateButtonTitleKey: has_iCSC ? (__bridge NSString*)SecCopyCKString(SEC_CK_AR_USE_CODE) : @"",
                                        (id)kCFUserNotificationAlertTopMostKey: (id)kCFBooleanTrue,
                                        (__bridge id)SBUserNotificationHideButtonsInAwayView: @YES,
                                        (__bridge id)SBUserNotificationDontDismissOnUnlock: @YES,
                                        (__bridge id)SBUserNotificationDismissOnLock: @NO,
										(__bridge id)SBUserNotificationOneButtonPerLine: @YES,
                                        };
    SInt32 err = 0;
    currentAlert = CFUserNotificationCreate(NULL, 0.0, kCFUserNotificationPlainAlertLevel, &err, (__bridge CFDictionaryRef)(pendingAttributes));
    
    if (err) {
        NSLog(@"Can't make pending notification err=%x", (int)err);
    } else {
        currentAlertIsForApplicants = false;
        currentAlertSource = CFUserNotificationCreateRunLoopSource(NULL, currentAlert, reminderChoice, 0);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), currentAlertSource, kCFRunLoopDefaultMode);
    }
}

static void kickOutChoice(CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
    NSLog(@"kOC %@ %lu", userNotification, responseFlags);
    if (kCFUserNotificationAlternateResponse == responseFlags) {
        // We need to let things unwind to main for the new state to get saved
        doOnceInMain(^{
            BOOL ok = [[LSApplicationWorkspace defaultWorkspace] openSensitiveURL:[NSURL URLWithString:castleKeychainUrl] withOptions:nil];
            NSLog(@"ok=%d opening %@", ok, [NSURL URLWithString:castleKeychainUrl]);
        });
    }
    cancelCurrentAlert(true);
}

static void postKickedOutAlert(enum DepartureReason reason)
{
    NSString *deviceType = getLocalizedDeviceClass();
	NSString *message = nil;
    debugState = @"pKOA A";
	bool ok_to_use_code = iCloudResetAvailable();
    debugState = @"pKOA B";
   
	switch (reason) {
		case kSOSNeverLeftCircle:
            // Was: SEC_CK_CR_BODY_NEVER_LEFT
            return;
			break;
			
		case kSOSWithdrewMembership:
            // Was: SEC_CK_CR_BODY_WITHDREW
            // "... if you turn off a switch you have some idea why the light is off" - Murf
            return;
			break;
			
		case kSOSMembershipRevoked:
			message = [NSString stringWithFormat:(__bridge NSString*)SecCopyCKString(SEC_CK_CR_BODY_REVOKED), deviceType];
			break;
			
		case kSOSLeftUntrustedCircle:
			message = [NSString stringWithFormat:(__bridge NSString*)SecCopyCKString(SEC_CK_CR_BODY_LEFT_UNTRUSTED), deviceType];
			ok_to_use_code = false;
			break;
			
        case kSOSNeverAppliedToCircle:
            // We didn't get kicked out, we were never here.   This should only happen if we changed iCloud accounts
            // (and we had sync on in the previous one, and never had it on in the new one).   As this is explicit
            // user action alot of thd "Light switch" argument (above) applies.
            return;
            break;
            
		default:
			message = [NSString stringWithFormat:(__bridge NSString*)SecCopyCKString(SEC_CK_CR_BODY_UNKNOWN), deviceType];
			ok_to_use_code = false;
            syslog(LOG_ERR, "Unknown DepartureReason %d", reason);
			break;
	}
	
    NSDictionary *kickedAttributes = @{
                                       (id)kCFUserNotificationAlertHeaderKey: (__bridge NSString*)SecCopyCKString(SEC_CK_CR_TITLE),
                                       (id)kCFUserNotificationAlertMessageKey: message,
                                       (id)kCFUserNotificationDefaultButtonTitleKey: (__bridge NSString*)SecCopyCKString(SEC_CK_CR_OK),
                                       (id)kCFUserNotificationAlternateButtonTitleKey: ok_to_use_code ? (__bridge NSString*)SecCopyCKString(SEC_CK_CR_USE_CODE)
                                       : @"",
                                       (id)kCFUserNotificationAlertTopMostKey: (id)kCFBooleanTrue,
                                       (__bridge id)SBUserNotificationHideButtonsInAwayView: @YES,
                                       (__bridge id)SBUserNotificationDontDismissOnUnlock: @YES,
                                       (__bridge id)SBUserNotificationDismissOnLock: @NO,
                                       (__bridge id)SBUserNotificationOneButtonPerLine: @YES,
                                       };
    SInt32 err = 0;
    
    if (currentAlertIsForKickOut) {
        debugState = @"pKOA C";
        NSLog(@"Updating existing alert %@ with %@", currentAlert, kickedAttributes);
        CFUserNotificationUpdate(currentAlert, 0, kCFUserNotificationPlainAlertLevel, (__bridge CFDictionaryRef)(kickedAttributes));
    } else {
        debugState = @"pKOA D";

        CFUserNotificationRef note = CFUserNotificationCreate(NULL, 0.0, kCFUserNotificationPlainAlertLevel, &err, (__bridge CFDictionaryRef)(kickedAttributes));
        if (err) {
            NSLog(@"Can't make kicked out notification err=%x", (int)err);
        } else {
            currentAlertIsForApplicants = false;
            currentAlertIsForKickOut = true;
            
            currentAlert = note;
            NSLog(@"New ko alert %@ a=%@", currentAlert, kickedAttributes);
            currentAlertSource = CFUserNotificationCreateRunLoopSource(NULL, currentAlert, kickOutChoice, 0);
            CFRunLoopAddSource(CFRunLoopGetCurrent(), currentAlertSource, kCFRunLoopDefaultMode);
            int backupStateChangeToken;
            notify_register_dispatch("com.apple.EscrowSecurityAlert.reset", &backupStateChangeToken, dispatch_get_main_queue(), ^(int token) {
                if (currentAlert == note) {
                    NSLog(@"Backup state might have changed (dS=%@)", debugState);
                    postKickedOutAlert(reason);
                } else {
                    NSLog(@"Backup state may have changed, but we don't care anymore (dS=%@)", debugState);
                }
            });
            debugState = @"pKOA E";
            CFRunLoopRun();
            debugState = @"pKOA F";
            notify_cancel(backupStateChangeToken);
        }
    }
    debugState = @"pKOA Z";
}

static bool processEvents()
{
    debugState = @"processEvents A";
    CFErrorRef error = NULL;
    CFErrorRef departError = NULL;
    SOSCCStatus circleStatus = SOSCCThisDeviceIsInCircle(&error);
    NSDate *nowish = [NSDate date];
    PersistantState *state = [PersistantState loadFromStorage];
    enum DepartureReason departureReason = SOSCCGetLastDepartureReason(&departError);
    NSLog(@"CircleStatus %d -> %d{%d} (s=%p)", state.lastCircleStatus, circleStatus, departureReason, state);

    NSTimeInterval timeUntilApplicationAlert = [state.pendingApplicationReminder timeIntervalSinceDate:nowish];
    
    NSLog(@"Time until pendingApplicationReminder (%@) %f", [state.pendingApplicationReminder debugDescription], timeUntilApplicationAlert);
    
    if (circleStatus == kSOSCCRequestPending && timeUntilApplicationAlert <= 0) {
        debugState = @"reminderAlert";
        postApplicationReminderAlert(nowish, state, state.pendingApplicationReminderAlertInterval);
    } else if (circleStatus == kSOSCCRequestPending) {
        scheduleActivity(ceil(timeUntilApplicationAlert));
    }
    
    if (((circleStatus == kSOSCCNotInCircle || circleStatus == kSOSCCCircleAbsent) && state.lastCircleStatus == kSOSCCInCircle) || state.debugShowLeftReason || (circleStatus == kSOSCCNotInCircle && state.lastCircleStatus == kSOSCCCircleAbsent && state.absentCircleWithNoReason)) {
        debugState = @"processEvents B";
        // Use to be in the circle, now we aren't.   We ought to tell the user why.
        
        if (state.debugShowLeftReason) {
            NSLog(@"debugShowLeftReason is %@", state.debugShowLeftReason);
            departureReason = [state.debugShowLeftReason intValue];
            state.debugShowLeftReason = nil;
            departError = NULL;
            [state writeToStorage];
        }
        
        if (kSOSDepartureReasonError != departureReason) {
            if (circleStatus == kSOSCCCircleAbsent && departureReason == kSOSNeverLeftCircle) {
                // We don't yet know why the circle has vanished, remember our current ignorance
                state.absentCircleWithNoReason = YES;
            } else {
                state.absentCircleWithNoReason = NO;
            }
            NSLog(@"Depature reason %d", departureReason);
            postKickedOutAlert(departureReason);
            NSLog(@"pKOA returned (cS %d lCS %d)", circleStatus, state.lastCircleStatus);
        } else {
            NSLog(@"Can't get last depature reason: %@", departError);
        }
    }
    
    debugState = @"processEvents C";
    
    if (circleStatus != state.lastCircleStatus) {
        SOSCCStatus lastCircleStatus = state.lastCircleStatus;
        state.lastCircleStatus = circleStatus;
        
        if (lastCircleStatus != kSOSCCRequestPending && circleStatus == kSOSCCRequestPending) {
            state.applcationDate = nowish;
            state.pendingApplicationReminder = [nowish dateByAddingTimeInterval: state.pendingApplicationReminderAlertInterval];
            scheduleActivity(state.pendingApplicationReminderAlertInterval);
        }
        if (lastCircleStatus == kSOSCCRequestPending && circleStatus != kSOSCCRequestPending) {
            NSLog(@"Pending request completed");
            state.applcationDate = [NSDate distantPast];
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
                    NSLog(@"-- CC after original alert gone (currentAlertIsForApplicants %d, pA %p, cA %p -- %@)", currentAlertIsForApplicants ? 1:0, postedAlert, currentAlert, currentAlert);
                    notify_cancel(token);
                } else {
                    CFErrorRef localError = NULL;
                    SOSCCStatus newCircleStatus = SOSCCThisDeviceIsInCircle(&localError);
                    if (newCircleStatus != kSOSCCRequestPending) {
                        if (newCircleStatus == kSOSCCError)
                            NSLog(@"No longer pending (nCS=%d, alert=%@) error: %@", newCircleStatus, currentAlert, localError);
                        else
                            NSLog(@"No longer pending (nCS=%d, alert=%@)", newCircleStatus, currentAlert);
                        cancelCurrentAlert(true);
                    } else {
                        NSLog(@"Still pending...");
                    }
                }
            });
            debugState = @"processEvents D2";
            NSLog(@"NOTE: currentAlertIsForApplicants %d, token %d", currentAlertIsForApplicants ? 1:0, notifyToken);
            CFRunLoopRun();
            return true;
        }
        debugState = @"processEvents D3";
        NSLog(@"SOSCCThisDeviceIsInCircle status %d, not checking applicants", circleStatus);
        return false;
    }
    
    debugState = @"processEvents E";
    applicants = [NSMutableDictionary new];
    for (id applicantInfo in (__bridge_transfer NSArray *)(SOSCCCopyApplicantPeerInfo(&error))) {
        Applicant *applicant = [[Applicant alloc] initWithPeerInfo:(__bridge SOSPeerInfoRef)(applicantInfo)];
        applicants[applicant.idString] = applicant;
    }
    
    int notify_token = -42;
    debugState = @"processEvents F";
    int notify_register_status = notify_register_dispatch(kSOSCCCircleChangedNotification, &notify_token, dispatch_get_main_queue(), ^(int token) {
        NSLog(@"Notified: %s", kSOSCCCircleChangedNotification);
        CFErrorRef circleStatusError = NULL;
        
        bool needsUpdate = false;
        CFErrorRef copyPeerError = NULL;
        NSMutableSet *newIds = [NSMutableSet new];
        for (id applicantInfo in (__bridge_transfer NSArray *)(SOSCCCopyApplicantPeerInfo(&copyPeerError))) {
            Applicant *newApplicant = [[Applicant alloc] initWithPeerInfo:(__bridge SOSPeerInfoRef)(applicantInfo)];
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
                        NSLog(@"Update to %@ >> %@ with pending order, should work out Ok though", existingApplicant, newApplicant);
                        break;
                }
            } else {
                needsUpdate = true;
                applicants[newApplicant.idString] = newApplicant;
            }
        }
        if (copyPeerError) {
            NSLog(@"Could not update peer info array: %@", copyPeerError);
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
            NSLog(@"All applicants were handled elsewhere");
            cancelCurrentAlert(true);
        }
        SOSCCStatus currentCircleStatus = SOSCCThisDeviceIsInCircle(&circleStatusError);
        if (kSOSCCInCircle != currentCircleStatus) {
            NSLog(@"Left circle (%d), not handing remaining %lu applicants", currentCircleStatus, (unsigned long)newIds.count);
            cancelCurrentAlert(true);
        }
        if (needsUpdate) {
            askAboutAll(false);
        } else {
            NSLog(@"needsUpdate false, not updating alert");
        }
    });
    NSLog(@"ACC token %d, status %d", notify_token, notify_register_status);
    debugState = @"processEvents F2";
    
    if (applicants.count == 0) {
        NSLog(@"No applicants");
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

int main (int argc, const char * argv[])
{
    @autoreleasepool {
        xpc_transaction_begin();
        
        // NOTE: DISPATCH_QUEUE_PRIORITY_LOW will not actually manage to drain events
        // in a lot of cases (like circleStatus != kSOSCCInCircle)
        xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(xpc_object_t object) {
            char *event_description = xpc_copy_description(object);
            NSLog(@"notifyd event: %s\nAlert (%p) %s %s\ndebugState: %@", event_description, currentAlert, currentAlertIsForApplicants ? "for applicants" : "!applicants", currentAlertIsForKickOut ? "KO" : "!KO", debugState);
            free(event_description);
        });
        
        xpc_activity_register(kLaunchLaterXPCName, XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        });
        
        
        int falseInARow = 0;
        while (falseInARow < 2) {
            if (processEvents()) {
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
    
    NSLog(@"Done");
    xpc_transaction_end();
    return(0);
}
