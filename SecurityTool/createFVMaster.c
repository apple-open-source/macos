/*
 * Copyright (c) 2011 Apple, Inc. All Rights Reserved.
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
 * createFVMaster.c
 */

#include "createFVMaster.h"

#include "readline.h"
#include "security.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <Security/SecKeychain.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeychain.h>
#include <Security/oidsalg.h>
#include <Security/oidsattr.h>
#include <limits.h>

#include "srCdsaUtils.h"

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

const char * const _masterKeychainName = "FileVaultMaster.keychain";
const char * const _masterKeychainPath = "./FileVaultMaster";

/*
 * Parameters used to create key pairs and certificates in
 * SR_CertificateAndKeyCreate().
 */
#define SR_KEY_ALGORITHM			CSSM_ALGID_RSA
#define SR_KEY_SIZE_IN_BITS			1024

#define SR2_KEY_SIZE_IN_BITS		2048        // Recommended size for FileVault2 (FDE)

/*
 * The CSSM_ALGORITHMS and OID values defining the signature
 * algorithm in the generated certificate.
 */
#define SR_CERT_SIGNATURE_ALGORITHM	CSSM_ALGID_SHA1WithRSA
#define SR_CERT_SIGNATURE_ALG_OID	CSSMOID_SHA1WithRSA

OSStatus makeMasterPassword(const char *fvmkcName, const char *masterPasswordPassword, uint32 keySizeInBits, SecKeychainRef *keychainRef);

OSStatus createPair(CFStringRef hostName,CFStringRef userName,SecKeychainRef keychainRef, uint32 keySizeInBits, CFDataRef *cert);
OSStatus generateKeyPair(CSSM_CSP_HANDLE cspHand, CSSM_DL_DB_HANDLE dlDbHand, CSSM_ALGORITHMS keyAlg,
    uint32 keySizeInBits, const char *keyLabel, CSSM_KEY_PTR *pubKeyPtr, CSSM_KEY_PTR *privKeyPtr);
OSStatus createRootCert(CSSM_TP_HANDLE tpHand, CSSM_CL_HANDLE clHand, CSSM_CSP_HANDLE cspHand,
    CSSM_KEY_PTR subjPubKey, CSSM_KEY_PTR signerPrivKey, const char *hostName, const char *userName,
    CSSM_ALGORITHMS sigAlg, const CSSM_OID *sigOid, CSSM_DATA_PTR certData);
void randUint32(uint32 *u);

static char *secCopyCString(CFStringRef theString);

OSStatus makeMasterPassword(const char *fvmkcName, const char *masterPasswordPassword, uint32 keySizeInBits, SecKeychainRef *keychainRef)
{
    /*
        OSStatus SecFileVaultMakeMasterPassword(CFStringRef masterPasswordPassword);

        *** In the real code, this will be done directly rather than exec'ing a tool, since there are too many parameters to specify
        *** this needs to be done as root, since the keychain will be a system keychain
        /usr/bin/certtool y c k=/Library/Keychains/FileVaultMaster.keychain p=<masterPasswordPassword>
        /usr/bin/certtool c   k=/Library/Keychains/FileVaultMaster.keychain o=/Library/Keychains/FileVaultMaster.cer
        Two steps: create the keychain, then create the keypair
    */

    SecAccessRef initialAccess = NULL;

    if (!masterPasswordPassword)
    {
        sec_error("You must supply a non-empty password");
        return -2;
    }

    //  We return an error if the keychain already exists
    OSStatus status = SecKeychainCreate(fvmkcName, strlen(masterPasswordPassword), masterPasswordPassword, false, initialAccess, keychainRef);
    if (status!=noErr)
    {
		if (status==errSecDuplicateKeychain || status==CSSMERR_DL_DATASTORE_ALREADY_EXISTS)
            sec_error("The keychain file %s already exists", fvmkcName);
        return status;
    }

    // Create the key pair
    char host[PATH_MAX];
	int rx = gethostname(host, sizeof(host));
    if (rx)
        strcpy(host, "localhost");

    CFStringRef hostName = CFSTR("FileVault Recovery Key");		// This is what shows up in Keychain Access display
    CFStringRef userName = CFStringCreateWithCString(kCFAllocatorDefault, host, kCFStringEncodingUTF8);
    CFDataRef certData = NULL;
    printf("Generating a %d bit key pair; this may take several minutes\n", keySizeInBits);
    status = createPair(hostName,userName,*keychainRef,keySizeInBits, &certData);
    if (status)
        sec_error("Error in createPair: %s", sec_errstr(status));
    if (certData)
        CFRelease(certData);

    return status;
}

