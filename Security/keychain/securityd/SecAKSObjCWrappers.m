/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "SecAKSObjCWrappers.h"
#if __has_include(<UserManagement/UserManagement.h>)
#import <UserManagement/UserManagement.h>
#endif

@implementation SecAKSObjCWrappers

+ (bool)aksEncryptWithKeybag:(keybag_handle_t)keybag keyclass:(keyclass_t)keyclass plaintext:(NSData*)plaintext
                 outKeyclass:(keyclass_t*)outKeyclass ciphertext:(NSMutableData*)ciphertext personaId:(const void*)personaId personaIdLength:(size_t)personaIdLength error:(NSError**)error
{
    CFErrorRef cfError = NULL;
    bool result = false;
    if (personaId) {
        result = ks_crypt_diversify(kAKSKeyOpEncrypt, keybag, keyclass, (uint32_t)plaintext.length, plaintext.bytes, outKeyclass, (__bridge CFMutableDataRef)ciphertext, personaId, personaIdLength, &cfError);
    } else {
        result = ks_crypt(kAKSKeyOpEncrypt, keybag, keyclass, (uint32_t)plaintext.length, plaintext.bytes, outKeyclass, (__bridge CFMutableDataRef)ciphertext, &cfError);
    }
    BridgeCFErrorToNSErrorOut(error, cfError);
    return result;
}

+ (bool)aksDecryptWithKeybag:(keybag_handle_t)keybag keyclass:(keyclass_t)keyclass ciphertext:(NSData*)ciphertext
                 outKeyclass:(keyclass_t*)outKeyclass plaintext:(NSMutableData*)plaintext personaId:(const void*)personaId personaIdLength:(size_t)personaIdLength error:(NSError**)error
{
    CFErrorRef cfError = NULL;
    bool result = false;
    if (personaId) {
        result = ks_crypt_diversify(kAKSKeyOpDecrypt, keybag, keyclass, (uint32_t)ciphertext.length, ciphertext.bytes, outKeyclass, (__bridge CFMutableDataRef)plaintext, personaId, personaIdLength, &cfError);
    } else {
        result = ks_crypt(kAKSKeyOpDecrypt, keybag, keyclass, (uint32_t)ciphertext.length, ciphertext.bytes, outKeyclass, (__bridge CFMutableDataRef)plaintext, &cfError);
    }
    BridgeCFErrorToNSErrorOut(error, cfError);
    return result;
}

@end
