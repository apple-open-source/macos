
#if OCTAGON

#import "keychain/ckks/CKKSKeychainViewState.h"

@interface CKKSKeychainViewState ()
@property OctagonStateMachine* zoneStateMachine;
@end

@implementation CKKSKeychainViewState

- (instancetype)initWithZoneID:(CKRecordZoneID*)zoneID
              viewStateMachine:(OctagonStateMachine*)stateMachine
    notifyViewChangedScheduler:(CKKSNearFutureScheduler*)notifyViewChangedScheduler
      notifyViewReadyScheduler:(CKKSNearFutureScheduler*)notifyViewReadyScheduler
{
    if((self = [super init])) {
        _zoneName = zoneID.zoneName;
        _zoneID = zoneID;

        _zoneStateMachine = stateMachine;

        _notifyViewChangedScheduler = notifyViewChangedScheduler;
        _notifyViewReadyScheduler = notifyViewReadyScheduler;
    }
    return self;
}

- (CKKSZoneKeyState*)zoneCKKSState
{
    return self.zoneStateMachine.currentState;
}

@end

#endif
