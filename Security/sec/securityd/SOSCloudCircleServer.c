//
//  SOSCloudCircleServer.c
//  sec
//
//  Created by Mitch Adler on 11/15/12.
//
//

#include <AssertMacros.h>
#include <CoreFoundation/CFURL.h>

#include <securityd/SOSCloudCircleServer.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfoInternal.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/debugging.h>
#include "SOSCloudKeychainClient.h"

#include <corecrypto/ccrng.h>
#include <corecrypto/ccrng_pbkdf2_prng.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecFramework.h>

#include <utilities/SecFileLocations.h>
#include <utilities/SecAKSWrappers.h>
#include <SecItemServer.h>
#include <SecItemPriv.h>

#include <TargetConditionals.h>

#include <utilities/iCloudKeychainTrace.h>

#if TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR
#include <MobileGestalt.h>
#else
#include <AppleSystemInfo/AppleSystemInfo.h>

// We need authorization, but that doesn't exist
// on sec built for desktop (iOS in a process)
// Define AuthorizationRef here to make SystemConfiguration work
// as if it's on iOS.
typedef const struct AuthorizationOpaqueRef *	AuthorizationRef;
#endif

#define SOSCKCSCOPE "sync"

#define USE_SYSTEMCONFIGURATION_PRIVATE_HEADERS
#import <SystemConfiguration/SystemConfiguration.h>

#include <notify.h>

CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef error) {
    if (error) {
        secerror("Error putting: %@", error);
        CFReleaseSafe(error);
    }
};

static SOSCCAccountDataSourceFactoryBlock accountDataSourceOverride = NULL;

bool SOSKeychainAccountSetFactoryForAccount(SOSCCAccountDataSourceFactoryBlock block)
{
    accountDataSourceOverride = Block_copy(block);

    return true;
}

static void do_with_account(void (^action)(SOSAccountRef account));


//
// Constants
//
CFStringRef kSOSInternalAccessGroup = CFSTR("com.apple.security.sos");

CFStringRef kSOSAccountLabel = CFSTR("iCloud Keychain Account Meta-data");
CFStringRef kSOSPeerDataLabel = CFSTR("iCloud Peer Data Meta-data");

static CFStringRef accountFileName = CFSTR("PersistedAccount.plist");

static CFDictionaryRef SOSItemCopyQueryForSyncItems(CFStringRef service, bool returnData)
{
    return CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                        kSecClass,           kSecClassGenericPassword,
                                        kSecAttrService,     service,
                                        kSecAttrAccessGroup, kSOSInternalAccessGroup,
                                        kSecReturnData,      returnData ? kCFBooleanTrue : kCFBooleanFalse,
                                        NULL);
}

CFDataRef SOSItemGet(CFStringRef service, CFErrorRef* error)
{
    CFDictionaryRef query = SOSItemCopyQueryForSyncItems(service, true);

    CFDataRef result = NULL;

    OSStatus copyResult = SecItemCopyMatching(query, (CFTypeRef*) &result);

    CFReleaseNull(query);

    if (copyResult != noErr) {
        SecError(copyResult, error, CFSTR("Error %@ reading for service '%@'"), result, service);
        CFReleaseNull(result);
        return NULL;
    }

    if (!isData(result)) {
        SOSCreateErrorWithFormat(kSOSErrorProcessingFailure, NULL, error, NULL, CFSTR("SecItemCopyMatching returned non-data in '%@'"), service);
        return NULL;
    }

    return result;
}

static CFDataRef SOSKeychainCopySavedAccountData()
{
    CFErrorRef error = NULL;
    CFDataRef accountData = SOSItemGet(kSOSAccountLabel, &error);
    if (!accountData)
        secnotice("account", "Failed to load account: %@", error);
    CFReleaseNull(error);

    return accountData;
}


bool SOSItemUpdateOrAdd(CFStringRef service, CFStringRef accessibility, CFDataRef data, CFErrorRef *error)
{
    CFDictionaryRef query = SOSItemCopyQueryForSyncItems(service, false);

    CFDictionaryRef update = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                          kSecValueData,         data,
                                                          kSecAttrAccessible,    accessibility,
                                                          NULL);
    OSStatus saveStatus = SecItemUpdate(query, update);

    if (errSecItemNotFound == saveStatus) {
        CFMutableDictionaryRef add = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, query);
        CFDictionaryForEach(update, ^(const void *key, const void *value) {
            CFDictionaryAddValue(add, key, value);
        });
        saveStatus = SecItemAdd(add, NULL);
        CFReleaseNull(add);
    }

    CFReleaseNull(query);
    CFReleaseNull(update);

    return SecError(saveStatus, error, CFSTR("Error saving %@ to service '%@'"), data, service);
}

