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
#include <TargetConditionals.h>
#include <Security/SecBase.h>

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#include <Security/SecTask.h>
#endif

__BEGIN_DECLS

/*!
    @enum Class Value Constants (Private)
    @discussion Predefined item class constants used to get or set values in
        a dictionary. The kSecClass constant is the key and its value is one
        of the constants defined here.
    @constant kSecClassAppleSharePassword Specifies AppleShare password items.
*/
extern const CFStringRef kSecClassAppleSharePassword;


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
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

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
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

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
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

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
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

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
        kSecAttrSynchronizable
        kSecAttrSyncViewHint

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
    @constant kSecAttrIsSensitive Specifies a dictionary key whose value
        is a CFBooleanRef indicating whether the key in question must be wrapped
        with an algorithm other than CSSM_ALGID_NONE.
    @constant kSecAttrWasAlwaysSensitive Specifies a dictionary key whose value
        is a CFBooleanRef indicating that the key in question has always been
        marked as sensitive.
    @constant kSecAttrIsExtractable Specifies a dictionary key whose value
        is a CFBooleanRef indicating whether the key in question may be wrapped.
    @constant kSecAttrWasNeverExtractable Specifies a dictionary key whose value
        is a CFBooleanRef indicating that the key in question has never been
        marked as extractable.
    @constant kSecAttrCanSignRecover Specifies a dictionary key whole value is a
        CFBooleanRef indicating whether the key in question can be used to
        perform sign recovery.
    @constant kSecAttrCanVerifyRecover Specifies a dictionary key whole value is
        a CFBooleanRef indicating whether the key in question can be used to
        perform verify recovery.
    @constant kSecAttrTombstone Specifies a dictionary key whose value is
        a CFBooleanRef indicating that the item in question is a tombstone.
    @constant kSecAttrNoLegacy Specifies a dictionary key whose
        value is a CFBooleanRef indicating that the query must be run on the
        syncable backend even for non syncable items.
*/
extern const CFStringRef kSecAttrScriptCode;
extern const CFStringRef kSecAttrAlias;
extern const CFStringRef kSecAttrHasCustomIcon;
extern const CFStringRef kSecAttrVolume;
extern const CFStringRef kSecAttrAddress;
extern const CFStringRef kSecAttrAFPServerSignature;
extern const CFStringRef kSecAttrCRLType;
extern const CFStringRef kSecAttrCRLEncoding;
extern const CFStringRef kSecAttrKeyCreator;
extern const CFStringRef kSecAttrIsPrivate;
extern const CFStringRef kSecAttrIsModifiable;
extern const CFStringRef kSecAttrStartDate;
extern const CFStringRef kSecAttrEndDate;
extern const CFStringRef kSecAttrIsSensitive;
extern const CFStringRef kSecAttrWasAlwaysSensitive;
extern const CFStringRef kSecAttrIsExtractable;
extern const CFStringRef kSecAttrWasNeverExtractable;
extern const CFStringRef kSecAttrCanSignRecover;
extern const CFStringRef kSecAttrCanVerifyRecover;
extern const CFStringRef kSecAttrTombstone;
extern const CFStringRef kSecAttrNoLegacy
    __OSX_AVAILABLE(10.11) __IOS_AVAILABLE(9.3) __TVOS_AVAILABLE(9.3) __WATCHOS_AVAILABLE(2.3);
extern const CFStringRef kSecAttrSyncViewHint
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);
extern const CFStringRef kSecAttrMultiUser
    __OSX_AVAILABLE(10.11.5) __IOS_AVAILABLE(9.3) __TVOS_AVAILABLE(9.3) __WATCHOS_AVAILABLE(2.3);
extern const CFStringRef kSecAttrTokenOID
     __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);


/*!
    @enum kSecAttrAccessible Value Constants (Private)
    @constant kSecAttrAccessibleAlwaysPrivate Private alias for kSecAttrAccessibleAlways,
        which is going to be deprecated for 3rd party use.
    @constant kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate for kSecAttrAccessibleAlwaysThisDeviceOnly,
        which is going to be deprecated for 3rd party use.
*/
extern const CFStringRef kSecAttrAccessibleAlwaysPrivate
;//%%%    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);
extern const CFStringRef kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate
;//%%%    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/*  View Hint Constants */

extern const CFStringRef kSecAttrViewHintPCSMasterKey;
extern const CFStringRef kSecAttrViewHintPCSiCloudDrive;
extern const CFStringRef kSecAttrViewHintPCSPhotos;
extern const CFStringRef kSecAttrViewHintPCSCloudKit;
extern const CFStringRef kSecAttrViewHintPCSEscrow;
extern const CFStringRef kSecAttrViewHintPCSFDE;
extern const CFStringRef kSecAttrViewHintPCSMailDrop;
extern const CFStringRef kSecAttrViewHintPCSiCloudBackup;
extern const CFStringRef kSecAttrViewHintPCSNotes;
extern const CFStringRef kSecAttrViewHintPCSiMessage;
#if SEC_OS_IPHONE
extern const CFStringRef kSecAttrViewHintFeldspar;
#endif /* SEC_OS_IPHONE */
extern const CFStringRef kSecAttrViewHintPCSSharing;

