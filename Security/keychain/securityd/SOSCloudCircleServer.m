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
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>

#import "keychain/SecureObjectSync/SOSAccountTransaction.h"

#include "keychain/securityd/SOSCloudCircleServer.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>

#include "keychain/SecureObjectSync/SOSCircle.h"
#include "keychain/SecureObjectSync/SOSAccount.h"
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSAccountGhost.h"

#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"
#include "keychain/SecureObjectSync/SOSPeerInfoInternal.h"
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSMessage.h"
#include "keychain/SecureObjectSync/SOSBackupInformation.h"
#include "keychain/SecureObjectSync/SOSDataSource.h"
#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "keychain/SecureObjectSync/SOSAuthKitHelpers.h"
#import "keychain/ot/OTManager.h"
#import "keychain/SigninMetrics/OctagonSignPosts.h"
#import "NSError+UsefulConstructors.h"

#include <utilities/SecADWrapper.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>

#include <utilities/SecCFError.h>
#include <utilities/debugging.h>
#include <utilities/SecCoreCrypto.h>
#include <utilities/SecTrace.h>

#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"

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
#include "keychain/securityd/SecItemServer.h"
#include <Security/SecItemPriv.h>

#include <TargetConditionals.h>

#include <utilities/iCloudKeychainTrace.h>
#include <Security/SecAccessControlPriv.h>
#include "keychain/securityd/SecDbKeychainItem.h"

#include <os/activity.h>
#include <xpc/private.h>

#include <os/state_private.h>

#if TARGET_OS_IPHONE
#include <MobileGestalt.h>
#else
#include <AppleSystemInfo/AppleSystemInfo.h>
#endif

#define SOSCKCSCOPE "sync"
#define RUN_AS_ROOT_ERROR 550

#define USE_SYSTEMCONFIGURATION_PRIVATE_HEADERS
#import <SystemConfiguration/SystemConfiguration.h>

#include <notify.h>

static int64_t getTimeDifference(time_t start);
CFStringRef const SOSAggdSyncCompletionKey  = CFSTR("com.apple.security.sos.synccompletion");
CFStringRef const SOSAggdSyncTimeoutKey = CFSTR("com.apple.security.sos.timeout");

typedef SOSDataSourceFactoryRef (^SOSCCAccountDataSourceFactoryBlock)(void);

static SOSCCAccountDataSourceFactoryBlock accountDataSourceOverride = NULL;



//
// Forward declared
//

static void do_with_account(void (^action)(SOSAccountTransaction* txn));

//
// Constants
//

CFStringRef kSOSAccountLabel = CFSTR("iCloud Keychain Account Meta-data");

CFStringRef kSOSBurnedRecoveryAttemptCount = CFSTR("Burned Recovery Attempt Count");

CFStringRef kSOSBurnedRecoveryAttemptAttestationDate = CFSTR("Burned Recovery Attempt Attestation Date");

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

static void SOSKeychainAccountEnsureSaved(CFDataRef accountAsData)
{
    static CFDataRef sLastSavedAccountData = NULL;

    CFErrorRef saveError = NULL;
    require_quiet(!CFEqualSafe(sLastSavedAccountData, accountAsData), exit);

    if (!SOSItemUpdateOrAdd(kSOSAccountLabel, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate, accountAsData, &saveError)) {
        secerror("Can't save account: %@", saveError);
        goto exit;
    }

    CFAssignRetained(sLastSavedAccountData, CFRetainSafe(accountAsData));

exit:
    CFReleaseNull(saveError);
}

static SOSAccount* SOSKeychainAccountCreateSharedAccount(CFDictionaryRef our_gestalt)
{
    secdebug("account", "Created account");

    CFDataRef savedAccount = SOSKeychainCopySavedAccountData();
    SOSAccount* account = NULL;

    SOSDataSourceFactoryRef factory = accountDataSourceOverride ? accountDataSourceOverride()
                                                                : SecItemDataSourceFactoryGetDefault();

    require_quiet(factory, done);

    if (savedAccount) {
        NSError* inflationError = NULL;

        account = [SOSAccount accountFromData:(__bridge NSData*) savedAccount
                                      factory:factory
                                        error:&inflationError];

        if (account){
            [account.trust updateGestalt:account newGestalt:our_gestalt];
        } else {
            secerror("Got error inflating account: %@", inflationError);
        }

    }
    CFReleaseNull(savedAccount);

    if (!account) {
        account = SOSAccountCreate(kCFAllocatorDefault, our_gestalt, factory);

        if (!account)
            secerror("Got NULL creating account");
    }

    [account startStateMachine];

done:
    CFReleaseNull(savedAccount);
    return account;
}

//
// Mark: Gestalt Handling
//

CFStringRef SOSGestaltVersion = NULL;
CFStringRef SOSGestaltModel = NULL;
CFStringRef SOSGestaltDeviceName = NULL;

void
SOSCCSetGestalt_Server(CFStringRef deviceName,
                       CFStringRef version,
                       CFStringRef model,
                       CFStringRef serial)
{
    SOSGestaltDeviceName = CFRetainSafe(deviceName);
    SOSGestaltVersion = CFRetainSafe(version);
    SOSGestaltModel = CFRetainSafe(model);
    SOSGestaltSerial = CFRetainSafe(serial);
}

CFStringRef SOSCCCopyOSVersion(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        if (SOSGestaltVersion == NULL) {
            CFDictionaryRef versions = _CFCopySystemVersionDictionary();
            if (versions) {
                CFTypeRef versionValue = CFDictionaryGetValue(versions, _kCFSystemVersionBuildVersionKey);
                if (isString(versionValue))
                    SOSGestaltVersion = CFRetainSafe((CFStringRef) versionValue);
            }

            CFReleaseNull(versions);
            if (SOSGestaltVersion == NULL) {
                SOSGestaltVersion = CFSTR("Unknown model");
            }
        }
    });
    return CFRetainSafe(SOSGestaltVersion);
}


static CFStringRef CopyModelName(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        if (SOSGestaltModel == NULL) {
#if TARGET_OS_IPHONE
            SOSGestaltModel = MGCopyAnswer(kMGQDeviceName, NULL);
#else
            SOSGestaltModel = ASI_CopyComputerModelName(FALSE);
#endif
            if (SOSGestaltModel == NULL)
                SOSGestaltModel = CFSTR("Unknown model");
        }
    });
    return CFStringCreateCopy(kCFAllocatorDefault, SOSGestaltModel);
}

static CFStringRef CopyComputerName(SCDynamicStoreRef store)
{
    if (SOSGestaltDeviceName == NULL) {
        CFStringRef deviceName = SCDynamicStoreCopyComputerName(store, NULL);
        if (deviceName == NULL) {
            deviceName = CFSTR("Unknown name");
        }
        return deviceName;
    }
    return SOSGestaltDeviceName;
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
    CFStringRef osVersion = SOSCCCopyOSVersion();

    SInt32 version = _EngineMessageProtocolV2Enabled() ? kEngineMessageProtocolVersion : 0;
    CFNumberRef protocolVersion = CFNumberCreate(0, kCFNumberSInt32Type, &version);
    
    CFDictionaryRef gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kPIUserDefinedDeviceNameKey,  computerName,
                                                           kPIDeviceModelNameKey,        modelName,
                                                           kPIMessageProtocolVersionKey, protocolVersion,
                                                           kPIOSVersionKey,              osVersion,
                                                           NULL);
    CFReleaseSafe(osVersion);
    CFReleaseSafe(modelName);
    CFReleaseSafe(computerName);
    CFReleaseSafe(protocolVersion);

    return gestalt;
}

