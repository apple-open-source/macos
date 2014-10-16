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
 * bfContext.cpp - glue between BlockCrytpor and ssleay Blowfish
 *				 implementation
 */
 
#include "bfContext.h"

BlowfishContext::~BlowfishContext()
{
	deleteKey();
}

void BlowfishContext::deleteKey()
{
	memset(&mBfKey, 0, sizeof(mBfKey));
	mInitFlag = false;
}

/* 
 * Standard CSPContext init, called from CSPFullPluginSession::init().
 * Reusable, e.g., query followed by en/decrypt.
 */
void BlowfishContext::init( 
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
	symmetricKeyBits(context, session(), CSSM_ALGID_BLOWFISH, 
		encrypting ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keyData, keyLen);
	if((keyLen < BF_MIN_KEY_SIZE_BYTES) || (keyLen > BF_MAX_KEY_SIZE_BYTES)) {
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

	/* init key only if key size or key bits have changed */
	if(!sameKeySize || memcmp(mRawKey, keyData, mRawKeySize)) {
		BF_set_key(&mBfKey, (int)keyLen, keyData);
	
		/* save this raw key data */
		memmove(mRawKey, keyData, keyLen); 
		mRawKeySize = (unsigned int)keyLen;
	}
	
	/* Finally, have BlockCryptor do its setup */
	setup(BF_BLOCK, context);
	mInitFlag = true;
}	

/*
 * Functions called by BlockCryptor
 */
void BlowfishContext::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen != BF_BLOCK) {
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
	}
	if(cipherTextLen < BF_BLOCK) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	BF_ecb_encrypt((const unsigned char *)plainText, (unsigned char *)cipherText,
		&mBfKey, BF_ENCRYPT);
	cipherTextLen = BF_BLOCK;
}

void BlowfishContext::decryptBlock(
	const void		*cipherText,		// length implied (one block)
	size_t			cipherTextLen,
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen < BF_BLOCK) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	BF_ecb_encrypt((const unsigned char *)cipherText, (unsigned char *)plainText,
		&mBfKey, BF_DECRYPT);
	plainTextLen = BF_BLOCK;
}
