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
#include <opensslUtils/opensslAsn1.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <AppleCSPSession.h>
#include <AppleCSPUtils.h>
#include <assert.h>
#include <security_utilities/debugging.h>
#include <Security/oidsalg.h>
#include <YarrowConnection.h>

#define dhKeyDebug(args...)	secdebug("dhKey", ## args)

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
 
/* constructor with optional existing DSA key */
DHBinaryKey::DHBinaryKey(DH *dhKey)
	: mDhKey(dhKey)
{
}

DHBinaryKey::~DHBinaryKey()
{
	if(mDhKey) {
		DH_free(mDhKey);
		mDhKey = NULL;
	}
}

void DHBinaryKey::generateKeyBlob(
	Allocator 		&allocator,
	CssmData			&blob,
	CSSM_KEYBLOB_FORMAT	&format,
	AppleCSPSession		&session,
	const CssmKey		*paramKey,	/* optional, unused here */
	CSSM_KEYATTR_FLAGS 	&attrFlags)	/* IN/OUT */
{

	switch(mKeyHeader.KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		{
			switch(format) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					// take default
					format = DH_PUB_KEY_FORMAT;	
					break;
				case DH_PUB_KEY_FORMAT:
				case CSSM_KEYBLOB_RAW_FORMAT_X509:
					// proceed
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
					/* use PKCS3 - caller won't care if we change this...right? */
					format = DH_PUB_KEY_FORMAT;
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_FORMAT);
			}
			
			assert(mDhKey != NULL);
			CssmAutoData encodedKey(allocator);
			CSSM_RETURN crtn = DHPublicKeyEncode(mDhKey, format, 
				encodedKey);
			if(crtn) {
				CssmError::throwMe(crtn);
			}
			blob = encodedKey.release();
			break;
		}
		case CSSM_KEYCLASS_PRIVATE_KEY:
		{
			switch(format) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					// i.e., use default
					format = DH_PRIV_KEY_FORMAT;
					break;
				case DH_PRIV_KEY_FORMAT:
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
					// proceed 
					break;

				case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
				{
					/*
					 * Use public blob; calculate it if we
					 * don't already have it. 
					 */
					assert(mDhKey != NULL);
					if(mDhKey->pub_key == NULL) {
						int irtn = DH_generate_key(mDhKey);
						if(!irtn) {
							throwRsaDsa("DH_generate_key");
						}
					}
					assert(mDhKey->pub_key != NULL);
					setUpData(blob, 
						BN_num_bytes(mDhKey->pub_key), 
						*DH_Factory::privAllocator);
					BN_bn2bin(mDhKey->pub_key, blob);
					format = DH_PUB_KEY_FORMAT;
					return;
				}

				default:
					CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_FORMAT);
			}
			assert(mDhKey != NULL);
			CssmAutoData encodedKey(allocator);
			CSSM_RETURN crtn = DHPrivateKeyEncode(mDhKey, format, 
				encodedKey);
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

	NSS_DHParameterBlock algParamBlock;
	NSS_DHParameter &algParams = algParamBlock.params;
	uint32 privValueLen = 0;		// only nonzero from externally generated
									//   params
	SecNssCoder coder;				// for temp allocs of decoded parameters
	
	if(paramData != NULL) {
		/* this contains the DER encoding of a DHParameterBlock */
		CSSM_RETURN crtn;
		crtn = DHParamBlockDecode(*paramData, algParamBlock, coder);
		if(crtn) {
			CssmError::throwMe(crtn);
		}

		/* snag the optional private key length field */
		if(algParams.privateValueLength.Data) {
			privValueLen = cssmDataToInt(algParams.privateValueLength);
		}
		
		/* ensure caller's key size matches the incoming params */
		uint32 paramKeyBytes;
		if(privValueLen) {
			paramKeyBytes = (privValueLen + 7) / 8;
		}
		else {
			paramKeyBytes = algParams.prime.Length;
			/* trim off possible m.s. byte of zero */
			const unsigned char *uo = 
				(const unsigned char *)algParams.prime.Data;
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
		memset(&algParamBlock, 0, sizeof(algParamBlock));
		dhGenParams(keyBits, DH_GENERATOR_DEFAULT, 0, algParams, coder);
	}
					
	/* create key, stuff params into it */
	rPrivBinKey.mDhKey = DH_new();
	if(rPrivBinKey.mDhKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);		
	}
	DH *dhKey = rPrivBinKey.mDhKey;
	dhKey->p = cssmDataToBn(algParams.prime);
	dhKey->g = cssmDataToBn(algParams.base);
	dhKey->length = privValueLen;
	cspDhDebug("private DH binary key dhKey %p", dhKey);
	
	/* generate the key (both public and private capabilities) */
	int irtn = DH_generate_key(dhKey);
	if(!irtn) {
		throwRsaDsa("DH_generate_key");
	}
	
	/* public key is a subset */
	rPubBinKey.mDhKey = DH_new();
	if(rPubBinKey.mDhKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);		
	}
	DH *pubDhKey = rPubBinKey.mDhKey;
	pubDhKey->pub_key = BN_dup(dhKey->pub_key);
	/* these params used for X509 style key blobs */
	pubDhKey->p = BN_dup(dhKey->p);
	pubDhKey->g = BN_dup(dhKey->g);
	cspDhDebug("public DH binary key pubDhKey %p", pubDhKey);
}



