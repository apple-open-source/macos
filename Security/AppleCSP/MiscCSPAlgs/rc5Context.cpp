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
 * rc5Context.cpp - glue between BlockCrytpor and ssleay RC5 implementation
 * Written by Doug Mitchell 04/03/2001
 */
 
#include <openssl/rc5.h>
#include <misc/rc5_locl.h>
#include "rc5Context.h"

RC5Context::~RC5Context()
{
	memset(&rc5Key, 0, sizeof(RC5_32_KEY));
}
	
/* 
 * Standard CSPContext init, called from CSPFullPluginSession::init().
 * Reusable, e.g., query followed by en/decrypt.
 */
void RC5Context::init( 
	const Context &context, 
	bool encrypting)
{
	UInt32 		keyLen;
	UInt8 		*keyData 	= NULL;
	UInt32		rounds = RC5_16_ROUNDS;
	
	/* obtain key from context */
	symmetricKeyBits(context, CSSM_ALGID_RC5, 
		encrypting ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keyData, keyLen);
	if((keyLen < RC5_MIN_KEY_SIZE_BYTES) || (keyLen > RC5_MAX_KEY_SIZE_BYTES)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	
	/* 
	 * Optional rounds
	 */
	rounds = context.getInt(CSSM_ATTRIBUTE_ROUNDS);
	if(rounds == 0) {
		/* default */
		rounds = RC5_16_ROUNDS;
	}

	/* init the low-level state */
	RC5_32_set_key(&rc5Key, keyLen, keyData, rounds);

	/* Finally, have BlockCryptor do its setup */
	setup(RC5_BLOCK_SIZE_BYTES, context);
}	

/*
 * Functions called by BlockCryptor
 */
void RC5Context::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen != RC5_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
	}
	if(cipherTextLen < RC5_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	
	/*
	 * Low-level code operates on array of unsigned 32-bit integers 
	 */
	RC5_32_INT	d[2];
	RC5_32_INT l;
	c2l((unsigned char *)plainText, l); d[0]=l;
	c2l((unsigned char *)plainText, l); d[1]=l;
	RC5_32_encrypt(d, &rc5Key);
	l=d[0]; l2c(l, (unsigned char *)cipherText);
	l=d[1]; l2c(l, (unsigned char *)cipherText);
	cipherTextLen = RC5_BLOCK_SIZE_BYTES;
}

void RC5Context::decryptBlock(
	const void		*cipherText,		// length implied (one block)
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen < RC5_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	/*
	 * Low-level code operates on array of unsigned 32-bit integers 
	 */
	RC5_32_INT	d[2];
	RC5_32_INT l;
	c2l((unsigned char *)cipherText, l); d[0]=l;
	c2l((unsigned char *)cipherText, l); d[1]=l;
	RC5_32_decrypt(d, &rc5Key);
	l=d[0]; l2c(l, (unsigned char *)plainText);
	l=d[1]; l2c(l, (unsigned char *)plainText);
	plainTextLen = RC5_BLOCK_SIZE_BYTES;
}

