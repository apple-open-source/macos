#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#include <err.h>

static void usage(void) __dead2;
static bool create_item(NSString *acct);
static bool verify_item(NSString *acct, bool deleteit);
static void initial_state(void) __dead2;
static uint64_t update_state(void);
static void reset(void) __dead2;

static NSString *kCanaryAccessGroup = @"com.apple.security.test.canary";
static NSString *kCanaryStateAccount = @"com.apple.security.test.canaryState";

int
main(int argc, char *argv[])
{
    int ch;
    uint64_t iter;
    bool success;

    iter = 0;
    while ((ch = getopt(argc, argv, "ir")) != -1) {
        switch (ch) {
        case 'i':
            initial_state();
            /*notreached*/
        case 'r':
            reset();
            /*notreached*/
        default:
            usage();
            /*notreached*/
        }
    }

    iter = update_state();
    fprintf(stderr, "iter = %llu\n\n", iter);

    @autoreleasepool {
        if (iter > 0) {
            printf("[TEST] Verify and delete previous canary item\n");
            success = verify_item([NSString stringWithFormat:@"canary%llu", iter - 1], true);
            printf("[%s]\n", success ? "PASS" : "FAIL");
            fprintf(stderr, "\n");
        }

        printf("[TEST] Create canary item\n");
        success = create_item([NSString stringWithFormat:@"canary%llu", iter]);
        printf("[%s]\n", success ? "PASS" : "FAIL");
    }
}

static void
usage(void)
{

    fprintf(stderr, "usage: secitemcanarytest -i        Generate initial state\n"
                    "       secitemcanarytest           Normal operation\n"
                    "       secitemcanarytest -r        Reset everything\n");
    exit(1);
}

static bool
create_item(NSString *acct)
{
    OSStatus status;
    NSDictionary *attrs;
    int nerrors = 0;

    attrs = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"secitemcanarytest-oneItem",
        (id)kSecAttrAccount : acct,
        (id)kSecAttrAccessGroup : kCanaryAccessGroup,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
        (id)kSecValueData : [NSData dataWithBytes:"password" length: 8],
    };
    status = SecItemAdd((__bridge CFDictionaryRef)attrs, NULL);
    if (status != 0) {
        nerrors++;
        fprintf(stderr, "SecItemAdd(%s): %d\n", acct.UTF8String, status);
    } else {
        printf("created: %s\n", acct.UTF8String);
    }

    if (!verify_item(acct, false)) {
        nerrors++;
    }

    return (nerrors == 0);
}

static bool
verify_item(NSString *acct, bool deleteit)
{
    OSStatus status;
    NSDictionary *query;
    CFTypeRef result;
    int nerrors = 0;

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kCanaryAccessGroup,
        (id)kSecAttrAccount : acct,
        (id)kSecReturnAttributes : @YES,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
    };
    result = NULL;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status != 0) {
        nerrors++;
        fprintf(stderr, "SecItemCopyMatching(%s): %d\n", acct.UTF8String, status);
    } else {
        if (CFGetTypeID(result) != CFArrayGetTypeID()) {
            nerrors++;
            fprintf(stderr, "SecItemCopyMatching(%s): not array\n", acct.UTF8String);
        } else if (CFArrayGetCount(result) != 1) {
            nerrors++;
            fprintf(stderr, "SecItemCopyMatching(%s): incorrect number of results\n", acct.UTF8String);
        } else {
            printf("verified: %s\n", acct.UTF8String);
        }
        CFRelease(result);
    }

    if (deleteit) {
        status = SecItemDelete((__bridge CFDictionaryRef)query);
        if (status != 0) {
            nerrors++;
            fprintf(stderr, "SecItemDelete(%s): %d\n", acct.UTF8String, status);
        } else {
            printf("deleted: %s\n", acct.UTF8String);
        }
    }

    return (nerrors == 0);
}

static void
initial_state(void)
{
    OSStatus status;
    uint64_t state;
    NSDictionary *attrs;

    state = 0;
    attrs = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"canary test state",
        (id)kSecAttrAccount : kCanaryStateAccount,
        (id)kSecAttrAccessGroup : kCanaryAccessGroup,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
        (id)kSecValueData : [NSData dataWithBytes:&state length:sizeof(state)],
    };
    status = SecItemAdd((__bridge CFDictionaryRef)attrs, NULL);
    switch (status) {
    case 0:
        exit(0);
        /*notreached*/
    default:
        errx(1, "SecItemAdd: %d", status);
        /*notreached*/
    }
}

static uint64_t
update_state(void)
{
    OSStatus status;
    NSMutableDictionary *query;
    NSDictionary *update;
    CFTypeRef result;
    uint64_t state = 0, next_state;

    query = [NSMutableDictionary dictionary];
    query[(id)kSecClass] = (id)kSecClassGenericPassword;
    query[(id)kSecAttrAccessGroup] = kCanaryAccessGroup;
    query[(id)kSecAttrAccount] = kCanaryStateAccount;
    query[(id)kSecUseDataProtectionKeychain] = (id)kCFBooleanTrue;
    query[(id)kSecReturnData] = @YES;

    status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    switch (status) {
    case 0:
        if (result != NULL && CFGetTypeID(result) == CFDataGetTypeID() && CFDataGetLength(result) == sizeof(state)) {
            memcpy(&state, CFDataGetBytePtr(result), sizeof(state));
        } else {
            errx(1, "invalid state");
            /*notreached*/
        }
        break;
    default:
        errx(1, "failed to retrieve state: SecItemCopyMatching(state): %d", status);
        /*notreached*/
    }

    next_state = state + 1;

    query[(id)kSecReturnData] = nil;
    update = @{
        (id)kSecValueData : [NSData dataWithBytes:&next_state length:sizeof(next_state)],
    };
    status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update);

    if (status != 0) {
        errx(1, "SecItemUpdate: %d", status);
    }

    return state;
}

static void
reset(void)
{
    OSStatus status;
    NSDictionary *query;

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : kCanaryAccessGroup,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
    };
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    switch (status) {
    case 0:
    case errSecItemNotFound:
        exit(0);
        /*notreached*/
    default:
        errx(1, "SecItemDelete: %d", status);
        /*notreached*/
    }
}
