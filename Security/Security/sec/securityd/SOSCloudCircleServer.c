/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <AssertMacros.h>
#include <CoreFoundation/CFURL.h>

#include <securityd/SOSCloudCircleServer.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSAccountPriv.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfoInternal.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>
#include <SecureObjectSync/SOSMessage.h>
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSKVSKeys.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/debugging.h>
#include <CKBridge/SOSCloudKeychainClient.h>

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
#include <securityd/SecItemServer.h>
#include <Security/SecItemPriv.h>

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

static SOSCCAccountDataSourceFactoryBlock accountDataSourceOverride = NULL;

bool SOSKeychainAccountSetFactoryForAccount(SOSCCAccountDataSourceFactoryBlock block)
{
    accountDataSourceOverride = Block_copy(block);

    return true;
}

//
// Forward declared
//

static void do_with_account(void (^action)(SOSAccountRef account));
static void do_with_account_async(void (^action)(SOSAccountRef account));

//
// Constants
//
CFStringRef kSOSInternalAccessGroup = CFSTR("com.apple.security.sos");

CFStringRef kSOSAccountLabel = CFSTR("iCloud Keychain Account Meta-data");

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

CFDataRef SOSItemCopy(CFStringRef service, CFErrorRef* error)
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
        CFReleaseNull(result);
        return NULL;
    }

    return result;
}

static CFDataRef SOSKeychainCopySavedAccountData()
{
    CFErrorRef error = NULL;
    CFDataRef accountData = SOSItemCopy(kSOSAccountLabel, &error);
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
#if 0
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
#endif

bool SOSCCCircleIsOn_Artifact(void) {
    bool circle_on = false;
    CFDataRef accountStatus = NULL;
    CFURLRef accountStatusFileURL = SecCopyURLForFileInKeychainDirectory(accountStatusFileName);
    require_quiet(accountStatusFileURL && CFURLResourceIsReachable(accountStatusFileURL, NULL), xit);
    accountStatus = (CFDataRef) CFPropertyListReadFromFile(accountStatusFileURL);

    if(isData(accountStatus)) {
        size_t size = CFDataGetLength(accountStatus);
        const uint8_t *der = CFDataGetBytePtr(accountStatus);
        const uint8_t *der_p = der;

        const uint8_t *sequence_end;
        der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, der_p, der_p + size);
        der_p = ccder_decode_bool(&circle_on, der_p, sequence_end);
        (void) der_p;
    }

xit:
    CFReleaseSafe(accountStatusFileURL);
    CFReleaseSafe(accountStatus);

    return circle_on;
}

