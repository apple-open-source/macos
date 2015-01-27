/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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
#include "SecOTRRemote.h"
#include <Security/SecOTRSession.h>
#include <Security/SecOTRIdentityPriv.h>
#include <securityd/SecItemServer.h>
#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccount.h"

CFDataRef SecOTRSessionCreateRemote_internal(CFDataRef publicAccountData, CFDataRef publicPeerId, CFDataRef privateAccountData, CFErrorRef *error) {
    SOSDataSourceFactoryRef ds = SecItemDataSourceFactoryGetDefault();
        
    SOSAccountRef privateAccount = (privateAccountData == NULL) ? SOSKeychainAccountGetSharedAccount() : SOSAccountCreateFromData(kCFAllocatorDefault, privateAccountData, ds, error);
    
    SOSAccountRef publicAccount = (publicAccountData == NULL) ? SOSKeychainAccountGetSharedAccount() : SOSAccountCreateFromData(kCFAllocatorDefault, publicAccountData, ds, error);
    
    if(!privateAccount || !publicAccount)
        return NULL;
    
    __block SOSFullPeerInfoRef full_peer_info = NULL;
    SOSAccountForEachCircle(privateAccount, ^(SOSCircleRef circle) {
        if (full_peer_info == NULL) {
            full_peer_info = SOSAccountGetMyFullPeerInCircle(privateAccount, circle, error);
        }
    });
    
    SecKeyRef privateKeyRef = SOSFullPeerInfoCopyDeviceKey(full_peer_info, error);
    if(!privateKeyRef) {
        secnotice("otr_keysetup", "Could not get private key for FullPeerInfo");
        return NULL;
    }
    
    CFStringRef publicKeyString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, publicPeerId, kCFStringEncodingUTF8);
    __block SOSPeerInfoRef peer_info = NULL;
    SOSAccountForEachCircle(publicAccount, ^(SOSCircleRef circle) {
        if (peer_info == NULL) {
            peer_info = SOSCircleCopyPeerWithID(circle, publicKeyString, error);
        }
    });
    
    SecKeyRef publicKeyRef = SOSPeerInfoCopyPubKey(peer_info);
    
    SecOTRFullIdentityRef privateIdentity = SecOTRFullIdentityCreateFromSecKeyRef(kCFAllocatorDefault, privateKeyRef, error);
    SecOTRPublicIdentityRef publicIdentity = SecOTRPublicIdentityCreateFromSecKeyRef(kCFAllocatorDefault, publicKeyRef, error);
    
    SecOTRSessionRef ourSession = SecOTRSessionCreateFromID(kCFAllocatorDefault, privateIdentity, publicIdentity);
    
    CFMutableDataRef exportSession = CFDataCreateMutable(kCFAllocatorDefault, 0);
    SecOTRSAppendSerialization(ourSession, exportSession);
    
    return exportSession;
}

CFDataRef _SecOTRSessionCreateRemote(CFDataRef publicPeerId, CFErrorRef *error) {
    return SecOTRSessionCreateRemote_internal(NULL, publicPeerId, NULL, error);
}

bool _SecOTRSessionProcessPacketRemote(CFDataRef sessionData, CFDataRef inputPacket, CFDataRef* outputSessionData, CFDataRef* outputPacket, bool *readyForMessages, CFErrorRef *error) {
    
    SecOTRSessionRef session = SecOTRSessionCreateFromData(kCFAllocatorDefault, sessionData);
    
    CFMutableDataRef negotiationResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    if (inputPacket) {
        SecOTRSProcessPacket(session, inputPacket, negotiationResponse);
    } else {
        SecOTRSAppendStartPacket(session, negotiationResponse);
    }
    
    CFMutableDataRef outputSession = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    SecOTRSAppendSerialization(session, outputSession);
    *outputSessionData = outputSession;
    
    *outputPacket = negotiationResponse;
    
    *readyForMessages = SecOTRSGetIsReadyForMessages(session);
    
    return true;
}

