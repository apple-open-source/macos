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
	File:		StorageManager.cpp

	Contains:	Working with multiple keychains

	Copyright:	2000 by Apple Computer, Inc., all rights reserved.

	To Do:
*/

#include "StorageManager.h"
#include "KCEventNotifier.h"

#include <Security/cssmapple.h>
#include <sys/types.h>
#include <pwd.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <algorithm>
#include <string>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#include <Security/AuthSession.h>
#include <Security/debugging.h>

#include "KCCursor.h"
#include "Globals.h"
#include "DefaultKeychain.h"

using namespace CssmClient;
using namespace KeychainCore;

StorageManager::StorageManager() :
    mSavedList(),
    mKeychains(),
    mMultiDLDb(mSavedList.list(), true) // Passinng true enables use of Secure Storage
{
}

// Create KC if it doesn't exist	
Keychain
StorageManager::keychain(const DLDbIdentifier &dLDbIdentifier)
{
	//StLock<Mutex> _(mKeychainsLock);
    KeychainMap::iterator it = mKeychains.find(dLDbIdentifier);
    if (it != mKeychains.end())
		return it->second;

	// The keychain is not in our cache.  Create it.
	Keychain keychain(mMultiDLDb->database(dLDbIdentifier));

	// Add the keychain to the cache.
	mKeychains.insert(KeychainMap::value_type(dLDbIdentifier, keychain));
	return keychain;
}

// Create KC if it doesn't exist	
Keychain
StorageManager::makeKeychain(const DLDbIdentifier &dLDbIdentifier)
{
	Keychain keychain(keychain(dLDbIdentifier));

	const vector<DLDbIdentifier> &list = mMultiDLDb->list();
	if (find(list.begin(), list.end(), dLDbIdentifier) != list.end())
	{
		// The dLDbIdentifier for this keychain is already on our search list.
		return keychain;
	}

	// If the keychain doesn't exist don't bother adding it to the search list yet.
	if (!keychain->exists())
		return keychain;

	// The keychain exists and is not in our search list add it to the search
	// list and the cache.  Then inform mMultiDLDb.
	mSavedList.revert(true);
	mSavedList.add(dLDbIdentifier);
	mSavedList.save();

	// @@@ Will happen again when kSecKeychainListChangedEvent notification is received.
	mMultiDLDb->list(mSavedList.list());

	KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);

	return keychain;
}

void
StorageManager::created(const Keychain &keychain) // Be notified a Keychain just got created.
{
    DLDbIdentifier dLDbIdentifier = keychain->dLDbIdentifier();
	
    // If we don't have a default Keychain yet.  Make the newly created keychain the default.
    DefaultKeychain &defaultKeychain = globals().defaultKeychain;
    if (!defaultKeychain.isSet())
        defaultKeychain.dLDbIdentifier(dLDbIdentifier);

	// Add the keychain to the search list and the cache.  Then inform mMultiDLDb.
	mSavedList.revert(true);
	mSavedList.add(dLDbIdentifier);
	mSavedList.save();

	// @@@ Will happen again when kSecKeychainListChangedEvent notification is received.
	mMultiDLDb->list(mSavedList.list());

	KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
}


KCCursor
StorageManager::createCursor(SecItemClass itemClass, const SecKeychainAttributeList *attrList)
{
	return KCCursor(DbCursor(mMultiDLDb), itemClass, attrList);
}

KCCursor
StorageManager::createCursor(const SecKeychainAttributeList *attrList)
{
	return KCCursor(DbCursor(mMultiDLDb), attrList);
}

void
StorageManager::lockAll()
{
    for (KeychainMap::iterator ix = mKeychains.begin(); ix != mKeychains.end(); ix++)
	{
		Keychain keychain(ix->second);
		if (keychain->isActive())
			keychain->lock();
	}
}

void
StorageManager::reload(bool force)
{
    // Reinitialize list from CFPrefs if changed.  When force is true force a prefs revert now.
    if (mSavedList.revert(force))
        mMultiDLDb->list(mSavedList.list());
}

size_t
StorageManager::size()
{
    reload();
    return mMultiDLDb->list().size();
}

Keychain
StorageManager::at(unsigned int ix)
{
    reload();
    if (ix >= mMultiDLDb->list().size())
        MacOSError::throwMe(errSecInvalidKeychain);

    return keychain(mMultiDLDb->list().at(ix));
}

Keychain
StorageManager::operator[](unsigned int ix)
{
    return at(ix);
}	

void StorageManager::remove(const list<SecKeychainRef>& kcsToRemove)
{
	//StLock<Mutex> _(mKeychainsLock);
	mSavedList.revert(true);
	DLDbIdentifier defaultId = globals().defaultKeychain.dLDbIdentifier();
	bool unsetDefault=false;
    for (list<SecKeychainRef>::const_iterator ix = kcsToRemove.begin();ix!=kcsToRemove.end();ix++)
	{
		// Find the keychain object for the given ref
		Keychain keychainToRemove;
		try
		{
			keychainToRemove = KeychainRef::required(*ix);
		}
		catch (const MacOSError& err)
		{
			if (err.osStatus() == errSecInvalidKeychain)
				continue;
			throw;
		}
		
		// Remove it from the saved list
		mSavedList.remove(keychainToRemove->dLDbIdentifier());
		if (keychainToRemove->dLDbIdentifier() == defaultId)
			unsetDefault=true;
		// Now remove it from the map
		KeychainMap::iterator it = mKeychains.find(keychainToRemove->dLDbIdentifier());
		if (it==mKeychains.end())
			continue;
		mKeychains.erase(it);
	}
	mSavedList.save();
	mMultiDLDb->list(mSavedList.list());
	KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	if (unsetDefault)
		globals().defaultKeychain.unset();
}

