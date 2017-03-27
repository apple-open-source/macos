/*
 * Copyright (c) 2014 - 2016 Apple Inc. All Rights Reserved.
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
 @function	SecBreadcrumbCreateFromPassword
 @abstract	Encryptes the password using a random key and then returns
 the encrypted password (breadcrumb) and the password encrypted random key.

 @param inPassword is the password to encrypt and use to encrypt the random key.
 @param outBreadcrumb is the password encrypted using a random key.
 @param outEncryptedKey is the random key encrypted using inPassword.
 @param outError An optional pointer to a CFErrorRef. This value is set
 if an error occurred. If not NULL, the caller is responsible for
 releasing the CFErrorRef.
 @result On return a Boolean indicating success or failure.

 @discussion This function generates the breadcrumb that will be used to
 update the user's keychain password when their Apple ID Login password
 is changed on appleid.apple.com.
*/

Boolean
SecBreadcrumbCreateFromPassword(CFStringRef inPassword,
	CFDataRef *outBreadcrumb,
	CFDataRef *outEncryptedKey,
	CFErrorRef *outError);


/*!
 @function	SecBreadcrumbCopyPassword
 @abstract	Decryptes the encrypted key using the password and uses the key to
 decrypt the breadcrumb and returns the password stored in the breadcrumb.

 @param inPassword is the password to decrypt the encrypted random key.
 @param inBreadcrumb is the breadcrumb encrypted by the key. It contains
 and encrypted version of the users old password.
 @param inEncryptedKey is an encrypted version of the key used to encrypt the
 breadcrumb.
 @param outPassword is the cleartext password that was stored in the breadcrumb.
 @param outError An optional pointer to a CFErrorRef. This value is set
 if an error occurred. If not NULL, the caller is responsible for
 releasing the CFErrorRef.
 @result On return a Boolean indicating success or failure.

 @discussion This function uses the password to decrypt the encrypted key and then
 uses that key to decrypt the breadcrumb.
*/

Boolean
SecBreadcrumbCopyPassword(CFStringRef inPassword,
	CFDataRef inBreadcrumb,
	CFDataRef inEncryptedKey,
	CFStringRef *outPassword,
	CFErrorRef *outError);

/*
 * Change password used to encrypt the key from old password to new password
 */

CFDataRef
SecBreadcrumbCreateNewEncryptedKey(CFStringRef oldPassword,
                                   CFStringRef newPassword,
                                   CFDataRef encryptedKey,
                                   CFErrorRef *outError);
