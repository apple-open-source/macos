/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 *  SecurityCore
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */
#ifndef __KEYCHAINAPIPRIV__
#define __KEYCHAINAPIPRIV__

#include <Security/SecKeychainAPI.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Private keychain item attributes */
enum 
{
	kSecClassItemAttr            = 'clas',                       /* Item class (KCItemClass) */
	kSecAliasItemAttr            = 'alis',                       /* Alias attribute (required by CDSA). */
	kSecProtectedDataItemAttr    = 'prot',                       /* Item's data is protected (encrypted) (Boolean) */

                                                                 /* Certificate attributes */
    kSecSubjectItemAttr          = 'subj',                       /* Subject distinguished name (DER-encoded data) */
    kSecCommonNameItemAttr       = 'cn  ',                       /* Common Name (UTF8-encoded string) */
    kSecIssuerItemAttr           = 'issu',                       /* Issuer distinguished name (DER-encoded data) */
    kSecSerialNumberItemAttr     = 'snbr',                       /* Certificate serial number (DER-encoded data) */
    kSecEMailItemAttr            = 'mail',                       /* E-mail address (ASCII-encoded string) */
    kSecPublicKeyHashItemAttr    = 'hpky',                       /* Hash of public key (PublicKeyHash), 20 bytes max. */
    kSecIssuerURLItemAttr        = 'iurl',                       /* URL of the certificate issuer (ASCII-encoded string) */
                                                                 /* Shared by keys and certificates */
    kSecEncryptItemAttr          = 'encr',                       /* Encrypt (Boolean) */
    kSecDecryptItemAttr          = 'decr',                       /* Decrypt (Boolean) */
    kSecSignItemAttr             = 'sign',                       /* Sign (Boolean) */
    kSecVerifyItemAttr           = 'veri',                       /* Verify (Boolean) */
    kSecWrapItemAttr             = 'wrap',                       /* Wrap (Boolean) */
    kSecUnwrapItemAttr           = 'unwr',                       /* Unwrap (Boolean) */
    kSecStartDateItemAttr        = 'sdat',                       /* Start Date (UInt32) */
    kSecEndDateItemAttr          = 'edat'                        /* End Date (UInt32) */
};

OSStatus SecKeychainChangePassword(SecKeychainRef keychainRef, UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword);

OSStatus SecKeychainCopyLogin(SecKeychainRef *keychainRef);

OSStatus SecKeychainLogin(UInt32 nameLength, void* name, UInt32 passwordLength, void* password);

OSStatus SecKeychainLogout();

#if defined(__cplusplus)
}
#endif

#endif // __KEYCHAINAPIPRIV__
