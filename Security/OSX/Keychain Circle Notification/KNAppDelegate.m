/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#import "KNAppDelegate.h"
#import "KDSecCircle.h"
#import "KDCirclePeer.h"
#import "NSDictionary+compactDescription.h"
#import <AOSUI/NSImageAdditions.h>
#import <AppleSystemInfo/AppleSystemInfo.h>
#import <Security/SecFrameworkStrings.h>
#import "notify.h"
#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>
#import <utilities/SecXPCError.h>
#import <os/variant_private.h>

#import <Accounts/Accounts.h>
#import <AOSAccounts/MobileMePrefsCoreAEPrivate.h>
#import <AOSAccounts/MobileMePrefsCore.h>
#import <AOSAccounts/ACAccountStore+iCloudAccount.h>
#import <AOSAccounts/iCloudAccount.h>

#include <msgtracer_client.h>
#include <msgtracer_keys.h>
#include <CrashReporterSupport/CrashReporterSupportPrivate.h>
#import <ProtectedCloudStorage/CloudIdentity.h>
#import "CoreCDP/CDPFollowUpController.h"
#import "CoreCDP/CDPFollowUpContext.h"
#import <CoreCDP/CDPAccount.h>

static const char     * const kLaunchLaterXPCName      = "com.apple.security.Keychain-Circle-Notification-TICK";
static const NSString * const kKickedOutKey            = @"KickedOut";
static const NSString * const kValidOnlyOutOfCircleKey = @"ValidOnlyOutOfCircle";
static const NSString * const kPasswordChangedOrTrustedDeviceChanged = @"TDorPasswordChanged";
static NSString *prefpane = @"/System/Library/PreferencePanes/iCloudPref.prefPane";
#define kPublicKeyNotAvailable "com.apple.security.publickeynotavailable"
#define kPublicKeyAvailable "com.apple.security.publickeyavailable"
static NSString *KeychainPCDetailsAEAction            = @"AKPCDetailsAEAction";
bool _hasPostedAndStillInError = false;
bool _haveCheckedForICDPStatusOnceInCircle = false;
bool _isAccountICDP = false;

@implementation KNAppDelegate

static NSUserNotificationCenter *appropriateNotificationCenter()
{
    return [NSUserNotificationCenter _centerForIdentifier: @"com.apple.security.keychain-circle-notification"
													 type: _NSUserNotificationCenterTypeSystem];
}

static BOOL isErrorFromXPC(CFErrorRef error)
{
    // Error due to XPC failure does not provide information about the circle.
    if (error && (CFEqual(sSecXPCErrorDomain, CFErrorGetDomain(error)))) {
        return YES;
    }
    return NO;
}

static void PSKeychainSyncIsUsingICDP(void)
{
    ACAccountStore *accountStore = [[ACAccountStore alloc] init];
    ACAccount *primaryiCloudAccount = nil;
    
    if ([accountStore respondsToSelector:@selector(icaPrimaryAppleAccount)]){
        primaryiCloudAccount = [accountStore icaPrimaryAppleAccount];
    }
    
    NSString *dsid = primaryiCloudAccount.icaPersonID;
    BOOL isICDPEnabled = NO;
    if (dsid) {
        isICDPEnabled = [CDPAccount isICDPEnabledForDSID:dsid];
        NSLog(@"iCDP: PSKeychainSyncIsUsingICDP returning %@", isICDPEnabled ? @"TRUE" : @"FALSE");
    } else {
        NSLog(@"iCDP: no primary account");
    }
    
    _isAccountICDP = isICDPEnabled;
}

-(void) startFollowupKitRepair
{
    NSError *localError = NULL;
    CDPFollowUpController *cdpd = [[CDPFollowUpController alloc] init];
    CDPFollowUpContext *context = [CDPFollowUpContext contextForStateRepair];
    [cdpd postFollowUpWithContext:context error:&localError ];
    if(localError){
        secnotice("kcn", "request to CoreCDP to follow up failed: %@", localError);
    }
    else{
        secnotice("kcn", "CoreCDP handling follow up");
        _hasPostedAndStillInError = false;
    }
}

