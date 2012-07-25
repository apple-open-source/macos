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
// AppleCSPUtils.cpp - CSP-wide utility functions
//

#include "AppleCSPUtils.h"
#include <Security/cssmerr.h>
#include <security_utilities/alloc.h>
#include <security_cdsa_utilities/cssmdates.h>
#include <string.h>
#include <FEECSPUtils.h>
#include <SHA1_MD5_Object.h>
#include "RSA_DSA_keys.h"
#include <syslog.h>

/*
 * Validate key attribute bits per specified key type.
 *
 * Used to check requested key attributes for new keys and for validating
 * incoming existing keys. For checking key attributes for new keys,
 * assumes that KEYATTR_RETURN_xxx bits have been checked elsewhere
 * and stripped off before coming here.
 */
void cspValidateKeyAttr(
	cspKeyType 	keyType,
	uint32 		keyAttr)
{
	uint32 sensitiveBit = (keyAttr & CSSM_KEYATTR_SENSITIVE)    ? 1 : 0;
	uint32 extractBit   = (keyAttr & CSSM_KEYATTR_EXTRACTABLE)  ? 1 : 0;

	/* first general CSP-wide checks */
	if(keyAttr & KEY_ATTR_RETURN_MASK) {
		//errorLog0(" KEY_ATTR_RETURN bits set\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	}
	if(keyAttr & CSSM_KEYATTR_PERMANENT) {
		//errorLog0(" PERMANENT bit not supported\n");
		CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK);
	}
	if(keyAttr & CSSM_KEYATTR_PRIVATE) {
		CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK);
	}
	/* Anything else? */
	
	/* now check per keyType */
	switch(keyType) {
		case CKT_Session:
			break;

		case CKT_Public:
			if(sensitiveBit || !extractBit) {
				//errorLog0("Public keys must be extractable in the clear\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
			}
			break;

		case CKT_Private:
			//if(!sensitiveBit) {
			//	errorLog0("Private keys must have KEYATTR_SENSITIVE\n");
			//	CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
			//}

			/*
			 * One more restriction - EXTRACTABLE - caller must check since
			 * that involves KEYUSE bits.
			 */
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	return;
}

/*
 * Perform sanity check of incoming key attribute bits for a given
 * key type, and return a cspKeyStorage value.
 *
 * Called from any routine which generates a new key. This specifically
 * excludes WrapKey().
 */
