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
 * rc2Context.cpp - glue between BlockCrytpor and ssleay RC2 implementation
 */
 
#include <openssl/rc2_legacy.h>
#include <misc/rc2_locl.h>
#include "rc2Context.h"

RC2Context::~RC2Context()
{
	memset(&rc2Key, 0, sizeof(RC2_KEY));
}
	
/* 
 * Standard CSPContext init, called from CSPFullPluginSession::init().
 * Reusable, e.g., query followed by en/decrypt.
 */
void RC2Context::init( 
	const Context &context, 
	bool encrypting)
{
	CSSM_SIZE	keyLen;
	uint8 		*keyData 	= NULL;
	uint32		effectiveBits;
	
	/* obtain key from context */
	symmetricKeyBits(context, session(), CSSM_ALGID_RC2, 
		encrypting ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keyData, keyLen);
	if((keyLen < RC2_MIN_KEY_SIZE_BYTES) || (keyLen > RC2_MAX_KEY_SIZE_BYTES)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	
	/* 
	 * Optional effective key size in bits - either from Context,
	 * or the key
	 */
	effectiveBits = context.getInt(CSSM_ATTRIBUTE_EFFECTIVE_BITS);
	if(effectiveBits == 0) {
		CssmKey &key = context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, 
			CSSMERR_CSP_MISSING_ATTR_KEY);
		effectiveBits = key.KeyHeader.LogicalKeySizeInBits;
	}

	/* init the low-level state */
	RC2_set_key(&rc2Key, (int)keyLen, keyData, effectiveBits);

	/* Finally, have BlockCryptor do its setup */
	setup(RC2_BLOCK_SIZE_BYTES, context);
}	

/*
 * Functions called by BlockCryptor
 */
void RC2Context::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen != RC2_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
	}
	if(cipherTextLen < RC2_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	
	/*
	 * Low-level code operates on array of unsigned 32-bit integers 
	 */
	RC2_INT	d[2];
	RC2_INT l;
	const unsigned char *pt = (const unsigned char *)plainText;
	c2l(pt, l); d[0]=l;
	c2l(pt, l); d[1]=l;
	RC2_encrypt(d, &rc2Key);
	unsigned char *ct = (unsigned char *)cipherText;
	l=d[0]; l2c(l, ct);
	l=d[1]; l2c(l, ct);
	cipherTextLen = RC2_BLOCK_SIZE_BYTES;
}

void RC2Context::decryptBlock(
	const void		*cipherText,		// length implied (one block)
	size_t			cipherTextLen,
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen < RC2_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	/*
	 * Low-level code operates on array of unsigned 32-bit integers 
	 */
	RC2_INT	d[2];
	RC2_INT l;
	const unsigned char *ct = (const unsigned char *)cipherText;
	c2l(ct, l); d[0]=l;
	c2l(ct, l); d[1]=l;
	RC2_decrypt(d, &rc2Key);
	unsigned char *pt = (unsigned char *)plainText;
	l=d[0]; l2c(l, pt);
	l=d[1]; l2c(l, pt);
	plainTextLen = RC2_BLOCK_SIZE_BYTES;
}

