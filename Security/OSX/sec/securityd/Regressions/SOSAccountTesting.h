/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#ifndef SEC_SOSAccountTesting_h
#define SEC_SOSAccountTesting_h

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>

#include "SOSTestDataSource.h"
#include "SOSRegressionUtilities.h"

#include "SOSTransportTestTransports.h"

#include <utilities/SecCFWrappers.h>

//
// Implicit transaction helpers
//

static inline bool SOSAccountResetToOffering_wTxn(SOSAccountRef account, CFErrorRef* error)
{
    __block bool result = false;
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        result = SOSAccountResetToOffering(txn, error);
    });
    return result;
}

static inline bool SOSAccountJoinCirclesAfterRestore_wTxn(SOSAccountRef account, CFErrorRef* error)
{
    __block bool result = false;
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        result = SOSAccountJoinCirclesAfterRestore(txn, error);
    });
    return result;
}

static inline bool SOSAccountJoinCircles_wTxn(SOSAccountRef account, CFErrorRef* error)
{
    __block bool result = false;
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        result = SOSAccountJoinCircles(txn, error);
    });
    return result;
}

static inline bool SOSAccountCheckHasBeenInSync_wTxn(SOSAccountRef account)
{
    return SOSAccountHasCompletedInitialSync(account);
}

static inline void SOSAccountPeerGotInSync_wTxn(SOSAccountRef account, SOSPeerInfoRef peer)
{
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        CFMutableSetRef views = SOSPeerInfoCopyEnabledViews(peer);
        SOSAccountPeerGotInSync(txn, SOSPeerInfoGetPeerID(peer), views);
        CFReleaseNull(views);
    });
}

static inline bool SOSAccountSetBackupPublicKey_wTxn(SOSAccountRef account, CFDataRef backupKey, CFErrorRef* error)
{
    __block bool result = false;
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        result = SOSAccountSetBackupPublicKey(txn, backupKey, error);
    });
    return result;
}

static inline bool SOSAccountRemoveBackupPublickey_wTxn(SOSAccountRef account, CFErrorRef* error)
{
    __block bool result = false;
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        result = SOSAccountRemoveBackupPublickey(txn, error);
    });
    return result;
}

static inline SOSViewResultCode SOSAccountUpdateView_wTxn(SOSAccountRef account, CFStringRef viewname, SOSViewActionCode actionCode, CFErrorRef *error) {
    __block SOSViewResultCode result = false;
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        result = SOSAccountUpdateView(account, viewname, actionCode, error);
    });
    return result;
}

static inline bool SOSAccountSetMyDSID_wTxn(SOSAccountRef account, CFStringRef dsid, CFErrorRef* error)
{
    __block bool result = false;
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        result = SOSAccountSetMyDSID(txn, dsid, error);
    });
    return result;
}

//
// Account comparison
//

#define kAccountsAgreeTestMin 9
#define kAccountsAgreeTestPerPeer 1
#define accountsAgree(x) (kAccountsAgreeTestMin + kAccountsAgreeTestPerPeer * (x))

static void SOSAccountResetToTest(SOSAccountRef a, CFStringRef accountName) {
    SOSUnregisterTransportKeyParameter(a->key_transport);

    CFReleaseNull(a->circle_transport);
    CFReleaseNull(a->kvs_message_transport);
    CFReleaseNull(a->key_transport);

    SOSAccountEnsureFactoryCirclesTest(a, accountName);
}


static SOSAccountRef SOSAccountCreateBasicTest(CFAllocatorRef allocator,
                                               CFStringRef accountName,
                                               CFDictionaryRef gestalt,
                                               SOSDataSourceFactoryRef factory) {
    SOSAccountRef a = SOSAccountCreateBasic(allocator, gestalt, factory);
    
    return a;
}

static SOSAccountRef SOSAccountCreateTest(CFAllocatorRef allocator,
                                          CFStringRef accountName,
                                          CFDictionaryRef gestalt,
                                          SOSDataSourceFactoryRef factory) {
    SOSAccountRef a = SOSAccountCreateBasicTest(allocator, accountName, gestalt, factory);
    
    SOSAccountResetToTest(a, accountName);

    return a;
}

static SOSAccountRef SOSAccountCreateTestFromData(CFAllocatorRef allocator,
                                                  CFDataRef data,
                                                  CFStringRef accountName,
                                                  SOSDataSourceFactoryRef factory) {
    SOSAccountRef a = SOSAccountCreateFromData(allocator, data, factory, NULL);
    if (!a) {
        CFDictionaryRef gestalt = SOSCreatePeerGestaltFromName(accountName);
        a = SOSAccountCreate(allocator, gestalt, factory);
        CFReleaseNull(gestalt);
    }
    
    SOSAccountResetToTest(a, accountName);

    return a;
}


