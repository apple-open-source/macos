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


//
// wrapKey.cpp - wrap/unwrap key functions for AppleCSPSession
//

/*
 * Currently the Security Server wraps public keys when they're stored, so we have
 * to allow this. We might not want to do this in the real world. 
 */
#define ALLOW_PUB_KEY_WRAP		1

#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"
#include "AppleCSPKeys.h"
#include "pkcs8.h"
#include "cspdebugging.h"

/*
 * Wrap key function. Used for two things:
 *
 *  -- Encrypt and encode a private or session key for export to
 *     a foreign system or program. Any type of keys may be used
 *     for the  unwrapped key and the wrapping (encrypting) key,
 *     as long as this CSP understands those keys. The context
 *     must be of  class ALGCLASS_SYMMETRIC or ALGCLASS_ASYMMETRIC,
 *     matching the wrapping key. 
 *
 *	   In the absence of an explicit CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT
 *     attribute, private keys will be PKCS8 wrapped; session keys will be 
 *     PKCS7 wrapped. Both input keys may be in raw or reference 
 *     format. Wrapped key will have BlobType CSSM_KEYBLOB_WRAPPED.
 * 
 *  -- Convert a reference key to a RAW key (with no encrypting).
 *     This is called a NULL wrap; no wrapping key need be present in
 *     the context, but the context must be of class 
 *	   ALGCLASS_SYMMETRIC and algorithm ALGID_NONE. 
 *
 * There are serious inconsistencies in the specification of wrap 
 * algorithms to be used in the various CDSA specs (c914.pdf, 
 * CSP Behavior spec) and between those specs and the PKCS standards
 * PKCS7, PKCS8, RFC2630). Here is what this module implements:
 *
 * On a wrap key op, the caller can add a CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT
 * attribute to the context to specify the wrapping algorithm to be used.
 * If it's there, that's what we use if appropriate for the incoming key
 * types. Otherwise we figure out a reasonable default from the incoming
 * key types. The wrapped key always has the appropriate KeyHeader.Format
 * field set indicating how it was wrapped. Defaults are shows below. 
 *
 * The format CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM is used to indicate
 * a modified CMS-style wrapping which is similar to that specified in
 * RFC2630, with some modification. 
 *
 * Default wrapping if none specified based on ther unwrapped key as 
 * follows:
 *
 * UnwrappedKey			Wrap format
 * ------------			-----------
 * Symmetric 			PKCS7
 * Public				APPLE_CUSTOM
 * FEE private			APPLE_CUSTOM
 * Other private		PKCS8
 */
 
