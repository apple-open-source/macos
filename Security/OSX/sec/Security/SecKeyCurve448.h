/*
 * Copyright (c) 2010,2012-2014 Apple Inc. All Rights Reserved.
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
	@header SecKeyCurve448.h
	The functions provided in SecKeyCurve448.h implement and manage Curve448
    public or private key.
*/

#ifndef _SECURITY_SECKEYCURVE448_H_
#define _SECURITY_SECKEYCURVE448_H_

#include <Security/SecBase.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CFData.h>

__BEGIN_DECLS

/* Given an Ed448 public key in encoded form return a SecKeyRef representing
   that key. Supported encoding are kSecKeyEncodingBytes. */
SecKeyRef SecKeyCreateEd448PublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding);

/* Given an Ed448 private key in encoded form return a SecKeyRef representing
   that key. Supported encoding is 57 bytes of key material. */
SecKeyRef SecKeyCreateEd448PrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding);

/* Given an X448 public key in encoded form return a SecKeyRef representing
   that key. Supported encoding are kSecKeyEncodingBytes. */
SecKeyRef SecKeyCreateX448PublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding);

/* Given an X448 private key in encoded form return a SecKeyRef representing
   that key. Supported encoding is 56 bytes of key material. */
SecKeyRef SecKeyCreateX448PrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding);

__END_DECLS

#endif /* !_SECURITY_SECKEYCURVE448_H_ */
