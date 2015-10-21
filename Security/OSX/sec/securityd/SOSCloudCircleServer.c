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
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>

#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSMessage.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSAccountHSAJoin.h>

#include <Security/SecureObjectSync/SOSKVSKeys.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/debugging.h>
#include <utilities/SecCoreCrypto.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

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
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>

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
#define RUN_AS_ROOT_ERROR 550

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

CFStringRef kSOSBurnedRecoveryAttemptCount = CFSTR("Burned Recovery Attempt Count");

CFStringRef kSOSBurnedRecoveryAttemptAttestationDate = CFSTR("Burned Recovery Attempt Attestation Date");

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
    if (!accountData) {
        secnotice("account", "Failed to load account: %@", error);
        secerror("Failed to load account: %@", error);
    }
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

static void SOSKeychainAccountEnsureSaved(SOSAccountRef account)
{
    static CFDataRef sLastSavedAccountData = NULL;

    CFErrorRef saveError = NULL;
    CFDataRef accountAsData = NULL;

    accountAsData = SOSAccountCopyEncodedData(account, kCFAllocatorDefault, &saveError);

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

CF_EXPORT CFDictionaryRef _CFCopySystemVersionDictionary(void);
CF_EXPORT CFStringRef _kCFSystemVersionBuildVersionKey;

CFStringRef CopyOSVersion(void)
{
    static dispatch_once_t once;
    static CFStringRef osVersion = NULL;
    dispatch_once(&once, ^{
#if TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR
        osVersion = MGCopyAnswer(kMGQBuildVersion, NULL);
#else
        CFDictionaryRef versions = _CFCopySystemVersionDictionary();

        if (versions) {
            CFTypeRef versionValue = CFDictionaryGetValue(versions, _kCFSystemVersionBuildVersionKey);

            if (isString(versionValue))
                osVersion = CFRetainSafe((CFStringRef) versionValue);
        }

        CFReleaseNull(versions);
#endif
        // What to do on MacOS.
        if (osVersion == NULL)
            osVersion = CFSTR("Unknown model");
    });
    return CFRetainSafe(osVersion);
}


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
            secinfo("server", "Engine v2 : %s", v2_enabled ? "enabled":"disabled");
        }
        CFReleaseSafe(v2Pref);
    });
    
    return v2_enabled;
#else
    return false;
#endif
}


static CFDictionaryRef CreateDeviceGestaltDictionary(SCDynamicStoreRef store, CFArrayRef keys, void *context)
{
    CFStringRef modelName = CopyModelName();
    CFStringRef computerName = CopyComputerName(store);
    CFStringRef osVersion = CopyOSVersion();

    SInt32 version = _EngineMessageProtocolV2Enabled() ? kEngineMessageProtocolVersion : 0;
    CFNumberRef protocolVersion = CFNumberCreate(0, kCFNumberSInt32Type, &version);
    
    CFDictionaryRef gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kPIUserDefinedDeviceNameKey,  computerName,
                                                           kPIDeviceModelNameKey,        modelName,
                                                           kPIMessageProtocolVersionKey, protocolVersion,
                                                           kPIOSVersionKey,              osVersion,
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
            CFDictionaryRef gestalt = CreateDeviceGestaltDictionary(store, keys, context);
            if (SOSAccountUpdateGestalt(account, gestalt)) {
                notify_post(kSOSCCCircleChangedNotification);
            }
            CFReleaseSafe(gestalt);
        }
    });
}


static CFDictionaryRef CreateDeviceGestaltDictionaryAndRegisterForUpdate(dispatch_queue_t queue, void *info)
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
    gestalt = CreateDeviceGestaltDictionary(store, keys, info);
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

        CFDictionaryRef gestalt = CreateDeviceGestaltDictionaryAndRegisterForUpdate(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);

        if (!gestalt) {
#if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR
            gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL);
#else
            secerror("Didn't get machine gestalt! This is going to be ugly.");
