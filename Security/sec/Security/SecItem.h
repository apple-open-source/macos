/*
 * Copyright (c) 2006-2013 Apple Inc. All Rights Reserved.
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
    @header SecItem
    SecItem defines CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
*/

#ifndef _SECURITY_SECITEM_H_
#define _SECURITY_SECITEM_H_

#include <Security/SecBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>

__BEGIN_DECLS

/*!
    @enum Class Key Constant
    @discussion Predefined key constant used to get or set item class values in
        a dictionary. Its value is one of the constants defined in the Value
        Constants for kSecClass.
    @constant kSecClass Specifies a dictionary key whose value is the item's
        class code.  You use this key to get or set a value of type CFTypeRef
        that contains the item class code.
*/
extern CFTypeRef kSecClass
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @enum Class Value Constants
    @discussion Predefined item class constants used to get or set values in
        a dictionary. The kSecClass constant is the key and its value is one
        of the constants defined here.
    @constant kSecClassGenericPassword Specifies generic password items.
    @constant kSecClassInternetPassword Specifies Internet password items.
    @constant kSecClassCertificate Specifies certificate items.
    @constant kSecClassKey Specifies key items.
    @constant kSecClassIdentity Specifies identity items.
*/
extern CFTypeRef kSecClassGenericPassword
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern CFTypeRef kSecClassInternetPassword
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecClassCertificate
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern CFTypeRef kSecClassKey
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern CFTypeRef kSecClassIdentity
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);


