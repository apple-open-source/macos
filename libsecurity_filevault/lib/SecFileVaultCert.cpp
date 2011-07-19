/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 * SecFileVaultCert.cpp -- Certificate support for FileVault
 */

#include "SecFileVaultCert.h"
#include "srCdsaUtils.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <unistd.h>
#include <fcntl.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeychain.h>
#include <security_utilities/errors.h>

using namespace Security;

#pragma mark -------------------- SecFileVaultCert public implementation --------------------

SecFileVaultCert::SecFileVaultCert()
{
}

SecFileVaultCert::~SecFileVaultCert()
{
}

OSStatus SecFileVaultCert::createPair(CFStringRef hostName,CFStringRef userName,SecKeychainRef keychainRef, CFDataRef *cert)
{
	SecCertificateRef	certRef = NULL;
	CSSM_DL_DB_HANDLE 	dlDbHand = {0, 0};
	CSSM_CSP_HANDLE		cspHand = 0;
	CSSM_TP_HANDLE		tpHand = 0;
	CSSM_CL_HANDLE		clHand = 0;
	CSSM_KEY_PTR		pubKey = NULL;
	CSSM_KEY_PTR		privKey = NULL;
	CSSM_DATA			certData = {0, NULL};
	char 				*hostStr = NULL;
	char				*userStr = NULL;
	OSStatus			ortn;
	CSSM_OID 			algOid = SR_CERT_SIGNATURE_ALG_OID;
    	
	hostStr = srCfStrToCString(hostName);
	userStr = srCfStrToCString(userName);
	if (!hostStr || !userStr)	// probably not ASCII capable
        MacOSError::throwMe(paramErr);
	
	// open keychain, connect to all the CDSA modules we'll need
	
	ortn = SecKeychainGetCSPHandle(keychainRef, &cspHand);
	if (ortn)
        MacOSError::throwMe(ortn);

	ortn = SecKeychainGetDLDBHandle(keychainRef, &dlDbHand);
	if (ortn)
        MacOSError::throwMe(ortn);

	tpHand = srTpStartup();
	if (tpHand == 0)
		MacOSError::throwMe(ioErr);

	clHand = srClStartup();
	if (clHand == 0)
		MacOSError::throwMe(ioErr);
	
	// generate key pair, private key stored in keychain
	ortn = generateKeyPair(cspHand, dlDbHand, SR_KEY_ALGORITHM, SR_KEY_SIZE_IN_BITS,
		"FileVault Master Password Key", &pubKey, &privKey);
	if (ortn)
        MacOSError::throwMe(ortn);

	// generate the cert
	ortn = createRootCert(tpHand,clHand,cspHand,pubKey,privKey,hostStr,userStr,
		SR_CERT_SIGNATURE_ALGORITHM,&algOid,&certData);
	if (ortn)
        MacOSError::throwMe(ortn);

	// store the cert in the same DL/DB as the key pair [see SecCertificateCreateFromData]
	ortn = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &certRef);
	if (ortn)
        MacOSError::throwMe(ortn);

	ortn = SecCertificateAddToKeychain(certRef, keychainRef);
	if (ortn)
        MacOSError::throwMe(ortn);

    CFRelease(certRef);

	// return the cert to caller
    *cert = CFDataCreate(NULL, certData.Data, certData.Length);

    // cleanup
	if (hostStr)
		free(hostStr);
	if (userStr)
		free(userStr);
	if (tpHand)
		CSSM_ModuleDetach(tpHand);
	if (clHand)
		CSSM_ModuleDetach(clHand);
	if (pubKey)
    {
		CSSM_FreeKey(cspHand, 
			NULL,			// access cred
			pubKey,
			CSSM_FALSE);	// delete
		APP_FREE(pubKey);
	}
	if (privKey)
    {
		CSSM_FreeKey(cspHand, 
			NULL,			// access cred
			privKey,
			CSSM_FALSE);	// delete
		APP_FREE(privKey);
	}

	return ortn;
}

#pragma mark -------------------- SecFileVaultCert private implementation --------------------