static inline bool SOSAccountAssertUserCredentialsAndUpdate(SOSAccountRef account,
                                                     CFStringRef user_account, CFDataRef user_password,
                                                     CFErrorRef *error)
{
    bool success = false;
    success = SOSAccountAssertUserCredentials(account, user_account, user_password, error);
    require_quiet(success, done);
    
    success = SOSAccountGenerationSignatureUpdate(account, error);
    
done:
    return success;
}



static void unretired_peers_is_subset(const char* label, CFArrayRef peers, CFSetRef allowed_peers)
{
    CFArrayForEach(peers, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;

        CFErrorRef leftError = NULL;
        CFErrorRef rightError = NULL;
        
        ok(SOSPeerInfoIsRetirementTicket(pi) || SOSPeerInfoIsCloudIdentity(pi) || CFSetContainsValue(allowed_peers, pi), "Peer is allowed (%s) Peer: %@, Allowed %@", label, pi, allowed_peers);
        
        CFReleaseNull(leftError);
        CFReleaseNull(rightError);
    });
}

static void accounts_agree_internal(char *label, SOSAccountRef left, SOSAccountRef right, bool check_peers)
{
    CFErrorRef error = NULL;
    {
        CFArrayRef leftPeers = SOSAccountCopyActivePeers(left, &error);
        ok(leftPeers, "Left peers (%@) - %s", error, label);
        CFReleaseNull(error);

        CFArrayRef rightPeers = SOSAccountCopyActivePeers(right, &error);
        ok(rightPeers, "Right peers (%@) - %s", error, label);
        CFReleaseNull(error);

        ok(CFEqual(leftPeers, rightPeers), "Matching peers (%s) Left: %@, Right: %@", label, leftPeers, rightPeers);

        if (check_peers) {
            CFMutableSetRef allowed_identities = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);

            SOSFullPeerInfoRef leftFullPeer = SOSAccountCopyAccountIdentityPeerInfo(left, kCFAllocatorDefault, NULL);

            if (leftFullPeer)
                CFSetAddValue(allowed_identities, SOSFullPeerInfoGetPeerInfo(leftFullPeer));

            CFReleaseNull(leftFullPeer);
            
            SOSFullPeerInfoRef rightFullPeer = SOSAccountCopyAccountIdentityPeerInfo(right, kCFAllocatorDefault, NULL);
            
            if (rightFullPeer)
                CFSetAddValue(allowed_identities, SOSFullPeerInfoGetPeerInfo(rightFullPeer));

            CFReleaseNull(rightFullPeer);

            unretired_peers_is_subset(label, leftPeers, allowed_identities);

            CFReleaseNull(allowed_identities);
        }

        CFReleaseNull(leftPeers);
        CFReleaseNull(rightPeers);
    }
    {
        CFArrayRef leftConcurringPeers = SOSAccountCopyConcurringPeers(left, &error);
        ok(leftConcurringPeers, "Left peers (%@) - %s", error, label);

        CFArrayRef rightConcurringPeers = SOSAccountCopyConcurringPeers(right, &error);
        ok(rightConcurringPeers, "Right peers (%@) - %s", error, label);

        ok(CFEqual(leftConcurringPeers, rightConcurringPeers), "Matching concurring peers Left: %@, Right: %@", leftConcurringPeers, rightConcurringPeers);

        CFReleaseNull(leftConcurringPeers);
        CFReleaseNull(rightConcurringPeers);
    }
    {
        CFArrayRef leftApplicants = SOSAccountCopyApplicants(left, &error);
        ok(leftApplicants, "Left Applicants (%@) - %s", error, label);

        CFArrayRef rightApplicants = SOSAccountCopyApplicants(right, &error);
        ok(rightApplicants, "Left Applicants (%@) - %s", error, label);

        ok(CFEqual(leftApplicants, rightApplicants), "Matching applicants (%s) Left: %@, Right: %@", label, leftApplicants, rightApplicants);

        CFReleaseNull(leftApplicants);
        CFReleaseNull(rightApplicants);
    }
}

static inline void accounts_agree(char *label, SOSAccountRef left, SOSAccountRef right)
{
    accounts_agree_internal(label, left, right, true);
}


//
// Change handling
//

