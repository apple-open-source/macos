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
// Item.cpp
//

#include "Item.h"

#include "Globals.h"
#include "Schema.h"
#include "KCEventNotifier.h"
#include "cssmdatetime.h"
#include <Security/keychainacl.h>
#include <Security/SecKeychainAPIPriv.h>
#include <Security/aclsupport.h>
#include <Security/osxsigning.h>

using namespace KeychainCore;
using namespace CSSMDateTimeUtils;

//
// ItemImpl
//

// NewItemImpl constructor
ItemImpl::ItemImpl(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data)
: mDbAttributes(new DbAttributes())
{
	if (length && data)
		mData.reset(new CssmDataContainer(data, length));

	mDbAttributes->recordType(Schema::recordTypeFor(itemClass));
	mDbAttributes->add(Schema::attributeInfo(kSecCreatorItemAttr), itemCreator);

    SInt64 date;
	GetCurrentMacLongDateTime(date);
    setAttribute(Schema::attributeInfo(kSecCreationDateItemAttr), date);
    setAttribute(Schema::attributeInfo(kSecModDateItemAttr), date);
}

ItemImpl::ItemImpl(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void* data)
: mDbAttributes(new DbAttributes())
{
	if (length && data)
		mData.reset(new CssmDataContainer(data, length));


	mDbAttributes->recordType(Schema::recordTypeFor(itemClass));

	if(attrList) 
	{
		for(UInt32 i=0; i < attrList->count; i++)
		{
			mDbAttributes->add(Schema::attributeInfo(attrList->attr[i].tag), CssmData(attrList->attr[i].data,  attrList->attr[i].length));
		}
	}

    SInt64 date;
	GetCurrentMacLongDateTime(date);
    setAttribute(Schema::attributeInfo(kSecCreationDateItemAttr), date);
    setAttribute(Schema::attributeInfo(kSecModDateItemAttr), date);
}

// DbItemImpl constructor
ItemImpl::ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey, const DbUniqueRecord &uniqueId)
: mUniqueId(uniqueId), mKeychain(keychain), mPrimaryKey(primaryKey)
{
	mKeychain->addItem(mPrimaryKey, this);
}

// PrimaryKey ItemImpl constructor
ItemImpl::ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey)
: mKeychain(keychain), mPrimaryKey(primaryKey)
{
	mKeychain->addItem(mPrimaryKey, this);
}

// Constructor used when copying an item to a keychain.

ItemImpl::ItemImpl(ItemImpl &item) :
    mData(item.modifiedData() ? NULL : new CssmDataContainer()),
    mDbAttributes(new DbAttributes())
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
		mData.reset(new CssmDataContainer(item.modifiedData()->Data,
			item.modifiedData()->Length));
}

ItemImpl::~ItemImpl()
{
	if (mKeychain && *mPrimaryKey)
		mKeychain->removeItem(*mPrimaryKey, this);
}

