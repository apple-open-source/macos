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

static const char     * const kLaunchLaterXPCName      = "com.apple.security.Keychain-Circle-Notification-TICK";
static const NSString * const kKickedOutKey            = @"KickedOut";
static const NSString * const kValidOnlyOutOfCircleKey = @"ValidOnlyOutOfCircle";
static const NSString * const kPasswordChangedOrTrustedDeviceChanged = @"TDorPasswordChanged";
static NSString *prefpane = @"/System/Library/PreferencePanes/iCloudPref.prefPane";
#define kPublicKeyNotAvailable "com.apple.security.publickeynotavailable"
static NSString *KeychainPCDetailsAEAction            = @"AKPCDetailsAEAction";

@implementation KNAppDelegate

static NSUserNotificationCenter *appropriateNotificationCenter()
{
    return [NSUserNotificationCenter _centerForIdentifier: @"com.apple.security.keychain-circle-notification"
													 type: _NSUserNotificationCenterTypeSystem];
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
    else
        secnotice("kcn", "CoreCDP handling follow up");
}

- (void) handleDismissedNotification
{
    ACAccountStore *accountStore = [[ACAccountStore alloc] init];
    ACAccount *primaryiCloudAccount = nil;

    if ([accountStore respondsToSelector:@selector(icaPrimaryAppleAccount)]){
        primaryiCloudAccount = [accountStore icaPrimaryAppleAccount];
    }

    if(primaryiCloudAccount){
        bool			  localICDP = false;
        NSString *dsid = primaryiCloudAccount.icaPersonID;
        if (dsid) {
            NSDictionary	  *options = @{ (__bridge id) kPCSSetupDSID : dsid, };
            PCSIdentitySetRef identity = PCSIdentitySetCreate((__bridge CFDictionaryRef) options, NULL, NULL);

            if (identity) {
                localICDP = PCSIdentitySetIsICDP(identity, NULL);
                CFRelease(identity);
            }
        }
        if(localICDP){
            secnotice("kcn", "handling dismissed notification, would start a follow up");
            [self startFollowupKitRepair];
        }
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
    else
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


// Copied from sysdiagnose/src/utils.m
static bool isAppleInternal(void)
{
	static bool ret = false;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
#if TARGET_OS_IPHONE
		ret = CRIsAppleInternal();
#else
		ret = CRHasBeenAppleInternalRecently();
#endif
	});
	return ret;
}


#define ICKC_EVENT_DISABLED          "com.apple.security.secureobjectsync.disabled"
#define ICKC_EVENT_DEPARTURE_REASON  "com.apple.security.secureobjectsync.departurereason"
#define ICKC_EVENT_NUM_PEERS         "com.apple.security.secureobjectsync.numcircledevices"

