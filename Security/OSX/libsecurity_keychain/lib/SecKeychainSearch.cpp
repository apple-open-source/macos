/*
 * Copyright (c) 2000-2004,2011-2016 Apple Inc. All Rights Reserved.
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
#include <Security/SecCertificatePriv.h>
#include <security_keychain/KCCursor.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/Item.h>
#include <security_cdsa_utilities/Schema.h>
#include <syslog.h>

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
	KCCursorImpl *itemCursor = KCCursorImpl::required(searchRef);
	if (!itemCursor->next(item))
		return errSecItemNotFound;

	*itemRef=item->handle();

	bool itemChecked = false;
	do {
		/* see if we should convert outgoing item to a unified SecCertificateRef */
		SecItemClass tmpItemClass = Schema::itemClassFor(item->recordType());
		if (tmpItemClass == kSecCertificateItemClass) {
			SecPointer<Certificate> certificate(static_cast<Certificate *>(&*item));
			CssmData certData = certificate->data();
			CFDataRef data = NULL;
			if (certData.Data && certData.Length) {
				data = CFDataCreate(NULL, certData.Data, certData.Length);
			}
			if (!data) {
				/* zero-length or otherwise bad cert data; skip to next item */
				if (*itemRef) {
					CFRelease(*itemRef);
					*itemRef = NULL;
				}
				if (!itemCursor->next(item))
					return errSecItemNotFound;
				*itemRef=item->handle();
				continue;
			}
			SecKeychainItemRef tmpRef = *itemRef;
			*itemRef = (SecKeychainItemRef) SecCertificateCreateWithKeychainItem(NULL, data, tmpRef);
			if (data)
				CFRelease(data);
			if (tmpRef)
				CFRelease(tmpRef);
			if (NULL == *itemRef) {
				/* unable to create unified certificate item; skip to next item */
				if (!itemCursor->next(item))
					return errSecItemNotFound;
				*itemRef=item->handle();
				continue;
			}
			itemChecked = true;
		}
		else {
			itemChecked = true;
		}
	} while (!itemChecked);

	if (NULL == *itemRef) {
		/* never permit a NULL item reference to be returned without an error result */
		return errSecItemNotFound;
	}

	END_SECAPI
}
