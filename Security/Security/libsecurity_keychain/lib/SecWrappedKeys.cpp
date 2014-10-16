/*
 * Copyright (c) 2004,2011-2014 Apple Inc. All Rights Reserved.
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
 * SecWrappedKeys.cpp - SecExportRep and SecImportRep methods dealing with 
 *		wrapped private keys (other than PKCS8 format).
 */

#include "SecExternalRep.h"
#include "SecImportExportUtils.h"
#include "SecImportExportPem.h"
#include "SecImportExportCrypto.h"
#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include <Security/SecKeyPriv.h>
#include <security_asn1/SecNssCoder.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_utilities/devrandom.h>

#include <assert.h>

using namespace Security;
using namespace KeychainCore;

static int hexToDigit(
	char digit,
	uint8 *rtn)		// RETURNED
{
	if((digit >= '0') && (digit <= '9')) {
		*rtn = digit - '0';
		return 0;
	}
	if((digit >= 'a') && (digit <= 'f')) {
		*rtn = digit - 'a' + 10;
		return 0;
	}
	if((digit >= 'A') && (digit <= 'F')) {
		*rtn = digit - 'A' + 10;
		return 0;
	}
	return -1;
}

/* 
 * Convert two ascii characters starting at cp to an unsigned char.
 * Returns nonzero on error.
 */
static int hexToUchar(
	const char *cp,
	uint8 *rtn)		// RETURNED
{
	uint8 rtnc = 0;
	uint8 c;
	if(hexToDigit(*cp++, &c)) {
		return -1;
	}
	rtnc = c << 4;
	if(hexToDigit(*cp, &c)) {
		return -1;
	}
	rtnc |= c;
	*rtn = rtnc;
	return 0;
}

/*
 * Given an array of PEM parameter lines, infer parameters for key derivation and 
 * encryption.
 */
static OSStatus opensslPbeParams(
	CFArrayRef			paramLines,			// elements are CFStrings
	SecNssCoder			&coder,				// IV allocd with this
	/* remaining arguments RETURNED */
	CSSM_ALGORITHMS		&pbeAlg,
	CSSM_ALGORITHMS		&keyAlg,
	CSSM_ALGORITHMS		&encrAlg,
	CSSM_ENCRYPT_MODE	&encrMode,
	CSSM_PADDING		&encrPad,
	uint32				&keySizeInBits,
	unsigned			&blockSizeInBytes,
	CSSM_DATA			&iv)
{
	/* 
	 * This format requires PEM parameter lines. We could have gotten here
	 * without them if caller specified wrong format.
	 */
	 if(paramLines == NULL) {
		SecImpExpDbg("importWrappedKeyOpenssl: no PEM parameter lines");
		return errSecUnknownFormat;
	 }
	 CFStringRef dekInfo = NULL;
	 CFIndex numLines = CFArrayGetCount(paramLines);
	 for(CFIndex dex=0; dex<numLines; dex++) {
		CFStringRef str = (CFStringRef)CFArrayGetValueAtIndex(paramLines, dex);
		CFRange range;
		range = CFStringFind(str, CFSTR("DEK-Info: "), 0);
		if(range.length != 0) {
			dekInfo = str;
			break;
		}
	 }
	 if(dekInfo == NULL) {
		SecImpExpDbg("importWrappedKeyOpenssl: no DEK-Info lines");
		return errSecUnknownFormat;
	 }
	 
	 /* drop down to C strings for low level grunging */
	 char cstr[1024];
	 if(!CFStringGetCString(dekInfo, cstr, sizeof(cstr), kCFStringEncodingASCII)) {
		SecImpExpDbg("importWrappedKeyOpenssl: bad DEK-Info line (1)");
		return errSecUnknownFormat;
	 }
	 
	/* 
	 * This line looks like this:
	 * DEK-Info: DES-CBC,A22977A0A6A6F696
	 * 
	 * Now parse, getting the cipher spec and the IV.
	 */
	char *cp = strchr(cstr, ':');
	if(cp == NULL) {
		SecImpExpDbg("importWrappedKeyOpenssl: bad DEK-Info line (2)");
		return errSecUnknownFormat;
	}
	if((cp[1] == ' ') && (cp[2] != '\0')) {
		/* as it normally does... */
		cp += 2;
	}
	
	/* We only support DES and 3DES here */
	if(!strncmp(cp, "DES-EDE3-CBC", 12)) {
		keyAlg = CSSM_ALGID_3DES_3KEY;
		encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
		keySizeInBits = 64 * 3;
		blockSizeInBytes = 8;
	}
	else if(!strncmp(cp, "DES-CBC", 7)) {
		keyAlg = CSSM_ALGID_DES;
		encrAlg = CSSM_ALGID_DES;
		keySizeInBits = 64;
		blockSizeInBytes = 8;
	}
	else {
		SecImpExpDbg("importWrappedKeyOpenssl: unrecognized wrap alg (%s)",
			cp);
		return errSecUnknownFormat;
	}

	/* these are more or less fixed */
	pbeAlg   = CSSM_ALGID_PBE_OPENSSL_MD5;
	encrMode = CSSM_ALGMODE_CBCPadIV8;
	encrPad  = CSSM_PADDING_PKCS7;
	
	/* now get the ASCII hex version of the IV */
	cp = strchr(cp, ',');
	if(cp == NULL) {
		SecImpExpDbg("importWrappedKeyOpenssl: No IV in DEK-Info line");
		return errSecUnknownFormat;
	}
	if(cp[1] != '\0') {
		cp++;
	}
	
	/* remainder should be just the IV */
	if(strlen(cp) != (blockSizeInBytes * 2)) {
		SecImpExpDbg("importWrappedKeyOpenssl: bad IV in DEK-Info line (1)");
		return errSecUnknownFormat;
	}
	
	coder.allocItem(iv, blockSizeInBytes);
	for(unsigned dex=0; dex<blockSizeInBytes; dex++) {
		if(hexToUchar(cp + (dex * 2), &iv.Data[dex])) {
			SecImpExpDbg("importWrappedKeyOpenssl: bad IV in DEK-Info line (2)");
			return errSecUnknownFormat;
		}
	}
	return errSecSuccess;
}

