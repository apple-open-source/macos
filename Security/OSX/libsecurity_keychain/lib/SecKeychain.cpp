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

#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>
#include <security_keychain/KCCursor.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_client/wrapkey.h>
#include <security_keychain/KCExceptions.h>
#include <securityd_client/ssblob.h>
#include <Security/SecAccess.h>
#include <Security/SecTrustedApplicationPriv.h>
#include "SecBridge.h"
#include "CCallbackMgr.h"
#include <security_cdsa_utilities/Schema.h>
#include <security_cdsa_client/mdsclient.h>
#include <pwd.h>
#include <os/activity.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/Authorization.h>
#include "TokenLogin.h"
#include "LegacyAPICounts.h"

extern "C" {
#include "ctkloginhelper.h"
}

OSStatus
SecKeychainMDSInstall()
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainMDSInstall", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	Security::MDSClient::Directory d;
	d.install();

	END_SECAPI
}

CFTypeID
SecKeychainGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().KeychainImpl.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecKeychainGetVersion(UInt32 *returnVers)
{
    COUNTLEGACYAPI
    if (!returnVers)
		return errSecSuccess;

	*returnVers = 0x02028000;
	return errSecSuccess;
}


OSStatus
SecKeychainOpen(const char *pathName, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainOpen", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(keychainRef)=globals().storageManager.make(pathName, false)->handle();

	END_SECAPI
}

OSStatus
SecKeychainCreate(const char *pathName, UInt32 passwordLength, const void *password,
	Boolean promptUser, SecAccessRef initialAccess, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCreate", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
    
    KCThrowParamErrIf_(!pathName);
	Keychain keychain = globals().storageManager.make(pathName, true, true);

	// @@@ the call to StorageManager::make above leaves keychain the the cache.
	// If the create below fails we should probably remove it.
	if(promptUser)
		keychain->create();
	else
	{
		KCThrowParamErrIf_(!password);
		keychain->create(passwordLength, password);
	}

	RequiredParam(keychainRef)=keychain->handle();

	END_SECAPI
}


OSStatus
SecKeychainDelete(SecKeychainRef keychainOrArray)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainDelete", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	KCThrowIf_(!keychainOrArray, errSecInvalidKeychain);
	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);

	globals().storageManager.remove(keychains, true);

	END_SECAPI
}


OSStatus
SecKeychainSetSettings(SecKeychainRef keychainRef, const SecKeychainSettings *newSettings)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainSetSettings", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	Keychain keychain = Keychain::optional(keychainRef);
	if (newSettings->version==SEC_KEYCHAIN_SETTINGS_VERS1)
	{
		UInt32 lockInterval=newSettings->lockInterval;
		bool lockOnSleep=newSettings->lockOnSleep;
		keychain->setSettings(lockInterval, lockOnSleep);
	}

	END_SECAPI
}


OSStatus
SecKeychainCopySettings(SecKeychainRef keychainRef, SecKeychainSettings *outSettings)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCopySettings", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	Keychain keychain = Keychain::optional(keychainRef);
	if (outSettings->version==SEC_KEYCHAIN_SETTINGS_VERS1)
	{
		uint32 lockInterval;
		bool lockOnSleep;
		
		keychain->getSettings(lockInterval, lockOnSleep);
		outSettings->lockInterval=lockInterval;
		outSettings->lockOnSleep=lockOnSleep;
	}

	END_SECAPI
}


OSStatus
SecKeychainUnlock(SecKeychainRef keychainRef, UInt32 passwordLength, const void *password, Boolean usePassword)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainUnlock", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	Keychain keychain = Keychain::optional(keychainRef);

	if (usePassword)
		keychain->unlock(CssmData(const_cast<void *>(password), passwordLength));
	else
		keychain->unlock();

	END_SECAPI
}


OSStatus
SecKeychainLock(SecKeychainRef	keychainRef)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainLock", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	Keychain keychain = Keychain::optional(keychainRef);
	keychain->lock();

	END_SECAPI
}


OSStatus
SecKeychainLockAll(void)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainLockAll", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	globals().storageManager.lockAll();

	END_SECAPI
}


OSStatus SecKeychainResetLogin(UInt32 passwordLength, const void* password, Boolean resetSearchList)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainResetLogin", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
        //
        // Get the current user (using fallback method if necessary)
        //
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
        {
            MacOSError::throwMe(errAuthorizationInternal);
        }

        SecurityServer::ClientSession().resetKeyStorePassphrase(password ? CssmData(const_cast<void *>(password), passwordLength) : CssmData());
        secwarning("SecKeychainResetLogin: reset AKS passphrase");
		if (password)
		{
			// Clear the plist and move aside (rename) the existing login.keychain
			globals().storageManager.resetKeychain(resetSearchList);

			// Create the login keychain without UI
			globals().storageManager.login((UInt32)userName.length(), userName.c_str(), passwordLength, password, true);
			
			// Set it as the default
			Keychain keychain = globals().storageManager.loginKeychain();
			globals().storageManager.defaultKeychain(keychain);
		}
		else
		{
			// Create the login keychain, prompting for password
			// (implicitly calls resetKeychain, login, and defaultKeychain)
			globals().storageManager.makeLoginAuthUI(NULL, true);
		}
        secwarning("SecKeychainResetLogin: reset osx keychain");

		// Post a "list changed" event after a reset, so apps can refresh their list.
		// Make sure we are not holding mLock when we post this event.
		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);


	END_SECAPI
}

OSStatus
SecKeychainCopyDefault(SecKeychainRef *keychainRef)
{
	BEGIN_SECAPI

	RequiredParam(keychainRef)=globals().storageManager.defaultKeychain()->handle();

	END_SECAPI
}


OSStatus
SecKeychainSetDefault(SecKeychainRef keychainRef)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainSetDefault", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	globals().storageManager.defaultKeychain(Keychain::optional(keychainRef));

	END_SECAPI
}