static void SOSCCProcessGestaltUpdate(SCDynamicStoreRef store, CFArrayRef keys, void *context)
{
    do_with_account(^(SOSAccountTransaction* txn) {
        if(txn.account){
            CFDictionaryRef gestalt = CreateDeviceGestaltDictionary(store, keys, context);
            if ([txn.account.trust updateGestalt:txn.account newGestalt:gestalt]) {
                secnotice("circleOps", "Changed our peer's gestalt information.  This is not a circle change.");
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

os_state_block_t accountStateBlock = ^os_state_data_t(os_state_hints_t hints) {
    os_state_data_t retval = NULL;
    CFDataRef savedAccount = NULL;
    if(hints->osh_api != OS_STATE_API_REQUEST) return NULL;
    
    /* Get account DER */
    savedAccount = SOSKeychainCopySavedAccountData();
    require_quiet(savedAccount, errOut);

    /* make a os_state_data_t object to return. */
    size_t statelen = CFDataGetLength(savedAccount);
    retval = (os_state_data_t)calloc(1, OS_STATE_DATA_SIZE_NEEDED(statelen));
    require_quiet(retval, errOut);
    
    retval->osd_type = OS_STATE_DATA_PROTOCOL_BUFFER;
    memcpy(retval->osd_data, CFDataGetBytePtr(savedAccount), statelen);
    retval->osd_size = statelen;
    strlcpy(retval->osd_title, "CloudCircle Account Object", sizeof(retval->osd_title));

errOut:
    CFReleaseNull(savedAccount);
    return retval;
};

#define FOR_EXISTING_ACCOUNT 1
#define CREATE_ACCOUNT_IF_NONE 0

static SOSAccount* GetSharedAccount(bool onlyIfItExists) {
    static SOSAccount* sSharedAccount = NULL;
    static dispatch_once_t onceToken;

#if !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
    if(geteuid() == 0){
        secerror("Cannot inflate account object as root");
        return NULL;
    }
#endif
    
    if(onlyIfItExists) {
        return sSharedAccount;
    }

    dispatch_once(&onceToken, ^{
        secdebug("account", "Account Creation start");

        CFDictionaryRef gestalt = CreateDeviceGestaltDictionaryAndRegisterForUpdate(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);

        if (!gestalt) {
#if TARGET_OS_IPHONE && TARGET_OS_SIMULATOR
            gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL);
#else
            secerror("Didn't get machine gestalt! This is going to be ugly.");
#endif
        }
        
        sSharedAccount = SOSKeychainAccountCreateSharedAccount(gestalt);
        
        SOSAccountAddChangeBlock(sSharedAccount, ^(SOSAccount *account, SOSCircleRef circle,
                                                   CFSetRef peer_additions,      CFSetRef peer_removals,
                                                   CFSetRef applicant_additions, CFSetRef applicant_removals) {
            CFErrorRef pi_error = NULL;
            SOSPeerInfoRef me = account.peerInfo;
            if(!me) {
                secinfo("circleOps", "Change block called with no peerInfo");
                return;
            }
            
            if(!SOSCircleHasPeer(circle, me, NULL)) {
                secinfo("circleOps", "Change block called while not in circle");
                return;
            }
            
            // TODO: Figure out why peer_additions isn't right in some cases (like when joining a v2 circle with a v0 peer.
            if (CFSetGetCount(peer_additions) != 0) {
                secnotice("updates", "Requesting Ensure Peer Registration.");
                SOSCloudKeychainRequestEnsurePeerRegistration(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
            } else {
                secinfo("updates", "Not requesting Ensure Peer Registration, since it's not needed");
            }
            
            if (CFSetContainsValue(peer_additions, me)) {
                // TODO: Potentially remove from here and move this to the engine
                // TODO: We also need to do this when our views change.
                CFMutableSetRef peers = SOSCircleCopyPeers(circle, kCFAllocatorDefault);
                CFSetRemoveValue(peers, me);
                if (!CFSetIsEmpty(peers)) {
                    SOSCCRequestSyncWithPeers(peers);
                }
                CFReleaseNull(peers);
            }
            
            CFReleaseNull(pi_error);

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
                    SOSAccountRemoveBackupPeers(account, removed, &localError);
                    if(localError)
                        secerror("Had trouble removing: %@, error: %@", removed, localError);
                    CFReleaseNull(localError);
                    CFReleaseNull(removed);
                }
                secnotice("circleOps", "peer counts changed, posting kSOSCCCircleChangedNotification");
                account.notifyCircleChangeOnExit = true;
           }
        });
    
        SOSCloudKeychainSetItemsChangedBlock(^CFArrayRef(CFDictionaryRef changes) {
            CFRetainSafe(changes);
            __block CFMutableArrayRef handledKeys = NULL;
            do_with_account(^(SOSAccountTransaction* txn) {
                CFStringRef changeDescription = SOSItemsChangedCopyDescription(changes, false);
                secdebug(SOSCKCSCOPE, "Received: %@", changeDescription);
                CFReleaseSafe(changeDescription);
                
                CFErrorRef error = NULL;
                handledKeys = SOSTransportDispatchMessages(txn, changes, &error);
                if (!handledKeys || error) {
                    secerror("Error handling updates: %@", error);
                }
                CFReleaseNull(error);
            });
            CFReleaseSafe(changes);
            return handledKeys;
        });
        CFReleaseSafe(gestalt);

        sSharedAccount.saveBlock = ^(CFDataRef flattenedAccount, CFErrorRef flattenFailError) {
            if (flattenedAccount) {
                SOSKeychainAccountEnsureSaved(flattenedAccount);
            } else {
                secerror("Failed to transform account into data, error: %@", flattenFailError);
            }
        };
        // TODO: We should not be doing extra work whenever securityd is launched, let's see if we can eliminate this call
        SOSCloudKeychainRequestEnsurePeerRegistration(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
 
        // provide state handler to sysdiagnose and logging
        os_state_add_handler(dispatch_get_global_queue(0, 0), accountStateBlock);

        [sSharedAccount ghostBustSchedule];

    });

    return sSharedAccount;
}

CFTypeRef GetSharedAccountRef(void)
{
    return (__bridge CFTypeRef)GetSharedAccount(FOR_EXISTING_ACCOUNT);
}

static void do_with_account(void (^action)(SOSAccountTransaction* txn)) {
    @autoreleasepool {
        SOSAccount* account = GetSharedAccount(CREATE_ACCOUNT_IF_NONE);

        if(account){
            [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
                action(txn);
            }];
        }
    }
}

static bool isValidUser(CFErrorRef* error) {
#if !TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
    if(geteuid() == 0){
        secerror("Cannot inflate account object as root");
        SOSErrorCreate(kSOSErrorUnsupported, error, NULL, CFSTR("Cannot inflate account object as root"));
        return false;
    }
#endif

    return true;
}

static bool do_if_after_first_unlock(CFErrorRef *error, dispatch_block_t action)
{
#if TARGET_OS_SIMULATOR
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

static bool do_with_account_if_after_first_unlock(CFErrorRef *error, bool (^action)(SOSAccountTransaction* txn, CFErrorRef* error))
{
    __block bool action_result = false;

    return isValidUser(error) && do_if_after_first_unlock(error, ^{
        do_with_account(^(SOSAccountTransaction* txn) {
            action_result = action(txn, error);
        });

    }) && action_result;
}

static bool isAssertionLockAcquireError(CFErrorRef error) {
    return (CFErrorGetCode(error) == kIOReturnNotPermitted) && (CFEqualSafe(CFErrorGetDomain(error), kSecKernDomain));
}

static bool do_with_account_while_unlocked(CFErrorRef *error, bool (^action)(SOSAccountTransaction* txn, CFErrorRef* error))
{
    bool result = false;

    CFErrorRef statusError = NULL;

    __block bool action_result = false;
    __block bool attempted_action = false;
    __block CFErrorRef localError = NULL;


    if(!isValidUser(error)){
        if (error && !*error && localError) {
            CFTransferRetained(*error, localError);
        }
        CFReleaseNull(localError);
        CFReleaseNull(statusError);
        
        return result;
    }

    result = SecAKSDoWithUserBagLockAssertion(&localError, ^{
        // SOSAccountGhostBustingOptions need to be retrieved from RAMP while not holding the account queue
        // yet we only want to request RAMP info if it's "time" to ghostbust.

#if GHOSTBUST_PERIODIC && (TARGET_OS_IOS || TARGET_OS_OSX)
        __block bool ghostbustnow = false;
        __block SOSAccountGhostBustingOptions gbOptions = 0;

        // Avoid mutual deadlock for just checking date.
        // Check to see if we're InCircle using the client API - will read cached value if available; otherwise it'll do the round trip and lock appropriately
        SOSCCStatus circleStatus = SOSCCThisDeviceIsInCircle(NULL);
        if(circleStatus == kSOSCCInCircle) {
            // Only need the account object to check settings
            SOSAccount *tmpAccount = GetSharedAccount(FOR_EXISTING_ACCOUNT);
            if(tmpAccount.settings) {
                ghostbustnow = [tmpAccount ghostBustCheckDate];
            }

            // Get ramp settings from the Cloud
            if(ghostbustnow) {
                gbOptions = [SOSAccount  ghostBustGetRampSettings];
            }
        }
#endif

        do_with_account(^(SOSAccountTransaction* txn) {
            SOSAccount *account = txn.account;
            if ([account isInCircle:(NULL)] && [SOSAuthKitHelpers accountIsHSA2]) {
                if(![SOSAuthKitHelpers peerinfoHasMID: account]) {
                    // This is the first good opportunity to update our FullPeerInfo and
                    // push the resulting circle.
                    [SOSAuthKitHelpers updateMIDInPeerInfo: account];
                }
            }
#if GHOSTBUST_PERIODIC && (TARGET_OS_IOS || TARGET_OS_OSX)
            if(ghostbustnow) {
                [account ghostBustPeriodic:gbOptions complete:^(bool ghostBusted, NSError *error) {
                    secnotice("ghostbust", "GhostBusting: %@", ghostBusted ? CFSTR("true"): CFSTR("false"));
                }];
            }
#endif
            attempted_action = true;
            action_result = action(txn, error);
        });
    });



    // For <rdar://problem/24355048> 13E196: Circle join fails after successful recovery with a mach error if performed while device is locked
    // If we fail with an error attempting to get an assertion while someone else has one and the system is unlocked, it must be trying to lock.
    // we assume our caller will hold the lock assertion for us to finsh our job.
    // to be extra paranoid we track if we tried the caller's block. If we did we don't do it again.

    if(result || !isAssertionLockAcquireError(localError)){
        if (error && !*error && localError) {
            CFTransferRetained(*error, localError);
        }
        CFReleaseNull(localError);
        CFReleaseNull(statusError);
        
        return (result && action_result);
    }
    if(attempted_action){
        if (error && !*error && localError) {
            CFTransferRetained(*error, localError);
        }
        CFReleaseNull(localError);
        CFReleaseNull(statusError);
        
        return (result && action_result);
    }

    bool isUnlocked = false;
    (void) SecAKSGetIsUnlocked(&isUnlocked, &statusError);
    if(!isUnlocked){
        secnotice("while-unlocked-hack", "Not trying action, aks bag locked (%@)", statusError);
        if (error && !*error && localError) {
            CFTransferRetained(*error, localError);
        }
        CFReleaseNull(localError);
        CFReleaseNull(statusError);
        
        return result && action_result;
    }

    CFReleaseNull(localError);

    secnotice("while-unlocked-hack", "Trying action while unlocked without assertion");

    result = true;
    do_with_account(^(SOSAccountTransaction* txn) {
        action_result = action(txn, &localError);
    });

    secnotice("while-unlocked-hack", "Action %s (%@)", action_result ? "succeeded" : "failed", localError);

    if (error && !*error && localError) {
        CFTransferRetained(*error, localError);
    }
    CFReleaseNull(localError);
    CFReleaseNull(statusError);

    return result && action_result;
}



CFTypeRef SOSKeychainAccountGetSharedAccount()
{
    __block SOSAccount* result = NULL;
    result = GetSharedAccount(FOR_EXISTING_ACCOUNT);
    
    if(!result) {
        secnotice("secAccount", "Failed request for account object");
    }
    return (__bridge CFTypeRef)result;
}

//
// Mark: Credential processing
//


SOSViewResultCode SOSCCView_Server(CFStringRef viewname, SOSViewActionCode action, CFErrorRef *error) {
    __block SOSViewResultCode status = kSOSCCGeneralViewError;

    do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool retval = false;
        
        switch(action) {
        case kSOSCCViewQuery:
                status = [txn.account.trust viewStatus:txn.account name:viewname err:error];
                retval = true;
                break;
        case kSOSCCViewEnable:
                status = [txn.account.trust updateView:txn.account name:viewname code:action err:error];
                retval = true;
                break;

        case kSOSCCViewDisable:
                status = [txn.account.trust updateView:txn.account name:viewname code:action err:error];
                retval = true;
                break;
        default:
            secnotice("views", "Bad SOSViewActionCode - %d", (int) action);
            retval = false;
            break;
        }
        return retval;
    });
    return status;
}

bool SOSCCViewSetWithAnalytics_Server(CFSetRef enabledViews, CFSetRef disabledViews, CFDataRef parentEvent) {
    __block bool status = false;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCViewSet);

    do_with_account_if_after_first_unlock(NULL, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        status = [txn.account.trust updateViewSetsWithAnalytics:txn.account enabled:enabledViews disabled:disabledViews parentEvent:(__bridge NSData*)parentEvent];
        return true;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCViewSet, OctagonSignpostNumber1(SOSSignpostNameSOSCCViewSet), (int)status);
    return status;
}


bool SOSCCViewSet_Server(CFSetRef enabledViews, CFSetRef disabledViews) {
    return SOSCCViewSetWithAnalytics_Server(enabledViews, disabledViews, NULL);
}


void sync_the_last_data_to_kvs(CFTypeRef account, bool waitForeverForSynchronization){
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSyncTheLastDataToKVS);
    __block CFErrorRef localError = NULL;

    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);

    secnoticeq("force-push", "calling SOSCloudKeychainSynchronizeAndWait");

    SOSCloudKeychainSynchronizeAndWait(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        if (sync_error) {
            secerrorq("SOSCloudKeychainSynchronizeAndWait: %@", sync_error);
            localError = sync_error;
        } else {
            secnoticeq("force-push", "returned from call; in callback to SOSCloudKeychainSynchronizeAndWait: results: %@", returnedValues);
        }
        
        dispatch_semaphore_signal(wait_for);
    });

    if(waitForeverForSynchronization)
        dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
    else
        dispatch_semaphore_wait(wait_for, dispatch_time(DISPATCH_TIME_NOW, 60ull * NSEC_PER_SEC));
    
    wait_for = nil;
    bool subTaskSuccess = (localError == NULL) ? true : false;
    OctagonSignpostEnd(signPost, SOSSignpostNameSyncTheLastDataToKVS, OctagonSignpostNumber1(SOSSignpostNameSyncTheLastDataToKVS), (int)subTaskSuccess);
}