/* 
 * Common code to derive an openssl-wrap style wrap/unwrap key.
 */
static OSStatus deriveKeyOpensslWrap(
	const SecKeyImportExportParameters	*keyParams,		// required 
	CSSM_CSP_HANDLE						cspHand,		// required
	impExpVerifyPhrase					vp,				// import/export
	CSSM_ALGORITHMS						pbeAlg,
	CSSM_ALGORITHMS						keyAlg,
	uint32								keySizeInBits,
	const CSSM_DATA						&salt,	
	CSSM_KEY_PTR						derivedKey)
{
	CFDataRef	cfPhrase = NULL;
	CSSM_KEY	*passKey = NULL;
	OSStatus	ortn;
	
	/* passphrase or passkey? */
	ortn = impExpPassphraseCommon(keyParams, cspHand, SPF_Data, vp,
		(CFTypeRef *)&cfPhrase, &passKey);
	if(ortn) {
		return ortn;
	}
	/* subsequent errors to errOut: */

	CSSM_CRYPTO_DATA		seed;
	CSSM_CC_HANDLE			ccHand = 0;
	CSSM_ACCESS_CREDENTIALS	creds;
	SecNssCoder				coder;
	CSSM_DATA				param = {0, NULL};
	CSSM_DATA				dummyLabel;
	
	memset(&seed, 0, sizeof(seed));
	if(cfPhrase != NULL) {
		size_t len = CFDataGetLength(cfPhrase);
		coder.allocItem(seed.Param, len);
		memmove(seed.Param.Data, CFDataGetBytePtr(cfPhrase), len);
		CFRelease(cfPhrase);
	}

	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	ortn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		pbeAlg,
		keyAlg,
		keySizeInBits,
		&creds,
		passKey,				// BaseKey
		1,						// iterCount - yup, this is what openssl does
		&salt,
		&seed,	
		&ccHand);
	if(ortn) {
		SecImpExpDbg("deriveKeyOpensslWrap: CSSM_CSP_CreateDeriveKeyContext error");
		goto errOut;
	}
	
	memset(derivedKey, 0, sizeof(CSSM_KEY));

	dummyLabel.Data = (uint8 *)"temp unwrap key";
	dummyLabel.Length = strlen((char *)dummyLabel.Data);
	
	ortn = CSSM_DeriveKey(ccHand,
		&param,					// i.e., derived IV - don't want one
		CSSM_KEYUSE_ANY,
		/* not extractable even for the short time this key lives */
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE,
		&dummyLabel,
		NULL,			// cred and acl
		derivedKey);
	if(ortn) {
		SecImpExpDbg("importWrappedKeyOpenssl: PKCS5 v1.5 CSSM_DeriveKey failure");
	}
	
