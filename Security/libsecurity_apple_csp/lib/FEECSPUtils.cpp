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
 * FEECSPUtils.h - Misc. utility function for FEE/CryptKit CSP. 
 *
 * Created 2/20/2001 by dmitch.
 */
 
#ifdef	CRYPTKIT_CSP_ENABLE

#include <security_utilities/debugging.h>
#include <security_utilities/logging.h>
#include "FEECSPUtils.h"
#include "FEEKeys.h"
#include <security_cryptkit/feeFunctions.h>
#include <security_cryptkit/feePublicKey.h>

#define feeMiscDebug(args...)	secdebug("feeMisc", ## args)

/* Given a FEE error, throw appropriate CssmError */
void CryptKit::throwCryptKit(
	feeReturn 	frtn, 
	const char	*op)		/* optional */
{
	if(op) {
		Security::Syslog::error("Apple CSP %s: %s", op, feeReturnString(frtn));
	}
	switch(frtn) {
		case FR_Success:
			return;
		case FR_BadPubKey:
		case FR_BadPubKeyString:
		case FR_IncompatibleKey:
		case FR_BadKeyBlob:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
		case FR_IllegalDepth:
			CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_SIZE);
		case FR_BadSignatureFormat:		/* signature corrupted */
			CssmError::throwMe(CSSMERR_CSP_INVALID_SIGNATURE);
		case FR_InvalidSignature:		/* signature intact, but not valid */
			CssmError::throwMe(CSSMERR_CSP_VERIFY_FAILED);
		case FR_IllegalArg:			/* illegal argument */
			CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);
		case FR_BadCipherText:		/* malformed ciphertext */
		case FR_BadEnc64:			/* bad enc64() format */
			CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
		case FR_Unimplemented:		/* unimplemented function */
			CssmError::throwMe(CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED);
		case FR_Memory:		/* unimplemented function */
			CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
		case FR_ShortPrivData:
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_SEED);
		case FR_IllegalCurve:		/* e.g., ECDSA with Montgomery curve */
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);

		/* I don't think we should ever see these no matter what the 
		 * caller throws at us */
		case FR_WrongSignatureType:	/* ElGamal vs. ECDSA */
		case FR_BadUsageName:		/* bad usageName */
		case FR_BadCipherFile:
		case FR_Internal:			/* internal library error */
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
}

/* 
 * Given a Context:
 * -- obtain CSSM key of specified CSSM_ATTRIBUTE_TYPE
 * -- validate keyClass
 * -- validate keyUsage
 * -- convert to feePubKey, allocating the feePubKey if necessary
 *
 * Returned key can be of algorithm CSSM_ALGID_ECDSA or CSSM_ALGID_FEE; 
 * caller has to verify proper algorithm for operation.
 */
feePubKey CryptKit::contextToFeeKey(
	const Context 		&context,
	AppleCSPSession	 	&session,
	CSSM_ATTRIBUTE_TYPE	attrType,	  // CSSM_ATTRIBUTE_KEY, CSSM_ATTRIBUTE_PUBLIC_KEY
	CSSM_KEYCLASS		keyClass,	  // CSSM_KEYCLASS_{PUBLIC,PRIVATE}_KEY
	CSSM_KEYUSE			usage,		  // CSSM_KEYUSE_ENCRYPT, CSSM_KEYUSE_SIGN, etc.
	bool				&mallocdKey)  // RETURNED
{
    CssmKey &cssmKey = 
		context.get<CssmKey>(attrType, CSSMERR_CSP_MISSING_ATTR_KEY);
	const CSSM_KEYHEADER &hdr = cssmKey.KeyHeader;
	switch(hdr.AlgorithmId) {
		case CSSM_ALGID_FEE:
		case CSSM_ALGID_ECDSA:
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_ALGID_MISMATCH);
	}
	if(hdr.KeyClass != keyClass) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	cspValidateIntendedKeyUsage(&hdr, usage);
	cspVerifyKeyTimes(hdr);
	return cssmKeyToFee(cssmKey, session, mallocdKey);
}

/* 
 * Convert a CssmKey to a feePubKey. May result in the creation of a new
 * feePubKey (when cssmKey is a raw key); allocdKey is true in that case
 * in which case the caller generally has to free the allocd key).
 */
