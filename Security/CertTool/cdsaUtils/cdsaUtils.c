/*
	File:		 cdsaUtils.c 
	
	Description: common CDSA access utilities

	Author:		dmitch

	Copyright: 	© Copyright 2001 Apple Computer, Inc. All rights reserved.
	
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

#include "cdsaUtils.h"
#include <stdlib.h>
#include <stdio.h>
#include <Security/SecCertificate.h>
#include <strings.h>

static CSSM_VERSION vers = {2, 0};
static const CSSM_GUID testGuid = { 0xFADE, 0, 0, { 1,2,3,4,5,6,7,0 }};

/*
 * Standard app-level memory functions required by CDSA.
 */
void * cuAppMalloc (uint32 size, void *allocRef) {
	return( malloc(size) );
}

void cuAppFree (void *mem_ptr, void *allocRef) {
	free(mem_ptr);
 	return;
}

void * cuAppRealloc (void *ptr, uint32 size, void *allocRef) {
	return( realloc( ptr, size ) );
}

void * cuAppCalloc (uint32 num, uint32 size, void *allocRef) {
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
 * Add a certificate to an open DLDB.
 */
CSSM_RETURN cuAddCertToDb(
	CSSM_DL_DB_HANDLE	dlDbHand,
	const CSSM_DATA		*cert,
	CSSM_CERT_TYPE		certType,
	CSSM_CERT_ENCODING	certEncoding,
	const char			*printName,		// C string
	const CSSM_DATA		*publicKeyHash)		
{
	CSSM_DB_ATTRIBUTE_DATA			attrs[6];
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA_PTR		attr = &attrs[0];
	CSSM_DATA						certTypeData;
	CSSM_DATA						certEncData;
	CSSM_DATA						printNameData;
	CSSM_RETURN						crtn;
	CSSM_DB_UNIQUE_RECORD_PTR		recordPtr;
	
	/* issuer and serial number required, fake 'em */
	CSSM_DATA						issuer = {6, (uint8 *)"issuer"};
	CSSM_DATA						serial = {6, (uint8 *)"serial"};
	
	/* we spec six attributes, skipping alias */
	certTypeData.Data = (uint8 *)&certType;
	certTypeData.Length = sizeof(CSSM_CERT_TYPE);
	certEncData.Data = (uint8 *)&certEncoding;
	certEncData.Length = sizeof(CSSM_CERT_ENCODING);
	printNameData.Data = (uint8 *)printName;
	printNameData.Length = strlen(printName) + 1;
	
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "CertType";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &certTypeData;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "CertEncoding";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_UINT32;
	attr->NumberOfValues = 1;
	attr->Value = &certEncData;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "PrintName";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &printNameData;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "PublicKeyHash";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = (CSSM_DATA_PTR)publicKeyHash;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "Issuer";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &issuer;
	
	attr++;
	attr->Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr->Info.Label.AttributeName = "SerialNumber";
	attr->Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr->NumberOfValues = 1;
	attr->Value = &serial;
	
	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = 6;
	recordAttrs.AttributeData = attrs;
	
	crtn = CSSM_DL_DataInsert(dlDbHand,
		CSSM_DL_DB_RECORD_X509_CERTIFICATE,
		&recordAttrs,
		cert,
		&recordPtr);
	if(crtn) {
		cuPrintError("CSSM_DL_DataInsert", crtn);
	}
	else {
		CSSM_DL_FreeUniqueRecord(dlDbHand, recordPtr);
	}
	return crtn;
}

/*
 * Add a certificate to an open DLDB.
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
 * This prototype does not exist in public Security headers in 10.1, but the
 * function is in fact exported from the Security framework. A future release
 * will include a public prototype for this function.
 */
#if 1
extern void cssmPerror(const char *how, CSSM_RETURN error);
#else
#include <Security/cssmapple.h>
#endif
/*
 * Log CSSM error.
 */
void cuPrintError(char *op, CSSM_RETURN err)
{
	cssmPerror(op, err);
}
