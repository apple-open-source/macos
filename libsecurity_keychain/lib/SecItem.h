/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <AvailabilityMacros.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*!
    @enum Class Key Constant
    @discussion Predefined key constant used to get or set item class values in
        a dictionary. The kSecClass constant is the key and its value is one
        of the constants defined in the Value Constants for kSecClass.
    @constant kSecClass Specifies a dictionary key whose value is the item's
        class code.  You use this key to get or set a value of type CFTypeRef
        that contains the item class code.
*/
extern const CFTypeRef kSecClass
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @enum Class Value Constants
    @discussion Predefined item class constants used to get or set values in
        a dictionary. The kSecClass constant is the key and its value is one
        of the constants defined here.
    @constant kSecClassGenericPassword Specifies generic password items; used
        as a value for the kSecClass dictionary key.
    @constant kSecClassInternetPassword Specifies Internet password items;
        used as a value for the kSecClass dictionary key.
    @constant kSecClassAppleSharePassword Specifies AppleShare password items;
        used as a value for the kSecClass dictionary key.
    @constant kSecClassCertificate Specifies certificate items; used as a value
        for the kSecClass dictionary key.
    @constant kSecClassPublicKey Specifies public key items; used as a value
        for the kSecClass dictionary key.
    @constant kSecClassPrivateKey Specifies private key items; used as a value
        for the kSecClass dictionary key.
    @constant kSecClassSymmetricKey Specifies symmetric key items (also known
        as session key items); used as a value for the kSecClass dictionary
        key.
    @constant kSecClassIdentity Specifies identity items; used as a value for
        the kSecClass dictionary key.
*/
extern const CFTypeRef kSecClassGenericPassword
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecClassInternetPassword
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecClassAppleSharePassword
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecClassCertificate
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecClassPublicKey
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecClassPrivateKey
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecClassSymmetricKey
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecClassIdentity
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