#endif
        }
        
        sSharedAccount = SOSKeychainAccountCreateSharedAccount(gestalt);

        SOSAccountAddChangeBlock(sSharedAccount, ^(SOSCircleRef circle,
                                                   CFSetRef peer_additions,      CFSetRef peer_removals,
                                                   CFSetRef applicant_additions, CFSetRef applicant_removals) {
            CFErrorRef pi_error = NULL;
            SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(sSharedAccount->my_identity);
            if (!me) {
                secerror("Error finding me for change: %@", pi_error);
            } else {
                // TODO: Figure out why peer_additions isn't right in some cases (like when joining a v2 circle with a v0 peer.
                if (SOSCircleHasPeer(circle, me, NULL) /* && CFSetGetCount(peer_additions) != 0 */) {
                    secnotice("updates", "Requesting Ensure Peer Registration.");
                    SOSCloudKeychainRequestEnsurePeerRegistration(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
                } else {
                    secinfo("updates", "Not requesting Ensure Peer Registration, since it's not needed");
                }
                
                if (CFSetContainsValue(peer_additions, me)) {
                    // TODO: Potentially remove from here and move this to the engine
                    // TODO: We also need to do this when our views change.        
                    SOSCCSyncWithAllPeers();
                }
            }
            
            CFReleaseNull(pi_error);

            // TODO: We should notify the engine of these changes here
            if (CFSetGetCount(peer_additions) != 0 ||
                CFSetGetCount(peer_removals) != 0 ||
                CFSetGetCount(applicant_additions) != 0 ||
                CFSetGetCount(applicant_removals) != 0) {

                if(CFSetGetCount(peer_removals) != 0)
                {
                    CFErrorRef localError = NULL;
                    CFMutableArrayRef removed = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
                    CFSetForEach(peer_removals, ^(const void *value) {
                        CFArrayAppendValue(removed, value);
                    });
                    SOSAccountRemoveBackupPeers(sSharedAccount, removed, &localError);
                    if(localError)
                        secerror("Had trouble removing: %@, error: %@", removed, localError);
                    CFReleaseNull(localError);
                    CFReleaseNull(removed);
                }
                notify_post(kSOSCCCircleChangedNotification);
                // This might be a bit chatty for now, but it will get things moving for clients.
                notify_post(kSOSCCViewMembershipChangedNotification);

           }
        });
    
        SOSCloudKeychainSetItemsChangedBlock(^CFArrayRef(CFDictionaryRef changes) {
            CFRetainSafe(changes);
            __block CFMutableArrayRef handledKeys = NULL;
            do_with_account(^(SOSAccountRef account) {
                CFStringRef changeDescription = SOSItemsChangedCopyDescription(changes, false);
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

        // TODO: We should not be doing extra work whenever securityd is launched, let's see if we can eliminate this call
        SOSCloudKeychainRequestEnsurePeerRegistration(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
    });
    
    
    return sSharedAccount;
}

static void do_with_account_dynamic(void (^action)(SOSAccountRef account), bool sync) {
    SOSAccountRef account = GetSharedAccount();
    
    if(account){
        dispatch_block_t do_action_and_save =  ^{
            SOSPeerInfoRef mpi = SOSAccountGetMyPeerInfo(account);
            bool wasInCircle = SOSAccountIsInCircle(account, NULL);
            CFSetRef beforeViews = mpi ? SOSPeerInfoCopyEnabledViews(mpi) : NULL;

            action(account);

            // Fake transaction around using the account object
            SOSAccountFinishTransaction(account);

            mpi = SOSAccountGetMyPeerInfo(account); // Update the peer
            bool isInCircle = SOSAccountIsInCircle(account, NULL);

            CFSetRef afterViews = mpi ? SOSPeerInfoCopyEnabledViews(mpi) : NULL;

            if(!CFEqualSafe(beforeViews, afterViews) || wasInCircle != isInCircle) {
                notify_post(kSOSCCViewMembershipChangedNotification);
            }

            CFReleaseNull(beforeViews);
            CFReleaseNull(afterViews);

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

static bool do_if_after_first_unlock(CFErrorRef *error, dispatch_block_t action)
{
#if TARGET_IPHONE_SIMULATOR
    action();
    return true;
#else
    bool beenUnlocked = false;
    require_quiet(SecAKSGetHasBeenUnlocked(&beenUnlocked, error), fail);

    require_action_quiet(beenUnlocked, fail,
                         SOSCreateErrorWithFormat(kSOSErrorNotReady, NULL, error, NULL,
                                                  CFSTR("Keybag never unlocked, ask after first unlock")));

    action();
    return true;

fail:
    return false;
#endif
}

static bool do_with_account_if_after_first_unlock(CFErrorRef *error, bool (^action)(SOSAccountRef account, CFErrorRef* error))
{
    __block bool action_result = false;

#if !(TARGET_OS_EMBEDDED)
    if(geteuid() == 0){
        secerror("Cannot inflate account object as root");
        if(error)
            *error = CFErrorCreate(kCFAllocatorDefault, CFSTR("com.apple.security"), RUN_AS_ROOT_ERROR, NULL);
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
        if(error)
            *error = CFErrorCreate(kCFAllocatorDefault, CFSTR("com.apple.security"), RUN_AS_ROOT_ERROR, NULL);
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


SOSViewResultCode SOSCCView_Server(CFStringRef viewname, SOSViewActionCode action, CFErrorRef *error) {
    __block SOSViewResultCode status = kSOSCCGeneralViewError;

    do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        switch(action) {
        case kSOSCCViewQuery:
            status = SOSAccountViewStatus(account, viewname, error);
            break;
        case kSOSCCViewEnable:
        case kSOSCCViewDisable: // fallthrough
            status = SOSAccountUpdateView(account, viewname, action, error);
            secnotice("views", "HEY!!!!!! I'm Changing VIEWS- %d", (int) status);
            break;
        default:
            secnotice("views", "Bad SOSViewActionCode - %d", (int) action);
            return false;
            break;
        }
        return true;
    });
    return status;
}


bool SOSCCViewSet_Server(CFSetRef enabledViews, CFSetRef disabledViews) {
    __block bool status = false;
    
    do_with_account_if_after_first_unlock(NULL, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        status = SOSAccountUpdateViewSets(account, enabledViews, disabledViews);
        return true;
    });
    return status;
}



SOSSecurityPropertyResultCode SOSCCSecurityProperty_Server(CFStringRef property, SOSSecurityPropertyActionCode action, CFErrorRef *error) {
    
    __block SOSViewResultCode status = kSOSCCGeneralSecurityPropertyError;
    do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        switch(action) {
            case kSOSCCSecurityPropertyQuery:
                status = SOSAccountSecurityPropertyStatus(account, property, error);
                break;
            case kSOSCCSecurityPropertyEnable:
            case kSOSCCSecurityPropertyDisable: // fallthrough
                status = SOSAccountUpdateSecurityProperty(account, property, action, error);
                secnotice("secprop", "HEY!!!!!! I'm Changing SecurityProperties- %d", (int) status);
                break;
            default:
                secnotice("secprop", "Bad SOSSecurityPropertyActionCode - %d", (int) action);
                return false;
                break;
        }
        return true;
    });
    return status;
}

void sync_the_last_data_to_kvs(SOSAccountRef account, bool waitForeverForSynchronization){
    
    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    dispatch_retain(wait_for); // Both this scope and the block own it.
    
    __block bool success = false;
    
    secnoticeq("force-push", "calling SOSCloudKeychainSynchronizeAndWait");
    
    CFMutableArrayRef keysToGet = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSCloudKeychainSynchronizeAndWait(keysToGet, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        
        if (sync_error) {
            secerrorq("SOSCloudKeychainSynchronizeAndWait: %@", sync_error);
        } else {
            secnoticeq("force-push", "returned from call; in callback to SOSCloudKeychainSynchronizeAndWait: results: %@", returnedValues);
            
            success = true;
        }
        
        dispatch_semaphore_signal(wait_for);
        dispatch_release(wait_for);
    });
    
    CFReleaseNull(keysToGet);
    
    if(waitForeverForSynchronization)
        dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
    else
        dispatch_semaphore_wait(wait_for, dispatch_time(DISPATCH_TIME_NOW, 60ull * NSEC_PER_SEC));
    
    dispatch_release(wait_for);
}

#define kWAIT2MINID "EFRESH"

static bool EnsureFreshParameters(SOSAccountRef account, CFErrorRef *error) {
    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    dispatch_retain(wait_for); // Both this scope and the block own it.

    CFMutableArrayRef keysToGet = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(keysToGet, kSOSKVSKeyParametersKey);
    // Only get key parameters due to: <rdar://problem/22794892> Upgrading from Donner with an iCDP enabled account resets iCloud keychain on devices in circle

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

static bool SOSCCAssertUserCredentialsAndOptionalDSID(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFErrorRef *error) {
    secnotice("updates", "Setting credentials and dsid (%@) for %@", dsid, user_label);
    bool result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        if (dsid != NULL && CFStringCompare(dsid, CFSTR(""), 0) != 0) {
            CFStringRef accountDSID = SOSAccountGetValue(account, kSOSDSIDKey, NULL);
            if( accountDSID == NULL){
                SOSAccountUpdateDSID(account, dsid);
                secdebug("updates", "Setting dsid, current dsid is empty for this account: %@", dsid);
            }
            else if(CFStringCompare(dsid, accountDSID, 0) != kCFCompareEqualTo){
                secnotice("updates", "Changing DSID from: %@ to %@", accountDSID, dsid);

                //DSID has changed, blast the account!
                SOSAccountSetToNew(account);

                //update DSID to the new DSID
                SOSAccountUpdateDSID(account, dsid);
            }
            else {
                secnotice("updates", "Not Changing DSID: %@ to %@", accountDSID, dsid);
            }
            
        }
        
        // Short Circuit if this passes, return immediately.
        if(SOSAccountTryUserCredentials(account, user_label, user_password, NULL)) {
            return true;
        }

        if (!EnsureFreshParameters(account, block_error)) {
            return false;
        }
        if (!SOSAccountAssertUserCredentials(account, user_label, user_password, block_error)) {
            secnotice("updates", "EnsureFreshParameters/SOSAccountAssertUserCredentials error: %@", *block_error);
            return false;
        }
        return true;
    });
    
    if (result && Flush(error)) {
        result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
            return SOSAccountGenerationSignatureUpdate(account, error);
        });
    }

    return result;
}

