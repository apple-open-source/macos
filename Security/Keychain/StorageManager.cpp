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


/*
	File:		StorageManager.cpp

	Contains:	Working with multiple keychains

*/

#include "StorageManager.h"
#include "KCEventNotifier.h"

#include <Security/cssmapple.h>
#include <sys/types.h>
#include <sys/param.h>
#include <syslog.h>
#include <pwd.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <algorithm>
#include <string>
#include <Security/AuthorizationTags.h>
#include <Security/AuthSession.h>
#include <Security/debugging.h>
#include <Security/SecCFTypes.h>
#include <Security/AuthSession.h>
#include <Security/SecurityAgentClient.h>
#include <Security/ssclient.h>

#include "KCCursor.h"
#include "Globals.h"

using namespace CssmClient;
using namespace KeychainCore;

// normal debug calls, which get stubbed out for deployment builds
#define x_debug(str) secdebug("KClogin",(str))
#define x_debug1(fmt,arg1) secdebug("KClogin",(fmt),(arg1))
#define x_debug2(fmt,arg1,arg2) secdebug("KClogin",(fmt),(arg1),(arg2))

//-----------------------------------------------------------------------------------

StorageManager::StorageManager() :
	mSavedList(kSecPreferencesDomainUser),
	mCommonList(kSecPreferencesDomainCommon),
	mDomain(kSecPreferencesDomainUser),
    mKeychains()
{
	// get session attributes
	SessionAttributeBits sessionAttrs;
	if (OSStatus err = SessionGetInfo(callerSecuritySession,
		NULL, &sessionAttrs))
			CssmError::throwMe(err);
	
	// If this is the root session, switch to system preferences.
	// (In SecurityServer debug mode, you'll get a (fake) root session
	// that has graphics access. Ignore that to help testing.)
	if ((sessionAttrs & sessionIsRoot)
			IFDEBUG( && !(sessionAttrs & sessionHasGraphicAccess))) {
		secdebug("storagemgr", "switching to system preferences");
		mDomain = kSecPreferencesDomainSystem;
		mSavedList.set(kSecPreferencesDomainSystem);
	}
}

// Create KC if it doesn't exist	
Keychain
StorageManager::keychain(const DLDbIdentifier &dLDbIdentifier)
{
	StLock<Mutex> _(mLock);
	return _keychain(dLDbIdentifier);
}

Keychain
StorageManager::_keychain(const DLDbIdentifier &dLDbIdentifier)
{
	if (!dLDbIdentifier)
		return Keychain();

    KeychainMap::iterator it = mKeychains.find(dLDbIdentifier);
    if (it != mKeychains.end())
		return it->second;

	// The keychain is not in our cache.  Create it.
	Module module(dLDbIdentifier.ssuid().guid());
	DL dl;
	if (dLDbIdentifier.ssuid().subserviceType() & CSSM_SERVICE_CSP)
		dl = SSCSPDL(module);
	else
		dl = DL(module);

	dl->subserviceId(dLDbIdentifier.ssuid().subserviceId());
	dl->version(dLDbIdentifier.ssuid().version());
	Db db(dl, dLDbIdentifier.dbName());

	Keychain keychain(db);
	// Add the keychain to the cache.
	mKeychains.insert(KeychainMap::value_type(dLDbIdentifier, &*keychain));

	return keychain;
}

// Called from KeychainImpl's destructor remove it from the map.
void 
StorageManager::removeKeychain(const DLDbIdentifier &dLDbIdentifier, KeychainImpl *keychainImpl)
{
	// @@@ Work out locking StLock<Mutex> _(mLock);
	KeychainMap::iterator it = mKeychains.find(dLDbIdentifier);
	if (it != mKeychains.end() && it->second == keychainImpl)
		mKeychains.erase(it);
}

