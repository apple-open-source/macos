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
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Identity.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"

#include "SOSTestDataSource.h"
#include "SOSRegressionUtilities.h"

#include "SOSTransportTestTransports.h"
#include "testmore.h"
#include <utilities/SecCFWrappers.h>
//
// Implicit transaction helpers
//

static inline bool SOSAccountResetToOffering_wTxn(SOSAccount* acct, CFErrorRef* error)
{
    __block bool result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        SecKeyRef user_key = SOSAccountGetPrivateCredential(txn.account, error);
        if (!user_key)
            return;
        result = [acct.trust resetToOffering:txn key:user_key err:error];
    }];
    return result;
}

static inline bool SOSAccountJoinCirclesAfterRestore_wTxn(SOSAccount* acct, CFErrorRef* error)
{
    __block bool result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountJoinCirclesAfterRestore(txn, error);
    }];
    return result;
}

static inline bool SOSAccountJoinCircles_wTxn(SOSAccount* acct, CFErrorRef* error)
{
    __block bool result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountJoinCircles(txn, error);
    }];
    return result;
}

static inline bool SOSAccountCheckHasBeenInSync_wTxn(SOSAccount* account)
{
    return SOSAccountHasCompletedInitialSync(account);
}

static inline void SOSAccountPeerGotInSync_wTxn(SOSAccount* acct, SOSPeerInfoRef peer)
{
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        CFMutableSetRef views = SOSPeerInfoCopyEnabledViews(peer);
        SOSAccountPeerGotInSync(txn, SOSPeerInfoGetPeerID(peer), views);
        CFReleaseNull(views);
    }];
}

static inline CFArrayRef SOSAccountCopyViewUnawarePeers_wTxn(SOSAccount* acct, CFErrorRef* error) {
    __block CFArrayRef result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountCopyViewUnaware(txn.account, error);
    }];
    return result;
}

static inline bool SOSAccountSetBackupPublicKey_wTxn(SOSAccount* acct, CFDataRef backupKey, CFErrorRef* error)
{
    __block bool result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountSetBackupPublicKey(txn, backupKey, error);
    }];
    return result;
}

static inline bool SOSAccountRemoveBackupPublickey_wTxn(SOSAccount* acct, CFErrorRef* error)
{
    __block bool result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountRemoveBackupPublickey(txn, error);
    }];
    return result;
}

static inline SOSViewResultCode SOSAccountUpdateView_wTxn(SOSAccount* acct, CFStringRef viewname, SOSViewActionCode actionCode, CFErrorRef *error) {
    __block SOSViewResultCode result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = [acct.trust updateView:acct name:viewname code:actionCode err:error];
    }];
    return result;
}

static inline bool SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(SOSAccount *account, CFStringRef viewname) {
    __block bool result = false;
    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountIsMyPeerInBackupAndCurrentInView(account, viewname);
    }];
    return result;
}

static inline bool SOSAccountIsPeerInBackupAndCurrentInView_wTxn(SOSAccount *account, SOSPeerInfoRef peerInfo, CFStringRef viewname) {
    __block bool result = false;
    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountIsPeerInBackupAndCurrentInView(account, peerInfo, viewname);
    }];
    return result;
}

static inline bool SOSAccountRecoveryKeyIsInBackupAndCurrentInView_wTxn(SOSAccount *account, CFStringRef viewname) {
    __block bool result = false;
    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountRecoveryKeyIsInBackupAndCurrentInView(account, viewname);
    }];
    return result;
}

static inline SOSBackupSliceKeyBagRef SOSAccountBackupSliceKeyBagForView_wTxn(SOSAccount *account, CFStringRef viewname, CFErrorRef *error) {
    __block SOSBackupSliceKeyBagRef result = NULL;
    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountBackupSliceKeyBagForView(account, viewname, error);
    }];
    return result;
}




//
// Account comparison
//

#define kAccountsAgreeTestMin 9
#define kAccountsAgreeTestPerPeer 1
#define accountsAgree(x) (kAccountsAgreeTestMin + kAccountsAgreeTestPerPeer * (x))

static void SOSAccountResetToTest(SOSAccount* a, CFStringRef accountName) {
    SOSUnregisterTransportKeyParameter(a.key_transport);
    SOSUnregisterTransportCircle((SOSCircleStorageTransport*)a.circle_transport);
    SOSUnregisterTransportMessage((SOSMessage*)a.kvs_message_transport);

    if(key_transports)
        CFArrayRemoveAllValue(key_transports, (__bridge CFTypeRef)(a.key_transport));
    if(message_transports){
        CFArrayRemoveAllValue(message_transports, (__bridge CFTypeRef)a.kvs_message_transport);
    }
    if(circle_transports)
        CFArrayRemoveAllValue(circle_transports, (__bridge CFTypeRef)a.circle_transport);

    a.circle_transport = nil;
    a.key_transport = nil;
    a.kvs_message_transport = nil;

    [a performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        SOSAccountEnsureFactoryCirclesTest(a, accountName);
    }];
}


static SOSAccount* SOSAccountCreateBasicTest(CFAllocatorRef allocator,
                                               CFStringRef accountName,
                                               CFDictionaryRef gestalt,
                                               SOSDataSourceFactoryRef factory) {
    SOSAccount* a;
    a =  SOSAccountCreate(kCFAllocatorDefault, gestalt, factory);
    
    return a;
}

static SOSAccount* SOSAccountCreateTest(CFAllocatorRef allocator,
                                          CFStringRef accountName,
                                          CFDictionaryRef gestalt,
                                          SOSDataSourceFactoryRef factory) {
    SOSAccount* a = SOSAccountCreateBasicTest(allocator, accountName, gestalt, factory);
    
    SOSAccountResetToTest(a, accountName);
    if(a)
        SOSAccountInflateTestTransportsForCircle(a, SOSCircleGetName([a.trust getCircle:NULL]), accountName, NULL);
    return a;
}

static SOSAccount* SOSAccountCreateTestFromData(CFAllocatorRef allocator,
                                                  CFDataRef data,
                                                  CFStringRef accountName,
                                                  SOSDataSourceFactoryRef factory) {
    SOSAccount* a = [SOSAccount accountFromData:(__bridge NSData*) data
                                        factory:factory
                                          error:nil];
    if (!a) {
        CFDictionaryRef gestalt = SOSCreatePeerGestaltFromName(accountName);
        a = SOSAccountCreate(allocator, gestalt, factory);
        CFReleaseNull(gestalt);
    }
    
    SOSAccountResetToTest(a, accountName);
    if(a)
        SOSAccountInflateTestTransportsForCircle(a, SOSCircleGetName([a.trust getCircle:NULL]), accountName, NULL);

    return a;
}


static inline bool SOSAccountAssertUserCredentialsAndUpdate(SOSAccount* account,
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

static void accounts_agree_internal(char *label, SOSAccount* left, SOSAccount* right, bool check_peers)
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

            SOSFullPeerInfoRef leftFullPeer = [left.trust CopyAccountIdentityPeerInfo];

            if (leftFullPeer)
                CFSetAddValue(allowed_identities, SOSFullPeerInfoGetPeerInfo(leftFullPeer));
            CFReleaseNull(leftFullPeer);

            SOSFullPeerInfoRef rightFullPeer = [right.trust CopyAccountIdentityPeerInfo];
            
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

static inline void accounts_agree(char *label, SOSAccount* left, SOSAccount* right)
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

static inline CFStringRef SOSAccountCopyName(SOSAccount* account) {
    SOSPeerInfoRef pi = account.peerInfo;
    
    return pi ? CFStringCreateCopy(kCFAllocatorDefault, SOSPeerInfoGetPeerName(pi)) : CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("%@"), account);
}

