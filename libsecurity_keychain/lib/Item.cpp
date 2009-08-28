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
// Item.cpp
//

#include "Item.h"

#include "Certificate.h"
#include "KeyItem.h"
#include "ExtendedAttribute.h"

#include "Globals.h"
#include <security_cdsa_utilities/Schema.h>
#include "KCEventNotifier.h"
#include "cssmdatetime.h"
#include <security_cdsa_client/keychainacl.h>
#include <security_utilities/osxcode.h>
#include <security_utilities/trackingallocator.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/cssmapple.h>

#define SENDACCESSNOTIFICATIONS 1

//%%% schema indexes should be defined in Schema.h
#define APPLEDB_CSSM_PRINTNAME_ATTRIBUTE        1   /* schema index for label attribute of keys or certificates */
#define APPLEDB_GENERIC_PRINTNAME_ATTRIBUTE     7   /* schema index for label attribute of password items */
#define IS_PASSWORD_ITEM_CLASS(X)             ( (X) == kSecInternetPasswordItemClass || \
                                                (X) == kSecGenericPasswordItemClass || \
                                                (X) == kSecAppleSharePasswordItemClass ) ? 1 : 0

using namespace KeychainCore;
using namespace CSSMDateTimeUtils;

//
// ItemImpl
//

// NewItemImpl constructor
ItemImpl::ItemImpl(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data, bool dontDoAttributes)
	: mDbAttributes(new DbAttributes()),
	mKeychain(NULL),
	mDoNotEncrypt(false),
	mInCache(false),
	mMutex(Mutex::recursive)
{
	if (length && data)
		mData = new CssmDataContainer(data, length);

	mDbAttributes->recordType(Schema::recordTypeFor(itemClass));

	if (itemCreator)
		mDbAttributes->add(Schema::attributeInfo(kSecCreatorItemAttr), itemCreator);
}

ItemImpl::ItemImpl(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void* data)
	: mDbAttributes(new DbAttributes()),
	mKeychain(NULL),
	mDoNotEncrypt(false),
	mInCache(false),
	mMutex(Mutex::recursive)
{
	if (length && data)
		mData = new CssmDataContainer(data, length);


	mDbAttributes->recordType(Schema::recordTypeFor(itemClass));

	if(attrList) 
	{
		for(UInt32 i=0; i < attrList->count; i++)
		{
			mDbAttributes->add(Schema::attributeInfo(attrList->attr[i].tag), CssmData(attrList->attr[i].data,  attrList->attr[i].length));
		}
	}
}

// DbItemImpl constructor
ItemImpl::ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey, const DbUniqueRecord &uniqueId)
	: mUniqueId(uniqueId), mKeychain(keychain), mPrimaryKey(primaryKey),
	mDoNotEncrypt(false), mInCache(false),
	mMutex(Mutex::recursive)
{
}

// PrimaryKey ItemImpl constructor
ItemImpl::ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey)
	: mKeychain(keychain), mPrimaryKey(primaryKey), mDoNotEncrypt(false),
	mInCache(false),
	mMutex(Mutex::recursive)
{
}

ItemImpl* ItemImpl::make(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId)
{
	ItemImpl* ii = new ItemImpl(keychain, primaryKey, uniqueId);
	keychain->addItem(primaryKey, ii);
	return ii;
}



ItemImpl* ItemImpl::make(const Keychain &keychain, const PrimaryKey &primaryKey)
{
	ItemImpl* ii = new ItemImpl(keychain, primaryKey);
	keychain->addItem(primaryKey, ii);
	return ii;
}


	
// Constructor used when copying an item to a keychain.

ItemImpl::ItemImpl(ItemImpl &item) :
	mData(item.modifiedData() ? NULL : new CssmDataContainer()),
	mDbAttributes(new DbAttributes()),
	mKeychain(NULL),
	mDoNotEncrypt(false),
	mInCache(false),
	mMutex(Mutex::recursive)
{
	mDbAttributes->recordType(item.recordType());
	CSSM_DB_RECORD_ATTRIBUTE_INFO *schemaAttributes = NULL;

	if (item.mKeychain) {
		// get the entire source item from its keychain. This requires figuring
		// out the schema for the item based on its record type.
		
		for (uint32 i = 0; i < Schema::DBInfo.NumberOfRecordTypes; i++)
			if (item.recordType() == Schema::DBInfo.RecordAttributeNames[i].DataRecordType) {
				schemaAttributes = &Schema::DBInfo.RecordAttributeNames[i];
				break;
			}
				
		if (schemaAttributes == NULL)
			// the source item is invalid
			MacOSError::throwMe(errSecInvalidItemRef);

		for (uint32 i = 0; i < schemaAttributes->NumberOfAttributes; i++)
			mDbAttributes->add(schemaAttributes->AttributeInfo[i]);

        item.getContent(mDbAttributes.get(), mData.get());
	}

    // @@@ We don't deal with modified attributes.
	
	if (item.modifiedData())
		// the copied data comes from the source item
		mData = new CssmDataContainer(item.modifiedData()->Data,
			item.modifiedData()->Length);
}

ItemImpl::~ItemImpl()
{
}



void
ItemImpl::aboutToDestruct()
{
	if (mKeychain && *mPrimaryKey)
		mKeychain->removeItem(mPrimaryKey, this);
}



