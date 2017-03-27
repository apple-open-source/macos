/*
 * Copyright (c) 2006,2011-2014 Apple Inc. All Rights Reserved.
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

#include <security_utilities/casts.h>
#include "SecKeychainItemExtendedAttributes.h"
#include "SecKeychainItemPriv.h"
#include "ExtendedAttribute.h"
#include "SecBridge.h"
#include "StorageManager.h"
#include "KCCursor.h"

/* I'm not sure we need this */
#if 0
CFTypeID SecKeychainItemExtendedAttributesGetTypeID(void);

static CFTypeID SecKeychainItemExtendedAttributesGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().ExtendedAttribute.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}
#endif

extern "C" Boolean SecKeyIsCDSAKey(SecKeyRef ref);

/*
 * Determine if incoming itemRef can be considered for 
 * this mechanism; throw if not.
 */
static void isItemRefCapable(
	SecKeychainItemRef			itemRef)
{
	CFTypeID id = CFGetTypeID(itemRef);
	if((id == gTypes().ItemImpl.typeID) ||
	   (id == gTypes().Certificate.typeID) || 
	   (id == SecKeyGetTypeID() && SecKeyIsCDSAKey((SecKeyRef)itemRef))) {
		return;
	}
	else {
		MacOSError::throwMe(errSecNoSuchAttr);
	}   
}	

static void cfStringToData(
	CFStringRef cfStr,
	CssmOwnedData &dst)
{
	CFDataRef cfData = CFStringCreateExternalRepresentation(NULL, cfStr,
		kCFStringEncodingUTF8, 0);
	if(cfData == NULL) {
		/* can't convert to UTF8!? */
		MacOSError::throwMe(errSecParam);
	}
	dst.copy(CFDataGetBytePtr(cfData), CFDataGetLength(cfData)); 
	CFRelease(cfData);
}

/*
 * Look up an ExtendedAttribute item associated with specified item.
 * Returns true if found, false if not. 
 * Throws errSecNoSuchAttr if item does not reside on a keychain.
 */
static bool lookupExtendedAttr(
	SecKeychainItemRef			itemRef,
	CFStringRef					attrName,
	Item						&foundItem)
{
	isItemRefCapable(itemRef);
	
	/* 
	 * Get the info about the extended attribute to look up:
	 * -- RecordType
	 * -- ItemID (i.e., PrimaryKey blob)
	 * -- AttributeName
	 */

	Item inItem = ItemImpl::required(itemRef);
	const CssmData &itemID	= inItem->itemID();
	CSSM_DB_RECORDTYPE recType = inItem->recordType();
	if(!inItem->keychain()) {
		/* item must reside on a keychain */
		MacOSError::throwMe(errSecNoSuchAttr);
	}
	
	CssmAutoData nameData(Allocator::standard());
	cfStringToData(attrName, nameData);
	CssmData nameCData = nameData;
	
	SecKeychainAttribute attrs[3];
	attrs[0].tag    = kExtendedAttrRecordTypeAttr;
	attrs[0].length = sizeof(UInt32);
	attrs[0].data   = (void *)&recType;
	attrs[1].tag    = kExtendedAttrItemIDAttr;
	attrs[1].length = (UInt32)itemID.Length;
	attrs[1].data   = itemID.Data;
	attrs[2].tag    = kExtendedAttrAttributeNameAttr;
	attrs[2].length = (UInt32)nameCData.Length;
	attrs[2].data   = nameCData.Data;
	SecKeychainAttributeList attrList = {3, attrs};
	
	StorageManager::KeychainList kcList;
	kcList.push_back(inItem->keychain());
	
	KCCursor cursor(kcList, (SecItemClass) CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE, &attrList);
	try {
		return cursor->next(foundItem);
	}
	catch(const CssmError &err) {
		if(err.error == CSSMERR_DL_INVALID_RECORDTYPE) {
			/* this keychain not set up for extended attributes yet */
			return false;
		}
		else {
			throw;
		}
	} 
}