static inline CFStringRef CopyChangesDescription(CFDictionaryRef changes) {
    
    CFStringRef pendingChanges = CFDictionaryCopyCompactDescription((CFDictionaryRef) CFDictionaryGetValue(changes, kCFNull));
    
    CFMutableStringRef peerTable = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));
    
    __block CFStringRef separator = CFSTR("");
    
    CFDictionaryForEach(changes, ^(const void *key, const void *value) {
        if (CFGetTypeID(key) == SOSAccountGetTypeID()) {
            CFStringRef accountName = SOSAccountCopyName((__bridge SOSAccount*)key);
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

static bool AddNewChanges(CFMutableDictionaryRef changesRecord, CFMutableDictionaryRef newKeysAndValues, SOSAccount* sender)
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
        CKKeyParameterTest* tpt = (__bridge CKKeyParameterTest*) value;
        if (AddNewChanges(changes, SOSTransportKeyParameterTestGetChanges(tpt), SOSTransportKeyParameterTestGetAccount(tpt))) {
            changed |= true;
            CFSetAddValue(changedAccounts, (__bridge CFTypeRef)(SOSTransportKeyParameterTestGetAccount(tpt)));
        }
        SOSTransportKeyParameterTestClearChanges(tpt);
    });
    CFArrayForEach(circle_transports, ^(const void *value) {
        SOSCircleStorageTransportTest *tpt = (__bridge SOSCircleStorageTransportTest *) value;
        if (AddNewChanges(changes, [tpt SOSTransportCircleTestGetChanges], [tpt getAccount])) {
            changed |= true;
            CFSetAddValue(changedAccounts, (__bridge CFTypeRef)SOSTransportCircleTestGetAccount(tpt));
        }
        SOSTransportCircleTestClearChanges(tpt);
    });
    CFArrayForEach(message_transports, ^(const void *value) {
        if([(__bridge SOSMessage*)value SOSTransportMessageGetTransportType] == kKVSTest){
            SOSMessageKVSTest* tpt = (__bridge SOSMessageKVSTest*) value;
            CFDictionaryRemoveValue(SOSTransportMessageKVSTestGetChanges(tpt), kCFNull);
            if (AddNewChanges(changes, SOSTransportMessageKVSTestGetChanges(tpt), SOSTransportMessageKVSTestGetAccount(tpt))) {
                changed |= true;
                CFSetAddValue(changedAccounts, (__bridge CFTypeRef)(SOSTransportMessageKVSTestGetAccount(tpt)));
            }
            SOSTransportMessageTestClearChanges(tpt);
        }
    });
    
    secnotice("process-changes", "Accounts with change (%@): %@", changed ? CFSTR("YES") : CFSTR("NO"), changedAccounts);
    
    CFReleaseNull(changedAccounts);

    return changed;
}

static void FillChanges(CFMutableDictionaryRef changes, SOSAccount* forAccount)
{
    CFArrayForEach(key_transports, ^(const void *value) {
        CKKeyParameterTest* tpt = (__bridge CKKeyParameterTest*) value;
        if(CFEqualSafe((__bridge CFTypeRef)(forAccount), (__bridge CFTypeRef)(SOSTransportKeyParameterTestGetAccount(tpt)))){
            AddNewChanges(changes, SOSTransportKeyParameterTestGetChanges(tpt), SOSTransportKeyParameterTestGetAccount(tpt));
            SOSTransportKeyParameterTestClearChanges(tpt);
        }
    });
    CFArrayForEach(circle_transports, ^(const void *value) {
        SOSCircleStorageTransportTest* tpt = (__bridge SOSCircleStorageTransportTest*) value;
        if([forAccount isEqual: SOSTransportCircleTestGetAccount(tpt)]){
            AddNewChanges(changes, [tpt SOSTransportCircleTestGetChanges], SOSTransportCircleTestGetAccount(tpt));
            SOSTransportCircleTestClearChanges(tpt);
        }
    });
    CFArrayForEach(message_transports, ^(const void *value) {
        if([(__bridge SOSMessage*)value SOSTransportMessageGetTransportType] == kKVSTest){
            SOSMessageKVSTest* tpt = (__bridge SOSMessageKVSTest*) value;
            if(CFEqualSafe((__bridge CFTypeRef)(forAccount), (__bridge CFTypeRef)(SOSTransportMessageKVSTestGetAccount(tpt)))){
                CFDictionaryRemoveValue(SOSTransportMessageKVSTestGetChanges(tpt), kCFNull);
                AddNewChanges(changes, SOSTransportMessageKVSTestGetChanges(tpt), SOSTransportMessageKVSTestGetAccount(tpt));
                SOSTransportMessageTestClearChanges(tpt);
            }
        }
    });

}