/*!
    @enum Attribute Key Constants
    @discussion Predefined item attribute keys used to get or set values in a
        dictionary. Not all attributes apply to each item class. The table
        below lists the currently defined attributes for each item class:

    kSecClassGenericPassword item attributes:
        kSecAttrCreationDate
        kSecAttrModifcationDate
        kSecAttrDescription
        kSecAttrComment
        kSecAttrCreator
        kSecAttrType
        kSecAttrScriptCode (private)
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrInvisible
        kSecAttrNegative
        kSecAttrCustomIcon (private)
        kSecAttrProtected (private)
        kSecAttrAccount
        kSecAttrService
        kSecAttrGeneric

    kSecClassInternetPassword item attributes:
        kSecAttrCreationDate
        kSecAttrModifcationDate
        kSecAttrDescription
        kSecAttrComment
        kSecAttrCreator
        kSecAttrType
        kSecAttrScriptCode (private)
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrInvisible
        kSecAttrNegative
        kSecAttrCustomIcon (private)
        kSecAttrProtected (private)
        kSecAttrAccount
        kSecAttrSecurityDomain
        kSecAttrServer
        kSecAttrProtocol
        kSecAttrAuthenticationType
        kSecAttrPort
        kSecAttrPath

    kSecClassAppleSharePassword item attributes:
        kSecAttrCreationDate
        kSecAttrModifcationDate
        kSecAttrDescription
        kSecAttrComment
        kSecAttrCreator
        kSecAttrType
        kSecAttrScriptCode (private)
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrInvisible
        kSecAttrNegative
        kSecAttrCustomIcon (private)
        kSecAttrProtected (private)
        kSecAttrAccount
        kSecAttrVolume
        kSecAttrAddress
        kSecAttrAFPServerSignature

    kSecClassCertificate item attributes:
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
        kSecAttrKeyClass
        kSecAttrLabel
        kSecAttrAlias (private)
        kSecAttrApplicationLabel
        kSecAttrPermanent
        kSecAttrPrivate
        kSecAttrModifiable
        kSecAttrApplicationTag
        kSecAttrKeyCreator
        kSecAttrKeyType
        kSecAttrKeySizeInBits
        kSecAttrEffectiveKeySize
        kSecAttrStartDate
        kSecAttrEndDate
        kSecAttrSensitive
        kSecAttrAlwaysSensitive
        kSecAttrExtractable
        kSecAttrNeverExtractable
        kSecAttrEncrypt
        kSecAttrDecrypt
        kSecAttrDerive
        kSecAttrSign
        kSecAttrVerify
        kSecAttrSignRecover
        kSecAttrVerifyRecover
        kSecAttrWrap
        kSecAttrUnwrap

    kSecClassIdentity item attributes:
        Since an identity is the combination of a private key and a
        certificate, this class shares attributes of both kSecClassKey and
        kSecClassCertificate.

    @constant kSecAttrCreationDate (read-only) Specifies a dictionary key whose
        value is the item's creation date. You use this key to get a value
        of type CFDateRef that represents the date the item was created.
    @constant kSecAttrModifcationDate (read-only) Specifies a dictionary key
        whose value is the item's modification date. You use this key to get
        a value of type CFDateRef that represents the last time the item was
        updated.
    @constant kSecAttrDescription Specifies a dictionary key whose value is
        the item's description attribute. You use this key to set or get a
        value of type CFStringRef that represents a user-visible string
        describing this particular kind of item (e.g. "disk image password").
    @constant kSecAttrComment Specifies a dictionary key whose value is the
        item's comment attribute. You use this key to set or get a value of
        type CFStringRef containing the user-editable comment for this item.
    @constant kSecAttrCreator Specifies a dictionary key whose value is the
        item's creator attribute. You use this key to set or get a value of
        type CFNumberRef that represents the item's creator. This number is
        the unsigned integer representation of a four-character code (e.g.
        'aCrt').
    @constant kSecAttrType Specifies a dictionary key whose value is the item's
        type attribute. You use this key to set or get a value of type
        CFNumberRef that represents the item's type. This number is the
        unsigned integer representation of a four-character code (e.g.
        'aTyp').
    @constant kSecAttrLabel Specifies a dictionary key whose value is the
        item's label attribute. You use this key to set or get a value of
        type CFStringRef containing the user-visible label for this item.
    @constant kSecAttrInvisible Specifies a dictionary key whose value is the
        item's invisible attribute. You use this key to set or get a value
        of type CFBooleanRef that indicates whether the item is invisible (i.e.
        should not be displayed.)
    @constant kSecAttrNegative Specifies a dictionary key whose value is the
        item's negative attribute. You use this key to set or get a value of
        type CFBooleanRef that indicates whether there is a valid password
        associated with this keychain item. This is useful if your application
        doesn't want a password for some particular service to be stored in
        the keychain, but prefers that it always be entered by the user.
    @constant kSecAttrAccount Specifies a dictionary key whose value is the
        item's account attribute. You use this key to set or get a CFStringRef
        that contains an account name. (Items of class
        kSecClassGenericPassword, kSecClassInternetPassword, and
        kSecClassAppleSharePassword have this attribute.)
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
        kSecClassInternetPassword and kSecClassAppleSharePassword have this
        attribute.)
    @constant kSecAttrAuthenticationType Specifies a dictionary key whose value
        is the item's authentication type attribute. You use this key to set
        or get a value of type CFNumberRef that denotes the authentication
        scheme for this item (see the SecAuthenticationType enum in
        SecKeychain.h). (Items of class kSecClassInternetPassword have this
        attribute.)
    @constant kSecAttrPort Specifies a dictionary key whose value is the item's
        port attribute. You use this key to set or get a CFNumberRef value
        that represents an Internet port number. (Items of class
        kSecClassInternetPassword have this attribute.)
    @constant kSecAttrPath Specifies a dictionary key whose value is the item's
        path attribute.  You use this key to set or get a CFStringRef value
        that represents a path. (Items of class kSecClassInternetPassword
        have this attribute.)
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
    @constant kSecAttrAlias Specifies a dictionary key whose value is the
        item's alias. You use this key to get or set a value of type CFDataRef
        which represents an alias. For certificate items, the alias is either
        a single email address, an array of email addresses, or the common
        name of the certificate if it does not contain any email address.
        (Items of class kSecClassCertificate have this attribute.)
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
        a value of type CFNumberRef that denotes the certificate type (see the
        CSSM_CERT_TYPE enum in cssmtype.h). (Items of class
        kSecClassCertificate have this attribute.)
    @constant kSecAttrCertificateEncoding (read-only) Specifies a dictionary
        key whose value is the item's certificate encoding. You use this key
        to get a value of type CFNumberRef that denotes the certificate
        encoding (see the CSSM_CERT_ENCODING enum in cssmtype.h). (Items of
        class kSecClassCertificate have this attribute.)
    @constant kSecAttrKeyClass (read only) Specifies a dictionary key whose
        value is one of kSecKeyClassPrivate, kSecKeyClassPublic or
        kSecKeyClassSymmetric.
    @constant kSecAttrApplicationLabel Specifies a dictionary key whose value
        is the key's application label attribute. This is different from the
        kSecAttrLabel (which is intended to be human-readable). This attribute
        is used to look up a key programmatically; in particular, for keys of
        class kSecKeyClassPrivate and kSecKeyClassPublic, the value of this
        attribute is the hash of the public key.
    @constant kSecAttrPermanent Specifies a dictionary key whose value is a
        CFBooleanRef indicating whether the key in question will be stored
        permanently.
    @constant kSecAttrPrivate Specifies a dictionary key whose value is a
        CFBooleanRef indicating whether the raw key material of the key in
        question is private.
    @constant kSecAttrModifiable Specifies a dictionary key whose value is a
        CFBooleanRef indicating whether any of the attributes of this key are
        modifiable.
    @constant kSecAttrApplicationTag Specifies a dictionary key whose value is a
        CFDataRef containing private tag data.
    @constant kSecAttrKeyCreator Specifies a dictionary key whose value is a
        CFDataRef containing a CSSM_GUID structure representing the module ID of
        the CSP which owns this key.
    @constant kSecAttrKeyType Specifies a dictionary key whose value is a
        CFNumberRef indicating the algorithm associated with this key (see the
        CSSM_ALGORITHMS enum in cssmtype.h).
    @constant kSecAttrKeySizeInBits Specifies a dictionary key whose value
        is a CFNumberRef indicating the number of bits in this key.
    @constant kSecAttrEffectiveKeySize Specifies a dictionary key whose value
        is a CFNumberRef indicating the effective number of bits in this key.
        For example, a DES key has a kSecAttrKeySizeInBits of 64, but a
        kSecAttrEffectiveKeySize of 56 bits.
    @constant kSecAttrStartDate Specifies a dictionary key whose value is a
        CFDateRef indicating the earliest date on which this key may be used.
        If kSecAttrStartDate is not present, the restriction does not apply.
    @constant kSecAttrEndDate Specifies a dictionary key whose value is a
        CFDateRef indicating the last date on which this key may be used.
        If kSecAttrEndDate is not present, the restriction does not apply.
    @constant kSecAttrSensitive Specifies a dictionary key whose value
        is a CFBooleanRef indicating whether the key in question must be wrapped
        with an algorithm other than CSSM_ALGID_NONE.
    @constant kSecAttrAlwaysSensitive Specifies a dictionary key whose value
        is a CFBooleanRef indicating that the key in question has always been
        marked as sensitive.
    @constant kSecAttrExtractable Specifies a dictionary key whose value
        is a CFBooleanRef indicating whether the key in question may be wrapped.
    @constant kSecAttrNeverExtractable Specifies a dictionary key whose value
        is a CFBooleanRef indicating that the key in question has never been
        marked as extractable.
    @constant kSecAttrEncrypt Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        encrypt data.
    @constant kSecAttrDecrypt Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        decrypt data.
    @constant kSecAttrDerive Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        derive another key.
    @constant kSecAttrSign Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        create a digital signature.
    @constant kSecAttrVerify Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        verify a digital signature.
    @constant kSecAttrSignRecover Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        perform sign recovery.
    @constant kSecAttrVerifyRecover Specifies a dictionary key whole value is
        a CFBooleanRef indicating whether the key in question can be used to
        perform verify recovery.
    @constant kSecAttrWrap Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        wrap another key.
    @constant kSecAttrUnwrap Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        unwrap another key.
*/
extern const CFTypeRef kSecAttrCreationDate
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrModifcationDate
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrDescription
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrComment
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrCreator
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrType
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrLabel
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrInvisible
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrNegative
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrAccount
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrService
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrGeneric
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrSecurityDomain
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrServer
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrProtocol
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrAuthenticationType
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrPort
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrPath
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrVolume
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrAddress
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrAFPServerSignature
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrAlias
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrSubject
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrIssuer
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrSerialNumber
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrSubjectKeyID
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrPublicKeyHash
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrCertificateType
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrCertificateEncoding
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrKeyClass
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrApplicationLabel
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrPermanent
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrPrivate
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrModifiable
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrApplicationTag
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrKeyCreator
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrKeyType
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrKeySizeInBits
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrEffectiveKeySize
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrStartDate
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrEndDate
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrSensitive
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrAlwaysSensitive
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrExtractable
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrNeverExtractable
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrEncrypt
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrDecrypt
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrDerive
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrSign
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrVerify
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrSignRecover
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrVerifyRecover
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrWrap
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrUnwrap
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @enum Search Constants
    @discussion Predefined search constants used to set values in a query
        dictionary. You can specify a combination of search attributes and
        item attributes when looking for matching items with the
        SecItemCopyMatching function.
    @constant kSecMatchKeyUsage Specifies a dictionary key whose value is a
        CFNumberRef representing allowable key uses (see the CSSM_KEYUSE enum
        in cssmtype.h). If provided, returned certificates or identities will
        be limited to those whose key usage matches this value.
    @constant kSecMatchPolicy Specifies a dictionary key whose value is a
        SecPolicyRef. If provided, returned certificates or identities must
        verify with this policy.
    @constant kSecMatchIssuers Specifies a dictionary key whose value is a
        CFArray of X.500 names (of type CFDataRef), as returned by the
        SSLCopyDistinguishedNames function (SecureTransport.h). If provided,
        returned certificates or identities will be limited to those whose
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
extern const CFTypeRef kSecMatchKeyUsage
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchPolicy
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchItemList
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchSearchList
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchIssuers
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchEmailAddressIfPresent
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchSubjectContains
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchCaseInsensitive
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchTrustedOnly
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchValidOnDate
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchLimit
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchLimitOne
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecMatchLimitAll
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


