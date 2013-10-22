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
 */
 
/*
 * pkcs12Keychain.h - P12Coder keychain-related functions.
 */
 
#include "pkcs12Coder.h"
#include "pkcs12Templates.h"
#include "pkcs12Utils.h"
#include "pkcs12Debug.h"
#include "pkcs12Crypto.h"
#include <Security/cssmerr.h>
#include <security_cdsa_utils/cuDbUtils.h>			// cuAddCrlToDb()
#include <security_asn1/nssUtils.h>
#include <security_cdsa_utilities/KeySchema.h>			/* private API */
#include <security_keychain/SecImportExportCrypto.h>	/* private API */

/*
 * Store the results of a successful decode in app-specified 
 * keychain per mImportFlags. Also assign public key hash attributes to any 
 * private keys found.
 */
void P12Coder::storeDecodeResults()
{
	assert(mKeychain != NULL);
	assert(mDlDbHand.DLHandle != 0);
	if(mImportFlags & kSecImportKeys) {
		setPrivateKeyHashes();
	}
	if(mImportFlags & kSecImportCertificates) {
		for(unsigned dex=0; dex<numCerts(); dex++) {
			P12CertBag *certBag = mCerts[dex];
			SecCertificateRef secCert = certBag->getSecCert();
			OSStatus ortn = SecCertificateAddToKeychain(secCert, mKeychain);
			CFRelease(secCert);
			switch(ortn) {
				case errSecSuccess:					// normal
					p12DecodeLog("cert added to keychain");
					break;
				case errSecDuplicateItem:	// dup cert, OK< skip
					p12DecodeLog("skipping dup cert");
					break;
				default:
					p12ErrorLog("SecCertificateAddToKeychain failure\n");
					MacOSError::throwMe(ortn);
			}
		}
	}
	
	if(mImportFlags & kSecImportCRLs) {
		for(unsigned dex=0; dex<numCrls(); dex++) {
			P12CrlBag *crlBag = mCrls[dex];
			CSSM_RETURN crtn = cuAddCrlToDb(mDlDbHand,
				clHand(),
				&crlBag->crlData(),
				NULL);			// no URI known
			switch(crtn) {
				case CSSM_OK:								// normal
					p12DecodeLog("CRL added to keychain");
					break;
				case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:	// dup, ignore
					p12DecodeLog("skipping dup CRL");
					break;
				default:
					p12LogCssmError("Error adding CRL to keychain", crtn);
					CssmError::throwMe(crtn);
			}
		}
	}
	
	/* If all of that succeeded, post notification for imported keys */
	if(mImportFlags & kSecImportKeys) {
		notifyKeyImport();
	}
}

/*
 * Assign appropriate public key hash attribute to each 
 * private key. 
 */
void P12Coder::setPrivateKeyHashes()
{
	CSSM_KEY_PTR newKey;
	
	for(unsigned dex=0; dex<numKeys(); dex++) {
		P12KeyBag *keyBag = mKeys[dex];
		
		CSSM_DATA newLabel = {0, NULL};
		CFStringRef friendlyName = keyBag->friendlyName();
		newKey = NULL;
		CSSM_RETURN crtn = p12SetPubKeyHash(mCspHand,
			mDlDbHand,
			keyBag->label(),
			p12StringToUtf8(friendlyName, mCoder),
			mCoder,
			newLabel,
			newKey);
		if(friendlyName) {
			CFRelease(friendlyName);
		}
		switch(crtn) {
			case CSSM_OK:
				/* update key's label in case we have to delete on error */
				keyBag->setLabel(newLabel);
				p12DecodeLog("set pub key hash for private key");
				break;
			case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:
				/*
				 * Special case: update keyBag's CSSM_KEY and proceed without error
				 */
				p12DecodeLog("ignoring dup private key");
				assert(newKey != NULL);
				keyBag->setKey(newKey);
				keyBag->dupKey(true);
				/* update key's label in case we have to delete on error */
				keyBag->setLabel(newLabel);
				break;
			default:
				p12ErrorLog("p12SetPubKeyHash failure\n");
				CssmError::throwMe(crtn);
		}
	}
}

/*
 * Post keychain notification for imported keys. 
 */
void P12Coder::notifyKeyImport()
{
	if(mKeychain == NULL) {
		/* Can't notify if user only gave us DLDB */
		return;
	}
	for(unsigned dex=0; dex<numKeys(); dex++) {
		P12KeyBag *keyBag = mKeys[dex];
		if(keyBag->dupKey()) {
			/* no notification for keys we merely looked up */
			continue;
		}
		CssmData &labelData = CssmData::overlay(keyBag->label());
		OSStatus ortn = impExpKeyNotify(mKeychain, labelData, *keyBag->key());
		if(ortn) {
			p12ErrorLog("notifyKeyImport: impExpKeyNotify returned %ld\n", (unsigned long)ortn);
			MacOSError::throwMe(ortn);
		}
	}
}

/*
 * Given a P12KeyBag, find a matching P12CertBag. Keys and certs
 * "match" if their localKeyIds match. Returns NULL if not found.
 */
