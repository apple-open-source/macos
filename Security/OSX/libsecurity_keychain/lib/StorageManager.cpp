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
#include <algorithm>
#include <string>
#include <stdio.h>
#include <security_utilities/debugging.h>
#include <security_keychain/SecCFTypes.h>
#include <securityd_client/ssclient.h>
#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/SecTask.h>
#include <security_keychain/SecCFTypes.h>
#include <Security/SecCFAllocator.h>
#include "TrustSettingsSchema.h"
#include <security_cdsa_client/wrapkey.h>
#include <securityd_client/ssblob.h>
#include <SecBasePriv.h>
#include "TokenLogin.h"

//%%% add this to AuthorizationTagsPriv.h later
#ifndef AGENT_HINT_LOGIN_KC_SUPPRESS_RESET_PANEL
#define AGENT_HINT_LOGIN_KC_SUPPRESS_RESET_PANEL "loginKCCreate:suppressResetPanel"
#endif

#include "KCCursor.h"
#include "Globals.h"


using namespace CssmClient;
using namespace KeychainCore;

#define kLoginKeychainPathPrefix "~/Library/Keychains/"
#define kUserLoginKeychainPath "~/Library/Keychains/login.keychain"
#define kEmptyKeychainSizeInBytes   20460

//-----------------------------------------------------------------------------------

static SecPreferencesDomain defaultPreferenceDomain()
{
	SessionAttributeBits sessionAttrs;
	if (gServerMode) {
		secnotice("servermode", "StorageManager initialized in server mode");
		sessionAttrs = sessionIsRoot;
	} else {
		MacOSError::check(SessionGetInfo(callerSecuritySession, NULL, &sessionAttrs));
	}

	// If this is the root session, use system preferences.
	// (In SecurityServer debug mode, you'll get a (fake) root session
	// that has graphics access. Ignore that to help testing.)
	if ((sessionAttrs & sessionIsRoot)
			IFDEBUG( && !(sessionAttrs & sessionHasGraphicAccess))) {
		secnotice("storagemgr", "using system preferences");
		return kSecPreferencesDomainSystem;
	}

	// otherwise, use normal (user) preferences
	return kSecPreferencesDomainUser;
}

static bool isAppSandboxed()
{
	bool result = false;
	SecTaskRef task = SecTaskCreateFromSelf(NULL);
	if(task != NULL) {
		CFTypeRef appSandboxValue = SecTaskCopyValueForEntitlement(task,
			CFSTR("com.apple.security.app-sandbox"), NULL);
		if(appSandboxValue != NULL) {
			result = true;
			CFRelease(appSandboxValue);
		}
		CFRelease(task);
	}
	return result;
}

static bool shouldAddToSearchList(const DLDbIdentifier &dLDbIdentifier)
{
	// Creation of a private keychain should not modify the search list: rdar://13529331
	// However, we want to ensure the login and System keychains are in
	// the search list if that is not the case when they are created.
	// Note that App Sandbox apps may not modify the list in either case.

	bool loginOrSystemKeychain = false;
	const char *dbname = dLDbIdentifier.dbName();
	if (dbname) {
		if ((!strcmp(dbname, "/Library/Keychains/System.keychain")) ||
			(strstr(dbname, "/login.keychain")) ) {
			loginOrSystemKeychain = true;
		}
	}
	return (loginOrSystemKeychain && !isAppSandboxed());
}


StorageManager::StorageManager() :
	mSavedList(defaultPreferenceDomain()),
	mCommonList(kSecPreferencesDomainCommon),
	mDomain(kSecPreferencesDomainUser),
	mMutex(Mutex::recursive)
{
}


Mutex*
StorageManager::getStorageManagerMutex()
{
	return &mKeychainMapMutex;
}


Keychain
StorageManager::keychain(const DLDbIdentifier &dLDbIdentifier)
{
	StLock<Mutex>_(mKeychainMapMutex);

	if (!dLDbIdentifier)
		return Keychain();

    KeychainMap::iterator it = mKeychainMap.end();

    // If we have a keychain object for the munged keychain, return that.
    // Don't hit the filesystem to check file status if we've already done that work...
    DLDbIdentifier munge_dldbi = forceMungeDLDbIDentifier(dLDbIdentifier);
    it = mKeychainMap.find(munge_dldbi);
    if (it != mKeychainMap.end()) {
        return it->second;
    }

    // If we have a keychain object for the un/demunged keychain, return that.
    // We might be in the middle of an upgrade, where the -db file exists as a bit-perfect copy of the original file.
    DLDbIdentifier demunge_dldbi = demungeDLDbIdentifier(dLDbIdentifier);
    it = mKeychainMap.find(demunge_dldbi);
    if (it != mKeychainMap.end()) {
        return it->second;
    }

    // Okay, we haven't seen this keychain before. Do the full process...
    DLDbIdentifier dldbi = mungeDLDbIdentifier(dLDbIdentifier, false);
    it = mKeychainMap.find(dldbi); // Almost certain not to find it here
    if (it != mKeychainMap.end())
	{
        return it->second;
	}

	if (gServerMode) {
		secnotice("servermode", "keychain reference in server mode");
		return Keychain();
	}

    // The keychain is not in our cache.  Create it.
    Db db(makeDb(dldbi));

	Keychain keychain(db);
	// Add the keychain to the cache.
    registerKeychain(keychain);

	return keychain;
}

// Note: this must be a munged DLDbidentifier.
CssmClient::Db
StorageManager::makeDb(DLDbIdentifier dLDbIdentifier) {
    Module module(dLDbIdentifier.ssuid().guid());

    DL dl;
    if (dLDbIdentifier.ssuid().subserviceType() & CSSM_SERVICE_CSP)
        dl = SSCSPDL(module);
    else
        dl = DL(module);

    dl->subserviceId(dLDbIdentifier.ssuid().subserviceId());
    dl->version(dLDbIdentifier.ssuid().version());

    CssmClient::Db db(dl, dLDbIdentifier.dbName());

    return db;
}

// StorageManager is responsible for silently switching to newer-style keychains.
// If the keychain requested is in ~/Library/Keychains/, and there is a
// newer keychain available (with extension ".keychain-db"), open that one
// instead of the one requested.
//
// Because of backwards compatibility reasons, we can't update the plist
// files on disk to point to the upgraded keychains. We will be asked to
// load "/Users/account/Library/Keychains/login.keychain", hence this
// modification to 'login.keychain-db'.
DLDbIdentifier
StorageManager::mungeDLDbIdentifier(const DLDbIdentifier& dLDbIdentifier, bool isReset) {
    if(!dLDbIdentifier.dbName()) {
        // If this DLDbIdentifier doesn't have a filename, don't munge it
        return dLDbIdentifier;
    }

    string path = dLDbIdentifier.dbName();

    bool shouldCreateProtected = globals().integrityProtection();

    // If we don't have a DLDbIdentifier, we can't return one
    if(dLDbIdentifier.mImpl == NULL) {
        return DLDbIdentifier();
    }

    // Ensure we're in ~/Library/Keychains
    if(pathInHomeLibraryKeychains(path)) {
        string pathdb = makeKeychainDbFilename(path);

        struct stat st;

        int path_stat_err = 0;
        bool path_exists = (::stat(path.c_str(), &st) == 0);
        if(!path_exists) {
            path_stat_err = errno;
        }

        int pathdb_stat_err = 0;
        bool pathdb_exists = (::stat(pathdb.c_str(), &st) == 0);
        if(!pathdb_exists) {
            pathdb_stat_err = errno;
        }

        // If protections are off, don't change the requested filename.
        // If protictions are on and the -db file exists, always use it.
        //
        // If we're resetting, and we're creating a new-style keychain, use the -db path.
        // If we're resetting, and we're creating an old-style keychain, use the original path.
        //
        //  Protection  pathdb_exists path_exists resetting Result
        //  DISABLED       X           X             X       original
        //  ENABLED        1           X             X       -db
        //  ENABLED        0           0             X       -db
        //  ENABLED        0           1             0       original
        //  ENABLED        0           1             1       -db
        //
        bool switchPaths = shouldCreateProtected && (pathdb_exists || (!pathdb_exists && !path_exists) || isReset);

        if(switchPaths) {
            secinfo("integrity", "switching to keychain-db: %s from %s (%d %d %d_%d %d_%d)", pathdb.c_str(), path.c_str(), isReset, shouldCreateProtected, path_exists, path_stat_err, pathdb_exists, pathdb_stat_err);
            path = pathdb;
        } else {
            secinfo("integrity", "not switching: %s from %s (%d %d %d_%d %d_%d)", pathdb.c_str(), path.c_str(), isReset, shouldCreateProtected, path_exists, path_stat_err, pathdb_exists, pathdb_stat_err);
        }
    }

    DLDbIdentifier id(dLDbIdentifier.ssuid(), path.c_str(), dLDbIdentifier.dbLocation());
    return id;
}

DLDbIdentifier
StorageManager::forceMungeDLDbIDentifier(const DLDbIdentifier& dLDbIdentifier) {
    if(!dLDbIdentifier.dbName() || dLDbIdentifier.mImpl == NULL) {
        return dLDbIdentifier;
    }

    string path = dLDbIdentifier.dbName();
    string pathdb = makeKeychainDbFilename(path);

    DLDbIdentifier id(dLDbIdentifier.ssuid(), pathdb.c_str(), dLDbIdentifier.dbLocation());
    return id;
}

