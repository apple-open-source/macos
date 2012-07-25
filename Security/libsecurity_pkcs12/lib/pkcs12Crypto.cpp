/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * pkc12Crypto.cpp - PKCS12-specific cyptrographic routines
 */

#include "pkcs12Crypto.h"
#include "pkcs12Utils.h"
#include "pkcs12Debug.h"
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_keychain/Access.h>

/*
 * Given appropriate P12-style parameters, cook up a CSSM_KEY.
 * Caller MUST CSSM_FreeKey() when it's done with the key. 
 */
#define KEY_LABEL	"p12 key"

CSSM_RETURN p12KeyGen(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY			&key,
	bool				isForEncr,	// true: en/decrypt   false: MAC
	CSSM_ALGORITHMS		keyAlg,
	CSSM_ALGORITHMS		pbeHashAlg,	// actually must be SHA1 for now
	uint32				keySizeInBits,
	uint32				iterCount,
	const CSSM_DATA		&salt,
	/* exactly one of the following two must be valid */
	const CSSM_DATA		*pwd,		// unicode external representation 
	const CSSM_KEY		*passKey,
	CSSM_DATA			&iv)		// referent is optional
{
	CSSM_RETURN					crtn;
	CSSM_CC_HANDLE 				ccHand;
	CSSM_DATA					dummyLabel;
	CSSM_ACCESS_CREDENTIALS		creds;
	
	memset(&key, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));

	/* infer key derivation algorithm */
	CSSM_ALGORITHMS deriveAlg = CSSM_ALGID_NONE;
	if(pbeHashAlg != CSSM_ALGID_SHA1) {
		return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	if(isForEncr) {
		/*
		 * FIXME - if this key is going to be used to wrap/unwrap a 
		 * shrouded key bag, its usage will change accordingly...
		 */
		deriveAlg = CSSM_ALGID_PKCS12_PBE_ENCR;
	}
	else {
		deriveAlg = CSSM_ALGID_PKCS12_PBE_MAC;
	}
	CSSM_CRYPTO_DATA seed;
	if(pwd) {
		seed.Param = *pwd;
	}
	else {
		seed.Param.Data = NULL;
		seed.Param.Length = 0;
	}
	seed.Callback = NULL;
	seed.CallerCtx = NULL;
	
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		deriveAlg,
		keyAlg,
		keySizeInBits,
		&creds,
		passKey,		// BaseKey
		iterCount,
		&salt,
		&seed,			// seed
		&ccHand);
	if(crtn) {
		p12LogCssmError("CSSM_CSP_CreateDeriveKeyContext", crtn);
		return crtn;
	}
	
	dummyLabel.Length = strlen(KEY_LABEL);
	dummyLabel.Data = (uint8 *)KEY_LABEL;
	
	/* KEYUSE_ANY - this is just an ephemeral session key */
	crtn = CSSM_DeriveKey(ccHand,
		&iv,
		CSSM_KEYUSE_ANY,
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE,
		&dummyLabel,
		NULL,			// cred and acl
		&key);
	CSSM_DeleteContext(ccHand);
	if(crtn) {
		p12LogCssmError("CSSM_DeriveKey", crtn);
	}
	return crtn;
}

/*
 * Decrypt (typically, an encrypted P7 ContentInfo contents)
 */
