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

#include <Security/Keychains.h>
#include <Security/KeyItem.h>
#include <Security/Item.h>

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
	if (id == gTypes().ItemImpl.typeID) {
		// keychain item. If it's in a protected group, return the group key
		if (SSGroup group = ItemImpl::required(SecKeychainItemRef(itemRef))->group())
			return &*group;
	} else if (id == gTypes().KeyItem.typeID) {
		// key item, return the key itself.
		if (CssmClient::Key key = KeyItem::required(SecKeyRef(itemRef))->key())
			return &*key;
	} else if (id == gTypes().KeychainImpl.typeID) {
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

	secdebug("kcitem", "SecKeychainItemGetTypeID()");
	return gTypes().ItemImpl.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecKeychainItemCreateFromContent(SecItemClass itemClass, SecKeychainAttributeList *attrList,
		UInt32 length, const void *data, SecKeychainRef keychainRef,
		SecAccessRef initialAccess, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemCreateFromContent(%lu, %p, %lu, %p, %p, %p)",
			itemClass, attrList, length, data, keychainRef, initialAccess);
		KCThrowParamErrIf_(length!=0 && data==NULL);
        Item item(itemClass, attrList, length, data);
		if (initialAccess)
			item->setAccess(Access::required(initialAccess));

        Keychain keychain = nil;
        try
        {
            keychain = Keychain::optional(keychainRef);
            if ( !keychain->exists() )
            {
                MacOSError::throwMe(errSecNoSuchKeychain);	// Might be deleted or not available at this time.
            }
        }
        catch(...)
        {
            keychain = globals().storageManager.defaultKeychainUI(item);
        }

        keychain->add(item);
        if (itemRef)
        	*itemRef = item->handle();
	END_SECAPI
}


OSStatus
SecKeychainItemModifyContent(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data)
{
    BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemModifyContent(%p, %p, %lu, %p)", itemRef, attrList, length, data);
		Item item = ItemImpl::required(itemRef);
		item->modifyContent(attrList, length, data);
	END_SECAPI
}


OSStatus
SecKeychainItemCopyContent(SecKeychainItemRef itemRef, SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData)
{
	BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemCopyContent(%p, %p, %p, %p, %p)",
			itemRef, itemClass, attrList, length, outData);
		Item item = ItemImpl::required(itemRef);
		item->getContent(itemClass, attrList, length, outData);
	END_SECAPI
}


OSStatus
SecKeychainItemFreeContent(SecKeychainAttributeList *attrList, void *data)
{
	BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemFreeContent(%p, %p)", attrList, data);
		ItemImpl::freeContent(attrList, data);
	END_SECAPI
}


OSStatus
SecKeychainItemModifyAttributesAndData(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data)
{
    BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemModifyAttributesAndData(%p, %p, %lu, %p)", itemRef, attrList, length, data);
		Item item = ItemImpl::required(itemRef);
		item->modifyAttributesAndData(attrList, length, data);
	END_SECAPI
}


OSStatus
SecKeychainItemCopyAttributesAndData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info, SecItemClass *itemClass, SecKeychainAttributeList **attrList, UInt32 *length, void **outData)
{
	BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemCopyAttributesAndData(%p, %p, %p, %p, %p, %p)", itemRef, info, itemClass, attrList, length, outData);
		Item item = ItemImpl::required(itemRef);
		item->getAttributesAndData(info, itemClass, attrList, length, outData);
	END_SECAPI
}


OSStatus
SecKeychainItemFreeAttributesAndData(SecKeychainAttributeList *attrList, void *data)
{
	BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemFreeAttributesAndData(%p, %p)", attrList, data);
		ItemImpl::freeAttributesAndData(attrList, data);
	END_SECAPI
}


OSStatus
SecKeychainItemDelete(SecKeychainItemRef itemRef)
{
    BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemFreeAttributesAndData(%p)", itemRef);
		Item item = ItemImpl::required( itemRef );
		Keychain keychain = item->keychain();
		KCThrowIf_( !keychain, errSecInvalidItemRef );
		
        keychain->deleteItem( item ); // item must be persistant.
	END_SECAPI
}


OSStatus
SecKeychainItemCopyKeychain(SecKeychainItemRef itemRef, SecKeychainRef* keychainRef)
{
    BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemCopyKeychain(%p, %p)", itemRef, keychainRef);
		Required(keychainRef) = ItemImpl::required(itemRef)->keychain()->handle();
	END_SECAPI
}


OSStatus
SecKeychainItemCreateCopy(SecKeychainItemRef itemRef, SecKeychainRef destKeychainRef,
	SecAccessRef initialAccess, SecKeychainItemRef *itemCopy)
{
    BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemCreateCopy(%p, %p, %p, %p)",
			itemRef, destKeychainRef, initialAccess, itemCopy);

		Item copy = ItemImpl::required(itemRef)->copyTo(Keychain::optional(destKeychainRef), Access::optional(initialAccess));
		if (itemCopy)
			*itemCopy = copy->handle();
	END_SECAPI
}


OSStatus
SecKeychainItemGetUniqueRecordID(SecKeychainItemRef itemRef, const CSSM_DB_UNIQUE_RECORD **uniqueRecordID)
{
    BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemGetUniqueRecordID(%p, %p)", itemRef, uniqueRecordID);
        Required(uniqueRecordID) = ItemImpl::required(itemRef)->dbUniqueRecord();
	END_SECAPI
}


OSStatus
SecKeychainItemGetDLDBHandle(SecKeychainItemRef itemRef, CSSM_DL_DB_HANDLE* dldbHandle)
{
    BEGIN_SECAPI
		secdebug("kcitem", "SecKeychainItemGetDLDBHandle(%p, %p)", itemRef, dldbHandle);
        *dldbHandle = ItemImpl::required(itemRef)->keychain()->database()->handle();
	END_SECAPI
}


OSStatus SecAccessCreateFromObject(CFTypeRef sourceRef,
	SecAccessRef *accessRef)
{
	BEGIN_SECAPI
	secdebug("kcitem", "SecAccessCreateFromObject(%p, %p)", sourceRef, accessRef);
	Required(accessRef);	// preflight
	SecPointer<Access> access = new Access(*aclBearer(sourceRef));
	*accessRef = access->handle();
	END_SECAPI
}


/*!
 */
OSStatus SecAccessModifyObject(SecAccessRef accessRef, CFTypeRef sourceRef)
{
	BEGIN_SECAPI
	secdebug("kcitem", "SecAccessModifyObject(%p, %p)", accessRef, sourceRef);
	Access::required(accessRef)->setAccess(*aclBearer(sourceRef), true);
	END_SECAPI
}

OSStatus
SecKeychainItemCopyAccess(SecKeychainItemRef itemRef, SecAccessRef* accessRef)
{
    BEGIN_SECAPI

	secdebug("kcitem", "SecKeychainItemCopyAccess(%p, %p)", itemRef, accessRef);
	Required(accessRef);	// preflight
	SecPointer<Access> access = new Access(*aclBearer(reinterpret_cast<CFTypeRef>(itemRef)));
	*accessRef = access->handle();

    END_SECAPI
}


OSStatus
SecKeychainItemSetAccess(SecKeychainItemRef itemRef, SecAccessRef accessRef)
{
    BEGIN_SECAPI

	secdebug("kcitem", "SecKeychainItemSetAccess(%p, %p)", itemRef, accessRef);
	Access::required(accessRef)->setAccess(*aclBearer(reinterpret_cast<CFTypeRef>(itemRef)), true);

    END_SECAPI
}