- (void) applicationDidFinishLaunching: (NSNotification *) aNotification
{
	appropriateNotificationCenter().delegate = self;
    int out_taken;
	secnotice("kcn", "Posted at launch: %@", appropriateNotificationCenter().deliveredNotifications);

    //register for public key not available notification, if occurs KCN can react
    notify_register_dispatch(kPublicKeyNotAvailable, &out_taken, dispatch_get_main_queue(), ^(int token) {
        CFErrorRef err = NULL;
        KNAppDelegate *me = self;
        enum DepartureReason departureReason = SOSCCGetLastDepartureReason(&err);
        SOSCCStatus currentCircleStatus     = SOSCCThisDeviceIsInCircle(&err);
        me.state 						 = [KNPersistentState loadFromStorage];

        secnotice("kcn", "got public key not available notification, but won't send notification unless circle transition matches");
        secnotice("kcn", "current circle status: %d, current departure reason: %d, last circle status: %d", currentCircleStatus, departureReason, me.state.lastCircleStatus);

        if(currentCircleStatus == kSOSCCError && me.state.lastCircleStatus == kSOSCCInCircle && (departureReason == kSOSNeverLeftCircle)) {
            secnotice("kcn", "circle status went from in circle to not in circle");
            [self postRequirePassword];
        }
        me.state.lastCircleStatus = currentCircleStatus;

        [me.state writeToStorage];
    });

    self.viewedIds    = [NSMutableSet new];
	self.circle       = [KDSecCircle new];
	KNAppDelegate *me = self;

	[self.circle addChangeCallback:^{
		secnotice("kcn", "{ChangeCallback}");

        CFErrorRef err = NULL;

        enum DepartureReason departureReason = SOSCCGetLastDepartureReason(&err);

		NSDate				*nowish			 = [NSDate date];
		SOSCCStatus	circleStatus 			 = SOSCCThisDeviceIsInCircle(&err);
		me.state 							 = [KNPersistentState loadFromStorage];
        secnotice("kcn", "applicationDidFinishLaunching");

        if(circleStatus == kSOSCCError && me.state.lastCircleStatus == kSOSCCInCircle && (departureReason == kSOSNeverLeftCircle)) {
            CFErrorRef error = NULL;
            SOSCCStatus			currentCircleStatus	 = SOSCCThisDeviceIsInCircle(&error);
            CFIndex errorCode = CFErrorGetCode(error);

            if(errorCode == kSOSErrorPublicKeyAbsent){
                secnotice("kcn", "We need the password to re-validate ourselves - it's changed on another device");
                me.state.lastCircleStatus = currentCircleStatus;
                [me.state writeToStorage];
                [me postRequirePassword];
            }
        }

        // Pending application reminder
		secnotice("kcn", "{ChangeCallback} scheduleActivity %@", me.state.pendingApplicationReminder);
		if (circleStatus == kSOSCCRequestPending)
			[me scheduleActivityAt:me.state.pendingApplicationReminder];


		// No longer in circle?
		if ((me.state.lastCircleStatus == kSOSCCInCircle     && (circleStatus == kSOSCCNotInCircle || circleStatus == kSOSCCCircleAbsent)) ||
			(me.state.lastCircleStatus == kSOSCCCircleAbsent && circleStatus == kSOSCCNotInCircle && me.state.absentCircleWithNoReason) ||
			me.state.debugLeftReason) {
			enum DepartureReason reason = kSOSNeverLeftCircle;
			if (me.state.debugLeftReason) {
				reason = [me.state.debugLeftReason intValue];
				me.state.debugLeftReason = nil;
				[me.state writeToStorage];
			} else {
				reason = SOSCCGetLastDepartureReason(&err);
				if (reason == kSOSDepartureReasonError) {
					secnotice("kcn", "SOSCCGetLastDepartureReason err: %@", err);
				}
				if (err) CFRelease(err);
			}

			if (reason != kSOSDepartureReasonError) {
				// Post kick-out alert

				// <rdar://problem/20862435> MessageTracer data to find out how many users were dropped & reset
				msgtracer_domain_t	domain = msgtracer_domain_new(ICKC_EVENT_DISABLED);
				msgtracer_msg_t		mt_msg = NULL;

				if (domain != NULL)
					mt_msg = msgtracer_msg_new(domain);

				if (mt_msg) {
					char	s[16];

					msgtracer_set(mt_msg, kMsgTracerKeySignature, ICKC_EVENT_DEPARTURE_REASON);
					snprintf(s, sizeof(s), "%u", reason);
					msgtracer_set(mt_msg, kMsgTracerKeyValue, s);

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
					msgtracer_set(mt_msg, kMsgTracerKeySignature2, ICKC_EVENT_NUM_PEERS);
					snprintf(s, sizeof(s), "%lld", num_peers);
					msgtracer_set(mt_msg, kMsgTracerKeyValue2, s);

					msgtracer_set(mt_msg, kMsgTracerKeySummarize, "NO");
					msgtracer_log(mt_msg, ASL_LEVEL_DEBUG, "");
				}

				// FIXME:
				// 1. Write here due to [me timerCheck] => [KNPersistentState loadFromStorage] below?!?
				// 2. Or change call order of timerCheck, pendingApplication reminder below???
				me.state.absentCircleWithNoReason = (circleStatus == kSOSCCCircleAbsent && reason == kSOSNeverLeftCircle);
				[me.state writeToStorage];
				secnotice("kcn", "{ChangeCallback} departure reason %d", reason);

				switch (reason) {
				case kSOSDiscoveredRetirement:
				case kSOSLostPrivateKey:
				case kSOSWithdrewMembership:
				case kSOSNeverAppliedToCircle:
					break;

				case kSOSNeverLeftCircle:
				case kSOSMembershipRevoked:
				case kSOSLeftUntrustedCircle:
				default:
					[me postKickedOutAlert: reason];
					break;
				}
			}
		}
		
		
		// Circle applications: pending request(s) started / completed
		if (me.circle.rawStatus != me.state.lastCircleStatus) {
			SOSCCStatus lastCircleStatus = me.state.lastCircleStatus;
			me.state.lastCircleStatus	 = circleStatus;
		
			if (lastCircleStatus != kSOSCCRequestPending && circleStatus == kSOSCCRequestPending) {
				secnotice("kcn", "{ChangeCallback} Pending request START");
				me.state.applicationDate			= nowish;
				me.state.pendingApplicationReminder = [me.state.applicationDate dateByAddingTimeInterval:[me getPendingApplicationReminderInterval]];
				[me.state writeToStorage];			// FIXME: move below? might be needed for scheduleActivityAt...
				[me scheduleActivityAt:me.state.pendingApplicationReminder];
			}
			
			if (lastCircleStatus == kSOSCCRequestPending && circleStatus != kSOSCCRequestPending) {
				secnotice("kcn", "Pending request completed");
				me.state.applicationDate			= [NSDate distantPast];
				me.state.pendingApplicationReminder = [NSDate distantFuture];
				[me.state writeToStorage];

				// Remove reminders
				NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
				for (NSUserNotification *note in noteCenter.deliveredNotifications) {
					if (note.userInfo[(NSString*) kValidOnlyOutOfCircleKey] && note.userInfo[@"ApplicationReminder"]) {
						secnotice("kcn", "{ChangeCallback} Removing notification %@", note);
						[appropriateNotificationCenter() removeDeliveredNotification: note];
					}
				}
			}
        }
		

		// Clear out (old) reset notifications
		if (me.circle.isInCircle) {
			secnotice("kcn", "{ChangeCallback} me.circle.isInCircle");
            NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
            for (NSUserNotification *note in noteCenter.deliveredNotifications) {
                if (note.userInfo[(NSString*) kValidOnlyOutOfCircleKey]) {
                    secnotice("kcn", "Removing existing notification (%@) now that we are in circle", note);
                    [appropriateNotificationCenter() removeDeliveredNotification: note];
                }
            }
        }

        //Clear out (old) password changed notifications
        if(me.circle.isInCircle){
            secnotice("kcn", "{ChangeCallback} me.circle.isInCircle");
            NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
            for (NSUserNotification *note in noteCenter.deliveredNotifications) {
                if (note.userInfo[(NSString*) kPasswordChangedOrTrustedDeviceChanged]) {
                    secnotice("kcn", "Removing existing notification (%@) now that we are valid again", note);
                    [appropriateNotificationCenter() removeDeliveredNotification: note];
                }
            }

        }

		// Applicants
		secnotice("kcn", "{ChangeCallback} Applicants");
		NSMutableSet *applicantIds = [NSMutableSet new];
		for (KDCirclePeer *applicant in me.circle.applicants) {
            if (!me.circle.isInCircle) {
                // Don't yammer on about circles we aren't in, and don't announce our own
                // join requests as if the user could approve them locally!
                break;
            }
			[me postForApplicant:applicant];
			[applicantIds addObject:applicant.idString];
		}
		

		// Update notifications
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
		
        me.state.lastCircleStatus = circleStatus;
        
		[me.state writeToStorage];
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

	// <rdar://problem/21988060> Improve wording of the iCloud keychain drop/reset error messages
	// Contrary to HI spec (and I think it makes more sense)
	// 1. otherButton  == top   : Not Now
	// 2. actionButton == bottom: Continue
	// 3. If we followed HI spec, replace "Activate" => "Dismiss" in note.userInfo below
	NSUserNotification *note = [NSUserNotification new];
	note.title				 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_APPROVAL_TITLE_OSX);
	note.informativeText	 = [NSString stringWithFormat: (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_APPROVAL_BODY_OSX), applicant.name];
	note._displayStyle		 = _NSUserNotificationDisplayStyleAlert;
    note._identityImage		 = [NSImage bundleImage];
	note._identityImageStyle = _NSUserNotificationIdentityImageStyleRectangleNoBorder;
	note.otherButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_NOT_NOW);
	note.actionButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CONTINUE);
	note.identifier			 = [[NSUUID new] UUIDString];
    note.userInfo = @{
		@"applicantName": applicant.name,
        @"applicantId"  : applicant.idString,
        @"Activate"     : (__bridge NSString *) kMMPropertyKeychainAADetailsAEAction,
	};

    secnotice("kcn", "About to post #%d/%lu (%@): %@", postCount, noteCenter.deliveredNotifications.count, applicant.idString, note);
	[appropriateNotificationCenter() deliverNotification:note];
	postCount++;
}

