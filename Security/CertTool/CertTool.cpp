/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All Rights Reserved.
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
	File:		 CertTool.cpp
	
	Description: certificate manipulation tool

	Author:		 dmitch
*/

#include <Security/Security.h>
#include <Security/certextensions.h>
#include <Security/cssmapple.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <Security/cssmalloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <sys/param.h>
#include <CdsaUtils/cuCdsaUtils.h>
#include <CdsaUtils/cuDbUtils.h>
#include <CdsaUtils/cuPrintCert.h>
#include <CdsaUtils/cuFileIo.h>
#include <CdsaUtils/cuPem.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "CertUI.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Security/utilities.h>
#include <Security/aclclient.h>

/*
 * Workaround flags.
 */
 
/* SecKeychainGetCSPHandle implemented? */
#define SEC_KEYCHAIN_GET_CSP		1

/* SecKeyCreatePair() implemented */
#define SEC_KEY_CREATE_PAIR			1

/* munge Label attr if manually generating or importing keys */
#define MUNGE_LABEL_ATTR			1

#define KC_DB_PATH			"Library/Keychains"		/* relative to home */

/* 
 * defaults for undocumented 'Z' option 
 */
#define ZDEF_KEY_LABEL		"testCert"
#define ZDEF_KEY_ALG		CSSM_ALGID_RSA
#define ZDEF_KEY_SIZE		512
#define ZDEF_KEY_USAGE		(kKeyUseSigning | kKeyUseEncrypting)
#define ZDEF_SIG_ALG		CSSM_ALGID_SHA1WithRSA
#define ZDEF_SIG_OID		CSSMOID_SHA1WithRSA
#define ZDEF_COMMON_NAME	"localhost"
#define ZDEF_ORG_NAME		"Apple Computer - DEBUG ONLY"
#define ZDEF_COUNTRY		"US"
#define ZDEF_STATE			"Washington"
#define ZDEF_CHALLENGE		"someChallenge"

	CSSM_BOOL			verbose = CSSM_FALSE;

static void usage(char **argv)
{
	printf("usage:\n");
	printf("   Create a keypair and cert: %s c [options]\n", argv[0]);
	printf("   Create a CSR:              %s r outFileName [options]\n", 
			argv[0]);
	printf("   Verify a CSR:              %s v infileName [options]\n", argv[0]);
	printf("   Import a certificate:      %s i inFileName [options]\n", argv[0]);
	printf("   Display a certificate:     %s d inFileName [options]\n", argv[0]);
	printf("   Import a CRL:              %s I inFileName [options]\n", argv[0]);
	printf("   Display a CRL:             %s D inFileName [options]\n", argv[0]);
	printf("   Display certs and CRLs in keychain: %s y [options]\n", argv[0]);
	printf("Options:\n");
	printf("   k=keychainName\n");
	printf("   c (create the keychain)\n");
	printf("   p=passphrase (specify passphrase at keychain creation)\n");
	printf("   o=outFileName (create cert command only)\n");
	printf("   v (verbose)\n");
	printf("   d (infile/outfile in DER format; default is PEM)\n");
	printf("   r=privateKeyFileName (optional; for Import Certificate only)\n");
	printf("   f=[18f] (private key format = PKCS1/PKCS8/FIPS186; default is PKCS1\n"
		   "     (openssl) for RSA, openssl for DSA, PKCS8 for Diffie-Hellman\n");
	#if SEC_KEY_CREATE_PAIR
	printf("   a (create key with default ACL)\n");
	#endif
	printf("   h(elp)\n");
	exit(1);
}

static void printError(const char *errDescription,const char *errLocation,OSStatus crtn)
{
	// Show error in text form. If verbose, show location and decimal and hex error values
	int len=64+(errLocation?strlen(errLocation):0);
	if (verbose)
	{
		char *buf=(char *)malloc(len);
		if (errDescription)
			fprintf(stderr,"%s : ",errDescription);
	//	sprintf(buf," %s : %d [0x%x] : ", errLocation,(int)crtn,(unsigned int)crtn);
	//	cuPrintError(buf, crtn);
		cuPrintError(errLocation, crtn);
		free(buf);
	}
	else
	{
		if (errDescription)
			fprintf(stderr,"%s\n",errDescription);
		else
		if (errLocation)
			fprintf(stderr,"%s\n",errLocation);
		else
			fprintf(stderr,"Error: %d [0x%x]\n",(int)crtn,(unsigned int)crtn);
	}
}

#if 	SEC_KEY_CREATE_PAIR
/*
 * Generate a key pair using the SecKeyCreatePair.
 */
static OSStatus generateSecKeyPair(
	SecKeychainRef		kcRef,
	CSSM_ALGORITHMS 	keyAlg,				// e.g., CSSM_ALGID_RSA
	uint32				keySizeInBits,
	CU_KeyUsage			keyUsage,			// CUK_Signing, etc. 
	CSSM_BOOL 			verbose,
	CSSM_KEY_PTR 		*pubKeyPtr,			// RETURNED, owned by Sec layer
	CSSM_KEY_PTR 		*privKeyPtr,		// RETURNED, owned by Sec layer
	SecKeyRef			*pubSecKey,			// caller must release
	SecKeyRef			*privSecKey)		// caller must release
{
	OSStatus ortn;
	CSSM_KEYUSE pubKeyUse = 0;
	CSSM_KEYUSE privKeyUse = 0;
	
	if(keyUsage & kKeyUseSigning) {
		pubKeyUse  |= CSSM_KEYUSE_VERIFY;
		privKeyUse |= CSSM_KEYUSE_SIGN;
	}
	if(keyUsage & kKeyUseEncrypting) {
		pubKeyUse  |= (CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_WRAP);
		privKeyUse |= (CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_UNWRAP);
	}
	ortn = SecKeyCreatePair(kcRef,
		keyAlg, keySizeInBits,
		0,					// contextHandle
		pubKeyUse,
		CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE | 
			CSSM_KEYATTR_RETURN_REF,
		privKeyUse,
		CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF |
			CSSM_KEYATTR_PERMANENT |CSSM_KEYATTR_EXTRACTABLE,
		NULL,		// FIXME - initialAccess
		pubSecKey,
		privSecKey);
	if(ortn) {
		printError("***Error creating key pair",
			"SecKeyCreatePair", ortn);
		cuPrintError("", ortn);
		return ortn;
	}
	
	/* extract CSSM keys for caller */
	ortn = SecKeyGetCSSMKey(*pubSecKey, const_cast<const CSSM_KEY **>(pubKeyPtr));
	if(ortn) {
		printError("***Error extracting public key",
			"SecKeyGetCSSMKey", ortn);
		cuPrintError("", ortn);
	}
	else ortn = SecKeyGetCSSMKey(*privSecKey, const_cast<const CSSM_KEY **>(privKeyPtr));
	if(ortn) {
		printError("***Error extracting private key",
			"SecKeyGetCSSMKey", ortn);
		cuPrintError("", ortn);
	}
	if(ortn) {
		CFRelease(*pubSecKey);
		*pubSecKey = NULL;
		CFRelease(*privSecKey);
		*privSecKey = NULL;
	}
	return ortn;
}
#endif	

