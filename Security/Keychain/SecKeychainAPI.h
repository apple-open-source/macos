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

/*!
	@header SecKeychainAPI
	The Security Keychain API contains all the APIs need to create a client and
	Keychain management application. It also contains a certificate, policy,
	identity and trust management API.

	NOTE: Any function with Create or Copy in the name returns an object that
	must be released.
*/
#ifndef _SECURITY_SECKEYCHAINAPI_H_
#define _SECURITY_SECKEYCHAINAPI_H_

#warning including obsolete header file SecKeychainAPI.h  Use #include <Security/Security.h> instead

#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>

#if defined(__cplusplus)
extern "C" {
#endif

OSStatus SecKeychainRelease(SecKeychainRef keychainRef);
OSStatus SecKeychainItemRelease(SecKeychainItemRef itemRef);
OSStatus SecKeychainSearchRelease(SecKeychainSearchRef searchRef);

/* OBSOLETE Please use SecKeychainSearchCopyNext instead. */
OSStatus SecKeychainCopySearchNextItem(SecKeychainSearchRef searchRef, SecKeychainItemRef *itemRef);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECKEYCHAINAPI_H_ */
