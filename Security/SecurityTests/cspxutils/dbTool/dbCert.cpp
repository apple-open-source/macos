/*
 * Copyright (c) 2003-2005,2008 Apple Inc. All Rights Reserved.
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
 * dbCert.cpp - import a possibly bad cert along with its private key
 */

#include "dbCert.h"
#include <Security/Security.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuDbUtils.h> 
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuPem.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Copied from clxutils/clAppUtils/tpUtils */
/* defined in SecKeychainAPIPriv.h */
static const int kSecAliasItemAttr = 'alis';

/* Macro to declare a CSSM_DB_SCHEMA_ATTRIBUTE_INFO */
#define SCHEMA_ATTR_INFO(id, name, type)	\
	{ id, (char *)name, {0, NULL},  CSSM_DB_ATTRIBUTE_FORMAT_ ## type }
	
/* Too bad we can't get this from inside of the Security framework. */
static CSSM_DB_SCHEMA_ATTRIBUTE_INFO certSchemaAttrInfo[] = 
{
	SCHEMA_ATTR_INFO(kSecCertTypeItemAttr, "CertType", UINT32),
	SCHEMA_ATTR_INFO(kSecCertEncodingItemAttr, "CertEncoding", UINT32),
	SCHEMA_ATTR_INFO(kSecLabelItemAttr, "PrintName", BLOB),
	SCHEMA_ATTR_INFO(kSecAliasItemAttr, "Alias", BLOB),
	SCHEMA_ATTR_INFO(kSecSubjectItemAttr, "Subject", BLOB),
	SCHEMA_ATTR_INFO(kSecIssuerItemAttr, "Issuer", BLOB),
	SCHEMA_ATTR_INFO(kSecSerialNumberItemAttr, "SerialNumber", BLOB),
	SCHEMA_ATTR_INFO(kSecSubjectKeyIdentifierItemAttr, "SubjectKeyIdentifier", BLOB),
	SCHEMA_ATTR_INFO(kSecPublicKeyHashItemAttr, "PublicKeyHash", BLOB)
};
#define NUM_CERT_SCHEMA_ATTRS	\
	(sizeof(certSchemaAttrInfo) / sizeof(CSSM_DB_SCHEMA_ATTRIBUTE_INFO))

/* Macro to declare a CSSM_DB_SCHEMA_INDEX_INFO */
#define SCHEMA_INDEX_INFO(id, indexNum, indexType)	\
	{ id, CSSM_DB_INDEX_ ## indexType,  CSSM_DB_INDEX_ON_ATTRIBUTE }
	

static CSSM_DB_SCHEMA_INDEX_INFO certSchemaIndices[] = 
{
	SCHEMA_INDEX_INFO(kSecCertTypeItemAttr, 0, UNIQUE),
	SCHEMA_INDEX_INFO(kSecIssuerItemAttr, 0, UNIQUE),
	SCHEMA_INDEX_INFO(kSecSerialNumberItemAttr, 0, UNIQUE),
	SCHEMA_INDEX_INFO(kSecCertTypeItemAttr, 1, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecSubjectItemAttr, 2, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecIssuerItemAttr, 3, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecSerialNumberItemAttr, 4, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecSubjectKeyIdentifierItemAttr, 5, NONUNIQUE),
	SCHEMA_INDEX_INFO(kSecPublicKeyHashItemAttr, 6, NONUNIQUE)
};
#define NUM_CERT_INDICES	\
	(sizeof(certSchemaIndices) / sizeof(CSSM_DB_SCHEMA_INDEX_INFO))


CSSM_RETURN tpAddCertSchema(
	CSSM_DL_DB_HANDLE	dlDbHand)
{
	return CSSM_DL_CreateRelation(dlDbHand,
		CSSM_DL_DB_RECORD_X509_CERTIFICATE,
		"CSSM_DL_DB_RECORD_X509_CERTIFICATE",
		NUM_CERT_SCHEMA_ATTRS,
		certSchemaAttrInfo,
		NUM_CERT_INDICES,
		certSchemaIndices);		
}


/* copied verbatim from certTool */

/*
 * Find private key by label, modify its Label attr to be the
 * hash of the associated public key. 
 */
static CSSM_RETURN setPubKeyHash(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_DL_DB_HANDLE 	dlDbHand,
	const char			*keyLabel,		// look up by this
	CSSM_DATA			*rtnKeyDigest)	// optionally RETURNED, if so,
										// caller owns and must cuAppFree
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
	predicate.Attribute.Info.Label.AttributeName = (char *)"Label";
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
	attr.Info.Label.AttributeName = (char *)"Label";
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
		cuPrintError("CSSM_DL_DataGetFirst", crtn);
		return crtn;
	}
	
	CSSM_KEY_PTR keyToDigest = (CSSM_KEY_PTR)recordData.Data;
	CSSM_DATA_PTR keyDigest = NULL;
	CSSM_CC_HANDLE ccHand;
	crtn = CSSM_CSP_CreatePassThroughContext(cspHand,
	 	keyToDigest,
		&ccHand);
	if(crtn) {
		cuPrintError("CSSM_CSP_CreatePassThroughContext", crtn);
		return crtn;
	}
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&keyDigest);
	if(crtn) {
		cuPrintError("CSSM_CSP_PassThrough(PUBKEYHASH)", crtn);
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
		cuPrintError("CSSM_GetAPIMemoryFunctions(DLHandle)", crtn);
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
		cuPrintError("CSSM_DL_DataModify(PUBKEYHASH)", crtn);
		return crtn;
	}
	crtn = CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	if(crtn) {
		cuPrintError("CSSM_DL_DataAbortQuery", crtn);
		/* let's keep going in this case */
	}
	crtn = CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	if(crtn) {
		cuPrintError("CSSM_DL_FreeUniqueRecord", crtn);
		/* let's keep going in this case */
		crtn = CSSM_OK;
	}
	
	/* free resources */
	if(rtnKeyDigest) {
		*rtnKeyDigest = *keyDigest;
	}
	else {
		cuAppFree(keyDigest->Data, NULL);
		/* FIXME - don't we have to free keyDigest itself? */
	}
	return CSSM_OK;
}