OSStatus SecKeychainCopySearchList(CFArrayRef *searchList)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCopySearchList", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(searchList);
	StorageManager &smr = globals().storageManager;
	StorageManager::KeychainList keychainList;
	smr.getSearchList(keychainList);
	*searchList = smr.convertFromKeychainList(keychainList);

	END_SECAPI
}

OSStatus SecKeychainSetSearchList(CFArrayRef searchList)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainSetSearchList", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(searchList);
	StorageManager &smr = globals().storageManager;
	StorageManager::KeychainList keychainList;
	smr.convertToKeychainList(searchList, keychainList);
	smr.setSearchList(keychainList);

	END_SECAPI
}

OSStatus SecKeychainCopyDomainDefault(SecPreferencesDomain domain, SecKeychainRef *keychainRef)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCopyDomainDefault", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(keychainRef)=globals().storageManager.defaultKeychain(domain)->handle();

	END_SECAPI
}

OSStatus SecKeychainSetDomainDefault(SecPreferencesDomain domain, SecKeychainRef keychainRef)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainSetDomainDefault", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	globals().storageManager.defaultKeychain(domain, Keychain::optional(keychainRef));

	END_SECAPI
}

OSStatus SecKeychainCopyDomainSearchList(SecPreferencesDomain domain, CFArrayRef *searchList)
{
	BEGIN_SECAPI

	RequiredParam(searchList);
	StorageManager &smr = globals().storageManager;
	StorageManager::KeychainList keychainList;
	smr.getSearchList(domain, keychainList);
	*searchList = smr.convertFromKeychainList(keychainList);

	END_SECAPI
}

OSStatus SecKeychainSetDomainSearchList(SecPreferencesDomain domain, CFArrayRef searchList)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainSetDomainSearchList", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(searchList);
	StorageManager &smr = globals().storageManager;
	StorageManager::KeychainList keychainList;
	smr.convertToKeychainList(searchList, keychainList);
	smr.setSearchList(domain, keychainList);

	END_SECAPI
}

OSStatus SecKeychainSetPreferenceDomain(SecPreferencesDomain domain)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainSetPreferenceDomain", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	globals().storageManager.domain(domain);

	END_SECAPI
}

OSStatus SecKeychainGetPreferenceDomain(SecPreferencesDomain *domain)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainGetPreferenceDomain", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
	
	*domain = globals().storageManager.domain();
	
	END_SECAPI
}


OSStatus
SecKeychainGetStatus(SecKeychainRef keychainRef, SecKeychainStatus *keychainStatus)
{
    BEGIN_SECAPI

	RequiredParam(keychainStatus) = (SecKeychainStatus)Keychain::optional(keychainRef)->status();

	END_SECAPI
}


OSStatus
SecKeychainGetPath(SecKeychainRef keychainRef, UInt32 *ioPathLength, char *pathName)
{
    BEGIN_SECAPI

	RequiredParam(pathName);
	RequiredParam(ioPathLength);

    const char *name = Keychain::optional(keychainRef)->name();
	UInt32 nameLen = (UInt32)strlen(name);
	UInt32 callersLen = *ioPathLength;
	*ioPathLength = nameLen;
	if (nameLen+1 > callersLen)  // if the client's buffer is too small (including null-termination), throw
		return errSecBufferTooSmall;
	strncpy(pathName, name, nameLen);
    pathName[nameLen] = 0;
	*ioPathLength = nameLen;   // set the length.
    		
	END_SECAPI
}

OSStatus
SecKeychainGetKeychainVersion(SecKeychainRef keychainRef, UInt32* version)
{
    BEGIN_SECAPI

	   RequiredParam(version);

    *version = Keychain::optional(keychainRef)->database()->dbBlobVersion();

    END_SECAPI
}

OSStatus
SecKeychainAttemptMigrationWithMasterKey(SecKeychainRef keychain, UInt32 version, const char* masterKeyFilename)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainAttemptMigrationWithMasterKey", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

    RequiredParam(masterKeyFilename);
    Keychain kc = Keychain::optional(keychain);

    SecurityServer::SystemKeychainKey keychainKey(masterKeyFilename);
    if(keychainKey.valid()) {
        // We've managed to read the key; now, create credentials using it
        string path = kc->name();

        CssmClient::Key keychainMasterKey(kc->csp(), keychainKey.key(), true);
        CssmClient::AclFactory::MasterKeyUnlockCredentials creds(keychainMasterKey, Allocator::standard(Allocator::sensitive));

        // Attempt the migrate, using our master key as the ACL override
        bool result = kc->keychainMigration(path, kc->database()->dbBlobVersion(), path, version, creds.getAccessCredentials());
        if(!result) {
            return errSecBadReq;
        }
        return (kc->database()->dbBlobVersion() == version ? errSecSuccess : errSecBadReq);
    } else {
        return errSecBadReq;
    }

    END_SECAPI
}


// @@@ Deprecated
UInt16
SecKeychainListGetCount(void)
{
    BEGIN_SECAPI

	return globals().storageManager.size();

	END_SECAPI1(0)
}


// @@@ Deprecated
OSStatus
SecKeychainListCopyKeychainAtIndex(UInt16 index, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	KeychainCore::StorageManager &smgr=KeychainCore::globals().storageManager;
	RequiredParam(keychainRef)=smgr[index]->handle();

	END_SECAPI
}


// @@@ Deprecated
OSStatus
SecKeychainListRemoveKeychain(SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	Required(keychainRef);
	Keychain keychain = Keychain::optional(*keychainRef);
	StorageManager::KeychainList keychainList;
	keychainList.push_back(keychain);
	globals().storageManager.remove(keychainList);
	*keychainRef = NULL;

	END_SECAPI
}


