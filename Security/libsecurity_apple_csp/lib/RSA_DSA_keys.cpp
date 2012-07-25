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
 * RSA_DSA_Keys.cpp - RSA, DSA related asymmetric key pair classes. 
 */

#include "RSA_DSA_keys.h"
#include <opensslUtils/opensslUtils.h>
#include <opensslUtils/opensslAsn1.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <AppleCSPSession.h>
#include <AppleCSPUtils.h>
#include <assert.h>
#include <security_utilities/debugging.h>
#include "RSA_DSA_utils.h"
#include <YarrowConnection.h>
#include <security_asn1/SecNssCoder.h>

#define RSA_PUB_EXPONENT	0x10001 	/* recommended by RSA */

#define rsaKeyDebug(args...)	secdebug("rsaKey", ## args)


/***
 *** RSA-style BinaryKey
 ***/
 
/* constructor with optional existing RSA key */
/* FIXME how to transmit OAEP params? */
RSABinaryKey::RSABinaryKey(RSA *rsaKey)
	: mRsaKey(rsaKey),
	  mOaep(false),
	  mLabel(Allocator::standard())
{
}

RSABinaryKey::~RSABinaryKey()
{
	if(mRsaKey) {
		RSA_free(mRsaKey);
		mRsaKey = NULL;
	}
}

void RSABinaryKey::setOaep(
	const CSSM_DATA		&label)
{
	mLabel.copy(label);
	mOaep = true;
}

void RSABinaryKey::generateKeyBlob(
	Allocator			&allocator,
	CssmData			&blob,
	CSSM_KEYBLOB_FORMAT	&format,	/* IN/OUT */
	AppleCSPSession		&session,
	const CssmKey		*paramKey,	/* optional, unused here */
	CSSM_KEYATTR_FLAGS	&attrFlags)	/* IN/OUT */
{
	bool			isPub;
	CSSM_RETURN		crtn;
	
	/* FIXME get label from context here for OAEP */
	
	/* 
	 * Here, the incoming default of CSSM_KEYBLOB_RAW_FORMAT_NONE
	 * is translated to our AppleCSP-custom defaults. App can override.
	 */
	switch(mKeyHeader.KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			isPub = true;
			switch(format) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					format = RSA_PUB_KEY_FORMAT;	// default
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
					if(mOaep) {
						/* have to take digest of the whole thing including label */
						format = CSSM_KEYBLOB_RAW_FORMAT_X509;
					}
					else {
						/* calculate digest on PKCS1 blob */
						format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;	
					}
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:
				case CSSM_KEYBLOB_RAW_FORMAT_X509:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_FORMAT);
			}
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			isPub = false;
			switch(format) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:	// default
					format = RSA_PRIV_KEY_FORMAT;	
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
					if(mOaep) {
						/* have to take digest of the whole thing including label */
						format = CSSM_KEYBLOB_RAW_FORMAT_X509;
					}
					else {
						/* calculate digest on PKCS1 blob */
						format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;	
					}
					isPub = true;
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_FORMAT);
			}
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}

	CssmAutoData encodedKey(allocator);
	if(mOaep) {
		CSSM_DATA label = mLabel;
		if(isPub) {
			crtn = RSAOAEPPublicKeyEncode(mRsaKey, &label, encodedKey);
		}
		else {
			crtn = RSAOAEPPrivateKeyEncode(mRsaKey, &label, encodedKey);
		}
	}
	else {
		if(isPub) {
			crtn = RSAPublicKeyEncode(mRsaKey, format, descData(), encodedKey);
		}
		else {
			crtn = RSAPrivateKeyEncode(mRsaKey, format, descData(), encodedKey);
		}
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	blob = encodedKey.release();
}
		
/***
 *** RSA-style AppleKeyPairGenContext
 ***/

/*
 * This one is specified in, and called from, CSPFullPluginSession. Our
 * only job is to prepare two subclass-specific BinaryKeys and call up to
 * AppleKeyPairGenContext.
 */