- (void) handleDismissedNotification
{
    if(_isAccountICDP){
        secnotice("kcn", "handling dismissed notification, would start a follow up");
        [self startFollowupKitRepair];
    }
    else
        secerror("unable to find primary account");
}

- (void) notifyiCloudPreferencesAbout: (NSString *) eventName
{
    if (eventName == nil)
        return;

    secnotice("kcn", "notifyiCloudPreferencesAbout %@", eventName);
    
    NSString *accountID = (__bridge_transfer NSString*)(MMCopyLoggedInAccountFromAccounts());
    ACAccountStore *accountStore = [[ACAccountStore alloc] init];
    ACAccount *primaryiCloudAccount = nil;
    
    if ([accountStore respondsToSelector:@selector(icaPrimaryAppleAccount)]){
        primaryiCloudAccount = [accountStore icaPrimaryAppleAccount];
    }
    
    if(primaryiCloudAccount){
        AEDesc	aeDesc;
        BOOL	createdAEDesc = createAEDescWithAEActionAndAccountID((__bridge NSString *) kMMServiceIDKeychainSync, eventName, accountID, &aeDesc);
        if (createdAEDesc) {
            NSArray *prefPaneURL = [NSArray arrayWithObject: [NSURL fileURLWithPath: prefpane ]];
            
            LSLaunchURLSpec	lsSpec = {
                .appURL			= NULL,
                .itemURLs		= (__bridge CFArrayRef)prefPaneURL,
                .passThruParams	= &aeDesc,
                .launchFlags	= kLSLaunchDefaults | kLSLaunchAsync,
                .asyncRefCon	= NULL,
            };
            
            OSErr			err = LSOpenFromURLSpec(&lsSpec, NULL);
            
            if (err)
                secerror("Can't send event %@, err=%d", eventName, err);
            AEDisposeDesc(&aeDesc);
        } else {
            secerror("unable to create and send aedesc for account: '%@' and action: '%@'\n", primaryiCloudAccount, eventName);
        }
    }
    secerror("unable to find primary account");
}

- (void) timerCheck
{
	NSDate *nowish = [NSDate new];

	self.state = [KNPersistentState loadFromStorage];
	if ([nowish compare:self.state.pendingApplicationReminder] != NSOrderedAscending) {
		secnotice("kcn", "REMINDER TIME:     %@ >>> %@", nowish, self.state.pendingApplicationReminder);

		// self.circle.rawStatus might not be valid yet
		if (SOSCCThisDeviceIsInCircle(NULL) == kSOSCCRequestPending) {
			// Still have a request pending, send reminder, and also in addtion to the UI
			// we need to send a notification for iCloud pref pane to pick up
			CFNotificationCenterPostNotificationWithOptions(
				CFNotificationCenterGetDistributedCenter(),
				CFSTR("com.apple.security.secureobjectsync.pendingApplicationReminder"),
				(__bridge const void *) [self.state.applicationDate description], NULL, 0
			);
			
			[self postApplicationReminder];
			self.state.pendingApplicationReminder = [nowish dateByAddingTimeInterval:[self getPendingApplicationReminderInterval]];
			[self.state writeToStorage];
		}
	}
}


- (void) scheduleActivityAt: (NSDate *) time
{
	if ([time compare:[NSDate distantFuture]] != NSOrderedSame) {
		NSTimeInterval howSoon = [time timeIntervalSinceNow];
		if (howSoon > 0)
			[self scheduleActivityIn:ceil(howSoon)];
		else
			[self timerCheck];
	}
}


