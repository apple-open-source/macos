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
 	File:		HMACSHA1.c
 	Contains:	Apple Data Security Services HMACSHA1 function definition.
 	Copyright:	(C) 1999 by Apple Computer, Inc., all rights reserved
 	Written by:	Michael Brouwer <mb@apple.com>
*/
#include "HMACSHA1.h"
#include "pbkdDigest.h"
#include <MiscCSPAlgs/SHA1.h>
#include <MiscCSPAlgs/MD5.h>
#include <string.h>
#include <stdlib.h>		// for malloc - maybe we should use CssmAllocator?
#include <Security/cssmerr.h>



struct hmacContext {
	DigestCtx	digest;
	UInt8 		k_opad[kSHA1BlockSize];
};

hmacContextRef hmacAlloc()
{
	hmacContextRef hmac = (hmacContextRef)malloc(sizeof(struct hmacContext));
	memset(hmac, 0, sizeof(struct hmacContext));
	return hmac;
}

void hmacFree(
	hmacContextRef hmac)
{
	if(hmac != NULL) {
		DigestCtxFree(&hmac->digest);
		memset(hmac, 0, sizeof(struct hmacContext));
		free(hmac);
	}
}

/* reusable init */
CSSM_RETURN hmacInit(
	hmacContextRef hmac,
	const void *keyPtr,
	UInt32 keyLen,
	CSSM_BOOL isSha1)		// true -> SHA1; false -> MD5
{	
	UInt8 	tk[kSHA1DigestSize];
	UInt8 	*key;
	UInt32 	byte;
	UInt8 	k_ipad[kSHA1BlockSize];
	UInt32	digestSize = sha1Digest ? kSHA1DigestSize : MD5_DIGEST_SIZE;
	
	DigestCtxInit(&hmac->digest, isSha1);
	
	/* If the key is longer than kSHA1BlockSize reset it to key=digest(key) */
	if (keyLen <= kSHA1BlockSize)
		key = (UInt8*)keyPtr;
	else {
		DigestCtxUpdate(&hmac->digest, (UInt8*)keyPtr, keyLen);
		DigestCtxFinal(&hmac->digest, tk);
		key = tk;
		keyLen = digestSize;
		DigestCtxInit(&hmac->digest, isSha1);
	}
	
	/* The HMAC_<DIG> transform looks like:
	   <DIG> (K XOR opad || <DIG> (K XOR ipad || text))
	   Where K is a n byte key
	   ipad is the byte 0x36 repeated 64 times.
	   opad is the byte 0x5c repeated 64 times.
	   text is the data being protected.
	  */
	/* Copy the key into k_ipad and k_opad while doing the XOR. */
	for (byte = 0; byte < keyLen; byte++)
	{
		k_ipad[byte] = key[byte] ^ 0x36;
		hmac->k_opad[byte] = key[byte] ^ 0x5c;
	}
	/* Fill the remainder of k_ipad and k_opad with 0 XORed with the appropriate value. */
	if (keyLen < kSHA1BlockSize)
	{
		memset (k_ipad + keyLen, 0x36, kSHA1BlockSize - keyLen);
		memset (hmac->k_opad + keyLen, 0x5c, kSHA1BlockSize - keyLen);
	}
	DigestCtxUpdate(&hmac->digest, k_ipad, kSHA1BlockSize);
	return CSSM_OK;
}

CSSM_RETURN hmacUpdate(
	hmacContextRef hmac,
	const void *textPtr,
	UInt32 textLen)
{
	DigestCtxUpdate(&hmac->digest, textPtr, textLen);
	return CSSM_OK;
}

CSSM_RETURN hmacFinal(
	hmacContextRef hmac,
	void *resultPtr)		// caller mallocs, must be appropriate output size for
							// current digest algorithm 
{
	UInt32 digestSize = hmac->digest.isSha1 ? kSHA1DigestSize : kHMACMD5DigestSize;
	
	DigestCtxFinal(&hmac->digest, resultPtr);
	DigestCtxInit(&hmac->digest, hmac->digest.isSha1);
	/* Perform outer digest */
	DigestCtxUpdate(&hmac->digest, hmac->k_opad, kSHA1BlockSize);
	DigestCtxUpdate(&hmac->digest, resultPtr, digestSize);
	DigestCtxFinal(&hmac->digest, resultPtr);
	return CSSM_OK;
}

/* one-shot, ignoring memory errors. */
void
hmacsha1 (const void *keyPtr, UInt32 keyLen,
		  const void *textPtr, UInt32 textLen,
		  void *resultPtr)
{
	hmacContextRef hmac = hmacAlloc();
	hmacInit(hmac, keyPtr, keyLen, CSSM_TRUE);
	hmacUpdate(hmac, textPtr, textLen);
	hmacFinal(hmac, resultPtr);
	hmacFree(hmac);
}