void AppleCSPSession::WrapKey(
		CSSM_CC_HANDLE CCHandle,
		const Context &Context,
        const AccessCredentials &AccessCred,
        const CssmKey &UnwrappedKey,
        const CssmData *DescriptiveData,
        CssmKey &WrappedKey,
		CSSM_PRIVILEGE Privilege)
{
	CssmKey::Header 		&wrappedHdr   = WrappedKey.header();
	bool 					isNullWrap = false;
	CssmKey					*wrappingKey = NULL;
	CSSM_KEYBLOB_FORMAT		wrapFormat;
	
	switch(UnwrappedKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		case CSSM_KEYCLASS_PRIVATE_KEY:
		case CSSM_KEYCLASS_SESSION_KEY:
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}

	/* wrapping key only required for non-NULL wrap */
	wrappingKey = Context.get<CssmKey>(CSSM_ATTRIBUTE_KEY);
	if(wrappingKey == NULL) {
		if((Context.algorithm() == CSSM_ALGID_NONE) &&
		   (Context.type() == CSSM_ALGCLASS_SYMMETRIC)) {
				// NULL wrap, OK
				isNullWrap = true;
		}
		else {
			errorLog0("WrapKey: missing wrapping key\n");
			CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_KEY);
		}
	}
	
	/*
	 * Validate misc. params as best we can
	 */
	if(isNullWrap) {
		wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_NONE;
	}
	else {
		/*
		 * Can only wrap session and private keys. 
		 */
		#if		!ALLOW_PUB_KEY_WRAP
		if(UnwrappedKey.keyClass() == CSSM_KEYCLASS_PUBLIC_KEY) {
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
		}
		#endif	/* ALLOW_PUB_KEY_WRAP */
		cspValidateIntendedKeyUsage(&wrappingKey->KeyHeader, CSSM_KEYUSE_WRAP);
		cspVerifyKeyTimes(wrappingKey->KeyHeader);
		
		/*
		 * make sure wrapping key type matches context
		 */
		CSSM_CONTEXT_TYPE wrapType;
		switch(wrappingKey->KeyHeader.KeyClass) {
			case CSSM_KEYCLASS_PUBLIC_KEY:
			case CSSM_KEYCLASS_PRIVATE_KEY:
				wrapType = CSSM_ALGCLASS_ASYMMETRIC;
				break;
			case CSSM_KEYCLASS_SESSION_KEY:
				wrapType = CSSM_ALGCLASS_SYMMETRIC;
				break;
			default:
				errorLog0("WrapKey: bad class of wrappingKey\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
		}
		if(wrapType != Context.type()) {
			errorLog0("WrapKey: mismatch wrappingKey/contextType\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);
		}
		if(Context.algorithm() == CSSM_ALGID_NONE) {
			errorLog0("WrapKey: null wrap alg, non-null key\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
		}

		/*
		 * Get optional wrap format, set default per incoming keys
		 * Note: no such atrribute ==> 0 ==> FORMAT_NONE, which we
		 * take to mean "use the default".
		 */
		wrapFormat = Context.getInt(CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT);
		if(wrapFormat == CSSM_KEYBLOB_WRAPPED_FORMAT_NONE) {
			/* figure out a default based on unwrapped key */
			switch(UnwrappedKey.keyClass()) {
				case CSSM_KEYCLASS_SESSION_KEY:
					wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
					break;
				case CSSM_KEYCLASS_PUBLIC_KEY:
					wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM; 
					break;
				case CSSM_KEYCLASS_PRIVATE_KEY:
					switch(UnwrappedKey.algorithm()) {
						case CSSM_ALGID_FEE:
							wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM; 
							break;
						default:
							wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8; 
							break;
					}
					break;
				default:
					/* NOT REACHED - checked above */
					break;
			}
		}		/* no format present or FORMAT_NONE */
	}
	
	/* make sure we have a valid format here */
	switch(wrapFormat) {
		case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7:
			if(UnwrappedKey.keyClass() != CSSM_KEYCLASS_SESSION_KEY) {
				/* this wrapping style only for symmetric keys */
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}
			break;
		case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8:
		case CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSL:
			if(UnwrappedKey.keyClass() != CSSM_KEYCLASS_PRIVATE_KEY) {
				/* these wrapping styles only for private keys */
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}
			break;
		case CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM:
			/* no restrictions (well AES can't be the wrap alg but that will 
			 * be caught later */
			break;
		case CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1:
			/* RSA private key, reference format, only */
			if(UnwrappedKey.keyClass() != CSSM_KEYCLASS_PRIVATE_KEY) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}
			if(UnwrappedKey.algorithm() != CSSM_ALGID_RSA) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
			}
			if(UnwrappedKey.blobType() != CSSM_KEYBLOB_REFERENCE) {
				CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
			}
			break;
		case CSSM_KEYBLOB_WRAPPED_FORMAT_NONE:
			if(isNullWrap) {
				/* only time this is OK */
				break;
			}
			/* else fall thru */
		default:
			dprintf1("KeyWrap: invalid wrapFormat (%d)\n", (int)wrapFormat);
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_WRAPPED_KEY_FORMAT);
	}
	/* get the blob to be wrappped */
	CssmData rawBlob;
	bool allocdRawBlob = false;
	CSSM_KEYBLOB_FORMAT rawFormat;
	
	/* 
	 * Outgoing same as incoming unless a partial key is completed during 
	 * generateKeyBlob()
	 */
	const CssmKey::Header &unwrappedHdr = UnwrappedKey.header();
	CSSM_KEYATTR_FLAGS unwrappedKeyAttrFlags = unwrappedHdr.KeyAttr;
	
	switch(UnwrappedKey.blobType()) {
		case CSSM_KEYBLOB_RAW:
			/* 
			 * Trivial case - we already have the blob.
			 * This op - wrapping a raw key - is not supported for the 
			 * CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1 format since that doesn't
			 * operate on a key blob.
			 */
			rawBlob = CssmData::overlay(UnwrappedKey.KeyData);
			rawFormat = UnwrappedKey.blobFormat();
			break;
		case CSSM_KEYBLOB_REFERENCE:
			/* get binary key, then get blob from it */
			{
				BinaryKey &binKey = lookupRefKey(UnwrappedKey);
				
				/*
				 * Subsequent tests for extractability: don't trust the 
				 * caller's header; use the one in the BinaryKey.
				 */
				CSSM_KEYATTR_FLAGS keyAttr = binKey.mKeyHeader.KeyAttr;
				if(!(keyAttr & CSSM_KEYATTR_EXTRACTABLE)) {
					/* this key not extractable in any form */
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
				}
				
				/* 
				 * CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1: we're ready to roll; 
				 * all we need is the reference key.
				 */
				if(wrapFormat == CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1) {
					break;
				}
				
				/*
				 * Null wrap - prevent caller from obtaining 
				 * clear bits if CSSM_KEYATTR_SENSITIVE
				 */
				if(isNullWrap && (keyAttr & CSSM_KEYATTR_SENSITIVE)) {
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
				}

				/*
				 * Special case for PKCS8 and openssl: need to get blob of a specific
				 * algorithm-dependent format. Caller can override our 
				 * preference with a 
				 * CSSM_ATTRIBUTE_{PRIVATE,PUBLIC,SESSION}_KEY_FORMAT 
				 * context attribute. 
				 */
				rawFormat = requestedKeyFormat(Context, UnwrappedKey);
				if(rawFormat == CSSM_KEYBLOB_RAW_FORMAT_NONE) {
					CSSM_ALGORITHMS keyAlg = binKey.mKeyHeader.AlgorithmId;
					switch(wrapFormat) {
						case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8:
							rawFormat = pkcs8RawKeyFormat(keyAlg);
							break;
						case CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSL:
							rawFormat = opensslRawKeyFormat(keyAlg);
							break;
						default:
							/* punt and take default for key type */
							break;
					}
				}
	
				/* 
				 * DescriptiveData for encoding, currently only used for 
				 * SSH1 keys.
				 */
				if((DescriptiveData != NULL) && (DescriptiveData->Length != 0)) {
					binKey.descData(*DescriptiveData);
				}
				
				/* optional parameter-bearing key */
				CssmKey *paramKey = Context.get<CssmKey>(CSSM_ATTRIBUTE_PARAM_KEY);
				binKey.generateKeyBlob(privAllocator,
					rawBlob,
					rawFormat,
					*this,
					paramKey,
					unwrappedKeyAttrFlags);
			}
			allocdRawBlob = true;		// remember - we need to free
			break;
			
		default:
			errorLog0("WrapKey: bad unwrappedKey BlobType\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}

	/*
	 * Prepare outgoing header.
	 */
	setKeyHeader(wrappedHdr,
		plugin.myGuid(),
		unwrappedHdr.algorithm(),		// same as incoming 
		unwrappedHdr.keyClass(),		// same as incoming
		unwrappedKeyAttrFlags,
		unwrappedHdr.KeyUsage);
	wrappedHdr.LogicalKeySizeInBits = unwrappedHdr.LogicalKeySizeInBits;
	wrappedHdr.WrapAlgorithmId = Context.algorithm(); 	// true for null 
														// and non-Null 
	wrappedHdr.StartDate = unwrappedHdr.StartDate;
	wrappedHdr.EndDate = unwrappedHdr.EndDate;
	wrappedHdr.Format = wrapFormat;
	if(isNullWrap) {
		wrappedHdr.BlobType = CSSM_KEYBLOB_RAW;
	}
	else {
		wrappedHdr.BlobType = CSSM_KEYBLOB_WRAPPED;
	}
	
	/* 
	 * special cases - break out here for Apple Custom and OpenSSHv1  
	 */
	if(!isNullWrap) {
		switch(wrapFormat) {
			case CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM:
				try {
					WrapKeyCms(CCHandle,
						Context,
						AccessCred,
						UnwrappedKey,
						rawBlob,
						allocdRawBlob,
						DescriptiveData,
						WrappedKey,
						Privilege);
				}
				catch(...) {
					if(allocdRawBlob) {
						freeCssmData(rawBlob, privAllocator);
					}
					throw;
				}
				if(allocdRawBlob) {
					freeCssmData(rawBlob, privAllocator);
				}
				return;
			case CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1:
			{
				/*
				 * 1. We don't have to worry about allocdRawBlob since this 
				 *    operation only works on reference keys and we did not
				 *    obtain the raw blob from the BinaryKey. 
				 * 2. This is a redundant lookupRefKey, I know, but since
				 *    that returns a reference, it would just be too messy to have
				 *    the previous call be in the same scope as this.
				 */
				BinaryKey &binKey = lookupRefKey(UnwrappedKey);
				WrapKeyOpenSSH1(CCHandle,
					Context,
					AccessCred,
					binKey,
					rawBlob,
					allocdRawBlob,
					DescriptiveData,
					WrappedKey,
					Privilege);
				return;
			}
			default:
				/* proceed to encrypt blob */
				break;
		}
	}	/* !isNullWrap */

	
	/*
	 * Generate wrapped blob. Careful, we need to conditionally free
	 * rawBlob on error.
	 */
	CssmData encryptedBlob;
	CssmData remData;
	WrappedKey.KeyData.Data = NULL;		// ignore possible incoming KeyData
	WrappedKey.KeyData.Length = 0;
	
	try {
		if(isNullWrap) {
			/* copy raw blob to caller's wrappedKey */
			copyCssmData(rawBlob, 
				CssmData::overlay(WrappedKey.KeyData), 
				normAllocator);
			wrappedHdr.Format   = rawFormat; 
		}
		else {
			/* encrypt rawBlob using caller's context, then encode to
			 * WrappedKey.KeyData */
			CSSM_SIZE bytesEncrypted;
			EncryptData(CCHandle,
				Context,
				&rawBlob,			// ClearBufs[]
				1,					// ClearBufCount
				&encryptedBlob,		// CipherBufs[],
				1,					// CipherBufCount,
				bytesEncrypted,
				remData,
				Privilege);
	
			// I'm not 100% sure about this....
			assert(remData.Length == 0);
			encryptedBlob.Length = bytesEncrypted;
			WrappedKey.KeyData = encryptedBlob;
			wrappedHdr.BlobType = CSSM_KEYBLOB_WRAPPED;
			// OK to be zero or not present 
			wrappedHdr.WrapMode = Context.getInt(
				CSSM_ATTRIBUTE_MODE);
		}
	}
	catch (...) {
		errorLog0("WrapKey: EncryptData() threw exception\n");
		if(allocdRawBlob) {
			freeCssmData(rawBlob, privAllocator);
		}
		freeCssmData(remData,normAllocator);
		throw;
	}
	if(allocdRawBlob) {
		freeCssmData(rawBlob, privAllocator);
	}
	freeCssmData(remData, normAllocator);
}

/*
 * Unwrap key function. Used for:
 *
 * -- Given key of BlobType CSSM_KEYBLOB_WRAPPED, decode and decrypt
 *    it, yielding a key in either raw or reference format. Unwrapping
 *    key may be either raw or reference. The context must match
 *    the unwrapping key (ALGCLASS_SYMMETRIC  or ALGCLASS_ASYMMETRIC).
 *
 *	  Private keys are assumed to be PKCS8 encoded; session keys  
 *    are assumed to be PKCS7 encoded. 
 *
 * -- Convert a Raw key to a reference key (with no decrypting).
 *    This is called a NULL unwrap; no unwrapping key need be present in
 *    the context, but the context must be of class 
 *    ALGCLASS_SYMMETRIC and algorithm ALGID_NONE.
 */ 
void AppleCSPSession::UnwrapKey(
		CSSM_CC_HANDLE CCHandle,
		const Context &Context,
		const CssmKey *PublicKey,
		const CssmKey &WrappedKey,
		uint32 KeyUsage,
		uint32 KeyAttr,
		const CssmData *KeyLabel,
		const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
		CssmKey &UnwrappedKey,
		CssmData &DescriptiveData,
		CSSM_PRIVILEGE Privilege)
{
	bool 					isNullUnwrap = false;
	CssmKey					*unwrappingKey = NULL;
	cspKeyType				keyType;				// CKT_Public, etc. 
	CSSM_KEYBLOB_FORMAT		wrapFormat = WrappedKey.blobFormat();
	
	/* obtain unwrapping key if present */
	unwrappingKey = Context.get<CssmKey>(CSSM_ATTRIBUTE_KEY);
	if(unwrappingKey == NULL) {
		if((Context.algorithm() == CSSM_ALGID_NONE) &&
		   (Context.type() == CSSM_ALGCLASS_SYMMETRIC)) {
				// NULL unwrap, OK
				isNullUnwrap = true;
		}
		else {
			errorLog0("UnwrapKey: missing wrapping key\n");
			CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_KEY);
		}
	}

	/* 
	 * validate unwrappingKey 
	 */
	if(!isNullUnwrap) {
		/* make sure unwrapping key type matches context */
		CSSM_CONTEXT_TYPE unwrapType;
		switch(unwrappingKey->KeyHeader.KeyClass) {
			case CSSM_KEYCLASS_PUBLIC_KEY:
			case CSSM_KEYCLASS_PRIVATE_KEY:
				unwrapType = CSSM_ALGCLASS_ASYMMETRIC;
				break;
			case CSSM_KEYCLASS_SESSION_KEY:
				unwrapType = CSSM_ALGCLASS_SYMMETRIC;
				break;
			default:
				errorLog0("UnwrapKey: bad class of wrappingKey\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
		}
		if(unwrapType != Context.type()) {
			errorLog0("UnwrapKey: mismatch unwrappingKey/contextType\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);
		}
		if(Context.algorithm() == CSSM_ALGID_NONE) {
			errorLog0("UnwrapKey: null wrap alg, non-null key\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
		}
		cspValidateIntendedKeyUsage(&unwrappingKey->KeyHeader, CSSM_KEYUSE_UNWRAP);
		cspVerifyKeyTimes(unwrappingKey->KeyHeader);
	}

	/* validate WrappedKey */
	switch(WrappedKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			#if 	!ALLOW_PUB_KEY_WRAP
			if(!isNullUnwrap) {
				errorLog0("UnwrapKey: unwrap of public key illegal\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}
			#endif	/* ALLOW_PUB_KEY_WRAP */
			keyType = CKT_Public;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			keyType = CKT_Private;
			break;
		case CSSM_KEYCLASS_SESSION_KEY:
			keyType = CKT_Session;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	if(isNullUnwrap) {
		if(WrappedKey.blobType() != CSSM_KEYBLOB_RAW) {
			errorLog0("UnwrapKey: expected raw blobType\n");
			CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
		}
	}
	else {
		if(WrappedKey.blobType() != CSSM_KEYBLOB_WRAPPED) {
			errorLog0("UnwrapKey: expected wrapped blobType\n");
			CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
		}
	}

	/* validate requested storage and usage */
	cspKeyStorage keyStorage = cspParseKeyAttr(keyType, KeyAttr);
	switch(keyStorage) {
		case CKS_Ref:
		case CKS_Data:
			break;		// OK
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	}
	cspValidateKeyUsageBits(keyType,  KeyUsage);

	/* prepare outgoing header */
	CssmKey::Header &unwrappedHdr = UnwrappedKey.header();
	const CssmKey::Header &wrappedHdr   = WrappedKey.header();
	setKeyHeader(unwrappedHdr,
		plugin.myGuid(),
		wrappedHdr.algorithm(),		// same as incoming 
		wrappedHdr.keyClass(),		// same as incoming
		KeyAttr & ~KEY_ATTR_RETURN_MASK,
		KeyUsage);
	unwrappedHdr.LogicalKeySizeInBits = wrappedHdr.LogicalKeySizeInBits;
	unwrappedHdr.StartDate = wrappedHdr.StartDate;
	unwrappedHdr.EndDate = wrappedHdr.EndDate;
	UnwrappedKey.KeyData.Data = NULL;	// ignore possible incoming KeyData
	UnwrappedKey.KeyData.Length = 0;
	
	/* validate wrappedKey format */
	if(!isNullUnwrap) {
		switch(wrapFormat) {
			case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7:
				if(WrappedKey.keyClass() != CSSM_KEYCLASS_SESSION_KEY) {
					/* this unwrapping style only for symmetric keys */
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
				}
				break;
			case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8:
			case CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSL:
				if(WrappedKey.keyClass() != CSSM_KEYCLASS_PRIVATE_KEY) {
					/* these unwrapping styles only for private keys */
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
				}
				break;
			case CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM:
				UnwrapKeyCms(CCHandle,
						Context,
						WrappedKey,
						CredAndAclEntry,
						UnwrappedKey,
						DescriptiveData,
						Privilege,
						keyStorage);
				return;
			case CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1:
				/* RSA private key, unwrap to ref key only */
				if(WrappedKey.keyClass() != CSSM_KEYCLASS_PRIVATE_KEY) {
					CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
				}
				if(WrappedKey.algorithm() != CSSM_ALGID_RSA) {
					CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
				}
				if(keyStorage != CKS_Ref) {
					errorLog0("UNwrapKey: OPENSSH1 only wraps to reference key\n");
					CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
				}
				UnwrapKeyOpenSSH1(CCHandle,
						Context,
						WrappedKey,
						CredAndAclEntry,
						UnwrappedKey,
						DescriptiveData,
						Privilege,
						keyStorage);
				return;
			default:
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_WRAPPED_KEY_FORMAT);
		}
	}

	/* Get key blob, decoding and decrypting if necessary */
	CssmData decodedBlob;
	CssmData remData;
	try {
		if(isNullUnwrap) {
			/* simple copy of raw blob */
			copyData(WrappedKey.KeyData, 
				UnwrappedKey.KeyData, 
				normAllocator);
			unwrappedHdr.BlobType = CSSM_KEYBLOB_RAW;
			unwrappedHdr.Format   = wrapFormat; 
		}
		else {
			decodedBlob = CssmData::overlay(WrappedKey.KeyData);
			CSSM_SIZE bytesDecrypted;		
			CssmData *unwrapData = 
				CssmData::overlay(&UnwrappedKey.KeyData);
				
			DecryptData(CCHandle,
				Context,
				&decodedBlob,		// CipherBufs[],
				1,					// CipherBufCount,
				unwrapData,			// ClearBufs[]
				1,					// ClearBufCount
				bytesDecrypted,
				remData,
				Privilege);
	
			// I'm not 100% sure about this....
			assert(remData.Length == 0);
			UnwrappedKey.KeyData.Length = bytesDecrypted;
			unwrappedHdr.BlobType = CSSM_KEYBLOB_RAW;
			
			/* 
			 * Figure out various header fields from resulting blob
			 */
			switch(wrapFormat) {
				case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7:
					unwrappedHdr.Format = 
						CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
					if(unwrappedHdr.LogicalKeySizeInBits == 0) {
						unwrappedHdr.LogicalKeySizeInBits =
							(unsigned)(bytesDecrypted * 8);
					}
					/* app has to infer/know algorithm */
					break;
				case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8:
					pkcs8InferKeyHeader(UnwrappedKey);
					break;
				case CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSL:
					/* 
					 * App told us key algorithm (in WrappedKey).
					 * Infer format and key size. 
					 */
					opensslInferKeyHeader(UnwrappedKey);
					break;
			}
		}
	}
	catch (...) {
		errorLog0("UnwrapKey: DecryptData() threw exception\n");
		freeCssmData(remData, normAllocator);
		throw;
	}
	freeCssmData(remData, normAllocator);

	/* 
	 * One more thing: cook up a BinaryKey if caller wants a 
	 * reference key.
	 */
	if(keyStorage == CKS_Ref) {
		/*
		 * We have a key in raw format; convert to BinaryKey.
		 */
		BinaryKey *binKey = NULL;
		CSPKeyInfoProvider *provider = infoProvider(UnwrappedKey);
		/* optional parameter-bearing key */
		CssmKey *paramKey = Context.get<CssmKey>(CSSM_ATTRIBUTE_PARAM_KEY);
		provider->CssmKeyToBinary(paramKey, UnwrappedKey.KeyHeader.KeyAttr, &binKey);
		addRefKey(*binKey, UnwrappedKey);
		delete provider;
	}
}

