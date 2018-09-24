/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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

/* debugging flags */
#ifdef	NDEBUG
#define SSL_DEBUG					0
#else
#define SSL_DEBUG					1
#endif	/* NDEBUG */


#define ENABLE_DTLS                 1

#define ENABLE_3DES			1		/* normally enabled */
#define ENABLE_AES			1		/* normally enabled, our first preference */
#define ENABLE_AES256		1		/* normally enabled */
#define ENABLE_ECDHE		1
#define ENABLE_ECDHE_RSA	1
#define ENABLE_AES_GCM      1


/*
 * Flags support for ECDSA on the server side.
 * Not implemented as of 6 Aug 2008.
 */
#define SSL_ECDSA_SERVER	0

/* Diffie-Hellman support */
#define APPLE_DH		1

/* Allow server to send a RSA key exchange, if rsaEncryptPubKey is specified by the client app */
#define ALLOW_RSA_SERVER_KEY_EXCHANGE 1

#if defined(__cplusplus)
}
#endif

#endif	/* _SSL_BUILD_FLAGS_H_ */
