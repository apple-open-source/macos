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
#include <Security/SecIdentityPriv.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <sys/param.h>
#include <unistd.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuDbUtils.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuPem.h>
#include <security_utilities/alloc.h>
#include <security_cdsa_client/aclclient.h>
#include <security_utilities/devrandom.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "CertUI.h"
#include <CoreFoundation/CoreFoundation.h>

#define KC_DB_PATH			"Library/Keychains"		/* relative to home */
#define SYSTEM_KDC			"com.apple.kerberos.kdc"

/* 
 * defaults for undocumented 'Z' option 
 */
#define ZDEF_KEY_LABEL		"testCert"
#define ZDEF_KEY_ALG		CSSM_ALGID_RSA
#define ZDEF_KEY_SIZE		512
#define ZDEF_KEY_USAGE		(kKeyUseSigning | kKeyUseEncrypting)
#define ZDEF_SIG_ALG		CSSM_ALGID_SHA256WithRSA
#define ZDEF_SIG_OID		CSSMOID_SHA256WithRSA
#define ZDEF_COMMON_NAME	"localhost"
#define ZDEF_ORG_NAME		"Apple Computer - DEBUG ONLY"
#define ZDEF_COUNTRY		"US"
#define ZDEF_STATE			"Washington"
#define ZDEF_CHALLENGE		"someChallenge"

/* 
 * Key and cert parameters for creating system identity
 */
#define SI_DEF_KEY_LABEL	"System Identity"
#define SI_DEF_KEY_ALG		CSSM_ALGID_RSA
#define SI_DEF_KEY_SIZE		(1024 * 2)
#define SI_DEF_KEY_USAGE	(kKeyUseSigning | kKeyUseEncrypting)
#define SI_DEF_SIG_ALG		CSSM_ALGID_SHA256WithRSA
#define SI_DEF_SIG_OID		CSSMOID_SHA256WithRSA
/* common name = domain */
#define SI_DEF_ORG_NAME		"System Identity"
/* org unit = hostname */
#define SI_DEF_VALIDITY		(60 * 60 * 24 * 365 * 20)	/* 20 years */

/*
 * Default validity
 */
#define DEFAULT_CERT_VALIDITY	(60 * 60 * 24 * 30)		/* 30 days */

/*
 * Validity period environment variable
 * <rdar://problem/16760570> Update certtool to support validate period via an environment variable
 */
#define VALIDITY_DAYS_ENVIRONMENT_VARIABLE	"CERTTOOL_EXPIRATION_DAYS"

	CSSM_BOOL			verbose = CSSM_FALSE;

