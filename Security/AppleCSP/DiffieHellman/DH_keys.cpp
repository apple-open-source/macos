/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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
 * DH_keys.cpp - Diffie-Hellman key pair support. 
 */

#include "DH_keys.h"
#include "DH_utils.h"
#include <opensslUtils/opensslUtils.h>
#include <opensslUtils/openRsaSnacc.h>
#include <Security/cssmdata.h>
#include <AppleCSP/AppleCSPSession.h>
#include <AppleCSP/AppleCSPUtils.h>
#include <assert.h>
#include <Security/debugging.h>
#include <AppleCSP/YarrowConnection.h>
#include <Security/appleoids.h>
#include <Security/cdsaUtils.h>
#include <Security/asn-octs.h>
#include <Security/sm_vdatypes.h>

#define dhKeyDebug(args...)	debug("dhKey", ## args)

/*
 * FIXME - the CDSA Algorithm Guide claims that the incoming params argument
 * for a GenerateAlgorithmParameters call is ignored for D-H. This means 
 * that there is no way for the caller to  specify 'g' (typically 2, 3, or 
 * 5). This seems WAY bogus but we'll code to the spec for now, assuming 
 * a hard-coded default generator. 
 */
#define DH_GENERATOR_DEFAULT	DH_GENERATOR_2


/***
 *** Diffie-Hellman-style BinaryKey
 ***/
 
/* constructor with optional existing RSA key */
DHBinaryKey::DHBinaryKey(DH *dhKey)
	: mDhKey(dhKey)
{
	mPubKey.Data = NULL;
	mPubKey.Length = 0;
}

DHBinaryKey::DHBinaryKey(const CSSM_DATA *pubBlob)
	: mDhKey(NULL)
{
	setPubBlob(pubBlob);
}

DHBinaryKey::~DHBinaryKey()
{
	if(mDhKey) {
		assert(mPubKey.Data == NULL);
		DH_free(mDhKey);
		mDhKey = NULL;
	}
	if(mPubKey.Data) {
		assert(mDhKey == NULL);
		DH_Factory::privAllocator->free(mPubKey.Data);
		mPubKey.Data = NULL;
		mPubKey.Length = 0;
	}
}