/* 
 * Workaround to manually generate a key pair and munge its DB attributes
 * to include the hash of the public key in the private key's Label attr.
 */
#if		MUNGE_LABEL_ATTR

/*
 * Find private key by label, modify its Label attr to be the
 * hash of the associated public key. 
 */
static CSSM_RETURN setPubKeyHash(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_DL_DB_HANDLE 	dlDbHand,
	const char			*keyLabel)		// look up by this
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_DATA						labelData;
	CSSM_HANDLE						resultHand;
	
	labelData.Data = (uint8 *)keyLabel;
	labelData.Length = strlen(keyLabel) + 1;	// incl. NULL
	query.RecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	predicate.DbOperator = CSSM_DB_EQUAL;
	
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = "Label";
	predicate.Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	/* hope this cast is OK */
	predicate.Attribute.Value = &labelData;
	query.SelectionPredicate = &predicate;
	
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0; // CSSM_QUERY_RETURN_DATA;	// FIXME - used?

	/* build Record attribute with one attr */
	CSSM_DB_RECORD_ATTRIBUTE_DATA recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA attr;
	attr.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr.Info.Label.AttributeName = "Label";
	attr.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;

	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	recordAttrs.NumberOfAttributes = 1;
	recordAttrs.AttributeData = &attr;
	
	CSSM_DATA recordData = {NULL, 0};
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		&recordData,	
		&record);
	/* abort only on success */
	if(crtn != CSSM_OK) {
		printError("***setPubKeyHash: can't find private key","CSSM_DL_DataGetFirst",crtn);
		return crtn;
	}
	
	CSSM_KEY_PTR keyToDigest = (CSSM_KEY_PTR)recordData.Data;
	CSSM_DATA_PTR keyDigest = NULL;
	CSSM_CC_HANDLE ccHand;
	crtn = CSSM_CSP_CreatePassThroughContext(cspHand,
	 	keyToDigest,
		&ccHand);
	if(crtn) {
		printError("***Error calculating public key hash. Aborting.",
			"CSSM_CSP_CreatePassThroughContext", crtn);
		return crtn;
	}
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&keyDigest);
	if(crtn) {
		printError("***Error calculating public key hash. Aborting.",
			"CSSM_CSP_PassThrough(PUBKEYHASH)", crtn);
		return -1;
	}
	CSSM_FreeKey(cspHand, NULL, keyToDigest, CSSM_FALSE);
	CSSM_DeleteContext(ccHand);
	
	/* 
	 * Replace Label attr data with hash.
	 * NOTE: the module which allocated this attribute data - a DL -
	 * was loaded and attached by the Sec layer, not by us. Thus 
	 * we can't use the memory allocator functions *we* used when 
	 * attaching to the CSPDL - we have to use the ones
	 * which the Sec layer registered with the DL.
	 */
	CSSM_API_MEMORY_FUNCS memFuncs;
	crtn = CSSM_GetAPIMemoryFunctions(dlDbHand.DLHandle, &memFuncs);
	if(crtn) {
		printError("***Error ","CSSM_GetAPIMemoryFunctions(DLHandle)",crtn);
		/* oh well, leak and continue */
	}
	else {
		memFuncs.free_func(attr.Value->Data, memFuncs.AllocRef);
		memFuncs.free_func(attr.Value, memFuncs.AllocRef);
	}
	attr.Value = keyDigest;
	
	/* modify key attributes */
	crtn = CSSM_DL_DataModify(dlDbHand,
			CSSM_DL_DB_RECORD_PRIVATE_KEY,
			record,
			&recordAttrs,
            NULL,				// DataToBeModified
			CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	if(crtn) {
		printError("***Error setting public key hash. Aborting.",
			"CSSM_DL_DataModify(PUBKEYHASH)", crtn);
		return crtn;
	}
	crtn = CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	if(crtn) {
		printError("***Error while stopping query",
			"CSSM_DL_DataAbortQuery", crtn);
		/* let's keep going in this case */
	}
	crtn = CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	if(crtn) {
		printError("***Error while freeing record",
			"CSSM_DL_FreeUniqueRecord", crtn);
		/* let's keep going in this case */
		crtn = CSSM_OK;
	}
	
	/* free resources */
	cuAppFree(keyDigest->Data, NULL);
	return CSSM_OK;
}
#endif	/* MUNGE_LABEL_ATTR */

/*
 * Generate a key pair using the CSPDL.
 */
