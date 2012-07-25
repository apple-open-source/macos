/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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

/*
 * sslBER.h - BER routines
 */

#ifndef	_SSL_BER_H_
#define _SSL_BER_H_

#ifndef	_SSL_PRIV_H_
#include "sslPriv.h"
#endif

#ifdef __cplusplus
extern	"C" {
#endif

/*
 * Given a PKCS-1 encoded RSA public key, extract the
 * modulus and public exponent.
 *
 * RSAPublicKey ::= SEQUENCE {
 *		modulus INTEGER, -- n
 *		publicExponent INTEGER -- e }
 */

OSStatus sslDecodeRsaBlob(
	const SSLBuffer	*blob,			/* PKCS-1 encoded */
	SSLBuffer		*modulus,		/* data mallocd and RETURNED */
	SSLBuffer		*exponent);		/* data mallocd and RETURNED */

/*
 * Given a raw modulus and exponent, cook up a
 * BER-encoded RSA public key blob.
 */

OSStatus sslEncodeRsaBlob(
	const SSLBuffer	*modulus,
	const SSLBuffer	*exponent,
	SSLBuffer		*blob);			/* data mallocd and RETURNED */

/*
 * Given a DER encoded DHParameter, extract the prime and generator.
 * modulus and public exponent.
 */
OSStatus sslDecodeDhParams(
	const SSLBuffer	*blob,			/* PKCS-1 encoded */
	SSLBuffer		*prime,			/* data mallocd and RETURNED */
	SSLBuffer		*generator);	/* data mallocd and RETURNED */

/*
 * Given a prime and generator, cook up a BER-encoded DHParameter blob.
 */
OSStatus sslEncodeDhParams(
	const SSLBuffer	*prime,
	const SSLBuffer	*generator,
	SSLBuffer		*blob);			/* data mallocd and RETURNED */

/*
 * Given an ECDSA public key in CSSM format, extract the SSL_ECDSA_NamedCurve
 * from its algorithm parameters.
 */
OSStatus sslEcdsaPeerCurve(
	CSSM_KEY_PTR pubKey,
	SSL_ECDSA_NamedCurve *namedCurve);

/*
 * Given an ECDSA public key in X509 format, extract the raw public key
 * bits in ECPOint format.
 */
OSStatus sslEcdsaPubKeyBits(
	CSSM_KEY_PTR	pubKey,
	SSLBuffer		*pubBits);		/* data mallocd and RETURNED */

#ifdef __cplusplus
}
#endif

#endif	/* _SSL_BER_H_ */