- (void) scheduleActivityIn: (int) alertInterval
{
    xpc_object_t options = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_DELAY, alertInterval);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_GRACE_PERIOD, XPC_ACTIVITY_INTERVAL_1_MIN);
    xpc_dictionary_set_bool  (options, XPC_ACTIVITY_REPEATING, false);
    xpc_dictionary_set_bool  (options, XPC_ACTIVITY_ALLOW_BATTERY, true);
    xpc_dictionary_set_string(options, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_UTILITY);
    
    xpc_activity_register(kLaunchLaterXPCName, options, ^(xpc_activity_t activity) {
		[self timerCheck];
    });
}


- (NSTimeInterval) getPendingApplicationReminderInterval
{
	if (self.state.pendingApplicationReminderInterval)
		return [self.state.pendingApplicationReminderInterval doubleValue];
	else
		return 24*60*60;
}

- (NSMutableSet *) makeApplicantSet {
    KNAppDelegate *me = self;
    NSMutableSet *applicantIds = [NSMutableSet new];
    for (KDCirclePeer *applicant in me.circle.applicants) {
        [me postForApplicant:applicant];
        [applicantIds addObject:applicant.idString];
    }
    return applicantIds;
}

- (bool) removeAllNotificationsOfType: (NSString *) typeString {
    bool didRemove = false;
    NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
    for (NSUserNotification *note in noteCenter.deliveredNotifications) {
        if (note.userInfo[typeString]) {
            [appropriateNotificationCenter() removeDeliveredNotification: note];
            didRemove = true;
        }
    }
    return didRemove;
}

static const char *sosCCStatusCString(SOSCCStatus status) {
    switch(status) {
        case kSOSCCError: return "kSOSCCError";
        case kSOSCCInCircle: return "kSOSCCInCircle";
        case kSOSCCNotInCircle: return "kSOSCCNotInCircle";
        case kSOSCCCircleAbsent: return "kSOSCCCircleAbsent";
        case kSOSCCRequestPending: return "kSOSCCRequestPending";
        default: return "unknown";
    }
}

static const char *sosDepartureReasonCString(enum DepartureReason departureReason){
    switch(departureReason) {
        case kSOSDepartureReasonError: return "kSOSDepartureReasonError";
        case kSOSNeverLeftCircle: return "kSOSNeverLeftCircle";
        case kSOSWithdrewMembership: return "kSOSWithdrewMembership";
        case kSOSMembershipRevoked: return "kSOSMembershipRevoked";
        case kSOSLeftUntrustedCircle: return "kSOSLeftUntrustedCircle";
        case kSOSNeverAppliedToCircle: return "kSOSNeverAppliedToCircle";
        case kSOSDiscoveredRetirement: return "kSOSDiscoveredRetirement";
        case kSOSLostPrivateKey: return "kSOSLostPrivateKey";
        default: return "unknown reason";
    }
}


