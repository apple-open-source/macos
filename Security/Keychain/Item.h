/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _H_DBITEM
#define _H_DBITEM

#include <Security/Keychains.h>
#include <Security/PrimaryKey.h>
#include <Security/securestorage.h>

namespace Security
{

using namespace CssmClient;

namespace KeychainCore
{
class Item;
class Keychain;

class ItemImpl : public ReferencedObject
{
    friend class Item;

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
	friend class KeychainImpl;

	// Add the receiver to keychain
	PrimaryKey add(const Keychain &keychain);
		
	// Get the default value for an attribute
	static const CSSM_DATA &defaultAttributeValue(const CSSM_DB_ATTRIBUTE_INFO &info);

public:
    ~ItemImpl();
    bool isPersistant() const;
    bool isModified() const;

	void update();

	// put a copy of the item into a given keychain
	Item copyTo(const Keychain &keychain);

    CSSM_DB_RECORDTYPE recordType() const;

	// Used for writing the record to the database.
    CssmClient::DbUniqueRecord dbUniqueRecord();
	const CssmClient::DbAttributes *modifiedAttributes() const;
	const CssmData *modifiedData() const;
	void didModify(); // Forget any attributes and data we just wrote to the db

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
	
	
	
	SSGroup group();


protected:
    void getContent(DbAttributes *dbAttributes, CssmDataContainer *itemData);

	// new item members
    auto_ptr<CssmDataContainer> mData;
    auto_ptr<CssmClient::DbAttributes> mDbAttributes;

	// db item members
    CssmClient::DbUniqueRecord mUniqueId;
	Keychain mKeychain;
    PrimaryKey mPrimaryKey;
	
};

class Item : public RefPointer<ItemImpl>
{
public:
    Item() {}
    Item(ItemImpl *impl) : RefPointer<ItemImpl>(impl) {}

    Item(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data)
	: RefPointer<ItemImpl>(new ItemImpl(itemClass, itemCreator, length, data)) {}
	
    Item(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void* data)
	: RefPointer<ItemImpl>(new ItemImpl(itemClass, attrList, length, data)) {}

    Item(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId)
    : RefPointer<ItemImpl>(new ItemImpl(keychain, primaryKey, uniqueId)) {}

    Item(const Keychain &keychain, const PrimaryKey &primaryKey)
    : RefPointer<ItemImpl>(new ItemImpl(keychain, primaryKey)) {}
	
	Item(ItemImpl &item)
	: RefPointer<ItemImpl>(new ItemImpl(item)) {}

    bool operator <(const Item &other) const { return **this < *other; }
    bool operator !=(const Item &other) const { return **this < *other || *other < **this; }
    bool operator ==(const Item &other) const { return !(*this != other); }

	typedef ItemImpl Impl;
};


typedef Ref<Item, ItemImpl, SecKeychainItemRef, errSecInvalidItemRef> ItemRef;


}; // end namespace KeychainCore

} // end namespace Security

#endif // _H_DBITEM