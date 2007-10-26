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
    @header SecItemPriv
    SecItemPriv defines private constants and SPI functions for access to
    Security items (certificates, identities, keys, and keychain items.)
*/

#ifndef _SECURITY_SECITEMPRIV_H_
#define _SECURITY_SECITEMPRIV_H_


#if defined(__cplusplus)
extern "C" {
#endif

/*!
    @enum Attribute Constants (Private)
    @discussion Predefined item attribute keys used to get or set values in a
        dictionary.
    @constant kSecAttrScriptCode Specifies a dictionary key whose value is the
        item's script code attribute. You use this tag to set or get a value
        of type CFNumberRef that represents a script code for this item's
        strings. (Note: use of this attribute is deprecated; string attributes
        should always be stored in UTF-8 encoding. This is currently private
        for use by syncing; new code should not ever access this attribute.)
    @constant kSecAttrCustomIcon Specifies a dictionary key whose value is the
        item's custom icon attribute. You use this tag to set or get a value
        of type CFBooleanRef that indicates whether the item should have an
        application-specific icon. (Note: use of this attribute is deprecated;
        custom item icons are not supported in Mac OS X. This is currently
        private for use by syncing; new code should not use this attribute.)
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
*/
extern const CFTypeRef kSecAttrScriptCode
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrCustomIcon
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrCRLType
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecAttrCRLEncoding
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


/*!
    @enum Other Constants (Private)
    @discussion Predefined constants used to set values in a dictionary.
    @constant kSecUseKeychain Specifies a dictionary key whose value is a
        keychain reference. You use this key to specify a value of type
        SecKeychainRef that indicates the keychain for an item.
    @constant kSecUseKeychainList Specifies a dictionary key whose value is
        either an array of keychains to search (CFArrayRef), or a single
        keychain (SecKeychainRef). If not provided, the user's default
        keychain list is searched. kSecUseKeychainList is ignored if an
        explicit kSecUseItemList is also provided.
*/
extern const CFTypeRef kSecUseKeychain
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
extern const CFTypeRef kSecUseKeychainList
    AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECITEMPRIV_H_ */
