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
// Keychains.cpp
//

#include "Keychains.h"
#include "KCEventNotifier.h"

#include "Item.h"
#include "KCCursor.h"
#include "Globals.h"
#include "Schema.h"
#include <Security/keychainacl.h>
#include <Security/cssmacl.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/cssmdb.h>
#include <Security/trackingallocator.h>
#include <Security/SecCFTypes.h>

using namespace KeychainCore;
using namespace CssmClient;


//
// KeychainSchemaImpl
//
KeychainSchemaImpl::KeychainSchemaImpl(const Db &db)
{
	DbCursor relations(db);
	relations->recordType(CSSM_DL_DB_SCHEMA_INFO);
	DbAttributes relationRecord(db, 1);
	relationRecord.add(Schema::RelationID);
	DbUniqueRecord outerUniqueId(db);

	while (relations->next(&relationRecord, NULL, outerUniqueId))
	{
		DbUniqueRecord uniqueId(db);

		uint32 relationID = relationRecord.at(0);
		if (CSSM_DB_RECORDTYPE_SCHEMA_START <= relationID && relationID < CSSM_DB_RECORDTYPE_SCHEMA_END)
			continue;

		// Create a cursor on the SCHEMA_ATTRIBUTES table for records with RelationID == relationID
		DbCursor attributes(db);
		attributes->recordType(CSSM_DL_DB_SCHEMA_ATTRIBUTES);
		attributes->add(CSSM_DB_EQUAL, Schema::RelationID, relationID);
	
		// Set up a record for retriving the SCHEMA_ATTRIBUTES
		DbAttributes attributeRecord(db, 2);
		attributeRecord.add(Schema::AttributeFormat);
		attributeRecord.add(Schema::AttributeID);
		attributeRecord.add(Schema::AttributeNameFormat);
	
	
		RelationInfoMap &rim = mDatabaseInfoMap[relationID];
		while (attributes->next(&attributeRecord, NULL, uniqueId))
		{
			// @@@ this if statement was blocking tags of different naming conventions
			//if(CSSM_DB_ATTRIBUTE_FORMAT(attributeRecord.at(2))==CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER)
				rim[attributeRecord.at(1)] = attributeRecord.at(0);
		}
		
		// Create a cursor on the CSSM_DL_DB_SCHEMA_INDEXES table for records with RelationID == relationID
		DbCursor indexes(db);
		indexes->recordType(CSSM_DL_DB_SCHEMA_INDEXES);
		indexes->conjunctive(CSSM_DB_AND);
		indexes->add(CSSM_DB_EQUAL, Schema::RelationID, relationID);
		indexes->add(CSSM_DB_EQUAL, Schema::IndexType, uint32(CSSM_DB_INDEX_UNIQUE));

		// Set up a record for retriving the SCHEMA_INDEXES
		DbAttributes indexRecord(db, 1);
		indexRecord.add(Schema::AttributeID);

		CssmAutoDbRecordAttributeInfo &infos = *new CssmAutoDbRecordAttributeInfo();
		mPrimaryKeyInfoMap.insert(PrimaryKeyInfoMap::value_type(relationID, &infos));
		infos.DataRecordType = relationID;
		while (indexes->next(&indexRecord, NULL, uniqueId))
		{
			CssmDbAttributeInfo &info = infos.add();
			info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
			info.Label.AttributeID = indexRecord.at(0);
			info.AttributeFormat = rim[info.Label.AttributeID]; // @@@ Might insert bogus value if DB is corrupt
		}
	}
}

KeychainSchemaImpl::~KeychainSchemaImpl()
{
	for_each_map_delete(mPrimaryKeyInfoMap.begin(), mPrimaryKeyInfoMap.end());
}

const KeychainSchemaImpl::RelationInfoMap &
KeychainSchemaImpl::relationInfoMapFor(CSSM_DB_RECORDTYPE recordType) const
{
	DatabaseInfoMap::const_iterator dit = mDatabaseInfoMap.find(recordType);
	if (dit == mDatabaseInfoMap.end())
		MacOSError::throwMe(errSecNoSuchClass);
	return dit->second;
}

bool
KeychainSchemaImpl::hasAttribute(CSSM_DB_RECORDTYPE recordType, uint32 attributeId) const
{
	const RelationInfoMap &rmap = relationInfoMapFor(recordType);
	RelationInfoMap::const_iterator rit = rmap.find(attributeId);
	return rit != rmap.end();
}

