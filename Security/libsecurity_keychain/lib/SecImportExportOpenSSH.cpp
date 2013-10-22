/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
 * opensshCoding.cpp - Encoding and decoding of OpenSSH format public keys.
 *
 * Created 8/29/2006 by dmitch.
 */

#include "SecImportExportOpenSSH.h"
#include "SecImportExportUtils.h"
#include "SecImportExportCrypto.h"
#include <ctype.h>
#include <CommonCrypto/CommonDigest.h>	/* for CC_MD5_DIGEST_LENGTH */
#include <security_utilities/debugging.h>
#include <security_cdsa_utils/cuCdsaUtils.h>

#define SecSSHDbg(args...)	secdebug("openssh", ## args)

#define SSHv2_PUB_KEY_NAME		"OpenSSHv2 Public Key"
#define SSHv1_PUB_KEY_NAME		"OpenSSHv1 Public Key"
#define SSHv1_PRIV_KEY_NAME		"OpenSSHv1 Private Key"

#pragma mark --- Utility functions ---

/* skip whitespace */
static void skipWhite(
	const unsigned char *&cp,
	unsigned &bytesLeft)
{
	while(bytesLeft != 0) {
		if(isspace((int)(*cp))) {
			cp++;
			bytesLeft--;
		}
		else {
			return;
		}
	}
}

/* find next whitespace or EOF - if EOF, rtn pointer points to one past EOF */
static const unsigned char *findNextWhite(
	const unsigned char *cp,
	unsigned &bytesLeft)
{
	while(bytesLeft != 0) {
		if(isspace((int)(*cp))) {
			return cp;
		}
		cp++;
		bytesLeft--;
	}
	return cp;
}

/* obtain comment as the n'th whitespace-delimited field */
static char *commentAsNthField(
	const unsigned char *key,
	unsigned keyLen,
	unsigned n)

{
	unsigned dex;
	
	skipWhite(key, keyLen);
	if(keyLen == 0) {
		return NULL;
	}
	for(dex=0; dex<(n-1); dex++) {
		key = findNextWhite(key, keyLen);
		if(keyLen == 0) {
			return NULL;
		}
		skipWhite(key, keyLen);
		if(keyLen == 0) {
			return NULL;
		}
	}
	
	/* cp points to start of nth field */
	char *rtnStr = (char *)malloc(keyLen + 1);
	memmove(rtnStr, key, keyLen);
	if(rtnStr[keyLen - 1] == '\n') {
		/* normal terminator - snip it off */
		rtnStr[keyLen - 1] = '\0';
	}
	else {
		rtnStr[keyLen] = '\0';
	}
	return rtnStr;

}

static uint32_t readUint32(
	const unsigned char *&cp,		// IN/OUT
	unsigned &len)					// IN/OUT 
{
	uint32_t r = 0;
	
	for(unsigned dex=0; dex<sizeof(uint32_t); dex++) {
		r <<= 8;
		r |= *cp++;
	}
	len -= 4;
	return r;
}

static uint16_t readUint16(
	const unsigned char *&cp,		// IN/OUT
	unsigned &len)					// IN/OUT 
{
	uint16_t r = *cp++;
	r <<= 8;
	r |= *cp++;
	len -= 2;
	return r;
}

/* Skip over an SSHv1 private key formatted bignum */
static void skipBigNum(
	const unsigned char *&cp,		// IN/OUT
	unsigned &len)					// IN/OUT 
{
	if(len < 2) {
		cp += len;
		len = 0;
		return;
	}
	uint16 numBits = readUint16(cp, len);
	unsigned numBytes = (numBits + 7) / 8;
	if(numBytes > len) {
		cp += len;
		len = 0;
		return;
	}
	cp += numBytes;
	len -= numBytes;
}

static char *genPrintName(
	const char *header,		// e.g. SSHv2_PUB_KEY_NAME
	const char *comment)	// optional, from key 
{
	size_t totalLen = strlen(header) + 1;
	if(comment) {
		/* append ": <comment>" */
		totalLen += strlen(comment);
		totalLen += 2;
	}
	char *rtnStr = (char *)malloc(totalLen);
	if(comment) {
		snprintf(rtnStr, totalLen, "%s: %s", header, comment);
	}
	else {
		strcpy(rtnStr, header);
	}
	return rtnStr;
}

#pragma mark --- Infer PrintName attribute from raw keys ---

/* obtain comment from OpenSSHv2 public key */
static char *opensshV2PubComment(
	const unsigned char *key,
	unsigned keyLen)
{
	/*
	 * The format here is
	 * header
	 * <space>
	 * keyblob
	 * <space>
	 * optional comment
	 * \n
	 */
	char *comment = commentAsNthField(key, keyLen, 3);
	char *rtnStr = genPrintName(SSHv2_PUB_KEY_NAME, comment);
	if(comment) {
		free(comment);
	}
	return rtnStr;
}

