/*
 * Copyright (c) 2003-2008 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECKEYCHAINITEMPRIV_H_
#define _SECURITY_SECKEYCHAINITEMPRIV_H_

#include <CoreFoundation/CFData.h>
#include <Security/SecBase.h>
#include <Security/SecKeychainItem.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Private keychain item attributes */
enum 
{
	kSecClassItemAttr            = 'clas',                       /* Item class (KCItemClass) */
	kSecProtectedDataItemAttr    = 'prot',                       /* Item's data is protected (encrypted) (Boolean) */
};

/* Temporary: CRL attributes */
enum 
{
	kSecCrlEncodingItemAttr			 = 'cren',
	kSecThisUpdateItemAttr			 = 'crtu',
	kSecNextUpdateItemAttr			 = 'crnu',
	kSecUriItemAttr					 = 'curi',	// URI from which it came	
	kSecCrlNumberItemAttr			 = 'crnm',
	kSecDeltaCrlNumberItemAttr		 = 'dlcr'
};

/* Unlock referral item attributes */
enum {
	kSecReferralTypeAttr			 = 'rtyp',	// type of referral
	kSecReferralDbNameAttr			 = 'rnam',	// database name
	kSecReferralDbGuidAttr			 = 'rgui',	// module GUID
	kSecReferralDbSSIDAttr			 = 'rssi',	// module subservice ID
	kSecReferralDbSSTypeAttr		 = 'rsty',	// subservice type
	kSecReferralDbNetnameAttr		 = 'rnnm',	// network name (blob)
	kSecReferralKeyLabelAttr		 = 'rlbl',	// key's Label
	kSecReferralKeyAppTagAttr		 = 'rkat'	// key's ApplicationTag
};


/* Extended Attribute record attributes */
enum {
	kExtendedAttrRecordTypeAttr		= 'eart',
	kExtendedAttrItemIDAttr			= 'eaii',
	kExtendedAttrAttributeNameAttr	= 'eaan',
	kExtendedAttrAttributeValueAttr	= 'eaav'
	/* also kSecModDateItemAttr from SecKeychainItem.h */
};

OSStatus SecKeychainItemCreateNew(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data, SecKeychainItemRef* itemRef);

OSStatus SecKeychainItemGetData(SecKeychainItemRef itemRef, UInt32 maxLength, void* data, UInt32* actualLength);

OSStatus SecKeychainItemGetAttribute(SecKeychainItemRef itemRef, SecKeychainAttribute* attribute, UInt32* actualLength);

OSStatus SecKeychainItemSetAttribute(SecKeychainItemRef itemRef, SecKeychainAttribute* attribute);

OSStatus SecKeychainItemAdd(SecKeychainItemRef itemRef);

OSStatus SecKeychainItemAddNoUI(SecKeychainRef keychainRef, SecKeychainItemRef itemRef);

OSStatus SecKeychainItemUpdate(SecKeychainItemRef itemRef);

OSStatus SecKeychainItemSetData(SecKeychainItemRef itemRef, UInt32 length, const void* data);

OSStatus SecKeychainItemFindFirst(SecKeychainRef keychainRef, const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef, SecKeychainItemRef *itemRef);

/*!
	@function SecKeychainItemCopyRecordIdentifier
	@abstract Returns the record identifier for a keychain item
	@param itemRef The item for which the localID is to be returned
	@param recordIdentifier The returned recordIdentifier
    @result A result code. See "Security Error Codes" (SecBase.h).
*/

OSStatus SecKeychainItemCopyRecordIdentifier(SecKeychainItemRef itemRef, CFDataRef *recordIdentifier);

/*!
	@function SecKeychainItemCopyFromRecordIdentifier
	@abstract Returns a SecKeychainItemRef, given a keychain and a recordIdentifier
	@param keychain The keychain in which the item is located
	@param itemRef The item for which the localID is to be returned
	@param recordIdentifier The returned localID
    @result A result code. See "Security Error Codes" (SecBase.h).
*/

OSStatus SecKeychainItemCopyFromRecordIdentifier(SecKeychainRef keychain,
												 SecKeychainItemRef *itemRef,
												 CFDataRef recordIdentifier);

/*!
	@function SecKeychainItemCopyAttributesAndEncryptedData
	@abstract Copies the data and/or attributes stored in the given keychain item. You must call SecKeychainItemFreeAttributesAndData()
			  when you no longer need the attributes and data. If you want to modify the attributes returned here, use SecKeychainModifyAttributesAndData().
			  The data is not decrypted.
	@param itemRef A reference to the keychain item to copy.
	@param info List of tags of attributes to retrieve.
	@param itemClass The item's class. You should pass NULL if not required.
	@param attrList on output, an attribute list with the attributes specified by info. You must call SecKeychainItemFreeAttributesAndData() when you no longer need this list.
	@param length on output the actual length of the data.
	@param outData Pointer to a buffer containing the data in this item. Pass NULL if not required. You must call SecKeychainItemFreeAttributesAndData() when you no longer need the data.
    @result A result code.  See "Security Error Codes" (SecBase.h). In addition, paramErr (-50) may be returned if not enough valid parameters are supplied.
*/
OSStatus SecKeychainItemCopyAttributesAndEncryptedData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info,
													   SecItemClass *itemClass, SecKeychainAttributeList **attrList,
													   UInt32 *length, void **outData);

/*!
	@function SecKeychainItemModifyEncryptedData
	@abstract Updates an existing keychain item after changing its data.
			  The data is not re-encrypted.
	@param itemRef A reference to the keychain item to modify.
	@param length The length of the buffer pointed to by data.
	@param data Pointer to a buffer containing the data to store.
    @result A result code.  See "Security Error Codes" (SecBase.h).
	@discussion The keychain item is written to the keychain's permanent data store. If the keychain item has not previously been added to a keychain, a call to the SecKeychainItemModifyContent function does nothing and returns noErr.
*/
OSStatus SecKeychainItemModifyEncryptedData(SecKeychainItemRef itemRef, UInt32 length, const void *data);

/*!
	@function SecKeychainItemCreateFromEncryptedContent
	@abstract Creates a new keychain item from the supplied parameters. The data is not re-encrypted.
	@param itemClass A constant identifying the class of item to create.
	@param length The length of the buffer pointed to by data.
	@param data A pointer to a buffer containing the data to store.
    @param keychainRef A reference to the keychain in which to add the item.
	@param initialAccess A reference to the access for this keychain item.
	@param itemRef On return, a pointer to a reference to the newly created keychain item (optional). When the item reference is no longer required, call CFRelease to deallocate memory occupied by the item.
	@param itemLocalID On return, the item's local ID data (optional). When the local ID data reference is no longer required, call CFRelease to deallocate memory occupied by the reference.
    @result A result code.  See "Security Error Codes" (SecBase.h). In addition, paramErr (-50) may be returned if not enough valid parameters are supplied, or memFullErr (-108) if there is not enough memory in the current heap zone to create the object.
*/
OSStatus SecKeychainItemCreateFromEncryptedContent(SecItemClass itemClass, UInt32 length, const void *data,
												   SecKeychainRef keychainRef, SecAccessRef initialAccess,
												   SecKeychainItemRef *itemRef, CFDataRef *itemLocalID);
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECKEYCHAINITEMPRIV_H_ */
