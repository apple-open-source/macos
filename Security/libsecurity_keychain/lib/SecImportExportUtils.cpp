/*
 * Copyright (c) 2004,2013 Apple Inc. All Rights Reserved.
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
 *
 * SecImportExportUtils.cpp - misc. utilities for import/export module
 */

#include "SecImportExportUtils.h"
#include "SecImportExportAgg.h"
#include "SecImportExportCrypto.h"
#include "SecIdentityPriv.h"
#include "SecItem.h"
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <Security/SecBase.h>
#pragma mark --- Debug support ---

#ifndef NDEBUG

const char *impExpExtFormatStr(
	SecExternalFormat format)
{
	switch(format) {
		case kSecFormatUnknown:			return "kSecFormatUnknown";
		case kSecFormatOpenSSL:			return "kSecFormatOpenSSL";
		case kSecFormatSSH:				return "kSecFormatSSH";
		case kSecFormatBSAFE:			return "kSecFormatBSAFE";
		case kSecFormatRawKey:			return "kSecFormatRawKey";
		case kSecFormatWrappedPKCS8:	return "kSecFormatWrappedPKCS8";
		case kSecFormatWrappedOpenSSL:  return "kSecFormatWrappedOpenSSL";
		case kSecFormatWrappedSSH:		return "kSecFormatWrappedSSH";
		case kSecFormatWrappedLSH:		return "kSecFormatWrappedLSH";
		case kSecFormatX509Cert:		return "kSecFormatX509Cert";
		case kSecFormatPEMSequence:		return "kSecFormatPEMSequence";
		case kSecFormatPKCS7:			return "kSecFormatPKCS7";
		case kSecFormatPKCS12:			return "kSecFormatPKCS12";
		case kSecFormatNetscapeCertSequence:  return "kSecFormatNetscapeCertSequence";
		default:						return "UNKNOWN FORMAT ENUM";
	}
}

const char *impExpExtItemTypeStr(
	SecExternalItemType itemType)
{
	switch(itemType) {
		case kSecItemTypeUnknown:		return "kSecItemTypeUnknown";
		case kSecItemTypePrivateKey:	return "kSecItemTypePrivateKey";
		case kSecItemTypePublicKey:		return "kSecItemTypePublicKey";
		case kSecItemTypeSessionKey:	return "kSecItemTypeSessionKey";
		case kSecItemTypeCertificate:   return "kSecItemTypeCertificate";
		case kSecItemTypeAggregate:		return "kSecItemTypeAggregate";
		default:						return "UNKNOWN ITEM TYPE ENUM";
	}
}
#endif  /* NDEBUG */

/*
 * Parse file extension and attempt to map it to format and type. Returns true
 * on success.
 */
bool impExpImportParseFileExten(
	CFStringRef			fstr,
	SecExternalFormat   *inputFormat,   // RETURNED
	SecExternalItemType	*itemType)		// RETURNED
{
	if(fstr == NULL) {
		/* nothing to work with */
		return false;
	}
	if(CFStringHasSuffix(fstr, CFSTR(".cer")) ||
	   CFStringHasSuffix(fstr, CFSTR(".crt"))) {
		*inputFormat = kSecFormatX509Cert;
		*itemType = kSecItemTypeCertificate;
		SecImpInferDbg("Inferring kSecFormatX509Cert from file name");
		return true;
	}
	if(CFStringHasSuffix(fstr, CFSTR(".p12")) ||
	   CFStringHasSuffix(fstr, CFSTR(".pfx"))) {
		*inputFormat = kSecFormatPKCS12;
		*itemType = kSecItemTypeAggregate;
		SecImpInferDbg("Inferring kSecFormatPKCS12 from file name");
		return true;
	}

	/* Get extension, look for key indicators as substrings */
	CFURLRef url = CFURLCreateWithString(NULL, fstr, NULL);
	if(url == NULL) {
		SecImpInferDbg("impExpImportParseFileExten: error creating URL");
		return false;
	}
	CFStringRef exten = CFURLCopyPathExtension(url);
	CFRelease(url);
	if(exten == NULL) {
		/* no extension, app probably passed in only an extension */
		exten = fstr;
		CFRetain(exten);
	}
	bool ortn = false;
	CFRange cfr;
	cfr = CFStringFind(exten, CFSTR("p7"), kCFCompareCaseInsensitive);
	if(cfr.length != 0) {
		*inputFormat = kSecFormatPKCS7;
		*itemType = kSecItemTypeAggregate;
		SecImpInferDbg("Inferring kSecFormatPKCS7 from file name");
		ortn = true;
	}
	if(!ortn) {
		cfr = CFStringFind(exten, CFSTR("p8"), kCFCompareCaseInsensitive);
		if(cfr.length != 0) {
			*inputFormat = kSecFormatWrappedPKCS8;
			*itemType = kSecItemTypePrivateKey;
			SecImpInferDbg("Inferring kSecFormatPKCS8 from file name");
			ortn = true;
		}
	}
	CFRelease(exten);
	return ortn;
}