errOut:
	if(ccHand != 0) {
		CSSM_DeleteContext(ccHand);
	}
	if(passKey != NULL) {
		CSSM_FreeKey(cspHand, NULL, passKey, CSSM_FALSE);
		free(passKey);
	}
	return ortn;
}

OSStatus SecImportRep::importWrappedKeyOpenssl(
	SecKeychainRef						importKeychain, // optional
	CSSM_CSP_HANDLE						cspHand,		// required
	SecItemImportExportFlags			flags,
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableArrayRef					outArray)		// optional, append here 
{
	assert(mExternFormat == kSecFormatWrappedOpenSSL);
	
	/* I think this is an assert - only private keys are wrapped in opensssl format */
	assert(mExternType == kSecItemTypePrivateKey);
	assert(cspHand != 0);
	
	if(keyParams == NULL) {
		return errSecParam;
	}
	
	OSStatus				ortn;
	SecNssCoder				coder;
	impExpKeyUnwrapParams   unwrapParams;
	CSSM_ALGORITHMS			pbeAlg = CSSM_ALGID_NONE;
	CSSM_ALGORITHMS			keyAlg = CSSM_ALGID_NONE;
	uint32					keySizeInBits;
	unsigned				blockSizeInBytes;

	memset(&unwrapParams, 0, sizeof(unwrapParams));

	/* parse PEM header lines */
	ortn = opensslPbeParams(mPemParamLines, coder,
		pbeAlg, keyAlg, 
		unwrapParams.encrAlg, 
		unwrapParams.encrMode,
		unwrapParams.encrPad, 
		keySizeInBits, 
		blockSizeInBytes, 
		unwrapParams.iv);
	if(ortn) {
		return ortn;
	}
	
	/* derive unwrapping key */
	CSSM_KEY unwrappingKey;
	
	ortn = deriveKeyOpensslWrap(keyParams, cspHand, VP_Import, pbeAlg, keyAlg, 
		keySizeInBits, 
		unwrapParams.iv,		/* salt = IV for these algs */
		&unwrappingKey);
	if(ortn) {
		return ortn;
	}
	
	/* set up key to unwrap */
	CSSM_KEY				wrappedKey;
	CSSM_KEYHEADER			&hdr = wrappedKey.KeyHeader;
	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
	/* CspId : don't care */
	hdr.BlobType = CSSM_KEYBLOB_WRAPPED;
	hdr.Format = CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSL;
	hdr.AlgorithmId = mKeyAlg;
	hdr.KeyClass = CSSM_KEYCLASS_PRIVATE_KEY;
	/* LogicalKeySizeInBits : calculated by CSP during unwrap */
	hdr.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	hdr.KeyUsage = CSSM_KEYUSE_ANY;

	wrappedKey.KeyData.Data = (uint8 *)CFDataGetBytePtr(mExternal);
	wrappedKey.KeyData.Length = CFDataGetLength(mExternal);
	
	unwrapParams.unwrappingKey = &unwrappingKey;
	
	/* GO */
	ortn =  impExpImportKeyCommon(&wrappedKey, importKeychain, cspHand,
		flags, keyParams, &unwrapParams, NULL, outArray);
		
	if(unwrappingKey.KeyData.Data != NULL) {
		CSSM_FreeKey(cspHand, NULL, &unwrappingKey, CSSM_FALSE);
	}
	return ortn;
}

/*
 * Hard coded parameters for export, we only do one flavor.
 */
#define OPENSSL_WRAP_KEY_ALG	CSSM_ALGID_3DES_3KEY
#define OPENSSL_WRAP_PBE_ALG	CSSM_ALGID_PBE_OPENSSL_MD5
#define OPENSSL_WRAP_KEY_SIZE   (64 * 3)
#define OPENSSL_WRAP_ENCR_ALG   CSSM_ALGID_3DES_3KEY_EDE
#define OPENSSL_WRAP_ENCR_MODE  CSSM_ALGMODE_CBCPadIV8
#define OPENSSL_WRAP_ENCR_PAD   CSSM_PADDING_PKCS7