void
ItemImpl::didModify()
{
	StLock<Mutex>_(mMutex);
	mData = NULL;
	mDbAttributes.reset(NULL);
}

const CSSM_DATA &
ItemImpl::defaultAttributeValue(const CSSM_DB_ATTRIBUTE_INFO &info)
{
	static const uint32 zeroInt = 0;
	static const double zeroDouble = 0.0;
	static const char timeBytes[] = "20010101000000Z";

	static const CSSM_DATA defaultFourBytes = { 4, (uint8 *) &zeroInt };
	static const CSSM_DATA defaultEightBytes = { 8, (uint8 *) &zeroDouble };
	static const CSSM_DATA defaultTime = { 16, (uint8 *) timeBytes };
	static const CSSM_DATA defaultZeroBytes = { 0, NULL };

	switch (info.AttributeFormat)
	{
		case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
		case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
			return defaultFourBytes;
			
		case CSSM_DB_ATTRIBUTE_FORMAT_REAL:
			return defaultEightBytes;
			
		case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
			return defaultTime;
			
		default:
			return defaultZeroBytes;
	}
}



PrimaryKey ItemImpl::addWithCopyInfo (Keychain &keychain, bool isCopy)
{
	StLock<Mutex>_(mMutex);
	// If we already have a Keychain we can't be added.
	if (mKeychain)
		MacOSError::throwMe(errSecDuplicateItem);

    // If we don't have any attributes we can't be added.
    // (this might occur if attempting to add the item twice, since our attributes
    // and data are set to NULL at the end of this function.)
    if (!mDbAttributes.get())
		MacOSError::throwMe(errSecDuplicateItem);

	CSSM_DB_RECORDTYPE recordType = mDbAttributes->recordType();

	// update the creation and update dates on the new item
	if (!isCopy)
	{
		KeychainSchema schema = keychain->keychainSchema();
		SInt64 date;
		GetCurrentMacLongDateTime(date);
		if (schema->hasAttribute(recordType, kSecCreationDateItemAttr))
		{
			setAttribute(schema->attributeInfoFor(recordType, kSecCreationDateItemAttr), date);
		}

		if (schema->hasAttribute(recordType, kSecModDateItemAttr))
		{
			setAttribute(schema->attributeInfoFor(recordType, kSecModDateItemAttr), date);
		}
	}
	
    // If the label (PrintName) attribute isn't specified, set a default label.
    if (!mDoNotEncrypt && !mDbAttributes->find(Schema::attributeInfo(kSecLabelItemAttr)))
    {
		// if doNotEncrypt was set all of the attributes are wrapped in the data blob.  Don't calculate here.
        CssmDbAttributeData *label = NULL;
        switch (recordType)
        {
            case CSSM_DL_DB_RECORD_GENERIC_PASSWORD:
                label = mDbAttributes->find(Schema::attributeInfo(kSecServiceItemAttr));
                break;

            case CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD:
            case CSSM_DL_DB_RECORD_INTERNET_PASSWORD:
                label = mDbAttributes->find(Schema::attributeInfo(kSecServerItemAttr));
                // if AppleShare server name wasn't specified, try the server address
                if (!label) label = mDbAttributes->find(Schema::attributeInfo(kSecAddressItemAttr));
                break;

            default:
                break;
        }
        // if all else fails, use the account name.
        if (!label)
			label = mDbAttributes->find(Schema::attributeInfo(kSecAccountItemAttr));

        if (label && label->size())
            setAttribute (Schema::attributeInfo(kSecLabelItemAttr), label->at<CssmData>(0));
    }

	// get the attributes that are part of the primary key
	const CssmAutoDbRecordAttributeInfo &primaryKeyInfos =
		keychain->primaryKeyInfosFor(recordType);

	// make sure each primary key element has a value in the item, otherwise
	// the database will complain. we make a set of the provided attribute infos
	// to avoid O(N^2) behavior.

	DbAttributes *attributes = mDbAttributes.get();
	typedef set<CssmDbAttributeInfo> InfoSet;
	InfoSet infoSet;

	if (!mDoNotEncrypt)
	{
		// make a set of all the attributes in the key
		for (uint32 i = 0; i < attributes->size(); i++)
			infoSet.insert(attributes->at(i).Info);

		for (uint32 i = 0; i < primaryKeyInfos.size(); i++) { // check to make sure all required attributes are in the key
			InfoSet::const_iterator it = infoSet.find(primaryKeyInfos.at(i));

			if (it == infoSet.end()) { // not in the key?  add the default
				// we need to add a default value to the item attributes
				attributes->add(primaryKeyInfos.at(i), defaultAttributeValue(primaryKeyInfos.at(i)));
			}
		}
	}
	
	Db db(keychain->database());
	if (mDoNotEncrypt)
	{
		mUniqueId = db->insertWithoutEncryption (recordType, NULL, mData.get());
	}
	else if (useSecureStorage(db))
	{
		// Add the item to the secure storage db
		SSDbImpl* impl = dynamic_cast<SSDbImpl *>(&(*db));
		if (impl == NULL)
		{
			CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
		}
		
		SSDb ssDb(impl);

		TrackingAllocator allocator(Allocator::standard());
                
		// hhs replaced with the new aclFactory class
		AclFactory aclFactory;
		const AccessCredentials *nullCred = aclFactory.nullCred();

		SecPointer<Access> access = mAccess;
		if (!access) {
			// create default access controls for the new item
			CssmDbAttributeData *data = mDbAttributes->find(Schema::attributeInfo(kSecLabelItemAttr));
			string printName = data ? CssmData::overlay(data->Value[0]).toString() : "keychain item";
			access = new Access(printName);
			
			// special case for "iTools" password - allow anyone to decrypt the item
			if (recordType == CSSM_DL_DB_RECORD_GENERIC_PASSWORD)
			{
				CssmDbAttributeData *data = mDbAttributes->find(Schema::attributeInfo(kSecServiceItemAttr));
				if (data && data->Value[0].Length == 6 && !memcmp("iTools", data->Value[0].Data, 6))
				{
					typedef vector<SecPointer<ACL> > AclSet;
					AclSet acls;
					access->findAclsForRight(CSSM_ACL_AUTHORIZATION_DECRYPT, acls);
					for (AclSet::const_iterator it = acls.begin(); it != acls.end(); it++)
						(*it)->form(ACL::allowAllForm);
				}
			}
		}

		// Get the handle of the DL underlying this CSPDL.
		CSSM_DL_DB_HANDLE dldbh;
		db->passThrough(CSSM_APPLECSPDL_DB_GET_HANDLE, NULL,
			reinterpret_cast<void **>(&dldbh));

		// Turn off autocommit on the underlying DL and remember the old state.
		CSSM_BOOL autoCommit = CSSM_TRUE;
		ObjectImpl::check(CSSM_DL_PassThrough(dldbh,
			CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
			0, reinterpret_cast<void **>(&autoCommit)));

		try
		{
			// Create a new SSGroup with temporary access controls
			Access::Maker maker;
			ResourceControlContext prototype;
			maker.initialOwner(prototype, nullCred);
			SSGroup ssGroup(ssDb, &prototype);
			
			try
			{
				// Insert the record using the newly created group.
				mUniqueId = ssDb->insert(recordType, mDbAttributes.get(),
										 mData.get(), ssGroup, maker.cred());
			}
			catch(...)
			{
				ssGroup->deleteKey(nullCred);
				throw;
			}

			// now finalize the access controls on the group
			access->setAccess(*ssGroup, maker);
			mAccess = NULL;	// use them and lose them
			if (autoCommit)
			{
				// autoCommit was on so commit now that we are done and turn
				// it back on.
				ObjectImpl::check(CSSM_DL_PassThrough(dldbh,
					CSSM_APPLEFILEDL_COMMIT, NULL, NULL));
				CSSM_DL_PassThrough(dldbh, CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
					reinterpret_cast<const void *>(autoCommit), NULL);
			}
		}
		catch (...)
		{
			if (autoCommit)
			{
				// autoCommit was off so rollback since we failed and turn
				// autoCommit back on.
				CSSM_DL_PassThrough(dldbh, CSSM_APPLEFILEDL_ROLLBACK, NULL, NULL);
				CSSM_DL_PassThrough(dldbh, CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
					reinterpret_cast<const void *>(autoCommit), NULL);
			}
			throw;
		}
	}
	else
	{
		// add the item to the (regular) db
		mUniqueId = db->insert(recordType, mDbAttributes.get(), mData.get());
	}

	mPrimaryKey = keychain->makePrimaryKey(recordType, mUniqueId);
    mKeychain = keychain;

	// Forget our data and attributes.
	mData = NULL;
	mDbAttributes.reset(NULL);

	return mPrimaryKey;
}