#if 0
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
#endif

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
    
    if(SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, CFSTR("ak"), NULL) == NULL) {
        return;
    }
    
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
    secdebug("account", "Created account");

    CFDataRef savedAccount = SOSKeychainCopySavedAccountData();
    SOSAccountRef account = NULL;
    SOSDataSourceFactoryRef factory = accountDataSourceOverride ? accountDataSourceOverride()
                                                                : SecItemDataSourceFactoryGetDefault();

    if (savedAccount) {
        CFErrorRef inflationError = NULL;

        account = SOSAccountCreateFromData(kCFAllocatorDefault, savedAccount, factory, &inflationError);

        if(account && SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, CFSTR("ak"), NULL) == NULL) {
            SOSAccountRef newAccount = SOSAccountCreate(kCFAllocatorDefault, our_gestalt, factory);
            
            if (!newAccount) {
                secnotice("repair_account", "Tried to repair bad account - got null account");
            } else {
                account = newAccount;
            }
        }

        if (account){
            SOSAccountUpdateGestalt(account, our_gestalt);
        } else {
            secerror("Got error inflating account: %@", inflationError);
        }
        
        CFReleaseNull(inflationError);
    }
    CFReleaseSafe(savedAccount);

    if (!account) {
        account = SOSAccountCreate(kCFAllocatorDefault, our_gestalt, factory);

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

static bool _EngineMessageProtocolV2Enabled(void)
{
#if DEBUG
    //sudo rhr
    static dispatch_once_t onceToken;
    static bool v2_enabled = false;
    dispatch_once(&onceToken, ^{
		CFTypeRef v2Pref = (CFNumberRef)CFPreferencesCopyValue(CFSTR("engineV2"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        
        if (v2Pref && CFGetTypeID(v2Pref) == CFBooleanGetTypeID()) {
            v2_enabled = CFBooleanGetValue((CFBooleanRef)v2Pref);
            secnotice("server", "Engine v2 : %s", v2_enabled ? "enabled":"disabled");
        }
        CFReleaseSafe(v2Pref);
    });
    
    return v2_enabled;
#else
    return false;
#endif
}


static CFDictionaryRef CFDictionaryCreateDeviceGestalt(SCDynamicStoreRef store, CFArrayRef keys, void *context)
{
    CFStringRef modelName = CopyModelName();
    CFStringRef computerName = CopyComputerName(store);
    SInt32 version = _EngineMessageProtocolV2Enabled() ? kEngineMessageProtocolVersion : 0;
    CFNumberRef protocolVersion = CFNumberCreate(0, kCFNumberSInt32Type, &version);

    
    CFDictionaryRef gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kPIUserDefinedDeviceName, computerName,
                                                           kPIDeviceModelName,       modelName,
                                                           kPIMessageProtocolVersion, protocolVersion,
                                                           NULL);
    CFReleaseSafe(modelName);
    CFReleaseSafe(computerName);
    CFReleaseSafe(protocolVersion);

    return gestalt;
}

static void SOSCCProcessGestaltUpdate(SCDynamicStoreRef store, CFArrayRef keys, void *context)
{
    do_with_account(^(SOSAccountRef account) {
        if(account){
            CFDictionaryRef gestalt = CFDictionaryCreateDeviceGestalt(store, keys, context);
            if (SOSAccountUpdateGestalt(account, gestalt)) {
                notify_post(kSOSCCCircleChangedNotification);
            }
            CFReleaseSafe(gestalt);
        }
    });
}


static CFDictionaryRef CFDictionaryCreateGestaltAndRegisterForUpdate(dispatch_queue_t queue, void *info)
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
    gestalt = CFDictionaryCreateDeviceGestalt(store, keys, info);
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

static SOSAccountRef GetSharedAccount(void) {
    static SOSAccountRef sSharedAccount = NULL;
    static dispatch_once_t onceToken;

#if !(TARGET_OS_EMBEDDED)
    if(geteuid() == 0){
        secerror("Cannot inflate account object as root");
        return NULL;
    }
#endif

    dispatch_once(&onceToken, ^{
        secdebug("account", "Account Creation start");

        CFDictionaryRef gestalt = CFDictionaryCreateGestaltAndRegisterForUpdate(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);

        if (!gestalt) {
#if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR
            gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL);
#else
            secerror("Didn't get machine gestalt! This is going to be ugly.");
#endif
        }
        
        sSharedAccount = SOSKeychainAccountCreateSharedAccount(gestalt);

        SOSCCSetThisDeviceDefinitelyNotActiveInCircle(SOSAccountIsInCircles(sSharedAccount, NULL));

        SOSAccountAddChangeBlock(sSharedAccount, ^(SOSCircleRef circle,
                                                   CFSetRef peer_additions,      CFSetRef peer_removals,
                                                   CFSetRef applicant_additions, CFSetRef applicant_removals) {
            CFErrorRef pi_error = NULL;
            SOSPeerInfoRef me = SOSAccountGetMyPeerInCircle(sSharedAccount, circle, &pi_error);
            if (!me) {
                secerror("Error finding me for change: %@", pi_error);
            } else {
                if (SOSCircleHasPeer(circle, me, NULL) && CFSetGetCount(peer_additions) != 0) {
                    secnotice("updates", "Requesting Ensure Peer Registration.");
                    SOSCloudKeychainRequestEnsurePeerRegistration(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
                }
                
                if (CFSetContainsValue(peer_additions, me)) {
                    SOSCCSyncWithAllPeers();
                }
            }
            
            CFReleaseNull(pi_error);
            
            if (CFSetGetCount(peer_additions) != 0 ||
                CFSetGetCount(peer_removals) != 0 ||
                CFSetGetCount(applicant_additions) != 0 ||
                CFSetGetCount(applicant_removals) != 0) {

                SOSCCSetThisDeviceDefinitelyNotActiveInCircle(SOSAccountIsInCircles(sSharedAccount, NULL));
                notify_post(kSOSCCCircleChangedNotification);
           }
        });
    
        SOSCloudKeychainSetItemsChangedBlock(^CFArrayRef(CFDictionaryRef changes) {
            CFRetainSafe(changes);
            __block CFMutableArrayRef handledKeys = NULL;
            do_with_account(^(SOSAccountRef account) {
                CFStringRef changeDescription = SOSChangesCopyDescription(changes, false);
                secdebug(SOSCKCSCOPE, "Received: %@", changeDescription);
                CFReleaseSafe(changeDescription);

                CFErrorRef error = NULL;
                handledKeys = SOSTransportDispatchMessages(account, changes, &error);
                if (!handledKeys) {
                    secerror("Error handling updates: %@", error);
                    CFReleaseNull(error);
                }
            });
	    CFReleaseSafe(changes);
            return handledKeys;
        });
        CFReleaseSafe(gestalt);
        
        SOSCloudKeychainRequestEnsurePeerRegistration(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
    });
    
    
    return sSharedAccount;
}

static void do_with_account_dynamic(void (^action)(SOSAccountRef account), bool sync) {
    SOSAccountRef account = GetSharedAccount();
    
    if(account){
        dispatch_block_t do_action_and_save =  ^{
            action(account);
            SOSKeychainAccountEnsureSaved(account);
        };
        
        if (sync) {
            dispatch_sync(SOSAccountGetQueue(account), do_action_and_save);
        } else {
            dispatch_async(SOSAccountGetQueue(account), do_action_and_save);
        }
    }
}

__unused static void do_with_account_async(void (^action)(SOSAccountRef account)) {
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

#if !(TARGET_OS_EMBEDDED)
    if(geteuid() == 0){
        secerror("Cannot inflate account object as root");
        return false;
    }
#endif
    return do_if_after_first_unlock(error, ^{
        do_with_account(^(SOSAccountRef account) {
            action_result = action(account, error);
        });

    }) && action_result;
}

static bool do_with_account_while_unlocked(CFErrorRef *error, bool (^action)(SOSAccountRef account, CFErrorRef* error))
{
    __block bool action_result = false;

#if !(TARGET_OS_EMBEDDED)
    if(geteuid() == 0){
        secerror("Cannot inflate account object as root");
        return false;
    }
#endif

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

#define kWAIT2MINID "EFRESH"

static bool EnsureFreshParameters(SOSAccountRef account, CFErrorRef *error) {
    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    dispatch_retain(wait_for); // Both this scope and the block own it.

    CFMutableArrayRef keysToGet = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(keysToGet, kSOSKVSKeyParametersKey);

    __block CFDictionaryRef valuesToUpdate = NULL;
    __block bool success = false;

    secnoticeq("fresh", "%s calling SOSCloudKeychainSynchronizeAndWait", kWAIT2MINID);

    SOSCloudKeychainSynchronizeAndWait(keysToGet, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {

        if (sync_error) {
            secerrorq("%s SOSCloudKeychainSynchronizeAndWait: %@", kWAIT2MINID, sync_error);
            if (error) {
                *error = sync_error;
                CFRetainSafe(*error);
            }
        } else {
            secnoticeq("fresh", "%s returned from call; in callback to SOSCloudKeychainSynchronizeAndWait: results: %@", kWAIT2MINID, returnedValues);
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
    CFMutableArrayRef handledKeys = NULL;
    if ((valuesToUpdate) && (account)) {
        handledKeys = SOSTransportDispatchMessages(account, valuesToUpdate, error);
        if (!handledKeys) {
            secerrorq("%s Freshness update failed: %@", kWAIT2MINID, error ? *error : NULL);
            success = false;
        }
    }
    CFReleaseNull(handledKeys);
    CFReleaseNull(valuesToUpdate);
    CFReleaseNull(keysToGet);

    return success;
}

static bool Flush(CFErrorRef *error) {
    __block bool success = false;

    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    dispatch_retain(wait_for); // Both this scope and the block own it.

    secnotice("flush", "Starting");

    SOSCloudKeychainFlush(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        success = (sync_error == NULL);
        if (error) {
            CFRetainAssign(*error, sync_error);
        }

        dispatch_semaphore_signal(wait_for);
        dispatch_release(wait_for);
    });

    dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
    dispatch_release(wait_for);

    secnotice("flush", "Returned %s", success? "Success": "Failure");

    return success;
}

bool SOSCCSetUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error)
{
    secnotice("updates", "Setting credentials for %@", user_label); // TODO: remove this notice
    bool result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        if (!EnsureFreshParameters(account, block_error)) {
            return false;
        }
        if (!SOSAccountAssertUserCredentials(account, user_label, user_password, block_error)) {
            secnotice("updates", "EnsureFreshParameters/SOSAccountAssertUserCredentials error: %@", *block_error);
            return false;
        }
        return true;
    });

    return result && Flush(error);
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
        status = SOSAccountIsInCircles(account, block_error);
        return true;
    }) ? status : kSOSCCError;
}

bool SOSCCRequestToJoinCircle_Server(CFErrorRef* error)
{
    __block bool result = true;

    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountJoinCircles(account, block_error);
        return result;
    });
}

