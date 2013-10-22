//
//  KNAppDelegate.m
//  Keychain Circle Notification
//
//  Created by J Osborne on 2/21/13.
//
//

#import "KNAppDelegate.h"
#import "KDSecCircle.h"
#import "KDCirclePeer.h"
#import "NSDictionary+compactDescription.h"
#import <AOSUI/NSImageAdditions.h>
#import <AppleSystemInfo/AppleSystemInfo.h>
#import <Security/SecFrameworkStrings.h>

#import <AOSAccounts/MobileMePrefsCoreAEPrivate.h>
#import <AOSAccounts/MobileMePrefsCore.h>

static char *kLaunchLaterXPCName = "com.apple.security.Keychain-Circle-Notification-TICK";

@implementation KNAppDelegate

static NSUserNotificationCenter *appropriateNotificationCenter()
{
    return [NSUserNotificationCenter _centerForIdentifier:@"com.apple.security.keychain-circle-notification" type:_NSUserNotificationCenterTypeSystem];
}

-(void)notifyiCloudPreferencesAbout:(NSString *)eventName;
{
    if (nil == eventName) {
        return;
    }
    
    NSString *account = (__bridge NSString *)(MMCopyLoggedInAccount());
    NSLog(@"notifyiCloudPreferencesAbout %@", eventName);
    
    AEDesc aeDesc;
    BOOL createdAEDesc = createAEDescWithAEActionAndAccountID((__bridge NSString *)kMMServiceIDKeychainSync, eventName, account, &aeDesc);
    if (createdAEDesc)
    {
        OSErr                                err;
        LSLaunchURLSpec         lsSpec;
        
        lsSpec.appURL = NULL;
        lsSpec.itemURLs = (__bridge CFArrayRef)([NSArray arrayWithObject:[NSURL fileURLWithPath:@"/System/Library/PreferencePanes/iCloudPref.prefPane"]]);
        lsSpec.passThruParams = &aeDesc;
        lsSpec.launchFlags = kLSLaunchDefaults | kLSLaunchAsync;
        lsSpec.asyncRefCon = NULL;
        
        err = LSOpenFromURLSpec(&lsSpec, NULL);
        
        if (err) {
            NSLog(@"Can't send event %@, err=%d", eventName, err);
        }
        AEDisposeDesc(&aeDesc);
    }
    else
    {
        NSLog(@"unable to create and send aedesc for account: '%@' and action: '%@'\n", account, eventName);
    }
}

-(void)showiCloudPrefrences
{
    static NSAppleScript *script = nil;
    if (!script) {
        script = [[NSAppleScript alloc] initWithSource:@"tell application \"System Preferences\"\n\
                  activate\n\
                  set the current pane to pane id \"com.apple.preferences.icloud\"\n\
                  end tell"];
    }
    
    NSDictionary *appleScriptError = nil;
    [script executeAndReturnError:&appleScriptError];
    
    if (appleScriptError) {
        NSLog(@"appleScriptError: %@", appleScriptError);
    } else {
        NSLog(@"NO appleScript error");
    }
}

-(void)timerCheck
{
	NSDate *nowish = [NSDate new];
	self.state = [KNPersistantState loadFromStorage];
	if ([nowish compare:self.state.pendingApplicationReminder] != NSOrderedAscending) {
		NSLog(@"REMINDER TIME:     %@ >>> %@", nowish, self.state.pendingApplicationReminder);
		// self.circle.rawStatus might not be valid yet
		if (SOSCCThisDeviceIsInCircle(NULL) == kSOSCCRequestPending) {
			// Still have a request pending, send reminder, and also in addtion to the UI
			// we need to send a notification for iCloud pref pane to pick up
			
			CFNotificationCenterPostNotificationWithOptions(CFNotificationCenterGetDistributedCenter(), CFSTR("com.apple.security.secureobjectsync.pendingApplicationReminder"), (__bridge const void *)([self.state.applcationDate description]), NULL, 0);
			
			[self postApplicationReminder];
			self.state.pendingApplicationReminder = [self.state.applcationDate dateByAddingTimeInterval:[self getPendingApplicationReminderInterval]];
			[self.state writeToStorage];
		}
	}
}

-(void)scheduleActivityAt:(NSDate*)time
{
	if ([time compare:[NSDate distantFuture]] != NSOrderedSame) {
		NSTimeInterval howSoon = [time timeIntervalSinceNow];
		if (howSoon > 0) {
			[self scheduleActivityIn:howSoon];
		} else {
			[self timerCheck];
		}
	}
}

