/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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
 * ExtendedAttribute.h - Extended Keychain Item Attribute class.
 *
 * Created 9/6/06 by dmitch.
 */

#ifndef _SECURITY_EXTENDED_ATTRIBUTE_H_
#define _SECURITY_EXTENDED_ATTRIBUTE_H_

#include <security_keychain/Item.h>
#include <security_cdsa_utilities/cssmdata.h>

/* this is not public */
typedef struct OpaqueSecExtendedAttributeRef *SecKeychainItemExtendedAttributeRef;

namespace Security
{

namespace KeychainCore
{

class ExtendedAttribute : public ItemImpl
{
	NOCOPY(ExtendedAttribute)
public:
	SECCFFUNCTIONS(ExtendedAttribute, SecKeychainItemExtendedAttributeRef, 
		errSecInvalidItemRef, gTypes().ExtendedAttribute)

	/* construct new ExtendedAttr from API */
	ExtendedAttribute(CSSM_DB_RECORDTYPE recordType, 
		const CssmData &itemID, 
		const CssmData attrName,
		const CssmData attrValue);

private:
	// db item contstructor
    ExtendedAttribute(const Keychain &keychain, 
		const PrimaryKey &primaryKey, 
		const CssmClient::DbUniqueRecord &uniqueId);

	// PrimaryKey item contstructor
    ExtendedAttribute(const Keychain &keychain, const PrimaryKey &primaryKey);

public:
	static ExtendedAttribute* make(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);
	static ExtendedAttribute* make(const Keychain &keychain, const PrimaryKey &primaryKey);
	
	ExtendedAttribute(ExtendedAttribute &extendedAttribute);

    virtual ~ExtendedAttribute() throw();

	virtual PrimaryKey add(Keychain &keychain);
	bool operator == (const ExtendedAttribute &other) const;
private:
	/* set up DB attrs based on member vars */
	void setupAttrs();
	
	CSSM_DB_RECORDTYPE		mRecordType;
	CssmAutoData			mItemID;
	CssmAutoData			mAttrName;
	CssmAutoData			mAttrValue;
};

} // end namespace KeychainCore

} // end namespace Security

#endif /* _SECURITY_EXTENDED_ATTRIBUTES_H_ */