- (void) processCircleState {
    CFErrorRef err = NULL;
    KNAppDelegate *me = self;

    enum DepartureReason departureReason = SOSCCGetLastDepartureReason(&err);
    if (isErrorFromXPC(err)) {
        secnotice("kcn", "SOSCCGetLastDepartureReason failed with xpc error: %@", err);
        CFReleaseNull(err);
        return;
    } else if (err) {
        secnotice("kcn", "SOSCCGetLastDepartureReason failed with: %@", err);
    }
    CFReleaseNull(err);

    SOSCCStatus circleStatus     = SOSCCThisDeviceIsInCircle(&err);
    if (isErrorFromXPC(err)) {
        secnotice("kcn", "SOSCCThisDeviceIsInCircle failed with xpc error: %@", err);
        CFReleaseNull(err);
        return;
    } else if (err) {
        secnotice("kcn", "SOSCCThisDeviceIsInCircle failed with: %@", err);
    }
    CFReleaseNull(err);

    NSDate                *nowish             = [NSDate date];
    me.state                             = [KNPersistentState loadFromStorage];
    SOSCCStatus lastCircleStatus         = me.state.lastCircleStatus;

    PSKeychainSyncIsUsingICDP();

    secnotice("kcn", "processCircleState starting ICDP: %s SOSCCStatus: %s DepartureReason: %s",
              (_isAccountICDP) ? "Enabled": "Disabled",
              sosCCStatusCString(circleStatus),
              sosDepartureReasonCString(departureReason));

    if(_isAccountICDP){
        me.state.lastCircleStatus = circleStatus;
        [me.state writeToStorage];

        switch(circleStatus) {
            case kSOSCCInCircle:
                secnotice("kcn", "iCDP: device is in circle!");
                _hasPostedAndStillInError = false;
                break;
            case kSOSCCRequestPending:
                [me scheduleActivityAt:me.state.pendingApplicationReminder];
                break;
            case kSOSCCCircleAbsent:
            case kSOSCCNotInCircle:
                [me outOfCircleAlert: departureReason];
                secnotice("kcn", "{ChangeCallback} Pending request START");
                me.state.applicationDate            = nowish;
                me.state.pendingApplicationReminder = [me.state.applicationDate dateByAddingTimeInterval:[me getPendingApplicationReminderInterval]];
                [me.state writeToStorage];            // FIXME: move below? might be needed for scheduleActivityAt...
                [me scheduleActivityAt:me.state.pendingApplicationReminder];
                break;
            case kSOSCCError:
                /*
                 You would think we could count on not being iCDP if the account was signed out.  Evidently that's wrong.
                 So we'll go based on the artifact that when the account object is reset (like by signing out) the
                 departureReason will be set to kSOSDepartureReasonError.  So we won't push to get back into a circle if that's
                 the current reason.  I've checked code for other ways we could be out.  If we boot and can't load the account
                 we'll end up with kSOSDepartureReasonError.  Then too if we end up in kSOSDepartureReasonError and reboot we end up
                 in the same place.  Leave it to cdpd to decide whether the user needs to sign in to an account.
                 */
                if(departureReason != kSOSDepartureReasonError) {
                    secnotice("kcn", "ICDP: We need the password to initiate trust");
                    [me postRequirePassword];
                    _hasPostedAndStillInError = true;
                } else {
                    secnotice("kcn", "iCDP: We appear to not be associated with an iCloud account");
                }
                break;
            default:
                secnotice("kcn", "Bad SOSCCStatus return %d", circleStatus);
                break;
        }
    } else { // SA version
        switch(circleStatus) {
            case kSOSCCInCircle:
                secnotice("kcn", "SA: device is in circle!");
                _hasPostedAndStillInError = false;
                break;
            case kSOSCCRequestPending:
                [me scheduleActivityAt:me.state.pendingApplicationReminder];
                secnotice("kcn", "{ChangeCallback} scheduleActivity %@", me.state.pendingApplicationReminder);
                break;
            case kSOSCCCircleAbsent:
            case kSOSCCNotInCircle:
                switch (departureReason) {
                    case kSOSDiscoveredRetirement:
                    case kSOSLostPrivateKey:
                    case kSOSWithdrewMembership:
                    case kSOSNeverAppliedToCircle:
                    case kSOSDepartureReasonError:
                    case kSOSMembershipRevoked:
                    default:
                        if(me.state.lastCircleStatus == kSOSCCInCircle) {
                            secnotice("kcn", "SA: circle status went from in circle to %s: reason: %s", sosCCStatusCString(circleStatus), sosDepartureReasonCString(departureReason));
                        }
                        break;

                    case kSOSNeverLeftCircle:
                    case kSOSLeftUntrustedCircle:
                        [me outOfCircleAlert: departureReason];
                        secnotice("kcn", "{ChangeCallback} Pending request START");
                        me.state.applicationDate            = nowish;
                        me.state.pendingApplicationReminder = [me.state.applicationDate dateByAddingTimeInterval:[me getPendingApplicationReminderInterval]];
                        [me.state writeToStorage];            // FIXME: move below? might be needed for scheduleActivityAt...
                        [me scheduleActivityAt:me.state.pendingApplicationReminder];
                        break;
                }
                break;
            case kSOSCCError:
                if(me.state.lastCircleStatus == kSOSCCInCircle && (departureReason == kSOSNeverLeftCircle)) {
                    secnotice("kcn", "SA: circle status went from in circle to error - we need the password");
                    [me postRequirePassword];
                    _hasPostedAndStillInError = true;
                }
                break;
            default:
                secnotice("kcn", "Bad SOSCCStatus return %d", circleStatus);
                break;
        }
    }


    // Circle applications: pending request(s) started / completed
    if (lastCircleStatus == kSOSCCRequestPending && circleStatus != kSOSCCRequestPending) {
        secnotice("kcn", "Pending request completed");
        me.state.applicationDate            = [NSDate distantPast];
        me.state.pendingApplicationReminder = [NSDate distantFuture];
        [me.state writeToStorage];

        // Remove reminders
        if([me removeAllNotificationsOfType: @"ApplicationReminder"]) {
            secnotice("kcn", "{ChangeCallback} removed application remoinders");
        }
    }

    // Clear out (old) reset notifications
    if (circleStatus == kSOSCCInCircle) {
        secnotice("kcn", "{ChangeCallback} kSOSCCInCircle");
        if([me removeAllNotificationsOfType: (NSString*) kValidOnlyOutOfCircleKey]) {
            secnotice("kcn", "Removed existing notifications now that we're in circle");
        }
        if([me removeAllNotificationsOfType: (NSString*) kPasswordChangedOrTrustedDeviceChanged]) {
            secnotice("kcn", "Removed existing password notifications now that we're in circle");
        }

        // Applicants
        secnotice("kcn", "{ChangeCallback} Applicants");
        NSMutableSet *applicantIds = [me makeApplicantSet];
        // Clear applicant notifications that aren't pending any more
        NSUserNotificationCenter *notificationCenter = appropriateNotificationCenter();
        secnotice("kcn", "Checking validity of %lu notes", (unsigned long)notificationCenter.deliveredNotifications.count);
        for (NSUserNotification *note in notificationCenter.deliveredNotifications) {
            if (note.userInfo[@"applicantId"] && ![applicantIds containsObject:note.userInfo[@"applicantId"]]) {
                secnotice("kcn", "No longer an applicant (%@) for %@ (I=%@)", note.userInfo[@"applicantId"], note, [note.userInfo compactDescription]);
                [notificationCenter removeDeliveredNotification:note];
            } else {
                secnotice("kcn", "Still an applicant (%@) for %@ (I=%@)", note.userInfo[@"applicantId"], note, [note.userInfo compactDescription]);
            }
        }
    } else { // Clear any pending applicant notifications since we aren't in circle or invalid
        if([me removeAllNotificationsOfType: (NSString*) @"applicantId"]) {
            secnotice("kcn", "Not in circle or invalid - removed applicant notes");
        }
    }

    me.state.lastCircleStatus = circleStatus;
    [me.state writeToStorage];
}