/* do a [NSString stringByDeletingPathExtension] equivalent */
CFStringRef impExpImportDeleteExtension(
	CFStringRef			fileStr)
{
	CFDataRef fileStrData = CFStringCreateExternalRepresentation(NULL, fileStr,
		kCFStringEncodingUTF8, 0);
	if(fileStrData == NULL) {
		return NULL;
	}

	CFURLRef urlRef = CFURLCreateFromFileSystemRepresentation(NULL,
		CFDataGetBytePtr(fileStrData), CFDataGetLength(fileStrData), false);
	if(urlRef == NULL) {
		CFRelease(fileStrData);
		return NULL;
	}
	CFURLRef rtnUrl = CFURLCreateCopyDeletingPathExtension(NULL, urlRef);
	CFStringRef rtnStr = NULL;
	CFRelease(urlRef);
	if(rtnUrl) {
		rtnStr = CFURLGetString(rtnUrl);
		CFRetain(rtnStr);
		CFRelease(rtnUrl);
	}
	CFRelease(fileStrData);
	return rtnStr;
}

#pragma mark --- mapping of external format to CDSA formats ---

/*
 * For the record, here is the mapping of SecExternalFormat, algorithm, and key
 * class to CSSM-style key format (CSSM_KEYBLOB_FORMAT -
 * CSSM_KEYBLOB_RAW_FORMAT_X509, etc). The entries in the table are the
 * last component of a CSSM_KEYBLOB_FORMAT. Format kSecFormatUnknown means
 * "default for specified class and algorithm", which is currently the
 * same as kSecFormatOpenSSL.
 *
 *							              algorithm/class
 *						      RSA			    DSA				   DH
 *						----------------  ----------------  ----------------
 * SecExternalFormat	 priv      pub     priv      pub	 priv      pub
 * -----------------	-------  -------  -------  -------  -------  -------
 * kSecFormatOpenSSL	 PKCS1    X509    OPENSSL   X509     PKCS3    X509
 * kSecFormatBSAFE       PKCS8    PKCS1   FIPS186   FIPS186  PKCS8    not supported
 * kSecFormatUnknown	 PKCS1    X509    OPENSSL   X509     PKCS3    X509
 * kSecFormatSSH		  SSH      SSH      n/s     n/s       n/s     n/s
 * kSecFormatSSHv2        n/s     SSH2      n/s     SSH2      n/s     n/s
 *
 * The only external format supported for ECDSA and ECDH keys is kSecFormatOpenSSL,
 * which translates to OPENSSL for private keys and X509 for public keys.
 */

/* Arrays expressing the above table. */

/* l.s. dimension is pub/priv for one alg */
typedef struct {
	CSSM_KEYBLOB_FORMAT priv;
	CSSM_KEYBLOB_FORMAT pub;
} algForms;

/*
 * indices into array of algForms defining all algs' formats for a given
 * SecExternalFormat
 */
#define SIE_ALG_RSA		0
#define SIE_ALG_DSA		1
#define SIE_ALG_DH		2
#define SIE_ALG_ECDSA	3
#define SIE_ALG_LAST	SIE_ALG_ECDSA
#define SIE_NUM_ALGS	(SIE_ALG_LAST + 1)

/* kSecFormatOpenSSL */
static algForms opensslAlgForms[SIE_NUM_ALGS] =
{
	{ CSSM_KEYBLOB_RAW_FORMAT_PKCS1,	CSSM_KEYBLOB_RAW_FORMAT_X509 },		// RSA
	{ CSSM_KEYBLOB_RAW_FORMAT_OPENSSL,  CSSM_KEYBLOB_RAW_FORMAT_X509 },		// DSA
	{ CSSM_KEYBLOB_RAW_FORMAT_PKCS3,	CSSM_KEYBLOB_RAW_FORMAT_X509 },		// D-H
	{ CSSM_KEYBLOB_RAW_FORMAT_OPENSSL,	CSSM_KEYBLOB_RAW_FORMAT_X509 },		// ECDSA
};

