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

#import "TPDummyDecrypter.h"

@implementation TPDummyDecrypter

+ (instancetype)dummyDecrypter
{
    return [[TPDummyDecrypter alloc] init];
}

- (nullable NSData *)decryptData:(NSData *)ciphertext
                         withKey:(NSData *)key
                           error:(NSError **)error
{
    // Repeating-key XOR
    NSMutableData *plaintext = [NSMutableData dataWithLength:ciphertext.length];
    uint8_t *plainbytes = plaintext.mutableBytes;
    const uint8_t *cipherbytes = ciphertext.bytes;
    const uint8_t *keybytes = key.bytes;
    NSUInteger keylen = key.length;
    for (NSUInteger i = 0; i < ciphertext.length; i++) {
        plainbytes[i] = cipherbytes[i] ^ keybytes[i % keylen];
    }
    return plaintext;
}

@end
