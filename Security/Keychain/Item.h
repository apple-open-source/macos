/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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


//
// Item.h
//
#ifndef _SECURITY_ITEM_H_
#define _SECURITY_ITEM_H_

#include <Security/Keychains.h>
#include <Security/PrimaryKey.h>
#include <Security/securestorage.h>
#include <Security/Access.h>

namespace Security
{

using namespace CssmClient;

namespace KeychainCore
{
class Keychain;

class ItemImpl : public SecCFObject
{
public:
	SECCFFUNCTIONS(ItemImpl, SecKeychainItemRef, errSecInvalidItemRef)

    friend class Item;
	friend class KeychainImpl;
protected:

	// new item constructors
    ItemImpl(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data);
	
	ItemImpl(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void* data);

	// db item contstructor
    ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);

	// PrimaryKey item contstructor
    ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey);
	
	ItemImpl(ItemImpl &item);

	void getAttributeFrom(CssmDbAttributeData *data, SecKeychainAttribute &attr,  UInt32 *actualLength);
	void getClass(SecKeychainAttribute &attr,  UInt32 *actualLength);

protected:
	// Methods called by KeychainImpl;

	// Add the receiver to keychain
	virtual PrimaryKey add(Keychain &keychain);
		
	// Get the default value for an attribute
	static const CSSM_DATA &defaultAttributeValue(const CSSM_DB_ATTRIBUTE_INFO &info);

public:
    virtual ~ItemImpl() throw();
    bool isPersistant() const;
    bool isModified() const;

	virtual void update();

	// put a copy of the item into a given keychain
	virtual Item copyTo(const Keychain &keychain, Access *newAccess = NULL);

    CSSM_DB_RECORDTYPE recordType() const;

	// Used for writing the record to the database.
    CssmClient::DbUniqueRecord dbUniqueRecord();
	const CssmClient::DbAttributes *modifiedAttributes() const;
	const CssmData *modifiedData() const;
	virtual void didModify(); // Forget any attributes and data we just wrote to the db

	Keychain keychain() const;
	PrimaryKey primaryKey() const;
	bool operator <(const ItemImpl &other) const;

	void getAttribute(SecKeychainAttribute& attr,  UInt32 *actualLength);
	void getData(CssmDataContainer& outData);
	
	void modifyContent(const SecKeychainAttributeList *attrList, UInt32 dataLength, const void *inData);
	void getContent(SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData);
	static void freeContent(SecKeychainAttributeList *attrList, void *data);
	static void freeAttributesAndData(SecKeychainAttributeList *attrList, void *data);

	void getAttributesAndData(SecKeychainAttributeInfo *info, SecItemClass *itemClass, SecKeychainAttributeList **attrList, UInt32 *length, void **outData);
	void modifyAttributesAndData(const SecKeychainAttributeList *attrList, UInt32 dataLength, const void *inData);

	void setAttribute(SecKeychainAttribute& attr);
	void setAttribute(const CssmDbAttributeInfo &info, const CssmPolyData &data);
	void setData(UInt32 length,const void *data);
	void setAccess(Access *newAccess);

	SSGroup group();

protected:
    void getContent(DbAttributes *dbAttributes, CssmDataContainer *itemData);
    void getLocalContent(SecKeychainAttributeList &attributeList);

    bool useSecureStorage(const CssmClient::Db &db);

	// new item members
    auto_ptr<CssmDataContainer> mData;
    auto_ptr<CssmClient::DbAttributes> mDbAttributes;
	SecPointer<Access> mAccess;

	// db item members
    CssmClient::DbUniqueRecord mUniqueId;
	Keychain mKeychain;
    PrimaryKey mPrimaryKey;
};


class Item : public SecPointer<ItemImpl>
{
public:
    Item();
    Item(ItemImpl *impl);
    Item(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data);
	Item(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void* data);
    Item(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);
    Item(const Keychain &keychain, const PrimaryKey &primaryKey);
	Item(ItemImpl &item);
};


} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_ITEM_H_