/* kSecFormatBSAFE */
static algForms bsafeAlgForms[SIE_NUM_ALGS] =
{
	{ CSSM_KEYBLOB_RAW_FORMAT_PKCS8,	CSSM_KEYBLOB_RAW_FORMAT_PKCS1 },	// RSA
	{ CSSM_KEYBLOB_RAW_FORMAT_FIPS186,  CSSM_KEYBLOB_RAW_FORMAT_FIPS186 },	// DSA
	{ CSSM_KEYBLOB_RAW_FORMAT_PKCS8,	CSSM_KEYBLOB_RAW_FORMAT_NONE },		// D-H
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,		CSSM_KEYBLOB_RAW_FORMAT_NONE },		// ECDSA not supported
};

/* kSecFormatSSH (v1) */
static algForms ssh1AlgForms[SIE_NUM_ALGS] =
{
	{ CSSM_KEYBLOB_RAW_FORMAT_OPENSSH,	CSSM_KEYBLOB_RAW_FORMAT_OPENSSH },	// RSA only
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,		CSSM_KEYBLOB_RAW_FORMAT_NONE },		// DSA not supported
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,		CSSM_KEYBLOB_RAW_FORMAT_NONE },		// D-H not supported
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,		CSSM_KEYBLOB_RAW_FORMAT_NONE },		// ECDSA not supported
};

/* kSecFormatSSHv2 */
static algForms ssh2AlgForms[SIE_NUM_ALGS] =
{
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,		CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2 },	// RSA - public only
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,		CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2 },	// DSA - public only
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,		CSSM_KEYBLOB_RAW_FORMAT_NONE },		// D-H not supported
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,		CSSM_KEYBLOB_RAW_FORMAT_NONE },		// ECDSA not supported
};

/*
 * This routine performs a lookup into the above 3-dimensional array to
 * map {algorithm, class, SecExternalFormat} to a CSSM_KEYBLOB_FORMAT.
 * Returns errSecUnsupportedFormat in the rare appropriate case.
 */
