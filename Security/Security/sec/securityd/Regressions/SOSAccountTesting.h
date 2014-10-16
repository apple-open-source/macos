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
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSAccountPriv.h>
#include <SecureObjectSync/SOSTransport.h>
#include "SOSTransportTestTransports.h"
//
// Account comparison
//

#define kAccountsAgreeTestMin 9
#define kAccountsAgreeTestPerPeer 1
#define accountsAgree(x) (kAccountsAgreeTestMin + kAccountsAgreeTestPerPeer * (x))


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
    
    a->retired_peers = CFDictionaryCreateMutableForCFTypes(allocator);
    SOSUnregisterTransportKeyParameter(a->key_transport);

    CFReleaseNull(a->circle_transports);
    CFReleaseNull(a->message_transports);
    CFReleaseNull(a->key_transport);

    SOSAccountEnsureFactoryCirclesTest(a, accountName);
    
    return a;
}


static void unretired_peers_is_subset(const char* label, CFArrayRef peers, CFArrayRef allowed_peers)
{
    CFArrayForEach(peers, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        CFErrorRef leftError = NULL;
        CFErrorRef rightError = NULL;
        
        ok(SOSPeerInfoIsRetirementTicket(pi) || SOSPeerInfoIsCloudIdentity(pi) || CFArrayContainsValue(allowed_peers, CFRangeMake(0, CFArrayGetCount(allowed_peers)), pi), "Peer is allowed (%s) Peer: %@, Allowed %@", label, pi, allowed_peers);
        
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
            CFMutableArrayRef allowed_identities = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

            CFArrayRef leftIdentities = SOSAccountCopyAccountIdentityPeerInfos(left, kCFAllocatorDefault, &error);
            ok(leftIdentities, "Get identities (%@)", error);
            CFReleaseNull(error);
            
            CFArrayAppendArray(allowed_identities, leftIdentities, CFRangeMake(0, CFArrayGetCount(leftIdentities)));
            
            CFReleaseNull(leftIdentities);
            
            CFArrayRef rightIdentities = SOSAccountCopyAccountIdentityPeerInfos(right, kCFAllocatorDefault, &error);
            ok(rightIdentities, "Get identities (%@)", error);
            CFReleaseNull(error);
            
            CFArrayAppendArray(allowed_identities, rightIdentities, CFRangeMake(0, CFArrayGetCount(rightIdentities)));
            
            CFReleaseNull(rightIdentities);

            unretired_peers_is_subset(label, leftPeers, allowed_identities);
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

static void CFDictionaryOverlayDictionary(CFMutableDictionaryRef target, CFDictionaryRef overlay) {
    CFDictionaryForEach(overlay, ^(const void *key, const void *value) {
        CFDictionarySetValue(target, key, value);
    });
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
        if (isArray(value) && (sender == NULL || !CFEqual(sender, key))) {
            CFArrayAppendKeys((CFMutableArrayRef) value, newKeysAndValues);
            if (CFDictionaryGetCount(newKeysAndValues))
                changes_added = true;
        }
    });

    CFDictionaryRemoveAllValues(newKeysAndValues);

    return changes_added;
}

static bool FillAllChanges(CFMutableDictionaryRef changes) {
    __block bool changed = false;
    CFArrayForEach(key_transports, ^(const void *value) {
        SOSTransportKeyParameterTestRef tpt = (SOSTransportKeyParameterTestRef) value;
        changed |= AddNewChanges(changes, SOSTransportKeyParameterTestGetChanges(tpt), SOSTransportKeyParameterTestGetAccount(tpt));
    });
    CFArrayForEach(circle_transports, ^(const void *value) {
        SOSTransportCircleTestRef tpt = (SOSTransportCircleTestRef) value;
        changed |= AddNewChanges(changes, SOSTransportCircleTestGetChanges(tpt), SOSTransportCircleTestGetAccount(tpt));
    });
    CFArrayForEach(message_transports, ^(const void *value) {
        SOSTransportMessageTestRef tpt = (SOSTransportMessageTestRef) value;
        CFDictionaryRemoveValue(SOSTransportMessageTestGetChanges(tpt), kCFNull);
        changed |=AddNewChanges(changes, SOSTransportMessageTestGetChanges(tpt), SOSTransportMessageTestGetAccount(tpt));
    });

    return changed;
}

static void FillChanges(CFMutableDictionaryRef changes, SOSAccountRef forAccount)
{
    CFArrayForEach(key_transports, ^(const void *value) {
        SOSTransportKeyParameterTestRef tpt = (SOSTransportKeyParameterTestRef) value;
        if(CFEqualSafe(forAccount, SOSTransportKeyParameterTestGetAccount(tpt))){
            AddNewChanges(changes, SOSTransportKeyParameterTestGetChanges(tpt), SOSTransportKeyParameterTestGetAccount(tpt));
        }
    });
    CFArrayForEach(circle_transports, ^(const void *value) {
        SOSTransportCircleTestRef tpt = (SOSTransportCircleTestRef) value;
        if(CFEqualSafe(forAccount, SOSTransportCircleTestGetAccount(tpt))){
            AddNewChanges(changes, SOSTransportCircleTestGetChanges(tpt), SOSTransportCircleTestGetAccount(tpt));
        }
    });
    CFArrayForEach(message_transports, ^(const void *value) {
        SOSTransportMessageTestRef tpt = (SOSTransportMessageTestRef) value;
        if(CFEqualSafe(forAccount, SOSTransportMessageTestGetAccount(tpt))){
            CFDictionaryRemoveValue(SOSTransportMessageTestGetChanges(tpt), kCFNull);
            AddNewChanges(changes, SOSTransportMessageTestGetChanges(tpt), SOSTransportMessageTestGetAccount(tpt));
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
        CFReleaseNull(account_pending_keys);

        account_pending_keys = CFDictionaryCopyKeys(full_list);
        CFDictionaryAddValue(changes, account, account_pending_keys);
        CFReleaseSafe(account_pending_keys); // The dictionary keeps it, we don't retain it here.
    }

    CFMutableArrayRef handled = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    secerror("Changes for %@: %@", SOSTransportKeyParameterTestGetName((SOSTransportKeyParameterTestRef) account->key_transport), account_pending_keys);

    CFErrorRef error = NULL;
    CFMutableDictionaryRef account_pending_messages = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayForEach(account_pending_keys, ^(const void *value) {
        CFDictionaryAddValue(account_pending_messages, value, CFDictionaryGetValue(full_list, value));
    });
    
    ok(handled = SOSTransportDispatchMessages(account, account_pending_messages, &error), "SOSTransportHandleMessages failed (%@)", error);
    
    if (isArray(handled)) {
        CFArrayForEach(handled, ^(const void *value) {
            CFArrayRemoveAllValue(account_pending_keys, value);
        });
    }
    
    CFReleaseNull(handled);
    CFReleaseNull(error);
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

static SOSAccountRef CreateAccountForLocalChanges(CFStringRef name, CFStringRef data_source_name)
{
    SOSDataSourceFactoryRef factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef ds = SOSTestDataSourceCreate();
    SOSTestDataSourceFactoryAddDataSource(factory, data_source_name, ds);
    SOSEngineRef engine = SOSEngineCreate(ds, NULL);
    ds->engine = engine;
    CFDictionaryRef gestalt = SOSCreatePeerGestaltFromName(name);

    SOSAccountRef result = SOSAccountCreateTest(kCFAllocatorDefault, name, gestalt, factory);

    CFReleaseNull(gestalt);

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

#endif
