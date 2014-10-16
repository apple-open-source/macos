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
#include <SecureObjectSync/SOSPeerInfoCollections.h>


bool SOSAccountDestroyCirclePeerInfoNamed(SOSAccountRef account, CFStringRef name, CFErrorRef* error) {
    if (CFDictionaryGetValue(account->circles, name) == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No circle named '%@'"), name);
        return false;
    }
    
    SOSFullPeerInfoRef circle_full_peer_info = (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, name);
    
    if (circle_full_peer_info) {        
        SOSFullPeerInfoPurgePersistentKey(circle_full_peer_info, NULL);
    }
    
    CFDictionaryRemoveValue(account->circle_identities, name);
    
    return true;
}

bool SOSAccountDestroyCirclePeerInfo(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    return SOSAccountDestroyCirclePeerInfoNamed(account, SOSCircleGetName(circle), error);
}

SOSPeerInfoRef SOSAccountGetMyPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi =  SOSAccountGetMyFullPeerInCircleNamed(account, SOSCircleGetName(circle), error);
    
    return fpi ? SOSFullPeerInfoGetPeerInfo(fpi) : NULL;
}

SOSPeerInfoRef SOSAccountGetMyPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error)
{
    SOSFullPeerInfoRef fpi =  SOSAccountGetMyFullPeerInCircleNamed(account, name, error);
    
    return fpi ? SOSFullPeerInfoGetPeerInfo(fpi) : NULL;
}


bool SOSAccountIsActivePeerInCircleNamed(SOSAccountRef account, CFStringRef circle_name, CFStringRef peerid, CFErrorRef* error) {
    SOSCircleRef circle = SOSAccountFindCircle(account, circle_name, error);
    if(!circle) return false;
    return SOSCircleHasActivePeerWithID(circle, peerid, error);
}

bool SOSAccountIsMyPeerActiveInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, SOSCircleGetName(circle), NULL);
    if(!fpi) return false;
    return SOSCircleHasActivePeer(circle, SOSFullPeerInfoGetPeerInfo(fpi), error);
}

bool SOSAccountIsMyPeerActiveInCircleNamed(SOSAccountRef account, CFStringRef circle_name, CFErrorRef* error) {
    SOSFullPeerInfoRef myfpi = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, circle_name, NULL);
    if(!myfpi) return false;
    SOSPeerInfoRef mypi = SOSFullPeerInfoGetPeerInfo(myfpi);
    if(!mypi) return false;
    CFStringRef peerid = SOSPeerInfoGetPeerID(mypi);
    if (!peerid) return false;
    return SOSAccountIsActivePeerInCircleNamed(account, circle_name, peerid, error);
}


//
// MARK: Peer Querying
//
static CFArrayRef SOSAccountCopySortedPeerArray(SOSAccountRef account,
                                                CFErrorRef *error,
                                                void (^action)(SOSCircleRef circle, CFMutableArrayRef appendPeersTo)) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;
    
    CFMutableArrayRef peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        action(circle, peers);
    });
    
    CFArrayOfSOSPeerInfosSortByID(peers);

    return peers;
}


CFArrayRef SOSAccountCopyNotValidPeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if(!SOSPeerInfoApplicationVerify(peer, account->user_public, NULL)) {
                CFArrayAppendValue(appendPeersTo, peer);
            }
        });
    });
}


CFArrayRef SOSAccountCopyValidPeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if(SOSPeerInfoApplicationVerify(peer, account->user_public, NULL)) {
                CFArrayAppendValue(appendPeersTo, peer);
            }
        });
    });
}


CFArrayRef SOSAccountCopyRetired(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachRetiredPeer(circle, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyApplicants(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachApplicant(circle, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyPeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyActivePeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyActiveValidPeers(SOSAccountRef account, CFErrorRef *error) {
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleForEachActiveValidPeer(circle, account->user_public, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(appendPeersTo, peer);
        });
    });
}

CFArrayRef SOSAccountCopyConcurringPeers(SOSAccountRef account, CFErrorRef *error)
{
    return SOSAccountCopySortedPeerArray(account, error, ^(SOSCircleRef circle, CFMutableArrayRef appendPeersTo) {
        SOSCircleAppendConcurringPeers(circle, appendPeersTo, NULL);
    });
}
