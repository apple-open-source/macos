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
 * FEEAsymmetricContext.cpp - CSPContexts for FEE asymmetric encryption
 *
 */

#ifdef	CRYPTKIT_CSP_ENABLE

#include "FEEAsymmetricContext.h"
#include "FEECSPUtils.h"
#include <security_cryptkit/falloc.h>
#include <CommonCrypto/CommonDigest.h>

/* validate context for FEED and FEEDExp - no unexpected attributes allowed */
static void validateFeedContext(
	const Context &context)
{
	/* Note we cannot distinguish between zero and "not there" */
	uint32 blockSize = context.getInt(CSSM_ATTRIBUTE_BLOCK_SIZE);
	if(blockSize != 0) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_BLOCK_SIZE);
	}
	CSSM_ENCRYPT_MODE cssmMode = context.getInt(CSSM_ATTRIBUTE_MODE);
	if(cssmMode != 0) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_MODE);
	}
	#if 0
	/* we allow this for CMS wrapping */
	CssmData *iv = context.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR);
	if(iv != NULL) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_INIT_VECTOR);
	}
	#endif
	CSSM_PADDING padding = context.getInt(CSSM_ATTRIBUTE_PADDING); 
	if(padding != 0) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
	}
}

/***
 *** FEED - 1:1 FEED - encrypt n bytes of plaintext, get (roughly) n bytes
 *** of ciphertext. Ciphertext is smaller than with FEED, but this is slower.
 ***/
CryptKit::FEEDContext::~FEEDContext()
{
	if(mFeeFeed) {
		feeFEEDFree(mFeeFeed);
		mFeeFeed = NULL;
	}
	if(mPrivKey && mAllocdPrivKey) {
		feePubKeyFree(mPrivKey);
	}
	if(mPubKey && mAllocdPubKey) {
		feePubKeyFree(mPubKey);
	}
	mPrivKey = NULL;
	mPubKey  = NULL;
	mInitFlag = false;
}

// called by CSPFullPluginSession; reusable
void CryptKit::FEEDContext::init(
	const Context &context, 
	bool encoding)
{
	if(mInitFlag && !opStarted()) {
		/* reusing - e.g. query followed by encrypt */
		return;
	}
	
	/* 
	 * Fetch FEE keys from context. This is an unusual algorithm - it requires
	 * two keys, one public and one private. The public key MUST be stored in
	 * the context with attribute type CSSM_ATTRIBUTE_PUBLIC_KEY, and the private 
	 * key with CSSM_ATTRIBUTE_KEY.
	 *
	 * For now, we require CSSM_KEYUSE_ANY for FEE keys used for this algorithm.
	 * Otherwise we'd have to allow both KEYUSE_ENCRYPT and KEYUSE_DECRYPT for
	 * both keys, and that would require some algorithm-specific hack in 
	 * cspValidateKeyUsageBits() which I really don't want to do.
	 */
	if(mPrivKey == NULL) {
		assert(!opStarted());
		mPrivKey = contextToFeeKey(context,
			session(),
			CSSM_ATTRIBUTE_KEY,
			CSSM_KEYCLASS_PRIVATE_KEY,
			CSSM_KEYUSE_ANY,
			mAllocdPrivKey);
	}
	else {
		assert(opStarted());	
	}
	if(mPubKey == NULL) {
		assert(!opStarted());
		mPubKey = contextToFeeKey(context,
			session(),
			CSSM_ATTRIBUTE_PUBLIC_KEY,
			CSSM_KEYCLASS_PUBLIC_KEY,
			CSSM_KEYUSE_ANY,
			mAllocdPubKey);
	}
	else {
		assert(opStarted());	
	}
	
	/* validate context - no other attributes allowed */
	validateFeedContext(context);

	if(mFeeFeed != NULL) {
		/* not reusable */
		assert(opStarted());
		feeFEEDFree(mFeeFeed);
		mFeeFeed = NULL;
	}
	
	/* OK, looks good. Cook up a feeFEED object. */
	mFeeFeed = feeFEEDNewWithPubKey(mPrivKey,
		mPubKey,
		encoding ? 1 : 0,
		feeRandCallback,
		&session());
	if(mFeeFeed == NULL) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	
	/* finally, have BlockCryptor set up its stuff. */
	unsigned plainBlockSize  = feeFEEDPlainBlockSize(mFeeFeed);
	unsigned cipherBlockSize = feeFEEDCipherBlockSize(mFeeFeed);
	setup(encoding ? plainBlockSize  : cipherBlockSize, // blockSizeIn
		  encoding ? cipherBlockSize : plainBlockSize,	// blockSizeOut
		  false,										// pkcsPad
		  true,											// needsFinal
		  BCM_ECB,
		  NULL);										// IV
	mInitFlag = true;
}