/*!
    @enum Attribute Key Constants
    @discussion Predefined item attribute keys used to get or set values in a
        dictionary. Not all attributes apply to each item class. The table
        below lists the currently defined attributes for each item class:

    kSecClassGenericPassword item attributes:
        kSecAttrAccessible
        kSecAttrAccessGroup
        kSecAttrCreationDate
        kSecAttrModificationDate
        kSecAttrDescription
        kSecAttrComment
        kSecAttrCreator
        kSecAttrType
        kSecAttrLabel
        kSecAttrIsInvisible
        kSecAttrIsNegative
        kSecAttrAccount
        kSecAttrService
        kSecAttrGeneric

    kSecClassInternetPassword item attributes:
        kSecAttrAccessible
        kSecAttrAccessGroup
        kSecAttrCreationDate
        kSecAttrModificationDate
        kSecAttrDescription
        kSecAttrComment
        kSecAttrCreator
        kSecAttrType
        kSecAttrLabel
        kSecAttrIsInvisible
        kSecAttrIsNegative
        kSecAttrAccount
        kSecAttrSecurityDomain
        kSecAttrServer
        kSecAttrProtocol
        kSecAttrAuthenticationType
        kSecAttrPort
        kSecAttrPath

    kSecClassCertificate item attributes:
        kSecAttrAccessible
        kSecAttrAccessGroup
        kSecAttrCertificateType
        kSecAttrCertificateEncoding
        kSecAttrLabel
        kSecAttrSubject
        kSecAttrIssuer
        kSecAttrSerialNumber
        kSecAttrSubjectKeyID
        kSecAttrPublicKeyHash

    kSecClassKey item attributes:
        kSecAttrAccessible
        kSecAttrAccessGroup
        kSecAttrKeyClass
        kSecAttrLabel
        kSecAttrApplicationLabel
        kSecAttrIsPermanent
        kSecAttrApplicationTag
        kSecAttrKeyType
        kSecAttrKeySizeInBits
        kSecAttrEffectiveKeySize
        kSecAttrCanEncrypt
        kSecAttrCanDecrypt
        kSecAttrCanDerive
        kSecAttrCanSign
        kSecAttrCanVerify
        kSecAttrCanWrap
        kSecAttrCanUnwrap

    kSecClassIdentity item attributes:
        Since an identity is the combination of a private key and a
        certificate, this class shares attributes of both kSecClassKey and
        kSecClassCertificate.

     @constant kSecAttrAccessible Specifies a dictionary key whose value
     indicates when your application needs access to an item's data.  You
     should choose the most restrictive option that meets your application's
     needs to allow the system to protect that item in the best way possible.
     See the "kSecAttrAccessible Value Constants" section for a list of
     values which can be specified.
     IMPORTANT: This attribute is currently not supported for OS X keychain
     items, unless the kSecAttrSynchronizable attribute is also present. If
     both attributes are specified on either OS X or iOS, the value for the
     kSecAttrAccessible key may only be one whose name does not end with
     "ThisDeviceOnly", as those cannot sync to another device.

     @constant kSecAttrAccessGroup Specifies a dictionary key whose value is
     a CFStringRef indicating which access group a item is in.  The access
     groups that a particular application has membership in are determined by
     two entitlements for that application.  The application-identifier
     entitlement contains the application's single access group, unless
     there is a keychain-access-groups entitlement present.  The latter
     has as its value a list of access groups; the first item in this list
     is the default access group. Unless a specific access group is provided
     as the value of kSecAttrAccessGroup when SecItemAdd is called, new items
     are created in the application's default access group.  Specifying this
     attribute in SecItemCopyMatching, SecItemUpdate, or SecItemDelete calls
     limits the search to the specified access group (of which the calling
     application must be a member to obtain matching results.)  To share
     keychain items between multiple applications, each application must have
     a common group listed in its keychain-access-groups entitlement, and each
     must specify this shared access group name as the value for the
     kSecAttrAccessGroup key in the dictionary passed to SecItem functions.

     @constant kSecAttrSynchronizable Specifies a dictionary key whose value is
     a CFBooleanRef indicating whether the item in question can be synchronized.
     To add a new item which can be synced to other devices, or to obtain
     synchronizable results from a query, supply this key with a value of
     kCFBooleanTrue. If the key is not supplied, or has a value of
     kCFBooleanFalse, then no synchronizable items will be added or returned.
     A predefined value, kSecAttrSynchronizableAny, may be provided instead of
     kCFBooleanTrue if both synchronizable and non-synchronizable results are
     desired.

     IMPORTANT: Specifying the kSecAttrSynchronizable key has several caveats:

         - Updating or deleting items using the kSecAttrSynchronizable key will
           affect all copies of the item, not just the one on your local device.
           Be sure that it makes sense to use the same password on all devices
           before deciding to make a password synchronizable.
         - Only password items can currently be synchronized. Keychain syncing
           is not supported for certificates or cryptographic keys.
         - Items stored or obtained using the kSecAttrSynchronizable key cannot
           specify SecAccessRef-based access control with kSecAttrAccess. If a
           password is intended to be shared between multiple applications, the
           kSecAttrAccessGroup key must be specified, and each application
           using this password must have a 'keychain-access-groups' entitlement
           with the specified access group value.
         - Items stored or obtained using the kSecAttrSynchronizable key may
           not also specify a kSecAttrAccessible value which is incompatible
           with syncing (namely, those whose names end with "ThisDeviceOnly".)
         - Items stored or obtained using the kSecAttrSynchronizable key cannot
           be specified by reference. You must pass kSecReturnAttributes and/or
           kSecReturnData to retrieve results; kSecReturnRef is currently not
           supported for synchronizable items.
         - Persistent references to synchronizable items should be avoided;
           while they may work locally, they cannot be moved between devices,
           and may not resolve if the item is modified on some other device.
         - When specifying a query that uses the kSecAttrSynchronizable key,
           search keys are limited to the item's class and attributes.
           The only search constant which may be used is kSecMatchLimit; other
           constants using the kSecMatch prefix are not supported at this time.

     @constant kSecAttrSynchronizableAny Specifies that both synchronizable and
     non-synchronizable results should be returned from this query. This may be
     used as a value for the kSecAttrSynchronizable dictionary key in a call to
     SecItemCopyMatching, SecItemUpdate, or SecItemDelete.

    @constant kSecAttrCreationDate (read-only) Specifies a dictionary key whose
        value is the item's creation date. You use this key to get a value
        of type CFDateRef that represents the date the item was created.
    @constant kSecAttrModificationDate (read-only) Specifies a dictionary key
        whose value is the item's modification date. You use this key to get
        a value of type CFDateRef that represents the last time the item was
        updated.
    @constant kSecAttrDescription Specifies a dictionary key whose value is
        the item's description attribute. You use this key to set or get a
        value of type CFStringRef that represents a user-visible string
        describing this particular kind of item (e.g., "disk image password").
    @constant kSecAttrComment Specifies a dictionary key whose value is the
        item's comment attribute. You use this key to set or get a value of
        type CFStringRef containing the user-editable comment for this item.
    @constant kSecAttrCreator Specifies a dictionary key whose value is the
        item's creator attribute. You use this key to set or get a value of
        type CFNumberRef that represents the item's creator. This number is
        the unsigned integer representation of a four-character code (e.g.,
        'aCrt').
    @constant kSecAttrType Specifies a dictionary key whose value is the item's
        type attribute. You use this key to set or get a value of type
        CFNumberRef that represents the item's type. This number is the
        unsigned integer representation of a four-character code (e.g.,
        'aTyp').
    @constant kSecAttrLabel Specifies a dictionary key whose value is the
        item's label attribute. You use this key to set or get a value of
        type CFStringRef containing the user-visible label for this item.
    @constant kSecAttrIsInvisible Specifies a dictionary key whose value is the
        item's invisible attribute. You use this key to set or get a value
        of type CFBooleanRef that indicates whether the item is invisible
        (i.e., should not be displayed.)
    @constant kSecAttrIsNegative Specifies a dictionary key whose value is the
        item's negative attribute. You use this key to set or get a value of
        type CFBooleanRef that indicates whether there is a valid password
        associated with this keychain item. This is useful if your application
        doesn't want a password for some particular service to be stored in
        the keychain, but prefers that it always be entered by the user.
    @constant kSecAttrAccount Specifies a dictionary key whose value is the
        item's account attribute. You use this key to set or get a CFStringRef
        that contains an account name. (Items of class
        kSecClassGenericPassword, kSecClassInternetPassword have this
        attribute.)
    @constant kSecAttrService Specifies a dictionary key whose value is the
        item's service attribute. You use this key to set or get a CFStringRef
        that represents the service associated with this item. (Items of class
        kSecClassGenericPassword have this attribute.)
    @constant kSecAttrGeneric Specifies a dictionary key whose value is the
        item's generic attribute. You use this key to set or get a value of
        CFDataRef that contains a user-defined attribute. (Items of class
        kSecClassGenericPassword have this attribute.)
    @constant kSecAttrSecurityDomain Specifies a dictionary key whose value
        is the item's security domain attribute. You use this key to set or
        get a CFStringRef value that represents the Internet security domain.
        (Items of class kSecClassInternetPassword have this attribute.)
    @constant kSecAttrServer Specifies a dictionary key whose value is the
        item's server attribute. You use this key to set or get a value of
        type CFStringRef that contains the server's domain name or IP address.
        (Items of class kSecClassInternetPassword have this attribute.)
    @constant kSecAttrProtocol Specifies a dictionary key whose value is the
        item's protocol attribute. You use this key to set or get a value of
        type CFNumberRef that denotes the protocol for this item (see the
        SecProtocolType enum in SecKeychainItem.h). (Items of class
        kSecClassInternetPassword have this attribute.)
    @constant kSecAttrAuthenticationType Specifies a dictionary key whose value
        is the item's authentication type attribute. You use this key to set
        or get a value of type CFNumberRef that denotes the authentication
        scheme for this item (see the kSecAttrAuthenticationType value
        constants below).
    @constant kSecAttrPort Specifies a dictionary key whose value is the item's
        port attribute. You use this key to set or get a CFNumberRef value
        that represents an Internet port number. (Items of class
        kSecClassInternetPassword have this attribute.)
    @constant kSecAttrPath Specifies a dictionary key whose value is the item's
        path attribute, typically this is the path component of the URL.  You use
        this key to set or get a CFStringRef value that represents a path. (Items
        of class kSecClassInternetPassword have this attribute.)
    @constant kSecAttrSubject (read-only) Specifies a dictionary key whose
        value is the item's subject. You use this key to get a value of type
        CFDataRef that contains the X.500 subject name of a certificate.
        (Items of class kSecClassCertificate have this attribute.)
    @constant kSecAttrIssuer (read-only) Specifies a dictionary key whose value
        is the item's issuer. You use this key to get a value of type
        CFDataRef that contains the X.500 issuer name of a certificate. (Items
        of class kSecClassCertificate have this attribute.)
    @constant kSecAttrSerialNumber (read-only) Specifies a dictionary key whose
        value is the item's serial number.  You use this key to get a value
        of type CFDataRef that contains the serial number data of a
        certificate. (Items of class kSecClassCertificate have this
        attribute.)
    @constant kSecAttrSubjectKeyID (read-only) Specifies a dictionary key whose
        value is the item's subject key ID. You use this key to get a value
        of type CFDataRef that contains the subject key ID of a certificate.
        (Items of class kSecClassCertificate have this attribute.)
    @constant kSecAttrPublicKeyHash (read-only) Specifies a dictionary key
        whose value is the item's public key hash. You use this key to get a
        value of type CFDataRef that contains the hash of a certificate's
        public key. (Items of class kSecClassCertificate have this attribute.)
    @constant kSecAttrCertificateType (read-only) Specifies a dictionary key
        whose value is the item's certificate type. You use this key to get
        a value of type CFNumberRef that denotes the certificate type
        (Currently only the value of this attribute must be equal to the
        version of the X509 certificate.  So 1 for v1 2 for v2 and 3 for v3
        certificates). Only items of class kSecClassCertificate have this
        attribute.
    @constant kSecAttrCertificateEncoding (read-only) Specifies a dictionary
        key whose value is the item's certificate encoding. You use this key
        to get a value of type CFNumberRef that denotes the certificate
        encoding (Currently only the value 3 meaning
        kSecAttrCertificateEncodingDER is supported). Only items of class
        kSecClassCertificate have this attribute.
    @constant kSecAttrKeyClass (read only) Specifies a dictionary key whose
        value is one of kSecAttrKeyClassPublic, kSecAttrKeyClassPrivate or
        kSecAttrKeyClassSymmetric.
    @constant kSecAttrApplicationLabel Specifies a dictionary key whose value
        is the key's application label attribute. This is different from the
        kSecAttrLabel (which is intended to be human-readable). This attribute
        is used to look up a key programmatically; in particular, for keys of
        class kSecAttrKeyClassPublic and kSecAttrKeyClassPrivate, the value of
        this attribute is the hash of the public key.
    @constant kSecAttrIsPermanent Specifies a dictionary key whose value is a
        CFBooleanRef indicating whether the key in question will be stored
        permanently.
    @constant kSecAttrApplicationTag Specifies a dictionary key whose value is a
        CFDataRef containing private tag data.
    @constant kSecAttrKeyType Specifies a dictionary key whose value is a
        CFNumberRef indicating the algorithm associated with this key
        (Currently only the value 42 is supported, alternatively you can use
        kSecAttrKeyTypeRSA).
    @constant kSecAttrKeySizeInBits Specifies a dictionary key whose value
        is a CFNumberRef indicating the number of bits in this key.
    @constant kSecAttrEffectiveKeySize Specifies a dictionary key whose value
        is a CFNumberRef indicating the effective number of bits in this key.
        For example, a DES key has a kSecAttrKeySizeInBits of 64, but a
        kSecAttrEffectiveKeySize of 56 bits.
    @constant kSecAttrCanEncrypt Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        encrypt data.
    @constant kSecAttrCanDecrypt Specifies a dictionary key whose value is a
        CFBooleanRef indicating whether the key in question can be used to
        decrypt data.
    @constant kSecAttrCanDerive Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        derive another key.
    @constant kSecAttrCanSign Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        create a digital signature.
    @constant kSecAttrCanVerify Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        verify a digital signature.
    @constant kSecAttrCanWrap Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        wrap another key.
    @constant kSecAttrCanUnwrap Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        unwrap another key.
*/
extern CFTypeRef kSecAttrAccessible
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_4_0);
extern CFTypeRef kSecAttrAccessGroup
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_3_0);
extern CFTypeRef kSecAttrSynchronizable
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFTypeRef kSecAttrSynchronizableAny
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern CFTypeRef kSecAttrCreationDate
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrModificationDate
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrDescription
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrComment
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCreator
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrType
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrLabel
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrIsInvisible
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrIsNegative
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAccount
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrService
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrGeneric
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrSecurityDomain
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrServer
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocol
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAuthenticationType
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrPort
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrPath
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrSubject
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrIssuer
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrSerialNumber
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrSubjectKeyID
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrPublicKeyHash
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCertificateType
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCertificateEncoding
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrKeyClass
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrApplicationLabel
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrIsPermanent
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrApplicationTag
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrKeyType
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrKeySizeInBits
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrEffectiveKeySize
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCanEncrypt
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCanDecrypt
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCanDerive
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCanSign
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCanVerify
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCanWrap
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrCanUnwrap
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @enum kSecAttrAccessible Value Constants
    @discussion Predefined item attribute constants used to get or set values
        in a dictionary. The kSecAttrAccessible constant is the key and its
        value is one of the constants defined here.
        When asking SecItemCopyMatching to return the item's data, the error
        errSecInteractionNotAllowed will be returned if the item's data is not
        available until a device unlock occurs.
    @constant kSecAttrAccessibleWhenUnlocked Item data can only be accessed
        while the device is unlocked. This is recommended for items that only
        need be accesible while the application is in the foreground.  Items
        with this attribute will migrate to a new device when using encrypted
        backups.
    @constant kSecAttrAccessibleAfterFirstUnlock Item data can only be
        accessed once the device has been unlocked after a restart.  This is
        recommended for items that need to be accesible by background
        applications. Items with this attribute will migrate to a new device
        when using encrypted backups.
    @constant kSecAttrAccessibleAlways Item data can always be accessed
        regardless of the lock state of the device.  This is not recommended
        for anything except system use. Items with this attribute will migrate
        to a new device when using encrypted backups.
    @constant kSecAttrAccessibleWhenUnlockedThisDeviceOnly Item data can only
        be accessed while the device is unlocked. This is recommended for items
        that only need be accesible while the application is in the foreground.
        Items with this attribute will never migrate to a new device, so after
        a backup is restored to a new device, these items will be missing.
    @constant kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly Item data can
        only be accessed once the device has been unlocked after a restart.
        This is recommended for items that need to be accessible by background
        applications. Items with this attribute will never migrate to a new
        device, so after a backup is restored to a new device these items will
        be missing.
    @constant kSecAttrAccessibleAlwaysThisDeviceOnly Item data can always
        be accessed regardless of the lock state of the device.  This option
        is not recommended for anything except system use. Items with this
        attribute will never migrate to a new device, so after a backup is
        restored to a new device, these items will be missing.
