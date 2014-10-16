/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// bsafeKeyGen.cpp - key generation routines
//
#include "bsafecspi.h"
#include "bsafePKCS1.h"
#include "cspdebugging.h"

/*
 * Stateless, private function to map a CSSM alg and pub/priv state
 * to B_INFO_TYPE and format. Returns true on success, false on
 * "I don't understand this algorithm". 
 */
bool BSafe::bsafeAlgToInfoType(
	CSSM_ALGORITHMS		alg,
	bool				isPublic,
	B_INFO_TYPE			&infoType,	// RETURNED
	CSSM_KEYBLOB_FORMAT	&format)	// RETURNED
{
 	switch(alg) {
		case CSSM_ALGID_RSA:
			if(isPublic) {
				infoType = RSA_PUB_KEYINFO_TYPE;
				format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
			}
			else {
				infoType = RSA_PRIV_KEYINFO_TYPE;
				format = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
			}
			return true;
		case CSSM_ALGID_DSA:
			format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
			if(isPublic) {
				infoType = DSA_PUB_KEYINFO_TYPE;
			}
			else {
				infoType = DSA_PRIV_KEYINFO_TYPE;
			}
			return true;
		default:
			return false;
	}
}


BSafe::BSafeBinaryKey::BSafeBinaryKey(
	bool isPub,
	uint32 Alg)
	: mIsPublic(isPub),
	  mAlg(Alg)
{
	BSafe::check(B_CreateKeyObject(&mBsKey), true);
}

BSafe::BSafeBinaryKey::~BSafeBinaryKey()
{
	B_DestroyKeyObject(&mBsKey);
}