static CFStringRef accountStatusFileName = CFSTR("accountStatus.plist");
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>

static const uint8_t* ccder_decode_bool(bool* boolean, const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_BOOLEAN, &payload_size, der, der_end);

    if (NULL == payload || (der_end - payload) < 1 || payload_size != 1) {
        return NULL;
    }

    if (boolean)
        *boolean = (*payload != 0);

    return payload + payload_size;
}

bool SOSCCCircleIsOn_Artifact(void) {
    CFURLRef accountStatusFileURL = SecCopyURLForFileInKeychainDirectory(accountStatusFileName);
    CFDataRef accountStatus = (CFDataRef) CFPropertyListReadFromFile(accountStatusFileURL);
    CFReleaseSafe(accountStatusFileURL);
    bool circle_on = false;

    if (accountStatus && !isData(accountStatus)) {
        CFReleaseNull(accountStatus);
    } else if(accountStatus) {
        size_t size = CFDataGetLength(accountStatus);
        const uint8_t *der = CFDataGetBytePtr(accountStatus);
        const uint8_t *der_p = der;

        const uint8_t *sequence_end;
        der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, der_p, der_p + size);
        der_p = ccder_decode_bool(&circle_on, der_p, sequence_end);
    } else {

    }
    return circle_on;
}


static size_t ccder_sizeof_bool(bool value __unused, CFErrorRef *error)
{
    return ccder_sizeof(CCDER_BOOLEAN, 1);
}


static uint8_t* ccder_encode_bool(bool value, const uint8_t *der, uint8_t *der_end)
{
    uint8_t value_byte = value;

    return ccder_encode_tl(CCDER_BOOLEAN, 1, der,
                           ccder_encode_body(1, &value_byte, der, der_end));
}

static void SOSCCCircleIsOn_SetArtifact(bool account_on) {
    static CFDataRef sLastSavedAccountStatus = NULL;
    CFErrorRef saveError = NULL;
    size_t der_size = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, ccder_sizeof_bool(account_on, NULL));
    uint8_t der[der_size];
    uint8_t *der_end = der + der_size;
    der_end = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                           ccder_encode_bool(account_on, der, der_end));

    CFDataRef accountStatusAsData = CFDataCreate(kCFAllocatorDefault, der_end, der_size);

    require_quiet(accountStatusAsData, exit);
    if (sLastSavedAccountStatus && CFEqual(sLastSavedAccountStatus, accountStatusAsData))  goto exit;

    CFURLRef accountStatusFileURL = SecCopyURLForFileInKeychainDirectory(accountStatusFileName);
    CFPropertyListWriteToFile((CFPropertyListRef) accountStatusAsData, accountStatusFileURL);
    CFReleaseSafe(accountStatusFileURL);

    CFReleaseNull(sLastSavedAccountStatus);
    sLastSavedAccountStatus = accountStatusAsData;
    accountStatusAsData = NULL;

exit:
    CFReleaseNull(saveError);
    CFReleaseNull(accountStatusAsData);
}

static void SOSCCCircleIsOn_UpdateArtifact(SOSCCStatus status)
{
	switch (status) {
		case kSOSCCCircleAbsent:
		case kSOSCCNotInCircle:
			SOSCCCircleIsOn_SetArtifact(false);
			break;
		case kSOSCCInCircle:
		case kSOSCCRequestPending:
			SOSCCCircleIsOn_SetArtifact(true);
			break;
		case kSOSCCError:
		default:
			// do nothing
			break;
	}
}

