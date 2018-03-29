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

#include <AssertMacros.h>

#include <stdio.h>
#include "SOSAccountPriv.h"
#include "SOSInternal.h"
#include "SOSViews.h"
#include "SOSPeerInfoV2.h"
#import <Security/SecureObjectSync/SOSAccountTrust.h>
#import <Security/SecureObjectSync/SOSAccountTrustClassic.h>

static CFStringRef kicloud_identity_name = CFSTR("Cloud Identity");

SecKeyRef SOSAccountCopyDeviceKey(SOSAccount* account, CFErrorRef *error) {
    SecKeyRef privateKey = NULL;
    SOSFullPeerInfoRef identity = NULL;

    SOSAccountTrustClassic *trust = account.trust;
    identity = trust.fullPeerInfo;
    require_action_quiet(identity, fail, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No identity to get key from")));

    privateKey = SOSFullPeerInfoCopyDeviceKey(identity, error);

fail:
    return privateKey;
}

SOSFullPeerInfoRef CopyCloudKeychainIdentity(SOSPeerInfoRef cloudPeer, CFErrorRef *error) {
    return SOSFullPeerInfoCreateCloudIdentity(NULL, cloudPeer, error);
}


static CF_RETURNS_RETAINED SecKeyRef GeneratePermanentFullECKey_internal(int keySize, CFStringRef name, CFTypeRef accessibility, CFBooleanRef sync,  CFErrorRef* error)
{
    SecKeyRef full_key = NULL;
    
    CFNumberRef key_size_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize);
    
    CFDictionaryRef priv_key_attrs = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                  kSecAttrIsPermanent,    kCFBooleanTrue,
                                                                  NULL);
    
    CFDictionaryRef keygen_parameters = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                     kSecAttrKeyType,        kSecAttrKeyTypeEC,
                                                                     kSecAttrKeySizeInBits,  key_size_num,
                                                                     kSecPrivateKeyAttrs,    priv_key_attrs,
                                                                     kSecAttrAccessible,     accessibility,
                                                                     kSecAttrAccessGroup,    kSOSInternalAccessGroup,
                                                                     kSecAttrLabel,          name,
                                                                     kSecAttrSynchronizable, sync,
                                                                     kSecUseTombstones,      kCFBooleanTrue,
                                                                     NULL);
    
    CFReleaseNull(priv_key_attrs);
    
    CFReleaseNull(key_size_num);
    OSStatus status = SecKeyGeneratePair(keygen_parameters, NULL, &full_key);
    CFReleaseNull(keygen_parameters);
    
    if (status)
        secerror("status: %ld", (long)status);
    if (status != errSecSuccess && error != NULL && *error == NULL) {
        *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL);
    }
    
    return full_key;
}

CF_RETURNS_RETAINED SecKeyRef GeneratePermanentFullECKey(int keySize, CFStringRef name, CFErrorRef* error) {
    return GeneratePermanentFullECKey_internal(keySize, name, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, kCFBooleanFalse, error);
}

static CF_RETURNS_RETAINED SecKeyRef GeneratePermanentFullECKeyForCloudIdentity(int keySize, CFStringRef name, CFErrorRef* error) {
    return GeneratePermanentFullECKey_internal(keySize, name, kSecAttrAccessibleWhenUnlocked, kCFBooleanTrue, error);
}

bool SOSAccountHasCircle(SOSAccount* account, CFErrorRef* error) {
    SOSAccountTrustClassic *trust = account.trust;
    SOSCircleRef circle = trust.trustedCircle;

    if (!circle)
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No trusted circle"));

    return circle != NULL;
}

bool SOSAccountHasFullPeerInfo(SOSAccount* account, CFErrorRef* error) {
    bool hasPeer = false;
    SOSAccountTrustClassic *trust = account.trust;
    SOSFullPeerInfoRef identity = trust.fullPeerInfo;

    require(SOSAccountHasCircle(account, error), fail);

    hasPeer = identity != NULL;

    if (!hasPeer)
        SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("No peer for circle"));

fail:
    return hasPeer;
}

bool SOSAccountIsAccountIdentity(SOSAccount* account, SOSPeerInfoRef peer_info, CFErrorRef *error)
{
    SOSFullPeerInfoRef identity = NULL;

    SOSAccountTrustClassic *trust = account.trust;
    identity = trust.fullPeerInfo;
    return CFEqualSafe(peer_info, SOSFullPeerInfoGetPeerInfo(identity));
}

bool SOSAccountFullPeerInfoVerify(SOSAccount* account, SecKeyRef privKey, CFErrorRef *error) {
    SOSFullPeerInfoRef identity = NULL;

    SOSAccountTrustClassic *trust = account.trust;
    identity = trust.fullPeerInfo;
    if(!identity) return false;
    SecKeyRef pubKey = SecKeyCreatePublicFromPrivate(privKey);
    bool retval = SOSPeerInfoApplicationVerify(SOSFullPeerInfoGetPeerInfo(identity), pubKey, error);
    CFReleaseNull(pubKey);
    return retval;
}

static bool UpdateKeyName(SecKeyRef key, SOSPeerInfoRef peer, CFErrorRef* error)
{
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecClass,             kSecClassKey,
                                                         kSecAttrSynchronizable,kCFBooleanTrue,
                                                         kSecUseTombstones,     kCFBooleanTrue,
                                                         kSecValueRef,          key,
                                                         NULL);
    
    CFStringRef new_name = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                    CFSTR("Cloud Identity - '%@'"), SOSPeerInfoGetPeerID(peer));
    
    CFDictionaryRef change = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                          kSecAttrLabel,        new_name,
                                                          NULL);
    
    bool result = SecError(SecItemUpdate(query, change), error, CFSTR("Couldn't update name"));
    
    CFReleaseNull(new_name);
    CFReleaseNull(query);
    CFReleaseNull(change);
    return result;
}

SOSPeerInfoRef GenerateNewCloudIdentityPeerInfo(CFErrorRef *error) {
    SecKeyRef cloud_key = GeneratePermanentFullECKeyForCloudIdentity(256, kicloud_identity_name, error);
    SOSPeerInfoRef cloud_peer = NULL;
    
    CFDictionaryRef gestalt = NULL;
    
    require_action_quiet(cloud_key, fail, SecError(errSecAllocate, error, CFSTR("Can't generate keypair for icloud identity")));

    gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kPIUserDefinedDeviceNameKey, CFSTR("iCloud"),
                                                           NULL);
    require_action_quiet(gestalt, fail, SecError(errSecAllocate, error, CFSTR("Can't allocate gestalt")));
    
    cloud_peer = SOSPeerInfoCreateCloudIdentity(kCFAllocatorDefault, gestalt, cloud_key, error);
    
    require(cloud_peer, fail);
    
    UpdateKeyName(cloud_key, cloud_peer, error);

fail:
    CFReleaseNull(gestalt);
    CFReleaseNull(cloud_key);
    
    return cloud_peer;
}