DLDbIdentifier
StorageManager::demungeDLDbIdentifier(const DLDbIdentifier& dLDbIdentifier) {
    if(dLDbIdentifier.dbName() == NULL) {
        return dLDbIdentifier;
    }

    string path = dLDbIdentifier.dbName();
    string dbSuffix = "-db";
    bool endsWithKeychainDb = (path.size() > dbSuffix.size() && (0 == path.compare(path.size() - dbSuffix.size(), dbSuffix.size(), dbSuffix)));

    // Ensure we're in ~/Library/Keychains, and that the path ends in "-db"
    if(pathInHomeLibraryKeychains(path) && endsWithKeychainDb) {
        // remove "-db" from the end.
        path.erase(path.end() - 3, path.end());
    }

    DLDbIdentifier id(dLDbIdentifier.ssuid(), path.c_str(), dLDbIdentifier.dbLocation());
    return id;
}

string
StorageManager::makeKeychainDbFilename(const string& filename) {
    string keychainDbSuffix = "-db";
    bool endsWithKeychainDb = (filename.size() > keychainDbSuffix.size() && (0 == filename.compare(filename.size() - keychainDbSuffix.size(), keychainDbSuffix.size(), keychainDbSuffix)));

    if(endsWithKeychainDb) {
        return filename;
    } else {
        return filename + keychainDbSuffix;
    }
}

bool
StorageManager::pathInHomeLibraryKeychains(const string& path) {
    return SecurityServer::CommonBlob::pathInHomeLibraryKeychains(path);
}

void
StorageManager::reloadKeychain(Keychain keychain) {
    StLock<Mutex>_(mKeychainMapMutex);

    DLDbIdentifier dLDbIdentifier = keychain->database()->dlDbIdentifier();

    keychain->changeDatabase(makeDb(mungeDLDbIdentifier(dLDbIdentifier, false)));

    // This keychain might have a different dldbidentifier now, depending on what
    // other processes have been doing to the keychain files. Let's re-register it, just
    // to be sure.
    registerKeychain(keychain);
}

void
StorageManager::removeKeychain(const DLDbIdentifier &dLDbIdentifier,
	KeychainImpl *keychainImpl)
{
    StLock<Mutex>_(mKeychainMapMutex);

    // Don't trust this dldbidentifier. Just look for the keychain and delete it.
    forceRemoveFromCache(keychainImpl);
}

void
StorageManager::didRemoveKeychain(const DLDbIdentifier &dLDbIdentifier)
{
	// Lock the recursive mutex

	StLock<Mutex>_(mKeychainMapMutex);

	KeychainMap::iterator it = mKeychainMap.find(dLDbIdentifier);
	if (it != mKeychainMap.end())
	{
		it->second->inCache(false);
		mKeychainMap.erase(it);
	}
}

// If the client does not keep references to keychains, they are destroyed on
// every API exit, and recreated on every API entrance.
//
// To improve performance, we'll cache keychains for some short period of time.
// We'll do this by CFRetaining the keychain object, and setting a timer to
// CFRelease it when time's up. This way, the client can still recover all its
// memory if it doesn't want the keychains around, but repeated API calls will
// be significantly faster.
//
void
StorageManager::tickleKeychain(KeychainImpl *keychainImpl) {
    static dispatch_once_t onceToken = 0;
    static dispatch_queue_t release_queue = NULL;
    dispatch_once(&onceToken, ^{
        release_queue = dispatch_queue_create("com.apple.security.keychain-cache-queue", DISPATCH_QUEUE_SERIAL);
    });

    __block KeychainImpl* kcImpl = keychainImpl;

    if(!kcImpl) {
        return;
    }

    // We really only want to cache CSPDL file-based keychains
    if(kcImpl->dlDbIdentifier().ssuid().guid() != gGuidAppleCSPDL) {
        return;
    }

    // Make a one-shot timer to release the keychain
    uint32_t seconds = 1;

    const string path = kcImpl->name();
    bool isSystemKeychain = (0 == path.compare("/Library/Keychains/System.keychain"));
    if(pathInHomeLibraryKeychains(path) || isSystemKeychain) {
        // These keychains are important and likely aren't on removable media.
        // Cache them longer.
        seconds = 5;
    }

    __block CFTypeRef kcHandle = kcImpl->handle(); // calls retain; this keychain object will stay around until our dispatch block fires.

    dispatch_async(release_queue, ^() {
        if(kcImpl->mCacheTimer) {
            // Update the cache timer to be seconds from now
            dispatch_source_set_timer(kcImpl->mCacheTimer, dispatch_time(DISPATCH_TIME_NOW, seconds * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, NSEC_PER_SEC/2);
            secdebug("keychain", "updating cache on %p %s", kcImpl, kcImpl->name());

            // We've added an extra retain to this keychain right before invoking this block. Release it.
            CFRelease(kcHandle);

        } else {
            // No cache timer; make one.
            kcImpl->mCacheTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, release_queue);
            dispatch_source_set_timer(kcImpl->mCacheTimer, dispatch_time(DISPATCH_TIME_NOW, seconds * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, NSEC_PER_SEC/2);
            secdebug("keychain", "taking cache on %p %s", kcImpl, kcImpl->name());

            dispatch_source_set_event_handler(kcImpl->mCacheTimer, ^{
                secdebug("keychain", "releasing cache on %p %s", kcImpl, kcImpl->name());
                dispatch_source_cancel(kcImpl->mCacheTimer);
                dispatch_release(kcImpl->mCacheTimer);
                kcImpl->mCacheTimer = NULL;
                CFRelease(kcHandle);
            });

            dispatch_resume(kcImpl->mCacheTimer);
        }
    });
}

// Create keychain if it doesn't exist, and optionally add it to the search list.
Keychain
StorageManager::makeKeychain(const DLDbIdentifier &dLDbIdentifier, bool add, bool isReset)
{
	StLock<Mutex>_(mKeychainMapMutex);

	Keychain theKeychain = keychain(mungeDLDbIdentifier(dLDbIdentifier, isReset));
	bool post = false;
	bool updateList = (add && shouldAddToSearchList(dLDbIdentifier));

	if (updateList)
	{
		mSavedList.revert(false);
		DLDbList searchList = mSavedList.searchList();
		if (find(searchList.begin(), searchList.end(), demungeDLDbIdentifier(dLDbIdentifier)) != searchList.end())
			return theKeychain;  // theKeychain is already in the searchList.

		mCommonList.revert(false);
		searchList = mCommonList.searchList();
		if (find(searchList.begin(), searchList.end(), demungeDLDbIdentifier(dLDbIdentifier)) != searchList.end())
			return theKeychain;  // theKeychain is already in the commonList don't add it to the searchList.

		// If theKeychain doesn't exist don't bother adding it to the search list yet.
		if (!theKeychain->exists())
			return theKeychain;

		// theKeychain exists and is not in our search list, so add it to the
		// search list.
		mSavedList.revert(true);
		mSavedList.add(demungeDLDbIdentifier(dLDbIdentifier));
		mSavedList.save();
		post = true;
	}

	if (post)
	{
		// Make sure we are not holding mStorageManagerLock anymore when we
		// post this event.
		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	}

	return theKeychain;
}

// Be notified a Keychain just got created.
void
StorageManager::created(const Keychain &keychain)
{
	StLock<Mutex>_(mKeychainMapMutex);

    DLDbIdentifier dLDbIdentifier = keychain->dlDbIdentifier();
	bool defaultChanged = false;
	bool updateList = shouldAddToSearchList(dLDbIdentifier);

	if (updateList)
 	{
		mSavedList.revert(true);
		// If we don't have a default Keychain yet.  Make the newly created
		// keychain the default.
		if (!mSavedList.defaultDLDbIdentifier())
		{
			mSavedList.defaultDLDbIdentifier(demungeDLDbIdentifier(dLDbIdentifier));
			defaultChanged = true;
		}

		// Add the keychain to the search list prefs.
		mSavedList.add(demungeDLDbIdentifier(dLDbIdentifier));
		mSavedList.save();

		// Make sure we are not holding mLock when we post these events.
		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	}

	if (defaultChanged)
	{
		KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent, dLDbIdentifier);
	}
}

KCCursor
StorageManager::createCursor(SecItemClass itemClass,
	const SecKeychainAttributeList *attrList)
{
	StLock<Mutex>_(mMutex);

	KeychainList searchList;
	getSearchList(searchList);
	return KCCursor(searchList, itemClass, attrList);
}

KCCursor
StorageManager::createCursor(const SecKeychainAttributeList *attrList)
{
	StLock<Mutex>_(mMutex);

	KeychainList searchList;
	getSearchList(searchList);
	return KCCursor(searchList, attrList);
}

void
StorageManager::lockAll()
{
	StLock<Mutex>_(mMutex);

    SecurityServer::ClientSession ss(Allocator::standard(), Allocator::standard());
    ss.lockAll (false);
}

Keychain
StorageManager::defaultKeychain()
{
	StLock<Mutex>_(mMutex);

	Keychain theKeychain;
    CFTypeRef ref;

	{
		mSavedList.revert(false);
		DLDbIdentifier defaultDLDbIdentifier(mSavedList.defaultDLDbIdentifier());
		if (defaultDLDbIdentifier)
		{
			theKeychain = keychain(defaultDLDbIdentifier);
            ref = theKeychain->handle(false);
		}
	}

	if (theKeychain /* && theKeychain->exists() */)
		return theKeychain;

	MacOSError::throwMe(errSecNoDefaultKeychain);
}