#define kWAIT2MINID "EFRESH"

static bool SyncKVSAndWait(CFErrorRef *error) {
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSyncKVSAndWait);

    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);

    __block bool success = false;

    secnoticeq("fresh", "EFP calling SOSCloudKeychainSynchronizeAndWait");

    os_activity_initiate("CloudCircle EFRESH", OS_ACTIVITY_FLAG_DEFAULT, ^(void) {
        SOSCloudKeychainSynchronizeAndWait(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(__unused CFDictionaryRef returnedValues, CFErrorRef sync_error) {
            secnotice("fresh", "EFP returned, callback error: %@", sync_error);

            success = (sync_error == NULL);
            if (error) {
                CFRetainAssign(*error, sync_error);
            }

            dispatch_semaphore_signal(wait_for);
        });


        dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
        secnotice("fresh", "EFP complete: %s %@", success ? "success" : "failure", error ? *error : NULL);
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSyncKVSAndWait, OctagonSignpostNumber1(SOSSignpostNameSyncKVSAndWait), (int)success);

    return success;
}

static bool Flush(CFErrorRef *error) {
    __block bool success = false;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameFlush);

    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    secnotice("flush", "Starting");

    SOSCloudKeychainFlush(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef sync_error) {
        success = (sync_error == NULL);
        if (error) {
            CFRetainAssign(*error, sync_error);
        }

        dispatch_semaphore_signal(wait_for);
    });

    dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);

    secnotice("flush", "Returned %s", success? "Success": "Failure");

    OctagonSignpostEnd(signPost, SOSSignpostNameFlush, OctagonSignpostNumber1(SOSSignpostNameFlush), (int)success);

    return success;
}

bool SOSCCTryUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFErrorRef *error) {
    secnotice("updates", "Trying credentials and dsid (%@) for %@", dsid, user_label);
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCTryUserCredentials);

    bool result = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        if (dsid != NULL && CFStringCompare(dsid, CFSTR(""), 0) != 0) {
            SOSAccountAssertDSID(txn.account, dsid);
        }
        return true;
    });

    require_quiet(result, done);

    require_quiet(SyncKVSAndWait(error), done); // Make sure we've seen what the server has
    require_quiet(Flush(error), done);          // And processed it already...before asserting

    result = do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *block_error) {
        return SOSAccountTryUserCredentials(txn.account, user_label, user_password, block_error);
    });

    require_quiet(result, done);
    require_quiet(Flush(error), done);

done:
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCTryUserCredentials, OctagonSignpostNumber1(SOSSignpostNameSOSCCTryUserCredentials), (int)result);

    return result;
}

static bool SOSCCAssertUserCredentialsAndOptionalDSID(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFErrorRef *error) {
    secnotice("updates", "Setting credentials and dsid (%@) for %@", dsid, user_label);

    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameAssertUserCredentialsAndOptionalDSID);

    bool result = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        if (dsid != NULL && CFStringCompare(dsid, CFSTR(""), 0) != 0) {
            SOSAccountAssertDSID(txn.account, dsid);
        }
        return true;
    });

    require_quiet(result, done);

    require_quiet(SyncKVSAndWait(error), done); // Make sure we've seen what the server has
    require_quiet(Flush(error), done);          // And processed it already...before asserting

    result = do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *block_error) {
        return SOSAccountAssertUserCredentials(txn.account, user_label, user_password, block_error);
    });

    require_quiet(result, done);
    require_quiet(Flush(error), done); // Process any incoming information..circles et.al. before fixing our signature

    result = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        return SOSAccountGenerationSignatureUpdate(txn.account, error);
    });

    secnotice("updates", "Complete credentials and dsid (%@) for %@: %d %@",
              dsid, user_label, result, error ? *error : NULL);
done:
    OctagonSignpostEnd(signPost, SOSSignpostNameAssertUserCredentialsAndOptionalDSID, OctagonSignpostNumber1(SOSSignpostNameAssertUserCredentialsAndOptionalDSID), (int)result);
    return result;
}

static bool SOSCCAssertUserCredentialsAndOptionalDSIDWithAnalytics(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, NSData* parentEvent, CFErrorRef *error) {
    secnotice("updates", "Setting credentials and dsid (%@) for %@", dsid, user_label);
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameAssertUserCredentialsAndOptionalDSID);

    NSError* localError = nil;
    SFSignInAnalytics* parent = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFSignInAnalytics class] fromData:parentEvent error:&localError];

    SFSignInAnalytics *syncAndWaitEvent = nil;
    SFSignInAnalytics *flushEvent = nil;
    SFSignInAnalytics *secondFlushEvent = nil;
    SFSignInAnalytics *generationSignatureUpdateEvent = nil;

    bool result = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        if (dsid != NULL && CFStringCompare(dsid, CFSTR(""), 0) != 0) {
            SOSAccountAssertDSID(txn.account, dsid);
        }
        return true;
    });

    require_quiet(result, done);
    syncAndWaitEvent = [parent newSubTaskForEvent:@"syncAndWaitEvent"];
    require_quiet(SyncKVSAndWait(error), done); // Make sure we've seen what the server has
    [syncAndWaitEvent stopWithAttributes:nil];

    flushEvent = [parent newSubTaskForEvent:@"flushEvent"];
    require_quiet(Flush(error), done);          // And processed it already...before asserting
    [flushEvent stopWithAttributes:nil];

    result = do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *block_error) {
        return SOSAccountAssertUserCredentials(txn.account, user_label, user_password, block_error);
    });

    require_quiet(result, done);
    secondFlushEvent = [parent newSubTaskForEvent:@"secondFlushEvent"];
    require_quiet(Flush(error), done); // Process any incoming information..circles et.al. before fixing our signature
    [secondFlushEvent stopWithAttributes:nil];

    generationSignatureUpdateEvent = [parent newSubTaskForEvent:@"generationSignatureUpdateEvent"];
    result = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        return SOSAccountGenerationSignatureUpdate(txn.account, error);
    });
    [generationSignatureUpdateEvent stopWithAttributes:nil];


done:
    if(syncAndWaitEvent){
        [syncAndWaitEvent stopWithAttributes:nil];
    }
    if(flushEvent){
        [flushEvent stopWithAttributes:nil];
    }
    if(secondFlushEvent){
        [secondFlushEvent stopWithAttributes:nil];
    }
    if(generationSignatureUpdateEvent){
        [generationSignatureUpdateEvent stopWithAttributes:nil];
    }
    secnotice("updates", "Complete credentials and dsid (%@) for %@: %d %@",
              dsid, user_label, result, error ? *error : NULL);

    OctagonSignpostEnd(signPost, SOSSignpostNameAssertUserCredentialsAndOptionalDSID, OctagonSignpostNumber1(SOSSignpostNameAssertUserCredentialsAndOptionalDSID), (int)result);

    return result;
}

bool SOSCCSetUserCredentialsAndDSID_Server(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFErrorRef *error)
{
    // TODO: Return error if DSID is NULL to insist our callers provide one?
    return SOSCCAssertUserCredentialsAndOptionalDSID(user_label, user_password, dsid, error);
}

bool SOSCCSetUserCredentialsAndDSIDWithAnalytics_Server(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFDataRef parentEvent, CFErrorRef *error)
{
    return SOSCCAssertUserCredentialsAndOptionalDSIDWithAnalytics(user_label, user_password, dsid, (__bridge NSData*)parentEvent, error);
}
bool SOSCCSetUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error)
{
    return SOSCCAssertUserCredentialsAndOptionalDSID(user_label, user_password, NULL, error);
}

bool SOSCCCanAuthenticate_Server(CFErrorRef *error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCanAuthenticate);

    bool result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        // If we reply that yes we can authenticate, then let's make sure we can authenticate for a while yet.
        // <rdar://problem/32732066>
        SOSAccountRestartPrivateCredentialTimer(txn.account);
        return SOSAccountGetPrivateCredential(txn.account, block_error) != NULL;
    });

    if (!result && error && *error && CFErrorGetDomain(*error) == kSOSErrorDomain) {
        CFIndex code = CFErrorGetCode(*error);
        if (code == kSOSErrorPrivateKeyAbsent || code == kSOSErrorPublicKeyAbsent) {
            CFReleaseNull(*error);
        }
    }

    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCanAuthenticate, OctagonSignpostNumber1(SOSSignpostNameSOSCCCanAuthenticate), (int)result);

    return result;
}

bool SOSCCPurgeUserCredentials_Server(CFErrorRef *error)
{
    return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        SOSAccountPurgePrivateCredential(txn.account);
        return true;
    });
}

SOSCCStatus SOSCCThisDeviceIsInCircle_Server(CFErrorRef *error)
{
    __block SOSCCStatus status;

    return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        status = [txn.account getCircleStatus:block_error];

        return true;
    }) ? status : kSOSCCError;
}

bool SOSCCRequestToJoinCircle_Server(CFErrorRef* error)
{
    __block bool result = true;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRequestToJoinCircle);

    bool joined = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountJoinCircles(txn, block_error);
        return result;
    });

    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRequestToJoinCircle, OctagonSignpostNumber1(SOSSignpostNameSOSCCRequestToJoinCircle), (int)joined);
    return joined;
}

bool SOSCCRequestToJoinCircleWithAnalytics_Server(CFDataRef parentEvent, CFErrorRef* error)
{
    __block bool result = true;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRequestToJoinCircle);

    bool requested = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountJoinCirclesWithAnalytics(txn, (__bridge NSData*)parentEvent, block_error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRequestToJoinCircle, OctagonSignpostNumber1(SOSSignpostNameSOSCCRequestToJoinCircle), (int)requested);
    return requested;
}

bool SOSCCAccountHasPublicKey_Server(CFErrorRef *error)
{
    __block bool result = true;
    __block CFErrorRef localError = NULL;
    
    bool hasPublicKey = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountHasPublicKey(txn.account, &localError);
        return result;
    });
    
    if(error != NULL && localError != NULL)
        *error = localError;
    
    return hasPublicKey;
}