OSStatus impExpKeyForm(
	SecExternalFormat		externForm,
	SecExternalItemType		itemType,
	CSSM_ALGORITHMS			alg,
	CSSM_KEYBLOB_FORMAT		*cssmForm,		// RETURNED
	CSSM_KEYCLASS			*cssmClass)		// RETRUNED
{
	if(itemType == kSecItemTypeSessionKey) {
		/* special trivial case */
		/* FIXME ensure caller hasn't specified bogus format */
		*cssmForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
		*cssmClass = CSSM_KEYCLASS_SESSION_KEY;
		return errSecSuccess;
	}
	if(externForm == kSecFormatUnknown) {
		/* default is openssl */
		externForm = kSecFormatOpenSSL;
	}

	unsigned algDex;
	switch(alg) {
		case CSSM_ALGID_RSA:
			algDex = SIE_ALG_RSA;
			break;
		case CSSM_ALGID_DSA:
			algDex = SIE_ALG_DSA;
			break;
		case CSSM_ALGID_DH:
			algDex = SIE_ALG_DH;
			break;
		case CSSM_ALGID_ECDSA:
			algDex = SIE_ALG_ECDSA;
			break;
		default:
			return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	const algForms *forms;
	switch(externForm) {
		case kSecFormatOpenSSL:
			forms = opensslAlgForms;
			break;
		case kSecFormatBSAFE:
			forms = bsafeAlgForms;
			break;
		case kSecFormatSSH:
			forms = ssh1AlgForms;
			break;
		case kSecFormatSSHv2:
			forms = ssh2AlgForms;
			break;
		default:
			return errSecUnsupportedFormat;
	}
	CSSM_KEYBLOB_FORMAT form = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	switch(itemType) {
		case kSecItemTypePrivateKey:
			form = forms[algDex].priv;
			*cssmClass = CSSM_KEYCLASS_PRIVATE_KEY;
			break;
		case kSecItemTypePublicKey:
			form = forms[algDex].pub;
			*cssmClass = CSSM_KEYCLASS_PUBLIC_KEY;
			break;
		default:
			return errSecUnsupportedFormat;
	}
	if(form == CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		/* not in the tables - abort */
		return errSecUnsupportedFormat;
	}
	else {
		*cssmForm = form;
		return errSecSuccess;
	}
}

/*
 * Given a raw key blob and zero to three known parameters (type, format,
 * algorithm), figure out all parameters. Used for private and public keys.
 */
static bool impExpGuessKeyParams(
	CFDataRef				keyData,
	SecExternalFormat		*externForm,		// IN/OUT
	SecExternalItemType		*itemType,			// IN/OUT
	CSSM_ALGORITHMS			*keyAlg)			// IN/OUT
{
	/* CSSM alg list: RSA, DSA, DH, ECDSA */
	CSSM_ALGORITHMS minAlg		 = CSSM_ALGID_RSA;
	CSSM_ALGORITHMS maxAlg		 = CSSM_ALGID_ECDSA;
	SecExternalFormat minForm    = kSecFormatOpenSSL;		// then SSH, BSAFE, then...
	SecExternalFormat maxForm    = kSecFormatSSHv2;
	SecExternalItemType minType  = kSecItemTypePrivateKey;  // just two
	SecExternalItemType maxType  = kSecItemTypePublicKey;

	switch(*externForm) {
		case kSecFormatUnknown:
			break;								// run through all formats
		case kSecFormatOpenSSL:
		case kSecFormatSSH:
		case kSecFormatSSHv2:
		case kSecFormatBSAFE:
			minForm = maxForm = *externForm;	// just test this one
			break;
		default:
			return false;
	}
	switch(*itemType) {
		case kSecItemTypeUnknown:
			break;
		case kSecItemTypePrivateKey:
		case kSecItemTypePublicKey:
			minType = maxType = *itemType;
			break;
		default:
			return false;
	}
	switch(*keyAlg) {
		case CSSM_ALGID_NONE:
			break;
		case CSSM_ALGID_RSA:
		case CSSM_ALGID_DSA:
		case CSSM_ALGID_DH:
		case CSSM_ALGID_ECDSA:
			minAlg = maxAlg = *keyAlg;
			break;
		default:
			return false;
	}

	CSSM_ALGORITHMS theAlg;
	SecExternalFormat theForm;
	SecExternalItemType theType;
	CSSM_CSP_HANDLE cspHand = cuCspStartup(CSSM_TRUE);
	if(cspHand == 0) {
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	}

	/*
	 * Iterate through all set of enabled {alg, type, format}.
	 * We do not assume that any of the enums are sequential hence this
	 * odd iteration algorithm....
	 */
	bool ourRtn = false;
	for(theAlg=minAlg; ; ) {
		for(theForm=minForm; ; ) {
			for(theType=minType; ; ) {

				/* do super lightweight null unwrap to parse */
				OSStatus ortn = impExpImportRawKey(keyData,
					theForm, theType, theAlg,
					NULL,		// no keychain
					cspHand,
					0,			// no flags
					NULL,		// no key params
					NULL,		// no printName
					NULL);		// no returned items
				if(ortn == errSecSuccess) {
					*externForm = theForm;
					*itemType = theType;
					*keyAlg = theAlg;
					ourRtn = true;
					goto done;
				}

				/* next type or break if we're done */
				if(theType == maxType) {
					break;
				}
				else switch(theType) {
					case kSecItemTypePrivateKey:
						theType = kSecItemTypePublicKey;
						break;
					default:
						assert(0);
						ourRtn = false;
						goto done;
				}
			}   /* for each class/type */

			/* next format or break if we're done */
			if(theForm == maxForm) {
				break;
			}
			else switch(theForm) {
				case kSecFormatOpenSSL:
					theForm = kSecFormatSSH;
					break;
				case kSecFormatSSH:
					theForm = kSecFormatBSAFE;
					break;
				case kSecFormatBSAFE:
					theForm = kSecFormatSSHv2;
					break;
				default:
					assert(0);
					ourRtn = false;
					goto done;
			}
		}		/* for each format */

		/* next alg or break if we're done */
		if(theAlg == maxAlg) {
			break;
		}
		else switch(theAlg) {
			case CSSM_ALGID_RSA:
				theAlg = CSSM_ALGID_DSA;
				break;
			case CSSM_ALGID_DSA:
				theAlg = CSSM_ALGID_DH;
				break;
			case CSSM_ALGID_DH:
				theAlg = CSSM_ALGID_ECDSA;
				break;
			default:
				/* i.e. CSSM_ALGID_ECDSA, we already broke at theAlg == maxAlg */
				assert(0);
				ourRtn = false;
				goto done;
		}
	}			/* for each alg */
done:
	cuCspDetachUnload(cspHand, CSSM_TRUE);
	return ourRtn;
}

/*
 * Guess an incoming blob's type, format and (for keys only) algorithm
 * by examining its contents. Returns true on success, in which case
 * *inputFormat, *itemType, and *keyAlg are all valid. Caller optionally
 * passes in valid values any number of these as a clue.
 */
bool impExpImportGuessByExamination(
	CFDataRef			inData,
	SecExternalFormat   *inputFormat,	// may be kSecFormatUnknown on entry
	SecExternalItemType	*itemType,		// may be kSecItemTypeUnknown on entry
	CSSM_ALGORITHMS		*keyAlg)		// CSSM_ALGID_NONE - unknown
{
	if( ( (*inputFormat == kSecFormatUnknown) ||
	      (*inputFormat == kSecFormatX509Cert)
		) &&
	   ( (*itemType == kSecItemTypeUnknown) ||
		 (*itemType == kSecItemTypeCertificate) ) ) {
		/*
		 * See if it parses as a cert
		 */
		CSSM_CL_HANDLE clHand = cuClStartup();
		if(clHand == 0) {
			return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
		}
		CSSM_HANDLE cacheHand;
		CSSM_RETURN crtn;
		CSSM_DATA cdata = { CFDataGetLength(inData),
						    (uint8 *)CFDataGetBytePtr(inData) };
		crtn = CSSM_CL_CertCache(clHand, &cdata, &cacheHand);
		bool brtn = false;
		if(crtn == CSSM_OK) {
			*inputFormat = kSecFormatX509Cert;
			*itemType = kSecItemTypeCertificate;
			SecImpInferDbg("Inferred kSecFormatX509Cert via CL");
			CSSM_CL_CertAbortCache(clHand, cacheHand);
			brtn = true;
		}
		cuClDetachUnload(clHand);
		if(brtn) {
			return true;
		}
	}
	/* TBD: need way to inquire of P12 lib if this is a valid-looking PFX */

	if( ( (*inputFormat == kSecFormatUnknown) ||
	      (*inputFormat == kSecFormatNetscapeCertSequence)
		) &&
	   ( (*itemType == kSecItemTypeUnknown) ||
		 (*itemType == kSecItemTypeAggregate) ) ) {
		/* See if it's a netscape cert sequence */
		CSSM_RETURN crtn = impExpNetscapeCertImport(inData, 0, NULL, NULL, NULL);
		if(crtn == CSSM_OK) {
			*inputFormat = kSecFormatNetscapeCertSequence;
			*itemType = kSecItemTypeAggregate;
			SecImpInferDbg("Inferred netscape-cert-sequence by decoding");
			return true;
		}
	}

	/* See if it's a key */
	return impExpGuessKeyParams(inData, inputFormat, itemType, keyAlg);
}

#pragma mark --- Key Import support ---

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 */
CSSM_RETURN impExpAddContextAttribute(CSSM_CC_HANDLE CCHandle,
	uint32 AttributeType,
	uint32 AttributeLength,
	const void *AttributePtr)
{
	CSSM_CONTEXT_ATTRIBUTE		newAttr;

	newAttr.AttributeType     = AttributeType;
	newAttr.AttributeLength   = AttributeLength;
	newAttr.Attribute.Data    = (CSSM_DATA_PTR)AttributePtr;
	return CSSM_UpdateContextAttributes(CCHandle, 1, &newAttr);
}

/*
 * Free memory via specified plugin's app-level allocator
 */
void impExpFreeCssmMemory(
	CSSM_HANDLE		hand,
	void 			*p)
{
	CSSM_API_MEMORY_FUNCS memFuncs;
	CSSM_RETURN crtn = CSSM_GetAPIMemoryFunctions(hand, &memFuncs);
	if(crtn) {
		return;
	}
	memFuncs.free_func(p, memFuncs.AllocRef);
}

/*
 * Calculate digest of any CSSM_KEY. Unlike older implementations
 * of this logic, you can actually calculate the public key hash
 * on any class of key, any format, raw CSP or CSPDL (though if
 * you're using the CSPDL, the key has to be a reference key
 * in that CSPDL session).
 *
 * Caller must free keyDigest->Data using impExpFreeCssmMemory() since
 * this is allocated by the CSP's app-specified allocator.
 */
CSSM_RETURN impExpKeyDigest(
	CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR	key,
	CSSM_DATA_PTR   keyDigest)		// contents allocd and RETURNED
{
	CSSM_DATA_PTR   localDigest;
	CSSM_CC_HANDLE  ccHand;

	CSSM_RETURN crtn = CSSM_CSP_CreatePassThroughContext(cspHand,
		key,
		&ccHand);
	if(crtn) {
		return crtn;
	}
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&localDigest);
	if(crtn) {
		SecImpExpDbg("CSSM_CSP_PassThrough(KEY_DIGEST) failure");
	}
	else {
		/*
		 * Give caller the Data referent and we'll free the
		 * CSSM_DATA struct itswelf.
		 */
		*keyDigest = *localDigest;
		impExpFreeCssmMemory(cspHand, localDigest);
	}
	CSSM_DeleteContext(ccHand);
	return crtn;
}