void
StorageManager::defaultKeychain(const Keychain &keychain)
{
	StLock<Mutex>_(mMutex);

	// Only set a keychain as the default if we own it and can read/write it,
	// and our uid allows modifying the directory for that preference domain.
	if (!keychainOwnerPermissionsValidForDomain(keychain->name(), mDomain))
		MacOSError::throwMe(errSecWrPerm);

	DLDbIdentifier oldDefaultId;
	DLDbIdentifier newDefaultId(keychain->dlDbIdentifier());
	{
		oldDefaultId = mSavedList.defaultDLDbIdentifier();
		mSavedList.revert(true);
		mSavedList.defaultDLDbIdentifier(demungeDLDbIdentifier(newDefaultId));
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
	StLock<Mutex>_(mMutex);

	if (domain == kSecPreferencesDomainDynamic)
		MacOSError::throwMe(errSecInvalidPrefsDomain);

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
	StLock<Mutex>_(mMutex);

	if (domain == kSecPreferencesDomainDynamic)
		MacOSError::throwMe(errSecInvalidPrefsDomain);

	if (domain == mDomain)
		defaultKeychain(keychain);
	else
		DLDbListCFPref(domain).defaultDLDbIdentifier(keychain->dlDbIdentifier());
}

Keychain
StorageManager::loginKeychain()
{
	StLock<Mutex>_(mMutex);

	Keychain theKeychain;
	{
		mSavedList.revert(false);
		DLDbIdentifier loginDLDbIdentifier(mSavedList.loginDLDbIdentifier());
		if (loginDLDbIdentifier)
		{
			theKeychain = keychain(loginDLDbIdentifier);
		}
	}

	if (theKeychain && theKeychain->exists())
		return theKeychain;

	MacOSError::throwMe(errSecNoSuchKeychain);
}

DLDbIdentifier
StorageManager::loginKeychainDLDbIdentifer()
{
    StLock<Mutex>_(mMutex);
    DLDbIdentifier loginDLDbIdentifier(mSavedList.loginDLDbIdentifier());
    return mungeDLDbIdentifier(loginDLDbIdentifier, false);
}

void
StorageManager::loginKeychain(Keychain keychain)
{
	StLock<Mutex>_(mMutex);

	mSavedList.revert(true);
	mSavedList.loginDLDbIdentifier(demungeDLDbIdentifier(keychain->dlDbIdentifier()));
	mSavedList.save();
}

size_t
StorageManager::size()
{
	StLock<Mutex>_(mMutex);

    mSavedList.revert(false);
	mCommonList.revert(false);
	return mSavedList.searchList().size() + mCommonList.searchList().size();
}

Keychain
StorageManager::at(unsigned int ix)
{
	StLock<Mutex>_(mMutex);

	mSavedList.revert(false);
	DLDbList dLDbList = mSavedList.searchList();
	if (ix < dLDbList.size())
	{
		return keychain(dLDbList[ix]);
	}
	else
	{
		ix -= dLDbList.size();
		mCommonList.revert(false);
		DLDbList commonList = mCommonList.searchList();
		if (ix >= commonList.size())
			MacOSError::throwMe(errSecInvalidKeychain);

		return keychain(commonList[ix]);
	}
}

Keychain
StorageManager::operator[](unsigned int ix)
{
	StLock<Mutex>_(mMutex);

    return at(ix);
}

void StorageManager::rename(Keychain keychain, const char* newName)
{

	StLock<Mutex>_(mKeychainMapMutex);

    bool changedDefault = false;
	DLDbIdentifier newDLDbIdentifier;
	{
		mSavedList.revert(true);
		DLDbIdentifier defaultId = mSavedList.defaultDLDbIdentifier();

        // Find the keychain object for the given ref
        DLDbIdentifier dLDbIdentifier = keychain->dlDbIdentifier();

        if(!keychain->database()->isLocked()) {
            // Bring our unlock state with us
            DLDbIdentifier dldbi(dLDbIdentifier.ssuid(), newName, dLDbIdentifier.dbLocation());
            keychain->database()->transferTo(dldbi);
        } else {
            keychain->database()->rename(newName);
        }

        if (demungeDLDbIdentifier(dLDbIdentifier) == defaultId)
            changedDefault=true;

		newDLDbIdentifier = keychain->dlDbIdentifier();
        // Rename the keychain in the search list.
        mSavedList.rename(demungeDLDbIdentifier(dLDbIdentifier), demungeDLDbIdentifier(newDLDbIdentifier));

		// If this was the default keychain change it accordingly
		if (changedDefault)
			mSavedList.defaultDLDbIdentifier(demungeDLDbIdentifier(newDLDbIdentifier));

		mSavedList.save();

        // If the keychain wasn't in the cache, don't touch the cache.
        // Otherwise, update the cache to use its current identifier.
        if(keychain->inCache()) {
            registerKeychain(keychain);
        }
    }

	KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);

	if (changedDefault)
		KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent,
			newDLDbIdentifier);
}

void StorageManager::registerKeychain(Keychain& kc) {
    registerKeychainImpl(kc.get());
}

void StorageManager::registerKeychainImpl(KeychainImpl* kcimpl) {
    if(!kcimpl) {
        return;
    }

    {
        StLock<Mutex> _(mKeychainMapMutex);

        // First, iterate through the cache to see if this keychain is there. If so, remove it.
        forceRemoveFromCache(kcimpl);

        // If we renamed this keychain on top of an existing one, let's drop the old one from the cache.
        KeychainMap::iterator it = mKeychainMap.find(kcimpl->dlDbIdentifier());
        if (it != mKeychainMap.end())
        {
            Keychain oldKeychain(it->second);
            oldKeychain->inCache(false);
            // @@@ Ideally we should invalidate or fault this keychain object.
        }

        mKeychainMap.insert(KeychainMap::value_type(kcimpl->dlDbIdentifier(), kcimpl));
        kcimpl->inCache(true);
    } // drop mKeychainMapMutex
}

void StorageManager::forceRemoveFromCache(KeychainImpl* inKeychainImpl) {
    try {
        // Wrap all this in a try-block and ignore all errors - we're trying to clean up these maps
        {
            StLock<Mutex> _(mKeychainMapMutex);
            for(KeychainMap::iterator it = mKeychainMap.begin(); it != mKeychainMap.end(); ) {
                if(it->second == inKeychainImpl) {
                    // Increment the iterator, but use its pre-increment value for the erase
                    it->second->inCache(false);
                    mKeychainMap.erase(it++);
                } else {
                    it++;
                }
            }
        } // drop mKeychainMapMutex
    } catch(UnixError ue) {
        secnotice("storagemgr", "caught UnixError: %d %s", ue.unixError(), ue.what());
    } catch (CssmError cssme) {
        const char* errStr = cssmErrorString(cssme.error);
        secnotice("storagemgr", "caught CssmError: %d %s", (int) cssme.error, errStr);
    } catch (MacOSError mose) {
        secnotice("storagemgr", "MacOSError: %d", (int)mose.osStatus());
    } catch(...) {
        secnotice("storagemgr", "Unknown error");
    }
}

