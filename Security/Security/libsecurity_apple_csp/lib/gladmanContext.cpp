/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * gladmanContext.cpp - glue between BlockCryptor and Gladman AES implementation
 */
 
#include "gladmanContext.h"
#include "cspdebugging.h"
#include <CommonCrypto/CommonCryptor.h>

/*
 * AES encrypt/decrypt.
 */
GAESContext::GAESContext(AppleCSPSession &session) : 
    BlockCryptor(session),
	mAesKey(NULL),
	mInitFlag(false),
	mRawKeySize(0),
	mWasEncrypting(false)
{ 
	cbcCapable(false);
	multiBlockCapable(false);
}

GAESContext::~GAESContext()
{
    if(mAesKey) {
        CCCryptorFinal(mAesKey,NULL,0,NULL);
        CCCryptorRelease(mAesKey);
        mAesKey = NULL;
    }
    
	deleteKey();
	memset(mRawKey, 0, MAX_AES_KEY_BITS / 8);
	mInitFlag = false;
}
	
void GAESContext::deleteKey()
{
	mRawKeySize = 0;
}

/* 
 * Standard CSPContext init, called from CSPFullPluginSession::init().
 * Reusable, e.g., query followed by en/decrypt. Even reusable after context
 * changed (i.e., new IV in Encrypted File System). 
 */
void GAESContext::init( 
	const Context &context, 
	bool encrypting)
{
	if(mInitFlag && !opStarted()) {
		return;
	}
	
	CSSM_SIZE	keyLen;
	uint8 		*keyData = NULL;
	bool		sameKeySize = false;
	
	/* obtain key from context */
	symmetricKeyBits(context, session(), CSSM_ALGID_AES, 
		encrypting ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keyData, keyLen);
	
	switch(keyLen) {
		case kCCKeySizeAES128:
		case kCCKeySizeAES192:
		case kCCKeySizeAES256:
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	
	/*
	 * Delete existing key if key size changed
	 */
	if(mRawKeySize == keyLen) {
		sameKeySize = true;
	}
	else {
		deleteKey();
	}
	
	/* 
	 * Init key only if key size or key bits have changed, or 
	 * we're doing a different operation than the previous key
	 * was scheduled for.
	 */
	if(!sameKeySize || (mWasEncrypting != encrypting) ||
		memcmp(mRawKey, keyData, mRawKeySize)) {
        (void) CCCryptorCreateWithMode(0, kCCModeECB, kCCAlgorithmAES128, ccDefaultPadding, NULL, keyData, keyLen, NULL, 0, 0, 0, &mAesKey);

		/* save this raw key data */
		memmove(mRawKey, keyData, keyLen); 
		mRawKeySize = (uint32)keyLen;
		mWasEncrypting = encrypting;
	}

	/* we handle CBC, and hence the IV, ourselves */
	CSSM_ENCRYPT_MODE cssmMode = context.getInt(CSSM_ATTRIBUTE_MODE);
    switch (cssmMode) {
		/* no mode attr --> 0 == CSSM_ALGMODE_NONE, not currently supported */
 		case CSSM_ALGMODE_CBCPadIV8:
		case CSSM_ALGMODE_CBC_IV8:
		{
			CssmData *iv = context.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR);
			if(iv == NULL) {
				CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
			}
			if(iv->Length != kCCBlockSizeAES128) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_INIT_VECTOR);
			}
		}
		break;
		default:
		break;
	}
	
	/* Finally, have BlockCryptor do its setup */
	setup(GLADMAN_BLOCK_SIZE_BYTES, context);
	mInitFlag = true;
}	

/*
 * Functions called by BlockCryptor
 * FIXME make this multi-block capabl3e 
 */
void GAESContext::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void 			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(cipherTextLen < plainTextLen) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
    (void) CCCryptorEncryptDataBlock(mAesKey, NULL, plainText, plainTextLen, cipherText);

	cipherTextLen = plainTextLen;
}

void GAESContext::decryptBlock(
	const void		*cipherText,		// length implied (one cipher block)
	size_t			cipherTextLen,	
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen < cipherTextLen) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
    (void) CCCryptorDecryptDataBlock(mAesKey, NULL, cipherText, cipherTextLen, plainText);
	plainTextLen = cipherTextLen;
}

