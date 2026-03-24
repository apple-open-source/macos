//
//  Copyright 2016 Apple. All rights reserved.
//

#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#import <Security/SecureObjectSync/SOSCloudCircle.h>
#include <err.h>
#include <TargetConditionals.h>

#if !(TARGET_OS_SIMULATOR || TARGET_OS_BRIDGE)
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
#endif

static void
fail(const char *fmt, ...) __printflike(1, 2) __attribute__((noreturn));


static void
fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("[FAIL] ");
    verrx(1, fmt, ap);
    va_end(ap);
}

static NSString *kAccessGroup1 = @"keychain-test1";
static NSString *kAccessGroup2 = @"keychain-test2";

static void
Cleanup(void)
{
    NSDictionary *query;
    OSStatus status;

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroup1,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
    };
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess) {
        printf("cleanup ag1: %d\n", (int)status);
    }

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroup2,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
    };
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess) {
        printf("cleanup ag2: %d\n", (int)status);
    }
}

static void
isPedestrian(const char *name, OSStatus status, bool ignorePedestrianFailures)
{
    if (!ignorePedestrianFailures) {
        if (status == errSecSuccess)
            return;
    } else {
        switch(status) {
            case errSecSuccess:
            case errSecItemNotFound:
            case errSecDuplicateItem:
                return;
            default:
                break;
        }
    }
    fail("[FAIL] %s non pedestrian error: %d", name, (int)status);
}

static void
CreateDeleteItem(NSString *account, NSString *accessGroup, bool ignorePedestrianFailures)
{
    NSDictionary *query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"secitemstresstest-oneItem",
        (id)kSecAttrAccount : account,
        (id)kSecAttrAccessGroup : accessGroup,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
        (id)kSecValueData : [NSData dataWithBytes:"password" length: 8],
    };
    OSStatus status;

    status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
    isPedestrian("SecItemAdd", status, ignorePedestrianFailures);
    
    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : account,
        (id)kSecAttrAccessGroup : accessGroup,
    };
    NSDictionary *values = @{
        (id)kSecAttrLabel : @"kaka",
    };
    CFTypeRef result = NULL;
    NSMutableDictionary *findQuery = [query mutableCopy];
    findQuery[(id)kSecReturnAttributes] =  @YES;
    findQuery[(id)kSecReturnData] =  @YES;
    findQuery[(id)kSecUseDataProtectionKeychain] = (id)kCFBooleanTrue;
    NSDictionary *resultDict;
    
    if(!ignorePedestrianFailures) {
        status = SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result);
        resultDict = CFBridgingRelease(result);
        isPedestrian("SecItemCopyMatching", status, ignorePedestrianFailures);
        if (!([resultDict[(id)kSecAttrLabel]  isEqual: @"secitemstresstest-oneItem"])) {
            fail("[FAIL] result not contain correct item label %s", "secitemstresstest-oneItem");
        }
    }
    
    status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)values);
    isPedestrian("SecItemUpdate", status, ignorePedestrianFailures);
    
    if (!ignorePedestrianFailures) {
        result = NULL;
        status = SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result);
        resultDict = CFBridgingRelease(result);
        isPedestrian("SecItemCopyMatching", status, ignorePedestrianFailures);
        if (!([resultDict[(id)kSecAttrLabel]  isEqual: @"kaka"])) {
            fail("[FAIL] result not contain correct item label %s", "kaka");
        }
    }
    
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    isPedestrian("SecItemDelete", status, ignorePedestrianFailures);
}

#define CONCURRENT_RUNTIME 20

static void
CreateDeleteConcurrentItems(int width)
{
    dispatch_semaphore_t sema;
    dispatch_group_t group;
    dispatch_queue_t q, labelQueue;
    int iter = 0;
    time_t old;
    __block unsigned long label = 0;

    q = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    labelQueue = dispatch_queue_create("label-queue", NULL);
    sema = dispatch_semaphore_create(width);
    group = dispatch_group_create();


    old = time(NULL);

    while (time(NULL) - old < CONCURRENT_RUNTIME) {
        size_t number = 10;

        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

        printf("iteration: %d\n", ++iter);

        dispatch_group_async(group, q, ^{
            dispatch_group_t inner = dispatch_group_create();
            if (inner == NULL) abort();
            __block unsigned long me;

            dispatch_sync(labelQueue, ^{
                me = label++;
                if (label == 0) abort();
            });

            dispatch_group_async(inner, q, ^{
                dispatch_apply(number, q, ^(size_t num) {
                    NSString *account = [NSString stringWithFormat:@"account1-%lu-%lu", me, (unsigned long)num];
                    CreateDeleteItem(account, kAccessGroup1, false);
                });
            });
            dispatch_group_async(inner, q, ^{
                dispatch_apply(number, q, ^(size_t num) {
                    NSString *account = [NSString stringWithFormat:@"account2-%lu-%lu", me, (unsigned long)num];
                    CreateDeleteItem(account, kAccessGroup1, false);
                });
            });
            dispatch_group_async(inner, q, ^{
                dispatch_apply(number, q, ^(size_t num) {
                    NSString *account = [NSString stringWithFormat:@"account1-%lu-%lu", me, (unsigned long)num];
                    CreateDeleteItem(account, kAccessGroup2, false);
                });
            });
            dispatch_group_async(inner, q, ^{
                dispatch_apply(number, q, ^(size_t num) {
                    NSString *account = [NSString stringWithFormat:@"account2-%lu-%lu", me, (unsigned long)num];
                    CreateDeleteItem(account, kAccessGroup2, false);
                });
            });
            
            dispatch_group_wait(inner, DISPATCH_TIME_FOREVER);
            dispatch_semaphore_signal(sema);
        });
    }
    
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
}

static void
CreateDeleteBackupConcurrentItemsMultiQueue(int width)
{
    dispatch_semaphore_t sema;
    dispatch_group_t group;
    dispatch_queue_t labelQueue;
    int iter = 0;
    time_t old;
    __block unsigned long label = 0;

    dispatch_queue_t queue0 = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
    dispatch_queue_t queue1 = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_queue_t queue2 = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0);
    dispatch_queue_t queue3 = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0);
    dispatch_queue_t queues[4] = {queue0, queue1, queue2, queue3};
    labelQueue = dispatch_queue_create("label-queue", NULL);
    sema = dispatch_semaphore_create(width);
    group = dispatch_group_create();

    old = time(NULL);

    while (time(NULL) - old < CONCURRENT_RUNTIME) {
        size_t number = 10;

        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

        printf("iteration: %d\n", ++iter);

        dispatch_group_async(group, queues[iter % 4], ^{
            dispatch_group_t inner = dispatch_group_create();
            if (inner == NULL) abort();
            __block unsigned long me;

            dispatch_sync(labelQueue, ^{
                me = label++;
                if (label == 0) abort();
            });

            dispatch_group_async(inner, queue0, ^{
                dispatch_apply(number, queue0, ^(size_t num) {
                    NSString *account = [NSString stringWithFormat:@"account1-%lu-%lu", me, (unsigned long)num];
                    CreateDeleteItem(account, kAccessGroup1, false);
                });
            });
            dispatch_group_async(inner, queue1, ^{
                dispatch_apply(number, queue1, ^(size_t num) {
                    NSString *account = [NSString stringWithFormat:@"account2-%lu-%lu", me, (unsigned long)num];
                    CreateDeleteItem(account, kAccessGroup1, false);
                });
            });
            dispatch_group_async(inner, queue2, ^{
                dispatch_apply(number, queue2, ^(size_t num) {
                    NSString *account = [NSString stringWithFormat:@"account1-%lu-%lu", me, (unsigned long)num];
                    CreateDeleteItem(account, kAccessGroup2, false);
                });
            });
            dispatch_group_async(inner, queue3, ^{
                dispatch_apply(number, queue3, ^(size_t num) {
                    NSString *account = [NSString stringWithFormat:@"account2-%lu-%lu", me, (unsigned long)num];
                    CreateDeleteItem(account, kAccessGroup2, false);
                });
            });

#if !(TARGET_OS_SIMULATOR || TARGET_OS_BRIDGE)
            // Perform backup with password on queue0
            dispatch_group_async(inner, queue2, ^{
                NSData *password = [NSData dataWithBytes:PASSWORD length:strlen(PASSWORD)];
                NSData *backup = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, (__bridge CFDataRef)password));
                if (backup == NULL) {
                    printf("backup with password failed in iteration %d\n", iter);
                }
            });

            // Perform backup without password on queue1
            dispatch_group_async(inner, queue3, ^{
                NSData *backup = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, NULL));
                if (backup == NULL) {
                    printf("backup without password failed in iteration %d\n", iter);
                }
            });

            // Check SOS trust and syncing on queue0
            dispatch_group_async(inner, queue0, ^{
                dispatch_apply(number, queue0, ^(size_t num) {
                    bool sosEnabled = SOSCCIsSOSTrustAndSyncingEnabled();
                    printf("SOS trust and syncing enabled first: %d\n", sosEnabled);
                });
            });

            // Check SOS trust and syncing on queue1
            dispatch_group_async(inner, queue1, ^{
                dispatch_apply(number, queue1, ^(size_t num) {
                    bool sosEnabled = SOSCCIsSOSTrustAndSyncingEnabled();
                    printf("SOS trust and syncing enabled second: %d\n", sosEnabled);
                });
            });