// If you pass NULL as the keychain, you must pass an oldName.
void StorageManager::renameUnique(Keychain keychain, CFStringRef oldName, CFStringRef newName, bool appendDbSuffix)
{
	StLock<Mutex>_(mMutex);

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
                CFStringAppendFormat(newNameCFStr, NULL, CFSTR("%s%d"), newNameCString, index);
                if(appendDbSuffix) {
                    CFStringAppend(newNameCFStr, CFSTR(kKeychainDbSuffix));
                } else {
                    CFStringAppend(newNameCFStr, CFSTR(kKeychainSuffix));	// add .keychain
                }
                char toUseBuff2[MAXPATHLEN];
                if ( CFStringGetCString(newNameCFStr, toUseBuff2, MAXPATHLEN, kCFStringEncodingUTF8) )	// make sure it fits in MAXPATHLEN, etc.
                {
                    struct stat filebuf;
                    if ( lstat(toUseBuff2, &filebuf) )
                    {
                        if(keychain) {
                            rename(keychain, toUseBuff2);
                            KeychainList kcList;
                            kcList.push_back(keychain);
                            remove(kcList, false);
                        } else {
                            // We don't have a Keychain object, so force the rename here if possible
                            char oldNameCString[MAXPATHLEN];
                            if ( CFStringGetCString(oldName, oldNameCString, MAXPATHLEN, kCFStringEncodingUTF8) ) {
                                int result = ::rename(oldNameCString, toUseBuff2);
                                secnotice("KClogin", "keychain force rename to %s: %d %d", newNameCString, result, (result == 0) ? 0 : errno);
                                if(result != 0) {
                                    UnixError::throwMe(errno);
                                }
                            } else {
                                secnotice("KClogin", "path is wrong, quitting");
                            }
                        }
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

#define KEYCHAIN_SYNC_KEY CFSTR("KeychainSyncList")
#define KEYCHAIN_SYNC_DOMAIN CFSTR("com.apple.keychainsync")

static CFStringRef MakeExpandedPath (const char* path)
{
	std::string name = DLDbListCFPref::ExpandTildesInPath (std::string (path));
	CFStringRef expanded = CFStringCreateWithCString (NULL, name.c_str (), 0);
	return expanded;
}

void StorageManager::removeKeychainFromSyncList (const DLDbIdentifier &id)
{
	StLock<Mutex>_(mMutex);

	// make a CFString of our identifier
	const char* idname = id.dbName ();
	if (idname == NULL)
	{
		return;
	}

	CFRef<CFStringRef> idString = MakeExpandedPath (idname);

	// check and see if this keychain is in the keychain syncing list
	CFArrayRef value =
		(CFArrayRef) CFPreferencesCopyValue (KEYCHAIN_SYNC_KEY,
											 KEYCHAIN_SYNC_DOMAIN,
											 kCFPreferencesCurrentUser,
											 kCFPreferencesAnyHost);
	if (value == NULL)
	{
		return;
	}

	// make a mutable copy of the dictionary
	CFRef<CFMutableArrayRef> mtValue = CFArrayCreateMutableCopy (NULL, 0, value);
	CFRelease (value);

	// walk the array, looking for the value
	CFIndex i;
	CFIndex limit = CFArrayGetCount (mtValue.get());
	bool found = false;

	for (i = 0; i < limit; ++i)
	{
		CFDictionaryRef idx = (CFDictionaryRef) CFArrayGetValueAtIndex (mtValue.get(), i);
		CFStringRef v = (CFStringRef) CFDictionaryGetValue (idx, CFSTR("DbName"));
		if (v == NULL)
		{
			return; // something is really wrong if this is taken
		}

        char* stringBuffer = NULL;
        const char* pathString = CFStringGetCStringPtr(v, 0);
        if (pathString == 0)
        {
            CFIndex maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(v), kCFStringEncodingUTF8) + 1;
            stringBuffer = (char*) malloc(maxLen);
            CFStringGetCString(v, stringBuffer, maxLen, kCFStringEncodingUTF8);
            pathString = stringBuffer;
        }
        
		CFStringRef vExpanded = MakeExpandedPath(pathString);
		CFComparisonResult result = CFStringCompare (vExpanded, idString.get(), 0);
        if (stringBuffer != NULL)
        {
            free(stringBuffer);
        }
        
		CFRelease (vExpanded);

		if (result == 0)
		{
			CFArrayRemoveValueAtIndex (mtValue.get(), i);
			found = true;
			break;
		}
	}

	if (found)
	{
#ifndef NDEBUG
		CFShow (mtValue.get());
#endif

		CFPreferencesSetValue (KEYCHAIN_SYNC_KEY,
							   mtValue,
							   KEYCHAIN_SYNC_DOMAIN,
							   kCFPreferencesCurrentUser,
							   kCFPreferencesAnyHost);
		CFPreferencesSynchronize (KEYCHAIN_SYNC_DOMAIN, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	}
}

void StorageManager::remove(const KeychainList &kcsToRemove, bool deleteDb)
{
	StLock<Mutex>_(mMutex);

	bool unsetDefault = false;
	bool updateList = (!isAppSandboxed());

	if (updateList)
	{
		mSavedList.revert(true);
		DLDbIdentifier defaultId = mSavedList.defaultDLDbIdentifier();
		for (KeychainList::const_iterator ix = kcsToRemove.begin();
			ix != kcsToRemove.end(); ++ix)
		{
			// Find the keychain object for the given ref
			Keychain theKeychain = *ix;
			DLDbIdentifier dLDbIdentifier = theKeychain->dlDbIdentifier();

			// Remove it from the saved list
			mSavedList.remove(demungeDLDbIdentifier(dLDbIdentifier));
            if (demungeDLDbIdentifier(dLDbIdentifier) == defaultId) {
				unsetDefault=true;
            }

			if (deleteDb)
			{
				removeKeychainFromSyncList (dLDbIdentifier);

				// Now remove it from the cache
				removeKeychain(dLDbIdentifier, theKeychain.get());
			}
		}

		if (unsetDefault)
            mSavedList.defaultDLDbIdentifier(DLDbIdentifier());

		mSavedList.save();
	}

	if (deleteDb)
	{
		// Delete the actual databases without holding any locks.
		for (KeychainList::const_iterator ix = kcsToRemove.begin();
			ix != kcsToRemove.end(); ++ix)
		{
			(*ix)->database()->deleteDb();
		}
	}

	if (updateList) {
		// Make sure we are not holding mLock when we post these events.
		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	}

	if (unsetDefault)
		KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent);
}

void
StorageManager::getSearchList(KeychainList &keychainList)
{
	// hold the global lock since we make keychain objects in this function

	// to do:  each of the items in this list must be retained, otherwise mayhem will occur
	StLock<Mutex>_(mMutex);

	if (gServerMode) {
		keychainList.clear();
		return;
	}

    mSavedList.revert(false);
	mCommonList.revert(false);

	// Merge mSavedList, mDynamicList and mCommonList
	DLDbList dLDbList = mSavedList.searchList();
	DLDbList dynamicList = mDynamicList.searchList();
	DLDbList commonList = mCommonList.searchList();
	KeychainList result;
	result.reserve(dLDbList.size() + dynamicList.size() + commonList.size());

	{
		for (DLDbList::const_iterator it = dynamicList.begin();
			it != dynamicList.end(); ++it)
		{
			Keychain k = keychain(*it);
			result.push_back(k);
		}

		for (DLDbList::const_iterator it = dLDbList.begin();
			it != dLDbList.end(); ++it)
		{
			Keychain k = keychain(*it);
			result.push_back(k);
		}

		for (DLDbList::const_iterator it = commonList.begin();
			it != commonList.end(); ++it)
		{
			Keychain k = keychain(*it);
			result.push_back(k);
		}
	}

	keychainList.swap(result);
}

void
StorageManager::setSearchList(const KeychainList &keychainList)
{
	StLock<Mutex>_(mMutex);

	DLDbList searchList, oldSearchList(mSavedList.searchList());
	for (KeychainList::const_iterator it = keychainList.begin(); it != keychainList.end(); ++it)
	{
        DLDbIdentifier dldbi = demungeDLDbIdentifier((*it)->dlDbIdentifier());

        // If this keychain is not in the common or dynamic lists, add it to the new search list
        DLDbList commonList = mCommonList.searchList();
        bool found = false;
        for(DLDbList::const_iterator jt = commonList.begin(); jt != commonList.end(); ++jt) {
            if((*jt) == dldbi) {
                found = true;
            }
        }

        DLDbList dynamicList = mDynamicList.searchList();
        for(DLDbList::const_iterator jt = dynamicList.begin(); jt != dynamicList.end(); ++jt) {
            if((*jt) == dldbi) {
                found = true;
            }
        }

        if(found) {
            continue;
        }

		searchList.push_back(dldbi);
	}

	{
		// Set the current searchlist to be what was passed in, the old list will be freed
		// upon exit of this stackframe.
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
	StLock<Mutex>_(mMutex);

	if (gServerMode) {
		keychainList.clear();
		return;
	}

	if (domain == kSecPreferencesDomainDynamic)
	{
		convertList(keychainList, mDynamicList.searchList());
	}
	else if (domain == mDomain)
	{
		mSavedList.revert(false);
		convertList(keychainList, mSavedList.searchList());
	}
	else
	{
		convertList(keychainList, DLDbListCFPref(domain).searchList());
	}
}

void StorageManager::forceUserSearchListReread()
{
	mSavedList.forceUserSearchListReread();
}

void
StorageManager::setSearchList(SecPreferencesDomain domain, const KeychainList &keychainList)
{
	StLock<Mutex>_(mMutex);

	if (domain == kSecPreferencesDomainDynamic)
		MacOSError::throwMe(errSecInvalidPrefsDomain);

	DLDbList searchList;
	convertList(searchList, keychainList);

	if (domain == mDomain)
	{
		DLDbList oldSearchList(mSavedList.searchList());
		{
			// Set the current searchlist to be what was passed in, the old list will be freed
			// upon exit of this stackframe.
			mSavedList.revert(true);
			mSavedList.searchList(searchList);
			mSavedList.save();
		}

		if (!(oldSearchList == searchList))
		{
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
	StLock<Mutex>_(mMutex);

	if (domain == kSecPreferencesDomainDynamic)
		MacOSError::throwMe(errSecInvalidPrefsDomain);

	if (domain == mDomain)
		return;	// no change

#if !defined(NDEBUG)
	switch (domain)
	{
	case kSecPreferencesDomainSystem:
		secnotice("storagemgr", "switching to system domain"); break;
	case kSecPreferencesDomainUser:
		secnotice("storagemgr", "switching to user domain (uid %d)", getuid()); break;
	default:
		secnotice("storagemgr", "switching to weird prefs domain %d", domain); break;
	}
#endif

	mDomain = domain;
	mSavedList.set(domain);
}

void
StorageManager::optionalSearchList(CFTypeRef keychainOrArray, KeychainList &keychainList)
{
	StLock<Mutex>_(mMutex);

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
			MacOSError::throwMe(errSecParam);
	}
}

// static methods.
void
StorageManager::convertToKeychainList(CFArrayRef keychainArray, KeychainList &keychainList)
{
	CFIndex count = CFArrayGetCount(keychainArray);
	if (!(count > 0))
		return;

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
		result.push_back(demungeDLDbIdentifier((*ix)->dlDbIdentifier()));
	}
	ids.swap(result);
}

void StorageManager::convertList(KeychainList &kcs, const DLDbList &ids)
{
	StLock<Mutex>_(mMutex);

	KeychainList result;
    result.reserve(ids.size());
	{
		for (DLDbList::const_iterator ix = ids.begin(); ix != ids.end(); ++ix)
			result.push_back(keychain(*ix));
	}
    kcs.swap(result);
}

#pragma mark ____ Login Functions ____

void StorageManager::login(AuthorizationRef authRef, UInt32 nameLength, const char* name, bool isReset)
{
	StLock<Mutex>_(mMutex);

    AuthorizationItemSet* info = NULL;
    OSStatus result = AuthorizationCopyInfo(authRef, NULL, &info);	// get the results of the copy rights call.
    Boolean created = false;
    if ( result == errSecSuccess && info->count )
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
                    login(nameLength, name, (UInt32)currItem->valueLength, currItem->value, isReset);
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
	StLock<Mutex>_(mMutex);

    if ( name == NULL || password == NULL )
        MacOSError::throwMe(errSecParam);

	login(name[0], name + 1, password[0], password + 1, false);
}

void StorageManager::login(UInt32 nameLength, const void *name,
	UInt32 passwordLength, const void *password, bool isReset)
{
	if (passwordLength != 0 && password == NULL)
	{
		secnotice("KCLogin", "StorageManager::login: invalid argument (NULL password)");
		MacOSError::throwMe(errSecParam);
	}

	DLDbIdentifier loginDLDbIdentifier;
	{
		mSavedList.revert(true);
		loginDLDbIdentifier = mSavedList.loginDLDbIdentifier();
	}

	secnotice("KCLogin", "StorageManager::login: loginDLDbIdentifier is %s", (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
	if (!loginDLDbIdentifier)
		MacOSError::throwMe(errSecNoSuchKeychain);


    //***************************************************************
    // gather keychain information
    //***************************************************************

    // user name
    int uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw == NULL) {
        secnotice("KCLogin", "StorageManager::login: invalid argument (NULL uid)");
        MacOSError::throwMe(errSecParam);
    }
    char *userName = pw->pw_name;

    // make keychain path strings
    std::string keychainPath = DLDbListCFPref::ExpandTildesInPath(kLoginKeychainPathPrefix);
    std::string shortnameKeychain = keychainPath + userName;
    std::string shortnameDotKeychain = shortnameKeychain + ".keychain";
    std::string loginDotKeychain = keychainPath + "login.keychain";
    std::string loginRenamed1Keychain = keychainPath + "login_renamed1.keychain";
    std::string loginKeychainDb =  keychainPath + "login.keychain-db";

    // check for existence of keychain files
    bool shortnameKeychainExists = false;
    bool shortnameDotKeychainExists = false;
    bool loginKeychainExists = false;
    bool loginRenamed1KeychainExists = false;
    bool loginKeychainDbExists = false;
    {
        struct stat st;
        int stat_result;
        stat_result = ::stat(shortnameKeychain.c_str(), &st);
        shortnameKeychainExists = (stat_result == 0);
        stat_result = ::stat(shortnameDotKeychain.c_str(), &st);
        shortnameDotKeychainExists = (stat_result == 0);
        stat_result = ::stat(loginDotKeychain.c_str(), &st);
        loginKeychainExists = (stat_result == 0);
        stat_result = ::stat(loginRenamed1Keychain.c_str(), &st);
        loginRenamed1KeychainExists = (stat_result == 0);
        stat_result = ::stat(loginKeychainDb.c_str(), &st);
        loginKeychainDbExists = (stat_result == 0);
    }

    // login.keychain-db is considered to be the same as login.keychain.
    // Our transparent keychain promotion on open will handle opening the right version of this file.
    loginKeychainExists |= loginKeychainDbExists;

    bool loginUnlocked = false;

    // make the keychain identifiers
    CSSM_VERSION version = {0, 0};
    DLDbIdentifier shortnameDLDbIdentifier = DLDbListCFPref::makeDLDbIdentifier(gGuidAppleCSPDL, version, 0, CSSM_SERVICE_CSP | CSSM_SERVICE_DL, shortnameKeychain.c_str(), NULL);
    DLDbIdentifier shortnameDotDLDbIdentifier = DLDbListCFPref::makeDLDbIdentifier(gGuidAppleCSPDL, version, 0, CSSM_SERVICE_CSP | CSSM_SERVICE_DL, shortnameDotKeychain.c_str(), NULL);
    DLDbIdentifier loginRenamed1DLDbIdentifier = DLDbListCFPref::makeDLDbIdentifier(gGuidAppleCSPDL, version, 0, CSSM_SERVICE_CSP | CSSM_SERVICE_DL, loginRenamed1Keychain.c_str(), NULL);

    //***************************************************************
    // make file renaming changes first
    //***************************************************************

    // if "~/Library/Keychains/shortname" exists, we need to migrate it forward;
    // either to login.keychain if there isn't already one, otherwise to shortname.keychain
    if (shortnameKeychainExists) {
        int rename_stat = 0;
        if (loginKeychainExists) {
            struct stat st;
            int tmp_result = ::stat(loginDotKeychain.c_str(), &st);
            if (tmp_result == 0) {
                if (st.st_size <= kEmptyKeychainSizeInBytes) {
                    tmp_result = ::unlink(loginDotKeychain.c_str());
                    rename_stat = ::rename(shortnameKeychain.c_str(), loginDotKeychain.c_str());
                    shortnameKeychainExists = (rename_stat != 0);
                }
            }
        }
        if (shortnameKeychainExists) {
            if (loginKeychainExists && !shortnameDotKeychainExists) {
                rename_stat = ::rename(shortnameKeychain.c_str(), shortnameDotKeychain.c_str());
                shortnameDotKeychainExists = (rename_stat == 0);
            } else if (!loginKeychainExists) {
                rename_stat = ::rename(shortnameKeychain.c_str(), loginDotKeychain.c_str());
                loginKeychainExists = (rename_stat == 0);
            } else {
                // we have all 3 keychains: login.keychain, shortname, and shortname.keychain.
                // on Leopard we never want a shortname keychain, so we must move it aside.
                char pathbuf[MAXPATHLEN];
                std::string shortnameRenamedXXXKeychain = keychainPath;
                shortnameRenamedXXXKeychain += userName;
                shortnameRenamedXXXKeychain += "_renamed_XXX.keychain";
                ::strlcpy(pathbuf, shortnameRenamedXXXKeychain.c_str(), sizeof(pathbuf));
                ::mkstemps(pathbuf, 9); // 9 == strlen(".keychain")
                rename_stat = ::rename(shortnameKeychain.c_str(), pathbuf);
                shortnameKeychainExists = (rename_stat != 0);
            }
        }
        if (rename_stat != 0) {
            MacOSError::throwMe(errno);
        }
    }

    //***************************************************************
    // handle special case where user previously reset the keychain
    //***************************************************************
    // Since 9A581, we have changed the definition of kKeychainRenamedSuffix from "_renamed" to "_renamed_".
    // Therefore, if "login_renamed1.keychain" exists and there is no plist, the user may have run into a
    // prior upgrade issue and clicked Reset. If we can successfully unlock login_renamed1.keychain with the
    // supplied password, then we will attempt to rename it to login.keychain if that file is empty, or with
    // "shortname.keychain" if it is not.

    if (loginRenamed1KeychainExists && (!loginKeychainExists ||
        (mSavedList.searchList().size() == 1 && mSavedList.member(demungeDLDbIdentifier(loginDLDbIdentifier))) )) {
        try
        {
            Keychain loginRenamed1KC(keychain(loginRenamed1DLDbIdentifier));
            secnotice("KCLogin", "Attempting to unlock renamed KC \"%s\"",
                      (loginRenamed1KC) ? loginRenamed1KC->name() : "<NULL>");
            loginRenamed1KC->unlock(CssmData(const_cast<void *>(password), passwordLength));
            // if we get here, we unlocked it
            if (loginKeychainExists) {
                struct stat st;
                int tmp_result = ::stat(loginDotKeychain.c_str(), &st);
                if (tmp_result == 0) {
                    if (st.st_size <= kEmptyKeychainSizeInBytes) {
                        tmp_result = ::unlink(loginDotKeychain.c_str());
                        tmp_result = ::rename(loginRenamed1Keychain.c_str(), loginDotKeychain.c_str());
                    } else if (!shortnameDotKeychainExists) {
                        tmp_result = ::rename(loginRenamed1Keychain.c_str(), shortnameDotKeychain.c_str());
                        shortnameDotKeychainExists = (tmp_result == 0);
                    } else {
                        throw 1;   // can't do anything with it except move it out of the way
                    }
                }
            } else {
                int tmp_result = ::rename(loginRenamed1Keychain.c_str(), loginDotKeychain.c_str());
                loginKeychainExists = (tmp_result == 0);
            }
        }
        catch(...)
        {
            // we failed to unlock the login_renamed1.keychain file with the login password.
            // move it aside so we don't try to deal with it again.
            char pathbuf[MAXPATHLEN];
            std::string loginRenamedXXXKeychain = keychainPath;
            loginRenamedXXXKeychain += "login_renamed_XXX.keychain";
            ::strlcpy(pathbuf, loginRenamedXXXKeychain.c_str(), sizeof(pathbuf));
            ::mkstemps(pathbuf, 9); // 9 == strlen(".keychain")
            ::rename(loginRenamed1Keychain.c_str(), pathbuf);
        }
    }

	// is it token login?
	CFRef<CFDictionaryRef> tokenLoginContext;
	CFRef<CFStringRef> smartCardPassword;
	OSStatus tokenContextStatus = TokenLoginGetContext(password, passwordLength, tokenLoginContext.take());
	// if login.keychain does not exist at this point, create it
	if (!loginKeychainExists || (isReset && !loginKeychainDbExists)) {
		// when we creating new KC and user is logged using token (i.e. smart card), we have to get
		// the password for that account first
		if (tokenContextStatus == errSecSuccess) {
			secnotice("KCLogin", "Going to create login keychain for sc login");
			AuthorizationRef authRef;
			OSStatus status = AuthorizationCreate(NULL, NULL, 0, &authRef);
			if (status == errSecSuccess) {
				AuthorizationItem right = { "com.apple.builtin.sc-kc-new-passphrase", 0, NULL, 0 };
				AuthorizationItemSet rightSet = { 1, &right };

				uint32_t reason, tries;
				reason = 0;
				tries = 0;
				AuthorizationItem envRights[] = {
					{ AGENT_HINT_RETRY_REASON, sizeof(reason), &reason, 0 },
					{ AGENT_HINT_TRIES, sizeof(tries), &tries, 0 }};

				AuthorizationItemSet envSet = { sizeof(envRights) / sizeof(*envRights), envRights };
				status = AuthorizationCopyRights(authRef, &rightSet, &envSet, kAuthorizationFlagDefaults|kAuthorizationFlagInteractionAllowed|kAuthorizationFlagExtendRights, NULL);
				if (status == errSecSuccess) {
					AuthorizationItemSet *returnedInfo;
					status = AuthorizationCopyInfo(authRef, NULL, &returnedInfo);
					if (status == errSecSuccess) {
						if (returnedInfo && (returnedInfo->count > 0)) {
							for (uint32_t index = 0; index < returnedInfo->count; index++) {
								AuthorizationItem &item = returnedInfo->items[index];
								if (!strcmp(AGENT_PASSWORD, item.name)) {
									CFIndex len = item.valueLength;
									if (len) {
										secnotice("KCLogin", "User entered pwd");
										smartCardPassword = CFStringCreateWithBytes(SecCFAllocatorZeroize(), (UInt8 *)item.value, (CFIndex)len, kCFStringEncodingUTF8, TRUE);
										memset(item.value, 0, len);
									}
								}
							}
						}
					}
					AuthorizationFreeItemSet(returnedInfo);
				}
				AuthorizationFree(authRef, 0);
			}
		}

        // but don't add it to the search list yet; we'll do that later
        Keychain theKeychain = makeKeychain(loginDLDbIdentifier, false, true);
		secnotice("KCLogin", "Creating login keychain %s", (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
		if (tokenContextStatus == errSecSuccess) {
			if (smartCardPassword.get()) {
				CFIndex length = CFStringGetLength(smartCardPassword);
				CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
				char *buffer = (char *)malloc(maxSize);
				if (CFStringGetCString(smartCardPassword, buffer, maxSize, kCFStringEncodingUTF8)) {
					secnotice("KCLogin", "Keychain is created using password provided by sc user");
					theKeychain->create((UInt32)strlen(buffer), buffer);
					memset(buffer, 0, maxSize);
				} else {
					secnotice("KCLogin", "Conversion failed");
					MacOSError::throwMe(errSecNotAvailable);
				}
			} else {
				secnotice("KCLogin", "User did not provide kc password");
				MacOSError::throwMe(errSecNotAvailable);
			}
		} else {
			theKeychain->create(passwordLength, password);
		}
        secnotice("KCLogin", "Login keychain created successfully");
        loginKeychainExists = true;
        // Set the prefs for this new login keychain.
        loginKeychain(theKeychain);
        // Login Keychain does not lock on sleep nor lock after timeout by default.
        theKeychain->setSettings(INT_MAX, false);
        loginUnlocked = true;
        mSavedList.revert(true);
    }

    //***************************************************************
    // make plist changes after files have been renamed or created
    //***************************************************************

    // if the shortname keychain exists in the search list, either rename or remove the entry
    if (mSavedList.member(demungeDLDbIdentifier(shortnameDLDbIdentifier))) {
        if (shortnameDotKeychainExists && !mSavedList.member(demungeDLDbIdentifier(shortnameDotDLDbIdentifier))) {
            // change shortname to shortname.keychain (login.keychain will be added later if not present)
            secnotice("KCLogin", "Renaming %s to %s in keychain search list",
                    (shortnameDLDbIdentifier) ? shortnameDLDbIdentifier.dbName() : "<NULL>",
                    (shortnameDotDLDbIdentifier) ? shortnameDotDLDbIdentifier.dbName() : "<NULL>");
            mSavedList.rename(demungeDLDbIdentifier(shortnameDLDbIdentifier),
                              demungeDLDbIdentifier(shortnameDotDLDbIdentifier));
        } else if (!mSavedList.member(demungeDLDbIdentifier(loginDLDbIdentifier))) {
            // change shortname to login.keychain
            secnotice("KCLogin", "Renaming %s to %s in keychain search list",
                    (shortnameDLDbIdentifier) ? shortnameDLDbIdentifier.dbName() : "<NULL>",
                    (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
            mSavedList.rename(demungeDLDbIdentifier(shortnameDLDbIdentifier),
                              demungeDLDbIdentifier(loginDLDbIdentifier));
        } else {
            // already have login.keychain in list, and renaming to shortname.keychain isn't an option,
            // so just remove the entry
            secnotice("KCLogin", "Removing %s from keychain search list", (shortnameDLDbIdentifier) ? shortnameDLDbIdentifier.dbName() : "<NULL>");
            mSavedList.remove(demungeDLDbIdentifier(shortnameDLDbIdentifier));
        }

        // note: save() will cause the plist to be unlinked if the only remaining entry is for login.keychain
        mSavedList.save();
        mSavedList.revert(true);
    }

    // make sure that login.keychain is in the search list
    if (!mSavedList.member(demungeDLDbIdentifier(loginDLDbIdentifier))) {
    	secnotice("KCLogin", "Adding %s to keychain search list", (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
        mSavedList.add(demungeDLDbIdentifier(loginDLDbIdentifier));
        mSavedList.save();
        mSavedList.revert(true);
    }

    // if we have a shortname.keychain, always include it in the plist (after login.keychain)
    if (shortnameDotKeychainExists && !mSavedList.member(demungeDLDbIdentifier(shortnameDotDLDbIdentifier))) {
        mSavedList.add(demungeDLDbIdentifier(shortnameDotDLDbIdentifier));
        mSavedList.save();
        mSavedList.revert(true);
    }

    // make sure that the default keychain is in the search list; if not, reset the default to login.keychain
	if (!mSavedList.member(mSavedList.defaultDLDbIdentifier())) {
    	secnotice("KCLogin", "Changing default keychain to %s", (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
        mSavedList.defaultDLDbIdentifier(demungeDLDbIdentifier(loginDLDbIdentifier));
        mSavedList.save();
        mSavedList.revert(true);
	}

    //***************************************************************
    // auto-unlock the login keychain(s)
    //***************************************************************
    // all our preflight fixups are finally done, so we can now attempt to unlock the login keychain

    OSStatus loginResult = errSecSuccess;
	if (!loginUnlocked) {
        try
        {
            Keychain theKeychain(keychain(loginDLDbIdentifier));
            secnotice("KCLogin", "Attempting to unlock login keychain \"%s\"",
                (theKeychain) ? theKeychain->name() : "<NULL>");
            theKeychain->unlock(CssmData(const_cast<void *>(password), passwordLength));
            loginUnlocked = true;
        }
        catch(const CssmError &e)
        {
            loginResult = e.osStatus(); // save this result
        }
    }

	if (!loginUnlocked || tokenContextStatus == errSecSuccess) {
		Keychain theKeychain(keychain(loginDLDbIdentifier));
		bool tokenLoginDataUpdated = false;

		for (UInt32 i = 0; i < 2; i++) {
			loginResult = errSecSuccess;

			CFRef<CFDictionaryRef> tokenLoginData;
			if (tokenLoginContext) {
				OSStatus status = TokenLoginGetLoginData(tokenLoginContext, tokenLoginData.take());
				if (status != errSecSuccess) {
					if (tokenLoginDataUpdated) {
						loginResult = status;
						break;
					}
					// updating unlock key fails if it is not token login
					secnotice("KCLogin", "Error %d, reconstructing unlock data", (int)status);
					status = TokenLoginUpdateUnlockData(tokenLoginContext, smartCardPassword);
					if (status == errSecSuccess) {
						loginResult = TokenLoginGetLoginData(tokenLoginContext, tokenLoginData.take());
						if (loginResult != errSecSuccess) {
							break;
						}
						tokenLoginDataUpdated = true;
					}
				}
			}

            try {
				// first try to unlock login keychain because if this fails, token keychain unlock fails as well
				if (tokenLoginData) {
					secnotice("KCLogin", "Going to unlock keybag using scBlob");
					OSStatus status = TokenLoginUnlockKeybag(tokenLoginContext, tokenLoginData);
					secnotice("KCLogin", "Keybag unlock result %d", (int)status);
					if (status)
						CssmError::throwMe(status); // to trigger login data regeneration
				}

                // build a fake key
                CssmKey key;
                key.header().BlobType = CSSM_KEYBLOB_RAW;
                key.header().Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
                key.header().AlgorithmId = CSSM_ALGID_3DES_3KEY;
                key.header().KeyClass = CSSM_KEYCLASS_SESSION_KEY;
                key.header().KeyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT | CSSM_KEYATTR_EXTRACTABLE;
                key.header().KeyAttr = 0;
                CFRef<CFDataRef> tokenLoginUnlockKey;
				if (tokenLoginData) {
					OSStatus status = TokenLoginGetUnlockKey(tokenLoginContext, tokenLoginUnlockKey.take());
					if (status)
						CssmError::throwMe(status); // to trigger login data regeneration
					key.KeyData = CssmData(tokenLoginUnlockKey.get());
				} else {
					key.KeyData = CssmData(const_cast<void *>(password), passwordLength);
				}
                // unwrap it into the CSP (but keep it raw)
                UnwrapKey unwrap(theKeychain->csp(), CSSM_ALGID_NONE);
                CssmKey masterKey;
                CssmData descriptiveData;
                unwrap(key,
                       KeySpec(CSSM_KEYUSE_ANY, CSSM_KEYATTR_EXTRACTABLE),
                       masterKey, &descriptiveData, NULL);
                
                CssmClient::Db db = theKeychain->database();
                
                // create the keychain, using appropriate credentials
                Allocator &alloc = db->allocator();
                AutoCredentials cred(alloc);	// will leak, but we're quitting soon :-)
                
                // use this passphrase
                cred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
                                  new(alloc) ListElement(CSSM_SAMPLE_TYPE_SYMMETRIC_KEY),
                                  new(alloc) ListElement(CssmData::wrap(theKeychain->csp()->handle())),
                                  new(alloc) ListElement(CssmData::wrap(masterKey)),
                                  new(alloc) ListElement(CssmData()));
                db->authenticate(CSSM_DB_ACCESS_READ, &cred);
                db->unlock();
                loginUnlocked = true;
            } catch (const CssmError &e) {
                if (tokenLoginData && !tokenLoginDataUpdated) {
                    // token login unlock key was invalid
					loginResult = TokenLoginUpdateUnlockData(tokenLoginContext, smartCardPassword);
                    if (loginResult == errSecSuccess) {
                        tokenLoginDataUpdated = true;
                        continue;
                    }
                }
                else {
                    loginResult = e.osStatus();
                }
            }
            break;
        }
    }

    // if "shortname.keychain" exists and is in the search list, attempt to auto-unlock it with the same password
    if (shortnameDotKeychainExists && mSavedList.member(demungeDLDbIdentifier(shortnameDotDLDbIdentifier))) {
        try
        {
            Keychain shortnameDotKC(keychain(shortnameDotDLDbIdentifier));
            secnotice("KCLogin", "Attempting to unlock short name keychain \"%s\"",
                (shortnameDotKC) ? shortnameDotKC->name() : "<NULL>");
            shortnameDotKC->unlock(CssmData(const_cast<void *>(password), passwordLength));
        }
        catch(const CssmError &e)
        {
            // ignore; failure to unlock this keychain is not considered an error
        }
    }

    if (loginResult != errSecSuccess) {
        MacOSError::throwMe(loginResult);
    }
}

void StorageManager::stashLogin()
{
    OSStatus loginResult = errSecSuccess;
    
    DLDbIdentifier loginDLDbIdentifier;
    {
        mSavedList.revert(true);
        loginDLDbIdentifier = mSavedList.loginDLDbIdentifier();
    }
    
	secnotice("KCLogin", "StorageManager::stash: loginDLDbIdentifier is %s", (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
	if (!loginDLDbIdentifier)
		MacOSError::throwMe(errSecNoSuchKeychain);
    
    try
    {
        CssmData empty;
        Keychain theKeychain(keychain(loginDLDbIdentifier));
        secnotice("KCLogin", "Attempting to use stash for login keychain \"%s\"",
                 (theKeychain) ? theKeychain->name() : "<NULL>");
        theKeychain->stashCheck();
    }
    catch(const CssmError &e)
    {
        loginResult = e.osStatus(); // save this result
    }
    
    
    if (loginResult != errSecSuccess) {
        MacOSError::throwMe(loginResult);
    }
}

void StorageManager::stashKeychain()
{
    OSStatus loginResult = errSecSuccess;
    
    DLDbIdentifier loginDLDbIdentifier;
    {
        mSavedList.revert(true);
        loginDLDbIdentifier = mSavedList.loginDLDbIdentifier();
    }
    
	secnotice("KCLogin", "StorageManager::stash: loginDLDbIdentifier is %s", (loginDLDbIdentifier) ? loginDLDbIdentifier.dbName() : "<NULL>");
	if (!loginDLDbIdentifier)
		MacOSError::throwMe(errSecNoSuchKeychain);
    
    try
    {
        Keychain theKeychain(keychain(loginDLDbIdentifier));
        secnotice("KCLogin", "Attempting to stash login keychain \"%s\"",
                 (theKeychain) ? theKeychain->name() : "<NULL>");
        theKeychain->stash();
    }
    catch(const CssmError &e)
    {
        loginResult = e.osStatus(); // save this result
    }


    if (loginResult != errSecSuccess) {
        MacOSError::throwMe(loginResult);
    }
}

void StorageManager::logout()
{
    // nothing left to do here
}

void StorageManager::changeLoginPassword(ConstStringPtr oldPassword, ConstStringPtr newPassword)
{
	StLock<Mutex>_(mMutex);

	loginKeychain()->changePassphrase(oldPassword, newPassword);
	secnotice("KClogin", "Changed login keychain password successfully");
}


void StorageManager::changeLoginPassword(UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword)
{
	StLock<Mutex>_(mMutex);

	loginKeychain()->changePassphrase(oldPasswordLength, oldPassword,  newPasswordLength, newPassword);
	secnotice("KClogin", "Changed login keychain password successfully");
}

// Clear out the keychain search list and rename the existing login.keychain.
//
void StorageManager::resetKeychain(Boolean resetSearchList)
{
	StLock<Mutex>_(mMutex);

    // Clear the keychain search list.
    Keychain keychain = NULL;
    DLDbIdentifier dldbi;
    try
    {
        if ( resetSearchList )
        {
            StorageManager::KeychainList keychainList;
            setSearchList(keychainList);
        }
        // Get a reference to the existing login keychain...
        // If we don't have one, we throw (not requiring a rename).
        //
        keychain = loginKeychain();
    } catch(const CommonError& e) {
        secnotice("KClogin", "Failed to open login keychain due to an error: %s", e.what());

        // Set up fallback rename.
        dldbi = loginKeychainDLDbIdentifer();

        struct stat exists;
        if(::stat(dldbi.dbName(), &exists) != 0) {
            // no file exists, everything is fine
            secnotice("KClogin", "no file exists; resetKeychain() is done");
            return;
        }
    }

    try{
        //
        // Rename the existing login.keychain (i.e. put it aside).
        //
        CFMutableStringRef newName = NULL;
        newName = CFStringCreateMutable(NULL, 0);
        CFStringRef currName = NULL;
        if(keychain) {
            currName = CFStringCreateWithCString(NULL, keychain->name(), kCFStringEncodingUTF8);
        } else {
            currName = CFStringCreateWithCString(NULL, dldbi.dbName(), kCFStringEncodingUTF8);
        }
        if ( newName && currName )
        {
            CFStringAppend(newName, currName);
            CFStringRef kcSuffix = CFSTR(kKeychainSuffix);
            CFStringRef kcDbSuffix = CFSTR(kKeychainDbSuffix);
            bool hasDbSuffix = false;
            if ( CFStringHasSuffix(newName, kcSuffix) )	// remove the .keychain extension
            {
                CFRange suffixRange = CFStringFind(newName, kcSuffix, 0);
                CFStringFindAndReplace(newName, kcSuffix, CFSTR(""), suffixRange, 0);
            }
            if (CFStringHasSuffix(newName, kcDbSuffix)) {
                hasDbSuffix = true;
                CFRange suffixRange = CFStringFind(newName, kcDbSuffix, 0);
                CFStringFindAndReplace(newName, kcDbSuffix, CFSTR(""), suffixRange, 0);
            }

            CFStringAppend(newName, CFSTR(kKeychainRenamedSuffix));	// add "_renamed_"
            try
            {
                secnotice("KClogin", "attempting keychain rename to %@", newName);
                renameUnique(keychain, currName, newName, hasDbSuffix);
            }
            catch(const CommonError& e)
            {
                // we need to release 'newName' & 'currName'
                secnotice("KClogin", "Failed to renameUnique due to an error: %s", e.what());
            }
            catch(...)
            {
                secnotice("KClogin", "Failed to renameUnique due to an unknown error");
            }
        }	 // else, let the login call report a duplicate
        else {
            secnotice("KClogin", "don't have paths, quitting");
        }
        if ( newName )
            CFRelease(newName);
        if ( currName )
            CFRelease(currName);
    }
    catch(const CommonError& e) {
        secnotice("KClogin", "Failed to reset login keychain due to an error: %s", e.what());
    }
    catch(...)
    {
        // We either don't have a login keychain, or there was a
        // failure to rename the existing one.
        secnotice("KClogin", "Failed to reset keychain due to an unknown error");
    }
}

#pragma mark ____ File Related ____

Keychain StorageManager::make(const char *pathName)
{
	return make(pathName, true);
}

Keychain StorageManager::make(const char *pathName, bool add)
{
    return make(pathName, add, false);
}

Keychain StorageManager::make(const char *pathName, bool add, bool isReset) {
    return makeKeychain(makeDLDbIdentifier(pathName), add, isReset);
}

DLDbIdentifier StorageManager::makeDLDbIdentifier(const char *pathName) {
	StLock<Mutex>_(mMutex);

	string fullPathName;
    if ( pathName[0] == '/' )
		fullPathName = pathName;
	else
    {
		// Get Home directory from environment.
		switch (mDomain)
		{
		case kSecPreferencesDomainUser:
			{
				const char *homeDir = getenv("HOME");
				if (homeDir == NULL)
				{
					// If $HOME is unset get the current user's home directory
					// from the passwd file.
					uid_t uid = geteuid();
					if (!uid) uid = getuid();
					struct passwd *pw = getpwuid(uid);
					if (!pw)
						MacOSError::throwMe(errSecParam);
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
    DLDbIdentifier dlDbIdentifier(ssuid, fullPathName.c_str(), DbLocation);
    return dlDbIdentifier;
}

Keychain StorageManager::makeLoginAuthUI(const Item *item, bool isReset)
{
	StLock<Mutex>_(mMutex);

    // Create a login/default keychain for the user using UI.
    // The user can cancel out of the operation, or create a new login keychain.
    // If auto-login is turned off, the user will be asked for their login password.
    //
    OSStatus result = errSecSuccess;
    Keychain keychain;	// We return this keychain.
    //
    // Set up the Auth ref to bring up UI.
    //
	AuthorizationItem *currItem, *authEnvirItemArrayPtr = NULL;
    AuthorizationRef authRef = NULL;
	try
	{
		result = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authRef);
		if ( result )
			MacOSError::throwMe(result);

		AuthorizationEnvironment envir;
		envir.count = 6;	// up to 6 hints can be used.
		authEnvirItemArrayPtr = (AuthorizationItem*)malloc(sizeof(AuthorizationItem) * envir.count);
		if ( !authEnvirItemArrayPtr )
			MacOSError::throwMe(errAuthorizationInternal);

		currItem = envir.items = authEnvirItemArrayPtr;

		//
		// 1st Hint (optional): The keychain item's account attribute string.
		//						When item is specified, we assume an 'add' operation is being attempted.
		char buff[256];
		UInt32 actLen = 0;
		SecKeychainAttribute attr = { kSecAccountItemAttr, 255, &buff };
		if ( item )
		{
			try
			{
				(*item)->getAttribute(attr, &actLen);
			}
			catch(...)
			{
				actLen = 0;	// This item didn't have the account attribute, so don't display one in the UI.
			}
		}
		currItem->name = AGENT_HINT_ATTR_NAME;	// name str that identifies this hint as attr name
		if ( actLen )	// Fill in the hint if we have an account attr
		{
			if ( actLen >= sizeof(buff) )
				buff[sizeof(buff)-1] = 0;
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
			currDefaultName = (char*)defaultKeychain()->name();	// Use the name if we have it.
			currItem->name = AGENT_HINT_LOGIN_KC_NAME;	// Name str that identifies this hint as kc path
			currItem->valueLength = (currDefaultName) ? strlen(currDefaultName) : 0;
			currItem->value = (currDefaultName) ? (void*)currDefaultName : (void*)"";
			currItem->flags = 0;
			currItem++;
		}
		catch(...)
		{
			envir.count--;
		}

		//
		// 3rd Hint (required): check if curr default keychain is unavailable.
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
		// 4th Hint (required): userName
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
		if ( userName.length() == 0 )	// did we ultimately get one?
			MacOSError::throwMe(errAuthorizationInternal);

		currItem->value = (void*)userName.c_str();
		currItem->valueLength = userName.length();
		currItem->flags = 0;

		//
		// 5th Hint (required): flags if user has more than 1 keychain (used for a later warning when reset to default).
		//
		currItem++;
		currItem->name = AGENT_HINT_LOGIN_KC_USER_HAS_OTHER_KCS_STR;
		Boolean moreThanOneKCExists = false;
		{
			// if item is NULL, then this is a user-initiated full reset
			if (item && mSavedList.searchList().size() > 1)
				moreThanOneKCExists = true;
		}
		currItem->value = &moreThanOneKCExists;
		currItem->valueLength = sizeof(Boolean);
		currItem->flags = 0;

		//
		// 6th Hint (required): If no item is involved, this is a user-initiated full reset.
		// We want to suppress the "do you want to reset to defaults?" panel in this case.
		//
		currItem++;
		currItem->name = AGENT_HINT_LOGIN_KC_SUPPRESS_RESET_PANEL;
		Boolean suppressResetPanel = (item == NULL) ? TRUE : FALSE;
		currItem->valueLength = sizeof(Boolean);
		currItem->value = (void*)&suppressResetPanel;
		currItem->flags = 0;

		//
		// Set up the auth rights and make the auth call.
		//
		AuthorizationItem authItem = { LOGIN_KC_CREATION_RIGHT, 0 , NULL, 0 };
		AuthorizationRights rights = { 1, &authItem };
		AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights;
		result = AuthorizationCopyRights(authRef, &rights, &envir, flags, NULL);
		if ( result )
			MacOSError::throwMe(result);
		try
		{
			resetKeychain(true); // Clears the plist, moves aside existing login.keychain
		}
		catch (...) // can throw if no existing login.keychain is found
		{
		}
		login(authRef, (UInt32)userName.length(), userName.c_str(), isReset); // Create login.keychain
		keychain = loginKeychain(); // Get newly-created login keychain
		defaultKeychain(keychain);	// Set it to be the default

		free(authEnvirItemArrayPtr);
		AuthorizationFree(authRef, kAuthorizationFlagDefaults);
	}

	catch (...)
	{
		// clean up allocations, then rethrow error
		if ( authEnvirItemArrayPtr )
			free(authEnvirItemArrayPtr);
		if ( authRef )
			AuthorizationFree(authRef, kAuthorizationFlagDefaults);
		throw;
	}

    return keychain;
}

Keychain StorageManager::defaultKeychainUI(Item &item)
{
	StLock<Mutex>_(mMutex);

    Keychain returnedKeychain;
    try
    {
        returnedKeychain = defaultKeychain(); // If we have one, return it.
        if ( returnedKeychain->exists() )
            return returnedKeychain;
    }
    catch(...)	// We could have one, but it isn't available (i.e. on a un-mounted volume).
    {
    }
    if ( globals().getUserInteractionAllowed() )
    {
        returnedKeychain = makeLoginAuthUI(&item, false); // If no Keychains is present, one will be created.
        if ( !returnedKeychain )
            MacOSError::throwMe(errSecInvalidKeychain);	// Something went wrong...
    }
    else
        MacOSError::throwMe(errSecInteractionNotAllowed); // If UI isn't allowed, return an error.

    return returnedKeychain;
}

void
StorageManager::addToDomainList(SecPreferencesDomain domain,
	const char* dbName, const CSSM_GUID &guid, uint32 subServiceType)
{
	StLock<Mutex>_(mMutex);

	if (domain == kSecPreferencesDomainDynamic)
		MacOSError::throwMe(errSecInvalidPrefsDomain);

	// make the identifier
	CSSM_VERSION version = {0, 0};
	DLDbIdentifier id = DLDbListCFPref::makeDLDbIdentifier (guid, version, 0,
		subServiceType, dbName, NULL);

	if (domain == mDomain)
	{
		// manipulate the user's list
		{
			mSavedList.revert(true);
			mSavedList.add(demungeDLDbIdentifier(id));
			mSavedList.save();
		}

		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	}
	else
	{
		// manipulate the other list
		DLDbListCFPref(domain).add(id);
	}
}

void
StorageManager::isInDomainList(SecPreferencesDomain domain,
	const char* dbName, const CSSM_GUID &guid, uint32 subServiceType)
{
	StLock<Mutex>_(mMutex);

	if (domain == kSecPreferencesDomainDynamic)
		MacOSError::throwMe(errSecInvalidPrefsDomain);

	CSSM_VERSION version = {0, 0};
	DLDbIdentifier id = DLDbListCFPref::makeDLDbIdentifier (guid, version, 0,
		subServiceType, dbName, NULL);

	// determine the list to search
	bool result;
	if (domain == mDomain)
	{
		result = mSavedList.member(demungeDLDbIdentifier(id));
	}
	else
	{
		result = DLDbListCFPref(domain).member(demungeDLDbIdentifier(id));
	}

	// do the search
	if (!result)
	{
		MacOSError::throwMe(errSecNoSuchKeychain);
	}
}

void
StorageManager::removeFromDomainList(SecPreferencesDomain domain,
	const char* dbName, const CSSM_GUID &guid, uint32 subServiceType)
{
	StLock<Mutex>_(mMutex);

	if (domain == kSecPreferencesDomainDynamic)
		MacOSError::throwMe(errSecInvalidPrefsDomain);

	// make the identifier
	CSSM_VERSION version = {0, 0};
	DLDbIdentifier id = DLDbListCFPref::makeDLDbIdentifier (guid, version, 0,
		subServiceType, dbName, NULL);

	if (domain == mDomain)
	{
		// manipulate the user's list
		{
			mSavedList.revert(true);
			mSavedList.remove(demungeDLDbIdentifier(id));
			mSavedList.save();
		}

		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	}
	else
	{
		// manipulate the other list
		DLDbListCFPref(domain).remove(id);
	}
}

bool
StorageManager::keychainOwnerPermissionsValidForDomain(const char* path, SecPreferencesDomain domain)
{
	struct stat sb;
	mode_t perms;
	const char* sysPrefDir = "/Library/Preferences";
	const char* errMsg = "Will not set default";
	char* mustOwnDir = NULL;
	struct passwd* pw = NULL;

	// get my uid
	uid_t uid = geteuid();
	if (!uid) uid = getuid();

	// our (e)uid must own the appropriate preferences or home directory
	// for the specified preference domain whose default we will be modifying
	switch (domain) {
		case kSecPreferencesDomainUser:
			mustOwnDir = getenv("HOME");
			if (mustOwnDir == NULL) {
				pw = getpwuid(uid);
				if (!pw) return false;
				mustOwnDir = pw->pw_dir;
			}
			break;
		case kSecPreferencesDomainSystem:
			mustOwnDir = (char*)sysPrefDir;
			break;
		case kSecPreferencesDomainCommon:
			mustOwnDir = (char*)sysPrefDir;
			break;
		default:
			return false;
	}

	if (mustOwnDir != NULL) {
		struct stat dsb;
		if ( (stat(mustOwnDir, &dsb) != 0) || (dsb.st_uid != uid) ) {
			fprintf(stderr, "%s: UID=%d does not own directory %s\n", errMsg, (int)uid, mustOwnDir);
			mustOwnDir = NULL; // will return below after calling endpwent()
		}
	}

	if (pw != NULL)
		endpwent();

	if (mustOwnDir == NULL)
		return false;

	// check that file actually exists
	if (stat(path, &sb) != 0) {
		fprintf(stderr, "%s: file %s does not exist\n", errMsg, path);
		return false;
	}

	// check flags
	if (sb.st_flags & (SF_IMMUTABLE | UF_IMMUTABLE)) {
		fprintf(stderr, "%s: file %s is immutable\n", errMsg, path);
		return false;
	}

	// check ownership
	if (sb.st_uid != uid) {
		fprintf(stderr, "%s: file %s is owned by UID=%d, but we have UID=%d\n",
			errMsg, path, (int)sb.st_uid, (int)uid);
		return false;
	}

	// check mode
	perms = sb.st_mode;
	perms |= 0600; // must have owner read/write permission set
	if (sb.st_mode != perms) {
		fprintf(stderr, "%s: file %s does not have the expected permissions\n", errMsg, path);
		return false;
	}

	// user owns file and can read/write it
	return true;
}
