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

/*!
	@header SecAccessPriv
	SecAccessPriv implements a way to set and manipulate access control rules and
	restrictions on SecKeychainItems. The functions here are private.
*/

#ifndef _SECURITY_SECACCESS_PRIV_H_
#define _SECURITY_SECACCESS_PRIV_H_

#include <Security/SecBase.h>
#include <Security/cssmtype.h>
#include <CoreFoundation/CFArray.h>


#if defined(__cplusplus)
extern "C" {
#endif

/*!
	@function SecKeychainAddIToolsPassword
	@abstract Creates a new iTools password using the access control list from iToolsTrustedApps.plist.
    @param keychain A reference to the keychain to which to add the password. Pass NULL to add the password to the default keychain.
	@param accountNameLength The length of the buffer pointed to by accountName.
	@param accountName A pointer to a string containing the account name associated with this password.
	@param passwordLength The length of the buffer pointed to by passwordData.
	@param passwordData A pointer to a buffer containing the password data to be stored in the keychain.
	@param itemRef On return, a reference to the new keychain item.
	@result A result code.  See "Security Error Codes" (SecBase.h).
	@discussion The SecKeychainAddIToolsPassword function adds a new iTools password to the specified keychain with an ACL composed of a list of trusted applications. A required parameter to identify the password is the accountName, which is an application-defined string. The servicename will always be "iTools". SecKeychainAddIToolsPassword optionally returns a reference to the newly added item. 
*/

OSStatus SecKeychainAddIToolsPassword(SecKeychainRef keychain, UInt32 accountNameLength, const char *accountName,
    UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECACCESS_PRIV_H_ */
