/*
 * Copyright (c) 2001-2003,2011-2012,2014 Apple Inc. All Rights Reserved.
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
	File:		 cuCdsaUtils.cpp
	
	Description: common CDSA access utilities

	Author:		 dmitch
*/

#include "cuCdsaUtils.h"
#include <stdlib.h>
#include <stdio.h>
#include <Security/SecCertificate.h>
#include <Security/cssmapple.h>				/* for cssmPerror() */
#include <Security/oidsalg.h>
#include <Security/TrustSettingsSchema.h>
#include <strings.h>

static CSSM_VERSION vers = {2, 0};
static const CSSM_GUID testGuid = { 0xFADE, 0, 0, { 1,2,3,4,5,6,7,0 }};

/*
 * Standard app-level memory functions required by CDSA.
 */
void * cuAppMalloc (CSSM_SIZE size, void *allocRef) {
	return( malloc(size) );
}

void cuAppFree (void *mem_ptr, void *allocRef) {
	free(mem_ptr);
 	return;
}

void * cuAppRealloc (void *ptr, CSSM_SIZE size, void *allocRef) {
	return( realloc( ptr, size ) );
}

void * cuAppCalloc (uint32 num, CSSM_SIZE size, void *allocRef) {
	return( calloc( num, size ) );
}

static CSSM_API_MEMORY_FUNCS memFuncs = {
	cuAppMalloc,
	cuAppFree,
	cuAppRealloc,
 	cuAppCalloc,
 	NULL
 };
 
CSSM_BOOL cuCompareCssmData(const CSSM_DATA *d1,
	const CSSM_DATA *d2)
{	
	if(d1->Length != d2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(d1->Data, d2->Data, d1->Length)) {
		return CSSM_FALSE;
	}
	return CSSM_TRUE;	
}

/*
 * Init CSSM; returns CSSM_FALSE on error. Reusable.
 */
static CSSM_BOOL cssmInitd = CSSM_FALSE;

CSSM_BOOL cuCssmStartup()
{
	CSSM_RETURN  crtn;
    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
	
	if(cssmInitd) {
		return CSSM_TRUE;
	}  
	crtn = CSSM_Init (&vers, 
		CSSM_PRIVILEGE_SCOPE_NONE,
		&testGuid,
		CSSM_KEY_HIERARCHY_NONE,
		&pvcPolicy,
		NULL /* reserved */);
	if(crtn != CSSM_OK) 
	{
		cuPrintError("CSSM_Init", crtn);
		return CSSM_FALSE;
	}
	else {
		cssmInitd = CSSM_TRUE;
		return CSSM_TRUE;
	}
}

/*
 * Attach to CSP. Returns zero on error.
 */
