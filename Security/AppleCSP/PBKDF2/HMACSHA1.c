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
#include <CryptKit/SHA1.h>
#include <string.h>
#include <stdlib.h>		// for malloc - maybe we should use CssmAllocator?
#include <Security/cssmerr.h>

struct hmacContext {
	sha1Obj sha1Context;
	UInt8 	k_opad[kSHA1BlockSize];
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
		if(hmac->sha1Context != NULL) {
			sha1Free (hmac->sha1Context);
		}
		memset(hmac, 0, sizeof(struct hmacContext));
		free(hmac);
	}
}

/* reusable init */
CSSM_RETURN hmacInit(
	hmacContextRef hmac,
	const void *keyPtr,
	UInt32 keyLen)
{	
	UInt8 	tk[kSHA1DigestSize];
	UInt8 	*key;
	UInt32 	byte;
	UInt8 	k_ipad[kSHA1BlockSize];

	if(hmac->sha1Context == NULL) {
		hmac->sha1Context = sha1Alloc();
		if(hmac->sha1Context == NULL) {
			return CSSMERR_CSP_MEMORY_ERROR;
		}
	}
	else {
		sha1Reinit(hmac->sha1Context);
	}
	
	/* If the key is longer than kSHA1BlockSize reset it to key=SHA1(key) */
	if (keyLen <= kSHA1BlockSize)
		key = (UInt8*)keyPtr;
	else {
		sha1AddData(hmac->sha1Context, (UInt8*)keyPtr, keyLen);
		memcpy (tk, sha1Digest(hmac->sha1Context), kSHA1DigestSize);
		key = tk;
		keyLen = kSHA1DigestSize;
		sha1Reinit (hmac->sha1Context);
	}
	
	/* The HMAC_SHA_1 transform looks like:
	   SHA1 (K XOR opad || SHA1 (K XOR ipad || text))
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
	sha1AddData (hmac->sha1Context, k_ipad, kSHA1BlockSize);
	return CSSM_OK;
}

CSSM_RETURN hmacUpdate(
	hmacContextRef hmac,
	const void *textPtr,
	UInt32 textLen)
{
	sha1AddData (hmac->sha1Context, (UInt8*)textPtr, textLen);
	return CSSM_OK;
}

CSSM_RETURN hmacFinal(
	hmacContextRef hmac,
	void *resultPtr)		// caller mallocs, must be HMACSHA1_OUT_SIZE bytes
{
	memcpy (resultPtr, sha1Digest (hmac->sha1Context), kSHA1DigestSize);
	sha1Reinit (hmac->sha1Context);
	/* Perform outer SHA1 */
	sha1AddData (hmac->sha1Context, hmac->k_opad, kSHA1BlockSize);
	sha1AddData (hmac->sha1Context, (UInt8*)resultPtr, kSHA1DigestSize);
	memcpy (resultPtr, sha1Digest (hmac->sha1Context), kSHA1DigestSize);
	return CSSM_OK;
}

/* one-shot, ignoring memory errors. */
void
hmacsha1 (const void *keyPtr, UInt32 keyLen,
		  const void *textPtr, UInt32 textLen,
		  void *resultPtr)
{
	hmacContextRef hmac = hmacAlloc();
	hmacInit(hmac, keyPtr, keyLen);
	hmacUpdate(hmac, textPtr, textLen);
	hmacFinal(hmac, resultPtr);
	hmacFree(hmac);
}

