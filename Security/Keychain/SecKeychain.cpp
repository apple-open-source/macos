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

#include <Security/SecKeychainAPIPriv.h>
#include <Security/SecKeychain.h>
#include <Security/KCCursor.h>
#include <Security/cssmdata.h>
#include <Security/KCExceptions.h>
#include "SecBridge.h"
#include "CCallbackMgr.h"
#include "Schema.h"
#include <Security/ktracecodes.h>
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
SecKeychainUnlock(SecKeychainRef keychainRef, UInt32 passwordLength, void *password, Boolean usePassword)
{
	BEGIN_SECAPI

	secdebug("kc", "SecKeychainUnlock(%p, %lu, %p, %d)", keychainRef, passwordLength, password, usePassword);
	Keychain keychain = Keychain::optional(keychainRef);

	if (usePassword)
		keychain->unlock(CssmData(password,passwordLength));
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
        KCThrowParamErrIf_(password==NULL);
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
        //
        // Clears the plist and moves aside (renames) an existing login.keychain
        //
        globals().storageManager.resetKeychain(resetSearchList);
        //
        // Creates a login keychain and sets it to the default.
        //
        globals().storageManager.login(userName.length(), userName.c_str(), passwordLength, password);	
        Keychain keychain = globals().storageManager.loginKeychain();
        globals().storageManager.defaultKeychain(keychain);
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
	Item item(kSecInternetPasswordItemClass, 'aapl', passwordLength, passwordData);
	
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
	Item item(kSecGenericPasswordItemClass, 'aapl', passwordLength, passwordData);

	if (serviceName && serviceNameLength)
	{
		CssmData service(const_cast<void *>(reinterpret_cast<const void *>(serviceName)), serviceNameLength);
		item->setAttribute(Schema::attributeInfo(kSecServiceItemAttr), service);
		// use service name as default label
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
    Debug::trace (kSecTraceSecurityFrameworkSecKeychainFindGenericPasswordBegin);

    BEGIN_SECAPI

	secdebug("kc", "SecKeychainFindGenericPassword(%p)", keychainOrArray);
	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);

	if (serviceName && serviceNameLength)
	{
		cursor->add (CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr),
			const_cast<char*>(serviceName));
	}
        
	if (accountName && accountNameLength)
	{
		cursor->add (CSSM_DB_EQUAL, Schema::attributeInfo(kSecAccountItemAttr),
			const_cast<char*>(accountName));
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
SecKeychainLogin(UInt32 nameLength, void* name, UInt32 passwordLength, void* password)
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

static CFStringRef copyErrorMessageFromBundle(OSStatus status,CFStringRef tableName);

// caller MUST release the string, since it is gotten with "CFCopyLocalizedStringFromTableInBundle"
// intended use of reserved param is to pass in CFStringRef with name of the Table for lookup
// Will look by default in "SecErrorMessages.strings" in the resources of Security.framework.


CFStringRef SecCopyErrorMessageString(OSStatus status, void *reserved)
{
	BEGIN_SECAPI

	return copyErrorMessageFromBundle(status,CFSTR("SecErrorMessages"));
        
	END_SECAPI1(NULL)
}

CFStringRef copyErrorMessageFromBundle(OSStatus status,CFStringRef tableName)
{
    CFStringRef errorString = nil;
    CFStringRef keyString = nil;
    CFURLRef bundleURL = NULL;
    CFBundleRef secBundle = NULL;

    // Make a CFURLRef from the CFString representation of the bundle’s path.
    bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        CFSTR("/System/Library/Frameworks/Security.framework/"),kCFURLPOSIXPathStyle,true);	//	Resources/
    if (!bundleURL)
        goto xit;
        
    // Make a bundle instance using the URLRef.
    secBundle = CFBundleCreate(kCFAllocatorDefault,bundleURL);
    if (!secBundle)
        goto xit;

    // Convert status to Int32 string representation, e.g. "-25924"
    keyString = CFStringCreateWithFormat (kCFAllocatorDefault,NULL,CFSTR("%d"),status);
    if (!keyString)
        goto xit;

	errorString = CFCopyLocalizedStringFromTableInBundle(keyString,tableName,secBundle,NULL);
    
xit:
    if (bundleURL)
        CFRelease(bundleURL);	
    if (secBundle)
        CFRelease(secBundle);	
    if (keyString)
        CFRelease(keyString);	

    return errorString;
}