-(void)scheduleActivityIn:(int)alertInterval
{
    xpc_object_t options = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_DELAY, alertInterval);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_GRACE_PERIOD, XPC_ACTIVITY_INTERVAL_1_MIN);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_REPEATING, false);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_ALLOW_BATTERY, true);
    xpc_dictionary_set_string(options, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_UTILITY);
    
    xpc_activity_register(kLaunchLaterXPCName, options, ^(xpc_activity_t activity) {
		[self timerCheck];
    });
}

-(NSTimeInterval)getPendingApplicationReminderInterval
{
	if (self.state.pendingApplicationReminderInterval) {
		return [self.state.pendingApplicationReminderInterval doubleValue];
	} else {
		return 48*24*60*60;
	}
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	appropriateNotificationCenter().delegate = self;
	
	NSLog(@"Posted at launch: %@", appropriateNotificationCenter().deliveredNotifications);
	
    self.viewedIds = [NSMutableSet new];
	self.circle = [KDSecCircle new];
	self.state = [KNPersistantState loadFromStorage];
	KNAppDelegate *me = self;
	
	[self.circle addChangeCallback:^{
		me.state = [KNPersistantState loadFromStorage];
		if ((me.state.lastCircleStatus == kSOSCCInCircle && !me.circle.isInCircle) || me.state.debugLeftReason) {
			enum DepartureReason reason = kSOSNeverLeftCircle;
			if (me.state.debugLeftReason) {
				reason = [me.state.debugLeftReason intValue];
				me.state.debugLeftReason = nil;
			} else {
				CFErrorRef err = NULL;
				reason = SOSCCGetLastDepartureReason(&err);
				if (reason == kSOSDepartureReasonError) {
					NSLog(@"SOSCCGetLastDepartureReason err: %@", err);
				}
			}
			
			//NSString *model = (__bridge NSString *)(ASI_CopyComputerModelName(FALSE));
			NSString *body = nil;
			switch (reason) {
				case kSOSDepartureReasonError:
				case kSOSNeverLeftCircle:
				case kSOSWithdrewMembership:
					break;
					
				default:
					NSLog(@"Unknown departure reason %d", reason);
					// fallthrough on purpose
					
				case kSOSMembershipRevoked:
				case kSOSLeftUntrustedCircle:
					body = NSLocalizedString(@"Approve this Mac from another device to use iCloud Keychain.", @"Body for iCloud Keychain Reset notification");
					break;
			}
			[me.state writeToStorage];
			NSLog(@"departure reason %d, body=%@", reason, body);
			if (body) {
				[me postKickedOutWithMessage: body];
			}
		}
		
		[me timerCheck];
		
		if (me.state.lastCircleStatus != kSOSCCRequestPending && me.circle.rawStatus == kSOSCCRequestPending) {
			NSLog(@"Entered RequestPending");
			NSDate *nowish = [NSDate new];
			me.state.applcationDate = nowish;
			me.state.pendingApplicationReminder = [me.state.applcationDate dateByAddingTimeInterval:[me getPendingApplicationReminderInterval]];
			[me.state writeToStorage];
			[me scheduleActivityAt:me.state.pendingApplicationReminder];
		}
		
		NSMutableSet *applicantIds = [NSMutableSet new];
		for (KDCirclePeer *applicant in me.circle.applicants) {
            if (!me.circle.isInCircle) {
                // We don't want to yammer on about circles we aren't in,
                // and we don't want to be extra confusing announcing our
                // own join requests as if the user could approve them
                // locally!
                break;
            }
			[me postForApplicant:applicant];
			[applicantIds addObject:applicant.idString];
		}
		
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
	
	[me scheduleActivityAt:me.state.pendingApplicationReminder];
}

-(BOOL)userNotificationCenter:(NSUserNotificationCenter *)center shouldPresentNotification:(NSUserNotification *)notification
{
	return YES;
}

-(void)userNotificationCenter:(NSUserNotificationCenter *)center didActivateNotification:(NSUserNotification *)notification
{
    if (notification.activationType == NSUserNotificationActivationTypeActionButtonClicked) {
        [self notifyiCloudPreferencesAbout:notification.userInfo[@"Activate"]];
    }

    // The "Later" seems handled Ok without doing anything here, but KickedOut & other special items need an action
	if (notification.userInfo[@"SPECIAL"]) {
		NSLog(@"ACTIVATED (remove): %@", notification);
		[appropriateNotificationCenter() removeDeliveredNotification:notification];
	} else {
		NSLog(@"ACTIVATED (NOT removed): %@", notification);
    }
}

-(void)userNotificationCenter:(NSUserNotificationCenter *)center didDismissAlert:(NSUserNotification *)notification
{
    [self notifyiCloudPreferencesAbout:notification.userInfo[@"Dismiss"]];
    
    if (!notification.userInfo[@"SPECIAL"]) {
		// If we don't do anything here & another notification comes in we
		// will repost the alert, which will be dumb.
        id applicantId = notification.userInfo[@"applicantId"];
        if (applicantId != nil) {
            [self.viewedIds addObject:applicantId];
        }
        NSLog(@"DISMISS (t) %@", notification);
	} else {
        NSLog(@"DISMISS (f) %@", notification);
		[appropriateNotificationCenter() removeDeliveredNotification:notification];
	}
}

-(void)postForApplicant:(KDCirclePeer*)applicant
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
	
	NSUserNotification *note = [NSUserNotification new];
	
    // Genstrings command line is: genstrings -o en.lproj -u KNAppDelegate.m
	note.title = [NSString stringWithFormat:NSLocalizedString(@"iCloud Keychain", @"Title for new keychain syncing device notification")];
	note.informativeText = [NSString stringWithFormat:NSLocalizedString(@"\\U201C%1$@\\U201D wants to use your passwords.", @"Message text for new keychain syncing device notification"), applicant.name];
	    
	note.hasActionButton = YES;
	note._displayStyle = _NSUserNotificationDisplayStyleAlert;
    note._identityImage = [NSImage bundleImage];
    note._identityImageHasBorder = NO;
    note._actionButtonIsSnooze = YES;
	note.actionButtonTitle = NSLocalizedString(@"Later", @"Button label to dismiss device notification");
	note.otherButtonTitle = NSLocalizedString(@"View", @"Button label to view device notification");
	
	note.identifier = [[NSUUID new] UUIDString];
    
    note.userInfo = @{@"applicantName": applicant.name,
                      @"applicantId": applicant.idString,
                      @"Dismiss": (__bridge NSString *)kMMPropertyKeychainMRRequestApprovalAEAction,
                      };

    NSLog(@"About to post#%d/%lu (%@): %@", postCount, (unsigned long)noteCenter.deliveredNotifications.count, applicant.idString, note);
	[appropriateNotificationCenter() deliverNotification:note];
	
	postCount++;
}

-(void)postKickedOutWithMessage:(NSString*)body
{
	NSUserNotificationCenter *noteCenter = appropriateNotificationCenter();
	for (NSUserNotification *note in noteCenter.deliveredNotifications) {
		if (note.userInfo[@"KickedOut"]) {
			if (note.isPresented) {
				NSLog(@"Already posted&presented (removing): %@", note);
				[appropriateNotificationCenter() removeDeliveredNotification: note];
			} else {
				NSLog(@"Already posted, but not presented: %@", note);
			}
		}
	}
	
	NSUserNotification *note = [NSUserNotification new];
	
	note.title = NSLocalizedString(@"iCloud Keychain Was Reset", @"Title for iCloud Keychain Reset notification");
	note.informativeText = body; // Already LOCed
	
    note._identityImage = [NSImage bundleImage];
    note._identityImageHasBorder = NO;
	note.otherButtonTitle = NSLocalizedString(@"Close", @"Close button");
	note.actionButtonTitle = NSLocalizedString(@"Options", @"Options Button");
	
	note.identifier = [[NSUUID new] UUIDString];
    
    note.userInfo = @{@"KickedOut": @1,
					  @"SPECIAL": @1,
                      @"Activate": (__bridge NSString *)kMMPropertyKeychainMRDetailsAEAction,
                      };

    NSLog(@"About to post#-/%lu (KICKOUT): %@", (unsigned long)noteCenter.deliveredNotifications.count, note);
	[appropriateNotificationCenter() deliverNotification:note];
}

-(void)postApplicationReminder
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
	
	NSUserNotification *note = [NSUserNotification new];
	
	note.title = NSLocalizedString(@"iCloud Keychain", @"Title for iCloud Keychain Application still pending (from this device) reminder");
	note.informativeText = NSLocalizedString(@"Approve this Mac from another device to use iCloud Keychain.", @"Body text for iCloud Keychain Application still pending (from this device) reminder");
	
    note._identityImage = [NSImage bundleImage];
    note._identityImageHasBorder = NO;
	note.otherButtonTitle = NSLocalizedString(@"Close", @"Close button");
	note.actionButtonTitle = NSLocalizedString(@"Options", @"Options Button");
	
	note.identifier = [[NSUUID new] UUIDString];
    
    note.userInfo = @{@"ApplicationReminder": @1,
					  @"SPECIAL": @1,
                      @"Activate": (__bridge NSString *)kMMPropertyKeychainWADetailsAEAction,
                      };
	
    NSLog(@"About to post#-/%lu (REMINDER): %@", (unsigned long)noteCenter.deliveredNotifications.count, note);
	[appropriateNotificationCenter() deliverNotification:note];
}

@end
