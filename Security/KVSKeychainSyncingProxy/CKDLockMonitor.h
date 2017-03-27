//
//  CKDLockMonitor.h
//  Security
//

#import "CKDLockMonitor.h"

@protocol CKDLockListener

- (void) unlocked;
- (void) locked;

@end

@protocol CKDLockMonitor

@property (readonly) BOOL unlockedSinceBoot;
@property (readonly) BOOL locked;

- (void)recheck;

- (void)connectTo: (NSObject<CKDLockListener>*) proxy;

@end