static inline void FillChangesMulti(CFMutableDictionaryRef changes, SOSAccount* account, ...)
{
    SOSAccount* next_account = account;
    va_list argp;
    va_start(argp, account);
    while(next_account != NULL) {
        FillChanges(changes, next_account);
        next_account = va_arg(argp, SOSAccount*);
    }
}

static inline CFMutableArrayRef CFDictionaryCopyKeys(CFDictionaryRef dictionary)
{
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    CFArrayAppendKeys(result, dictionary);

    return result;
}

#define kFeedChangesToTestCount 1
static inline void FeedChangesTo(CFMutableDictionaryRef changes, SOSAccount* acct)
{
    CFDictionaryRef full_list = (CFDictionaryRef) CFDictionaryGetValue(changes, kCFNull);

    if (!isDictionary(full_list))
        return; // Nothing recorded to send!

    CFMutableArrayRef account_pending_keys = (CFMutableArrayRef)CFDictionaryGetValue(changes, (__bridge CFTypeRef)(acct));

    if (!isArray(account_pending_keys)) {
        account_pending_keys = CFDictionaryCopyKeys(full_list);
        CFDictionaryAddValue(changes, (__bridge CFTypeRef)(acct), account_pending_keys);
        CFReleaseSafe(account_pending_keys); // The dictionary keeps it, we don't retain it here.
    }

    CFMutableDictionaryRef account_pending_messages = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayForEach(account_pending_keys, ^(const void *value) {
        CFDictionaryAddValue(account_pending_messages, value, CFDictionaryGetValue(full_list, value));
    });

    secnotice("changes", "Changes for %@:", SOSTransportKeyParameterTestGetName((CKKeyParameterTest*) acct.key_transport));

    CFDictionaryForEach(account_pending_messages, ^(const void *key, const void *value) {
        secnotice("changes", "  %@", key);
    });

    if(CFDictionaryGetCount(account_pending_messages) == 0) {
        CFReleaseNull(account_pending_messages);
        return;
    }
    
    __block CFMutableArrayRef handled = NULL;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        __block CFErrorRef error = NULL;
        ok(handled = SOSTransportDispatchMessages(txn, account_pending_messages, &error), "SOSTransportHandleMessages failed (%@)", error);
        CFReleaseNull(error);
    }];

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
    SOSAccount* account = NULL;
    while((account = va_arg(argp, SOSAccount*)) != NULL) {
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

static inline SOSAccount* CreateAccountForLocalChangesWithStartingAttributes(CFStringRef name, CFStringRef data_source_name, SOSPeerInfoDeviceClass devclass, CFStringRef serial, CFBooleanRef preferIDS, CFBooleanRef preferIDSFragmentation, CFBooleanRef preferIDSACKModel, CFStringRef transportType, CFStringRef deviceID) {
    
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
    SOSAccount* result = SOSAccountCreateTest(kCFAllocatorDefault, name, gestalt, factory);
    [result.trust updateV2Dictionary:result v2:testV2dict];

    CFReleaseSafe(SOSAccountCopyUUID(result));

    CFReleaseNull(gestalt);
    CFReleaseNull(testV2dict);
    
    return result;
}

static CFStringRef sGestaltTest    = CFSTR("GestaltTest");
static CFStringRef sV2Test         = CFSTR("V2Test");
static inline CFDictionaryRef SOSTestSaveStaticAccountState(SOSAccount* account) {    
    CFMutableDictionaryRef retval = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef gestalt = SOSAccountCopyGestalt(account);
    CFDictionaryRef v2dictionary = SOSAccountCopyV2Dictionary(account);
    CFDictionaryAddValue(retval, sGestaltTest, gestalt);
    CFDictionaryAddValue(retval, sV2Test, v2dictionary);
    CFReleaseNull(gestalt);
    CFReleaseNull(v2dictionary);
    return retval;
}

static inline void SOSTestRestoreAccountState(SOSAccount* account, CFDictionaryRef saved) {
    [account.trust updateGestalt:account newGestalt:CFDictionaryGetValue(saved, sGestaltTest)];
    [account.trust updateV2Dictionary:account v2:CFDictionaryGetValue(saved, sV2Test)];
}

static CFStringRef CFStringCreateRandomHexWithLength(size_t len) {
    if(len%2) len++;
    CFDataRef data = CFDataCreateWithRandomBytes(len/2);
    CFMutableStringRef retval = CFStringCreateMutable(kCFAllocatorDefault, len);
    CFStringAppendHexData(retval, data);
    CFReleaseNull(data);
    return retval;
}

static inline SOSAccount* CreateAccountForLocalChanges(CFStringRef name, CFStringRef data_source_name)
{
    CFStringRef randomSerial = CFStringCreateRandomHexWithLength(8);
    CFStringRef randomDevID = CFStringCreateRandomHexWithLength(16);
    SOSAccount* retval = CreateAccountForLocalChangesWithStartingAttributes(name, data_source_name, SOSPeerInfo_iOS, randomSerial,
                                                              kCFBooleanTrue, kCFBooleanTrue, kCFBooleanTrue, SOSTransportMessageTypeKVS, randomDevID);

    CFReleaseNull(randomSerial);
    CFReleaseNull(randomDevID);
    return retval;
}

static inline SOSAccount* CreateAccountForLocalChangesFromData(CFDataRef flattenedData, CFStringRef name, CFStringRef data_source_name)
{
    SOSDataSourceFactoryRef factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef ds = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(factory, data_source_name, ds);
    SOSEngineRef engine = SOSEngineCreate(ds, NULL);
    ds->engine = engine;

    SOSAccount* result = SOSAccountCreateTestFromData(kCFAllocatorDefault, flattenedData, name, factory);

    return result;
}



static inline int countPeers(SOSAccount* account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;

    peers = SOSAccountCopyPeers(account, &error);
    if(!peers)
        return 0;
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countActivePeers(SOSAccount* account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyActivePeers(account, &error);
    if(!peers)
        return 0;
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countActiveValidPeers(SOSAccount* account) {
    CFErrorRef error = NULL;
    CFArrayRef peers;
    
    peers = SOSAccountCopyActiveValidPeers(account, &error);
    if(!peers)
        return 0;
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}

static inline int countApplicants(SOSAccount* account) {
    CFErrorRef error = NULL;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);
    int retval = 0;
    
    if(applicants) retval = (int)CFArrayGetCount(applicants);
    CFReleaseNull(error);
    CFReleaseNull(applicants);
    return retval;
}


static inline void showActiveValidPeers(SOSAccount* account) {
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

static inline bool testAccountPersistence(SOSAccount* account) {

    __block bool retval = false;
    __block NSData* accountDER = NULL;

    SOSDataSourceFactoryRef test_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(test_factory, CFSTR("TestType"), test_source);

    SOSAccountCheckHasBeenInSync_wTxn(account);

    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        NSError* error = nil;

        // DER encode account to accountData - this allows checking discreet DER functions
        size_t size = [account.trust getDEREncodedSize:account err:&error];
        error = nil;
        uint8_t buffer[size];
        uint8_t* start = [account.trust encodeToDER:account err:&error start:buffer end:buffer + sizeof(buffer)];
        error = nil;

        ok_or_quit(start, "successful encoding", errOut);
        ok_or_quit(start == buffer, "Used whole buffer", errOut);

        accountDER = [NSData dataWithBytes:buffer length:size];
        ok_or_quit(accountDER, "Made CFData for Account", errOut);

        retval = true;
    errOut:
        do {} while(0);
    }];

    SOSAccount* reinflatedAccount = NULL;
    NSError* error = nil;

    if(!retval) {
        error = nil;
        return retval;
    }

    // Re-inflate to "inflated"
    reinflatedAccount = [SOSAccount accountFromData:accountDER
                                            factory:test_factory
                                              error:&error];
    ok(reinflatedAccount, "inflated: %@", error);
    error = nil;

    ok(CFEqualSafe((__bridge CFTypeRef)reinflatedAccount, (__bridge CFTypeRef)account), "Compares");

    // Repeat through SOSAccountCopyEncodedData() interface - this is the normally called combined interface
    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        NSError* error = nil;
        accountDER = [account encodedData:&error];
    }];

    error = nil;
    SOSAccount* reinflatedAccount2 = NULL;

    reinflatedAccount2 = [SOSAccount accountFromData:accountDER factory:test_factory error:&error];
    ok(reinflatedAccount2, "inflated2: %@", error);
    ok(CFEqual((__bridge CFTypeRef)account, (__bridge CFTypeRef)reinflatedAccount2), "Compares");

    retval = true;
    error = nil;
    return retval;
}

