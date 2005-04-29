/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
 * SecImportExportCrypto.cpp - low-level crypto routines for wrapping and unwrapping
 *							   keys.
 */

#include "SecImportExport.h"
#include "SecImportExportCrypto.h"
#include "SecImportExportUtils.h"
#include "Keychains.h"
#include "Access.h"
#include "Item.h"
#include "SecKeyPriv.h"
#include "KCEventNotifier.h"
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/KeySchema.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_utilities/devrandom.h>
#include <security_cdsa_client/securestorage.h>
#include <security_cdsa_client/dlclient.h>
#include <Security/cssmapi.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/*
 * Key attrribute names and values.
 *
 * This is where the public key hash goes.
 */
#define SEC_KEY_HASH_ATTR_NAME			"Label"

/*
 * This is where the publicly visible name goes.
 */
#define SEC_KEY_PRINT_NAME_ATTR_NAME	"PrintName"

/*
 * This the value we ultimately assign to the PrintName attr.
 */
#define SEC_KEY_PRINT_NAME_ATTR_VALUE   "Imported Private Key"

/*
 * Label and PrintName for imported keys other than private keys.
 */
#define SEC_PUBKEY_PRINT_NAME_ATTR_VALUE "Imported Key"

/*
 * Set private key's Label and PrintName attributes. On entry Label 
 * is typically a random string to faciliate finding the key in a DL; 
 * the PrintName is currently set to the same value by the DL. We
 * replace the Label attr with the public key hash and the PrintName
 * attr with a caller-supplied value.
 */
static CSSM_RETURN impExpSetPrivKeyLabel(
	CSSM_CSP_HANDLE 	cspHand,		// where the key lives
	CSSM_DL_DB_HANDLE 	dlDbHand,		// ditto
	CSSM_KEY_PTR		key,	
	const CSSM_DATA		*existKeyLabel,	// existing label, a random string
	const CSSM_DATA		*newPrintName,
	CssmOwnedData		&newLabel)		// RETURNED as what we set
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand = 0;
	CSSM_DATA						keyDigest = {0, NULL};
	
	crtn = impExpKeyDigest(cspHand, key, &keyDigest);
	if(crtn) {
		return crtn;
	}
	
	/* caller needs this for subsequent DL lookup */
	newLabel.copy(keyDigest);
	
	/*
	 * Look up the private key in the DL.
	 */
	query.RecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	predicate.DbOperator = CSSM_DB_EQUAL;
	
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = "Label";
	predicate.Attribute.Info.AttributeFormat = 
		CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	/* hope this cast is OK */
	predicate.Attribute.Value = (CSSM_DATA_PTR)existKeyLabel;
	query.SelectionPredicate = &predicate;
	
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0; // CSSM_QUERY_RETURN_DATA;	// FIXME - used?

	/* build Record attribute with two attrs */
	CSSM_DB_RECORD_ATTRIBUTE_DATA recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA attr[2];
	
	attr[0].Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr[0].Info.Label.AttributeName = SEC_KEY_HASH_ATTR_NAME;
	attr[0].Info.AttributeFormat     = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr[1].Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr[1].Info.Label.AttributeName = SEC_KEY_PRINT_NAME_ATTR_NAME;
	attr[1].Info.AttributeFormat     = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	
	recordAttrs.DataRecordType		 = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	recordAttrs.NumberOfAttributes   = 2;
	recordAttrs.AttributeData        = attr;
	
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		NULL,			// theData
		&record);
	/* abort only on success */
	if(crtn != CSSM_OK) {
		SecImpExpDbg("CSSM_DL_DataGetFirst error");
		record = NULL;
		resultHand = 0;
		goto errOut;
	}
	
	/* 
	 * Update existing attr data.
	 * NOTE: the module which allocated this attribute data - a DL -
	 * was loaded and attached by our client layer, not by us. Thus 
	 * we can't use the memory allocator functions *we* used when 
	 * attaching to the CSP - we have to use the ones
	 * which the client registered with the DL.
	 */
	impExpFreeCssmMemory(dlDbHand.DLHandle, attr[0].Value->Data);
	impExpFreeCssmMemory(dlDbHand.DLHandle, attr[0].Value);
	impExpFreeCssmMemory(dlDbHand.DLHandle, attr[1].Value->Data);
	impExpFreeCssmMemory(dlDbHand.DLHandle, attr[1].Value);
	attr[0].Value = &keyDigest;
	attr[1].Value = const_cast<CSSM_DATA *>(newPrintName);
	
	crtn = CSSM_DL_DataModify(dlDbHand,
			CSSM_DL_DB_RECORD_PRIVATE_KEY,
			record,
			&recordAttrs,
            NULL,				// DataToBeModified
			CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	if(crtn) {
		SecImpExpDbg("CSSM_DL_DataModify error");
		goto errOut;
	}
errOut:
	/* free resources */
	if(resultHand) {
		CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	}
	if(record) {
		CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	}
	if(keyDigest.Data)  {
		/* mallocd by CSP */
		impExpFreeCssmMemory(cspHand, keyDigest.Data);
	}
	return crtn;
}
	
