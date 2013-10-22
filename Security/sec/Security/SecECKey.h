/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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
	@header SecECKey
	The functions provided in SecECKey.h implement and manage a rsa
    public or private key.
*/

#ifndef _SECURITY_SECECKEY_H_
#define _SECURITY_SECECKEY_H_

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CFData.h>

__BEGIN_DECLS

typedef struct SecECPublicKeyParams {
	uint8_t             *modulus;			/* modulus */
	CFIndex             modulusLength;
	uint8_t             *exponent;			/* public exponent */
	CFIndex             exponentLength;
} SecECPublicKeyParams;

/* Given an EC public key in encoded form return a SecKeyRef representing
   that key. Supported encodings are kSecKeyEncodingPkcs1. */
SecKeyRef SecKeyCreateECPublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding);

/* Given an EC private key in encoded form return a SecKeyRef representing
   that key.  Supported encodings are kSecKeyEncodingPkcs1. */
SecKeyRef SecKeyCreateECPrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding);

/* @@@ Private @@@ */
OSStatus SecECKeyGeneratePair(CFDictionaryRef parameters,
    SecKeyRef *rsaPublicKey, SecKeyRef *rsaPrivateKey);

/* These are the named curves we support. These values come from RFC 4492
   section 5.1.1, with the exception of SSL_Curve_None which means
   "ECDSA not negotiated". */
typedef enum
{
	kSecECCurveNone = -1,
	kSecECCurveSecp256r1 = 23,
	kSecECCurveSecp384r1 = 24,
	kSecECCurveSecp521r1 = 25
} SecECNamedCurve;

/* Return a named curve enum for ecPrivateKey. */
SecECNamedCurve SecECKeyGetNamedCurve(SecKeyRef ecPrivateKey);
CFDataRef SecECKeyCopyPublicBits(SecKeyRef key);

__END_DECLS

#endif /* !_SECURITY_SECECKEY_H_ */
