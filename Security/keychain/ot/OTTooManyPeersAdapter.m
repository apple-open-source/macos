#if OCTAGON

#import "OTTooManyPeersAdapter.h"

#import <Security/SecInternalReleasePriv.h>
#import "utilities/debugging.h"
#import <os/feature_private.h>
#import <os/variant_private.h>

@implementation OTTooManyPeersActualAdapter

- (BOOL)shouldPopDialog
{
#if TARGET_OS_IOS
    static bool popDialogFF = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        popDialogFF = os_feature_enabled(Security, WarnTooManyPeers);
        secnotice("octagon", "popdialog: WarnTooManyPeers is %@ (via feature flags)", popDialogFF ? @"enabled" : @"disabled");
    });
    if (!popDialogFF) {
        return false;
    }

    return os_variant_has_internal_ui("iCloudKeychain");
#else
    return false;
#endif
}

- (unsigned long)getLimit
{
    return 50;
}

- (void)popDialogWithCount:(unsigned long)count limit:(unsigned long)limit
{
#if TARGET_OS_IOS
    // During Buddy, CFUserNotification immediately results in the "cancel" button result from CFUserNotificationReceiveResponse.
    // Should the user log into iCloud during Buddy, we want to pop the dialog later, at the home screen.
    // But we also don't want to loop forever, so only try 250 times, with 5 second delay between (total ~20 minutes).
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
        NSDictionary *alertOptions = @{
            (NSString *)kCFUserNotificationAlertMessageKey : [NSString stringWithFormat:@"AppleInternal only:\nYour account has %@ keychain devices, over the recommended performance limit of %@. Please see:\nhttps://at.apple.com/keychain-help", @(count), @(limit)],
            (NSString *)kCFUserNotificationAlertHeaderKey : @"UserSecrets: Keychain",
            (NSString *)kCFUserNotificationAlertTopMostKey: @YES,
        };

        for (unsigned int attempts = 250; attempts != 0; attempts--) {
            SInt32 errNote = 0;
            CFUserNotificationRef notification = CFUserNotificationCreate(NULL, 0, kCFUserNotificationPlainAlertLevel, &errNote, (__bridge CFDictionaryRef)alertOptions);
            if (notification != NULL) {
                secnotice("octagon", "popdialog: CFNotification %p %d", notification, (int)errNote);
                CFOptionFlags responseFlags = 0;
                errNote = CFUserNotificationReceiveResponse(notification, 0, &responseFlags);
                secnotice("octagon", "popdialog: user responded %d %d", (int)errNote, (int)responseFlags);
                CFRelease(notification);
                if (errNote == 0 && (responseFlags & 0x03) == kCFUserNotificationDefaultResponse) {
                    break;
                }
            } else {
                secnotice("octagon", "popdialog: Failed to create notification %d\n", (int)errNote);
                break;
            }

            sleep(5);
        }
    });
#endif
}

@end

#endif // OCTAGON
