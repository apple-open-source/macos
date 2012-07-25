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

#ifdef	BSAFE_CSP_ENABLE


//
// bsafeSymmetric.cpp - symmetric encryption contexts and algorithms
//
#include "bsafecspi.h"
#include <security_utilities/debugging.h>

#define bbprintf(args...)	secdebug("BSafeBuf", ## args)

#define VERBOSE_DEBUG	0
#if		VERBOSE_DEBUG
static void dumpBuf(
	char				*title,
	const CSSM_DATA		*d,
	uint32 				maxLen)
{
	unsigned i;
	uint32 len;
	
	if(title) {
		printf("%s:  ", title);
	}
	if(d == NULL) {
		printf("NO DATA\n");
		return;
	}
	printf("Total Length: %d\n   ", d->Length);
	len = maxLen;
	if(d->Length < len) {
		len = d->Length;
	}
	for(i=0; i<len; i++) {
		printf("%02X ", d->Data[i]);
		if((i % 16) == 15) {
			printf("\n   ");
		}
	}
	printf("\n");
}
#else
#define dumpBuf(t, d, m)
#endif	/* VERBOSE_DEBUG */

void BSafe::SymmetricKeyGenContext::generate(
	const Context 	&context, 
	CssmKey 		&symKey, 
	CssmKey 		&dummyKey)
{
	AppleSymmKeyGenContext::generateSymKey(
		context, 
		session(),
		symKey);		
}

// FIXME:
// We really should match the key algorithm to the en/decrypt
// algorithm. Also: verify key usage bits. 
void BSafe::BlockCipherContext::init(
	const Context &context, 
	bool encrypting)
{
	bool hasIV = false;
	bool requirePad = false;
	
    if (reusing(encrypting))
        return;	// all set to go
		
	cssmAlg = context.algorithm();
    switch(cssmAlg) {
		// most are handled below; break here to special cases
		case CSSM_ALGID_RC4:
			RC4init(context);
			return;
		case CSSM_ALGID_DES:
		case CSSM_ALGID_DESX:
		case CSSM_ALGID_3DES_3KEY_EDE:
		case CSSM_ALGID_RC5:
		case CSSM_ALGID_RC2:
			break;
		
		/* others here... */
        default:
			// Should never have gotten this far
			assert(0);
            CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);		
	}
	
	
	// these variables are used in the switch below and need to 
	// live until after setAlgorithm()
	BSafeItem 		iv;
    B_BLK_CIPHER_W_FEEDBACK_PARAMS spec;
	A_RC5_PARAMS	rc5Params;
	A_RC2_PARAMS	rc2Params;

    // crypto algorithm
    spec.encryptionParams = NULL_PTR;	// default, may change
    switch (cssmAlg) {
        case CSSM_ALGID_DES:
            spec.encryptionMethodName = POINTER("des");
            break;
        case CSSM_ALGID_DESX:
            spec.encryptionMethodName = POINTER("desx");
            break;
        case CSSM_ALGID_3DES_3KEY_EDE:
            spec.encryptionMethodName = POINTER("des_ede");
            break;
        case CSSM_ALGID_RC5:
            spec.encryptionMethodName = POINTER("rc5");
			spec.encryptionParams = POINTER(&rc5Params);
			rc5Params.version = 0x10;
			// FIXME - get this from context attr
			rc5Params.rounds = 1;
			rc5Params.wordSizeInBits = 32;
            break;
        case CSSM_ALGID_RC2:
		{
            spec.encryptionMethodName = POINTER("rc2");
			spec.encryptionParams = POINTER(&rc2Params);
			// effective key size in bits - either from Context,
			// or the key
			uint32 bits = context.getInt(CSSM_ATTRIBUTE_EFFECTIVE_BITS);
			if(bits == 0) {
				// OK, try the key
				CssmKey &key = context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, 
					CSSMERR_CSP_MISSING_ATTR_KEY);
				bits = key.KeyHeader.LogicalKeySizeInBits;
			}
			rc2Params.effectiveKeyBits = bits;
            break;
		}
    }

    // feedback mode
	cssmMode = context.getInt(CSSM_ATTRIBUTE_MODE);
    switch (cssmMode) {
		/* no mode attr --> 0 == CSSM_ALGMODE_NONE, not currently supported */
 		case CSSM_ALGMODE_CBCPadIV8:
			requirePad = true;
			// and fall thru
		case CSSM_ALGMODE_CBC_IV8: 
		{
            iv = context.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR, 
				CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
            spec.feedbackMethodName = POINTER("cbc");
            spec.feedbackParams = POINTER(&iv);
			hasIV = true;
            break;
        }
        case CSSM_ALGMODE_OFB_IV8: {
            iv = context.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR, 
				CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
            spec.feedbackMethodName = POINTER("ofb");
            spec.feedbackParams = POINTER(&iv);
			hasIV = true;
            break;
        }
        case CSSM_ALGMODE_ECB: {
            spec.feedbackMethodName = POINTER("ecb");
            spec.feedbackParams = POINTER(&blockSize);
            break;
        }
        default:
			errorLog1("BSafe symmetric init: illegal mode (%d)\n", (int)cssmMode);
            CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_MODE);
    }

    // padding
    spec.paddingParams = NULL_PTR;
	/* no padding attr --> 0 == PADDING_NONE */
	padEnable = false;
    uint32 cssmPadding = context.getInt(CSSM_ATTRIBUTE_PADDING);
	if(requirePad) {
		switch(cssmPadding) {
			case CSSM_PADDING_PKCS1:	// for backwards compatibility
			case CSSM_PADDING_PKCS5:
			case CSSM_PADDING_PKCS7:
				spec.paddingMethodName = POINTER("pad");
				padEnable = true;
				break;
			default:
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
		}
	}
	else {
		if(cssmPadding != CSSM_PADDING_NONE) {
            CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
		}
		else {
            spec.paddingMethodName = POINTER("nopad");
		}
	}

    // put it all together
    setAlgorithm(AI_FeedbackCipher, &spec);	// set BSafe algorithm
    setKeyFromContext(context);				// set BSafe key
    cipherInit();							// common cryption init
}