/*
 * Import a raw key. This can be used as a lightweight "guess" evaluator 
 * if a handle to the raw CSP is passed in (with no keychain), or as 
 * the real thing which does full keychain import.
 */
OSStatus impExpImportRawKey(
	CFDataRef						inData,
	SecExternalFormat				externForm,
	SecExternalItemType				itemType,
	CSSM_ALGORITHMS					keyAlg,
	SecKeychainRef					importKeychain, // optional
	CSSM_CSP_HANDLE					cspHand,		// required
	SecItemImportExportFlags		flags,
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableArrayRef				outArray)		// optional, append here 
{
	CSSM_RETURN			crtn;
	CSSM_KEY			wrappedKey;
	CSSM_KEYHEADER		&hdr = wrappedKey.KeyHeader;
	CSSM_CSP_HANDLE		rawCspHand = 0;
	CSSM_KEY_SIZE		keySize;
	CSSM_KEYBLOB_FORMAT format;
	CSSM_KEYCLASS		keyClass;
	
	/* First convert external format and types to CSSM style. */
	crtn = impExpKeyForm(externForm, itemType, keyAlg, &format, &keyClass);
	
	/* cook up key to be null-unwrapped */
	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	wrappedKey.KeyData.Length = CFDataGetLength(inData);
	wrappedKey.KeyData.Data   = (uint8 *)CFDataGetBytePtr(inData);
	
	hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
	/* CspId don't care */
	hdr.BlobType = CSSM_KEYBLOB_RAW;
	hdr.Format = format;
	hdr.AlgorithmId = keyAlg;
	hdr.KeyClass = keyClass;
	/* LogicalKeySizeInBits calculated below */
	/* attr and usage are for the incoming unwrapped key... */
	hdr.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	hdr.KeyUsage = CSSM_KEYUSE_ANY;
	
	/* 
	 * Get key size in bits from raw CSP. Doing this right now is a good
	 * optimization for the "guessing" case; getting the key size from he 
	 * raw CSP involves a full DER decode on an alg- and format-specific manner.
	 * If we've been given the wrong params, we'll fail right here without 
	 * the complication of a full UnwrapKey op.
	 */
	rawCspHand = cuCspStartup(CSSM_TRUE);
	if(rawCspHand == 0) {
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	}
	crtn = CSSM_QueryKeySizeInBits(rawCspHand, NULL, &wrappedKey, &keySize);
	cuCspDetachUnload(rawCspHand, CSSM_TRUE);
	if(crtn) {
		SecImpExpDbg("CSSM_QueryKeySizeInBits error");
		return crtn;
	}
	hdr.LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;
	
	impExpKeyUnwrapParams unwrapParams;
	memset(&unwrapParams, 0, sizeof(unwrapParams));
	unwrapParams.encrAlg		= CSSM_ALGID_NONE;
	unwrapParams.encrMode		= CSSM_ALGMODE_NONE;
	unwrapParams.unwrappingKey  = NULL;
	unwrapParams.encrPad		= CSSM_PADDING_NONE;
	
	return impExpImportKeyCommon(
		&wrappedKey,
		importKeychain,
		cspHand,
		flags,
		keyParams,
		&unwrapParams,
		outArray);
}

using namespace KeychainCore;

/* 
 * Post notification of a "new key added" event.
 * If you know of another way to do this, other than a dlclient-based lookup of the
 * existing key in order to get a KeychainCore::Item, by all means have at it. 
 */
