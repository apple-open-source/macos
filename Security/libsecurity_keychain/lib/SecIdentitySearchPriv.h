/*
 * Copyright (c) 2002-2010 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECIDENTITYSEARCHPRIV_H_
#define _SECURITY_SECIDENTITYSEARCHPRIV_H_

#include <Security/SecIdentitySearch.h>
#include <AvailabilityMacros.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*!
	@function SecIdentitySearchCreateWithAttributes
	@abstract Creates a search reference for finding identities that match specified attributes.
    @param attributes A dictionary containing optional attributes for controlling the search. Pass NULL to find all possible valid identities. See SecItem.h for a description of currently defined attributes.
    @param searchRef On return, an identity search reference. You are responsible for releasing this reference by calling the CFRelease function.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion This function is an advanced version of SecIdentitySearchCreate which allows finer-grained control over the search. The returned search reference is used to obtain matching identities in subsequent calls to the SecIdentitySearchCopyNext function. You must release the identity search reference by calling the CFRelease function.

	IMPORTANT: as of Mac OS X 10.7, this function is deprecated and will be removed in a future release.
	In 10.7 and later, you should use SecItemCopyMatching (see SecItem.h) to find identities that match specified attributes.
*/
OSStatus SecIdentitySearchCreateWithAttributes(CFDictionaryRef attributes, SecIdentitySearchRef* searchRef)
    /*AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;*/
	DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

/*!
	@function SecIdentitySearchCreateWithPolicy
	@abstract Creates a search reference for finding identities that match specified attributes.
	@param policy An optional policy reference. If provided, returned identities must be valid for this policy. Pass NULL to ignore policy when searching.
	@param idString An optional string containing a URI, RFC822 email address, DNS hostname, or other name which uniquely identifies the service requiring this identity. If a preferred identity has previously been specified for this name (see functions in SecIdentity.h), that identity will be returned first by the SecIdentitySearchCopyNext function. Pass NULL to ignore this string when searching.
	@param keyUsage A key usage value, as defined in cssmtype.h. Pass 0 to ignore key usage when searching.
	@param keychainOrArray A reference to an array of keychains to search, a single keychain, or NULL to search the user's default keychain search list.
	@param returnOnlyValidIdentities Pass TRUE to find only valid (non-expired) identities, or FALSE to obtain all identities which match the search criteria.
	@param searchRef On return, an identity search reference. You are responsible for releasing this reference by calling the CFRelease function.
	@result A result code. See "Security Error Codes" (SecBase.h).
	@discussion This function is an advanced version of SecIdentitySearchCreate which allows finer-grained control over the search. The returned search reference is used to obtain matching identities in subsequent calls to the SecIdentitySearchCopyNext function. You must release the identity search reference by calling the CFRelease function.

	IMPORTANT: as of Mac OS X 10.7, this function is deprecated and will be removed in a future release.
	In 10.7 and later, you should use SecItemCopyMatching (see SecItem.h) to find identities that match a given policy.
	
	To specify the policy which the identity must match, add this key/value pair to the query dictionary:
	- kSecMatchPolicy (value is the SecPolicyRef)
	
	To specify the service name which requires this identity, add this dictionary key:
	- kSecAttrService (value is a CFStringRef)
	
	To specify key usage(s) which the identity must have, add one or more of the following (values are CFBooleanRef):
	- kSecAttrCanEncrypt, kSecAttrCanDecrypt, kSecAttrCanDerive, kSecAttrCanSign, kSecAttrCanVerify, kSecAttrCanWrap, kSecAttrCanUnwrap

	To specify a list of keychains to search, add this dictionary key:
	- kSecMatchSearchList (value is a CFArrayRef containing one or more SecKeychainRef instances)
	
	To specify that only valid identities be returned, add this dictionary key:
	- kSecMatchTrustedOnly (value is a CFBooleanRef)
*/
OSStatus SecIdentitySearchCreateWithPolicy(SecPolicyRef policy, CFStringRef idString, uint32 keyUsage, CFTypeRef keychainOrArray, Boolean returnOnlyValidIdentities, SecIdentitySearchRef* searchRef)
	/*AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;*/
	DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECIDENTITYSEARCHPRIV_H_ */