/*
 * Given a CFTypeRef passphrase which may be a CFDataRef or a CFStringRef,
 * return a refcounted CFStringRef suitable for use with the PKCS12 library.
 * PKCS12 passphrases in CFData format must be UTF8 encoded.
 */
OSStatus impExpPassphraseToCFString(
	CFTypeRef   passin,
	CFStringRef *passout)	// may be the same as passin, but refcounted
{
	if(CFGetTypeID(passin) == CFStringGetTypeID()) {
		CFStringRef passInStr = (CFStringRef)passin;
		CFRetain(passInStr);
		*passout = passInStr;
		return errSecSuccess;
	}
	else if(CFGetTypeID(passin) == CFDataGetTypeID()) {
		CFDataRef cfData = (CFDataRef)passin;
		CFIndex len = CFDataGetLength(cfData);
		CFStringRef cfStr = CFStringCreateWithBytes(NULL,
			CFDataGetBytePtr(cfData), len, kCFStringEncodingUTF8, true);
		if(cfStr == NULL) {
			SecImpExpDbg("Passphrase not in UTF8 format");
			return errSecParam;
		}
		*passout = cfStr;
		return errSecSuccess;
	}
	else {
		SecImpExpDbg("Passphrase not CFData or CFString");
		return errSecParam;
	}
}