feePubKey CryptKit::cssmKeyToFee(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	bool			&allocdKey)	// RETURNED
{
	feePubKey feeKey = NULL;
	allocdKey = false;
	
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	switch(hdr->AlgorithmId) {
		case CSSM_ALGID_FEE:
		case CSSM_ALGID_ECDSA:
			break;
		default:
			// someone else's key (should never happen)
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	switch(hdr->BlobType) {
		case CSSM_KEYBLOB_RAW:
			feeKey = rawCssmKeyToFee(cssmKey);
			allocdKey = true;
			break;
		case CSSM_KEYBLOB_REFERENCE:
		{
			BinaryKey &binKey = session.lookupRefKey(cssmKey);
			FEEBinaryKey *feeBinKey = dynamic_cast<FEEBinaryKey *>(&binKey);
			/* this cast failing means that this is some other
			 * kind of binary key */
			if(feeBinKey == NULL) {
				feeMiscDebug("CryptKit::cssmKeyToFee: wrong BinaryKey subclass\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			assert(feeBinKey->feeKey() != NULL);
			feeKey = feeBinKey->feeKey();
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
	}
	return feeKey;
}

/* 
 * Convert a raw CssmKey to a newly alloc'd feePubKey.
 */
feePubKey CryptKit::rawCssmKeyToFee(
	const CssmKey	&cssmKey)
{
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	
	switch(hdr->AlgorithmId) {
		case CSSM_ALGID_FEE:
		case CSSM_ALGID_ECDSA:
			break;
		default:
			// someone else's key (should never happen)
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		case CSSM_KEYCLASS_PRIVATE_KEY:
			break;
		default:
			// someone else's key
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	
	feePubKey feeKey = feePubKeyAlloc();
	if(feeKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	
	feeReturn frtn = FR_IllegalArg;
	bool badFormat = false;
				
	/*
	 * The actual key init depends on key type and incoming format
	 */
	switch(hdr->AlgorithmId) {
		case CSSM_ALGID_FEE:
			switch(hdr->KeyClass) {
				case CSSM_KEYCLASS_PUBLIC_KEY:
					switch(hdr->Format) {
						case FEE_KEYBLOB_DEFAULT_FORMAT:
							/* FEE, public key, default: custom DER */
							frtn = feePubKeyInitFromDERPubBlob(feeKey,
								cssmKey.KeyData.Data,
								cssmKey.KeyData.Length);
							break;
						case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
							/* FEE, public key, native byte stream */
							frtn = feePubKeyInitFromPubBlob(feeKey,
								cssmKey.KeyData.Data,
								(unsigned int)cssmKey.KeyData.Length);
							break;
						default:
							badFormat = true;
							break;
					}
					break;
				case CSSM_KEYCLASS_PRIVATE_KEY:
					switch(hdr->Format) {
						case FEE_KEYBLOB_DEFAULT_FORMAT:
							/* FEE, private key, default: custom DER */
							frtn = feePubKeyInitFromDERPrivBlob(feeKey,
								cssmKey.KeyData.Data,
								cssmKey.KeyData.Length);
							break;
						case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
							/* FEE, private key, native byte stream */
							frtn = feePubKeyInitFromPrivBlob(feeKey,
								cssmKey.KeyData.Data,
								(unsigned int)cssmKey.KeyData.Length);
							break;
						default:
							badFormat = true;
							break;
					}
					break;
				default:
					/* not reached, we already checked */
					break;
			}
			/* end of case ALGID_FEE */
			break;
			
		case CSSM_ALGID_ECDSA:
			switch(hdr->KeyClass) {
				case CSSM_KEYCLASS_PUBLIC_KEY:
					switch(hdr->Format) {
						case CSSM_KEYBLOB_RAW_FORMAT_NONE:
						case CSSM_KEYBLOB_RAW_FORMAT_X509:
							/* ECDSA, public key, default: X509 */
							frtn = feePubKeyInitFromX509Blob(feeKey,
								cssmKey.KeyData.Data,
								cssmKey.KeyData.Length);
							break;
							
						case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:
							/*
							 * An oddity here: we can parse this incoming key, but 
							 * it contains both private and public parts. We throw
							 * out the private component here.
							 */
							frtn = feePubKeyInitFromOpenSSLBlob(feeKey, 
								1,		/* pubOnly */
								cssmKey.KeyData.Data,
								cssmKey.KeyData.Length);
							break;
						/* 
						 * NOTE: we cannot *import* a key in raw X9.62 format.
						 * We'd need to know the curve, i.e., the feeDepth.
						 * I suppose we could infer that from the blob length but
						 * a better way would be to have a new context attribute
						 * specifying which curve.
						 * For now, imported raw keys have to be in X509 format.
						 */
						case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
						default:
							badFormat = true;
							break;
					}
					break;
				case CSSM_KEYCLASS_PRIVATE_KEY:
					switch(hdr->Format) {
						case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
							/* ECDSA, private key, PKCS8 */
							frtn = feePubKeyInitFromPKCS8Blob(feeKey,
								cssmKey.KeyData.Data,
								cssmKey.KeyData.Length);
							break;
							
						case CSSM_KEYBLOB_RAW_FORMAT_NONE:
						case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:
							/* ECDSA, private, default: OpenSSL */
							/* see comment above re: OpenSSL public/private keys */
							frtn = feePubKeyInitFromOpenSSLBlob(feeKey, 
								0,		/* pubOnly */
								cssmKey.KeyData.Data,
								cssmKey.KeyData.Length);
							break;
						/* see comment above about X9.62 format public key blobs */
						case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
						default:
							badFormat = true;
							break;
					}
					break;
				default:
					/* not reached, we already checked */
					break;
			}
			/* end of case CSSM_ALGID_ECDSA */
			break;
	}
	if(badFormat) {
		CssmError::throwMe(hdr->KeyClass == CSSM_KEYCLASS_PRIVATE_KEY ?
			CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT :
			CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
	}
	if(frtn) {
		feePubKeyFree(feeKey);
		throwCryptKit(frtn, "feePubKeyInitFromKeyBlob");
	}
	return feeKey;
}

/*
 * Glue function which allows C code to use AppleCSPSession 
 * as an RNG. A ptr to this function gets passed down to 
 * CryptKit C functions as a feeRandFcn.
 */
feeReturn CryptKit::feeRandCallback(
	void *ref,					// actually an AppleCSPSession *
	unsigned char *bytes,		// must be alloc'd by caller 
	unsigned numBytes)
{
	AppleCSPSession *session = 
		reinterpret_cast<AppleCSPSession *>(ref);
	try {
		session->getRandomBytes(numBytes, bytes);
	}
	catch(...) {
		return FR_Internal;
	}
	return FR_Success;
}

#endif	/* CRYPTKIT_CSP_ENABLE */
