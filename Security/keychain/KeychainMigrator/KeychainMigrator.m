#import <Foundation/Foundation.h>
#import <DataMigration/DataMigration.h>
#import <os/log.h>

#import "KeychainMigrator.h"

#import <Security/SecItemPriv.h>

@implementation KeychainMigrator : DataClassMigrator

- (BOOL)performMigration
{
    bool upgradeInstall = false;
    bool eraseInstall = false;

    if ((self.userDataDisposition & kkDMUserDataDispositionErase) == kkDMUserDataDispositionErase) {
        eraseInstall = true;
    }

    if ((self.userDataDisposition & kkDMUserDataDispositionUpgrade) == kkDMUserDataDispositionUpgrade) {
        upgradeInstall = true;
    }

    if (!eraseInstall && !upgradeInstall) {
        os_log(OS_LOG_DEFAULT, "Skipping keychain migration erase:%d upgrade:%d", eraseInstall, upgradeInstall);
        return true;
    }

    os_log(OS_LOG_DEFAULT, "Performing keychain migration");
    OSStatus status = _SecKeychainForceUpgradeIfNeeded();

    if (status != noErr) {
        os_log(OS_LOG_DEFAULT, "Failed to perform keychain migration: %d", (int)status);
        return false;
    }

    return true;
}

@end
