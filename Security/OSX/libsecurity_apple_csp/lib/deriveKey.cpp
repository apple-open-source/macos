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
 	File:		deriveKey.cpp
	
 	Contains:	CSSM_DeriveKey functions

 	Copyright (c) 2000,2011-2012,2014 Apple Inc. All Rights Reserved.

*/

#include <HMACSHA1.h>
#include <pbkdf2.h>
#include <pbkdDigest.h>
#include <pkcs12Derive.h>
#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"
#include "AppleCSPContext.h"
#include "cspdebugging.h"
#include <security_cdsa_utilities/context.h>
#include <DH_exchange.h>
#include "FEEAsymmetricContext.h"

/* minimum legal values */
#define PBKDF2_MIN_SALT			8		/* bytes */
#define PBKDF2_MIN_ITER_CNT		1000	/* iteration count */

#define ALLOW_ZERO_PASSWORD		1 

void AppleCSPSession::DeriveKey_PBKDF2(
	const Context &context,
	const CssmData &Param,
	CSSM_DATA *keyData)
{
	/* validate algorithm-specific arguments */

	/* Param must point to a CSSM_PKCS5_PBKDF2_PARAMS */
	if(Param.Length != sizeof(CSSM_PKCS5_PBKDF2_PARAMS)) {
		errorLog0("DeriveKey_PBKDF2: Param wrong size\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_INPUT_POINTER);
	}
	const CSSM_PKCS5_PBKDF2_PARAMS *pbkdf2Params = 
		reinterpret_cast<const CSSM_PKCS5_PBKDF2_PARAMS *>(Param.Data);
	if(pbkdf2Params == NULL) {
		errorLog0("DeriveKey_PBKDF2: null Param.Data\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
	}
	
	/* Get passphrase from either baseKey or from CSSM_PKCS5_PBKDF2_PARAMS */
	CssmKey *passKey = context.get<CssmKey>(CSSM_ATTRIBUTE_KEY);
	CSSM_SIZE	passphraseLen = 0;
	uint8 	*passphrase = NULL;
	if(passKey != NULL) {
		AppleCSPContext::symmetricKeyBits(context, *this,
			CSSM_ALGID_SECURE_PASSPHRASE, CSSM_KEYUSE_DERIVE, 
			passphrase, passphraseLen);
	}
	else {
		passphraseLen = pbkdf2Params->Passphrase.Length;
		passphrase = pbkdf2Params->Passphrase.Data;
	}
	
	#if 	!ALLOW_ZERO_PASSWORD
	/* passphrase required */
	if(passphrase == NULL) {
		errorLog0("DeriveKey_PBKDF2: null Passphrase\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
	}
	if(passphraseLen == 0) {		
		/* FIXME - enforce minimum length? */
		errorLog0("DeriveKey_PBKDF2: zero length passphrase\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_INPUT_POINTER);
	}
	#endif	/* ALLOW_ZERO_PASSWORD */
	
	if(pbkdf2Params->PseudoRandomFunction != 
			CSSM_PKCS5_PBKDF2_PRF_HMAC_SHA1) {
		errorLog0("DeriveKey_PBKDF2: invalid PRF\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	
	/* salt, from context, required */
	CssmData salt = context.get<CssmData>(CSSM_ATTRIBUTE_SALT,
		CSSMERR_CSP_MISSING_ATTR_SALT);
	if((salt.Data == NULL) || (salt.Length < PBKDF2_MIN_SALT)){
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_SALT);
	}
	
	/* iteration count, from context, required */
	uint32 iterCount = context.getInt(CSSM_ATTRIBUTE_ITERATION_COUNT,
		CSSMERR_CSP_MISSING_ATTR_ITERATION_COUNT);
	if(iterCount < PBKDF2_MIN_ITER_CNT) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ITERATION_COUNT);
	}
	
	/* 
	 * allocate a temp buffer, length 
	 *    = MAX (hLen, saltLen + 4) + 2 * hLen
	 *    = MAX (kSHA1DigestSize, saltLen + 4) + 2 * kSHA1DigestSize
	 */
	size_t tempLen = salt.Length + 4;
	if(tempLen < kSHA1DigestSize) {
		tempLen = kSHA1DigestSize;
	}
	tempLen += (2 * kSHA1DigestSize);
	CSSM_DATA tempData = {0, NULL};
	setUpData(tempData, tempLen, privAllocator);
	
	/* go */
	pbkdf2 (hmacsha1, 
		kSHA1DigestSize,
		passphrase, (uint32)passphraseLen,
		salt.Data, (uint32)salt.Length,
		iterCount,
		keyData->Data, (uint32)keyData->Length,
		tempData.Data);
	freeData(&tempData, privAllocator, false);
}

/*
 * PKCS5 v1.5 key derivation. Also used for traditional openssl key 
 * derivation, which is mighty similar to PKCS5 v1.5, with the addition
 * of the ability to generate more than (keysize + ivsize) bytes. 
 */
void AppleCSPSession::DeriveKey_PKCS5_V1_5(
	const Context &context,
	CSSM_ALGORITHMS algId,
	const CssmData &Param,			// IV optional, mallocd by app to indicate
									//   size 
	CSSM_DATA *keyData)				// mallocd by caller to indicate size
{
	CSSM_DATA pwd = {0, NULL};
	
	/* password from either Seed.Param or from base key */
	CssmCryptoData *cryptData = 
		context.get<CssmCryptoData>(CSSM_ATTRIBUTE_SEED);
	if((cryptData != NULL) && (cryptData->Param.Length != 0)) {
		pwd = cryptData->Param;
	}
	else {
		/* Get secure passphrase from base key */
		CssmKey *passKey = context.get<CssmKey>(CSSM_ATTRIBUTE_KEY);
		if (passKey != NULL) {
			AppleCSPContext::symmetricKeyBits(context, *this,
				CSSM_ALGID_SECURE_PASSPHRASE, CSSM_KEYUSE_DERIVE, 
				pwd.Data, pwd.Length);
		}
	}

	if(pwd.Data == NULL) {
		errorLog0("DeriveKey_PKCS5_V1_5: null Passphrase\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
	}
	if(pwd.Length == 0) {		
		errorLog0("DeriveKey_PKCS5_V1_5: zero length passphrase\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_INPUT_POINTER);
	}

	CSSM_ALGORITHMS hashAlg;
	unsigned digestLen;
	bool opensslAlg = false;
	switch(algId) {
		case CSSM_ALGID_PKCS5_PBKDF1_MD5:
			hashAlg = CSSM_ALGID_MD5;
			digestLen = kMD5DigestSize;
			break;
		case CSSM_ALGID_PKCS5_PBKDF1_MD2:
			hashAlg = CSSM_ALGID_MD2;
			digestLen = kMD2DigestSize;
			break;
		case CSSM_ALGID_PKCS5_PBKDF1_SHA1:
			hashAlg = CSSM_ALGID_SHA1;
			digestLen = kSHA1DigestSize;
			break;
		case CSSM_ALGID_PBE_OPENSSL_MD5:
			hashAlg = CSSM_ALGID_MD5;
			digestLen = kMD5DigestSize;
			opensslAlg = true;
			break;			
		default:
			/* should not have been called */
			assert(0);
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}

	/* IV optional */
	CSSM_DATA iv = Param;
	
	/* total requested length can't exceed digest size for struct PKCS5 v1.5*/
	if(!opensslAlg && ((keyData->Length + iv.Length) > digestLen)) {
		errorLog0("DeriveKey_PKCS5_V1_5: requested length larger than digest\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH);
	}
	
	/* salt, from context, required */
	CssmData salt = context.get<CssmData>(CSSM_ATTRIBUTE_SALT,
		CSSMERR_CSP_MISSING_ATTR_SALT);
	if(salt.Data == NULL) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_SALT);
	}
	
	/* iteration count, from context, required */
	uint32 iterCount = context.getInt(CSSM_ATTRIBUTE_ITERATION_COUNT,
		CSSMERR_CSP_MISSING_ATTR_ITERATION_COUNT);
		
	/* 
	 * Apply the underlying hash function Hash for c iterations to
	 *  the concatenation of the password P and the salt S, then 
	 * extract the first dkLen octets to produce a derived key DK:
	 *
	 *		T1 = Hash (P || S) ,
	 *		T2 = Hash (T1) ,
	 *		...
	 *		Tc = Hash (Tc-1) ,
	 *		DK = Tc<0..dkLen-1> .
	 */
	DigestCtx ctx;
	uint8 *keyDataP		= keyData->Data;
	size_t keyBytesToGo = keyData->Length;
	uint8 *ivDataP		= iv.Data;
	size_t ivBytesToGo  = iv.Length;
	bool looping		= false;		// true for additional bytes for openssl
	unsigned char digestOut[kMaxDigestSize];
	
	for(;;) {
		/* this loop guaranteed to only run once if !opensslAlg */
		DigestCtxInit(&ctx, hashAlg);
		
		if(looping) {
			/* openssl addition: re-digest the digest here */
			DigestCtxUpdate(&ctx, digestOut, digestLen);
		}
		
		/* digest password then salt */
		DigestCtxUpdate(&ctx, pwd.Data, (uint32)pwd.Length);
		DigestCtxUpdate(&ctx, salt.Data, (uint32)salt.Length);

		DigestCtxFinal(&ctx, digestOut);
		
		/* now iterCount-1 more iterations */
		for(unsigned dex=1; dex<iterCount; dex++) {
			DigestCtxInit(&ctx, hashAlg);
			DigestCtxUpdate(&ctx, digestOut, digestLen);
			DigestCtxFinal(&ctx, digestOut);
		}
		
		/* first n bytes to the key */
		uint32 bytesAvail = digestLen;
		size_t toMove = (keyBytesToGo > bytesAvail) ? bytesAvail : keyBytesToGo;
		memmove(keyDataP, digestOut, toMove);
		uint8 *remainder = digestOut + toMove;
		bytesAvail   -= toMove;
		keyDataP     += toMove;
		keyBytesToGo -= toMove;
		
		/* then optionally some to IV */
		if(ivBytesToGo && bytesAvail) {
			toMove = (ivBytesToGo > bytesAvail) ? bytesAvail : ivBytesToGo;
			memmove(ivDataP, remainder, toMove);
			ivDataP     += toMove;
			ivBytesToGo -= toMove;
		}
		if((keyBytesToGo == 0) && (ivBytesToGo == 0)) {
			/* guaranteed true for PKCS5 v1.5 */
			break;
		}
		
		assert(opensslAlg == true);
		looping = true;
	}
	DigestCtxFree(&ctx);
}

/*
 * Member function initially declared for CSPAbstractPluginSession;
 * we're overriding the null version in CSPFullPluginSession.
 *
 * We'll generate any type of key (for now). 
 */
void AppleCSPSession::DeriveKey(
	CSSM_CC_HANDLE CCHandle,
	const Context &context,
	CssmData &Param,
	uint32 KeyUsage,
	uint32 KeyAttr,
	const CssmData *KeyLabel,
	const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
	CssmKey &DerivedKey)
{
	/* validate input args, common to all algorithms */
	switch(context.algorithm()) {
		case CSSM_ALGID_PKCS5_PBKDF2:
		case CSSM_ALGID_DH:
		case CSSM_ALGID_PKCS12_PBE_ENCR:
		case CSSM_ALGID_PKCS12_PBE_MAC:
		case CSSM_ALGID_PKCS5_PBKDF1_MD5:
		case CSSM_ALGID_PKCS5_PBKDF1_MD2:
		case CSSM_ALGID_PKCS5_PBKDF1_SHA1:
		case CSSM_ALGID_PBE_OPENSSL_MD5:
		case CSSM_ALGID_OPENSSH1:
		#if CRYPTKIT_CSP_ENABLE
		case CSSM_ALGID_ECDH:
		case CSSM_ALGID_ECDH_X963_KDF:
		#endif
			break;
		/* maybe more here, later */
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	DerivedKey.KeyData.Data = NULL;
	DerivedKey.KeyData.Length = 0;
	cspKeyStorage keyStorage = cspParseKeyAttr(CKT_Session, KeyAttr);
	cspValidateKeyUsageBits(CKT_Session, KeyUsage);
	
	/* outgoing key type, required (though any algorithm is OK) */
	uint32 keyType = context.getInt(CSSM_ATTRIBUTE_KEY_TYPE,
		CSSMERR_CSP_MISSING_ATTR_KEY_TYPE);

	/* outgoing key size, required - any nonzero value is OK */
	uint32 reqKeySize = context.getInt(
		CSSM_ATTRIBUTE_KEY_LENGTH, 
		CSSMERR_CSP_MISSING_ATTR_KEY_LENGTH);

	/* cook up a place to put the key data */
	uint32 keySizeInBytes = (reqKeySize + 7) / 8;
	SymmetricBinaryKey *binKey = NULL;
	CSSM_DATA_PTR keyData = NULL;
	
	switch(keyStorage) {
		case CKS_None:
			/* no way */
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
		case CKS_Ref:
			/* cook up a symmetric binary key */
			binKey = new SymmetricBinaryKey(reqKeySize);
			keyData = &binKey->mKeyData;
			break;
		case CKS_Data:
			/* key bytes --> caller's cssmKey */
			keyData = &DerivedKey.KeyData;
			setUpData(*keyData, keySizeInBytes, 
				normAllocator);
			break;
	}

	/* break off to algorithm-specific code, whose job it is
	 * to fill in keyData->Data with keyData->Length bytes */
	switch(context.algorithm()) {
		case CSSM_ALGID_PKCS5_PBKDF2:
			DeriveKey_PBKDF2(context,
				Param,
				keyData);
			break;
		case CSSM_ALGID_DH:
			DeriveKey_DH(context,
				Param,
				keyData,
				*this);
			break;
		case CSSM_ALGID_PKCS12_PBE_ENCR:
		case CSSM_ALGID_PKCS12_PBE_MAC:
			DeriveKey_PKCS12(context,
				*this,
				Param,
				keyData);
			break;
		case CSSM_ALGID_PKCS5_PBKDF1_MD5:
		case CSSM_ALGID_PKCS5_PBKDF1_MD2:
		case CSSM_ALGID_PKCS5_PBKDF1_SHA1:
		case CSSM_ALGID_PBE_OPENSSL_MD5:
			DeriveKey_PKCS5_V1_5(context,
				context.algorithm(),
				Param,
				keyData);
			break;
		case CSSM_ALGID_OPENSSH1:
			DeriveKey_OpenSSH1(context,
				context.algorithm(),
				Param,
				keyData);
			break;
		#if CRYPTKIT_CSP_ENABLE
		case CSSM_ALGID_ECDH:
		case CSSM_ALGID_ECDH_X963_KDF:
			CryptKit::DeriveKey_ECDH(context,
				context.algorithm(),
				Param,
				keyData,
				*this);
			break;
		#endif
		/* maybe more here, later */
		default:
			assert(0);
	}
	
	/* set up outgoing header */
	KeyAttr &= ~KEY_ATTR_RETURN_MASK;
	CSSM_KEYHEADER &hdr = DerivedKey.KeyHeader;
	setKeyHeader(hdr,
		plugin.myGuid(),
		keyType,
		CSSM_KEYCLASS_SESSION_KEY, 
		KeyAttr, 
		KeyUsage);
	/* handle derived size < requested size, legal for Diffie-Hellman */
	hdr.LogicalKeySizeInBits = (uint32)(keyData->Length * 8);
	
	if(keyStorage == CKS_Ref) {
		/* store and convert to ref key */
		addRefKey(*binKey, DerivedKey);
	}
	else {
		/* Raw data */
		hdr.BlobType = CSSM_KEYBLOB_RAW;
		hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING; 
	}
}