bool SOSCCAccountIsNew_Server(CFErrorRef *error)
{
    __block bool result = true;
    __block CFErrorRef localError = NULL;
    
    (void) do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountIsNew(txn.account, &localError);
        return result;
    });
    
    if(error != NULL && localError != NULL)
        *error = localError;
    
    return result;
}
bool SOSCCRequestToJoinCircleAfterRestore_Server(CFErrorRef* error)
{
    __block bool result = true;
    bool returned = false;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore);
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        SOSAccountEnsurePeerRegistration(txn.account, block_error);
        result = SOSAccountJoinCirclesAfterRestore(txn, block_error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore, OctagonSignpostNumber1(SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore), (int)result);
    return returned;

}

bool SOSCCRequestToJoinCircleAfterRestoreWithAnalytics_Server(CFDataRef parentEvent, CFErrorRef* error)
{
    __block bool result = true;
    bool returned = false;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore);
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        NSError* localError = nil;
        SFSignInAnalytics* parent = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFSignInAnalytics class] fromData:(__bridge NSData*)parentEvent error:&localError];

        SFSignInAnalytics *ensurePeerRegistrationEvent = [parent newSubTaskForEvent:@"ensurePeerRegistrationEvent"];
        SOSAccountEnsurePeerRegistration(txn.account, block_error);
        if(block_error && *block_error){
            NSError* blockError = (__bridge NSError*)*block_error;
            if(blockError){
                [ensurePeerRegistrationEvent logRecoverableError:blockError];
                secerror("ensure peer registration error: %@", blockError);
            }
        }
        [ensurePeerRegistrationEvent stopWithAttributes:nil];
        result = SOSAccountJoinCirclesAfterRestoreWithAnalytics(txn, (__bridge NSData*)parentEvent, block_error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore, OctagonSignpostNumber1(SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore), (int)result);
    return returned;

}

bool SOSCCRequestEnsureFreshParameters_Server(CFErrorRef* error)
{
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
            return SyncKVSAndWait(block_error);
        });
    if (returned) {
        returned = Flush(error);
    }
    return returned;
}

bool SOSCCApplyToARing_Server(CFStringRef ringName, CFErrorRef *error){
    __block bool result = true;
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        SOSFullPeerInfoRef fpi = txn.account.fullPeerInfo;
        SOSRingRef ring = [txn.account.trust copyRing:ringName err:error];

        if(fpi && ring) {
            result = SOSRingApply(ring, txn.account.accountKey, fpi , error);
        }
        CFReleaseNull(ring);
        return result;
    });
    return returned;
}

bool SOSCCWithdrawlFromARing_Server(CFStringRef ringName, CFErrorRef *error){
    __block bool result = true;
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        SOSFullPeerInfoRef fpi = txn.account.fullPeerInfo;
        SOSRingRef ring = [txn.account.trust copyRing:ringName err:error];
        if(fpi && ring) {
            result = SOSRingWithdraw(ring, txn.account.accountKey, fpi , error);
        }
        CFReleaseNull(ring);
        return result;
    });
    return returned;
}

bool SOSCCEnableRing_Server(CFStringRef ringName, CFErrorRef *error){
    __block bool result = true;
    bool returned = false;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        SOSFullPeerInfoRef fpi = txn.account.fullPeerInfo;
        SOSRingRef ring = [txn.account.trust copyRing:ringName err:error];
        if(fpi && ring) {
            result = SOSRingResetToOffering(ring, NULL, fpi, error);
        }
        CFReleaseNull(ring);
        return result;
    });
    return returned;
}

CFStringRef SOSCCGetAllTheRings_Server(CFErrorRef *error){
    __block CFMutableDictionaryRef result = NULL;
    __block CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    
    (void) do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        SOSAccountForEachRing(txn.account, ^SOSRingRef(CFStringRef name, SOSRingRef ring) {
            CFStringAppendFormat(description, NULL, CFSTR("%@\n"), ring);
            return NULL;
        });
        if(result)
            return true;
        return false;
    });
    
    return description;
}

SOSRingStatus SOSCCRingStatus_Server(CFStringRef ringName, CFErrorRef *error){
    __block bool result = true;
    SOSRingStatus returned;
    returned = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        SOSFullPeerInfoRef fpi = txn.account.fullPeerInfo;
        SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(fpi);
        
        SOSRingRef ring = [txn.account.trust copyRing:ringName err:error];
        if(myPeer && ring) {
            result = SOSRingDeviceIsInRing(ring, SOSPeerInfoGetPeerID(myPeer));
        }
        CFReleaseNull(ring);

        return result;
    });
    return returned;
}

bool SOSCCAccountSetToNew_Server(CFErrorRef *error)
{
	return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        SOSAccountSetToNew(txn.account);
		return true;
	});
}

bool SOSCCResetToOffering_Server(CFErrorRef* error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCResetToOffering);

    bool resetResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result = false;

        SecKeyRef user_key = SOSAccountGetPrivateCredential(txn.account, error);
        if (!user_key) {
            return result;
        }
        result = [txn.account.trust resetToOffering:txn key:user_key err:block_error];
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCResetToOffering, OctagonSignpostNumber1(SOSSignpostNameSOSCCResetToOffering), (int)resetResult);
    return resetResult;
}

bool SOSCCResetToEmpty_Server(CFErrorRef* error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCResetToEmpty);

    bool resetResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result = false;

        if (!SOSAccountHasPublicKey(txn.account, error)) {
            return result;
        }
        result = [txn.account.trust resetAccountToEmpty:txn.account transport:txn.account.circle_transport err:block_error];
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCResetToEmpty, OctagonSignpostNumber1(SOSSignpostNameSOSCCResetToEmpty), (int)resetResult);
    return resetResult;
}

bool SOSCCResetToEmptyWithAnalytics_Server(CFDataRef parentEvent, CFErrorRef* error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCResetToEmpty);

    bool resetResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result = false;

        if (!SOSAccountHasPublicKey(txn.account, error)) {
            return result;
        }
        result =  [txn.account.trust resetAccountToEmptyWithAnalytics:txn.account transport:txn.account.circle_transport parentEvent:(__bridge NSData*)parentEvent err:block_error];
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCResetToEmpty, OctagonSignpostNumber1(SOSSignpostNameSOSCCResetToEmpty), (int)resetResult);
    return resetResult;
}

bool SOSCCRemoveThisDeviceFromCircleWithAnalytics_Server(CFDataRef parentEvent, CFErrorRef* error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRemoveThisDeviceFromCircle);

    bool removeResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result = [txn.account.trust leaveCircleWithAccount:txn.account withAnalytics:(__bridge NSData*)parentEvent err:error];
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRemoveThisDeviceFromCircle, OctagonSignpostNumber1(SOSSignpostNameSOSCCRemoveThisDeviceFromCircle), (int)removeResult);

    return removeResult;
}

bool SOSCCRemoveThisDeviceFromCircle_Server(CFErrorRef* error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRemoveThisDeviceFromCircle);

    bool removeResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result = [txn.account.trust leaveCircle:txn.account err:block_error];
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRemoveThisDeviceFromCircle, OctagonSignpostNumber1(SOSSignpostNameSOSCCRemoveThisDeviceFromCircle), (int)removeResult);
    return removeResult;
}

bool SOSCCRemovePeersFromCircle_Server(CFArrayRef peers, CFErrorRef* error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRemovePeersFromCircle);

    bool removeResult =  do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result = SOSAccountRemovePeersFromCircle(txn.account, peers, block_error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRemovePeersFromCircle, OctagonSignpostNumber1(SOSSignpostNameSOSCCRemovePeersFromCircle), (int)removeResult);
    return removeResult;
}

bool SOSCCRemovePeersFromCircleWithAnalytics_Server(CFArrayRef peers, CFDataRef parentEvent, CFErrorRef* error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRemovePeersFromCircle);

    bool removeResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result = SOSAccountRemovePeersFromCircleWithAnalytics(txn.account, peers, (__bridge NSData*)parentEvent, block_error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRemovePeersFromCircle, OctagonSignpostNumber1(SOSSignpostNameSOSCCRemovePeersFromCircle), (int)removeResult);
    return removeResult;
}

bool SOSCCLoggedOutOfAccount_Server(CFErrorRef *error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCLoggedOutOfAccount);

    bool loggedOutResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        secnotice("circleOps", "Signed out of account!");


        bool waitForeverForSynchronization = true;
        
        bool result = [txn.account.trust leaveCircle:txn.account err:block_error];

        [txn restart]; // Make sure this gets finished before we set to new.

        sync_the_last_data_to_kvs((__bridge CFTypeRef)(txn.account), waitForeverForSynchronization);

        SOSAccountSetToNew(txn.account);


        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCLoggedOutOfAccount, OctagonSignpostNumber1(SOSSignpostNameSOSCCLoggedOutOfAccount), (int)loggedOutResult);
    return loggedOutResult;
}

bool SOSCCBailFromCircle_Server(uint64_t limit_in_seconds, CFErrorRef* error)
{
    return do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool waitForeverForSynchronization = false;
        
        bool result = SOSAccountBail(txn.account, limit_in_seconds, block_error);

        [txn restart]; // Make sure this gets finished before we set to new.

        sync_the_last_data_to_kvs((__bridge CFTypeRef)(txn.account), waitForeverForSynchronization);

        return result;
    });

}

CFArrayRef SOSCCCopyApplicantPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyApplicantPeerInfo);

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountCopyApplicants(txn.account, block_error);
        return result != NULL;
    });

    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyApplicantPeerInfo, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyApplicantPeerInfo), (int)(result != NULL));

    return result;
}

CFArrayRef SOSCCCopyGenerationPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountCopyGeneration(txn.account, block_error);
        return result != NULL;
    });
    
    return result;
}

CFArrayRef SOSCCCopyValidPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyValidPeerPeerInfo);

    @autoreleasepool {

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        @autoreleasepool {
            result = SOSAccountCopyValidPeers(txn.account, block_error);
        }
        return result != NULL;
    });
    }
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyValidPeerPeerInfo, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyValidPeerPeerInfo), (int)(result != NULL));

    return result;
}

bool SOSCCValidateUserPublic_Server(CFErrorRef* error)
{
    __block bool result = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCValidateUserPublic);

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSValidateUserPublic(txn.account, block_error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCValidateUserPublic, OctagonSignpostNumber1(SOSSignpostNameSOSCCValidateUserPublic), (int)result);

    return result;
}

CFArrayRef SOSCCCopyNotValidPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountCopyNotValidPeers(txn.account, block_error);
        return result != NULL;
    });
    
    return result;
}

CFArrayRef SOSCCCopyRetirementPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountCopyRetired(txn.account, block_error);
        return result != NULL;
    });

    return result;
}

CFArrayRef SOSCCCopyViewUnawarePeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyViewUnawarePeerInfo);

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountCopyViewUnaware(txn.account, block_error);
        return result != NULL;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyViewUnawarePeerInfo, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyViewUnawarePeerInfo), (int)(result != NULL));

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

static int64_t getTimeDifference(time_t start)
{
    time_t stop;
    int64_t duration;

    stop = time(NULL);

    duration = stop - start;

    return SecBucket1Significant(duration);
}

