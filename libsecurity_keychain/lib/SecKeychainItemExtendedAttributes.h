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

/*
 *	SecKeychainItemExtendedAttributes.h
 *	Created 9/6/06 by dmitch
 */
 
#ifndef _SEC_KEYCHAIN_ITEM_EXTENDED_ATTRIBUTES_H_
#define _SEC_KEYCHAIN_ITEM_EXTENDED_ATTRIBUTES_H_

#include <Security/SecBase.h>
#include <Security/cssmapple.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* 
 * Extended attributes extend the fixed set of keychain item attribute in a generally
 * extensible way. A given SecKeychainItemRef can have assigned to it any number
 * of extended attributes, each consisting of an attribute name (as a CFStringRef)
 * and an attribute value (as a CFDataRef). 
 *
 * Each extended attribute is a distinct record residing in the same keychain as 
 * the item to which it refers. In a given keychain, the set of the following properties 
 * of an extended attribute record must be unique:
 *
 *   -- the type of item to which the extended attribute is bound (kSecPublicKeyItemClass,
 *      kSecPrivateKeyItemClass, etc.)
 *   -- an identifier which uniquely identifies the item to which the extended attribute
 *      is bound. Currently this is the PrimaryKey blob. 
 *   -- the extended attribute's Attribute Name, specified in this interface as a
 *      CFString. 
 *
 * Thus, e.g., a given item can have at most one extended attribute with 
 * Attribute Name of CFSTR("SomeAttributeName").
 */
 
/* 
 * SecKeychainItemSetExtendedAttribute() - set an extended attribute by name and value.
 *
 * If the extended attribute specified by 'attrName' does not exist, one will be 
 * created with the value specified in 'attrValue'.
 *
 * If the extended attribute specified by 'attrName already exists, its value will be
 * replaced by the value specified in 'attrValue'.
 * 
 * If the incoming 'attrValue' is NULL, the extended attribute specified by 'attrName' 
 * will be deleted if it exists. If the incoming 'attrValue' is NULL  and no such 
 * attribute exists, the function will return errSecNoSuchAttr. 
 */
OSStatus SecKeychainItemSetExtendedAttribute(
	SecKeychainItemRef			itemRef,
	CFStringRef					attrName,		/* identifies the attribute */ 
	CFDataRef					attrValue);		/* value to set; NULL means delete the 
												 *    attribute */
	
/* 
 * SecKeychainItemCopyExtendedAttribute() -  Obtain the value of an an extended attribute. 
 * 
 * If the extended attribute specified by 'attrName' exists, its value will be returned
 * via the *attrValue argument. The caller must CFRelease() this returned value.
 *
 * If the extended attribute specified by 'attrName' does not exist, the function
 * will return errSecNoSuchAttr.
 */
OSStatus SecKeychainItemCopyExtendedAttribute(
	SecKeychainItemRef			itemRef,
	CFStringRef					attrName,
	CFDataRef					*attrValue);		/* RETURNED */
	
/*
 * SecKeychainItemCopyAllExtendedAttributes() - obtain all of an item's extended attributes. 
 *
 * This is used to determine all of the extended attributes associated with a given
 * SecKeychainItemRef. The Atrribute Names of all of the extended attributes are
 * returned in the *attrNames argument; on successful return this contains a
 * CFArray whose elements are CFStringRefs, each of which is an Attribute Name.
 * The caller must CFRelease() this array. 
 *
 * Optionally, the Attribute Values of all of the extended attributes is returned 
 * in the *attrValues argument; on successful return this contains a CFArray whose 
 * elements are CFDataRefs, each of which is an Attribute Value. The positions of 
 * the elements in this array correspond with the elements in *attrNames; i.e., 
 * the n'th element in *attrName is the Attribute Name corresponding to the 
 * Attribute Value found in the n'th element of *attrValues. 
 *
 * Pass in NULL for attrValues if you don't need the Attribute Values. Caller
 * must CFRelease the array returned via this argument. 
 *
 * If the item has no extended attributes, this function returns errSecNoSuchAttr.
 */
OSStatus SecKeychainItemCopyAllExtendedAttributes(
	SecKeychainItemRef			itemRef,
	CFArrayRef					*attrNames,			/* RETURNED, each element is a CFStringRef */
	CFArrayRef					*attrValues);		/* optional, RETURNED, each element is a 
													 *   CFDataRef */
#if defined(__cplusplus)
}
#endif

#endif	/* _SEC_KEYCHAIN_ITEM_EXTENDED_ATTRIBUTES_H_ */