// called by BlockCryptor
void CryptKit::FEEDContext::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)
{
	feeReturn frtn;
	unsigned actMoved;
	
	assert(mFeeFeed != NULL);
	frtn = feeFEEDEncryptBlock(mFeeFeed,
		(unsigned char *)plainText,
		(unsigned int)plainTextLen,
		(unsigned char *)cipherText,
		&actMoved,
		final ? 1 : 0);
	if(frtn) {
		throwCryptKit(frtn, "feeFEEDEncryptBlock");
	}
	if(actMoved > cipherTextLen) {
		/* Overflow already occurred! */
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	cipherTextLen = actMoved;
}

void CryptKit::FEEDContext::decryptBlock(
	const void		*cipherText,		// length implied (one cipher block)
	size_t			cipherTextLen,
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)
{
	feeReturn frtn;
	unsigned actMoved;
	
	assert(mFeeFeed != NULL);
	frtn = feeFEEDDecryptBlock(mFeeFeed,
		(unsigned char *)cipherText,
		(unsigned int)inBlockSize(),
		(unsigned char *)plainText,
		&actMoved,
		final ? 1 : 0);
	if(frtn) {
		throwCryptKit(frtn, "feeFEEDDecryptBlock");
	}
	if(actMoved > plainTextLen) {
		/* Overflow already occurred! */
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	plainTextLen = actMoved;
}

/*
 * Additional query size support, necessary because we don't conform to 
 * BlockCryptor's standard one-to-one block scheme
 */
 
#define BUFFER_DEBUG	0
#if		BUFFER_DEBUG
#define bprintf(s)		printf s
#else
#define bprintf(s)
#endif

size_t CryptKit::FEEDContext::inputSize(
	size_t 			outSize)			// input for given output size
{
	/*
	 * We've been assured that this is NOT called for the final() op...
	 */
	unsigned inSize;
	if(encoding()) {
		inSize = feeFEEDPlainTextSize(mFeeFeed, (unsigned int)outSize, 0);
	}
	else {
		inSize = feeFEEDCipherTextSize(mFeeFeed, (unsigned int)outSize, 0);
	}
	
	/* account for possible pending buffered input */
	if(inSize >= inBufSize()) {
		inSize -= inBufSize();
	}
	
	/* round up to next block size, then lop off one...anything from
	 * blockSize*n to (blockSize*n)-1 has same effect */
	unsigned inBlocks = (unsigned int)((inSize + inBlockSize()) / inBlockSize());
	inSize = (unsigned int)(inBlocks * inBlockSize()) - 1;
	bprintf(("--- FEEDContext::inputSize  inSize 0x%x outSize 0x%x\n",
		inSize, outSize));
	return inSize;
}

size_t CryptKit::FEEDContext::outputSize(
	bool 			final, 
	size_t 			inSize) 			// output for given input size
{
	size_t rtn;
	if(encoding()) {
		rtn = feeFEEDCipherTextSize(mFeeFeed, (unsigned int)(inSize + inBufSize()), final ? 1 : 0);
	}
	else {
		rtn = feeFEEDPlainTextSize(mFeeFeed, (unsigned int)(inSize + inBufSize()), final ? 1 : 0);
	}
	bprintf(("--- FEEDContext::outputSize inSize 0x%x outSize 0x%x final %d\n",
		inSize, rtn, final));
	return rtn;
}

void CryptKit::FEEDContext::minimumProgress(
	size_t 			&in, 
	size_t 			&out) 				// minimum progress chunks
{
	if(encoding()) {
		/*
		 * -- in  := one block plaintext
		 * -- out := current cipher size for one block plaintext
		 */
		in  = inBlockSize();
		out = feeFEEDCipherBufSize(mFeeFeed, 0);
	}
	else {
		/*
		 * -- in  := current cipher size for one block plaintext
		 * -- out := one block plaintext
		 */
		in  = feeFEEDCipherBufSize(mFeeFeed, 0); 
		out = outBlockSize();
	}
	
	/* 
	 * Either case - input adjusted for pending. Note inBufSize can be up to one 
	 * input block size, leaving the temp result zero here....
	 */
	assert(in >= inBufSize());
	in -= inBufSize();
	
	/* if it is zero, bump it up so caller can make something happen */
	if(in == 0) {
		in++;
	}
	bprintf(("--- FEEDContext::minProgres inSize 0x%x outSize 0x%x\n",
		in, out));
}

/***
 *** FEEDExp - 2:1 FEED - encrypt n bytes of plaintext, get (roughly) 2n bytes
 *** of ciphertext. Ciphertext is larger than with FEED, but this is faster.
 ***/
CryptKit::FEEDExpContext::~FEEDExpContext()
{
	if(mFeeFeedExp) {
		feeFEEDExpFree(mFeeFeedExp);
		mFeeFeedExp = NULL;
	}
	if(mFeeKey && mAllocdFeeKey) {
		feePubKeyFree(mFeeKey);
	}
	mFeeKey = NULL;
	mInitFlag = false;
}

// called by CSPFullPluginSession; reusable
void CryptKit::FEEDExpContext::init(
	const Context &context, 
	bool encoding)
{
	if(mInitFlag && !opStarted()) {
		/* reusing - e.g. query followed by encrypt */
		return;
	}
	
	/* fetch FEE key from context */
	CSSM_KEYCLASS 	keyClass;
	CSSM_KEYUSE		keyUse;
	
	if(encoding) {
		/* encrypting to public key */
		keyClass = CSSM_KEYCLASS_PUBLIC_KEY;
		keyUse   = CSSM_KEYUSE_ENCRYPT;
	}
	else {
		/* decrypting with private key */
		keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
		keyUse   = CSSM_KEYUSE_DECRYPT;
	}
	if(mFeeKey == NULL) {
		assert(!opStarted());
		mFeeKey = contextToFeeKey(context,
			session(),
			CSSM_ATTRIBUTE_KEY,
			keyClass,
			keyUse,
			mAllocdFeeKey);
	}
	else {
		assert(opStarted());
	}
	
	/* validate context - no other attributes allowed */
	validateFeedContext(context);

	/* OK, looks good. Cook up a feeFEEDExp object. */
	if(mFeeFeedExp != NULL) {
		/* not reusable */
		assert(opStarted());
		feeFEEDExpFree(mFeeFeedExp);
		mFeeFeedExp = NULL;
	}
	mFeeFeedExp = feeFEEDExpNewWithPubKey(mFeeKey,
		feeRandCallback,
		&session());
	if(mFeeFeedExp == NULL) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	
	/* finally, have BlockCryptor set up its stuff. */
	unsigned plainBlockSize = feeFEEDExpPlainBlockSize(mFeeFeedExp);
	unsigned cipherBlockSize = feeFEEDExpCipherBlockSize(mFeeFeedExp);
	setup(encoding ? plainBlockSize  : cipherBlockSize, // blockSizeIn
		  encoding ? cipherBlockSize : plainBlockSize,	// blockSizeOut
		  false,										// pkcs5Pad
		  true,											// needsFinal
		  BCM_ECB,
		  NULL);											// IV
	mInitFlag = true;
}

// called by BlockCryptor
void CryptKit::FEEDExpContext::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)
{
	feeReturn frtn;
	unsigned actMoved;
	
	assert(mFeeFeedExp != NULL);
	frtn = feeFEEDExpEncryptBlock(mFeeFeedExp,
		(unsigned char *)plainText,
		(unsigned int)plainTextLen,
		(unsigned char *)cipherText,
		&actMoved,
		final ? 1 : 0);
	if(frtn) {
		throwCryptKit(frtn, "feeFEEDExpEncryptBlock");
	}
	if(actMoved > cipherTextLen) {
		/* Overflow already occurred! */
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	cipherTextLen = actMoved;
}

void CryptKit::FEEDExpContext::decryptBlock(
	const void		*cipherText,		// length implied (one cipher block)
	size_t			cipherTextLen,
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)
{
	feeReturn frtn;
	unsigned actMoved;
	
	assert(mFeeFeedExp != NULL);
	frtn = feeFEEDExpDecryptBlock(mFeeFeedExp,
		(unsigned char *)cipherText,
		(unsigned int)inBlockSize(),
		(unsigned char *)plainText,
		&actMoved,
		final ? 1 : 0);
	if(frtn) {
		throwCryptKit(frtn, "feeFEEDExpDecryptBlock");
	}
	if(actMoved > plainTextLen) {
		/* Overflow already occurred! */
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	plainTextLen = actMoved;
}

/* convert uint32 to big-endian 4 bytes */
static void int32ToBytes(
	uint32_t i,
	unsigned char *b)
{
	for(int dex=3; dex>=0; dex--) {
		b[dex] = i;
		i >>= 8;
	}
}

/* 
 * X9.63 key derivation with optional SharedInfo passed as 
 * context attribute CSSM_ATTRIBUTE_SALT.
 */
static feeReturn ecdhKdf(
	const Context &context, 
	const unsigned char *Z,		/* shared secret, i.e., output of ECDH */
	unsigned ZLen,
	CSSM_DATA *K)				/* output RETURNED in K->Data, length K->Length bytes */
{
	/* SharedInfo via salt, from context, optional */
	const unsigned char *sharedInfo = NULL;
	CSSM_SIZE sharedInfoLen = 0;
	
	CssmData *salt = context.get<CssmData>(CSSM_ATTRIBUTE_SALT);
	if(salt != NULL) {
		sharedInfo = (const unsigned char *)salt->Data;
		sharedInfoLen = salt->Length;
	}
	
	unsigned char *outp = K->Data;
	CSSM_SIZE bytesToGo = K->Length;
	CC_SHA1_CTX sha1;
	uint32_t counter = 1;
	uint8 counterBytes[4];
	unsigned char digOut[CC_SHA1_DIGEST_LENGTH];
	
	do {
		/* K[i] = Hash(Z || Counter || SharedInfo) */
		CC_SHA1_Init(&sha1);
		CC_SHA1_Update(&sha1, Z, ZLen);
		int32ToBytes(counter, counterBytes);
		CC_SHA1_Update(&sha1, counterBytes, 4);
		if(sharedInfoLen) {
			CC_SHA1_Update(&sha1, sharedInfo, (CC_LONG)sharedInfoLen);
		}
		CC_SHA1_Final(digOut, &sha1);
		
		/* digest --> output */
		unsigned toMove = CC_SHA1_DIGEST_LENGTH;
		if(toMove > bytesToGo) {
			toMove = (unsigned int)bytesToGo;
		}
		memmove(outp, digOut, toMove);
		
		counter++;
		outp += toMove;
		bytesToGo -= toMove;
		
	} while(bytesToGo);
	
	return FR_Success;
}

/*
 * Elliptic curve Diffie-Hellman key exchange. The public key is 
 * specified in one of two ways - a raw X9.62 format public key 
 * string in Param, or a CSSM_KEY in the Context. 
 * Requested size, in keyData->Length, must be the same size as
 * the keys' modulus. Data is returned in keyData->Data, which is 
 * allocated by the caller.
 * Optionally performs X9.63 key derivation if algId == 
 * CSSM_ALGID_ECDH_X963_KDF, with the optional SharedInfo passed
 * as optional context attribute CSSM_ATTRIBUTE_SALT.
 */
void CryptKit::DeriveKey_ECDH (
	const Context &context,
	CSSM_ALGORITHMS algId,		
	const CssmData &Param,			// other's public key. may be empty
	CSSM_DATA *keyData,				// mallocd by caller
									// we fill in keyData->Length bytes
	AppleCSPSession &session)
{
	bool mallocdPrivKey;
	size_t privSize;
	
	/* private ECDH key from context - required */
	feePubKey privKey = contextToFeeKey(context, session, CSSM_ATTRIBUTE_KEY,
		CSSM_KEYCLASS_PRIVATE_KEY, CSSM_KEYUSE_DERIVE, mallocdPrivKey);
	if(privKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_KEY);
	}
	privSize = (feePubKeyBitsize(privKey) + 7) / 8;
	if((algId == CSSM_ALGID_ECDH) & (privSize != keyData->Length)) {
		/* exact match required here */
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	
	/*
	 * Public key ("their" key) can come from two places:
	 * -- in the context as a CSSM_ATTRIBUTE_PUBLIC_KEY. This is how 
	 *    public keys in X509 format must be used in this function.
	 * -- in the incoming Param, the raw unformatted (ANSI X9.62) form 
	 */
	bool mallocdPubKey = false;
	feePubKey pubKey = NULL;
	if(Param.Data == NULL) {
		/* this throws if no key present */
		pubKey = contextToFeeKey(context, session, CSSM_ATTRIBUTE_PUBLIC_KEY,
			CSSM_KEYCLASS_PUBLIC_KEY, CSSM_KEYUSE_DERIVE, mallocdPubKey);
	}
	if((pubKey == NULL) && (Param.Data == NULL)) {
		errorLog0("DeriveKey_ECDH: no pub_key\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	unsigned char *output = NULL;
	unsigned outputLen = 0;
	feeReturn frtn = feePubKeyECDH(privKey, pubKey, 
		(const unsigned char *)Param.Data, (unsigned)Param.Length,
		&output, &outputLen);
	if(frtn) {
		goto errOut;
	}
	switch(algId) {
		case CSSM_ALGID_ECDH:
			/*
			 * Raw ECDH - requested length must match the generated size
			 * exactly. If so, return the result unmodified.
			 */
			if(outputLen != keyData->Length) {
				errorLog0("DeriveKey_ECDH: length mismatch\n");
				frtn = FR_Internal;
				break;
			}
			memmove(keyData->Data, output, outputLen);
			break;
		case CSSM_ALGID_ECDH_X963_KDF:
			/* Further processing... */
			frtn = ecdhKdf(context, output, outputLen, keyData);
			break;
		default:
			/* shouldn't be here */
			frtn = FR_Internal;
			break;
	}

errOut:
	if(mallocdPrivKey) {
		feePubKeyFree(privKey);
	}
	if(mallocdPubKey) {
		feePubKeyFree(pubKey);
	}
	if(output != NULL) {
		ffree(output);
	}
	if(frtn) {
		throwCryptKit(frtn, NULL);
	}
}

#endif	/* CRYPTKIT_CSP_ENABLE */
