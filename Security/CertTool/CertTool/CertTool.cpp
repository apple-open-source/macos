/*
	File:		 CertTool.cpp
	
	Description: certificate manipulation tool

	Author:		dmitch

	Copyright: 	© Copyright 2002 Apple Computer, Inc. All rights reserved.
	
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple 
	            Computer, Inc. ("Apple") in consideration of your agreement to 
				the following terms, and your use, installation, modification 
				or redistribution of this Apple software constitutes acceptance 
				of these terms.  If you do not agree with these terms, please 
				do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following 
				terms, and subject to these terms, Apple grants you a personal, 
				non-exclusive license, under Apple's copyrights in this 
				original Apple software (the "Apple Software"), to use, 
				reproduce, modify and redistribute the Apple Software, with 
				or without modifications, in source and/or binary forms; 
				provided that if you redistribute the Apple Software in 
				its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all 
				such redistributions of the Apple Software.  Neither the 
				name, trademarks, service marks or logos of Apple Computer, 
				Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from 
				Apple.  Except as expressly stated in this notice, no other 
				rights or licenses, express or implied, are granted by Apple 
				herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works 
				in which the Apple Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  
				APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING 
				WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
				MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, 
				REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE 
				OR IN COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, 
				INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
				LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
				LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
				AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED 
				AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING 
				NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE 
				HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <Security/SecKeychainItem.h>
#include <Security/SecKeychain.h>
#include <Security/certextensions.h>
#include <Security/cssmapple.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <sys/param.h>
#include <cdsaUtils/cdsaUtils.h>
#include <cdsaUtils/printCert.h>
#include <cdsaUtils/fileIo.h>
#include <cdsaUtils/pem.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "CertUI.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Security/utilities.h>

/* will change soon */
#include <Security/SecCertificate.h>

/*
 * Workaround flags.
 */
 
/* SecKeychainGetCSPHandle implemented? */
#define SEC_KEYCHAIN_GET_CSP		0

/* SecCertificateAddToKeychain fully functional? */
#define SEC_CERT_ADD_TO_KC			1

/* SecKeyCreatePair() implemented */
#define SEC_KEY_CREATE_PAIR			0

#if 	!SEC_KEY_CREATE_PAIR
/* munge Label attr if manually generating keys */
#define MUNGE_LABEL_ATTR			1
#endif

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
#define ZDEF_COMMON_NAME	"10.0.61.5"
#define ZDEF_ORG_NAME		"Apple Computer - DEBUG ONLY"
#define ZDEF_COUNTRY		"US"
#define ZDEF_STATE			"Washington"
#define ZDEF_CHALLENGE		"someChallenge"

static void usage(char **argv)
{
	printf("usage:\n");
	printf("   Create a keypair and cert: %s c [options]\n", argv[0]);
	printf("   Create a CSR:              %s r outFileName [options]\n", 
			argv[0]);
	printf("   Verify a CSR:              %s v infileName [options]\n", argv[0]);
	#if		SEC_CERT_ADD_TO_KC
	printf("   Import a certificate:      %s i inFileName [options]\n", argv[0]);
	#else
	/* this one needs the printName */
	printf("   Import a certificate:      %s i inFileName printName [options]\n",
			argv[0]);
	#endif
	printf("   Display a certificate:     %s d inFileName [options]\n", argv[0]);
	printf("Options:\n");
	printf("   k=keychainName\n");
	printf("   c(reate the keychain)\n");
	printf("   v(erbose)\n");
	printf("   d (CSR in DER format; default is PEM)\n");
	printf("   h(elp)\n");
	exit(1);
}

#if 	SEC_KEY_CREATE_PAIR
#error	Work needed to generate key pair using Keychain.
#else	

/* 
 * Workaround to manually generate a key pair and munge its DB attributes
 * to include the hash of the public key in the private key's Label attr.
 */
#if		MUNGE_LABEL_ATTR