static void SOSKeychainAccountEnsureSaved(SOSAccountRef account)
{
    static CFDataRef sLastSavedAccountData = NULL;

    CFErrorRef saveError = NULL;
	SOSCCCircleIsOn_UpdateArtifact(SOSAccountIsInCircles(account, NULL));

    CFDataRef accountAsData = SOSAccountCopyEncodedData(account, kCFAllocatorDefault, &saveError);

    require_action_quiet(accountAsData, exit, secerror("Failed to transform account into data, error: %@", saveError));
    require_quiet(!CFEqualSafe(sLastSavedAccountData, accountAsData), exit);

    if (!SOSItemUpdateOrAdd(kSOSAccountLabel, kSecAttrAccessibleAlwaysThisDeviceOnly, accountAsData, &saveError)) {
        secerror("Can't save account: %@", saveError);
        goto exit;
    }

    CFReleaseNull(sLastSavedAccountData);
    sLastSavedAccountData = accountAsData;
    accountAsData = NULL;

exit:
    CFReleaseNull(saveError);
    CFReleaseNull(accountAsData);
}


/*
 Stolen from keychain_sync.c
 */

static bool clearAllKVS(CFErrorRef *error)
{
    return true;
    __block bool result = false;
    const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    SOSCloudKeychainClearAll(processQueue, ^(CFDictionaryRef returnedValues, CFErrorRef cerror)
                             {
                                 result = (cerror != NULL);
                                 dispatch_semaphore_signal(waitSemaphore);
                             });

	dispatch_semaphore_wait(waitSemaphore, finishTime);
    dispatch_release(waitSemaphore);

    return result;
}

static SOSAccountRef SOSKeychainAccountCreateSharedAccount(CFDictionaryRef our_gestalt)
{
    SOSAccountKeyInterestBlock updateKVSKeys = ^(bool getNewKeysOnly, CFArrayRef alwaysKeys, CFArrayRef afterFirstUnlockKeys, CFArrayRef unlockedKeys) {
        CFErrorRef error = NULL;

        if (!SOSCloudKeychainUpdateKeys(getNewKeysOnly, alwaysKeys, afterFirstUnlockKeys, unlockedKeys, &error))
        {
            secerror("Error updating keys: %@", error);
            // TODO: propagate error(s) to callers.
        } else {
            if (CFArrayGetCount(unlockedKeys) == 0) {
                secnotice(SOSCKCSCOPE, "Unlocked keys were empty!");
            }
            // This leaks 3 CFStringRefs in DEBUG builds.
            CFStringRef alwaysKeysDesc = SOSInterestListCopyDescription(alwaysKeys);
            CFStringRef afterFirstUnlockKeysDesc = SOSInterestListCopyDescription(afterFirstUnlockKeys);
            CFStringRef unlockedKeysDesc = SOSInterestListCopyDescription(unlockedKeys);
            secdebug(SOSCKCSCOPE, "Updating interest: always: %@,\nfirstUnlock: %@,\nunlockedKeys: %@",
                     alwaysKeysDesc,
                     afterFirstUnlockKeysDesc,
                     unlockedKeysDesc);
            CFReleaseNull(alwaysKeysDesc);
            CFReleaseNull(afterFirstUnlockKeysDesc);
            CFReleaseNull(unlockedKeysDesc);
        }

        CFReleaseNull(error);
    };

    SOSAccountDataUpdateBlock updateKVS = ^ bool (CFDictionaryRef changes, CFErrorRef *error) {
        SOSCloudKeychainPutObjectsInCloud(changes, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);

        CFStringRef changeDescription = SOSChangesCopyDescription(changes, true);
        secnotice("account", "Keys Sent: %@", changeDescription);
        CFReleaseSafe(changeDescription);

        return true;
    };

    secdebug("account", "Created account");

    CFDataRef savedAccount = SOSKeychainCopySavedAccountData();

    // At this point we might have an account structure from keychain that may or may not match the account we're building this for
    // murf ZZZ we should probably make sure this is a good thing before using it.

    SOSAccountRef account = NULL;

    if (savedAccount) {
        CFErrorRef inflationError = NULL;
        SOSDataSourceFactoryRef factory = accountDataSourceOverride ? accountDataSourceOverride() : SecItemDataSourceFactoryCreateDefault();

        account = SOSAccountCreateFromData(kCFAllocatorDefault, savedAccount, factory, updateKVSKeys, updateKVS, &inflationError);

        if (account)
            SOSAccountUpdateGestalt(account, our_gestalt);
        else
            secerror("Got error inflating account: %@", inflationError);
        CFReleaseNull(inflationError);
    }

    CFReleaseSafe(savedAccount);

    if (!account) {
		// If we get here then we are creating a new accout and so increment the peer count for ourselves.
        SOSDataSourceFactoryRef factory = accountDataSourceOverride ? accountDataSourceOverride() : SecItemDataSourceFactoryCreateDefault();

        account = SOSAccountCreate(kCFAllocatorDefault, our_gestalt, factory, updateKVSKeys, updateKVS);

        if (!account)
            secerror("Got NULL creating account");
    }

    return account;
}

