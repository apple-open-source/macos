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
#include <security_utilities/FileLockTransaction.h>
#include <security_keychain/SecCFTypes.h>
#include <securityd_client/ssblob.h>
#include <Security/TrustSettingsSchema.h>

#include "SecKeychainPriv.h"

#include <Security/SecKeychainItemPriv.h>
#include <CoreFoundation/CoreFoundation.h>
#include "DLDbListCFPref.h"
#include <fcntl.h>
#include <glob.h>
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
			formatBuf=reinterpret_cast<UInt32 *>(realloc(formatBuf, (capacity*sizeof(UInt32))));
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
:  mCacheTimer(NULL), mSuppressTickle(false), mAttemptedUpgrade(false), mDbItemMapMutex(Mutex::recursive), mDbDeletedItemMapMutex(Mutex::recursive),
      mInCache(false), mDb(db), mCustomUnlockCreds (this), mIsInBatchMode (false), mMutex(Mutex::recursive)
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
KeychainImpl::getMutexForObject() const
{
	return globals().storageManager.getStorageManagerMutex();
}

Mutex*
KeychainImpl::getKeychainMutex()
{
	return &mMutex;
}

ReadWriteLock*
KeychainImpl::getKeychainReadWriteLock()
{
    return &mRWLock;
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

    // Now that we've created, trigger setting the defaultCredentials
    mDb->open();
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

    // Now that we've created, trigger setting the defaultCredentials
    mDb->open();
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
    StLock<Mutex>_(mMutex);

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
    StLock<Mutex> _(mDbItemMapMutex);
	pair<DbItemMap::iterator, bool> p =
		mDbItemMap.insert(DbItemMap::value_type(primaryKey, inItem.get()));
	if (!p.second)
	{
		// There was already an ItemImpl * in mDbItemMap with key
		// primaryKey. Remove it, and try the add again.
		ItemImpl *oldItem = p.first->second;

		// @@@ If this happens we are breaking our API contract of
		// uniquifying items.  We really need to insert the item into the
		// map before we start the add.  And have the item be in an
		// "is being added" state.
		secnotice("keychain", "add of new item %p somehow replaced %p",
			inItem.get(), oldItem);

        mDbItemMap.erase(p.first);
        oldItem->inCache(false);
        forceRemoveFromCache(oldItem);
        mDbItemMap.insert(DbItemMap::value_type(primaryKey, inItem.get()));
	}

	inItem->inCache(true);
}

void
KeychainImpl::addCopy(Item &inItem)
{
    StReadWriteLock _(mRWLock, StReadWriteLock::Write);

	Keychain keychain(this);
	PrimaryKey primaryKey = inItem->addWithCopyInfo(keychain, true);
	completeAdd(inItem, primaryKey);
	postEvent(kSecAddEvent, inItem);
}

void
KeychainImpl::add(Item &inItem)
{
    // Make sure we hold a write lock on ourselves when we do this
    StReadWriteLock _(mRWLock, StReadWriteLock::Write);

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
            StLock<Mutex> _(mDbItemMapMutex);
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
				// primaryKey. Remove it, and try the add again.
				ItemImpl *oldItem = p.first->second;

				// @@@ If this happens we are breaking our API contract of
				// uniquifying items.  We really need to insert the item into
				// the map with the new primary key before we start the update.
				// And have the item be in an "is being updated" state.
				secnotice("keychain", "update of item %p somehow replaced %p",
					inItem.get(), oldItem);

                mDbItemMap.erase(p.first);
                oldItem->inCache(false);
                forceRemoveFromCache(oldItem);
                mDbItemMap.insert(DbItemMap::value_type(newPK, inItem.get()));
			}
		}
	}

    // Item updates now are technically a delete and re-add, so post these events instead of kSecUpdateEvent
    postEvent(kSecDeleteEvent, inItem, oldPK);
    postEvent(kSecAddEvent, inItem);
}

