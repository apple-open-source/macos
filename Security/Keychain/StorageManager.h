/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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
// StorageManager.h -- Working with multiple keychains
//
#ifndef _SECURITY_STORAGEMANAGER_H_
#define _SECURITY_STORAGEMANAGER_H_

#include <list>
#include <set>
#include <Security/multidldb.h>
#include <Security/DLDBListCFPref.h>
#include <Security/Keychains.h>
#include <Security/KeyItem.h>
#include <Security/Authorization.h>

#define kKeychainRenamedSuffix "_renamed"

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

    //bool onlist(const Keychain & keychain);

    // These will call addAndNotify() if the specified keychain already exists
	Keychain make(const char *fullPathName);
    Keychain make(const char *fullPathName, bool add);
    Keychain makeLoginAuthUI(Item &item);
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

	// Create KC if it doesn't exist, add to cache, but don't modify search list.	
    Keychain keychain(const DLDbIdentifier &dLDbIdentifier);

	// Same as keychain(const DLDbIdentifier &) but assumes mLock is already held.
    Keychain _keychain(const DLDbIdentifier &dLDbIdentifier);

	// Create KC if it doesn't exist, add it to the search list if it exists and is not already on it.
    Keychain makeKeychain(const DLDbIdentifier &dLDbIdentifier, bool add = true);


	// Keychain list maintenance

	// remove kcsToRemove from the search list
	void remove(const KeychainList &kcsToRemove, bool deleteDb = false);

	void getSearchList(KeychainList &keychainList);
	void setSearchList(const KeychainList &keychainList);
	
	void getSearchList(SecPreferencesDomain domain, KeychainList &keychainList);
	void setSearchList(SecPreferencesDomain domain, const KeychainList &keychainList);

    void rename(Keychain keychain, const char* newName);
    void renameUnique(Keychain keychain, CFStringRef newName);

	// Iff keychainOrArray is NULL return the default KeychainList in keychainList otherwise
	// if keychainOrArray is a CFArrayRef containing SecKeychainRef's convernt it to KeychainList,
	// if keychainOrArray is a SecKeychainRef return a KeychainList with one element.
	void optionalSearchList(CFTypeRef keychainOrArray, KeychainList &keychainList);

	// Convert CFArrayRef of SecKeychainRef's a KeychainList.  The array must not be NULL
	static void convertToKeychainList(CFArrayRef keychainArray, KeychainList &keychainList);

	// Convert KeychainList to a CFArrayRef of SecKeychainRef's.
	static CFArrayRef convertFromKeychainList(const KeychainList &keychainList);

	// Login keychain support
    void login(AuthorizationRef authRef, UInt32 nameLength, const char* name);
	void login(ConstStringPtr name, ConstStringPtr password);
	void login(UInt32 nameLength, const void *name, UInt32 passwordLength, const void *password);
	void logout();
	void changeLoginPassword(ConstStringPtr oldPassword, ConstStringPtr newPassword);
	void changeLoginPassword(UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword);

    void resetKeychain(Boolean resetSearchList);

	Keychain defaultKeychain();
    Keychain defaultKeychainUI(Item &item);
	void defaultKeychain(const Keychain &keychain);

	Keychain loginKeychain();
	void loginKeychain(Keychain keychain);
	
	Keychain defaultKeychain(SecPreferencesDomain domain);
	void defaultKeychain(SecPreferencesDomain domain, const Keychain &keychain);
	
	SecPreferencesDomain domain() { return mDomain; }
	void domain(SecPreferencesDomain newDomain);

	// To be called by KeychainImpl destructor only.
	void removeKeychain(const DLDbIdentifier &dLDbIdentifier, KeychainImpl *keychainImpl); 

private:
    typedef map<DLDbIdentifier, KeychainImpl *> KeychainMap;

	static void convertList(DLDbList &ids, const KeychainList &kcs);
	void convertList(KeychainList &kcs, const DLDbList &ids);

    // Only add if not there yet.  Writes out CFPref and broadcasts KCPrefListChanged notification
	void addAndNotify(const Keychain& keychainToAdd);
	
	// set default credentials for opening a keychain
	void setDefaultCredentials(const CssmClient::Db &db);

	DLDbListCFPref mSavedList;
	DLDbListCFPref mCommonList;
	SecPreferencesDomain mDomain; // current domain (in mSavedList and cache fields)
    KeychainMap mKeychains;		// the cache of Keychains
	Mutex mLock;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_STORAGEMANAGER_H_
