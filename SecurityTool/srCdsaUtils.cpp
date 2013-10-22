/*
 * Copyright (c) 2001,2003-2011 Apple, Inc. All Rights Reserved.
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
 * srCdsaUtils.cpp -- common CDSA access utilities
 */

#include "srCdsaUtils.h"
#include <stdlib.h>
#include <stdio.h>
#include <Security/SecCertificate.h>
#include <Security/cssmapple.h>				/* for cssmPerror() */
#include <Security/oidsalg.h>				/* for cssmPerror() */
#include <strings.h>

static CSSM_VERSION vers = {2, 0};
static const CSSM_GUID testGuid = { 0xFADE, 0, 0, { 1,2,3,4,5,6,7,0 }};

/*
 * Standard app-level memory functions required by CDSA.
 */
void * srAppMalloc (CSSM_SIZE size, void *allocRef) {
	return( malloc(size) );
}

void srAppFree (void *mem_ptr, void *allocRef) {
	free(mem_ptr);
 	return;
}

void * srAppRealloc (void *ptr, CSSM_SIZE size, void *allocRef) {
	return( realloc( ptr, size ) );
}

void * srAppCalloc (uint32 num, CSSM_SIZE size, void *allocRef) {
	return( calloc( num, size ) );
}

static CSSM_API_MEMORY_FUNCS memFuncs = {
	srAppMalloc,
	srAppFree,
	srAppRealloc,
 	srAppCalloc,
 	NULL
 };

CSSM_BOOL srCompareCssmData(const CSSM_DATA *d1,
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

CSSM_BOOL srCssmStartup()
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
		srPrintError("CSSM_Init", crtn);
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
CSSM_CSP_HANDLE srCspStartup(
	CSSM_BOOL bareCsp)		// true ==> CSP, false ==> CSP/DL
{
	CSSM_CSP_HANDLE cspHand;
	CSSM_RETURN		crtn;
	const CSSM_GUID *guid;

	/* common CSSM init */
	if(srCssmStartup() == CSSM_FALSE) {
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
		srPrintError("CSSM_ModuleLoad()", crtn);
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
		srPrintError("CSSM_ModuleAttach()", crtn);
		return 0;
	}
	return cspHand;
}

/* Attach to DL side of CSPDL */
CSSM_DL_HANDLE srDlStartup()
{
	CSSM_DL_HANDLE 	dlHand = 0;
	CSSM_RETURN		crtn;

	if(srCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleCSPDL,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		srPrintError("CSSM_ModuleLoad(Apple CSPDL)", crtn);
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
		srPrintError("CSSM_ModuleAttach(Apple CSPDL)", crtn);
		return 0;
	}
	return dlHand;
}

CSSM_CL_HANDLE srClStartup()
{
	CSSM_CL_HANDLE clHand;
	CSSM_RETURN crtn;

	if(srCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleX509CL,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		srPrintError("CSSM_ModuleLoad(AppleCL)", crtn);
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
		srPrintError("CSSM_ModuleAttach(AppleCL)", crtn);
		return 0;
	}
	else {
		return clHand;
	}
}

CSSM_TP_HANDLE srTpStartup()
{
	CSSM_TP_HANDLE tpHand;
	CSSM_RETURN crtn;

	if(srCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleX509TP,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		srPrintError("CSSM_ModuleLoad(AppleTP)", crtn);
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
		srPrintError("CSSM_ModuleAttach(AppleTP)", crtn);
		return 0;
	}
	else {
		return tpHand;
	}
}

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 */
CSSM_RETURN srAddContextAttribute(CSSM_CC_HANDLE CCHandle,
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
		srPrintError("CSSM_UpdateContextAttributes", crtn);
	}
	return crtn;
}


/*
 * Derive symmetric key.
 * Note in the X CSP, we never return an IV.
 */
CSSM_RETURN srCspDeriveKey(CSSM_CSP_HANDLE cspHand,
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
		srPrintError("CSSM_CSP_CreateDeriveKeyContext", crtn);
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
		srPrintError("CSSM_DeriveKey", crtn);
		return crtn;
	}
	crtn = CSSM_DeleteContext(ccHand);
	if(crtn) {
		srPrintError("CSSM_DeleteContext", crtn);
	}
	return crtn;
}

/*
 * Generate key pair of arbitrary algorithm.
 */

/* CSP DL currently does not perform DSA generate params; let CSP do it implicitly */
#define DO_DSA_GEN_PARAMS		0

CSSM_RETURN srCspGenKeyPair(CSSM_CSP_HANDLE cspHand,
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
		srPrintError("CSSM_CSP_CreateKeyGenContext", crtn);
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
					srPrintError("CSSM_GenerateAlgorithmParams", crtn);
					CSSM_DeleteContext(ccHand);
					return crtn;
				}
				srAppFree(dummy.Data, NULL);
			}
			break;
		#endif	/* DO_DSA_GEN_PARAMS */
		default:
			break;
	}

	/* optionally specify DL/DB storage location */
	if(dlDbHand) {
		crtn = srAddContextAttribute(ccHand,
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
		srPrintError("CSSM_GenerateKeyPair", ocrtn);
	}
	crtn = CSSM_DeleteContext(ccHand);
	if(crtn) {
		srPrintError("CSSM_DeleteContext", crtn);
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
CSSM_RETURN srAddCertToKC(
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
unsigned srDER_ToInt(const CSSM_DATA *DER_Data)
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
void srPrintError(const char *op, CSSM_RETURN err)
{
	cssmPerror(op, err);
}

/*
 * Convert a CFString into a C string as safely as we can. Caller must
 * free the result.
 */
char *srCfStrToCString(
	CFStringRef cfStr)
{
	CFIndex len = CFStringGetLength(cfStr) + 1;
	char *cstr = (char *)malloc(len);
	if(cstr == NULL) {
		return NULL;
	}
	if(!CFStringGetCString(cfStr, cstr, len, kCFStringEncodingASCII)) {
		printf("***CFStringGetCString error\n");
		free(cstr);
		return NULL;
	}
	return cstr;
}

