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


/*
	File:		Keychains.h

	Contains:	The keychain class

	Copyright:	2000 by Apple Computer, Inc., all rights reserved.

	To Do:
*/

#ifndef _H_KEYCHAINS_
#define _H_KEYCHAINS_

#include <Security/dlclient.h>
#include <Security/refcount.h>
#include <Security/utilities.h>
#include <Security/DLDBListCFPref.h>
#include <Security/Refs.h>
#include <Security/SecKeychainAPI.h>
#include <Security/SecKeychainAPIPriv.h>
#include <memory>

namespace Security
{

namespace KeychainCore
{

class KCCursor;
class Item;
class ItemImpl;
class Keychain;
class PrimaryKey;
class StorageManager;


class KeychainSchemaImpl : public ReferencedObject
{
public:
    KeychainSchemaImpl(const CssmClient::Db &db);
    ~KeychainSchemaImpl();

	CSSM_DB_ATTRIBUTE_FORMAT attributeFormatFor(CSSM_DB_RECORDTYPE recordType, uint32 attributeId) const;
	const CssmAutoDbRecordAttributeInfo &primaryKeyInfosFor(CSSM_DB_RECORDTYPE recordType);
	
	bool operator <(const KeychainSchemaImpl &other) const;
	bool operator ==(const KeychainSchemaImpl &other) const;

	void getAttributeInfoForRecordType(CSSM_DB_RECORDTYPE recordType, SecKeychainAttributeInfo **Info);
	CssmDbAttributeInfo attributeInfoForTag(UInt32 tag);

private:
	typedef map<CSSM_DB_RECORDTYPE, CssmAutoDbRecordAttributeInfo *> PrimaryKeyInfoMap;
	PrimaryKeyInfoMap mPrimaryKeyInfoMap;

	typedef map<uint32, CSSM_DB_ATTRIBUTE_FORMAT> RelationInfoMap;
	typedef map<CSSM_DB_RECORDTYPE, RelationInfoMap> DatabaseInfoMap;
	DatabaseInfoMap mDatabaseInfoMap;

	
};


class KeychainSchema : public RefPointer<KeychainSchemaImpl>
{
public:
    KeychainSchema() {}
    KeychainSchema(KeychainSchemaImpl *impl) : RefPointer<KeychainSchemaImpl>(impl) {}
    KeychainSchema(const CssmClient::Db &db) : RefPointer<KeychainSchemaImpl>(new KeychainSchemaImpl(db)) {}

	bool operator <(const KeychainSchema &other) const
	{ return ptr && other.ptr ? *ptr < *other.ptr : ptr < other.ptr; }
	bool operator ==(const KeychainSchema &other) const
	{ return ptr && other.ptr ? *ptr == *other.ptr : ptr == other.ptr; }

private:
	typedef KeychainSchemaImpl Impl;
};


class KeychainImpl : public ReferencedObject
{
    NOCOPY(KeychainImpl)
	friend class Keychain;
protected:
    KeychainImpl(const CssmClient::Db &db);

protected:
	// Methods called by ItemImpl;
	friend class ItemImpl;

	void didUpdate(ItemImpl *inItemImpl, PrimaryKey &oldPK,
						PrimaryKey &newPK);

public:
    virtual ~KeychainImpl();

    // Item calls
    void add(Item &item); // item must not be persistant.  Item will change.
    void deleteItem(Item &item); // item must be persistant.

    // Keychain calls
	void create(UInt32 passwordLength, const void *inPassword);
    void create(ConstStringPtr inPassword);
    void create();
    void create(const ResourceControlContext *rcc);
    void open(); // There is no close since the client lib deals with that itself. might throw

	// Locking and unlocking a keychain.
    void lock();
    void unlock();
	void unlock(const CssmData &password);
    void unlock(ConstStringPtr password); // @@@ This has a length limit, we should remove it.

	void getSettings(uint32 &outIdleTimeOut, bool &outLockOnSleep);
	void setSettings(uint32 inIdleTimeOut, bool inLockOnSleep);

	// Passing in NULL for either oldPassword or newPassword will cause them to be prompted for.
	// To specify a zero length password in either case the oldPasswordLength or newPasswordLength
	// value must be 0 and the oldPassword or newPassword must not be NULL.
	void changePassphrase(UInt32 oldPasswordLength, const void *oldPassword,
		UInt32 newPasswordLength, const void *newPassword);
	void changePassphrase(ConstStringPtr oldPassword, ConstStringPtr newPassword);

    void authenticate(const CSSM_ACCESS_CREDENTIALS *cred);     // Does not do an unlock.

	const char *name() const { return mDb->name(); }
	UInt32 status() const;
	bool exists();
	bool isActive() const;

	KCCursor createCursor(const SecKeychainAttributeList *attrList);
	KCCursor createCursor(SecItemClass itemClass, const SecKeychainAttributeList *attrList);
	CssmClient::Db database() { return mDb; }
	DLDbIdentifier dLDbIdentifier() const { return mDb->dlDbIdentifier(); }

	PrimaryKey makePrimaryKey(CSSM_DB_RECORDTYPE recordType, CssmClient::DbUniqueRecord &uniqueId);
	void gatherPrimaryKeyAttributes(CssmClient::DbAttributes& primaryKeyAttrs);
	
	const CssmAutoDbRecordAttributeInfo &primaryKeyInfosFor(CSSM_DB_RECORDTYPE recordType);

    Item item(const PrimaryKey& primaryKey);
    Item item(CSSM_DB_RECORDTYPE recordType, CssmClient::DbUniqueRecord &uniqueId);
	
	CssmDbAttributeInfo attributeInfoForTag(UInt32 tag);
	void getAttributeInfoForItemID(CSSM_DB_RECORDTYPE itemID, SecKeychainAttributeInfo **Info);
static	void freeAttributeInfo(SecKeychainAttributeInfo *Info);

private:
	KeychainSchema keychainSchema();
	void addItem(const PrimaryKey &primaryKey, ItemImpl *dbItemImpl);
	void removeItem(const PrimaryKey &primaryKey, const ItemImpl *inItemImpl); 

    CssmClient::Db mDb;
	Mutex mDbItemMapLock;
    typedef map<PrimaryKey, ItemImpl *> DbItemMap;
    DbItemMap mDbItemMap;

	KeychainSchema mKeychainSchema;
};


class Keychain : public RefPointer<KeychainImpl>
{
public:
    Keychain() {}
    Keychain(KeychainImpl *impl) : RefPointer<KeychainImpl>(impl) {}

	static Keychain optional(SecKeychainRef handle); 

private:
	friend class StorageManager;
    Keychain(const CssmClient::Db &db)
	: RefPointer<KeychainImpl>(new KeychainImpl(db)) {}

	typedef KeychainImpl Impl;
};


typedef Ref<Keychain, KeychainImpl, SecKeychainRef, errSecInvalidKeychain> KeychainRef;

} // end namespace KeychainCore

} // end namespace Security

#endif /* _H_KEYCHAINS_ */