*/
extern CFTypeRef kSecAttrAccessibleWhenUnlocked
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_4_0);
extern CFTypeRef kSecAttrAccessibleAfterFirstUnlock
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_4_0);
extern CFTypeRef kSecAttrAccessibleAlways
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_4_0);
extern CFTypeRef kSecAttrAccessibleWhenUnlockedThisDeviceOnly
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_4_0);
extern CFTypeRef kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_4_0);
extern CFTypeRef kSecAttrAccessibleAlwaysThisDeviceOnly
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_4_0);

/*!
    @enum kSecAttrProtocol Value Constants
    @discussion Predefined item attribute constants used to get or set values
		in a dictionary. The kSecAttrProtocol constant is the key and its
		value is one of the constants defined here.
    @constant kSecAttrProtocolFTP.
    @constant kSecAttrProtocolFTPAccount.
    @constant kSecAttrProtocolHTTP.
    @constant kSecAttrProtocolIRC.
    @constant kSecAttrProtocolNNTP.
    @constant kSecAttrProtocolPOP3.
    @constant kSecAttrProtocolSMTP.
    @constant kSecAttrProtocolSOCKS.
    @constant kSecAttrProtocolIMAP.
    @constant kSecAttrProtocolLDAP.
    @constant kSecAttrProtocolAppleTalk.
    @constant kSecAttrProtocolAFP.
    @constant kSecAttrProtocolTelnet.
    @constant kSecAttrProtocolSSH.
    @constant kSecAttrProtocolFTPS.
    @constant kSecAttrProtocolHTTPS.
    @constant kSecAttrProtocolHTTPProxy.
    @constant kSecAttrProtocolHTTPSProxy.
    @constant kSecAttrProtocolFTPProxy.
    @constant kSecAttrProtocolSMB.
    @constant kSecAttrProtocolRTSP.
    @constant kSecAttrProtocolRTSPProxy.
    @constant kSecAttrProtocolDAAP.
    @constant kSecAttrProtocolEPPC.
    @constant kSecAttrProtocolIPP.
    @constant kSecAttrProtocolNNTPS.
    @constant kSecAttrProtocolLDAPS.
    @constant kSecAttrProtocolTelnetS.
    @constant kSecAttrProtocolIMAPS.
    @constant kSecAttrProtocolIRCS.
    @constant kSecAttrProtocolPOP3S.
*/
extern CFTypeRef kSecAttrProtocolFTP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolFTPAccount
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolHTTP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolIRC
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolNNTP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolPOP3
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolSMTP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolSOCKS
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolIMAP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolLDAP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolAppleTalk
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolAFP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolTelnet
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolSSH
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolFTPS
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolHTTPS
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolHTTPProxy
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolHTTPSProxy
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolFTPProxy
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolSMB
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolRTSP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolRTSPProxy
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolDAAP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolEPPC
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolIPP
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolNNTPS
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolLDAPS
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolTelnetS
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolIMAPS
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolIRCS
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrProtocolPOP3S
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @enum kSecAttrAuthenticationType Value Constants
    @discussion Predefined item attribute constants used to get or set values
		in a dictionary. The kSecAttrAuthenticationType constant is the key
		and its value is one of the constants defined here.
    @constant kSecAttrAuthenticationTypeNTLM.
    @constant kSecAttrAuthenticationTypeMSN.
    @constant kSecAttrAuthenticationTypeDPA.
    @constant kSecAttrAuthenticationTypeRPA.
    @constant kSecAttrAuthenticationTypeHTTPBasic.
    @constant kSecAttrAuthenticationTypeHTTPDigest.
    @constant kSecAttrAuthenticationTypeHTMLForm.
    @constant kSecAttrAuthenticationTypeDefault.
