/*
 * Copyright (c) 2004,2006 Apple Computer, Inc. All Rights Reserved.
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
 * singleItemPicker.cpp - select a key or cert from a keychain
 */
 
#include <clAppUtils/keyPicker.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/Security.h>
#include <stdexcept>
#include <ctype.h>
#include <clAppUtils/identPicker.h>		/* for kcFileName() */
#include <clAppUtils/keyPicker.h>		/* for getKcItemAttr() */
#include <vector>
#include "singleItemPicker.h"

/*
 * Class representing one item in the keychain.
 */
class SimplePickerItem
{
public:
	SimplePickerItem(SecKeychainItemRef itemRef);
	~SimplePickerItem();
	
	CFDataRef			getPrintName()			{ return mPrintName; }
	SecKeychainItemRef	itemRef()				{ return mItemRef; }
	char				*kcFile()				{ return mKcFile; }
	
private:
	SecKeychainItemRef	mItemRef;
	CFDataRef			mPrintName;
	char				*mKcFile;		// file name of keychain this lives on 
};

SimplePickerItem::SimplePickerItem(SecKeychainItemRef itemRef)
	:	mItemRef(itemRef), 
		mPrintName(NULL), 
		mKcFile(NULL)
{
	OSStatus ortn = getKcItemAttr(itemRef, WA_PrintName, &mPrintName);
	if(ortn) {
		throw std::invalid_argument("printName attr not available");
	}
	
	/* stash name of the keychain this lives on */
	SecKeychainRef kcRef;
	ortn = SecKeychainItemCopyKeychain(itemRef, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainItemCopyKeychain", ortn);
		mKcFile = strdup("Unnamed keychain");
	}
	else {
		mKcFile = kcFileName(kcRef);
	}

	mItemRef = mItemRef;
	CFRetain(mItemRef);
}

SimplePickerItem::~SimplePickerItem()
{
	if(mItemRef) {
		CFRelease(mItemRef);
	}
	if(mPrintName) {
		CFRelease(mPrintName);
	}
	if(mKcFile) {
		free(mKcFile);
	}
}

typedef std::vector<SimplePickerItem *> ItemVector;

/* 
 * add SimplePickerItem objects of specified type to a ItemVector. 
 */
static void getPickerItems(
	SecKeychainRef	kcRef,
	SecItemClass	itemClass,	// CSSM_DL_DB_RECORD_{PRIVATE,PRIVATE}_KEY, etc. 
	ItemVector		&itemVector)
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
			SimplePickerItem *pickerItem = new SimplePickerItem(kcItem);
			itemVector.push_back(pickerItem);
		}
		catch(...) {
			printf("**** item failed SimplePickerItem constructor ***\n");
			/* but keep going */
		}
		/* SimplePickerKey holds a ref */
		CFRelease(kcItem);
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

OSStatus singleItemPicker(
	SecKeychainRef		kcRef,		// NULL means the default list
	KP_ItemType			itemType,		
	bool				takeFirst,	// take first key found
	SecKeychainItemRef	*itemRef)	// RETURNED
{

	SecItemClass itemClass;		
	OSStatus ortn = noErr;
	int ires;
	int dex;
	int numItems = 0;
	std::vector<SimplePickerItem *> items;

	switch(itemType) {
		case KPI_PrivateKey:
			itemClass = kSecPrivateKeyItemClass;
			break;
		case KPI_PublicKey:
			itemClass = kSecPublicKeyItemClass;
			break;
		case KPI_Cert:
			itemClass = kSecCertificateItemClass;
			break;
		default:
			printf("***BRRZAP! Wrong ItemType for singleItemPicker()\n");
			return paramErr;
	}
	/* First create a arrays of all of the items, parsed and ready for use */
	getPickerItems(kcRef, itemClass, items);
	numItems = items.size();
	
	if(numItems == 0) {
		printf("...singleItemPicker: no keys found\n");
		return errSecItemNotFound;
	}
	if(takeFirst) {
		*itemRef = items[0]->itemRef();
		CFRetain(*itemRef);
		goto done;
	}
	
	for(dex=0; dex<numItems; dex++) {
		/* display */
		SimplePickerItem *pi = items[dex];
		printf("[%d] item     : ", dex); 
		printCfData(pi->getPrintName()); 
		printf("\n");
		printf("    keychain : %s\n", pi->kcFile());
	}
	while(1) {
		fpurge(stdin);
		printf("\nEnter item number or CR to quit : ");
		fflush(stdout);
		char resp[64];
		getString(resp, sizeof(resp));
		if(resp[0] == '\0') {
			ortn = CSSMERR_CSSM_USER_CANCELED;
			break;
		}
		ires = atoi(resp);
		if((ires < 0) || (ires >= numItems)) {
			printf("***Invalid entry. Type a number between 0 and %d\n", numItems);
			continue;
		}
		break;
	}
	
	if(ortn == noErr) {
		*itemRef = items[ires]->itemRef();
		CFRetain(*itemRef);
	}
	
done:
	/* clean out SimplePickerItem array */
	for(dex=0; dex<numItems; dex++) {
		delete items[dex];
	}
	return ortn;
}