/*!
    @enum Return Type Constants
    @discussion Predefined return type keys used to set values in a dictionary.
        You use these keys to specify the type of results which should be
        returned by the SecItemCopyMatching function. You can specify zero
        or more of these return types. If more than one of these result types
        is specified, the result is returned as a CFDictionaryRef whose keys
        are the result types and values are the requested data.
    @constant kSecReturnData Specifies a dictionary key whose value is of type
        CFBooleanRef. A value of kCFBooleanTrue indicates that the data of
        an item should be returned. For keys and password items, data is
        secret (encrypted) and may require the user to enter a password for
        access.
    @constant kSecReturnAttributes Specifies a dictionary key whose value is
        of type CFBooleanRef. A value of kCFBooleanTrue indicates that the
        (non-encrypted) attributes of an item should be returned.
    @constant kSecReturnRef Specifies a dictionary key whose value is a
        CFBooleanRef. A value of kCFBooleanTrue indicates that a reference
        should be returned. Depending on the item class requested, the
        returned reference(s) may be of type SecKeychainItemRef, SecKeyRef,
        SecCertificateRef, or SecIdentityRef.
    @constant kSecReturnPersistentRef Specifies a dictionary key whose value
        is of type CFBooleanRef. A value of kCFBooleanTrue indicates that a
        persistent reference to an item (CFDataRef) should be returned.
*/
extern const CFTypeRef kSecReturnData
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecReturnAttributes
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecReturnRef
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecReturnPersistentRef
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