// if a database is key-unlockable, authenticate it with any matching unlock keys found in the KC list
void StorageManager::setDefaultCredentials(const Db &db)
{
	try {
		CssmAutoData index(db->allocator());
		if (!db->getUnlockKeyIndex(index.get()))
			return;		// no suggested index (probably not a CSPDL)
	
		TrackingAllocator alloc(CssmAllocator::standard());
	
		KCCursor search(createCursor(CSSM_DL_DB_RECORD_SYMMETRIC_KEY, NULL));
		CssmAutoData keyLabel(CssmAllocator::standard());
		keyLabel = StringData("SYSKC**");
		keyLabel.append(index);
		static const CSSM_DB_ATTRIBUTE_INFO infoLabel = {
			CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
			{"Label"},
			CSSM_DB_ATTRIBUTE_FORMAT_BLOB
		};
		search->add(CSSM_DB_EQUAL, infoLabel, keyLabel.get());
	
		// could run a loop below to catch *all* eligible keys,
		// but that's stretching it; and beware CSP scope if you add this...
		AutoCredentials cred(alloc);
		Item keyItem;
		if (search->next(keyItem)) {
			CssmClient::Key key = dynamic_cast<KeyItem &>(*keyItem).key();
	
			// create AccessCredentials from that key. Still allow interactive unlock
			const CssmKey &masterKey = key;
			CSSM_CSP_HANDLE cspHandle = key->csp()->handle();
			cred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
				new(alloc) ListElement(CSSM_WORDID_SYMMETRIC_KEY),
				new(alloc) ListElement(CssmData::wrap(cspHandle)),
				new(alloc) ListElement(CssmData::wrap(masterKey)));
			cred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
				new(alloc) ListElement(CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT));
	
			secdebug("storagemgr", "authenticating %s for default key credentials", db->name());
			db->authenticate(db->accessRequest(), &cred);
		}
	} catch (...) {
		secdebug("storagemgr", "setDefaultCredentials for %s abandoned due to exception", db->name());
	}
}

// Create KC if it doesn't exist, add it to the search list if it exists and is not already on it.
Keychain
StorageManager::makeKeychain(const DLDbIdentifier &dLDbIdentifier, bool add)
{
	Keychain keychain;
	bool post = false;

	{
		StLock<Mutex> _(mLock);
		keychain = _keychain(dLDbIdentifier);

		if (add)
		{
			mSavedList.revert(false);
			DLDbList searchList = mSavedList.searchList();
			if (find(searchList.begin(), searchList.end(), dLDbIdentifier) != searchList.end())
				return keychain;  // Keychain is already in the searchList.

			mCommonList.revert(false);
			searchList = mCommonList.searchList();
			if (find(searchList.begin(), searchList.end(), dLDbIdentifier) != searchList.end())
				return keychain;  // Keychain is already in the commonList don't add it to the searchList.
		
			// If the keychain doesn't exist don't bother adding it to the search list yet.
			if (!keychain->exists())
				return keychain;
		
			// The keychain exists and is not in our search list add it to the search
			// list and the cache.
			mSavedList.revert(true);
			mSavedList.add(dLDbIdentifier);
			mSavedList.save();
			post = true;
		}
	}

	if (post)
	{
		// Make sure we are not holding mLock when we post this event.
		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	}

	return keychain;
}

void
StorageManager::created(const Keychain &keychain) // Be notified a Keychain just got created.
{
    DLDbIdentifier dLDbIdentifier = keychain->dLDbIdentifier();
	bool defaultChanged = false;

 	{
		StLock<Mutex> _(mLock);

		mSavedList.revert(true);
		// If we don't have a default Keychain yet.  Make the newly created keychain the default.
		if (!mSavedList.defaultDLDbIdentifier())
		{
			mSavedList.defaultDLDbIdentifier(dLDbIdentifier);
			defaultChanged = true;
		}

		// Add the keychain to the search list prefs.
		mSavedList.add(dLDbIdentifier);
		mSavedList.save();
	}

	// Make sure we are not holding mLock when we post these events.
	KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);

	if (defaultChanged)
	{
		KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent, dLDbIdentifier);
	}
}

KCCursor
StorageManager::createCursor(SecItemClass itemClass, const SecKeychainAttributeList *attrList)
{
	KeychainList searchList;
	getSearchList(searchList);
	return KCCursor(searchList, itemClass, attrList);
}

KCCursor
StorageManager::createCursor(const SecKeychainAttributeList *attrList)
{
	KeychainList searchList;
	getSearchList(searchList);
	return KCCursor(searchList, attrList);
}

void
StorageManager::lockAll()
{
    SecurityServer::ClientSession ss(CssmAllocator::standard(), CssmAllocator::standard());
    ss.lockAll (false);
}

Keychain
StorageManager::defaultKeychain()
{
	Keychain theKeychain;
	{
		StLock<Mutex> _(mLock);
		mSavedList.revert(false);
		DLDbIdentifier defaultDLDbIdentifier(mSavedList.defaultDLDbIdentifier());
		if (defaultDLDbIdentifier)
		{
			theKeychain = _keychain(defaultDLDbIdentifier);
		}
	}

	if (theKeychain /* && theKeychain->exists() */)
		return theKeychain;

	MacOSError::throwMe(errSecNoDefaultKeychain);
}