*/
extern CFTypeRef kSecAttrAuthenticationTypeNTLM
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAuthenticationTypeMSN
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAuthenticationTypeDPA
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAuthenticationTypeRPA
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAuthenticationTypeHTTPBasic
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAuthenticationTypeHTTPDigest
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAuthenticationTypeHTMLForm
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecAttrAuthenticationTypeDefault
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @enum kSecAttrKeyClass Value Constants
    @discussion Predefined item attribute constants used to get or set values
		in a dictionary. The kSecAttrKeyClass constant is the key
		and its value is one of the constants defined here.
    @constant kSecAttrKeyClassPublic.
    @constant kSecAttrKeyClassPrivate.
    @constant kSecAttrKeyClassSymmetric.
*/
extern CFTypeRef kSecAttrKeyClassPublic
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern CFTypeRef kSecAttrKeyClassPrivate
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern CFTypeRef kSecAttrKeyClassSymmetric
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);

/*!
    @enum kSecAttrKeyType Value Constants
    @discussion Predefined item attribute constants used to get or set values
		in a dictionary. The kSecAttrKeyType constant is the key
		and its value is one of the constants defined here.
    @constant kSecAttrKeyTypeRSA.
    @constant kSecAttrKeyTypeEC.
*/
extern CFTypeRef kSecAttrKeyTypeRSA
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_2_0);
extern CFTypeRef kSecAttrKeyTypeEC
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_4_0);

