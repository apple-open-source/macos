/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
 @header SecItemPriv
 SecItemPriv defines private constants and SPI functions for access to
 Security items (certificates, identities, keys, and keychain items.)
 */

#ifndef _SECURITY_SECITEMPRIV_H_
#define _SECURITY_SECITEMPRIV_H_

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFError.h>
#include <Security/SecTask.h>

#if defined(__cplusplus)
extern "C" {
#endif

	/*!
	 @enum Class Value Constants (Private)
	 @discussion Predefined item class constants used to get or set values in
	 a dictionary. The kSecClass constant is the key and its value is one
	 of the constants defined here.
	 @constant kSecClassAppleSharePassword Specifies AppleShare password items.
	 */
	extern const CFTypeRef kSecClassAppleSharePassword;

	/*!
	 @enum Attribute Key Constants (Private)
	 @discussion Predefined item attribute keys used to get or set values in a
	 dictionary. Not all attributes apply to each item class. The table
	 below lists the currently defined attributes for each item class:

	 kSecClassGenericPassword item attributes:
	 kSecAttrAccessGroup
	 kSecAttrCreationDate
	 kSecAttrModificationDate
	 kSecAttrDescription
	 kSecAttrComment
	 kSecAttrCreator
	 kSecAttrType
	 kSecAttrScriptCode (private)
	 kSecAttrLabel
	 kSecAttrAlias (private)
	 kSecAttrIsInvisible
	 kSecAttrIsNegative
	 kSecAttrHasCustomIcon (private)
	 kSecAttrProtected (private)
	 kSecAttrAccount
	 kSecAttrService
	 kSecAttrGeneric

	 kSecClassInternetPassword item attributes:
	 kSecAttrAccessGroup
	 kSecAttrCreationDate
	 kSecAttrModificationDate
	 kSecAttrDescription
	 kSecAttrComment
	 kSecAttrCreator
	 kSecAttrType
	 kSecAttrScriptCode (private)
	 kSecAttrLabel
	 kSecAttrAlias (private)
	 kSecAttrIsInvisible
	 kSecAttrIsNegative
	 kSecAttrHasCustomIcon (private)
	 kSecAttrProtected (private)
	 kSecAttrAccount
	 kSecAttrSecurityDomain
	 kSecAttrServer
	 kSecAttrProtocol
	 kSecAttrAuthenticationType
	 kSecAttrPort
	 kSecAttrPath

	 kSecClassAppleSharePassword item attributes:
	 kSecAttrAccessGroup
	 kSecAttrCreationDate
	 kSecAttrModificationDate
	 kSecAttrDescription
	 kSecAttrComment
	 kSecAttrCreator
	 kSecAttrType
	 kSecAttrScriptCode (private)
	 kSecAttrLabel
	 kSecAttrAlias (private)
	 kSecAttrIsInvisible
	 kSecAttrIsNegative
	 kSecAttrHasCustomIcon (private)
	 kSecAttrProtected (private)
	 kSecAttrAccount
	 kSecAttrVolume
	 kSecAttrAddress
	 kSecAttrAFPServerSignature

	 kSecClassCertificate item attributes:
	 kSecAttrAccessGroup
	 kSecAttrCertificateType
	 kSecAttrCertificateEncoding
	 kSecAttrLabel
	 kSecAttrAlias (private)
	 kSecAttrSubject
	 kSecAttrIssuer
	 kSecAttrSerialNumber
	 kSecAttrSubjectKeyID
	 kSecAttrPublicKeyHash

	 kSecClassKey item attributes:
	 kSecAttrAccessGroup
	 kSecAttrKeyClass
	 kSecAttrLabel
	 kSecAttrAlias (private)
	 kSecAttrApplicationLabel
	 kSecAttrIsPermanent
	 kSecAttrIsPrivate (private)
	 kSecAttrIsModifiable (private)
	 kSecAttrApplicationTag
	 kSecAttrKeyCreator (private)
	 kSecAttrKeyType
	 kSecAttrKeySizeInBits
	 kSecAttrEffectiveKeySize
	 kSecAttrStartDate (private)
	 kSecAttrEndDate (private)
	 kSecAttrIsSensitive (private)
	 kSecAttrWasAlwaysSensitive (private)
	 kSecAttrIsExtractable (private)
	 kSecAttrWasNeverExtractable (private)
	 kSecAttrCanEncrypt
	 kSecAttrCanDecrypt
	 kSecAttrCanDerive
	 kSecAttrCanSign
	 kSecAttrCanVerify
	 kSecAttrCanSignRecover (private)
	 kSecAttrCanVerifyRecover (private)
	 kSecAttrCanWrap
	 kSecAttrCanUnwrap

	 kSecClassIdentity item attributes:
	 Since an identity is the combination of a private key and a
	 certificate, this class shares attributes of both kSecClassKey and
	 kSecClassCertificate.

	 @constant kSecAttrScriptCode Specifies a dictionary key whose value is the
	 item's script code attribute. You use this tag to set or get a value
	 of type CFNumberRef that represents a script code for this item's
	 strings. (Note: use of this attribute is deprecated; string attributes
	 should always be stored in UTF-8 encoding. This is currently private
	 for use by syncing; new code should not ever access this attribute.)
	 @constant kSecAttrAlias Specifies a dictionary key whose value is the
	 item's alias. You use this key to get or set a value of type CFDataRef
	 which represents an alias. For certificate items, the alias is either
	 a single email address, an array of email addresses, or the common
	 name of the certificate if it does not contain any email address.
	 (Items of class kSecClassCertificate have this attribute.)
	 @constant kSecAttrHasCustomIcon Specifies a dictionary key whose value is the
	 item's custom icon attribute. You use this tag to set or get a value
	 of type CFBooleanRef that indicates whether the item should have an
	 application-specific icon. (Note: use of this attribute is deprecated;
	 custom item icons are not supported in Mac OS X. This is currently
	 private for use by syncing; new code should not use this attribute.)
	 @constant kSecAttrVolume Specifies a dictionary key whose value is the
	 item's volume attribute. You use this key to set or get a CFStringRef
	 value that represents an AppleShare volume name. (Items of class
	 kSecClassAppleSharePassword have this attribute.)
	 @constant kSecAttrAddress Specifies a dictionary key whose value is the
	 item's address attribute. You use this key to set or get a CFStringRef
	 value that contains the AppleTalk zone name, or the IP or domain name
	 that represents the server address. (Items of class
	 kSecClassAppleSharePassword have this attribute.)
	 @constant kSecAttrAFPServerSignature Specifies a dictionary key whose value
	 is the item's AFP server signature attribute. You use this key to set
	 or get a CFDataRef value containing 16 bytes that represents the
	 server's signature block. (Items of class kSecClassAppleSharePassword
	 have this attribute.)
	 @constant kSecAttrCRLType (read-only) Specifies a dictionary key whose
	 value is the item's certificate revocation list type. You use this
	 key to get a value of type CFNumberRef that denotes the CRL type (see
	 the CSSM_CRL_TYPE enum in cssmtype.h). (Items of class
	 kSecClassCertificate have this attribute.)
	 @constant kSecAttrCRLEncoding (read-only) Specifies a dictionary key whose
	 value is the item's certificate revocation list encoding.  You use
	 this key to get a value of type CFNumberRef that denotes the CRL
	 encoding (see the CSSM_CRL_ENCODING enum in cssmtype.h). (Items of
	 class kSecClassCertificate have this attribute.)
	 @constant kSecAttrKeyCreator Specifies a dictionary key whose value is a
	 CFDataRef containing a CSSM_GUID structure representing the module ID of
	 the CSP that owns this key.
	 @constant kSecAttrIsPrivate Specifies a dictionary key whose value is a
	 CFBooleanRef indicating whether the raw key material of the key in
	 question is private.
	 @constant kSecAttrIsModifiable Specifies a dictionary key whose value is a
	 CFBooleanRef indicating whether any of the attributes of this key are
	 modifiable.
	 @constant kSecAttrStartDate Specifies a dictionary key whose value is a
	 CFDateRef indicating the earliest date on which this key may be used.
	 If kSecAttrStartDate is not present, the restriction does not apply.
	 @constant kSecAttrEndDate Specifies a dictionary key whose value is a
	 CFDateRef indicating the last date on which this key may be used.
	 If kSecAttrEndDate is not present, the restriction does not apply.
	 @constant kSecAttrWasAlwaysSensitive Specifies a dictionary key whose value
	 is a CFBooleanRef indicating that the key in question has always been
	 marked as sensitive.
	 @constant kSecAttrWasNeverExtractable Specifies a dictionary key whose value
	 is a CFBooleanRef indicating that the key in question has never been
	 marked as extractable.
	 @constant kSecAttrCanSignRecover Specifies a dictionary key whose value is a
	 CFBooleanRef indicating whether the key in question can be used to
	 perform sign recovery.
	 @constant kSecAttrCanVerifyRecover Specifies a dictionary key whose value is
	 a CFBooleanRef indicating whether the key in question can be used to
	 perform verify recovery.
	 @constant kSecAttrTombstone Specifies a dictionary key whose value is
	 a CFBooleanRef indicating that the item in question is a tombstone.
	 @constant kSecCredentialReference Specifies a CFDataRef containing CoreAuthentication
	 reference handle to be used when authorizing access to the item.
	 */
	extern CFTypeRef kSecAttrScriptCode;
	extern CFTypeRef kSecAttrAlias;
	extern CFTypeRef kSecAttrHasCustomIcon;
	extern CFTypeRef kSecAttrVolume;
	extern CFTypeRef kSecAttrAddress;
	extern CFTypeRef kSecAttrAFPServerSignature;
	extern CFTypeRef kSecAttrCRLType;
	extern CFTypeRef kSecAttrCRLEncoding;
	extern CFTypeRef kSecAttrKeyCreator;
	extern CFTypeRef kSecAttrIsPrivate;
	extern CFTypeRef kSecAttrIsModifiable;
	extern CFTypeRef kSecAttrStartDate;
	extern CFTypeRef kSecAttrEndDate;
	extern CFTypeRef kSecAttrWasAlwaysSensitive;
	extern CFTypeRef kSecAttrWasNeverExtractable;
	extern CFTypeRef kSecAttrCanSignRecover;
	extern CFTypeRef kSecAttrCanVerifyRecover;
	extern CFTypeRef kSecAttrTombstone;
	extern CFTypeRef kSecCredentialReference;

	/*!
	 @function SecItemCopyDisplayNames
	 @abstract Returns an array containing unique display names for each of the
	 certificates, keys, identities, or passwords in the provided items
	 array.
	 @param items An array containing items of type SecKeychainItemRef,
	 SecKeyRef, SecCertificateRef, or SecIdentityRef. All items in the
	 array should be of the same type.
	 @param displayNames On return, an array of CFString references containing
	 unique names for the supplied items. You are responsible for releasing
	 this array reference by calling the CFRelease function.
	 @result A result code. See "Security Error Codes" (SecBase.h).
	 @discussion Use this function to obtain item names which are suitable for
	 display in a menu or list view. The returned names are guaranteed to
	 be unique across the set of provided items.
	 */
	OSStatus SecItemCopyDisplayNames(CFArrayRef items, CFArrayRef *displayNames);

	/*!
	 @function SecItemDeleteAll
	 @abstract Removes all items from the keychain and added root certificates
	 from the trust store.
	 @result A result code. See "Security Error Codes" (SecBase.h).
	 */
	OSStatus SecItemDeleteAll(void);

/* Called by clients to push sync circle and message changes to us.
 Requires caller to have the kSecEntitlementKeychainSyncUpdates entitlement. */
CFArrayRef _SecKeychainSyncUpdateKeyParameter(CFDictionaryRef updates, CFErrorRef *error);
CFArrayRef _SecKeychainSyncUpdateCircle(CFDictionaryRef updates, CFErrorRef *error);
CFArrayRef _SecKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error);
    
CFDataRef _SecItemGetPersistentReference(CFTypeRef raw_item);

bool _SecKeychainBackupSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in, CFDictionaryRef *backup_out);
bool _SecKeychainRestoreSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECITEMPRIV_H_ */
