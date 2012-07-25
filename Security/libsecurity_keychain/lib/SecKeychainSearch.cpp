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

#include <Security/SecKeychainSearch.h>
#include <Security/SecKeychainSearchPriv.h>
#include <security_keychain/KCCursor.h>
#include <security_keychain/Item.h>

#include "SecBridge.h"

CFTypeID
SecKeychainSearchGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().KCCursorImpl.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecKeychainSearchCreateFromAttributes(CFTypeRef keychainOrArray, SecItemClass itemClass, const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef)
{
    BEGIN_SECAPI

	Required(searchRef);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, itemClass, attrList);
	*searchRef = cursor->handle();

	END_SECAPI
}


OSStatus
SecKeychainSearchCreateFromAttributesExtended(CFTypeRef keychainOrArray, SecItemClass itemClass, const SecKeychainAttributeList *attrList, CSSM_DB_CONJUNCTIVE dbConjunctive, CSSM_DB_OPERATOR dbOperator, SecKeychainSearchRef *searchRef)
{
    BEGIN_SECAPI
	
	Required(searchRef); // Make sure that searchRef is an invalid SearchRef
	
	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, itemClass, attrList, dbConjunctive, dbOperator);
	
	*searchRef = cursor->handle();
	
	END_SECAPI
}



OSStatus
SecKeychainSearchCopyNext(SecKeychainSearchRef searchRef, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI

	RequiredParam(itemRef);
	Item item;
	if (!KCCursorImpl::required(searchRef)->next(item))
		return errSecItemNotFound;

	*itemRef=item->handle();

	END_SECAPI
}