- (void) applicationDidFinishLaunching: (NSNotification *) aNotification
{
	appropriateNotificationCenter().delegate = self;
    int out_taken;
    int available;
	secnotice("kcn", "Posted at launch: %@", appropriateNotificationCenter().deliveredNotifications);

    notify_register_dispatch(kPublicKeyAvailable, &available, dispatch_get_main_queue(), ^(int token) {
        CFErrorRef err = NULL;
        KNAppDelegate *me = self;
        SOSCCStatus currentCircleStatus     = SOSCCThisDeviceIsInCircle(&err);

        if (isErrorFromXPC(err)) {
            secnotice("kcn", "SOSCCThisDeviceIsInCircle failed with: %@", err);
            CFReleaseNull(err);
            return;
        }
        CFReleaseNull(err);

        me.state                          = [KNPersistentState loadFromStorage];

        secnotice("kcn", "got public key available notification");
        me.state.lastCircleStatus = currentCircleStatus;
        [me.state writeToStorage];
    });

    //register for public key not available notification, if occurs KCN can react
    notify_register_dispatch(kPublicKeyNotAvailable, &out_taken, dispatch_get_main_queue(), ^(int token) {
        secnotice("kcn", "got public key not available notification");
        KNAppDelegate *me = self;
        [me processCircleState];
    });

    self.viewedIds    = [NSMutableSet new];
	self.circle       = [KDSecCircle new];
	KNAppDelegate *me = self;

	[self.circle addChangeCallback:^{
		secnotice("kcn", "{ChangeCallback}");
        [me processCircleState];
    }];
}