/* obtain comment from OpenSSHv1 public key */
static char *opensshV1PubComment(
	const unsigned char *key,
	unsigned keyLen)
{
	/*
	 * Format:
	 * numbits
	 * <space>
	 * e (bignum in decimal)
	 * <space>
	 * n (bignum in decimal) 
	 * <space>
	 * optional comment 
	 * \n
	 */
	char *comment = commentAsNthField(key, keyLen, 4);
	char *rtnStr = genPrintName(SSHv1_PUB_KEY_NAME, comment);
	if(comment) {
		free(comment);
	}
	return rtnStr;
}

static const char *authfile_id_string = "SSH PRIVATE KEY FILE FORMAT 1.1\n";

/* obtain comment from OpenSSHv1 private key, wrapped or clear */
static char *opensshV1PrivComment(
	const unsigned char *key,
	unsigned keyLen)
{
	/* 
	 * Format:
	 * "SSH PRIVATE KEY FILE FORMAT 1.1\n"
	 * 1 byte cipherSpec
	 * 4 byte spares
	 * 4 bytes numBits
	 * bignum n
	 * bignum e
	 * 4 byte comment length 
	 * comment
	 * private key components, possibly encrypted 
	 *
	 * A bignum is encoded like so:
	 * 2 bytes numBits
	 * (numBits + 7)/8 bytes of data
	 */
	/* length: ID string, NULL, Cipher, 4-byte spare */
	size_t len = strlen(authfile_id_string);
	if(keyLen < (len + 6)) {
		return NULL;
	}
	if(memcmp(authfile_id_string, key, len)) {
		return NULL;
	}
	key    += (len + 6);
	keyLen -= (len + 6);
	
	/* key points to numBits */
	if(keyLen < 4) {
		return NULL;
	}
	key += 4;
	keyLen -= 4;
	
	/* key points to n */
	skipBigNum(key, keyLen);
	if(keyLen == 0) {
		return NULL;
	}
	skipBigNum(key, keyLen);
	if(keyLen == 0) {
		return NULL;
	}
	
	char *comment = NULL;
	uint32 commentLen = readUint32(key, keyLen);
	if((commentLen != 0) && (commentLen <= keyLen)) {
		comment = (char *)malloc(commentLen + 1);
		memmove(comment, key, commentLen);
		comment[commentLen] = '\0';
	}
	
	char *rtnStr = genPrintName(SSHv1_PRIV_KEY_NAME, comment);
	if(comment) {
		free(comment);
	}
	return rtnStr;
}

/* 
 * Infer PrintName attribute from raw key's 'comment' field. 
 * Returned string is mallocd and must be freed by caller. 
 */
char *impExpOpensshInferPrintName(
	CFDataRef external, 
	SecExternalItemType externType, 
	SecExternalFormat externFormat)
{
	const unsigned char *key = (const unsigned char *)CFDataGetBytePtr(external);
	unsigned keyLen = (unsigned)CFDataGetLength(external);
	switch(externType) {
		case kSecItemTypePublicKey:
			switch(externFormat) {
				case kSecFormatSSH:
					return opensshV1PubComment(key, keyLen);
				case kSecFormatSSHv2:
					return opensshV2PubComment(key, keyLen);
				default:
					/* impossible, right? */
					break;
			}
			break;
		case kSecItemTypePrivateKey:
			switch(externFormat) {
				case kSecFormatSSH:
				case kSecFormatWrappedSSH:
					return opensshV1PrivComment(key, keyLen);
				default:
					break;
			}
			break;
		default:
			break;
	}
	return NULL;
}

#pragma mark --- Infer DescriptiveData from PrintName ---

/* 
 * Infer DescriptiveData (i.e., comment) from a SecKeyRef's PrintName
 * attribute.
 */