void RSAKeyPairGenContext::generate(
	const Context 	&context, 
	CssmKey 		&pubKey, 
	CssmKey 		&privKey)
{
	RSABinaryKey *pubBinKey  = new RSABinaryKey();
	RSABinaryKey *privBinKey = new RSABinaryKey();
	
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
	
// this one is specified in, and called from, AppleKeyPairGenContext
void RSAKeyPairGenContext::generate(
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
	RSABinaryKey &rPubBinKey = 
		dynamic_cast<RSABinaryKey &>(pubBinKey);
	RSABinaryKey &rPrivBinKey = 
		dynamic_cast<RSABinaryKey &>(privBinKey);

	/*
	 * One parameter from context: Key size in bits is required.
	 * FIXME - get public exponent from context?
	 */
	keyBits = context.getInt(CSSM_ATTRIBUTE_KEY_LENGTH,
				CSSMERR_CSP_MISSING_ATTR_KEY_LENGTH);
	if(keyBits > rsaMaxKeySize()) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH);
	}
	
	/* generate the private key */
	rPrivBinKey.mRsaKey = RSA_generate_key(keyBits,
		RSA_PUB_EXPONENT,
		NULL,			// no callback
		NULL);
	if(rPrivBinKey.mRsaKey == NULL) {
		rsaKeyDebug("RSA_generate_key returned NULL");
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);		// ???
	}
		
	/* public key is subset of private key */
	rPubBinKey.mRsaKey = RSA_new();
	if(rPrivBinKey.mRsaKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);	
	}
	RSA *pub = rPubBinKey.mRsaKey;
	RSA *priv = rPrivBinKey.mRsaKey;
	pub->n = BN_dup(priv->n);
	pub->e = BN_dup(priv->e);
	if((pub->n == NULL) || (pub->e == NULL)) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);		
	}
}


/***
 *** RSA-style CSPKeyInfoProvider.
 ***/
RSAKeyInfoProvider::RSAKeyInfoProvider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session) :
		CSPKeyInfoProvider(cssmKey, session)
{
}

CSPKeyInfoProvider *RSAKeyInfoProvider::provider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session)
{
	switch(cssmKey.algorithm()) {
		case CSSM_ALGID_RSA:
		case CSSM_ALGMODE_PKCS1_EME_OAEP:
			break;
		default:
			return NULL;
	}
	switch(cssmKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		case CSSM_KEYCLASS_PRIVATE_KEY:
			break;
		default:
			return NULL;
	}
	/* OK, we'll handle this one */
	return new RSAKeyInfoProvider(cssmKey, session);
}

/* Given a raw key, cook up a Binary key */
void RSAKeyInfoProvider::CssmKeyToBinary(
	CssmKey				*paramKey,		// ignored
	CSSM_KEYATTR_FLAGS	&attrFlags,		// IN/OUT, unused here
	BinaryKey 			**binKey)
{
	*binKey = NULL;
	RSA *rsaKey = NULL;
	CSSM_DATA label = {0, NULL};
	
	/* first cook up an RSA key */
	rsaKey = rawCssmKeyToRsa(mKey, label);
	
	/* now drop that into a BinaryKey */
	RSABinaryKey *rsaBinKey = new RSABinaryKey(rsaKey);
	*binKey = rsaBinKey;
	if(label.Data) {
		rsaBinKey->setOaep(label);
		free(label.Data);
	}
}
		
/* 
 * Obtain key size in bits.
 */
void RSAKeyInfoProvider::QueryKeySizeInBits(
	CSSM_KEY_SIZE &keySize)
{
	RSA *rsaKey = NULL;
	CSSM_DATA label = {0, NULL};
	
	if(mKey.blobType() != CSSM_KEYBLOB_RAW) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
	}
	rsaKey = rawCssmKeyToRsa(mKey, label);
	keySize.LogicalKeySizeInBits = RSA_size(rsaKey) * 8;
	keySize.EffectiveKeySizeInBits = keySize.LogicalKeySizeInBits;
	RSA_free(rsaKey);
	if(label.Data) {
		free(label.Data);
	}	
}

/* 
 * Obtain blob suitable for hashing in CSSM_APPLECSP_KEYDIGEST 
 * passthrough.
 */
bool RSAKeyInfoProvider::getHashableBlob(
	Allocator 	&allocator,
	CssmData		&blob)			// blob to hash goes here
{
	/*
	 * The optimized case, a raw key in the "proper" format already.
	 * Only public keys in PKCS1 format fit this bill. 
	 */
	assert(mKey.blobType() == CSSM_KEYBLOB_RAW);
	bool useAsIs = false;
	
	switch(mKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			if(mKey.blobFormat() == CSSM_KEYBLOB_RAW_FORMAT_PKCS1) {
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

/***
 *** DSA key support
 ***/
 

/***
 *** DSA-style BinaryKey
 ***/
 
/* constructor with optional existing DSA key */
DSABinaryKey::DSABinaryKey(DSA *dsaKey)
	: mDsaKey(dsaKey)
{
}

DSABinaryKey::~DSABinaryKey()
{
	if(mDsaKey) {
		DSA_free(mDsaKey);
		mDsaKey = NULL;
	}
}

void DSABinaryKey::generateKeyBlob(
	Allocator 		&allocator,
	CssmData			&blob,
	CSSM_KEYBLOB_FORMAT	&format,
	AppleCSPSession		&session,
	const CssmKey		*paramKey,	/* optional */
	CSSM_KEYATTR_FLAGS	&attrFlags)	/* IN/OUT */
{
	bool			isPub;
	CSSM_RETURN		crtn;
	
	/* 
	 * Here, the incoming default of CSSM_KEYBLOB_RAW_FORMAT_NONE
	 * is translated to our AppleCSP-custom defaults. App can override.
	 */
	switch(mKeyHeader.KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			isPub = true;
			switch(format) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					format = DSA_PUB_KEY_FORMAT;	// default
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:
				case CSSM_KEYBLOB_RAW_FORMAT_X509:
				case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_FORMAT);
			}
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			isPub = false;
			switch(format) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					format = DSA_PRIV_KEY_FORMAT;	// default
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
					/*
					 * This is calculated on the public key, which 
					 * is not always part of a DSA private key's encoding...
					 * so first calculate the public key. 
					 */
					dsaKeyPrivToPub(mDsaKey);
					isPub = true;
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:
					break;
				default:
					CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_FORMAT);
			}
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}

	/* possible conversion from partial binary key to fully 
	 * formed blob */
	DSA *dsaToEncode = mDsaKey;
	DSA *dsaUpgrade = NULL;
	if(isPub &&
	   (mDsaKey->p == NULL) &&
	   (paramKey != NULL)) {
		/*
		 * Don't modify BinaryKey; make a copy.
		 */
		dsaUpgrade = DSA_new();
		if(dsaUpgrade == NULL) {
			CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);	
		}
		dsaUpgrade->pub_key = BN_dup(mDsaKey->pub_key);
		crtn = dsaGetParamsFromKey(dsaUpgrade, *paramKey, session);
		if(crtn) {
			DSA_free(dsaUpgrade);
			CssmError::throwMe(crtn);
		}
		
		/* success - switch keys and inform caller of attr change */
		dsaToEncode = dsaUpgrade;
		attrFlags &= ~CSSM_KEYATTR_PARTIAL;
	}

	/*
	 * DSA private keys originating from BSAFE form - e.g., DSA private
	 * keys wrapped in a keychain (which have format FIPS186 by default)
	 * have no public key component. Generate the public key if we don't 
	 * have one.
	 */
	if(!isPub && (dsaToEncode->pub_key == NULL)) {
		dsaKeyPrivToPub(dsaToEncode);
	}
	
	CssmAutoData 	encodedKey(allocator);
	if(isPub) {
		crtn = DSAPublicKeyEncode(dsaToEncode, format, descData(), encodedKey);
	}
	else {
		crtn = DSAPrivateKeyEncode(dsaToEncode, format, descData(), encodedKey);
	}
	if(dsaUpgrade != NULL) {
		/* temp key, get rid of it */
		DSA_free(dsaUpgrade);
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	blob = encodedKey.release();
}
	
/***
 *** DSA-style AppleKeyPairGenContext
 ***/

/*
 * This one is specified in, and called from, CSPFullPluginSession. Our
 * only job is to prepare two subclass-specific BinaryKeys and call up to
 * AppleKeyPairGenContext.
 */
