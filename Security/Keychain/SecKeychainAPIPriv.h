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


/*
 *  SecKeychainAPIPriv.h
 */
#ifndef _SECURITY_KEYCHAINAPIPRIV_H_
#define _SECURITY_KEYCHAINAPIPRIV_H_

#include <Security/SecBase.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Private keychain item attributes */
enum 
{
	kSecClassItemAttr            = 'clas',                       /* Item class (KCItemClass) */
	kSecAliasItemAttr            = 'alis',                       /* Alias attribute (required by CDSA). */
	kSecProtectedDataItemAttr    = 'prot',                       /* Item's data is protected (encrypted) (Boolean) */
};

UInt16
SecKeychainListGetCount(void);

OSStatus
SecKeychainListCopyKeychainAtIndex(UInt16 index, SecKeychainRef *keychainRef);

OSStatus
SecKeychainListRemoveKeychain(SecKeychainRef *keychainRef);

OSStatus SecKeychainChangePassword(SecKeychainRef keychainRef, UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword);

OSStatus SecKeychainCopyLogin(SecKeychainRef *keychainRef);

OSStatus SecKeychainLogin(UInt32 nameLength, void* name, UInt32 passwordLength, void* password);

OSStatus SecKeychainLogout();

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_KEYCHAINAPIPRIV_H_ */