/* Convert a reference key to a raw key. */
static CSSM_RETURN refKeyToRaw(
	CSSM_CSP_HANDLE	cspHand,
	const CSSM_KEY	*refKey,	
	CSSM_KEY_PTR	rawKey)			// RETURNED
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(rawKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			CSSM_ALGID_NONE,
			CSSM_ALGMODE_NONE,
			&creds,				// passPhrase
			NULL,				// wrapping key
			NULL,				// init vector
			CSSM_PADDING_NONE,	// Padding
			0,					// Params
			&ccHand);
	if(crtn) {
		showError(crtn, "refKeyToRaw: context err");
		return crtn;
	}
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		refKey,
		NULL,			// DescriptiveData
		rawKey);
	if(crtn != CSSM_OK) {
		showError(crtn, "refKeyToRaw: CSSM_WrapKey");
		return crtn;
	}
	CSSM_DeleteContext(ccHand);
	return CSSM_OK;
}

/*
 * Find private key by label, modify its Label attr to be the
 * hash of the associated public key. 
 */
static CSSM_RETURN setPubKeyHash(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_DL_DB_HANDLE 	dlDbHand,
	const CSSM_KEY		*pubKey,		// to get hash
	CSSM_KEY_PTR		privKey,		// its record gets updated
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
	
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		NULL,			// hopefully optional ...theData,
		&record);
	/* abort only on success */
	if(crtn != CSSM_OK) {
		showError(crtn, "CSSM_DL_DataGetFirst");
		printf("***setPubKeyHash: can't find private key\n");
		return crtn;
	}
	
	/* do NULL unwrap of public key for use with raw CSP */
	CSSM_KEY rawPubKey;
	crtn = refKeyToRaw(cspHand, pubKey, &rawPubKey);
	if(crtn) {
		printf("***Error converting public key to raw format\n");
		return crtn;
	}
	
	/* connect to raw CSP */
	CSSM_CSP_HANDLE rawCspHand = cuCspStartup(CSSM_TRUE);
	if(rawCspHand == 0) {
		printf("***Error connecting to raw CSP; aborting.\n");
		return -1;
	}
	
	/* calculate hash of pub key */
	CSSM_DATA_PTR keyDigest = NULL;
	CSSM_CC_HANDLE ccHand;
	crtn = CSSM_CSP_CreatePassThroughContext(rawCspHand,
	 	&rawPubKey,
		&ccHand);
	if(ccHand == 0) {
		showError(crtn, "CSSM_CSP_CreatePassThroughContext");
		printf("***Error calculating public key hash. Aborting.\n");
		return -1;
	}
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&keyDigest);
	if(crtn) {
		showError(crtn, "CSSM_CSP_PassThrough(PUBKEYHASH)");
		printf("***Error calculating public key hash. Aborting.\n");
		return -1;
	}
	CSSM_FreeKey(cspHand, NULL, &rawPubKey, CSSM_FALSE);
	CSSM_DeleteContext(ccHand);
	CSSM_ModuleDetach(rawCspHand);
	
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
		showError(crtn, "CSSM_GetAPIMemoryFunctions(DLHandle)");
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
		showError(crtn, "CSSM_DL_DataModify(PUBKEYHASH)");
		printf("***Error setting public key hash. Aborting.\n");
		return crtn;
	}
	crtn = CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	if(crtn) {
		showError(crtn, "CSSM_DL_DataAbortQuery");
		/* let's keep going in this case */
	}
	crtn = CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	if(crtn) {
		showError(crtn, "CSSM_DL_FreeUniqueRecord");
		/* let's keep going in this case */
		crtn = CSSM_OK;
	}
	
	/* free resources */
	cuAppFree(keyDigest->Data, NULL);
	return CSSM_OK;
}
#endif	/* MUNGE_LABEL_ATTR */