cspKeyStorage cspParseKeyAttr(
	cspKeyType 	keyType,
	uint32 		keyAttr)
{
	uint32 sensitiveBit = (keyAttr & CSSM_KEYATTR_SENSITIVE)    ? 1 : 0;
	uint32 rtnDataBit   = (keyAttr & CSSM_KEYATTR_RETURN_DATA)  ? 1 : 0;
	uint32 rtnRefBit    = (keyAttr & CSSM_KEYATTR_RETURN_REF)   ? 1 : 0;
	uint32 extractBit   = (keyAttr & CSSM_KEYATTR_EXTRACTABLE)  ? 1 : 0;

	cspKeyStorage rtn;

	/* first general CDSA-wide checks */
	if(keyAttr & (CSSM_KEYATTR_ALWAYS_SENSITIVE |
				  CSSM_KEYATTR_NEVER_EXTRACTABLE)) {
		//errorLog0("ALWAYS_SENSITIVE, NEVER_EXTRACTABLE illegal at SPI\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	}
	switch(keyAttr & KEY_ATTR_RETURN_MASK) {
		/* ensure only one bit is set */
		case CSSM_KEYATTR_RETURN_DATA:
			rtn = CKS_Data;
			break;
		case CSSM_KEYATTR_RETURN_REF:
			rtn = CKS_Ref;
			break;
		case CSSM_KEYATTR_RETURN_NONE:
			rtn = CKS_None;
			break;
		case CSSM_KEYATTR_RETURN_DEFAULT:
			/* CSP default */
			rtnRefBit = 1;
			rtn = CKS_Ref;
			break;
		default:
			//errorLog0("Multiple KEYATTR_RETURN bits set\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	}

	/* now CSP-wide checks for all key types */
	if(keyType != CKT_Session) {
		/* session keys modifiable, no others are */
		if(keyAttr & CSSM_KEYATTR_MODIFIABLE) {
			//errorLog0("CSSM_KEYATTR_MODIFIABLE not supported\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
		}
	}
	if(rtnDataBit) {
		if(!extractBit) {
			//errorLog0("RETURN_DATA and !EXTRACTABLE not supported\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
		}
		if(sensitiveBit) {
			//errorLog0("RETURN_DATA and SENSITIVE not supported\n");
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
		}
	}

	/* now check per keyType. We're ust checking for things specific
	 * to KEYATTR_RETURN_xxx; cspValidateKeyAttr will check other fields. */
	 #if 0
	 // nothing for now
	switch(keyType) {
		case CKT_Session:
			break;

		case MKT_Public:
			break;

		case MKT_Private:
			if(rtnDataBit) {
				errorLog0("Private keys must be generated by ref\n");
				goto errorOut;
			}
			/*
			 * One more restriction - EXTRACTABLE - caller must check since
			 * that involves KEYUSE bits.
			 */
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	#endif	// 0

	/* validate other common static attributes */
	cspValidateKeyAttr(keyType, (keyAttr & ~KEY_ATTR_RETURN_MASK));
	return rtn;
}


/* used in cspValidateKeyUsageBits() */
/*
 * This is a vestige from OS9/ASA. In the real world there are in fact certs with
 * keyUsage extensions which specify, e.g., verify and wrap. I think we'll just
 * have to ignore the old exclusivity rules. 
 */
#define IGNORE_KEYUSE_EXCLUSIVITY	1
#if		IGNORE_KEYUSE_EXCLUSIVITY
#define checkExclusiveUsage(ku, cb, ob, em)
#else
static void checkExclusiveUsage(
	uint32		keyUsage,		// requested usage word
	uint32		checkBits,		// if any of these are set
	uint32		otherBits,		// these are the only other bits which can be set
	const char	*errMsg)
{
	if(keyUsage & checkBits) {
		if(keyUsage & ~otherBits) {
			errorLog0((char *)errMsg);
			CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYUSAGE_MASK);
		}
	}
}
#endif	/* IGNORE_KEYUSE_EXCLUSIVITY */

/*
 * Validate key usage bits for specified key type.
 */
void cspValidateKeyUsageBits (
	cspKeyType	keyType,
	uint32		keyUsage)
{
	/* general restrictions */
	checkExclusiveUsage(keyUsage,
		CSSM_KEYUSE_ANY,
		CSSM_KEYUSE_ANY,
		"CSSM_KEYUSE_ANY overload");
	checkExclusiveUsage(keyUsage,
			CSSM_KEYUSE_DERIVE,
			CSSM_KEYUSE_DERIVE,
			"CSSM_KEYUSE_DERIVE overload\n");

	/* brute force per key type. */
	switch(keyType) {
		case CKT_Session:
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
					CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
					"session key usage: encrypt/decrypt overload\n");
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY |
						CSSM_KEYUSE_SIGN_RECOVER | CSSM_KEYUSE_VERIFY_RECOVER,
					CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY |
						CSSM_KEYUSE_SIGN_RECOVER | CSSM_KEYUSE_VERIFY_RECOVER,
					"session key usage: sign/verify overload\n");
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP,
					CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP,
					"session key usage: wrap/unwrap overload\n");
			break;

		case CKT_Public:
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_ENCRYPT,
					CSSM_KEYUSE_ENCRYPT,
					"public key usage: encrypt overload\n");
			if(keyUsage & CSSM_KEYUSE_DECRYPT) {
				errorLog0("public key usage: DECRYPT illegal\n");
				CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYUSAGE_MASK);
			}
			if(keyUsage & (CSSM_KEYUSE_SIGN | CSSM_KEYUSE_SIGN_RECOVER)) {
				errorLog0("public key usage: SIGN illegal\n");
				CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYUSAGE_MASK);
			}
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_VERIFY_RECOVER,
					CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_VERIFY_RECOVER,
					"public key usage: verify overload\n");
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_WRAP,
					CSSM_KEYUSE_WRAP,
					"public key usage: wrap overload\n");
			if(keyUsage & CSSM_KEYUSE_UNWRAP) {
				errorLog0("public key usage: UNWRAP illegal\n");
				CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYUSAGE_MASK);
			}
			break;

		case CKT_Private:
			if(keyUsage & CSSM_KEYUSE_ENCRYPT) {
				errorLog0("private key usage: ENCRYPT illegal\n");
				CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYUSAGE_MASK);
			}
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_DECRYPT,
					CSSM_KEYUSE_DECRYPT,
					"private key usage: decrypt overload\n");
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_SIGN | CSSM_KEYUSE_SIGN_RECOVER,
					CSSM_KEYUSE_SIGN | CSSM_KEYUSE_SIGN_RECOVER,
					"private key usage: sign overload\n");
			if(keyUsage & (CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_VERIFY_RECOVER)) {
				errorLog0("private key usage: VERIFY illegal\n");
				CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYUSAGE_MASK);
			}
			if(keyUsage & CSSM_KEYUSE_WRAP) {
				errorLog0("private key usage: WRAP illegal\n");
				CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEYUSAGE_MASK);
			}
			checkExclusiveUsage(keyUsage,
					CSSM_KEYUSE_UNWRAP,
					CSSM_KEYUSE_UNWRAP,
					"private key usage: unwrap overload\n");
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
}