/*!
    @enum Search Constants
    @discussion Predefined search constants used to set values in a query
        dictionary. You can specify a combination of search attributes and
        item attributes when looking for matching items with the
        SecItemCopyMatching function.
    @constant kSecMatchPolicy Specifies a dictionary key whose value is a
        SecPolicyRef. If provided, returned certificates or identities must
        verify with this policy.
    @constant kSecMatchIssuers Specifies a dictionary key whose value is a
        CFArray of X.500 names (of type CFDataRef). If provided, returned
        certificates or identities will be limited to those whose
        certificate chain contains one of the issuers provided in this list.
    @constant kSecMatchEmailAddressIfPresent Specifies a dictionary key whose
        value is a CFStringRef containing an RFC822 email address. If
        provided, returned certificates or identities will be limited to those
        that contain the address, or do not contain any email address.
    @constant kSecMatchSubjectContains Specifies a dictionary key whose value
        is a CFStringRef. If provided, returned certificates or identities
        will be limited to those containing this string in the subject.
    @constant kSecMatchCaseInsensitive Specifies a dictionary key whose value
        is a CFBooleanRef. If this value is kCFBooleanFalse, or is not
        provided, then case-sensitive string matching is performed.
    @constant kSecMatchTrustedOnly Specifies a dictionary key whose value is
        a CFBooleanRef. If provided with a value of kCFBooleanTrue, only
        certificates which can be verified back to a trusted anchor will be
        returned. If this value is kCFBooleanFalse, or is not provided, then
        both trusted and untrusted certificates may be returned.
    @constant kSecMatchValidOnDate Specifies a dictionary key whose value is
        of type CFDateRef. If provided, returned keys, certificates or
        identities will be limited to those which are valid for the given date.
        Pass a value of kCFNull to indicate the current date.
    @constant kSecMatchLimit Specifies a dictionary key whose value is a
        CFNumberRef. If provided, this value specifies the maximum number of
        results to return. If not provided, results are limited to the first
        item found. Predefined values are provided for a single item
        (kSecMatchLimitOne) and all matching items (kSecMatchLimitAll).
    @constant kSecMatchLimitOne Specifies that results are limited to the first
        item found; used as a value for the kSecMatchLimit dictionary key.
    @constant kSecMatchLimitAll Specifies that an unlimited number of results
        may be returned; used as a value for the kSecMatchLimit dictionary
        key.
*/
extern CFTypeRef kSecMatchPolicy
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchItemList
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchSearchList
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchIssuers
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchEmailAddressIfPresent
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchSubjectContains
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchCaseInsensitive
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchTrustedOnly
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchValidOnDate
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchLimit
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchLimitOne
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecMatchLimitAll
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);


