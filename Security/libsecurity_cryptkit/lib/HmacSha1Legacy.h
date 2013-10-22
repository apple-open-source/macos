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
 	File:		HmacSha1Legacy.h
 	Contains:	HMAC/SHA1, bug-for-bug compatible a legacy implementation.
 	Copyright:	(C) 2001 by Apple Computer, Inc., all rights reserved
 	Written by:	Doug Mitchell
*/
#ifndef __HMAC_SHA1_LEGACY__
#define __HMAC_SHA1_LEGACY__

#if	!defined(__MACH__)
#include <ckconfig.h>
#else
#include <security_cryptkit/ckconfig.h>
#endif

#if	CRYPTKIT_HMAC_LEGACY

#include <MacTypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This version is bug-for-bug compatible with the HMACSHA1 implementation in 
 * an old crypto library. 
 */
struct hmacLegacyContext;
typedef struct hmacLegacyContext *hmacLegacyContextRef;

hmacLegacyContextRef hmacLegacyAlloc(void);
void hmacLegacyFree(
	hmacLegacyContextRef hmac);
OSStatus hmacLegacyInit(
	hmacLegacyContextRef hmac,
	const void *keyPtr,
	UInt32 keyLen);
OSStatus hmacLegacyUpdate(
	hmacLegacyContextRef hmac,
	const void *textPtr,
	UInt32 textLen);
OSStatus hmacLegacyFinal(
	hmacLegacyContextRef hmac,
	void *resultPtr);		// caller mallocs, must be kSHA1DigestSize bytes

#ifdef	__cplusplus
}
#endif

#endif	/* CRYPTKIT_HMAC_LEGACY */

#endif	/* __HMAC_SHA1_LEGACY__ */
