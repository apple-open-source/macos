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

#include <Security/SecKey.h>

#include "SecBridge.h"

#include <Security/Access.h>
#include <Security/Keychains.h>
#include <Security/KeyItem.h>

CFTypeID
SecKeyGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().keyItem.typeId;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}

OSStatus
SecKeyCreatePair(
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE publicKeyUsage,
	uint32 publicKeyAttr,
	CSSM_KEYUSE privateKeyUsage,
	uint32 privateKeyAttr,
	SecAccessRef initialAccess,
	SecKeyRef* publicKeyRef, 
	SecKeyRef* privateKeyRef)
{
	BEGIN_SECAPI

	Keychain keychain = Keychain::optional(keychainRef);
	RefPointer<Access> theAccess(initialAccess ? gTypes().access.required(initialAccess) : new Access("<key>"));
	RefPointer<KeyItem> pubItem, privItem;

	KeyItem::createPair(keychain,
        algorithm,
        keySizeInBits,
        contextHandle,
        publicKeyUsage,
        publicKeyAttr,
        privateKeyUsage,
        privateKeyAttr,
        theAccess,
        pubItem,
        privItem);

	// Return the generated keys.
	if (publicKeyRef)
		*publicKeyRef = gTypes().keyItem.handle(*pubItem);
	if (privateKeyRef)
		*privateKeyRef = gTypes().keyItem.handle(*privItem);

	END_SECAPI
}

OSStatus
SecKeyGetCSSMKey(SecKeyRef key, const CSSM_KEY **cssmKey)
{
	BEGIN_SECAPI

	Required(cssmKey) = &gTypes().keyItem.required(key)->cssmKey();

	END_SECAPI
}


//
// Private APIs
//

OSStatus
SecKeyGetCredentials(
	SecKeyRef keyRef,
	CSSM_ACL_AUTHORIZATION_TAG operation,
	SecCredentialType credentialType,
	const CSSM_ACCESS_CREDENTIALS **outCredentials)
{
	BEGIN_SECAPI

	RefPointer<KeyItem> keyItem(gTypes().keyItem.required(keyRef));
	Required(outCredentials) = keyItem->getCredentials(operation, credentialType);

	END_SECAPI
}

OSStatus
SecKeyImportPair(
	SecKeychainRef keychainRef,
	const CssmKey *publicCssmKey,
	const CssmKey *privateCssmKey,
	SecAccessRef initialAccess,
	SecKeyRef* publicKeyRef,
	SecKeyRef* privateKeyRef)
{
	BEGIN_SECAPI

	Keychain keychain = Keychain::optional(keychainRef);
	RefPointer<Access> theAccess(initialAccess ? gTypes().access.required(initialAccess) : new Access("<key>"));
	RefPointer<KeyItem> pubItem, privItem;

	KeyItem::importPair(keychain,
		Required(publicCssmKey),
		Required(privateCssmKey),
        theAccess,
        pubItem,
        privItem);

	// Return the generated keys.
	if (publicKeyRef)
		*publicKeyRef = gTypes().keyItem.handle(*pubItem);
	if (privateKeyRef)
		*privateKeyRef = gTypes().keyItem.handle(*privItem);

	END_SECAPI
}
