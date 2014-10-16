/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
 * p12Crypto.cpp - PKCS12 Crypto routines. App space reference version.
 *
 * Created 2/28/03 by Doug Mitchell.
 */

#include "p12Crypto.h"
#include "p12pbe.h"
#include <security_pkcs12/pkcs12Utils.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <Security/cssmapple.h>

/*
 * Free memory via specified plugin's app-level allocator
 */
static void appFreeCssmMemory(
	CSSM_HANDLE	hand,
	void 			*p)
{
	CSSM_API_MEMORY_FUNCS memFuncs;
	CSSM_RETURN crtn = CSSM_GetAPIMemoryFunctions(hand, &memFuncs);
	if(crtn) {
		cssmPerror("CSSM_GetAPIMemoryFunctions", crtn);
		/* oh well, leak and continue */
		return;
	}
	memFuncs.free_func(p, memFuncs.AllocRef);
}

/*
 * Given appropriate P12-style parameters, cook up a CSSM_KEY.
 * Eventually this will use DeriveKey; for now we do it ourself.
 */
CSSM_RETURN p12KeyGen_app(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY			&key,
	bool				isForEncr,	// true: en/decrypt   false: MAC
	CSSM_ALGORITHMS		keyAlg,
	CSSM_ALGORITHMS		pbeHashAlg,	// SHA1, MD5 only
	uint32				keySizeInBits,
	uint32				iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	CSSM_DATA			&iv,		// referent is optional
	SecNssCoder			&coder)		// for mallocing KeyData
{
	memset(&key, 0, sizeof(CSSM_KEY));
	unsigned keyBytes = (keySizeInBits + 7) / 8;
	coder.allocItem(key.KeyData, keyBytes);
	CSSM_RETURN crtn = p12PbeGen_app(pwd, 
		salt.Data, salt.Length,
		iterCount, 
		isForEncr ? PBE_ID_Key : PBE_ID_Mac,
		pbeHashAlg,
		cspHand,
		(unsigned char *)key.KeyData.Data,
		key.KeyData.Length);
	if(crtn) {
		cuPrintError("p12PbeGen(key)", crtn);
		return crtn;
	}
	
	/* fill in the blanks */
	CSSM_KEYHEADER &hdr = key.KeyHeader;
	hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
	/* CspId blank */
	hdr.BlobType = CSSM_KEYBLOB_RAW;
	hdr.AlgorithmId = keyAlg;
	hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
	hdr.KeyClass = CSSM_KEYCLASS_SESSION_KEY;
	hdr.KeyUsage = CSSM_KEYUSE_ANY;
	/* start/end date unknown, leave zero */
	hdr.WrapAlgorithmId = CSSM_ALGID_NONE;
	hdr.WrapMode = CSSM_ALGMODE_NONE;
	hdr.LogicalKeySizeInBits = keySizeInBits;
	
	/* P12 style IV derivation, optional */
	if(iv.Data != NULL) {
		crtn = p12PbeGen_app(pwd,
			salt.Data, salt.Length,
			iterCount, 
			PBE_ID_IV,
			pbeHashAlg,
			cspHand,
			iv.Data, iv.Length);
		if(crtn) {
			cuPrintError("p12PbeGen (IV)", crtn);
			return crtn;
		}
	}

	return CSSM_OK;
}

/*
 * Decrypt (typically, an encrypted P7 ContentInfo contents or
 * a P12 ShroudedKeyBag).
 */
CSSM_RETURN p12Decrypt_app(
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
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	SecNssCoder			&coder,		// for mallocing KeyData and plainText
	CSSM_DATA			&plainText)
{
	CSSM_RETURN crtn;
	CSSM_KEY ckey;
	CSSM_CC_HANDLE ccHand = 0;
	
	/* P12 style IV derivation, optional */
	CSSM_DATA iv = {0, NULL};
	CSSM_DATA_PTR ivPtr = NULL;
	if(blockSizeInBytes) {
		coder.allocItem(iv, blockSizeInBytes);
		ivPtr = &iv;
	}
	
	/* P12 style key derivation */
	crtn = p12KeyGen_app(cspHand, ckey, true, keyAlg, pbeHashAlg,
		keySizeInBits, iterCount, salt, pwd, iv, coder);
	if(crtn) {
		return crtn;
	}	
		
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
		return crtn;
	}
	
	/* go - CSP mallocs ptext and rem data */
	CSSM_DATA ourPtext = {0, NULL};
	CSSM_DATA remData = {0, NULL};
	uint32 bytesDecrypted;
	crtn = CSSM_DecryptData(ccHand,
		&cipherText,
		1,
		&ourPtext,
		1,
		&bytesDecrypted,
		&remData);
	if(crtn) {
		cuPrintError("CSSM_EncryptData", crtn);
	}
	else {
		coder.allocCopyItem(ourPtext, plainText);
		plainText.Length = bytesDecrypted;
		
		/* plaintext copied into coder space; free the memory allocated
		 * by the CSP */
		appFreeCssmMemory(cspHand, ourPtext.Data);
	}
	/* an artifact of CSPFUllPLuginSession - this never contains
	 * valid data but sometimes gets mallocds */
	if(remData.Data) {
		appFreeCssmMemory(cspHand, remData.Data);
	}
	CSSM_DeleteContext(ccHand);
	return crtn;
}

