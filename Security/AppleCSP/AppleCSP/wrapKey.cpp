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

#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"
#ifdef USE_SNACC
#include "pkcs_7_8.h"
#endif
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
 *	   Private keys will be PKCS8 encoded; session keys will be 
 *     PKCS7 encoded. Both input keys may be in raw or reference 
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
 * Default wrapping if none specified:
 *
 * UnwrappedKey type   WrappingKey type 	Format
 * -----------------   ----------------		-------------------------
 * 3DES		   		   3DES					CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM
 * any				   Other symmetric		CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7
 * any				   Other asymmetric		CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8
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
	try {
		/* wrapping key only required for non-NULL wrap */
		CssmKey &wrappingKeyRef = 
			Context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, 
			CSSMERR_CSP_MISSING_ATTR_KEY);
		wrappingKey = &wrappingKeyRef;
	}
	catch (const CssmError err) {
		if((err.error == CSSMERR_CSP_MISSING_ATTR_KEY) &&
		   (Context.algorithm() == CSSM_ALGID_NONE) &&
		   (Context.type() == CSSM_ALGCLASS_SYMMETRIC)) {
				// NULL wrap, OK
				isNullWrap = true;
		}
		else {
			errorLog0("WrapKey: missing wrapping key\n");
			throw;
		}
	}
	catch (...) {
		throw;
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
		if(UnwrappedKey.keyClass() == CSSM_KEYCLASS_PUBLIC_KEY) {
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
		}
		cspValidateIntendedKeyUsage(&wrappingKey->KeyHeader, CSSM_KEYUSE_WRAP);

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
			/* figure out a default */
			if(wrapType == CSSM_ALGCLASS_ASYMMETRIC) {
				/* easy */
#ifdef USE_SNACC
				wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8;
#else
				wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM; 
#endif
			}
			else {
				CASSERT(wrapType == CSSM_ALGCLASS_SYMMETRIC);
				if((wrappingKey->algorithm() == CSSM_ALGID_3DES_3KEY) &&
				   (UnwrappedKey.algorithm() == CSSM_ALGID_3DES_3KEY)) {
					/* apple custom CMS */
					wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM; 
				}
				else {
					/* normal case for symmetric wrapping keys */
#ifdef USE_SNACC
					wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
#else
					wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM; 
#endif
				}
			}	/* default for symmetric wrapping key */
		}		/* no format present or FORMAT_NONE */
	}
	
	/* make sure we have a valid format here */
	switch(wrapFormat) {
#if 0
		case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7:
		case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8:
#endif
		case CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM:
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
	
	switch(UnwrappedKey.blobType()) {
		case CSSM_KEYBLOB_RAW:
			/* trivial case */
			rawBlob = CssmData::overlay(UnwrappedKey.KeyData);
			rawFormat = UnwrappedKey.blobFormat();
			break;
		case CSSM_KEYBLOB_REFERENCE:
			/* get binary key, then get blob from it */
			{
				BinaryKey &binKey = lookupRefKey(UnwrappedKey);
				/*
				 * Special case for null wrap - prevent caller from obtaining 
				 * clear bits if CSSM_KEYATTR_SENSITIVE or !CSSM_KEYATTR_EXTRACTABLE.
				 * Don't trust the caller's header; use the one in the BinaryKey.
				 */
				if(isNullWrap) {
					CSSM_KEYATTR_FLAGS keyAttr = binKey.mKeyHeader.KeyAttr;
					if((keyAttr & CSSM_KEYATTR_SENSITIVE) ||
					   !(keyAttr & CSSM_KEYATTR_EXTRACTABLE)) {
						CssmError::throwMe(
							CSSMERR_CSP_INVALID_KEYATTR_MASK);
					}
				}
				rawFormat = requestedKeyFormat(Context, UnwrappedKey);
				binKey.generateKeyBlob(privAllocator,
					rawBlob,
					rawFormat);
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
	copyCssmHeader(UnwrappedKey.header(), wrappedHdr, normAllocator);
	wrappedHdr.WrapAlgorithmId = Context.algorithm(); 	// true for null 
														// and non-Null 
	wrappedHdr.Format = wrapFormat;
	
	/* 
	 * special case - break out here for custom Apple CMS  
	 */
	if(!isNullWrap && (wrapFormat == CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM)) {
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
	}

	
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
			wrappedHdr.BlobType = CSSM_KEYBLOB_RAW;
			wrappedHdr.Format   = rawFormat; 
		}
#ifdef USE_SNACC
		else {
			/* encrypt rawBlob using caller's context, then encode to
			 * WrappedKey.KeyData */
			uint32 bytesEncrypted;
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
			if(wrapFormat == CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7) {
				cspEncodePkcs7(Context.algorithm(),
					Context.getInt(CSSM_ATTRIBUTE_MODE),
					encryptedBlob,
					CssmData::overlay(WrappedKey.KeyData), 
					normAllocator);
			}
			else {
				CASSERT(wrapFormat == CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8);
				cspEncodePkcs8(Context.algorithm(),
					Context.getInt(CSSM_ATTRIBUTE_MODE),
					encryptedBlob,
					CssmData::overlay(WrappedKey.KeyData),
					normAllocator);
			}
			wrappedHdr.BlobType = CSSM_KEYBLOB_WRAPPED;
			// OK to be zero or not present 
			wrappedHdr.WrapMode = Context.getInt(
				CSSM_ATTRIBUTE_MODE);
		}
#endif
	}
	catch (...) {
		errorLog0("WrapKey: EncryptData() threw exception\n");
		if(allocdRawBlob) {
			freeCssmData(rawBlob, privAllocator);
		}
		/* mallocd in EncryptData, thus normAllocator */
		freeCssmData(encryptedBlob, normAllocator);
		freeCssmData(remData,normAllocator);
		throw;
	}
	if(allocdRawBlob) {
		freeCssmData(rawBlob, privAllocator);
	}
	freeCssmData(encryptedBlob, normAllocator);
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
	try {
		CssmKey &unwrappingKeyRef = 
			Context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, 
			CSSMERR_CSP_MISSING_ATTR_KEY);
		unwrappingKey = &unwrappingKeyRef;
	}
	catch (const CssmError err) {
		if((err.error == CSSMERR_CSP_MISSING_ATTR_KEY) &&
		   (Context.algorithm() == CSSM_ALGID_NONE) &&
		   (Context.type() == CSSM_ALGCLASS_SYMMETRIC)) {
				// NULL unwrap, OK
				isNullUnwrap = true;
		}
		else {
			errorLog0("UnwrapKey: missing wrapping key\n");
			throw;
		}
	}
	catch (...) {
		throw;
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
	}

	/* validate WrappedKey */
	switch(WrappedKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			if(!isNullUnwrap) {
				errorLog0("UnwrapKey: unwrap of public key illegal\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
			}
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
	copyCssmHeader(WrappedKey.header(), unwrappedHdr, normAllocator);
	unwrappedHdr.WrapAlgorithmId = Context.algorithm(); // true for null 
														// and non-Null 
	/* GUID must be appropriate */
	unwrappedHdr.CspId = plugin.myGuid();

	UnwrappedKey.KeyData.Data = NULL;	// ignore possible incoming KeyData
	UnwrappedKey.KeyData.Length = 0;
	
	/* validate wrappedKey format */
	if(!isNullUnwrap) {
		switch(wrapFormat) {
#ifdef USE_SNACC
			case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7:
			case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8:
				break;
#endif
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
#ifdef USE_SNACC
		else {
			/* decode wrapped blob, then decrypt to UnwrappedKey.KeyData
			 * using caller's context */
			CSSM_KEYBLOB_FORMAT rawFormat;
			if(wrapFormat == CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7) {
				cspDecodePkcs7(WrappedKey,
					decodedBlob,
					rawFormat,
					normAllocator);
			}
			else {
				cspDecodePkcs8(WrappedKey,
					decodedBlob, 
					rawFormat,
					normAllocator);
			}
			uint32 bytesDecrypted;		
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
			unwrappedHdr.Format   = rawFormat;
		}
#endif
	}
	catch (...) {
		errorLog0("UnwrapKey: DecryptData() threw exception\n");
		freeCssmData(decodedBlob, normAllocator);
		freeCssmData(remData, normAllocator);
		throw;
	}
	freeCssmData(decodedBlob, normAllocator);
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
		provider->CssmKeyToBinary(&binKey);
		addRefKey(*binKey, UnwrappedKey);
		delete provider;
	}
}