static OSStatus generateKeyPair(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_DL_DB_HANDLE 	dlDbHand,
	CSSM_ALGORITHMS 	keyAlg,				// e.g., CSSM_ALGID_RSA
	uint32				keySizeInBits,
	const char 			*keyLabel,			// C string
	CU_KeyUsage			keyUsage,			// CUK_Signing, etc. 
	CSSM_BOOL 			verbose,
	CSSM_KEY_PTR 		*pubKeyPtr,			// mallocd, created, RETURNED
	CSSM_KEY_PTR 		*privKeyPtr)		// mallocd, created, RETURNED
{
	CSSM_KEY_PTR pubKey = reinterpret_cast<CSSM_KEY_PTR>(
		APP_MALLOC(sizeof(CSSM_KEY)));
	CSSM_KEY_PTR privKey = reinterpret_cast<CSSM_KEY_PTR>(
		APP_MALLOC(sizeof(CSSM_KEY)));
	if((pubKey == NULL) || (privKey == NULL)) {
		return memFullErr;
	}
	
	CSSM_RETURN crtn;
	CSSM_KEYUSE pubKeyUse = 0;
	CSSM_KEYUSE privKeyUse = 0;
	
	if(keyUsage & kKeyUseSigning) {
		pubKeyUse  |= CSSM_KEYUSE_VERIFY;
		privKeyUse |= CSSM_KEYUSE_SIGN;
	}
	if(keyUsage & kKeyUseEncrypting) {
		pubKeyUse  |= (CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_WRAP);
		privKeyUse |= (CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_UNWRAP);
	}

	crtn = cuCspGenKeyPair(cspHand,
		&dlDbHand,
		keyAlg,
		keyLabel,
		strlen(keyLabel) + 1,
		keySizeInBits,
		pubKey,
		pubKeyUse,
		CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_REF,
		privKey,
		privKeyUse,
		CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT |
			CSSM_KEYATTR_EXTRACTABLE);
	if(crtn) {
		APP_FREE(pubKey);
		APP_FREE(privKey);
		return paramErr;
	}
	if(verbose) {
		printf("...%u bit key pair generated.\n", 
			(unsigned)keySizeInBits);
	}
	
	#if 	MUNGE_LABEL_ATTR
	/* bind private key to cert by public key hash */
	crtn = setPubKeyHash(cspHand,
		dlDbHand,
		keyLabel);
	if(crtn) {
		printError("***Error setting public key hash. Continuing at peril.",
			"setPubKeyHash", crtn);
	}
	#endif	/* MUNGE_LABEL_ATTR */
	
	*pubKeyPtr = pubKey;
	*privKeyPtr = privKey;
	return noErr;
}

static OSStatus verifyCsr(
	CSSM_CL_HANDLE	clHand,
	const char		*fileName,
	CSSM_BOOL		pemFormat)
{
	unsigned char *csr = NULL;
	unsigned csrLen;
	CSSM_DATA csrData;
	unsigned char *der = NULL;
	unsigned derLen = 0;
	
	if(readFile(fileName, &csr, &csrLen)) {
		printf("***Error reading CSR from file %s. Aborting.\n",
			fileName);
		return ioErr;
	}
	if(pemFormat) {
		int rtn = pemDecode(csr, csrLen, &der, &derLen);
		if(rtn) {
			printf("***%s: Bad PEM formatting. Aborting.\n", fileName);
			return ioErr;
		}
		csrData.Data = der;
		csrData.Length = derLen;
	}
	else {
		csrData.Data = csr;
		csrData.Length = csrLen;
	}
	
	CSSM_RETURN crtn = CSSM_CL_PassThrough(clHand,
		0,			// CCHandle
		CSSM_APPLEX509CL_VERIFY_CSR,
		&csrData,
		NULL);
	if(crtn) {
		printError("***Error verifying CSR","Verify CSR",crtn);
	}
	else {
		printf("...CSR verified successfully.\n");
	}
	if(der) {
		free(der);
	}
	if(csr) {
		free(csr);
	}
	return crtn;
}

typedef enum {
	CC_Cert,
	CC_CRL
} CertOrCrl;

static OSStatus displayCertCRL(
	const char		*fileName,
	CSSM_BOOL		pemFormat,
	CertOrCrl		certOrCrl,
	CSSM_BOOL		verbose)
{
	unsigned char *rawData = NULL;
	unsigned rawDataSize;
	unsigned char *derData = NULL;
	unsigned derDataSize;
	int rtn;

	rtn = readFile(fileName, &rawData, &rawDataSize);
	if(rtn) {
		printf("Error reading %s; aborting.\n", fileName);
		return ioErr;
	}
	if(pemFormat && isPem(rawData, rawDataSize)) {
		/*
		 * Here we cut the user some slack. See if the thing is actually 
		 * PEM encoded and assume DER-encoded if it's not.
		 */
		rtn = pemDecode(rawData, rawDataSize, &derData, &derDataSize);
		if(rtn) {
			printf("***%s: Bad PEM formatting. Aborting.\n", fileName);
			return ioErr;
		}
		rawData = derData;
		rawDataSize = derDataSize;
	}
	if(certOrCrl == CC_Cert) {
		printCert(rawData, rawDataSize, verbose);
	}
	else {
		printCrl(rawData, rawDataSize, verbose);
	}
	if(derData != NULL) {
		free(derData);
	}
	return noErr;
}