OSStatus
SecKeychainAttributeInfoForItemID(SecKeychainRef keychainRef, UInt32 itemID, SecKeychainAttributeInfo **info)
{
	BEGIN_SECAPI

	Keychain keychain = Keychain::optional(keychainRef);
	keychain->getAttributeInfoForItemID(itemID, info);

	END_SECAPI
}


OSStatus
SecKeychainFreeAttributeInfo(SecKeychainAttributeInfo *info)
{
	BEGIN_SECAPI

	KeychainImpl::freeAttributeInfo(info);

	END_SECAPI
}


pascal OSStatus
SecKeychainAddCallback(SecKeychainCallback callbackFunction, SecKeychainEventMask eventMask, void* userContext)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainAddCallback", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(callbackFunction);
	CCallbackMgr::AddCallback(callbackFunction,eventMask,userContext);

	END_SECAPI
}	


OSStatus
SecKeychainRemoveCallback(SecKeychainCallback callbackFunction)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainRemoveCallback", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(callbackFunction);
	CCallbackMgr::RemoveCallback(callbackFunction);

	END_SECAPI
}	

OSStatus
SecKeychainAddInternetPassword(SecKeychainRef keychainRef, UInt32 serverNameLength, const char *serverName, UInt32 securityDomainLength, const char *securityDomain, UInt32 accountNameLength, const char *accountName, UInt32 pathLength, const char *path, UInt16 port, SecProtocolType protocol, SecAuthenticationType authenticationType, UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainAddInternetPassword", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	KCThrowParamErrIf_(passwordLength!=0 && passwordData==NULL);
	// @@@ Get real itemClass
	Item item(kSecInternetPasswordItemClass, 'aapl', passwordLength, passwordData, false);
	
	if (serverName && serverNameLength)
	{
		CssmData server(const_cast<void *>(reinterpret_cast<const void *>(serverName)), serverNameLength);
		item->setAttribute(Schema::attributeInfo(kSecServerItemAttr), server);
		// use server name as default label
		item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), server);
	}
		
	if (accountName && accountNameLength)
	{
		CssmData account(const_cast<void *>(reinterpret_cast<const void *>(accountName)), accountNameLength);
		item->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
	}

	if (securityDomain && securityDomainLength)
		item->setAttribute(Schema::attributeInfo(kSecSecurityDomainItemAttr),
			CssmData(const_cast<void *>(reinterpret_cast<const void *>(securityDomain)), securityDomainLength));
		
	item->setAttribute(Schema::attributeInfo(kSecPortItemAttr), UInt32(port));
	item->setAttribute(Schema::attributeInfo(kSecProtocolItemAttr), protocol);
	item->setAttribute(Schema::attributeInfo(kSecAuthenticationTypeItemAttr), authenticationType);
		
	if (path && pathLength)
		item->setAttribute(Schema::attributeInfo(kSecPathItemAttr),
			CssmData(const_cast<void *>(reinterpret_cast<const void *>(path)), pathLength));

	Keychain keychain = nil;
	try
    {
        keychain = Keychain::optional(keychainRef);
        if ( !keychain->exists() )
        {
            MacOSError::throwMe(errSecNoSuchKeychain);	// Might be deleted or not available at this time.
        }
    }
    catch(...)
    {
        keychain = globals().storageManager.defaultKeychainUI(item);
    }

    keychain->add(item);

    if (itemRef)
		*itemRef = item->handle();

    END_SECAPI
}


OSStatus
SecKeychainFindInternetPassword(CFTypeRef keychainOrArray, UInt32 serverNameLength, const char *serverName, UInt32 securityDomainLength, const char *securityDomain, UInt32 accountNameLength, const char *accountName, UInt32 pathLength, const char *path, UInt16 port, SecProtocolType protocol, SecAuthenticationType authenticationType, UInt32 *passwordLength, void **passwordData, SecKeychainItemRef *itemRef)
												
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainFindInternetPassword", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, kSecInternetPasswordItemClass, NULL);

	if (serverName && serverNameLength)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServerItemAttr),
			CssmData(const_cast<char *>(serverName), serverNameLength));
	}

	if (securityDomain && securityDomainLength)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecSecurityDomainItemAttr),
			CssmData (const_cast<char*>(securityDomain), securityDomainLength));
	}

	if (accountName && accountNameLength)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecAccountItemAttr),
			CssmData (const_cast<char*>(accountName), accountNameLength));
	}

	if (port)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecPortItemAttr),
			UInt32(port));
	}

	if (protocol)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecProtocolItemAttr),
			protocol);
	}

	if (authenticationType)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecAuthenticationTypeItemAttr),
			authenticationType);
	}

	if (path  && pathLength)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecPathItemAttr), path);
	}

	Item item;
	if (!cursor->next(item))
		return errSecItemNotFound;

	// Get its data (only if necessary)
	if (passwordData || passwordLength)
	{
		CssmDataContainer outData;
		item->getData(outData);
		if (passwordLength) {
			*passwordLength=(UInt32)outData.length();
		}
		outData.Length=0;
		if (passwordData) {
			*passwordData=outData.data();
		}
		outData.Data=NULL;
	}

	if (itemRef)
		*itemRef=item->handle();

    END_SECAPI
}


OSStatus
SecKeychainAddGenericPassword(SecKeychainRef keychainRef, UInt32 serviceNameLength, const char *serviceName, UInt32 accountNameLength, const char *accountName, UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainAddGenericPassword", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	KCThrowParamErrIf_(passwordLength!=0 && passwordData==NULL);
	// @@@ Get real itemClass

	Item item(kSecGenericPasswordItemClass, 'aapl', passwordLength, passwordData, false);

	if (serviceName && serviceNameLength)
	{
		CssmData service(const_cast<void *>(reinterpret_cast<const void *>(serviceName)), serviceNameLength);
		item->setAttribute(Schema::attributeInfo(kSecServiceItemAttr), service);
        item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), service);
	}

	if (accountName && accountNameLength)
	{
		CssmData account(const_cast<void *>(reinterpret_cast<const void *>(accountName)), accountNameLength);
		item->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
	}

	Keychain keychain = nil;
	try
    {
        keychain = Keychain::optional(keychainRef);
        if ( !keychain->exists() )
        {
            MacOSError::throwMe(errSecNoSuchKeychain);	// Might be deleted or not available at this time.
        }
    }
    catch(...)
    {
        keychain = globals().storageManager.defaultKeychainUI(item);
    }

	keychain->add(item);
	if (itemRef)
		*itemRef = item->handle();

    END_SECAPI
}


