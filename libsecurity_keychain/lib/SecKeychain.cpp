/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
#include <security_keychain/KCExceptions.h>
#include <securityd_client/ssblob.h>
#include "SecBridge.h"
#include "CCallbackMgr.h"
#include <security_cdsa_utilities/Schema.h>
#include <security_utilities/ktracecodes.h>
#include <pwd.h>

CFTypeID
SecKeychainGetTypeID(void)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainGetTypeID()");
	return gTypes().KeychainImpl.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecKeychainGetVersion(UInt32 *returnVers)
{
	secdebug("kc", "SecKeychainGetVersion(%p)", returnVers);
    if (!returnVers)
		return noErr;

	*returnVers = 0x02028000;
	return noErr;
}


OSStatus
SecKeychainOpen(const char *pathName, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainOpen(\"%s\", %p)", pathName, keychainRef);
	RequiredParam(keychainRef)=globals().storageManager.make(pathName, false)->handle();

	END_SECAPI
}


OSStatus
SecKeychainOpenWithGuid(const CSSM_GUID *guid, uint32 subserviceId, uint32 subserviceType, const char* dbName,
						const CSSM_NET_ADDRESS *dbLocation, SecKeychainRef *keychain)
{
    BEGIN_SECAPI

	// range check parameters
	RequiredParam (guid);
	RequiredParam (dbName);
	
	// create a DLDbIdentifier that describes what should be opened
    const CSSM_VERSION *version = NULL;
    const CssmSubserviceUid ssuid(*guid, version, subserviceId, subserviceType);
	DLDbIdentifier dLDbIdentifier(ssuid, dbName, dbLocation);
	
	// make a keychain from the supplied info
	RequiredParam(keychain) = globals().storageManager.makeKeychain(dLDbIdentifier, false)->handle ();

	END_SECAPI
}


OSStatus
SecKeychainCreate(const char *pathName, UInt32 passwordLength, const void *password,
	Boolean promptUser, SecAccessRef initialAccess, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainCreate(\"%s\", %lu, %p, %d, %p, %p)", pathName, passwordLength, password, promptUser, initialAccess, keychainRef);
	KCThrowParamErrIf_(!pathName);
	Keychain keychain = globals().storageManager.make(pathName);

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

	secdebug("kc", "SecKeychainDelete(%p)", keychainOrArray);
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

	secdebug("kc", "SecKeychainSetSettings(%p, %p)", keychainRef, newSettings);
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

	secdebug("kc", "SecKeychainCopySettings(%p, %p)", keychainRef, outSettings);
	Keychain keychain = Keychain::optional(keychainRef);
	if (outSettings->version==SEC_KEYCHAIN_SETTINGS_VERS1)
	{
		UInt32 lockInterval;
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

	secdebug("kc", "SecKeychainUnlock(%p, %lu, %p, %d)", keychainRef, passwordLength, password, usePassword);
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

	secdebug("kc", "SecKeychainLock(%p)", keychainRef);
	Keychain keychain = Keychain::optional(keychainRef);
	keychain->lock();

	END_SECAPI
}


OSStatus
SecKeychainLockAll(void)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainLockAll()");
	globals().storageManager.lockAll();

	END_SECAPI
}


OSStatus SecKeychainResetLogin(UInt32 passwordLength, const void* password, Boolean resetSearchList)
{
	BEGIN_SECAPI
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
            MacOSError::throwMe(errAuthorizationInternal);
		
		if (password)
		{
			// Clear the plist and move aside (rename) the existing login.keychain
			globals().storageManager.resetKeychain(resetSearchList);

			// Create the login keychain without UI
			globals().storageManager.login(userName.length(), userName.c_str(), passwordLength, password);
			
			// Set it as the default
			Keychain keychain = globals().storageManager.loginKeychain();
			globals().storageManager.defaultKeychain(keychain);
		}
		else
		{
			// Create the login keychain, prompting for password
			// (implicitly calls resetKeychain, login, and defaultKeychain)
			globals().storageManager.makeLoginAuthUI(NULL);
		}

		// Post a "list changed" event after a reset, so apps can refresh their list.
		// Make sure we are not holding mLock when we post this event.
		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);

	END_SECAPI
}

