
#ifndef CKKSMockLockStateProvider_h
#define CKKSMockLockStateProvider_h

#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSLockStateTracker.h"

@interface CKKSMockLockStateProvider : NSObject <CKKSLockStateProviderProtocol>
@property BOOL aksCurrentlyLocked;

- (instancetype)initWithCurrentLockStatus:(BOOL)currentlyLocked;
@end

#endif

#endif /* CKKSMockLockStateProvider_h */