OSStatus
SecKeychainFindGenericPassword(CFTypeRef keychainOrArray, UInt32 serviceNameLength, const char *serviceName, UInt32 accountNameLength, const char *accountName, UInt32 *passwordLength, void **passwordData, SecKeychainItemRef *itemRef)
																			   
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainFindGenericPassword", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);

	if (serviceName && serviceNameLength)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr),
			CssmData(const_cast<char *>(serviceName), serviceNameLength));
	}

	if (accountName && accountNameLength)
	{
		cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecAccountItemAttr),
			CssmData(const_cast<char *>(accountName), accountNameLength));
	}

	Item item;
	if (!cursor->next(item))
		return errSecItemNotFound;

	// Get its data (only if necessary)
	if (passwordData || passwordLength)
	{
		CssmDataContainer outData;
		item->getData(outData);
		if (passwordLength) {
			*passwordLength=(UInt32)outData.length();
		}
		outData.Length=0;
		if (passwordData) {
			*passwordData=outData.data();
		}
		outData.Data=NULL;
	}

	if (itemRef)
		*itemRef=item->handle();

	END_SECAPI
}


OSStatus
SecKeychainSetUserInteractionAllowed(Boolean state) 
{
	BEGIN_SECAPI

	globals().setUserInteractionAllowed(state);

    END_SECAPI
}


OSStatus
SecKeychainGetUserInteractionAllowed(Boolean *state) 
{
	BEGIN_SECAPI

	Required(state)=globals().getUserInteractionAllowed();

    END_SECAPI
}


OSStatus
SecKeychainGetDLDBHandle(SecKeychainRef keychainRef, CSSM_DL_DB_HANDLE *dldbHandle)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainGetDLDBHandle", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(dldbHandle);
	
	Keychain keychain = Keychain::optional(keychainRef);
	*dldbHandle = keychain->database()->handle();

    END_SECAPI
}

static ModuleNexus<Mutex> gSecReturnedKeychainCSPsMutex;
static ModuleNexus<std::set<CssmClient::CSP>> gSecReturnedKeychainCSPs;

OSStatus
SecKeychainGetCSPHandle(SecKeychainRef keychainRef, CSSM_CSP_HANDLE *cspHandle)
{
    BEGIN_SECAPI

	RequiredParam(cspHandle);

	Keychain keychain = Keychain::optional(keychainRef);

    // Once we vend this handle, we can no longer delete this CSP object via RAII (and thus call CSSM_ModuleDetach on the CSP).
    // Keep a global pointer to it to force the CSP to stay live forever.
    CssmClient::CSP returnedKeychainCSP = keychain->csp();
    {
        StLock<Mutex> _(gSecReturnedKeychainCSPsMutex());
        gSecReturnedKeychainCSPs().insert(returnedKeychainCSP);
    }
	*cspHandle = returnedKeychainCSP->handle();

	END_SECAPI
}


OSStatus
SecKeychainCopyAccess(SecKeychainRef keychainRef, SecAccessRef *accessRef)
{
	BEGIN_SECAPI

	MacOSError::throwMe(errSecUnimplemented);//%%%for now

	END_SECAPI
}


OSStatus
SecKeychainSetAccess(SecKeychainRef keychainRef, SecAccessRef accessRef)
{
	BEGIN_SECAPI

	MacOSError::throwMe(errSecUnimplemented);//%%%for now

	END_SECAPI
}


#pragma mark ---- Private API ----


OSStatus
SecKeychainChangePassword(SecKeychainRef keychainRef, UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainChangePassword", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	Keychain keychain = Keychain::optional(keychainRef);
        keychain->changePassphrase (oldPasswordLength, oldPassword,  newPasswordLength, newPassword);

    END_SECAPI
}


OSStatus
SecKeychainCopyLogin(SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCopyLogin", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(keychainRef)=globals().storageManager.loginKeychain()->handle();

    END_SECAPI
}


OSStatus
SecKeychainLogin(UInt32 nameLength, const void* name, UInt32 passwordLength, const void* password)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainLogin", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	try
	{
		if (password) {
            globals().storageManager.login(nameLength, name,  passwordLength, password, false);
        } else {
            globals().storageManager.stashLogin();
        }
	}
	catch (CommonError &e)
	{
        secnotice("KCLogin", "SecKeychainLogin failed: %d, password was%s supplied", (int)e.osStatus(), password?"":" not");
		if (e.osStatus() == CSSMERR_DL_OPERATION_AUTH_DENIED)
		{
			return errSecAuthFailed;
		}
		else
		{
			return e.osStatus();
		}
	}

    catch (...) {
        __secapiresult=errSecInternalComponent;
    }
    secnotice("KCLogin", "SecKeychainLogin result: %d, password was%s supplied", (int)__secapiresult, password?"":" not");

    END_SECAPI
}

OSStatus SecKeychainStash()
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainStash", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
    
	try
	{
		globals().storageManager.stashKeychain();
	}
	catch (CommonError &e)
	{
		if (e.osStatus() == CSSMERR_DL_OPERATION_AUTH_DENIED)
		{
			return errSecAuthFailed;
		}
		else
		{
			return e.osStatus();
		}
	}
	
    END_SECAPI
}