P12CertBag *P12Coder::findCertForKey(
	P12KeyBag *keyBag)
{
	assert(keyBag != NULL);
	CSSM_DATA &keyKeyId = keyBag->localKeyIdCssm();
	
	for(unsigned dex=0; dex<numCerts(); dex++) {
		P12CertBag *certBag = mCerts[dex];
		CSSM_DATA &certKeyId = certBag->localKeyIdCssm();
		if(nssCompareCssmData(&keyKeyId, &certKeyId)) {
			p12DecodeLog("findCertForKey SUCCESS");
			return certBag;
		}
	}
	p12DecodeLog("findCertForKey FAILURE");
	return NULL;
}

/*
 * Export items specified as SecKeychainItemRefs.
 */
void P12Coder::exportKeychainItems(
	CFArrayRef				items)
{
	assert(items != NULL);
	CFIndex numItems = CFArrayGetCount(items);
	for(CFIndex dex=0; dex<numItems; dex++) {
		const void *item = CFArrayGetValueAtIndex(items, dex);
		if(item == NULL) {
			p12ErrorLog("exportKeychainItems: NULL item\n");
			MacOSError::throwMe(errSecParam);
		}
		CFTypeID itemType = CFGetTypeID(item);
		if(itemType == SecCertificateGetTypeID()) {
			addSecCert((SecCertificateRef)item);
		}
		else if(itemType == SecKeyGetTypeID()) {
			addSecKey((SecKeyRef)item);
		}
		else {
			p12ErrorLog("exportKeychainItems: unknown item\n");
			MacOSError::throwMe(errSecParam);		
		}
	}
}

/*
 * Gross kludge to work around the fact that SecKeyRefs have no attributes which 
 * are visible at the Sec layer. Not only are the attribute names we happen 
 * to know about (Label, PrintName) not publically visible anywhere in the 
 * system, but the *format* of the attr names for SecKeyRefs differs from
 * the format of all other SecKeychainItems (NAME_AS_STRING for SecKeys, 
 * NAME_AS_INTEGER for everything else).
 *
 * So. We use the privately accessible schema definition table for
 * keys to map from the attr name strings we happen to know about to a 
 * totally private name-as-int index which we can then use in the 
 * SecKeychainItemCopyAttributesAndData mechanism. 
 *
 * This will go away if SecKeyRef defines its actual attrs as strings, AND
 * the SecKeychainSearch mechanism knows to specify attr names for SecKeyRefs
 * as strings rather than integers. 
 */
static OSStatus attrNameToInt(
	const char *name, 
	UInt32 *attrInt)
{
	const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *attrList = 
		KeySchema::KeySchemaAttributeList;
	unsigned numAttrs = KeySchema::KeySchemaAttributeCount;
	for(unsigned dex=0; dex<numAttrs; dex++) {
		const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *info = &attrList[dex];
		if(!strcmp(name, info->AttributeName)) {
			*attrInt = info->AttributeId;
			return errSecSuccess;
		}
	}
	return errSecParam;
}