void
StorageManager::defaultKeychain(const Keychain &keychain)
{
	DLDbIdentifier oldDefaultId;
	DLDbIdentifier newDefaultId(keychain->dLDbIdentifier());
	{
		StLock<Mutex> _(mLock);
		oldDefaultId = mSavedList.defaultDLDbIdentifier();
		mSavedList.revert(true);
		mSavedList.defaultDLDbIdentifier(newDefaultId);
		mSavedList.save();
	}

	if (!(oldDefaultId == newDefaultId))
	{
		// Make sure we are not holding mLock when we post this event.
		KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent, newDefaultId);
	}
}

Keychain
StorageManager::defaultKeychain(SecPreferencesDomain domain)
{
	if (domain == mDomain)
		return defaultKeychain();
	else
	{
		DLDbIdentifier defaultDLDbIdentifier(DLDbListCFPref(domain).defaultDLDbIdentifier());
		if (defaultDLDbIdentifier)
			return keychain(defaultDLDbIdentifier);

		MacOSError::throwMe(errSecNoDefaultKeychain);
	}
}

void
StorageManager::defaultKeychain(SecPreferencesDomain domain, const Keychain &keychain)
{
	if (domain == mDomain)
		defaultKeychain(keychain);
	else
		DLDbListCFPref(domain).defaultDLDbIdentifier(keychain->dLDbIdentifier());
}

Keychain
StorageManager::loginKeychain()
{
	Keychain theKeychain;
	{
		StLock<Mutex> _(mLock);
		mSavedList.revert(false);
		DLDbIdentifier loginDLDbIdentifier(mSavedList.loginDLDbIdentifier());
		if (loginDLDbIdentifier)
		{
			theKeychain = _keychain(loginDLDbIdentifier);
		}
	}

	if (theKeychain && theKeychain->exists())
		return theKeychain;

	MacOSError::throwMe(errSecNoSuchKeychain);
}

void
StorageManager::loginKeychain(Keychain keychain)
{
	StLock<Mutex> _(mLock);
	mSavedList.revert(true);
	mSavedList.loginDLDbIdentifier(keychain->dLDbIdentifier());
	mSavedList.save();
}

size_t
StorageManager::size()
{
	StLock<Mutex> _(mLock);
    mSavedList.revert(false);
	mCommonList.revert(false);
	return mSavedList.searchList().size() + mCommonList.searchList().size();
}

Keychain
StorageManager::at(unsigned int ix)
{
	StLock<Mutex> _(mLock);
	mSavedList.revert(false);
	DLDbList dLDbList = mSavedList.searchList();
	if (ix < dLDbList.size())
	{
		return _keychain(dLDbList[ix]);
	}
	else
	{
		ix -= dLDbList.size();
		mCommonList.revert(false);
		DLDbList commonList = mCommonList.searchList();
		if (ix >= commonList.size())
			MacOSError::throwMe(errSecInvalidKeychain);

		return _keychain(commonList[ix]);
	}
}

Keychain
StorageManager::operator[](unsigned int ix)
{
    return at(ix);
}	

void StorageManager::rename(Keychain keychain, const char* newName)
{
	// This is not a generic purpose rename method for keychains.
    // The keychain doesn't remain in the cache.
    //
    bool changedDefault = false;
	DLDbIdentifier newDLDbIdentifier;
	{
		StLock<Mutex> _(mLock);
		mSavedList.revert(true);
		DLDbIdentifier defaultId = mSavedList.defaultDLDbIdentifier();

        // Find the keychain object for the given ref
        DLDbIdentifier dLDbIdentifier = keychain->dLDbIdentifier();

        // Remove it from the saved list
        mSavedList.remove(dLDbIdentifier);
        if (dLDbIdentifier == defaultId)
            changedDefault=true;

		// Actually rename the database on disk.
        keychain->database()->rename(newName);

		newDLDbIdentifier = keychain->dLDbIdentifier();

        // Now update the keychain map to use the newDLDbIdentifier 
        KeychainMap::iterator it = mKeychains.find(dLDbIdentifier);
        if (it != mKeychains.end())
        {
            mKeychains.erase(it);
            mKeychains.insert(KeychainMap::value_type(newDLDbIdentifier, keychain));
        }

		// If this was the default keychain change it accordingly
		if (changedDefault)
			mSavedList.defaultDLDbIdentifier(newDLDbIdentifier);

		mSavedList.save();
	}

	// @@@ We need a kSecKeychainRenamedEvent so other clients can close this keychain and move on with life.
	//KCEventNotifier::PostKeychainEvent(kSecKeychainRenamedEvent);

	// Make sure we are not holding mLock when we post these events.
	KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);

	if (changedDefault)
		KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent, newDLDbIdentifier);
}