static CSSM_RETURN importPrivateKey(
	CSSM_DL_DB_HANDLE	dlDbHand,		
	CSSM_CSP_HANDLE		cspHand,
	const char			*privKeyFileName,
	CSSM_ALGORITHMS		keyAlg,
	CSSM_BOOL			pemFormat,			// of the file
	CSSM_KEYBLOB_FORMAT	keyFormat)			// of the key blob itself, NONE means use 
											//   default
{
	unsigned char 			*derKey = NULL;
	unsigned 				derKeyLen;
	unsigned char 			*pemKey = NULL;
	unsigned 				pemKeyLen;
	CSSM_KEY 				wrappedKey;
	CSSM_KEY 				unwrappedKey;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_CC_HANDLE			ccHand = 0;
	CSSM_RETURN				crtn;
	CSSM_DATA				labelData;
	CSSM_KEYHEADER_PTR		hdr = &wrappedKey.KeyHeader;
	CSSM_DATA 				descData = {0, NULL};
	CSSM_CSP_HANDLE			rawCspHand = 0;
	const char				*privKeyLabel = NULL;
	
	/*
	 * Validate specified format for clarity 
	 */
	switch(keyAlg) {
		case CSSM_ALGID_RSA:
			switch(keyFormat) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					keyFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;	// default
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
					break;
				default:
					printf("***RSA Private key must be in PKCS1 or PKCS8 format\n");
					return CSSMERR_CSSM_INTERNAL_ERROR;
			}
			privKeyLabel = "Imported RSA key";
			break;
		case CSSM_ALGID_DSA:
			switch(keyFormat) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					keyFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;	// default
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
					break;
				default:
					printf("***DSA Private key must be in openssl, FIPS186, "
						"or PKCS8 format\n");
					return CSSMERR_CSSM_INTERNAL_ERROR;
			}
			privKeyLabel = "Imported DSA key";
			break;
		case CSSM_ALGID_DH:
			switch(keyFormat) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					keyFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;	// default
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
					break;
				default:
					printf("***Diffie-Hellman Private key must be in PKCS8 format.\n");
					return CSSMERR_CSSM_INTERNAL_ERROR;
			}
			privKeyLabel = "Imported Diffie-Hellman key";
			break;
	}
	if(readFile(privKeyFileName, &pemKey, &pemKeyLen)) {
		printf("***Error reading private key from file %s. Aborting.\n",
			privKeyFileName);
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
	/* subsequent errors to done: */
	if(pemFormat) {
		int rtn = pemDecode(pemKey, pemKeyLen, &derKey, &derKeyLen);
		if(rtn) {
			printf("***%s: Bad PEM formatting. Aborting.\n", privKeyFileName);
			crtn = CSSMERR_CSP_INVALID_KEY;
			goto done;
		}
	}
	else {
		derKey = pemKey;
		derKeyLen = pemKeyLen;
	}
	
	/* importing a raw key into the CSPDL involves a NULL unwrap */
	memset(&unwrappedKey, 0, sizeof(CSSM_KEY));
	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	
	/* set up the imported key to look like a CSSM_KEY */
	hdr->HeaderVersion 			= CSSM_KEYHEADER_VERSION;
	hdr->BlobType 				= CSSM_KEYBLOB_RAW;
	hdr->AlgorithmId		 	= keyAlg;
	hdr->KeyClass 				= CSSM_KEYCLASS_PRIVATE_KEY;
	hdr->KeyAttr 				= CSSM_KEYATTR_EXTRACTABLE;
	hdr->KeyUsage 				= CSSM_KEYUSE_ANY;
	hdr->Format 				= keyFormat;	
	wrappedKey.KeyData.Data 	= derKey;
	wrappedKey.KeyData.Length 	= derKeyLen;
	
	/* get key size in bits from raw CSP */
	rawCspHand = cuCspStartup(CSSM_TRUE);
	if(rawCspHand == 0) {
		printf("***Error attaching to CSP. Aborting.\n");
		crtn = CSSMERR_CSSM_INTERNAL_ERROR;
		goto done;
	}
	CSSM_KEY_SIZE keySize;
	crtn = CSSM_QueryKeySizeInBits(rawCspHand, NULL, &wrappedKey, &keySize);
	if(crtn) {
		printError("***Error finding size of key","CSSM_QueryKeySizeInBits",crtn);
		goto done;
	}
	hdr->LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;
	
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
		CSSM_ALGID_NONE,			// unwrapAlg
		CSSM_ALGMODE_NONE,			// unwrapMode
		&creds,
		NULL, 						// unwrappingKey
		NULL,						// initVector
		CSSM_PADDING_NONE, 			// unwrapPad
		0,							// Params
		&ccHand);
	if(crtn) {
		printError("***Error creating context","CSSM_CSP_CreateSymmetricContext",crtn);
		goto done;
	}
	
	/* add DL/DB to context */
	CSSM_CONTEXT_ATTRIBUTE newAttr;
	newAttr.AttributeType     = CSSM_ATTRIBUTE_DL_DB_HANDLE;
	newAttr.AttributeLength   = sizeof(CSSM_DL_DB_HANDLE);
	newAttr.Attribute.Data    = (CSSM_DATA_PTR)&dlDbHand;
	crtn = CSSM_UpdateContextAttributes(ccHand, 1, &newAttr);
	if(crtn) {
		printError("***Error updating context attributes","CSSM_UpdateContextAttributes",crtn);
		goto done;
	}
	
	/* do the NULL unwrap */
	labelData.Data = (uint8 *)privKeyLabel;
	labelData.Length = strlen(privKeyLabel) + 1;
	crtn = CSSM_UnwrapKey(ccHand,
		NULL,				// PublicKey
		&wrappedKey,
		CSSM_KEYUSE_ANY,
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE |
			CSSM_KEYATTR_EXTRACTABLE,
		&labelData,
		NULL,				// CredAndAclEntry
		&unwrappedKey,
		&descData);		// required
	if(crtn != CSSM_OK) {
		cuPrintError("CSSM_UnwrapKey", crtn);
		goto done;
	}
	
	/* one more thing: bind this private key to its public key */
	crtn = setPubKeyHash(cspHand, dlDbHand, privKeyLabel);
	
	/* We don't need the unwrapped key any more */
	CSSM_FreeKey(cspHand, 
		NULL,			// access cred
		&unwrappedKey,
		CSSM_FALSE);	// delete 

done:
	if(ccHand) {
		CSSM_DeleteContext(ccHand);
	}
	if(derKey) {
		free(derKey);
	}
	if(pemFormat && pemKey) {
		free(pemKey);
	}
	if(rawCspHand) {
		CSSM_ModuleDetach(rawCspHand);
	}
	return crtn;
}

static OSStatus importCert(
	SecKeychainRef		kcRef,
	CSSM_DL_DB_HANDLE	dlDbHand,		
	CSSM_CSP_HANDLE		cspHand,
	CSSM_CL_HANDLE		clHand,
	const char			*fileName,
	const char			*privKeyFileName,	// optional for importing priv key
	CSSM_BOOL			pemFormat,			// format of files
	CSSM_KEYBLOB_FORMAT	privKeyFormat)		// optional format of priv key 
{
	unsigned char *cert = NULL;
	unsigned certLen;
	CSSM_DATA certData;
	unsigned char *der = NULL;
	unsigned derLen = 0;
	
	if(readFile(fileName, &cert, &certLen)) {
		printf("***Error reading certificate from file %s. Aborting.\n",
			fileName);
		return ioErr;
	}
	if(pemFormat) {
		int rtn = pemDecode(cert, certLen, &der, &derLen);
		if(rtn) {
			printf("***%s: Bad PEM formatting. Aborting.\n", fileName);
			return ioErr;
		}
		certData.Data = der;
		certData.Length = derLen;
	}
	else {
		certData.Data = cert;
		certData.Length = certLen;
	}
	
	SecCertificateRef certRef;
	OSStatus ortn = SecCertificateCreateFromData(
		&certData,
		CSSM_CERT_X_509v3,
		CSSM_CERT_ENCODING_DER,
		&certRef);
	if(ortn) {
		printError("***Error creating certificate","SecCertificateCreateFromData",ortn);
		cuPrintError("", ortn);
		return ortn;
	}
	ortn = SecCertificateAddToKeychain(certRef, kcRef);
	if(ortn) {
		printError("***Error adding certificate to keychain","SecCertificateAddToKeychain",ortn);
		return ortn;
	}

	if(privKeyFileName) {
		/* Importing private key requires algorithm, from cert */
		CSSM_RETURN crtn;
		CSSM_KEY_PTR pubKey;
		crtn = CSSM_CL_CertGetKeyInfo(clHand, &certData, &pubKey);
		if(crtn) {
			printError("***Error obtaining public key from cert. Aborting","CSSM_CL_CertGetKeyInfo",crtn);
			return crtn;
		}
		crtn = importPrivateKey(dlDbHand, cspHand, privKeyFileName, 
			pubKey->KeyHeader.AlgorithmId, pemFormat, privKeyFormat);
		if(crtn) {
			printError("***Error importing private key. Aborting","importPrivateKey",crtn);
			return crtn;
		}
		/* this was mallocd by the CL */
		cuAppFree(pubKey->KeyData.Data, NULL);
		cuAppFree(pubKey, NULL);
	}
	printf("...certificate successfully imported.\n");
	if(der) {
		free(der);
	}
	if(cert) {
		free(cert);
	}
	return noErr;
}