/*!
    @enum Return Type Key Constants
    @discussion Predefined return type keys used to set values in a dictionary.
        You use these keys to specify the type of results which should be
        returned by the SecItemCopyMatching or SecItemAdd function. You can
        specify zero or more of these return types. If more than one of these
        result types is specified, the result is returned as a CFDictionaryRef
        whose keys are the result types and values are the requested data.
    @constant kSecReturnData Specifies a dictionary key whose value is of type
        CFBooleanRef. A value of kCFBooleanTrue indicates that the data of
        an item (CFDataRef) should be returned. For keys and password
        items, data is secret (encrypted) and may require the user to enter
        a password for access.
    @constant kSecReturnAttributes Specifies a dictionary key whose value is
        of type CFBooleanRef. A value of kCFBooleanTrue indicates that the
        (non-encrypted) attributes of an item (CFDictionaryRef) should be
        returned.
    @constant kSecReturnRef Specifies a dictionary key whose value is a
        CFBooleanRef. A value of kCFBooleanTrue indicates that a reference
        should be returned. Depending on the item class requested, the
        returned reference(s) may be of type SecKeychainItemRef, SecKeyRef,
        SecCertificateRef, or SecIdentityRef.
    @constant kSecReturnPersistentRef Specifies a dictionary key whose value
        is of type CFBooleanRef. A value of kCFBooleanTrue indicates that a
        persistent reference to an item (CFDataRef) should be returned.
*/
extern CFTypeRef kSecReturnData
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecReturnAttributes
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecReturnRef
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecReturnPersistentRef
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);