/*
 * Given a CFTypeRef passphrase which may be a CFDataRef or a CFStringRef,
 * return a refcounted CFDataRef whose bytes are suitable for use with
 * PKCS5 (v1.5 and v2.0) key derivation.
 */
OSStatus impExpPassphraseToCFData(
	CFTypeRef   passin,
	CFDataRef   *passout)	// may be the same as passin, but refcounted
{
	if(CFGetTypeID(passin) == CFDataGetTypeID()) {
		CFDataRef passInData = (CFDataRef)passin;
		CFRetain(passInData);
		*passout = passInData;
		return errSecSuccess;
	}
	else if(CFGetTypeID(passin) == CFStringGetTypeID()) {
		CFStringRef passInStr = (CFStringRef)passin;
		CFDataRef outData;
		outData = CFStringCreateExternalRepresentation(NULL,
			passInStr,
			kCFStringEncodingUTF8,
			0);		// lossByte 0 ==> no loss allowed
		if(outData == NULL) {
			/* Well try with lossy conversion */
			SecImpExpDbg("Trying lossy conversion of CFString passphrase to UTF8");
			outData = CFStringCreateExternalRepresentation(NULL,
				passInStr,
				kCFStringEncodingUTF8,
				1);
			if(outData == NULL) {
				SecImpExpDbg("Failure on conversion of CFString passphrase to UTF8");
				/* what do we do now, Batman? */
				return errSecParam;
			}
		}
		*passout = outData;
		return errSecSuccess;
	}
	else {
		SecImpExpDbg("Passphrase not CFData or CFString");
		return errSecParam;
	}
}

/*
* Add a CFString to a crypto context handle.
*/
static CSSM_RETURN impExpAddStringAttr(
	CSSM_CC_HANDLE ccHand,
	CFStringRef str,
	CSSM_ATTRIBUTE_TYPE attrType)
{
	/* CFStrings are passed as external rep in UTF8 encoding by convention */
	CFDataRef outData;
	outData = CFStringCreateExternalRepresentation(NULL,
		str, kCFStringEncodingUTF8,	0);		// lossByte 0 ==> no loss allowed
	if(outData == NULL) {
		SecImpExpDbg("impExpAddStringAttr: bad string format");
		return errSecParam;
	}

	CSSM_DATA attrData;
	attrData.Data = (uint8 *)CFDataGetBytePtr(outData);
	attrData.Length = CFDataGetLength(outData);
	CSSM_RETURN crtn = impExpAddContextAttribute(ccHand, attrType, sizeof(CSSM_DATA),
		&attrData);
	CFRelease(outData);
	if(crtn) {
		SecImpExpDbg("impExpAddStringAttr: CSSM_UpdateContextAttributes error");
	}
	return crtn;
}

/*
 * Generate a secure passphrase key. Caller must eventually CSSM_FreeKey the result.
 */