/*
 * Validate existing key's usage bits against intended use.
 */

/*
 * For now, a key marked for KEYUSE_{WRAP|UNWRAP} can also be used for
 * KEYUSE_{ENCRYPT|DECRYPT}. This is a temporary workaround for
 * Radar 2716153.
 */
#define RELAXED_WRAP_USAGE		1

void cspValidateIntendedKeyUsage(
	const CSSM_KEYHEADER	*hdr,
	CSSM_KEYUSE				intendedUsage)
{
	uint32 		keyUsage = hdr->KeyUsage;
	cspKeyType	keyType;

	/* first, the obvious */
	if(keyUsage & CSSM_KEYUSE_ANY) {
		/* OK for now */
		return;
	}
	if(!(keyUsage & intendedUsage)) {
		#if		RELAXED_WRAP_USAGE
		if(! ( ( (keyUsage & CSSM_KEYUSE_WRAP) && 
		         (intendedUsage == CSSM_KEYUSE_ENCRYPT)
			   ) ||
			   ( (keyUsage & CSSM_KEYUSE_UNWRAP) && 
		         (intendedUsage == CSSM_KEYUSE_DECRYPT)
			   )
			 ) )
		#endif
		CssmError::throwMe(CSSMERR_CSP_KEY_USAGE_INCORRECT);
	}

	/* now validate all of the key's usage bits - this is mainly to
	 * prevent and detect tampering */
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_SESSION_KEY:
			keyType = CKT_Session;
			break;
		case CSSM_KEYCLASS_PUBLIC_KEY:
			keyType = CKT_Public;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			keyType = CKT_Private;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	try {
		cspValidateKeyUsageBits(keyType, keyUsage);
	}
	catch (...) {
		/* override error.... */
		CssmError::throwMe(CSSMERR_CSP_KEY_USAGE_INCORRECT);
	}
}

/*
 * Set up a key header.
 */
void setKeyHeader(
	CSSM_KEYHEADER &hdr,
	const Guid &myGuid,
	CSSM_ALGORITHMS alg, 
	CSSM_KEYCLASS keyClass,
	CSSM_KEYATTR_FLAGS attrs, 
	CSSM_KEYUSE use)
{
    memset(&hdr, 0, sizeof(CSSM_KEYHEADER));
    hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
    hdr.CspId = myGuid;
    hdr.AlgorithmId = alg;
    hdr.KeyClass = keyClass;
    hdr.KeyUsage = use;
    hdr.KeyAttr = attrs;

    // defaults (change as needed)
    hdr.WrapAlgorithmId = CSSM_ALGID_NONE;
}

/*
 * Ensure that indicated CssmData can handle 'length' bytes 
 * of data. Malloc the Data ptr if necessary.
 */