static uint64_t initialSyncTimeoutFromDefaultsWrite(void)
{
    uint64_t timeout = 10;
    
    //sudo defaults write /Library/Preferences/com.apple.authd enforceEntitlement -bool true
    CFTypeRef initialSyncTimeout = (CFNumberRef)CFPreferencesCopyValue(CFSTR("InitialSync.WaitPeriod"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    
    if (isNumber(initialSyncTimeout)) {
        CFNumberGetValue((CFNumberRef)initialSyncTimeout, kCFNumberSInt64Type, &timeout);
    }
    CFReleaseNull(initialSyncTimeout);
    return timeout;
}

bool SOSCCWaitForInitialSyncWithAnalytics_Server(CFDataRef parentEvent, CFErrorRef* error) {
    __block dispatch_semaphore_t inSyncSema = NULL;
    __block bool result = false;
    __block bool synced = false;
    bool timed_out = false;
    __block CFStringRef inSyncCallID = NULL;
    __block time_t start;
    __block CFBooleanRef shouldUseInitialSyncV0 = false;
    SFSignInAnalytics* syncingEvent = nil;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCWaitForInitialSync);

    NSError* localError = nil;
    SFSignInAnalytics* parent = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFSignInAnalytics class] fromData:(__bridge NSData*)parentEvent error:&localError];

    secnotice("initial sync", "Wait for initial sync start!");

    result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        shouldUseInitialSyncV0 = (CFBooleanRef)SOSAccountGetValue(txn.account, kSOSInitialSyncTimeoutV0, error);
        bool alreadyInSync = (SOSAccountHasCompletedInitialSync(txn.account));

        if (!alreadyInSync) {
            start = time(NULL);
            inSyncSema = dispatch_semaphore_create(0);

            SFSignInAnalytics* callWhenInSyncEvent = [parent newSubTaskForEvent:@"callWhenInSync"];
            inSyncCallID = SOSAccountCallWhenInSync(txn.account, ^bool(SOSAccount* mightBeSynced) {
                synced = true;

                if(inSyncSema){
                    dispatch_semaphore_signal(inSyncSema);
                    NSDictionary* attributes = @{@"finishedSyncing" : @YES};
                    [syncingEvent stopWithAttributes:attributes];
                }
                return true;
            });
            NSDictionary* attributes = @{};
            [callWhenInSyncEvent stopWithAttributes:attributes];
        }
        else{
            synced = true;
        }
        return true;
    });

    require_quiet(result, fail);


    if(inSyncSema){
        syncingEvent = [parent newSubTaskForEvent:@"initialSyncEvent"];
        if(shouldUseInitialSyncV0){
            secnotice("piggy","setting initial sync timeout to 5 minutes");
            timed_out = dispatch_semaphore_wait(inSyncSema, dispatch_time(DISPATCH_TIME_NOW, 300ull * NSEC_PER_SEC));
        }
        else{
            uint64_t timeoutFromDefaultsWrite = initialSyncTimeoutFromDefaultsWrite();
            secnotice("piggy","setting initial sync timeout to %llu seconds", timeoutFromDefaultsWrite);
            timed_out = dispatch_semaphore_wait(inSyncSema, dispatch_time(DISPATCH_TIME_NOW, timeoutFromDefaultsWrite * NSEC_PER_SEC));
        }
    }
    if (timed_out && shouldUseInitialSyncV0) {
        do_with_account(^(SOSAccountTransaction* txn) {
            if (SOSAccountUnregisterCallWhenInSync(txn.account, inSyncCallID)) {
                if(inSyncSema){
                    inSyncSema = NULL; // We've canceled the timeout so we must be the last.
                }
            }
        });
        NSError* error = [NSError errorWithDomain:@"securityd" code:errSecTimedOut userInfo:@{NSLocalizedDescriptionKey: @"timed out waiting for initial sync"}];
        [syncingEvent logUnrecoverableError:error];
        NSDictionary* attributes = @{@"finishedSyncing" : @NO, @"legacyPiggybacking" : @YES};
        [syncingEvent stopWithAttributes:attributes];
    }

    require_quiet(result, fail);

    if(result)
    {
        SecADClientPushValueForDistributionKey(SOSAggdSyncCompletionKey, getTimeDifference(start));
    }
    else if(!result)
    {
        SecADAddValueForScalarKey(SOSAggdSyncTimeoutKey, 1);
    }

    secnotice("initial sync", "Finished!: %d", result);

fail:
    CFReleaseNull(inSyncCallID);
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCWaitForInitialSync, OctagonSignpostNumber1(SOSSignpostNameSOSCCWaitForInitialSync), (int)result);

    return result;
}

bool SOSCCWaitForInitialSync_Server(CFErrorRef* error) {
    
    __block dispatch_semaphore_t inSyncSema = NULL;
    __block bool result = false;
    __block bool synced = false;
    bool timed_out = false;
    __block CFStringRef inSyncCallID = NULL;
    __block time_t start;
    __block CFBooleanRef shouldUseInitialSyncV0 = false;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCWaitForInitialSync);

    secnotice("initial sync", "Wait for initial sync start!");
    
    result = do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        shouldUseInitialSyncV0 = (CFBooleanRef)SOSAccountGetValue(txn.account, kSOSInitialSyncTimeoutV0, error);
        bool alreadyInSync = (SOSAccountHasCompletedInitialSync(txn.account));

        if (!alreadyInSync) {
            start = time(NULL);
            inSyncSema = dispatch_semaphore_create(0);
            
            inSyncCallID = SOSAccountCallWhenInSync(txn.account, ^bool(SOSAccount* mightBeSynced) {
                synced = true;
                
                if(inSyncSema){
                    dispatch_semaphore_signal(inSyncSema);

                }
                return true;
            });
        }
        else{
            synced = true;
        }
        return true;
    });

    require_quiet(result, fail);
    
    if(inSyncSema){
        if(shouldUseInitialSyncV0){
            secnotice("piggy","setting initial sync timeout to 5 minutes");
            timed_out = dispatch_semaphore_wait(inSyncSema, dispatch_time(DISPATCH_TIME_NOW, 300ull * NSEC_PER_SEC));
        }
        else{
            uint64_t timeoutFromDefaultsWrite = initialSyncTimeoutFromDefaultsWrite();
            secnotice("piggy","setting initial sync timeout to %llu seconds", timeoutFromDefaultsWrite);
            timed_out = dispatch_semaphore_wait(inSyncSema, dispatch_time(DISPATCH_TIME_NOW, timeoutFromDefaultsWrite * NSEC_PER_SEC));
        }
    }
    if (timed_out && shouldUseInitialSyncV0) {
        do_with_account(^(SOSAccountTransaction* txn) {
            if (SOSAccountUnregisterCallWhenInSync(txn.account, inSyncCallID)) {
                if(inSyncSema){
                    inSyncSema = NULL; // We've canceled the timeout so we must be the last.
                }
            }
        });
    }
    
    require_quiet(result, fail);

    if(result)
    {
        SecADClientPushValueForDistributionKey(SOSAggdSyncCompletionKey, getTimeDifference(start));
    }
    else if(!result)
    {
        SecADAddValueForScalarKey(SOSAggdSyncTimeoutKey, 1);
    }

    secnotice("initial sync", "Finished!: %d", result);

fail:
    CFReleaseNull(inSyncCallID);
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCWaitForInitialSync, OctagonSignpostNumber1(SOSSignpostNameSOSCCWaitForInitialSync), (int)result);

    return result;
}


static CFArrayRef SOSAccountCopyYetToSyncViews(SOSAccount* account, CFErrorRef *error) {
    __block CFArrayRef result = NULL;

    CFTypeRef valueFetched = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, error);
    if (valueFetched == kCFBooleanTrue) {
        SOSPeerInfoRef myPI = account.peerInfo;
        if (myPI) {
            SOSPeerInfoWithEnabledViewSet(myPI, ^(CFSetRef enabled) {
                result = CFSetCopyValues(enabled);
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

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        views = SOSAccountCopyYetToSyncViews(txn.account, error);

        return true;
    });

    return views;
}

bool SOSWrapToBackupSliceKeyBagForView_Server(CFStringRef viewName, CFDataRef input, CFDataRef* output, CFDataRef* bskbEncoded, CFErrorRef* error) {
    CFErrorRef localerror = NULL;
    SOSBackupSliceKeyBagRef bskb = SOSBackupSliceKeyBagForView(viewName, &localerror);

    if(bskbEncoded && bskb) {
        *bskbEncoded = SOSBSKBCopyEncoded(bskb, &localerror);
    }

    if(output) {
        *output = SOSWrapToBackupSliceKeyBag(bskb, input, &localerror);
    }

    if(error) {
        *error = localerror;
    }
    return localerror == NULL;
}

SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagForView(CFStringRef viewName, CFErrorRef* error){
    __block SOSBackupSliceKeyBagRef bskb = NULL;
    (void) do_with_account(^ (SOSAccountTransaction* txn) {
            bskb = SOSAccountBackupSliceKeyBagForView(txn.account, viewName, error);
            });
    return bskb;
}

CFDataRef SOSWrapToBackupSliceKeyBag(SOSBackupSliceKeyBagRef bskb, CFDataRef input, CFErrorRef* error) {
    CFDataRef encrypted = NULL;
    bskb_keybag_handle_t bskb_handle = 0;

    require_quiet(bskb, exit);

    bskb_handle = SOSBSKBLoadLocked(bskb, error);
    require_quiet(bskb_handle, exit);

    SecAccessControlRef access = NULL;
    require_quiet(access = SecAccessControlCreate(kCFAllocatorDefault, error), exit);
    require_quiet(SecAccessControlSetProtection(access, kSecAttrAccessibleWhenUnlocked, error), exit);

    // ks_encrypt_data takes a dictionary as its plaintext.
    CFMutableDictionaryRef plaintext = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(plaintext, CFSTR("data"), input);

    require_quiet(ks_encrypt_data_legacy(bskb_handle, access, NULL, plaintext, NULL, &encrypted, false, error), exit);

exit:
    CFReleaseNull(bskb);
    if(bskb_handle != 0) {
        ks_close_keybag(bskb_handle, error);
    }
    if(error && *error) {
        secnotice("backup", "Failed to wrap to a BKSB: %@", *error);
    }
    return encrypted;

}

CFDictionaryRef SOSCCCopyEscrowRecord_Server(CFErrorRef *error){
    
    __block CFDictionaryRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
       CFErrorRef localError = NULL;
       SOSCCStatus status = [txn.account getCircleStatus:&localError];

        CFStringRef dsid = SOSAccountGetValue(txn.account, kSOSDSIDKey, error);
        CFDictionaryRef escrowRecords = NULL;
        CFDictionaryRef record = NULL;
        switch(status) {
            case kSOSCCInCircle:
                //get the escrow record in the peer info!
                escrowRecords = SOSPeerInfoCopyEscrowRecord(txn.account.peerInfo);
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
                escrowRecords = SOSAccountGetValue(txn.account, kSOSEscrowRecord, error);
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

CFDictionaryRef SOSCCCopyBackupInformation_Server(CFErrorRef *error) {
    __block CFDictionaryRef result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        result = SOSBackupInformation(txn, error);
        return true;
    });
    return result;
}

bool SOSCCSetEscrowRecord_Server(CFStringRef escrow_label, uint64_t tries, CFErrorRef *error){
   
    if (escrow_label == NULL) {
        return false;
    }

    __block bool result = true;
    __block CFErrorRef block_error = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        SOSCCStatus status = [txn.account getCircleStatus:&block_error];

        CFStringRef dsid = SOSAccountGetValue(txn.account, kSOSDSIDKey, error);

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
                if(!SOSFullPeerInfoAddEscrowRecord(txn.account.fullPeerInfo, dsid, escrowRecord, error)){
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
                
                if(!SOSAccountAddEscrowRecords(txn.account, dsid, escrowRecord, error)) {
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
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCAcceptApplicants);

    bool acceptResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result =  SOSAccountAcceptApplicants(txn.account, applicants, block_error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCAcceptApplicants, OctagonSignpostNumber1(SOSSignpostNameSOSCCAcceptApplicants), (int)acceptResult);
    return acceptResult;
}

bool SOSCCRejectApplicants_Server(CFArrayRef applicants, CFErrorRef* error)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCAcceptApplicants);

    bool rejectResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result = SOSAccountRejectApplicants(txn.account, applicants, block_error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCAcceptApplicants, OctagonSignpostNumber1(SOSSignpostNameSOSCCAcceptApplicants), (int)rejectResult);
    return rejectResult;
}