CSSM_DB_ATTRIBUTE_FORMAT 
KeychainSchemaImpl::attributeFormatFor(CSSM_DB_RECORDTYPE recordType, uint32 attributeId) const
{
	const RelationInfoMap &rmap = relationInfoMapFor(recordType);
	RelationInfoMap::const_iterator rit = rmap.find(attributeId);
	if (rit == rmap.end())
		MacOSError::throwMe(errSecNoSuchAttr);

	return rit->second;
}

CssmDbAttributeInfo
KeychainSchemaImpl::attributeInfoFor(CSSM_DB_RECORDTYPE recordType, uint32 attributeId) const
{
	CSSM_DB_ATTRIBUTE_INFO info;
	info.AttributeFormat = attributeFormatFor(recordType, attributeId);
	info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
	info.Label.AttributeID = attributeId;

	return info;
}

void
KeychainSchemaImpl::getAttributeInfoForRecordType(CSSM_DB_RECORDTYPE recordType, SecKeychainAttributeInfo **Info) const
{
	const RelationInfoMap &rmap = relationInfoMapFor(recordType);

	SecKeychainAttributeInfo *theList=reinterpret_cast<SecKeychainAttributeInfo *>(malloc(sizeof(SecKeychainAttributeInfo)));
	
	UInt32 capacity=rmap.size();
	UInt32 *tagBuf=reinterpret_cast<UInt32 *>(malloc(capacity*sizeof(UInt32)));
	UInt32 *formatBuf=reinterpret_cast<UInt32 *>(malloc(capacity*sizeof(UInt32)));
	UInt32 i=0;
	
	
	for (RelationInfoMap::const_iterator rit = rmap.begin(); rit != rmap.end(); ++rit)
	{
		if (i>=capacity)
		{
			capacity *= 2;
			if (capacity <= i) capacity = i + 1;
			tagBuf=reinterpret_cast<UInt32 *>(realloc(tagBuf, (capacity*sizeof(UInt32))));
			formatBuf=reinterpret_cast<UInt32 *>(realloc(tagBuf, (capacity*sizeof(UInt32))));
		}
		tagBuf[i]=rit->first;
		formatBuf[i++]=rit->second;
	}
	
	theList->count=i;
	theList->tag=tagBuf;
	theList->format=formatBuf;
	*Info=theList;		
}


const CssmAutoDbRecordAttributeInfo &
KeychainSchemaImpl::primaryKeyInfosFor(CSSM_DB_RECORDTYPE recordType) const
{
	PrimaryKeyInfoMap::const_iterator it;
	it = mPrimaryKeyInfoMap.find(recordType);
	
	if (it == mPrimaryKeyInfoMap.end())
		MacOSError::throwMe(errSecNoSuchClass); // @@@ Not really but whatever.

	return *it->second;
}

bool
KeychainSchemaImpl::operator <(const KeychainSchemaImpl &other) const
{
	return mDatabaseInfoMap < other.mDatabaseInfoMap;
}

bool
KeychainSchemaImpl::operator ==(const KeychainSchemaImpl &other) const
{
	return mDatabaseInfoMap == other.mDatabaseInfoMap;
}


//
// KeychainImpl
//
KeychainImpl::KeychainImpl(const Db &db)
: mDb(db)
{
}

KeychainImpl::~KeychainImpl()
{
}

bool
KeychainImpl::operator ==(const KeychainImpl &keychain) const
{
	return dLDbIdentifier() == keychain.dLDbIdentifier();
}

KCCursor
KeychainImpl::createCursor(SecItemClass itemClass, const SecKeychainAttributeList *attrList)
{
	StorageManager::KeychainList keychains;
	keychains.push_back(Keychain(this));
	return KCCursor(keychains, itemClass, attrList);
}

KCCursor
KeychainImpl::createCursor(const SecKeychainAttributeList *attrList)
{
	StorageManager::KeychainList keychains;
	keychains.push_back(Keychain(this));
	return KCCursor(keychains, attrList);
}