OSStatus impExpWrappedKeyOpenSslExport(
	SecKeyRef							secKey,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableDataRef					outData,		// output appended here
	const char							**pemHeader,	// RETURNED
	CFArrayRef							*pemParamLines) // RETURNED
{
	DevRandomGenerator		rng;
	SecNssCoder				coder;
	CSSM_CSP_HANDLE			cspHand = 0;
	OSStatus				ortn;
	bool					releaseCspHand = false;
	CFMutableArrayRef		paramLines;
	CFStringRef				cfStr;
	char					dekStr[100];
	char					ivStr[3];
	
	if(keyParams == NULL) {
		return errSecParam;
	}
	
	/* we need a CSPDL handle - try to get it from the key */	
	ortn = SecKeyGetCSPHandle(secKey, &cspHand);
	if(ortn) {
		cspHand = cuCspStartup(CSSM_FALSE);
		if(cspHand == 0) {
			return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
		}
		releaseCspHand = true;
	}
	/* subsequent errors to errOut: */
	
	/* 8 bytes of random IV/salt */
	uint8 saltIv[8];
	CSSM_DATA saltIvData = { 8, saltIv} ;
	rng.random(saltIv, 8);
	
	/* derive wrapping key */
	CSSM_KEY	wrappingKey;
	wrappingKey.KeyData.Data = NULL;
	wrappingKey.KeyData.Length = 0;
	ortn = deriveKeyOpensslWrap(keyParams, cspHand, VP_Export, 
		OPENSSL_WRAP_PBE_ALG, OPENSSL_WRAP_KEY_ALG,
		OPENSSL_WRAP_KEY_SIZE,
		saltIvData,		// IV == salt for this wrapping alg
		&wrappingKey);
	if(ortn) {
		goto errOut;
	}
	
	/* wrap the outgoing key */
	CSSM_KEY wrappedKey;
	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	
	ortn = impExpExportKeyCommon(cspHand, secKey, &wrappingKey, &wrappedKey,
		OPENSSL_WRAP_ENCR_ALG, OPENSSL_WRAP_ENCR_MODE, OPENSSL_WRAP_ENCR_PAD,
		CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSL, 
		CSSM_ATTRIBUTE_NONE, CSSM_KEYBLOB_RAW_FORMAT_NONE,
		NULL, &saltIvData);
	if(ortn) {
		goto errOut;
	}
	
	/*
	 * That wrapped key's KeyData is our output 
	 */
	CFDataAppendBytes(outData, wrappedKey.KeyData.Data, wrappedKey.KeyData.Length);
	
	/* PEM header depends on key algorithm */
	switch(wrappedKey.KeyHeader.AlgorithmId) {
		case CSSM_ALGID_RSA:
			*pemHeader = PEM_STRING_RSA;
			break;  
		case CSSM_ALGID_DH:
			*pemHeader = PEM_STRING_DH_PRIVATE;
			break; 
		case CSSM_ALGID_DSA:
			*pemHeader = PEM_STRING_DSA;
			break;  
		case CSSM_ALGID_ECDSA:
			*pemHeader = PEM_STRING_ECDSA_PRIVATE;
			break;  
		default:
			SecImpExpDbg("impExpWrappedKeyOpenSslExport unknown private key alg "
				"%lu", (unsigned long)wrappedKey.KeyHeader.AlgorithmId);
			/* punt though I think something is seriously hosed */
			*pemHeader = "Private Key";
	}
	CSSM_FreeKey(cspHand, NULL, &wrappedKey, CSSM_FALSE);
	
	/* 
	 * Last thing: set up outgoing PEM parameter lines
	 */
	assert(pemParamLines != NULL);
	paramLines = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	cfStr = CFStringCreateWithCString(NULL, 
		"Proc-Type: 4,ENCRYPTED", kCFStringEncodingASCII);
	CFArrayAppendValue(paramLines, cfStr);
	CFRelease(cfStr);		// owned by array now */
	strcpy(dekStr, "DEK-Info: DES-EDE3-CBC,");
	/* next goes the IV */
	for(unsigned dex=0; dex<8; dex++) {
		sprintf(ivStr, "%02X", saltIv[dex]);
		strcat(dekStr, ivStr);
	}
	cfStr = CFStringCreateWithCString(NULL, dekStr, kCFStringEncodingASCII);
	CFArrayAppendValue(paramLines, cfStr);
	CFRelease(cfStr);		// owned by array now */
	/* and an empty line */
	cfStr = CFStringCreateWithCString(NULL, "", kCFStringEncodingASCII);
	CFArrayAppendValue(paramLines, cfStr);
	CFRelease(cfStr);		// owned by array now */
	*pemParamLines = paramLines;
	
errOut:
	if(wrappingKey.KeyData.Data != NULL) {
		CSSM_FreeKey(cspHand, NULL, &wrappingKey, CSSM_FALSE);
	}
	return ortn;

}