/*!
    @enum Value Type Key Constants
    @discussion Predefined value type keys used to pass values in a dictionary.
        You can specify zero or more of these types depending on the function
        you are calling.  For SecItemCopyMatching or SecItemAdd these are
        used as keys in the results dictionary.
    @constant kSecValueData Specifies a dictionary key whose value is of type
        CFDataRef.  For keys and password items, data is secret (encrypted)
        and may require the user to enter a password for access.
    @constant kSecValueRef Specifies a dictionary key whose value, depending
        on the item class requested, is of type SecKeychainItemRef, SecKeyRef,
        SecCertificateRef, or SecIdentityRef.
    @constant kSecValuePersistentRef Specifies a dictionary key whose value
        is of type CFDataRef.  The bytes in this CFDataRef can be stored by
        the caller and used on a subsequent invocation of the application (or
        even a different application) to retrieve the item referenced by it.
*/
extern CFTypeRef kSecValueData
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecValueRef
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);
extern CFTypeRef kSecValuePersistentRef
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);


/*!
    @enum Other Constants
    @discussion Predefined constants used to set values in a dictionary.
    @constant kSecUseItemList Specifies a dictionary key whose value is a
        CFArray of items. If provided, this array is treated as the set of
        all possible items to search, or add if the API being called is
        SecItemAdd. The items in this array may be of type SecKeyRef,
        SecCertificateRef, SecIdentityRef, or CFDataRef (for a persistent
        item reference.) The items in the array must all be of the same
        type. When this attribute is provided, no keychains are searched.
*/
extern CFTypeRef kSecUseItemList
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @function SecItemCopyMatching
    @abstract Returns one or more items which match a search query.
    @param query A dictionary containing an item class specification and
        optional attributes for controlling the search. See the "Keychain
        Search Attributes" section for a description of currently defined
        search attributes.
    @param result On return, a CFTypeRef reference to the found item(s). The
        exact type of the result is based on the search attributes supplied
        in the query, as discussed below.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion Attributes defining a search are specified by adding key/value
        pairs to the query dictionary.

    A typical query consists of:

      * a kSecClass key, whose value is a constant from the Class
        Constants section that specifies the class of item(s) to be searched
      * one or more keys from the "Attribute Key Constants" section, whose value
        is the attribute data to be matched
      * one or more keys from the "Search Constants" section, whose value is
        used to further refine the search
      * a key from the "Return Type Key Constants" section, specifying the type of
        results desired

   Result types are specified as follows:

      * To obtain the data of a matching item (CFDataRef), specify
        kSecReturnData with a value of kCFBooleanTrue.
      * To obtain the attributes of a matching item (CFDictionaryRef), specify
        kSecReturnAttributes with a value of kCFBooleanTrue.
      * To obtain a reference to a matching item (SecKeychainItemRef,
        SecKeyRef, SecCertificateRef, or SecIdentityRef), specify kSecReturnRef
        with a value of kCFBooleanTrue.
      * To obtain a persistent reference to a matching item (CFDataRef),
        specify kSecReturnPersistentRef with a value of kCFBooleanTrue. Note
        that unlike normal references, a persistent reference may be stored
        on disk or passed between processes.
      * If more than one of these result types is specified, the result is
        returned as a CFDictionaryRef containing all the requested data.
      * If a result type is not specified, no results are returned.

    By default, this function returns only the first match found. To obtain
    more than one matching item at a time, specify kSecMatchLimit with a value
    greater than 1. The result will be a CFArrayRef containing up to that
    number of matching items; the items' types are described above.

    To filter a provided list of items down to those matching the query,
    specify a kSecMatchItemList whose value is a CFArray of SecKeychainItemRef,
    SecKeyRef, SecCertificateRef, or SecIdentityRef items. The objects in the
    provided array must be of the same type.

    To convert from a persistent item reference to a normal item reference,
    specify a kSecValuePersistentRef whose value a CFDataRef (the persistent
    reference), and a kSecReturnRef whose value is kCFBooleanTrue.
