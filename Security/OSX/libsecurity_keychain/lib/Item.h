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
// Item.h
//
#ifndef _SECURITY_ITEM_H_
#define _SECURITY_ITEM_H_

#include <security_keychain/Keychains.h>
#include <security_keychain/PrimaryKey.h>
#include <security_cdsa_client/securestorage.h>
#include <security_keychain/Access.h>

namespace Security
{

using namespace CssmClient;

namespace KeychainCore
{
class Keychain;

class ItemImpl : public SecCFObject
{
public:
	SECCFFUNCTIONS_CREATABLE(ItemImpl, SecKeychainItemRef, gTypes().ItemImpl)

    static ItemImpl *required(SecKeychainItemRef ptr);
    static ItemImpl *optional(SecKeychainItemRef ptr);

    friend class Item;
	friend class KeychainImpl;
protected:

	// new item constructors
    ItemImpl(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data, bool inhibitCheck = false);

	ItemImpl(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void* data);

	// db item constructor
    ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);

	// PrimaryKey item constructor
    ItemImpl(const Keychain &keychain, const PrimaryKey &primaryKey);

public:

	static ItemImpl* make(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);
	static ItemImpl* make(const Keychain &keychain, const PrimaryKey &primaryKey);

	ItemImpl(ItemImpl &item);

	// Return true if we got the attribute, false if we only got the actualLength.
	void getAttributeFrom(CssmDbAttributeData *data, SecKeychainAttribute &attr,  UInt32 *actualLength);
	void getClass(SecKeychainAttribute &attr,  UInt32 *actualLength);

	// For iOS keys
	void setPersistentRef(CFDataRef ref);
	// returns NULL for securityd keys, or the (non-NULL) persistent ref for iOS keys
	CFDataRef getPersistentRef();
	
	PrimaryKey addWithCopyInfo(Keychain &keychain, bool isCopy);
	Mutex* getMutexForObject() const;

    // Return true iff the item integrity has not been compromised.
    virtual bool checkIntegrity();
    bool checkIntegrity(AclBearer& key);
    static bool checkIntegrityFromDictionary(AclBearer& key, DbAttributes* dbAttributes);

protected:
	// Methods called by KeychainImpl;

	// Add the receiver to keychain
	virtual PrimaryKey add(Keychain &keychain);

    // Prepare a dbAttributes to extract all possible attributes with a call to
    // getContent.
    void fillDbAttributesFromSchema(DbAttributes& dbAttributes, CSSM_DB_RECORDTYPE recordType, Keychain keychain = NULL);

    // Get all current attributes of this item. This will call out to the
    // database (if there is one) and then overly the current pending updates.
    // You must delete the returned object.
    DbAttributes* getCurrentAttributes();

    // Return a canonical form of this item's attributes
    void encodeAttributes(CssmOwnedData &attributeBlob);

    // Return a canonical form of the attributes passed in
    static void encodeAttributesFromDictionary(CssmOwnedData &attributeBlob, DbAttributes* dbAttributes);

    // Return a canonical digest of the record type and attributes of the item
    void computeDigest(CssmOwnedData &sha2);

    // Return a canonical digest of the record type and attributes passed in
    static void computeDigestFromDictionary(CssmOwnedData &sha2, DbAttributes* dbAttributes);

	// Get the default value for an attribute
	static const CSSM_DATA &defaultAttributeValue(const CSSM_DB_ATTRIBUTE_INFO &info);

public:
    virtual ~ItemImpl();
    bool isPersistent();
    bool isModified();

	virtual void update();

	void aboutToDestruct();

	// put a copy of the item into a given keychain
	virtual Item copyTo(const Keychain &keychain, Access *newAccess = NULL);

    CSSM_DB_RECORDTYPE recordType();

	// Used for writing the record to the database.
    CssmClient::DbUniqueRecord dbUniqueRecord();
	const CssmClient::DbAttributes *modifiedAttributes();
	const CssmData *modifiedData();
	virtual void didModify(); // Forget any attributes and data we just wrote to the db

	Keychain keychain();
	PrimaryKey primaryKey();
	bool operator < (const ItemImpl &other);

	void getAttribute(SecKeychainAttribute& attr,  UInt32 *actualLength);
	void getData(CssmDataContainer& outData);

	void modifyContent(const SecKeychainAttributeList *attrList, UInt32 dataLength, const void *inData);
	void getContent(SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData);
	static void freeContent(SecKeychainAttributeList *attrList, void *data);
	static void freeAttributesAndData(SecKeychainAttributeList *attrList, void *data);