void
KeychainImpl::deleteItem(Item &inoutItem)
{
    StReadWriteLock _(mRWLock, StReadWriteLock::Write);

	{
		// item must be persistent
		if (!inoutItem->isPersistent())
			MacOSError::throwMe(errSecInvalidItemRef);

        secinfo("kcnotify", "starting deletion of item %p", inoutItem.get());

		DbUniqueRecord uniqueId = inoutItem->dbUniqueRecord();
		PrimaryKey primaryKey = inoutItem->primaryKey();
		uniqueId->deleteRecord();

        // Move the item from mDbItemMap to mDbDeletedItemMap. We need the item
        // to give to the client process when we receive the kSecDeleteEvent
        // notification, but if that notification never arrives, we don't want
        // the item hanging around. When didDeleteItem is called by CCallbackMgr,
        // we'll remove all traces of the item.

        if (inoutItem->inCache()) {
            StLock<Mutex> _(mDbItemMapMutex);
            StLock<Mutex> __(mDbDeletedItemMapMutex);
            // Only look for it if it's in the cache
            DbItemMap::iterator it = mDbItemMap.find(primaryKey);

            if (it != mDbItemMap.end() && (ItemImpl*) it->second == inoutItem.get()) {
                mDbDeletedItemMap.insert(DbItemMap::value_type(primaryKey, it->second));
                mDbItemMap.erase(it);
            }
        }

		// Post the notification for the item deletion with
		// the primaryKey obtained when the item still existed
	}
	
	postEvent(kSecDeleteEvent, inoutItem);
}