static inline CFStringRef CFArrayCopyCompactDescription(CFArrayRef array) {
    if (!isArray(array))
        return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<Not an array! %@>"), array);
    
    CFMutableStringRef result = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));
    
    __block CFStringRef separator = CFSTR("");
    CFArrayForEach(array, ^(const void *value) {
        CFStringAppendFormat(result, NULL, CFSTR("%@%@"), separator, value);
        separator = CFSTR(",");
    });
    
    CFStringAppend(result, CFSTR("]"));
    
    CFReleaseSafe(separator);
    
    return result;
}

static inline CFStringRef SOSAccountCopyName(SOSAccountRef account) {
    SOSPeerInfoRef pi = SOSAccountGetMyPeerInfo(account);
    
    return pi ? CFStringCreateCopy(kCFAllocatorDefault, SOSPeerInfoGetPeerName(pi)) : CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("%@"), account);
}

static inline CFStringRef CopyChangesDescription(CFDictionaryRef changes) {
    
    CFStringRef pendingChanges = CFDictionaryCopyCompactDescription((CFDictionaryRef) CFDictionaryGetValue(changes, kCFNull));
    
    CFMutableStringRef peerTable = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));
    
    __block CFStringRef separator = CFSTR("");
    
    CFDictionaryForEach(changes, ^(const void *key, const void *value) {
        if (CFGetTypeID(key) == SOSAccountGetTypeID()) {
            CFStringRef accountName = SOSAccountCopyName((SOSAccountRef) key);
            CFStringRef arrayDescription = CFArrayCopyCompactDescription(value);
            
            CFStringAppendFormat(peerTable, NULL, CFSTR("%@%@:%@"), separator, accountName, arrayDescription);
            separator = CFSTR(", ");
            
            CFReleaseSafe(accountName);
            CFReleaseSafe(arrayDescription);
        }
    });
    
    CFStringAppend(peerTable, CFSTR("]"));
    
    CFStringRef result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<TestChanges %@ %@>"), pendingChanges, peerTable);
    CFReleaseNull(pendingChanges);
    CFReleaseNull(peerTable);
    
    return result;
};

static void CFDictionaryOverlayDictionary(CFMutableDictionaryRef target, CFMutableDictionaryRef overlay) {
    CFMutableSetRef keysToRemove = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    CFDictionaryForEach(overlay, ^(const void *key, const void *value) {
        const void *current_value = CFDictionaryGetValue(target, key);
        if (CFEqualSafe(current_value, value) || (isNull(value) && current_value == NULL)) {
            CFSetAddValue(keysToRemove, key);
        } else {
            CFDictionarySetValue(target, key, value);
        }
    });

    CFSetForEach(keysToRemove, ^(const void *value) {
        CFDictionaryRemoveValue(overlay, value);
    });

    CFReleaseNull(keysToRemove);
}

static void CFArrayAppendKeys(CFMutableArrayRef keys, CFDictionaryRef newKeysToAdd) {
    CFDictionaryForEach(newKeysToAdd, ^(const void *key, const void *value) {
        CFArrayAppendValue(keys, key);
    });
}

static bool AddNewChanges(CFMutableDictionaryRef changesRecord, CFMutableDictionaryRef newKeysAndValues, SOSAccountRef sender)
{
    __block bool changes_added = false;
    CFMutableDictionaryRef emptyDictionary = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(changesRecord, kCFNull, emptyDictionary);
    CFReleaseNull(emptyDictionary);

    CFDictionaryOverlayDictionary((CFMutableDictionaryRef) CFDictionaryGetValue(changesRecord, kCFNull), newKeysAndValues);

    CFDictionaryForEach(changesRecord, ^(const void *key, const void *value) {
        if (isArray(value) && (sender == NULL || sender != key)) {
            CFArrayAppendKeys((CFMutableArrayRef) value, newKeysAndValues);
            if (CFDictionaryGetCount(newKeysAndValues))
                changes_added = true;
        }
    });

    if (changes_added)
        secnotice("changes", "Changes from %@: %@", sender, newKeysAndValues);

    CFDictionaryRemoveAllValues(newKeysAndValues);

    return changes_added;
}