void StorageManager::renameUnique(Keychain keychain, CFStringRef newName)
{
    bool doneCreating = false;
    int index = 1;
    do
    {
        char newNameCString[MAXPATHLEN];
        if ( CFStringGetCString(newName, newNameCString, MAXPATHLEN, kCFStringEncodingUTF8) )	// make sure it fits in MAXPATHLEN, etc.
        {
            // Construct the new name...
            //
            CFMutableStringRef newNameCFStr = NULL;
            newNameCFStr = CFStringCreateMutable(NULL, MAXPATHLEN); 
            if ( newNameCFStr )
            {
                CFStringAppendFormat(newNameCFStr, NULL, CFSTR("%s%d"), &newNameCString, index);
                CFStringAppend(newNameCFStr, CFSTR(kKeychainSuffix));	// add .keychain
                char toUseBuff2[MAXPATHLEN];
                if ( CFStringGetCString(newNameCFStr, toUseBuff2, MAXPATHLEN, kCFStringEncodingUTF8) )	// make sure it fits in MAXPATHLEN, etc.
                {
                    struct stat filebuf;
                    if ( lstat(toUseBuff2, &filebuf) )
                    {
                        rename(keychain, toUseBuff2);
                        doneCreating = true;
                    }
                    else
                        index++;
                }
                else
                    doneCreating = true;	// failure to get c string.
                CFRelease(newNameCFStr);
            }
            else
                doneCreating = false; // failure to create mutable string.
        }
        else
            doneCreating = false; // failure to get the string (i.e. > MAXPATHLEN?)
    }
    while (!doneCreating && index != INT_MAX);
}

void StorageManager::remove(const KeychainList &kcsToRemove, bool deleteDb)
{
	bool unsetDefault = false;
	{
		StLock<Mutex> _(mLock);
		mSavedList.revert(true);
		DLDbIdentifier defaultId = mSavedList.defaultDLDbIdentifier();
		for (KeychainList::const_iterator ix = kcsToRemove.begin(); ix != kcsToRemove.end(); ++ix)
		{
			// Find the keychain object for the given ref
			Keychain keychainToRemove = *ix;
			DLDbIdentifier dLDbIdentifier = keychainToRemove->dLDbIdentifier();
	
			// Remove it from the saved list
			mSavedList.remove(dLDbIdentifier);
			if (dLDbIdentifier == defaultId)
				unsetDefault=true;

			if (deleteDb)
			{
				keychainToRemove->database()->deleteDb();
				// Now remove it from the map
				KeychainMap::iterator it = mKeychains.find(dLDbIdentifier);
				if (it == mKeychains.end())
					continue;
				mKeychains.erase(it);
			}
		}

		if (unsetDefault)
			mSavedList.defaultDLDbIdentifier(DLDbIdentifier());

		mSavedList.save();
	}

	// Make sure we are not holding mLock when we post these events.
	KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);

	if (unsetDefault)
		KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent);
}

void
StorageManager::getSearchList(KeychainList &keychainList)
{
	StLock<Mutex> _(mLock);
    mSavedList.revert(false);
	mCommonList.revert(false);

	// Merge mSavedList and common list
	DLDbList dLDbList = mSavedList.searchList();
	DLDbList commonList = mCommonList.searchList();
	KeychainList result;
	result.reserve(dLDbList.size() + commonList.size());

    for (DLDbList::const_iterator it = dLDbList.begin(); it != dLDbList.end(); ++it)
    {
        Keychain keychain(_keychain(*it));
        result.push_back(keychain);
    }

	for (DLDbList::const_iterator it = commonList.begin(); it != commonList.end(); ++it)
	{
		Keychain keychain(_keychain(*it));
		result.push_back(keychain);
	}

	keychainList.swap(result);
}

void
StorageManager::setSearchList(const KeychainList &keychainList)
{
	DLDbList commonList = mCommonList.searchList();

	// Strip out the common list part from the end of the search list.
	KeychainList::const_iterator it_end = keychainList.end();
	DLDbList::const_reverse_iterator end_common = commonList.rend();
	for (DLDbList::const_reverse_iterator it_common = commonList.rbegin(); it_common != end_common; ++it_common)
	{
		// Eliminate common entries from the end of the passed in keychainList.
		if (it_end == keychainList.begin())
			break;

		--it_end;
		if (!((*it_end)->dLDbIdentifier() == *it_common))
		{
			++it_end;
			break;
		}
	}

	/* it_end now points one past the last element in keychainList which is not in commonList. */
	DLDbList searchList, oldSearchList(mSavedList.searchList());
	for (KeychainList::const_iterator it = keychainList.begin(); it != it_end; ++it)
	{
		searchList.push_back((*it)->dLDbIdentifier());
	}

	{
		// Set the current searchlist to be what was passed in, the old list will be freed
		// upon exit of this stackframe.
		StLock<Mutex> _(mLock);
		mSavedList.revert(true);
		mSavedList.searchList(searchList);
    	mSavedList.save();
	}

	if (!(oldSearchList == searchList))
	{
		// Make sure we are not holding mLock when we post this event.
		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	}
}