static CSSM_RETURN impExpCreatePassKey(
	const SecKeyImportExportParameters *keyParams,  // required
	CSSM_CSP_HANDLE		cspHand,		// MUST be CSPDL
	impExpVerifyPhrase	verifyPhrase,   // for secure passphrase
	CSSM_KEY_PTR		*passKey)		// mallocd and RETURNED
{
	CSSM_RETURN crtn;
	CSSM_CC_HANDLE ccHand;
	uint32 verifyAttr;
	CSSM_DATA dummyLabel;
	CSSM_KEY_PTR ourKey = NULL;

	SecImpExpDbg("Generating secure passphrase key");
	ourKey = (CSSM_KEY_PTR)malloc(sizeof(CSSM_KEY));
	if(ourKey == NULL) {
		return errSecAllocate;
	}
	memset(ourKey, 0, sizeof(CSSM_KEY));

	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		CSSM_ALGID_SECURE_PASSPHRASE,
		4,				// keySizeInBits must be non zero
		NULL,			// Seed
		NULL,			// Salt
		NULL,			// StartDate
		NULL,			// EndDate
		NULL,			// Params
		&ccHand);
	if(crtn) {
		SecImpExpDbg("impExpCreatePassKey: CSSM_CSP_CreateKeyGenContext error");
		return crtn;
	}
	/* subsequent errors to errOut: */

	/* additional context attributes specific to this type of key gen */
	assert(keyParams != NULL);			// or we wouldn't be here
	if(keyParams->alertTitle != NULL) {
		crtn = impExpAddStringAttr(ccHand, keyParams->alertTitle,
			CSSM_ATTRIBUTE_ALERT_TITLE);
		if(crtn) {
			goto errOut;
		}
	}
	if(keyParams->alertPrompt != NULL) {
		crtn = impExpAddStringAttr(ccHand, keyParams->alertPrompt,
			CSSM_ATTRIBUTE_PROMPT);
		if(crtn) {
			goto errOut;
		}
	}
	verifyAttr = (verifyPhrase == VP_Export) ? 1 : 0;
	crtn = impExpAddContextAttribute(ccHand, CSSM_ATTRIBUTE_VERIFY_PASSPHRASE,
		sizeof(uint32), (const void *)((size_t) verifyAttr));
	if(crtn) {
		SecImpExpDbg("impExpCreatePassKey: CSSM_UpdateContextAttributes error");
		goto errOut;
	}

	dummyLabel.Data = (uint8 *)"Secure Passphrase";
	dummyLabel.Length = strlen((char *)dummyLabel.Data);

	crtn = CSSM_GenerateKey(ccHand,
		CSSM_KEYUSE_ANY,
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE,
		&dummyLabel,
		NULL,			// ACL
		ourKey);
	if(crtn) {
		SecImpExpDbg("impExpCreatePassKey: CSSM_GenerateKey error");
	}
errOut:
	CSSM_DeleteContext(ccHand);
	if(crtn == CSSM_OK) {
		*passKey = ourKey;
	}
	else if(ourKey != NULL) {
		free(ourKey);
	}
	return crtn;
}

/*
 * Obtain passphrase, given a SecKeyImportExportParameters.
 *
 * Passphrase comes from one of two places: app-specified, in
 * SecKeyImportExportParameters.passphrase (either as CFStringRef
 * or CFDataRef); or via the secure passphrase mechanism.
 *
 * Passphrase is returned in AT MOST one of two forms:
 *
 * -- Secure passphrase is returned as a CSSM_KEY_PTR, which the
 *    caller must CSSM_FreeKey later as well as free()ing the actual
 *    CSSM_KEY_PTR.
 * -- CFTypeRef for app-supplied passphrases. This can be one of
 *    two types:
 *
 *    -- CFStringRef, for use with P12
 *    -- CFDataRef, for more general use (e.g. for PKCS5).
 *
 *    In either case the caller must CFRelease the result.
 */
OSStatus impExpPassphraseCommon(
	const SecKeyImportExportParameters *keyParams,
	CSSM_CSP_HANDLE			cspHand,		// MUST be CSPDL, for passKey generation
	impExpPassphraseForm	phraseForm,
	impExpVerifyPhrase		verifyPhrase,   // for secure passphrase
	CFTypeRef				*phrase,		// RETURNED, or
	CSSM_KEY_PTR			*passKey)		// mallocd and RETURNED
{
	assert(keyParams != NULL);

	/* Give precedence to secure passphrase */
	if(keyParams->flags & kSecKeySecurePassphrase) {
		assert(passKey != NULL);
		return impExpCreatePassKey(keyParams, cspHand, verifyPhrase, passKey);
	}
	else if(keyParams->passphrase != NULL) {
		CFTypeRef phraseOut;
		OSStatus ortn;
		assert(phrase != NULL);
		switch(phraseForm) {
			case SPF_String:
				ortn = impExpPassphraseToCFString(keyParams->passphrase,
					(CFStringRef *)&phraseOut);
				break;
			case SPF_Data:
				ortn = impExpPassphraseToCFData(keyParams->passphrase,
					(CFDataRef *)&phraseOut);
				break;
			default:
				/* only called internally */
				assert(0);
				ortn = errSecParam;
		}
		if(ortn == errSecSuccess) {
			*phrase = phraseOut;
		}
		return ortn;
	}
	else {
		return errSecPassphraseRequired;
	}
}