bool SOSCCSetUserCredentialsAndDSID_Server(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFErrorRef *error)
{
    // TODO: Return error if DSID is NULL to insist our callers provide one?
    return SOSCCAssertUserCredentialsAndOptionalDSID(user_label, user_password, dsid, error);
}

bool SOSCCSetUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error)
{
    return SOSCCAssertUserCredentialsAndOptionalDSID(user_label, user_password, NULL, error);
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

SOSCCStatus SOSCCThisDeviceIsInCircle_Server(CFErrorRef *error)
{
    __block SOSCCStatus status;

    return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        status = SOSAccountGetCircleStatus(account, block_error);
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

bool SOSCCApplyToARing_Server(CFStringRef ringName, CFErrorRef *error){
    __block bool result = true;
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
        SOSRingRef ring = SOSAccountGetRing(account, ringName, error);
        if(fpi && ring)
            result = SOSRingApply(ring, account->user_public, fpi , error);
        return result;
    });
    return returned;
}

bool SOSCCWithdrawlFromARing_Server(CFStringRef ringName, CFErrorRef *error){
    __block bool result = true;
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
        SOSRingRef ring = SOSAccountGetRing(account, ringName, error);
        if(fpi && ring)
            result = SOSRingWithdraw(ring, account->user_public, fpi , error);
        return result;
    });
    return returned;
}