void BSafe::BlockCipherContext::RC4init(
	const Context &context)
{
    setAlgorithm(AI_RC4, NULL);		// set BSafe algorithm
    setKeyFromContext(context);		// set BSafe key
	padEnable = false;
    cipherInit();					// common cryption init
}

void BSafe::BlockCipherContext::trackUpdate(size_t inSize, size_t outSize)
{
	size_t newPending = pending + inSize;
	pending = newPending % blockSize;
	
	/*
	 * Most of the time, the max size buffered by BSAFE is 
	 * blockSize - 1 bytes. When decrypting and padding is enabled,
	 * BSAFE buffers up to a full block.
	 */
	if(!mDirection && 					//Êdecrypting
	   padEnable &&						// padding
	   (pending == 0) &&				// mod result was 0
	   (newPending > 0)) {				// but nonzero total
		/* BSAFE is holding a whole block in its buffer */
		pending = blockSize;
	}
	bbprintf("===trackUpdte: %s; inSize=%d newPending=%d pending=%d", 
		(mDirection ? "encrypt" : "decrypt"),
		inSize, newPending, pending);
}

size_t BSafe::BlockCipherContext::inputSize(size_t outSize)
{
    // if we have an 'outSize' output buffer, how many input bytes may we feed in?
    size_t wholeBlocks = outSize / blockSize;
    return wholeBlocks * blockSize - pending + (blockSize - 1);
}

size_t BSafe::BlockCipherContext::outputSize(bool final, size_t inSize)
{
    // how much output buffer will we need for 'size' input bytes?
	
	size_t totalToGo = inSize + pending;
	// total to go, rounded up to next block
	size_t numBlocks = (totalToGo + blockSize - 1) / blockSize;
	size_t outSize;
	
	/*
	 * encrypting: may get one additional block on final() if padding
	 * decrypting: outsize always <= insize 
	 */
	if(mDirection && 						// encrypting
		final &&							// last time
		padEnable &&  						// padding enabled
		((totalToGo % blockSize) == 0)) {	// even ptext len
			numBlocks++;					// extra pad block
	}
	outSize = numBlocks * blockSize;
	bbprintf("===outputSize: %s; final=%d inSize=%d pending=%d outSize=%d", 
		(mDirection ? "encrypt" : "decrypt"),
		final, inSize, pending, outSize);
	return outSize;
}

void BSafe::BlockCipherContext::minimumProgress(size_t &inSize, size_t &outSize)
{
    // eat up buffer, proceed one full block
    inSize = blockSize - pending;
    outSize = blockSize;
}
#endif	/* BSAFE_CSP_ENABLE */