//
// Mark: Gestalt Handling
//

static CFStringRef CopyModelName(void)
{
    static dispatch_once_t once;
    static CFStringRef modelName = NULL;
    dispatch_once(&once, ^{
#if TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR
        modelName = MGCopyAnswer(kMGQDeviceName, NULL);
#else
        modelName = ASI_CopyComputerModelName(FALSE);
#endif
        if (modelName == NULL)
            modelName = CFSTR("Unknown model");
    });
    return CFStringCreateCopy(kCFAllocatorDefault, modelName);
}

static CFStringRef CopyComputerName(SCDynamicStoreRef store)
{
    CFStringRef deviceName = SCDynamicStoreCopyComputerName(store, NULL);
    if (deviceName == NULL) {
        deviceName = CFSTR("Unknown name");
    }
    return deviceName;
}

static CFDictionaryRef GatherDeviceGestalt(SCDynamicStoreRef store, CFArrayRef keys, void *context)
{
    CFStringRef modelName = CopyModelName();
    CFStringRef computerName = CopyComputerName(store);

    CFDictionaryRef gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kPIUserDefinedDeviceName, computerName,
                                                           kPIDeviceModelName,       modelName,
                                                           NULL);
    CFRelease(modelName);
    CFRelease(computerName);

    return gestalt;
}

static void SOSCCProcessGestaltUpdate(SCDynamicStoreRef store, CFArrayRef keys, void *context)
{
    do_with_account(^(SOSAccountRef account) {
        CFDictionaryRef gestalt = GatherDeviceGestalt(store, keys, context);
        if (SOSAccountUpdateGestalt(account, gestalt)) {
            notify_post(kSOSCCCircleChangedNotification);
        }
        CFReleaseSafe(gestalt);
    });
}


static CFDictionaryRef RegisterForGestaltUpdate(dispatch_queue_t queue, void *info)
{
    SCDynamicStoreContext context = { .info = info };
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("com.apple.securityd.cloudcircleserver"), SOSCCProcessGestaltUpdate, &context);
    CFStringRef computerKey = SCDynamicStoreKeyCreateComputerName(NULL);
    CFArrayRef keys = NULL;
    CFDictionaryRef gestalt = NULL;

    if (store == NULL || computerKey == NULL) {
        goto done;
    }
    keys = CFArrayCreate(NULL, (const void **)&computerKey, 1, &kCFTypeArrayCallBacks);
    if (keys == NULL) {
        goto done;
    }
    gestalt = GatherDeviceGestalt(store, keys, info);
    SCDynamicStoreSetNotificationKeys(store, keys, NULL);
    SCDynamicStoreSetDispatchQueue(store, queue);

done:
    if (store) CFRelease(store);
    if (computerKey) CFRelease(computerKey);
    if (keys) CFRelease(keys);
    return gestalt;
}

static void do_with_account(void (^action)(SOSAccountRef account));
static void do_with_account_async(void (^action)(SOSAccountRef account));