CSSM_RETURN p12Decrypt(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		&cipherText,
	CSSM_ALGORITHMS		keyAlg,				
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ALGORITHMS		pbeHashAlg,			// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				blockSizeInBytes,	// for IV
	CSSM_PADDING		padding,			// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBCPadIV8, etc.
	uint32				iterCount,
	const CSSM_DATA		&salt,
	/* exactly one of the following two must be valid */
	const CSSM_DATA		*pwd,		// unicode external representation
	const CSSM_KEY		*passKey,
	SecNssCoder			&coder,		// for mallocing plainText
	CSSM_DATA			&plainText)
{
	CSSM_RETURN crtn;
	CSSM_KEY ckey;
	CSSM_CC_HANDLE ccHand = 0;
	CSSM_DATA ourPtext = {0, NULL};
	CSSM_DATA remData = {0, NULL};
	
	/* P12 style IV derivation, optional */
	CSSM_DATA iv = {0, NULL};
	CSSM_DATA_PTR ivPtr = NULL;
	if(blockSizeInBytes) {
		coder.allocItem(iv, blockSizeInBytes);
		ivPtr = &iv;
	}
	
	/* P12 style key derivation */
	crtn = p12KeyGen(cspHand, ckey, true, keyAlg, pbeHashAlg,
		keySizeInBits, iterCount, salt, pwd, passKey, iv);
	if(crtn) {
		return crtn;
	}	
	/* subsequent errors to errOut: */
	
	/* CSSM context */
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
		encrAlg,
		mode,
		NULL,			// access cred
		&ckey,
		ivPtr,			// InitVector, optional
		padding,	
		NULL,			// Params
		&ccHand);
	if(crtn) {
		cuPrintError("CSSM_CSP_CreateSymmetricContext", crtn);
		goto errOut;
	}
	
	/* go - CSP mallocs ptext and rem data */
	CSSM_SIZE bytesDecrypted;
	crtn = CSSM_DecryptData(ccHand,
		&cipherText,
		1,
		&ourPtext,
		1,
		&bytesDecrypted,
		&remData);
	if(crtn) {
		cuPrintError("CSSM_DecryptData", crtn);
	}
	else {
		coder.allocCopyItem(ourPtext, plainText);
		plainText.Length = bytesDecrypted;
		
		/* plaintext copied into coder space; free the memory allocated
		 * by the CSP */
		freeCssmMemory(cspHand, ourPtext.Data);
	}
	/* an artifact of CSPFullPLuginSession - this never contains
	 * valid data but sometimes gets mallocds */
	if(remData.Data) {
		freeCssmMemory(cspHand, remData.Data);
	}
errOut:
	if(ccHand) {
		CSSM_DeleteContext(ccHand);
	}
	CSSM_FreeKey(cspHand, NULL, &ckey, CSSM_FALSE);
	return crtn;
}

/*
 * Decrypt (typically, an encrypted P7 ContentInfo contents)
 */
CSSM_RETURN p12Encrypt(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		&plainText,
	CSSM_ALGORITHMS		keyAlg,				
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ALGORITHMS		pbeHashAlg,			// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				blockSizeInBytes,	// for IV
	CSSM_PADDING		padding,			// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBCPadIV8, etc.
	uint32				iterCount,
	const CSSM_DATA		&salt,
	/* exactly one of the following two must be valid */
	const CSSM_DATA		*pwd,		// unicode external representation
	const CSSM_KEY		*passKey,
	SecNssCoder			&coder,		// for mallocing cipherText
	CSSM_DATA			&cipherText)
{
	CSSM_RETURN crtn;
	CSSM_KEY ckey;
	CSSM_CC_HANDLE ccHand = 0;
	CSSM_DATA ourCtext = {0, NULL};
	CSSM_DATA remData = {0, NULL};
	
	/* P12 style IV derivation, optional */
	CSSM_DATA iv = {0, NULL};
	CSSM_DATA_PTR ivPtr = NULL;
	if(blockSizeInBytes) {
		coder.allocItem(iv, blockSizeInBytes);
		ivPtr = &iv;
	}
	
	/* P12 style key derivation */
	crtn = p12KeyGen(cspHand, ckey, true, keyAlg, pbeHashAlg,
		keySizeInBits, iterCount, salt, pwd, passKey, iv);
	if(crtn) {
		return crtn;
	}	
	/* subsequent errors to errOut: */
		
	/* CSSM context */
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
		encrAlg,
		mode,
		NULL,			// access cred
		&ckey,
		ivPtr,			// InitVector, optional
		padding,	
		NULL,			// Params
		&ccHand);
	if(crtn) {
		cuPrintError("CSSM_CSP_CreateSymmetricContext", crtn);
		goto errOut;
	}
	
	/* go - CSP mallocs ctext and rem data */
	CSSM_SIZE bytesEncrypted;
	crtn = CSSM_EncryptData(ccHand,
		&plainText,
		1,
		&ourCtext,
		1,
		&bytesEncrypted,
		&remData);
	if(crtn) {
		cuPrintError("CSSM_DecryptData", crtn);
	}
	else {
		coder.allocCopyItem(ourCtext, cipherText);
		cipherText.Length = bytesEncrypted;
		
		/* plaintext copied into coder space; free the memory allocated
		 * by the CSP */
		freeCssmMemory(cspHand, ourCtext.Data);
	}
	/* an artifact of CSPFUllPLuginSession - this never contains
	 * valid data but sometimes gets mallocds */
	if(remData.Data) {
		freeCssmMemory(cspHand, remData.Data);
	}
