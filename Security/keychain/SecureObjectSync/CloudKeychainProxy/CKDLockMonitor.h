//
//  CKDLockMonitor.h
//  Security
//

#import "CKDLockMonitor.h"

@protocol CKDLockListener <NSObject>

- (void) unlocked;
- (void) locked;

@end

@protocol CKDLockMonitor <NSObject>

@property (readonly) BOOL unlockedSinceBoot;
@property (readonly) BOOL locked;

- (void)recheck;

- (void)connectTo: (NSObject<CKDLockListener>*) proxy;

@end
