/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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

#include <Security/SecIdentitySearch.h>
#include <Security/IdentityCursor.h>
#include <Security/Identity.h>

#include "SecBridge.h"


CFTypeID
SecIdentitySearchGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().IdentityCursor.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecIdentitySearchCreate(
	CFTypeRef keychainOrArray,
	CSSM_KEYUSE keyUsage,
	SecIdentitySearchRef *searchRef)
{
    BEGIN_SECAPI

	Required(searchRef);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	SecPointer<IdentityCursor> identityCursor(new IdentityCursor (keychains, keyUsage));
	*searchRef = identityCursor->handle();

	END_SECAPI
}


OSStatus
SecIdentitySearchCopyNext(
	SecIdentitySearchRef searchRef, 
	SecIdentityRef *identityRef)
{
    BEGIN_SECAPI

	RequiredParam(identityRef);
	SecPointer<Identity> identityPtr;
	if (!IdentityCursor::required(searchRef)->next(identityPtr))
		return errSecItemNotFound;

	*identityRef = identityPtr->handle();

    END_SECAPI
}
