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

#include <Security/SecKeychainSearch.h>

#include "SecBridge.h"

CFTypeID
SecKeychainSearchGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().cursor.typeId;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecKeychainSearchCreateFromAttributes(CFTypeRef keychainOrArray, SecItemClass itemClass, const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef)
{
    BEGIN_SECAPI

	Required(searchRef); // Make sure that searchRef is an invalid SearchRef

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, itemClass, attrList);
	*searchRef = gTypes().cursor.handle(*cursor);

	END_SECAPI
}


OSStatus
SecKeychainSearchCopyNext(SecKeychainSearchRef searchRef, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI

	RequiredParam(itemRef);
	Item item;
	if (!gTypes().cursor.required(searchRef)->next(item))
		return errSecItemNotFound;

	*itemRef=gTypes().item.handle(*item);

	END_SECAPI
}