extern const CFStringRef kSecAttrViewHintAppleTV;
extern const CFStringRef kSecAttrViewHintHomeKit;
extern const CFStringRef kSecAttrViewHintThumper;
extern const CFStringRef kSecAttrViewHintContinuityUnlock;
extern const CFStringRef kSecAttrViewHintAccessoryPairing;

#if SEC_OS_IPHONE
extern const CFStringRef kSecUseSystemKeychain
    __TVOS_AVAILABLE(9.2)
    __WATCHOS_AVAILABLE(3.0)
    __OSX_AVAILABLE(10.11.4)
    __IOS_AVAILABLE(9.3);

extern const CFStringRef kSecUseSyncBubbleKeychain
    __TVOS_AVAILABLE(9.2)
    __WATCHOS_AVAILABLE(3.0)
    __OSX_AVAILABLE(10.11.4)
    __IOS_AVAILABLE(9.3);
#endif /* SEC_OS_IPHONE */

/*!
    @enum Other Constants (Private)
    @discussion Predefined constants used to set values in a dictionary.
    @constant kSecUseTombstones Specifies a dictionary key whose value is a
        CFBooleanRef if present this overrides the default behaviour for when
        we make tombstones.  The default being we create tombstones for
        synchronizable items unless we are explicitly deleting or updating a
        tombstone.  Setting this to false when calling SecItemDelete or
        SecItemUpdate will ensure no tombstones are created.  Setting it to
        true will ensure we create tombstones even when deleting or updating non
        synchronizable items.
    @constant kSecUseKeychain Specifies a dictionary key whose value is a
        keychain reference. You use this key to specify a value of type
        SecKeychainRef that indicates the keychain to which SecItemAdd
        will add the provided item(s).
    @constant kSecUseKeychainList Specifies a dictionary key whose value is
        either an array of keychains to search (CFArrayRef), or a single
        keychain (SecKeychainRef). If not provided, the user's default
        keychain list is searched. kSecUseKeychainList is ignored if an
        explicit kSecUseItemList is also provided.  This key can be used
        for the SecItemCopyMatching, SecItemUpdate and SecItemDelete calls.
    @constant kSecUseCredentialReference Specifies a CFDataRef containing
        AppleCredentialManager reference handle to be used when authorizing access
        to the item.
    @constant kSecUseCallerName Specifies a dictionary key whose value
        is a CFStringRef that represents a user-visible string describing
        the caller name for which the application is attempting to authenticate.
        The caller must have 'com.apple.private.LocalAuthentication.CallerName'
        entitlement set to YES to use this feature, otherwise it is ignored.
*/
extern const CFStringRef kSecUseTombstones
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
extern const CFStringRef kSecUseCredentialReference
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);
extern const CFStringRef kSecUseCallerName
    __OSX_AVAILABLE(10.11.4) __IOS_AVAILABLE(9.3) __TVOS_AVAILABLE(9.3) __WATCHOS_AVAILABLE(2.3);

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

#if SEC_OS_IPHONE
/*!
 @function SecItemDeleteAllWithAccessGroups
 @abstract Deletes all items for each class for the given access groups
 @param accessGroups An array of access groups for the items
 @result A result code. See "Security Error Codes" (SecBase.h).
 @discussion Provided for use by MobileInstallation to allow cleanup after uninstall
    Requires entitlement "com.apple.private.uninstall.deletion"
 */
bool SecItemDeleteAllWithAccessGroups(CFArrayRef accessGroups, CFErrorRef *error);
#endif /* SEC_OS_IPHONE */

/*
    Ensure the escrow keybag has been used to unlock the system keybag before
    calling either of these APIs.
    The password argument is optional, passing NULL implies no backup password
    was set.  We're assuming there will always be a backup keybag, except in
    the OTA case where the loaded OTA backup bag will be used.
 */
CFDataRef _SecKeychainCopyBackup(CFDataRef backupKeybag, CFDataRef password);
CFDataRef _SecKeychainCopyOTABackup(void);
OSStatus _SecKeychainRestoreBackup(CFDataRef backup, CFDataRef backupKeybag,
    CFDataRef password);

#if SEC_OS_IPHONE
bool
_SecKeychainWriteBackupToFileDescriptor(CFDataRef backupKeybag, CFDataRef password, int fd, CFErrorRef *error);

bool
_SecKeychainRestoreBackupFromFileDescriptor(int fd, CFDataRef backupKeybag, CFDataRef password, CFErrorRef *error);