- (void) postRequirePassword
{
    ACAccountStore *accountStore = [[ACAccountStore alloc] init];
    ACAccount *primaryiCloudAccount = nil;
    bool			  localICDP = false;
    
    if ([accountStore respondsToSelector:@selector(icaPrimaryAppleAccount)]){
        primaryiCloudAccount = [accountStore icaPrimaryAppleAccount];
    }
    
    if(primaryiCloudAccount){
        NSString *dsid = primaryiCloudAccount.icaPersonID;
        
        if (dsid) {
            NSDictionary	  *options = @{ (__bridge id) kPCSSetupDSID : dsid, };
            PCSIdentitySetRef identity = PCSIdentitySetCreate((__bridge CFDictionaryRef) options, NULL, NULL);
            
            if (identity) {
                localICDP = PCSIdentitySetIsICDP(identity, NULL);
                CFRelease(identity);
            }
        }
        if(!localICDP){
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
            if (isAppleInternal()) {
                NSString *reason_str = [NSString stringWithFormat:(__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CR_REASON_INTERNAL), @"Device became untrusted or password changed"];
                message = [message stringByAppendingString: reason_str];
            }
            
            NSUserNotification *note = [NSUserNotification new];
            note.title				 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_PWD_REQUIRED_TITLE);
            note.informativeText	 = message;
            note._identityImage		 = [NSImage bundleImage];
            note._identityImageStyle = _NSUserNotificationIdentityImageStyleRectangleNoBorder;
            note.otherButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_NOT_NOW);
            note.actionButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CONTINUE);
            note.identifier			 = [[NSUUID new] UUIDString];
            
            note.userInfo = @{
                              kPasswordChangedOrTrustedDeviceChanged			: @1,
                              @"Activate"				: (__bridge NSString *) kMMPropertyKeychainPCDetailsAEAction,
                              };
            
            secnotice("kcn", "body=%@", note.informativeText);
            secnotice("kcn", "About to post #-/%lu (PASSWORD/TRUSTED DEVICE): %@", noteCenter.deliveredNotifications.count, note);
            [appropriateNotificationCenter() deliverNotification:note];
        }
        else{
            secnotice("kcn","would have posted needs password and then followed up");
            [self startFollowupKitRepair];
        }
    }
}