static CSSM_RETURN importPrivateKey(
	CSSM_DL_DB_HANDLE	dlDbHand,		
	CSSM_CSP_HANDLE		cspHand,
	const char			*privKeyFileName,
	CSSM_ALGORITHMS		keyAlg,
	CSSM_BOOL			pemFormat,	// of the file
	CSSM_KEYBLOB_FORMAT	keyFormat,	// of the key blob itself, NONE means 
									//   use default
	CSSM_DATA			*keyHash)	// OPTIONALLY RETURNED - if so, caller
									//   owns and must cuAppFree()
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
					printf("***RSA Private key must be in PKCS1 or PKCS8 "
						"format\n");
					return CSSMERR_CSSM_INTERNAL_ERROR;
			}
			privKeyLabel = "Imported RSA key";
			break;
		case CSSM_ALGID_DSA:
			switch(keyFormat) {
				case CSSM_KEYBLOB_RAW_FORMAT_NONE:
					keyFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;	
								// default
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
					printf("***Diffie-Hellman Private key must be in"
						"PKCS8 format.\n");
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
			printf("***%s: Bad PEM formatting. Aborting.\n", 
				privKeyFileName);
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
		cuPrintError("CSSM_QueryKeySizeInBits",crtn);
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
		cuPrintError("CSSM_CSP_CreateSymmetricContext", crtn);
		goto done;
	}
	
	/* add DL/DB to context */
	CSSM_CONTEXT_ATTRIBUTE newAttr;
	newAttr.AttributeType     = CSSM_ATTRIBUTE_DL_DB_HANDLE;
	newAttr.AttributeLength   = sizeof(CSSM_DL_DB_HANDLE);
	newAttr.Attribute.Data    = (CSSM_DATA_PTR)&dlDbHand;
	crtn = CSSM_UpdateContextAttributes(ccHand, 1, &newAttr);
	if(crtn) {
		cuPrintError("CSSM_UpdateContextAttributes", crtn);
		goto done;
	}
	
	/* do the NULL unwrap */
	labelData.Data = (uint8 *)privKeyLabel;
	labelData.Length = strlen(privKeyLabel) + 1;
	crtn = CSSM_UnwrapKey(ccHand,
		NULL,				// PublicKey
		&wrappedKey,
		CSSM_KEYUSE_ANY,
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT | 
			CSSM_KEYATTR_SENSITIVE |CSSM_KEYATTR_EXTRACTABLE,
		&labelData,
		NULL,				// CredAndAclEntry
		&unwrappedKey,
		&descData);		// required
	if(crtn != CSSM_OK) {
		cuPrintError("CSSM_UnwrapKey", crtn);
		goto done;
	}
	
	/* one more thing: bind this private key to its public key */
	crtn = setPubKeyHash(cspHand, dlDbHand, privKeyLabel, keyHash);
	
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
		free(derKey);		// mallocd by readFile() */
	}
	if(pemFormat && pemKey) {
		free(pemKey);
	}
	if(rawCspHand) {
		CSSM_ModuleDetach(rawCspHand);
	}
	return crtn;
}


