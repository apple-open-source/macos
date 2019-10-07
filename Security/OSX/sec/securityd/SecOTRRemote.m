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

#include "AssertMacros.h"
#include <stdio.h>
#include "SecOTRRemote.h"
#include <Security/SecOTRSession.h>
#include <Security/SecOTRIdentityPriv.h>
#include <securityd/SecItemServer.h>
#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountPriv.h"

#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"

CFDataRef SecOTRSessionCreateRemote_internal(CFDataRef publicAccountData, CFDataRef publicPeerId, CFDataRef privateAccountData, CFErrorRef *error) {
    SOSDataSourceFactoryRef ds = SecItemDataSourceFactoryGetDefault();

    SOSAccount* privateAccount = NULL;
    SOSAccount* publicAccount = NULL;
    CFStringRef   publicKeyString = NULL;
    SecKeyRef     privateKeyRef = NULL;
    SecKeyRef     publicKeyRef = NULL;
    SecOTRFullIdentityRef privateIdentity = NULL;
    SecOTRPublicIdentityRef publicIdentity = NULL;
    CFDataRef result = NULL;
    SecOTRSessionRef ourSession = NULL;
    
    require_quiet(ds, fail);
    require_quiet(publicPeerId, fail);

    if (privateAccountData) {
        NSError* ns_error = nil;
        privateAccount = [SOSAccount accountFromData:(__bridge NSData*) privateAccountData factory:ds error:&ns_error];
        if (error && *error == NULL && !privateAccount) {
            *error = (CFErrorRef) CFBridgingRetain(ns_error);
        }
    } else {
        privateAccount = (__bridge SOSAccount*)(SOSKeychainAccountGetSharedAccount());
    }

    require_quiet(privateAccount, fail);

    privateKeyRef = SOSAccountCopyDeviceKey(privateAccount, error);
    require_quiet(privateKeyRef, fail);

    privateIdentity = SecOTRFullIdentityCreateFromSecKeyRef(kCFAllocatorDefault, privateKeyRef, error);
    require_quiet(privateIdentity, fail);
    CFReleaseNull(privateKeyRef);

    publicKeyString = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, publicPeerId, kCFStringEncodingUTF8);
    require_quiet(publicKeyString, fail);

    if (publicAccountData) {
        NSError* ns_error = nil;
        publicAccount = [SOSAccount accountFromData:(__bridge NSData*) publicAccountData factory:ds error:&ns_error];
        if (error && *error == NULL && !publicAccount) {
            *error = (CFErrorRef) CFBridgingRetain(ns_error);
        }
    } else {
        publicAccount = (__bridge SOSAccount*)(SOSKeychainAccountGetSharedAccount());
    }

    require_quiet(publicAccount, fail);

    publicKeyRef = [publicAccount.trust copyPublicKeyForPeer:publicKeyString err:error];

    if(!publicKeyRef){
        if(!ds){
            CFReleaseNull(ourSession);
            CFReleaseNull(publicKeyString);
            privateAccount= nil;
            publicAccount = nil;
            CFReleaseNull(privateKeyRef);
            CFReleaseNull(publicKeyRef);
            CFReleaseNull(publicIdentity);
            CFReleaseNull(privateIdentity);
            return result;
        }
    }
    publicIdentity = SecOTRPublicIdentityCreateFromSecKeyRef(kCFAllocatorDefault, publicKeyRef, error);
    require_quiet(publicIdentity, fail);

    CFReleaseNull(publicKeyRef);

    ourSession = SecOTRSessionCreateFromID(kCFAllocatorDefault, privateIdentity, publicIdentity);
    
    CFMutableDataRef exportSession = CFDataCreateMutable(kCFAllocatorDefault, 0);
    SecOTRSAppendSerialization(ourSession, exportSession);

    result = exportSession;
    exportSession = NULL;

fail:
    CFReleaseNull(ourSession);
    CFReleaseNull(publicKeyString);
    privateAccount= nil;
    publicAccount = nil;
    CFReleaseNull(privateKeyRef);
    CFReleaseNull(publicKeyRef);
    CFReleaseNull(publicIdentity);
    CFReleaseNull(privateIdentity);
    return result;
}

CFDataRef _SecOTRSessionCreateRemote(CFDataRef publicPeerId, CFErrorRef *error) {
    return SecOTRSessionCreateRemote_internal(NULL, publicPeerId, NULL, error);
}

bool _SecOTRSessionProcessPacketRemote(CFDataRef sessionData, CFDataRef inputPacket, CFDataRef* outputSessionData, CFDataRef* outputPacket, bool *readyForMessages, CFErrorRef *error) {
    
    bool result = false;
    SecOTRSessionRef session = SecOTRSessionCreateFromData(kCFAllocatorDefault, sessionData);
    require_quiet(session, done);
    
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
    CFReleaseNull(session);
    
    result = true;

done:
    return result;
}

