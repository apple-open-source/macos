/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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
    @header SecMLDSAKey.h
    The functions provided in SecMLDSAKey.h implement and manage ML-DSA cipher
*/

#ifndef _SECURITY_SECMLDSAKEY_H_
#define _SECURITY_SECMLDSAKEY_H_

#include <Security/SecBase.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>

__BEGIN_DECLS

SecKeyRef SecKeyCreateMLDSAPublicKey(CFAllocatorRef allocator, const uint8_t *keyData, CFIndex keyDataLength);
SecKeyRef SecKeyCreateMLDSAPrivateKey(CFAllocatorRef allocator, const uint8_t *keyData, CFIndex keyDataLength);
OSStatus SecMLDSAKeyGeneratePair(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey);

__END_DECLS

#endif /* !_SECURITY_SECMLDSAKEY_H_ */
