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

static CFStringRef kicloud_identity_name = CFSTR("Cloud Identity");


SOSFullPeerInfoRef CopyCloudKeychainIdentity(SOSPeerInfoRef cloudPeer, CFErrorRef *error) {
    return SOSFullPeerInfoCreateCloudIdentity(NULL, cloudPeer, error);
}


SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircleNamedIfPresent(SOSAccountRef account, CFStringRef name, CFErrorRef *error) {
    if (CFDictionaryGetValue(account->circles, name) == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No circle named '%@'"), name);
        return NULL;
    }
    
    return (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, name);
}



static SecKeyRef GeneratePermanentFullECKey_internal(int keySize, CFStringRef name, CFTypeRef accessibility, CFBooleanRef sync,  CFErrorRef* error)
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

static SecKeyRef GeneratePermanentFullECKey(int keySize, CFStringRef name, CFErrorRef* error) {
    return GeneratePermanentFullECKey_internal(keySize, name, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, kCFBooleanFalse, error);
}

static SecKeyRef GeneratePermanentFullECKeyForCloudIdentity(int keySize, CFStringRef name, CFErrorRef* error) {
    return GeneratePermanentFullECKey_internal(keySize, name, kSecAttrAccessibleWhenUnlocked, kCFBooleanTrue, error);
}


bool SOSAccountIsAccountIdentity(SOSAccountRef account, SOSPeerInfoRef peer_info, CFErrorRef *error)
{
    __block bool matches = false;
    CFDictionaryForEach(account->circle_identities, ^(const void *key, const void *value) {
        if (!matches) {
            matches = CFEqual(peer_info, SOSFullPeerInfoGetPeerInfo((SOSFullPeerInfoRef) value));
        }
    });
    
    return matches;
}



SOSFullPeerInfoRef SOSAccountMakeMyFullPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error) {
    if (CFDictionaryGetValue(account->circles, name) == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No circle named '%@'"), name);
        return NULL;
    }
    SOSFullPeerInfoRef circle_full_peer_info = (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, name);
    
    
    if (circle_full_peer_info == NULL) {
        CFStringRef keyName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("ID for %@-%@"), SOSPeerGestaltGetName(account->gestalt), name);
        SecKeyRef full_key = GeneratePermanentFullECKey(256, keyName, error);
        CFReleaseNull(keyName);
        
        if (full_key) {
            circle_full_peer_info = SOSFullPeerInfoCreate(kCFAllocatorDefault, account->gestalt, full_key, error);
            
            CFReleaseNull(full_key);
            
            if (!circle_full_peer_info) {
                secerror("Can't make FullPeerInfo for %@-%@ (%@) - is AKS ok?", SOSPeerGestaltGetName(account->gestalt), name, error ? (void*)*error : (void*)CFSTR("-"));
                return circle_full_peer_info;
            }
            
            CFDictionarySetValue(account->circle_identities, name, circle_full_peer_info);
            CFReleaseNull(circle_full_peer_info);
            circle_full_peer_info = (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, name);
        }
        else
            secerror("No full_key: %@:", error ? *error : NULL);
    }
    
    return circle_full_peer_info;
}


SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    return SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, SOSCircleGetName(circle), error);
}


SOSPeerInfoRef GenerateNewCloudIdentityPeerInfo(CFErrorRef *error) {
    SecKeyRef cloud_key = GeneratePermanentFullECKeyForCloudIdentity(256, kicloud_identity_name, error);
    SOSPeerInfoRef cloud_peer = NULL;
    CFDictionaryRef query = NULL;
    CFDictionaryRef change = NULL;
    CFStringRef new_name = NULL;
    
    CFDictionaryRef gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kPIUserDefinedDeviceName, CFSTR("iCloud"),
                                                           NULL);
    require_action_quiet(gestalt, fail, SecError(errSecAllocate, error, CFSTR("Can't allocate gestalt")));
    
    cloud_peer = SOSPeerInfoCreateCloudIdentity(kCFAllocatorDefault, gestalt, cloud_key, error);
    
    require(cloud_peer, fail);
    
    query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                         kSecClass,             kSecClassKey,
                                         kSecAttrSynchronizable,kCFBooleanTrue,
                                         kSecUseTombstones,     kCFBooleanTrue,
                                         kSecValueRef,          cloud_key,
                                         NULL);
    
    new_name = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                        CFSTR("Cloud Identity - '%@'"), SOSPeerInfoGetPeerID(cloud_peer));
    
    change = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                          kSecAttrLabel,        new_name,
                                          NULL);
    
    SecError(SecItemUpdate(query, change), error, CFSTR("Couldn't update name"));
    
fail:
    CFReleaseNull(new_name);
    CFReleaseNull(query);
    CFReleaseNull(change);
    CFReleaseNull(gestalt);
    CFReleaseNull(cloud_key);
    
    return cloud_peer;
}
