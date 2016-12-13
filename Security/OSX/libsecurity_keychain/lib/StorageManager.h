/*
 * Copyright (c) 2000-2012 Apple Inc. All Rights Reserved.
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
// StorageManager.h -- Working with multiple keychains
//
#ifndef _SECURITY_STORAGEMANAGER_H_
#define _SECURITY_STORAGEMANAGER_H_

#include <list>
#include <set>
#include <security_keychain/DLDBListCFPref.h>
#include <security_keychain/DynamicDLDBList.h>
#include <security_keychain/Keychains.h>
#include <security_keychain/KeyItem.h>
#include <Security/Authorization.h>

#define kLegacyKeychainRenamedSuffix    "_renamed"
#define kKeychainRenamedSuffix          "_renamed_"

namespace Security
{

namespace KeychainCore
{

class StorageManager
{
    NOCOPY(StorageManager)
public:
    typedef vector<Keychain> KeychainList;
	typedef vector<DLDbIdentifier> DLDbList;

	StorageManager();
    ~StorageManager() {}

	Mutex* getStorageManagerMutex();
	
    //bool onlist(const Keychain & keychain);

    // These will call addAndNotify() if the specified keychain already exists
	Keychain make(const char *fullPathName);
    Keychain make(const char *fullPathName, bool add);
    Keychain make(const char *fullPathName, bool add, bool isReset);
    Keychain makeLoginAuthUI(const Item *item, bool isReset);
    void created(const Keychain &keychain); // Be notified a Keychain just got created.

	// Misc
    void lockAll();

    void add(const Keychain& keychainToAdd); // Only add if not there yet.  Doesn't write out CFPref

    // Vector-like methods.
	size_t size();
	Keychain at(unsigned int ix);
	Keychain operator[](unsigned int ix);

	KCCursor createCursor(const SecKeychainAttributeList *attrList);
	KCCursor createCursor(SecItemClass itemClass, const SecKeychainAttributeList *attrList);

	// Lookup a keychain object in the cache.  If it doesn't exist, create a
	// new one and add to cache. Doesn't modify search lists.
	// Note this doesn't create an actual database just a reference to one
	// that may or may not exist.
    Keychain keychain(const DLDbIdentifier &dLDbIdentifier);

	// Remove a keychain from the cache if it's in it.
	void removeKeychain(const DLDbIdentifier &dLDbIdentifier, KeychainImpl *keychainImpl);
	// Be notified a (smart card) keychain was removed.
	void didRemoveKeychain(const DLDbIdentifier &dLDbIdentifier);
	
	// Create KC if it doesn't exist, add it to the search list if it exists and is not already on it.
    Keychain makeKeychain(const DLDbIdentifier &dLDbIdentifier, bool add, bool isReset);

    // Reload a keychain from the on-disk database
    void reloadKeychain(Keychain keychain);

    // Register a keychain in the keychain cache
    void registerKeychain(Keychain& kc);
    void registerKeychainImpl(KeychainImpl* kc);

	// Keychain list maintenance

	// remove kcsToRemove from the search list
	void remove(const KeychainList &kcsToRemove, bool deleteDb = false);

	void getSearchList(KeychainList &keychainList);
	void setSearchList(const KeychainList &keychainList);
	void forceUserSearchListReread ();

	void getSearchList(SecPreferencesDomain domain, KeychainList &keychainList);
	void setSearchList(SecPreferencesDomain domain, const KeychainList &keychainList);

    void rename(Keychain keychain, const char* newName);
    void renameUnique(Keychain keychain, CFStringRef oldName, CFStringRef newName, bool appendDbSuffix);

	// Iff keychainOrArray is NULL return the default KeychainList in keychainList otherwise
	// if keychainOrArray is a CFArrayRef containing SecKeychainRef's convernt it to KeychainList,
	// if keychainOrArray is a SecKeychainRef return a KeychainList with one element.
	void optionalSearchList(CFTypeRef keychainOrArray, KeychainList &keychainList);

	// Convert CFArrayRef of SecKeychainRef's a KeychainList.  The array must not be NULL
	static void convertToKeychainList(CFArrayRef keychainArray, KeychainList &keychainList);

	// Convert KeychainList to a CFArrayRef of SecKeychainRef's.
	static CFArrayRef convertFromKeychainList(const KeychainList &keychainList);

	// Login keychain support
    void login(AuthorizationRef authRef, UInt32 nameLength, const char* name, bool isReset);
	void login(ConstStringPtr name, ConstStringPtr password);
	void login(UInt32 nameLength, const void *name, UInt32 passwordLength, const void *password, bool isReset);
    void stashLogin();
    void stashKeychain();
	void logout();
	void changeLoginPassword(ConstStringPtr oldPassword, ConstStringPtr newPassword);
	void changeLoginPassword(UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword);

    // Token login support
    CFDataRef getTokenLoginMasterKey(UInt32 passwordLength, const void *password);
    CFDataRef unwrapTokenLoginMasterKey(CFDictionaryRef masterKeyData, CFStringRef tokenID, CFStringRef pin);
    
    void resetKeychain(Boolean resetSearchList);

	Keychain defaultKeychain();
    Keychain defaultKeychainUI(Item &item);
	void defaultKeychain(const Keychain &keychain);

	Keychain loginKeychain();
    DLDbIdentifier loginKeychainDLDbIdentifer();

	void loginKeychain(Keychain keychain);
	
	Keychain defaultKeychain(SecPreferencesDomain domain);
	void defaultKeychain(SecPreferencesDomain domain, const Keychain &keychain);
	
	SecPreferencesDomain domain() { return mDomain; }
	void domain(SecPreferencesDomain newDomain);
	
	bool keychainOwnerPermissionsValidForDomain(const char* path, SecPreferencesDomain domain);

	// non-file based Keychain manipulation
	void addToDomainList(SecPreferencesDomain domain, const char* dbName, const CSSM_GUID &guid, uint32 subServiceType);
	void isInDomainList(SecPreferencesDomain domain, const char* dbName, const CSSM_GUID &guid, uint32 subServiceType);
	void removeFromDomainList(SecPreferencesDomain domain, const char* dbName, const CSSM_GUID &guid, uint32 subServiceType);
	
private:
	static void convertList(DLDbList &ids, const KeychainList &kcs);
	void convertList(KeychainList &kcs, const DLDbList &ids);

    DLDbIdentifier makeDLDbIdentifier(const char* pathName);
    CssmClient::Db makeDb(DLDbIdentifier dLDbIdentifier);

    // Use this when you want to be extra sure this keychain is removed from the
    // cache. Iterates over the whole cache to find all instances. This function
    // will take the cache map mutex.
    void forceRemoveFromCache(KeychainImpl* inItemImpl);

public:
    // Change the DLDBIdentifier to reflect the files on-disk. Currently:
    //   If the keychain is in ~/Library/Keychains and either
    //     the .keychain-db version of the file exists or
    //     (global integrity protection is on AND isReset is true)
    //  then change the filename to include ".keychain-db".
    //
    //  Otherwise, leave it alone.
    static DLDbIdentifier mungeDLDbIdentifier(const DLDbIdentifier& dLDbIdentifier, bool isReset);

    // Change the DLDbIdentifier to always use the pattern ending with "-db".
    static DLDbIdentifier forceMungeDLDbIDentifier(const DLDbIdentifier& dLDbIdentifier);

    // Due to compatibility requirements, we need the DLDbListCFPref lists to
    // never see a ".keychain-db" filename. Call this function to give them what
    // they need.
    static DLDbIdentifier demungeDLDbIdentifier(const DLDbIdentifier& dLDbIdentifier);

    // Take a filename, and give it the extension .keychain-db
    static string makeKeychainDbFilename(const string& filename);

    // Check if a keychain path is in some user's ~/Library/Keychains/ folder.
    static bool pathInHomeLibraryKeychains(const string& path);

    // Notify the StorageManager that you're accessing this keychain. Used for
    // time-based caching purposes.
    void tickleKeychain(KeychainImpl *keychainImpl);

private:
    // Only add if not there yet.  Writes out CFPref and broadcasts KCPrefListChanged notification
	void addAndNotify(const Keychain& keychainToAdd);

	// remove a keychain from the sync list
	void removeKeychainFromSyncList (const DLDbIdentifier &id);

    typedef map<DLDbIdentifier, KeychainImpl *> KeychainMap;
	// Reference map of all keychains we know about that aren't deleted
	// or removed
    KeychainMap mKeychainMap;

	// The dynamic search list.
	DynamicDLDBList mDynamicList;

	DLDbListCFPref mSavedList;
	DLDbListCFPref mCommonList;
	SecPreferencesDomain mDomain; // current domain (in mSavedList and cache fields)
	Mutex mMutex;
	RecursiveMutex mKeychainMapMutex;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_STORAGEMANAGER_H_