static void do_with_account_dynamic(void (^action)(SOSAccountRef account), bool sync) {
    static SOSAccountRef sSharedAccount;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        secdebug(SOSCKCSCOPE, "Account Creation start");

        CFDictionaryRef gestalt = RegisterForGestaltUpdate(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);

        if (!gestalt) {
#if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR
            gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL);
#else
            secerror("Didn't get machine gestalt! This is going to be ugly.");
#endif
        }

        sSharedAccount = SOSKeychainAccountCreateSharedAccount(gestalt);

        CFReleaseSafe(gestalt);

        SOSCCSetThisDeviceDefinitelyNotActiveInCircle(SOSAccountIsInCircles(sSharedAccount, NULL));

        SOSAccountAddChangeBlock(sSharedAccount, ^(SOSCircleRef circle,
                                                   CFArrayRef peer_additions,      CFArrayRef peer_removals,
                                                   CFArrayRef applicant_additions, CFArrayRef applicant_removals) {
            CFErrorRef pi_error = NULL;
            SOSPeerInfoRef me = SOSAccountGetMyPeerInCircle(sSharedAccount, circle, &pi_error);
            if (!me) {
                secerror("Error finding me for change: %@", pi_error);
                CFReleaseNull(pi_error);
            } else {
                CFReleaseSafe(pi_error);

                if (CFArrayContainsValue(peer_additions, CFRangeMake(0, CFArrayGetCount(peer_additions)), me)) {
                    SOSCCSyncWithAllPeers();
                }
            }

            if (CFArrayGetCount(peer_additions) != 0 ||
                CFArrayGetCount(peer_removals) != 0 ||
                CFArrayGetCount(applicant_additions) != 0 ||
                CFArrayGetCount(applicant_removals) != 0) {

                SOSCCSetThisDeviceDefinitelyNotActiveInCircle(SOSAccountIsInCircles(sSharedAccount, NULL));
                notify_post(kSOSCCCircleChangedNotification);
           }
        });

        SOSCloudKeychainSetItemsChangedBlock(^(CFDictionaryRef changes) {
            CFRetainSafe(changes);
            do_with_account_async(^(SOSAccountRef account) {
                CFStringRef changeDescription = SOSChangesCopyDescription(changes, false);
                secdebug(SOSCKCSCOPE, "Received: %@", changeDescription);
                CFReleaseSafe(changeDescription);

                CFErrorRef error = NULL;
                if (!SOSAccountHandleUpdates(account, changes, &error)) {
                    secerror("Error handling updates: %@", error);
                    CFReleaseNull(error);
                    return;
                }
                    CFReleaseNull(error);
                CFReleaseSafe(changes);
            });
        });

    });

    dispatch_block_t do_action_and_save =  ^{
        action(sSharedAccount);
        SOSKeychainAccountEnsureSaved(sSharedAccount);
    };

    if (sync) {
        dispatch_sync(SOSAccountGetQueue(sSharedAccount), do_action_and_save);
    } else {
        dispatch_async(SOSAccountGetQueue(sSharedAccount), do_action_and_save);
    }
}

static void do_with_account_async(void (^action)(SOSAccountRef account)) {
    do_with_account_dynamic(action, false);
}

static void do_with_account(void (^action)(SOSAccountRef account)) {
    do_with_account_dynamic(action, true);
}

#if TARGET_IPHONE_SIMULATOR
#define MKBDeviceUnlockedSinceBoot() true
#endif

static bool do_if_after_first_unlock(CFErrorRef *error, dispatch_block_t action)
{
    bool beenUnlocked = false;
    require_quiet(SecAKSGetHasBeenUnlocked(&beenUnlocked, error), fail);

    require_action_quiet(beenUnlocked, fail,
                         SOSCreateErrorWithFormat(kSOSErrorNotReady, NULL, error, NULL,
                                                  CFSTR("Keybag never unlocked, ask after first unlock")));

    action();
    return true;

fail:
    return false;
}

static bool do_with_account_if_after_first_unlock(CFErrorRef *error, bool (^action)(SOSAccountRef account, CFErrorRef* error))
{
    __block bool action_result = false;

    return do_if_after_first_unlock(error, ^{
        do_with_account(^(SOSAccountRef account) {
            action_result = action(account, error);
        });

    }) && action_result;
}

static bool do_with_account_while_unlocked(CFErrorRef *error, bool (^action)(SOSAccountRef account, CFErrorRef* error))
{
    __block bool action_result = false;

    return SecAKSDoWhileUserBagLocked(error, ^{
        do_with_account(^(SOSAccountRef account) {
            action_result = action(account, error);
        });

    }) && action_result;
}

SOSAccountRef SOSKeychainAccountGetSharedAccount()
{
    __block SOSAccountRef result = NULL;

    do_with_account(^(SOSAccountRef account) {
        result = account;
    });

    return result;
}

//
// Mark: Credential processing
//


bool SOSCCTryUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error)
{
    return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        return SOSAccountTryUserCredentials(account, user_label, user_password, block_error);
    });
}