void
StorageManager::getSearchList(SecPreferencesDomain domain, KeychainList &keychainList)
{
	if (domain == mDomain)
	{
		StLock<Mutex> _(mLock);
		mSavedList.revert(false);
		convertList(keychainList, mSavedList.searchList());
	}
	else
	{
		convertList(keychainList, DLDbListCFPref(domain).searchList());
	}
}

void
StorageManager::setSearchList(SecPreferencesDomain domain, const KeychainList &keychainList)
{
	DLDbList searchList;
	convertList(searchList, keychainList);

	if (domain == mDomain)
	{
		DLDbList oldSearchList(mSavedList.searchList());
		{
			// Set the current searchlist to be what was passed in, the old list will be freed
			// upon exit of this stackframe.
			StLock<Mutex> _(mLock);
			mSavedList.revert(true);
			mSavedList.searchList(searchList);
			mSavedList.save();
		}

		if (!(oldSearchList == searchList))
		{
			// Make sure we are not holding mLock when we post this event.
			KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
		}
	}
	else
	{
		DLDbListCFPref(domain).searchList(searchList);
	}
}

void
StorageManager::domain(SecPreferencesDomain domain)
{
	StLock<Mutex> _(mLock);
	if (domain == mDomain)
		return;	// no change

#if !defined(NDEBUG)
	switch (domain)
	{
	case kSecPreferencesDomainSystem:
		secdebug("storagemgr", "switching to system domain"); break;
	case kSecPreferencesDomainUser:
		secdebug("storagemgr", "switching to user domain (uid %d)", getuid()); break;
	default:
		secdebug("storagemgr", "switching to weird prefs domain %d", domain); break;
	}
#endif

	mDomain = domain;
	mSavedList.set(domain);
}

void
StorageManager::optionalSearchList(CFTypeRef keychainOrArray, KeychainList &keychainList)
{
	if (!keychainOrArray)
		getSearchList(keychainList);
	else
	{
		CFTypeID typeID = CFGetTypeID(keychainOrArray);
		if (typeID == CFArrayGetTypeID())
			convertToKeychainList(CFArrayRef(keychainOrArray), keychainList);
		else if (typeID == gTypes().KeychainImpl.typeID)
			keychainList.push_back(KeychainImpl::required(SecKeychainRef(keychainOrArray)));
		else
			MacOSError::throwMe(paramErr);
	}
}

// static methods.
void
StorageManager::convertToKeychainList(CFArrayRef keychainArray, KeychainList &keychainList)
{
	assert(keychainArray);
	CFIndex count = CFArrayGetCount(keychainArray);
	KeychainList keychains(count);
	for (CFIndex ix = 0; ix < count; ++ix)
	{
		keychains[ix] = KeychainImpl::required(SecKeychainRef(CFArrayGetValueAtIndex(keychainArray, ix)));
	}

	keychainList.swap(keychains);
}

CFArrayRef
StorageManager::convertFromKeychainList(const KeychainList &keychainList)
{
	CFRef<CFMutableArrayRef> keychainArray(CFArrayCreateMutable(NULL, keychainList.size(), &kCFTypeArrayCallBacks));

	for (KeychainList::const_iterator ix = keychainList.begin(); ix != keychainList.end(); ++ix)
	{
		SecKeychainRef keychainRef = (*ix)->handle();
		CFArrayAppendValue(keychainArray, keychainRef);
		CFRelease(keychainRef);
	}

	// Counter the CFRelease that CFRef<> is about to do when keychainArray goes out of scope.
	CFRetain(keychainArray);
	return keychainArray;
}

void StorageManager::convertList(DLDbList &ids, const KeychainList &kcs)
{
	DLDbList result;
	result.reserve(kcs.size());
	for (KeychainList::const_iterator ix = kcs.begin(); ix != kcs.end(); ++ix)
	{
		result.push_back((*ix)->dLDbIdentifier());
	}
	ids.swap(result);
}

void StorageManager::convertList(KeychainList &kcs, const DLDbList &ids)
{
	KeychainList result;
    result.reserve(ids.size());
    for (DLDbList::const_iterator ix = ids.begin(); ix != ids.end(); ++ix)
    {
        Keychain keychain(_keychain(*ix));
        result.push_back(keychain);
    }
    kcs.swap(result);
}