OSStatus SecKeychainItemSetExtendedAttribute(
	SecKeychainItemRef			itemRef,
	CFStringRef					attrName,
	CFDataRef					attrValue)			/* NULL means delete the attribute */
{
    // <rdar://25635468>
    //%%% This needs to detect SecCertificateRef items, and when it does, SecKeychainItemDelete must be updated

    BEGIN_SECAPI
	
	if((itemRef == NULL) || (attrName == NULL)) {
		return errSecParam;
	}
	
	/* is there already a matching ExtendedAttribute item? */
	Item foundItem;
	bool haveMatch = lookupExtendedAttr(itemRef, attrName, foundItem);
	if(attrValue == NULL) {
		/* caller asking us to delete existing record */
		if(!foundItem) {
			return errSecNoSuchAttr;
		}
		foundItem->keychain()->deleteItem(foundItem);
		return errSecSuccess;
	}

	CSSM_DATA attrCValue = {int_cast<CFIndex, CSSM_SIZE>(CFDataGetLength(attrValue)), (uint8 *)CFDataGetBytePtr(attrValue)};
	
	if(haveMatch) {
		/* update existing extended attribute record */
		CssmDbAttributeInfo attrInfo(kExtendedAttrAttributeValueAttr, CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
		foundItem->setAttribute(attrInfo, attrCValue);
		foundItem->update();
	}
	else {
		/* create a new one, add it to the same keychain as itemRef */
		Item inItem = ItemImpl::required(itemRef);
		CssmAutoData nameData(Allocator::standard());
		cfStringToData(attrName, nameData);
		CssmData nameCData = nameData;
		SecPointer<ExtendedAttribute> extAttr(new ExtendedAttribute(
				inItem->recordType(), inItem->itemID(), nameCData,
				CssmData::overlay(attrCValue)));
		Item outItem(extAttr);
		inItem->keychain()->add(outItem);
	}
	
	END_SECAPI
}

OSStatus SecKeychainItemCopyExtendedAttribute(
	SecKeychainItemRef			itemRef,
	CFStringRef					attrName,
	CFDataRef					*attrValue)		/* RETURNED */
{
    // <rdar://25635468>
    //%%% This needs to detect SecCertificateRef items

    BEGIN_SECAPI
	
	if((itemRef == NULL) || (attrName == NULL) || (attrValue == NULL)) {
		return errSecParam;
	}
	
	Item foundItem;
	if(!lookupExtendedAttr(itemRef, attrName, foundItem)) {
		return errSecNoSuchAttr;
	}
	
	/* 
	 * Found it - its kExtendedAttrAttributeValueAttr value is what the 
	 * caller is looking for.
	 * We'd like to use getAttribute() here, but that requires that we know
	 * the size of the attribute before hand...
	 */
	UInt32 tag = kExtendedAttrAttributeValueAttr;
	UInt32 format = 0;
	SecKeychainAttributeInfo attrInfo = {1, &tag, &format};
	SecKeychainAttributeList *attrList = NULL;
	foundItem->getAttributesAndData(&attrInfo, NULL, &attrList, NULL, NULL);
	if((attrList == NULL) || (attrList->count != 1)) {
		/* should never happen... */
		MacOSError::throwMe(errSecNoSuchAttr);
	}
	*attrValue = CFDataCreate(NULL, (const UInt8 *)attrList->attr->data, 
		attrList->attr->length);
	ItemImpl::freeAttributesAndData(attrList, NULL);
	END_SECAPI
}

OSStatus SecKeychainItemCopyAllExtendedAttributes(
	SecKeychainItemRef			itemRef,
	CFArrayRef					*attrNames,			/* RETURNED, each element is a CFStringRef */
	CFArrayRef					*attrValues)		/* optional, RETURNED, each element is a 
													 *   CFDataRef */
{
    // <rdar://25635468>
    //%%% This needs to detect SecCertificateRef items, and when it does, SecKeychainItemDelete must be updated

    BEGIN_SECAPI
	
	if((itemRef == NULL) || (attrNames == NULL)) {
		return errSecParam;
	}

	isItemRefCapable(itemRef);
	
	/* 
	 * Get the info about the extended attribute to look up:
	 * -- RecordType
	 * -- ItemID (i.e., PrimaryKey blob)
	 */

	Item inItem = ItemImpl::required(itemRef);
	const CssmData &itemID	= inItem->itemID();
	CSSM_DB_RECORDTYPE recType = inItem->recordType();
	if(!inItem->keychain()) {
		/* item must reside on a keychain */
		MacOSError::throwMe(errSecNoSuchAttr);
	}
	
	SecKeychainAttribute attrs[2];
	attrs[0].tag    = kExtendedAttrRecordTypeAttr;
	attrs[0].length = sizeof(UInt32);
	attrs[0].data   = (void *)&recType;
	attrs[1].tag    = kExtendedAttrItemIDAttr;
	attrs[1].length = (UInt32)itemID.Length;
	attrs[1].data   = itemID.Data;
	SecKeychainAttributeList attrList = {2, attrs};
	
	StorageManager::KeychainList kcList;
	kcList.push_back(inItem->keychain());
	
	CFMutableArrayRef outNames = NULL;
	CFMutableArrayRef outValues = NULL;
	OSStatus ourRtn = errSecSuccess;
	
	KCCursor cursor(kcList, (SecItemClass) CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE, &attrList);
	for(;;) {
		bool gotOne = false;
		Item foundItem;
		try {
			gotOne = cursor->next(foundItem);
		}
		catch(...) {
			break;
		} 
		if(!gotOne) {
			break;
		}
		
		/* 
		 * Found one - return its kExtendedAttrAttributeNameAttr and
		 * (optionally) kExtendedAttrAttributeValueAttr attribute values
		 * to caller.
		 */
		UInt32 tags[2] = { kExtendedAttrAttributeNameAttr, kExtendedAttrAttributeValueAttr };
		UInt32 formats[2] = {0};
		SecKeychainAttributeInfo attrInfo = {2, tags, formats};
		SecKeychainAttributeList *attrList = NULL;
		foundItem->getAttributesAndData(&attrInfo, NULL, &attrList, NULL, NULL);
		if((attrList == NULL) || (attrList->count != 2)) {
			/* should never happen... */
			ourRtn = errSecNoSuchAttr;
			break;
		}
		if(outNames == NULL) {
			outNames = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}
		if((outValues == NULL) && (attrValues != NULL)) {
			/* this one's optional */
			outValues = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}
		
		/* 
		 * I don't see how we can assume that the order of the returned
		 * attributes is the same as the order of the tags we specified 
		 */
		for(unsigned dex=0; dex<2; dex++) {
			SecKeychainAttribute *attr = &attrList->attr[dex];
			CFDataRef cfd = NULL;
			CFStringRef cfs = NULL;
			switch(attr->tag) {
				case kExtendedAttrAttributeNameAttr:
					cfd = CFDataCreate(NULL, (const UInt8 *)attr->data, attr->length);
					
					/* We created this attribute's data via CFStringCreateExternalRepresentation, so
					 * this should always work... */
					cfs = CFStringCreateFromExternalRepresentation(NULL, cfd, kCFStringEncodingUTF8);
					CFArrayAppendValue(outNames, cfs);
					CFRelease(cfd);
					CFRelease(cfs);
					break;
				case kExtendedAttrAttributeValueAttr:
					if(outValues == NULL) {
						break;
					}
					cfd = CFDataCreate(NULL, (const UInt8 *)attr->data, attr->length);
					CFArrayAppendValue(outValues, cfd);
					CFRelease(cfd);
					break;
				default:
					/* should never happen, right? */
					MacOSError::throwMe(errSecInternalComponent);
			}
		}
		ItemImpl::freeAttributesAndData(attrList, NULL);
	}	/* main loop fetching matching Extended Attr records */
	
	if(ourRtn) {
		if(outNames) {
			CFRelease(outNames);
		}
		if(outValues) {
			CFRelease(outValues);
		}
		MacOSError::throwMe(ourRtn);
	}
	
	if(outNames == NULL) {
		/* no extended attributes found */
		return errSecNoSuchAttr;
	}
	*attrNames = outNames;
	if(outValues) {
		*attrValues = outValues;
	}
	
	END_SECAPI
}
