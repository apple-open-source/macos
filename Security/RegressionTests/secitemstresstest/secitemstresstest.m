//
//  Copyright 2016 Apple. All rights reserved.
//

/*
 * This is to fool os services to not provide the Keychain manager
 * interface tht doens't work since we don't have unified headers
 * between iOS and OS X. rdar://23405418/
 */
#define __KEYCHAINCORE__ 1

#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#include <err.h>

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
        (id)kSecAttrNoLegacy : (id)kCFBooleanTrue,
    };
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess || status == errSecItemNotFound)
        printf("cleanup ag1: %d\n", (int)status);

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kAccessGroup2,
        (id)kSecAttrNoLegacy : (id)kCFBooleanTrue,
    };
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess || status != errSecItemNotFound)
        printf("cleanup ag2: %d\n", (int)status);
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
        (id)kSecAttrNoLegacy : (id)kCFBooleanTrue,
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

    status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)values);
    isPedestrian("SecItemUpdate", status, ignorePedestrianFailures);

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
        printf("[TEST] testing serial items\n");

        CreateDeleteItem(@"account1", kAccessGroup1, false);
        CreateDeleteItem(@"account2", kAccessGroup1, false);
        CreateDeleteItem(@"account1", kAccessGroup2, false);
        CreateDeleteItem(@"account2", kAccessGroup2, false);
        printf("[PASS]\n");

        Cleanup();
        printf("[TEST] testing concurrent items\n");

        CreateDeleteConcurrentItems(2);
        CreateDeleteConcurrentItems(10);
        printf("[PASS]\n");

        Cleanup();
        printf("[TEST] testing concurrent same item\n");

        CreateDeleteConcurrentSameItem(2);
        CreateDeleteConcurrentSameItem(10);
        printf("[PASS]\n");


        return 0;
    }
}