static inline bool SOSTestStartCircleWithAccount(SOSAccount* account, CFMutableDictionaryRef changes, CFStringRef cfaccount, CFDataRef cfpassword) {
    bool retval = false;
    if(!SOSAccountAssertUserCredentialsAndUpdate(account, cfaccount, cfpassword, NULL))
        return retval;
    is(ProcessChangesUntilNoChange(changes, account, NULL), 1, "updates");
    if(!SOSAccountResetToOffering_wTxn(account, NULL))
        return retval;
    is(ProcessChangesUntilNoChange(changes, account, NULL), 1, "updates");
    retval = true;

    return retval;
}

static inline bool SOSTestApproveRequest(SOSAccount* approver, CFIndex napplicants) {
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

static inline bool SOSTestJoinWith(CFDataRef cfpassword, CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccount* joiner) {
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

static inline bool SOSTestJoinWithApproval(CFDataRef cfpassword, CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccount* approver, SOSAccount* joiner, bool dropUserKey, int expectedCount, bool expectCleanup) {
    //CFErrorRef error = NULL;
    // retval will return op failures, not count failures - we'll still report those from in here.
    bool retval = false;
    
    ok(retval = SOSTestJoinWith(cfpassword, cfaccount, changes, joiner), "Application Made");
    
    ProcessChangesUntilNoChange(changes, approver, joiner, NULL);
    
    int nrounds = 2;
    if(dropUserKey) SOSAccountPurgePrivateCredential(joiner);  // lose the userKey so we don't "fix" the ghost problem yet.
    else nrounds = 3;
    
    if(expectCleanup) nrounds++;
    
    ok(retval &= SOSTestApproveRequest(approver, 1), "Accepting Request to Join");
    ProcessChangesUntilNoChange(changes, approver, joiner, NULL);
    
    accounts_agree_internal("Successful join shows same circle view", joiner, approver, false);
    is(countPeers(joiner), expectedCount, "There should be %d valid peers", expectedCount);
    return retval;
}


static inline bool SOSTestChangeAccountDeviceName(SOSAccount* account, CFStringRef name) {
    bool retval = false;
    CFMutableDictionaryRef mygestalt = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, SOSPeerGetGestalt(account.peerInfo));
    require_quiet(mygestalt, retOut);
    CFDictionarySetValue(mygestalt, kPIUserDefinedDeviceNameKey, name);
    retval = [account.trust updateGestalt:account newGestalt:mygestalt];
retOut:
    CFReleaseNull(mygestalt);
    return retval;
}

/*
 * this simulates a piggy-back join at the account level
 */

static inline bool SOSTestJoinThroughPiggyBack(CFDataRef cfpassword, CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccount* approver, SOSAccount* joiner, bool dropUserKey, int expectedCount, bool expectCleanup) {
    // retval will return op failures, not count failures - we'll still report those from in here.
    bool retval = false;
    CFErrorRef error = NULL;

    ok(SOSAccountAssertUserCredentialsAndUpdate(approver, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    // This makes sure the joiner sees the current key parameters
    ProcessChangesUntilNoChange(changes, approver, joiner, NULL);
    
    SecKeyRef privKey = SOSAccountGetPrivateCredential(approver, &error);
    ok(privKey, "got privkey from approver (%@)", error);
    CFReleaseNull(error);

    ok(SOSAccountTryUserPrivateKey(joiner, privKey, &error), "assert same credentials on joiner (%@)", error);
    CFReleaseNull(error);
    // This gives the joiner a chance to see the current circle - this is the account-level equivalent of the Flush added to stashAccountCredential
    ProcessChangesUntilNoChange(changes, approver, joiner, NULL);

    SOSPeerInfoRef joinerPI = SOSAccountCopyApplication(joiner, &error);
    ok(joinerPI, "Joiner peerinfo available as application %@", error);
    CFReleaseNull(error);

    CFDataRef theBlob = SOSAccountCopyCircleJoiningBlob(approver, joinerPI, &error);
    ok(theBlob, "Made a joining blob (%@)", error);
    CFReleaseNull(error);
    

    bool joined = SOSAccountJoinWithCircleJoiningBlob(joiner, theBlob, kPiggyV1, &error);
    ok(joined, "Joiner posted circle with itself in it (%@)", error);
    CFReleaseNull(error);

    CFReleaseNull(joinerPI);
    CFReleaseNull(theBlob);

    is(ProcessChangesUntilNoChange(changes, approver, joiner, NULL), 2, "updates");
    
    ok((retval = [joiner isInCircle:NULL]), "Joiner is in");
    
    accounts_agree_internal("Successful join shows same circle view", joiner, approver, false);
    is(countPeers(joiner), expectedCount, "There should be %d valid peers", expectedCount);
    return retval;
}


static inline SOSAccount* SOSTestCreateAccountAsSerialClone(CFStringRef name, SOSPeerInfoDeviceClass devClass, CFStringRef serial, CFStringRef idsID) {
    return CreateAccountForLocalChangesWithStartingAttributes(name, CFSTR("TestSource"), devClass, serial, kCFBooleanTrue, kCFBooleanTrue, kCFBooleanTrue, SOSTransportMessageTypeKVS, idsID);
}

static inline bool SOSTestMakeGhostInCircle(CFStringRef name, SOSPeerInfoDeviceClass devClass, CFStringRef serial, CFStringRef idsID,
                                     CFDataRef cfpassword, CFStringRef cfaccount, CFMutableDictionaryRef changes,
                                     SOSAccount* approver, int expectedCount) {
    bool retval = false;
    SOSAccount* ghostAccount = SOSTestCreateAccountAsSerialClone(name, devClass, serial, idsID);
    ok(ghostAccount, "Created Ghost Account");
    require_quiet(ghostAccount, retOut);
    if(!ghostAccount) return false;
    ok(retval = SOSTestJoinWithApproval(cfpassword, cfaccount, changes, approver, ghostAccount, DROP_USERKEY, expectedCount, true), "Ghost Joined Circle with expected result");
retOut:
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