static bool FillAllChanges(CFMutableDictionaryRef changes) {
    __block bool changed = false;
    
    CFMutableSetRef changedAccounts = CFSetCreateMutable(kCFAllocatorDefault, 0, NULL);
    
    CFArrayForEach(key_transports, ^(const void *value) {
        SOSTransportKeyParameterTestRef tpt = (SOSTransportKeyParameterTestRef) value;
        if (AddNewChanges(changes, SOSTransportKeyParameterTestGetChanges(tpt), SOSTransportKeyParameterTestGetAccount(tpt))) {
            changed |= true;
            CFSetAddValue(changedAccounts, SOSTransportKeyParameterTestGetAccount(tpt));
        }
        SOSTransportKeyParameterTestClearChanges(tpt);
    });
    CFArrayForEach(circle_transports, ^(const void *value) {
        SOSTransportCircleTestRef tpt = (SOSTransportCircleTestRef) value;
        if (AddNewChanges(changes, SOSTransportCircleTestGetChanges(tpt), SOSTransportCircleTestGetAccount(tpt))) {
            changed |= true;
            CFSetAddValue(changedAccounts, SOSTransportCircleTestGetAccount(tpt));
        }
        SOSTransportCircleTestClearChanges(tpt);
    });
    CFArrayForEach(message_transports, ^(const void *value) {
        if(SOSTransportMessageGetTransportType((SOSTransportMessageRef)value, NULL) == kKVSTest){
            SOSTransportMessageTestRef tpt = (SOSTransportMessageTestRef) value;
            CFDictionaryRemoveValue(SOSTransportMessageTestGetChanges(tpt), kCFNull);
            if (AddNewChanges(changes, SOSTransportMessageTestGetChanges(tpt), SOSTransportMessageTestGetAccount((SOSTransportMessageRef)tpt))) {
                changed |= true;
                CFSetAddValue(changedAccounts, SOSTransportMessageTestGetAccount((SOSTransportMessageRef)tpt));
            }
            SOSTransportMessageTestClearChanges(tpt);
        }
        else if(SOSTransportMessageGetTransportType((SOSTransportMessageRef)value, NULL) == kIDSTest){
            SOSTransportMessageRef ids = (SOSTransportMessageRef) value;
            CFDictionaryRemoveValue(SOSTransportMessageIDSTestGetChanges(ids), kCFNull);
            if (AddNewChanges(changes, SOSTransportMessageIDSTestGetChanges(ids), SOSTransportMessageTestGetAccount(ids))) {
                changed |= true;
                CFSetAddValue(changedAccounts, SOSTransportMessageTestGetAccount(ids));
            }
            SOSTransportMessageIDSTestClearChanges(ids);
        }
    });
    
    secnotice("process-changes", "Accounts with change (%@): %@", changed ? CFSTR("YES") : CFSTR("NO"), changedAccounts);
    
    CFReleaseNull(changedAccounts);

    return changed;
}

static void FillChanges(CFMutableDictionaryRef changes, SOSAccountRef forAccount)
{
    CFArrayForEach(key_transports, ^(const void *value) {
        SOSTransportKeyParameterTestRef tpt = (SOSTransportKeyParameterTestRef) value;
        if(CFEqualSafe(forAccount, SOSTransportKeyParameterTestGetAccount(tpt))){
            AddNewChanges(changes, SOSTransportKeyParameterTestGetChanges(tpt), SOSTransportKeyParameterTestGetAccount(tpt));
            SOSTransportKeyParameterTestClearChanges(tpt);
        }
    });
    CFArrayForEach(circle_transports, ^(const void *value) {
        SOSTransportCircleTestRef tpt = (SOSTransportCircleTestRef) value;
        if(CFEqualSafe(forAccount, SOSTransportCircleTestGetAccount(tpt))){
            AddNewChanges(changes, SOSTransportCircleTestGetChanges(tpt), SOSTransportCircleTestGetAccount(tpt));
            SOSTransportCircleTestClearChanges(tpt);
        }
    });
    CFArrayForEach(message_transports, ^(const void *value) {
        if(SOSTransportMessageGetTransportType((SOSTransportMessageRef)value, NULL) == kKVSTest){
            SOSTransportMessageTestRef tpt = (SOSTransportMessageTestRef) value;
            if(CFEqualSafe(forAccount, SOSTransportMessageTestGetAccount((SOSTransportMessageRef)tpt))){
                CFDictionaryRemoveValue(SOSTransportMessageTestGetChanges(tpt), kCFNull);
                AddNewChanges(changes, SOSTransportMessageTestGetChanges(tpt), SOSTransportMessageTestGetAccount((SOSTransportMessageRef)tpt));
                SOSTransportMessageTestClearChanges(tpt);
            }
        }
        else{
            SOSTransportMessageRef tpt = (SOSTransportMessageRef) value;
            if(CFEqualSafe(forAccount, SOSTransportMessageTestGetAccount((SOSTransportMessageRef)tpt))){
                CFDictionaryRemoveValue(SOSTransportMessageIDSTestGetChanges(tpt), kCFNull);
                AddNewChanges(changes, SOSTransportMessageIDSTestGetChanges(tpt), SOSTransportMessageTestGetAccount((SOSTransportMessageRef)tpt));
                SOSTransportMessageIDSTestClearChanges(tpt);
            }
        }
    });

}