OSStatus createPair(CFStringRef hostName,CFStringRef userName,SecKeychainRef keychainRef, uint32 keySizeInBits, CFDataRef *cert)
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

	hostStr = secCopyCString(hostName);
	userStr = secCopyCString(userName);
	if (!hostStr || !userStr)	// could not convert to UTF-8
	{
    	ortn = paramErr;
        goto xit;
    }

	// open keychain, connect to all the CDSA modules we'll need

	ortn = SecKeychainGetCSPHandle(keychainRef, &cspHand);
	if (ortn)
        goto xit;

	ortn = SecKeychainGetDLDBHandle(keychainRef, &dlDbHand);
	if (ortn)
        goto xit;

	tpHand = srTpStartup();
	if (tpHand == 0)
	{
    	ortn = ioErr;
        goto xit;
    }

	clHand = srClStartup();
	if (clHand == 0)
	{
    	ortn = ioErr;
        goto xit;
    }

	// generate key pair, private key stored in keychain
	ortn = generateKeyPair(cspHand, dlDbHand, SR_KEY_ALGORITHM, keySizeInBits,
		"FileVault Master Password Key", &pubKey, &privKey);
	if (ortn)
        goto xit;

	// generate the cert
	ortn = createRootCert(tpHand,clHand,cspHand,pubKey,privKey,hostStr,userStr,
		SR_CERT_SIGNATURE_ALGORITHM,&algOid,&certData);
	if (ortn)
        goto xit;

	// store the cert in the same DL/DB as the key pair [see SecCertificateCreateFromData]
	ortn = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &certRef);
	if (ortn)
        goto xit;

	ortn = SecCertificateAddToKeychain(certRef, keychainRef);
	if (ortn)
        goto xit;

	// return the cert to caller
    *cert = CFDataCreate(NULL, certData.Data, certData.Length);

    // cleanup
xit:
    if (certRef)
        CFRelease(certRef);
    if (certData.Data)
        free(certData.Data);
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

/*
* Given a CFStringRef, this function allocates and returns a pointer
* to a null-terminated 'C' string copy. If conversion of the string
* to UTF8 fails for some reason, the function will return NULL.
*
* The caller must free this pointer
*/

char *secCopyCString(CFStringRef theString)
{
	CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(theString), kCFStringEncodingUTF8) + 1;
	char* buffer = (char*) malloc(maxLength);
	Boolean converted = CFStringGetCString(theString, buffer, maxLength, kCFStringEncodingUTF8);
	if (!converted) {
       free(buffer);
       buffer = NULL;
	}
	return buffer;
}


#pragma mark -------------------- SecFileVaultCert private implementation --------------------

OSStatus createRootCert(
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
	extp->extension.basicConstraints.cA = CSSM_FALSE;
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
	randUint32(&certReq.serialNumber);		// random serial number
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
		sec_error("CSSM_TP_SubmitCredRequest: %s", sec_errstr(crtn));
		goto xit;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		sec_error("CSSM_TP_RetrieveCredResult: %s", sec_errstr(crtn));
		goto xit;
	}
	if(resultSet == NULL) {
		sec_error("CSSM_TP_RetrieveCredResult: returned NULL result set");
		crtn = ioErr;
		goto xit;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
    certData->Length = encCert->CertBlob.Length;
    certData->Data = malloc(encCert->CertBlob.Length);
    if (certData->Data)
        memcpy(certData->Data, encCert->CertBlob.Data, encCert->CertBlob.Length);
	crtn = noErr;

xit:
	/* free resources allocated by TP */
	APP_FREE(refId.Data);
	APP_FREE(encCert->CertBlob.Data);
	APP_FREE(encCert);
	APP_FREE(resultSet);
	return crtn;
}

/* Convert a reference key to a raw key. */
CSSM_RETURN refKeyToRaw(
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
		sec_error("CSSM_CSP_CreateSymmetricContext: refKeyToRaw context err: %s", sec_errstr(crtn));
		return crtn;
	}

	crtn = CSSM_WrapKey(ccHand,
		&creds,
		refKey,
		NULL,			// DescriptiveData
		rawKey);
	if(crtn != CSSM_OK) {
		sec_error("CSSM_WrapKey: refKeyToRaw wrap err: %s", sec_errstr(crtn));
		return crtn;
	}
	CSSM_DeleteContext(ccHand);
	return CSSM_OK;
}

/*
 * Find private key by label, modify its Label attr to be the
 * hash of the associated public key.
 */
CSSM_RETURN setPubKeyHash(
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
		sec_error("CSSM_DL_DataGetFirst: setPubKeyHash: can't find private key: %s", sec_errstr(crtn));
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
            sec_error("setPubKeyHash: Error converting public key to raw format: %s", sec_errstr(crtn));
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
        sec_error("CSSM_CSP_CreatePassThroughContext: Error calculating public key hash. Aborting: %s", sec_errstr(crtn));
		return -1;
	}
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&keyDigest);
	if(crtn) {
        sec_error("CSSM_CSP_PassThrough(PUBKEYHASH): Error calculating public key hash. Aborting: %s", sec_errstr(crtn));
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
        sec_error("CSSM_GetAPIMemoryFunctions(DLHandle): Error: %s", sec_errstr(crtn));
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
        sec_error("CSSM_DL_DataModify(PUBKEYHASH): Error setting public key hash. Aborting: %s", sec_errstr(crtn));
		return crtn;
	}
	crtn = CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	if(crtn) {
        sec_error("CSSM_DL_DataAbortQuery: Error while stopping query: %s", sec_errstr(crtn));
		/* let's keep going in this case */
	}
	crtn = CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	if(crtn) {
        sec_error("CSSM_DL_FreeUniqueRecord: Error while freeing record: %s", sec_errstr(crtn));
		/* let's keep going in this case */
		crtn = CSSM_OK;
	}

	/* free resources */
    if (keyDigest)
    {
        srAppFree(keyDigest->Data, NULL);
        srAppFree(keyDigest, NULL);
    }
	return CSSM_OK;
}

