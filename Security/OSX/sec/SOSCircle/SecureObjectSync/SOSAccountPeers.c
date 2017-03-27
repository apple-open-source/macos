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


#include <stdio.h>
#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>


bool SOSAccountIsMyPeerActive(SOSAccountRef account, CFErrorRef* error) {
    SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(account->my_identity);
    return me ? SOSCircleHasActivePeer(account->trusted_circle, me, error) : false;
}

//
// MARK: Peer Querying
//


static void sosArrayAppendPeerCopy(CFMutableArrayRef appendPeersTo, SOSPeerInfoRef peer) {
    SOSPeerInfoRef peerInfo = SOSPeerInfoCreateCopy(kCFAllocatorDefault, peer, NULL);
    CFArrayAppendValue(appendPeersTo, peerInfo);
    CFRelease(peerInfo);
}

static CFArrayRef SOSAccountCopySortedPeerArray(SOSAccountRef account,
                                                CFErrorRef *error,
                                                void (^action)(SOSCircleRef circle, CFMutableArrayRef appendPeersTo)) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;
    
    CFMutableArrayRef peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    action(account->trusted_circle, peers);

    CFArrayOfSOSPeerInfosSortByID(peers);

    return peers;
}


CFArrayRef SOSAccountCopyNotValidPeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if(!SOSPeerInfoApplicationVerify(peer, account->user_public, NULL)) {
                sosArrayAppendPeerCopy(appendPeersTo, peer);
            }
        });
    });
}


CFArrayRef SOSAccountCopyValidPeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if(SOSPeerInfoApplicationVerify(peer, account->user_public, NULL)) {
                sosArrayAppendPeerCopy(appendPeersTo, peer);
            }
        });
    });
}

void SOSAccountForEachCirclePeerExceptMe(SOSAccountRef account, void (^action)(SOSPeerInfoRef peer)) {
    SOSPeerInfoRef myPi = SOSAccountGetMyPeerInfo(account);
    if (account->trusted_circle && myPi) {
        CFStringRef myPi_id = SOSPeerInfoGetPeerID(myPi);
        SOSCircleForEachPeer(account->trusted_circle, ^(SOSPeerInfoRef peer) {
            CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
            if (peerID && !CFEqual(peerID, myPi_id)) {
                action(peer);
            }
        });
    }
}

CFArrayRef SOSAccountCopyPeersToListenTo(SOSAccountRef account, CFErrorRef *error) {
    SOSPeerInfoRef myPeerInfo = SOSFullPeerInfoGetPeerInfo(account->my_identity);
    CFStringRef myID = myPeerInfo ? SOSPeerInfoGetPeerID(myPeerInfo) : NULL;
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if(!CFEqualSafe(myID, SOSPeerInfoGetPeerID(peer)) &&
               SOSPeerInfoApplicationVerify(peer, account->user_public, NULL) &&
               !SOSPeerInfoIsRetirementTicket(peer)) {
                CFArrayAppendValue(appendPeersTo, peer);
            }
        });
    });
}

CFArrayRef SOSAccountCopyRetired(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachRetiredPeer(circle, ^(SOSPeerInfoRef peer) {
            sosArrayAppendPeerCopy(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyViewUnaware(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if (!SOSPeerInfoVersionHasV2Data(peer) ) {
                sosArrayAppendPeerCopy(appendPeersTo, peer);
            } else {
                CFSetRef peerEnabledViews = SOSPeerInfoCopyEnabledViews(peer);
                CFSetRef enabledV0Views = CFSetCreateIntersection(kCFAllocatorDefault, peerEnabledViews, SOSViewsGetV0ViewSet());
                if(CFSetGetCount(enabledV0Views) != 0) {
                    sosArrayAppendPeerCopy(appendPeersTo, peer);
                }
                CFReleaseNull(peerEnabledViews);
                CFReleaseNull(enabledV0Views);
            }
        });
    });
}

CFArrayRef SOSAccountCopyApplicants(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachApplicant(circle, ^(SOSPeerInfoRef peer) {
            sosArrayAppendPeerCopy(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyPeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            sosArrayAppendPeerCopy(appendPeersTo, peer);
        });
    });
}