- (BOOL) userNotificationCenter: (NSUserNotificationCenter *) center
	  shouldPresentNotification: (NSUserNotification *) notification
{
	return YES;
}


- (void) userNotificationCenter: (NSUserNotificationCenter *) center
		didActivateNotification: (NSUserNotification *) notification
{
	if (notification.activationType == NSUserNotificationActivationTypeActionButtonClicked) {
		[self notifyiCloudPreferencesAbout:notification.userInfo[@"Activate"]];
	}
}


- (void) userNotificationCenter: (NSUserNotificationCenter *) center
				didDismissAlert: (NSUserNotification *) notification
{
    [self handleDismissedNotification];

    // If we don't do anything here & another notification comes in we
    // will repost the alert, which will be dumb.
    id applicantId = notification.userInfo[@"applicantId"];
    if (applicantId != nil) {
        [self.viewedIds addObject:applicantId];
    }
}


- (void) postForApplicant: (KDCirclePeer *) applicant
{
	static int postCount = 0;

	if ([self.viewedIds containsObject:applicant.idString]) {
		secnotice("kcn", "Already viewed %@, skipping", applicant);
		return;
	}

	NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
	for (NSUserNotification *note in noteCenter.deliveredNotifications) {
		if ([applicant.idString isEqualToString:note.userInfo[@"applicantId"]]) {
			if (note.isPresented) {
				secnotice("kcn", "Already posted&presented: %@ (I=%@)", note, note.userInfo);
				return;
			} else {
				secnotice("kcn", "Already posted, but not presented: %@ (I=%@)", note, note.userInfo);
			}
		}
	}

	NSUserNotification *note = [NSUserNotification new];
    note.title               = [NSString stringWithFormat: (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_APPROVAL_TITLE), applicant.name];
    note.informativeText	 = [KNAppDelegate localisedApprovalBodyWithDeviceTypeFromPeerInfo:applicant.peerObject];
	note._displayStyle		 = _NSUserNotificationDisplayStyleAlert;
    note._identityImage		 = [NSImage bundleImageNamed:kAOSUISpyglassAppleID];
	note._identityImageStyle = _NSUserNotificationIdentityImageStyleRectangleNoBorder;
	note.otherButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_DECLINE);
	note.actionButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_APPROVE);
	note.identifier			 = [[NSUUID new] UUIDString];
    note.userInfo = @{
		@"applicantName": applicant.name,
        @"applicantId"  : applicant.idString,
        @"Activate"     : (__bridge NSString *) kMMPropertyKeychainAADetailsAEAction,
	};

    secnotice("kcn", "About to post #%d/%lu (%@): %@", postCount, noteCenter.deliveredNotifications.count, applicant.idString, note);
	[appropriateNotificationCenter() deliverNotification:note];
    [self.viewedIds addObject:applicant.idString];
	postCount++;
}

