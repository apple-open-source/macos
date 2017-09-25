/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
#import <Foundation/Foundation.h>
#include <Security/SecBase.h>
#import "AsymKeybagBackup.h"
#import <Security/SecItemPriv.h>

#if USE_KEYSTORE
#include <libaks.h>
#endif

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include "sec/ipc/securityd_client.h"

#if OCTAGON
#import "SecBackupKeybagEntry.h"
#endif

#if OCTAGON
#if USE_KEYSTORE

static bool writeKeybagPublicKey(CFDataRef keybag, CFDataRef identifier, CFErrorRef *error) {
    bool ok = true;
    NSData* publickeyHash = (__bridge NSData *)(identifier);
    NSData* musr = [[NSData alloc] init];
    NSError *localError = NULL;

    SecBackupKeybagEntry *kbe = [[SecBackupKeybagEntry alloc] initWithPublicKey: (__bridge NSData*)keybag publickeyHash: (NSData*) publickeyHash user: musr];
    ok = [kbe saveToDatabase: &localError];
    CFErrorPropagate((__bridge CFErrorRef)(localError), error);
    return ok;
}

static bool deleteKeybagViaPublicKeyHash(CFDataRef publickeyHash, CFStringRef agrp, CFDataRef musr, CFErrorRef *error) {
    bool ok = true;
    NSData* keybag = nil;
    NSError *localError = NULL;

    SecBackupKeybagEntry *kbe = [[SecBackupKeybagEntry alloc] initWithPublicKey: keybag publickeyHash: (__bridge NSData*)publickeyHash user: (__bridge NSData *)(musr)];
    ok = [kbe deleteFromDatabase: &localError];

    CFErrorPropagate((__bridge CFErrorRef)(localError), error);
    return ok;
}

static bool deleteAllKeybags(CFErrorRef *error) {
    NSError *localError = NULL;

    bool ok = [SecBackupKeybagEntry deleteAll: &localError];

    CFErrorPropagate((__bridge CFErrorRef)(localError), error);
    return ok;
}

#endif
#endif

bool _SecServerBackupKeybagAdd(SecurityClient *client, CFDataRef passcode, CFDataRef *identifier, CFDataRef *pathinfo, CFErrorRef *error) {
    bool ok = true;
#if OCTAGON
#if USE_KEYSTORE
    CFURLRef upathinfo = NULL;
    CFDataRef keybagData = NULL;
    uuid_t uuid;
    char uuidstr[37];
    CFDataRef tidentifier = NULL;

    secerror("_SecServerBackupKeybagAdd: passlen: %ld", CFDataGetLength(passcode) );

    require_action(passcode && CFDataGetLength(passcode), xit, ok = SecError(errSecParam, error, CFSTR("passcode is required")));
    keybag_handle_t handle = bad_keybag_handle;

    kern_return_t kr = aks_create_bag(CFDataGetBytePtr(passcode), (int)CFDataGetLength(passcode), kAppleKeyStoreAsymmetricBackupBag, &handle);
    require_action(kr == kIOReturnSuccess, xit, ok = SecError(errSecParam, error, CFSTR("could not create keybag: %d"), kr));

    void *keybag = NULL;
    int keybag_size = 0;
    kr = aks_save_bag(handle, &keybag, &keybag_size);
    require_action(kr == kIOReturnSuccess, xit, ok = SecError(errSecParam, error, CFSTR("could not save keybag: %d"), kr));
    keybagData = CFDataCreate(kCFAllocatorDefault, keybag, keybag_size);

    // For now, use uuid as key in db; will use publicKey hash later
    kr = aks_get_bag_uuid(handle, uuid);
    require_action(kr == kIOReturnSuccess, xit, ok = SecError(errSecParam, error, CFSTR("could not get keybag uuid: %d"), kr));

    tidentifier = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)uuidstr, sizeof(uuidstr));
    uuid_unparse_lower(uuid, uuidstr);
    if (identifier)
        *identifier = CFRetainSafe(tidentifier);

    require_action(writeKeybagPublicKey(keybagData, tidentifier, error), xit, ok = SecError(errSecParam, error, CFSTR("passcode is required")));

    if (pathinfo)
        *pathinfo = NULL;
xit:
    CFReleaseNull(tidentifier);
    CFReleaseNull(upathinfo);
    CFReleaseNull(keybagData);
#endif /* USE_KEYSTORE */
#endif
    return ok;
}

bool _SecServerBackupKeybagDelete(CFDictionaryRef attributes, bool deleteAll, CFErrorRef *error) {
    bool ok = true;
#if OCTAGON
#if USE_KEYSTORE
    // Look for matching publicKeyHash

    if (deleteAll) {
        deleteAllKeybags(error);
    } else {
        CFDataRef publickeyHash = (CFDataRef)CFDictionaryGetValue(attributes, kSecAttrPublicKeyHash);
        CFStringRef agrp = (CFStringRef)CFDictionaryGetValue(attributes, kSecAttrAccessGroup);
        CFDataRef musr = (CFDataRef)CFDictionaryGetValue(attributes, kSecAttrMultiUser);

        require_action(publickeyHash, xit, ok = SecError(errSecParam, error, CFSTR("publickeyHash is required")));
        ok = deleteKeybagViaPublicKeyHash(publickeyHash, agrp, musr, error);
    }
xit:
#endif /* USE_KEYSTORE */
#endif /* OCTAGON */
    return ok;
}