/*
 * Generate a key pair using the CSPDL.
 */
OSStatus generateKeyPair(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_DL_DB_HANDLE 	dlDbHand,
	CSSM_ALGORITHMS 	keyAlg,				// e.g., CSSM_ALGID_RSA
	uint32				keySizeInBits,
	const char 			*keyLabel,			// C string
	CSSM_KEY_PTR 		*pubKeyPtr,			// mallocd, created, RETURNED
	CSSM_KEY_PTR 		*privKeyPtr)		// mallocd, created, RETURNED
{
	CSSM_KEY_PTR pubKey = (CSSM_KEY_PTR)(APP_MALLOC(sizeof(CSSM_KEY)));
	CSSM_KEY_PTR privKey = (CSSM_KEY_PTR)(APP_MALLOC(sizeof(CSSM_KEY)));
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
        sec_error("setPubKeyHash: Error setting public key hash. Continuing at peril: %s", sec_errstr(crtn));
	}

	*pubKeyPtr = pubKey;
	*privKeyPtr = privKey;
	return noErr;
}

//	Fill a uint32 with random data
void randUint32(uint32 *u)
{
	int dev = open("/dev/random", O_RDONLY);
	if(dev < 0) {
		return;
	}
	read(dev, u, sizeof(*u));
	close(dev);
}


//==========================================================================

int
keychain_createMFV(int argc, char * const *argv)
{
	int zero_password = 0;
	char *password = NULL;
    const char *keychainName = NULL;
	int result = 0, ch = 0;
	Boolean do_prompt = FALSE;
    SecKeychainRef keychainRef = NULL;
    uint32 keySizeInBits = SR2_KEY_SIZE_IN_BITS;    // default

/* AG: getopts optstring name [args]
    AG: while loop calling getopt is used to extract password from cl from user
    password is the only option to keychain_create
    optstring  is  a  string  containing the legitimate option
    characters.  If such a character is followed by  a  colon,
    the  option  requires  an  argument,  so  getopt  places a
    pointer to the following text in the same argv-element, or
    the  text  of  the following argv-element, in optarg.
*/
	while ((ch = getopt(argc, argv, "hp:s:P")) != -1)
	{
		switch  (ch)
		{
		case 'p':
			password = optarg;
			break;
		case 'P':
			do_prompt = TRUE;
			break;
        case 's':
        //  Specify the keysize in bits (default 1024)
            keySizeInBits = atoi(optarg);
            if (!(keySizeInBits == SR_KEY_SIZE_IN_BITS || keySizeInBits == SR2_KEY_SIZE_IN_BITS || keySizeInBits == 4096))
                return 2;
            break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
/*
    AG:   The external variable optind is  the  index  of  the  next
       array  element  of argv[] to be processed; it communicates
       from one call of getopt() to the  next  which  element  to
       process.
       The variable optind is the index of the next element of the argv[] vector to 	be processed. It shall be initialized to 1 by the system, and getopt() shall 	update it when it finishes with each element of argv[]. When an element of argv[] 	contains multiple option characters, it is unspecified how getopt() determines 	which options have already been processed.

*/
	argc -= optind;
	argv += optind;

	if (argc > 1)
        return 2; /* @@@ Return 2 triggers usage message. */

    keychainName = (argc == 1)?*argv:_masterKeychainName;
    if (!keychainName || *keychainName == '\0')
        return -1;

	if (!password && !do_prompt)
	{
		int compare = 1;
		int tries;

		for (tries = 3; tries-- > 0;)
		{
			char *firstpass;

			password = getpass("password for new keychain: ");
			if (!password)
			{
				result = -1;
				goto loser;
			}

			firstpass = malloc(strlen(password) + 1);
			strcpy(firstpass, password);
			password = getpass("retype password for new keychain: ");
			compare = password ? strcmp(password, firstpass) : 1;
			memset(firstpass, 0, strlen(firstpass));
			free(firstpass);
			if (!password)
			{
				result = -1;
				goto loser;
			}

			if (compare)
			{
				fprintf(stderr, "passwords don't match\n");
				memset(password, 0, strlen(password));
			}
			else
			{
				zero_password = 1;
				break;
			}
		}

		if (compare)
		{
			result = 1;
			goto loser;
		}
	}

	do
	{
	//	result = do_create(keychain, password, do_prompt);
		result = makeMasterPassword(keychainName, password, keySizeInBits, &keychainRef);
        if (keychainRef)
            CFRelease(keychainRef);
		if (zero_password)
			memset(password, 0, strlen(password));
		if (result)
			goto loser;

		argc--;
		argv++;
        keychainName = *argv;
	} while (argc > 0);

loser:

	return result;
}
