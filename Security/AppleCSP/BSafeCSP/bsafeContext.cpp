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
// bsafeContext.cpp - implementation of class BSafe::BSafeContext
//					  and some of its subclasses
// 				

#include "bsafecspi.h"
#include "bsafePKCS1.h"
#include <bkey.h>
#include <balg.h>
#include <algobj.h>
#include "cspdebugging.h"

#define DATA(cData)		POINTER(cData.data()), cData.length()

A_SURRENDER_CTX * const BSafe::BSafeContext::bsSurrender = NULL;


//
// Construct an algorithm object
//
BSafe::BSafeContext::BSafeContext(AppleCSPSession &session)
	: AppleCSPContext(session)
{
    bsAlgorithm = NULL;
    bsKey = NULL;
	bsBinKey = NULL;
    bsRandom = NULL;
    initialized = false;
	opStarted = false;
#ifdef SAFER
    inUpdate = NULL;
    inOutUpdate = NULL;
    inFinal = NULL;
    outFinal = NULL;
    outFinalR = NULL;
#endif //SAFER
}

BSafe::BSafeContext::~BSafeContext()
{
    reset();
}

void BSafe::BSafeContext::reset()
{
    B_DestroyAlgorithmObject(&bsAlgorithm);
    B_DestroyAlgorithmObject(&bsRandom);
	destroyBsKey();
}

/* 
 * Clear key state. We only destroy bsKey if we don't have a 
 * BinaryKey.
 */
void BSafe::BSafeContext::destroyBsKey()
{
	if(bsBinKey == NULL) {
		B_DestroyKeyObject(&bsKey);
	}
	else {
		// bsKey gets destroyed when bsBinKey gets deleted
		bsBinKey = NULL;
		bsKey = NULL;
	}
}

void BSafe::check(int status, bool isKeyOp)
{
	if(status == 0) {
		return;
	}
	dprintf1("BSAFE Error %d\n", status);
    switch (status) {
		case BE_ALLOC:
			throw std::bad_alloc();
		case BE_SIGNATURE:
			CssmError::throwMe(CSSMERR_CSP_VERIFY_FAILED);
		case BE_OUTPUT_LEN:
			CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
		case BE_INPUT_LEN:
			CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
		case BE_EXPONENT_EVEN:
		case BE_EXPONENT_LEN:
		case BE_EXPONENT_ONE:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
		case BE_DATA:
		case BE_INPUT_DATA:
			if(isKeyOp) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			else {
				CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
			}
		case BE_MODULUS_LEN:
		case BE_OVER_32K:
		case BE_INPUT_COUNT:
		case BE_CANCEL:
			//@@@ later...
        default:
			//@@@ translate BSafe errors intelligently
            CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
    }
}


void BSafe::BSafeContext::setAlgorithm(
	B_INFO_TYPE bAlgType, 
	const void *info)
{
    B_DestroyAlgorithmObject(&bsAlgorithm);	// clear any old BSafe algorithm
    check(B_CreateAlgorithmObject(&bsAlgorithm));
    check(B_SetAlgorithmInfo(bsAlgorithm, bAlgType, POINTER(info)));
}

/* safely create bsKey */
void BSafe::BSafeContext::createBsKey()
{
	/* reset to initial key state - some keys can't be reused */
    destroyBsKey();
    check(B_CreateKeyObject(&bsKey));
}