#pragma mark ÑÑÑÑ Login Functions ÑÑÑÑ

void StorageManager::login(AuthorizationRef authRef, UInt32 nameLength, const char* name)
{
    AuthorizationItemSet* info = NULL;
    OSStatus result = AuthorizationCopyInfo(authRef, NULL, &info);	// get the results of the copy rights call.
    Boolean created = false;
    if ( result == noErr && info->count )
    {
        // Grab the password from the auth context (info) and create the keychain...
        //
        AuthorizationItem* currItem = info->items;
        for (UInt32 index = 1; index <= info->count; index++) //@@@plugin bug won't return a specific context.
        {
            if (strcmp(currItem->name, kAuthorizationEnvironmentPassword) == 0)
            {
                // creates the login keychain with the specified password
                try
                {
                    login(nameLength, name, currItem->valueLength, currItem->value);
                    created = true;
                }
                catch(...)
                {
                }
                break;
            }
            currItem++;
        }
    }
    if ( info )
        AuthorizationFreeItemSet(info);

    if ( !created )
        MacOSError::throwMe(errAuthorizationInternal);
}

void StorageManager::login(ConstStringPtr name, ConstStringPtr password)
{
    if ( name == NULL || password == NULL )
        MacOSError::throwMe(paramErr);

	login(name[0], name + 1, password[0], password + 1);
}