static void ToggleKeyAttribute(
	CFArrayRef keyAttrs,
	CFTypeRef attr,
	CSSM_KEYATTR_FLAGS mask,
	CSSM_KEYATTR_FLAGS &result)
{
	// If the keyAttrs array contains the given attribute,
	// set the corresponding keyattr flags, otherwise clear them.
	// (Note: caller verifies that keyAttrs is not NULL.)
	CFIndex numItems = CFArrayGetCount(keyAttrs);
	result &= ~(mask);
	if (numItems > 0) {
		CFRange range = CFRangeMake(0, numItems);
		if (CFArrayContainsValue(keyAttrs, range, attr))
			result |= mask;
	}
}

CSSM_KEYATTR_FLAGS ConvertArrayToKeyAttributes(SecKeyRef aKey, CFArrayRef keyAttrs)
{
	CSSM_KEYATTR_FLAGS result = CSSM_KEYATTR_RETURN_DEFAULT;

	if (aKey) {
		const CSSM_KEY* cssmKey = NULL;
		if (errSecSuccess == SecKeyGetCSSMKey(aKey, &cssmKey))
			result = cssmKey->KeyHeader.KeyAttr;
	}

	if (!keyAttrs)
		return result;

	CFMutableArrayRef attrs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFIndex idx, count = CFArrayGetCount(keyAttrs);
	for (idx=0; idx<count; idx++) {
		CFTypeRef attr = (CFTypeRef) CFArrayGetValueAtIndex(keyAttrs, idx);
		if (attr && (CFNumberGetTypeID() == CFGetTypeID(attr))) {
			// Convert numeric CSSM keyattr values to equivalent attribute constants
			uint32 value;
			if (CFNumberGetValue((CFNumberRef)attr, kCFNumberSInt32Type, &value)) {
				switch (value) {
					case CSSM_KEYATTR_SENSITIVE:
						attr = kSecAttrIsSensitive;
						break;
					case CSSM_KEYATTR_EXTRACTABLE:
						attr = kSecAttrIsExtractable;
						break;
					case CSSM_KEYATTR_PERMANENT:
						attr = kSecAttrIsPermanent;
						break;
					default:
						attr = NULL;
						break;
				}
			}
		}
		if (attr)
			CFArrayAppendValue(attrs, attr);
	}

	// Set key attribute flag in result if present in the array, otherwise clear
	ToggleKeyAttribute(attrs, kSecAttrIsSensitive, CSSM_KEYATTR_SENSITIVE, result);
	ToggleKeyAttribute(attrs, kSecAttrIsExtractable, CSSM_KEYATTR_EXTRACTABLE, result);
	ToggleKeyAttribute(attrs, kSecAttrIsPermanent, CSSM_KEYATTR_PERMANENT, result);

	// if caller specified an attributes array which omitted kSecAttrIsExtractable,
	// this implies the sensitive attribute; force it on so that at least one bit
	// is set. If our result is 0, this is indistinguishable from the case where
	// no key attributes were specified, causing a default bitmask to be used
	// in subsequent import code.

	if (0==(result & CSSM_KEYATTR_EXTRACTABLE))
		result |= CSSM_KEYATTR_SENSITIVE;

	CFRelease(attrs);
	return result;
}

Boolean ConvertSecKeyImportExportParametersToSecImportExportKeyParameters(SecKeyRef aKey,
	const SecItemImportExportKeyParameters* newPtr, SecKeyImportExportParameters* oldPtr)
{
	Boolean result = false;

	if (NULL != oldPtr && NULL != newPtr)
	{
		oldPtr->version = newPtr->version;
		oldPtr->flags = newPtr->flags;
		oldPtr->passphrase = newPtr->passphrase;
		oldPtr->alertTitle = newPtr->alertTitle;
		oldPtr->alertPrompt = newPtr->alertPrompt;
		oldPtr->accessRef = newPtr->accessRef;
		oldPtr->keyUsage = ConvertArrayToKeyUsage(newPtr->keyUsage);
		oldPtr->keyAttributes = ConvertArrayToKeyAttributes(aKey, newPtr->keyAttributes);
		result = true;
	}
	return result;
}