static inline void FillChangesMulti(CFMutableDictionaryRef changes, SOSAccountRef account, ...)
{
    SOSAccountRef next_account = account;
    va_list argp;
    va_start(argp, account);
    while(next_account != NULL) {
        FillChanges(changes, next_account);
        next_account = va_arg(argp, SOSAccountRef);
    }
}

static inline CFMutableArrayRef CFDictionaryCopyKeys(CFDictionaryRef dictionary)
{
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    CFArrayAppendKeys(result, dictionary);

    return result;
}

#define kFeedChangesToTestCount 1
static inline void FeedChangesTo(CFMutableDictionaryRef changes, SOSAccountRef account)
{
    CFDictionaryRef full_list = (CFDictionaryRef) CFDictionaryGetValue(changes, kCFNull);

    if (!isDictionary(full_list))
        return; // Nothing recorded to send!

    CFMutableArrayRef account_pending_keys = (CFMutableArrayRef)CFDictionaryGetValue(changes, account);

    if (!isArray(account_pending_keys)) {
        account_pending_keys = CFDictionaryCopyKeys(full_list);
        CFDictionaryAddValue(changes, account, account_pending_keys);
        CFReleaseSafe(account_pending_keys); // The dictionary keeps it, we don't retain it here.
    }

    CFMutableDictionaryRef account_pending_messages = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayForEach(account_pending_keys, ^(const void *value) {
        CFDictionaryAddValue(account_pending_messages, value, CFDictionaryGetValue(full_list, value));
    });

    secnotice("changes", "Changes for %@:", SOSTransportKeyParameterTestGetName((SOSTransportKeyParameterTestRef) account->key_transport));

    CFDictionaryForEach(account_pending_messages, ^(const void *key, const void *value) {
        secnotice("changes", "  %@", key);
    });

    __block CFMutableArrayRef handled = NULL;
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        __block CFErrorRef error = NULL;
        ok(handled = SOSTransportDispatchMessages(txn, account_pending_messages, &error), "SOSTransportHandleMessages failed (%@)", error);
        CFReleaseNull(error);
    });

    if (isArray(handled)) {
        CFArrayForEach(handled, ^(const void *value) {
            CFArrayRemoveAllValue(account_pending_keys, value);
        });
    }
    CFReleaseNull(account_pending_messages);
    CFReleaseNull(handled);
}

#define kFeedChangesToMultieTestCountPer 1

static inline void FeedChangesToMultiV(CFMutableDictionaryRef changes, va_list argp)
{
    SOSAccountRef account = NULL;
    while((account = va_arg(argp, SOSAccountRef)) != NULL) {
        FeedChangesTo(changes, account);
    }
}

static inline void FeedChangesToMulti(CFMutableDictionaryRef changes, ...)
{
    va_list argp;
    va_start(argp, changes);

    FeedChangesToMultiV(changes, argp);

    va_end(argp);
}

static inline void InjectChangeToMulti(CFMutableDictionaryRef changes,
                                       CFStringRef changeKey, CFTypeRef changeValue, ...)
{
    CFMutableDictionaryRef changes_to_send = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                                     changeKey, changeValue,
                                                                                     NULL);
    AddNewChanges(changes, changes_to_send, NULL);
    CFReleaseNull(changes_to_send);

    va_list argp;
    va_start(argp, changeValue);
    FeedChangesToMultiV(changes, argp);
    va_end(argp);
}

static inline bool ProcessChangesOnceV(CFMutableDictionaryRef changes, va_list argp)
{
    bool result = FillAllChanges(changes);

    FeedChangesToMultiV(changes, argp);

    return result;
}


static inline bool ProcessChangesOnce(CFMutableDictionaryRef changes, ...)
{
    va_list argp;
    va_start(argp, changes);

    bool result = ProcessChangesOnceV(changes, argp);

    va_end(argp);

    return result;
}

static inline int ProcessChangesUntilNoChange(CFMutableDictionaryRef changes, ...)
{
    va_list argp;
    va_start(argp, changes);

    int result = 0;
    bool new_data = false;
    do {
        va_list argp_copy;
        va_copy(argp_copy, argp);
        
        new_data = ProcessChangesOnceV(changes, argp_copy);

        ++result;

        va_end(argp_copy);
    } while (new_data);
    
    va_end(argp);

    return result;
    
}