OSStatus
SecKeychainLogout()
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainLogout", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	globals().storageManager.logout();

    END_SECAPI
}

/* (non-exported C utility routine) 'Makes' a keychain based on a full path
*/
static Keychain make(const char *name)
{
	return globals().storageManager.make(name);
}

/*  'Makes' a keychain based on a full path for legacy "KC" CoreServices APIs.
    Note this version doesn't take an accessRef or password.
    The "KC" create API takes a keychainRef...
*/
OSStatus SecKeychainMakeFromFullPath(const char *fullPathName, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	RequiredParam(fullPathName);
        RequiredParam(keychainRef)=make(fullPathName)->handle();
	END_SECAPI
}


/* Determines if the keychainRef is a valid keychain.
*/
OSStatus SecKeychainIsValid(SecKeychainRef keychainRef, Boolean* isValid)
{
    BEGIN_SECAPI

	*isValid = false;
        if (KeychainImpl::optional(keychainRef)->dlDbIdentifier().ssuid().guid() == gGuidAppleCSPDL)
            *isValid = true;
	END_SECAPI
}

/* Removes a keychain from the keychain search list for legacy "KC" CoreServices APIs.
*/
OSStatus SecKeychainRemoveFromSearchList(SecKeychainRef keychainRef)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainRemoveFromSearchList", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
        StorageManager::KeychainList singleton;
        singleton.push_back(KeychainImpl::required(keychainRef));
        globals().storageManager.remove(singleton);
	END_SECAPI
}

/* Create a keychain based on a keychain Ref for legacy "KC" CoreServices APIs.
*/
OSStatus SecKeychainCreateNew(SecKeychainRef keychainRef, UInt32 passwordLength, const char* inPassword)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCreateNew", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
        RequiredParam(inPassword);
        KeychainImpl::required(keychainRef)->create(passwordLength, inPassword);
	END_SECAPI
}

/* Modify a keychain so that it can be synchronized.
*/
OSStatus SecKeychainRecodeKeychain(SecKeychainRef keychainRef, CFArrayRef dbBlobArray, CFDataRef extraData)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainRecodeKeychain", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	// do error checking for required parameters
	RequiredParam(dbBlobArray);
	RequiredParam(extraData);

	const CssmData extraCssmData(const_cast<UInt8 *>(CFDataGetBytePtr(extraData)),
		CFDataGetLength(extraData));

	CFIndex dbBlobArrayCount = CFArrayGetCount(dbBlobArray);
	size_t space = sizeof(uint8) + (dbBlobArrayCount * sizeof(SecurityServer::DbHandle));
	void *dataPtr = (void*)malloc(space);
	if ( !dataPtr )
		return errSecAllocate;
	//
	// Get a DbHandle(IPCDbHandle) from securityd for each blob in the array that we'll authenticate with.
	//
	uint8* sizePtr = (uint8*)dataPtr;
	*sizePtr = dbBlobArrayCount;
	SecurityServer::DbHandle *currDbHandle = (SecurityServer::DbHandle *)(sizePtr+1);
	CFIndex index;
	SecurityServer::ClientSession ss(Allocator::standard(), Allocator::standard());
	for (index=0; index < dbBlobArrayCount; index++)
	{
		CFDataRef cfBlobData = (CFDataRef)CFArrayGetValueAtIndex(dbBlobArray, index);
		const CssmData thisKCData(const_cast<UInt8 *>(CFDataGetBytePtr(cfBlobData)), CFDataGetLength(cfBlobData));
		//
		// Since it's to a DbHandle that's not on our disk (it came from user's iDisk),
		// it's OK to use the mIdentifier and access credentials of the keychain we're recoding.
		//
		Keychain kc = KeychainImpl::required(keychainRef);
		*currDbHandle = ss.decodeDb(kc->dlDbIdentifier(), kc->defaultCredentials(), thisKCData); /* returns a DbHandle (IPCDbHandle) */
		
		currDbHandle++;
	}
	// do the work
	Keychain keychain = Keychain::optional(keychainRef);
	const CssmData data(const_cast<UInt8 *>((uint8*)dataPtr), space);
	Boolean recodeFailed = false;
	
	int errCode=errSecSuccess;
	
	try
    {
		keychain->recode(data, extraCssmData);
	}
	catch (MacOSError e)
	{
		errCode = e.osStatus();
		recodeFailed = true;
	}
	catch (UnixError ue)
	{
		errCode = ue.unixError();
	}
	
	currDbHandle = (SecurityServer::DbHandle *)(sizePtr+1);
	for (index=0; index < dbBlobArrayCount; index++)
	{
		ss.releaseDb(*currDbHandle);
		currDbHandle++;
	}
	if ( dataPtr )
		free(dataPtr);
	
	if ( recodeFailed )
	{
		return errCode;
	}
	
	END_SECAPI
}

OSStatus SecKeychainCopySignature(SecKeychainRef keychainRef, CFDataRef *keychainSignature) 
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCopySignature", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	// do error checking for required parameters
	RequiredParam(keychainSignature);

	// make a keychain object "wrapper" for this keychain ref
	Keychain keychain = Keychain::optional(keychainRef);
	CssmAutoData data(keychain->database()->allocator());
	keychain->copyBlob(data.get());

	// get the cssmDBBlob
	const SecurityServer::DbBlob *cssmDBBlob =
		data.get().interpretedAs<const SecurityServer::DbBlob>();

	// convert from CDSA standards to CF standards
	*keychainSignature = CFDataCreate(kCFAllocatorDefault,
		cssmDBBlob->randomSignature.bytes,
		sizeof(SecurityServer::DbBlob::Signature));

	END_SECAPI
}

