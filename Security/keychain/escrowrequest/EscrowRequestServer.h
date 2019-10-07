
#import <Foundation/Foundation.h>
#import <CloudServices/SecureBackup.h>
#import <CoreCDP/CDPStateController.h>
#import "keychain/escrowrequest/EscrowRequestXPCProtocol.h"
#import "keychain/escrowrequest/EscrowRequestController.h"

@class SecEscrowPendingRecord;
@class CKKSLockStateTracker;

NS_ASSUME_NONNULL_BEGIN

extern NSString* ESRPendingSince;

// For securityd->securityd communication, the EscrowRequestServer can pretend to be SecEscrowRequest
@interface EscrowRequestServer : NSObject <EscrowRequestXPCProtocol, SecEscrowRequestable>
@property EscrowRequestController* controller;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLockStateTracker:(CKKSLockStateTracker*)lockStateTracker;

+ (EscrowRequestServer*)server;

@end

NS_ASSUME_NONNULL_END