void
KeychainImpl::create(UInt32 passwordLength, const void *inPassword)
{
	if (!inPassword)
	{
		create();
		return;
	}

	CssmAllocator &alloc = CssmAllocator::standard();
        
	// @@@ Share this instance

	const CssmData password(const_cast<void *>(inPassword), passwordLength);
        AclFactory::PasswordChangeCredentials pCreds (password, alloc);
        const AccessCredentials* aa = pCreds;
        
	// @@@ Create a nice wrapper for building the default AclEntryPrototype. 
	TypedList subject(alloc, CSSM_ACL_SUBJECT_TYPE_ANY);
	AclEntryPrototype protoType(subject);
	AuthorizationGroup &authGroup = protoType.authorization();
	CSSM_ACL_AUTHORIZATION_TAG tag = CSSM_ACL_AUTHORIZATION_ANY;
	authGroup.NumberOfAuthTags = 1;
	authGroup.AuthTags = &tag;

	const ResourceControlContext rcc(protoType, const_cast<AccessCredentials *>(aa));
	create(&rcc);
}

void KeychainImpl::create(ConstStringPtr inPassword)
{
    if ( inPassword )
        create(static_cast<UInt32>(inPassword[0]), &inPassword[1]);
    else
        create();
}

void
KeychainImpl::create()
{
	CssmAllocator &alloc = CssmAllocator::standard();
	// @@@ Share this instance
#ifdef OBSOLETE
	KeychainAclFactory aclFactory(alloc);

	const AccessCredentials *cred = aclFactory.keychainPromptUnlockCredentials();
#endif
        AclFactory aclFactor;
        const AccessCredentials *cred = aclFactor.unlockCred ();
        
	// @@@ Create a nice wrapper for building the default AclEntryPrototype.
	TypedList subject(alloc, CSSM_ACL_SUBJECT_TYPE_ANY);
	AclEntryPrototype protoType(subject);
	AuthorizationGroup &authGroup = protoType.authorization();
	CSSM_ACL_AUTHORIZATION_TAG tag = CSSM_ACL_AUTHORIZATION_ANY;
	authGroup.NumberOfAuthTags = 1;
	authGroup.AuthTags = &tag;

	const ResourceControlContext rcc(protoType, const_cast<AccessCredentials *>(cred));
	create(&rcc);
}

void
KeychainImpl::create(const ResourceControlContext *rcc)
{
	mDb->dbInfo(&Schema::DBInfo); // Set the schema (to force a create)
	mDb->resourceControlContext(rcc);
    try
    {
        mDb->create();
    }
    catch (...)
    {
		mDb->resourceControlContext(NULL);
        mDb->dbInfo(NULL); // Clear the schema (to not break an open call later)
        throw;
    }
	mDb->resourceControlContext(NULL);
	mDb->dbInfo(NULL); // Clear the schema (to not break an open call later)
	globals().storageManager.created(Keychain(this));
}

void
KeychainImpl::open()
{
	mDb->open();
}

void
KeychainImpl::lock()
{
	mDb->lock();
}

void
KeychainImpl::unlock()
{
	mDb->unlock();
}

void
KeychainImpl::unlock(const CssmData &password)
{
	mDb->unlock(password);
}

void
KeychainImpl::unlock(ConstStringPtr password)
{
	if (password)
	{
		const CssmData data(const_cast<unsigned char *>(&password[1]), password[0]);
		unlock(data);
	}
	else
		unlock();
}

void
KeychainImpl::getSettings(uint32 &outIdleTimeOut, bool &outLockOnSleep)
{
	mDb->getSettings(outIdleTimeOut, outLockOnSleep);
}

void
KeychainImpl::setSettings(uint32 inIdleTimeOut, bool inLockOnSleep)
{
	mDb->setSettings(inIdleTimeOut, inLockOnSleep);
}
void 
KeychainImpl::changePassphrase(UInt32 oldPasswordLength, const void *oldPassword,
	UInt32 newPasswordLength, const void *newPassword)
{
	// @@@ When AutoCredentials is actually finished we should no logner use a tracking allocator.
	TrackingAllocator allocator(CssmAllocator::standard());
	AutoCredentials cred = AutoCredentials(allocator);
	if (oldPassword)
	{
		const CssmData &oldPass = *new(allocator) CssmData(const_cast<void *>(oldPassword), oldPasswordLength);
		TypedList &oldList = *new(allocator) TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK);
		oldList.append(new(allocator) ListElement(CSSM_SAMPLE_TYPE_PASSWORD));
		oldList.append(new(allocator) ListElement(oldPass));
		cred += oldList;
	}

	if (newPassword)
	{
		const CssmData &newPass = *new(allocator) CssmData(const_cast<void *>(newPassword), newPasswordLength);
		TypedList &newList = *new(allocator) TypedList(allocator, CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK);
		newList.append(new(allocator) ListElement(CSSM_SAMPLE_TYPE_PASSWORD));
		newList.append(new(allocator) ListElement(newPass));
		cred += newList;
	}

	mDb->changePassphrase(&cred);
}