OSStatus
SecKeychainCopyDefault(SecKeychainRef *keychainRef)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainCopyDefault(%p)", keychainRef);
	RequiredParam(keychainRef)=globals().storageManager.defaultKeychain()->handle();

	END_SECAPI
}


OSStatus
SecKeychainSetDefault(SecKeychainRef keychainRef)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainSetDefault(%p)", keychainRef);
	globals().storageManager.defaultKeychain(Keychain::optional(keychainRef));

	END_SECAPI
}

OSStatus SecKeychainCopySearchList(CFArrayRef *searchList)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainCopySearchList(%p)", searchList);
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

	secdebug("kc", "SecKeychainSetSearchList(%p)", searchList);
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

	secdebug("kc", "SecKeychainCopyDefault(%p)", keychainRef);
	RequiredParam(keychainRef)=globals().storageManager.defaultKeychain(domain)->handle();

	END_SECAPI
}

OSStatus SecKeychainSetDomainDefault(SecPreferencesDomain domain, SecKeychainRef keychainRef)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainSetDefault(%p)", keychainRef);
	globals().storageManager.defaultKeychain(domain, Keychain::optional(keychainRef));

	END_SECAPI
}

OSStatus SecKeychainCopyDomainSearchList(SecPreferencesDomain domain, CFArrayRef *searchList)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainCopyDomainSearchList(%p)", searchList);
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

	secdebug("kc", "SecKeychainSetDomainSearchList(%p)", searchList);
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

	globals().storageManager.domain(domain);

	END_SECAPI
}

OSStatus SecKeychainGetPreferenceDomain(SecPreferencesDomain *domain)
{
	BEGIN_SECAPI
	
	*domain = globals().storageManager.domain();
	
	END_SECAPI
}


OSStatus
SecKeychainGetStatus(SecKeychainRef keychainRef, SecKeychainStatus *keychainStatus)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainGetStatus(%p): %p", keychainRef, keychainStatus);
	RequiredParam(keychainStatus) = (SecKeychainStatus)Keychain::optional(keychainRef)->status();

	END_SECAPI
}


OSStatus
SecKeychainGetPath(SecKeychainRef keychainRef, UInt32 *ioPathLength, char *pathName)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainGetPath(%p, %p, %p)", keychainRef, ioPathLength, pathName);
	RequiredParam(pathName);
	RequiredParam(ioPathLength);

    const char *name = Keychain::optional(keychainRef)->name();
	UInt32 nameLen = strlen(name);
	if (nameLen+1 > *ioPathLength)  // if the client's buffer is too small (including null-termination), throw
		CssmError::throwMe(CSSMERR_CSSM_BUFFER_TOO_SMALL);
	strncpy(pathName, name, nameLen);
    pathName[nameLen] = 0;
	*ioPathLength = nameLen;   // set the length.
    		
	END_SECAPI
}


// @@@ Depricated
UInt16
SecKeychainListGetCount(void)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainListGetCount()");
	return globals().storageManager.size();

	END_SECAPI1(0)
}


// @@@ Depricated
OSStatus
SecKeychainListCopyKeychainAtIndex(UInt16 index, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainListCopyKeychainAtIndex(%d, %p)", index, keychainRef);
	KeychainCore::StorageManager &smgr=KeychainCore::globals().storageManager;
	RequiredParam(keychainRef)=smgr[index]->handle();

	END_SECAPI
}


