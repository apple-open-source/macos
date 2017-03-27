//
//  CKDAKSLockMonitor.h
//  Security
//


#import "CKDLockMonitor.h"
#import "XPCNotificationDispatcher.h"

#import <Foundation/Foundation.h>

@interface CKDAKSLockMonitor : NSObject<CKDLockMonitor, XPCNotificationListener>

@property (readonly) BOOL unlockedSinceBoot;
@property (readonly) BOOL locked;

@property (weak) NSObject<CKDLockListener>* listener;

+ (instancetype) monitor;

- (instancetype) init;

- (void) recheck;

- (void) connectTo: (NSObject<CKDLockListener>*) listener;

@end
