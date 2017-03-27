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
// Item.cpp
//

#include "Item.h"

#include "Certificate.h"
#include "KeyItem.h"
#include "ExtendedAttribute.h"

#include "Globals.h"
#include <security_cdsa_utilities/Schema.h>
#include "KCEventNotifier.h"
#include "KCExceptions.h"
#include "cssmdatetime.h"
#include <security_cdsa_client/keychainacl.h>
#include <security_utilities/osxcode.h>
#include <security_utilities/trackingallocator.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/cssmapple.h>
#include <CommonCrypto/CommonDigest.h>
#include <utilities/der_plist.h>

#include <security_utilities/CSPDLTransaction.h>
#include <SecBasePriv.h>

#define SENDACCESSNOTIFICATIONS 1

//%%% schema indexes should be defined in Schema.h
#define _kSecAppleSharePasswordItemClass		'ashp'
#define APPLEDB_CSSM_PRINTNAME_ATTRIBUTE        1   /* schema index for label attribute of keys or certificates */
#define APPLEDB_GENERIC_PRINTNAME_ATTRIBUTE     7   /* schema index for label attribute of password items */
#define IS_PASSWORD_ITEM_CLASS(X)             ( (X) == kSecInternetPasswordItemClass || \
                                                (X) == kSecGenericPasswordItemClass || \
                                                (X) == _kSecAppleSharePasswordItemClass ) ? 1 : 0

using namespace KeychainCore;
using namespace CSSMDateTimeUtils;

//
// ItemImpl
//

ItemImpl *ItemImpl::required(SecKeychainItemRef ptr)
{
    if (ptr != NULL) {
        if (ItemImpl *pp = optional(ptr)) {
            return pp;
        }
    }
    MacOSError::throwMe(errSecInvalidItemRef);
}

ItemImpl *ItemImpl::optional(SecKeychainItemRef ptr)
{
    if (ptr != NULL && CFGetTypeID(ptr) == SecKeyGetTypeID()) {
        return dynamic_cast<ItemImpl *>(KeyItem::fromSecKeyRef(ptr));
    } else if (SecCFObject *p = SecCFObject::optional(ptr)) {
        if (ItemImpl *pp = dynamic_cast<ItemImpl *>(p)) {
            return pp;
        } else {
            MacOSError::throwMe(errSecInvalidItemRef);
        }
    } else {
        return NULL;
    }
}

// NewItemImpl constructor
ItemImpl::ItemImpl(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data, bool dontDoAttributes)
	: mDbAttributes(new DbAttributes()),
	mKeychain(NULL),
	secd_PersistentRef(NULL),
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
	secd_PersistentRef(NULL),
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
	secd_PersistentRef(NULL), mDoNotEncrypt(false), mInCache(false),
	mMutex(Mutex::recursive)
{
}

