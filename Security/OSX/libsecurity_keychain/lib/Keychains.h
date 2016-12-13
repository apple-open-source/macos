/*
 * Copyright (c) 2000-2004,2011-2014 Apple Inc. All Rights Reserved.
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

//
// Keychains.h - The Keychain class
//
#ifndef _SECURITY_KEYCHAINS_H_
#define _SECURITY_KEYCHAINS_H_

#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/dlclient.h>
#include <security_utilities/refcount.h>
#include <security_utilities/seccfobject.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <memory>
#include "SecCFTypes.h"
#include "defaultcreds.h"

class EventBuffer;

namespace Security
{

namespace KeychainCore
{

class KCCursor;
class Item;
class PrimaryKey;
class StorageManager;

class KeychainSchemaImpl : public RefCount
{
	NOCOPY(KeychainSchemaImpl)
public:
	friend class KeychainSchema;
protected:
    KeychainSchemaImpl(const CssmClient::Db &db);
public:
    virtual ~KeychainSchemaImpl();

	CSSM_DB_ATTRIBUTE_FORMAT attributeFormatFor(CSSM_DB_RECORDTYPE recordType, uint32 attributeId) const;
	const CssmAutoDbRecordAttributeInfo &primaryKeyInfosFor(CSSM_DB_RECORDTYPE recordType) const;
	
	bool operator <(const KeychainSchemaImpl &other) const;
	bool operator ==(const KeychainSchemaImpl &other) const;

	void getAttributeInfoForRecordType(CSSM_DB_RECORDTYPE recordType, SecKeychainAttributeInfo **Info) const;
	CssmDbAttributeInfo attributeInfoFor(CSSM_DB_RECORDTYPE recordType, uint32 attributeId) const;
	bool hasAttribute(CSSM_DB_RECORDTYPE recordType, uint32 attributeId) const;
	bool hasRecordType(CSSM_DB_RECORDTYPE recordType) const;

	void didCreateRelation(CSSM_DB_RECORDTYPE inRelationID,
		const char *inRelationName,
		uint32 inNumberOfAttributes,
		const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
		uint32 inNumberOfIndexes,
		const CSSM_DB_SCHEMA_INDEX_INFO *pIndexInfo);

private:
	typedef map<CSSM_DB_RECORDTYPE, CssmAutoDbRecordAttributeInfo *> PrimaryKeyInfoMap;
	PrimaryKeyInfoMap mPrimaryKeyInfoMap;

	typedef map<uint32, CSSM_DB_ATTRIBUTE_FORMAT> RelationInfoMap;
	typedef map<CSSM_DB_RECORDTYPE, RelationInfoMap> DatabaseInfoMap;
	DatabaseInfoMap mDatabaseInfoMap;
	Mutex mMutex;

private:
	const RelationInfoMap &relationInfoMapFor(CSSM_DB_RECORDTYPE recordType) const;
};


class KeychainSchema : public RefPointer<KeychainSchemaImpl>
{
public:
    KeychainSchema() {}
    KeychainSchema(KeychainSchemaImpl *impl) : RefPointer<KeychainSchemaImpl>(impl) {}
    KeychainSchema(const CssmClient::Db &db) : RefPointer<KeychainSchemaImpl>(new KeychainSchemaImpl(db)) {}
    ~KeychainSchema();
    
	bool operator <(const KeychainSchema &other) const
	{ return ptr && other.ptr ? *ptr < *other.ptr : ptr < other.ptr; }
	bool operator ==(const KeychainSchema &other) const
	{ return ptr && other.ptr ? *ptr == *other.ptr : ptr == other.ptr; }

private:
	typedef KeychainSchemaImpl Impl;
};


class ItemImpl;

class KeychainImpl : public SecCFObject, private CssmClient::Db::DefaultCredentialsMaker
{
    NOCOPY(KeychainImpl)
public:
	SECCFFUNCTIONS(KeychainImpl, SecKeychainRef, errSecInvalidKeychain, gTypes().KeychainImpl)

	friend class Keychain;
	friend class ItemImpl;
	friend class KeyItem;
    friend class KCCursorImpl;
    friend class StorageManager;
protected:
    KeychainImpl(const CssmClient::Db &db);

protected:
	// Methods called by ItemImpl;
	void didUpdate(const Item &inItem, PrimaryKey &oldPK,
		PrimaryKey &newPK);
	void completeAdd(Item &item, PrimaryKey &key);

public:
    virtual ~KeychainImpl();

	Mutex* getKeychainMutex();
	Mutex* getMutexForObject() const;

    ReadWriteLock* getKeychainReadWriteLock();
	void aboutToDestruct();

	bool operator ==(const KeychainImpl &) const;

    // Item calls
	void add(Item &item);
	void addCopy(Item &item);
    void deleteItem(Item &item); // item must be persistent.

    // Keychain calls
	void create(UInt32 passwordLength, const void *inPassword);
	void createWithBlob(CssmData &blob);
    void create(ConstStringPtr inPassword);
    void create();
    void create(const ResourceControlContext *rcc);
    void open();

	// Locking and unlocking a keychain.
    void lock();
    void unlock();
	void unlock(const CssmData &password);
    void unlock(ConstStringPtr password); // @@@ This has a length limit, we should remove it.
    void stash();
    void stashCheck();

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
	CssmClient::Db database() { StLock<Mutex>_(mDbMutex); return mDb; }
    void changeDatabase(CssmClient::Db db);
	DLDbIdentifier dlDbIdentifier() const { return mDb->dlDbIdentifier(); }

	CssmClient::CSP csp();

	PrimaryKey makePrimaryKey(CSSM_DB_RECORDTYPE recordType, CssmClient::DbUniqueRecord &uniqueId);

    // This will make a primary key for this record type, and populate it completely from the given attributes
	PrimaryKey makePrimaryKey(CSSM_DB_RECORDTYPE recordType, CssmClient::DbAttributes *currentAttributes);
	void gatherPrimaryKeyAttributes(CssmClient::DbAttributes& primaryKeyAttrs);
	
	const CssmAutoDbRecordAttributeInfo &primaryKeyInfosFor(CSSM_DB_RECORDTYPE recordType);

    Item item(const PrimaryKey& primaryKey);
    Item item(CSSM_DB_RECORDTYPE recordType, CssmClient::DbUniqueRecord &uniqueId);

    // Check for an item that may have been deleted.
    Item itemdeleted(const PrimaryKey& primaryKey);

	CssmDbAttributeInfo attributeInfoFor(CSSM_DB_RECORDTYPE recordType, UInt32 tag);
	void getAttributeInfoForItemID(CSSM_DB_RECORDTYPE itemID, SecKeychainAttributeInfo **Info);
	static void freeAttributeInfo(SecKeychainAttributeInfo *Info);
	KeychainSchema keychainSchema();
	void resetSchema();
	void didDeleteItem(ItemImpl *inItemImpl);
	
	void recode(const CssmData &data, const CssmData &extraData);
	void copyBlob(CssmData &dbBlob);
	
	void setBatchMode(Boolean mode, Boolean rollBack);
	
	// yield default open() credentials for this keychain (as of now)
	const AccessCredentials *defaultCredentials();

	// Only call these functions while holding globals().apiLock.
	bool inCache() const throw() { return mInCache; }
	void inCache(bool inCache) throw() { mInCache = inCache; }
	
	void postEvent(SecKeychainEvent kcEvent, ItemImpl* item);
    void postEvent(SecKeychainEvent kcEvent, ItemImpl* item, PrimaryKey pk);
	
	void addItem(const PrimaryKey &primaryKey, ItemImpl *dbItemImpl);

    bool mayDelete();

    // Returns true if this keychain supports the attribute integrity and
    // partition ID protections
    bool hasIntegrityProtection();

private:
    // Checks for and triggers a keychain database upgrade
    // DO NOT hold any of the keychain locks when you call this
    bool performKeychainUpgradeIfNeeded();

    // Notify the keychain that you're accessing it. Used in conjunction with
    // the StorageManager for time-based caching.
    void tickle();

    // Used by StorageManager to remember the timer->keychain pairing
    dispatch_source_t mCacheTimer;

    // Set this to true to make tickling do nothing.
    bool mSuppressTickle;

public:
    // Grab the locks and then call attemptKeychainMigration
    // The access credentials are only used when downgrading version, and will be passed along with ACL edits
    bool keychainMigration(const string oldPath, const uint32 dbBlobVersion, const string newPath, const uint32 newBlobVersion, const AccessCredentials *cred = NULL);

private:
    // Attempt to upgrade this keychain's database
    uint32 attemptKeychainMigration(const string oldPath, const uint32 oldBlobVersion, const string newPath, const uint32 newBlobVersion, const AccessCredentials *cred);

    // Attempt to rename this keychain, if someone hasn't beaten us to it
    void attemptKeychainRename(const string oldPath, const string newPath, uint32 blobVersion);

    // Remember if we've attempted to upgrade this keychain's database
    bool mAttemptedUpgrade;

	void removeItem(const PrimaryKey &primaryKey, ItemImpl *inItemImpl);

    // Use this when you want to be extra sure this item is removed from the
    // cache. Iterates over the whole cache to find all instances. This function
    // will take both cache map mutexes, so you must not hold only the
    // mDbDeletedItemMapMutex when you call this function.
    void forceRemoveFromCache(ItemImpl* inItemImpl);

    // Looks up an item in the item cache.
    //
    // To use this in a thread-safe manner, you must hold this keychain's mutex
    // from before you begin this operation until you have safely completed a
    // CFRetain on the resulting ItemImpl.
	ItemImpl *_lookupItem(const PrimaryKey &primaryKey);

    // Looks up a deleted item in the deleted item map. Does not check the normal map.
    //
    // To use this in a thread-safe manner, you must hold this keychain's mutex
    // from before you begin this operation until you have safely completed a
    // CFRetain on the resulting ItemImpl.
    ItemImpl *_lookupDeletedItemOnly(const PrimaryKey &primaryKey);

	const AccessCredentials *makeCredentials();

    typedef map<PrimaryKey, ItemImpl *> DbItemMap;
	// Reference map of all items we know about that have a primaryKey
    DbItemMap mDbItemMap;
    Mutex mDbItemMapMutex;

    // Reference map of all items we know about that have been deleted
    // but we haven't yet received a deleted notification about.
    // We need this for when we delete an item (and so don't want it anymore)
    // but stil need the item around to pass along to the client process with the
    // deletion notification (if they've registered for such things).
    DbItemMap mDbDeletedItemMap;
    Mutex mDbDeletedItemMapMutex;

    // Note on ItemMapMutexes: STL maps are not thread-safe, so you must hold the
    // mutex for the entire duration of your access/modification to the map.
    // Otherwise, other processes might interrupt your iterator by adding/removing
    // items. If you must hold both mutexes, you must take mDbItemMapMutex before
    // mDbDeletedItemMapMutex.

	// True iff we are in the cache of keychains in StorageManager
	bool mInCache;

    CssmClient::Db mDb;

	KeychainSchema mKeychainSchema;
	
	// Data for auto-unlock credentials
	DefaultCredentials mCustomUnlockCreds;
	bool mIsInBatchMode;
	EventBuffer *mEventBuffer;
    mutable Mutex mMutex;

    // Now that we sometimes change the database object, Db object
    // creation/returning needs a mutex. You should only hold this if you're
    // copying or changing the mDb object.
    Mutex mDbMutex;

    // Used to protect mDb across calls.
    // Grab a read lock if you expect to read from this keychain in the future.
    // The write lock is taken if we're replacing the database wholesale with something new.
    ReadWriteLock mRWLock;
};


CFIndex GetKeychainRetainCount(Keychain& kc);

class Keychain : public SecPointer<KeychainImpl>
{
public:
    Keychain();
    Keychain(KeychainImpl *impl) : SecPointer<KeychainImpl>(impl) {}
    ~Keychain();

	static Keychain optional(SecKeychainRef handle); 

private:
	friend class StorageManager;
    friend class KeychainImpl;
    friend class TrustKeychains;
    Keychain(const CssmClient::Db &db)
	: SecPointer<KeychainImpl>(new KeychainImpl(db)) {}

	typedef KeychainImpl Impl;
};


} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_KEYCHAINS_H_