bool SOSCCEnableRing_Server(CFStringRef ringName, CFErrorRef *error){
    __block bool result = true;
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
        SOSRingRef ring = SOSAccountGetRing(account, ringName, error);
        if(fpi && ring)
            result = SOSRingResetToOffering(ring, NULL, fpi, error); ;
        return result;
    });
    return returned;
}

CFStringRef SOSCCGetAllTheRings_Server(CFErrorRef *error){
    __block CFMutableDictionaryRef result = NULL;
    __block CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    
    (void) do_with_account_while_unlocked(error, ^bool(SOSAccountRef account, CFErrorRef *error) {
        result = SOSAccountGetRings(account, error);
    
        if(isDictionary(result)){
            CFDictionaryForEach(result, ^(const void *key, const void *value) {
                CFStringAppendFormat(description, NULL, CFSTR("%@"), value);
            });
        }
        if(result)
            return true;
        return false;
    });
    
    return description;
}

SOSRingStatus SOSCCRingStatus_Server(CFStringRef ringName, CFErrorRef *error){
    __block bool result = true;
    SOSRingStatus returned;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
        SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(fpi);
        
        SOSRingRef ring = SOSAccountGetRing(account, ringName, error);
        if(myPeer && ring)
            result = SOSRingDeviceIsInRing(ring, SOSPeerInfoGetPeerID(myPeer));
        return result;
    });
    return returned;
}