void BSafe::BSafeBinaryKey::generateKeyBlob(
		Allocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format,		// input val ignored for now
		AppleCSPSession		&session,
		const CssmKey		*paramKey,		// optional, unused here
		CSSM_KEYATTR_FLAGS 	&attrFlags)		// IN/OUT 
{
 	assert(mBsKey != NULL);

	B_INFO_TYPE 	bsType;
	if(!bsafeAlgToInfoType(mAlg, mIsPublic, bsType, format)) {
		CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	if(format == CSSM_KEYBLOB_RAW_FORMAT_PKCS1) {
		/* special case, encode the PKCS1 format blob */
		CssmRemoteData rData(
			Allocator::standard(Allocator::sensitive), blob);
		BS_GetKeyPkcs1(mBsKey, rData); 
		rData.release();
	}
	else {
		BSafeItem *info;
		BSafe::check(
			B_GetKeyInfo((POINTER *)&info, mBsKey, bsType), true);
		blob = info->copy<CssmData>(allocator);
	}
}

//
// This is called from CSPFullPluginSession
//
void BSafe::BSafeKeyPairGenContext::generate(
	const Context 	&context, 
	CssmKey 		&pubKey, 
	CssmKey 		&privKey)
{
	BSafeBinaryKey	*pubBinKey  = new BSafeBinaryKey(true, 
		context.algorithm());
	BSafeBinaryKey	*privBinKey = new BSafeBinaryKey(false, 
		context.algorithm());
	
	try {
		AppleKeyPairGenContext::generate(context, 
			session(),
			pubKey, 
			pubBinKey, 
			privKey, 
			privBinKey);
	}
	catch (...) {
		delete pubBinKey;
		delete privBinKey;
		throw;
	}
}

//
// Called from AppleKeyPairGenContext
//
void BSafe::BSafeKeyPairGenContext::generate(
		const Context 	&context,
		BinaryKey		&pubBinKey,		// valid on successful return
		BinaryKey		&privBinKey,	// ditto
		uint32			&keySize)		// ditto
{
	/* these casts throw exceptions if the keys are of the 
	 * wrong classes, which is a major bogon, since we created
	 * the keys in the above generate() function */
	BSafeBinaryKey &bsPubBinKey = 
		dynamic_cast<BSafeBinaryKey &>(pubBinKey);
	BSafeBinaryKey &bsPrivBinKey = 
		dynamic_cast<BSafeBinaryKey &>(privBinKey);
	
    if (!initialized) {
		setupAlgorithm(context, keySize);
        check(B_GenerateInit(bsAlgorithm, chooser(), bsSurrender), true);
        initialized = true;
    }

	setRandom();
    check(B_GenerateKeypair(bsAlgorithm, 
		bsPubBinKey.bsKey(), 
		bsPrivBinKey.bsKey(), 
		bsRandom, 
		bsSurrender), true);
}

void BSafe::BSafeKeyPairGenContext::setupAlgorithm(
	const Context 	&context,
	uint32			&keySize)
{
	switch(context.algorithm()) {
		case CSSM_ALGID_RSA:
		{
			A_RSA_KEY_GEN_PARAMS genParams;
			keySize = genParams.modulusBits = 
				context.getInt(CSSM_ATTRIBUTE_KEY_LENGTH,
							CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH);
			if (CssmData *params = 
				context.get<CssmData>(CSSM_ATTRIBUTE_ALG_PARAMS)) {
				genParams.publicExponent = BSafeItem(*params);
			} else {
				static unsigned char exponent[] = { 1, 0, 1 };
				genParams.publicExponent = BSafeItem(exponent, sizeof(exponent));
			}
			/*
			 * For test purposes, we avoid the 'strong' key generate
			 * algorithm if a CSSM_ALGMODE_CUSTOM mode atrtribute
			 * is present in the context. This is not published and
			 * not supported in the real world. 
			 */
			uint32 mode = context.getInt(CSSM_ATTRIBUTE_MODE);
			if(mode == CSSM_ALGMODE_CUSTOM) {
				setAlgorithm(AI_RSAKeyGen, &genParams);
			}
			else {
				setAlgorithm(AI_RSAStrongKeyGen, &genParams);
			}
		}
			break;
		case CSSM_ALGID_DSA:
		{
			A_DSA_PARAMS genParams;
			genParams.prime = 
				BSafeItem(context.get<CssmData>(
					CSSM_ATTRIBUTE_PRIME,
					CSSMERR_CSP_MISSING_ATTR_ALG_PARAMS));
			genParams.subPrime = 
				BSafeItem(context.get<CssmData>(
					CSSM_ATTRIBUTE_SUBPRIME,
					CSSMERR_CSP_MISSING_ATTR_ALG_PARAMS));
			genParams.base = 
				BSafeItem(context.get<CssmData>(
					CSSM_ATTRIBUTE_BASE,
					CSSMERR_CSP_MISSING_ATTR_ALG_PARAMS));
			setAlgorithm(AI_DSAKeyGen, &genParams);
			keySize = B_IntegerBits(genParams.prime.data, genParams.prime.len);
		}
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
}

//
// DSA Parameter Generation
//
void BSafe::BSafeKeyPairGenContext::generate(
	const Context &context, 
	uint32 bitSize,
    CssmData &params,
    uint32 &attrCount, 
	Context::Attr * &attrs)
{
	assert(context.algorithm() == CSSM_ALGID_DSA);
	
    B_ALGORITHM_OBJ genAlg = NULL;
    B_ALGORITHM_OBJ result = NULL;

    try {
        check(B_CreateAlgorithmObject(&genAlg));

        B_DSA_PARAM_GEN_PARAMS genParams;
        genParams.primeBits = bitSize;
        check(B_SetAlgorithmInfo(genAlg, AI_DSAParamGen, POINTER(&genParams)));
        setRandom();
        check(B_GenerateInit(genAlg, chooser(), bsSurrender), true);
        check(B_CreateAlgorithmObject(&result));
        check(B_GenerateParameters(genAlg, result, bsRandom, bsSurrender));

        // get parameters out of algorithm object
        A_DSA_PARAMS *kParams = NULL;
        check(B_GetAlgorithmInfo((POINTER *)&kParams, result, AI_DSAKeyGen), true);

        // shred them into context attribute form
        attrs = normAllocator->alloc<Context::Attr>(3);
        attrs[0] = Context::Attr(CSSM_ATTRIBUTE_PRIME,
                   *BSafeItem(kParams->prime).copyp<CssmData>(*normAllocator));
        attrs[1] = Context::Attr(CSSM_ATTRIBUTE_SUBPRIME,
                   *BSafeItem(kParams->subPrime).copyp<CssmData>(*normAllocator));
        attrs[2] = Context::Attr(CSSM_ATTRIBUTE_BASE,
                   *BSafeItem(kParams->base).copyp<CssmData>(*normAllocator));
        attrCount = 3;

        // clean up
        B_DestroyAlgorithmObject(&result);
        B_DestroyAlgorithmObject(&genAlg);
    } catch (...) {
        // clean up
        B_DestroyAlgorithmObject(&result);
        B_DestroyAlgorithmObject(&genAlg);
        throw;
    }
}

/*
 * CSPKeyInfoProvider for asymmetric BSAFE keys. 
 */
BSafe::BSafeKeyInfoProvider::BSafeKeyInfoProvider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session) :
		CSPKeyInfoProvider(cssmKey, session)
{
}

CSPKeyInfoProvider *BSafe::BSafeKeyInfoProvider::provider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session)
{
	switch(cssmKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		case CSSM_KEYCLASS_PRIVATE_KEY:
			break;
		default:
			return NULL;
	}
	switch(mKey.algorithm()) {
		case CSSM_ALGID_RSA:
		case CSSM_ALGID_DSA:
			break;
		default:
			return NULL;
	}
	/* OK, we'll handle this one */
	return new BSafeKeyInfoProvider(cssmKey, session);
}

