/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All Rights Reserved.
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
 * keyPicker.cpp - select a key pair from a keychain
 */
 
#include "keyPicker.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/Security.h>
#include <stdexcept>
#include <ctype.h>
#include <clAppUtils/identPicker.h>		/* for kcFileName() */
#include <vector>

/*
 * Obtain either public key hash or PrintName for a given SecKeychainItem. Works on public keys,
 * private keys, identities, and certs. Caller must release the returned result. 
 */
OSStatus getKcItemAttr(
	SecKeychainItemRef kcItem,
	WhichAttr whichAttr,
	CFDataRef *rtnAttr)			/* RETURNED */
{
	/* main job is to figure out which attrType to ask for, and from what Sec item */
	SecKeychainItemRef attrFromThis;
	SecKeychainAttrType	attrType = 0;
	OSStatus ortn;
	bool releaseKcItem = false;
	
	CFTypeID cfId = CFGetTypeID(kcItem);
	if(cfId == SecIdentityGetTypeID()) {
		/* switch over to cert */
		ortn = SecIdentityCopyCertificate((SecIdentityRef)kcItem, 
			(SecCertificateRef *)&attrFromThis);
		if(ortn)
			cssmPerror("SecIdentityCopyCertificate", ortn);
			return ortn;
		kcItem = attrFromThis;
		releaseKcItem = true;
		cfId = SecCertificateGetTypeID();
	}
	
	if(cfId == SecCertificateGetTypeID()) {
		switch(whichAttr) {
			case WA_Hash:
				attrType = kSecPublicKeyHashItemAttr;
				break;
			case WA_PrintName:
				attrType = kSecLabelItemAttr;
				break;
			default:
				printf("getKcItemAttr: WhichAttr\n");
				return paramErr;
		}
	}
	else if(cfId == SecKeyGetTypeID()) {
		switch(whichAttr) {
			case WA_Hash:
				attrType = kSecKeyLabel;
				break;
			case WA_PrintName:
				attrType = kSecKeyPrintName;
				break;
			default:
				printf("getKcItemAttr: WhichAttr\n");
				return paramErr;
		}
	}
	
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 1;
	attrInfo.tag = &attrType;
	attrInfo.format = NULL;	// ???
	SecKeychainAttributeList *attrList = NULL;
	
	ortn = SecKeychainItemCopyAttributesAndData(
		kcItem, 
		&attrInfo,
		NULL,			// itemClass
		&attrList, 
		NULL,			// don't need the data
		NULL);
	if(releaseKcItem) {
		CFRelease(kcItem);
	}
	if(ortn) {
		cssmPerror("SecKeychainItemCopyAttributesAndData", ortn);
		return paramErr;
	}
	SecKeychainAttribute *attr = attrList->attr;
	*rtnAttr = CFDataCreate(NULL, (UInt8 *)attr->data, attr->length);
	SecKeychainItemFreeAttributesAndData(attrList, NULL);
	return noErr;
}

/*
 * Class representing one key in the keychain.
 */
class PickerKey
{
public:
	PickerKey(SecKeyRef keyRef);
	~PickerKey();
	
	bool		isUsed()					{ return mIsUsed;}
	void		isUsed(bool u)				{ mIsUsed = u; }
	bool		isPrivate()					{ return mIsPrivate; }
	CFDataRef	getPrintName()				{ return mPrintName; }
	CFDataRef	getPubKeyHash()				{ return mPubKeyHash; }
	SecKeyRef	keyRef()					{ return mKeyRef; }
	
	PickerKey	*partnerKey()				{ return mPartner; }
	void		partnerKey(PickerKey *pk)	{ mPartner = pk; }
	char		*kcFile()					{ return mKcFile; }
	
private:
	SecKeyRef	mKeyRef;
	CFDataRef	mPrintName;
	CFDataRef	mPubKeyHash;
	bool		mIsPrivate;		// private/public key
	bool		mIsUsed;		// has been spoken for 
	PickerKey	*mPartner;		// other member of public/private pair
	char		*mKcFile;		// file name of keychain this lives on 
};

PickerKey::PickerKey(SecKeyRef keyRef)
	:	mKeyRef(NULL), 
		mPrintName(NULL), 
		mPubKeyHash(NULL), 
		mIsPrivate(false), 
		mIsUsed(false),
		mPartner(NULL),
		mKcFile(NULL)
{
	if(CFGetTypeID(keyRef) != SecKeyGetTypeID()) {
		throw std::invalid_argument("not a key");
	}
	
	OSStatus ortn = getKcItemAttr((SecKeychainItemRef)keyRef, WA_Hash, &mPubKeyHash);
	if(ortn) {
		throw std::invalid_argument("pub key hash not available");
	}
	ortn = getKcItemAttr((SecKeychainItemRef)keyRef, WA_PrintName, &mPrintName);
	if(ortn) {
		throw std::invalid_argument("pub key hash not available");
	}
	
	const CSSM_KEY *cssmKey;
	ortn = SecKeyGetCSSMKey(keyRef, &cssmKey);
	if(ortn) {
		/* should never happen */
		cssmPerror("SecKeyGetCSSMKey", ortn);
		throw std::invalid_argument("SecKeyGetCSSMKey error");
	}
	if(cssmKey->KeyHeader.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY) {
		mIsPrivate = true;
	}
	
	/* stash name of the keychain this lives on */
	SecKeychainRef kcRef;
	ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)keyRef, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainItemCopyKeychain", ortn);
		mKcFile = strdup("Unnamed keychain");
	}
	else {
		mKcFile = kcFileName(kcRef);
	}

	mKeyRef = keyRef;
	CFRetain(mKeyRef);
}

