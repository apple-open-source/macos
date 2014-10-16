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


#ifndef _SOSFULLPEERINFO_H_
#define _SOSFULLPEERINFO_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKey.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <SecureObjectSync/SOSPeerInfo.h>

__BEGIN_DECLS

typedef struct __OpaqueSOSFullPeerInfo   *SOSFullPeerInfoRef;

enum {
    kSOSFullPeerVersion = 1,
};

SOSFullPeerInfoRef SOSFullPeerInfoCreate(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef *error);

SOSFullPeerInfoRef SOSFullPeerInfoCreateCloudIdentity(CFAllocatorRef allocator, SOSPeerInfoRef peer, CFErrorRef* error);

SOSPeerInfoRef SOSFullPeerInfoGetPeerInfo(SOSFullPeerInfoRef fullPeer);
SecKeyRef      SOSFullPeerInfoCopyDeviceKey(SOSFullPeerInfoRef fullPeer, CFErrorRef* error);

bool SOSFullPeerInfoPurgePersistentKey(SOSFullPeerInfoRef peer, CFErrorRef* error);

SOSPeerInfoRef SOSFullPeerInfoPromoteToRetiredAndCopy(SOSFullPeerInfoRef peer, CFErrorRef *error);

bool SOSFullPeerInfoValidate(SOSFullPeerInfoRef peer, CFErrorRef* error);

bool SOSFullPeerInfoUpdateGestalt(SOSFullPeerInfoRef peer, CFDictionaryRef gestalt, CFErrorRef* error);

bool SOSFullPeerInfoPromoteToApplication(SOSFullPeerInfoRef fpi, SecKeyRef user_key, CFErrorRef *error);

bool SOSFullPeerInfoUpgradeSignatures(SOSFullPeerInfoRef fpi, SecKeyRef user_key, CFErrorRef *error);

//
// DER Import Export
//
SOSFullPeerInfoRef SOSFullPeerInfoCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                        const uint8_t** der_p, const uint8_t *der_end);

SOSFullPeerInfoRef SOSFullPeerInfoCreateFromData(CFAllocatorRef allocator, CFDataRef fullPeerData, CFErrorRef *error);

size_t      SOSFullPeerInfoGetDEREncodedSize(SOSFullPeerInfoRef peer, CFErrorRef *error);
uint8_t*    SOSFullPeerInfoEncodeToDER(SOSFullPeerInfoRef peer, CFErrorRef* error,
                                   const uint8_t* der, uint8_t* der_end);

CFDataRef SOSFullPeerInfoCopyEncodedData(SOSFullPeerInfoRef peer, CFAllocatorRef allocator, CFErrorRef *error);

__END_DECLS

#endif