*/
OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result)
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @function SecItemAdd
    @abstract Add one or more items to a keychain.
    @param attributes A dictionary containing an item class specification and
        optional entries specifying the item's attribute values. See the
        "Attribute Key Constants" section for a description of currently defined
        attributes.
    @param result On return, a CFTypeRef reference to the newly added item(s).
        The exact type of the result is based on the values supplied
        in attributes, as discussed below. Pass NULL if this result is not
        required.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion Attributes defining an item are specified by adding key/value
        pairs to the attributes dictionary.  To add multiple items to a keychain
        at once use the kSecUseItemList key with an array of items as its value.
        This is currently only supported for non password items.

    Result types are specified as follows:

      * To obtain the data of the added item (CFDataRef), specify
        kSecReturnData with a value of kCFBooleanTrue.
      * To obtain all the attributes of the added item (CFDictionaryRef),
        specify kSecReturnAttributes with a value of kCFBooleanTrue.
      * To obtain a reference to the added item (SecKeychainItemRef, SecKeyRef,
        SecCertificateRef, or SecIdentityRef), specify kSecReturnRef with a
        value of kCFBooleanTrue.
      * To obtain a persistent reference to the added item (CFDataRef), specify
        kSecReturnPersistentRef with a value of kCFBooleanTrue. Note that
        unlike normal references, a persistent reference may be stored on disk
        or passed between processes.
      * If more than one of these result types is specified, the result is
        returned as a CFDictionaryRef containing all the requested data.
      * If a result type is not specified, no results are returned.
*/
OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result)
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @function SecItemUpdate
    @abstract Modify zero or more items which match a search query.
    @param query A dictionary containing an item class specification and
        optional attributes for controlling the search. See the "Attribute
        Constants" and "Search Constants" sections for a description of
        currently defined search attributes.
    @param attributesToUpdate A dictionary containing one or more attributes
        whose values should be set to the ones specified. Only real keychain
        attributes are permitted in this dictionary (no "meta" attributes are
        allowed.) See the "Attribute Key Constants" section for a description of
        currently defined value attributes.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion Attributes defining a search are specified by adding key/value
        pairs to the query dictionary.
*/
OSStatus SecItemUpdate(CFDictionaryRef query,
    CFDictionaryRef attributesToUpdate)
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @function SecItemDelete
    @abstract Delete zero or more items which match a search query.
    @param query A dictionary containing an item class specification and
        optional attributes for controlling the search. See the "Attribute
        Constants" and "Search Constants" sections for a description of
        currently defined search attributes.
    @result A result code.  See "Security Error Codes" (SecBase.h).
    @discussion Attributes defining a search are specified by adding key/value
        pairs to the query dictionary.

    By default, this function deletes all items matching the specified query.
    You can change this behavior by specifying one of the follow keys:

      * To delete an item identified by a transient reference, specify
        kSecValueRef with a reference returned by using the kSecReturnRef
        key in a previous call to SecItemCopyMatching or SecItemAdd.
      * To delete an item identified by a persistent reference, specify
        kSecValuePersistentRef with a persistent reference returned by
        using the kSecReturnPersistentRef key to SecItemCopyMatching or
        SecItemAdd.
      * To delete multiple items specify kSecMatchItemList with an array
        of references.
      * If more than one of these result keys is specified, the behavior is
        undefined.
*/
OSStatus SecItemDelete(CFDictionaryRef query)
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

__END_DECLS

#endif /* !_SECURITY_SECITEM_H_ */