void
KeychainImpl::changePassphrase(ConstStringPtr oldPassword, ConstStringPtr newPassword)
{
	const void *oldPtr, *newPtr;
	UInt32 oldLen, newLen;
	if (oldPassword)
	{
		oldLen = oldPassword[0];
		oldPtr = oldPassword + 1;
	}
	else
	{
		oldLen = 0;
		oldPtr = NULL;
	}

	if (newPassword)
	{
		newLen = newPassword[0];
		newPtr = newPassword + 1;
	}
	else
	{
		newLen = 0;
		newPtr = NULL;
	}

	changePassphrase(oldLen, oldPtr, newLen, newPtr);
}

void
KeychainImpl::authenticate(const CSSM_ACCESS_CREDENTIALS *cred)
{
	// @@@ This should do an authenticate which is not the same as unlock.
	if (!exists())
		MacOSError::throwMe(errSecNoSuchKeychain);

	MacOSError::throwMe(unimpErr);
}

UInt32
KeychainImpl::status() const
{
	// @@@ We should figure out the read/write status though a DL passthrough or some other way.
	// @@@ Also should locked be unlocked read only or just read-only?
	return (mDb->isLocked() ? 0 : kSecUnlockStateStatus | kSecWritePermStatus) | kSecReadPermStatus;
}

bool
KeychainImpl::exists()
{
	bool exists = true;
	try
	{
		open();
		// Ok to leave the mDb open since it will get closed when it goes away.
	}
	catch (const CssmError &e)
	{
		if (e.cssmError() != CSSMERR_DL_DATASTORE_DOESNOT_EXIST)
			throw;
		exists = false;
	}

	return exists;
}

bool
KeychainImpl::isActive() const
{
	return mDb->isActive();
}

void
KeychainImpl::add(Item &inItem)
{
	Keychain keychain(this);
	PrimaryKey primaryKey = inItem->add(keychain);
	{
		StLock<Mutex> _(mDbItemMapLock);
		mDbItemMap[primaryKey] = inItem.get();
	}

    KCEventNotifier::PostKeychainEvent(kSecAddEvent, this, inItem);
}

void
KeychainImpl::didUpdate(ItemImpl *inItemImpl, PrimaryKey &oldPK,
						PrimaryKey &newPK)
{
	// Make sure we only hold mDbItemMapLock as long as we need to.
	{
		StLock<Mutex> _(mDbItemMapLock);
		DbItemMap::iterator it = mDbItemMap.find(oldPK);
		if (it != mDbItemMap.end() && it->second == inItemImpl)
			mDbItemMap.erase(it);
		mDbItemMap[newPK] = inItemImpl;
	}

    KCEventNotifier::PostKeychainEvent( kSecUpdateEvent, this, inItemImpl );
}

void
KeychainImpl::deleteItem(Item &inoutItem)
{
	// item must be persistant.
	if (!inoutItem->isPersistant())
		MacOSError::throwMe(errSecInvalidItemRef);

	DbUniqueRecord uniqueId = inoutItem->dbUniqueRecord();
	PrimaryKey primaryKey = inoutItem->primaryKey();
	uniqueId->deleteRecord();

	// Don't kill the ref or clear the Item() since this potentially
	// messes up things for the receiver of the kSecDeleteEvent notification.
	//inoutItem->killRef();
	//inoutItem = Item();

    // Post the notification for the item deletion with
	// the primaryKey obtained when the item still existed
	KCEventNotifier::PostKeychainEvent(kSecDeleteEvent, dLDbIdentifier(), primaryKey);
}


CssmClient::CSP
KeychainImpl::csp()
{
	if (!mDb->dl()->subserviceMask() & CSSM_SERVICE_CSP)
		MacOSError::throwMe(errSecInvalidKeychain);

	SSDb ssDb(safe_cast<SSDbImpl *>(&(*mDb)));
	return ssDb->csp();
}