errOut:
	if(ccHand) {
		CSSM_DeleteContext(ccHand);
	}
	CSSM_FreeKey(cspHand, NULL, &ckey, CSSM_FALSE);
	return crtn;
}

/*
 * Calculate the MAC for a PFX. Caller is either going compare
 * the result against an existing PFX's MAC or drop the result into 
 * a newly created PFX.
 */
CSSM_RETURN p12GenMac(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		&ptext,	// e.g., NSS_P12_DecodedPFX.derAuthSaafe
	CSSM_ALGORITHMS		alg,	// better be SHA1!
	unsigned			iterCount,
	const CSSM_DATA		&salt,
	/* exactly one of the following two must be valid */
	const CSSM_DATA		*pwd,		// unicode external representation
	const CSSM_KEY		*passKey,
	SecNssCoder			&coder,		// for mallocing macData
	CSSM_DATA			&macData)	// RETURNED 
{
	CSSM_RETURN crtn;
	CSSM_CC_HANDLE ccHand = 0;
	
	/* P12 style key derivation */
	unsigned keySizeInBits;
	CSSM_ALGORITHMS hmacAlg;
	switch(alg) {
		case CSSM_ALGID_SHA1:
			keySizeInBits = 160;
			hmacAlg = CSSM_ALGID_SHA1HMAC;
			break;
		case CSSM_ALGID_MD5:
			/* not even sure if this is legal in p12 world... */
			keySizeInBits = 128;
			hmacAlg = CSSM_ALGID_MD5HMAC;
			break;
		default:
			return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	CSSM_KEY macKey;
	CSSM_DATA iv = {0, NULL};
	crtn = p12KeyGen(cspHand, macKey, false, hmacAlg, alg,
		keySizeInBits, iterCount, salt, pwd, passKey, iv);
	if(crtn) {
		return crtn;
	}	
	/* subsequent errors to errOut: */

	/* prealloc the mac data */
	coder.allocItem(macData, keySizeInBits / 8);
	crtn = CSSM_CSP_CreateMacContext(cspHand, hmacAlg, &macKey, &ccHand);
	if(crtn) {
		cuPrintError("CSSM_CSP_CreateMacContext", crtn);
		goto errOut;
	}
	
	crtn = CSSM_GenerateMac (ccHand, &ptext, 1, &macData);
	if(crtn) {
		cuPrintError("CSSM_GenerateMac", crtn);
	}
errOut:
	if(ccHand) {
		CSSM_DeleteContext(ccHand);
	}
	CSSM_FreeKey(cspHand, NULL, &macKey, CSSM_FALSE);
	return crtn;
}

/*
 * Unwrap a shrouded key.
 */
CSSM_RETURN p12UnwrapKey(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_DL_DB_HANDLE_PTR	dlDbHand,		// optional
	int					keyIsPermanent,		// nonzero - store in DB
	const CSSM_DATA		&shroudedKeyBits,
	CSSM_ALGORITHMS		keyAlg,				// of the unwrapping key
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ALGORITHMS		pbeHashAlg,			// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				blockSizeInBytes,	// for IV
	CSSM_PADDING		padding,			// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBCPadIV8, etc.
	uint32				iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		*pwd,		// unicode external representation
	const CSSM_KEY		*passKey,
	SecNssCoder			&coder,		// for mallocing privKey
	const CSSM_DATA		&labelData,
	SecAccessRef		access,		// optional 
	bool				noAcl,
	CSSM_KEYUSE			keyUsage,
	CSSM_KEYATTR_FLAGS	keyAttrs,

	/*
	 * Result: a private key, reference format, optionaly stored
	 * in dlDbHand
	 */
	CSSM_KEY_PTR		&privKey)
{
	CSSM_RETURN crtn;
	CSSM_KEY ckey;
	CSSM_CC_HANDLE ccHand = 0;
	CSSM_KEY wrappedKey;
	CSSM_KEY unwrappedKey;
	CSSM_KEYHEADER &hdr = wrappedKey.KeyHeader;
	CSSM_DATA descrData = {0, NULL};	// not used for PKCS8 wrap 
	CSSM_KEYATTR_FLAGS reqAttr = keyAttrs;
	
	ResourceControlContext rcc;
	ResourceControlContext *rccPtr = NULL;
	Security::KeychainCore::Access::Maker maker;
	
	/* P12 style IV derivation, optional */
	CSSM_DATA iv = {0, NULL};
	CSSM_DATA_PTR ivPtr = NULL;
	if(blockSizeInBytes) {
		coder.allocItem(iv, blockSizeInBytes);
		ivPtr = &iv;
	}
	
	/* P12 style key derivation */
	crtn = p12KeyGen(cspHand, ckey, true, keyAlg, pbeHashAlg,
		keySizeInBits, iterCount, salt, pwd, passKey, iv);
	if(crtn) {
		return crtn;
	}	
	/* subsequent errors to errOut: */
		
	/* CSSM context */
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
		encrAlg,
		mode,
		NULL,			// access cred
		&ckey,
		ivPtr,			// InitVector, optional
		padding,	
		NULL,			// Params
		&ccHand);
	if(crtn) {
		p12LogCssmError("CSSM_CSP_CreateSymmetricContext", crtn);
		goto errOut;
	}
	if(dlDbHand) {
		crtn = p12AddContextAttribute(ccHand, 
			CSSM_ATTRIBUTE_DL_DB_HANDLE,
			sizeof(CSSM_ATTRIBUTE_DL_DB_HANDLE),
			dlDbHand);
		if(crtn) {
			p12LogCssmError("AddContextAttribute", crtn);
			goto errOut;
		}
	}
	
	/*
	 * Cook up minimal WrappedKey header fields
	 */
	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	memset(&unwrappedKey, 0, sizeof(CSSM_KEY));
	
	hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
	hdr.BlobType = CSSM_KEYBLOB_WRAPPED;
	hdr.Format = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8;
	
	/* 
	 * This one we do not know. The CSP will figure out the format 
	 * of the unwrapped key after it decrypts the raw key material. 
	 */
	hdr.AlgorithmId = CSSM_ALGID_NONE;
	hdr.KeyClass = CSSM_KEYCLASS_PRIVATE_KEY;
	
	/* also inferred by CSP */
	hdr.LogicalKeySizeInBits = 0;
	hdr.KeyAttr = CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_EXTRACTABLE;
	hdr.KeyUsage = CSSM_KEYUSE_ANY;
	hdr.WrapAlgorithmId = encrAlg;
	hdr.WrapMode = mode;
	
	if(dlDbHand && keyIsPermanent) {
		reqAttr |= CSSM_KEYATTR_PERMANENT;
	}

	wrappedKey.KeyData = shroudedKeyBits;
	
	if(!noAcl) {
		// Create a Access::Maker for the initial owner of the private key.
		memset(&rcc, 0, sizeof(rcc));
		maker.initialOwner(rcc);
		rccPtr = &rcc;
	}
	
	crtn = CSSM_UnwrapKey(ccHand,
		NULL,				// PublicKey
		&wrappedKey,
		keyUsage,
		reqAttr,
		&labelData,
		rccPtr,					// CredAndAclEntry
		privKey,
		&descrData);			// required
	if(crtn) {
		p12LogCssmError("CSSM_UnwrapKey", crtn);
		if(crtn == CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA) {
			/* report in a keychain-friendly way */
			crtn = errSecDuplicateItem;
		}
	}
	
	// Finally fix the acl and owner of the private key to the 
	// specified access control settings.
	if((crtn == CSSM_OK) && !noAcl) {
		try {
			CssmClient::KeyAclBearer bearer(
				cspHand, *privKey, Allocator::standard());
			SecPointer<KeychainCore::Access> initialAccess(access ?
				KeychainCore::Access::required(access) :		/* caller-supplied */
				new KeychainCore::Access("privateKey"));		/* default */
			initialAccess->setAccess(bearer, maker);
		}
		catch (const CssmError &e) {
			/* not implemented means we're talking to the CSP which does
			 * not implement ACLs */
			if(e.error != CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED) {
				crtn = e.error;
			}
		}
		catch(...) {
			p12ErrorLog("p12 exception on setAccess\n");
			crtn = errSecAuthFailed;	/* ??? */
		}
	}

errOut:
	if(ccHand) {
		CSSM_DeleteContext(ccHand);
	}
	CSSM_FreeKey(cspHand, NULL, &ckey, CSSM_FALSE);
	return crtn;
}