/* form of *info varies per bKeyInfo */
void BSafe::BSafeContext::setKeyAtom(
	B_INFO_TYPE bKeyInfo, 
	const void *info)
{
	/* debug only */
	if((bKeyInfo == KI_RSAPublicBER) || (bKeyInfo == KI_RSAPublic)) {
			printf("Aargh! Unhandled KI_RSAPublic!\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	assert(bKeyInfo != KI_RSAPublicBER);		// handled elsewhere for now
	assert(bKeyInfo != KI_RSAPublic);			// handled elsewhere for now
	createBsKey();
    check(B_SetKeyInfo(bsKey, bKeyInfo, POINTER(info)), true);
}

//
// Set outSize for RSA keys.
//
void BSafe::BSafeContext::setRsaOutSize(
	bool isPubKey)
{
	assert(bsKey != NULL);

	A_RSA_KEY *keyInfo;
	if(isPubKey) {
		keyInfo = getKey<A_RSA_KEY>(bsKey, KI_RSAPublic);
	}
	else {
		keyInfo = getKey<A_RSA_KEY>(bsKey, KI_RSAPrivate);
	}
	mOutSize = (B_IntegerBits(keyInfo->modulus.data, 
		keyInfo->modulus.len) + 7) / 8;
}

// 
// Handle various forms of reference key. Symmetric
// keys are stored as SymmetricBinaryKey, with raw key bytes
// in keyData. Our asymmetric keys are stored as BSafeBinaryKeys,
// with an embedded ready-to-use B_KEY_OBJ. 
// 
void BSafe::BSafeContext::setRefKey(CssmKey &key)
{
	bool isPubKey = false;
	
	switch(key.keyClass()) {
		case CSSM_KEYCLASS_SESSION_KEY:
		{
			assert(key.blobFormat() == 
				CSSM_KEYBLOB_REF_FORMAT_INTEGER);
			
			BinaryKey &binKey = session().lookupRefKey(key);
			// fails if this is not a SymmetricBinaryKey
			SymmetricBinaryKey *symBinKey =
				dynamic_cast<SymmetricBinaryKey *>(&binKey);
			if(symBinKey == NULL) {
				errorLog0("BSafe::setRefKey(1): wrong BinaryKey subclass\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			setKeyFromCssmData(KI_Item, symBinKey->mKeyData);
			return;
		}
		case CSSM_KEYCLASS_PUBLIC_KEY:
			isPubKey = true;		// and fall thru
		case CSSM_KEYCLASS_PRIVATE_KEY:
		{
			BinaryKey &binKey = session().lookupRefKey(key);
			destroyBsKey();
			bsBinKey = dynamic_cast<BSafeBinaryKey *>(&binKey);
			/* this cast failing means that this is some other
			 * kind of binary key */
			if(bsBinKey == NULL) {
				errorLog0("BSafe::setRefKey(2): wrong BinaryKey subclass\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			assert(bsBinKey->bsKey() != NULL);
			bsKey = bsBinKey->bsKey();
			if(key.algorithm() == CSSM_ALGID_RSA) {
				setRsaOutSize(isPubKey);
			}
			return;
		}
		default:
		    CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
}

void BSafe::BSafeContext::setKeyFromContext(
	const Context &context, 
	bool required)
{
    CssmKey &key = 
		context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, CSSMERR_CSP_MISSING_ATTR_KEY);
	
	switch(key.blobType()) {
		case CSSM_KEYBLOB_REFERENCE:
			setRefKey(key);
			return;
		case CSSM_KEYBLOB_RAW:
			break;		// to main routine
		default:
			CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
	}
	
	bool isPubKey;
	switch (key.keyClass()) {
		case CSSM_KEYCLASS_SESSION_KEY:
			/* symmetric, one format supported for all algs */
			switch (key.blobFormat()) {
				case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
					setKeyFromCssmKey(KI_Item, key);
					return;
				default:
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
			}
		case CSSM_KEYCLASS_PUBLIC_KEY:
			isPubKey = true;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			isPubKey = false;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	
	/* We know it's an asymmetric key; get some info */
	B_INFO_TYPE infoType;
	CSSM_KEYBLOB_FORMAT expectedFormat;
	
	if(!bsafeAlgToInfoType(key.algorithm(),
		isPubKey,
		infoType, 
		expectedFormat)) {
		/* unknown alg! */
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	
	/* 
	 * Correct format? 
	 * NOTE: if we end up supporting multiple incoming key formats, they'll
	 * have to be handled here.
	 */
	if(expectedFormat != key.blobFormat()) {
		errorLog1("setKeyFromContext: invalid blob format (%d)\n", 
			(int)key.blobFormat());
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
	}
	
	/*
	 * Most formats can be handled directly by BSAFE. Handle the special cases 
	 * requiring additional processing here. 
	 */
	switch(expectedFormat)  {
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:
			/* RSA public keys */
			createBsKey();
			BS_setKeyPkcs1(CssmData::overlay(key.KeyData), bsKey);
			break;
		default:
			setKeyFromCssmKey(infoType, key);
			break;
	}
	
	/* 
	 * One more thing - set mOutSize for RSA keys
	 */
	if(key.algorithm() == CSSM_ALGID_RSA) {
		setRsaOutSize(isPubKey);	
	}	
}

#define BSAFE_RANDSIZE	32

void BSafe::BSafeContext::setRandom()
{
    if (bsRandom == NULL) {
        check(B_CreateAlgorithmObject(&bsRandom));
        check(B_SetAlgorithmInfo(bsRandom, AI_X962Random_V0, NULL_PTR));
        check(B_RandomInit(bsRandom, chooser(), bsSurrender));
        uint8 seed[BSAFE_RANDSIZE];
		session().getRandomBytes(BSAFE_RANDSIZE, seed);
        check(B_RandomUpdate(bsRandom, seed, sizeof(seed), bsSurrender));
    }
}


//
// Operational methods of BSafeContext
//
void BSafe::BSafeContext::init(const Context &, bool)
{
    // some algorithms don't need init(), because all is done in the context constructor
}

// update for input-only block/stream algorithms
void BSafe::BSafeContext::update(const CssmData &data)
{
	opStarted = true;
    check(inUpdate(bsAlgorithm, POINTER(data.data()), data.length(), bsSurrender));
}

// update for input/output block/stream algorithms
void BSafe::BSafeContext::update(void *inp, size_t &inSize, void *outp, size_t &outSize)
{
    unsigned int length;
	opStarted = true;
    check(inOutUpdate(bsAlgorithm, POINTER(outp), &length, outSize,
                               POINTER(inp), inSize, bsRandom, bsSurrender));
    // always eat all input (inSize unchanged)
    outSize = length;

    // let the algorithm manager track I/O sizes, if needed
    trackUpdate(inSize, outSize);
}

// output-generating final call
void BSafe::BSafeContext::final(CssmData &out)
{
    unsigned int length;
    if (outFinal) {
        check(outFinal(bsAlgorithm, 
			POINTER(out.data()), 
			&length, 
			out.length(), 
			bsSurrender));
	}
    else {
        check(outFinalR(bsAlgorithm, 
			POINTER(out.data()), 
			&length, 
			out.length(),
			bsRandom, 
			bsSurrender));
	}
    out.length(length);
	initialized = false;
}

// verifying final call (takes additional input)
void BSafe::BSafeContext::final(const CssmData &in)
{
 	int status;
	
	/* note sig verify errors can show up as lots of BSAFE statuses;
	 * munge them all into the appropriate error */
   if (inFinal) {
        status = inFinal(bsAlgorithm, 
			POINTER(in.data()), 
			in.length(), 
			bsSurrender);
	}
    else {
        status = inFinalR(bsAlgorithm, 
			POINTER(in.data()), 
			in.length(), 
			bsRandom, 
			bsSurrender);
	}
	if(status != 0) {
		if((mType == CSSM_ALGCLASS_SIGNATURE) && (mDirection == false)) {
			/* yep, sig verify error */
			CssmError::throwMe(CSSMERR_CSP_VERIFY_FAILED);
		}
		/* other error, use standard trap */
		check(status);
	}
	initialized = false;
}

size_t BSafe::BSafeContext::outputSize(bool final, size_t inSize)
{
    // this default implementation only makes sense for single-output end-loaded algorithms
    return final ? mOutSize : 0;
}

void BSafe::BSafeContext::trackUpdate(size_t, size_t)
{ /* do nothing */ }

//
// Common features of CipherContexts.
//
void BSafe::CipherContext::cipherInit()
{
    // set handlers
    if (encoding) {
        inOutUpdate = B_EncryptUpdate;
        outFinalR = B_EncryptFinal;
    } else {
        inOutUpdate = B_DecryptUpdate;
        outFinalR = B_DecryptFinal;
    }
    outFinal = NULL;

    // init the algorithm
    check((encoding ? B_EncryptInit : B_DecryptInit)
          (bsAlgorithm, bsKey, chooser(), bsSurrender));

    // buffers start empty
    pending = 0;

    // state is now valid
    initialized = true;
	opStarted = false;
}
#endif	/* BSAFE_CSP_ENABLE */