void impExpOpensshInferDescData(
	SecKeyRef keyRef,
	CssmOwnedData &descData)
{
	OSStatus ortn;
	SecKeychainAttributeInfo attrInfo;
	SecKeychainAttrType	attrType = kSecKeyPrintName;
	attrInfo.count = 1;
	attrInfo.tag = &attrType;
	attrInfo.format = NULL;	
	SecKeychainAttributeList *attrList = NULL;
	
	ortn = SecKeychainItemCopyAttributesAndData(
		(SecKeychainItemRef)keyRef, 
		&attrInfo,
		NULL,			// itemClass
		&attrList, 
		NULL,			// don't need the data
		NULL);
	if(ortn) {
		SecSSHDbg("SecKeychainItemCopyAttributesAndData returned %ld", (unsigned long)ortn);
		return;
	}
	/* subsequent errors to errOut: */
	SecKeychainAttribute *attr = attrList->attr;
		
	/*
	 * On a previous import, we would have set this to something like 
	 * "OpenSSHv2 Public Key: comment".
	 * We want to strip off everything up to the actual comment.
	 */
	unsigned toStrip = 0;	
	
	/* min length of attribute value for this code to be meaningful */
	unsigned len = strlen(SSHv2_PUB_KEY_NAME) + 1;	
	char *printNameStr = NULL;
	if(len < attr->length) {
		printNameStr = (char *)malloc(attr->length + 1);
		memmove(printNameStr, attr->data, attr->length);
		printNameStr[attr->length] = '\0';
		if(strstr(printNameStr, SSHv2_PUB_KEY_NAME) == printNameStr) {
			toStrip = strlen(SSHv2_PUB_KEY_NAME);
		}
		else if(strstr(printNameStr, SSHv1_PUB_KEY_NAME) == printNameStr) {
			toStrip = strlen(SSHv1_PUB_KEY_NAME);
		}
		else if(strstr(printNameStr, SSHv1_PRIV_KEY_NAME) == printNameStr) {
			toStrip = strlen(SSHv1_PRIV_KEY_NAME);
		}
		if(toStrip) {
			/* only strip if we have ": " after toStrip bytes */
			if((printNameStr[toStrip] == ':') && (printNameStr[toStrip+1] == ' ')) {
				toStrip += 2;
			}
		}
	}
	if(printNameStr) {
		free(printNameStr);
	}
	len = attr->length;

	unsigned char *attrVal;

	if(len < toStrip) {
		SecSSHDbg("impExpOpensshInferDescData: string parse screwup");
		goto errOut;
	}
	if(len > toStrip) {
		/* Normal case of stripping off leading header */
		len -= toStrip;
	}
	else {
		/* 
		 * If equal, then the attr value *is* "OpenSSHv2 Public Key: " with 
		 * no comment. Not sure how that could happen, but let's be careful.
		 */
		toStrip = 0;
	}
	
	attrVal = ((unsigned char *)attr->data) + toStrip;
	descData.copy(attrVal, len);
errOut:
	SecKeychainItemFreeAttributesAndData(attrList, NULL);
	return;
}

#pragma mark --- Derive SSHv1 wrap/unwrap key ---

/* 
 * Common code to derive a wrap/unwrap key for OpenSSHv1.
 * Caller must CSSM_FreeKey when done.
 */
static CSSM_RETURN openSSHv1DeriveKey(
	CSSM_CSP_HANDLE		cspHand,
	const SecKeyImportExportParameters	*keyParams,		// required 
	impExpVerifyPhrase  verifyPhrase,					// for secure passphrase
	CSSM_KEY_PTR		symKey)							// RETURNED
{
	CSSM_KEY					*passKey = NULL;
	CFDataRef					cfPhrase = NULL;
	CSSM_RETURN					crtn;
	OSStatus					ortn;
	CSSM_DATA					dummyLabel;
	uint32						keyAttr;
	CSSM_CC_HANDLE 				ccHand = 0;
	CSSM_ACCESS_CREDENTIALS		creds;
	CSSM_CRYPTO_DATA			seed;
	CSSM_DATA					nullParam = {0, NULL};
	
	memset(symKey, 0, sizeof(CSSM_KEY));
	
	/* passphrase or passkey? */
	ortn = impExpPassphraseCommon(keyParams, cspHand, SPF_Data, verifyPhrase,
		(CFTypeRef *)&cfPhrase, &passKey);
	if(ortn) {
		return ortn;
	}
	/* subsequent errors to errOut: */

	memset(&seed, 0, sizeof(seed));
	if(cfPhrase != NULL) {
		/* TBD - caller-supplied empty passphrase means "export in the clear" */
		size_t len = CFDataGetLength(cfPhrase);
		seed.Param.Data = (uint8 *)malloc(len);
		seed.Param.Length = len;
		memmove(seed.Param.Data, CFDataGetBytePtr(cfPhrase), len);
		CFRelease(cfPhrase);
	}

	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		CSSM_ALGID_OPENSSH1,
		CSSM_ALGID_OPENSSH1,
		CC_MD5_DIGEST_LENGTH * 8,
		&creds,
		passKey,		// BaseKey
		0,				// iterationCount
		NULL,			// salt
		&seed,
		&ccHand);
	if(crtn) {
		SecSSHDbg("openSSHv1DeriveKey CSSM_CSP_CreateDeriveKeyContext failure");
		goto errOut;
	}
	
	/* not extractable even for the short time this key lives */
	keyAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE;
	dummyLabel.Data = (uint8 *)"temp unwrap key";
	dummyLabel.Length = strlen((char *)dummyLabel.Data);
	
	crtn = CSSM_DeriveKey(ccHand,
		&nullParam,
		CSSM_KEYUSE_ANY,
		keyAttr,
		&dummyLabel,
		NULL,			// cred and acl
		symKey);
	if(crtn) {
		SecSSHDbg("openSSHv1DeriveKey CSSM_DeriveKey failure");
	}