/* cook up a Binary key */
void BSafe::BSafeKeyInfoProvider::CssmKeyToBinary(
	CssmKey				*paramKey,		// optional, ignored
	CSSM_KEYATTR_FLAGS	&attrFlags,		// IN/OUT
	BinaryKey 			**binKey)
{
	*binKey = NULL;
	
	const CSSM_KEYHEADER *hdr = &mKey.KeyHeader;
	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	
	B_INFO_TYPE 		bsType;
	CSSM_KEYBLOB_FORMAT	format;
	bool 				isPub;
	
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			isPub = true;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			isPub = false;
			break;
		default:
			// someone else's key
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	if(!bsafeAlgToInfoType(hdr->AlgorithmId, isPub, bsType, format)) {
		// someone else's key
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	if(hdr->Format != format) {	
		dprintf0("BSafe::cssmKeyToBinary: format mismatch\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
	}
		
	BSafeBinaryKey *bsBinKey = new BSafeBinaryKey(isPub,
		hdr->AlgorithmId);
		
	// set up key material as appropriate
	if(format == CSSM_KEYBLOB_RAW_FORMAT_PKCS1) {
		/* special case, decode the PKCS1 format blob */
		BS_setKeyPkcs1(mKey, bsBinKey->bsKey());
	}
	else {
		/* normal case, use key blob as is */
		BSafeItem item(mKey.KeyData);
		BSafe::check(
			B_SetKeyInfo(bsBinKey->bsKey(), bsType, POINTER(&item)), true);
	}
	*binKey = bsBinKey;
}
		
/* 
 * Obtain key size in bits.
 */
void BSafe::BSafeKeyInfoProvider::QueryKeySizeInBits(
	CSSM_KEY_SIZE &keySize)
{
	if(mKey.blobType() != CSSM_KEYBLOB_RAW) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
	}
	
	/* cook up BSAFE key */
	B_KEY_OBJ bKey;
	A_RSA_KEY *rsaKeyInfo = NULL;
	A_DSA_PUBLIC_KEY *dsaPubKeyInfo = NULL;
	A_DSA_PRIVATE_KEY *dsaPrivKeyInfo = NULL;
	ITEM *sizeItem = NULL;
	BSafe::check(B_CreateKeyObject(&bKey), true);
	B_INFO_TYPE infoType;
	
	switch(mKey.algorithm()) {
		case CSSM_ALGID_RSA:
			switch(mKey.keyClass()) {
				case CSSM_KEYCLASS_PUBLIC_KEY:
					if(mKey.blobFormat() != 
							CSSM_KEYBLOB_RAW_FORMAT_PKCS1) {
						CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
					}
					
					/* convert from PKCS1 blob to raw key */
					BS_setKeyPkcs1(mKey, bKey);
					infoType = KI_RSAPublic;
					/* break to common RSA code */
					break;
				case CSSM_KEYCLASS_PRIVATE_KEY:
				{
					if(mKey.blobFormat() != 
							CSSM_KEYBLOB_RAW_FORMAT_PKCS8) {
						CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
					}
					
					/* convert from PKCS8 blob to raw key */
					BSafeItem item(mKey.KeyData);
					BSafe::check(
						B_SetKeyInfo(bKey, KI_PKCS_RSAPrivateBER,
							POINTER(&item)), true);
					infoType = KI_RSAPrivate;
					break;
				}
				default:
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}
			rsaKeyInfo = getKey<A_RSA_KEY>(bKey, infoType);
			sizeItem = &rsaKeyInfo->modulus;
			break;
			
		case CSSM_ALGID_DSA:
			/* untested as of 9/11/00 */
			if(mKey.blobFormat() != 
					CSSM_KEYBLOB_RAW_FORMAT_FIPS186) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
			}
			switch(mKey.keyClass()) {
				case CSSM_KEYCLASS_PUBLIC_KEY:
				{
					BSafeItem item(mKey.KeyData);
					BSafe::check(B_SetKeyInfo(bKey, 
						DSA_PUB_KEYINFO_TYPE, 
						(POINTER)&item), true);
					
					/* get the key bits */
					dsaPubKeyInfo = getKey<A_DSA_PUBLIC_KEY>(bKey, 
						KI_DSAPublic);
					sizeItem = &dsaPubKeyInfo->params.prime;
					break;
				}
				case CSSM_KEYCLASS_PRIVATE_KEY:
				{
					BSafeItem item(mKey.KeyData);
					BSafe::check(B_SetKeyInfo(bKey, 
						DSA_PRIV_KEYINFO_TYPE, 
						(POINTER)&item), true);
					
					/* get the key bits */
					dsaPrivKeyInfo = getKey<A_DSA_PRIVATE_KEY>(bKey, 
						KI_DSAPrivate);
					sizeItem = &dsaPrivKeyInfo->params.prime;
					break;
				}
				default:
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	uint32 iSize = B_IntegerBits(sizeItem->data, sizeItem->len);
	keySize.LogicalKeySizeInBits = iSize;
	keySize.EffectiveKeySizeInBits = iSize;
	B_DestroyKeyObject(&bKey);
}

#endif	/* BSAFE_CSP_ENABLE */

