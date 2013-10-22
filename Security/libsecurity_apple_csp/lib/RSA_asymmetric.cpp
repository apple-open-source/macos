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
 * RSA_asymmetric.cpp - CSPContext for RSA asymmetric encryption
 */
 
#include "RSA_asymmetric.h"
#include "RSA_DSA_utils.h"
#include <security_utilities/debugging.h>
#include <opensslUtils/opensslUtils.h>

#define rsaCryptDebug(args...)	secdebug("rsaCrypt", ## args)
#define rbprintf(args...)		secdebug("rsaBuf", ## args)

static ModuleNexus<Mutex> gMutex;

RSA_CryptContext::~RSA_CryptContext()
{
	StLock<Mutex> _(gMutex());
	if(mAllocdRsaKey) {
		assert(mRsaKey != NULL);
		RSA_free(mRsaKey);
		mRsaKey = NULL;
		mAllocdRsaKey = false;
	}
}
	
/* called by CSPFullPluginSession */
void RSA_CryptContext::init(const Context &context, bool encoding /*= true*/)
{
	StLock<Mutex> _(gMutex());
	
	if(mInitFlag && !opStarted()) {
		/* reusing - e.g. query followed by encrypt */
		return;
	}

	/* optional mode to use alternate key class (e.g., decrypt with public key) */
	CSSM_KEYCLASS  keyClass;
    switch (context.getInt(CSSM_ATTRIBUTE_MODE)) {
        case CSSM_ALGMODE_PUBLIC_KEY:
			keyClass = CSSM_KEYCLASS_PUBLIC_KEY;
            break;
        case CSSM_ALGMODE_PRIVATE_KEY:
			keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
            break;
        case CSSM_ALGMODE_NONE:	
			/* default, not present in context: infer from op type */
			keyClass = encoding ? CSSM_KEYCLASS_PUBLIC_KEY : CSSM_KEYCLASS_PRIVATE_KEY;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_MODE);
	}
	
	/* fetch key from context */
	if(mRsaKey == NULL) {
		assert(!opStarted());
		CSSM_DATA label = {0, NULL};
		mRsaKey = contextToRsaKey(context,
			session(),
			keyClass,
			encoding ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
			mAllocdRsaKey,
			label);
		if(label.Data) {
			mLabel.copy(label);
			mOaep = true;
			free(label.Data);
		}
	}
	else {
		assert(opStarted());	
	}

	unsigned cipherBlockSize = RSA_size(mRsaKey);
	unsigned plainBlockSize;

	/* padding - not present means value zero, CSSM_PADDING_NONE */
	uint32 padding = context.getInt(CSSM_ATTRIBUTE_PADDING);
	switch(padding) {
		case CSSM_PADDING_NONE:
			mPadding = RSA_NO_PADDING;
			plainBlockSize = cipherBlockSize;
			break;
		case CSSM_PADDING_PKCS1:
			mPadding = RSA_PKCS1_PADDING;
			plainBlockSize = cipherBlockSize - 11;
			break;
		case CSSM_PADDING_APPLE_SSLv2:
			rsaCryptDebug("RSA_CryptContext::init using CSSM_PADDING_APPLE_SSLv2");
			mPadding = RSA_SSLV23_PADDING;
			plainBlockSize = cipherBlockSize - 11;
			break;
		default:
			rsaCryptDebug("RSA_CryptContext::init bad padding (0x%x)",
				(unsigned)padding);
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
	}
	
	/* optional blinding attribute */
	uint32 blinding = context.getInt(CSSM_ATTRIBUTE_RSA_BLINDING);
	if(blinding) {
		if(RSA_blinding_on(mRsaKey, NULL) <= 0) {
			/* actually no legit failures */
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
		}
	}
	else {
		RSA_blinding_off(mRsaKey);
	}

	/* finally, have BlockCryptor set up its stuff. */
	setup(encoding ? plainBlockSize  : cipherBlockSize, // blockSizeIn
		  encoding ? cipherBlockSize : plainBlockSize,	// blockSizeOut
		  false,										// pkcs5Pad
		  false,										// needsFinal
		  BCM_ECB,
		  NULL);											// IV
	mInitFlag = true;

}
/* called by BlockCryptor */
void RSA_CryptContext::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)
{
	StLock<Mutex> _(gMutex());
	
	int irtn;
	
	/* FIXME do OAEP encoding here */
	if(mRsaKey->d == NULL) {
		irtn =	RSA_public_encrypt((int)plainTextLen,
			(unsigned char *)plainText,
			(unsigned char *)cipherText, 
			mRsaKey,
			mPadding);
	}
	else {
		irtn =	RSA_private_encrypt((int)plainTextLen,
			(unsigned char *)plainText,
			(unsigned char *)cipherText, 
			mRsaKey,
			mPadding);
	}
	if(irtn < 0) {
		throwRsaDsa("RSA_public_encrypt");
	}
	else if((unsigned)irtn > cipherTextLen) {
		rsaCryptDebug("RSA_public_encrypt overflow");
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	cipherTextLen = (size_t)irtn;
}

void RSA_CryptContext::decryptBlock(
	const void		*cipherText,		// length implied (one cipher block)
	size_t			cipherTextLen,
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)
{
	StLock<Mutex> _(gMutex());
	
	int irtn;
	
	rsaCryptDebug("decryptBlock padding %d", mPadding);
	/* FIXME do OAEP encoding here */
	if(mRsaKey->d == NULL) {
		irtn = RSA_public_decrypt((int)inBlockSize(),
			(unsigned char *)cipherText,
			(unsigned char *)plainText, 
			mRsaKey,
			mPadding);
	}
	else {
		irtn = RSA_private_decrypt((int)inBlockSize(),
			(unsigned char *)cipherText,
			(unsigned char *)plainText, 
			mRsaKey,
			mPadding);
	}
	if(irtn < 0) {
		rsaCryptDebug("decryptBlock err");
		throwRsaDsa("RSA_private_decrypt");
	}
	else if((unsigned)irtn > plainTextLen) {
		rsaCryptDebug("RSA_private_decrypt overflow");
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	plainTextLen = (size_t)irtn;
}

size_t RSA_CryptContext::outputSize(
	bool 			final,				// ignored
	size_t 			inSize /*= 0*/)		// output for given input size
{
	StLock<Mutex> _(gMutex());
	
	size_t rawBytes = inSize + inBufSize();
	size_t rawBlocks = (rawBytes + inBlockSize() - 1) / inBlockSize();
	rbprintf("--- RSA_CryptContext::outputSize inSize 0x%lux outSize 0x%lux mInBufSize 0x%lux",
		(unsigned long)inSize, (unsigned long)(rawBlocks * outBlockSize()), (unsigned long)inBufSize());
	return rawBlocks * outBlockSize();
}