void
ItemImpl::didModify()
{
	mData.reset(NULL);
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

PrimaryKey
ItemImpl::add(const Keychain &keychain)
{
	// If we already have a Keychain we can't be added.
	if (mKeychain)
		MacOSError::throwMe(errSecDuplicateItem);

    // If we don't have any attributes we can't be added.
    // (this might occur if attempting to add the item twice, since our attributes
    // and data are set to NULL at the end of this function.)
    if (!mDbAttributes.get())
		MacOSError::throwMe(errSecDuplicateItem);

    // If the label (PrintName) attribute isn't specified, set a default label.
    if (!mDbAttributes->find(Schema::attributeInfo(kSecLabelItemAttr)))
    {
        CssmDbAttributeData *label = NULL;
        switch (mDbAttributes->recordType())
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
        if (!label) label = mDbAttributes->find(Schema::attributeInfo(kSecAccountItemAttr));

        if (label && label->size())
            mDbAttributes->add(Schema::attributeInfo(kSecLabelItemAttr), label->at<CssmData>(0));
    }

	// get the attributes that are part of the primary key
	const CssmAutoDbRecordAttributeInfo &primaryKeyInfos =
		keychain->primaryKeyInfosFor(recordType());

	// make sure each primary key element has a value in the item, otherwise
	// the database will complain. we make a set of the provided attribute infos
	// to avoid O(N^2) behavior.

	DbAttributes *attributes = mDbAttributes.get();
	typedef set<CssmDbAttributeInfo> InfoSet;
	InfoSet infoSet;

	for (uint32 i = 0; i < attributes->size(); i++)
		infoSet.insert(attributes->at(i).Info);

	for (uint32 i = 0; i < primaryKeyInfos.size(); i++) {
		InfoSet::const_iterator it = infoSet.find(primaryKeyInfos.at(i));

		if (it == infoSet.end()) {
			// we need to add a default value to the item attributes
			attributes->add(primaryKeyInfos.at(i),
				defaultAttributeValue(primaryKeyInfos.at(i)));
		}
	}

	Db db(keychain->database());
	if (db->dl()->subserviceMask() & CSSM_SERVICE_CSP)
	{
		// Add the item to the secure storage db
		SSDb ssDb(safe_cast<SSDbImpl *>(&(*db)));

		TrackingAllocator allocator(CssmAllocator::standard());
		// @@@ Share this instance
		KeychainAclFactory aclFactory(allocator);

		AclEntryPrototype anyEncrypt(TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_ANY));
		AuthorizationGroup &anyEncryptAuthGroup = anyEncrypt.authorization();
		CSSM_ACL_AUTHORIZATION_TAG encryptTag = CSSM_ACL_AUTHORIZATION_ENCRYPT;
		anyEncryptAuthGroup.NumberOfAuthTags = 1;
		anyEncryptAuthGroup.AuthTags = &encryptTag;

		const AccessCredentials *nullCred = aclFactory.nullCredentials();

		const ResourceControlContext credAndAclEntry
			(anyEncrypt, const_cast<AccessCredentials *>(nullCred));

		// Create a new SSGroup with owner = ANY, encrypt = ANY
		SSGroup ssGroup(ssDb, &credAndAclEntry);

		// Now we edit the acl to look like we want it to.

		// Find the PrintName (which we want SecurityAgent to display when evaluating the ACL
		CssmDbAttributeData *data = mDbAttributes->find(Schema::attributeInfo(kSecLabelItemAttr));
		CssmData noName;
		CssmData &printName = data ? CssmData::overlay(data->Value[0]) : noName;

		// @@@ This code should use KeychainACL instead, but that class will need some changes.
		// Defering integration with KeychainACL to Puma.

		// Figure out if we should special case this to have an anyAllow in this ACL or not.
		// Currently only generic password items with sevicename "iTools" passwords are always anyAllow.
		bool anyAllow = false;
		if (mDbAttributes->recordType() == CSSM_DL_DB_RECORD_GENERIC_PASSWORD)
		{
			CssmDbAttributeData *data = mDbAttributes->find(Schema::attributeInfo(kSecServiceItemAttr));
			if (data && data->Value[0].Length == 6 && !memcmp("iTools", data->Value[0].Data, 6))
				anyAllow = true;
		}

		CssmList &list = *new(allocator) CssmList();
	
		// List is a threshold acl with 2 elements or 3 if anyAllow is true.
		list.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_THRESHOLD));   
		list.append(new(allocator) ListElement(1));
		list.append(new(allocator) ListElement(2 + anyAllow));

		// If anyAllow is true start the threshold list with a any allow sublist.
		if(anyAllow)
		{
			CssmList &anySublist = *new(allocator) CssmList();
			anySublist.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_ANY));
			list.append(new(allocator) ListElement(anySublist));
		}

		// Now add a sublist to trust the current application.
		auto_ptr<CodeSigning::OSXCode> code(CodeSigning::OSXCode::main());
		const char *path = code->canonicalPath().c_str();
		CssmData comment(const_cast<char *>(path), strlen(path) + 1);
		TrustedApplication app(path, comment);
		CssmList &appSublist = *new(allocator) CssmList();
		appSublist.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_CODE_SIGNATURE));
		appSublist.append(new(allocator) ListElement(CSSM_ACL_CODE_SIGNATURE_OSX));
		appSublist.append(new(allocator) ListElement(app->signature()));
		appSublist.append(new(allocator) ListElement(app->comment()));
		list.append(new(allocator) ListElement(appSublist));

		// Finally add the keychain prompt sublist to the list so we default to asking
		// the user for permission if all else fails.
		CssmList &promptSublist = *new(allocator) CssmList();
		promptSublist.append(new(allocator) ListElement(CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT));
		promptSublist.append(new(allocator) ListElement(printName));
		list.append(new(allocator) ListElement(promptSublist));	

		// The acl prototype we want to add contains the list we just made.
		AclEntryPrototype promptDecrypt(list);

		// Now make sure it only authorizes decrypt.
		AuthorizationGroup &promptDecryptAuthGroup = promptDecrypt.authorization();
		CSSM_ACL_AUTHORIZATION_TAG decryptTag = CSSM_ACL_AUTHORIZATION_DECRYPT;
		promptDecryptAuthGroup.NumberOfAuthTags = 1;
		promptDecryptAuthGroup.AuthTags = &decryptTag;

		// Add an acl entry for decrypt we just made
		AclEdit edit(promptDecrypt);
		ssGroup->changeAcl(nullCred, edit);

		try
		{
			// Insert the record using the newly created group.
			mUniqueId = ssDb->insert(recordType(), mDbAttributes.get(),
									 mData.get(), ssGroup, nullCred);
		}
		catch(...)
		{
			ssGroup->deleteKey(nullCred);
			throw;
		}

		// Change the owner so change acl = KeychainPrompt
		AclEntryPrototype promptOwner(TypedList(allocator, CSSM_ACL_SUBJECT_TYPE_KEYCHAIN_PROMPT,
			new(allocator) ListElement(allocator, printName)));
		AclOwnerPrototype owner(promptOwner);
		ssGroup->changeOwner(nullCred, owner);
	}
	else
	{
		// add the item to the (regular) db
		mUniqueId = db->insert(recordType(), mDbAttributes.get(), mData.get());
	}

	mPrimaryKey = keychain->makePrimaryKey(recordType(), mUniqueId);
    mKeychain = keychain;

	// Forget our data and attributes.
	mData.reset(NULL);
	mDbAttributes.reset(NULL);

	return mPrimaryKey;
}