CFDataRef SOSAccountCopyAccountStateFromKeychain(CFErrorRef *error){
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFTypeRef result = NULL;
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("com.apple.security.sos"));
    CFDictionaryAddValue(query, kSecAttrAccessible, CFSTR("dku"));
    CFDictionaryAddValue(query, kSecAttrTombstone, kCFBooleanFalse);
    CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanFalse);
    CFDictionaryAddValue(query, kSecReturnData, kCFBooleanTrue);
    
    SecItemCopyMatching(query, &result);

    if(!isData(result)){
        SOSErrorCreate(kSOSErrorUnexpectedType, error, NULL, CFSTR("Expected CFData, got: %@"), result);
        CFReleaseNull(result);
        return NULL;
    }
    return result;
}

bool SOSAccountDeleteAccountStateFromKeychain(CFErrorRef *error){
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    bool result = false;
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("com.apple.security.sos"));
    CFDictionaryAddValue(query, kSecAttrAccessible, CFSTR("dku"));
    CFDictionaryAddValue(query, kSecAttrTombstone, kCFBooleanFalse);
    CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanFalse);
    
    result = SecItemDelete(query);
    return result;
}

CFDataRef SOSAccountCopyEngineStateFromKeychain(CFErrorRef *error){
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFTypeRef result = NULL;
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("engine-state"));
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("com.apple.security.sos"));
    CFDictionaryAddValue(query, kSecAttrAccessible, CFSTR("dk"));
    CFDictionaryAddValue(query, kSecAttrService, CFSTR("SOSDataSource-ak"));
    CFDictionaryAddValue(query, kSecAttrTombstone, kCFBooleanFalse);
    CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanFalse);
    CFDictionaryAddValue(query, kSecReturnData, kCFBooleanTrue);
    
    SecItemCopyMatching(query, &result);
    
    if(!isData(result)){
        SOSErrorCreate(kSOSErrorUnexpectedType, error, NULL, CFSTR("Expected CFData, got: %@"), result);
        CFReleaseNull(result);
        return NULL;
    }
    return result;
}

bool SOSAccountDeleteEngineStateFromKeychain(CFErrorRef *error){
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    bool result = false;
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("engine-state"));
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("com.apple.security.sos"));
    CFDictionaryAddValue(query, kSecAttrAccessible, CFSTR("dk"));
    CFDictionaryAddValue(query, kSecAttrService, CFSTR("SOSDataSource-ak"));
    CFDictionaryAddValue(query, kSecAttrTombstone, kCFBooleanFalse);
    CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanFalse);
    
    result = SecItemDelete(query);
    return result;
}


CFArrayRef SOSAccountCopyActivePeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
            sosArrayAppendPeerCopy(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyActiveValidPeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachActiveValidPeer(circle, account->user_public, ^(SOSPeerInfoRef peer) {
            sosArrayAppendPeerCopy(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyConcurringPeers(SOSAccountRef account, CFErrorRef *error)
{
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleAppendConcurringPeers(circle, appendPeersTo, NULL);
    });
}

SOSPeerInfoRef SOSAccountCopyPeerWithID(SOSAccountRef account, CFStringRef peerid, CFErrorRef *error) {
    if(!account->trusted_circle) return NULL;
    return SOSCircleCopyPeerWithID(account->trusted_circle, peerid, error);
}

CFBooleanRef SOSAccountPeersHaveViewsEnabled(SOSAccountRef account, CFArrayRef viewNames, CFErrorRef *error) {
    CFBooleanRef result = NULL;
    CFMutableSetRef viewsRemaining = NULL;
    CFSetRef viewsToLookFor = NULL;

    require_quiet(SOSAccountHasPublicKey(account, error), done);
    require_quiet(SOSAccountIsInCircle(account, error), done);

    viewsToLookFor = CFSetCreateCopyOfArrayForCFTypes(viewNames);
    viewsRemaining = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, viewsToLookFor);
    CFReleaseNull(viewsToLookFor);

    SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
        if (SOSPeerInfoApplicationVerify(peer, account->user_public, NULL)) {
            CFSetRef peerViews = SOSPeerInfoCopyEnabledViews(peer);
            CFSetSubtract(viewsRemaining, peerViews);
            CFReleaseNull(peerViews);
        }
    });

    result = CFSetIsEmpty(viewsRemaining) ? kCFBooleanTrue : kCFBooleanFalse;

done:
    CFReleaseNull(viewsToLookFor);
    CFReleaseNull(viewsRemaining);

    return result;
}

