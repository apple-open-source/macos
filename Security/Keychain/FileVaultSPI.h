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
	@header FileVaultSPI
	The functions provided in FileVaultSPI implement code for Loginwindow to use when mounting
    FileVault home directories. Functions are also provided to allow recovery of an image with
    a recovery key pair (Master "Password")
*/

#ifndef _SECURITY_FILEVAULTSPI_H_
#define _SECURITY_FILEVAULTSPI_H_

#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <CoreFoundation/CoreFoundation.h>

#if defined(__cplusplus)
extern "C" {
#endif

#pragma mark -------------------- SecFileVault functions --------------------

/*!
	@function SecFileVaultCreate
    @abstract Creates a FileVault disk image. If you want to enable a recovery key and already have a certificate to use, you can call SecFileVaultCreateUsingCertificate.
    @param password The password for the image. This should be the same as the user's login password.
	@param enableMasterPassword If set, the first identity in the special keychain will be used as the master key for the disk image. 
    @param dmgout The file name and path for the FileVault disk image.
    @param volumeName The volume name for the mounted FileVault disk image (e.g. MYVOL).
    @param sizeSpec The size of the resulting FileVault disk image. See man hdiutil (e.g. CFSTR("20g")).
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/

OSStatus SecFileVaultCreate (CFStringRef password, bool enableMasterPassword, CFURLRef dmgout, CFStringRef volumeName,
    CFStringRef sizeSpec);

/*!
	@function SecFileVaultMount
    @abstract Used when logging in to mount a FileVault disk image.
    @param password The password for the image. This will be the same as the user's login password.
	@param enableMasterPassword If set, the first identity in the special keychain will be used as the master key for the disk image.
    @param dmgout The file name for the FileVault disk image. This will be the same as was specified with SecFileVaultCreate.
    @param mountpoint The mountpoint for the mounted FileVault disk image. This will be passed to "hdiutil mount" as the mountpoint parameter
    @param devicepath The devicepath for the mounted FileVault disk image. Caller is responsible for freeing this string
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/

OSStatus SecFileVaultUserMount (CFStringRef password, CFURLRef dmgin, CFURLRef mountpoint,CFStringRef *devicepath);

/*!
	@function SecFileVaultMasterMount
    @abstract To change the password for a FileVault disk image, the image must be mounted. After calling this, you can call SecFileVaultUserChangePassword to change the user's password. This is the same as SecFileVaultMount, except that the master key identity will be used to unlock the image.
    @param dmgin The file name for the FileVault disk image.
    @param mountpoint The mountpoint for the mounted FileVault disk image. This will be passed to "hdiutil mount" as the mountpoint parameter
    @param devicepath The devicepath for the mounted FileVault disk image. Caller is responsible for freeing this string
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecFileVaultMasterMount(CFURLRef dmgin, CFURLRef mountpoint,CFStringRef *devicepath);

/*!
	@function SecFileVaultUnmount
    @abstract Unmount a FileVault disk image. This will be called on logout. This is the inverse operation to SecFileVaultMount or SecFileVaultMasterMount. Essentially "hdiutil unmount -force <mountpoint>", followed by "hdiutil detach <devicepath>".
    @param mountpoint The mountpoint for the mounted FileVault disk image.
    @param devicepath The devicepath for the mounted FileVault disk image.
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecFileVaultUnmount(CFURLRef mountpoint,CFStringRef devicepath);

/*!
	@function SecFileVaultUserChangePassword
    @abstract Change the user password for a FileVault disk image to that given as the parameter.  This should be called when changing the user's account password info, or when resetting a user's forgotten password with . The image must have already been mounted with either SecFileVaultMount or SecFileVaultMasterMount.
    @param mountpoint The mountpoint for the mounted FileVault disk image.
    @param devicepath The devicepath for the mounted FileVault disk image.
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecFileVaultUserChangePassword(CFStringRef password, CFStringRef devicepath);

/*!
	@function SecFileVaultMakeMasterPassword
    @abstract This will create a special keychain in a special location if necessary and will generate a self-signed public/private key pair.  This is what to call if the user pushes an "Enable Master Password" button (irrevocable).
    @param masterPasswordPassword The password to use for the special keychain that will contain the key pair.
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecFileVaultMakeMasterPassword(CFStringRef masterPasswordPassword);

/*!
	@function SecFileVaultMasterPasswordEnabled
    @abstract This will return "true" if FileVault Master recovery keychain exists, and false if not. Note that this a machine-wide setting.
    @param keychainRef Returns a reference to the keychain, which you must release. Pass in NULL if you do not need a reference to the keychain
    @result "true" or "false".
*/
Boolean SecFileVaultMasterPasswordEnabled(SecKeychainRef *keychainRef);

/*!
	@function SecFileVaultChangeMasterPasswordPassword
    @abstract This will change the keychain password for the special Master Password keychain. This has the same effect as changing the password for this keychain with Keychain Access.
    @param oldPassword The current password for the special Master Password keychain containing the recovery key pair.
    @param newPassword The new password for the special Master Password keychain containing the recovery key pair.
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecFileVaultChangeMasterPasswordPassword(CFStringRef oldPassword,CFStringRef newPassword);

#pragma mark -------------------- SecFileVault extended functions --------------------

/*!
	@function SecFileVaultMount
    @abstract Used when logging in to mount a FileVault disk image.
    @param password The password for the image. This will be the same as the user's login password.
	@param certificate A certificate file in DER encoding (.cer extension).
    @param dmgout The file name for the FileVault disk image. This will be the same as was specified with SecFileVaultCreate.
    @param mountpoint The mountpoint for the mounted FileVault disk image. This will be passed to "hdiutil mount" as the mountpoint parameter
    @param devicepath The devicepath for the mounted FileVault disk image. Caller is responsible for freeing this string
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/

OSStatus SecFileVaultMount (CFStringRef password, CFURLRef certificate, CFURLRef dmgin, CFURLRef mountpoint,
    CFStringRef *devicepath);

/*!
	@function SecFileVaultCreateUsingCertificate
    @abstract Creates a FileVault disk image.
    @param password The password for the image. This should be the same as the user's login password.
	@param certificate A certificate file in DER encoding (.cer extension).
    @param dmgout The file name and path for the FileVault disk image.
    @param volumeName The volume name for the mounted FileVault disk image (e.g. MYVOL).
    @param sizeSpec The size of the resulting FileVault disk image. See man hdiutil (e.g. CFSTR("20g")).
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/

OSStatus SecFileVaultCreateUsingCertificate (CFStringRef password, CFURLRef certificate, CFURLRef dmgout,
    CFStringRef volumeName, CFStringRef sizeSpec);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_FILEVAULTSPI_H_ */