+ (NSString *)localisedApprovalBodyWithDeviceTypeFromPeerInfo:(id)peerInfo {
    NSString *type = (__bridge NSString *)SOSPeerInfoGetPeerDeviceType((__bridge SOSPeerInfoRef)(peerInfo));
    CFStringRef localisedType = NULL;
    if ([type isEqualToString:@"iPad"]) {
        localisedType = SecCopyCKString(SEC_CK_APPROVAL_BODY_OSX_IPAD);
    } else if ([type isEqualToString:@"iPhone"]) {
        localisedType = SecCopyCKString(SEC_CK_APPROVAL_BODY_OSX_IPHONE);
    } else if ([type isEqualToString:@"iPod"]) {
        localisedType = SecCopyCKString(SEC_CK_APPROVAL_BODY_OSX_IPOD);
    } else if ([type isEqualToString:@"Mac"]) {
        localisedType = SecCopyCKString(SEC_CK_APPROVAL_BODY_OSX_MAC);
    } else {
        localisedType = SecCopyCKString(SEC_CK_APPROVAL_BODY_OSX_GENERIC);
    }
    return (__bridge_transfer NSString *)localisedType;
}

- (void) postRequirePassword
{
    SOSCCStatus currentCircleStatus     = SOSCCThisDeviceIsInCircle(NULL);
    if(currentCircleStatus != kSOSCCError) {
        secnotice("kcn", "postRequirePassword when not needed");
        return;
    }

    enum DepartureReason departureReason = SOSCCGetLastDepartureReason(NULL);

    if(_isAccountICDP){
        secnotice("kcn","would have posted needs password and then followed up");
        [self startFollowupKitRepair];
    } else if(departureReason == kSOSNeverLeftCircle) { // The only SA case for prompting
        NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
        for (NSUserNotification *note in noteCenter.deliveredNotifications) {
            if (note.userInfo[(NSString*) kPasswordChangedOrTrustedDeviceChanged]) {
                if (note.isPresented) {
                    secnotice("kcn", "Already posted & presented: %@", note);
                    [appropriateNotificationCenter() removeDeliveredNotification: note];
                } else {
                    secnotice("kcn", "Already posted, but not presented: %@", note);
                }
            }
        }

        NSString *message = CFBridgingRelease(SecCopyCKString(SEC_CK_PWD_REQUIRED_BODY_OSX));
        if (os_variant_has_internal_ui("iCloudKeychain")) {
            NSString *reason_str = [NSString stringWithFormat:(__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CR_REASON_INTERNAL), "Device became untrusted or password changed"];
            message = [message stringByAppendingString: reason_str];
        }

        NSUserNotification *note = [NSUserNotification new];
        note.title                 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_PWD_REQUIRED_TITLE);
        note.informativeText     = message;
        note._identityImage         = [NSImage bundleImageNamed:kAOSUISpyglassAppleID];
        note._identityImageStyle = _NSUserNotificationIdentityImageStyleRectangleNoBorder;
        note.otherButtonTitle     = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_NOT_NOW);
        note.actionButtonTitle     = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CONTINUE);
        note.identifier             = [[NSUUID new] UUIDString];

        note.userInfo = @{
                          kPasswordChangedOrTrustedDeviceChanged            : @1,
                          @"Activate"                : (__bridge NSString *) kMMPropertyKeychainPCDetailsAEAction,
                          };

        secnotice("kcn", "body=%@", note.informativeText);
        secnotice("kcn", "About to post #-/%lu (PASSWORD/TRUSTED DEVICE): %@", noteCenter.deliveredNotifications.count, note);
        [appropriateNotificationCenter() deliverNotification:note];
    } else {
        secnotice("kcn", "postRequirePassword when not needed for SA");
    }
}