static bool EnsureFreshParameters(SOSAccountRef account, CFErrorRef *error) {
    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    dispatch_retain(wait_for); // Both this scope and the block own it.

    CFMutableArrayRef keysToGet = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(keysToGet, kSOSKVSKeyParametersKey);

    __block CFDictionaryRef valuesToUpdate = NULL;
    __block bool success = false;

    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        CFStringRef circle_key = SOSCircleKeyCreateWithName(SOSCircleGetName(circle), NULL);
        CFArrayAppendValue(keysToGet, circle_key);
        CFReleaseNull(circle_key);
    });

    secnotice("updates", "***** EnsureFreshParameters *****");

    SOSCloudKeychainSynchronizeAndWait(keysToGet, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {

        if (sync_error) {
            secerror("SOSCloudKeychainSynchronizeAndWait: %@", sync_error);
            if (error) {
                *error = sync_error;
                CFRetainSafe(*error);
            }
        } else {
            secnotice("updates", "SOSCloudKeychainSynchronizeAndWait: results: %@", returnedValues);
            valuesToUpdate = returnedValues;
            CFRetainSafe(valuesToUpdate);
            success = true;
        }

        dispatch_semaphore_signal(wait_for);
        dispatch_release(wait_for);
    });

    dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
    // TODO: Maybe we timeout here... used to dispatch_time(DISPATCH_TIME_NOW, 30ull * NSEC_PER_SEC));
    dispatch_release(wait_for);

    if ((valuesToUpdate) && (account)) {
        if (!SOSAccountHandleUpdates(account, valuesToUpdate, error)) {
            secerror("Freshness update failed: %@", *error);

            success = false;
        }
    }

    CFReleaseNull(valuesToUpdate);
    CFReleaseNull(keysToGet);

    return success;
}

static bool EnsureFreshParameters_once(SOSAccountRef account, CFErrorRef *error) {
    static dispatch_once_t once;
    __block bool retval = false;
    dispatch_once(&once, ^{
        retval = EnsureFreshParameters(account, error);
    });
    return retval;
}

bool SOSCCSetUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error)
{
    return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        if (!EnsureFreshParameters(account, block_error)) {
            secnotice("updates", "EnsureFreshParameters error: %@", *block_error);
            return false;
        }
        if (!SOSAccountAssertUserCredentials(account, user_label, user_password, block_error)) {
            secnotice("updates", "EnsureFreshParameters/SOSAccountAssertUserCredentials error: %@", *block_error);
            return false;
        }

        return true;
    });

}

bool SOSCCCanAuthenticate_Server(CFErrorRef *error)
{
    bool result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        return SOSAccountGetPrivateCredential(account, block_error) != NULL;
    });

    if (!result && error && *error && CFErrorGetDomain(*error) == kSOSErrorDomain) {
        CFIndex code = CFErrorGetCode(*error);
        if (code == kSOSErrorPrivateKeyAbsent || code == kSOSErrorPublicKeyAbsent) {
            CFReleaseNull(*error);
        }
    }

    return result;
}

bool SOSCCPurgeUserCredentials_Server(CFErrorRef *error)
{
    return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        SOSAccountPurgePrivateCredential(account);
        return true;
    });
}


#if USE_BETTER
static bool sAccountInCircleCache = false;

static void do_with_not_in_circle_bool_queue(bool start_account, dispatch_block_t action)
{
    static dispatch_queue_t account_start_queue;
    static dispatch_queue_t not_in_circle_queue;
    static bool account_started = false;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        not_in_circle_queue = dispatch_queue_create("nis queue", DISPATCH_QUEUE_SERIAL);
        account_start_queue = dispatch_queue_create("init nis queue", DISPATCH_QUEUE_SERIAL);;
        account_started = false;
    });

    __block bool done = false;
    dispatch_sync(not_in_circle_queue, ^{
        if (account_started) {
            done = true;
            action();
        }
    });

    if (!done && start_account) {
        dispatch_sync(account_start_queue, ^{
            __block bool do_start = false;
            dispatch_sync(not_in_circle_queue, ^{
                do_start = !account_started;
                account_started = true;
            });
            if (do_start)
                SOSCCThisDeviceIsInCircle(NULL); // Inflate account.
        });

        dispatch_sync(not_in_circle_queue, action);
    }
}
#endif

bool SOSCCThisDeviceDefinitelyNotActiveInCircle()
{
    return !SOSCCCircleIsOn_Artifact();
#if USE_BETTER
    __block bool result = false;
    do_with_not_in_circle_bool_queue(true, ^{
        result = sAccountInCircleCache;
    });

    return result;
#endif
}

void SOSCCSetThisDeviceDefinitelyNotActiveInCircle(SOSCCStatus currentStatus)
{
    SOSCCCircleIsOn_UpdateArtifact(currentStatus);
#if USE_BETTER
    do_with_not_in_circle_bool_queue(false, ^{
        sAccountInCircleCache = notActive;
    });
#endif
}