	void getAttributesAndData(SecKeychainAttributeInfo *info, SecItemClass *itemClass,
							  SecKeychainAttributeList **attrList, UInt32 *length, void **outData);
	void modifyAttributesAndData(const SecKeychainAttributeList *attrList, UInt32 dataLength, const void *inData);

	void setAttribute(SecKeychainAttribute& attr);
	void setAttribute(const CssmDbAttributeInfo &info, const CssmPolyData &data);
	void setData(UInt32 length,const void *data);
	void setAccess(Access *newAccess);
	void copyRecordIdentifier(CSSM_DATA &data);
	SSGroup group();

    void getContent(DbAttributes *dbAttributes, CssmDataContainer *itemData);
    void getLocalContent(SecKeychainAttributeList *attributeList, UInt32 *outLength, void **outData);

    bool useSecureStorage(const CssmClient::Db &db);
	virtual void willRead();

    // create a persistent reference to this item
    void copyPersistentReference(CFDataRef &outDataRef, bool isSecIdentityRef=false);
	static Item makeFromPersistentReference(const CFDataRef persistentRef, bool *isIdentityRef=NULL);

	// for keychain syncing
	void doNotEncrypt () {mDoNotEncrypt = true;}

	// for posting events on this item
	void postItemEvent (SecKeychainEvent theEvent);

	// Only call these functions while holding globals().apiLock.
	bool inCache() const throw() { return mInCache; }
	void inCache(bool inCache) throw() { mInCache = inCache; }

	/* For binding to extended attributes. */
	virtual const CssmData &itemID();

	/* Overrides for SecCFObject methods */
	bool equal(SecCFObject &other);
    virtual CFHashCode hash();
	
    bool mayDelete();
    
protected:

    /* Saves the item with a new SSGroup and ACL. If you pass in an old SSGroup,
     * the ACL will be copied from the old group, and the old group deleted. */
    void updateSSGroup(Db& db, CSSM_DB_RECORDTYPE recordType, CssmDataContainer* data, Keychain keychain = NULL, SecPointer<Access> access = NULL);

    // Helper function to abstract out error handling. Does not report any errors.
    void deleteSSGroup(SSGroup & ssgroup, const AccessCredentials* nullCred);

    void doChange(Keychain keychain, CSSM_DB_RECORDTYPE recordType, void (^tryChange) () );

    // Add integrity acl entry to access.
    void addIntegrity(Access &access, bool force = false);

    // Set the integrity of this item to whatever my attributes are now
    // If force, then perform this even if the underlying keychain claims to not
    // support it. (This is needed because during an upgrade, the underlying
    // keychain is confused about its actual version until it's written to disk.)
    virtual void setIntegrity(bool force = false);

    // Set the integrity of this bearer to be whatever my attributes are now
    virtual void setIntegrity(AclBearer &bearer, bool force = false);

    // Call this function to remove the integrity and partition_id ACLs from
    // this item. You're not supposed to be able to do this, so force the issue
    // by providing credentials to this keychain.
    virtual void removeIntegrity(const AccessCredentials *cred);
    virtual void removeIntegrity(AclBearer &bearer, const AccessCredentials *cred);

	// new item members
	RefPointer<CssmDataContainer> mData;
	auto_ptr<CssmClient::DbAttributes> mDbAttributes;
	SecPointer<Access> mAccess;

	// db item members
	CssmClient::DbUniqueRecord mUniqueId;
	Keychain mKeychain;
	PrimaryKey mPrimaryKey;

	// non-NULL only for secd items (managed by secd, not securityd)
	CFDataRef secd_PersistentRef;

private:
	// keychain syncing flags
	bool mDoNotEncrypt;

	// mInCache is protected by globals().apiLock
	// True iff we are in the cache of items in mKeychain
	bool mInCache;

protected:
	Mutex mMutex;
};


class Item : public SecPointer<ItemImpl>
{
public:
    Item();
    Item(ItemImpl *impl);
    Item(SecItemClass itemClass, OSType itemCreator, UInt32 length, const void* data, bool inhibitCheck);
	Item(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void* data);
    Item(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);
    Item(const Keychain &keychain, const PrimaryKey &primaryKey);
	Item(ItemImpl &item);
};


CFIndex GetItemRetainCount(Item& item);

} // end namespace KeychainCore

} // end namespace Security



#endif // !_SECURITY_ITEM_H_