PickerKey::~PickerKey()
{
	if(mKeyRef) {
		CFRelease(mKeyRef);
	}
	if(mPubKeyHash) {
		CFRelease(mPubKeyHash);
	}
	if(mPrintName) {
		CFRelease(mPrintName);
	}
	if(mKcFile) {
		free(mKcFile);
	}
}

typedef std::vector<PickerKey *> KeyVector;

/* 
 * add PickerKey objects of specified type to a KeyVector. 
 */
static void getPickerKeys(
	SecKeychainRef	kcRef,
	SecItemClass	itemClass,	// actually CSSM_DL_DB_RECORD_{PRIVATE,PRIVATE}_KEY for now
	KeyVector		&keyVector)
{
	SecKeychainSearchRef 	srchRef = NULL;
	SecKeychainItemRef		kcItem;
	
	OSStatus ortn = SecKeychainSearchCreateFromAttributes(kcRef,
		itemClass,
		NULL,			// any attrs
		&srchRef);
	if(ortn) {
		cssmPerror("SecKeychainSearchCreateFromAttributes", ortn);
		return;
	}
	do {
		ortn = SecKeychainSearchCopyNext(srchRef, &kcItem);
		if(ortn) {
			break;
		}
		try {
			PickerKey *pickerKey = new PickerKey((SecKeyRef)kcItem);
			keyVector.push_back(pickerKey);
		}
		catch(...) {
			printf("**** key item that failed PickerKey construct ***\n");
			/* but keep going */
		}
	} while(ortn == noErr);
	CFRelease(srchRef);
}

/*
 * Print contents of a CFData assuming it's printable 
 */
static void printCfData(CFDataRef cfd)
{
	CFIndex len = CFDataGetLength(cfd);
	const UInt8 *cp = CFDataGetBytePtr(cfd);
	for(CFIndex dex=0; dex<len; dex++) {
		char c = cp[dex];
		if(isprint(c)) {
			putchar(c);
		}
		else {
			printf(".%02X.", c);
		}
	}
}

OSStatus keyPicker(
	SecKeychainRef  kcRef,		// NULL means the default list
	SecKeyRef		*pubKey,	// RETURNED
	SecKeyRef		*privKey)   // RETURNED
{

	/* First create a arrays of all of the keys, parsed and ready for use */
	
	std::vector<PickerKey *> privKeys;
	std::vector<PickerKey *> pubKeys;
	getPickerKeys(kcRef, CSSM_DL_DB_RECORD_PRIVATE_KEY, privKeys);
	getPickerKeys(kcRef, CSSM_DL_DB_RECORD_PUBLIC_KEY,  pubKeys);
	
	/* now interate thru private keys, looking for a partner for each one */
	int numPairs = 0;
	unsigned numPrivKeys = privKeys.size();
	unsigned numPubKeys  = pubKeys.size();
	
	for(unsigned privDex=0; privDex<numPrivKeys; privDex++) {
		PickerKey *privPk = privKeys[privDex];
		CFDataRef privHash = privPk->getPubKeyHash();
		for(unsigned pubDex=0; pubDex<numPubKeys; pubDex++) {
			PickerKey *pubPk = pubKeys[pubDex];
			if(pubPk->isUsed()) {
				/* already spoken for */
				continue;
			}
			if(!CFEqual(privHash, pubPk->getPubKeyHash())) {  
				/* public key hashes don't match */
				continue;
			}
			
			/* got a match */
			pubPk->partnerKey(privPk);
			privPk->partnerKey(pubPk);
			pubPk->isUsed(true);
			privPk->isUsed(true);
			
			/* display */
			printf("[%d] privKey  : ", numPairs); printCfData(privPk->getPrintName()); printf("\n");
			printf("    pubKey   : ");  printCfData(pubPk->getPrintName());printf("\n");
			printf("    keychain : %s\n", privPk->kcFile());
			
			numPairs++;
		}
	}
	
	if(numPairs == 0) {
		printf("*** keyPicker: no key pairs found.\n");
		return paramErr;
	}
	
	OSStatus ortn = noErr;
	int ires;
	while(1) {
		fpurge(stdin);
		printf("\nEnter key pair number or CR to quit : ");
		fflush(stdout);
		char resp[64];
		getString(resp, sizeof(resp));
		if(resp[0] == '\0') {
			ortn = CSSMERR_CSSM_USER_CANCELED;
			break;
		}
		ires = atoi(resp);
		if((ires < 0) || (ires >= numPairs)) {
			printf("***Invalid entry. Type a number between 0 and %d\n", numPairs-1);
			continue;
		}
		break;
	}
	
	if(ortn == noErr) {
		/* find the ires'th partnered private key */
		int goodOnes = 0;
		for(unsigned privDex=0; privDex<numPrivKeys; privDex++) {
			PickerKey *privPk = privKeys[privDex];
			if(!privPk->isUsed()) {
				continue;
			}
			if(goodOnes == ires) {
				/* this is it */
				*privKey = privPk->keyRef();
				*pubKey = privPk->partnerKey()->keyRef();
			}
			goodOnes++;
		}
	}
	
	/* clean out PickerKey arrays */
	for(unsigned privDex=0; privDex<numPrivKeys; privDex++) {
		delete privKeys[privDex];
	}
	for(unsigned pubDex=0; pubDex<numPubKeys; pubDex++) {
		delete pubKeys[pubDex];
	}
	return ortn;
}