OSStatus impExpKeyNotify(
	SecKeychainRef	importKeychain,
	const CssmData	&keyLabel,		// stored with this, we use it to do a lookup
	const CSSM_KEY	&cssmKey)		// unwrapped key in CSSM format
{
	/* 
	 * Look up key in the DLDB by label, key class, algorithm, and key size.
	 */
	CSSM_DB_RECORDTYPE recordType;
	const CSSM_KEYHEADER &hdr = cssmKey.KeyHeader;
	
	switch(hdr.KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			recordType = CSSM_DL_DB_RECORD_PUBLIC_KEY;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			recordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
			break;
		case CSSM_KEYCLASS_SESSION_KEY:
			recordType = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;
			break;
		default:
			return paramErr;
	}
	assert(importKeychain != NULL);
	Keychain keychain = KeychainImpl::required(importKeychain);
	CssmClient::SSDb ssDb(safe_cast<CssmClient::SSDbImpl *>(&(*keychain->database())));
	
	CssmClient::DbAttributes dbAttributes;
	CssmClient::DbUniqueRecord uniqueId;
	CssmClient::SSDbCursor dbCursor(ssDb, 3);		// three attributes
	dbCursor->recordType(recordType);
	dbCursor->add(CSSM_DB_EQUAL, KeySchema::Label, keyLabel);
	dbCursor->add(CSSM_DB_EQUAL, KeySchema::KeyType, hdr.AlgorithmId);
	dbCursor->add(CSSM_DB_EQUAL, KeySchema::KeySizeInBits, hdr.LogicalKeySizeInBits);
	CssmClient::Key key;
	if (!dbCursor->nextKey(&dbAttributes, key, uniqueId)) {
		SecImpExpDbg("impExpKeyNotify: key not found");
		return errSecItemNotFound;
	}
	
	/* 
	 * Get a Keychain-style Item, post notification. 
	 */
	Item keyItem = keychain->item(recordType, uniqueId);
	KCEventNotifier::PostKeychainEvent(kSecAddEvent, keychain, keyItem);

	return noErr;
}

/*
 * Size of random label string in ASCII chars to facilitate DL lookup.
 */
#define SEC_RANDOM_LABEL_LEN		16

#define SEC_KEYATTR_RETURN_MASK		\
	(CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_RETURN_NONE)

/*
 * Common code to unwrap a key, used for raw keys (which do a NULL unwrap) and 
 * wrapped keys.
 */