void StorageManager::replace(const list<SecKeychainRef>& newKCList)
{
	// replace keychains list with new list
	CssmClient::DLDbList dldbList;
	convert(newKCList,dldbList);
}

void StorageManager::convert(const list<SecKeychainRef>& SecKeychainRefList,CssmClient::DLDbList& dldbList)
{
    // Convert a list of SecKeychainRefs to a DLDbList
	dldbList.clear();		// If we don't clear list, we should use "add" instead of push_back
	for (list<SecKeychainRef>::const_iterator ix = SecKeychainRefList.begin();ix!=SecKeychainRefList.end();ix++)
	{
		// Find the keychain object for the given ref
		Keychain keychain;
		try
		{
			keychain = KeychainRef::required(*ix);
		}
		catch (const MacOSError& err)
		{
			if (err.osStatus() == errSecInvalidKeychain)
				continue;
			throw;
		}
		
		// Add it to the list
		dldbList.push_back(keychain->dLDbIdentifier());
	}
}


#pragma mark ÑÑÑÑ Login Functions ÑÑÑÑ

void StorageManager::login(ConstStringPtr name, ConstStringPtr password)
{
    if ( name == NULL || password == NULL )
        MacOSError::throwMe(paramErr);

	login(name[0], name + 1, password[0], password + 1);
}

void StorageManager::login(UInt32 nameLength, const void *name, UInt32 passwordLength, const void *password)
{
    // @@@ set up the login session on behalf of loginwindow
    // @@@ (this code should migrate into loginwindow)
    debug("KClogin", "setting up login session");
    if (OSStatus ssnErr = SessionCreate(sessionKeepCurrentBootstrap,
            sessionHasGraphicAccess | sessionHasTTY))
        debug("KClogin", "session setup failed status=%ld", ssnErr);

    if (name == NULL || (passwordLength != 0 && password == NULL))
        MacOSError::throwMe(paramErr);

	// Make sure name is zero terminated
	string theName(reinterpret_cast<const char *>(name), nameLength);
	Keychain keychain = make(theName.c_str());
	try
	{
		keychain->unlock(CssmData(const_cast<void *>(password), passwordLength));
        debug("KClogin", "keychain unlock successful");
	}
	catch(const CssmError &e)
	{
		if (e.osStatus() != CSSMERR_DL_DATASTORE_DOESNOT_EXIST)
			throw;
        debug("KClogin", "creating login keychain");
		keychain->create(passwordLength, password);
		// Login Keychain does not lock on sleep nor lock after timeout by default.
		keychain->setSettings(INT_MAX, false);
	}

	// @@@ Create a authorization credential for the current user.
    debug("KClogin", "creating login authorization");
	const AuthorizationItem envList[] =
	{
		{ kAuthorizationEnvironmentUsername, nameLength, const_cast<void *>(name), 0 },
		{ kAuthorizationEnvironmentPassword, passwordLength, const_cast<void *>(password), 0 },
		{ kAuthorizationEnvironmentShared, 0, NULL, 0 }
	};
	const AuthorizationEnvironment environment =
	{
		sizeof(envList) / sizeof(*envList),
		const_cast<AuthorizationItem *>(envList)
	};
	if (OSStatus authErr = AuthorizationCreate(NULL, &environment,
            kAuthorizationFlagExtendRights | kAuthorizationFlagPreAuthorize, NULL))
        debug("KClogin", "failed to create login auth, status=%ld", authErr);
}

void StorageManager::logout()
{
    // nothing left to do here
}

void StorageManager::changeLoginPassword(ConstStringPtr oldPassword, ConstStringPtr newPassword)
{
	globals().defaultKeychain.keychain()->changePassphrase(oldPassword, newPassword);
}


void StorageManager::changeLoginPassword(UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword)
{
	globals().defaultKeychain.keychain()->changePassphrase(oldPasswordLength, oldPassword,  newPasswordLength, newPassword);
}

#pragma mark ÑÑÑÑ File Related ÑÑÑÑ

Keychain StorageManager::make(const char *pathName)
{
	string fullPathName;
    if ( pathName[0] == '/' )
		fullPathName = pathName;
	else
    {
		// Get Home directory from environment.
		const char *homeDir = getenv("HOME");
		if (homeDir == NULL)
		{
			// If $HOME is unset get the current users home directory from the passwd file.
			struct passwd *pw = getpwuid(getuid());
			if (!pw)
				MacOSError::throwMe(paramErr);

			homeDir = pw->pw_dir;
		}

		fullPathName = homeDir;
		fullPathName += "/Library/Keychains/";
		fullPathName += pathName;
	}

    const CSSM_NET_ADDRESS *DbLocation = NULL;	// NULL for keychains
    const CSSM_VERSION *version = NULL;
    uint32 subserviceId = 0;
    CSSM_SERVICE_TYPE subserviceType = CSSM_SERVICE_DL | CSSM_SERVICE_CSP;
    const CssmSubserviceUid ssuid( gGuidAppleCSPDL, version, 
                                   subserviceId, subserviceType );
	DLDbIdentifier dLDbIdentifier( ssuid, fullPathName.c_str(), DbLocation );
	return makeKeychain( dLDbIdentifier );
}

KeychainSchema
StorageManager::keychainSchemaFor(const CssmClient::Db &db)
{
	KeychainSchema schema(db);
	pair<KeychainSchemaSet::iterator, bool> result = mKeychainSchemaSet.insert(db);
	if (result.second)
		return schema;
	return *result.first;
}