/*
 * Wrap a private key, yielding shrouded key bits. 
 */
CSSM_RETURN p12WrapKey(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		privKey,
	const CSSM_ACCESS_CREDENTIALS *privKeyCreds,
	CSSM_ALGORITHMS		keyAlg,				// of the unwrapping key
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ALGORITHMS		pbeHashAlg,			// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				blockSizeInBytes,	// for IV
	CSSM_PADDING		padding,			// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBCPadIV8, etc.
	uint32				iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		*pwd,		// unicode external representation
	const CSSM_KEY		*passKey,
	SecNssCoder			&coder,		// for mallocing keyBits
	CSSM_DATA			&shroudedKeyBits)	// RETURNED
{
	CSSM_RETURN crtn;
	CSSM_KEY ckey;
	CSSM_CC_HANDLE ccHand = 0;
	CSSM_KEY wrappedKey;
	CSSM_CONTEXT_ATTRIBUTE attr;
	CSSM_DATA descrData = {0, NULL};
	CSSM_ACCESS_CREDENTIALS creds;
	
	/* key must be extractable */
	if (!(privKey->KeyHeader.KeyAttr & CSSM_KEYATTR_EXTRACTABLE)) {
		return errSecDataNotAvailable;
	}

	if(privKeyCreds == NULL) {
		/* i.e., key is from the bare CSP with no ACL support */
		memset(&creds, 0, sizeof(creds));
		privKeyCreds = &creds;
	}
	
	/* P12 style IV derivation, optional */
	CSSM_DATA iv = {0, NULL};
	CSSM_DATA_PTR ivPtr = NULL;
	if(blockSizeInBytes) {
		coder.allocItem(iv, blockSizeInBytes);
		ivPtr = &iv;
	}
	
	/* P12 style key derivation */
	crtn = p12KeyGen(cspHand, ckey, true, keyAlg, pbeHashAlg,
		keySizeInBits, iterCount, salt, pwd, passKey, iv);
	if(crtn) {
		return crtn;
	}	
	/* subsequent errors to errOut: */
		
	/* CSSM context */
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
		encrAlg,
		mode,
		NULL,			// access cred
		&ckey,
		ivPtr,			// InitVector, optional
		padding,	
		NULL,			// Params
		&ccHand);
	if(crtn) {
		p12LogCssmError("CSSM_CSP_CreateSymmetricContext", crtn);
		goto errOut;
	}
	
	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	
	/* specify PKCS8 wrap format */
	attr.AttributeType = CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT;
	attr.AttributeLength = sizeof(uint32);
	attr.Attribute.Uint32 = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8;
	crtn = CSSM_UpdateContextAttributes(
		ccHand,
		1,
		&attr);
	if(crtn) {
		p12LogCssmError("CSSM_UpdateContextAttributes", crtn);
		goto errOut;
	}
	
	crtn = CSSM_WrapKey(ccHand,
		privKeyCreds,
		privKey,
		&descrData,			// DescriptiveData
		&wrappedKey);
	if(crtn) {
		p12LogCssmError("CSSM_WrapKey", crtn);
	}
	else {
		coder.allocCopyItem(wrappedKey.KeyData, shroudedKeyBits);
		
		/* this was mallocd by CSP */
		freeCssmMemory(cspHand, wrappedKey.KeyData.Data);
	}
errOut:
	if(ccHand) {
		CSSM_DeleteContext(ccHand);
	}
	CSSM_FreeKey(cspHand, NULL, &ckey, CSSM_FALSE);
	return crtn;
}