Item
ItemImpl::copyTo(const Keychain &keychain)
{
	Item item(*this);
	keychain->add(item);
	return item;
}

void
ItemImpl::update()
{
	if (!mKeychain)
		MacOSError::throwMe(errSecNoSuchKeychain); 
		
	// Don't update if nothing changed.
	if (!isModified())
		return;

	// Set the modification date on the item.
    SInt64 date;
	GetCurrentMacLongDateTime(date);
    setAttribute(Schema::attributeInfo(kSecModDateItemAttr), date);

	// Make sure that we have mUniqueId
	dbUniqueRecord();
	Db db(mUniqueId->database());
	if (db->dl()->subserviceMask() & CSSM_SERVICE_CSP)
	{
		// Add the item to the secure storage db
		SSDbUniqueRecord ssUniqueId(safe_cast<SSDbUniqueRecordImpl *>
									(&(*mUniqueId)));

		// @@@ Share this instance
		const AccessCredentials *autoPrompt = globals().credentials();


		// Only call this is user interaction is enabled.
		ssUniqueId->modify(recordType(),
						   mDbAttributes.get(),
						   mData.get(),
						   CSSM_DB_MODIFY_ATTRIBUTE_REPLACE,
						   autoPrompt);
	}
	else
	{
		mUniqueId->modify(recordType(),
						  mDbAttributes.get(),
						  mData.get(),
						  CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	}

	PrimaryKey oldPK = mPrimaryKey;
	mPrimaryKey = mKeychain->makePrimaryKey(recordType(), mUniqueId);

	// Forget our data and attributes.
	mData.reset(NULL);
	mDbAttributes.reset(NULL);

	// Let the Keychain update what it needs to.
	mKeychain->didUpdate(this, oldPK, mPrimaryKey);
}

void
ItemImpl::getClass(SecKeychainAttribute &attr, UInt32 *actualLength)
{
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
    setAttribute(Schema::attributeInfo(attr.tag), CssmData(attr.data, attr.length));
}

CSSM_DB_RECORDTYPE
ItemImpl::recordType() const
{
	if (mDbAttributes.get())
		return mDbAttributes->recordType();

	return mPrimaryKey->recordType();
}

const DbAttributes *
ItemImpl::modifiedAttributes() const
{
	return mDbAttributes.get();
}

const CssmData *
ItemImpl::modifiedData() const
{
	return mData.get();
}

void
ItemImpl::setData(UInt32 length,const void *data)
{
	mData.reset(new CssmDataContainer(data, length));
}

CssmClient::DbUniqueRecord
ItemImpl::dbUniqueRecord()
{
	if (!mUniqueId)
	{
		assert(mKeychain && mPrimaryKey);
		DbCursor cursor(mPrimaryKey->createCursor(mKeychain));
		if (!cursor->next(NULL, NULL, mUniqueId))
		{
			killRef();
			MacOSError::throwMe(errSecInvalidItemRef);
		}
	}

	return mUniqueId;
}

PrimaryKey
ItemImpl::primaryKey() const
{
	return mPrimaryKey;
}

bool
ItemImpl::isPersistant() const
{
	return mKeychain;
}

bool
ItemImpl::isModified() const
{
	return mData.get() || mDbAttributes.get();
}

Keychain
ItemImpl::keychain() const
{
	return mKeychain;
}

bool
ItemImpl::operator <(const ItemImpl &other) const
{

	if (*mData)
	{
		// Pointer compare
		return this < &other;
	}

	// XXX Deal with not having a mPrimaryKey
	return *mPrimaryKey < *(other.mPrimaryKey);

}

void
ItemImpl::setAttribute(const CssmDbAttributeInfo &info, const CssmPolyData &data)
{
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
            MacLongDateTimeToTimeString(*reinterpret_cast<const SInt64 *>(buf),
                                        16, &timeString);
            buf = &timeString;
            length = 16;
        }
    }

	mDbAttributes->add(info, CssmData(const_cast<void*>(buf), length));
}