/*
 * Calculate the MAC for a PFX. Caller is either going compare
 * the result against an existing PFX's MAC or drop the result into 
 * a newly created PFX.
 */
CSSM_RETURN p12GenMac_app(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		&ptext,	// e.g., NSS_P12_DecodedPFX.derAuthSaafe
	CSSM_ALGORITHMS		alg,	// better be SHA1!
	unsigned			iterCount,
	const CSSM_DATA		&salt,
	const CSSM_DATA		&pwd,		// unicode, double null terminated
	SecNssCoder			&coder,		// for mallocing macData
	CSSM_DATA			&macData)	// RETURNED 
{
	CSSM_RETURN crtn;
	
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
	crtn = p12KeyGen_app(cspHand, macKey, false, hmacAlg, alg,
		keySizeInBits, iterCount, salt, pwd, iv, coder);
	if(crtn) {
		return crtn;
	}	

	/* prealloc the mac data */
	coder.allocItem(macData, keySizeInBits / 8);
	CSSM_CC_HANDLE ccHand = 0;
	crtn = CSSM_CSP_CreateMacContext(cspHand, hmacAlg, &macKey, &ccHand);
	if(crtn) {
		cuPrintError("CSSM_CSP_CreateMacContext", crtn);
		return crtn;
	}
	
	crtn = CSSM_GenerateMac (ccHand, &ptext, 1, &macData);
	if(crtn) {
		cuPrintError("CSSM_GenerateMac", crtn);
	}
	CSSM_DeleteContext(ccHand);
	return crtn;
}

/*
 * Verify MAC on an existing PFX.  
 */
CSSM_RETURN p12VerifyMac_app(
	const NSS_P12_DecodedPFX 	&pfx,
	CSSM_CSP_HANDLE				cspHand,
	const CSSM_DATA				&pwd,	// unicode, double null terminated
	SecNssCoder					&coder)	// for temp mallocs
{
	if(pfx.macData == NULL) {
		return CSSMERR_CSP_INVALID_SIGNATURE;
	}
	NSS_P12_MacData &macData = *pfx.macData;
	NSS_P7_DigestInfo &digestInfo  = macData.mac;
	CSSM_OID &algOid = digestInfo.digestAlgorithm.algorithm;
	CSSM_ALGORITHMS macAlg;
	if(!cssmOidToAlg(&algOid, &macAlg)) {
		return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	uint32 iterCount = 0;
	CSSM_DATA &citer = macData.iterations;
	if(!p12DataToInt(citer, iterCount)) {
		return CSSMERR_CSP_INVALID_ATTR_ROUNDS;
	}
	if(iterCount == 0) {
		/* optional, default 1 */
		iterCount = 1;
	}

	/*
	 * In classic fashion, the PKCS12 spec now says:
	 *
	 *      When password integrity mode is used to secure a PFX PDU, 
	 *      an SHA-1 HMAC is computed on the BER-encoding of the contents 
	 *      of the content field of the authSafe field in the PFX PDU.
	 *
	 * So here we go.
	 */
	CSSM_DATA genMac;
	CSSM_RETURN crtn = p12GenMac_app(cspHand, *pfx.authSafe.content.data, 
		macAlg, iterCount, macData.macSalt, pwd, coder, genMac);
	if(crtn) {
		return crtn;
	}
	if(nssCompareCssmData(&genMac, &digestInfo.digest)) {
		return CSSM_OK;
	}
	else {
		return CSSMERR_CSP_VERIFY_FAILED;
	}
}

