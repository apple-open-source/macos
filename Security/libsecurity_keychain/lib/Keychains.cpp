/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// Keychains.cpp
//

#include "KCEventNotifier.h"
#include "Keychains.h"

#include "Item.h"
#include "KCCursor.h"
#include "Globals.h"
#include <security_cdsa_utilities/Schema.h>
#include <security_cdsa_client/keychainacl.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <security_utilities/trackingallocator.h>
#include <security_keychain/SecCFTypes.h>

#include "SecKeychainPriv.h"

#include <Security/SecKeychainItemPriv.h>
#include <CoreFoundation/CoreFoundation.h>
#include "DLDbListCFPref.h"
#include <fcntl.h>
#include <sys/param.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/time.h>

static dispatch_once_t SecKeychainSystemKeychainChecked;

OSStatus SecKeychainSystemKeychainCheckWouldDeadlock()
{
    dispatch_once(&SecKeychainSystemKeychainChecked, ^{});
    return errSecSuccess;
}

using namespace KeychainCore;
using namespace CssmClient;


typedef struct EventItem
{
	SecKeychainEvent kcEvent;
	Item item;
} EventItem;

typedef std::list<EventItem> EventBufferSuper;
class EventBuffer : public EventBufferSuper
{
public:
	EventBuffer () {}
	virtual ~EventBuffer ();
};


EventBuffer::~EventBuffer ()
{
}



//
// KeychainSchemaImpl
//
KeychainSchemaImpl::KeychainSchemaImpl(const Db &db) : mMutex(Mutex::recursive)
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
		if (CSSM_DB_RECORDTYPE_SCHEMA_START <= relationID
			&& relationID < CSSM_DB_RECORDTYPE_SCHEMA_END)
			continue;

		// Create a cursor on the SCHEMA_ATTRIBUTES table for records with
		// RelationID == relationID
		DbCursor attributes(db);
		attributes->recordType(CSSM_DL_DB_SCHEMA_ATTRIBUTES);
		attributes->add(CSSM_DB_EQUAL, Schema::RelationID, relationID);
	
		// Set up a record for retriving the SCHEMA_ATTRIBUTES
		DbAttributes attributeRecord(db, 2);
		attributeRecord.add(Schema::AttributeFormat);
		attributeRecord.add(Schema::AttributeID);	

		RelationInfoMap &rim = mDatabaseInfoMap[relationID];
		while (attributes->next(&attributeRecord, NULL, uniqueId))
			rim[attributeRecord.at(1)] = attributeRecord.at(0);
		
		// Create a cursor on the CSSM_DL_DB_SCHEMA_INDEXES table for records
		// with RelationID == relationID
		DbCursor indexes(db);
		indexes->recordType(CSSM_DL_DB_SCHEMA_INDEXES);
		indexes->conjunctive(CSSM_DB_AND);
		indexes->add(CSSM_DB_EQUAL, Schema::RelationID, relationID);
		indexes->add(CSSM_DB_EQUAL, Schema::IndexType,
			uint32(CSSM_DB_INDEX_UNIQUE));

		// Set up a record for retriving the SCHEMA_INDEXES
		DbAttributes indexRecord(db, 1);
		indexRecord.add(Schema::AttributeID);

		CssmAutoDbRecordAttributeInfo &infos =
			*new CssmAutoDbRecordAttributeInfo();
		mPrimaryKeyInfoMap.
			insert(PrimaryKeyInfoMap::value_type(relationID, &infos));
		infos.DataRecordType = relationID;
		while (indexes->next(&indexRecord, NULL, uniqueId))
		{
			CssmDbAttributeInfo &info = infos.add();
			info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
			info.Label.AttributeID = indexRecord.at(0);
			// @@@ Might insert bogus value if DB is corrupt
			info.AttributeFormat = rim[info.Label.AttributeID];
		}
	}
}

KeychainSchemaImpl::~KeychainSchemaImpl()
{
	try
	{
        map<CSSM_DB_RECORDTYPE, CssmAutoDbRecordAttributeInfo *>::iterator it = mPrimaryKeyInfoMap.begin();
        while (it != mPrimaryKeyInfoMap.end())
        {
            delete it->second;
            it++;
        }
		// for_each_map_delete(mPrimaryKeyInfoMap.begin(), mPrimaryKeyInfoMap.end());
	}
	catch(...)
	{
	}
}