/* Still on the !SEC_KEY_CREATE_PAIR workaround */

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
		CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
		privKey,
		privKeyUse,
		CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT);
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
		pubKey, 
		privKey, 
		keyLabel);
	if(crtn) {
		printf("***Error setting public key hash. Continuing at peril.\n");
	}
	#endif	/* MUNGE_LABEL_ATTR */
	
	*pubKeyPtr = pubKey;
	*privKeyPtr = privKey;
	return noErr;
}
#endif	/* SEC_KEY_CREATE_PAIR */

static void verifyCsr(
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
		return;
	}
	if(pemFormat) {
		int rtn = pemDecode(csr, csrLen, &der, &derLen);
		if(rtn) {
			printf("***%s: Bad PEM formatting. Aborting.\n", fileName);
			return;
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
		cuPrintError("Verify CSR", crtn);
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
}

static void displayCert(
	const char		*fileName,
	CSSM_BOOL		pemFormat)
{
	unsigned char *rawCert = NULL;
	unsigned rawCertSize;
	unsigned char *derCert = NULL;
	unsigned derCertSize;
	int rtn;

	rtn = readFile(fileName, &rawCert, &rawCertSize);
	if(rtn) {
		printf("Error reading %s; aborting.\n", fileName);
		return;
	}
	if(pemFormat) {
		rtn = pemDecode(rawCert, rawCertSize, &derCert, &derCertSize);
		if(rtn) {
			printf("***%s: Bad PEM formatting. Aborting.\n", fileName);
			return;
		}
		printCert(derCert, derCertSize, CSSM_TRUE);
		free(derCert);
	}
	else {
		printCert(rawCert, rawCertSize, CSSM_TRUE);
	}
}

static void importCert(
	SecKeychainRef		kcRef,			// if SEC_CERT_ADD_TO_KC
	CSSM_DL_DB_HANDLE	dlDbHand,		// otherwise
	const char			*fileName,
	CSSM_BOOL			pemFormat,
	/* cruft needed by cuAddCertToDb */
	const char			*printName)		// C string
{
	unsigned char *cert = NULL;
	unsigned certLen;
	CSSM_DATA certData;
	unsigned char *der = NULL;
	unsigned derLen = 0;
	#if 	!SEC_CERT_ADD_TO_KC
	CSSM_DATA pubKeyHash = {3, (uint8 *)"foo"};
	#endif
	
	if(readFile(fileName, &cert, &certLen)) {
		printf("***Error reading certificate from file %s. Aborting.\n",
			fileName);
		return;
	}
	if(pemFormat) {
		int rtn = pemDecode(cert, certLen, &der, &derLen);
		if(rtn) {
			printf("***%s: Bad PEM formatting. Aborting.\n", fileName);
			return;
		}
		certData.Data = der;
		certData.Length = derLen;
	}
	else {
		certData.Data = cert;
		certData.Length = certLen;
	}
	
	#if SEC_CERT_ADD_TO_KC
	SecCertificateRef certRef;
	OSStatus ortn = SecCertificateCreateFromData(
		&certData,
		CSSM_CERT_X_509v3,
		CSSM_CERT_ENCODING_DER,
		&certRef);
	if(ortn) {
		printf("***SecCertificateCreateFromData returned %d; aborting.\n", 
			(int)ortn);
		return;
	}
	ortn = SecCertificateAddToKeychain(certRef, kcRef);
	if(ortn) {
		printf("***SecCertificateAddToKeychain returned %d; aborting.\n", 
			(int)ortn);
		return;
	}
	#else
	CSSM_RETURN crtn = cuAddCertToDb(dlDbHand,
		&certData,
		CSSM_CERT_X_509v3,
		CSSM_CERT_ENCODING_DER,
		printName,			// printName
		&pubKeyHash);
	if(crtn) {
		printf("***Error adding cert to keychain. Aborting.\n");
		return;
	}
	#endif	/* SEC_CERT_ADD_TO_KC */

	printf("...certificate successfully imported.\n");
	if(der) {
		free(der);
	}
	if(cert) {
		free(cert);
	}
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
	CSSM_DATA_PTR		certData)			// mallocd and RETURNED
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
		cuPrintError("CSSM_TP_SubmitCredRequest", crtn);
		return crtn;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		cuPrintError("CSSM_TP_RetrieveCredResult", crtn);
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

typedef enum {
	CO_Nop,
	CO_CreateCert,
	CO_CreateCSR,
	CO_VerifyCSR,
	CO_ImportCert,
	CO_DisplayCert
} CertOp;

int main(int argc, char **argv)
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
	CSSM_BOOL			verbose = CSSM_FALSE;
	CSSM_ALGORITHMS 	keyAlg;
	CSSM_ALGORITHMS 	sigAlg;
	const CSSM_OID		*sigOid;
	CSSM_DATA			certData = {0, NULL};
	CSSM_RETURN			crtn;
	CU_KeyUsage			keyUsage = 0;
	bool				isRoot;
	CSSM_DATA			keyLabel;
	#if 	!SEC_KEY_CREATE_PAIR && !MUNGE_LABEL_ATTR
	CSSM_DATA			pubKeyHash = {3, (uint8 *)"foo"};
	#endif
	CSSM_BOOL			createCsr = CSSM_FALSE;			// else create cert
	int					optArgs = 0;
	
	/* command line arguments */
	char				*fileName = NULL;
	CSSM_BOOL			pemFormat = CSSM_TRUE;
	char				*certPrintName = NULL;
	CertOp				op = CO_Nop;
	uint32				keySizeInBits;
	char				*kcName = NULL;
	CSSM_BOOL			useAllDefaults = CSSM_FALSE;	// undoc'd cmd option
	
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
			#if	SEC_CERT_ADD_TO_KC
			if(argc < 3) {
				usage(argv);
			}
			optArgs = 3;
			#else
			if(argc < 4) {
				usage(argv);
			}
			certPrintName = argv[3];
			optArgs = 4;
			#endif	/* SEC_CERT_ADD_TO_KC */
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
		default:
			usage(argv);
	}
	for(arg=optArgs; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'k':
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
			case 'Z':
				/* undocumented "use all defaults quickly" option */
				useAllDefaults = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	if(op == CO_DisplayCert) {
		/* ready to roll */
		displayCert(fileName, pemFormat);
		return 0;
	}
	
	clHand = cuClStartup();
	if(clHand == 0) {
		printf("Error connecting to CL. Aborting.\n");
		exit(1);
	}
	
	/* that's all we need for verifying a CSR */
	if(op == CO_VerifyCSR) {
		verifyCsr(clHand, fileName, pemFormat);
		goto abort;
	}
	
	/* remaining ops need TP and CSP as well */
	#if !SEC_KEYCHAIN_GET_CSP
	/* get it from keychain */
	cspHand = cuCspStartup(CSSM_FALSE);
	if(cspHand == 0) {
		printf("Error connecting to CSP/DL. Aborting.\n");
		exit(1);
	}
	#endif
	tpHand = cuTpStartup();
	if(tpHand == 0) {
		printf("Error connecting to TP. Aborting.\n");
		exit(1);
	}
	
	if(kcName) {
		char *userHome = getenv("HOME");
	
		if(userHome == NULL) {
			/* well, this is probably not going to work */
			userHome = "";
		}
		sprintf(kcPath, "%s/%s/%s", userHome, KC_DB_PATH, kcName);
	
	}
	else {
		/* use default keychain */
		ortn = SecKeychainCopyDefault(&kcRef);
		if(ortn) {
			showError(ortn, "SecKeychainCopyDefault");
			exit(1);
		}
		ortn = SecKeychainGetPath(kcRef, &kcPathLen, kcPath);
		if(ortn) {
			showError(ortn, "SecKeychainGetPath");
			exit(1);
		}
		
		/* 
		 * OK, we have a path, we have to release the first KC ref, 
		 * then get another one by opening it 
		 */
		CFRelease(kcRef);
	}
	if(createKc) {
		ortn = SecKeychainCreate(kcPath,
			0,		// no password
			NULL,	// ditto
			true,	// promptUser
			nil,	// initialAccess
			&kcRef);
		/* fixme - do we have to open it? */
		if(ortn) {
			showError(ortn, "SecKeychainCreateNew");
			printf("***Error creating keychain at %s; aborting.\n", kcPath);
			exit(1);
		}
	}
	else {
		ortn = SecKeychainOpen(kcPath, &kcRef);
		if(ortn) {
			showError(ortn, "SecKeychainOpen");
			printf("Cannot open keychain at %s. Aborting.\n", kcPath);
			exit(1);
		}
	}
	
	/* get associated DL/DB handle */
	ortn = SecKeychainGetDLDBHandle(kcRef, &dlDbHand);
	if(ortn) {
		showError(ortn, "SecKeychainGetDLDBHandle");
		exit(1);
	}

	if(op == CO_ImportCert) {
		importCert(kcRef, dlDbHand, fileName, pemFormat, certPrintName);
		goto abort;
	}
	
	#if SEC_KEYCHAIN_GET_CSP
	/* create cert, CSR need CSP handle */
	ortn = SecKeychainGetCSPHandle(kcRef, &cspHand);
	if(ortn) {
		showError(ortn, "SecKeychainGetCSPHandle");
		exit(1);
	}
	#endif
	
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
	ortn = generateKeyPair(cspHand,
		dlDbHand,
		keyAlg,
		keySizeInBits,
		labelBuf,
		keyUsage,
		verbose,
		&pubKey,
		&privKey);
	if(ortn) {
		printf("Error generating keys; aborting.\n");
		goto abort;
	}
	
	/* get signing algorithm per the signing key */
	if(useAllDefaults) {
		sigAlg = ZDEF_SIG_ALG;
		sigOid = &ZDEF_SIG_OID;
	}
	else {
		ortn = getSigAlg(privKey, sigAlg, sigOid);
		if(ortn) {
			printf("Can not sign with this private key. Aborting.\n");
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
	ortn = createCertCsr(createCsr,
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
	if(ortn) {
		goto abort;
	}
	if(verbose) {
		printCert(certData.Data, certData.Length, CSSM_FALSE); 
		printCertShutdown();
	}
	
	if(createCsr) {
		/* just write this to a file */
		unsigned char *pem = NULL;
		unsigned pemLen;
		int rtn;
		
		if(pemFormat) {
			rtn = pemEncode(certData.Data, certData.Length, &pem, &pemLen,
				"CERTIFICATE REQUEST");
			if(rtn) {
				/* very unlikely, I think malloc is the only failure */
				printf("***Error PEM-encoding CSR. Aborting.\n");
				goto abort;
			}
			rtn = writeFile(fileName, pem, pemLen); 
		}
		else {
			rtn = writeFile(fileName, certData.Data, certData.Length);
		}
		if(rtn) {
			printf("***Error writing CSR to %s\n", fileName);
		}
		else {
			printf("Wrote %u bytes of CSR to %s\n", (unsigned)certData.Length, 
				fileName);
		}
		if(pem) {
			free(pem);
		}
	}
	else {
		/* store the cert in the same DL/DB as the key pair */
		#if SEC_CERT_ADD_TO_KC
		crtn = cuAddCertToKC(kcRef,
			&certData,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			labelBuf,			// printName
			&keyLabel);
		#else
		crtn = cuAddCertToDb(dlDbHand,
			&certData,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			labelBuf,			// printName
			&pubKeyHash);
		#endif	/* SEC_CERT_ADD_TO_KC */
		if(crtn == CSSM_OK) {
			printf("..cert stored in Keychain.\n");
		}
	}
abort:
	/* CLEANUP */
	return 0;
}