void DSAKeyPairGenContext::generate(
	const Context 	&context, 
	CssmKey 		&pubKey, 
	CssmKey 		&privKey)
{
	DSABinaryKey *pubBinKey  = new DSABinaryKey();
	DSABinaryKey *privBinKey = new DSABinaryKey();
	
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
void DSAKeyPairGenContext::generate(
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
	DSABinaryKey &rPubBinKey = 
		dynamic_cast<DSABinaryKey &>(pubBinKey);
	DSABinaryKey &rPrivBinKey = 
		dynamic_cast<DSABinaryKey &>(privBinKey);

	/*
	 * Parameters from context: 
	 *   Key size in bits, required;
	 *   {p,q,g} from generateParams, optional
	 */
	keyBits = context.getInt(CSSM_ATTRIBUTE_KEY_LENGTH,
				CSSMERR_CSP_MISSING_ATTR_KEY_LENGTH);
	if(keyBits > DSA_MAX_KEY_SIZE) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH);
	}
	CssmData *paramData = context.get<CssmData>(CSSM_ATTRIBUTE_ALG_PARAMS);

	NSS_DSAAlgParams algParams;
	SecNssCoder coder;			// generated algParams mallocd from here
	if(paramData != NULL) {
		/* this contains the DER encoding of a NSS_DSAAlgParams */
		CSSM_RETURN crtn = DSADecodeAlgParams(algParams, paramData->Data,
			paramData->Length, coder);
		if(crtn) {
			CssmError::throwMe(crtn);
		}
	}
	else {
		/* no alg params specified; generate them now using null (random) seed */
		dsaGenParams(keyBits, NULL, 0, algParams, coder);
	}
					
	/* create key, stuff params into it */
	rPrivBinKey.mDsaKey = DSA_new();
	if(rPrivBinKey.mDsaKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);		
	}
	DSA *dsaKey = rPrivBinKey.mDsaKey;
	dsaKey->p = cssmDataToBn(algParams.p);
	dsaKey->q = cssmDataToBn(algParams.q);
	dsaKey->g = cssmDataToBn(algParams.g);
	
	/* generate the key (both public and private capabilities) */
	int irtn = DSA_generate_key(dsaKey);
	if(!irtn) {
		throwRsaDsa("DSA_generate_key");
	}
	
	/* public key is subset of private key */
	rPubBinKey.mDsaKey = DSA_new();
	if(rPrivBinKey.mDsaKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);	
	}
	DSA *pub     = rPubBinKey.mDsaKey;
	DSA *priv    = rPrivBinKey.mDsaKey;
	pub->p       = BN_dup(priv->p);
	pub->q       = BN_dup(priv->q);
	pub->g       = BN_dup(priv->g);
	pub->pub_key = BN_dup(priv->pub_key);
	if((pub->p == NULL) || (pub->q == NULL) || (pub->g == NULL) ||
			(pub->pub_key == NULL)) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);		
	}
}

/*
 * Generate keygen parameters, stash them in a context attr array for later use
 * when actually generating the keys.
 */
void DSAKeyPairGenContext::generate(
	const Context &context, 
	uint32 bitSize,
    CssmData &params,
    uint32 &attrCount, 
	Context::Attr * &attrs)
{
	void *seed = NULL;
	unsigned seedLen = 0;

	/* optional seed from context */
	CssmData *seedData = context.get<CssmData>(CSSM_ATTRIBUTE_SEED);
	if(seedData) {
		seed = seedData->data();
		seedLen = seedData->length();
	}

	/* generate the params, temp alloc from SecNssCoder  */
	NSS_DSAAlgParams algParams;
	SecNssCoder coder;
	dsaGenParams(bitSize, seed, seedLen, algParams, coder);
	
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
	DSAEncodeAlgParams(algParams, aDerData);

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
void DSAKeyPairGenContext::freeGenAttrs()
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
 * Generate DSA algorithm parameters from optional seed input, returning result
 * into NSS_DSAAlgParamss.[pqg]. This is called from both GenerateParameters and from
 * KeyPairGenerate (if no GenerateParameters has yet been called). 
 */
void DSAKeyPairGenContext::dsaGenParams(
	uint32				keySizeInBits,
	const void			*inSeed,		// optional
	unsigned			inSeedLen,
	NSS_DSAAlgParams 	&algParams,
	SecNssCoder			&coder)			// contents of algParams mallocd from here
{
	unsigned char seedBuf[SHA1_DIGEST_SIZE];
	void *seedPtr;
	
	/* validate key size */
	if((keySizeInBits < DSA_MIN_KEY_SIZE) || 
	   (keySizeInBits > DSA_MAX_KEY_SIZE) ||
	   (keySizeInBits & DSA_KEY_BITS_MASK)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH);
	}
	
	/* seed from one of three sources */
	if(inSeed == NULL) {
		/* 20 random seed bytes */
		session().getRandomBytes(SHA1_DIGEST_SIZE, seedBuf);
		seedPtr = seedBuf;
	}
	else if(inSeedLen == SHA1_DIGEST_SIZE) {
		/* perfect */
		seedPtr = (void *)inSeed;
	}
	else {
		/* hash caller's seed */
		cspGenSha1Hash(inSeed, inSeedLen, seedBuf);
		seedPtr = seedBuf;
	}

	DSA *dsaKey = DSA_generate_parameters(keySizeInBits,
		(unsigned char *)seedPtr,	
		SHA1_DIGEST_SIZE,
		NULL,		// counter_ret
		NULL,		// h_ret
		NULL, 
		NULL);
	if(dsaKey == NULL) {
		throwRsaDsa("DSA_generate_parameters");
	}
	
	/* stuff dsaKey->[pqg] into a caller's NSS_DSAAlgParams */
	bnToCssmData(dsaKey->p, algParams.p, coder);
	bnToCssmData(dsaKey->q, algParams.q, coder);
	bnToCssmData(dsaKey->g, algParams.g, coder);
	
	DSA_free(dsaKey);
}