OSStatus SecKeychainCopyBlob(SecKeychainRef keychainRef, CFDataRef *dbBlob)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCopyBlob", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	// do error checking for required parameters
	RequiredParam(dbBlob);

	// make a keychain object "wrapper" for this keychain ref
	Keychain keychain = Keychain::optional(keychainRef);
	CssmAutoData data(keychain->database()->allocator());
	keychain->copyBlob(data.get());

	// convert from CDSA standards to CF standards
	*dbBlob = CFDataCreate(kCFAllocatorDefault, data, data.length());

	END_SECAPI
}

// make a new keychain with pre-existing secrets
OSStatus SecKeychainCreateWithBlob(const char* fullPathName, CFDataRef dbBlob, SecKeychainRef *kcRef)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainCreateWithBlob", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
	
	KCThrowParamErrIf_(!fullPathName);
	KCThrowParamErrIf_(!dbBlob);
	
	Keychain keychain = globals().storageManager.make(fullPathName);

	CssmData blob(const_cast<unsigned char *>(CFDataGetBytePtr(dbBlob)), CFDataGetLength(dbBlob));

	// @@@ the call to StorageManager::make above leaves keychain the the cache.
	// If the create below fails we should probably remove it.
	keychain->createWithBlob(blob);

	RequiredParam(kcRef)=keychain->handle();

	//

	END_SECAPI
}

// add a non-file based DB to the keychain list
OSStatus SecKeychainAddDBToKeychainList (SecPreferencesDomain domain, const char* dbName,
										 const CSSM_GUID *guid, uint32 subServiceType)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainAddDBToKeychainList", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	RequiredParam(dbName);
	StorageManager &smr = globals().storageManager;
	smr.addToDomainList(domain, dbName, *guid, subServiceType);

	END_SECAPI
}

// determine if a non-file based DB is in the keychain list
OSStatus SecKeychainDBIsInKeychainList (SecPreferencesDomain domain, const char* dbName,
										const CSSM_GUID *guid, uint32 subServiceType)
{
	BEGIN_SECAPI

	   RequiredParam(dbName);
	StorageManager &smr = globals().storageManager;
	smr.isInDomainList(domain, dbName, *guid, subServiceType);
	END_SECAPI
}

// remove a non-file based DB from the keychain list
OSStatus SecKeychainRemoveDBFromKeychainList (SecPreferencesDomain domain, const char* dbName,
											  const CSSM_GUID *guid, uint32 subServiceType)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainRemoveDBFromKeychainList", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
	RequiredParam(dbName);
	StorageManager &smr = globals().storageManager;
	smr.removeFromDomainList(domain, dbName, *guid, subServiceType);
	END_SECAPI
}


// set server mode -- must be called before any other Sec* etc. call
void SecKeychainSetServerMode()
{
	gServerMode = true;
}



OSStatus SecKeychainSetBatchMode (SecKeychainRef kcRef, Boolean mode, Boolean rollback)
{
	BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainSetBatchMode", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
	RequiredParam(kcRef);
	Keychain keychain = Keychain::optional(kcRef);
	keychain->setBatchMode(mode, rollback);
	END_SECAPI
}



OSStatus SecKeychainCleanupHandles()
{
	BEGIN_SECAPI

	   END_SECAPI // which causes the handle cache cleanup routine to run
}

OSStatus SecKeychainVerifyKeyStorePassphrase(uint32_t retries)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainVerifyKeyStorePassphrase", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
    SecurityServer::ClientSession().verifyKeyStorePassphrase(retries);
    END_SECAPI
}

OSStatus SecKeychainChangeKeyStorePassphrase()
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainChangeKeyStorePassphrase", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);
    SecurityServer::ClientSession().changeKeyStorePassphrase();
    END_SECAPI
}

static OSStatus SecKeychainGetMasterKey(SecKeychainRef userKeychainRef, CFDataRef *masterKey, CFStringRef password)
{
    BEGIN_SECAPI

	   os_activity_t activity = os_activity_create("SecKeychainGetMasterKey", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

    // make a keychain object "wrapper" for this keychain ref
	Keychain keychain = Keychain::optional(userKeychainRef);

    CssmClient::Db db = keychain->database();
	
	// create the keychain, using appropriate credentials
	Allocator &alloc = db->allocator();
	AutoCredentials cred(alloc);	// will leak, but we're quitting soon :-)

    char passphrase[1024];
    CFStringGetCString(password, passphrase, sizeof(passphrase), kCFStringEncodingUTF8);
    
    // use this passphrase
    cred += TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK,
                      new(alloc) ListElement(CSSM_SAMPLE_TYPE_PASSWORD),
                      new(alloc) ListElement(StringData(passphrase)));
	db->authenticate(CSSM_DB_ACCESS_READ, &cred);

	CSSM_DL_DB_HANDLE dlDb = db->handle();
	CssmData dlDbData = CssmData::wrap(dlDb);
	CssmKey refKey;
	KeySpec spec(CSSM_KEYUSE_ANY,
                 CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE);

    DeriveKey derive(keychain->csp(), CSSM_ALGID_KEYCHAIN_KEY, CSSM_ALGID_3DES_3KEY, 3 * 64);
	derive(&dlDbData, spec, refKey);
	
	// now extract the raw keybits
	CssmKey rawKey;
	WrapKey wrap(keychain->csp(), CSSM_ALGID_NONE);
	wrap(refKey, rawKey);
    
    *masterKey = CFDataCreate(kCFAllocatorDefault, rawKey.keyData(), rawKey.length());
    
    END_SECAPI
}

static const char     *kAutologinPWFilePath = "/etc/kcpassword";
static const uint32_t kObfuscatedPasswordSizeMultiple = 12;
static const uint32_t buffer_size = 512;
static const uint8_t  kObfuscationKey[] = {0x7d, 0x89, 0x52, 0x23, 0xd2, 0xbc, 0xdd, 0xea, 0xa3, 0xb9, 0x1f};

