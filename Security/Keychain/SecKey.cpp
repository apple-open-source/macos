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

CFTypeID
SecKeyGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().keyItem.typeId;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecKeyCreatePair(
	SecKeychainRef keychain,
	CSSM_ALGORITHMS algorithm,
	UInt32 keySizeInBits,
	CSSM_KEYUSE publicKeyUsage, 
	uint32 publicKeyAttr, 
	SecKeychainItemRef* publicKeyItemRef, 
	CSSM_KEYUSE privateKeyUsage, 
	uint32 privateKeyAttr, 
	SecKeychainItemRef* privateKeyItemRef,
	SecAccessRef initialAccess)
{
	BEGIN_SECAPI

	MacOSError::throwMe(unimpErr);//%%%for now

	END_SECAPI
}

OSStatus
SecKeyGetCSSMKey(SecKeyRef key, const CSSM_KEY **cssmKey)
{
	BEGIN_SECAPI

	Required(cssmKey) = &gTypes().keyItem.required(key)->cssmKey();

	END_SECAPI
}
