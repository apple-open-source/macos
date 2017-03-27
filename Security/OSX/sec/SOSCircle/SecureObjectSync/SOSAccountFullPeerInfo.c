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
#include "SOSInternal.h"
#include "SOSViews.h"
#include "SOSPeerInfoV2.h"

static CFStringRef kicloud_identity_name = CFSTR("Cloud Identity");

SecKeyRef SOSAccountCopyDeviceKey(SOSAccountRef account, CFErrorRef *error) {
    SecKeyRef privateKey = NULL;

    require_action_quiet(account->my_identity, fail, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No identity to get key from")));

    privateKey = SOSFullPeerInfoCopyDeviceKey(account->my_identity, error);

fail:
    return privateKey;
}

SOSFullPeerInfoRef CopyCloudKeychainIdentity(SOSPeerInfoRef cloudPeer, CFErrorRef *error) {
    return SOSFullPeerInfoCreateCloudIdentity(NULL, cloudPeer, error);
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

SecKeyRef GeneratePermanentFullECKey(int keySize, CFStringRef name, CFErrorRef* error) {
    return GeneratePermanentFullECKey_internal(keySize, name, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, kCFBooleanFalse, error);
}

static SecKeyRef GeneratePermanentFullECKeyForCloudIdentity(int keySize, CFStringRef name, CFErrorRef* error) {
    return GeneratePermanentFullECKey_internal(keySize, name, kSecAttrAccessibleWhenUnlocked, kCFBooleanTrue, error);
}

bool SOSAccountEnsureFullPeerAvailable(SOSAccountRef account, CFErrorRef * error) {
    require_action_quiet(account->trusted_circle, fail, SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("Don't have circle")));

    if (account->my_identity == NULL) {
        CFStringRef keyName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("ID for %@-%@"), SOSPeerGestaltGetName(account->gestalt), SOSCircleGetName(account->trusted_circle));
        SecKeyRef full_key = GeneratePermanentFullECKey(256, keyName, error);
        CFReleaseNull(keyName);

        if (full_key) {
            CFSetRef initialViews = SOSViewCopyViewSet(kViewSetInitial);

            CFReleaseNull(account->my_identity);
            account->my_identity = SOSFullPeerInfoCreateWithViews(kCFAllocatorDefault, account->gestalt, account->backup_key, initialViews,
                                                                  full_key, error);
            CFDictionaryRef v2dictionaryTestUpdates = SOSAccountGetValue(account, kSOSTestV2Settings, NULL);
            if(v2dictionaryTestUpdates) SOSFullPeerInfoUpdateV2Dictionary(account->my_identity, v2dictionaryTestUpdates, NULL);
            CFReleaseNull(initialViews);
            CFReleaseNull(full_key);

            CFSetRef pendingDefaultViews = SOSViewCopyViewSet(kViewSetDefault);
            SOSAccountPendEnableViewSet(account, pendingDefaultViews);
            CFReleaseNull(pendingDefaultViews);

            SOSAccountSetValue(account, kSOSUnsyncedViewsKey, kCFBooleanTrue, NULL);

            if (!account->my_identity) {
                secerror("Can't make FullPeerInfo for %@-%@ (%@) - is AKS ok?", SOSPeerGestaltGetName(account->gestalt), SOSCircleGetName(account->trusted_circle), error ? (void*)*error : (void*)CFSTR("-"));
            }
            else{
                secnotice("fpi", "alert KeychainSyncingOverIDSProxy the fpi is available");
                notify_post(kSecServerPeerInfoAvailable);
                if(account->deviceID)
                    SOSFullPeerInfoUpdateDeviceID(account->my_identity, account->deviceID, error);
            }
        }
        else {
            secerror("No full_key: %@:", error ? *error : NULL);

        }
    }

fail:
    return account->my_identity != NULL;
}

bool SOSAccountHasCircle(SOSAccountRef account, CFErrorRef* error) {
    if (!account->trusted_circle)
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No trusted circle"));

    return account->trusted_circle != NULL;
}

bool SOSAccountHasFullPeerInfo(SOSAccountRef account, CFErrorRef* error) {
    bool hasPeer = false;

    require(SOSAccountHasCircle(account, error), fail);

    hasPeer = account->my_identity != NULL;

    if (!hasPeer)
        SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("No peer for circle"));

fail:
    return hasPeer;
}

bool SOSAccountIsAccountIdentity(SOSAccountRef account, SOSPeerInfoRef peer_info, CFErrorRef *error)
{
    return CFEqualSafe(peer_info, SOSFullPeerInfoGetPeerInfo(account->my_identity));
}

SOSPeerInfoRef SOSAccountGetMyPeerInfo(SOSAccountRef account) {
    return SOSFullPeerInfoGetPeerInfo(SOSAccountGetMyFullPeerInfo(account));
}

CFStringRef SOSAccountGetMyPeerID(SOSAccountRef a) {
    return SOSPeerInfoGetPeerID(SOSAccountGetMyPeerInfo(a));
}

SOSFullPeerInfoRef SOSAccountGetMyFullPeerInfo(SOSAccountRef account) {
    return account->trusted_circle ? account->my_identity : NULL;
}

bool SOSAccountFullPeerInfoVerify(SOSAccountRef account, SecKeyRef privKey, CFErrorRef *error) {
    if(!account->my_identity) return false;
    SecKeyRef pubKey = SecKeyCreatePublicFromPrivate(privKey);
    bool retval = SOSPeerInfoApplicationVerify(SOSFullPeerInfoGetPeerInfo(account->my_identity), pubKey, error);
    CFReleaseNull(pubKey);
    return retval;
}

SOSPeerInfoRef GenerateNewCloudIdentityPeerInfo(CFErrorRef *error) {
    SecKeyRef cloud_key = GeneratePermanentFullECKeyForCloudIdentity(256, kicloud_identity_name, error);
    SOSPeerInfoRef cloud_peer = NULL;
    CFDictionaryRef query = NULL;
    CFDictionaryRef change = NULL;
    CFStringRef new_name = NULL;
    
    CFDictionaryRef gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kPIUserDefinedDeviceNameKey, CFSTR("iCloud"),
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