static void obfuscate(void *buffer, size_t bufferLength)
{
	uint8_t       *pBuf = (uint8_t *) buffer;
	const uint8_t *pKey = kObfuscationKey, *eKey = pKey + sizeof( kObfuscationKey );

	while (bufferLength--) {
		*pBuf = *pBuf ^ *pKey;
		++pKey;
		++pBuf;
		if (pKey == eKey)
			pKey = kObfuscationKey;
	}
}

static bool _SASetAutologinPW(CFStringRef inAutologinPW)
{
	bool    result = false;
	struct stat sb;

	// Delete the kcpassword file if it exists already
	if (stat(kAutologinPWFilePath, &sb) == 0)
		unlink( kAutologinPWFilePath );

    // NIL incoming password ==> clear auto login password (above) without setting a new one. In other words: turn auto login off.
    if (inAutologinPW != NULL) {
		char buffer[buffer_size];
		const char *pwAsUTF8String = CFStringGetCStringPtr(inAutologinPW, kCFStringEncodingUTF8);
		if (pwAsUTF8String == NULL) {
			if (CFStringGetCString(inAutologinPW, buffer, buffer_size, kCFStringEncodingUTF8)) pwAsUTF8String = buffer;
		}

		if (pwAsUTF8String != NULL) {
			size_t pwLength = strlen(pwAsUTF8String) + 1;
			size_t obfuscatedPWLength;
			char *obfuscatedPWBuffer;

			// The size of the obfuscated password should be the smallest multiple of
			// kObfuscatedPasswordSizeMultiple greater than or equal to pwLength.
			obfuscatedPWLength = (((pwLength - 1) / kObfuscatedPasswordSizeMultiple) + 1) * kObfuscatedPasswordSizeMultiple;
			obfuscatedPWBuffer = (char *) malloc(obfuscatedPWLength);

			// Copy the password (including null terminator) to beginning of obfuscatedPWBuffer
			bcopy(pwAsUTF8String, obfuscatedPWBuffer, pwLength);

			// Pad remainder of obfuscatedPWBuffer with random bytes
			{
				char *p;
				char *endOfBuffer = obfuscatedPWBuffer + obfuscatedPWLength;

				for (p = obfuscatedPWBuffer + pwLength; p < endOfBuffer; ++p)
					*p = random() & 0x000000FF;
			}

			obfuscate(obfuscatedPWBuffer, obfuscatedPWLength);

			int pwFile = open(kAutologinPWFilePath, O_CREAT | O_WRONLY | O_NOFOLLOW, S_IRUSR | S_IWUSR);
			if (pwFile >= 0) {
				size_t wrote = write(pwFile, obfuscatedPWBuffer, obfuscatedPWLength);
				if (wrote == obfuscatedPWLength)
					result = true;
				close(pwFile);
			}

			chmod(kAutologinPWFilePath, S_IRUSR | S_IWUSR);
			free(obfuscatedPWBuffer);
		}
	}

    return result;
}

