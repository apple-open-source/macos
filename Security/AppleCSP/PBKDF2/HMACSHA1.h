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
 	File:		HMACSHA1.h
 	Contains:	Apple Data Security Services HMAC{SHA1,MD5} function declaration.
 	Copyright:	(C) 1999 by Apple Computer, Inc., all rights reserved
 	Written by:	Michael Brouwer <mb@apple.com>
*/
#ifndef __HMACSHA1__
#define __HMACSHA1__

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <Security/cssmtype.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define kHMACSHA1DigestSize  20
#define kHMACMD5DigestSize	 16

/* XXX These should really be in SHA1.h */
#define kSHA1DigestSize  	20
#define kSHA1BlockSize  	64

/* This function create an HMACSHA1 digest of kHMACSHA1DigestSizestSize bytes
 * and outputs it to resultPtr.  See RFC 2104 for details.  */
void
hmacsha1 (const void *keyPtr, UInt32 keyLen,
		  const void *textPtr, UInt32 textLen,
		  void *resultPtr);
		  
/*
 * Staged version.
 *
 * Opaque reference to an hmac session 
 */
struct hmacContext;
typedef struct hmacContext *hmacContextRef;

hmacContextRef hmacAlloc();
void hmacFree(
	hmacContextRef hmac);
CSSM_RETURN hmacInit(
	hmacContextRef hmac,
	const void *keyPtr,
	UInt32 keyLen,
	CSSM_BOOL sha1Digest);		// true -> SHA1; false -> MD5
CSSM_RETURN hmacUpdate(
	hmacContextRef hmac,
	const void *textPtr,
	UInt32 textLen);
CSSM_RETURN hmacFinal(
	hmacContextRef hmac,
	void *resultPtr);		// caller mallocs, must be kSHA1DigestSize bytes

#ifdef	__cplusplus
}
#endif

#endif /* __HMACSHA1__ */
