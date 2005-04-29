/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		sslBuildFlags.h

	Contains:	Common build flags 

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_SSL_BUILD_FLAGS_H_
#define _SSL_BUILD_FLAGS_H_				1

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * general Keychain functionality.
 */
 
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
	
#if defined(__cplusplus)
}
#endif

#endif	/* _SSL_BUILD_FLAGS_H_ */