#endif

            dispatch_group_wait(inner, DISPATCH_TIME_FOREVER);
            dispatch_semaphore_signal(sema);
        });
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
}

static void
CreateDeleteConcurrentSameItem(int width)
{
    dispatch_semaphore_t sema;
    dispatch_group_t group;
    dispatch_queue_t q;
    time_t old;

    q = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    sema = dispatch_semaphore_create(width);
    group = dispatch_group_create();

    old = time(NULL);

    while (time(NULL) - old < CONCURRENT_RUNTIME) {
        size_t number = 10;

        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

        dispatch_group_async(group, q, ^{
            dispatch_apply(number, q, ^(size_t num) {
                CreateDeleteItem(@"account1", kAccessGroup1, true);
            });

            dispatch_semaphore_signal(sema);
        });
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
}



int main (int argc, const char * argv[])
{
    @autoreleasepool {

        Cleanup();
        printf("[TEST] secitemstresstest\n");
        printf("[BEGIN] testing serial items\n");

        CreateDeleteItem(@"account1", kAccessGroup1, false);
        CreateDeleteItem(@"account2", kAccessGroup1, false);
        CreateDeleteItem(@"account1", kAccessGroup2, false);
        CreateDeleteItem(@"account2", kAccessGroup2, false);
        printf("[PASS]\n");

        Cleanup();
        printf("[BEGIN] testing concurrent items\n");

        CreateDeleteConcurrentItems(2);
        CreateDeleteConcurrentItems(10);
        printf("[PASS]\n");
        
        Cleanup();
        printf("[BEGIN] testing concurrent same item\n");

        CreateDeleteConcurrentSameItem(2);
        CreateDeleteConcurrentSameItem(10);
        printf("[PASS]\n");
        
        Cleanup();
        printf("[BEGIN] testing concurrent items with backup and multiple queues\n");

#if !(TARGET_OS_SIMULATOR || TARGET_OS_BRIDGE)
        BagMe(kAppleKeyStoreBackupBag);
#endif
        CreateDeleteBackupConcurrentItemsMultiQueue(2);
        CreateDeleteBackupConcurrentItemsMultiQueue(10);
        printf("[PASS]\n");

        Cleanup();
        
        printf("[SUMMARY]\n");
        printf("test completed\n");

        return 0;
    }
}