// @@@ Depricated
OSStatus
SecKeychainListRemoveKeychain(SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainListRemoveKeychain(%p)", keychainRef);
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

	secdebug("kc", "SecKeychainAttributeInfoForItemID(%p, %lu, %p)", keychainRef, itemID, info);
	Keychain keychain = Keychain::optional(keychainRef);
	keychain->getAttributeInfoForItemID(itemID, info);

	END_SECAPI
}


OSStatus
SecKeychainFreeAttributeInfo(SecKeychainAttributeInfo *info)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainFreeAttributeInfo(%p)", info);
	KeychainImpl::freeAttributeInfo(info);

	END_SECAPI
}


pascal OSStatus
SecKeychainAddCallback(SecKeychainCallback callbackFunction, SecKeychainEventMask eventMask, void* userContext)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainAddCallback(%p, %08lx, %p)", callbackFunction, eventMask, userContext);
	RequiredParam(callbackFunction);
	CCallbackMgr::AddCallback(callbackFunction,eventMask,userContext);

	END_SECAPI
}	


OSStatus
SecKeychainRemoveCallback(SecKeychainCallback callbackFunction)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainRemoveCallback(%p)", callbackFunction);
	RequiredParam(callbackFunction);
	CCallbackMgr::RemoveCallback(callbackFunction);

	END_SECAPI
}	

OSStatus
SecKeychainAddInternetPassword(SecKeychainRef keychainRef, UInt32 serverNameLength, const char *serverName, UInt32 securityDomainLength, const char *securityDomain, UInt32 accountNameLength, const char *accountName, UInt32 pathLength, const char *path, UInt16 port, SecProtocolType protocol, SecAuthenticationType authenticationType, UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainAddInternetPassword(%p)", keychainRef);
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

	secdebug("kc", "SecKeychainFindInternetPassword(%p)", keychainOrArray);
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
		*passwordLength=outData.length();
		outData.Length=0;
		*passwordData=outData.data();
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

	secdebug("kc", "SecKeychainAddGenericPassword(%p)", keychainRef);
	KCThrowParamErrIf_(passwordLength!=0 && passwordData==NULL);
	// @@@ Get real itemClass
	Item item(kSecGenericPasswordItemClass, 'aapl', passwordLength, passwordData, false);

	if (serviceName && serviceNameLength)
	{
		CssmData service(const_cast<void *>(reinterpret_cast<const void *>(serviceName)), serviceNameLength);
		item->setAttribute(Schema::attributeInfo(kSecServiceItemAttr), service);
		// use service name as default label (UNLESS the service is iTools and we have an account name [3787371])
		const char *iTools = "iTools";
		if (accountNameLength && serviceNameLength==strlen(iTools) && !memcmp(serviceName, iTools, serviceNameLength))
		{
			CssmData account(const_cast<void *>(reinterpret_cast<const void *>(accountName)), accountNameLength);
			item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), account);
		}
		else
		{
			item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), service);
		}
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
    Debug::trace (kSecTraceSecurityFrameworkSecKeychainFindGenericPasswordBegin);

    BEGIN_SECAPI

	secdebug("kc", "SecKeychainFindGenericPassword(%p)", keychainOrArray);
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
		*passwordLength=outData.length();
		outData.Length=0;
		*passwordData=outData.data();
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

	secdebug("kc", "SecKeychainSetUserInteractionAllowed(%d)", state);
	globals().setUserInteractionAllowed(state);

    END_SECAPI
}


OSStatus
SecKeychainGetUserInteractionAllowed(Boolean *state) 
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainGetUserInteractionAllowed()");
	Required(state)=globals().getUserInteractionAllowed();

    END_SECAPI
}


OSStatus
SecKeychainGetDLDBHandle(SecKeychainRef keychainRef, CSSM_DL_DB_HANDLE *dldbHandle)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainGetDLDBHandle(%p, %p)", keychainRef, dldbHandle);
	RequiredParam(dldbHandle);
	
	Keychain keychain = Keychain::optional(keychainRef);
	*dldbHandle = keychain->database()->handle();

    END_SECAPI
}


