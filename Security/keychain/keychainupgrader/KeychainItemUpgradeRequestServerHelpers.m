
#import <Foundation/Foundation.h>

#import "utilities/debugging.h"
#import "keychain/keychainupgrader/KeychainItemUpgradeRequestServer.h"
#import "keychain/keychainupgrader/KeychainItemUpgradeRequestServerHelpers.h"

static bool KeychainItemUpgradeRequestServerEnabled = false;
bool KeychainItemUpgradeRequestServerIsEnabled(void) {
    return KeychainItemUpgradeRequestServerEnabled;
}
void KeychainItemUpgradeRequestServerSetEnabled(bool enabled) {
    KeychainItemUpgradeRequestServerEnabled = enabled;
}

void KeychainItemUpgradeRequestServerInitialize(void) {
    secnotice("keychainitemupgrade", "performing KeychainItemUpgradeRequestServerInitialize");
    KeychainItemUpgradeRequestServer* server = [KeychainItemUpgradeRequestServer server];
    
    [server.controller triggerKeychainItemUpdateRPC:^(NSError * _Nullable error) {
        secnotice("keychainitemupgrade", "kicking off keychain item upgrade");
    }];
}
