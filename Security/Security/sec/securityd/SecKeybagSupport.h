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

/*!
 @header SecKeybagSupport.h - The thing that does the stuff with the gibli.
 */

#ifndef _SECURITYD_SECKEYBAGSUPPORT_H_
#define _SECURITYD_SECKEYBAGSUPPORT_H_

#include <CoreFoundation/CoreFoundation.h>
#include <utilities/SecAKSWrappers.h>

#if TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR)
#define USE_KEYSTORE  1
#elif TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
#define USE_KEYSTORE  1
#else /* no keystore on this platform */
#define USE_KEYSTORE  0
#endif

#if USE_KEYSTORE
#include <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>
#endif /* USE_KEYSTORE */

__BEGIN_DECLS

// TODO: Get this out of this file
#if USE_KEYSTORE
typedef int32_t keyclass_t;
#else

/* TODO: this needs to be available in the sim! */
typedef int32_t keyclass_t;
typedef int32_t key_handle_t;
enum key_classes {
    key_class_ak = 6,
    key_class_ck,
    key_class_dk,
    key_class_aku,
    key_class_cku,
    key_class_dku,
    key_class_akpu
};
#endif /* !USE_KEYSTORE */

enum SecKsCryptoOp {
    kSecKsWrap = 10,
    kSecKsUnwrap,
    kSecKsDelete
};


/* KEYBAG_NONE is private to security and have special meaning.
 They should not collide with AppleKeyStore constants, but are only referenced
 in here.
 */
#define KEYBAG_NONE (-1)   /* Set q_keybag to KEYBAG_NONE to obtain cleartext data. */
#define KEYBAG_DEVICE (g_keychain_keybag) /* actual keybag used to encrypt items */
extern keybag_handle_t g_keychain_keybag;

bool use_hwaes(void);
bool ks_crypt(uint32_t operation, keybag_handle_t keybag,
              keyclass_t keyclass, uint32_t textLength, const uint8_t *source, keyclass_t *actual_class,
              CFMutableDataRef dest, CFErrorRef *error);
#if USE_KEYSTORE
bool ks_crypt_acl(uint32_t operation, keybag_handle_t keybag,
                  keyclass_t keyclass, uint32_t textLength, const uint8_t *source,
                  CFMutableDataRef dest, CFDataRef acl, CFDataRef acm_context, CFDataRef caller_access_groups,
                  CFErrorRef *error);
#endif
bool ks_open_keybag(CFDataRef keybag, CFDataRef password, keybag_handle_t *handle, CFErrorRef *error);
bool ks_close_keybag(keybag_handle_t keybag, CFErrorRef *error);

__END_DECLS

#endif /* _SECURITYD_SECKEYBAGSUPPORT_H_ */
