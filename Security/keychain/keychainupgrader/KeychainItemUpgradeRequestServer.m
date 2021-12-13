
#import "utilities/debugging.h"

#import "keychain/keychainupgrader/KeychainItemUpgradeRequestController.h"
#import "keychain/keychainupgrader/KeychainItemUpgradeRequestServer.h"

#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSAnalytics.h"

@implementation KeychainItemUpgradeRequestServer

- (instancetype)initWithLockStateTracker:(CKKSLockStateTracker*)lockStateTracker
{
    if((self = [super init])) {
        _controller = [[KeychainItemUpgradeRequestController alloc] initWithLockStateTracker:lockStateTracker];
    }
    return self;
}

+ (KeychainItemUpgradeRequestServer*)server
{
    static KeychainItemUpgradeRequestServer* server;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        server = [[KeychainItemUpgradeRequestServer alloc] initWithLockStateTracker:[CKKSLockStateTracker globalTracker]];
    });
    return server;
}

@end
