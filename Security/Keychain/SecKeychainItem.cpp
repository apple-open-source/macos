/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

#include <Security/SecKeychainItem.h>

#include "SecBridge.h"
#include "KCExceptions.h"
#include "Access.h"


//
// Given a polymorphic Sec type object, return
// its AclBearer component.
// Note: Login ACLs are not hooked into this layer;
// modules or attachments have no Sec* layer representation.
//
RefPointer<AclBearer> aclBearer(CFTypeRef itemRef)
{
	// well, exactly what kind of something are you?
	CFTypeID id = CFGetTypeID(itemRef);
	if (id == gTypes().item.typeId) {
		// keychain item. If it's in a protected group, return the group key
		if (SSGroup group = gTypes().item.required(SecKeychainItemRef(itemRef))->group())
			return &*group;
	} else if (id == gTypes().keyItem.typeId) {
		// key item
		//@@@ not hooked up yet
	} else if (id == gTypes().keychain.typeId) {
		// keychain (this yields the database ACL)
		//@@@ not hooked up yet
	}
	// Guess not. Bummer
	MacOSError::throwMe(errSecNoAccessForItem);
}


CFTypeID
SecKeychainItemGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().item.typeId;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecKeychainItemCreateFromContent(SecItemClass itemClass, SecKeychainAttributeList *attrList,
		UInt32 length, const void *data, SecKeychainRef keychainRef,
		SecAccessRef initialAccess, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI
		KCThrowParamErrIf_(length!=0 && data==NULL);
        Item item(itemClass, attrList, length, data);
		if (initialAccess)
			item->setAccess(gTypes().access.required(initialAccess));
        Keychain::optional(keychainRef)->add(item);
        if (itemRef)
        	*itemRef = gTypes().item.handle(*item);
	END_SECAPI
}


OSStatus
SecKeychainItemModifyContent(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data)
{
    BEGIN_SECAPI
		Item item = gTypes().item.required(itemRef);
		item->modifyContent(attrList, length, data);
	END_SECAPI
}


OSStatus
SecKeychainItemCopyContent(SecKeychainItemRef itemRef, SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData)
{
	BEGIN_SECAPI
		Item item = gTypes().item.required(itemRef);
		item->getContent(itemClass, attrList, length, outData);
	END_SECAPI
}


OSStatus
SecKeychainItemFreeContent(SecKeychainAttributeList *attrList, void *data)
{
	BEGIN_SECAPI
		ItemImpl::freeContent(attrList, data);
	END_SECAPI
}


OSStatus
SecKeychainItemModifyAttributesAndData(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data)
{
    BEGIN_SECAPI
		Item item = gTypes().item.required(itemRef);
		item->modifyAttributesAndData(attrList, length, data);
	END_SECAPI
}


OSStatus
SecKeychainItemCopyAttributesAndData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info, SecItemClass *itemClass, SecKeychainAttributeList **attrList, UInt32 *length, void **outData)
{
	BEGIN_SECAPI
		Item item = gTypes().item.required(itemRef);
		item->getAttributesAndData(info, itemClass, attrList, length, outData);
	END_SECAPI
}


OSStatus
SecKeychainItemFreeAttributesAndData(SecKeychainAttributeList *attrList, void *data)
{
	BEGIN_SECAPI
		ItemImpl::freeAttributesAndData(attrList, data);
	END_SECAPI
}


OSStatus
SecKeychainItemDelete(SecKeychainItemRef itemRef)
{
    BEGIN_SECAPI
		Item item = gTypes().item.required( itemRef );
		Keychain keychain = item->keychain();
		KCThrowIf_( !keychain, errSecInvalidItemRef );
		
        keychain->deleteItem( item ); // item must be persistant.
	END_SECAPI
}


OSStatus
SecKeychainItemCopyKeychain(SecKeychainItemRef itemRef, SecKeychainRef* keychainRef)
{
    BEGIN_SECAPI
		Required(keychainRef) = gTypes().keychain.handle(*gTypes().item.required(itemRef)->keychain());
	END_SECAPI
}


OSStatus
SecKeychainItemCreateCopy(SecKeychainItemRef itemRef, SecKeychainRef destKeychainRef,
	SecAccessRef initialAccess, SecKeychainItemRef *itemCopy)
{
    BEGIN_SECAPI
		Item copy = gTypes().item.required(itemRef)->copyTo(Keychain::optional(destKeychainRef));
		if (itemCopy)
			*itemCopy = gTypes().item.handle(*copy);
	END_SECAPI
}


OSStatus
SecKeychainItemGetUniqueRecordID(SecKeychainItemRef keyItemRef, CSSM_DB_UNIQUE_RECORD* uniqueRecordID)
{
    BEGIN_SECAPI
        uniqueRecordID = gTypes().item.required(keyItemRef)->dbUniqueRecord();
	END_SECAPI
}


OSStatus
SecKeychainItemGetDLDBHandle(SecKeychainItemRef itemRef, CSSM_DL_DB_HANDLE* dldbHandle)
{
    BEGIN_SECAPI
        *dldbHandle = gTypes().item.required(itemRef)->keychain()->database()->handle();
	END_SECAPI
}


OSStatus SecAccessCreateFromObject(CFTypeRef sourceRef,
	SecAccessRef *accessRef)
{
	BEGIN_SECAPI
	Required(accessRef);	// preflight
	RefPointer<Access> access = new Access(*aclBearer(sourceRef));
	*accessRef = gTypes().access.handle(*access);
	END_SECAPI
}


/*!
 */
OSStatus SecAccessModifyObject(SecAccessRef accessRef, CFTypeRef sourceRef)
{
	BEGIN_SECAPI
	gTypes().access.required(accessRef)->setAccess(*aclBearer(sourceRef), true);
	END_SECAPI
}

OSStatus
SecKeychainItemCopyAccess(SecKeychainItemRef itemRef, SecAccessRef* accessRef)
{
    BEGIN_SECAPI

	Required(accessRef);	// preflight
	RefPointer<Access> access = new Access(*aclBearer(reinterpret_cast<CFTypeRef>(itemRef)));
	*accessRef = gTypes().access.handle(*access);

    END_SECAPI
}


OSStatus
SecKeychainItemSetAccess(SecKeychainItemRef itemRef, SecAccessRef accessRef)
{
    BEGIN_SECAPI

	gTypes().access.required(accessRef)->setAccess(*aclBearer(reinterpret_cast<CFTypeRef>(itemRef)), true);

    END_SECAPI
}