static void usage(char **argv)
{
	printf("usage:\n");
	printf("   Create a keypair and cert: %s c [options]\n", argv[0]);
	printf("   Create a CSR:              %s r outFileName [options]\n", 
			argv[0]);
	printf("   Verify a CSR:              %s v infileName [options]\n", argv[0]);
	printf("   Create a system Identity:  %s C domainName [options]\n", argv[0]);
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
	printf("   f=[18fo] (private key format = PKCS1/PKCS8/FIPS186; default is PKCS1\n"
		   "     (openssl) for RSA, openssl for DSA, PKCS8 for Diffie-Hellman,\n"
		   "     OpenSSL for ECDSA\n");
	printf("   x=[asSm] (Extended Key Usage: a=Any; s=SSL Client; S=SSL Server; m=SMIME)\n");
	printf("   a (create key with default ACL)\n");
	printf("   u (create key with ACL limiting access to current UID)\n");
	printf("   P (Don't create system identity if one already exists for specified domain)\n");
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

void check_obsolete_keychain(const char *kcName)
{
	if(!strcmp(kcName, "/System/Library/Keychains/X509Anchors")) {
		fprintf(stderr, "***************************************************************\n");
		fprintf(stderr, "                         WARNING\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "The keychain you are accessing, X509Anchors, is no longer\n");
		fprintf(stderr, "used by Mac OS X as the system root certificate store.\n");
		fprintf(stderr, "Please read the security man page for information on the \n");
		fprintf(stderr, "add-trusted-cert command. New system root certificates should\n");
		fprintf(stderr, "be added to the Admin Trust Settings domain and to the \n");
		fprintf(stderr, "System keychain in /Library/Keychains.\n");
		fprintf(stderr, "***************************************************************\n");
	}
	else if(!strcmp(kcName, "/System/Library/Keychains/X509Certificates")) {
		fprintf(stderr, "***************************************************************\n");
		fprintf(stderr, "                         WARNING\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "The keychain you are accessing, X509Certificates, is no longer\n");
		fprintf(stderr, "used by Mac OS X as the system intermediate certificate\n");
		fprintf(stderr, "store. New system intermediate certificates should be added\n");
		fprintf(stderr, "to the System keychain in /Library/Keychains.\n");
		fprintf(stderr, "***************************************************************\n");
	}
}

//
// Create a SecAccessRef with a custom form.
// Both the owner and the ACL set allow free access to the specified,
// UID, but nothing to anyone else.
//
static OSStatus makeUidAccess(uid_t uid, SecAccessRef *rtnAccess)
{
	// make the "uid/gid" ACL subject
	// this is a CSSM_LIST_ELEMENT chain
	CSSM_ACL_PROCESS_SUBJECT_SELECTOR selector = {
		CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION,	// selector version
		CSSM_ACL_MATCH_UID,	// set mask: match uids (only)
		uid,				// uid to match
		0					// gid (not matched here)
	};
	CSSM_LIST_ELEMENT subject2 = { NULL, 0 };
	subject2.Element.Word.Data = (UInt8 *)&selector;
	subject2.Element.Word.Length = sizeof(selector);
	CSSM_LIST_ELEMENT subject1 = {
		&subject2, CSSM_ACL_SUBJECT_TYPE_PROCESS, CSSM_LIST_ELEMENT_WORDID
	};


	// rights granted (replace with individual list if desired)
	CSSM_ACL_AUTHORIZATION_TAG rights[] = {
		CSSM_ACL_AUTHORIZATION_ANY	// everything
	};
	// owner component (right to change ACL)
	CSSM_ACL_OWNER_PROTOTYPE owner = {
		// TypedSubject
		{ CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
		// Delegate
		false
	};
	// ACL entries (any number, just one here)
	CSSM_ACL_ENTRY_INFO acls[] = {
		{
			// prototype
			{
				// TypedSubject
				{ CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
				false,	// Delegate
				// rights for this entry
				{ sizeof(rights) / sizeof(rights[0]), rights },
				// rest is defaulted
			}
		}
	};

	return SecAccessCreateFromOwnerAndACL(&owner,
		sizeof(acls) / sizeof(acls[0]), acls, rtnAccess);
}

static OSStatus setKeyPrintName(
	SecKeyRef keyRef,
	const char *keyName)
{
	UInt32 tag = kSecKeyPrintName;
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 1;
	attrInfo.tag = &tag;
	attrInfo.format = NULL;
	SecKeychainAttributeList *copiedAttrs = NULL;
	
	OSStatus ortn = SecKeychainItemCopyAttributesAndData(
		(SecKeychainItemRef)keyRef, 
		&attrInfo,
		NULL,			// itemClass
		&copiedAttrs, 
		NULL,			// length - don't need the data
		NULL);			// outData
	if(ortn) {
		printError("***Error creating key pair",
			"SecKeychainItemCopyAttributesAndData", ortn);
		return ortn;
	}
	
	SecKeychainAttributeList newAttrList;
	newAttrList.count = 1;
	SecKeychainAttribute newAttr;
	newAttrList.attr = &newAttr;
	newAttr.tag = copiedAttrs->attr->tag;
	newAttr.length = strlen(keyName);
	newAttr.data = (void*)keyName;
	ortn = SecKeychainItemModifyAttributesAndData((SecKeychainItemRef)keyRef, 
		&newAttrList, 0, NULL);
	if(ortn) {
		printError("***Error creating key pair",
			"SecKeychainItemModifyAttributesAndData", ortn);
	}
	SecKeychainItemFreeAttributesAndData(copiedAttrs, NULL);
	return ortn;
}

/*
 * Generate a key pair using SecKeyCreatePair.
 */
static OSStatus generateSecKeyPair(
	SecKeychainRef		kcRef,
	CSSM_ALGORITHMS 	keyAlg,				// e.g., CSSM_ALGID_RSA
	uint32				keySizeInBits,
	CU_KeyUsage			keyUsage,			// CUK_Signing, etc. 
	CSSM_BOOL 			aclForUid,			// ACL for current UID
	CSSM_BOOL 			verbose,
	const char			*keyLabel,			// optional 
	CSSM_KEY_PTR 		*pubKeyPtr,			// RETURNED, owned by Sec layer
	CSSM_KEY_PTR 		*privKeyPtr,		// RETURNED, owned by Sec layer
	SecKeyRef			*pubSecKey,			// caller must release
	SecKeyRef			*privSecKey)		// caller must release
{
	OSStatus ortn;
	CSSM_KEYUSE pubKeyUse = 0;
	CSSM_KEYUSE privKeyUse = 0;
	SecAccessRef secAccess = NULL;
	
	if(aclForUid) {
		ortn = makeUidAccess(geteuid(), &secAccess);
		if(ortn) {
			printError("***Error creating key pair",
				"SecAccessCreateFromOwnerAndACL", ortn);
			return ortn;
		}
	}
	if(keyUsage & kKeyUseSigning) {
		pubKeyUse  |= CSSM_KEYUSE_VERIFY;
		privKeyUse |= CSSM_KEYUSE_SIGN;
	}
	if(keyUsage & kKeyUseEncrypting) {
		pubKeyUse  |= (CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_WRAP);
		privKeyUse |= (CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_UNWRAP);
	}
	if(keyUsage & kKeyUseDerive) {
		pubKeyUse  |= CSSM_KEYUSE_DERIVE;
		privKeyUse |= CSSM_KEYUSE_DERIVE;
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
		secAccess,	
		pubSecKey,
		privSecKey);
	if(secAccess) {
		CFRelease(secAccess);
	}
	if(ortn) {
		printError("***Error creating key pair",
			"SecKeyCreatePair", ortn);
		cuPrintError("", ortn);
		return ortn;
	}
	if(keyLabel != NULL) {
		ortn = setKeyPrintName(*pubSecKey, keyLabel);
		if(ortn) {
			return ortn;
		}
		ortn = setKeyPrintName(*privSecKey, keyLabel);
		if(ortn) {
			return ortn;
		}
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
	predicate.Attribute.Info.Label.AttributeName = (char*) "Label";
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
	attr.Info.Label.AttributeName = (char*) "Label";
	attr.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;

	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	recordAttrs.NumberOfAttributes = 1;
	recordAttrs.AttributeData = &attr;
	
	CSSM_DATA recordData = {0, NULL};
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
	if(keyUsage & kKeyUseDerive) {
		pubKeyUse  |= CSSM_KEYUSE_DERIVE;
		privKeyUse |= CSSM_KEYUSE_DERIVE;
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
	
	/* bind private key to cert by public key hash */
	crtn = setPubKeyHash(cspHand,
		dlDbHand,
		keyLabel);
	if(crtn) {
		printError("***Error setting public key hash. Continuing at peril.",
			"setPubKeyHash", crtn);
	}
	
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
		case CSSM_ALGID_ECDSA:
			switch(keyFormat) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					keyFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;	// default
					break;
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:
					break;
				default:
					printf("***ECDSA Private key must be in PKCS8 or OpenSSLformat.\n");
					return CSSMERR_CSSM_INTERNAL_ERROR;
			}
			privKeyLabel = "Imported ECDSA key";
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
	crtn = CSSM_QueryKeySizeInBits(rawCspHand, CSSM_INVALID_HANDLE, &wrappedKey, &keySize);
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

/* Get validity period */
uint32 notValidAfter(int isSystemDomain)
{
  char *validityEnv = NULL;
  uint32 validAfter = 0;
  int result = 0;
  
  if (isSystemDomain) return SI_DEF_VALIDITY;

  validityEnv = getenv(VALIDITY_DAYS_ENVIRONMENT_VARIABLE);

  if (validityEnv != NULL) {
    result = sscanf(validityEnv, "%u", &validAfter);

    /* could we actually parse it? */
    if (result == 1) {
      /* check that it is between 30 days and 20 years */
      if (validAfter < 30) validAfter = 30;
      if (validAfter > (365 * 20)) validAfter = (365 * 20);
      
      /* convert to seconds, which is what we need to use */
      validAfter *= (60 * 60 * 24);

      return validAfter;
    }
  }
  
  return DEFAULT_CERT_VALIDITY;
}


/* serial number is generated randomly */
#define SERIAL_NUM_LENGTH	4

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
	const CSSM_OID		*extendedKeyUse,	// optional 
	CSSM_BOOL			useAllDefaults,		// secret 'Z' option
	const char			*systemDomain,		// domain name for system identities
	CSSM_DATA_PTR		certData)			// cert or CSR: mallocd and RETURNED
{
	CE_DataAndType 				exts[4];
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
	unsigned char				serialNum[SERIAL_NUM_LENGTH];
	CSSM_BOOL			isSystemKDC = CSSM_FALSE;
	
	/* Note a lot of the CSSM_APPLE_TP_CERT_REQUEST fields are not 
	 * used for the createCsr option, but we'll fill in as much as is practical
	 * for either case.
	 */
	if(issuerCert != NULL) {
		printf("createCertCsr: issuerCert not implemented\n");
		return unimpErr;
	}
	
	numExts = 0;

	if (systemDomain != NULL && strncmp(SYSTEM_KDC, systemDomain, strlen(SYSTEM_KDC)) == 0) {
		isSystemKDC = CSSM_TRUE;
	}
	
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
		if(systemDomain) {
			extp->type = DT_KeyUsage;
			extp->critical = CSSM_FALSE;
			extp->extension.keyUsage = 0;
			if(keyUsage & kKeyUseSigning) {
				extp->extension.keyUsage |= 
					(CE_KU_DigitalSignature);
			}
			if (isSystemKDC) {
			    if(keyUsage & kKeyUseEncrypting) {
				extp->extension.keyUsage |= 
					(CE_KU_KeyEncipherment);
			    }			    
			}
			else {
			    if(keyUsage & kKeyUseEncrypting) {
				extp->extension.keyUsage |= 
					(CE_KU_KeyEncipherment | CE_KU_DataEncipherment);
			    }
			    if(keyUsage & kKeyUseDerive) {
				extp->extension.keyUsage |= CE_KU_KeyAgreement;
			    }
			}
			extp++;
			numExts++;
		}
		
		/* BasicConstraints */
		if(!systemDomain) {
			extp->type = DT_BasicConstraints;
			extp->critical = CSSM_TRUE;
			extp->extension.basicConstraints.cA = CSSM_TRUE;
			extp->extension.basicConstraints.pathLenConstraintPresent = CSSM_FALSE;
			extp++;
			numExts++;
		}
		
		/* Extended Key Usage, optional */
		if (extendedKeyUse != NULL) {
			extp->type = DT_ExtendedKeyUsage;
			extp->critical = CSSM_FALSE;
			extp->extension.extendedKeyUsage.numPurposes = 1;
			extp->extension.extendedKeyUsage.purposes = const_cast<CSSM_OID_PTR>(extendedKeyUse);
			extp++;
			numExts++;
		}

	    if (isSystemKDC) {
		uint8_t	    oidData[] = {0x2B, 0x6, 0x1, 0x5, 0x2, 0x3, 0x5};
		CSSM_OID    oid = {sizeof(oidData), oidData};
		extp->type = DT_ExtendedKeyUsage;
		extp->critical = CSSM_FALSE;
		extp->extension.extendedKeyUsage.numPurposes = 1;
		extp->extension.extendedKeyUsage.purposes = &oid;
		extp++;
		numExts++;
	    }
	    
		
	}
	
	/* name array */
	if(systemDomain) {
		subjectNames[0].string 	= systemDomain;
		subjectNames[0].oid 	= &CSSMOID_CommonName;
		subjectNames[1].string 	= SI_DEF_ORG_NAME;
		subjectNames[1].oid 	= &CSSMOID_OrganizationName;
		numNames = 2;
	}
	else if(useAllDefaults) {
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
	certReq.numSubjectNames = numNames;
	certReq.subjectNames = subjectNames;
	
	/* random serial number */
	try {
		DevRandomGenerator drg;
		drg.random(serialNum, SERIAL_NUM_LENGTH);
		/* MS bit must be zero */
		serialNum[0] &= 0x7f;
		certReq.serialNumber = ((uint32)(serialNum[0])) << 24 |
							   ((uint32)(serialNum[1])) << 16 |
							   ((uint32)(serialNum[2])) << 8 |
							   ((uint32)(serialNum[3]));
	}
	catch(...) {
		certReq.serialNumber = 0x12345678;	
	}
	
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
	certReq.notAfter = notValidAfter((systemDomain == NULL) ? 0 : 1);

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

	CssmClient::AclFactory factory;
	CallerAuthContext.CallerCredentials = 
		const_cast<Security::AccessCredentials *>(factory.promptCred());
	
	CSSM_RETURN crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,
		&estTime,
		&refId);
		
	/* before proceeding, free resources allocated thus far */
	if(!useAllDefaults && (systemDomain == NULL)) {
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
	CO_DumpDb,			// display certs & CRLs from a DB
	CO_SystemIdentity
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
	SecKeyRef			pubSecKey = NULL;
	SecKeyRef			privSecKey = NULL;
	CSSM_BOOL			useSecKey = CSSM_FALSE; 		// w/default ACL
	const CSSM_OID		*extKeyUse = NULL;
	CSSM_BOOL			aclForUid = CSSM_FALSE;			// ACL limited to current uid */
	CSSM_BOOL			avoidDupIdentity = CSSM_FALSE;
	const char			*domainName = NULL;
	CFStringRef			cfDomain = NULL;
	
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
			
		case 'C':
			if(argc < 3) {
				usage(argv);
			}
			op = CO_SystemIdentity;
			domainName = argv[2];
			cfDomain = CFStringCreateWithCString(NULL, 
					domainName, kCFStringEncodingASCII);
			optArgs = 3;
			/* custom ExtendedKeyUse for this type of cert */
			extKeyUse = &CSSMOID_APPLE_EKU_SYSTEM_IDENTITY;
			/* *some* ACL - default, or per-uid (specified later) */
			useSecKey = CSSM_TRUE;
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
					case 'o':
						privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;
						break;
					default:usage(argv);
				}
				break;
			case 'x':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'a':
						extKeyUse = &CSSMOID_ExtendedKeyUsageAny;
						break;
					case 's':
						extKeyUse = &CSSMOID_ClientAuth;
						break;
					case 'S':
						extKeyUse = &CSSMOID_ServerAuth;
						break;
					case 'm':
						extKeyUse = &CSSMOID_EmailProtection;
						break;
					default:
						usage(argv);
				}
				break;
			case 'Z':
				/* undocumented "use all defaults quickly" option */
				useAllDefaults = CSSM_TRUE;
				break;
			case 'a':
				/* default ACL */
				useSecKey = CSSM_TRUE;
				break;
			case 'u':
				/* ACL for uid */
				useSecKey = CSSM_TRUE;
				aclForUid = CSSM_TRUE;
				break;
			case 'P':
				avoidDupIdentity = CSSM_TRUE;
				break;
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
		case CO_SystemIdentity:
			if(avoidDupIdentity) {
				/* 
				 * We're done if there already exists an identity for 
				 * the specified domain. 
				 */
				CFStringRef actualDomain = NULL;
				SecIdentityRef idRef = NULL;
				bool done = false;
				
				OSStatus ortn = SecIdentityCopySystemIdentity(cfDomain,
					&idRef, &actualDomain);
				if(ortn == noErr) {
					if((actualDomain != NULL) && CFEqual(actualDomain, cfDomain)) {
						printf("...System identity already exists for domain %s. Done.\n",
							domainName);
						done = true;
					}
				}
				if(actualDomain) {
					CFRelease(actualDomain);
				}
				if(idRef) {
					CFRelease(idRef);
				}
				if(done) {
					CFRelease(cfDomain);
					return 0;
				}
			}
			break;
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
	
	/* Cook up a keychain path */
	if(op == CO_SystemIdentity) {
		/* this one's implied and hard coded */
		const char *sysKcPath = kSystemKeychainDir  kSystemKeychainName;
		strncpy(kcPath, sysKcPath, MAXPATHLEN);
	}
	else if(kcName) {
		if(kcName[0] == '/') {
			/* specific keychain not in Library/Keychains */
			check_obsolete_keychain(kcName);
			strncpy(kcPath, kcName, MAXPATHLEN); 
		}
		else {
			const char *userHome = getenv("HOME");
		
			if(userHome == NULL) {
				/* well, this is probably not going to work */
				userHome = "";
			}
			snprintf(kcPath, MAXPATHLEN, "%s/%s/%s", userHome, KC_DB_PATH, kcName);
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
		Security::Allocator &alloc = Security::Allocator::standard();
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

	ortn = SecKeychainGetCSPHandle(kcRef, &cspHand);
	if(ortn) {
		printError("***Error getting keychain CSP handle","SecKeychainGetCSPHandle",ortn);
		exit(1);
	}
	
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
	
	/*** op = CO_CreateCert, CO_CreateCSR, CO_SystemIdentity ***/
	
	/*
	 * TBD: eventually we want to present the option of using an existing 
	 * SecIdentityRef from the keychain as the signing cert/key. 
	 */
	isRoot = true;
	
	/*
	 * Generate a key pair - via CDSA if no ACL is requested, else 
	 * SecKeyCreatePair().
	 */
	char labelBuf[200];
	if(op == CO_SystemIdentity) {
		strncpy(labelBuf, domainName, sizeof(labelBuf));
	}
	else if(useAllDefaults) {
		/* the secret 'Z' option */
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
	if(op == CO_SystemIdentity) {
		keyAlg = SI_DEF_KEY_ALG;
		keySizeInBits = SI_DEF_KEY_SIZE;
	}
	else if(useAllDefaults) {
		keyAlg = ZDEF_KEY_ALG;
		keySizeInBits = ZDEF_KEY_SIZE;
	}
	else {
		getKeyParams(keyAlg, keySizeInBits);
	}

	/* get usage for keys and certs */
	if(op == CO_SystemIdentity) {
		keyUsage = SI_DEF_KEY_USAGE;
	}
	else if(useAllDefaults) {
		keyUsage = ZDEF_KEY_USAGE;
	}
	else {
		keyUsage = getKeyUsage(isRoot);
	}
	
	printf("...Generating key pair...\n");
	
	if(useSecKey) {
		/* generate keys using SecKeyCreatePair */
		ourRtn = generateSecKeyPair(kcRef,
			keyAlg,
			keySizeInBits,
			keyUsage,
			aclForUid,
			verbose,
			labelBuf,
			&pubKey,
			&privKey,
			&pubSecKey,
			&privSecKey);
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
	if(op == CO_SystemIdentity) {
		sigAlg = SI_DEF_SIG_ALG;
		sigOid = &SI_DEF_SIG_OID;
	}
	else if(useAllDefaults) {
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
		extKeyUse,
		useAllDefaults,
		domainName,
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
			const char *headerStr;
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
	if((op == CO_CreateCert) || (op == CO_SystemIdentity)) {
		/* store the cert in the same DL/DB as the key pair */
		SecCertificateRef certRef = NULL;
		
		OSStatus ortn = SecCertificateCreateFromData(
			&certData,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			&certRef);
		if(ortn) {
			printError("***Error creating certificate",
				"SecCertificateCreateFromData", ortn);
			cuPrintError("", ortn);
			ourRtn = ortn;
		}
		else {
			ortn = SecCertificateAddToKeychain(certRef, kcRef);
			if(ortn) {
				printError("***Error adding certificate to keychain",
					"SecCertificateAddToKeychain", ortn);
				ourRtn = ortn;
			}
		}
		if(ourRtn == noErr) {
			printf("..cert stored in Keychain.\n");
			if(op == CO_SystemIdentity) {
				/*
				 * Get the SecIdentityRef assocaited with this cert and 
				 * register it 
				 */
				SecIdentityRef idRef;
				ortn = SecIdentityCreateWithCertificate(kcRef, certRef, &idRef);
				if(ortn) {
					printError("Cannot register Identity",
						"SecIdentityCreateWithCertificate", ortn);
				}
				else {
					ortn = SecIdentitySetSystemIdentity(cfDomain, idRef);
					CFRelease(idRef);
					if(ortn) {
						printError("Cannot register Identity",
							"SecIdentitySetSystemIdentity", ortn);
					}
					else {
						printf("..identity registered for domain %s.\n", domainName);
					
					}
				}
				CFRelease(cfDomain);
			}	/* CO_SystemIdentity */
		}	/* cert store successful */
		if(certRef) {
			CFRelease(certRef);
		}
	}	/* generated/stored a cert */
abort:
	/* CLEANUP */
	if(pubSecKey != NULL) {
		CFRelease(pubSecKey);
	}
	if(privSecKey != NULL) {
		CFRelease(privSecKey);
	}
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