OSStatus impExpImportKeyCommon(
	const CSSM_KEY					*wrappedKey,
	SecKeychainRef					importKeychain, // optional
	CSSM_CSP_HANDLE					cspHand,		// required, if importKeychain is 
													// present, must be from there
	SecItemImportExportFlags		flags,
	const SecKeyImportExportParameters	*keyParams, // optional 
	const impExpKeyUnwrapParams		*unwrapParams,
	CFMutableArrayRef				outArray)		// optional, append here 
{
	CSSM_CC_HANDLE		ccHand = 0;
	CSSM_RETURN			crtn;
	CSSM_DATA			labelData;
	CSSM_KEY_PTR		unwrappedKey = NULL;
	CSSM_DL_DB_HANDLE   dlDbHandle;
	CSSM_DL_DB_HANDLE   *dlDbPtr = NULL;
	OSStatus			ortn;
	CSSM_ACCESS_CREDENTIALS	nullCreds;
	uint8				randLabel[SEC_RANDOM_LABEL_LEN + 1];
	CSSM_KEYUSE			keyUsage = 0;			// default
	CSSM_KEYATTR_FLAGS  keyAttributes = 0;		// default
	const CSSM_KEYHEADER &hdr = wrappedKey->KeyHeader;
	CSSM_DATA			descrData = {0, NULL};
	ResourceControlContext rcc;
	Security::KeychainCore::Access::Maker maker;
	ResourceControlContext *rccPtr = NULL;
	SecAccessRef		accessRef = keyParams ? keyParams->accessRef : NULL;
	CssmAutoData		keyLabel(Allocator::standard());
	
	assert(unwrapParams != NULL);
	assert(cspHand != 0);
	
	if(importKeychain) {
		ortn = SecKeychainGetDLDBHandle(importKeychain, &dlDbHandle);
		if(ortn) {
			return ortn;
		}
		dlDbPtr = &dlDbHandle;
	}
	
	/* FIXME - who owns this key memory? */
	unwrappedKey = (CSSM_KEY_PTR)malloc(sizeof(CSSM_KEY));
	memset(unwrappedKey, 0, sizeof(CSSM_KEY));
	memset(&nullCreds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	
	/* context for unwrap */
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			unwrapParams->encrAlg,
			unwrapParams->encrMode,
			&nullCreds,
			unwrapParams->unwrappingKey,
			unwrapParams->iv.Data ? &unwrapParams->iv : NULL,
			unwrapParams->encrPad,
			0,				// Params
			&ccHand);
	if(crtn) {
		goto errOut;
	}
	if(dlDbPtr) {
		/* Importing to a keychain - add DLDB to context */
		crtn = impExpAddContextAttribute(ccHand, 
			CSSM_ATTRIBUTE_DL_DB_HANDLE,
			sizeof(CSSM_ATTRIBUTE_DL_DB_HANDLE),
			dlDbPtr);
		if(crtn) {
			SecImpExpDbg("impExpImportKeyCommon: CSSM_UpdateContextAttributes error");
			goto errOut;
		}
	}

	if((hdr.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY) && (dlDbPtr != NULL)) {
		/* Generate random 16-char label to facilitate DL lookup */
		char *randAscii = (char *)randLabel;		
		uint8 randBinary[SEC_RANDOM_LABEL_LEN / 2];
		unsigned randBinaryLen = SEC_RANDOM_LABEL_LEN / 2;
		DevRandomGenerator rng;

		rng.random(randBinary, randBinaryLen);
		for(unsigned i=0; i<randBinaryLen; i++) {
			sprintf(randAscii, "%02X", randBinary[i]);
			randAscii += 2;
		}
		labelData.Data = randLabel;
		labelData.Length = SEC_RANDOM_LABEL_LEN;
		/* actual keyLabel value set later */
	}
	else {
		labelData.Data = (uint8 *)SEC_PUBKEY_PRINT_NAME_ATTR_VALUE;
		labelData.Length = strlen(SEC_PUBKEY_PRINT_NAME_ATTR_VALUE);
		keyLabel.copy(labelData);
	}
	
	/*
	 * key attr flags and usage. First the defaults.
	 */
	if(keyParams) {
		keyUsage = keyParams->keyUsage;
		keyAttributes = keyParams->keyAttributes;
	}
	if(keyUsage == 0) {
		/* default */
		keyUsage = CSSM_KEYUSE_ANY;
	}
	if(keyAttributes == 0) {
		/* default */
		keyAttributes = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
		if(dlDbPtr) {
			keyAttributes |= CSSM_KEYATTR_PERMANENT;
		}
		if(hdr.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY) {
			keyAttributes |= (CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_EXTRACTABLE);
		}
	}
	else {
		/* caller-supplied; ensure we're generating a reference key */
		keyAttributes &= ~SEC_KEYATTR_RETURN_MASK;
		keyAttributes |= CSSM_KEYATTR_RETURN_REF;
	}
		
	if( (dlDbPtr != NULL) &&							// not permanent, no ACL
	    (hdr.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY) &&	// ACLs only for private key
		( (keyParams == NULL) ||						// NULL --> default ACL
		  !(keyParams->flags & kSecKeyNoAccessControl)  // explicity request no ACL
		)
	  ) {
		/* 
		 * Prepare to set up either a default ACL or one provided by caller via
		 * keyParams->accessRef.
		 */
		memset(&rcc, 0, sizeof(rcc));
		maker.initialOwner(rcc);
		rccPtr = &rcc;
	}

	/* 
	 * Additional optional parameters: block size, rounds, 
	 * effectiveKeySize.
	 * WARNING: block size and rounds, used for RC5, have not been tested.
	 * OpenSSL, as of Panther ship, did not support RC5 encryption.
	 */
	if(unwrapParams->effectiveKeySizeInBits != 0) {
		assert(unwrapParams->unwrappingKey->KeyHeader.AlgorithmId ==
			CSSM_ALGID_RC2);
		SecImpExpDbg("impExpImportKeyCommon: setting effectiveKeySizeInBits to %lu",
			unwrapParams->effectiveKeySizeInBits);
		crtn = impExpAddContextAttribute(ccHand, 
			CSSM_ATTRIBUTE_EFFECTIVE_BITS,
			sizeof(uint32),
			(void *)unwrapParams->effectiveKeySizeInBits);
		if(crtn) {
			SecImpExpDbg("impExpImportKeyCommon: CSSM_UpdateContextAttributes error");
			goto errOut;
		}
	}
	
	if(unwrapParams->rounds != 0) {
		assert(unwrapParams->unwrappingKey->KeyHeader.AlgorithmId ==
			CSSM_ALGID_RC5);
		SecImpExpDbg("impExpImportKeyCommon: setting rounds to %lu",
			unwrapParams->rounds);
		crtn = impExpAddContextAttribute(ccHand, 
			CSSM_ATTRIBUTE_ROUNDS,
			sizeof(uint32),
			(void *)unwrapParams->rounds);
		if(crtn) {
			SecImpExpDbg("impExpImportKeyCommon: CSSM_UpdateContextAttributes error");
			goto errOut;
		}
	}

	if(unwrapParams->blockSizeInBits != 0) {
		/* Our RC5 implementation has a fixed block size */
		if(unwrapParams->blockSizeInBits != 64) {
			SecImpExpDbg("WARNING impExpImportKeyCommon: setting block size to %lu",
				unwrapParams->blockSizeInBits);
			/* 
			 * WIth the current CSP this will actually be ignored 
			 */
			crtn = impExpAddContextAttribute(ccHand, 
				CSSM_ATTRIBUTE_BLOCK_SIZE,
				sizeof(uint32),
				(void *)unwrapParams->blockSizeInBits);
			if(crtn) {
				SecImpExpDbg("impExpImportKeyCommon: CSSM_UpdateContextAttributes error");
				goto errOut;
			}
		}
	}

	/* Here we go */
	crtn = CSSM_UnwrapKey(ccHand,
		NULL,				// public key
		(const CSSM_WRAP_KEY *)wrappedKey,
		keyUsage,
		keyAttributes,
		&labelData,
		rccPtr,				// CredAndAclEntry
		unwrappedKey,
		&descrData);		// required
	if(crtn != CSSM_OK) {
		SecImpExpDbg("CSSM_UnwrapKey failure");
		if(crtn == CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA) {
			/* report in a keychain-friendly way */
			crtn = errSecDuplicateItem;
		}
		goto errOut;
	}
	
	/* Private key: update Label as public key hash */
	if((hdr.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY) && (dlDbPtr != NULL)) {
		CSSM_DATA newPrintName;
		newPrintName.Data = (uint8 *)SEC_KEY_PRINT_NAME_ATTR_VALUE;
		newPrintName.Length = strlen((char *)newPrintName.Data);
		crtn = impExpSetPrivKeyLabel(cspHand, *dlDbPtr, unwrappedKey, 
			&labelData, &newPrintName, keyLabel);
		if(crtn) {
			goto errOut;
		}
	}
	
	/* Private key: adjust ACL as appropriate */
	if(rccPtr != NULL) {
		SecPointer<KeychainCore::Access> theAccess(accessRef ? 
			KeychainCore::Access::required(accessRef) : 
			new KeychainCore::Access("Imported Private Key"));
		try {
			CssmClient::KeyAclBearer bearer(cspHand, *unwrappedKey, Allocator::standard());
			theAccess->setAccess(bearer, maker);
		}
		catch (const CssmError &e) {
			/* not implemented means we're talking to the raw CSP which does
			 * not implement ACLs */
			if(e.error != CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED) {
				crtn = e.error;
			}
		}
		catch(...) {
			SecImpExpDbg("keyImport: exception on setAccess\n");
			crtn = errSecAuthFailed;	/* ??? */
		}
	}
	
	/*
	 * If importKeychain is non-NULL we've already added the key to the keychain.
	 * Convert it to a SecKeyRef if the caller wants that too.
	 */
	if(outArray) {
		SecKeyRef keyRef;
		OSStatus ortn;
		
		ortn = SecKeyCreate(unwrappedKey, &keyRef);
		if(ortn) {
			SecImpExpDbg("SecKeyCreate failure");
			goto errOut;
		}
		CFArrayAppendValue(outArray, keyRef);
	}
	
	if(importKeychain) {
		impExpKeyNotify(importKeychain, keyLabel.get(), *unwrappedKey);
	}
	
errOut:
	if(ccHand != 0) {
		CSSM_DeleteContext(ccHand);
	}
	return crtn;
}
	