- (void) outOfCircleAlert: (int) reason
{

    if(!_isAccountICDP){
        NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
        for (NSUserNotification *note in noteCenter.deliveredNotifications) {
            if (note.userInfo[(NSString*) kKickedOutKey]) {
                if (note.isPresented) {
                    secnotice("kcn", "Already posted&presented (removing): %@", note);
                    [appropriateNotificationCenter() removeDeliveredNotification: note];
                } else {
                    secnotice("kcn", "Already posted, but not presented: %@", note);
                }
            }
        }

        NSString *message = CFBridgingRelease(SecCopyCKString(SEC_CK_PWD_REQUIRED_BODY_OSX));
        if (os_variant_has_internal_ui("iCloudKeychain")) {
            NSString *reason_str = [NSString stringWithFormat:(__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CR_REASON_INTERNAL), sosDepartureReasonCString(reason)];
            message = [message stringByAppendingString: reason_str];
        }

        // <rdar://problem/21988060> Improve wording of the iCloud keychain drop/reset error messages
        // Contrary to HI spec (and I think it makes more sense)
        // 1. otherButton  == top   : Not Now
        // 2. actionButton == bottom: Continue
        // 3. If we followed HI spec, replace "Activate" => "Dismiss" in note.userInfo below
        NSUserNotification *note = [NSUserNotification new];
        note.title                 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_PWD_REQUIRED_TITLE);
        note.informativeText     = message;
        note._identityImage         = [NSImage bundleImageNamed:kAOSUISpyglassAppleID];
        note._identityImageStyle = _NSUserNotificationIdentityImageStyleRectangleNoBorder;
        note.otherButtonTitle     = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_NOT_NOW);
        note.actionButtonTitle     = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CONTINUE);
        note.identifier             = [[NSUUID new] UUIDString];

        note.userInfo = @{
                          kKickedOutKey            : @1,
                          kValidOnlyOutOfCircleKey: @1,
                          @"Activate"                : (__bridge NSString *) kMMPropertyKeychainMRDetailsAEAction,
                          };

        secnotice("kcn", "body=%@", note.informativeText);
        secnotice("kcn", "About to post #-/%lu (KICKOUT): %@", noteCenter.deliveredNotifications.count, note);
        [appropriateNotificationCenter() deliverNotification:note];
    }

    else{
        secnotice("kcn","outOfCircleAlert starting followup repair");
        [self startFollowupKitRepair];
    }
}

- (void) postApplicationReminder
{
	NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
	for (NSUserNotification *note in noteCenter.deliveredNotifications) {
		if (note.userInfo[@"ApplicationReminder"]) {
			if (note.isPresented) {
				secnotice("kcn", "Already posted&presented (removing): %@", note);
				[appropriateNotificationCenter() removeDeliveredNotification: note];
			} else {
				secnotice("kcn", "Already posted, but not presented: %@", note);
			}
		}
	}

	// <rdar://problem/21988060> Improve wording of the iCloud keychain drop/reset error messages
	// Contrary to HI spec (and I think it makes more sense)
	// 1. otherButton  == top   : Not Now
	// 2. actionButton == bottom: Continue
	// 3. If we followed HI spec, replace "Activate" => "Dismiss" in note.userInfo below
	NSUserNotification *note = [NSUserNotification new];
	note.title				 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_REMINDER_TITLE_OSX);
	note.informativeText	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_REMINDER_BODY_OSX);
    note._identityImage 	 = [NSImage bundleImageNamed:kAOSUISpyglassAppleID];
	note._identityImageStyle = _NSUserNotificationIdentityImageStyleRectangleNoBorder;
	note.otherButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_NOT_NOW);
	note.actionButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CONTINUE);
	note.identifier			 = [[NSUUID new] UUIDString];
    
    note.userInfo = @{
		@"ApplicationReminder"	: @1,
		kValidOnlyOutOfCircleKey: @1,
        @"Activate"				: (__bridge NSString *) kMMPropertyKeychainWADetailsAEAction,
    };
	
    secnotice("kcn", "About to post #-/%lu (REMINDER): %@ (I=%@)", noteCenter.deliveredNotifications.count, note, [note.userInfo compactDescription]);
	[appropriateNotificationCenter() deliverNotification:note];
}

@end