//
// MARK: Account creation
//

static CFStringRef modelFromType(SOSPeerInfoDeviceClass cls) {
    switch(cls) {
        case SOSPeerInfo_macOS: return CFSTR("Mac Pro");
        case SOSPeerInfo_iOS: return CFSTR("iPhone");
        case SOSPeerInfo_iCloud: return CFSTR("iCloud");
        case SOSPeerInfo_watchOS: return CFSTR("needWatchOSDeviceName");
        case SOSPeerInfo_tvOS: return CFSTR("needTVOSDeviceName");
        default: return CFSTR("GENERICOSTHING");
    }
}

static inline SOSAccountRef CreateAccountForLocalChangesWithStartingAttributes(CFStringRef name, CFStringRef data_source_name, SOSPeerInfoDeviceClass devclass, CFStringRef serial, CFBooleanRef preferIDS, CFBooleanRef preferIDSFragmentation, CFBooleanRef preferIDSACKModel, CFStringRef transportType, CFStringRef deviceID) {
    
    SOSDataSourceFactoryRef factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef ds = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(factory, data_source_name, ds);
    SOSEngineRef engine = SOSEngineCreate(ds, NULL);
    ds->engine = engine;
    
    CFMutableDictionaryRef gestalt = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(gestalt, kPIUserDefinedDeviceNameKey, name);
    CFDictionaryAddValue(gestalt, kPIDeviceModelNameKey, modelFromType(devclass));
    CFDictionaryAddValue(gestalt, kPIOSVersionKey, CFSTR("TESTRUN"));
    
    CFMutableDictionaryRef testV2dict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(testV2dict, sSerialNumberKey, serial);
    CFDictionaryAddValue(testV2dict, sPreferIDS, preferIDS);
    CFDictionaryAddValue(testV2dict, sPreferIDSFragmentation, preferIDSFragmentation);
    CFDictionaryAddValue(testV2dict, sPreferIDSACKModel, preferIDSACKModel);
    CFDictionaryAddValue(testV2dict, sTransportType, transportType);
    CFDictionaryAddValue(testV2dict, sDeviceID, deviceID);
    SOSAccountRef result = SOSAccountCreateTest(kCFAllocatorDefault, name, gestalt, factory);
    SOSAccountUpdateV2Dictionary(result, testV2dict);

    CFReleaseSafe(SOSAccountCopyUUID(result));

    CFReleaseNull(gestalt);
    CFReleaseNull(testV2dict);
    
    return result;
}

static CFStringRef sGestaltTest    = CFSTR("GestaltTest");
static CFStringRef sV2Test         = CFSTR("V2Test");
static inline CFDictionaryRef SOSTestSaveStaticAccountState(SOSAccountRef account) {
    CFMutableDictionaryRef retval = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef gestalt = SOSAccountCopyGestalt(account);
    CFDictionaryRef v2dictionary = SOSAccountCopyV2Dictionary(account);
    CFDictionaryAddValue(retval, sGestaltTest, gestalt);
    CFDictionaryAddValue(retval, sV2Test, v2dictionary);
    CFReleaseNull(gestalt);
    CFReleaseNull(v2dictionary);
    return retval;
}

static inline void SOSTestRestoreAccountState(SOSAccountRef account, CFDictionaryRef saved) {
    SOSAccountUpdateGestalt(account, CFDictionaryGetValue(saved, sGestaltTest));
    SOSAccountUpdateV2Dictionary(account, CFDictionaryGetValue(saved, sV2Test));
}

static CFStringRef CFStringCreateRandomHexWithLength(size_t len) {
    if(len%2) len++;
    CFDataRef data = CFDataCreateWithRandomBytes(len/2);
    CFMutableStringRef retval = CFStringCreateMutable(kCFAllocatorDefault, len);
    CFStringAppendHexData(retval, data);
    CFReleaseNull(data);
    return retval;
}

static inline SOSAccountRef CreateAccountForLocalChanges(CFStringRef name, CFStringRef data_source_name)
{
    CFStringRef randomSerial = CFStringCreateRandomHexWithLength(8);
    CFStringRef randomDevID = CFStringCreateRandomHexWithLength(16);
    SOSAccountRef retval = CreateAccountForLocalChangesWithStartingAttributes(name, data_source_name, SOSPeerInfo_iOS, randomSerial,
                                                              kCFBooleanTrue, kCFBooleanTrue, kCFBooleanTrue, SOSTransportMessageTypeIDSV2, randomDevID);
    CFReleaseNull(randomSerial);
    CFReleaseNull(randomDevID);
    return retval;
}