// PrimaryKey ItemImpl constructor
ItemImpl::ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey)
: mKeychain(keychain), mPrimaryKey(primaryKey),	secd_PersistentRef(NULL), mDoNotEncrypt(false),
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
	secd_PersistentRef(NULL),
	mDoNotEncrypt(false),
	mInCache(false),
	mMutex(Mutex::recursive)
{
	mDbAttributes->recordType(item.recordType());

	if (item.mKeychain) {
		// get the entire source item from its keychain. This requires figuring
		// out the schema for the item based on its record type.
        // Ask the remote item to fill our attributes dictionary, because it probably has an attached keychain to ask
        item.fillDbAttributesFromSchema(*mDbAttributes, item.recordType());

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
	if (secd_PersistentRef) {
		CFRelease(secd_PersistentRef);
	}
}



Mutex*
ItemImpl::getMutexForObject() const
{
	if (mKeychain.get())
	{
		return mKeychain->getKeychainMutex();
	}

	return NULL;
}


void
ItemImpl::aboutToDestruct()
{
    if(mKeychain.get()) {
        mKeychain->forceRemoveFromCache(this);
    }
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

void ItemImpl::fillDbAttributesFromSchema(DbAttributes& dbAttributes, CSSM_DB_RECORDTYPE recordType, Keychain keychain) {
    // If we weren't passed a keychain, use our own.
    keychain = !!keychain ? keychain : mKeychain;

    // Without a keychain, there's no one to ask.
    if(!keychain) {
        return;
    }

    SecKeychainAttributeInfo* infos;
    keychain->getAttributeInfoForItemID(recordType, &infos);

    secnotice("integrity", "filling %u attributes for type %u", (unsigned int)infos->count, recordType);

    for (uint32 i = 0; i < infos->count; i++) {
        CSSM_DB_ATTRIBUTE_INFO info;
        memset(&info, 0, sizeof(info));

        info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
        info.Label.AttributeID = infos->tag[i];
        info.AttributeFormat = infos->format[i];

        dbAttributes.add(info);
    }

    keychain->freeAttributeInfo(infos);
}

DbAttributes* ItemImpl::getCurrentAttributes() {
    DbAttributes* dbAttributes;
    secnotice("integrity", "getting current attributes...");

    if(mUniqueId.get()) {
        // If we have a unique id, there's an item in the database backing us. Ask for its attributes.
        dbAttributes = new DbAttributes(dbUniqueRecord()->database(), 1);
        fillDbAttributesFromSchema(*dbAttributes, recordType());
        mUniqueId->get(dbAttributes, NULL);

        // and fold in any updates.
        if(mDbAttributes.get()) {
            secnotice("integrity", "adding %d attributes from mDbAttributes", mDbAttributes->size());
            dbAttributes->updateWithDbAttributes(&(*mDbAttributes.get()));
        }
    } else if (mDbAttributes.get()) {
        // We don't have a backing item, so all our attributes are in mDbAttributes. Copy them.
        secnotice("integrity", "no unique id, using %d attributes from mDbAttributes", mDbAttributes->size());
        dbAttributes = new DbAttributes();
        dbAttributes->updateWithDbAttributes(&(*mDbAttributes.get()));
    } else {
        // No attributes at all. We should maybe throw here, but let's not.
        secnotice("integrity", "no attributes at all");
        dbAttributes = new DbAttributes();
    }
    dbAttributes->recordType(recordType());
    // TODO: We don't set semanticInformation. Issue?

    return dbAttributes;
}


void ItemImpl::encodeAttributes(CssmOwnedData &attributeBlob) {
    // Sometimes we don't have our attributes. Find them.
    auto_ptr<DbAttributes> dbAttributes(getCurrentAttributes());
    encodeAttributesFromDictionary(attributeBlob, dbAttributes.get());

}

void ItemImpl::encodeAttributesFromDictionary(CssmOwnedData &attributeBlob, DbAttributes* dbAttributes) {
    // Create a CFDictionary from dbAttributes and call der_encode_dictionary on it
    CFRef<CFMutableDictionaryRef> attributes;
    attributes.take(CFDictionaryCreateMutable(NULL, dbAttributes->size(), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    secnotice("integrity", "looking at %d attributes", dbAttributes->size());
    // TODO: include record type and semantic information?

    for(int i = 0; i < dbAttributes->size(); i++) {
        CssmDbAttributeData& data = dbAttributes->attributes()[i];
        CssmDbAttributeInfo& datainfo = data.info();

        // Sometimes we need to normalize the info. Calling Schema::attributeInfo is the best way to do that.
        // There's no avoiding the try-catch structure here, since only some of the names are in Schema::attributeInfo,
        // but it will only indicate that by throwing.
        CssmDbAttributeInfo& actualInfo = datainfo;
        try {
            if(datainfo.nameFormat() == CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER && Schema::haveAttributeInfo(datainfo.intName())) {
                actualInfo = Schema::attributeInfo(datainfo.intName());
            }
        } catch(...) {
            actualInfo = datainfo;
        }

        // Pull the label/name out of this data
        CFRef<CFDataRef> label = NULL;

        switch(actualInfo.nameFormat()) {
            case CSSM_DB_ATTRIBUTE_NAME_AS_STRING: {
                const char* stringname = actualInfo.stringName();
                label.take(CFDataCreate(NULL, reinterpret_cast<const UInt8*>(stringname), strlen(stringname)));
                break;
            }
            case CSSM_DB_ATTRIBUTE_NAME_AS_OID: {
                const CssmOid& oidname = actualInfo.oidName();
                label.take(CFDataCreate(NULL, reinterpret_cast<const UInt8*>(oidname.data()), oidname.length()));
                break;
            }
            case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER: {
                uint32 iname = actualInfo.intName();
                label.take(CFDataCreate(NULL, reinterpret_cast<const UInt8*>(&(iname)), sizeof(uint32)));
                break;
            }
        }

        if(data.size() == 0) {
            // This attribute doesn't have a value, and so shouldn't be included in the digest.
            continue;
        }

        // Do not include the Creation or Modification date attributes in the hash.
        // Use this complicated method of checking so we'll catch string and integer names.
        SecKeychainAttrType cdat = kSecCreationDateItemAttr;
        SecKeychainAttrType cmod = kSecModDateItemAttr;
        if((CFDataGetLength(label) == sizeof(SecKeychainAttrType)) &&
                ((memcmp(CFDataGetBytePtr(label), &cdat, sizeof(SecKeychainAttrType)) == 0) ||
                 (memcmp(CFDataGetBytePtr(label), &cmod, sizeof(SecKeychainAttrType)) == 0))) {
            continue;
        }

        // Collect the raw data for each value of this CssmDbAttributeData
        CFRef<CFMutableArrayRef> attributeDataContainer;
        attributeDataContainer.take(CFArrayCreateMutable(NULL, data.size(), &kCFTypeArrayCallBacks));

        for(int j = 0; j < data.size(); j++) {
            CssmData& entry = data.values()[j];

            CFRef<CFDataRef> datadata = NULL;
            switch(actualInfo.format()) {
                case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
                case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
                case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
                    datadata.take(CFDataCreate(NULL, reinterpret_cast<const UInt8*>(data.values()[j].data()), data.values()[j].length()));
                    break;

                case CSSM_DB_ATTRIBUTE_FORMAT_UINT32: {
                    uint32 x = entry.length() == 1 ? *reinterpret_cast<uint8 *>(entry.Data) :
                               entry.length() == 2 ? *reinterpret_cast<uint16 *>(entry.Data) :
                               entry.length() == 4 ? *reinterpret_cast<uint32 *>(entry.Data) : 0;
                    datadata.take(CFDataCreate(NULL, reinterpret_cast<const UInt8*>(&x), sizeof(x)));
                    break;
                }

                case CSSM_DB_ATTRIBUTE_FORMAT_SINT32: {
                    sint32 x = entry.length() == 1 ? *reinterpret_cast<sint8 *>(entry.Data) :
                               entry.length() == 2 ? *reinterpret_cast<sint16 *>(entry.Data) :
                               entry.length() == 4 ? *reinterpret_cast<sint32 *>(entry.Data) : 0;
                    datadata.take(CFDataCreate(NULL, reinterpret_cast<const UInt8*>(&x), sizeof(x)));
                    break;
                }
                // CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM is unimplemented here but
                // has some canonicalization requirements, see DbValue.cpp

                default:
                    continue;
            }

            CFArrayAppendValue(attributeDataContainer, datadata);
        }
        CFDictionaryAddValue(attributes, label, attributeDataContainer);
    }

    // Now that we have a CFDictionary containing a bunch of CFDatas, turn that
    // into a der blob.

    CFErrorRef error;
    CFRef<CFDataRef> derBlob;
    derBlob.take(CFPropertyListCreateDERData(NULL, attributes, &error));

    // TODO: How do we check error here?

    if(!derBlob) {
        return;
    }

    attributeBlob.length(CFDataGetLength(derBlob));
    attributeBlob.copy(CFDataGetBytePtr(derBlob), CFDataGetLength(derBlob));
}

void ItemImpl::computeDigest(CssmOwnedData &sha2) {
    auto_ptr<DbAttributes> dbAttributes(getCurrentAttributes());
    ItemImpl::computeDigestFromDictionary(sha2, dbAttributes.get());
}

void ItemImpl::computeDigestFromDictionary(CssmOwnedData &sha2, DbAttributes* dbAttributes) {
    try{
        CssmAutoData attributeBlob(Allocator::standard());
        encodeAttributesFromDictionary(attributeBlob, dbAttributes);

        sha2.length(CC_SHA256_DIGEST_LENGTH);
        CC_SHA256(attributeBlob.get().data(), static_cast<CC_LONG>(attributeBlob.get().length()), sha2);
        secnotice("integrity", "finished: %s", sha2.get().toHex().c_str());
    } catch (MacOSError mose) {
        secnotice("integrity", "MacOSError: %d", (int)mose.osStatus());
    } catch (...) {
        secnotice("integrity", "unknown exception");
    }
}

void ItemImpl::addIntegrity(Access &access, bool force) {
    if(!force && (!mKeychain || !mKeychain->hasIntegrityProtection())) {
        secnotice("integrity", "skipping integrity add due to keychain version\n");
        return;
    }

    ACL * acl = NULL;
    CssmAutoData digest(Allocator::standard());
    computeDigest(digest);

    // First, check if this already has an integrity tag
    vector<ACL *> acls;
    access.findSpecificAclsForRight(CSSM_ACL_AUTHORIZATION_INTEGRITY, acls);

    if(acls.size() >= 1) {
        // Use the existing ACL
        acl = acls[0];
        secnotice("integrity", "previous integrity acl exists; setting integrity");
        acl->setIntegrity(digest.get());

        // Delete all extra ACLs
        for(int i = 1; i < acls.size(); i++) {
            secnotice("integrity", "extra integrity acls exist; removing %d",i);
            acls[i]->remove();
        }
    } else if(acls.size() == 0) {
        // Make a new ACL
        secnotice("integrity", "no previous integrity acl exists; making a new one");
        acl = new ACL(digest.get());
        access.add(acl);
    }
}

 void ItemImpl::setIntegrity(bool force) {
     if(!force && (!mKeychain || !mKeychain->hasIntegrityProtection())) {
         secnotice("integrity", "skipping integrity set due to keychain version");
         return;
     }

     // For Items, only passwords should have integrity
     if(!(recordType() == CSSM_DL_DB_RECORD_GENERIC_PASSWORD || recordType() == CSSM_DL_DB_RECORD_INTERNET_PASSWORD)) {
         return;
     }

     // If we're not on an SSDb, we shouldn't have integrity
     Db db(mKeychain->database());
     if (!useSecureStorage(db)) {
         return;
     }

     setIntegrity(*group(), force);
 }

void ItemImpl::setIntegrity(AclBearer &bearer, bool force) {
    if(!force && (!mKeychain || !mKeychain->hasIntegrityProtection())) {
        secnotice("integrity", "skipping integrity acl set due to keychain version");
        return;
    }

    SecPointer<Access> access = new Access(bearer);

    access->removeAclsForRight(CSSM_ACL_AUTHORIZATION_PARTITION_ID);
    addIntegrity(*access, force);
    access->setAccess(bearer, true);
}

void ItemImpl::removeIntegrity(const AccessCredentials *cred) {
    removeIntegrity(*group(), cred);
}

void ItemImpl::removeIntegrity(AclBearer &bearer, const AccessCredentials *cred) {
    SecPointer<Access> access = new Access(bearer);
    vector<ACL *> acls;

    access->findSpecificAclsForRight(CSSM_ACL_AUTHORIZATION_INTEGRITY, acls);
    for(int i = 0; i < acls.size(); i++) {
        acls[i]->remove();
    }

    access->findSpecificAclsForRight(CSSM_ACL_AUTHORIZATION_PARTITION_ID, acls);
    for(int i = 0; i < acls.size(); i++) {
        acls[i]->remove();
    }

    access->editAccess(bearer, true, cred);
}

bool ItemImpl::checkIntegrity() {
    // Note: subclasses are responsible for checking themselves.

    // If we don't have a keychain yet, we don't have any group. Return true?
    if(!isPersistent()) {
        secnotice("integrity", "no keychain, integrity is valid?");
        return true;
    }

    if(!mKeychain || !mKeychain->hasIntegrityProtection()) {
        secnotice("integrity", "skipping integrity check due to keychain version");
        return true;
    }

    // Collect our SSGroup, if it exists.
    dbUniqueRecord();
    SSGroup ssGroup = group();
    if(ssGroup) {
        return checkIntegrity(*ssGroup);
    }

    // If we don't have an SSGroup, we can't be invalid. return true.
    return true;
}

bool ItemImpl::checkIntegrity(AclBearer& aclBearer) {
    if(!mKeychain || !mKeychain->hasIntegrityProtection()) {
        secnotice("integrity", "skipping integrity check due to keychain version");
        return true;
    }

    auto_ptr<DbAttributes> dbAttributes(getCurrentAttributes());
    return checkIntegrityFromDictionary(aclBearer, dbAttributes.get());
}

bool ItemImpl::checkIntegrityFromDictionary(AclBearer& aclBearer, DbAttributes* dbAttributes) {
    try {
        AutoAclEntryInfoList aclInfos;
        aclBearer.getAcl(aclInfos, CSSM_APPLE_ACL_TAG_INTEGRITY);

        // We should only expect there to be one integrity tag. If there's not,
        // take the first one and ignore the rest. We should probably attempt delete
        // them.

        AclEntryInfo &info = aclInfos.at(0);
        auto_ptr<ACL> acl(new ACL(info, Allocator::standard()));

        for(int i = 1; i < aclInfos.count(); i++) {
            secnotice("integrity", "*** DUPLICATE INTEGRITY ACL, something has gone wrong");
        }

        CssmAutoData digest(Allocator::standard());
        computeDigestFromDictionary(digest, dbAttributes);
        if (acl->integrity() == digest.get()) {
            return true;
        }
    }
    catch (CssmError cssme) {
        const char* errStr = cssmErrorString(cssme.error);
        secnotice("integrity", "caught CssmError: %d %s", (int) cssme.error, errStr);

        if(cssme.error == CSSMERR_CSP_ACL_ENTRY_TAG_NOT_FOUND) {
            // TODO: No entry, run migrator?
            return true;
        }
        if(cssme.error == CSSMERR_CSP_INVALID_ACL_SUBJECT_VALUE) {
            // something went horribly wrong with fetching acl.

            secnotice("integrity", "INVALID ITEM (too many integrity acls)");
            return false;
        }
        if(cssme.error == CSSMERR_CSP_VERIFY_FAILED) {
            secnotice("integrity", "MAC verification failed; something has gone very wrong");
            return false; // No MAC, no integrity.
        }

        throw cssme;
    }

    secnotice("integrity", "***** INVALID ITEM");
    return false;
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
    mDbAttributes->canonicalize(); // make sure we'll find the label with the thing Schema::attributeInfo returns
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

    try {
        mKeychain = keychain;

        Db db(keychain->database());
        if (mDoNotEncrypt)
        {
            mUniqueId = db->insertWithoutEncryption (recordType, NULL, mData.get());
        }
        else if (useSecureStorage(db))
        {
            updateSSGroup(db, recordType, mData.get(), keychain, mAccess);
            mAccess = NULL; // use them and lose them - TODO: should this only be unset if there's no error in saveToNewSSGroup? Unclear.
        }
        else
        {
            // add the item to the (regular) db
            mUniqueId = db->insert(recordType, mDbAttributes.get(), mData.get());
        }

        mPrimaryKey = keychain->makePrimaryKey(recordType, mUniqueId);
    } catch(...) {
        mKeychain = NULL;
        throw;
    }

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
    // We'll be removing any Partition or Integrity ACLs from this item during
    // the copy. Note that creating a new item from this one fetches the data,
    // so this process must now be on the ACL/partition ID list for this item,
    // and an attacker without access can't cause this removal.
    //
    // The integrity and partition ID acls will get re-added once the item lands
    // in the new keychain, if it supports them. If it doesn't, removing the
    // integrity acl as it leaves will prevent any issues if the item is
    // modified in the unsupported keychain and then re-copied back into an
    // integrity keychain.

	StLock<Mutex>_(mMutex);
	Item item(*this);
	if (newAccess) {
        newAccess->removeAclsForRight(CSSM_ACL_AUTHORIZATION_PARTITION_ID);
        newAccess->removeAclsForRight(CSSM_ACL_AUTHORIZATION_INTEGRITY);
		item->setAccess(newAccess);
    } else {
		/* Attempt to copy the access from the current item to the newly created one. */
		SSGroup myGroup = group();
		if (myGroup)
		{
			SecPointer<Access> access = new Access(*myGroup);
            access->removeAclsForRight(CSSM_ACL_AUTHORIZATION_PARTITION_ID);
            access->removeAclsForRight(CSSM_ACL_AUTHORIZATION_INTEGRITY);
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

	Db db(dbUniqueRecord()->database());
	if (mDoNotEncrypt)
	{
		CSSM_DB_RECORD_ATTRIBUTE_DATA attrData;
		memset (&attrData, 0, sizeof (attrData));
		attrData.DataRecordType = aRecordType;

		dbUniqueRecord()->modifyWithoutEncryption(aRecordType,
                                                  &attrData,
                                                  mData.get(),
                                                  CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	}
	else if (useSecureStorage(db))
	{
        // Pass mData to updateSSGroup. If we have any data to change (and it's
        // therefore non-null), it'll save to a new SSGroup; otherwise, it will
        // update the old ssgroup. This prevents a RAA on attribute update, while
        // still protecting new data from being decrypted by old SSGroups with
        // outdated attributes.
        updateSSGroup(db, recordType(), mData.get());
    }
	else
	{
		dbUniqueRecord()->modify(aRecordType,
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
ItemImpl::updateSSGroup(Db& db, CSSM_DB_RECORDTYPE recordType, CssmDataContainer* newdata, Keychain keychain, SecPointer<Access> access)
{
    // hhs replaced with the new aclFactory class
    AclFactory aclFactory;
    const AccessCredentials *nullCred = aclFactory.nullCred();

    bool haveOldUniqueId = !!mUniqueId.get();
    SSDbUniqueRecord ssUniqueId(NULL);
    SSGroup ssGroup(NULL);
    if(haveOldUniqueId) {
        ssUniqueId = SSDbUniqueRecord(dynamic_cast<SSDbUniqueRecordImpl *>(&(*mUniqueId)));
        if (ssUniqueId.get() == NULL) {
            CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
        }
        ssGroup = ssUniqueId->group();
    }

    // If we have new data OR no old unique id, save to a new group
    bool saveToNewSSGroup = (!!newdata) || (!haveOldUniqueId);

    // If there aren't any attributes, make up some blank ones.
    if (!mDbAttributes.get())
    {
        secnotice("integrity", "making new dbattributes");
        mDbAttributes.reset(new DbAttributes());
        mDbAttributes->recordType(mPrimaryKey->recordType());
    }

    // Add the item to the secure storage db
    SSDbImpl* impl = dynamic_cast<SSDbImpl *>(&(*db));
    if (impl == NULL)
    {
        CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
    }

    SSDb ssDb(impl);

    TrackingAllocator allocator(Allocator::standard());

    if ((!access) && (haveOldUniqueId)) {
        // Copy the ACL from the old group.
        secnotice("integrity", "copying old ACL");
        access = new Access(*(ssGroup));

        // We can't copy these over to the new item; they're going to be reset.
        // Remove them before securityd complains.
        access->removeAclsForRight(CSSM_ACL_AUTHORIZATION_PARTITION_ID);
        access->removeAclsForRight(CSSM_ACL_AUTHORIZATION_INTEGRITY);
    } else if (!access) {
        secnotice("integrity", "setting up new ACL");
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
    } else {
        secnotice("integrity", "passed an Access, use it");
        // Access is non-null. Do nothing.
    }

    // If we have an old group and an old mUniqueId, then we're in the middle of an update.
    // mDbAttributes contains the newest attributes, but not the old ones. Find
    // them, merge them, and shove them all back into mDbAttributes. This lets
    // us re-apply them all to the new item.
    if(haveOldUniqueId) {
        mDbAttributes.reset(getCurrentAttributes());
    }

    // Create a CSPDL transaction. Note that this does things when it goes out of scope.
    CSPDLTransaction transaction(db);

    Access::Maker maker;
    ResourceControlContext prototype;
    maker.initialOwner(prototype, nullCred);

    if(saveToNewSSGroup) {
        secnotice("integrity", "saving to a new SSGroup");

        // If we're updating an item, it has an old group and possibly an
        // old mUniqueId. Delete these from the database, so we can insert
        // new ones.
        if(haveOldUniqueId) {
            secnotice("integrity", "deleting old mUniqueId");
            mUniqueId->deleteRecord();
            mUniqueId.release();
        } else {
            secnotice("integrity", "no old mUniqueId");
        }

        // Create a new SSGroup with temporary access controls
        SSGroup newSSGroup(ssDb, &prototype);
        const AccessCredentials * cred = maker.cred();

        try {
            doChange(keychain, recordType, ^{
                mUniqueId = ssDb->ssInsert(recordType, mDbAttributes.get(), newdata, newSSGroup, cred);
            });

            // now finalize the access controls on the group
            addIntegrity(*access);
            access->setAccess(*newSSGroup, maker);

            // We have to reset this after we add the integrity, since it needs the attributes
            mDbAttributes.reset(NULL);

            transaction.commit();
        }
        catch (CssmError cssme) {
            const char* errStr = cssmErrorString(cssme.error);
            secnotice("integrity", "caught CssmError during add: %d %s", (int) cssme.error, errStr);

            // Delete the new SSGroup that we just created
            deleteSSGroup(newSSGroup, nullCred);
            throw;
        }
        catch (MacOSError mose) {
            secnotice("integrity", "caught MacOSError during add: %d", (int) mose.osStatus());

            deleteSSGroup(newSSGroup, nullCred);
            throw;
        }
        catch (...)
        {
            secnotice("integrity", "caught unknown exception during add");

            deleteSSGroup(newSSGroup, nullCred);
            throw;
        }
    } else {
        // Modify the old SSGroup
        secnotice("integrity", "modifying the existing SSGroup");

        try {
            doChange(keychain, recordType, ^{
                assert(!newdata);
                const AccessCredentials *autoPrompt = globals().itemCredentials();
                ssUniqueId->modify(recordType,
                        mDbAttributes.get(),
                        newdata,
                        CSSM_DB_MODIFY_ATTRIBUTE_REPLACE,
                        autoPrompt);
            });

            // Update the integrity on the SSGroup
            setIntegrity(*ssGroup);

            // We have to reset this after we add the integrity, since it needs the attributes
            mDbAttributes.reset(NULL);

            transaction.commit();
        }
        catch (CssmError cssme) {
            const char* errStr = cssmErrorString(cssme.error);
            secnotice("integrity", "caught CssmError during modify: %d %s", (int) cssme.error, errStr);
            throw;
        }
        catch (MacOSError mose) {
            secnotice("integrity", "caught MacOSError during modify: %d", (int) mose.osStatus());
            throw;
        }
        catch (...)
        {
            secnotice("integrity", "caught unknown exception during modify");
            throw;
        }

    }
}

// Helper function to delete a group and swallow all errors
void ItemImpl::deleteSSGroup(SSGroup & ssgroup, const AccessCredentials* nullCred) {
    try{
        ssgroup->deleteKey(nullCred);
    } catch(CssmError error) {
        secnotice("integrity", "caught cssm error during deletion of group: %d %s", (int) error.osStatus(), error.what());
    } catch(MacOSError error) {
        secnotice("integrity", "caught macos error during deletion of group: %d %s", (int) error.osStatus(), error.what());
    } catch(UnixError error) {
        secnotice("integrity", "caught unix error during deletion of group: %d %s", (int) error.osStatus(), error.what());
    }
}

void
ItemImpl::doChange(Keychain keychain, CSSM_DB_RECORDTYPE recordType, void (^tryChange) ())
{
    // Insert the record using the newly created group.
    try {
        tryChange();
    } catch (CssmError cssme) {
        // If there's a "duplicate" of this item, it might be an item with corrupt/invalid attributes
        // Try to extract the item and check its attributes, then try again if necessary
        auto_ptr<CssmClient::DbAttributes> primaryKeyAttrs;
        if(cssme.error == CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA) {
            secnotice("integrity", "possible duplicate, trying to delete invalid items");

            Keychain kc = (keychain ? keychain : mKeychain);
            if(!kc) {
                secnotice("integrity", "no valid keychain");
            }

            // Only check for corrupt items if the keychain supports them
            if((!kc) || !kc->hasIntegrityProtection()) {
                secnotice("integrity", "skipping integrity check for corrupt items due to keychain support");
                throw;
            } else {
                primaryKeyAttrs.reset(getCurrentAttributes());
                PrimaryKey pk = kc->makePrimaryKey(recordType, primaryKeyAttrs.get());

                bool tryAgain = false;

                // Because things are lazy, maybe our keychain has a version
                // of this item with different attributes. Ask it!
                ItemImpl* maybeItem = kc->_lookupItem(pk);
                if(maybeItem) {
                    if(!maybeItem->checkIntegrity()) {
                        Item item(maybeItem);
                        kc->deleteItem(item);
                        tryAgain = true;
                    }
                } else {
                    // Our keychain doesn't know about any item with this primary key, so maybe
                    // we have a corrupt item in the database. Let's check.

                    secnotice("integrity", "making a cursor from primary key");
                    CssmClient::DbCursor cursor = pk->createCursor(kc);
                    DbUniqueRecord uniqueId;

                    StLock<Mutex> _mutexLocker(*kc->getKeychainMutex());

                    // The item on-disk might have more or different attributes than we do, since we're
                    // only searching via primary key. Fetch all of its attributes.
                    auto_ptr<DbAttributes>dbDupAttributes (new DbAttributes(kc->database(), 1));
                    fillDbAttributesFromSchema(*dbDupAttributes, recordType, kc);

                    // Occasionally this cursor won't return the item attributes (for an unknown reason).
                    // However, we know the attributes any item with this primary key should have, so use those instead.
                    while (cursor->next(dbDupAttributes.get(), NULL, uniqueId)) {
                        secnotice("integrity", "got an item...");

                        SSGroup group = safer_cast<SSDbUniqueRecordImpl &>(*uniqueId).group();
                        if(!ItemImpl::checkIntegrityFromDictionary(*group, dbDupAttributes.get())) {
                            secnotice("integrity", "item is invalid! deleting...");
                            uniqueId->deleteRecord();
                            tryAgain = true;
                        }
                    }
                }

                if(tryAgain) {
                    secnotice("integrity", "trying again...");
                    tryChange();
                } else {
                    // We didn't find an invalid item, the duplicate item exception is real
                    secnotice("integrity", "duplicate item exception is real; throwing it on");
                    throw;
                }
            }
        } else {
            throw;
        }
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

    // Check that our Db still matches our keychain's db. If not, find this item again in the new Db.
    // Why silly !(x == y) construction? Compiler calls operator bool() on each pointer otherwise.
    if(!(mUniqueId->database() == keychain()->database())) {
        secnotice("integrity", "updating db of mUniqueRecord");

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

	size_t length = data.Length;
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
    unique_ptr<StReadWriteLock> __(mKeychain == NULL ? NULL : new StReadWriteLock(*(mKeychain->getKeychainReadWriteLock()), StReadWriteLock::Write));

	if (!mDbAttributes.get())
	{
		mDbAttributes.reset(new DbAttributes());
		mDbAttributes->recordType(mPrimaryKey->recordType());
	}

	if(attrList) // optional
	{
		for(UInt32 ix=0; ix < attrList->count; ix++)
		{
            SecKeychainAttrType attrTag = attrList->attr[ix].tag;

            if (attrTag == APPLEDB_CSSM_PRINTNAME_ATTRIBUTE)
            {
                // must remap a caller-supplied kSecKeyPrintName attribute tag for key items, since it isn't in the schema
                // (note that this will ultimately match kGenericPrintName in Schema.cpp)
                attrTag = kSecLabelItemAttr;
            }

			mDbAttributes->add(Schema::attributeInfo(attrTag), CssmData(attrList->attr[ix].data,  attrList->attr[ix].length));
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
        DbAttributes dbAttributes(dbUniqueRecord()->database(), attrCount);
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
                attrList->attr[ix].length = (UInt32)dbAttributes.at(ix).Value[0].Length;

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
				*length=(UInt32)itemData.length();
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
		secinfo("kcnotify", "ItemImpl::getContent(%p, %p, %p, %p) retrieved content",
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
                // (note that this will ultimately match kGenericPrintName in Schema.cpp)
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
	DbAttributes dbAttributes(dbUniqueRecord()->database(), attrCount);
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
				attr[ix].length = (UInt32)dbAttributes.at(ix).Value[0].Length;

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

		if (length) *length=(UInt32)itemData.length();
		itemData.Length=0;

#if SENDACCESSNOTIFICATIONS
		secinfo("kcnotify", "ItemImpl::getAttributesAndData(%p, %p, %p, %p, %p) retrieved data",
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

	DbAttributes dbAttributes(dbUniqueRecord()->database(), 1);
	dbAttributes.add(Schema::attributeInfo(attr.tag));
	dbUniqueRecord()->get(&dbAttributes, NULL);
	getAttributeFrom(&dbAttributes.at(0), attr, actualLength);
}

void
ItemImpl::getAttributeFrom(CssmDbAttributeData *data, SecKeychainAttribute &attr, UInt32 *actualLength)
{
	StLock<Mutex>_(mMutex);
    static const uint32 zero = 0;
    UInt32 length;
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
        else if (data->format() == CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE)
            length = 0; // Should we throw here?
        else // All other formats
            length = 0;
	}
    else // Get the first value
    {
        length = (UInt32)data->Value[0].Length;
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
    secinfo("kcnotify", "ItemImpl::getData retrieved data");

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
			group = safer_cast<SSDbUniqueRecordImpl &>(*dbUniqueRecord()).group();
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
		UInt32 length = (UInt32)data->Length;
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
				UInt32 length = (UInt32)data->Value[0].Length;
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
    if (itemData)
    {
		Db db(dbUniqueRecord()->database());
		if (mDoNotEncrypt)
		{
			dbUniqueRecord()->getWithoutEncryption (dbAttributes, itemData);
			return;
		}
		if (useSecureStorage(db))
		{
            try {
                if(!checkIntegrity()) {
                    secnotice("integrity", "item has no integrity, denying access");
                    CssmError::throwMe(errSecInvalidItemRef);
                }
            } catch(CssmError cssme) {
                secnotice("integrity", "error while checking integrity, denying access: %s", cssme.what());
                throw cssme;
            }

			SSDbUniqueRecordImpl* impl = dynamic_cast<SSDbUniqueRecordImpl *>(&(*dbUniqueRecord()));
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

    dbUniqueRecord()->get(dbAttributes, itemData);
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

Item ItemImpl::makeFromPersistentReference(const CFDataRef persistentRef, bool *isIdentityRef)
{
	CssmData dictData((void*)::CFDataGetBytePtr(persistentRef), ::CFDataGetLength(persistentRef));
	NameValueDictionary dict(dictData);

	Keychain keychain;
	Item item = (ItemImpl *) NULL;

	if (isIdentityRef) {
		*isIdentityRef = (dict.FindByName(IDENTITY_KEY) != 0) ? true : false;
	}

	// make sure we have a database identifier
	if (dict.FindByName(SSUID_KEY) != 0)
	{
		DLDbIdentifier dlDbIdentifier = NameValueDictionary::MakeDLDbIdentifierFromNameValueDictionary(dict);
		DLDbIdentifier newDlDbIdentifier(dlDbIdentifier.ssuid(),
				DLDbListCFPref::ExpandTildesInPath(dlDbIdentifier.dbName()).c_str(),
				dlDbIdentifier.dbLocation());

		keychain = globals().storageManager.keychain(newDlDbIdentifier);

		const NameValuePair* aDictItem = dict.FindByName(ITEM_KEY);
		if (aDictItem && keychain)
		{
			PrimaryKey primaryKey(aDictItem->Value());
			item = keychain->item(primaryKey);
		}
	}
	KCThrowIf_( !item, errSecItemNotFound );
	return item;
}

void ItemImpl::copyPersistentReference(CFDataRef &outDataRef, bool isSecIdentityRef)
{
	if (secd_PersistentRef) {
		outDataRef = secd_PersistentRef;
		return;
	}
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

	if (isSecIdentityRef) {
		uint32_t value = -1;
		CssmData valueData((void*)&value, sizeof(value));
		dict.Insert (new NameValuePair(IDENTITY_KEY, valueData));
	}

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

bool ItemImpl::equal(SecCFObject &other)
{
	// First check to see if both items have a primary key and
	// if the primary key is the same.  If so then these
	// items must be equal
    ItemImpl& other_item = (ItemImpl&)other;
	if (mPrimaryKey != NULL && mPrimaryKey == other_item.mPrimaryKey)
	{
		return true;
	}

	// The primary keys do not match so do a CFHash of the
	// data of the item and compare those for equality
	CFHashCode this_hash = hash();
	CFHashCode other_hash = other.hash();
	return (this_hash == other_hash);
}

CFHashCode ItemImpl::hash()
{
	CFHashCode result = SecCFObject::hash();

	StLock<Mutex>_(mMutex);
	RefPointer<CssmDataContainer> data_to_hash;

	// Use the item data for the hash
	if (mData && *mData)
	{
		data_to_hash = mData;
	}

	// If there is no primary key AND not data ????
	// just return the 'old' hash value which is the
	// object pointer.
	if (NULL != data_to_hash.get())
	{
		CFDataRef temp_data = NULL;
		unsigned char digest[CC_SHA256_DIGEST_LENGTH];

		if (data_to_hash->length() < 80)
		{
			// If it is less than 80 bytes then CFData can be used
			temp_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
				(const UInt8 *)data_to_hash->data(), data_to_hash->length(), kCFAllocatorNull);

		}
		// CFData truncates its hash value to 80 bytes. ????
		// In order to do the 'right thing' a SHA 256 hash will be used to
		// include all of the data
		else
		{
			memset(digest, 0, CC_SHA256_DIGEST_LENGTH);

			CC_SHA256((const void *)data_to_hash->data(), (CC_LONG)data_to_hash->length(), digest);

			temp_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
				(const UInt8 *)digest, CC_SHA256_DIGEST_LENGTH, kCFAllocatorNull);
		}

		if (NULL != temp_data)
		{
			result = CFHash(temp_data);
			CFRelease(temp_data);
		}

	}

	return result;
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

CFIndex KeychainCore::GetItemRetainCount(Item& item)
{
	return CFGetRetainCount(item->handle(false));
}

void ItemImpl::setPersistentRef(CFDataRef ref)
{
	if (secd_PersistentRef) {
		CFRelease(secd_PersistentRef);
	}
	secd_PersistentRef = ref;
	CFRetain(ref);
}

CFDataRef ItemImpl::getPersistentRef()
{
	return secd_PersistentRef;
}



bool ItemImpl::mayDelete()
{
    ObjectImpl* uniqueIDImpl = mUniqueId.get();
    
    if (uniqueIDImpl != NULL)
    {
        bool result = mUniqueId->isIdle();
        return result;
    }
    else
    {
        return true;
    }
}