CFArrayRef SOSCCCopyPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountCopyPeers(txn.account, block_error);
        return result != NULL;
    });

    return result;
}

CFArrayRef SOSCCCopyConcurringPeerPeerInfo_Server(CFErrorRef* error)
{
    __block CFArrayRef result = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyConcurringPeerPeerInfo);

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountCopyConcurringPeers(txn.account, block_error);
        return result != NULL;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyConcurringPeerPeerInfo, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyConcurringPeerPeerInfo), (int)(result != NULL));

    return result;
}

SOSPeerInfoRef SOSCCCopyMyPeerInfo_Server(CFErrorRef* error)
{
    __block SOSPeerInfoRef result = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyMyPeerInfo);

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        // Create a copy to be DERed/sent back to client
        result = SOSPeerInfoCreateCopy(kCFAllocatorDefault, txn.account.peerInfo, block_error);
        return result != NULL;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyMyPeerInfo, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyMyPeerInfo), (int)(result != NULL));

    return result;
}

CFDataRef SOSCCCopyAccountState_Server(CFErrorRef* error)
{
    __block CFDataRef accountState = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        // Copy account state from the keychain
        accountState = SOSAccountCopyAccountStateFromKeychain(block_error);
        return accountState != NULL;
    });
    
    return accountState;
}

bool SOSCCDeleteAccountState_Server(CFErrorRef* error)
{
    __block bool result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        // Delete account state from the keychain
        result = SOSAccountDeleteAccountStateFromKeychain(block_error);
        return result;
    });
    
    return result;
}

CFDataRef SOSCCCopyEngineData_Server(CFErrorRef* error)
{
    __block CFDataRef engineState = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        // Copy engine state from the keychain
        engineState = SOSAccountCopyEngineStateFromKeychain(block_error);
        return engineState != NULL;
    });
    
    return engineState;
}

bool SOSCCDeleteEngineState_Server(CFErrorRef* error)
{
    __block bool result = NULL;
    
    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        // Delete engine state from the keychain
        result = SOSAccountDeleteEngineStateFromKeychain(block_error);
        return result;
    });
    
    return result;
}



SOSPeerInfoRef SOSCCSetNewPublicBackupKey_Server(CFDataRef newPublicBackup, CFErrorRef *error){
    __block SOSPeerInfoRef result = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCSetNewPublicBackupKey);

    secnotice("devRecovery", "SOSCCSetNewPublicBackupKey_Server acquiring account lock");
    (void) do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        secnotice("devRecovery", "SOSCCSetNewPublicBackupKey_Server acquired account lock");
        if(SOSAccountSetBackupPublicKey(txn,newPublicBackup, error)){
            secnotice("devRecovery", "SOSCCSetNewPublicBackupKey_Server, new public backup is set in account");
            [txn restart];  // Finish the transaction to update any changes to the peer info.

            // Create a copy to be DERed/sent back to client
            result = SOSPeerInfoCreateCopy(kCFAllocatorDefault, txn.account.peerInfo, block_error);
            secnotice("devRecovery", "SOSCCSetNewPublicBackupKey_Server, new public backup is set and pushed");
        }
        else
        {
            secnotice("devRecovery", "SOSCCSetNewPublicBackupKey_Server, could not set new public backup");
        }
        return result != NULL;
    });

    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCSetNewPublicBackupKey, OctagonSignpostNumber1(SOSSignpostNameSOSCCSetNewPublicBackupKey), (int)(result != NULL));
    return result;
}

bool SOSCCRegisterSingleRecoverySecret_Server(CFDataRef aks_bag, bool setupV0Only, CFErrorRef *error){
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRegisterSingleRecoverySecret);

    bool registerResult = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        bool result =  SOSAccountSetBSKBagForAllSlices(txn.account, aks_bag, setupV0Only, error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRegisterSingleRecoverySecret, OctagonSignpostNumber1(SOSSignpostNameSOSCCRegisterSingleRecoverySecret), (int)registerResult);
    return registerResult;
}

CFStringRef SOSCCCopyIncompatibilityInfo_Server(CFErrorRef* error)
{
    __block CFStringRef result = NULL;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountCopyIncompatibilityInfo(txn.account, block_error);
        return result != NULL;
    });

    return result;
}

bool SOSCCkSecXPCOpIsThisDeviceLastBackup_Server(CFErrorRef *error) {
    bool result = do_with_account_while_unlocked(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        return SOSAccountIsLastBackupPeer(txn.account, block_error);
    });
    return result;
}

enum DepartureReason SOSCCGetLastDepartureReason_Server(CFErrorRef* error)
{
    __block enum DepartureReason result = kSOSDepartureReasonError;

    (void) do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        result = SOSAccountGetLastDepartureReason(txn.account, block_error);
        return result != kSOSDepartureReasonError;
    });

    return result;
}

bool SOSCCSetLastDepartureReason_Server(enum DepartureReason reason, CFErrorRef *error){
	return do_with_account_if_after_first_unlock(error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
		SOSAccountSetLastDepartureReason(txn.account, reason);
		return true;
	});
}

bool SOSCCProcessEnsurePeerRegistration_Server(CFErrorRef* error)
{
    secnotice("updates", "Request for registering peers");
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCProcessEnsurePeerRegistration);

    bool processResult = do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        bool result = SOSAccountEnsurePeerRegistration(txn.account, error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCProcessEnsurePeerRegistration, OctagonSignpostNumber1(SOSSignpostNameSOSCCProcessEnsurePeerRegistration), (int)processResult);
    return processResult;
}

CF_RETURNS_RETAINED CFSetRef SOSCCProcessSyncWithPeers_Server(CFSetRef peers, CFSetRef backupPeers, CFErrorRef *error) {
    __block CFSetRef result = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCProcessSyncWithPeers);

    if (!do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        result = SOSAccountProcessSyncWithPeers(txn, peers, backupPeers, error);
        return result != NULL;
    })) {
        // Be sure we don't return a result if we got an error
        CFReleaseNull(result);
    }
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCProcessSyncWithPeers, OctagonSignpostNumber1(SOSSignpostNameSOSCCProcessSyncWithPeers), (int)(result != NULL));

    return result;
}

SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers_Server(CFErrorRef* error)
{
    /*
     #define kIOReturnLockedRead      iokit_common_err(0x2c3) // device read locked
     #define kIOReturnLockedWrite     iokit_common_err(0x2c4) // device write locked
    */
    __block SyncWithAllPeersReason result = kSyncWithAllPeersSuccess;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCProcessSyncWithAllPeers);

    CFErrorRef action_error = NULL;
    
    if (!do_with_account_while_unlocked(&action_error, ^bool (SOSAccountTransaction* txn, CFErrorRef* block_error) {
        return SOSAccountRequestSyncWithAllPeers(txn, block_error);
    })) {
        if (action_error) {
            if (SecErrorGetOSStatus(action_error) == errSecInteractionNotAllowed) {
                secnotice("updates", "SOSAccountSyncWithAllKVSPeers failed because device is locked; letting CloudKeychainProxy know");
                result = kSyncWithAllPeersLocked;        // tell CloudKeychainProxy to call us back when device unlocks
                CFReleaseNull(action_error);
            } else {
                secerror("Unexpected error: %@", action_error);
            }
        }

        SecErrorPropagate(action_error, error);
    }

    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCProcessSyncWithAllPeers, OctagonSignpostNumber1(SOSSignpostNameSOSCCProcessSyncWithAllPeers), (int)result);

    return result;
}

//
// Sync requesting
//

void SOSCCRequestSyncWithPeer(CFStringRef peerID) {
    CFArrayRef peers = CFArrayCreateForCFTypes(kCFAllocatorDefault, peerID, NULL);

    SOSCCRequestSyncWithPeersList(peers);

    CFReleaseNull(peers);
}

void SOSCCRequestSyncWithPeers(CFSetRef /*SOSPeerInfoRef/CFStringRef*/ peerIDs) {
    CFMutableArrayRef peerIDArray = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    CFSetForEach(peerIDs, ^(const void *value) {
        if (isString(value)) {
            CFArrayAppendValue(peerIDArray, value);
        } else if (isSOSPeerInfo(value)) {
            SOSPeerInfoRef peer = asSOSPeerInfo(value);
            CFArrayAppendValue(peerIDArray, SOSPeerInfoGetPeerID(peer));
        } else {
            secerror("Bad element, skipping: %@", value);
        }
    });

    SOSCCRequestSyncWithPeersList(peerIDArray);

    CFReleaseNull(peerIDArray);
}

void SOSCCRequestSyncWithPeersList(CFArrayRef /*CFStringRef*/ peerIDs) {
    os_activity_initiate("CloudCircle RequestSyncWithPeersList", OS_ACTIVITY_FLAG_DEFAULT, ^(void) {
        OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRequestSyncWithPeersList);

        CFArrayRef empty = CFArrayCreateForCFTypes(kCFAllocatorDefault, NULL);

        CFStringArrayPerformWithDescription(peerIDs, ^(CFStringRef description) {
            secnotice("syncwith", "Request Sync With: %@", description);
        });

        SOSCloudKeychainRequestSyncWithPeers(peerIDs, empty,
                                             dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
        CFReleaseNull(empty);
        OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRequestSyncWithPeersList, OctagonSignpostNumber1(SOSSignpostNameSOSCCRequestSyncWithPeersList), (int)true);
    });
}

