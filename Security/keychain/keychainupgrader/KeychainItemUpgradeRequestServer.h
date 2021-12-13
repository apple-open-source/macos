
#import <Foundation/Foundation.h>
#import "keychain/keychainupgrader/KeychainItemUpgradeRequestController.h"

@class CKKSLockStateTracker;

NS_ASSUME_NONNULL_BEGIN

@interface KeychainItemUpgradeRequestServer : NSObject
@property KeychainItemUpgradeRequestController* controller;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLockStateTracker:(CKKSLockStateTracker*)lockStateTracker;

+ (KeychainItemUpgradeRequestServer*)server;

@end

NS_ASSUME_NONNULL_END