/***
 *** Diffie-Hellman CSPKeyInfoProvider.
 ***/
DHKeyInfoProvider::DHKeyInfoProvider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session) :
		CSPKeyInfoProvider(cssmKey, session)
{
}

CSPKeyInfoProvider *DHKeyInfoProvider::provider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session)
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
	return new DHKeyInfoProvider(cssmKey, session);
}

/* Given a raw key, cook up a Binary key */
void DHKeyInfoProvider::CssmKeyToBinary(
	CssmKey				*paramKey,		// optional, ignored here
	CSSM_KEYATTR_FLAGS	&attrFlags,		// IN/OUT
	BinaryKey 			**binKey)
{
	*binKey = NULL;

	assert(mKey.blobType() == CSSM_KEYBLOB_RAW);
	switch(mKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		case CSSM_KEYCLASS_PRIVATE_KEY:
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}

	/* first cook up an DH key, then drop that into a BinaryKey */
	DH *dhKey = rawCssmKeyToDh(mKey);
	DHBinaryKey *dhBinKey = new DHBinaryKey(dhKey);
	*binKey = dhBinKey;
	cspDhDebug("CssmKeyToBinary dhKey %p", dhKey);
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
	DH *dhKey = rawCssmKeyToDh(mKey);
	
	/* DH_size requires the p parameter, which some public keys don't have */
	if(dhKey->p != NULL) {
		numBits = DH_size(dhKey) * 8;
	}
	else {
		assert(dhKey->pub_key != NULL);
		numBits = BN_num_bytes(dhKey->pub_key) * 8;
	}
	DH_free(dhKey);
	keySize.LogicalKeySizeInBits = numBits;
	keySize.EffectiveKeySizeInBits = numBits;
}

/* 
 * Obtain blob suitable for hashing in CSSM_APPLECSP_KEYDIGEST 
 * passthrough.
 */
bool DHKeyInfoProvider::getHashableBlob(
	Allocator 	&allocator,
	CssmData		&blob)			// blob to hash goes here
{
	/*
	 * The optimized case, a raw key in the "proper" format already.
	 */
	assert(mKey.blobType() == CSSM_KEYBLOB_RAW);
	bool useAsIs = false;
	
	switch(mKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			if(mKey.blobFormat() == CSSM_KEYBLOB_RAW_FORMAT_PKCS3) {
				useAsIs = true;
			}
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			break;
		default:
			/* shouldn't be here */
			assert(0);
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	if(useAsIs) {
		const CssmData &keyBlob = CssmData::overlay(mKey.KeyData);
		copyCssmData(keyBlob, blob, allocator);
		return true;
	}
	
	/* caller converts to binary and proceeds */
	return false;
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
	NSS_DHParameterBlock algParamBlock;
	SecNssCoder coder;
	NSS_DHParameter &algParams = algParamBlock.params;
	dhGenParams(bitSize, DH_GENERATOR_DEFAULT, 0, algParams, coder);
	
	/* drop in the required OID */
	algParamBlock.oid = CSSMOID_PKCS3;
	
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
	CssmAutoData aDerData(session());
	PRErrorCode perr;
	perr = SecNssEncodeItemOdata(&algParamBlock, kSecAsn1DHParameterBlockTemplate, 
		aDerData);
	if(perr) {
		/* only known error... */
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}

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
	NSS_DHParameter	&algParams,
	SecNssCoder		&coder)				// temp contents of algParams
										//    mallocd here
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
	
	/* stuff dhKey->{p,g,length}] into a caller's NSS_DHParameter */
	bnToCssmData(dhKey->p, algParams.prime, coder);
	bnToCssmData(dhKey->g, algParams.base, coder);
	CSSM_DATA &privValData = algParams.privateValueLength;
	if(privValueLength) {
		intToCssmData(privValueLength, privValData, coder);
	}
	else {
		privValData.Data = NULL;
		privValData.Length = 0;
	}
	DH_free(dhKey);
}