void SOSCCRequestSyncWithBackupPeerList(CFArrayRef /* CFStringRef */ backupPeerIDs) {
    os_activity_initiate("CloudCircle SOSCCRequestSyncWithBackupPeerList", OS_ACTIVITY_FLAG_DEFAULT, ^(void) {
        OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRequestSyncWithBackupPeerList);

        CFArrayRef empty = CFArrayCreateForCFTypes(kCFAllocatorDefault, NULL);

        CFStringArrayPerformWithDescription(backupPeerIDs, ^(CFStringRef description) {
            secnotice("syncwith", "Request backup sync With: %@", description);
        });

        SOSCloudKeychainRequestSyncWithPeers(empty, backupPeerIDs,
                                             dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);

        CFReleaseNull(empty);
        OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRequestSyncWithBackupPeerList, OctagonSignpostNumber1(SOSSignpostNameSOSCCRequestSyncWithBackupPeerList), (int)true);

    });
}

bool SOSCCIsSyncPendingFor(CFStringRef peerID, CFErrorRef *error) {
    return false;
}

void SOSCCEnsurePeerRegistration(void)
{
    os_activity_initiate("CloudCircle EnsurePeerRegistration", OS_ACTIVITY_FLAG_DEFAULT, ^(void) {
        OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCEnsurePeerRegistration);
        SOSCloudKeychainRequestEnsurePeerRegistration(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), NULL);
        OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCEnsurePeerRegistration, OctagonSignpostNumber1(SOSSignpostNameSOSCCEnsurePeerRegistration), (int)true);

    });
}

CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateMessage(CFDictionaryRef updates)
{
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCHandleUpdateMessage);

    CFArrayRef result = NULL;
    SOSAccount* account = (__bridge SOSAccount *)(SOSKeychainAccountGetSharedAccount());   //HACK to make sure itemsChangedBlock is set

    result = account ? SOSCloudKeychainHandleUpdateMessage(updates) : CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCHandleUpdateMessage, OctagonSignpostNumber1(SOSSignpostNameSOSCCHandleUpdateMessage), (int)(result != NULL));

    return result;
}

SOSPeerInfoRef SOSCCCopyApplication_Server(CFErrorRef *error) {
    __block SOSPeerInfoRef application = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyApplication);

    do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        application = SOSAccountCopyApplication(txn.account, error);
        return application != NULL;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyApplication, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyApplication), (int)(application != NULL));

    return application;
}

bool SOSCCCleanupKVSKeys_Server(CFErrorRef *error) {
    bool result = do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        return SOSAccountCleanupAllKVSKeys(txn.account, error);
    });
    if(result && error && *error) {
        CFReleaseNull(*error);
    }
    return result;
}

bool SOSCCTestPopulateKVSWithBadKeys_Server(CFErrorRef *error)
{
    __block bool result = false;
    do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        return SOSAccountPopulateKVSWithBadKeys(txn.account, error);
    });
    return result;
}
CFDataRef SOSCCCopyCircleJoiningBlob_Server(SOSPeerInfoRef applicant, CFErrorRef *error) {
    __block CFDataRef pbblob = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyCircleJoiningBlob);

    do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        pbblob = SOSAccountCopyCircleJoiningBlob(txn.account, applicant, error);
        return pbblob != NULL;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyCircleJoiningBlob, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyCircleJoiningBlob), (int)(pbblob != NULL));

    return pbblob;
}

CFDataRef SOSCCCopyInitialSyncData_Server(SOSInitialSyncFlags flags, CFErrorRef *error) {
    __block CFDataRef pbblob = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyInitialSyncData);

    do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        pbblob = SOSAccountCopyInitialSyncData(txn.account, flags, error);
        return pbblob != NULL;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyInitialSyncData, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyInitialSyncData), (int)(pbblob != NULL));

    return pbblob;
}

bool SOSCCJoinWithCircleJoiningBlob_Server(CFDataRef joiningBlob, PiggyBackProtocolVersion version, CFErrorRef *error) {
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCJoinWithCircleJoiningBlob);

    bool joinResult = do_with_account_while_unlocked(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        bool result = SOSAccountJoinWithCircleJoiningBlob(txn.account, joiningBlob, version, error);
        return result;
    });

    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCJoinWithCircleJoiningBlob, OctagonSignpostNumber1(SOSSignpostNameSOSCCJoinWithCircleJoiningBlob), (int)joinResult);
    return joinResult;
}

CFBooleanRef SOSCCPeersHaveViewsEnabled_Server(CFArrayRef viewNames, CFErrorRef *error) {
    __block CFBooleanRef result = NULL;
    do_with_account_if_after_first_unlock(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCPeersHaveViewsEnabled);
        result = SOSAccountPeersHaveViewsEnabled(txn.account, viewNames, error);
        OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCPeersHaveViewsEnabled, OctagonSignpostNumber1(SOSSignpostNameSOSCCPeersHaveViewsEnabled), (int)(result != NULL));
        return result != NULL;
    });

    return result;
}

bool SOSCCRegisterRecoveryPublicKey_Server(CFDataRef recovery_key, CFErrorRef *error){
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCRegisterRecoveryPublicKey);

    bool registerResult = do_with_account_if_after_first_unlock(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        bool result = false;
        if(recovery_key != NULL && CFDataGetLength(recovery_key) != 0) {
            result = SOSAccountRegisterRecoveryPublicKey(txn, recovery_key, error);
        }
        else {
            result = SOSAccountClearRecoveryPublicKey(txn, recovery_key, error);
        }
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCRegisterRecoveryPublicKey, OctagonSignpostNumber1(SOSSignpostNameSOSCCRegisterRecoveryPublicKey), (int)registerResult);
    return registerResult;
}

CFDataRef SOSCCCopyRecoveryPublicKey_Server(CFErrorRef *error){

    __block CFDataRef result = NULL;
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCCopyRecoveryPublicKey);
    do_with_account_if_after_first_unlock(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        result = SOSAccountCopyRecoveryPublicKey(txn, error);
        return result != NULL;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCCopyRecoveryPublicKey, OctagonSignpostNumber1(SOSSignpostNameSOSCCCopyRecoveryPublicKey), (int)(result != NULL));
    return result;
}

bool SOSCCMessageFromPeerIsPending_Server(SOSPeerInfoRef peer, CFErrorRef *error) {
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCMessageFromPeerIsPending);

    bool pendingResult = do_with_account_if_after_first_unlock(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        bool result = SOSAccountMessageFromPeerIsPending(txn, peer, error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCMessageFromPeerIsPending, OctagonSignpostNumber1(SOSSignpostNameSOSCCMessageFromPeerIsPending), (int)pendingResult);
    return pendingResult;
}

bool SOSCCSendToPeerIsPending_Server(SOSPeerInfoRef peer, CFErrorRef *error) {
    OctagonSignpost signPost = OctagonSignpostBegin(SOSSignpostNameSOSCCSendToPeerIsPending);

    bool sendResult = do_with_account_if_after_first_unlock(error, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        bool result = SOSAccountSendToPeerIsPending(txn, peer, error);
        return result;
    });
    OctagonSignpostEnd(signPost, SOSSignpostNameSOSCCSendToPeerIsPending, OctagonSignpostNumber1(SOSSignpostNameSOSCCSendToPeerIsPending), (int)sendResult);
    return sendResult;
}

void SOSCCResetOTRNegotiation_Server(CFStringRef peerid)
{
    CFErrorRef localError = NULL;
     do_with_account_while_unlocked(&localError, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
         SOSAccountResetOTRNegotiationCoder(txn.account, peerid);
         return true;
    });
    if(localError)
    {
        secerror("error resetting otr negotation: %@", localError);
    }
}

void SOSCCPeerRateLimiterSendNextMessage_Server(CFStringRef peerid, CFStringRef accessGroup)
{
    CFErrorRef localError = NULL;
    do_with_account_while_unlocked(&localError, ^bool(SOSAccountTransaction* txn, CFErrorRef *error) {
        SOSAccountTimerFiredSendNextMessage(txn, (__bridge NSString*)peerid, (__bridge NSString*)accessGroup);
        return true;
    });
    if(localError)
    {
        secerror("error sending next message: %@", localError);
    }
}

void SOSCCPerformWithOctagonSigningKey(void (^action)(SecKeyRef octagonPrivSigningKey, CFErrorRef error))
{
    CFErrorRef error = NULL;
    do_with_account_if_after_first_unlock(&error, ^bool(SOSAccountTransaction *txn, CFErrorRef *err) {
        SOSFullPeerInfoRef fpi = txn.account.trust.fullPeerInfo;
        SecKeyRef signingKey = SOSFullPeerInfoCopyOctagonSigningKey(fpi, err);
        CFErrorRef errorArg = err ? *err : NULL;
        action(signingKey, errorArg);
        CFReleaseNull(signingKey);
        return true;
    });
    CFReleaseNull(error);
}

void SOSCCPerformWithOctagonSigningPublicKey(void (^action)(SecKeyRef octagonPublicKey, CFErrorRef error))
{
    CFErrorRef error = NULL;
    do_with_account_if_after_first_unlock(&error, ^bool(SOSAccountTransaction *txn, CFErrorRef *err) {
        SOSFullPeerInfoRef fpi = txn.account.trust.fullPeerInfo;
        SecKeyRef signingKey = SOSFullPeerInfoCopyOctagonPublicSigningKey(fpi, err);
        CFErrorRef errorArg = err ? *err : NULL;
        action(signingKey, errorArg);
        CFReleaseNull(signingKey);
        return true;
    });
    CFReleaseNull(error);
}

void SOSCCPerformWithOctagonEncryptionKey(void (^action)(SecKeyRef octagonPrivEncryptionKey, CFErrorRef error))
{
    CFErrorRef error = NULL;
    do_with_account_if_after_first_unlock(&error, ^bool(SOSAccountTransaction *txn, CFErrorRef *err) {
        SOSFullPeerInfoRef fpi = txn.account.trust.fullPeerInfo;
        SecKeyRef signingKey = SOSFullPeerInfoCopyOctagonEncryptionKey(fpi, err);
        CFErrorRef errorArg = err ? *err : NULL;
        action(signingKey, errorArg);
        CFReleaseNull(signingKey);
        return true;
    });
    CFReleaseNull(error);
}

void SOSCCPerformWithOctagonEncryptionPublicKey(void (^action)(SecKeyRef octagonPublicEncryptionKey, CFErrorRef error))
{
    CFErrorRef error = NULL;
    do_with_account_if_after_first_unlock(&error, ^bool(SOSAccountTransaction *txn, CFErrorRef *err) {
        SOSFullPeerInfoRef fpi = txn.account.trust.fullPeerInfo;
        SecKeyRef signingKey = SOSFullPeerInfoCopyOctagonPublicEncryptionKey(fpi, err);
        CFErrorRef errorArg = err ? *err : NULL;
        action(signingKey, errorArg);
        CFReleaseNull(signingKey);
        return true;
    });
    CFReleaseNull(error);
}