/* 
 * Common code to wrap a key for export.
 */
CSSM_RETURN impExpExportKeyCommon(
	CSSM_CSP_HANDLE		cspHand,		// for all three keys
	SecKeyRef			secKey,
	CSSM_KEY_PTR		wrappingKey,
	CSSM_KEY_PTR		wrappedKey,		// RETURNED
	CSSM_ALGORITHMS		wrapAlg,
	CSSM_ENCRYPT_MODE   wrapMode,
	CSSM_PADDING		wrapPad,
	CSSM_KEYBLOB_FORMAT	wrapFormat,		// NONE, PKCS7, PKCS8
	CSSM_ATTRIBUTE_TYPE blobAttrType,	// optional raw key format attr
	CSSM_KEYBLOB_FORMAT blobForm,		// ditto
	const CSSM_DATA		*iv)
{
	OSStatus ortn;
	CSSM_RETURN crtn;
	
	const CSSM_KEY *unwrappedKey;
	ortn = SecKeyGetCSSMKey(secKey, &unwrappedKey);
	if(ortn) {
		SecImpExpDbg("impExpExportKeyCommon SecKeyGetCSSMKey error");
		return ortn;
	}

	/*
	 * Creds needed only for wrapping private keys.
	 * We bother checking in case we ever want to use this to wrap 
	 * session keys.
	 */
	CSSM_ACCESS_CREDENTIALS	nullCreds;
	memset(&nullCreds, 0, sizeof(nullCreds));
	const CSSM_ACCESS_CREDENTIALS *creds = &nullCreds;		// default
	
	CSSM_KEYCLASS keyClass = unwrappedKey->KeyHeader.KeyClass;
	if(keyClass == CSSM_KEYCLASS_PRIVATE_KEY) {
		ortn = SecKeyGetCredentials(secKey,
			CSSM_ACL_AUTHORIZATION_DECRYPT,			// HACK will change!
			kSecCredentialTypeDefault,
			&creds);
		if(ortn) {
			SecImpExpDbg("impExpExportKeyCommon SecKeyGetCredentials error");
			return ortn;
		}
	}
	
	CSSM_CC_HANDLE ccHand;
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
		wrapAlg,
		wrapMode,
		&nullCreds,			// creds for wrapping key, never a private key here
		wrappingKey,
		iv,
		wrapPad,
		0,					// Params
		&ccHand);
	if(ortn) {
		SecImpExpDbg("impExpExportKeyCommon CSSM_CSP_CreateSymmetricContext error");
		return crtn;
	}
	
	/* a couple of optional caller-specified attributes */
	if(wrapFormat != CSSM_KEYBLOB_WRAPPED_FORMAT_NONE) {
		crtn = impExpAddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT,
			sizeof(uint32),		
			(void *)wrapFormat);
		if(crtn) {
			SecImpExpDbg("impExpExportKeyCommon AddContextAttribute error (1)");
			CSSM_DeleteContext(ccHand);
			return crtn;
		}
	}
	
	if(blobAttrType != CSSM_ATTRIBUTE_NONE) {
		crtn = impExpAddContextAttribute(ccHand,
			blobAttrType,
			sizeof(uint32),		
			(void *)blobForm);
		if(crtn) {
			SecImpExpDbg("impExpExportKeyCommon AddContextAttribute error");
			return crtn;
		}
	}

	CSSM_DATA descData = {0, 0};
	memset(wrappedKey, 0, sizeof(wrappedKey));

	crtn = CSSM_WrapKey(ccHand,
		creds,
		unwrappedKey,
		&descData,	
		wrappedKey);
	CSSM_DeleteContext(ccHand);
	switch(crtn) {
		case CSSM_OK:
			break;
		case CSSMERR_CSP_INVALID_KEYATTR_MASK:
		{
			/*
			 * This is what comes back when we try to wrap an unextractable
			 * key, or when we null wrap a sensitive key. Give the caller 
			 * some useful info.
			 */
			CSSM_KEYATTR_FLAGS attr = unwrappedKey->KeyHeader.KeyAttr;
			if(!(attr & CSSM_KEYATTR_EXTRACTABLE)) {
				SecImpExpDbg("impExpExportKeyCommon !EXTRACTABLE");
				return errSecDataNotAvailable;
			}
			if((attr & CSSM_KEYATTR_SENSITIVE) && (wrappingKey == NULL)) {
				SecImpExpDbg("impExpExportKeyCommon !SENSITIVE, NULL wrap");
				return errSecPassphraseRequired;
			}

		}
		default:
			SecImpExpDbg("impExpExportKeyCommon CSSM_WrapKey error");
	}
	return crtn;
}
