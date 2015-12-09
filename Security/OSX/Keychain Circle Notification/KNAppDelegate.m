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

#import <AOSAccounts/MobileMePrefsCoreAEPrivate.h>
#import <AOSAccounts/MobileMePrefsCore.h>

#include <msgtracer_client.h>
#include <msgtracer_keys.h>
#include <CrashReporterSupport/CrashReporterSupportPrivate.h>

static const char     * const kLaunchLaterXPCName      = "com.apple.security.Keychain-Circle-Notification-TICK";
static const NSString * const kKickedOutKey            = @"KickedOut";
static const NSString * const kValidOnlyOutOfCircleKey = @"ValidOnlyOutOfCircle";


@implementation KNAppDelegate

static NSUserNotificationCenter *appropriateNotificationCenter()
{
    return [NSUserNotificationCenter _centerForIdentifier: @"com.apple.security.keychain-circle-notification"
													 type: _NSUserNotificationCenterTypeSystem];
}


- (void) notifyiCloudPreferencesAbout: (NSString *) eventName
{
	if (eventName == nil)
		return;

	NSString *account = (__bridge NSString *)(MMCopyLoggedInAccount());
	NSLog(@"notifyiCloudPreferencesAbout %@", eventName);

	AEDesc	aeDesc;
	BOOL	createdAEDesc = createAEDescWithAEActionAndAccountID((__bridge NSString *) kMMServiceIDKeychainSync, eventName, account, &aeDesc);
	if (createdAEDesc) {
		LSLaunchURLSpec	lsSpec = {
			.appURL			= NULL,
			.itemURLs		= (__bridge CFArrayRef)([NSArray arrayWithObject: [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/iCloudPref.prefPane"]]),
			.passThruParams	= &aeDesc,
			.launchFlags	= kLSLaunchDefaults | kLSLaunchAsync,
			.asyncRefCon	= NULL,
		};
		OSErr			err = LSOpenFromURLSpec(&lsSpec, NULL);
		
		if (err)
			NSLog(@"Can't send event %@, err=%d", eventName, err);
		AEDisposeDesc(&aeDesc);
	} else {
		NSLog(@"unable to create and send aedesc for account: '%@' and action: '%@'\n", account, eventName);
	}
}


- (void) showiCloudPreferences
{
    static NSAppleScript *script = nil;
    if (!script) {
		static NSString *script_src = @"tell application \"System Preferences\"\n"
									   "activate\n"
									   "set the current pane to pane id \"com.apple.preferences.icloud\"\n"
									   "end tell";
		script = [[NSAppleScript alloc] initWithSource: script_src];
    }
    
    NSDictionary *scriptError = nil;
    [script executeAndReturnError:&scriptError];
    
    if (scriptError)
        NSLog(@"scriptError: %@", scriptError);
	else
        NSLog(@"showiCloudPreferences success");
}


- (void) timerCheck
{
	NSDate *nowish = [NSDate new];

	self.state = [KNPersistentState loadFromStorage];
	if ([nowish compare:self.state.pendingApplicationReminder] != NSOrderedAscending) {
		NSLog(@"REMINDER TIME:     %@ >>> %@", nowish, self.state.pendingApplicationReminder);

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
bool isAppleInternal(void)
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
	NSLog(@"Posted at launch: %@", appropriateNotificationCenter().deliveredNotifications);
	
    self.viewedIds    = [NSMutableSet new];
	self.circle       = [KDSecCircle new];
//	self.state        = [KNPersistentState loadFromStorage];
	KNAppDelegate *me = self;

	[self.circle addChangeCallback:^{
		NSLog(@"{ChangeCallback}");
/*		SOSCCStatus			circleStatus	 = SOSCCThisDeviceIsInCircle(&error);
		NSDate				*nowish			 = [NSDate date];
		PersistentState 	*state     		 = [PersistentState loadFromStorage];
		enum DepartureReason departureReason = SOSCCGetLastDepartureReason(&departError);	*/
//		me.circle.rawStatus		 			 = SOSCCThisDeviceIsInCircle(&error);
		NSDate				*nowish			 = [NSDate date];
		SOSCCStatus	circleStatus 			 = me.circle.rawStatus;
		me.state 							 = [KNPersistentState loadFromStorage];


		// Pending application reminder
		NSLog(@"{ChangeCallback} scheduleActivity %@", me.state.pendingApplicationReminder);
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
				CFErrorRef err = NULL;
				reason = SOSCCGetLastDepartureReason(&err);
				if (reason == kSOSDepartureReasonError) {
					NSLog(@"SOSCCGetLastDepartureReason err: %@", err);
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
				NSLog(@"{ChangeCallback} departure reason %d", reason);

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
				NSLog(@"{ChangeCallback} Pending request START");
				me.state.applicationDate			= nowish;
				me.state.pendingApplicationReminder = [me.state.applicationDate dateByAddingTimeInterval:[me getPendingApplicationReminderInterval]];
				[me.state writeToStorage];			// FIXME: move below? might be needed for scheduleActivityAt...
				[me scheduleActivityAt:me.state.pendingApplicationReminder];
			}
			
			if (lastCircleStatus == kSOSCCRequestPending && circleStatus != kSOSCCRequestPending) {
				NSLog(@"Pending request completed");
				me.state.applicationDate			= [NSDate distantPast];
				me.state.pendingApplicationReminder = [NSDate distantFuture];
				[me.state writeToStorage];

				// Remove reminders
				NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
				for (NSUserNotification *note in noteCenter.deliveredNotifications) {
					if (note.userInfo[kValidOnlyOutOfCircleKey] && note.userInfo[@"ApplicationReminder"]) {
						NSLog(@"{ChangeCallback} Removing notification %@", note);
						[appropriateNotificationCenter() removeDeliveredNotification: note];
					}
				}
			}
		
			// [me.state writeToStorage];
		}
		
		
		// CircleJoinRequested
/*		if (circleStatus != kSOSCCInCircle) {
			if (circleStatus == kSOSCCRequestPending && currentAlert) { ... }	*/

		// Clear out (old) reset notifications
		if (me.circle.isInCircle) {
			NSLog(@"{ChangeCallback} me.circle.isInCircle");
            NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
            for (NSUserNotification *note in noteCenter.deliveredNotifications) {
                if (note.userInfo[kValidOnlyOutOfCircleKey]) {
                    NSLog(@"Removing existing notification (%@) now that we are in circle", note);
                    [appropriateNotificationCenter() removeDeliveredNotification: note];
                }
            }
        }


		// Applicants
		NSLog(@"{ChangeCallback} Applicants");
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
		NSLog(@"Checking validity of %lu notes", (unsigned long)notificationCenter.deliveredNotifications.count);
		for (NSUserNotification *note in notificationCenter.deliveredNotifications) {
			if (note.userInfo[@"applicantId"] && ![applicantIds containsObject:note.userInfo[@"applicantId"]]) {
				NSLog(@"No longer an applicant (%@) for %@ (I=%@)", note.userInfo[@"applicantId"], note, [note.userInfo compactDescription]);
				[notificationCenter removeDeliveredNotification:note];
			} else {
				NSLog(@"Still an applicant (%@) for %@ (I=%@)", note.userInfo[@"applicantId"], note, [note.userInfo compactDescription]);
			}
		}
		
        me.state.lastCircleStatus = me.circle.rawStatus;
        
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
	[self notifyiCloudPreferencesAbout:notification.userInfo[@"Dismiss"]];

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
		NSLog(@"Already viewed %@, skipping", applicant);
		return;
	}

	NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
	for (NSUserNotification *note in noteCenter.deliveredNotifications) {
		if ([applicant.idString isEqualToString:note.userInfo[@"applicantId"]]) {
			if (note.isPresented) {
				NSLog(@"Already posted&presented: %@ (I=%@)", note, note.userInfo);
				return;
			} else {
				NSLog(@"Already posted, but not presented: %@ (I=%@)", note, note.userInfo);
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

    NSLog(@"About to post #%d/%lu (%@): %@", postCount, noteCenter.deliveredNotifications.count, applicant.idString, note);
	[appropriateNotificationCenter() deliverNotification:note];
	postCount++;
}


- (void) postKickedOutAlert: (int) reason
{
	NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
	for (NSUserNotification *note in noteCenter.deliveredNotifications) {
		if (note.userInfo[kKickedOutKey]) {
			if (note.isPresented) {
				NSLog(@"Already posted&presented (removing): %@", note);
				[appropriateNotificationCenter() removeDeliveredNotification: note];
			} else {
				NSLog(@"Already posted, but not presented: %@", note);
			}
		}
	}

	NSString *message = (__bridge_transfer NSString *) SecCopyCKString(SEC_CK_PWD_REQUIRED_BODY_OSX);
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

	NSLog(@"body=%@", note.informativeText);
    NSLog(@"About to post #-/%lu (KICKOUT): %@", noteCenter.deliveredNotifications.count, note);
	[appropriateNotificationCenter() deliverNotification:note];
}


- (void) postApplicationReminder
{
	NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
	for (NSUserNotification *note in noteCenter.deliveredNotifications) {
		if (note.userInfo[@"ApplicationReminder"]) {
			if (note.isPresented) {
				NSLog(@"Already posted&presented (removing): %@", note);
				[appropriateNotificationCenter() removeDeliveredNotification: note];
			} else {
				NSLog(@"Already posted, but not presented: %@", note);
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
	
    NSLog(@"About to post #-/%lu (REMINDER): %@ (I=%@)", noteCenter.deliveredNotifications.count, note, [note.userInfo compactDescription]);
	[appropriateNotificationCenter() deliverNotification:note];
}

@end