void KeychainImpl::changeDatabase(CssmClient::Db db)
{
    StLock<Mutex>_(mDbMutex);
    mDb = db;
    mDb->defaultCredentials(this);
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

PrimaryKey
KeychainImpl::makePrimaryKey(CSSM_DB_RECORDTYPE recordType, DbAttributes* currentAttributes)
{
    StLock<Mutex>_(mMutex);

    DbAttributes primaryKeyAttrs;
    primaryKeyAttrs.recordType(recordType);
    gatherPrimaryKeyAttributes(primaryKeyAttrs);

    for(int i = 0; i < primaryKeyAttrs.size(); i++) {
        CssmDbAttributeData& attr = primaryKeyAttrs[i];

        CssmDbAttributeData * actual = currentAttributes->find(attr.info());
        if(actual) {
            attr.set(*actual, Allocator::standard());
        }
    }
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
	StLock<Mutex> _(mMutex);
	
	const CssmAutoDbRecordAttributeInfo &infos =
		primaryKeyInfosFor(primaryKeyAttrs.recordType());

	// @@@ fix this to not copy info.		
	for (uint32 i = 0; i < infos.size(); i++)
		primaryKeyAttrs.add(infos.at(i));
}

ItemImpl *
KeychainImpl::_lookupItem(const PrimaryKey &primaryKey)
{
    StLock<Mutex> _(mDbItemMapMutex);
	DbItemMap::iterator it = mDbItemMap.find(primaryKey);
	if (it != mDbItemMap.end())
	{
        return it->second;
	}
	
	return NULL;
}

ItemImpl *
KeychainImpl::_lookupDeletedItemOnly(const PrimaryKey &primaryKey)
{
    StLock<Mutex> _(mDbDeletedItemMapMutex);
    DbItemMap::iterator it = mDbDeletedItemMap.find(primaryKey);
    if (it != mDbDeletedItemMap.end())
    {
        return it->second;
    }

    return NULL;
}

Item
KeychainImpl::item(const PrimaryKey &primaryKey)
{
	StLock<Mutex>_(mMutex);
	
	// Lookup the item in the map while holding the apiLock.
	ItemImpl *itemImpl = _lookupItem(primaryKey);
	if (itemImpl) {
		return Item(itemImpl);
    }

	try
	{
		// We didn't find it so create a new item with just a keychain and
		// a primary key.  Some other thread might have beaten
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
// Check for an item that may have been deleted.
Item
KeychainImpl::itemdeleted(const PrimaryKey& primaryKey) {
    StLock<Mutex>_(mMutex);

    Item i = _lookupDeletedItemOnly(primaryKey);
    if(i.get()) {
        return i;
    } else {
        return item(primaryKey);
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
    StLock<Mutex> __(mDbItemMapMutex);
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
    secinfo("kcnotify", "%p notified that item %p was deleted", this, inItemImpl);
	removeItem(inItemImpl->primaryKey(), inItemImpl);
}

void
KeychainImpl::removeItem(const PrimaryKey &primaryKey, ItemImpl *inItemImpl)
{
	StLock<Mutex>_(mMutex);

	// If inItemImpl isn't in the cache to begin with we are done.
	if (!inItemImpl->inCache())
		return;

    {
        StLock<Mutex> _(mDbItemMapMutex);
        DbItemMap::iterator it = mDbItemMap.find(primaryKey);
        if (it != mDbItemMap.end() && (ItemImpl*) it->second == inItemImpl) {
            mDbItemMap.erase(it);
        }
    } // drop mDbItemMapMutex

    {
        StLock<Mutex> _(mDbDeletedItemMapMutex);
        DbItemMap::iterator it = mDbDeletedItemMap.find(primaryKey);
        if (it != mDbDeletedItemMap.end() && (ItemImpl*) it->second == inItemImpl) {
            mDbDeletedItemMap.erase(it);
        }
    } // drop mDbDeletedItemMapMutex

	inItemImpl->inCache(false);
}

void
KeychainImpl::forceRemoveFromCache(ItemImpl* inItemImpl) {
    try {
        // Wrap all this in a try-block and ignore all errors - we're trying to clean up these maps
        {
            StLock<Mutex> _(mDbItemMapMutex);
            for(DbItemMap::iterator it = mDbItemMap.begin(); it != mDbItemMap.end(); ) {
                if(it->second == inItemImpl) {
                    // Increment the iterator, but use its pre-increment value for the erase
                    it->second->inCache(false);
                    mDbItemMap.erase(it++);
                } else {
                    it++;
                }
            }
        } // drop mDbItemMapMutex

        {
            StLock<Mutex> _(mDbDeletedItemMapMutex);
            for(DbItemMap::iterator it = mDbDeletedItemMap.begin(); it != mDbDeletedItemMap.end(); ) {
                if(it->second == inItemImpl) {
                    // Increment the iterator, but use its pre-increment value for the erase
                    it->second->inCache(false);
                    mDbDeletedItemMap.erase(it++);
                } else {
                    it++;
                }
            }
        } // drop mDbDeletedItemMapMutex
    } catch(UnixError ue) {
        secnotice("keychain", "caught UnixError: %d %s", ue.unixError(), ue.what());
    } catch (CssmError cssme) {
        const char* errStr = cssmErrorString(cssme.error);
        secnotice("keychain", "caught CssmError: %d %s", (int) cssme.error, errStr);
    } catch (MacOSError mose) {
        secnotice("keychain", "MacOSError: %d", (int)mose.osStatus());
    } catch(...) {
        secnotice("keychain", "Unknown error");
    }
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
		KCEventNotifier::PostKeychainEvent((SecKeychainEvent) kSecKeychainLeftBatchModeEvent);
		mEventBuffer->clear();
	}
	else
	{
		KCEventNotifier::PostKeychainEvent((SecKeychainEvent) kSecKeychainEnteredBatchModeEvent);
	}
}
void
KeychainImpl::postEvent(SecKeychainEvent kcEvent, ItemImpl* item)
{
    postEvent(kcEvent, item, NULL);
}

void
KeychainImpl::postEvent(SecKeychainEvent kcEvent, ItemImpl* item, PrimaryKey pk)
{
	PrimaryKey primaryKey;

    if(pk.get()) {
        primaryKey = pk;
    } else {
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

void KeychainImpl::tickle() {
    if(!mSuppressTickle) {
        globals().storageManager.tickleKeychain(this);
    }
}


bool KeychainImpl::performKeychainUpgradeIfNeeded() {
    // Grab this keychain's mutex. This might not be sufficient, since the
    // keychain might have outstanding cursors. We'll grab the RWLock later if needed.
    StLock<Mutex>_(mMutex);

    if(!globals().integrityProtection()) {
        secnotice("integrity", "skipping upgrade for %s due to global integrity protection being disabled", mDb->name());
        return false;
    }

    // We need a CSP database for 'upgrade' to be meaningful
    if((mDb->dl()->subserviceMask() & CSSM_SERVICE_CSP) == 0) {
        return false;
    }

    // We only want to upgrade file-based Apple keychains. Check the GUID.
    if(mDb->dl()->guid() != gGuidAppleCSPDL) {
        secinfo("integrity", "skipping upgrade for %s due to guid mismatch\n", mDb->name());
        return false;
    }

    // If we've already attempted an upgrade on this keychain, don't bother again
    if(mAttemptedUpgrade) {
        return false;
    }

    // Don't upgrade the System root certificate keychain (to make old tp code happy)
    if(strncmp(mDb->name(), SYSTEM_ROOT_STORE_PATH, strlen(SYSTEM_ROOT_STORE_PATH)) == 0) {
        secinfo("integrity", "skipping upgrade for %s\n", mDb->name());
        return false;
    }

    uint32 dbBlobVersion = SecurityServer::DbBlob::version_MacOS_10_0;

    try {
        dbBlobVersion = mDb->dbBlobVersion();
    } catch (CssmError cssme) {
        if(cssme.error == CSSMERR_DL_DATASTORE_DOESNOT_EXIST) {
            // oh well! We tried to get the blob version of a database
            // that doesn't exist. It doesn't need migration, so do nothing.
            secnotice("integrity", "dbBlobVersion() failed for a non-existent database");
            return false;
        } else {
            // Some other error occurred. We can't upgrade this keychain, so fail.
            const char* errStr = cssmErrorString(cssme.error);
            secnotice("integrity", "dbBlobVersion() failed for a CssmError: %d %s", (int) cssme.error, errStr);
            return false;
        }
    } catch (...) {
        secnotice("integrity", "dbBlobVersion() failed for an unknown reason");
        return false;
    }



    // Check the location of this keychain
    string path = mDb->name();
    string keychainDbPath = StorageManager::makeKeychainDbFilename(path);

    bool inHomeLibraryKeychains = StorageManager::pathInHomeLibraryKeychains(path);

    string keychainDbSuffix = "-db";
    bool endsWithKeychainDb = (path.size() > keychainDbSuffix.size() && (0 == path.compare(path.size() - keychainDbSuffix.size(), keychainDbSuffix.size(), keychainDbSuffix)));

    bool isSystemKeychain = (0 == path.compare("/Library/Keychains/System.keychain"));

    bool result = false;

    if(inHomeLibraryKeychains && endsWithKeychainDb && dbBlobVersion == SecurityServer::DbBlob::version_MacOS_10_0) {
        // something has gone horribly wrong: an old-versioned keychain has a .keychain-db name. Rename it.
        string basePath = path;
        basePath.erase(basePath.end()-3, basePath.end());

        attemptKeychainRename(path, basePath, dbBlobVersion);

        // If we moved to a good path, we might still want to perform the upgrade. Update our variables.
        path = mDb->name();

        try {
            dbBlobVersion = mDb->dbBlobVersion();
        } catch (CssmError cssme) {
            const char* errStr = cssmErrorString(cssme.error);
            secnotice("integrity", "dbBlobVersion() after a rename failed for a CssmError: %d %s", (int) cssme.error, errStr);
            return false;
        } catch (...) {
            secnotice("integrity", "dbBlobVersion() failed for an unknown reason after a rename");
            return false;
        }

        endsWithKeychainDb = (path.size() > keychainDbSuffix.size() && (0 == path.compare(path.size() - keychainDbSuffix.size(), keychainDbSuffix.size(), keychainDbSuffix)));
        keychainDbPath = StorageManager::makeKeychainDbFilename(path);
        secnotice("integrity", "after rename, our database thinks that it is %s", path.c_str());
    }

    // Migrate an old keychain in ~/Library/Keychains
    if(inHomeLibraryKeychains && dbBlobVersion != SecurityServer::DbBlob::version_partition && !endsWithKeychainDb) {
        // We can only attempt to migrate an unlocked keychain.
        if(mDb->isLocked()) {
            // However, it's possible that while we weren't doing any keychain operations, someone upgraded the keychain,
            // and then locked it. No way around hitting the filesystem here: check for the existence of a new file and,
            // if no new file exists, quit.
            DLDbIdentifier mungedDLDbIdentifier = StorageManager::mungeDLDbIdentifier(mDb->dlDbIdentifier(), false);
            string mungedPath(mungedDLDbIdentifier.dbName());

            // If this matches the file we already have, skip the upgrade. Otherwise, continue.
            if(mungedPath == path) {
                secnotice("integrity", "skipping upgrade for locked keychain %s\n", mDb->name());
                return false;
            }
        }

        result = keychainMigration(path, dbBlobVersion, keychainDbPath, SecurityServer::DbBlob::version_partition);
    } else if(inHomeLibraryKeychains && dbBlobVersion == SecurityServer::DbBlob::version_partition && !endsWithKeychainDb) {
        // This is a new-style keychain with the wrong name, try to rename it
        attemptKeychainRename(path, keychainDbPath, dbBlobVersion);
        result = true;
    } else if(isSystemKeychain && dbBlobVersion == SecurityServer::DbBlob::version_partition) {
        // Try to "unupgrade" the system keychain, to clean up our old issues
        secnotice("integrity", "attempting downgrade for %s version %d (%d %d %d)", path.c_str(), dbBlobVersion, inHomeLibraryKeychains, endsWithKeychainDb, isSystemKeychain);

        // First step: acquire the credentials to allow for ACL modification
        SecurityServer::SystemKeychainKey skk(kSystemUnlockFile);
        if(skk.valid()) {
            // We've managed to read the key; now, create credentials using it
            CssmClient::Key systemKeychainMasterKey(csp(), skk.key(), true);
            CssmClient::AclFactory::MasterKeyUnlockCredentials creds(systemKeychainMasterKey, Allocator::standard(Allocator::sensitive));

            // Attempt the downgrade, using our master key as the ACL override
            result = keychainMigration(path, dbBlobVersion, path, SecurityServer::DbBlob::version_MacOS_10_0, creds.getAccessCredentials());
        } else {
            secnotice("integrity", "Couldn't read System.keychain key, skipping update");
        }
    } else {
        secinfo("integrity", "not attempting migration for %s version %d (%d %d %d)", path.c_str(), dbBlobVersion, inHomeLibraryKeychains, endsWithKeychainDb, isSystemKeychain);

        // Since we don't believe any migration needs to be done here, mark the
        // migration as "attempted" to short-circuit future checks.
        mAttemptedUpgrade = true;
    }

    // We might have changed our location on disk. Let StorageManager know.
    globals().storageManager.registerKeychainImpl(this);

    // if we attempted a migration, try to clean up leftover files from <rdar://problem/23950408> XARA backup have provided me with 12GB of login keychain copies
    if(result) {
        string pattern = path + "_*_backup";
        glob_t pglob = {};
        secnotice("integrity", "globbing for %s", pattern.c_str());
        int globresult = glob(pattern.c_str(), GLOB_MARK, NULL, &pglob);
        if(globresult == 0) {
            secnotice("integrity", "glob: %lu results", pglob.gl_pathc);
            if(pglob.gl_pathc > 10) {
                // There are more than 10 backup files, indicating a problem.
                // Delete all but one of them. Under rdar://23950408, they should all be identical.
                secnotice("integrity", "saving backup file: %s", pglob.gl_pathv[0]);
                for(int i = 1; i < pglob.gl_pathc; i++) {
                    secnotice("integrity", "cleaning up backup file: %s", pglob.gl_pathv[i]);
                    // ignore return code; this is a best-effort cleanup
                    unlink(pglob.gl_pathv[i]);
                }
            }

            struct stat st;
            bool pathExists = (::stat(path.c_str(), &st) == 0);
            bool keychainDbPathExists = (::stat(keychainDbPath.c_str(), &st) == 0);

            if(!pathExists && keychainDbPathExists && pglob.gl_pathc >= 1) {
                // We have a file at keychainDbPath, no file at path, and at least one backup keychain file.
                //
                // Move the backup file to path, to simulate the current  "split-world" view,
                // which copies from path to keychainDbPath, then modifies keychainDbPath.
                secnotice("integrity", "moving backup file %s to %s", pglob.gl_pathv[0], path.c_str());
                ::rename(pglob.gl_pathv[0], path.c_str());
            }
        }

        globfree(&pglob);
    }

    return result;
}

bool KeychainImpl::keychainMigration(const string oldPath, const uint32 dbBlobVersion, const string newPath, const uint32 newBlobVersion, const AccessCredentials *cred) {
    secnotice("integrity", "going to migrate %s at version %d to", oldPath.c_str(), dbBlobVersion);
    secnotice("integrity", "                 %s at version %d", newPath.c_str(), newBlobVersion);

    // We need to opportunistically perform the upgrade/reload dance.
    //
    // If the keychain is unlocked, try to upgrade it.
    // In either case, reload the database from disk.
    // We need this keychain's read/write lock.

    // Try to grab the keychain write lock.
    StReadWriteLock lock(mRWLock, StReadWriteLock::TryWrite);

    // If we didn't manage to grab the lock, there's readers out there
    // currently reading this keychain. Abort the upgrade.
    if(!lock.isLocked()) {
        secnotice("integrity", "couldn't get read-write lock, aborting upgrade");
        return false;
    }

    // Take the file lock on the existing database. We don't need to commit this txion, because we're not planning to
    // change the original keychain.
    FileLockTransaction fileLockmDb(mDb);

    // Let's reload this keychain to see if someone changed it on disk
    globals().storageManager.reloadKeychain(this);

    bool result = false;

    try {
        // We can only attempt an upgrade if the keychain is currently unlocked
        // There's a TOCTTOU issue here, but it's going to be rare in practice, and the upgrade will simply fail.
        if(!mDb->isLocked()) {
            secnotice("integrity", "have a plan to migrate database %s", mDb->name());
            // Database blob is out of date. Attempt a migration.
            uint32 convertedVersion = attemptKeychainMigration(oldPath, dbBlobVersion, newPath, newBlobVersion, cred);
            if(convertedVersion == newBlobVersion) {
                secnotice("integrity", "conversion succeeded");
                result = true;
            } else {
                secnotice("integrity", "conversion failed, keychain is still %d", convertedVersion);
            }
        } else {
            secnotice("integrity", "keychain is locked, can't upgrade");
        }
    } catch (CssmError cssme) {
        const char* errStr = cssmErrorString(cssme.error);
        secnotice("integrity", "caught CssmError: %d %s", (int) cssme.error, errStr);
    } catch (...) {
        // Something went wrong, but don't worry about it.
        secnotice("integrity", "caught unknown error");
    }

    // No matter if the migrator succeeded, we need to reload this keychain from disk.
    secnotice("integrity", "reloading keychain after migration");
    globals().storageManager.reloadKeychain(this);
    secnotice("integrity", "database %s is now version %d", mDb->name(), mDb->dbBlobVersion());

    return result;
}

// Make sure you have this keychain's mutex and write lock when you call this function!
uint32 KeychainImpl::attemptKeychainMigration(const string oldPath, const uint32 oldBlobVersion, const string newPath, const uint32 newBlobVersion, const AccessCredentials* cred) {
    if(mDb->dbBlobVersion() == newBlobVersion) {
        // Someone else upgraded this, hurray!
        secnotice("integrity", "reloaded keychain version %d, quitting", mDb->dbBlobVersion());
        return newBlobVersion;
    }

    mAttemptedUpgrade = true;
    uint32 newDbVersion = oldBlobVersion;

    if( (oldBlobVersion == SecurityServer::DbBlob::version_MacOS_10_0 && newBlobVersion == SecurityServer::DbBlob::version_partition) ||
        (oldBlobVersion == SecurityServer::DbBlob::version_partition && newBlobVersion == SecurityServer::DbBlob::version_MacOS_10_0 && cred != NULL)) {
        // Here's the upgrade outline:
        //
        //   1. Make a copy of the keychain with the new file path
        //   2. Open that keychain database.
        //   3. Recode it to use the new version.
        //   4. Notify the StorageManager that the DLDB identifier for this keychain has changed.
        //
        // If we're creating a new keychain file, on failure, try to delete the new file. Otherwise,
        // everyone will try to use it.

        secnotice("integrity", "attempting migration from version %d to %d", oldBlobVersion, newBlobVersion);

        Db db;
        bool newFile = (oldPath != newPath);

        try {
            DLDbIdentifier dldbi(dlDbIdentifier().ssuid(), newPath.c_str(), dlDbIdentifier().dbLocation());
            if(newFile) {
                secnotice("integrity", "creating a new keychain at %s", newPath.c_str());
                db = mDb->cloneTo(dldbi);
            } else {
                secnotice("integrity", "using old keychain at %s", newPath.c_str());
                db = mDb;
            }
            FileLockTransaction fileLockDb(db);

            if(newFile) {
                // since we're creating a completely new file, if this migration fails, delete the new file
                fileLockDb.setDeleteOnFailure();
            }

            // Let the upgrade begin.
            newDbVersion = db->recodeDbToVersion(newBlobVersion);
            if(newDbVersion != newBlobVersion) {
                // Recoding failed. Don't proceed.
                secnotice("integrity", "recodeDbToVersion failed, version is still %d", newDbVersion);
                return newDbVersion;
            }

            secnotice("integrity", "recoded db successfully, adding extra integrity");

            Keychain keychain(db);

            // Breaking abstraction, but what're you going to do?
            // Don't upgrade this keychain, since we just upgraded the DB
            // But the DB won't return any new data until the txion commits
            keychain->mAttemptedUpgrade = true;
            keychain->mSuppressTickle = true;

            SecItemClass classes[] = {kSecGenericPasswordItemClass,
                                      kSecInternetPasswordItemClass,
                                      kSecPublicKeyItemClass,
                                      kSecPrivateKeyItemClass,
                                      kSecSymmetricKeyItemClass};

            for(int i = 0; i < sizeof(classes) / sizeof(classes[0]); i++) {
                Item item;
                KCCursor kcc = keychain->createCursor(classes[i], NULL);

                // During recoding, we might have deleted some corrupt keys.
                // Because of this, we might have zombie SSGroup records left in
                // the database that have no matching key. Tell the KCCursor to
                // delete these if found.
                // This will also try to suppress any other invalid items.
                kcc->setDeleteInvalidRecords(true);

                while(kcc->next(item)) {
                    try {
                        if(newBlobVersion == SecurityServer::DbBlob::version_partition) {
                            // Force the item to set integrity. The keychain is confused about its version because it hasn't written to disk yet,
                            // but if we've reached this point, the keychain supports integrity.
                            item->setIntegrity(true);
                        } else if(newBlobVersion == SecurityServer::DbBlob::version_MacOS_10_0) {
                            // We're downgrading this keychain. Pass in whatever credentials our caller thinks will allow this ACL modification.
                            item->removeIntegrity(cred);
                        }
                    } catch(CssmError cssme) {
                        // During recoding, we might have deleted some corrupt keys. Because of this, we might have zombie SSGroup records left in
                        // the database that have no matching key. If we get a DL_RECORD_NOT_FOUND error, delete the matching item record.
                        if (cssme.osStatus() == CSSMERR_DL_RECORD_NOT_FOUND) {
                            secnotice("integrity", "deleting corrupt (Not Found) record");
                            keychain->deleteItem(item);
                        } else if(cssme.osStatus() == CSSMERR_CSP_INVALID_KEY) {
                            secnotice("integrity", "deleting corrupt key record");
                            keychain->deleteItem(item);
                        } else {
                            throw;
                        }
                    }
                }
            }

            // Tell securityd we're done with the upgrade, to re-enable all protections
            db->recodeFinished();

            // If we reach here, tell the file locks to commit the transaction and return the new blob version
            fileLockDb.success();

            secnotice("integrity", "success, returning version %d", newDbVersion);
            return newDbVersion;
        } catch(UnixError ue) {
            secnotice("integrity", "caught UnixError: %d %s", ue.unixError(), ue.what());
        } catch (CssmError cssme) {
            const char* errStr = cssmErrorString(cssme.error);
            secnotice("integrity", "caught CssmError: %d %s", (int) cssme.error, errStr);
        } catch (MacOSError mose) {
            secnotice("integrity", "MacOSError: %d", (int)mose.osStatus());
        } catch (const std::bad_cast & e) {
            secnotice("integrity", "***** bad cast: %s", e.what());
        } catch (...) {
            // We failed to migrate. We won't commit the transaction, so the blob on-disk stays the same.
            secnotice("integrity", "***** unknown error");
        }
    } else {
        secnotice("integrity", "no migration path for %s at version %d to", oldPath.c_str(), oldBlobVersion);
        secnotice("integrity", "                      %s at version %d", newPath.c_str(), newBlobVersion);
        return oldBlobVersion;
    }

    // If we reached here, the migration failed. Return the old version.
    return oldBlobVersion;
}

void KeychainImpl::attemptKeychainRename(const string oldPath, const string newPath, uint32 blobVersion) {
    secnotice("integrity", "attempting to rename keychain (%d) from %s to %s", blobVersion, oldPath.c_str(), newPath.c_str());

    // Take the file lock on this database, so other people won't try to move it before we do
    // NOTE: during a migration from a v256 to a v512 keychain, the db is first copied from the .keychain to the
    //       .keychain-db path. Other non-migrating processes, if they open the keychain, enter this function to
    //       try to move it back. These will attempt to take the .keychain-db file lock, but they will not succeed
    //       until the migration is finished. Once they acquire that, they might try to take the .keychain file lock.
    //       This is technically lock inversion, but deadlocks will not happen since the migrating process creates the
    //       .keychain-db file lock before creating the .keychain-db file, so other processes will not try to grab the
    //       .keychain-db lock in this function before the migrating process already has it.
    FileLockTransaction fileLockmDb(mDb);

    // first, check if someone renamed this keychain while we were grabbing the file lock
    globals().storageManager.reloadKeychain(this);

    uint32 dbBlobVersion = SecurityServer::DbBlob::version_MacOS_10_0;

    try {
        dbBlobVersion = mDb->dbBlobVersion();
    } catch (...) {
        secnotice("integrity", "dbBlobVersion() failed for an unknown reason while renaming, aborting rename");
        return;
    }

    if(dbBlobVersion != blobVersion) {
        secnotice("integrity", "database version changed while we were grabbing the file lock; aborting rename");
        return;
    }

    if(oldPath != mDb->name()) {
        secnotice("integrity", "database location changed while we were grabbing the file lock; aborting rename");
        return;
    }

    // we're still at the original location and version; go ahead and do the move
    globals().storageManager.rename(this, newPath.c_str());
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

bool KeychainImpl::hasIntegrityProtection() {
    StLock<Mutex>_(mMutex);

    // This keychain only supports integrity if there's a database attached, that database is an Apple CSPDL, and the blob version is high enough
    if(mDb && (mDb->dl()->guid() == gGuidAppleCSPDL)) {
        if(mDb->dbBlobVersion() >= SecurityServer::DbBlob::version_partition) {
            return true;
        } else {
            secnotice("integrity", "keychain blob version does not support integrity");
            return false;
        }
    } else {
        secnotice("integrity", "keychain guid does not support integrity");
        return false;
    }
}