static inline SOSAccountRef CreateAccountForLocalChangesFromData(CFDataRef flattenedData, CFStringRef name, CFStringRef data_source_name)
{
    SOSDataSourceFactoryRef factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef ds = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(factory, data_source_name, ds);
    SOSEngineRef engine = SOSEngineCreate(ds, NULL);
    ds->engine = engine;

    SOSAccountRef result = SOSAccountCreateTestFromData(kCFAllocatorDefault, flattenedData, name, factory);

    return result;
}



static inline int countPeers(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyPeers(account, &error);
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countActivePeers(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyActivePeers(account, &error);
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countActiveValidPeers(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyActiveValidPeers(account, &error);
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countApplicants(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);
    int retval = 0;
    
    if(applicants) retval = (int)CFArrayGetCount(applicants);
    CFReleaseNull(error);
    CFReleaseNull(applicants);
    return retval;
}


static inline void showActiveValidPeers(SOSAccountRef account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyActiveValidPeers(account, &error);
    CFArrayForEach(peers, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        ok(0, "Active Valid Peer %@", pi);
    });
    CFReleaseNull(peers);
}

#define ok_or_quit(COND,MESSAGE,LABEL) ok(COND, MESSAGE); if(!(COND)) goto LABEL

static inline bool testAccountPersistence(SOSAccountRef account) {
    SOSDataSourceFactoryRef test_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(test_factory, CFSTR("TestType"), test_source);
    CFErrorRef error = NULL;
    bool retval = false;
    SOSAccountRef reinflatedAccount = NULL;
    CFDataRef accountDER = NULL;

    SOSAccountCheckHasBeenInSync_wTxn(account);

    // DER encode account to accountData - this allows checking discreet DER functions
    size_t size = SOSAccountGetDEREncodedSize(account, &error);
    CFReleaseNull(error);
    uint8_t buffer[size];
    uint8_t* start = SOSAccountEncodeToDER(account, &error, buffer, buffer + sizeof(buffer));
    CFReleaseNull(error);

    ok_or_quit(start, "successful encoding", errOut);
    ok_or_quit(start == buffer, "Used whole buffer", errOut);

    accountDER = CFDataCreate(kCFAllocatorDefault, buffer, size);
    ok_or_quit(accountDER, "Made CFData for Account", errOut);


    // Re-inflate to "inflated"
    reinflatedAccount = SOSAccountCreateFromData(kCFAllocatorDefault, accountDER, test_factory, &error);
    CFReleaseNull(error);
    CFReleaseNull(accountDER);

    ok(reinflatedAccount, "inflated");
    ok(CFEqualSafe(reinflatedAccount, account), "Compares");

    // Repeat through SOSAccountCopyEncodedData() interface - this is the normally called combined interface
    accountDER = SOSAccountCopyEncodedData(reinflatedAccount, kCFAllocatorDefault, &error);
    CFReleaseNull(error);
    CFReleaseNull(reinflatedAccount);
    reinflatedAccount = SOSAccountCreateFromData(kCFAllocatorDefault, accountDER, test_factory, &error);
    ok(reinflatedAccount, "inflated2");
    ok(CFEqual(account, reinflatedAccount), "Compares");

    retval = true;
errOut:
    CFReleaseNull(reinflatedAccount);
    CFReleaseNull(accountDER);
    return retval;
}

static inline bool SOSTestStartCircleWithAccount(SOSAccountRef account, CFMutableDictionaryRef changes, CFStringRef cfaccount, CFDataRef cfpassword) {
    bool retval = false;
    require(SOSAccountAssertUserCredentialsAndUpdate(account, cfaccount, cfpassword, NULL), retOut);
    is(ProcessChangesUntilNoChange(changes, account, NULL), 1, "updates");
    require(SOSAccountResetToOffering_wTxn(account, NULL), retOut);
    is(ProcessChangesUntilNoChange(changes, account, NULL), 1, "updates");
    retval = true;
retOut:
    return retval;
}

static inline bool SOSTestApproveRequest(SOSAccountRef approver, CFIndex napplicants) {
    bool retval = false;
    CFErrorRef error = NULL;
    CFArrayRef applicants = SOSAccountCopyApplicants(approver, &error);
    
    ok(applicants && CFArrayGetCount(applicants) == napplicants, "See %ld applicant(s) %@ (%@)", napplicants, applicants, error);
    CFStringRef approvername = SOSAccountCopyName(approver);
    ok((retval = SOSAccountAcceptApplicants(approver, applicants, &error)), "%@ accepts (%@)", approvername, error);
    CFReleaseNull(error);
    CFReleaseNull(applicants);
    CFReleaseNull(approvername);
    
    return retval;
}

#define DROP_USERKEY true
#define KEEP_USERKEY false

static inline bool SOSTestJoinWith(CFDataRef cfpassword, CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccountRef joiner) {
    CFErrorRef error = NULL;
    // retval will return op failures, not count failures - we'll still report those from in here.
    bool retval = false;
    
    FeedChangesTo(changes, joiner);
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(joiner, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ProcessChangesUntilNoChange(changes, joiner, NULL);
    
    ok(retval = SOSAccountJoinCircles_wTxn(joiner, &error), "Applying (%@)", error);
    CFReleaseNull(error);
    return retval;
}

static inline bool SOSTestJoinWithApproval(CFDataRef cfpassword, CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccountRef approver, SOSAccountRef joiner, bool dropUserKey, int expectedCount, bool expectCleanup) {
    //CFErrorRef error = NULL;
    // retval will return op failures, not count failures - we'll still report those from in here.
    bool retval = false;
    
    ok(retval = SOSTestJoinWith(cfpassword, cfaccount, changes, joiner), "Applyication Made");
    
    is(ProcessChangesUntilNoChange(changes, approver, joiner, NULL), 2, "updates");
    
    int nrounds = 2;
    if(dropUserKey) SOSAccountPurgePrivateCredential(joiner);  // lose the userKey so we don't "fix" the ghost problem yet.
    else nrounds = 3;
    
    if(expectCleanup) nrounds++;
    
    ok(retval &= SOSTestApproveRequest(approver, 1), "Accepting Request to Join");
    is(ProcessChangesUntilNoChange(changes, approver, joiner, NULL), nrounds, "updates");
    
    accounts_agree_internal("Successful join shows same circle view", joiner, approver, false);
    is(countPeers(joiner), expectedCount, "There should be %d valid peers", expectedCount);
    return retval;
}


static inline SOSAccountRef SOSTestCreateAccountAsSerialClone(CFStringRef name, SOSPeerInfoDeviceClass devClass, CFStringRef serial, CFStringRef idsID) {
    return CreateAccountForLocalChangesWithStartingAttributes(name, CFSTR("TestSource"), devClass, serial, kCFBooleanTrue, kCFBooleanTrue, kCFBooleanTrue, SOSTransportMessageTypeIDSV2, idsID);
}

static inline bool SOSTestMakeGhostInCircle(CFStringRef name, SOSPeerInfoDeviceClass devClass, CFStringRef serial, CFStringRef idsID,
                                     CFDataRef cfpassword, CFStringRef cfaccount, CFMutableDictionaryRef changes,
                                     SOSAccountRef approver, int expectedCount) {
    bool retval = false;
    SOSAccountRef ghostAccount = SOSTestCreateAccountAsSerialClone(name, devClass, serial, idsID);
    ok(ghostAccount, "Created Ghost Account");
    require_quiet(ghostAccount, retOut);
    if(!ghostAccount) return false;
    ok(retval = SOSTestJoinWithApproval(cfpassword, cfaccount, changes, approver, ghostAccount, DROP_USERKEY, expectedCount, true), "Ghost Joined Circle with expected result");
    CFReleaseNull(ghostAccount);
retOut:
    return retval;
}

static inline bool SOSTestChangeAccountDeviceName(SOSAccountRef account, CFStringRef name) {
    bool retval = false;
    CFMutableDictionaryRef mygestalt = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, SOSPeerGetGestalt(SOSAccountGetMyPeerInfo(account)));
    require_quiet(mygestalt, retOut);
    CFDictionarySetValue(mygestalt, kPIUserDefinedDeviceNameKey, name);
    retval = SOSAccountUpdateGestalt(account, mygestalt);
retOut:
    CFReleaseNull(mygestalt);
    return retval;
}

static inline void SOSTestCleanup() {
    SOSUnregisterAllTransportMessages();
    SOSUnregisterAllTransportCircles();
    SOSUnregisterAllTransportKeyParameters();
    CFArrayRemoveAllValues(key_transports);
    CFArrayRemoveAllValues(circle_transports);
    CFArrayRemoveAllValues(message_transports);
}


#endif