void setUpCssmData(
	CssmData			&data,
	size_t				length,
	Allocator		&allocator)
{
	/* FIXME - I'm sure Perry has more elegant ways of doing this,
	 * but I can't figure them out. */
	if(data.Length == 0) {
		data.Data = (uint8 *)allocator.malloc(length);
	}
	else if(data.Length < length) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
	}
	data.Length = length;
}

void setUpData(
	CSSM_DATA			&data,
	size_t				length,
	Allocator		&allocator)
{
	setUpCssmData(CssmData::overlay(data), length, allocator);
}

void freeCssmData(
	CssmData			&data, 
	Allocator		&allocator)
{
	if(data.Data) {
		allocator.free(data.Data);
		data.Data = NULL;
	}
	data.Length = 0;
}
	
void freeData(
	CSSM_DATA			*data, 
	Allocator		&allocator,
	bool				freeStruct)		// free the CSSM_DATA itself
{
	if(data == NULL) {
		return;
	}
	if(data->Data) {
		allocator.free(data->Data);
		data->Data = NULL;
	}
	data->Length = 0;
	if(freeStruct) {
		allocator.free(data);
	}
}

/*
 * Copy source to destination, mallocing destination if necessary.
 */
void copyCssmData(
	const CssmData		&src,
	CssmData			&dst,
	Allocator		&allocator)
{
	setUpCssmData(dst, src.Length, allocator);
	memmove(dst.Data, src.Data, src.Length);
}

void copyData(
	const CSSM_DATA		&src,
	CSSM_DATA			&dst,
	Allocator		&allocator)
{
	copyCssmData(CssmData::overlay(src), 
		CssmData::overlay(dst), 
		allocator);
}

/*
 * Compare two CSSM_DATAs, return CSSM_TRUE if identical.
 */
CSSM_BOOL cspCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2)
{	
	if((data1 == NULL) || (data1->Data == NULL) || 
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return CSSM_FALSE;
	}
	if(data1->Length != data2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(data1->Data, data2->Data, data1->Length) == 0) {
		return CSSM_TRUE;
	}
	else {
		return CSSM_FALSE;
	}
}

/*
 * This takes care of mallocing the KeyLabel field. 
 */
void copyCssmHeader(
	const CssmKey::Header	&src,
	CssmKey::Header			&dst,
	Allocator			&allocator)
{
	dst = src;
}

/*
 * Given a wrapped key, infer its raw format for custom Apple unwrapping. 
 * This is a real kludge; it only works as long as each the key's
 * default format is used to generate the blob to be wrapped. 
 */
CSSM_KEYBLOB_FORMAT inferFormat(
	const CssmKey		&wrappedKey)
{
	switch(wrappedKey.keyClass()) {
		case CSSM_KEYCLASS_SESSION_KEY:
			return CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
		case CSSM_KEYCLASS_PUBLIC_KEY:
			switch(wrappedKey.algorithm()) {
				case CSSM_ALGID_RSA:
					return RSA_PUB_KEY_FORMAT;
				case CSSM_ALGID_DSA:
					return DSA_PUB_KEY_FORMAT;
				#ifdef	CRYPTKIT_CSP_ENABLE
				case CSSM_ALGID_FEE:
					return FEE_KEYBLOB_DEFAULT_FORMAT;
				case CSSM_ALGID_ECDSA:
					return CSSM_KEYBLOB_RAW_FORMAT_X509;
				#endif
				case CSSM_ALGID_DH:
					return CSSM_KEYBLOB_RAW_FORMAT_PKCS3;
				default:
					/* punt */
					return CSSM_KEYBLOB_RAW_FORMAT_NONE;
			}
		case CSSM_KEYCLASS_PRIVATE_KEY:
			switch(wrappedKey.algorithm()) {
				case CSSM_ALGID_RSA:
					return RSA_PRIV_KEY_FORMAT;
				case CSSM_ALGID_DSA:
					return DSA_PRIV_KEY_FORMAT;
				#ifdef	CRYPTKIT_CSP_ENABLE
				case CSSM_ALGID_FEE:
					return FEE_KEYBLOB_DEFAULT_FORMAT;
				case CSSM_ALGID_ECDSA:
					return CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;
				#endif
				case CSSM_ALGID_DH:
					return CSSM_KEYBLOB_RAW_FORMAT_PKCS3;
				default:
					/* punt */
					return CSSM_KEYBLOB_RAW_FORMAT_NONE;
			}
		default:
			/* punt */
			return CSSM_KEYBLOB_RAW_FORMAT_NONE;
	}
}

