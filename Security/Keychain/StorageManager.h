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


/*
	File:		StorageManager.h

	Contains:	Working with multiple keychains

	Copyright:	2000 by Apple Computer, Inc., all rights reserved.

	To Do:
*/

#ifndef _H_STORAGEMANAGER_
#define _H_STORAGEMANAGER_

#include <list>
#include <Security/multidldb.h>
#include <Security/DLDBListCFPref.h>
#include <Security/Keychains.h>

namespace Security
{

namespace KeychainCore
{

class StorageManager
{
    NOCOPY(StorageManager)
public:
	StorageManager();
    ~StorageManager() {}

    //bool onlist(const Keychain & keychain);

    // These will call addAndNotify() if the specified keychain already exists
    Keychain make(const char *fullPathName);
    void created(const Keychain &keychain); // Be notified a Keychain just got created.

	// Misc
    void lockAll();
    void reload(bool force = false);

    void add(const Keychain& keychainToAdd); // Only add if not there yet.  Doesn't write out CFPref

    // Vector-like methods.
	size_t size();
	Keychain at(unsigned int ix);
	Keychain operator[](unsigned int ix);

    void erase(const Keychain& keychainToRemove);

	KCCursor createCursor(const SecKeychainAttributeList *attrList);
	KCCursor createCursor(SecItemClass itemClass, const SecKeychainAttributeList *attrList);

     // Create KC if it doesn't exist, add to cache, but don't modify search list.	
    Keychain keychain(const DLDbIdentifier &dlDbIdentifier);

     // Create KC if it doesn't exist, add it to the search list if it is not already on it.
    Keychain makeKeychain(const DLDbIdentifier &dlDbIdentifier);


	// Keychain list maintenance
	void remove(const list<SecKeychainRef>& kcsToRemove);	    // remove keychains from list
	void replace(const list<SecKeychainRef>& newKCList);		// replace keychains list with new list
	void convert(const list<SecKeychainRef>& SecKeychainRefList,CssmClient::DLDbList& dldbList);	// maybe should be private

	// Login keychain support
	void login(ConstStringPtr name, ConstStringPtr password);
	void login(UInt32 nameLength, const void *name, UInt32 passwordLength, const void *password);
	void logout();
	void changeLoginPassword(ConstStringPtr oldPassword, ConstStringPtr newPassword);
	void changeLoginPassword(UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword);

private:
    typedef map<DLDbIdentifier, Keychain> KeychainMap;
	typedef set<KeychainSchema> KeychainSchemaSet;

    // Only add if not there yet.  Writes out CFPref and broadcasts KCPrefListChanged notification
	void addAndNotify(const Keychain& keychainToAdd);
	KeychainSchema keychainSchemaFor(const CssmClient::Db &db);

	//Mutex mKeychainsLock;
    DLDbListCFPref mSavedList;
    KeychainMap mKeychains;		// the array of Keychains
    CssmClient::MultiDLDb mMultiDLDb;
	KeychainSchemaSet mKeychainSchemaSet;
};

} // end namespace KeychainCore

} // end namespace Security

#endif /* _H_STORAGEMANAGER_ */