errOut:
	if(ccHand != 0) {
		CSSM_DeleteContext(ccHand);
	}
	if(passKey != NULL) {
		CSSM_FreeKey(cspHand, NULL, passKey, CSSM_FALSE);
		free(passKey);
	}
	if(seed.Param.Data) {
		memset(seed.Param.Data, 0, seed.Param.Length);
		free(seed.Param.Data);
	}
	return crtn;
}

#pragma mark -- OpenSSHv1 Wrap/Unwrap ---

/*
 * If cspHand is provided instead of importKeychain, the CSP 
 * handle MUST be for the CSPDL, not for the raw CSP.
 */
OSStatus impExpWrappedOpenSSHImport(
	CFDataRef							inData,
	SecKeychainRef						importKeychain, // optional
	CSSM_CSP_HANDLE						cspHand,		// required
	SecItemImportExportFlags			flags,
	const SecKeyImportExportParameters	*keyParams,		// optional 
	const char							*printName,
	CFMutableArrayRef					outArray)		// optional, append here 
{
	OSStatus				ortn;
	impExpKeyUnwrapParams   unwrapParams;

	assert(cspHand != 0);
	if(keyParams == NULL) {
		return errSecParam;
	}
	memset(&unwrapParams, 0, sizeof(unwrapParams));

	/* derive unwrapping key */
	CSSM_KEY unwrappingKey;
	
	ortn = openSSHv1DeriveKey(cspHand, keyParams, VP_Import, &unwrappingKey);
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
	hdr.Format = CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1;
	hdr.AlgorithmId = CSSM_ALGID_RSA;		/* the oly algorithm supported in SSHv1 */
	hdr.KeyClass = CSSM_KEYCLASS_PRIVATE_KEY;
	/* LogicalKeySizeInBits : calculated by CSP during unwrap */
	hdr.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	hdr.KeyUsage = CSSM_KEYUSE_ANY;

	wrappedKey.KeyData.Data = (uint8 *)CFDataGetBytePtr(inData);
	wrappedKey.KeyData.Length = CFDataGetLength(inData);
	
	unwrapParams.unwrappingKey = &unwrappingKey;
	unwrapParams.encrAlg = CSSM_ALGID_OPENSSH1;
	
	/* GO */
	ortn =  impExpImportKeyCommon(&wrappedKey, importKeychain, cspHand,
		flags, keyParams, &unwrapParams, printName, outArray);
		
	if(unwrappingKey.KeyData.Data != NULL) {
		CSSM_FreeKey(cspHand, NULL, &unwrappingKey, CSSM_FALSE);
	}
	return ortn;
}

OSStatus impExpWrappedOpenSSHExport(
	SecKeyRef							secKey,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	const CssmData						&descData,
	CFMutableDataRef					outData)		// output appended here
{
	CSSM_CSP_HANDLE cspdlHand = 0;
	OSStatus ortn;
	bool releaseCspHand = false;
	CSSM_RETURN crtn;
	
	if(keyParams == NULL) {
		return errSecParam;
	}

	/* we need a CSPDL handle - try to get it from the key */	
	ortn = SecKeyGetCSPHandle(secKey, &cspdlHand);
	if(ortn) {
		cspdlHand = cuCspStartup(CSSM_FALSE);
		if(cspdlHand == 0) {
			return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
		}
		releaseCspHand = true;
	}
	/* subsequent errors to errOut: */

	/* derive wrapping key */
	CSSM_KEY wrappingKey;
	crtn = openSSHv1DeriveKey(cspdlHand, keyParams, VP_Export, &wrappingKey);
	if(crtn) {
		goto errOut;
	}
	
	/* GO */
	CSSM_KEY wrappedKey;
	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	
	crtn = impExpExportKeyCommon(cspdlHand, secKey, &wrappingKey, &wrappedKey,
		CSSM_ALGID_OPENSSH1, CSSM_ALGMODE_NONE, CSSM_PADDING_NONE,
		CSSM_KEYBLOB_WRAPPED_FORMAT_OPENSSH1, CSSM_ATTRIBUTE_NONE, CSSM_KEYBLOB_RAW_FORMAT_NONE, 
		&descData,
		NULL);	// IV
	if(crtn) {
		goto errOut;
	}
	
	/* the wrappedKey's KeyData is out output */
	CFDataAppendBytes(outData, wrappedKey.KeyData.Data, wrappedKey.KeyData.Length);
	CSSM_FreeKey(cspdlHand, NULL, &wrappedKey, CSSM_FALSE);
	
errOut:
	if(releaseCspHand) {
		cuCspDetachUnload(cspdlHand, CSSM_FALSE);
	}
	return crtn;

}
