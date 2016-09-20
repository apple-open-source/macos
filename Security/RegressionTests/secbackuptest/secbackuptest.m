//
//  Copyright 2015 - 2016 Apple. All rights reserved.
//

/*
 * This is to fool os services to not provide the Keychain manager
 * interface tht doens't work since we don't have unified headers
 * between iOS and OS X. rdar://23405418/
 */
#define __KEYCHAINCORE__ 1

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include <TargetConditionals.h>

#include <Security/SecItemPriv.h>
#include <sys/stat.h>
#include <err.h>

#if TARGET_OS_SIMULATOR
int
main(void)
{
    return 0;
}
#else

#include <libaks.h>

static NSData *keybag = NULL;
static NSString *keybaguuid = NULL;

static void
BagMe(void)
{
    keybag_handle_t handle;
    kern_return_t result;
    char uuidstr[37];
    uuid_t uuid;
    void *data = NULL;
    int length;

    result = aks_create_bag("foo", 3, kAppleKeyStoreAsymmetricBackupBag, &handle);
    if (result)
        errx(1, "aks_create_bag: %08x", result);

    result = aks_save_bag(handle, &data, &length);
    if (result)
        errx(1, "aks_save_bag");

    result = aks_get_bag_uuid(handle, uuid);
    if (result)
        errx(1, "aks_get_bag_uuid");

    uuid_unparse_lower(uuid, uuidstr);

    keybaguuid = [NSString stringWithUTF8String:uuidstr];
    keybag = [NSData dataWithBytes:data length:length];
}

int main (int argc, const char * argv[])
{
    @autoreleasepool {
        NSData *password = NULL;
        CFErrorRef error = NULL;
        NSString *uuid = NULL;

        BagMe();
        password = [NSData dataWithBytes:"foo" length:3];

        NSData *backup = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, (__bridge CFDataRef)password));
        if (backup == NULL) {
            errx(1, "backup failed");
        }

        char path[] = "/tmp/secbackuptestXXXXXXX";
        int fd = mkstemp(path);

        bool status = _SecKeychainWriteBackupToFileDescriptor((__bridge CFDataRef)keybag, (__bridge CFDataRef)password, fd, &error);
        if (!status) {
            NSLog(@"backup failed: %@", error);
            errx(1, "failed backup 2");
        }

        uuid = CFBridgingRelease(_SecKeychainCopyKeybagUUIDFromFileDescriptor(fd, &error));
        if (uuid == NULL) {
            NSLog(@"getting uuid failed failed: %@", error);
            errx(1, "failed getting uuid");
        }

        if (![uuid isEqual:keybaguuid]) {
            NSLog(@"getting uuid failed failed: %@ vs %@", uuid, keybaguuid);
            errx(1, "failed compare uuid");
        }

        struct stat sb;
        fstat(fd, &sb);

        if (sb.st_size != (off_t)[backup length])
            warn("backup different ");

        if (abs((int)(sb.st_size - (off_t)[backup length])) > 1000)
            errx(1, "backup different enough to fail");

        status = _SecKeychainRestoreBackupFromFileDescriptor(fd, (__bridge CFDataRef)keybag, (__bridge CFDataRef)password, &error);
        if (!status) {
            NSLog(@"restore failed: %@", error);
            errx(1, "restore failed");
        }

        close(fd);
        unlink(path);

        NSData *backup2 = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, (__bridge CFDataRef)password));
        if (backup2 == NULL) {
            errx(1, "backup 3 failed");
        }

        if (abs((int)(sb.st_size - (off_t)[backup2 length])) > 1000)
            errx(1, "backup different enough to fail (mem vs backup2): %d vs %d", (int)sb.st_size, (int)[backup2 length]);
        if (abs((int)([backup length] - [backup2 length])) > 1000)
            errx(1, "backup different enough to fail (backup1 vs backup2: %d vs %d", (int)[backup length], (int)[backup2 length]);

        return 0;
    }
}

#endif /* TARGET_OS_SIMULATOR */