CSSM_CSP_HANDLE cuCspStartup(
	CSSM_BOOL bareCsp)		// true ==> CSP, false ==> CSP/DL
{
	CSSM_CSP_HANDLE cspHand;
	CSSM_RETURN		crtn;
	const CSSM_GUID *guid;
	
	/* common CSSM init */
	if(cuCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	if(bareCsp) {
		guid = &gGuidAppleCSP;
	}
	else {
		guid = &gGuidAppleCSPDL;
	}
	crtn = CSSM_ModuleLoad(guid,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		cuPrintError("CSSM_ModuleLoad()", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (guid,
		&vers,
		&memFuncs,			// memFuncs
		0,					// SubserviceID
		CSSM_SERVICE_CSP,	
		0,					// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,				// FunctionTable
		0,					// NumFuncTable
		NULL,				// reserved
		&cspHand);
	if(crtn) {
		cuPrintError("CSSM_ModuleAttach()", crtn);
		return 0;
	}
	return cspHand;
}

/* Attach to DL side of CSPDL */
CSSM_DL_HANDLE cuDlStartup()
{
	CSSM_DL_HANDLE 	dlHand = 0;
	CSSM_RETURN		crtn;
	
	if(cuCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleCSPDL,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		cuPrintError("CSSM_ModuleLoad(Apple CSPDL)", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleCSPDL,
		&vers,
		&memFuncs,			// memFuncs
		0,					// SubserviceID
		CSSM_SERVICE_DL,	
		0,					// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,				// FunctionTable
		0,					// NumFuncTable
		NULL,				// reserved
		&dlHand);
	if(crtn) {
		cuPrintError("CSSM_ModuleAttach(Apple CSPDL)", crtn);
		return 0;
	}
	return dlHand;
}

CSSM_CL_HANDLE cuClStartup()
{
	CSSM_CL_HANDLE clHand;
	CSSM_RETURN crtn;
	
	if(cuCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleX509CL,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		cuPrintError("CSSM_ModuleLoad(AppleCL)", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleX509CL,
		&vers,
		&memFuncs,				// memFuncs
		0,						// SubserviceID
		CSSM_SERVICE_CL,		// SubserviceFlags - Where is this used?
		0,						// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,					// FunctionTable
		0,						// NumFuncTable
		NULL,					// reserved
		&clHand);
	if(crtn) {
		cuPrintError("CSSM_ModuleAttach(AppleCL)", crtn);
		return 0;
	}
	else {
		return clHand;
	}
}

CSSM_TP_HANDLE cuTpStartup()
{
	CSSM_TP_HANDLE tpHand;
	CSSM_RETURN crtn;
	
	if(cuCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleX509TP,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		cuPrintError("CSSM_ModuleLoad(AppleTP)", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleX509TP,
		&vers,
		&memFuncs,				// memFuncs
		0,						// SubserviceID
		CSSM_SERVICE_TP,		// SubserviceFlags
		0,						// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,					// FunctionTable
		0,						// NumFuncTable
		NULL,					// reserved
		&tpHand);
	if(crtn) {
		cuPrintError("CSSM_ModuleAttach(AppleTP)", crtn);
		return 0;
	}
	else {
		return tpHand;
	}
}

/* detach and unload */
CSSM_RETURN cuCspDetachUnload(
	CSSM_CSP_HANDLE cspHand,
	CSSM_BOOL bareCsp)					// true ==> CSP, false ==> CSP/DL
{
	CSSM_RETURN crtn = CSSM_ModuleDetach(cspHand);
	if(crtn) {
		return crtn;
	}
	const CSSM_GUID *guid;
	if(bareCsp) {
		guid = &gGuidAppleCSP;
	}
	else {
		guid = &gGuidAppleCSPDL;
	}
	return CSSM_ModuleUnload(guid, NULL, NULL);
}

CSSM_RETURN cuClDetachUnload(
	CSSM_CL_HANDLE  clHand)
{
	CSSM_RETURN crtn = CSSM_ModuleDetach(clHand);
	if(crtn) {
		return crtn;
	}
	return CSSM_ModuleUnload(&gGuidAppleX509CL, NULL, NULL);

}

CSSM_RETURN cuDlDetachUnload(
	CSSM_DL_HANDLE  dlHand)
{
	CSSM_RETURN crtn = CSSM_ModuleDetach(dlHand);
	if(crtn) {
		return crtn;
	}
	return CSSM_ModuleUnload(&gGuidAppleCSPDL, NULL, NULL);

}
CSSM_RETURN cuTpDetachUnload(
	CSSM_TP_HANDLE  tpHand)
{
	CSSM_RETURN crtn = CSSM_ModuleDetach(tpHand);
	if(crtn) {
		return crtn;
	}
	return CSSM_ModuleUnload(&gGuidAppleX509TP, NULL, NULL);

}

/*
 * open a DB, ensure it's empty.
 */
CSSM_DB_HANDLE cuDbStartup(
	CSSM_DL_HANDLE		dlHand,			// from dlStartup()
	const char 			*dbName)
{
	CSSM_DB_HANDLE				dbHand = 0;
	CSSM_RETURN					crtn;
	CSSM_DBINFO					dbInfo;
	
	/* first delete possible existing DB, ignore error */
	crtn = CSSM_DL_DbDelete(dlHand, dbName, NULL, NULL);
	switch(crtn) {
		/* only allowed error is "no such file" */
		case CSSM_OK:
		case CSSMERR_DL_DATASTORE_DOESNOT_EXIST:
			break;
		default:
			cuPrintError("CSSM_DL_DbDelete", crtn);
			return 0;
	}
	
	memset(&dbInfo, 0, sizeof(CSSM_DBINFO));
	
	/* now create it */
	crtn = CSSM_DL_DbCreate(dlHand, 
		dbName,
		NULL,						// DbLocation
		&dbInfo,
		// &Security::KeychainCore::Schema::DBInfo,
		CSSM_DB_ACCESS_PRIVILEGED,
		NULL,						// CredAndAclEntry
		NULL,						// OpenParameters
		&dbHand);
	if(crtn) {
		cuPrintError("CSSM_DL_DbCreate", crtn);
	}
	return dbHand;
}

/*
 * Attach to existing DB or create an empty new one.
 */
CSSM_DB_HANDLE cuDbStartupByName(CSSM_DL_HANDLE dlHand,
	char 		*dbName,
	CSSM_BOOL 	doCreate,
	CSSM_BOOL	quiet)
{
	CSSM_RETURN 	crtn;
	CSSM_DB_HANDLE	dbHand;
	
	/* try open existing DB in either case */
	
	crtn = CSSM_DL_DbOpen(dlHand,
		dbName, 
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&dbHand);
	if(crtn == CSSM_OK) {
		return dbHand;
	}
	if(!doCreate) {
		if(!quiet) {
			printf("***no such data base (%s)\n", dbName);
			cuPrintError("CSSM_DL_DbOpen", crtn);
		}
		return 0;
	}
	/* have to create one */
	return cuDbStartup(dlHand, dbName);
}

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 */
static
CSSM_RETURN cuAddContextAttribute(CSSM_CC_HANDLE CCHandle,
	uint32 AttributeType,
	uint32 AttributeLength,
	const void *AttributePtr)
{
	CSSM_CONTEXT_ATTRIBUTE		newAttr;	
	CSSM_RETURN					crtn;
	
	newAttr.AttributeType     = AttributeType;
	newAttr.AttributeLength   = AttributeLength;
	newAttr.Attribute.Data    = (CSSM_DATA_PTR)AttributePtr;
	crtn = CSSM_UpdateContextAttributes(CCHandle, 1, &newAttr);
	if(crtn) {
		cuPrintError("CSSM_UpdateContextAttributes", crtn);
	}
	return crtn;
}


/*
 * Derive symmetric key.
 * Note in the X CSP, we never return an IV. 
 */
CSSM_RETURN cuCspDeriveKey(CSSM_CSP_HANDLE cspHand,
		uint32				keyAlg,			// CSSM_ALGID_RC5, etc.
		const char 			*keyLabel,
		unsigned 			keyLabelLen,
		uint32 				keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
		uint32 				keySizeInBits,
		CSSM_DATA_PTR		password,		// in PKCS-5 lingo
		CSSM_DATA_PTR		salt,			// ditto
		uint32				iterationCnt,	// ditto
		CSSM_KEY_PTR		key)
{
	CSSM_RETURN					crtn;
	CSSM_CC_HANDLE 				ccHand;
	uint32						keyAttr;
	CSSM_DATA					dummyLabel;
	CSSM_PKCS5_PBKDF2_PARAMS 	pbeParams;
	CSSM_DATA					pbeData;
	CSSM_ACCESS_CREDENTIALS		creds;
	
	memset(key, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		CSSM_ALGID_PKCS5_PBKDF2,
		keyAlg,
		keySizeInBits,
		&creds,
		NULL,			// BaseKey
		iterationCnt,
		salt,
		NULL,			// seed
		&ccHand);
	if(crtn) {
		cuPrintError("CSSM_CSP_CreateDeriveKeyContext", crtn);
		return crtn;
	}
	keyAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_REF | 
			  CSSM_KEYATTR_SENSITIVE;
	dummyLabel.Length = keyLabelLen;
	dummyLabel.Data = (uint8 *)keyLabel;
	
	/* passing in password is pretty strange....*/
	pbeParams.Passphrase = *password;
	pbeParams.PseudoRandomFunction = CSSM_PKCS5_PBKDF2_PRF_HMAC_SHA1;
	pbeData.Data = (uint8 *)&pbeParams;
	pbeData.Length = sizeof(pbeParams);
	crtn = CSSM_DeriveKey(ccHand,
		&pbeData,
		keyUsage,
		keyAttr,
		&dummyLabel,
		NULL,			// cred and acl
		key);
	if(crtn) {
		cuPrintError("CSSM_DeriveKey", crtn);
		return crtn;
	}
	crtn = CSSM_DeleteContext(ccHand);
	if(crtn) {
		cuPrintError("CSSM_DeleteContext", crtn);
	}
	return crtn;
}

/*
 * Generate key pair of arbitrary algorithm. 
 */
 
/* CSP DL currently does not perform DSA generate params; let CSP do it implicitly */
#define DO_DSA_GEN_PARAMS		0

CSSM_RETURN cuCspGenKeyPair(CSSM_CSP_HANDLE cspHand,
	CSSM_DL_DB_HANDLE *dlDbHand,	// optional
	uint32 algorithm,
	const char *keyLabel,
	unsigned keyLabelLen,
	uint32 keySize,					// in bits
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	CSSM_KEYUSE pubKeyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_KEYATTR_FLAGS pubAttrs,	// CSSM_KEYATTR_EXTRACTABLE, etc. 
	CSSM_KEY_PTR privKey,			// mallocd by caller
	CSSM_KEYUSE privKeyUsage,		// CSSM_KEYUSE_DECRYPT, etc.
	CSSM_KEYATTR_FLAGS privAttrs)	// CSSM_KEYATTR_EXTRACTABLE, etc. 
{
	CSSM_RETURN				crtn;
	CSSM_RETURN				ocrtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_DATA				keyLabelData;
	
	keyLabelData.Data        = (uint8 *)keyLabel,
	keyLabelData.Length      = keyLabelLen;
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		algorithm,
		keySize,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		NULL,					// Params
		&ccHand);
	if(crtn) {
		cuPrintError("CSSM_CSP_CreateKeyGenContext", crtn);
		return crtn;
	}

	/* post-context-create algorithm-specific stuff */
	switch(algorithm) {		 
		#if DO_DSA_GEN_PARAMS
		case CSSM_ALGID_DSA:
			/* 
			 * extra step - generate params - this just adds some
			 * info to the context
			 */
			{
				CSSM_DATA dummy = {0, NULL};
				crtn = CSSM_GenerateAlgorithmParams(ccHand, 
					keySize, &dummy);
				if(crtn) {
					cuPrintError("CSSM_GenerateAlgorithmParams", crtn);
					CSSM_DeleteContext(ccHand);
					return crtn;
				}
				cuAppFree(dummy.Data, NULL);
			}
			break;
		#endif	/* DO_DSA_GEN_PARAMS */
		default:
			break;
	}
	
	/* optionally specify DL/DB storage location */
	if(dlDbHand) {
		crtn = cuAddContextAttribute(ccHand, 
			CSSM_ATTRIBUTE_DL_DB_HANDLE,
			sizeof(CSSM_ATTRIBUTE_DL_DB_HANDLE),
			dlDbHand);
		if(crtn) {
			CSSM_DeleteContext(ccHand);
			return crtn;
		}
	}
	ocrtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubAttrs,
		&keyLabelData,
		pubKey,
		privKeyUsage,
		privAttrs,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(ocrtn) {
		cuPrintError("CSSM_GenerateKeyPair", ocrtn);
	}
	crtn = CSSM_DeleteContext(ccHand);
	if(crtn) {
		cuPrintError("CSSM_DeleteContext", crtn);
		if(ocrtn == CSSM_OK) {
			/* error on CSSM_GenerateKeyPair takes precedence */
			ocrtn = crtn;
		}
	}
	return ocrtn;
}


/*
 * Add a certificate to an open Keychain.
 */
CSSM_RETURN cuAddCertToKC(
	SecKeychainRef		keychain,
	const CSSM_DATA		*cert,
	CSSM_CERT_TYPE		certType,
	CSSM_CERT_ENCODING	certEncoding,
	const char			*printName,		// C string
	const CSSM_DATA		*keyLabel)		// ??
{
	SecCertificateRef certificate;
	
	OSStatus rslt = SecCertificateCreateFromData(cert, certType, certEncoding, &certificate);
	if (!rslt)
	{
		rslt = SecCertificateAddToKeychain(certificate, keychain);
		CFRelease(certificate);
	}

	return rslt;
}

/*
 * Convert a CSSM_DATA_PTR, referring to a DER-encoded int, to an
 * unsigned.
 */
unsigned cuDER_ToInt(const CSSM_DATA *DER_Data)
{
	uint32		rtn = 0;
	unsigned	i = 0;

	while(i < DER_Data->Length) {
		rtn |= DER_Data->Data[i];
		if(++i == DER_Data->Length) {
			break;
		}
		rtn <<= 8;
	}
	return rtn;
}

/*
 * Log CSSM error.
 */
void cuPrintError(const char *op, CSSM_RETURN err)
{
	cssmPerror(op, err);
}

/*
 * Verify a CRL against system anchors and intermediate certs. 
 */
CSSM_RETURN cuCrlVerify(
	CSSM_TP_HANDLE			tpHand, 
	CSSM_CL_HANDLE 			clHand,
	CSSM_CSP_HANDLE 		cspHand,
	const CSSM_DATA			*crlData,
	CSSM_DL_DB_HANDLE_PTR	certKeychain,	// intermediate certs
	const CSSM_DATA 		*anchors,		// optional - if NULL, use Trust Settings
	uint32 					anchorCount)
{
	/* main job is building a CSSM_TP_VERIFY_CONTEXT and its components */
	CSSM_TP_VERIFY_CONTEXT			vfyCtx;
	CSSM_TP_CALLERAUTH_CONTEXT		authCtx;
	
	memset(&vfyCtx, 0, sizeof(CSSM_TP_VERIFY_CONTEXT));
	memset(&authCtx, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	
	/* CSSM_TP_CALLERAUTH_CONTEXT components */
	/* 
		typedef struct cssm_tp_callerauth_context {
			CSSM_TP_POLICYINFO Policy;
			CSSM_TIMESTRING VerifyTime;
			CSSM_TP_STOP_ON VerificationAbortOn;
			CSSM_TP_VERIFICATION_RESULTS_CALLBACK CallbackWithVerifiedCert;
			uint32 NumberOfAnchorCerts;
			CSSM_DATA_PTR AnchorCerts;
			CSSM_DL_DB_LIST_PTR DBList;
			CSSM_ACCESS_CREDENTIALS_PTR CallerCredentials;
		} CSSM_TP_CALLERAUTH_CONTEXT, *CSSM_TP_CALLERAUTH_CONTEXT_PTR;
	*/
	CSSM_FIELD	policyId;
	CSSM_APPLE_TP_CRL_OPTIONS crlOpts;
	policyId.FieldOid = CSSMOID_APPLE_TP_REVOCATION_CRL;
	policyId.FieldValue.Data = (uint8 *)&crlOpts;
	policyId.FieldValue.Length = sizeof(crlOpts);
	crlOpts.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
	/* perhaps this should be user-specifiable */
	crlOpts.CrlFlags = CSSM_TP_ACTION_FETCH_CRL_FROM_NET;
	crlOpts.crlStore = NULL;

	authCtx.Policy.NumberOfPolicyIds = 1;
	authCtx.Policy.PolicyIds = &policyId;
	authCtx.Policy.PolicyControl = NULL;
	
	authCtx.VerifyTime = NULL;
	authCtx.VerificationAbortOn = CSSM_TP_STOP_ON_POLICY;
	authCtx.CallbackWithVerifiedCert = NULL;
	
	/* anchors */
	authCtx.NumberOfAnchorCerts = anchorCount;
	authCtx.AnchorCerts = const_cast<CSSM_DATA_PTR>(anchors);
	
	/* DBList of intermediate certs, plus possible System.keychain and 
   	 * system roots */
	CSSM_DL_DB_HANDLE handles[3];
	unsigned numDbs = 0;
	CSSM_DL_HANDLE dlHand = 0;
	if(certKeychain != NULL) {
		handles[0] = *certKeychain;
		numDbs++;
	}
	if(anchors == NULL) {
		/* Trust Settings requires two more DBs */
		if(numDbs == 0) {
			/* new DL handle */
			dlHand = cuDlStartup();
			handles[numDbs].DLHandle = dlHand;
			handles[numDbs + 1].DLHandle = dlHand;
		}
		else {
			/* use the same one passed in for certKeychain */
			handles[numDbs].DLHandle = handles[0].DLHandle;
			handles[numDbs + 1].DLHandle = handles[0].DLHandle;
		}
		handles[numDbs].DBHandle = cuDbStartupByName(handles[numDbs].DLHandle,
			(char*) ADMIN_CERT_STORE_PATH, CSSM_FALSE, CSSM_TRUE);
		numDbs++;
		
		handles[numDbs].DBHandle = cuDbStartupByName(handles[numDbs].DLHandle,
			(char*) SYSTEM_ROOT_STORE_PATH, CSSM_FALSE, CSSM_TRUE);
		numDbs++;
	}
	CSSM_DL_DB_LIST dlDbList;
	dlDbList.DLDBHandle = handles;
	dlDbList.NumHandles = numDbs;
	
	authCtx.DBList = &dlDbList; 
	authCtx.CallerCredentials = NULL;
	
	/* CSSM_TP_VERIFY_CONTEXT */
	vfyCtx.ActionData.Data = NULL;
	vfyCtx.ActionData.Length = 0;
	vfyCtx.Action = CSSM_TP_ACTION_DEFAULT;
	vfyCtx.Cred = &authCtx;

	/* CSSM_APPLE_TP_ACTION_DATA */
	CSSM_APPLE_TP_ACTION_DATA tpAction;
	if(anchors == NULL) {
		/* enable Trust Settings */
		tpAction.Version = CSSM_APPLE_TP_ACTION_VERSION;
		tpAction.ActionFlags = CSSM_TP_ACTION_TRUST_SETTINGS;
		vfyCtx.ActionData.Data   = (uint8 *)&tpAction;
		vfyCtx.ActionData.Length = sizeof(tpAction);
	}
	
	/* cook up CSSM_ENCODED_CRL */
	CSSM_ENCODED_CRL encCrl;
	encCrl.CrlType = CSSM_CRL_TYPE_X_509v2;
	encCrl.CrlEncoding = CSSM_CRL_ENCODING_DER;
	encCrl.CrlBlob = *crlData;
	
	/* CDSA API requires a SignerCertGroup; for us, all the certs are in
	 * certKeyChain... */
	CSSM_CERTGROUP certGroup;
	certGroup.CertType = CSSM_CERT_X_509v1;
	certGroup.CertEncoding = CSSM_CERT_ENCODING_DER;
	certGroup.NumCerts = 0;
	certGroup.GroupList.CertList = NULL;
	certGroup.CertGroupType = CSSM_CERTGROUP_DATA;
	
	CSSM_RETURN crtn = CSSM_TP_CrlVerify(tpHand,
		clHand,
		cspHand,
		&encCrl,
		&certGroup,
		&vfyCtx,
		NULL);			// RevokerVerifyResult
	if(crtn) {
		cuPrintError("CSSM_TP_CrlVerify", crtn);
	}
	if(anchors == NULL) {
		/* close the DBs and maybe the DL we opened */
		unsigned dexToClose = (certKeychain == NULL) ? 0 : 1;
		CSSM_DL_DbClose(handles[dexToClose++]);
		CSSM_DL_DbClose(handles[dexToClose]);
		if(dlHand != 0) {
			cuDlDetachUnload(dlHand);
		}
	}
	return crtn;
}