PrimaryKey
ItemImpl::add (Keychain &keychain)
{
	return addWithCopyInfo (keychain, false);
}



Item
ItemImpl::copyTo(const Keychain &keychain, Access *newAccess)
{
	StLock<Mutex>_(mMutex);
	Item item(*this);
	if (newAccess)
		item->setAccess(newAccess);
	else
	{
		/* Attempt to copy the access from the current item to the newly created one. */
		SSGroup myGroup = group();
		if (myGroup)
		{
			SecPointer<Access> access = new Access(*myGroup);
			item->setAccess(access);
		}
	}

	keychain->addCopy(item);
	return item;
}

void
ItemImpl::update()
{
	StLock<Mutex>_(mMutex);
	if (!mKeychain)
		MacOSError::throwMe(errSecNoSuchKeychain); 
		
	// Don't update if nothing changed.
	if (!isModified())
		return;

	CSSM_DB_RECORDTYPE aRecordType = recordType();
	KeychainSchema schema = mKeychain->keychainSchema();

	// Update the modification date on the item if there is a mod date attribute.
	if (schema->hasAttribute(aRecordType, kSecModDateItemAttr))
	{
		SInt64 date;
		GetCurrentMacLongDateTime(date);
		setAttribute(schema->attributeInfoFor(aRecordType, kSecModDateItemAttr), date);
	}

	// Make sure that we have mUniqueId
	dbUniqueRecord();
	Db db(mUniqueId->database());
	if (mDoNotEncrypt)
	{
		CSSM_DB_RECORD_ATTRIBUTE_DATA attrData;
		memset (&attrData, 0, sizeof (attrData));
		attrData.DataRecordType = aRecordType;
		
		mUniqueId->modifyWithoutEncryption(aRecordType,
										   &attrData,
										   mData.get(),
										   CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	}
	else if (useSecureStorage(db))
	{
		// Add the item to the secure storage db
		SSDbUniqueRecordImpl * impl = dynamic_cast<SSDbUniqueRecordImpl *>(&(*mUniqueId));
		if (impl == NULL)
		{
			CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
		}
		
		SSDbUniqueRecord ssUniqueId(impl);

		// @@@ Share this instance
		const AccessCredentials *autoPrompt = globals().itemCredentials();


		// Only call this is user interaction is enabled.
		ssUniqueId->modify(aRecordType,
						   mDbAttributes.get(),
						   mData.get(),
						   CSSM_DB_MODIFY_ATTRIBUTE_REPLACE,
						   autoPrompt);
	}
	else
	{
		mUniqueId->modify(aRecordType,
						  mDbAttributes.get(),
						  mData.get(),
						  CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	}

	if (!mDoNotEncrypt)
	{
		PrimaryKey oldPK = mPrimaryKey;
		mPrimaryKey = mKeychain->makePrimaryKey(aRecordType, mUniqueId);

		// Forget our data and attributes.
		mData = NULL;
		mDbAttributes.reset(NULL);

		// Let the Keychain update what it needs to.
		mKeychain->didUpdate(this, oldPK, mPrimaryKey);
	}
}

void
ItemImpl::getClass(SecKeychainAttribute &attr, UInt32 *actualLength)
{
	StLock<Mutex>_(mMutex);
	if (actualLength)
		*actualLength = sizeof(SecItemClass);

	if (attr.length < sizeof(SecItemClass))
		MacOSError::throwMe(errSecBufferTooSmall);

	SecItemClass aClass = Schema::itemClassFor(recordType());
	memcpy(attr.data, &aClass, sizeof(SecItemClass));
}

void
ItemImpl::setAttribute(SecKeychainAttribute& attr)
{
	StLock<Mutex>_(mMutex);
    setAttribute(Schema::attributeInfo(attr.tag), CssmData(attr.data, attr.length));
}

CSSM_DB_RECORDTYPE
ItemImpl::recordType()
{
	StLock<Mutex>_(mMutex);
	if (mDbAttributes.get())
		return mDbAttributes->recordType();

	return mPrimaryKey->recordType();
}

const DbAttributes *
ItemImpl::modifiedAttributes()
{
	StLock<Mutex>_(mMutex);
	return mDbAttributes.get();
}

const CssmData *
ItemImpl::modifiedData()
{
	StLock<Mutex>_(mMutex);
	return mData.get();
}

void
ItemImpl::setData(UInt32 length,const void *data)
{
	StLock<Mutex>_(mMutex);
	mData = new CssmDataContainer(data, length);
}

void
ItemImpl::setAccess(Access *newAccess)
{
	StLock<Mutex>_(mMutex);
	mAccess = newAccess;
}

CssmClient::DbUniqueRecord
ItemImpl::dbUniqueRecord()
{
	StLock<Mutex>_(mMutex);
    if (!isPersistent()) // is there no database attached?
    {
        MacOSError::throwMe(errSecNotAvailable);
    }

	if (!mUniqueId)
	{
		DbCursor cursor(mPrimaryKey->createCursor(mKeychain));
		if (!cursor->next(NULL, NULL, mUniqueId))
			MacOSError::throwMe(errSecInvalidItemRef);
	}

	return mUniqueId;
}

PrimaryKey
ItemImpl::primaryKey()
{
	return mPrimaryKey;
}

bool
ItemImpl::isPersistent()
{
	return mKeychain;
}

bool
ItemImpl::isModified()
{
	StLock<Mutex>_(mMutex);
	return mData.get() || mDbAttributes.get();
}

Keychain
ItemImpl::keychain()
{
	return mKeychain;
}

bool
ItemImpl::operator < (const ItemImpl &other)
{
	if (mData && *mData)
	{
		// Pointer compare
		return this < &other;
	}

	return mPrimaryKey < other.mPrimaryKey;
}

void
ItemImpl::setAttribute(const CssmDbAttributeInfo &info, const CssmPolyData &data)
{
	StLock<Mutex>_(mMutex);
	if (!mDbAttributes.get())
	{
		mDbAttributes.reset(new DbAttributes());
		mDbAttributes->recordType(mPrimaryKey->recordType());
	}

	uint32 length = data.Length;
	const void *buf = reinterpret_cast<const void *>(data.Data);
    uint8 timeString[16];

    // XXX This code is duplicated in KCCursorImpl::KCCursorImpl()
    // Convert a 4 or 8 byte TIME_DATE to a CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE
    // style attribute value.
    if (info.format() == CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE)
    {
        if (length == sizeof(UInt32))
        {
            MacSecondsToTimeString(*reinterpret_cast<const UInt32 *>(buf), 16, &timeString);
            buf = &timeString;
            length = 16;
        }
        else if (length == sizeof(SInt64))
        {
            MacLongDateTimeToTimeString(*reinterpret_cast<const SInt64 *>(buf), 16, &timeString);
            buf = &timeString;
            length = 16;
        }
    }

	mDbAttributes->add(info, CssmData(const_cast<void*>(buf), length));
}

void
ItemImpl::modifyContent(const SecKeychainAttributeList *attrList, UInt32 dataLength, const void *inData)
{
	StLock<Mutex>_(mMutex);
	if (!mDbAttributes.get())
	{
		mDbAttributes.reset(new DbAttributes());
		mDbAttributes->recordType(mPrimaryKey->recordType());
	}

	if(attrList) // optional
	{
		for(UInt32 ix=0; ix < attrList->count; ix++)
		{
			mDbAttributes->add(Schema::attributeInfo(attrList->attr[ix].tag), CssmData(attrList->attr[ix].data,  attrList->attr[ix].length));
		}
	}
	
	if(inData)
	{
		mData = new CssmDataContainer(inData, dataLength);
	}
	
	update();
}

void
ItemImpl::getContent(SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData)
{
	StLock<Mutex>_(mMutex);
    // If the data hasn't been set we can't return it.
    if (!mKeychain && outData)
    {
		CssmData *data = mData.get();
		if (!data)
			MacOSError::throwMe(errSecDataNotAvailable);
    }
    // TODO: need to check and make sure attrs are valid and handle error condition


    if (itemClass)
		*itemClass = Schema::itemClassFor(recordType());
    
    bool getDataFromDatabase = mKeychain && mPrimaryKey;
    if (getDataFromDatabase) // are we attached to a database?
    {
        dbUniqueRecord();

		// get the number of attributes requested by the caller
		UInt32 attrCount = attrList ? attrList->count : 0;
    
        // make a DBAttributes structure and populate it
        DbAttributes dbAttributes(mUniqueId->database(), attrCount);
        for (UInt32 ix = 0; ix < attrCount; ++ix)
        {
            dbAttributes.add(Schema::attributeInfo(attrList->attr[ix].tag));
        }
        
        // request the data from the database (since we are a reference "item" and the data is really stored there)
        CssmDataContainer itemData;
		getContent(&dbAttributes, outData ? &itemData : NULL);

        // retrieve the data from result
        for (UInt32 ix = 0; ix < attrCount; ++ix)
        {
            if (dbAttributes.at(ix).NumberOfValues > 0)
            {
                attrList->attr[ix].data = dbAttributes.at(ix).Value[0].Data;	
                attrList->attr[ix].length = dbAttributes.at(ix).Value[0].Length;
    
                // We don't want the data released, it is up the client
                dbAttributes.at(ix).Value[0].Data = NULL;
                dbAttributes.at(ix).Value[0].Length = 0;
            }
            else
            {
                attrList->attr[ix].data = NULL;	
                attrList->attr[ix].length = 0;
            }
        }

		// clean up
		if (outData)
		{
			*outData=itemData.data();
			itemData.Data = NULL;
			
			if (length)
				*length=itemData.length();
			itemData.Length = 0;
		}
    }
    else
    {
		getLocalContent(attrList, length, outData);
	}

	// Inform anyone interested that we are doing this
#if SENDACCESSNOTIFICATIONS
    if (outData)
    {
		secdebug("kcnotify", "ItemImpl::getContent(%p, %p, %p, %p) retrieved content",
			itemClass, attrList, length, outData);

        KCEventNotifier::PostKeychainEvent(kSecDataAccessEvent, mKeychain, this);
    }
#endif
}

void
ItemImpl::freeContent(SecKeychainAttributeList *attrList, void *data)
{
    Allocator &allocator = Allocator::standard(); // @@@ This might not match the one used originally
    if (data)
		allocator.free(data);

    UInt32 attrCount = attrList ? attrList->count : 0;
    for (UInt32 ix = 0; ix < attrCount; ++ix)
    {
        allocator.free(attrList->attr[ix].data);
        attrList->attr[ix].data = NULL;
    }
}

void
ItemImpl::modifyAttributesAndData(const SecKeychainAttributeList *attrList, UInt32 dataLength, const void *inData)
{
	StLock<Mutex>_(mMutex);
	if (!mKeychain)
		MacOSError::throwMe(errSecNoSuchKeychain);

	if (!mDoNotEncrypt)
	{
		if (!mDbAttributes.get())
		{
			mDbAttributes.reset(new DbAttributes());
			mDbAttributes->recordType(mPrimaryKey->recordType());
		}

		CSSM_DB_RECORDTYPE recordType = mDbAttributes->recordType();
		UInt32 attrCount = attrList ? attrList->count : 0;
		for (UInt32 ix = 0; ix < attrCount; ix++)
		{
            SecKeychainAttrType attrTag = attrList->attr[ix].tag;

            if (attrTag == kSecLabelItemAttr)
            {
                // must remap a caller-supplied label attribute tag for password items, since it isn't in the schema
                if (IS_PASSWORD_ITEM_CLASS( Schema::itemClassFor(recordType) ))
                    attrTag = APPLEDB_GENERIC_PRINTNAME_ATTRIBUTE;
            }

            CssmDbAttributeInfo info=mKeychain->attributeInfoFor(recordType, attrTag);
							
			if (attrList->attr[ix].length || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_STRING  || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_BLOB
			 || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_STRING  || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM
			 || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32)
				mDbAttributes->add(info, CssmData(attrList->attr[ix].data, attrList->attr[ix].length));
			else
				mDbAttributes->add(info);
		}
	}
	
	if(inData)
	{
		mData = new CssmDataContainer(inData, dataLength);
	}
	
	update();
}

void
ItemImpl::getAttributesAndData(SecKeychainAttributeInfo *info, SecItemClass *itemClass,
							   SecKeychainAttributeList **attrList, UInt32 *length, void **outData)
{
	StLock<Mutex>_(mMutex);
	// If the data hasn't been set we can't return it.
	if (!mKeychain && outData)
	{
		CssmData *data = mData.get();
		if (!data)
			MacOSError::throwMe(errSecDataNotAvailable);
	}
	// TODO: need to check and make sure attrs are valid and handle error condition

    SecItemClass myItemClass = Schema::itemClassFor(recordType());
	if (itemClass)
		*itemClass = myItemClass;

	// @@@ This call won't work for floating items (like certificates).
	dbUniqueRecord();

    UInt32 attrCount = info ? info->count : 0;
	DbAttributes dbAttributes(mUniqueId->database(), attrCount);
    for (UInt32 ix = 0; ix < attrCount; ix++)
	{
		CssmDbAttributeData &record = dbAttributes.add();
		record.Info.AttributeNameFormat=CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
		record.Info.Label.AttributeID=info->tag[ix];
    
        if (record.Info.Label.AttributeID == kSecLabelItemAttr)
        {
            // must remap a caller-supplied label attribute tag for password items, since it isn't in the schema
            if (IS_PASSWORD_ITEM_CLASS( myItemClass ))
                record.Info.Label.AttributeID = APPLEDB_GENERIC_PRINTNAME_ATTRIBUTE;
        }
	}

	CssmDataContainer itemData;
    getContent(&dbAttributes, outData ? &itemData : NULL);

	if (info && attrList)
	{
		SecKeychainAttributeList *theList=reinterpret_cast<SecKeychainAttributeList *>(malloc(sizeof(SecKeychainAttributeList)));
		SecKeychainAttribute *attr=reinterpret_cast<SecKeychainAttribute *>(malloc(sizeof(SecKeychainAttribute)*attrCount));
		theList->count=attrCount;
		theList->attr=attr;
	
		for (UInt32 ix = 0; ix < attrCount; ++ix)
		{
			attr[ix].tag=info->tag[ix];
        
			if (dbAttributes.at(ix).NumberOfValues > 0)
			{
				attr[ix].data = dbAttributes.at(ix).Value[0].Data;	
				attr[ix].length = dbAttributes.at(ix).Value[0].Length;
	
				// We don't want the data released, it is up the client
				dbAttributes.at(ix).Value[0].Data = NULL;
				dbAttributes.at(ix).Value[0].Length = 0;
			}
			else
			{
				attr[ix].data = NULL;	
				attr[ix].length = 0;
			}
		}
		*attrList=theList;
	}

	if (outData)
	{
		*outData=itemData.data();
		itemData.Data=NULL;
		
		if (length) *length=itemData.length();
		itemData.Length=0;
				
#if SENDACCESSNOTIFICATIONS
		secdebug("kcnotify", "ItemImpl::getAttributesAndData(%p, %p, %p, %p, %p) retrieved data",
			info, itemClass, attrList, length, outData);

		KCEventNotifier::PostKeychainEvent(kSecDataAccessEvent, mKeychain, this);
#endif
	}
	
}

void
ItemImpl::freeAttributesAndData(SecKeychainAttributeList *attrList, void *data)
{
	Allocator &allocator = Allocator::standard(); // @@@ This might not match the one used originally

	if (data)
		allocator.free(data);

	if (attrList)
	{
		for (UInt32 ix = 0; ix < attrList->count; ++ix)
		{
			allocator.free(attrList->attr[ix].data);
		}
		free(attrList->attr);
		free(attrList);
	}
}

void
ItemImpl::getAttribute(SecKeychainAttribute& attr, UInt32 *actualLength)
{
	StLock<Mutex>_(mMutex);
	if (attr.tag == kSecClassItemAttr)
		return getClass(attr, actualLength);

	if (mDbAttributes.get())
	{
		CssmDbAttributeData *data = mDbAttributes->find(Schema::attributeInfo(attr.tag));
		if (data)
		{
			getAttributeFrom(data, attr, actualLength);
			return;
		}
	}

	if (!mKeychain)
		MacOSError::throwMe(errSecNoSuchAttr);
		
	dbUniqueRecord();
	DbAttributes dbAttributes(mUniqueId->database(), 1);
	dbAttributes.add(Schema::attributeInfo(attr.tag));
	mUniqueId->get(&dbAttributes, NULL);
	getAttributeFrom(&dbAttributes.at(0), attr, actualLength);
}

void
ItemImpl::getAttributeFrom(CssmDbAttributeData *data, SecKeychainAttribute &attr, UInt32 *actualLength)
{
	StLock<Mutex>_(mMutex);
    static const uint32 zero = 0;
    uint32 length;
    const void *buf = NULL;

    // Temporary storage for buf.
    sint64 macLDT;
    uint32 macSeconds;
    sint16 svalue16;
    uint16 uvalue16;
    sint8 svalue8;
    uint8 uvalue8;

	if (!data)
        length = 0;
    else if (data->size() < 1) // Attribute has no values.
    {
        if (data->format() == CSSM_DB_ATTRIBUTE_FORMAT_SINT32
            || data->format() == CSSM_DB_ATTRIBUTE_FORMAT_UINT32)
        {
            length = sizeof(zero);
            buf = &zero;
        }
        else if (CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE)
            length = 0; // Should we throw here?
        else // All other formats
            length = 0;
	}
    else // Get the first value
    {
        length = data->Value[0].Length;
        buf = data->Value[0].Data;

        if (data->format() == CSSM_DB_ATTRIBUTE_FORMAT_SINT32)
        {
            if (attr.length == sizeof(sint8))
            {
                length = attr.length;
                svalue8 = sint8(*reinterpret_cast<const sint32 *>(buf));
                buf = &svalue8;
            }
            else if (attr.length == sizeof(sint16))
            {
                length = attr.length;
                svalue16 = sint16(*reinterpret_cast<const sint32 *>(buf));
                buf = &svalue16;
            }
        }
        else if (data->format() == CSSM_DB_ATTRIBUTE_FORMAT_UINT32)
        {
            if (attr.length == sizeof(uint8))
            {
                length = attr.length;
                uvalue8 = uint8(*reinterpret_cast<const uint32 *>(buf));
                buf = &uvalue8;
            }
            else if (attr.length == sizeof(uint16))
            {
                length = attr.length;
                uvalue16 = uint16(*reinterpret_cast<const uint32 *>(buf));
                buf = &uvalue16;
            }
        }
        else if (data->format() == CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE)
        {
            if (attr.length == sizeof(uint32))
            {
                TimeStringToMacSeconds(data->Value[0], macSeconds);
                buf = &macSeconds;
                length = attr.length;
            }
            else if (attr.length == sizeof(sint64))
            {
                TimeStringToMacLongDateTime(data->Value[0], macLDT);
                buf = &macLDT;
                length = attr.length;
            }
        }
    }

	if (actualLength)
		*actualLength = length;

    if (length)
    {
        if (attr.length < length)
			MacOSError::throwMe(errSecBufferTooSmall);

        memcpy(attr.data, buf, length);
    }
}

void
ItemImpl::getData(CssmDataContainer& outData)
{
	StLock<Mutex>_(mMutex);
	if (!mKeychain)
	{
		CssmData *data = mData.get();
		// If the data hasn't been set we can't return it.
		if (!data)
			MacOSError::throwMe(errSecDataNotAvailable);

		outData = *data;
		return;
	}

    getContent(NULL, &outData);

#if SENDACCESSNOTIFICATIONS
    secdebug("kcnotify", "ItemImpl::getData retrieved data");

	//%%%<might> be done elsewhere, but here is good for now
	KCEventNotifier::PostKeychainEvent(kSecDataAccessEvent, mKeychain, this);
#endif
}

SSGroup
ItemImpl::group()
{
	StLock<Mutex>_(mMutex);
	SSGroup group;
	if (!!mUniqueId)
	{
		Db db(mKeychain->database());
		if (useSecureStorage(db))
		{
			group = safer_cast<SSDbUniqueRecordImpl &>(*mUniqueId).group();
		}
	}

	return group;
}

void ItemImpl::getLocalContent(SecKeychainAttributeList *attributeList, UInt32 *outLength, void **outData)
{
	StLock<Mutex>_(mMutex);
	willRead();
    Allocator &allocator = Allocator::standard(); // @@@ This might not match the one used originally
	if (outData)
	{
		CssmData *data = mData.get();
		if (!data)
			MacOSError::throwMe(errSecDataNotAvailable);

		// Copy the data out of our internal cached copy.
		uint32 length = data->Length;
		*outData = allocator.malloc(length);
		memcpy(*outData, data->Data, length);
		if (outLength)
			*outLength = length;
	}

	if (attributeList)
	{
		if (!mDbAttributes.get())
			MacOSError::throwMe(errSecDataNotAvailable);

		// Pull attributes out of a "floating" item, i.e. one that isn't attached to a database
		for (UInt32 ix = 0; ix < attributeList->count; ++ix)
		{
			SecKeychainAttribute &attribute = attributeList->attr[ix];
			CssmDbAttributeData *data = mDbAttributes->find(Schema::attributeInfo(attribute.tag));
			if (data && data->NumberOfValues > 0)
			{
				// Copy the data out of our internal cached copy.
				uint32 length = data->Value[0].Length;
				attribute.data = allocator.malloc(length);
				memcpy(attribute.data, data->Value[0].Data, length);
				attribute.length = length;
			}
			else
			{
				attribute.length = 0;
				attribute.data = NULL;
			}
		}
	}
}

void
ItemImpl::getContent(DbAttributes *dbAttributes, CssmDataContainer *itemData)
{
	StLock<Mutex>_(mMutex);
    // Make sure mUniqueId is set.
    dbUniqueRecord();
    if (itemData)
    {
		Db db(mUniqueId->database());
		if (mDoNotEncrypt)
		{
			mUniqueId->getWithoutEncryption (dbAttributes, itemData);
			return;
		}
		if (useSecureStorage(db))
		{
			SSDbUniqueRecordImpl* impl = dynamic_cast<SSDbUniqueRecordImpl *>(&(*mUniqueId));
			if (impl == NULL)
			{
				CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
			}
			
			SSDbUniqueRecord ssUniqueId(impl);
			const AccessCredentials *autoPrompt = globals().itemCredentials();
			ssUniqueId->get(dbAttributes, itemData, autoPrompt);
			return;
		}
	}

    mUniqueId->get(dbAttributes, itemData); 
}

bool
ItemImpl::useSecureStorage(const Db &db)
{
	StLock<Mutex>_(mMutex);
	switch (recordType())
	{
	case CSSM_DL_DB_RECORD_GENERIC_PASSWORD:
	case CSSM_DL_DB_RECORD_INTERNET_PASSWORD:
	case CSSM_DL_DB_RECORD_APPLESHARE_PASSWORD:
		if (db->dl()->subserviceMask() & CSSM_SERVICE_CSP)
			return true;
		break;
	default:
		break;
	}
	return false;
}

void ItemImpl::willRead()
{
}


void ItemImpl::copyPersistentReference(CFDataRef &outDataRef)
{
	StLock<Mutex>_(mMutex);
    // item must be in a keychain and have a primary key to be persistent
    if (!mKeychain || !mPrimaryKey) {
        MacOSError::throwMe(errSecItemNotFound);
    }
    DLDbIdentifier dlDbIdentifier = mKeychain->dlDbIdentifier();
    DLDbIdentifier newDlDbIdentifier(dlDbIdentifier.ssuid(),
        DLDbListCFPref::AbbreviatedPath(mKeychain->name()).c_str(),
        dlDbIdentifier.dbLocation());
    NameValueDictionary dict;
    NameValueDictionary::MakeNameValueDictionaryFromDLDbIdentifier(newDlDbIdentifier, dict);

    CssmData* pKey = mPrimaryKey;
    dict.Insert (new NameValuePair(ITEM_KEY, *pKey));

    // flatten the NameValueDictionary
    CssmData dictData;
    dict.Export(dictData);
    outDataRef = ::CFDataCreate(kCFAllocatorDefault, dictData.Data, dictData.Length);
    free (dictData.Data);
}

void ItemImpl::copyRecordIdentifier(CSSM_DATA &data)
{
	StLock<Mutex>_(mMutex);
	CssmClient::DbUniqueRecord uniqueRecord = dbUniqueRecord ();
	uniqueRecord->getRecordIdentifier(data);
}

/*
 * Obtain blob used to bind a keychain item to an Extended Attribute record.
 * We just use the PrimaryKey blob as the default. Note that for standard Items,
 * this can cause the loss of extended attribute bindings if a Primary Key
 * attribute changes. 
 */
const CssmData &ItemImpl::itemID()
{
	StLock<Mutex>_(mMutex);
	if(mPrimaryKey->length() == 0) {
		/* not in a keychain; we don't have a primary key */
		MacOSError::throwMe(errSecNoSuchAttr);
	}
	return *mPrimaryKey;
}


void ItemImpl::postItemEvent(SecKeychainEvent theEvent)
{
	mKeychain->postEvent(theEvent, this);
}



//
// Item -- This class is here to magically create the right subclass of ItemImpl
// when constructing new items.
//
Item::Item()
{
}

Item::Item(ItemImpl *impl) : SecPointer<ItemImpl>(impl)
{
}

Item::Item(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data, bool inhibitCheck)
{
	if (!inhibitCheck)
	{
		if (itemClass == CSSM_DL_DB_RECORD_X509_CERTIFICATE
			|| itemClass == CSSM_DL_DB_RECORD_PUBLIC_KEY
			|| itemClass == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| itemClass == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
			MacOSError::throwMe(errSecNoSuchClass); /* @@@ errSecInvalidClass */
	}
	
	*this = new ItemImpl(itemClass, itemCreator, length, data, inhibitCheck);
}

Item::Item(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void* data)
{
	*this = new ItemImpl(itemClass, attrList, length, data);
}

Item::Item(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId)
	: SecPointer<ItemImpl>(
		primaryKey->recordType() == CSSM_DL_DB_RECORD_X509_CERTIFICATE
		? Certificate::make(keychain, primaryKey, uniqueId)
		: (primaryKey->recordType() == CSSM_DL_DB_RECORD_PUBLIC_KEY
		   || primaryKey->recordType() == CSSM_DL_DB_RECORD_PRIVATE_KEY
		   || primaryKey->recordType() == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		? KeyItem::make(keychain, primaryKey, uniqueId)
		: primaryKey->recordType() == CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE
		   ? ExtendedAttribute::make(keychain, primaryKey, uniqueId)
		   : ItemImpl::make(keychain, primaryKey, uniqueId))
{
}

Item::Item(const Keychain &keychain, const PrimaryKey &primaryKey)
	: SecPointer<ItemImpl>(
		primaryKey->recordType() == CSSM_DL_DB_RECORD_X509_CERTIFICATE
		? Certificate::make(keychain, primaryKey)
		: (primaryKey->recordType() == CSSM_DL_DB_RECORD_PUBLIC_KEY
		   || primaryKey->recordType() == CSSM_DL_DB_RECORD_PRIVATE_KEY
		   || primaryKey->recordType() == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		? KeyItem::make(keychain, primaryKey)
		: primaryKey->recordType() == CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE
		   ? ExtendedAttribute::make(keychain, primaryKey) 
		   : ItemImpl::make(keychain, primaryKey))
{
}

Item::Item(ItemImpl &item)
	: SecPointer<ItemImpl>(
		item.recordType() == CSSM_DL_DB_RECORD_X509_CERTIFICATE
		? new Certificate(safer_cast<Certificate &>(item))
		: (item.recordType() == CSSM_DL_DB_RECORD_PUBLIC_KEY
		   || item.recordType() == CSSM_DL_DB_RECORD_PRIVATE_KEY
		   || item.recordType() == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		? new KeyItem(safer_cast<KeyItem &>(item))
		: item.recordType() == CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE
		  ? new ExtendedAttribute(safer_cast<ExtendedAttribute &>(item))
		  : new ItemImpl(item)) 
{
}
