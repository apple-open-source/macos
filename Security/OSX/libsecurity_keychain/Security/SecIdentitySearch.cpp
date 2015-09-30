/*
 * Copyright (c) 2002-2004,2011,2014-2015 Apple Inc. All Rights Reserved.
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

#include <Security/SecIdentitySearch.h>
#include <Security/SecIdentitySearchPriv.h>
#include <Security/SecPolicyPriv.h>
#include <security_keychain/IdentityCursor.h>
#include <security_keychain/Identity.h>

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

OSStatus SecIdentitySearchCreateWithAttributes(
    CFDictionaryRef attributes,
    SecIdentitySearchRef* searchRef)
{
    BEGIN_SECAPI

    //
    // %%%TBI This function needs a new form of IdentityCursor that takes
    // the supplied attributes as input.
    //
	Required(searchRef);
	StorageManager::KeychainList keychains;
	globals().storageManager.getSearchList(keychains);
	SecPointer<IdentityCursor> identityCursor(new IdentityCursor (keychains, 0));
	*searchRef = identityCursor->handle();

    END_SECAPI
}

OSStatus SecIdentitySearchCreateWithPolicy(
    SecPolicyRef policy,
    CFStringRef idString,
    CSSM_KEYUSE keyUsage,
    CFTypeRef keychainOrArray,
    Boolean returnOnlyValidIdentities,
    SecIdentitySearchRef* searchRef)
{
    BEGIN_SECAPI

	Required(searchRef);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	CFRef<SecPolicyRef> policyRef = SecPolicyCreateItemImplInstance(policy);
	SecPointer<IdentityCursorPolicyAndID> identityCursor(new IdentityCursorPolicyAndID (keychains, keyUsage, idString, policyRef, returnOnlyValidIdentities));

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