CFStringRef SOSCCCopyDeviceID_Server(CFErrorRef *error)
{
    __block CFStringRef result = NULL;
    
    (void) do_with_account_while_unlocked(error, ^bool(SOSAccountRef account, CFErrorRef *error) {
        result = SOSAccountCopyDeviceID(account, error);
        return (!isNull(result));
    });
    return result;
}

bool SOSCCSetDeviceID_Server(CFStringRef IDS, CFErrorRef *error){
    
    bool didSetID = false;
    __block bool result = false;
    __block CFErrorRef blockError = NULL;

    didSetID = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountSetMyDSID(account, IDS, block_error);
        if(block_error)
            blockError = CFRetainSafe(*block_error);
        return result;
    });
    
    if(error){
        *error = blockError;
    }
    return didSetID;
}

HandleIDSMessageReason SOSCCHandleIDSMessage_Server(CFDictionaryRef messageDict, CFErrorRef* error)
{
    // TODO: Locking flow:
    /*
     COMMON:
        - Get PeerCoder instance from SOSPeerCoderManager(Currently Engine)
            - Get Account lock and Initialize PeerCoder instance if it isn't valid yet.
     INCOMING:
        - Decode incoming msg on coder.
        - Pass msg along to SOSPeerRef if decoding is done.
        - Force reply from coder while in handshake mode. (or ask ckd to ask us later?)
        - save coder state.

        - Lookup SOSPeerRef in SOSEngineRef (getting engine lock temporarily to get peer.
        - Ask peer to handle decoded message
            - be notified of changed objects in all peers and update peer/engine states
        - save peer/engine state

     OUTGOING:
        - Ask coder to send an outgoing message if it is negotiating
        - Ask peer to create a message if needed
        - Encode peer msg with coder
        - save coder state
        - send reply to ckd for transporting
     */
    
    __block HandleIDSMessageReason result = kHandleIDSMessageSuccess;
    CFErrorRef action_error = NULL;
    
    if (!do_with_account_while_unlocked(&action_error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSTransportMessageIDSHandleMessage(account, messageDict, error);
        return result;
    })) {
        if (action_error) {
        if (SecErrorGetOSStatus(action_error) == errSecInteractionNotAllowed) {
            secnotice("updates", "SOSCCHandleIDSMessage_Server failed because device is locked; letting IDSKeychainSyncingProxy know");
            result = kHandleIDSMessageLocked;        // tell IDSKeychainSyncingProxy to call us back when device unlocks
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

bool SOSCCIDSPingTest_Server(CFStringRef message, CFErrorRef *error){
    bool didSendTestMessages = false;
    __block bool result = true;
    __block CFErrorRef blockError = NULL;
    
    didSendTestMessages = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountStartPingTest(account, message, block_error);
        if(block_error)
            blockError = CFRetainSafe(*block_error);
        return result;
    });
    if(blockError && error != NULL)
        *error = blockError;
    
    return didSendTestMessages;
}

bool SOSCCIDSServiceRegistrationTest_Server(CFStringRef message, CFErrorRef *error){
    bool didSendTestMessages = false;
    __block bool result = true;
    __block CFErrorRef blockError = NULL;
    
    didSendTestMessages = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountSendIDSTestMessage(account, message, &blockError);
        return result;
    });
    if(blockError != NULL && error != NULL)
        *error = blockError;
    
    return didSendTestMessages;
}

bool SOSCCIDSDeviceIDIsAvailableTest_Server(CFErrorRef *error){
    bool didSendTestMessages = false;
    __block bool result = true;
    __block CFErrorRef blockError = NULL;

    didSendTestMessages = do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountRetrieveDeviceIDFromIDSKeychainSyncingProxy(account, &blockError);
        return result;
    });
    if(blockError != NULL && error != NULL)
        *error = blockError;

    
    return didSendTestMessages;
}

