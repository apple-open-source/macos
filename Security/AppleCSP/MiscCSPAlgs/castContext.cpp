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
 * castContext.cpp - glue between BlockCrytpor and ssleay CAST128 (CAST5)
 *				 implementation
 *
 * Written by Doug Mitchell 4/24/2003
 */
 
#include "castContext.h"

CastContext::~CastContext()
{
	deleteKey();
}

void CastContext::deleteKey()
{
	memset(&mCastKey, 0, sizeof(mCastKey));
	mInitFlag = false;
}

/* 
 * Standard CSPContext init, called from CSPFullPluginSession::init().
 * Reusable, e.g., query followed by en/decrypt.
 */
void CastContext::init( 
	const Context &context, 
	bool encrypting)
{
	if(mInitFlag && !opStarted()) {
		return;
	}

	UInt32 		keyLen;
	UInt8 		*keyData = NULL;
	bool		sameKeySize = false;
	
	/* obtain key from context */
	symmetricKeyBits(context, CSSM_ALGID_CAST, 
		encrypting ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keyData, keyLen);
	if((keyLen < CAST_MIN_KEY_LENGTH) || (keyLen > CAST_KEY_LENGTH)) {
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
		CAST_set_key(&mCastKey, keyLen, keyData);
	
		/* save this raw key data */
		memmove(mRawKey, keyData, keyLen); 
		mRawKeySize = keyLen;
	}
	
	/* Finally, have BlockCryptor do its setup */
	setup(CAST_BLOCK, context);
	mInitFlag = true;
}	

/*
 * Functions called by BlockCryptor
 */
void CastContext::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen != CAST_BLOCK) {
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
	}
	if(cipherTextLen < CAST_BLOCK) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	CAST_ecb_encrypt((const unsigned char *)plainText, (unsigned char *)cipherText,
		&mCastKey, CAST_ENCRYPT);
	cipherTextLen = CAST_BLOCK;
}

void CastContext::decryptBlock(
	const void		*cipherText,		// length implied (one block)
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen < CAST_BLOCK) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	CAST_ecb_encrypt((const unsigned char *)cipherText, (unsigned char *)plainText,
		&mCastKey, CAST_DECRYPT);
	plainTextLen = CAST_BLOCK;
}