/***
 *** DSA-style CSPKeyInfoProvider.
 ***/
DSAKeyInfoProvider::DSAKeyInfoProvider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session) :
		CSPKeyInfoProvider(cssmKey, session)
{

}
CSPKeyInfoProvider *DSAKeyInfoProvider::provider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session)
{
	switch(cssmKey.algorithm()) {
		case CSSM_ALGID_DSA:
			break;
		default:
			return NULL;
	}
	switch(cssmKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		case CSSM_KEYCLASS_PRIVATE_KEY:
			break;
		default:
			return NULL;
	}
	/* OK, we'll handle this one */
	return new DSAKeyInfoProvider(cssmKey, session);
}

/* Given a raw key, cook up a Binary key */
void DSAKeyInfoProvider::CssmKeyToBinary(
	CssmKey				*paramKey,		// optional
	CSSM_KEYATTR_FLAGS	&attrFlags,		// IN/OUT
	BinaryKey 			**binKey)
{
	*binKey = NULL;
	DSA *dsaKey = NULL;
	
	/* first cook up an DSA key, then drop that into a BinaryKey */
	dsaKey = rawCssmKeyToDsa(mKey, mSession, paramKey);
	if(dsaKey->p == NULL) {
		attrFlags |= CSSM_KEYATTR_PARTIAL;
	}
	else {
		attrFlags &= ~CSSM_KEYATTR_PARTIAL;
	}
	DSABinaryKey *dsaBinKey = new DSABinaryKey(dsaKey);
	*binKey = dsaBinKey;
}
		
/* 
 * Obtain key size in bits.
 */
void DSAKeyInfoProvider::QueryKeySizeInBits(
	CSSM_KEY_SIZE &keySize)
{
	DSA *dsaKey = NULL;
	
	if(mKey.blobType() != CSSM_KEYBLOB_RAW) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
	}
	dsaKey = rawCssmKeyToDsa(mKey,
		mSession,
		NULL);		// no param key allowed here
	if(dsaKey->p != NULL) {
		/* normal fully-formed key */
		keySize.LogicalKeySizeInBits = BN_num_bits(dsaKey->p);
		keySize.EffectiveKeySizeInBits = keySize.LogicalKeySizeInBits;
		DSA_free(dsaKey);
	}
	else {
		/* partial key, get an approximation from pub_key */
		keySize.LogicalKeySizeInBits = BN_num_bits(dsaKey->pub_key);
		DSA_free(dsaKey);
		/* and indicate this anomaly like so */
		CssmError::throwMe(CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE);
	}
}

/* 
 * Obtain blob suitable for hashing in CSSM_APPLECSP_KEYDIGEST 
 * passthrough.
 */
bool DSAKeyInfoProvider::getHashableBlob(
	Allocator 	&allocator,
	CssmData		&blob)			// blob to hash goes here
{
	/* No optimized case for DSA keys */
	return false;
}