bool SOSCCAccountSetToNew_Server(CFErrorRef *error)
{
	__block bool result = true;

	return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
		clearAllKVS(NULL);
		SOSAccountSetToNew(account);
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
        result = SOSAccountLeaveCircle(account, block_error);
        return result;
    });
}

bool SOSCCRemovePeersFromCircle_Server(CFArrayRef peers, CFErrorRef* error)
{
    __block bool result = true;

    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountRemovePeersFromCircle(account, peers, block_error);
        return result;
    });
}


bool SOSCCLoggedOutOfAccount_Server(CFErrorRef *error)
{
    __block bool result = true;
    
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        secnotice("sosops", "Signed out of account!");
        
        bool waitForeverForSynchronization = true;
        
        result = SOSAccountLeaveCircle(account, block_error);

        SOSAccountFinishTransaction(account); // Make sure this gets finished before we set to new.

        SOSAccountSetToNew(account);
        
        sync_the_last_data_to_kvs(account, waitForeverForSynchronization);

        return result;
    });
}

bool SOSCCBailFromCircle_Server(uint64_t limit_in_seconds, CFErrorRef* error)
{
    __block bool result = true;

    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        bool waitForeverForSynchronization = false;
        
        result = SOSAccountBail(account, limit_in_seconds, block_error);
       
        SOSAccountFinishTransaction(account); // Make sure this gets finished before we set to new.
                
        sync_the_last_data_to_kvs(account, waitForeverForSynchronization);

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

CFArrayRef SOSCCCopyViewUnawarePeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        result = SOSAccountCopyViewUnaware(account, block_error);
        return result != NULL;
    });

    return result;
}

CFArrayRef SOSCCCopyEngineState_Server(CFErrorRef* error)
{
    CFArrayRef result = NULL;
    SOSDataSourceFactoryRef dsf = SecItemDataSourceFactoryGetDefault();
    SOSDataSourceRef ds = SOSDataSourceFactoryCreateDataSource(dsf, kSecAttrAccessibleWhenUnlocked, error);
    if (ds) {
        SOSEngineRef engine = SOSDataSourceGetSharedEngine(ds, error);
        result = SOSEngineCopyPeerConfirmedDigests(engine, error);
        SOSDataSourceRelease(ds, error);
    }

    return result;
}

bool SOSCCWaitForInitialSync_Server(CFErrorRef* error) {
    __block dispatch_semaphore_t inSyncSema = NULL;
    
    bool result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        bool alreadyInSync = SOSAccountCheckHasBeenInSync(account);
        int token = -1;
        if (!alreadyInSync) {
            inSyncSema = dispatch_semaphore_create(0);
            dispatch_retain(inSyncSema);
            notify_register_dispatch(kSOSCCInitialSyncChangedNotification, &token,
                                     dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(int token) {
                                         dispatch_semaphore_signal(inSyncSema);
                                         dispatch_release(inSyncSema);

                                         notify_cancel(token);
                                     });
        }
        return true;
    });

    if (result && inSyncSema != NULL) {
        dispatch_semaphore_wait(inSyncSema, DISPATCH_TIME_FOREVER);
        dispatch_release(inSyncSema);
    }

    return result;
}

static CFArrayRef SOSAccountCopyYetToSyncViews(SOSAccountRef account, CFErrorRef *error) {
    CFArrayRef result = NULL;

    CFTypeRef valueFetched = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, error);
    if (valueFetched == kCFBooleanTrue) {
        SOSPeerInfoRef myPI = SOSAccountGetMyPeerInfo(account);
        if (myPI) {
            SOSPeerInfoWithEnabledViewSet(myPI, ^(CFSetRef enabled) {
                CFSetCopyValues(enabled);
            });
        }
    } else if (isSet(valueFetched)) {
        result = CFSetCopyValues((CFSetRef)valueFetched);
    }

    if (result == NULL) {
        result = CFArrayCreateForCFTypes(kCFAllocatorDefault, NULL);
    }

    return result;
}

CFArrayRef SOSCCCopyYetToSyncViewsList_Server(CFErrorRef* error) {

    __block CFArrayRef views = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        views = SOSAccountCopyYetToSyncViews(account, error);

        return true;
    });

    return views;
}