bool SOSCCRequestToJoinCircleAfterRestore_Server(CFErrorRef* error)
{
    __block bool result = true;
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        SOSAccountEnsurePeerRegistration(account, block_error);
        result = SOSAccountJoinCirclesAfterRestore(account, block_error);
        return result;
    });
    return returned;

}

bool SOSCCRequestEnsureFreshParameters_Server(CFErrorRef* error)
{
    __block bool result = true;
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
            result = EnsureFreshParameters(account, NULL);
            return result;
        });
    return returned;
}

CFStringRef SOSCCRequestDeviceID_Server(CFErrorRef *error)
{
    __block CFStringRef result = NULL;
    
    (void) do_with_account_while_unlocked(error, ^bool(SOSAccountRef account, CFErrorRef *error) {
        result = SOSAccountGetDeviceID(account, error);
        return (!isNull(result));
    });
    return result;
}

bool SOSCCSetDeviceID_Server(CFStringRef IDS, CFErrorRef *error){
    __block bool result = true;

    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountSetMyDSID(account, IDS, block_error);
        return result;
    });
    
}
bool SOSCCResetToOffering_Server(CFErrorRef* error)
{
    __block bool result = true;

    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        clearAllKVS(NULL);
        result = SOSAccountResetToOffering(account, block_error);
        return result;
    });

}