CFStringRef
_SecKeychainCopyKeybagUUIDFromFileDescriptor(int fd, CFErrorRef *error);
#endif /* SEC_OS_IPHONE */

OSStatus _SecKeychainBackupSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in, CFDictionaryRef *backup_out);
OSStatus _SecKeychainRestoreSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in);

/* Called by clients to push sync circle and message changes to us.
   Requires caller to have the kSecEntitlementKeychainSyncUpdates entitlement. */
CFArrayRef _SecKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error);

#if !TARGET_OS_IPHONE
CFDataRef _SecItemGetPersistentReference(CFTypeRef raw_item);
#endif

/* Returns an OSStatus value for the given CFErrorRef, returns errSecInternal if the
   domain of the provided error is not recognized.  Passing NULL returns errSecSuccess (0). */
OSStatus SecErrorGetOSStatus(CFErrorRef error);

bool _SecKeychainRollKeys(bool force, CFErrorRef *error);

CFDictionaryRef _SecSecuritydCopyWhoAmI(CFErrorRef *error);

#if SEC_OS_IPHONE
bool _SecSyncBubbleTransfer(CFArrayRef services, uid_t uid, CFErrorRef *error);
#else /* SEC_OS_IPHONE */
bool _SecSyncBubbleTransfer(CFArrayRef services, CFErrorRef *error);
#endif /* SEC_OS_IPHONE */

bool _SecSystemKeychainTransfer(CFErrorRef *error);
#if SEC_OS_IPHONE
bool _SecSyncDeleteUserViews(uid_t uid, CFErrorRef *error);
#endif /* SEC_OS_IPHONE */

OSStatus SecItemUpdateTokenItems(CFTypeRef tokenID, CFArrayRef tokenItemsAttributes);

#if SEC_OS_OSX
CFTypeRef SecItemCreateFromAttributeDictionary_osx(CFDictionaryRef refAttributes);
#endif

#if SEC_OS_IPHONE
/*!
 * @function SecCopyLastError
 * @abstract return the last CFErrorRef for this thread
 * @param status the error code returned from the API call w/o CFErrorRef or 0
 * @result NULL or a retained CFError of the matching error code
 *
 * @discussion There are plenty of API calls in Security.framework that
 * doesn't return an CFError in case of an error, many of them actually have
 * a CFErrorRef internally, but throw it away at the last moment.
 * This might be your chance to get hold of it. The status code pass in is there
 * to avoid stale copies of CFErrorRef.

 * Note, not all interfaces support returning a CFErrorRef on the thread local
 * storage. This is especially true when going though old CDSA style API.
 */

CFErrorRef
SecCopyLastError(OSStatus status)
    __TVOS_AVAILABLE(10.0)
    __WATCHOS_AVAILABLE(3.0)
    __IOS_AVAILABLE(10.0);


bool
SecItemUpdateWithError(CFDictionaryRef inQuery,
                       CFDictionaryRef inAttributesToUpdate,
                       CFErrorRef *error)
    __TVOS_AVAILABLE(10.0)
    __WATCHOS_AVAILABLE(3.0)
    __IOS_AVAILABLE(10.0);
#endif // SEC_OS_IPHONE

#if SEC_OS_OSX
/*!
 @function SecItemParentCachePurge
 @abstract Clear the cache of parent certificates used in SecItemCopyParentCertificates.
 */
void SecItemParentCachePurge();
#endif


#if SEC_OS_OSX_INCLUDES
/*!
 @function SecItemCopyParentCertificates
 @abstract Retrieve an array of possible issuing certificates for a given certificate.
 @param certificate A reference to a certificate whose issuers are being sought.
 @param context Pass NULL in this parameter to indicate that the default certificate
 source(s) should be searched. The default is to search all available keychains.
 Values of context other than NULL are currently ignored.
 @result An array of zero or more certificates whose normalized subject matches the
 normalized issuer of the provided certificate. Note that no cryptographic validation
 of the signature is performed by this function; its purpose is only to provide a list
 of candidate certificates.
 */
CFArrayRef SecItemCopyParentCertificates(SecCertificateRef certificate, void *context)
__OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);

/*!
 @function SecItemCopyStoredCertificate
 @abstract Retrieve the first stored instance of a given certificate.
 @param certificate A reference to a certificate.
 @param context Pass NULL in this parameter to indicate that the default certificate
 source(s) should be searched. The default is to search all available keychains.
 Values of context other than NULL are currently ignored.
 @result Returns a certificate reference if the given certificate exists in a keychain,
 or NULL if the certificate cannot be found in any keychain. The caller is responsible
 for releasing the returned certificate reference when finished with it.
 */
SecCertificateRef SecItemCopyStoredCertificate(SecCertificateRef certificate, void *context)
__OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_NA);
#endif /* SEC_OS_OSX */

__END_DECLS

#endif /* !_SECURITY_SECITEMPRIV_H_ */