void P12Coder::addSecKey(
	SecKeyRef	keyRef)
{
	/* get the cert's attrs (not data) */
	
	/* 
	 * Convert the attr name strings we happen to know about to 
	 * unknowable name-as-int values.
	 */
	UInt32 printNameTag;
	OSStatus ortn = attrNameToInt(P12_KEY_ATTR_PRINT_NAME, &printNameTag);
	if(ortn) {
		p12ErrorLog("addSecKey: problem looking up key attr name\n");
		MacOSError::throwMe(ortn);
	}
	UInt32 labelHashTag;
	ortn = attrNameToInt(P12_KEY_ATTR_LABEL_AND_HASH, &labelHashTag);
	if(ortn) {
		p12ErrorLog("addSecKey: problem looking up key attr name\n");
		MacOSError::throwMe(ortn);
	}

	UInt32 tags[2];
	tags[0] = printNameTag;
	tags[1] = labelHashTag;
	
	/* I don't know what the format field is for */
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 2;
	attrInfo.tag = tags;
	attrInfo.format = NULL;	// ???
	
	/* FIXME header says this is an IN/OUT param, but it's not */
	SecKeychainAttributeList *attrList = NULL;
	
	ortn = SecKeychainItemCopyAttributesAndData(
		(SecKeychainItemRef)keyRef, 
		&attrInfo,
		NULL,			// itemClass
		&attrList, 
		NULL,			// don't need the data
		NULL);
	if(ortn) {
		p12ErrorLog("addSecKey: SecKeychainItemCopyAttributesAndData "
			"error\n");
		MacOSError::throwMe(ortn);		
	}
	
	/* Snag the attrs, convert to something useful */
	CFStringRef friendName = NULL;
	CFDataRef localKeyId = NULL;
	for(unsigned i=0; i<attrList->count; i++) {
		SecKeychainAttribute *attr = &attrList->attr[i];
		if(attr->tag == printNameTag) {
			friendName = CFStringCreateWithBytes(NULL, 
				(UInt8 *)attr->data, attr->length, 
				kCFStringEncodingUTF8,	false);
		}
		else if(attr->tag == labelHashTag) {
			localKeyId = CFDataCreate(NULL, (UInt8 *)attr->data, attr->length);
		}
		else {
			p12ErrorLog("addSecKey: unexpected attr tag\n");
			MacOSError::throwMe(errSecParam);		
			
		}
	}
	
	/*
	 * Infer the CSP associated with this key.
	 * FIXME: this should be an attribute of the SecKeyRef itself,
	 * not inferred from the keychain it happens to be living on
	 * (SecKeyRefs should not have to be attached to Keychains at
	 * this point).
	 */
	SecKeychainRef kcRef;
	ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)keyRef, &kcRef);
	if(ortn) {
		p12ErrorLog("addSecKey: SecKeychainItemCopyKeychain returned %d\n", (int)ortn);
		MacOSError::throwMe(ortn);		
	}
	CSSM_CSP_HANDLE cspHand;
	ortn = SecKeychainGetCSPHandle(kcRef, &cspHand);
	if(ortn) {
		p12ErrorLog("addSecKey: SecKeychainGetCSPHandle returned %d\n", (int)ortn);
		MacOSError::throwMe(ortn);
	}
	CFRelease(kcRef);
	
	/* and the CSSM_KEY itself */
	const CSSM_KEY *cssmKey;
	ortn = SecKeyGetCSSMKey(keyRef, &cssmKey);
	if(ortn) {
		p12ErrorLog("addSecKey: SecKeyGetCSSMKey returned %d\n", (int)ortn);
		MacOSError::throwMe(ortn);		
	}
	
	/* Cook up a key bag and save it */
	P12KeyBag *keyBag = new P12KeyBag(cssmKey,
		cspHand, 
		friendName,	localKeyId, 
		NULL, 			// other attrs
		mCoder,
		keyRef);
	addKey(keyBag);
	SecKeychainItemFreeAttributesAndData(attrList, NULL);
	if(friendName) {
		CFRelease(friendName);
	}
	if(localKeyId) {
		CFRelease(localKeyId);
	}
}

void P12Coder::addSecCert(
	SecCertificateRef	certRef)
{
	/* get the cert's attrs and data */
	/* I don't know what the format field is for */
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 2;
	UInt32 tags[2] = {kSecLabelItemAttr, kSecPublicKeyHashItemAttr};
	attrInfo.tag = tags;
	attrInfo.format = NULL;	// ???
	
	/* FIXME header says this is an IN/OUT param, but it's not */
	SecKeychainAttributeList *attrList = NULL;
	UInt32 certLen;
	void *certData;
	
	OSStatus ortn = SecKeychainItemCopyAttributesAndData(
		(SecKeychainItemRef)certRef, 
		&attrInfo,
		NULL,			// itemClass
		&attrList, 
		&certLen,	
		&certData);
	if(ortn) {
		p12ErrorLog("addSecCert: SecKeychainItemCopyAttributesAndData "
			"error\n");
		MacOSError::throwMe(ortn);		
	}
	
	/* Snag the attrs, convert to something useful */
	CFStringRef friendName = NULL;
	CFDataRef localKeyId = NULL;
	for(unsigned i=0; i<attrList->count; i++) {
		SecKeychainAttribute *attr = &attrList->attr[i];
		switch(attr->tag) {
			case kSecPublicKeyHashItemAttr:
				localKeyId = CFDataCreate(NULL, (UInt8 *)attr->data, attr->length);
				break;
			case kSecLabelItemAttr:
				/* FIXME: always in UTF8? */
				friendName = CFStringCreateWithBytes(NULL, 
					(UInt8 *)attr->data, attr->length, kCFStringEncodingUTF8,
					false);
				break;
			default:
				p12ErrorLog("addSecCert: unexpected attr tag\n");
				MacOSError::throwMe(errSecParam);		
			
		}
	}
	
	/* Cook up a cert bag and save it */
	CSSM_DATA cData = {certLen, (uint8 *)certData};
	P12CertBag *certBag = new P12CertBag(CT_X509, cData, friendName,
		localKeyId, NULL, mCoder);
	addCert(certBag);
	SecKeychainItemFreeAttributesAndData(attrList, certData);
	if(friendName) {
		CFRelease(friendName);
	}
	if(localKeyId) {
		CFRelease(localKeyId);
	}
}

/*
 * Delete anything stored in a keychain during decode, called on 
 * decode error.
 * Currently the only thing we have to deal with is private keys,
 * since certs and CRLs don't get stored until the end of a successful
 * decode. 
 */
void P12Coder::deleteDecodedItems()
{
	if(!(mImportFlags & kSecImportKeys)) {
		/* no keys stored, done */
		return;
	}
	if(mDlDbHand.DLHandle == 0) {
		/* no keychain, done */
		return;
	}
	
	unsigned nKeys = numKeys();
	for(unsigned dex=0; dex<nKeys; dex++) {
		P12KeyBag *keyBag = mKeys[dex];
		p12DeleteKey(mDlDbHand, keyBag->label());
	}

}

