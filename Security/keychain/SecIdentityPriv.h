/*
 * Copyright (c) 2002-2011,2012-2013,2016 Apple Inc. All Rights Reserved.
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

/*!
    @header SecIdentityPriv
    The functions provided in SecIdentityPriv.h implement a convenient way to
    match private keys with certificates.
*/

#ifndef _SECURITY_SECIDENTITYPRIV_H_
#define _SECURITY_SECIDENTITYPRIV_H_

#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>
#include <CoreFoundation/CFBase.h>

__BEGIN_DECLS

/*! @function SecIdentityCreate
    @abstract create a new identity object from the provided certificate and its associated private key.
    @param allocator CFAllocator to allocate the identity object. Pass NULL to use the default allocator.
    @param certificate A certificate reference.
    @param privateKey A private key reference.
    @result An identity reference.
*/
SecIdentityRef SecIdentityCreate(
     CFAllocatorRef allocator,
     SecCertificateRef certificate,
     SecKeyRef privateKey)
    __SEC_MAC_AND_IOS_UNKNOWN;
    //__OSX_AVAILABLE_STARTING(__MAC_10_3, __SEC_IPHONE_UNKNOWN);

#if SEC_OS_OSX
/*!
    @function SecIdentityCompare
    @abstract Compares two SecIdentityRef instances for equality.
    @param identity1 An identity reference.
    @param identity2 An identity reference.
    @param compareOptions A value containing option flags. Currently there are no compare options, so 0 should be passed for this parameter.
    @result An enumerated value of type CFComparisonResult. See CFBase.h.
    @discussion Two identities are considered equal if they contain identical certificate and private key components.
    @deprecated in Mac OS X 10.5 and later; the CFEqual function should be used instead (CFBase.h).
 */
CFComparisonResult SecIdentityCompare(
    SecIdentityRef identity1,
    SecIdentityRef identity2,
    CFOptionFlags compareOptions)
     DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @function SecIdentityFindPreferenceItem
    @abstract Returns an identity preference item, given an identity string.
    @param keychainOrArray A reference to an array of keychains to search, a single keychain, or NULL to search the user's default keychain search list.
    @param idString A string containing a URI, hostname, or email (RFC822) address.
    @param itemRef On return, a reference to the keychain item which was found. The caller is responsible for releasing this reference.
    @result A result code.  See "Security Error Codes" (SecBase.h).
    @discussion An identity preference item maps a particular identity to a string, such as a URI or email address. It specifies that this identity should be preferred in transactions which match the provided string.
    @deprecated in Mac OS X 10.7 and later; use SecIdentityCopyPreferred() instead (SecIdentity.h)

     WARNING: This function is based on an implementation detail and will go away
     in a future release; its use should be avoided at all costs. It does not
     provide a way to find a preference item based on key usage, and it can only
     find preferences which are stored as keychain items, so it may fail to find
     the item you expect. Please use the public API functions to manipulate
     identity preferences.
*/
OSStatus SecIdentityFindPreferenceItem(
     CFTypeRef keychainOrArray,
     CFStringRef idString,
     SecKeychainItemRef *itemRef)
     DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

/*!
    @function SecIdentityAddPreferenceItem
    @abstract Adds a new identity preference item to the specified keychain.
    @param keychainRef A reference to the keychain in which to store the preference item. Pass NULL to specify the user's default keychain.
    @param identityRef An identity reference.
    @param idString A string containing a URI, hostname, or email (RFC822) address.
    @param itemRef On return, a reference to the new keychain item. The caller is responsible for releasing this reference. Pass NULL if the reference is not needed.
    @result A result code.  See "Security Error Codes" (SecBase.h).
    @discussion An identity preference item maps a particular identity to a string, such as a URI or email address. It specifies that this identity should be preferred in transactions which match the provided string.
    @deprecated in Mac OS X 10.5; use SecIdentitySetPreference() instead (SecIdentity.h).
*/
OSStatus SecIdentityAddPreferenceItem(
     SecKeychainRef keychainRef,
     SecIdentityRef identityRef,
     CFStringRef idString,
     SecKeychainItemRef *itemRef)
     DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @function SecIdentityUpdatePreferenceItem
    @abstract Given an existing identity preference keychain item, update it with the provided identity.
    @param itemRef An identity preference keychain item, as returned by SecIdentityFindPreferenceItem or SecIdentityAddPreferenceItem.
    @param identityRef An identity reference.
    @result A result code.  See "Security Error Codes" (SecBase.h).
    @discussion This function is used to update an existing preference item when a different identity is preferred.
    @deprecated in Mac OS X 10.5; use SecIdentitySetPreference() instead (SecIdentity.h).
*/
OSStatus SecIdentityUpdatePreferenceItem(
     SecKeychainItemRef itemRef,
     SecIdentityRef identityRef)
     DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @function SecIdentityCopyFromPreferenceItem
    @abstract Given an existing identity preference keychain item, obtain a SecIdentityRef for the identity it specifies.
    @param itemRef An identity preference keychain item, as returned by SecIdentityFindPreferenceItem or SecIdentityAddPreferenceItem.
    @param identityRef On return, an identity reference. The caller is responsible for releasing this reference.
    @result A result code.  See "Security Error Codes" (SecBase.h).
    @discussion This function is used to obtain a SecIdentityRef from an existing preference item.
    @deprecated in Mac OS X 10.5; use SecIdentityCopyPreference() instead (SecIdentity.h).
*/
OSStatus SecIdentityCopyFromPreferenceItem(
     SecKeychainItemRef itemRef,
     SecIdentityRef *identityRef)
     DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @function ConvertArrayToKeyUsage
    @abstract Given an array of key usages defined in SecItem.h return the equivalent CSSM_KEYUSE
    @param usage An CFArrayRef containing CFTypeRefs defined in SecItem.h
          kSecAttrCanEncrypt,
          kSecAttrCanDecrypt,
          kSecAttrCanDerive,
          kSecAttrCanSign,
          kSecAttrCanVerify,
          kSecAttrCanWrap,
          kSecAttrCanUnwrap
          If the CFArrayRef is NULL then the CSSM_KEYUSAGE will be CSSM_KEYUSE_ANY
    @result A CSSM_KEYUSE.  Derived from the passed in Array
*/
CSSM_KEYUSE ConvertArrayToKeyUsage(CFArrayRef usage)
  __SEC_MAC_ONLY_UNKNOWN;
#endif // SEC_OS_OSX

__END_DECLS

#endif /* _SECURITY_SECIDENTITYPRIV_H_ */