OSStatus
SecKeychainGetCSPHandle(SecKeychainRef keychainRef, CSSM_CSP_HANDLE *cspHandle)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainGetCSPHandle(%p, %p)", keychainRef, cspHandle);
	RequiredParam(cspHandle);

	Keychain keychain = Keychain::optional(keychainRef);
	*cspHandle = keychain->csp()->handle();

	END_SECAPI
}


OSStatus
SecKeychainCopyAccess(SecKeychainRef keychainRef, SecAccessRef *accessRef)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainCopyAccess(%p, %p)", keychainRef, accessRef);
	MacOSError::throwMe(unimpErr);//%%%for now

	END_SECAPI
}


OSStatus
SecKeychainSetAccess(SecKeychainRef keychainRef, SecAccessRef accessRef)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainSetAccess(%p, %p)", keychainRef, accessRef);
	MacOSError::throwMe(unimpErr);//%%%for now

	END_SECAPI
}


#pragma mark ---- Private API ----


OSStatus
SecKeychainChangePassword(SecKeychainRef keychainRef, UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainChangePassword(%p, %lu, %p, %lu, %p)", keychainRef,
		oldPasswordLength, oldPassword, newPasswordLength, newPassword);
	Keychain keychain = Keychain::optional(keychainRef);
        keychain->changePassphrase (oldPasswordLength, oldPassword,  newPasswordLength, newPassword);

    END_SECAPI
}


OSStatus
SecKeychainCopyLogin(SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainCopyLogin(%p)", keychainRef);
	RequiredParam(keychainRef)=globals().storageManager.loginKeychain()->handle();

    END_SECAPI
}


OSStatus
SecKeychainLogin(UInt32 nameLength, const void* name, UInt32 passwordLength, const void* password)
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainLogin(%lu, %p, %lu, %p)", nameLength, name, passwordLength, password);
	globals().storageManager.login(nameLength, name,  passwordLength, password);

    END_SECAPI
}


OSStatus
SecKeychainLogout()
{
    BEGIN_SECAPI

	secdebug("kc", "SecKeychainLogout()");
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
        RequiredParam(inPassword);
        KeychainImpl::required(keychainRef)->create(passwordLength, inPassword);
	END_SECAPI
}

/* Modify a keychain so that it can be synchronized.
*/
OSStatus SecKeychainRecodeKeychain(SecKeychainRef keychainRef, CFDataRef dbBlob, CFDataRef extraData)
{
	BEGIN_SECAPI

	// do error checking for required parameters
	RequiredParam(dbBlob);
	RequiredParam(extraData);

	// convert from CF standards to CDSA standards
	const CssmData data(const_cast<UInt8 *>(CFDataGetBytePtr(dbBlob)),
		CFDataGetLength(dbBlob));
	const CssmData extraCssmData(const_cast<UInt8 *>(CFDataGetBytePtr(extraData)),
		CFDataGetLength(extraData));

	// do the work
	Keychain keychain = Keychain::optional(keychainRef);
	keychain->recode(data, extraCssmData);

	END_SECAPI
}

OSStatus SecKeychainCopySignature(SecKeychainRef keychainRef, CFDataRef *keychainSignature) 
{
	BEGIN_SECAPI

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
	
	secdebug("kc", "SecKeychainCreateWithBlob(\"%s\", %p, %p)", fullPathName, dbBlob, kcRef);
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

	secdebug("kc", "SecKeychainAddDBToKeychainList(%d, %s)", domain, dbName);
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
	secdebug("kc", "SecKeychainDBIsInKeychainList(%d, %s)", domain, dbName);
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
	secdebug("kc", "SecKeychainRemoveDBFromKeychainList(%d, %s)", domain, dbName);
	RequiredParam(dbName);
	StorageManager &smr = globals().storageManager;
	smr.removeFromDomainList(domain, dbName, *guid, subServiceType);
	END_SECAPI
}

