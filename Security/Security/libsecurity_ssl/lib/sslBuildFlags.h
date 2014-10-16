/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012,2014 Apple Inc. All Rights Reserved.
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
 * sslBuildFlags.h - Common build flags
 */

#ifndef	_SSL_BUILD_FLAGS_H_
#define _SSL_BUILD_FLAGS_H_				1

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Implementation-specific functionality.
 */
#if 1
#undef USE_CDSA_CRYPTO				/* use corecrypto, instead of CDSA */
#undef USE_SSLCERTIFICATE			/* use CF-based certs, not structs */
#endif

/*
 * Work around the Netscape Server Key Exchange bug. When this is
 * true, only do server key exchange if both of the following are
 * true:
 *
 *   -- an export-grade ciphersuite has been negotiated, and
 *   -- an encryptPrivKey is present in the context
 */
#define SSL_SERVER_KEYEXCH_HACK		0

/*
 * RSA functions which use a public key to do encryption force
 * the proper usage bit because the CL always gives us
 * a pub key (from a cert) with only the verify bit set.
 * This needs a mod to the CL to do the right thing, and that
 * might not be enough - what if server certs don't have the
 * appropriate usage bits?
 */
#define RSA_PUB_KEY_USAGE_HACK		1

/* debugging flags */
#ifdef	NDEBUG
#define SSL_DEBUG					0
#define ERROR_LOG_ENABLE			0
#else
#define SSL_DEBUG					1
#define ERROR_LOG_ENABLE			1
#endif	/* NDEBUG */

/*
 * Server-side PAC-based EAP support currently enabled only for debug builds.
 */
#ifdef	NDEBUG
#define SSL_PAC_SERVER_ENABLE		0
#else
#define SSL_PAC_SERVER_ENABLE		1
#endif

#define ENABLE_SSLV2                0

/* Experimental */
#define ENABLE_DTLS                 1

#define ENABLE_3DES			1		/* normally enabled */
#define ENABLE_RC4			1		/* normally enabled */
#define ENABLE_DES			0		/* normally disabled */
#define ENABLE_RC2			0		/* normally disabled */
#define ENABLE_AES			1		/* normally enabled, our first preference */
#define ENABLE_AES256		1		/* normally enabled */
#define ENABLE_ECDHE		1
#define ENABLE_ECDHE_RSA	1
#define ENABLE_ECDH			1
#define ENABLE_ECDH_RSA		1

#if defined(__cplusplus)
}
#endif

#endif	/* _SSL_BUILD_FLAGS_H_ */