void
ItemImpl::modifyContent(const SecKeychainAttributeList *attrList, UInt32 dataLength, const void *inData)
{
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
		mData.reset(new CssmDataContainer(inData, dataLength));
	}
	
	update();
}

void
ItemImpl::getContent(SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData)
{

	// If the data hasn't been set we can't return it.
	if (!mKeychain && outData)
	{
		CssmData *data = mData.get();
		if (!data)
			MacOSError::throwMe(errSecDataNotAvailable);
	}
	// TODO: need to check and make sure attrs are valid and handle error condition


	if(itemClass)
		*itemClass = Schema::itemClassFor(recordType());
		
	dbUniqueRecord();

    UInt32 attrCount = attrList ? attrList->count : 0;
	DbAttributes dbAttributes(mUniqueId->database(), attrCount);
    for (UInt32 ix = 0; ix < attrCount; ++ix)
        dbAttributes.add(Schema::attributeInfo(attrList->attr[ix].tag));

	CssmDataContainer itemData;
    getContent(&dbAttributes, outData ? &itemData : NULL);

	if (outData) KCEventNotifier::PostKeychainEvent(kSecDataAccessEvent, mKeychain, this);

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

	if (outData)
	{
		*outData=itemData.data();
		itemData.Data=NULL;
		
		*length=itemData.length();
		itemData.Length=0;
	}
	
}