OSStatus SecFileVaultCert::createRootCert(
	CSSM_TP_HANDLE		tpHand,		
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		subjPubKey,
	CSSM_KEY_PTR		signerPrivKey,
	const char			*hostName,			// CSSMOID_CommonName
	const char 			*userName,			// CSSMOID_Description
	CSSM_ALGORITHMS 	sigAlg,
	const CSSM_OID		*sigOid,
	CSSM_DATA_PTR		certData)			// mallocd and RETURNED
{
	CE_DataAndType 				exts[2];
	CE_DataAndType 				*extp = exts;
	unsigned					numExts;
	CSSM_DATA					refId;		// mallocd by
											//    CSSM_TP_SubmitCredRequest
	CSSM_APPLE_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET			reqSet;
	sint32						estTime;
	CSSM_BOOL					confirmRequired;
	CSSM_TP_RESULT_SET_PTR		resultSet;
	CSSM_ENCODED_CERT			*encCert;
	CSSM_APPLE_TP_NAME_OID		subjectNames[2];
	CSSM_TP_CALLERAUTH_CONTEXT 	CallerAuthContext;
	CSSM_FIELD					policyId;
	
	numExts = 0;
	
	certReq.challengeString = NULL;
	
	/* KeyUsage extension */
	extp->type = DT_KeyUsage;
	extp->critical = CSSM_FALSE;
	extp->extension.keyUsage = CE_KU_DigitalSignature | 
							   CE_KU_KeyCertSign |
							   CE_KU_KeyEncipherment |
							   CE_KU_DataEncipherment;
	extp++;
	numExts++;

	/* BasicConstraints */
	extp->type = DT_BasicConstraints;
	extp->critical = CSSM_TRUE;
	extp->extension.basicConstraints.cA = CSSM_TRUE;
	extp->extension.basicConstraints.pathLenConstraintPresent = CSSM_FALSE;
	extp++;
	numExts++;
	
	/* name array */
	subjectNames[0].string 	= hostName;
	subjectNames[0].oid 	= &CSSMOID_CommonName;
	subjectNames[1].string	= userName;
	subjectNames[1].oid 	= &CSSMOID_Description;

	/* certReq */
	certReq.cspHand = cspHand;
	certReq.clHand = clHand;
	randUint32(certReq.serialNumber);		// random serial number
	certReq.numSubjectNames = 2;
	certReq.subjectNames = subjectNames;
	
	certReq.numIssuerNames = 0;				// root for now
	certReq.issuerNames = NULL;
	certReq.issuerNameX509 = NULL;
	certReq.certPublicKey = subjPubKey;
	certReq.issuerPrivateKey = signerPrivKey;
	certReq.signatureAlg = sigAlg;
	certReq.signatureOid = *sigOid;
	certReq.notBefore = 0;				
	certReq.notAfter = 60 * 60 * 24 * 365;	// seconds from now, one year
	certReq.numExtensions = numExts;
	certReq.extensions = exts;
	
	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;
	
	/* a CSSM_TP_CALLERAUTH_CONTEXT to specify an OID */
	memset(&CallerAuthContext, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	memset(&policyId, 0, sizeof(CSSM_FIELD));
	policyId.FieldOid = CSSMOID_APPLE_TP_LOCAL_CERT_GEN;

	CallerAuthContext.Policy.NumberOfPolicyIds = 1;
	CallerAuthContext.Policy.PolicyIds = &policyId;

	CSSM_RETURN crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,
		&estTime,
		&refId);
		
	if(crtn) {
		printError("***Error submitting credential request", 
			"CSSM_TP_SubmitCredRequest", crtn);
		return crtn;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		printError("***Error retreiving credential request",
			"CSSM_TP_RetrieveCredResult", crtn);
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

/* Convert a reference key to a raw key. */
CSSM_RETURN SecFileVaultCert::refKeyToRaw(
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
		printError("refKeyToRaw: context err",
			"CSSM_CSP_CreateSymmetricContext", crtn);
		return crtn;
	}

	crtn = CSSM_WrapKey(ccHand,
		&creds,
		refKey,
		NULL,			// DescriptiveData
		rawKey);
	if(crtn != CSSM_OK) {
		printError("refKeyToRaw: wrap err", "CSSM_WrapKey", crtn);
		return crtn;
	}
	CSSM_DeleteContext(ccHand);
	return CSSM_OK;
}

/*
 * Find private key by label, modify its Label attr to be the
 * hash of the associated public key. 
 */
CSSM_RETURN SecFileVaultCert::setPubKeyHash(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_DL_DB_HANDLE 	dlDbHand,
	const CSSM_KEY		*pubOrPrivKey,	// to get hash; raw or ref/CSPDL
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
	predicate.Attribute.Info.AttributeFormat = 
		CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	predicate.Attribute.Value = &labelData;
	query.SelectionPredicate = &predicate;
	
	query.QueryLimits.TimeLimit = 0;
	query.QueryLimits.SizeLimit = 1;
	query.QueryFlags = 0; 

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
		printError("***setPubKeyHash: can't find private key",
			"CSSM_DL_DataGetFirst", crtn);
		return crtn;
	}
	
	/* 
	 * If specified key is a ref key, do NULL unwrap for use with raw CSP.
	 * If the CSPDL and SecurityServer support the key digest passthrough 
	 * this is unnecessary.
	 */
	CSSM_KEY rawKeyToDigest;
	if(pubOrPrivKey->KeyHeader.BlobType == CSSM_KEYBLOB_REFERENCE) {
		crtn = refKeyToRaw(cspHand, pubOrPrivKey, &rawKeyToDigest);
		if(crtn) {
			printError("***Error converting public key to raw format", 
				"setPubKeyHash", crtn);
			return crtn;
		}
	}
	else {
		/* use as is */
		rawKeyToDigest = *pubOrPrivKey;
	}
	
	/* connect to raw CSP */
	CSSM_CSP_HANDLE rawCspHand = srCspStartup(CSSM_TRUE);
	if(rawCspHand == 0) {
		printf("***Error connecting to raw CSP; aborting.\n");
		return -1;
	}
	
	/* calculate hash of pub key from private or public part */
	CSSM_DATA_PTR keyDigest = NULL;
	CSSM_CC_HANDLE ccHand;
	crtn = CSSM_CSP_CreatePassThroughContext(rawCspHand,
	 	&rawKeyToDigest,
		&ccHand);
	if(ccHand == 0) {
		printError("***Error calculating public key hash. Aborting:",
			"CSSM_CSP_CreatePassThroughContext", crtn);
		return -1;
	}
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&keyDigest);
	if(crtn) {
		printError("***Error calculating public key hash. Aborting:",
			"CSSM_CSP_PassThrough(PUBKEYHASH)", crtn);				// <<<<<<<<<<<<<<<<<<<
		return crtn;
	}
	if(pubOrPrivKey->KeyHeader.BlobType == CSSM_KEYBLOB_REFERENCE) {
		/* created in refKeyToRaw().... */
		CSSM_FreeKey(cspHand, NULL, &rawKeyToDigest, CSSM_FALSE);
	}
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
		printError("***Error ", "CSSM_GetAPIMemoryFunctions(DLHandle)",
			crtn);
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
		printError("***Error setting public key hash. Aborting",
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
	srAppFree(keyDigest->Data, NULL);	//***
	return CSSM_OK;
}