CFDictionaryRef SOSCCCopyEscrowRecord_Server(CFErrorRef *error){
    
    __block CFDictionaryRef result = NULL;
    __block CFErrorRef block_error = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool(SOSAccountRef account, CFErrorRef *error) {
        SOSCCStatus status = SOSAccountGetCircleStatus(account, &block_error);
        CFStringRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);
        CFDictionaryRef escrowRecords = NULL;
        CFDictionaryRef record = NULL;
        switch(status) {
            case kSOSCCInCircle:
                //get the escrow record in the peer info!
                escrowRecords = SOSPeerInfoCopyEscrowRecord(SOSAccountGetMyPeerInfo(account));
                if(escrowRecords){
                    record = CFDictionaryGetValue(escrowRecords, dsid);
                    if(record)
                        result = CFRetainSafe(record);
                }
                CFReleaseNull(escrowRecords);
                break;
            case kSOSCCRequestPending:
                //set the escrow record in the peer info/application?
                break;
            case kSOSCCNotInCircle:
            case kSOSCCCircleAbsent:
                //set the escrow record in the account expansion!
                escrowRecords = SOSAccountGetValue(account, kSOSEscrowRecord, error);
                if(escrowRecords){
                    record = CFDictionaryGetValue(escrowRecords, dsid);
                    if(record)
                        result = CFRetainSafe(record);
                }
                break;
            default:
                secdebug("account", "no circle status!");
                break;
        }
        return true;
    });
    
    return result;
}

bool SOSCCSetEscrowRecord_Server(CFStringRef escrow_label, uint64_t tries, CFErrorRef *error){
   
    __block bool result = true;
    __block CFErrorRef block_error = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool(SOSAccountRef account, CFErrorRef *error) {
        SOSCCStatus status = SOSAccountGetCircleStatus(account, &block_error);
        CFStringRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);

        CFMutableStringRef timeDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));
        CFAbsoluteTime currentTimeAndDate = CFAbsoluteTimeGetCurrent();
        
        withStringOfAbsoluteTime(currentTimeAndDate, ^(CFStringRef decription) {
            CFStringAppend(timeDescription, decription);
        });
        CFStringAppend(timeDescription, CFSTR("]"));
       
        CFNumberRef attempts = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, (const void*)&tries);
        
        CFMutableDictionaryRef escrowTimeAndTries = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionaryAddValue(escrowTimeAndTries, kSOSBurnedRecoveryAttemptCount, attempts);
        CFDictionaryAddValue(escrowTimeAndTries, kSOSBurnedRecoveryAttemptAttestationDate, timeDescription);
       
        CFMutableDictionaryRef escrowRecord = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionaryAddValue(escrowRecord, escrow_label, escrowTimeAndTries);

        switch(status) {
            case kSOSCCInCircle:
                //set the escrow record in the peer info!
                if(!SOSFullPeerInfoAddEscrowRecord(SOSAccountGetMyFullPeerInfo(account), dsid, escrowRecord, error)){
                    secdebug("accout", "Could not set escrow record in the full peer info");
                    result = false;
                }
                break;
            case kSOSCCRequestPending:
                //set the escrow record in the peer info/application?
                break;
            case kSOSCCNotInCircle:
            case kSOSCCCircleAbsent:
                //set the escrow record in the account expansion!
                
                if(!SOSAccountAddEscrowRecords(account, dsid, escrowRecord, error)) {
                    secdebug("account", "Could not set escrow record in expansion data");
                    result = false;
                }
                break;
            default:
                secdebug("account", "no circle status!");
                break;
        }
        CFReleaseNull(attempts);
        CFReleaseNull(timeDescription);
        CFReleaseNull(escrowTimeAndTries);
        CFReleaseNull(escrowRecord);
        
        return true;
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

SOSPeerInfoRef SOSCCCopyMyPeerInfo_Server(CFErrorRef* error)
{
    __block SOSPeerInfoRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        // Create a copy to be DERed/sent back to client
        result = SOSPeerInfoCreateCopy(kCFAllocatorDefault, SOSAccountGetMyPeerInfo(account), block_error);
        return result != NULL;
    });

    return result;
}

