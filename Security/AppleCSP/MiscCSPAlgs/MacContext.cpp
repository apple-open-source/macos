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
 * MacContext.cpp - AppleCSPContext for HMACSHA1
 */

#include "MacContext.h"
#include <PBKDF2/HMACSHA1.h>
#include <Security/cssmerr.h>
#include <Security/utilities.h>
#ifdef	CRYPTKIT_CSP_ENABLE
#include <CryptKit/HmacSha1Legacy.h>
#endif	/* CRYPTKIT_CSP_ENABLE */

MacContext::~MacContext()
{
	if(mHmac) {
		hmacFree(mHmac);
		mHmac = NULL;
	}
}
	
/* called out from CSPFullPluginSession....
 * both generate and verify: */
void MacContext::init(const Context &context, bool isSigning)
{
	if(mHmac == NULL) {
		mHmac = hmacAlloc();
		if(mHmac == NULL) {
			CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
		}
	}
	
	/* obtain key from context */
	UInt32 		keyLen;
	UInt8 		*keyData 	= NULL;
	
	symmetricKeyBits(context, CSSM_ALGID_SHA1HMAC, 
		isSigning ? CSSM_KEYUSE_SIGN : CSSM_KEYUSE_VERIFY,
		keyData, keyLen);
	if((keyLen < HMAC_MIN_KEY_SIZE) || (keyLen > HMAC_MAX_KEY_SIZE)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	
	CSSM_RETURN crtn = hmacInit(mHmac, keyData, keyLen);
	if(crtn) {
		CssmError::throwMe(crtn);
	}
}

void MacContext::update(const CssmData &data)
{
	CSSM_RETURN crtn = hmacUpdate(mHmac,
		data.data(),
		data.length());
	if(crtn) {
		CssmError::throwMe(crtn);
	}
}

/* generate only */
void MacContext::final(CssmData &out)
{
	if(out.length() < kHMACSHA1DigestSize) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	hmacFinal(mHmac, out.data());
}

/* verify only */
void MacContext::final(const CssmData &in)
{
	unsigned char mac[kHMACSHA1DigestSize];
	hmacFinal(mHmac, mac);
	if(memcmp(mac, in.data(), kHMACSHA1DigestSize)) {
		CssmError::throwMe(CSSMERR_CSP_VERIFY_FAILED);
	}
}

size_t MacContext::outputSize(bool final, size_t inSize)
{
	return kHMACSHA1DigestSize;
}

#ifdef 	CRYPTKIT_CSP_ENABLE

MacLegacyContext::~MacLegacyContext()
{
	if(mHmac) {
		hmacLegacyFree(mHmac);
		mHmac = NULL;
	}
}
	
/* called out from CSPFullPluginSession....
 * both generate and verify: */
void MacLegacyContext::init(const Context &context, bool isSigning)
{
	if(mHmac == NULL) {
		mHmac = hmacLegacyAlloc();
		if(mHmac == NULL) {
			CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
		}
	}
	
	/* obtain key from context */
	UInt32 		keyLen;
	UInt8 		*keyData 	= NULL;
	
	/* FIXME - this may require a different key alg */
	symmetricKeyBits(context, CSSM_ALGID_SHA1HMAC, 
		isSigning ? CSSM_KEYUSE_SIGN : CSSM_KEYUSE_VERIFY,
		keyData, keyLen);
	if((keyLen < HMAC_MIN_KEY_SIZE) || (keyLen > HMAC_MAX_KEY_SIZE)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	
	OSStatus ortn = hmacLegacyInit(mHmac, keyData, keyLen);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
}

void MacLegacyContext::update(const CssmData &data)
{
	OSStatus ortn = hmacLegacyUpdate(mHmac,
		data.data(),
		data.length());
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
}

/* generate only */
void MacLegacyContext::final(CssmData &out)
{
	if(out.length() < kHMACSHA1DigestSize) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	hmacLegacyFinal(mHmac, out.data());
}

/* verify only */
void MacLegacyContext::final(const CssmData &in)
{
	unsigned char mac[kHMACSHA1DigestSize];
	hmacLegacyFinal(mHmac, mac);
	if(memcmp(mac, in.data(), kHMACSHA1DigestSize)) {
		CssmError::throwMe(CSSMERR_CSP_VERIFY_FAILED);
	}
}

size_t MacLegacyContext::outputSize(bool final, size_t inSize)
{
	return kHMACSHA1DigestSize;
}

#endif	/* CRYPTKIT_CSP_ENABLE */