const KeychainSchemaImpl::RelationInfoMap &
KeychainSchemaImpl::relationInfoMapFor(CSSM_DB_RECORDTYPE recordType) const
{
	DatabaseInfoMap::const_iterator dit = mDatabaseInfoMap.find(recordType);
	if (dit == mDatabaseInfoMap.end())
		MacOSError::throwMe(errSecNoSuchClass);
	return dit->second;
}

bool KeychainSchemaImpl::hasRecordType (CSSM_DB_RECORDTYPE recordType) const
{
	DatabaseInfoMap::const_iterator it = mDatabaseInfoMap.find(recordType);
	return it != mDatabaseInfoMap.end();
}
	
bool
KeychainSchemaImpl::hasAttribute(CSSM_DB_RECORDTYPE recordType, uint32 attributeId) const
{
	try
	{
		const RelationInfoMap &rmap = relationInfoMapFor(recordType);
		RelationInfoMap::const_iterator rit = rmap.find(attributeId);
		return rit != rmap.end();
	}
	catch (MacOSError result)
	{
		if (result.osStatus () == errSecNoSuchClass)
		{
			return false;
		}
		else
		{
			throw;
		}
	}
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
	
	size_t capacity=rmap.size();
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

void
KeychainSchemaImpl::didCreateRelation(CSSM_DB_RECORDTYPE relationID,
	const char *inRelationName,
	uint32 inNumberOfAttributes,
	const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
	uint32 inNumberOfIndexes,
	const CSSM_DB_SCHEMA_INDEX_INFO *pIndexInfo)
{
	StLock<Mutex>_(mMutex);
	
	if (CSSM_DB_RECORDTYPE_SCHEMA_START <= relationID
		&& relationID < CSSM_DB_RECORDTYPE_SCHEMA_END)
		return;

    // if our schema is already in the map, return
    if (mPrimaryKeyInfoMap.find(relationID) != mPrimaryKeyInfoMap.end())
    {
        return;
    }
    
	RelationInfoMap &rim = mDatabaseInfoMap[relationID];
	for (uint32 ix = 0; ix < inNumberOfAttributes; ++ix)
		rim[pAttributeInfo[ix].AttributeId] = pAttributeInfo[ix].DataType;

	CssmAutoDbRecordAttributeInfo *infos = new CssmAutoDbRecordAttributeInfo();
    
	mPrimaryKeyInfoMap.
		insert(PrimaryKeyInfoMap::value_type(relationID, infos));
	infos->DataRecordType = relationID;
	for (uint32 ix = 0; ix < inNumberOfIndexes; ++ix)
		if (pIndexInfo[ix].IndexType == CSSM_DB_INDEX_UNIQUE)
		{
			CssmDbAttributeInfo &info = infos->add();
			info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
			info.Label.AttributeID = pIndexInfo[ix].AttributeId;
			info.AttributeFormat = rim[info.Label.AttributeID];
		}
}



KeychainSchema::~KeychainSchema()

{
}



struct Event
{
	SecKeychainEvent eventCode;
	PrimaryKey primaryKey;
};
typedef std::list<Event> EventList;

#define SYSTEM_KEYCHAIN_CHECK_UNIX_BASE_NAME "/var/run/systemkeychaincheck"
#define SYSTEM_KEYCHAIN_CHECK_UNIX_DOMAIN_SOCKET_NAME (SYSTEM_KEYCHAIN_CHECK_UNIX_BASE_NAME ".socket")
#define SYSTEM_KEYCHAIN_CHECK_COMPLETE_FILE_NAME (SYSTEM_KEYCHAIN_CHECK_UNIX_BASE_NAME ".done")

static void check_system_keychain()
{
	// sadly we can't use XPC here, XPC_DOMAIN_TYPE_SYSTEM doesn't exist yet.  Also xpc-helper uses the
	// keychain API (I assume for checking codesign things).   So we use Unix Domain Sockets.
	
	// NOTE: if we hit a system error we attempt to log it, and then just don't check the system keychain.
	// In theory a system might be able to recover from this state if we let it try to muddle along, and
	// past behaviour didn't even try this hard to do the keychain check.  In particular we could be in a
	// sandbox'ed process.   So we just do our best and let another process try again.
	
	struct stat keycheck_file_info;
	if (stat(SYSTEM_KEYCHAIN_CHECK_COMPLETE_FILE_NAME, &keycheck_file_info) < 0) {
		int server_fd = socket(PF_UNIX, SOCK_STREAM, 0);
		if (server_fd < 0) {
			syslog(LOG_ERR, "Can't get socket (%m) system keychain may be unchecked");
			return;
		}
		
		struct sockaddr_un keychain_check_server_address;
		keychain_check_server_address.sun_family = AF_UNIX;
		if (strlcpy(keychain_check_server_address.sun_path, SYSTEM_KEYCHAIN_CHECK_UNIX_DOMAIN_SOCKET_NAME, sizeof(keychain_check_server_address.sun_path)) > sizeof(keychain_check_server_address.sun_path)) {
			// It would be nice if we could compile time assert this
			syslog(LOG_ERR, "Socket path too long, max length %lu, your length %lu", (unsigned long)sizeof(keychain_check_server_address.sun_path), (unsigned long)strlen(SYSTEM_KEYCHAIN_CHECK_UNIX_DOMAIN_SOCKET_NAME));
			close(server_fd);
			return;
		}
		keychain_check_server_address.sun_len = SUN_LEN(&keychain_check_server_address);
		
		int rc = connect(server_fd, (struct sockaddr *)&keychain_check_server_address, keychain_check_server_address.sun_len);
		if (rc < 0) {
			syslog(LOG_ERR, "Can not connect to %s: %m", SYSTEM_KEYCHAIN_CHECK_UNIX_DOMAIN_SOCKET_NAME);
			close(server_fd);
			return;
		}
		
		// this read lets us block until the EOF comes, we don't ever get a byte (and if we do, we don't care about it)
		char byte;
		ssize_t read_size = read(server_fd, &byte, 1);
		if (read_size < 0) {
			syslog(LOG_ERR, "Error reading from system keychain checker: %m");
		}
		
		close(server_fd);
		return;
	}
}

//
// KeychainImpl
//
KeychainImpl::KeychainImpl(const Db &db)
	: mInCache(false), mDb(db), mCustomUnlockCreds (this), mIsInBatchMode (false), mMutex(Mutex::recursive)
{
	dispatch_once(&SecKeychainSystemKeychainChecked, ^{
		check_system_keychain();
	});
	mDb->defaultCredentials(this);	// install activation hook
	mEventBuffer = new EventBuffer;
}

KeychainImpl::~KeychainImpl() 
{
	try
	{
		// Remove ourselves from the cache if we are in it.
        // fprintf(stderr, "Removing %p from storage manager cache.\n", handle(false));
		globals().storageManager.removeKeychain(dlDbIdentifier(), this);
		delete mEventBuffer;
	}
	catch(...)
	{
	}
}

Mutex*
KeychainImpl::getMutexForObject()
{
	return globals().storageManager.getStorageManagerMutex();
}

Mutex*
KeychainImpl::getKeychainMutex()
{
	return &mMutex;
}

void KeychainImpl::aboutToDestruct()
{
    // remove me from the global cache, we are done
    // fprintf(stderr, "Destructing keychain object\n");
    DLDbIdentifier identifier = dlDbIdentifier();
    globals().storageManager.removeKeychain(identifier, this);
}

bool
KeychainImpl::operator ==(const KeychainImpl &keychain) const
{
	return dlDbIdentifier() == keychain.dlDbIdentifier();
}

KCCursor
KeychainImpl::createCursor(SecItemClass itemClass, const SecKeychainAttributeList *attrList)
{
	StLock<Mutex>_(mMutex);
	
	StorageManager::KeychainList keychains;
	keychains.push_back(Keychain(this));
	return KCCursor(keychains, itemClass, attrList);
}

KCCursor
KeychainImpl::createCursor(const SecKeychainAttributeList *attrList)
{
	StLock<Mutex>_(mMutex);
	
	StorageManager::KeychainList keychains;
	keychains.push_back(Keychain(this));
	return KCCursor(keychains, attrList);
}

void
KeychainImpl::create(UInt32 passwordLength, const void *inPassword)
{
	StLock<Mutex>_(mMutex);
	
	if (!inPassword)
	{
		create();
		return;
	}

	Allocator &alloc = Allocator::standard();

	// @@@ Share this instance

	const CssmData password(const_cast<void *>(inPassword), passwordLength);
	AclFactory::PasswordChangeCredentials pCreds (password, alloc);
	AclFactory::AnyResourceContext rcc(pCreds);
	create(&rcc);
}

void KeychainImpl::create(ConstStringPtr inPassword)
{
	StLock<Mutex>_(mMutex);
	
    if ( inPassword )
        create(static_cast<UInt32>(inPassword[0]), &inPassword[1]);
    else
        create();
}

void
KeychainImpl::create()
{
	StLock<Mutex>_(mMutex);
	
	AclFactory aclFactory;
	AclFactory::AnyResourceContext rcc(aclFactory.unlockCred());
	create(&rcc);
}

void KeychainImpl::createWithBlob(CssmData &blob)
{
	StLock<Mutex>_(mMutex);
	
	mDb->dbInfo(&Schema::DBInfo);
	AclFactory aclFactory;
	AclFactory::AnyResourceContext rcc(aclFactory.unlockCred());
	mDb->resourceControlContext (&rcc);
	try
	{
		mDb->createWithBlob(blob);
	}
	catch (...)
	{
		mDb->resourceControlContext(NULL);
		mDb->dbInfo(NULL);
		throw;
	}
	mDb->resourceControlContext(NULL);
	mDb->dbInfo(NULL); // Clear the schema (to not break an open call later)
	globals().storageManager.created(Keychain(this));

    KCEventNotifier::PostKeychainEvent (kSecKeychainListChangedEvent, this, NULL);
}
	
void
KeychainImpl::create(const ResourceControlContext *rcc)
{
	StLock<Mutex>_(mMutex);
	
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
	StLock<Mutex>_(mMutex);
	
	mDb->open();
}

void
KeychainImpl::lock()
{
	StLock<Mutex>_(mMutex);
	
	mDb->lock();
}

void
KeychainImpl::unlock()
{
	StLock<Mutex>_(mMutex);
	
	mDb->unlock();
}

void
KeychainImpl::unlock(const CssmData &password)
{
	StLock<Mutex>_(mMutex);
	
	mDb->unlock(password);
}

void
KeychainImpl::unlock(ConstStringPtr password)
{
	StLock<Mutex>_(mMutex);
	
	if (password)
	{
		const CssmData data(const_cast<unsigned char *>(&password[1]), password[0]);
		unlock(data);
	}
	else
		unlock();
}

void
KeychainImpl::stash()
{
  	StLock<Mutex>_(mMutex);
	
	mDb->stash();
}

void
KeychainImpl::stashCheck()
{
  	StLock<Mutex>_(mMutex);
	
	mDb->stashCheck();
}

void
KeychainImpl::getSettings(uint32 &outIdleTimeOut, bool &outLockOnSleep)
{
	StLock<Mutex>_(mMutex);
	
	mDb->getSettings(outIdleTimeOut, outLockOnSleep);
}

void
KeychainImpl::setSettings(uint32 inIdleTimeOut, bool inLockOnSleep)
{
	StLock<Mutex>_(mMutex);
	
	// The .Mac syncing code only makes sense for the AppleFile CSP/DL,
	// but other DLs such as the OCSP and LDAP DLs do not expose a way to
	// change settings or the password. To make a minimal change that only affects
	// the smartcard case, we only look for that CSP/DL
	
	bool isSmartcard = 	(mDb->dl()->guid() == gGuidAppleSdCSPDL);
	
	// get the old keychain blob so that we can tell .Mac to resync it
	CssmAutoData oldBlob(mDb ->allocator());
	if (!isSmartcard)
		mDb->copyBlob(oldBlob.get());
	
	mDb->setSettings(inIdleTimeOut, inLockOnSleep);
}

void 
KeychainImpl::changePassphrase(UInt32 oldPasswordLength, const void *oldPassword,
	UInt32 newPasswordLength, const void *newPassword)
{
	StLock<Mutex>_(mMutex);
	
	bool isSmartcard = 	(mDb->dl()->guid() == gGuidAppleSdCSPDL);

	TrackingAllocator allocator(Allocator::standard());
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

	// get the old keychain blob so that we can tell .Mac to resync it
	CssmAutoData oldBlob(mDb->allocator());
	if (!isSmartcard)
		mDb->copyBlob(oldBlob.get());
	
	mDb->changePassphrase(&cred);
}

void
KeychainImpl::changePassphrase(ConstStringPtr oldPassword, ConstStringPtr newPassword)
{
	StLock<Mutex>_(mMutex);
	
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
	StLock<Mutex>_(mMutex);
	
	if (!exists())
		MacOSError::throwMe(errSecNoSuchKeychain);

	MacOSError::throwMe(errSecUnimplemented);
}

UInt32
KeychainImpl::status() const
{
	// @@@ We should figure out the read/write status though a DL passthrough
	// or some other way. Also should locked be unlocked read only or just
	// read-only?
	return (mDb->isLocked() ? 0 : kSecUnlockStateStatus | kSecWritePermStatus)
		| kSecReadPermStatus;
}

bool
KeychainImpl::exists()
{
	StLock<Mutex>_(mMutex);
	
	bool exists = true;
	try
	{
		open();
		// Ok to leave the mDb open since it will get closed when it goes away.
	}
	catch (const CssmError &e)
	{
		if (e.osStatus() != CSSMERR_DL_DATASTORE_DOESNOT_EXIST)
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

void KeychainImpl::completeAdd(Item &inItem, PrimaryKey &primaryKey)
{

	
	// The inItem shouldn't be in the cache yet
	assert(!inItem->inCache());

	// Insert inItem into mDbItemMap with key primaryKey.  p.second will be
	// true if it got inserted. If not p.second will be false and p.first
	// will point to the current entry with key primaryKey.
	pair<DbItemMap::iterator, bool> p =
		mDbItemMap.insert(DbItemMap::value_type(primaryKey, inItem.get()));
	if (!p.second)
	{
		// There was already an ItemImpl * in mDbItemMap with key
		// primaryKey.  Get a ref to the pointer to it so we can assign a
		// new value to it below.
		ItemImpl *oldItem = p.first->second;

		// @@@ If this happens we are breaking our API contract of 
		// uniquifying items.  We really need to insert the item into the
		// map before we start the add.  And have the item be in an
		// "is being added" state.
		assert(oldItem->inCache());
		secdebug("keychain", "add of new item %p somehow replaced %p",
			inItem.get(), oldItem);
		oldItem->inCache(false);
		oldItem = inItem.get();
	}

	inItem->inCache(true);
}

void
KeychainImpl::addCopy(Item &inItem)
{
	Keychain keychain(this);
	PrimaryKey primaryKey = inItem->addWithCopyInfo(keychain, true);
	completeAdd(inItem, primaryKey);
	postEvent(kSecAddEvent, inItem);
}

void
KeychainImpl::add(Item &inItem)
{
	Keychain keychain(this);
	PrimaryKey primaryKey = inItem->add(keychain);
	completeAdd(inItem, primaryKey);
	postEvent(kSecAddEvent, inItem);
}

void
KeychainImpl::didUpdate(const Item &inItem, PrimaryKey &oldPK,
						PrimaryKey &newPK)
{
	// If the primary key hasn't changed we don't need to update mDbItemMap.
	if (oldPK != newPK)
	{
		// If inItem isn't in the cache we don't need to update mDbItemMap.
		assert(inItem->inCache());
		if (inItem->inCache())
		{
			// First remove the entry for inItem in mDbItemMap with key oldPK.
			DbItemMap::iterator it = mDbItemMap.find(oldPK);
			if (it != mDbItemMap.end() && (ItemImpl*) it->second == inItem.get())
				mDbItemMap.erase(it);

			// Insert inItem into mDbItemMap with key newPK.  p.second will be
			// true if it got inserted. If not p.second will be false and
			// p.first will point to the current entry with key newPK.
			pair<DbItemMap::iterator, bool> p =
				mDbItemMap.insert(DbItemMap::value_type(newPK, inItem.get()));
			if (!p.second)
			{
				// There was already an ItemImpl * in mDbItemMap with key
				// primaryKey.  Get a ref to the pointer to it so we can assign
				// a new value to it below.
				ItemImpl *oldItem = p.first->second;

				// @@@ If this happens we are breaking our API contract of 
				// uniquifying items.  We really need to insert the item into
				// the map with the new primary key before we start the update.
				// And have the item be in an "is being updated" state.
				assert(oldItem->inCache());
				secdebug("keychain", "update of item %p somehow replaced %p",
					inItem.get(), oldItem);
				oldItem->inCache(false);
				oldItem = inItem.get();
			}
		}
	}
	
	postEvent(kSecUpdateEvent, inItem);
}

void
KeychainImpl::deleteItem(Item &inoutItem)
{
	{
		// We don't need to hold the DO mutex through event posting, and, in fact, doing so causes deadlock.
		// Hold it only as long as needed, instead.
	
		
		// item must be persistent
		if (!inoutItem->isPersistent())
			MacOSError::throwMe(errSecInvalidItemRef);

		DbUniqueRecord uniqueId = inoutItem->dbUniqueRecord();
		PrimaryKey primaryKey = inoutItem->primaryKey();
		uniqueId->deleteRecord();

		// Don't remove the item from the mDbItemMap here since this would cause
		// us to report a new item to our caller when we receive the
		// kSecDeleteEvent notification.
		// It will be removed before we post the notification, because
		// CCallbackMgr will call didDeleteItem() 

		// Post the notification for the item deletion with
		// the primaryKey obtained when the item still existed
	}
	
	postEvent(kSecDeleteEvent, inoutItem);
}


CssmClient::CSP
KeychainImpl::csp()
{
	StLock<Mutex>_(mMutex);
	
	if (!mDb->dl()->subserviceMask() & CSSM_SERVICE_CSP)
		MacOSError::throwMe(errSecInvalidKeychain);

	// Try to cast first to a CSPDL to handle case where we don't have an SSDb
	try
	{
		CssmClient::CSPDL cspdl(dynamic_cast<CssmClient::CSPDLImpl *>(&*mDb->dl()));
		return CSP(cspdl);
	}
	catch (...)
	{
		SSDbImpl* impl = dynamic_cast<SSDbImpl *>(&(*mDb));
		if (impl == NULL)
		{
			CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
		}
		
		SSDb ssDb(impl);
		return ssDb->csp();
	}
}

PrimaryKey
KeychainImpl::makePrimaryKey(CSSM_DB_RECORDTYPE recordType, DbUniqueRecord &uniqueId)
{
	StLock<Mutex>_(mMutex);
	
	DbAttributes primaryKeyAttrs(uniqueId->database());
	primaryKeyAttrs.recordType(recordType);
	gatherPrimaryKeyAttributes(primaryKeyAttrs);
	uniqueId->get(&primaryKeyAttrs, NULL);
	return PrimaryKey(primaryKeyAttrs);
}

const CssmAutoDbRecordAttributeInfo &
KeychainImpl::primaryKeyInfosFor(CSSM_DB_RECORDTYPE recordType)
{
	StLock<Mutex>_(mMutex);
	
	try
	{
		return keychainSchema()->primaryKeyInfosFor(recordType);
	}
	catch (const CommonError &error)
	{
		switch (error.osStatus())
		{
		case errSecNoSuchClass:
		case CSSMERR_DL_INVALID_RECORDTYPE:
			resetSchema();
			return keychainSchema()->primaryKeyInfosFor(recordType);
		default:
			throw;
		}
	}
}

void KeychainImpl::gatherPrimaryKeyAttributes(DbAttributes& primaryKeyAttrs)
{
	StLock<Mutex>_(mMutex);
	
	const CssmAutoDbRecordAttributeInfo &infos =
		primaryKeyInfosFor(primaryKeyAttrs.recordType());

	// @@@ fix this to not copy info.		
	for (uint32 i = 0; i < infos.size(); i++)
		primaryKeyAttrs.add(infos.at(i));
}

ItemImpl *
KeychainImpl::_lookupItem(const PrimaryKey &primaryKey)
{
	DbItemMap::iterator it = mDbItemMap.find(primaryKey);
	if (it != mDbItemMap.end())
	{
		if (it->second == NULL)
		{
			// we've been weak released...
			mDbItemMap.erase(it);
		}
		else
		{
			return it->second;
		}
	}
	
	return NULL;
}

Item
KeychainImpl::item(const PrimaryKey &primaryKey)
{
	StLock<Mutex>_(mMutex);
	
	// Lookup the item in the map while holding the apiLock.
	ItemImpl *itemImpl = _lookupItem(primaryKey);
	if (itemImpl)
		return Item(itemImpl);

	try
	{
		// We didn't find it so create a new item with just a keychain and
		// a primary key.  However since we aren't holding
		// globals().apiLock anymore some other thread might have beaten
		// us to creating this item and adding it to the cache.  If that
		// happens we retry the lookup.
		return Item(this, primaryKey);
	}
	catch (const MacOSError &e)
	{
		// If the item creation failed because some other thread already
		// inserted this item into the cache we retry the lookup.
		if (e.osStatus() == errSecDuplicateItem)
		{
			// Lookup the item in the map while holding the apiLock.
			ItemImpl *itemImpl = _lookupItem(primaryKey);
			if (itemImpl)
				return Item(itemImpl);
		}
		throw;
	}
}


Item
KeychainImpl::item(CSSM_DB_RECORDTYPE recordType, DbUniqueRecord &uniqueId)
{
	StLock<Mutex>_(mMutex);
	
	PrimaryKey primaryKey = makePrimaryKey(recordType, uniqueId);
	{
		// Lookup the item in the map while holding the apiLock.
		ItemImpl *itemImpl = _lookupItem(primaryKey);
		
		if (itemImpl)
		{
			return Item(itemImpl);
		}
	}

	try
	{
		// We didn't find it so create a new item with a keychain, a primary key
		// and a DbUniqueRecord. However since we aren't holding
		// globals().apiLock anymore some other thread might have beaten
		// us to creating this item and adding it to the cache.  If that
		// happens we retry the lookup.
		return Item(this, primaryKey, uniqueId);
	}
	catch (const MacOSError &e)
	{
		// If the item creation failed because some other thread already
		// inserted this item into the cache we retry the lookup.
		if (e.osStatus() == errSecDuplicateItem)
		{
			// Lookup the item in the map while holding the apiLock.
			ItemImpl *itemImpl = _lookupItem(primaryKey);
			if (itemImpl)
				return Item(itemImpl);
		}
		throw;
	}
}

KeychainSchema
KeychainImpl::keychainSchema()
{
	StLock<Mutex>_(mMutex);
	if (!mKeychainSchema)
		mKeychainSchema = KeychainSchema(mDb);

	return mKeychainSchema;
}

void KeychainImpl::resetSchema()
{
	mKeychainSchema = NULL;	// re-fetch it from db next time
}


// Called from DbItemImpl's constructor (so it is only partially constructed),
// add it to the map. 
void
KeychainImpl::addItem(const PrimaryKey &primaryKey, ItemImpl *dbItemImpl)
{
	StLock<Mutex>_(mMutex);
	
	// The dbItemImpl shouldn't be in the cache yet
	assert(!dbItemImpl->inCache());

	// Insert dbItemImpl into mDbItemMap with key primaryKey.  p.second will
	// be true if it got inserted. If not p.second will be false and p.first
	// will point to the current entry with key primaryKey.
	pair<DbItemMap::iterator, bool> p =
		mDbItemMap.insert(DbItemMap::value_type(primaryKey, dbItemImpl));
	
	if (!p.second)
	{
		// There was already an ItemImpl * in mDbItemMap with key primaryKey.
		// There is a race condition here when being called in multiple threads
		// We might have added an item using add and received a notification at
		// the same time.
		MacOSError::throwMe(errSecDuplicateItem);
	}

	dbItemImpl->inCache(true);
}

void
KeychainImpl::didDeleteItem(ItemImpl *inItemImpl)
{
	StLock<Mutex>_(mMutex);
	
	// Called by CCallbackMgr
    secdebug("kcnotify", "%p notified that item %p was deleted", this, inItemImpl);
	removeItem(inItemImpl->primaryKey(), inItemImpl);
}

void
KeychainImpl::removeItem(const PrimaryKey &primaryKey, ItemImpl *inItemImpl)
{
	StLock<Mutex>_(mMutex);

	// If inItemImpl isn't in the cache to begin with we are done.
	if (!inItemImpl->inCache())
		return;

	DbItemMap::iterator it = mDbItemMap.find(primaryKey);
	if (it != mDbItemMap.end() && (ItemImpl*) it->second == inItemImpl)
		mDbItemMap.erase(it);

	inItemImpl->inCache(false);
}

void
KeychainImpl::getAttributeInfoForItemID(CSSM_DB_RECORDTYPE itemID,
	SecKeychainAttributeInfo **Info)
{
	StLock<Mutex>_(mMutex);
	
	try
	{
		keychainSchema()->getAttributeInfoForRecordType(itemID, Info);
	}
	catch (const CommonError &error)
	{
		switch (error.osStatus())
		{
		case errSecNoSuchClass:
		case CSSMERR_DL_INVALID_RECORDTYPE:
			resetSchema();
			keychainSchema()->getAttributeInfoForRecordType(itemID, Info);
		default:
			throw;
		}
	}
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
	StLock<Mutex>_(mMutex);
	
	try
	{
		return keychainSchema()->attributeInfoFor(recordType, tag);
	}
	catch (const CommonError &error)
	{
		switch (error.osStatus())
		{
		case errSecNoSuchClass:
		case CSSMERR_DL_INVALID_RECORDTYPE:
			resetSchema();
			return keychainSchema()->attributeInfoFor(recordType, tag);
		default:
			throw;
		}
	}
}

void
KeychainImpl::recode(const CssmData &data, const CssmData &extraData)
{
	StLock<Mutex>_(mMutex);
	
	mDb->recode(data, extraData);
}

void
KeychainImpl::copyBlob(CssmData &data)
{
	StLock<Mutex>_(mMutex);
	
	mDb->copyBlob(data);
}

void
KeychainImpl::setBatchMode(Boolean mode, Boolean rollback)
{
	StLock<Mutex>_(mMutex);
	
	mDb->setBatchMode(mode, rollback);
	mIsInBatchMode = mode;
	if (!mode)
	{
		if (!rollback) // was batch mode being turned off without an abort?
		{
			// dump the buffer
			EventBuffer::iterator it = mEventBuffer->begin();
			while (it != mEventBuffer->end())
			{
				PrimaryKey primaryKey;
				if (it->item)
				{
					primaryKey = it->item->primaryKey();
				}
				
				KCEventNotifier::PostKeychainEvent(it->kcEvent, mDb->dlDbIdentifier(), primaryKey);
				
				++it;
			}
			
		}

		// notify that a keychain has changed in too many ways to count
		KCEventNotifier::PostKeychainEvent(kSecKeychainLeftBatchModeEvent);
		mEventBuffer->clear();
	}
	else
	{
		KCEventNotifier::PostKeychainEvent(kSecKeychainEnteredBatchModeEvent);
	}
}

void
KeychainImpl::postEvent(SecKeychainEvent kcEvent, ItemImpl* item)
{
	PrimaryKey primaryKey;

	{
		StLock<Mutex>_(mMutex);
		
		if (item != NULL)
		{
			primaryKey = item->primaryKey();
		}
	}

	if (!mIsInBatchMode)
	{
		KCEventNotifier::PostKeychainEvent(kcEvent, mDb->dlDbIdentifier(), primaryKey);
	}
	else
	{
		StLock<Mutex>_(mMutex);
		
		EventItem it;
		it.kcEvent = kcEvent;
		if (item != NULL)
		{
			it.item = item;
		}
		
		mEventBuffer->push_back (it);
	}
}

Keychain::Keychain()
{
	dispatch_once(&SecKeychainSystemKeychainChecked, ^{
		check_system_keychain();
	});
}

Keychain::~Keychain()
{
}



Keychain
Keychain::optional(SecKeychainRef handle)
{
	if (handle)
		return KeychainImpl::required(handle);
	else
		return globals().storageManager.defaultKeychain();
}


CFIndex KeychainCore::GetKeychainRetainCount(Keychain& kc)
{
	CFTypeRef ref = kc->handle(false);
	return CFGetRetainCount(ref);
}


//
// Create default credentials for this keychain.
// This is triggered upon default open (i.e. a Db::activate() with no set credentials).
//
// This function embodies the "default credentials" logic for Keychain-layer databases.
//
const AccessCredentials *
KeychainImpl::makeCredentials()
{
	return defaultCredentials();
}


const AccessCredentials *
KeychainImpl::defaultCredentials()
{
	StLock<Mutex>_(mMutex);
	
	// Use custom unlock credentials for file keychains which have a referral
	// record and the standard credentials for all others.
	
	if (mDb->dl()->guid() == gGuidAppleCSPDL && mCustomUnlockCreds(mDb))
		return &mCustomUnlockCreds;
	else
	if (mDb->dl()->guid() == gGuidAppleSdCSPDL)
		return globals().smartcardCredentials();
	else
		return globals().keychainCredentials();
}



bool KeychainImpl::mayDelete()
{
    return true;
}
