#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/tests/CKKSMockLockStateProvider.h"
#import "tests/secdmockaks/mockaks.h"

@implementation CKKSMockLockStateProvider
@synthesize aksCurrentlyLocked = _aksCurrentlyLocked;

- (instancetype)initWithCurrentLockStatus:(BOOL)currentlyLocked
{
    if((self = [super init])) {
        _aksCurrentlyLocked = currentlyLocked;
    }
    return self;
}

- (BOOL)queryAKSLocked {
    return self.aksCurrentlyLocked;
}

- (BOOL)aksCurrentlyLocked {
    return _aksCurrentlyLocked;
}

- (void)setAksCurrentlyLocked:(BOOL)aksCurrentlyLocked
{
    if(aksCurrentlyLocked) {
        [SecMockAKS lockClassA];
    } else {
        [SecMockAKS unlockAllClasses];
    }

    _aksCurrentlyLocked = aksCurrentlyLocked;
}

@end

#endif
