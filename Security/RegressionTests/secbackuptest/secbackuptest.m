//
//  Copyright 2015 - 2016 Apple. All rights reserved.
//

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include <TargetConditionals.h>

#include <Security/SecItemPriv.h>
#include <sys/stat.h>
#include <err.h>

#if TARGET_OS_SIMULATOR || TARGET_OS_BRIDGE
int
main(void)
{
    return 0;
}
#else

#include <AppleKeyStore/libaks.h>

static NSData *keybag = NULL;
static NSString *keybaguuid = NULL;
#define PASSWORD "foo"

static void
BagMe(keybag_type_t bag_type)
{
    keybag_handle_t handle;
    kern_return_t result;
    char uuidstr[37];
    uuid_t uuid;
    void *data = NULL;
    int length;

    result = aks_create_bag(PASSWORD, strlen(PASSWORD), bag_type, &handle);
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

static int doit(bool require_password_for_backup) {
    @autoreleasepool {
        CFErrorRef error = NULL;
        NSString *uuid = NULL;
        NSData *password = [NSData dataWithBytes:PASSWORD length:strlen(PASSWORD)];

        NSData *backup = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, require_password_for_backup ? (__bridge CFDataRef)password : NULL));
        if (backup == NULL) {
            errx(1, "backup failed");
        }

        char path[] = "/tmp/secbackuptestXXXXXXX";
        int fd = mkstemp(path);
        if (fd < 0) {
            errx(1, "mkstmp failed");
        }

        bool status = _SecKeychainWriteBackupToFileDescriptor((__bridge CFDataRef)keybag, require_password_for_backup ? (__bridge CFDataRef)password : NULL, fd, &error);
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
        if (fstat(fd, &sb) == -1) {
            err(1, "fstat");
        }

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

int main (int argc, const char * argv[])
{
    BagMe(kAppleKeyStoreAsymmetricBackupBag);
    int retValAsym = doit(false);

    BagMe(kAppleKeyStoreBackupBag);
    int retValSym = doit(true);

    return retValAsym ? retValAsym : retValSym;
}

#endif /* TARGET_OS_SIMULATOR */