void DHBinaryKey::generateKeyBlob(
	CssmAllocator 		&allocator,
	CssmData			&blob,
	CSSM_KEYBLOB_FORMAT	&format)
{
	switch(mKeyHeader.KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		{
			/* trivial case, just copy the public blob */
			assert(mDhKey == NULL);
			assert(mPubKey.Data != NULL);
			format = DH_PUB_KEY_FORMAT;
			copyCssmData(CssmData::overlay(mPubKey), blob, allocator);
			break;
		}
		case CSSM_KEYCLASS_PRIVATE_KEY:
		{
			assert(mDhKey != NULL);
			assert(mPubKey.Data == NULL);
			format = DH_PRIV_KEY_FORMAT;
			CssmAutoData encodedKey(allocator);
			CSSM_RETURN crtn = DHPrivateKeyEncode(mDhKey, encodedKey);
			if(crtn) {
				CssmError::throwMe(crtn);
			}
			blob = encodedKey.release();
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
}

/* for importing.... */	
void DHBinaryKey::setPubBlob(const CSSM_DATA *pubBlob)
{
	assert(mDhKey == NULL);
	assert(mPubKey.Data == NULL);
	setUpData(mPubKey, pubBlob->Length, *DH_Factory::privAllocator);
	memmove(mPubKey.Data, pubBlob->Data, pubBlob->Length);
}

/* for creating from a full DH private key... */
void DHBinaryKey::setPubBlob(DH *privKey)
{
	assert(mDhKey == NULL);
	assert(mPubKey.Data == NULL);
	setUpData(mPubKey, BN_num_bytes(privKey->pub_key), 
		*DH_Factory::privAllocator);
	BN_bn2bin(privKey->pub_key, mPubKey.Data);
}

/***
 *** Diffie-Hellman style AppleKeyPairGenContext
 ***/

/*
 * This one is specified in, and called from, CSPFullPluginSession. Our
 * only job is to prepare two subclass-specific BinaryKeys and call up to
 * AppleKeyPairGenContext.
 */
void DHKeyPairGenContext::generate(
	const Context 	&context, 
	CssmKey 		&pubKey, 
	CssmKey 		&privKey)
{
	DHBinaryKey *pubBinKey  = new DHBinaryKey();
	DHBinaryKey *privBinKey = new DHBinaryKey();
	
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

/* 	
 * obtain a 32-bit integer from a BigIntegerStr.
 */
static uint32 bigIntStrToInt(
	const BigIntegerStr &bint,
	CSSM_RETURN toThrow)				// throws this if out of range
{
	size_t bytes = bint.Len();
	if(bytes > 4) {
		dhKeyDebug("DH integer overflow");
		if(toThrow) {
			CssmError::throwMe(toThrow);
		}
		else {
			return 0;
		}
	}
	uint32 rtn = 0;
	const unsigned char *uo = (const unsigned char *)bint.Octs();
	for(size_t i=0; i<bytes; i++) {
		rtn <<= 8;
		rtn |= uo[i];
	}
	return rtn;
}
/*
 * This one is specified in, and called from, AppleKeyPairGenContext
 */
void DHKeyPairGenContext::generate(
	const Context 	&context,
	BinaryKey		&pubBinKey,	
	BinaryKey		&privBinKey,
	uint32			&keyBits)
{
	/* 
	 * These casts throw exceptions if the keys are of the 
	 * wrong classes, which would be a major bogon, since we created
	 * the keys in the above generate() function.
	 */
	DHBinaryKey &rPubBinKey = 
		dynamic_cast<DHBinaryKey &>(pubBinKey);
	DHBinaryKey &rPrivBinKey = 
		dynamic_cast<DHBinaryKey &>(privBinKey);

	/*
	 * Parameters from context: 
	 *   Key size in bits, required;
	 *   {p,g,privKeyLength} from generateParams, optional
	 * NOTE: currently the openssl D-H imnplementation ignores the 
	 * privKeyLength field. 
	 */
	keyBits = context.getInt(CSSM_ATTRIBUTE_KEY_LENGTH,
				CSSMERR_CSP_MISSING_ATTR_KEY_LENGTH);
	CssmData *paramData = context.get<CssmData>(CSSM_ATTRIBUTE_ALG_PARAMS);

	DHParameterBlock algParamBlock;
	DHParameter *algParams = NULL;
	uint32 privValueLen = 0;		// only nonzero from externally generated
									//   params
	
	if(paramData != NULL) {
		/* this contains the DER encoding of a DHParameterBlock */
		try {
			SC_decodeAsnObj(*paramData, algParamBlock);
		}
		catch(...) {
			/*
			 * CDSA Extension: the CDSA Algorithm Guide says that the D-H
			 * parameter block is supposed to be wrapped with its accompanying
			 * OID. However Openssl does not do this; it just exports 
			 * an encoded DHParameter rather than a DHParameterBlock.
			 * For compatibility we'll try decoding the parameters as one
			 * of these. 
			 */
			if(algParamBlock.params) {
				delete algParamBlock.params;
				algParamBlock.params = NULL;
			}
			algParamBlock.params = new DHParameter;
			try {
				SC_decodeAsnObj(*paramData, *algParamBlock.params);
				dhKeyDebug("Trying openssl-style DH param decoding");
			}
			catch(...) {
				dhKeyDebug("openssl-style DH param decoding FAILED");
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
			}
		}
		
		algParams = algParamBlock.params;
		if(algParams == NULL) {
			dhKeyDebug("Bad DH param decoding");
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
		}

		/* snag the optional private key length field */
		if(algParams->privateValueLength) {
			privValueLen = bigIntStrToInt(*algParams->privateValueLength,
				CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
		}
		
		/* ensure caller's key size matches the incoming params */
		uint32 paramKeyBytes;
		if(privValueLen) {
			paramKeyBytes = (privValueLen + 7) / 8;
		}
		else {
			paramKeyBytes = algParams->prime.Len();
			/* trim off possible m.s. byte of zero */
			const unsigned char *uo = 
				(const unsigned char *)algParams->prime.Octs();
			if(*uo == 0) {
				paramKeyBytes--;
			}
		}
		uint32 reqBytes = (keyBits + 7) / 8;
		if(paramKeyBytes != reqBytes) {
			dhKeyDebug("DH key size mismatch (req %d  param %d)",
				(int)reqBytes, (int)paramKeyBytes);
			CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_SIZE);
		}
	}
	else {
		/* no alg params specified; generate them now */
		dhKeyDebug("DH implicit alg param calculation");
		algParamBlock.params = new DHParameter;
		algParams = algParamBlock.params;
		dhGenParams(keyBits, DH_GENERATOR_DEFAULT, 0, *algParams);
	}
					
	/* create key, stuff params into it */
	rPrivBinKey.mDhKey = DH_new();
	if(rPrivBinKey.mDhKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);		
	}
	DH *dhKey = rPrivBinKey.mDhKey;
	dhKey->p = bigIntStrToBn(algParams->prime);
	dhKey->g = bigIntStrToBn(algParams->base);
	dhKey->length = privValueLen;
	
	/* generate the key (both public and private capabilities) */
	int irtn = DH_generate_key(dhKey);
	if(!irtn) {
		throwRsaDsa("DH_generate_key");
	}
	
	/* public key just a blob */
	rPubBinKey.setPubBlob(dhKey);
}



/***
 *** Diffie-Hellman CSPKeyInfoProvider.
 ***/
DHKeyInfoProvider::DHKeyInfoProvider(
	const CssmKey &cssmKey) :
		CSPKeyInfoProvider(cssmKey)
{
	switch(cssmKey.algorithm()) {
		case CSSM_ALGID_DH:
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	switch(cssmKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		case CSSM_KEYCLASS_PRIVATE_KEY:
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	/* OK, we'll handle this one */
	return;
}

/* Given a raw key, cook up a Binary key */
void DHKeyInfoProvider::CssmKeyToBinary(
	BinaryKey **binKey)
{
	*binKey = NULL;

	assert(mKey.blobType() == CSSM_KEYBLOB_RAW);
	switch(mKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		{
			/* trivial case - no DH * */
			DHBinaryKey *dhKey = new DHBinaryKey(&mKey.KeyData);
			*binKey = dhKey;
			break;
		}
		case CSSM_KEYCLASS_PRIVATE_KEY:
		{
			/* first cook up an DH key, then drop that into a BinaryKey */
			DH *dhKey = rawCssmKeyToDh(mKey);
			DHBinaryKey *dhBinKey = new DHBinaryKey(dhKey);
			*binKey = dhBinKey;
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
}
		
/* 
 * Obtain key size in bits.
 * FIXME - I doubt that this is, or can be, exactly accurate.....
 */
void DHKeyInfoProvider::QueryKeySizeInBits(
	CSSM_KEY_SIZE &keySize)
{
	uint32 numBits = 0;
	
	if(mKey.blobType() != CSSM_KEYBLOB_RAW) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
	}
	switch(mKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			/* trivial case */
			numBits = mKey.KeyData.Length * 8;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
		{
			DH *dhKey = rawCssmKeyToDh(mKey);
			numBits = DH_size(dhKey) * 8;
			DH_free(dhKey);
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	keySize.LogicalKeySizeInBits = numBits;
	keySize.EffectiveKeySizeInBits = numBits;
}


/*
 * Generate keygen parameters, stash them in a context attr array for later use
 * when actually generating the keys.
 */
 
void DHKeyPairGenContext::generate(
	const Context &context, 
	uint32 bitSize,
    CssmData &params,		// RETURNED here,
    uint32 &attrCount, 		// here, 
	Context::Attr * &attrs)	// and here
{
	/* generate the params */
	DHParameterBlock algParamBlock;
	algParamBlock.params = new DHParameter;
	DHParameter *algParams = algParamBlock.params;
	dhGenParams(bitSize, DH_GENERATOR_DEFAULT, 0, *algParams);
	
	/* drop in the required OID */
	algParamBlock.oid.Set(pkcs_3_arc);
	
	/*
	 * Here comes the fun part. 
	 * We "return" the DER encoding of these generated params in two ways:
	 * 1. Copy out to app via the params argument, mallocing if Data ptr is NULL.
	 *    The app must free this. 
	 * 2. Cook up a 1-element Context::attr array containing one ALG_PARAM attr,
	 *    a CSSM_DATA_PTR containing the DER encoding. We have to save a ptr to
	 *    this attr array and free it, the CSSM_DATA it points to, and the DER
	 *    encoding *that* points to, in our destructor. 
	 *
	 * First, DER encode.
	 */
	size_t maxSize = sizeofBigInt(algParams->prime) + 
					 sizeofBigInt(algParams->base) 
					 + 30;		// includes oid, tag, length
	if(algParams->privateValueLength) {
		maxSize += sizeofBigInt(*algParams->privateValueLength);
	}
	CssmAutoData aDerData(session());
	SC_encodeAsnObj(algParamBlock, aDerData, maxSize);

	/* copy/release that into a mallocd CSSM_DATA. */
	CSSM_DATA_PTR derData = (CSSM_DATA_PTR)session().malloc(sizeof(CSSM_DATA));
	*derData = aDerData.release();
	
	/* stuff that into a one-element Attr array which we keep after returning */
	freeGenAttrs();
	mGenAttrs = (Context::Attr *)session().malloc(sizeof(Context::Attr));
	mGenAttrs->AttributeType   = CSSM_ATTRIBUTE_ALG_PARAMS;
	mGenAttrs->AttributeLength = sizeof(CSSM_DATA);
	mGenAttrs->Attribute.Data  = derData;

	/* and "return" this stuff */
	copyCssmData(CssmData::overlay(*derData), params, session());
	attrCount = 1;
	attrs = mGenAttrs;
}

/* free mGenAttrs and its referents if present */
void DHKeyPairGenContext::freeGenAttrs()
{
	if(mGenAttrs == NULL) {
		return;
	}
	if(mGenAttrs->Attribute.Data) {
		if(mGenAttrs->Attribute.Data->Data) {
			session().free(mGenAttrs->Attribute.Data->Data);
		}
		session().free(mGenAttrs->Attribute.Data);
	}
	session().free(mGenAttrs);
}

/*
 * Generate DSA algorithm parameters returning result
 * into DHParameter.{prime,base,privateValueLength]. 
 * This is called from both GenerateParameters and from
 * KeyPairGenerate (if no GenerateParameters has yet been called). 
 *
 * FIXME - privateValueLength not implemented in openssl, not here 
 * either for now. 
 */
 
void DHKeyPairGenContext::dhGenParams(
	uint32			keySizeInBits,
	unsigned		g,					// probably should be BIGNUM
	int				privValueLength, 	// optional
	DHParameter 	&algParams)
{
	/* validate key size */
	if((keySizeInBits < DH_MIN_KEY_SIZE) || 
	   (keySizeInBits > DH_MAX_KEY_SIZE)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH);
	}

	/* create an openssl-style DH key with minimal setup */
	DH *dhKey = DH_generate_parameters(keySizeInBits, g, NULL, NULL);
	if(dhKey == NULL) {
		throwRsaDsa("DSA_generate_parameters");
	}
	
	/* stuff dhKey->{p,g,length}] into a caller's DSAAlgParams */
	bnToBigIntStr(dhKey->p, algParams.prime);
	bnToBigIntStr(dhKey->g, algParams.base);
	if(privValueLength) {
		algParams.privateValueLength = new BigIntegerStr();
		snaccIntToBigIntegerStr(g, *algParams.privateValueLength);
	}
	DH_free(dhKey);
}