/*!
    @enum Other Constants
    @discussion Predefined constants used to set values in a dictionary.
    @constant kSecUseItemList Specifies a dictionary key whose value is a
        CFArray of items. If provided, this array is treated as the set of
        all possible items to search. The items in this array may be of type
        SecKeychainItemRef, SecKeyRef, SecCertificateRef, SecIdentityRef, or
        CFDataRef (for a persistent item reference.) The items in the array
        must all be of the same type. When this attribute is provided, no
        keychains are searched.
*/
extern const CFTypeRef kSecUseItemList
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

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
      * one or more keys from the Attribute Constants section, whose value
        is the attribute data to be matched
      * one or more keys from the Search Constants section, whose value is
        used to further refine the search
      * a key from the Return Type Constants section, specifying the type of
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

    By default, this function returns only the first match found. To obtain
    more than one matching item at a time, specify kSecMatchLimit with a value
    greater than 1. The result will be a CFArrayRef of results whose types
    are described above.

    To filter a provided list of items down to those matching the query,
    specify a kSecMatchItemList whose value is a CFArray of SecKeychainItemRef,
    SecKeyRef, SecCertificateRef, or SecIdentityRef items. The objects in the
    provided array must be of the same type.

    To convert from persistent item references to normal item references,
    specify a kSecMatchItemList whose value is a CFArray containing one or
    more CFDataRef elements (the persistent reference), and a kSecReturnRef
    whose value is kCFBooleanTrue. The objects in the provided array must be
    of the same type.