PrimaryKey
KeychainImpl::makePrimaryKey(CSSM_DB_RECORDTYPE recordType, DbUniqueRecord &uniqueId)
{
	DbAttributes primaryKeyAttrs(uniqueId->database());
	primaryKeyAttrs.recordType(recordType);
	gatherPrimaryKeyAttributes(primaryKeyAttrs);
	uniqueId->get(&primaryKeyAttrs, NULL);
	return PrimaryKey(primaryKeyAttrs);
}

const CssmAutoDbRecordAttributeInfo &
KeychainImpl::primaryKeyInfosFor(CSSM_DB_RECORDTYPE recordType)
{
	return keychainSchema()->primaryKeyInfosFor(recordType);
}

void KeychainImpl::gatherPrimaryKeyAttributes(DbAttributes& primaryKeyAttrs)
{
	const CssmAutoDbRecordAttributeInfo &infos =
		primaryKeyInfosFor(primaryKeyAttrs.recordType());

	// @@@ fix this to not copy info.		
	for (uint32 i = 0; i < infos.size(); i++)
		primaryKeyAttrs.add(infos.at(i));
}

Item
KeychainImpl::item(const PrimaryKey& primaryKey)
{
	{
		StLock<Mutex> _(mDbItemMapLock);
		DbItemMap::iterator it = mDbItemMap.find(primaryKey);
		if (it != mDbItemMap.end())
		{
			return Item(it->second);
		}
	}

	// Create an item with just a primary key
    return Item(this, primaryKey);
}

Item
KeychainImpl::item(CSSM_DB_RECORDTYPE recordType, DbUniqueRecord &uniqueId)
{
	PrimaryKey primaryKey = makePrimaryKey(recordType, uniqueId);
	{
		StLock<Mutex> _(mDbItemMapLock);
		DbItemMap::iterator it = mDbItemMap.find(primaryKey);
		if (it != mDbItemMap.end())
		{
			return Item(it->second);
		}
	}

	// Create a new item
    return Item(this, primaryKey, uniqueId);
}

KeychainSchema
KeychainImpl::keychainSchema()
{
	if (!mKeychainSchema)
	{
		// @@@ Use cache in storageManager
		mKeychainSchema = KeychainSchema(mDb);
	}

	return mKeychainSchema;
}

// Called from DbItemImpl's constructor (so it is only paritally constructed), add it to the map. 
void
KeychainImpl::addItem(const PrimaryKey &primaryKey, ItemImpl *dbItemImpl)
{
	StLock<Mutex> _(mDbItemMapLock);
	DbItemMap::iterator it = mDbItemMap.find(primaryKey);
	if (it != mDbItemMap.end())
	{
		// @@@ There is a race condition here when being called in multiple threads
		// We might have added an item using add and received a notification at the same time
		//assert(true);
		throw errSecDuplicateItem;
		//mDbItemMap.erase(it);
		// @@@ What to do here?
	}

	mDbItemMap.insert(DbItemMap::value_type(primaryKey, dbItemImpl));
}

void
KeychainImpl::removeItem(const PrimaryKey &primaryKey, const ItemImpl *inItemImpl)
{
	// Sent from DbItemImpl's destructor, remove it from the map. 
	StLock<Mutex> _(mDbItemMapLock);
	DbItemMap::iterator it = mDbItemMap.find(primaryKey);
	if (it != mDbItemMap.end() && it->second == inItemImpl)
		mDbItemMap.erase(it);
}

void
KeychainImpl::getAttributeInfoForItemID(CSSM_DB_RECORDTYPE itemID, SecKeychainAttributeInfo **Info)
{
	keychainSchema()->getAttributeInfoForRecordType(itemID, Info);
}

void 
KeychainImpl::freeAttributeInfo(SecKeychainAttributeInfo *Info)
{
	free(Info->tag);
	free(Info->format);
	free(Info);
}

CssmDbAttributeInfo
KeychainImpl::attributeInfoFor(CSSM_DB_RECORDTYPE recordType, UInt32 tag)
{
	return keychainSchema()->attributeInfoFor(recordType, tag);

}

Keychain
Keychain::optional(SecKeychainRef handle)
{
	if (handle)
		return gTypes().keychain.required(handle);
	else
		return globals().defaultKeychain;
}

