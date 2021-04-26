
#if OCTAGON

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ot/OctagonStateMachine.h"

NS_ASSUME_NONNULL_BEGIN
@class OctagonStateMachine;

@interface CKKSKeychainViewState : NSObject
@property (readonly) NSString* zoneName;
@property (readonly) CKRecordZoneID* zoneID;

@property (readonly) CKKSZoneKeyState* zoneCKKSState;

/* Trigger this to tell the whole machine that this view has changed */
@property CKKSNearFutureScheduler* notifyViewChangedScheduler;

/* Trigger this to tell the whole machine that this view is more ready then before */
@property CKKSNearFutureScheduler* notifyViewReadyScheduler;

- (instancetype)initWithZoneID:(CKRecordZoneID*)zoneID
              viewStateMachine:(OctagonStateMachine*)stateMachine
    notifyViewChangedScheduler:(CKKSNearFutureScheduler*)notifyViewChangedScheduler
      notifyViewReadyScheduler:(CKKSNearFutureScheduler*)notifyViewReadyScheduler;
@end

NS_ASSUME_NONNULL_END

#endif