void
ItemImpl::freeContent(SecKeychainAttributeList *attrList, void *data)
{
	CssmAllocator &allocator = CssmAllocator::standard(); // @@@ This might not match the one used originally
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
	if (!mKeychain)
		MacOSError::throwMe(errSecNoSuchKeychain);

	if (!mDbAttributes.get())
	{
		mDbAttributes.reset(new DbAttributes());
		mDbAttributes->recordType(mPrimaryKey->recordType());
	}

    UInt32 attrCount = attrList ? attrList->count : 0;
	for (UInt32 ix = 0; ix < attrCount; ix++)
	{
		CssmDbAttributeInfo info=mKeychain->attributeInfoForTag(attrList->attr[ix].tag);
						
		if (attrList->attr[ix].length || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_STRING  || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_BLOB
		 || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_STRING  || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM
		 || info.AttributeFormat==CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32)
			mDbAttributes->add(info, CssmData(attrList->attr[ix].data, attrList->attr[ix].length));
		else
			mDbAttributes->add(info);
	}
	
	if(inData)
	{
		mData.reset(new CssmDataContainer(inData, dataLength));
	}
	
	update();
}

void
ItemImpl::getAttributesAndData(SecKeychainAttributeInfo *info, SecItemClass *itemClass, SecKeychainAttributeList **attrList, UInt32 *length, void **outData)
{
	// If the data hasn't been set we can't return it.
	if (!mKeychain && outData)
	{
		CssmData *data = mData.get();
		if (!data)
			MacOSError::throwMe(errSecDataNotAvailable);
	}
	// TODO: need to check and make sure attrs are valid and handle error condition


	if(itemClass)
		*itemClass = Schema::itemClassFor(recordType());
		
	dbUniqueRecord();

    UInt32 attrCount = info ? info->count : 0;
	DbAttributes dbAttributes(mUniqueId->database(), attrCount);
    for (UInt32 ix = 0; ix < attrCount; ix++)
	{
		CssmDbAttributeData &record = dbAttributes.add();
		record.Info.AttributeNameFormat=CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
		record.Info.Label.AttributeID=info->tag[ix];
	}

	CssmDataContainer itemData;
    getContent(&dbAttributes, outData ? &itemData : NULL);

	if(info && attrList)
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
		
		*length=itemData.length();
		itemData.Length=0;
				
		KCEventNotifier::PostKeychainEvent(kSecDataAccessEvent, mKeychain, this);
	}
	
}

void
ItemImpl::freeAttributesAndData(SecKeychainAttributeList *attrList, void *data)
{
	CssmAllocator &allocator = CssmAllocator::standard(); // @@@ This might not match the one used originally

	if (data)
		allocator.free(data);

	if(attrList)
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
    static const uint32 zero = 0;
    uint32 length;
    const void *buf;

    // Temporary storage for buf.
    SInt64 macLDT;
    UInt32 macSeconds;
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
            if (attr.length == sizeof(UInt32))
            {
                TimeStringToMacSeconds(data->Value[0], macSeconds);
                buf = &macSeconds;
                length = attr.length;
            }
            else if (attr.length == sizeof(SInt64))
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

	//%%%<might> be done elsewhere, but here is good for now
	KCEventNotifier::PostKeychainEvent(kSecDataAccessEvent, mKeychain, this);
}

SSGroup
ItemImpl::group()
{
	SSGroup group;
	if (&*mUniqueId)
	{
		Db db(mKeychain->database());
		if (db->dl()->subserviceMask() & CSSM_SERVICE_CSP)
		{
			group = safer_cast<SSDbUniqueRecordImpl &>(*mUniqueId).group();
		}
	}

	return group;
}

void
ItemImpl::getContent(DbAttributes *dbAttributes, CssmDataContainer *itemData)
{
    // Make sure mUniqueId is set.
	dbUniqueRecord();
	if (itemData)
	{
		Db db(mUniqueId->database());
		if (db->dl()->subserviceMask() & CSSM_SERVICE_CSP)
		{
			SSDbUniqueRecord ssUniqueId(safe_cast<SSDbUniqueRecordImpl *>(&(*mUniqueId)));
			const AccessCredentials *autoPrompt = globals().credentials();
			ssUniqueId->get(dbAttributes, itemData, autoPrompt);
            return;
		}
    }

    mUniqueId->get(dbAttributes, itemData); 
}