bool SOSCCResetToEmpty_Server(CFErrorRef* error)
{
    __block bool result = true;

    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountResetToEmpty(account, block_error);
        return result;
    });

}

bool SOSCCRemoveThisDeviceFromCircle_Server(CFErrorRef* error)
{
    __block bool result = true;

    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountLeaveCircles(account, block_error);
        return result;
    });
}

bool SOSCCBailFromCircle_Server(uint64_t limit_in_seconds, CFErrorRef* error)
{
    __block bool result = true;

    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountBail(account, limit_in_seconds, block_error);
        return result;
    });

}

CFArrayRef SOSCCCopyApplicantPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyApplicants(account, block_error);
        return result != NULL;
    });

    return result;
}

CFArrayRef SOSCCCopyGenerationPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyGeneration(account, block_error);
        return result != NULL;
    });
    
    return result;
}

CFArrayRef SOSCCCopyValidPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyValidPeers(account, block_error);
        return result != NULL;
    });
    
    return result;
}

bool SOSCCValidateUserPublic_Server(CFErrorRef* error)
{
    __block bool result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSValidateUserPublic(account, block_error);
        return result;
    });
    
    return result;
}

CFArrayRef SOSCCCopyNotValidPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyNotValidPeers(account, block_error);
        return result != NULL;
    });
    
    return result;
}

CFArrayRef SOSCCCopyRetirementPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyRetired(account, block_error);
        return result != NULL;
    });
    
    return result;
}

bool SOSCCAcceptApplicants_Server(CFArrayRef applicants, CFErrorRef* error)
{
    __block bool result = true;
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountAcceptApplicants(account, applicants, block_error);
        return result;
    });

}

bool SOSCCRejectApplicants_Server(CFArrayRef applicants, CFErrorRef* error)
{
    __block bool result = true;
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountRejectApplicants(account, applicants, block_error);
        return result;
    });
}

CFArrayRef SOSCCCopyPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyPeers(account, block_error);
        return result != NULL;
    });

    return result;
}

CFArrayRef SOSCCCopyConcurringPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyConcurringPeers(account, block_error);
        return result != NULL;
    });

    return result;
}

CFStringRef SOSCCCopyIncompatibilityInfo_Server(CFErrorRef* error)
{
    __block CFStringRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyIncompatibilityInfo(account, block_error);
        return result != NULL;
    });

    return result;
}

enum DepartureReason SOSCCGetLastDepartureReason_Server(CFErrorRef* error)
{
    __block enum DepartureReason result = kSOSDepartureReasonError;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountGetLastDepartureReason(account, block_error);
        return result != kSOSDepartureReasonError;
    });

    return result;
}


bool SOSCCProcessEnsurePeerRegistration_Server(CFErrorRef* error)
{
    secnotice("updates", "Request for registering peers");
    return do_with_account_while_unlocked(error, ^bool(SOSAccountRef account, CFErrorRef *error) {
        return SOSAccountEnsurePeerRegistration(account, error);
    });
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

CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateKeyParameter(CFDictionaryRef updates)
{
    CFArrayRef result = NULL;
    SOSAccountRef account = SOSKeychainAccountGetSharedAccount();   //HACK to make sure itemsChangedBlock is set
    (account) ? (result = SOSCloudKeychainHandleUpdateKeyParameter(updates)) : (result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault));
    return result;
}

CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateCircle(CFDictionaryRef updates)
{
    CFArrayRef result = NULL;
    SOSAccountRef account = SOSKeychainAccountGetSharedAccount();   //HACK to make sure itemsChangedBlock is set
    (account) ? (result = SOSCloudKeychainHandleUpdateKeyParameter(updates)) : (result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault));
    return result;
}

CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateMessage(CFDictionaryRef updates)
{
    CFArrayRef result = NULL;
    SOSAccountRef account = SOSKeychainAccountGetSharedAccount();   //HACK to make sure itemsChangedBlock is set
    (account) ? (result = SOSCloudKeychainHandleUpdateKeyParameter(updates)) : (result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault));
    return result;
}