/*
 * Given a key and a Context, obtain the optional associated 
 * CSSM_ATTRIBUTE_{PUBLIC,PRIVATE,SYMMETRIC}_KEY_FORMAT attribute as a 
 * CSSM_KEYBLOB_FORMAT.
 */
CSSM_KEYBLOB_FORMAT requestedKeyFormat(
	const Context 	&context,
	const CssmKey	&key)
{
	CSSM_ATTRIBUTE_TYPE attrType;
	
	switch(key.keyClass()) {
		case CSSM_KEYCLASS_SESSION_KEY:
			attrType = CSSM_ATTRIBUTE_SYMMETRIC_KEY_FORMAT;
			break;
		case CSSM_KEYCLASS_PUBLIC_KEY:
			attrType = CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			attrType = CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT;
			break;
		default:
			return CSSM_KEYBLOB_RAW_FORMAT_NONE;
	}
	/* not present ==> 0 ==> CSSM_KEYBLOB_RAW_FORMAT_NONE */
	return context.getInt(attrType);
}

/* one-shot SHA1 digest */
void cspGenSha1Hash(
	const void 		*inData,
	size_t			inDataLen,
	void			*out)		// caller mallocs, digest goes here
{
	SHA1Object sha1;
	
	sha1.digestInit();
	sha1.digestUpdate(inData, inDataLen);
	sha1.digestFinal(out);
}

/*
 * Convert a CSSM_DATE to a CssmUniformDate, or NULL if the CSSM_DATE
 * is empty.
 */
static CssmUniformDate *cspGetUniformDate(
	const CSSM_DATE &cdate)
{
	bool isZero = true;
	unsigned char *cp = (unsigned char *)&cdate;
	for(unsigned i=0; i<sizeof(cdate); i++) {
		if(*cp++ != 0) {
			isZero = false;
			break;
		}
	}
	if(isZero) {
		return NULL;
	}
	else {
		return new CssmUniformDate(CssmDate::overlay(cdate));
	}
}

/*
 * Get "now" as a CssmUniformDate.
 */
static CssmUniformDate *cspNow()
{
	CFAbsoluteTime cfTime = CFAbsoluteTimeGetCurrent();
	return new CssmUniformDate(cfTime);
}

#define keyDateDebug(args...)	secdebug("keyDate", ## args) 

/*
 * Verify temporal validity of specified key. 
 * An empty (all zero) time field means "ignore this".
 * Throws CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE or 
 * CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE as appropriate. 
 */
void cspVerifyKeyTimes(
	const CSSM_KEYHEADER &hdr)
{
	CSSM_RETURN err = CSSM_OK;
	CssmUniformDate *now = NULL;	// evaluate lazily
	CssmUniformDate *end = NULL;	// ditto
	CssmUniformDate *start = cspGetUniformDate(hdr.StartDate);

	if(start) {
		now = cspNow();
		if(*now < *start) {
			keyDateDebug("Invalid start date");
			err = CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE;
		}
		else {
			keyDateDebug("Valid start date");
		}
	}
	else {
		keyDateDebug("Empty start date");
	}

	if(!err) {
		end = cspGetUniformDate(hdr.EndDate);
		if(end) {
			if(now == NULL) {
				now = cspNow();
			}
			if(*now > *end) {
				keyDateDebug("Invalid end date");
				err = CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE;
			}
			else {
				keyDateDebug("Valid end date");
			}
		}
		else {
			keyDateDebug("Empty end date");
		}
	}
	if(now) {
		delete now;
	}
	if(end) {
		delete end;
	}
	if(start) {
		delete start;
	}
	if(err) {
		CssmError::throwMe(err);
	}
}