OSStatus SecKeychainStoreUnlockKey(SecKeychainRef userKeychainRef, SecKeychainRef systemKeychainRef, CFStringRef username, CFStringRef password) {
	COUNTLEGACYAPI
    SecTrustedApplicationRef itemPath;
    SecAccessRef ourAccessRef = NULL;
    
    OSStatus result = errSecParam;
    
	if (userKeychainRef == NULL) {
		// We don't have a specific user keychain, fall back
		if (_SASetAutologinPW(password))
			result = errSecSuccess;

		return result;
	}

    CFDataRef masterKey = NULL;
    result = SecKeychainGetMasterKey(userKeychainRef, &masterKey, password);
    if (errSecSuccess != result) {
        return result;
    }
    
    result = SecKeychainStash();
    if (errSecSuccess != result) {
        if (masterKey != NULL) CFRelease(masterKey);
        return result;
    }
    
    CFMutableArrayRef trustedApplications = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (noErr == SecTrustedApplicationCreateApplicationGroup("com.apple.security.auto-login", NULL, &itemPath) && itemPath)
        CFArrayAppendValue(trustedApplications, itemPath);
    
    if (trustedApplications && (CFArrayGetCount(trustedApplications) > 0)) {
        if (errSecSuccess == (result = SecAccessCreate(CFSTR("Auto-Login applications"), trustedApplications, &ourAccessRef))) {
			SecKeychainRef internalSystemKeychainRef = NULL;
            if (NULL == systemKeychainRef) {
				SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem, &internalSystemKeychainRef);
            } else {
                internalSystemKeychainRef = systemKeychainRef;
            }
            
            const void *queryKeys[] =   { kSecClass,
                kSecAttrService,
                kSecAttrAccount,
                kSecUseKeychain,
            };
            const void *queryValues[] = { kSecClassGenericPassword,
                CFSTR("com.apple.loginwindow.auto-login"),
                username,
				internalSystemKeychainRef,
            };
            
            const void *updateKeys[] =   { kSecAttrAccess,
                kSecValueData,
            };
            const void *updateValues[] = { ourAccessRef,
                masterKey,
            };
            
            CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, queryKeys, queryValues, sizeof(queryValues)/sizeof(*queryValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDictionaryRef update = CFDictionaryCreate(kCFAllocatorDefault, updateKeys, updateValues, sizeof(updateValues)/sizeof(*updateValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            
            result = SecItemUpdate(query, update);
            
            if (errSecSuccess != result) {
                const void *addKeys[] =   { kSecClass,
                    kSecAttrService,
                    kSecAttrAccount,
                    kSecUseKeychain,
                    kSecAttrAccess,
                    kSecValueData,
                };
                const void *addValues[] = { kSecClassGenericPassword,
                    CFSTR("com.apple.loginwindow.auto-login"),
                    username,
					internalSystemKeychainRef,
                    ourAccessRef,
                    masterKey,
                };
                
                CFDictionaryRef add = CFDictionaryCreate(kCFAllocatorDefault, addKeys, addValues, sizeof(addValues)/sizeof(*addValues), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                result = SecItemAdd(add, NULL);
                if (NULL != add) CFRelease(add);
            }

            if (NULL != query) CFRelease(query);
            if (NULL != update) CFRelease(update);

			// If the caller wanted us to locate the system keychain reference, it's okay to go ahead and free our magically created one
			if (systemKeychainRef == NULL) CFRelease(internalSystemKeychainRef);
        }
    }

    if (NULL != masterKey) CFRelease(masterKey);
    if (NULL != trustedApplications) CFRelease(trustedApplications);
    if (NULL != ourAccessRef) CFRelease(ourAccessRef);

    return result;
}

OSStatus SecKeychainGetUserPromptAttempts(uint32_t * attempts)
{
    BEGIN_SECAPI

    os_activity_t activity = os_activity_create("SecKeychainGetUserPromptAttempts", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

    if(attempts) {
        SecurityServer::ClientSession().getUserPromptAttempts(*attempts);
    }

    END_SECAPI
}

OSStatus SecKeychainStoreUnlockKeyWithPubKeyHash(CFDataRef pubKeyHash, CFStringRef tokenID, CFDataRef wrapPubKeyHash,
                                                 SecKeychainRef userKeychain, CFStringRef password)
{
	COUNTLEGACYAPI
	CFRef<CFStringRef> pwd;
	OSStatus result;

	if (password == NULL || CFStringGetLength(password) == 0) {
		AuthorizationRef authorizationRef;
		result = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
		if (result != errAuthorizationSuccess) {
			secnotice("SecKeychain", "failed to create authorization");
			return result;
		}

		AuthorizationItem myItems = {"com.apple.ctk.pair", 0, NULL, 0};
		AuthorizationRights myRights = {1, &myItems};

		char pathName[PATH_MAX];
		UInt32 pathLength = PATH_MAX;
		result = SecKeychainGetPath(userKeychain, &pathLength, pathName);
		if (result != errSecSuccess) {
			secnotice("SecKeychain", "failed to create authorization");
			return result;
		}

		Boolean checkPwd = TRUE;
		Boolean ignoreSession = TRUE;
		AuthorizationItem envItems[] = {
			{AGENT_HINT_KEYCHAIN_PATH, pathLength, pathName, 0},
			{AGENT_HINT_KEYCHAIN_CHECK, sizeof(checkPwd), &checkPwd},
			{AGENT_HINT_IGNORE_SESSION, sizeof(ignoreSession), &ignoreSession}
		};

		AuthorizationEnvironment environment  = {3, envItems};
		AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights;
		result = AuthorizationCopyRights(authorizationRef, &myRights, &environment, flags, NULL);
        secnotice("SecKeychain", "Authorization result: %d", (int)result);

		if (result == errAuthorizationSuccess) {
			AuthorizationItemSet *items;
			result = AuthorizationCopyInfo(authorizationRef, kAuthorizationEnvironmentPassword, &items);
            secnotice("SecKeychain", "Items copy result: %d", (int)result);
			if (result == errAuthorizationSuccess) {
                secnotice("SecKeychain", "Items count: %d", items->count);
				if (items->count > 0) {
					pwd = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)items->items[0].value, kCFStringEncodingUTF8);
                    if (pwd) {
                        secnotice("SecKeychain", "Got kcpass");
                    }
				}
				AuthorizationFreeItemSet(items);
			}
		}
		AuthorizationFree(authorizationRef, kAuthorizationFlagDefaults);
		if (result != errAuthorizationSuccess) {
			secnotice("SecKeychain", "did not get authorization to pair the card");
			return result;
		}
	} else {
		pwd.take(password);
	}

	if (!pwd) {
		secnotice("SecKeychain", "did not get kcpass");
		return errSecInternalComponent;
	}

	CFRef<CFDataRef> masterKey;
	result = SecKeychainGetMasterKey(userKeychain, masterKey.take(), pwd);
	if (result != errSecSuccess) {
		secnotice("SecKeychain", "Failed to get master key: %d", (int) result);
		return result;
	}

	CFRef<CFDataRef> scBlob;
	result = TokenLoginGetScBlob(wrapPubKeyHash, tokenID, pwd, scBlob.take());
	if (result != errSecSuccess) {
		secnotice("SecKeychain", "Failed to get stash: %d", (int) result);
		return result;
	}

	result = TokenLoginCreateLoginData(tokenID, pubKeyHash, wrapPubKeyHash, masterKey, scBlob);
	if (result != errSecSuccess) {
		secnotice("SecKeychain", "Failed to create login data: %d", (int) result);
		return result;
	}

	secnotice("SecKeychain", "SecKeychainStoreUnlockKeyWithPubKeyHash result %d", (int) result);
    
    // create SC KEK
    // this might fail if KC password is different from user's password
    uid_t uid = geteuid();
    if (!uid) {
        uid = getuid();
    }
    struct passwd *passwd = getpwuid(uid);
    if (passwd) {
        CFRef<CFStringRef> username = CFStringCreateWithCString(kCFAllocatorDefault, passwd->pw_name, kCFStringEncodingUTF8);
        OSStatus kekRes = TKAddSecureToken(username, pwd, tokenID, wrapPubKeyHash);
        if (kekRes != noErr) {
            secnotice("SecKeychain", "Failed to register SC token: %d", (int) kekRes); // do not fail because KC functionality be still OK
        }
    } else {
        secnotice("SecKeychain", "Unable to get name for uid %d", uid);
    }
	return result;
}

OSStatus SecKeychainEraseUnlockKeyWithPubKeyHash(CFDataRef pubKeyHash)
{
	COUNTLEGACYAPI
    OSStatus result = TokenLoginDeleteUnlockData(pubKeyHash);
    if (result != errSecSuccess) {
        secnotice("SecKeychain", "Failed to erase stored wrapped unlock key: %d", (int) result);
    }
    return result;
}