*/
OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result)
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @function SecItemAdd
    @abstract Add one or more items to a keychain.
    @param attributes A dictionary containing an item class specification and
        optional entries specifying the item's attribute values. See the
        Attribute Constants section for a description of currently defined
        attributes.
    @param result On return, a CFTypeRef reference to the newly added item(s).
        The exact type of the result is based on the values supplied
        in attributes, as discussed below. Pass NULL if this result is not
        required.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion Attributes defining an item are specified by adding key/value
        pairs to the attributes dictionary.

    Result types are specified as follows:

      * To obtain the data of the added item (CFDataRef), specify
        kSecReturnData with a value of kCFBooleanTrue.
      * To obtain all the attributes of the added item (CFDictionaryRef),
        specify kSecReturnAttributes with a value of kCFBooleanTrue.
      * To obtain a reference to the added item (SecKeychainItemRef, SecKeyRef,
        SecCertificateRef, or SecIdentityRef), specify kSecReturnRef with a
        value of kCFBooleanTrue. This is the default behavior if a result
        type is not explicitly specified.
      * To obtain a persistent reference to the added item (CFDataRef), specify
        kSecReturnPersistentRef with a value of kCFBooleanTrue. Note that
        unlike normal references, a persistent reference may be stored on disk
        or passed between processes.
      * If more than one of these result types is specified, the result is
        returned as a CFDictionaryRef containing all the requested data.

    To specify a keychain where the items should be added, provide a
    kSecUseKeychain key whose value is a SecKeychainRef. If a keychain is not
    explicitly specified, the item is added to the user's default keychain.
*/
OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result)
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @function SecItemUpdate
    @abstract Modify zero or more items which match a search query.
    @param query A dictionary containing an item class specification and
        optional attributes for controlling the search. See the Attribute
        Constants and Search Constants sections for a description of currently
        defined search attributes.
    @param attributesToUpdate A dictionary containing one or more attributes
        whose values should be set to the ones specified. Only real keychain
        attributes are permitted in this dictionary (no "meta" attributes are
        allowed.) See the Attribute Constants section for a description of
        currently defined value attributes.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion Attributes defining a search are specified by adding key/value
        pairs to the query dictionary.
*/
OSStatus SecItemUpdate(CFDictionaryRef query,
    CFDictionaryRef attributesToUpdate)
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

/*!
    @function SecItemDelete
    @abstract Delete zero or more items which match a search query.
    @param query A dictionary containing an item class specification and
        optional attributes for controlling the search. See the Attribute
        Constants and Search Constants sections for a description of currently
        defined search attributes.
    @result A result code.  See "Security Error Codes" (SecBase.h).
    @discussion Attributes defining a search are specified by adding key/value
        pairs to the query dictionary.

    By default, this function deletes all items matching the specified query.
    You can change this behavior by specifying one of the follow keys:

      * To delete an item identified by a transient reference, specify
        kSecMatchItemList with a reference returned by using the kSecReturnRef
        key in a previous call to SecItemCopyMatching or SecItemAdd.
      * To delete an item identified by a persistent reference, specify
        kSecMatchItemList with a persistent reference returned by using the
        kSecReturnPersistentRef key to SecItemCopyMatching or SecItemAdd.
      * If more than one of these result keys is specified, the behavior is
        undefined.

    To specify the list of keychains from which items may be deleted, provide
    a kSecUseKeychainList whose value is an array of SecKeychainRef. If not
    explicitly specified, the user's default keychain list is searched.
*/
OSStatus SecItemDelete(CFDictionaryRef query)
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

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
OSStatus SecItemCopyDisplayNames(CFArrayRef items, CFArrayRef *displayNames)
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECITEM_H_ */