CSSM_RETURN importBadCert(
	CSSM_DL_HANDLE dlHand,
	const char *dbFileName, 
	const char *certFile, 
	const char *keyFile, 
	CSSM_ALGORITHMS	keyAlg,
	CSSM_BOOL pemFormat,			// of the file
	CSSM_KEYBLOB_FORMAT	keyFormat,	// of the key blob itself, NONE means 
									//   use default
	CSSM_BOOL verbose)
{
	CSSM_DL_DB_HANDLE dlDbHand = {dlHand, 0};
	CSSM_RETURN crtn;
	CSSM_DATA keyDigest = {0, NULL};
	CSSM_DATA certData = {0, NULL};
	unsigned len;
	
	CSSM_CSP_HANDLE cspHand = cuCspStartup(CSSM_FALSE);
	if(cspHand == 0) {
		printf("***Error attaching to CSPDL. Aborting.\n");
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	}
	
	/*
	 * 1. Open the (already existing) DB.
	 */
	dlDbHand.DBHandle = cuDbStartupByName(dlHand,
		(char *)dbFileName,		// bogus non-const prototype
		CSSM_FALSE,				// do NOT create it
		CSSM_FALSE);			// quiet
	if(dlDbHand.DBHandle == 0) {
		printf("Error opening %s. Aborting.\n", dbFileName);
		return CSSMERR_DL_DATASTORE_DOESNOT_EXIST;
	}
	
	/*
	 * Import key to DB, snagging its key digest along the way.
	 */
	crtn = importPrivateKey(dlDbHand, cspHand,
		keyFile, keyAlg, pemFormat, keyFormat,
		&keyDigest);
	if(crtn) {
		printf("***Error importing key %s. Aborting.\n", keyFile);
		goto errOut;
	}
	
	/* 
	 * Now the cert.
	 */
	if(readFile(certFile, &certData.Data, &len)) {
		printf("***Error reading cert from %s. Aborting.\n", certFile);
		goto errOut;
	}
	certData.Length = len;
	crtn = cuAddCertToDb(dlDbHand, &certData, 
		CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
		certFile,			// printName
		&keyDigest);
	if(crtn == CSSMERR_DL_INVALID_RECORDTYPE) {
		/* virgin DB, no cert schema: add schema and retry */
		crtn = tpAddCertSchema(dlDbHand);
		if(crtn == CSSM_OK) {
			crtn = cuAddCertToDb(dlDbHand, &certData, 
				CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
				certFile,			// printName
				&keyDigest);
		}
	}
	if(crtn) {
		printf("***Error importing cert %s. Aborting.\n", certFile);
	}
errOut:
	if(keyDigest.Data) {
		cuAppFree(keyDigest.Data, NULL);
	}
	if(dlDbHand.DBHandle) {
		CSSM_DL_DbClose(dlDbHand);
	}
	if(certData.Data) {
		free(certData.Data);
	}
	return crtn;
}