- (void) postKickedOutAlert: (int) reason
{
    ACAccountStore *accountStore = [[ACAccountStore alloc] init];
    ACAccount *primaryiCloudAccount = nil;
    bool			  localICDP = false;
    
    if ([accountStore respondsToSelector:@selector(icaPrimaryAppleAccount)]){
        primaryiCloudAccount = [accountStore icaPrimaryAppleAccount];
    }
    
    if(primaryiCloudAccount){
        NSString *dsid = primaryiCloudAccount.icaPersonID;
        
        if (dsid) {
            NSDictionary	  *options = @{ (__bridge id) kPCSSetupDSID : dsid, };
            PCSIdentitySetRef identity = PCSIdentitySetCreate((__bridge CFDictionaryRef) options, NULL, NULL);
            
            if (identity) {
                localICDP = PCSIdentitySetIsICDP(identity, NULL);
                CFRelease(identity);
            }
        }
        if(!localICDP){
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
            if (isAppleInternal()) {
                static const char *departureReasonStrings[] = {
                    "kSOSDepartureReasonError",
                    "kSOSNeverLeftCircle",
                    "kSOSWithdrewMembership",
                    "kSOSMembershipRevoked",
                    "kSOSLeftUntrustedCircle",
                    "kSOSNeverAppliedToCircle",
                    "kSOSDiscoveredRetirement",
                    "kSOSLostPrivateKey",
                    "unknown reason"
                };
                int idx = (kSOSDepartureReasonError <= reason && reason <= kSOSLostPrivateKey) ? reason : (kSOSLostPrivateKey + 1);
                NSString *reason_str = [NSString stringWithFormat:(__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CR_REASON_INTERNAL), departureReasonStrings[idx]];
                message = [message stringByAppendingString: reason_str];
            }
            
            // <rdar://problem/21988060> Improve wording of the iCloud keychain drop/reset error messages
            // Contrary to HI spec (and I think it makes more sense)
            // 1. otherButton  == top   : Not Now
            // 2. actionButton == bottom: Continue
            // 3. If we followed HI spec, replace "Activate" => "Dismiss" in note.userInfo below
            NSUserNotification *note = [NSUserNotification new];
            note.title				 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_PWD_REQUIRED_TITLE);
            note.informativeText	 = message;
            note._identityImage		 = [NSImage bundleImage];
            note._identityImageStyle = _NSUserNotificationIdentityImageStyleRectangleNoBorder;
            note.otherButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_NOT_NOW);
            note.actionButtonTitle	 = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_CONTINUE);
            note.identifier			 = [[NSUUID new] UUIDString];
            
            note.userInfo = @{
                              kKickedOutKey			: @1,
                              kValidOnlyOutOfCircleKey: @1,
                              @"Activate"				: (__bridge NSString *) kMMPropertyKeychainMRDetailsAEAction,
                              };
            
            secnotice("kcn", "body=%@", note.informativeText);
            secnotice("kcn", "About to post #-/%lu (KICKOUT): %@", noteCenter.deliveredNotifications.count, note);
            [appropriateNotificationCenter() deliverNotification:note];
        }
        
        else{
            secnotice("kcn","postKickedOutAlert starting followup repair");
            [self startFollowupKitRepair];
        }
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
    note._identityImage 	 = [NSImage bundleImage];
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
