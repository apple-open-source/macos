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
 	File:		deriveKey.cpp
	
 	Contains:	CSSM_DeriveKey functions

 	Copyright:	(C) 2000 by Apple Computer, Inc., all rights reserved

 	Written by:	Doug Mitchell <dmitch@apple.com>	
*/

#include <PBKDF2/HMACSHA1.h>
#include <PBKDF2/pbkdf2.h>
#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"
#include "cspdebugging.h"
#include <Security/context.h>
#include <Security/utilities.h>

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
	
	uint32	passphraseLen = pbkdf2Params->Passphrase.Length;
	uint8 	*passphrase = pbkdf2Params->Passphrase.Data;
	
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
	uint32 tempLen = salt.Length + 4;
	if(tempLen < kSHA1DigestSize) {
		tempLen = kSHA1DigestSize;
	}
	tempLen += (2 * kSHA1DigestSize);
	CSSM_DATA tempData = {0, NULL};
	setUpData(tempData, tempLen, privAllocator);
	
	/* go */
	pbkdf2 (hmacsha1, 
		kSHA1DigestSize,
		passphrase, passphraseLen,
		salt.Data, salt.Length,
		iterCount,
		keyData->Data, keyData->Length,
		tempData.Data);
	freeData(&tempData, privAllocator, false);
}

/*
 * Member function initially declared for CSPAbstractPluginSession;
 * we're overriding the null version in CSPFullPluginSession.
 *
 * Currently we only support one derive key algorithm - 
 * CSSM_ALGID_PKCS5_PBKDF2, with PRF CSSM_PKCS5_PBKDF2_PRF_HMAC_SHA1 
 * PRF. We'll generate any type of key (for now). 
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
	hdr.LogicalKeySizeInBits = reqKeySize;
	
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