static OSStatus importCRL(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	const char			*fileName,
	CSSM_BOOL			pemFormat)
{
	unsigned char *crl = NULL;
	unsigned crlLen;
	CSSM_DATA crlData;
	unsigned char *der = NULL;
	unsigned derLen = 0;
	
	if(readFile(fileName, &crl, &crlLen)) {
		printf("***Error reading CRL from file %s. Aborting.\n",
			fileName);
		return ioErr;
	}
	if(pemFormat) {
		int rtn = pemDecode(crl, crlLen, &der, &derLen);
		if(rtn) {
			printf("***%s: Bad PEM formatting. Aborting.\n", fileName);
			return ioErr;
		}
		crlData.Data = der;
		crlData.Length = derLen;
	}
	else {
		crlData.Data = crl;
		crlData.Length = crlLen;
	}
	CSSM_RETURN crtn = cuAddCrlToDb(dlDbHand, clHand, &crlData, NULL);
	if(crtn) {
		printError("***Error adding CRL to keychain. Aborting","cuAddCrlToDb",crtn);
	}
	else {
		printf("...CRL successfully imported.\n");
	}
	if(der) {
		free(der);
	}
	if(crl) {
		free(crl);
	}
	return noErr;
}

static OSStatus createCertCsr(
	CSSM_BOOL			createCsr,			// true: CSR, false: Cert
	CSSM_TP_HANDLE		tpHand,				// eventually, a SecKeychainRef
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		subjPubKey,
	CSSM_KEY_PTR		signerPrivKey,
	CSSM_ALGORITHMS 	sigAlg,
	const CSSM_OID		*sigOid,
	CU_KeyUsage			keyUsage,			// kKeyUseSigning, etc. 
	/*
	 * Issuer's RDN is obtained from the issuer cert, if present, or is
	 * assumed to be the same as the subject name (i.e., we're creating 
	 * a self-signed root cert).
	 */ 
	const CSSM_DATA		*issuerCert,
	CSSM_BOOL			useAllDefaults,
	CSSM_DATA_PTR		certData)			// cert or CSR: mallocd and RETURNED
{
	CE_DataAndType 				exts[2];
	CE_DataAndType 				*extp = exts;
	unsigned					numExts;
	
	CSSM_DATA					refId;		// mallocd by CSSM_TP_SubmitCredRequest
	CSSM_APPLE_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET			reqSet;
	sint32						estTime;
	CSSM_BOOL					confirmRequired;
	CSSM_TP_RESULT_SET_PTR		resultSet;
	CSSM_ENCODED_CERT			*encCert;
	CSSM_APPLE_TP_NAME_OID		subjectNames[MAX_NAMES];
	uint32						numNames;
	CSSM_TP_CALLERAUTH_CONTEXT 	CallerAuthContext;
	CSSM_FIELD					policyId;
	
	/* Note a lot of the CSSM_APPLE_TP_CERT_REQUEST fields are not 
	 * used for the createCsr option, but we'll fill in as much as is practical
	 * for either case.
	 */
	if(issuerCert != NULL) {
		printf("createCertCsr: issuerCert not implemented\n");
		return unimpErr;
	}
	
	numExts = 0;
	
	char challengeBuf[400];
	if(createCsr) {
		if(useAllDefaults) {
			strcpy(challengeBuf, ZDEF_CHALLENGE);
		}
		else {
			while(1) {
				getStringWithPrompt("Enter challenge string: ", 
					challengeBuf, sizeof(challengeBuf));
				if(challengeBuf[0] != '\0') {
					break;
				}
			}
		}
		certReq.challengeString = challengeBuf;
	}
	else {
		/* creating cert */
		certReq.challengeString = NULL;
		
		/* KeyUsage extension */
		extp->type = DT_KeyUsage;
		extp->critical = CSSM_FALSE;
		extp->extension.keyUsage = 0;
		if(keyUsage & kKeyUseSigning) {
			extp->extension.keyUsage |= 
				(CE_KU_DigitalSignature | CE_KU_KeyCertSign);
		}
		if(keyUsage & kKeyUseEncrypting) {
			extp->extension.keyUsage |= 
				(CE_KU_KeyEncipherment | CE_KU_DataEncipherment);
		}
		extp++;
		numExts++;
	
		/* BasicConstraints */
		extp->type = DT_BasicConstraints;
		extp->critical = CSSM_TRUE;
		extp->extension.basicConstraints.cA = CSSM_TRUE;
		extp->extension.basicConstraints.pathLenConstraintPresent = CSSM_FALSE;
		extp++;
		numExts++;
	}
	
	/* name array, get from user. */
	if(useAllDefaults) {
		subjectNames[0].string 	= ZDEF_COMMON_NAME;
		subjectNames[0].oid 	= &CSSMOID_CommonName;
		subjectNames[1].string	= ZDEF_ORG_NAME;
		subjectNames[1].oid 	= &CSSMOID_OrganizationName;
		subjectNames[2].string	= ZDEF_COUNTRY;
		subjectNames[2].oid 	= &CSSMOID_CountryName;
		subjectNames[3].string	= ZDEF_STATE;
		subjectNames[3].oid 	= &CSSMOID_StateProvinceName;
		numNames = 4;
	}
	else {
		getNameOids(subjectNames, &numNames);
	}
	
	/* certReq */
	certReq.cspHand = cspHand;
	certReq.clHand = clHand;
	certReq.serialNumber = 0x12345678;		// TBD - random? From user? 
	certReq.numSubjectNames = numNames;
	certReq.subjectNames = subjectNames;
	
	/* TBD - if we're passed in a signing cert, certReq.issuerNameX509 will 
	 * be obtained from that cert. For now we specify "self-signed" cert
	 * by not providing an issuer name at all. */
	certReq.numIssuerNames = 0;				// root for now
	certReq.issuerNames = NULL;
	certReq.issuerNameX509 = NULL;
	certReq.certPublicKey = subjPubKey;
	certReq.issuerPrivateKey = signerPrivKey;
	certReq.signatureAlg = sigAlg;
	certReq.signatureOid = *sigOid;
	certReq.notBefore = 0;					// TBD - from user
	certReq.notAfter = 60 * 60 * 24 * 30;	// seconds from now
	certReq.numExtensions = numExts;
	certReq.extensions = exts;
	
	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;
	
	/* a CSSM_TP_CALLERAUTH_CONTEXT to specify an OID */
	memset(&CallerAuthContext, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	memset(&policyId, 0, sizeof(CSSM_FIELD));
	if(createCsr) {
		policyId.FieldOid = CSSMOID_APPLE_TP_CSR_GEN;
	}
	else {
		policyId.FieldOid = CSSMOID_APPLE_TP_LOCAL_CERT_GEN;
	}
	CallerAuthContext.Policy.NumberOfPolicyIds = 1;
	CallerAuthContext.Policy.PolicyIds = &policyId;

	#if SEC_KEY_CREATE_PAIR
	/* from SUJag */
	CssmClient::AclFactory factory;
	CallerAuthContext.CallerCredentials = 
		const_cast<Security::AccessCredentials *>(factory.promptCred());
	#endif	/* SEC_KEY_CREATE_PAIR */
	
	CSSM_RETURN crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,
		&estTime,
		&refId);
		
	/* before proceeding, free resources allocated thus far */
	if(!useAllDefaults) {
		freeNameOids(subjectNames, numNames);
	}
	
	if(crtn) {
		printError("***Error submitting credential request","CSSM_TP_SubmitCredRequest",crtn);
		return crtn;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		printError("***Error retreiving credential request","CSSM_TP_RetrieveCredResult",crtn);
		return crtn;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult returned NULL result set.\n");
		return ioErr;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	*certData = encCert->CertBlob;
	
	/* free resources allocated by TP */
	APP_FREE(refId.Data);
	APP_FREE(encCert);
	APP_FREE(resultSet);
	return noErr;
}

/* dump all certs & CRLs in a DL/DB */
static OSStatus dumpCrlsCerts(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	CSSM_BOOL			verbose)
{
	CSSM_RETURN crtn;
	unsigned numItems;
	
	crtn = cuDumpCrlsCerts(dlDbHand, clHand, CSSM_TRUE, numItems, verbose);
	if(crtn && (crtn != CSSMERR_DL_INVALID_RECORDTYPE)) {
		/* invalid record type just means "this hasn't been set up
		 * for certs yet". */
		return noErr;
	}
	printf("...%u certificates found\n", numItems);
	crtn = cuDumpCrlsCerts(dlDbHand, clHand, CSSM_FALSE, numItems, verbose);
	if(crtn && (crtn != CSSMERR_DL_INVALID_RECORDTYPE)) {
		/* invalid record type just means "this hasn't been set up
		 * for CRLs yet". */
		return noErr;
	}
	printf("...%u CRLs found\n", numItems);
	return noErr;
}


typedef enum {
	CO_Nop,
	CO_CreateCert,
	CO_CreateCSR,
	CO_VerifyCSR,
	CO_ImportCert,
	CO_DisplayCert,
	CO_ImportCRL,
	CO_DisplayCRL,
	CO_DumpDb		// display certs & CRLs from a DB
} CertOp;

int realmain (int argc, char **argv)
{
	SecKeychainRef 		kcRef = nil;
	char 				kcPath[MAXPATHLEN + 1];
	UInt32 				kcPathLen = MAXPATHLEN + 1;
	CSSM_BOOL			createKc = CSSM_FALSE;
	OSStatus 			ortn;
	CSSM_DL_DB_HANDLE 	dlDbHand = {0, 0};
	CSSM_CSP_HANDLE		cspHand = 0;
	CSSM_TP_HANDLE		tpHand = 0;
	CSSM_CL_HANDLE		clHand = 0;
	CSSM_KEY_PTR		pubKey;
	CSSM_KEY_PTR		privKey;
	int					arg;
	char				*argp;
	CSSM_ALGORITHMS 	keyAlg;
	CSSM_ALGORITHMS 	sigAlg;
	const CSSM_OID		*sigOid;
	CSSM_DATA			certData = {0, NULL};
	CSSM_RETURN			crtn;
	CU_KeyUsage			keyUsage = 0;
	bool				isRoot;
	CSSM_DATA			keyLabel;
	CSSM_BOOL			createCsr = CSSM_FALSE;			// else create cert
	int					optArgs = 0;
	UInt32 				pwdLen = 0;
	Boolean 			promptUser = true;
	char				*allocdPassPhrase = NULL;
	OSStatus			ourRtn = noErr;
	
	/* command line arguments */
	char				*fileName = NULL;
	CSSM_BOOL			pemFormat = CSSM_TRUE;
	CertOp				op = CO_Nop;
	uint32				keySizeInBits;
	char				*kcName = NULL;
	CSSM_BOOL			useAllDefaults = CSSM_FALSE;	// undoc'd cmd option
	char				*passPhrase = NULL;
	const char			*privKeyFileName = NULL;		// optional openssl-style private key
	CSSM_KEYBLOB_FORMAT	privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	#if		SEC_KEY_CREATE_PAIR
	SecKeyRef			pubSecKey = NULL;
	SecKeyRef			privSecKey = NULL;
	#endif
	CSSM_BOOL			useSecKey = CSSM_FALSE; 		// w/default ACL
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'c':
			op = CO_CreateCert;
			optArgs = 2;
			break;
		case 'r':
			if(argc < 3) {
				usage(argv);
			}
			op = CO_CreateCSR;
			createCsr = CSSM_TRUE;
			fileName = argv[2];
			optArgs = 3;
			break;
		case 'v':
			if(argc < 3) {
				usage(argv);
			}
			op = CO_VerifyCSR;
			fileName = argv[2];
			optArgs = 3;
			break;
		case 'i':
			if(argc < 3) {
				usage(argv);
			}
			optArgs = 3;
			op = CO_ImportCert;
			fileName = argv[2];
			break;
		case 'd':
			if(argc < 3) {
				usage(argv);
			}
			op = CO_DisplayCert;
			fileName = argv[2];
			optArgs = 3;
			break;
		case 'I':
			if(argc < 3) {
				usage(argv);
			}
			optArgs = 3;
			op = CO_ImportCRL;
			fileName = argv[2];
			break;
		case 'D':
			if(argc < 3) {
				usage(argv);
			}
			op = CO_DisplayCRL;
			fileName = argv[2];
			optArgs = 3;
			break;
		case 'y':
			op = CO_DumpDb;
			optArgs = 2;
			break;
		default:
			usage(argv);
	}
	for(arg=optArgs; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'k':
				if(argp[1] != '=') {
					usage(argv);
				}
				kcName = &argp[2];
				break;
		    case 'v':
				verbose = CSSM_TRUE;
				break;
			case 'd':
				pemFormat = CSSM_FALSE;
				break;
			case 'c':
				createKc = CSSM_TRUE;
				break;
			case 'p':
				if(argp[1] != '=') {
					usage(argv);
				}
				passPhrase = &argp[2];
				break;
			case 'o':
				if((op != CO_CreateCert) || (argp[1] != '=')){
					usage(argv);
				}
				fileName = &argp[2];
				break;
			case 'r':
				privKeyFileName = &argp[2];
				break;
			case 'f':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case '1':
						privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
						break;
					case '8':
						privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
						break;
					case 'f':
						privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
						break;
					default:usage(argv);
				}
				break;
			case 'Z':
				/* undocumented "use all defaults quickly" option */
				useAllDefaults = CSSM_TRUE;
				break;
			#if SEC_KEY_CREATE_PAIR
			case 'a':
				useSecKey = CSSM_TRUE;
				break;
			#endif
			default:
				usage(argv);
		}
	}
	if((passPhrase != NULL) && !createKc) {
		printf("***passphrase specification only allowed on keychain create. Aborting.\n");
		exit(1);
	}	
	switch(op) {
		case CO_DisplayCert:
			/* ready to roll */
			displayCertCRL(fileName, pemFormat, CC_Cert, verbose);
			return 0;
		case CO_DisplayCRL:
			displayCertCRL(fileName, pemFormat, CC_CRL, verbose);
			return 0;
		default:
			/* proceed */
			break;	
	}
	
	clHand = cuClStartup();
	if(clHand == 0) {
		printf("Error connecting to CL. Aborting.\n");
		exit(1);
	}
	
	/* that's all we need for verifying a CSR */
	if(op == CO_VerifyCSR) {
		ourRtn = verifyCsr(clHand, fileName, pemFormat);
		goto abort;
	}
	
	if(kcName) {
		if(kcName[0] == '/') {
			/* specific keychain not in Library/Keychains */
			strcpy(kcPath, kcName); 
		}
		else {
			char *userHome = getenv("HOME");
		
			if(userHome == NULL) {
				/* well, this is probably not going to work */
				userHome = "";
			}
			sprintf(kcPath, "%s/%s/%s", userHome, KC_DB_PATH, kcName);
		}
	}
	else {
		/* use default keychain */
		ortn = SecKeychainCopyDefault(&kcRef);
		if(ortn) {
			printError("***Error retreiving default keychain","SecKeychainCopyDefault",ortn);
			exit(1);
		}
		ortn = SecKeychainGetPath(kcRef, &kcPathLen, kcPath);
		if(ortn) {
			printError("***Error retreiving default keychain path","SecKeychainGetPath",ortn);
			exit(1);
		}
		
		/* 
		 * OK, we have a path, we have to release the first KC ref, 
		 * then get another one by opening it 
		 */
		CFRelease(kcRef);
	}

	if(passPhrase != NULL) {
		pwdLen = strlen(passPhrase);
		/* work around bug - incoming passphrase gets freed */
		Security::CssmAllocator &alloc = Security::CssmAllocator::standard();
		allocdPassPhrase = (char *)alloc.malloc(pwdLen);
		memmove(allocdPassPhrase, passPhrase, pwdLen);
		promptUser = false;
	}
	if(createKc) {
		ortn = SecKeychainCreate(kcPath,
			pwdLen,
			allocdPassPhrase,
			promptUser,	
			nil,	// initialAccess
			&kcRef);
		/* fixme - do we have to open it? */
		if(ortn) {
			printError("***Error creating keychain","SecKeychainCreate",ortn);
			printf("***Path: %s\n", kcPath);
			exit(1);
		}
	}
	else {
		ortn = SecKeychainOpen(kcPath, &kcRef);
		if(ortn) {
			printError("***Error opening keychain. Aborting","SecKeychainOpen",ortn);
			printf("***Path: %s\n", kcPath);
			exit(1);
		}
	}
	
	/* get associated DL/DB handle */
	ortn = SecKeychainGetDLDBHandle(kcRef, &dlDbHand);
	if(ortn) {
		printError("***Error getting keychain handle","SecKeychainGetDLDBHandle",ortn);
		exit(1);
	}

	#if SEC_KEYCHAIN_GET_CSP
	ortn = SecKeychainGetCSPHandle(kcRef, &cspHand);
	if(ortn) {
		printError("***Error getting keychain CSP handle","SecKeychainGetCSPHandle",ortn);
		exit(1);
	}
	#else
	cspHand = cuCspStartup(CSSM_FALSE);
	if(cspHand == 0) {
		printf("Error connecting to CSP/DL. Aborting.\n");
		exit(1);
	}
	#endif	/* SEC_KEYCHAIN_GET_CSP */
	
	switch(op) {
		case CO_ImportCert:
			ourRtn = importCert(kcRef, dlDbHand, cspHand, clHand, fileName, privKeyFileName, 
				pemFormat, privKeyFormat);
			goto abort;
		case CO_ImportCRL:
			ourRtn = importCRL(dlDbHand, clHand, fileName, pemFormat);
			goto abort;
		case CO_DumpDb:
			ourRtn = dumpCrlsCerts(dlDbHand, clHand, verbose);
			goto abort;
		default:
			break;
	}
	
	/* remaining ops need TP as well */
	tpHand = cuTpStartup();
	if(tpHand == 0) {
		printf("Error connecting to TP. Aborting.\n");
		exit(1);
	}
	
	/*** op = CO_CreateCert or CO_CreateCSR ***/
	
	/*
	 * TBD: eventually we want to present the option of using an existing 
	 * SecIdentityRef from the keychain as the signing cert/key. If none
	 * found or the user says they want a root, we generate the signing key
	 * pair as follows....
	 */
	isRoot = true;
	
	/*
	 * Generate a key pair. For now we do this via CDSA.
	 */
	char labelBuf[200];
	if(useAllDefaults) {
		strcpy(labelBuf, ZDEF_KEY_LABEL);
	}
	else {
		while(1) {
			getStringWithPrompt("Enter key and certificate label: ", labelBuf,
				sizeof(labelBuf));
			if(labelBuf[0] != '\0') {
				break;
			}
		}
	}
	keyLabel.Data = (uint8 *)labelBuf;
	keyLabel.Length = strlen(labelBuf);
	
	/* get key algorithm and size */
	if(useAllDefaults) {
		keyAlg = ZDEF_KEY_ALG;
		keySizeInBits = ZDEF_KEY_SIZE;
	}
	else {
		getKeyParams(keyAlg, keySizeInBits);
	}

	/* get usage for keys and certs */
	if(useAllDefaults) {
		keyUsage = ZDEF_KEY_USAGE;
	}
	else {
		keyUsage = getKeyUsage(isRoot);
	}
	
	printf("...Generating key pair...\n");
	
	if(useSecKey) {
		/* generate keys using SecKeyCreatePair */
		#if		SEC_KEY_CREATE_PAIR
		ourRtn = generateSecKeyPair(kcRef,
			keyAlg,
			keySizeInBits,
			keyUsage,
			verbose,
			&pubKey,
			&privKey,
			&pubSecKey,
			&privSecKey);
		#else
		/* can't happen, useSecKey must be false */
		#endif
	}
	else {
		/* generate keys using CSPDL */
		ourRtn = generateKeyPair(cspHand,
			dlDbHand,
			keyAlg,
			keySizeInBits,
			labelBuf,
			keyUsage,
			verbose,
			&pubKey,
			&privKey);
	}
	if(ourRtn) {
		printError("Error generating keys; aborting","generateKeyPair",ourRtn);
		goto abort;
	}
	
	/* get signing algorithm per the signing key */
	if(useAllDefaults) {
		sigAlg = ZDEF_SIG_ALG;
		sigOid = &ZDEF_SIG_OID;
	}
	else {
		ourRtn = getSigAlg(privKey, sigAlg, sigOid);
		if(ourRtn) {
			printError("Cannot sign with this private key. Aborting","getSigAlg",ourRtn);
			goto abort;
		}
	}
	
	if(createCsr) {
		printf("...creating CSR...\n");
	}
	else {
		printf("...creating certificate...\n");
	}
	/* generate the cert */
	ourRtn = createCertCsr(createCsr,
		tpHand,
		clHand,
		cspHand,
		pubKey,
		privKey,
		sigAlg,
		sigOid,
		keyUsage,
		NULL,		// issuer cert
		useAllDefaults,
		&certData);
	if(ourRtn) {
		goto abort;
	}
	if(verbose) {
		printCert(certData.Data, certData.Length, CSSM_FALSE); 
		printCertShutdown();
	}
	
	if(fileName) {
		/* 
		 * Create CSR, or create cert with outFileName option.
		 * Write results to a file 
		 */
		unsigned char *pem = NULL;
		unsigned pemLen;
		int rtn;
		
		if(pemFormat) {
			char *headerStr;
			switch(op) {
				case CO_CreateCSR:
					headerStr = "CERTIFICATE REQUEST";
					break;
				case CO_CreateCert:
					headerStr = "CERTIFICATE";
					break;
				default:
					printf("***INTERNAL ERROR; aborting.\n");
					exit(1);
			}
			rtn = pemEncode(certData.Data, certData.Length, &pem, &pemLen, headerStr);
			if(rtn) {
				/* very unlikely, I think malloc is the only failure */
				printf("***Error PEM-encoding output. Aborting.\n");
				goto abort;
			}
			rtn = writeFile(fileName, pem, pemLen); 
		}
		else {
			rtn = writeFile(fileName, certData.Data, certData.Length);
		}
		if(rtn) {
			printf("***Error writing CSR to %s\n", fileName);
			ourRtn = ioErr;
		}
		else {
			printf("Wrote %u bytes of CSR to %s\n", (unsigned)certData.Length, 
				fileName);
		}
		if(pem) {
			free(pem);
		}
	}
	if(op == CO_CreateCert) {
		/* store the cert in the same DL/DB as the key pair */
		crtn = cuAddCertToKC(kcRef,
			&certData,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			labelBuf,			// printName
			&keyLabel);
		if(crtn == CSSM_OK) {
			printf("..cert stored in Keychain.\n");
		}
		else {
			printError("Cannot store certificate","cuAddCertToKC",crtn);
			ourRtn = crtn;
		}
	}
abort:
	/* CLEANUP */
	#if		SEC_KEY_CREATE_PAIR
	if(pubSecKey != NULL) {
		CFRelease(pubSecKey);
	}
	if(privSecKey != NULL) {
		CFRelease(privSecKey);
	}
	#endif
	return ourRtn;
}

int main (int argc, char **argv)
{
	try {
		return realmain (argc, argv);
	}
	catch (AbortException e)
	{
		putchar ('\n'); // prompt on the next line.
		return 1;
	}
}