/*
 * Generate a key pair using the CSPDL.
 */
OSStatus SecFileVaultCert::generateKeyPair(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_DL_DB_HANDLE 	dlDbHand,
	CSSM_ALGORITHMS 	keyAlg,				// e.g., CSSM_ALGID_RSA
	uint32				keySizeInBits,
	const char 			*keyLabel,			// C string
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
	CSSM_KEYUSE pubKeyUse;
	CSSM_KEYUSE privKeyUse;
	
	pubKeyUse = CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_ENCRYPT | 
			CSSM_KEYUSE_WRAP;
	privKeyUse = CSSM_KEYUSE_SIGN | CSSM_KEYUSE_DECRYPT | 
			CSSM_KEYUSE_UNWRAP;

	crtn = srCspGenKeyPair(cspHand,
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
		CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF | 
			CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE);

	if(crtn) {
		APP_FREE(pubKey);
		APP_FREE(privKey);
		return paramErr;
	}

	/* bind private key to cert by public key hash */
	crtn = setPubKeyHash(cspHand,
		dlDbHand,
		pubKey, 
		keyLabel);
	if(crtn) {
		printError("***Error setting public key hash. Continuing at peril",
			"setPubKeyHash", crtn);
	}
	
	*pubKeyPtr = pubKey;
	*privKeyPtr = privKey;
	return noErr;
}

#pragma mark -------------------- utility functions --------------------

void SecFileVaultCert::printError(const char *errDescription,const char *errLocation,OSStatus crtn)
{
	int len = 1;		// trailing NULL in any case
	if(errDescription) {
		len += strlen(errDescription);
	}
	if(errLocation) {
		len += strlen(errLocation);
	}
	char *buf = (char *)malloc(len);
	buf[0] = 0;
	if(errDescription) {
		strcpy(buf, errDescription);
	}
	if(errLocation) {
		strcat(buf, errLocation);
	}
	cssmPerror(buf, crtn);
	free(buf);
}

//	Fill a uint32 with random data
void SecFileVaultCert::randUint32(uint32 &u)
{
	int dev = open("/dev/random", O_RDONLY);
	if(dev < 0) {
		return;
	}
	read(dev, &u, sizeof(u));
	close(dev);
}