SOSPeerInfoRef SOSCCSetNewPublicBackupKey_Server(CFDataRef newPublicBackup, CFErrorRef *error){
    __block SOSPeerInfoRef result = NULL;

    (void) do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        if(SOSAccountSetBackupPublicKey(account,newPublicBackup, error)){
            // Create a copy to be DERed/sent back to client
            result = SOSPeerInfoCreateCopy(kCFAllocatorDefault, SOSAccountGetMyPeerInfo(account), block_error);
            secdebug("backup", "SOSCCSetNewPublicBackupKey_Server, new public backup is set");
        }
        else
        {
            secerror("SOSCCSetNewPublicBackupKey_Server, could not set new public backup");
        }
        return result != NULL;
    });

    return result;
}

bool SOSCCRegisterSingleRecoverySecret_Server(CFDataRef aks_bag, bool setupV0Only, CFErrorRef *error){
    return do_with_account_while_unlocked(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        return SOSAccountSetBSKBagForAllSlices(account, aks_bag, setupV0Only, error);
    });
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

bool SOSCCCheckPeerAvailability_Server(CFErrorRef *error)
{
    __block bool pingedPeersInCircle = false;
    __block dispatch_semaphore_t peerSemaphore = NULL;
    __block bool peerIsAvailable = false;
    
    static dispatch_queue_t time_out;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        time_out = dispatch_queue_create("peersAvailableTimeout", DISPATCH_QUEUE_SERIAL);
    });
    __block int token = -1;
    
    bool result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
        
        peerSemaphore = dispatch_semaphore_create(0);
        dispatch_retain(peerSemaphore);
        notify_register_dispatch(kSOSCCPeerAvailable, &token, time_out, ^(int token) {
            if(peerSemaphore != NULL){
                dispatch_semaphore_signal(peerSemaphore);
                dispatch_release(peerSemaphore);
                peerIsAvailable = true;
                notify_cancel(token);
            }
        });
        
        pingedPeersInCircle = SOSAccountCheckPeerAvailability(account, block_error);
        return pingedPeersInCircle;
    });
    
    if (result) {
        dispatch_semaphore_wait(peerSemaphore, dispatch_time(DISPATCH_TIME_NOW, 7ull * NSEC_PER_SEC));
    }
    
    if(peerSemaphore != NULL)
        dispatch_release(peerSemaphore);
    
    if(time_out != NULL && peerSemaphore != NULL){
        dispatch_sync(time_out, ^{
            if(!peerIsAvailable){
                dispatch_release(peerSemaphore);
                peerSemaphore = NULL;
                notify_cancel(token);
                secnotice("peer available", "checking peer availability timed out, releasing semaphore");
            }
        });
    }
    if(!peerIsAvailable){
        CFStringRef errorMessage = CFSTR("There are no peers in the circle currently available");
        CFDictionaryRef userInfo = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kCFErrorLocalizedDescriptionKey, errorMessage, NULL);
        if(error != NULL){
            *error =CFErrorCreate(kCFAllocatorDefault, CFSTR("com.apple.security.ids.error"), kSecIDSErrorNoPeersAvailable, userInfo);
            secerror("%@", *error);
        }
        CFReleaseNull(userInfo);
        return false;
    }
    else
        return true;
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

bool SOSCCSetLastDepartureReason_Server(enum DepartureReason reason, CFErrorRef *error){
	__block bool result = true;

	return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountRef account, CFErrorRef* block_error) {
		SOSAccountSetLastDepartureReason(account, reason);
		return result;
	});
}

bool SOSCCSetHSA2AutoAcceptInfo_Server(CFDataRef pubKey, CFErrorRef *error) {
	__block bool result = true;

	return do_with_account_if_after_first_unlock(error, ^(SOSAccountRef account,
			CFErrorRef *block_error) {
		result = SOSAccountSetHSAPubKeyExpected(account, pubKey, error);
		return (bool)result;
	});
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

CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateMessage(CFDictionaryRef updates)
{
    CFArrayRef result = NULL;
    SOSAccountRef account = SOSKeychainAccountGetSharedAccount();   //HACK to make sure itemsChangedBlock is set

    (account) ? (result = SOSCloudKeychainHandleUpdateMessage(updates)) : (result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault));
    return result;
}