void StorageManager::login(UInt32 nameLength, const void *name, UInt32 passwordLength, const void *password)
{
	x_debug("StorageManager::login: entered");
	mSavedList.revert(true);
	if (passwordLength != 0 && password == NULL)
	{
		x_debug("StorageManager::login: invalid argument (NULL password)");
		MacOSError::throwMe(paramErr);
	}

	DLDbIdentifier loginDLDbIdentifier(mSavedList.loginDLDbIdentifier());
	x_debug1("StorageManager::login: loginDLDbIdentifier is %s", (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
	if (!loginDLDbIdentifier)
		MacOSError::throwMe(errSecNoSuchKeychain);

	Keychain theKeychain(keychain(loginDLDbIdentifier));
	try
	{
		x_debug2("Attempting to unlock login keychain %s with %d-character password", (theKeychain) ? theKeychain->name() : "<NULL>", (unsigned int)passwordLength);
		theKeychain->unlock(CssmData(const_cast<void *>(password), passwordLength));
		x_debug("Login keychain unlocked successfully");
	}
	catch(const CssmError &e)
	{
		if (e.osStatus() != CSSMERR_DL_DATASTORE_DOESNOT_EXIST)
			throw;
		x_debug1("Creating login keychain %s", (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
		theKeychain->create(passwordLength, password);
		x_debug("Login keychain created successfully");
		// Set the prefs for this new login keychain.
		loginKeychain(theKeychain);
		// Login Keychain does not lock on sleep nor lock after timeout by default.
		theKeychain->setSettings(INT_MAX, false);
	}
}

void StorageManager::logout()
{
    // nothing left to do here
}

void StorageManager::changeLoginPassword(ConstStringPtr oldPassword, ConstStringPtr newPassword)
{
	loginKeychain()->changePassphrase(oldPassword, newPassword);
	secdebug("KClogin", "Changed login keychain password successfully");
}


void StorageManager::changeLoginPassword(UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword)
{
	loginKeychain()->changePassphrase(oldPasswordLength, oldPassword,  newPasswordLength, newPassword);
	secdebug("KClogin", "Changed login keychain password successfully");
}

// Clear out the keychain search list and rename the existing login.keychain.
//
void StorageManager::resetKeychain(Boolean resetSearchList)
{
    // Clear the keychain search list.
    //
    CFArrayRef emptySearchList = nil;
    try
    {
        if ( resetSearchList )
        {
            emptySearchList = CFArrayCreate(NULL, NULL, 0, NULL);
            StorageManager::KeychainList keychainList;
            convertToKeychainList(emptySearchList, keychainList);
            setSearchList(keychainList);
        }
        // Get a reference to the existing login keychain...
        // If we don't have one, we throw (not requiring a rename).
        //
        Keychain keychain = loginKeychain();
        //
        // Rename the existing login.keychain (i.e. put it aside).
        //
        CFMutableStringRef newName = NULL;
        newName = CFStringCreateMutable(NULL, 0);
        CFStringRef currName = NULL;
        currName = CFStringCreateWithCString(NULL, keychain->name(), kCFStringEncodingUTF8);
        if ( newName && currName )
        {
            CFStringAppend(newName, currName);
            CFStringRef kcSuffix = CFSTR(kKeychainSuffix);
            if ( CFStringHasSuffix(newName, kcSuffix) )	// remove the .keychain extension
            {
                CFRange suffixRange = CFStringFind(newName, kcSuffix, 0);
                CFStringFindAndReplace(newName, kcSuffix, CFSTR(""), suffixRange, 0);
            }
            CFStringAppend(newName, CFSTR(kKeychainRenamedSuffix));	// add "_renamed"
            try
            {
                renameUnique(keychain, newName);
            }
            catch(...)
            {
                // we need to release 'newName' & 'currName'
            }
        }	 // else, let the login call report a duplicate
        if ( newName )
            CFRelease(newName);
        if ( currName )
            CFRelease(currName);
    }
    catch(...)
    {
        // We either don't have a login keychain, or there was a
        // failure to rename the existing one.
    }
    if ( emptySearchList )
        CFRelease(emptySearchList);
}

#pragma mark ÑÑÑÑ File Related ÑÑÑÑ

Keychain StorageManager::make(const char *pathName)
{
	return make(pathName, true);
}

Keychain StorageManager::make(const char *pathName, bool add)
{
	string fullPathName;
    if ( pathName[0] == '/' )
		fullPathName = pathName;
	else
    {
		// Get Home directory from environment.
		switch (mDomain) {
		case kSecPreferencesDomainUser:
			{
				const char *homeDir = getenv("HOME");
				if (homeDir == NULL)
				{
					// If $HOME is unset get the current user's home directory from the passwd file.
					uid_t uid = geteuid();
					if (!uid) uid = getuid();
					struct passwd *pw = getpwuid(uid);
					if (!pw)
						MacOSError::throwMe(paramErr);
					homeDir = pw->pw_dir;
				}
				fullPathName = homeDir;
			}
			break;
		case kSecPreferencesDomainSystem:
			fullPathName = "";
			break;
		default:
			assert(false);	// invalid domain for this
		}

		fullPathName += "/Library/Keychains/";
		fullPathName += pathName;
	}

    const CSSM_NET_ADDRESS *DbLocation = NULL;	// NULL for keychains
    const CSSM_VERSION *version = NULL;
    uint32 subserviceId = 0;
    CSSM_SERVICE_TYPE subserviceType = CSSM_SERVICE_DL | CSSM_SERVICE_CSP;
    const CssmSubserviceUid ssuid(gGuidAppleCSPDL, version,
                                   subserviceId, subserviceType);
	DLDbIdentifier dLDbIdentifier(ssuid, fullPathName.c_str(), DbLocation);
	return makeKeychain(dLDbIdentifier, add);
}

Keychain StorageManager::makeLoginAuthUI(Item &item)
{
    // Create a login/default keychain for the user using UI.
    // The user can cancel out of the operation, or create a new login keychain.
    // If auto-login is turned off, the user will be asked for their login password.
    //
    OSStatus result = noErr;
    Keychain keychain = NULL;	// We return this keychain.
    //
    // Set up the Auth ref to bring up UI.
    //
    AuthorizationRef authRef = NULL;
    result = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authRef);
    if ( result != noErr )
        MacOSError::throwMe(errAuthorizationInternal);
    AuthorizationEnvironment envir;
    envir.count = 5;	// 5 hints are used.
    AuthorizationItem* authEnvirItemArrayPtr = (AuthorizationItem*)malloc(sizeof(AuthorizationItem) * envir.count);
    if ( !authEnvirItemArrayPtr )
    {
        if ( authRef )
            AuthorizationFree(authRef, kAuthorizationFlagDefaults);
        MacOSError::throwMe(errAuthorizationInternal);
    }
    envir.items = authEnvirItemArrayPtr;
    AuthorizationItem* currItem = authEnvirItemArrayPtr;
    //
    // 1st Hint (optional): The keychain item's account attribute string. 
    //						When item is specified, we assume an 'add' operation is being attempted.
    char buff[255];
    UInt32 actLen;
    SecKeychainAttribute attr = { kSecAccountItemAttr, 255, &buff };
    try
    {
        item->getAttribute(attr, &actLen);
    }
    catch(...)
    {
        actLen = 0;	// This item didn't have the account attribute, so don't display one in the UI.
    }
    currItem->name = AGENT_HINT_ATTR_NAME;	// name str that identifies this hint as attr name
    if ( actLen )	// Fill in the hint if we have a 'srvr' attr
    {
        if ( actLen > 255 )
            buff[255] = 0;
        else
            buff[actLen] = 0;
        currItem->valueLength = strlen(buff)+1;
        currItem->value = buff;
    }
    else
    {
        currItem->valueLength = 0;
        currItem->value = NULL;
    }
    currItem->flags = 0;
    //
    // 2nd Hint (optional): The item's keychain full path.
    //
    currItem++;
    char* currDefaultName = NULL;
    try
    {
		currDefaultName = (char*)globals().storageManager.defaultKeychain()->name();	// Use the name if we have it.
		currItem->name = AGENT_HINT_LOGIN_KC_NAME;	// Name str that identifies this hint as kc path
		currItem->valueLength = strlen(currDefaultName);
		currItem->value = (void*)currDefaultName;
		currItem->flags = 0;
		currItem++;
    }
    catch(...)
    {
		envir.count--;
    }
	
    //
    // 3rd Hint (optional): If curr default keychain is unavailable.
    // This is determined by the parent not existing.
    //
    currItem->name = AGENT_HINT_LOGIN_KC_EXISTS_IN_KC_FOLDER;
    Boolean loginUnavail = false;
    try
    {
        Keychain defaultKC = defaultKeychain();
        if ( !defaultKC->exists() )
        	loginUnavail = true;
    }
    catch(...)	// login.keychain not present
    {
    }
	currItem->valueLength = sizeof(Boolean);
    currItem->value = (void*)&loginUnavail;
    currItem->flags = 0;
    //
    // 4th Hint (required) userName
    //
    currItem++;
    currItem->name = AGENT_HINT_LOGIN_KC_USER_NAME;
    char* uName = getenv("USER");
    string userName = uName ? uName : "";
	if ( userName.length() == 0 )
    {
        uid_t uid = geteuid();
        if (!uid) uid = getuid();
        struct passwd *pw = getpwuid(uid);	// fallback case...
        if (pw)
            userName = pw->pw_name;
        endpwent();
    }
    if ( userName.length() != 0 )	// did we ultimately get one?
    {
        currItem->value = (void*)userName.c_str();
        currItem->valueLength = userName.length();
    }
    else	// trouble getting user name; can't continue...
    {
        if ( authRef )
            AuthorizationFree(authRef, kAuthorizationFlagDefaults);
        free(authEnvirItemArrayPtr);
        MacOSError::throwMe(errAuthorizationInternal);
    }
    currItem->flags = 0;
    //
    // 5th Hint (optional) flags if user has more than 1 keychain (used for a later warning when reset to default).
    //
    currItem++; // last hint...
    currItem->name = AGENT_HINT_LOGIN_KC_USER_HAS_OTHER_KCS_STR;
    Boolean moreThanOneKCExists = false;
	{
		StLock<Mutex> _(mLock);
		if (mSavedList.searchList().size() > 1)
			moreThanOneKCExists = true;
	}
    currItem->value = &moreThanOneKCExists;
    currItem->valueLength = sizeof(Boolean);
    currItem->flags = 0;
    //
    // Set up the auth rights and make the auth call.
    //
    AuthorizationItem authItem = { LOGIN_KC_CREATION_RIGHT, 0 , NULL, 0};
    AuthorizationRights rights = { 1, &authItem };
    result = AuthorizationCopyRights(authRef, &rights, &envir, kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights, NULL);
    free(authEnvirItemArrayPtr);	// done with the auth items.
    if ( result == errAuthorizationSuccess )	// On success, revert to defaults.
    {
        try
        {
            resetKeychain(true); // Clears the plist, moves aside existing login.keychain
            login(authRef, userName.length(), userName.c_str());	// Creates a login.keychain
            keychain = loginKeychain();	// Return it.
            defaultKeychain(keychain);	// Set it to the default.
        }
        catch(...)
        {
            // Reset failed, login.keychain creation failed, or setting it to default.
            // We need to release 'authRef'...
        }
    }
    if ( authRef )
        AuthorizationFree(authRef, kAuthorizationFlagDefaults);
    if ( result )
        MacOSError::throwMe(result);	// Any other error means we don't return a keychain.
    return keychain;
}

Keychain StorageManager::defaultKeychainUI(Item &item)
{
    Keychain returnedKeychain = NULL;
    try
    {
        returnedKeychain = globals().storageManager.defaultKeychain(); // If we have one, return it.
        if ( returnedKeychain->exists() )
            return returnedKeychain;
    }
    catch(...)	// We could have one, but it isn't available (i.e. on a un-mounted volume).
    {
    }
    if ( globals().getUserInteractionAllowed() )
    {
        returnedKeychain = makeLoginAuthUI(item); // If no Keychains Ä is present, one will be created.
        if ( !returnedKeychain )
            MacOSError::throwMe(errSecInvalidKeychain);	// Something went wrong...
    }
    else
        MacOSError::throwMe(errSecInteractionNotAllowed); // If UI isn't allowed, return an error.

    return returnedKeychain;
}
