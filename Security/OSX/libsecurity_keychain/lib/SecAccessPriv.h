/*
 * Copyright (c) 2002-2004,2011,2014 Apple Inc. All Rights Reserved.
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

/*!
	@function SecAccessCreateWithTrustedApplications
	@abstract Creates a SecAccess object with the specified trusted applications.
    @param trustedApplicationsPListPath A full path to the .plist file that contains the trusted applications. The extension must end in ".plist".
	@param accessLabel The access label for the new SecAccessRef.
	@param allowAny Flag that determines allow access to any application.
	@param returnedAccess On return, a new SecAccessRef.
	@result A result code.  See "Security Error Codes" (SecBase.h).
	@discussion The SecAccessCreateWithPList creates a SecAccess with the provided list of trusted applications. 
*/

OSStatus SecAccessCreateWithTrustedApplications(CFStringRef trustedApplicationsPListPath, CFStringRef accessLabel, Boolean allowAny, SecAccessRef* returnedAccess);

	
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECACCESS_PRIV_H_ */