SOSCCStatus SOSCCThisDeviceIsInCircle_Server(CFErrorRef *error)
{
    __block SOSCCStatus status;

    return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        status = SOSAccountIsInCircles(account, block_error);
        return true;
    }) ? status : kSOSCCError;
}

bool SOSCCRequestToJoinCircle_Server(CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        return SOSAccountJoinCircles(account, block_error);
    });
}

bool SOSCCRequestToJoinCircleAfterRestore_Server(CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        return SOSAccountJoinCirclesAfterRestore(account, block_error);
    });
}

bool SOSCCResetToOffering_Server(CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        clearAllKVS(NULL);
        return SOSAccountResetToOffering(account, block_error);
    });
}

bool SOSCCResetToEmpty_Server(CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        return SOSAccountResetToEmpty(account, block_error);
    });
}

bool SOSCCRemoveThisDeviceFromCircle_Server(CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        bool result = SOSAccountLeaveCircles(account, block_error);
        return result;
    });
}

bool SOSCCBailFromCircle_Server(uint64_t limit_in_seconds, CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        return SOSAccountBail(account, limit_in_seconds, block_error);
    });
}

CFArrayRef SOSCCCopyApplicantPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        result = SOSAccountCopyApplicants(account, block_error);
        return result != NULL;
    });

    return result;
}


bool SOSCCAcceptApplicants_Server(CFArrayRef applicants, CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        return SOSAccountAcceptApplicants(account, applicants, block_error);
    });
}

bool SOSCCRejectApplicants_Server(CFArrayRef applicants, CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        return SOSAccountRejectApplicants(account, applicants, block_error);
    });
}

CFArrayRef SOSCCCopyPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        result = SOSAccountCopyPeers(account, block_error);
        return result != NULL;
    });

    return result;
}

CFArrayRef SOSCCCopyConcurringPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        result = SOSAccountCopyConcurringPeers(account, block_error);
        return result != NULL;
    });

    return result;
}

CFStringRef SOSCCCopyIncompatibilityInfo_Server(CFErrorRef* error)
{
    __block CFStringRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        result = SOSAccountCopyIncompatibilityInfo(account, block_error);
        return result != NULL;
    });

    return result;
}

enum DepartureReason SOSCCGetLastDepartureReason_Server(CFErrorRef* error)
{
    __block enum DepartureReason result = kSOSDepartureReasonError;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        EnsureFreshParameters_once(account, NULL);
        result = SOSAccountGetLastDepartureReason(account, block_error);
        return result != kSOSDepartureReasonError;
    });

    return result;
}

SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers_Server(CFErrorRef* error)
{
    /*
     #define kIOReturnLockedRead      iokit_common_err(0x2c3) // device read locked
     #define kIOReturnLockedWrite     iokit_common_err(0x2c4) // device write locked
    */
    __block SyncWithAllPeersReason result = kSyncWithAllPeersSuccess;
    CFErrorRef action_error = NULL;

    if (!do_with_account_while_unlocked(&action_error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        CFErrorRef localError = NULL;
        if (!SOSAccountSyncWithAllPeers(account, &localError)) {
            secerror("sync with all peers failed: %@", localError);
            CFReleaseSafe(localError);
            // This isn't a device-locked error, but returning false will
            // have CloudKeychainProxy ask us to try sync again after next unlock
            result = kSyncWithAllPeersOtherFail;
            return false;
        }
        return true;
    })) {
        if (action_error) {
            if (SecErrorGetOSStatus(action_error) == errSecInteractionNotAllowed) {
                secnotice("updates", "SOSAccountSyncWithAllPeers failed because device is locked; letting CloudKeychainProxy know");
                result = kSyncWithAllPeersLocked;        // tell CloudKeychainProxy to call us back when device unlocks
                CFReleaseNull(action_error);
            } else {
                secerror("Unexpected error: %@", action_error);
            }

            if (error && *error == NULL) {
                *error = action_error;
                action_error = NULL;
            }

            CFReleaseNull(action_error);
        }
    }

    return result;
}

void SOSCCSyncWithAllPeers(void)
{
    SOSCloudKeychainRequestSyncWithAllPeers(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
}

void SOSCCHandleUpdate(CFDictionaryRef updates)
{
    SOSCloudKeychainHandleUpdate(updates);
}