void SOSCCPerformWithAllOctagonKeys(void (^action)(SecKeyRef octagonEncryptionKey, SecKeyRef octagonSigningKey, CFErrorRef error))
{
    CFErrorRef localError = NULL;
    do_with_account_if_after_first_unlock(&localError, ^bool(SOSAccountTransaction *txn, CFErrorRef *err) {
        SecKeyRef encryptionKey = NULL;
        SecKeyRef signingKey = NULL;
        CFErrorRef errorArg = err ? *err : NULL;
        
        SOSFullPeerInfoRef fpi = txn.account.trust.fullPeerInfo;
        require_action_quiet(fpi, fail, secerror("device does not have a peer"); SOSCreateError(kSOSErrorPeerNotFound, CFSTR("No Peer for Account"), NULL, &errorArg));
        
        signingKey = SOSFullPeerInfoCopyOctagonSigningKey(fpi, &errorArg);
        require_action_quiet(signingKey && !errorArg, fail, secerror("SOSCCPerformWithAllOctagonKeys signing key error: %@", errorArg));
        CFReleaseNull(errorArg);
        
        encryptionKey = SOSFullPeerInfoCopyOctagonEncryptionKey(fpi, &errorArg);
        require_action_quiet(encryptionKey && !errorArg, fail, secerror("SOSCCPerformWithAllOctagonKeys encryption key error: %@", errorArg));
        
        action(encryptionKey, signingKey, errorArg);
        CFReleaseNull(signingKey);
        CFReleaseNull(encryptionKey);
        CFReleaseNull(errorArg);
        return true;
    fail:
        action(NULL, NULL, errorArg);
        CFReleaseNull(errorArg);
        CFReleaseNull(signingKey);
        CFReleaseNull(encryptionKey);
        return true;
    });
    CFReleaseNull(localError);
}

static bool saveOctagonKeysToKeychain(NSString* keyLabel, NSData* keyDataToSave, int keySize, SecKeyRef octagonPublicKey, NSError** error) {
    NSError* localerror = nil;

    CFDataRef publicKeyHash = SecKeyCopyPublicKeyHash(octagonPublicKey);

    NSMutableDictionary* query = [@{
                                    (id)kSecClass                   : (id)kSecClassKey,
                                    (id)kSecAttrKeyType             : (id)kSecAttrKeyTypeEC,
                                    (id)kSecAttrKeyClass            : (id)kSecAttrKeyClassPrivate,
                                    (id)kSecAttrAccessGroup         : (id)kSOSInternalAccessGroup,
                                    (id)kSecAttrLabel               : keyLabel,
                                    (id)kSecAttrApplicationLabel    : (__bridge NSData*)(publicKeyHash),
                                    (id)kSecAttrSynchronizable      : (id)kCFBooleanFalse,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecValueData               : keyDataToSave,
                                    } mutableCopy];

    CFTypeRef result = NULL;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, &result);

    if(status == errSecSuccess) {
        CFReleaseNull(publicKeyHash);
        return true;
    }
    if(status == errSecDuplicateItem) {
        // Add every primary key attribute to this find dictionary
        NSMutableDictionary* findQuery = [[NSMutableDictionary alloc] init];
        findQuery[(id)kSecClass] = query[(id)kSecClass];
        findQuery[(id)kSecAttrKeyType] = query[(id)kSecAttrKeyTypeEC];
        findQuery[(id)kSecAttrKeyClass] = query[(id)kSecAttrKeyClassPrivate];
        findQuery[(id)kSecAttrAccessGroup] = query[(id)kSecAttrAccessGroup];
        findQuery[(id)kSecAttrLabel] = query[(id)kSecAttrLabel];
        findQuery[(id)kSecAttrApplicationLabel] = query[(id)kSecAttrApplicationLabel];
        findQuery[(id)kSecUseDataProtectionKeychain] = query[(id)kSecUseDataProtectionKeychain];

        NSMutableDictionary* updateQuery = [query mutableCopy];
        updateQuery[(id)kSecClass] = nil;

        status = SecItemUpdate((__bridge CFDictionaryRef)findQuery, (__bridge CFDictionaryRef)updateQuery);

        if(status) {
            localerror = [NSError
                          errorWithDomain:NSOSStatusErrorDomain
                          code:status
                          description:[NSString stringWithFormat:@"SecItemUpdate: %d", (int)status]];
        }
    } else {
        localerror = [NSError
                      errorWithDomain:NSOSStatusErrorDomain
                      code:status
                      description:[NSString stringWithFormat:@"SecItemAdd: %d", (int)status]];
    }
    if(localerror && error) {
        *error = localerror;
    }

    CFReleaseNull(publicKeyHash);

    return (status == errSecSuccess);
}

static NSString* createKeyLabel(NSDictionary *gestalt, NSString* circleName, NSString* prefix)
{
    NSString *keyName = [NSString stringWithFormat:@"ID for %@-%@",SOSPeerGestaltGetName((__bridge CFDictionaryRef)(gestalt)), circleName];

    NSString* octagonSigningKeyName = [prefix stringByAppendingString: keyName];

    return octagonSigningKeyName;
}

static NSError* saveKeysToKeychain(SOSAccount* account, NSData* octagonSigningFullKey, NSData* octagonEncryptionFullKey, SecKeyRef octagonSigningPublicKeyRef, SecKeyRef octagonEncryptionPublicKeyRef)
{
    NSError* saveToKeychainError = nil;

    NSString* circleName = (__bridge NSString*)(SOSCircleGetName(account.trust.trustedCircle));
    NSString* signingPrefix = @"Octagon Peer Signing ";
    NSString* encryptionPrefix = @"Octagon Peer Encryption ";
    NSString* octagonSigningKeyName = createKeyLabel(account.gestalt, circleName, signingPrefix);
    NSString* octagonEncryptionKeyName = createKeyLabel(account.gestalt, circleName, encryptionPrefix);

    /* behavior mimics GeneratePermanentFullECKey_internal */
    saveOctagonKeysToKeychain(octagonSigningKeyName, octagonSigningFullKey, 384, octagonSigningPublicKeyRef, &saveToKeychainError);
    if(saveToKeychainError) {
        secerror("octagon: could not save signing key: %@", saveToKeychainError);
        return saveToKeychainError;
    }
    saveOctagonKeysToKeychain(octagonEncryptionKeyName, octagonEncryptionFullKey, 384, octagonEncryptionPublicKeyRef, &saveToKeychainError);
    if(saveToKeychainError) {
        secerror("octagon: could not save encryption key: %@", saveToKeychainError);
        return saveToKeychainError;
    }

    return nil;
}

void SOSCCPerformUpdateOfAllOctagonKeys(CFDataRef octagonSigningFullKey, CFDataRef octagonEncryptionFullKey,
                                        CFDataRef signingPublicKey, CFDataRef encryptionPublicKey,
                                        SecKeyRef octagonSigningPublicKeyRef, SecKeyRef octagonEncryptionPublicKeyRef,
                                        void (^action)(CFErrorRef error))
{
    CFErrorRef localError = NULL;
    do_with_account_if_after_first_unlock(&localError, ^bool(SOSAccountTransaction *txn, CFErrorRef *err) {
        CFErrorRef updateOctagonKeysError = NULL;
        bool updatedPeerInfo = SOSAccountUpdatePeerInfoAndPush(txn.account, CFSTR("Updating Octagon Keys in SOS"), &updateOctagonKeysError, ^bool(SOSPeerInfoRef pi, CFErrorRef *error) {

            //save octagon key set to the keychain
            NSError* saveError = nil;
            saveError = saveKeysToKeychain(txn.account, (__bridge NSData*)octagonSigningFullKey, (__bridge NSData*)octagonEncryptionFullKey,
                     octagonSigningPublicKeyRef, octagonEncryptionPublicKeyRef);

            if(saveError) {
                secerror("octagon: failed to save Octagon keys to the keychain: %@", saveError);
                action((__bridge CFErrorRef)saveError);
                return false;
            }

            //now update the peer info to contain octagon keys
            if(pi){
                CFErrorRef setError = NULL;
                SOSPeerInfoSetOctagonKeysInDescription(pi, octagonSigningPublicKeyRef, octagonEncryptionPublicKeyRef, &setError);
                if(setError) {
                    secerror("octagon: Failed to set Octagon Keys in peerInfo: %@", setError);
                    action(setError);
                    return false;
                }
            } else {
                secnotice("octagon", "No peer info to update?");
                NSError *noPIError = [NSError errorWithDomain:(__bridge NSString*)kSOSErrorDomain code:kSOSErrorPeerNotFound userInfo:@{NSLocalizedDescriptionKey : @"Device has no full peer info"}];
                action((__bridge CFErrorRef)noPIError);
                return false;
            }

            secnotice("octagon", "Success! Upated Octagon keys in SOS!");

            action(nil);
            return true;
        });
        return updatedPeerInfo;
    });
    CFReleaseNull(localError);
}

void SOSCCPerformWithTrustedPeers(void (^action)(CFSetRef sosPeerInfoRefs, CFErrorRef error))
{
    CFErrorRef cfAccountError = NULL;
    do_with_account_if_after_first_unlock(&cfAccountError, ^bool(SOSAccountTransaction *txn, CFErrorRef *cferror) {
        CFSetRef sosPeerSet = [txn.account.trust copyPeerSetMatching:^bool(SOSPeerInfoRef peer) {
            return true;
        }];

        CFErrorRef errorArg = cferror ? *cferror : NULL;
        action(sosPeerSet, errorArg);
        CFReleaseNull(sosPeerSet);
        return true;
    });
    CFReleaseNull(cfAccountError);
}

void SOSCCPerformWithPeerID(void (^action)(CFStringRef peerID, CFErrorRef error))
{
    CFErrorRef cfAccountError = NULL;
    do_with_account_if_after_first_unlock(&cfAccountError, ^bool(SOSAccountTransaction *txn, CFErrorRef *cferror) {
        SOSAccount* account = txn.account;
        NSString* peerID = nil;
        CFErrorRef localError = nil;
        
        if([account getCircleStatus:nil] == kSOSCCInCircle){
            peerID = [txn.account peerID];
        }
        else{
            SOSErrorCreate(kSOSErrorNoCircle, &localError, NULL, CFSTR("Not in circle"));
        }
        action((__bridge CFStringRef)peerID, localError);
        CFReleaseNull(localError);
        return true;
    });
    CFReleaseNull(cfAccountError);
}

void
SOSCCAccountTriggerSyncWithBackupPeer_server(CFStringRef peer)
{
#if OCTAGON
    secnotice("syncwith", "SOSCCAccountTriggerSyncWithBackupPeer_server: %@", peer);
    if (peer == NULL) {
        return;
    }
    SOSAccount* account = (__bridge SOSAccount*)GetSharedAccountRef();
    [account triggerBackupForPeers:@[(__bridge NSString *)peer]];
#endif
}

