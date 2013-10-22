//
//  KNAppDelegate.h
//  Keychain Circle Notification
//
//  Created by J Osborne on 2/21/13.
//
//

#import <Cocoa/Cocoa.h>
#import <Foundation/NSUserNotification_Private.h>
#import "KNPersistantState.h"

@class KDSecCircle;

@interface KNAppDelegate : NSObject <NSApplicationDelegate, _NSUserNotificationCenterDelegatePrivate>

@property (assign) IBOutlet NSWindow *window;
@property (retain) KDSecCircle *circle;
@property (retain) NSMutableSet *viewedIds;
@property (retain) KNPersistantState *state;

@end
