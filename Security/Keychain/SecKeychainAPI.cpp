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
 *  SecKeychainAPI.cpp
 *  SecurityCore
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */


#include <Security/SecKeychainAPI.h>
#include "SecKeychainAPIPriv.h"
#include "Keychains.h"
#include "Globals.h"
#include "KCUtilities.h"
#include "KCEventNotifier.h"
#include "KCCursor.h"
#include "CCallbackMgr.h"
#include "KCExceptions.h"
#include "Schema.h"
#include <Security/globalizer.h>

using namespace Security;

using namespace KeychainCore;

//
// API boilerplate macros. These provide a frame for C++ code that is impermeable to exceptions.
// Usage:
//	BEGIN_API
//		... your C++ code here ...
//  END_API		// returns CSSM_RETURN on exception
//	END_API0	// returns nothing (void) on exception
//	END_API1(bad) // return (bad) on exception
//
#define BEGIN_SECAPI \
	try { \
		StLock<Mutex> _(globals().apiLock);
#define END_SECAPI \
	} \
	catch (const MacOSError &err) { return err.osStatus(); } \
	catch (const CssmCommonError &err) { return GetKeychainErrFromCSSMErr(err.cssmError())/*err.cssmError(CSSM_CSSM_BASE_ERROR)*/; } \
	catch (::std::bad_alloc) { return memFullErr; } \
	catch (...) { return internalComponentErr; } \
    return noErr;
#define END_SECAPI0		} catch (...) { return; }
#define END_SECAPI1(bad)	} catch (...) { return bad; }


OSStatus SecKeychainGetVersion(UInt32 *returnVers)
{
    if (!returnVers) return noErr;

	*returnVers=0x02028000;
	return noErr;
}


OSStatus SecKeychainOpen(const char *pathName, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI
		RequiredParam(keychainRef)=KeychainRef::handle(globals().storageManager.make(pathName));
	END_SECAPI
}

OSStatus SecKeychainCreateNew(const char *pathName, SecKeychainRef *keychainRef, UInt32 passwordLength, const void *password, Boolean promptUser)
{
    BEGIN_SECAPI

		KCThrowParamErrIf_(!pathName);

		Keychain keychain = globals().storageManager.make(pathName);
		
		if(promptUser)
		{
			keychain->create();
		}
		else
		{
            KCThrowParamErrIf_(!password);
            
			keychain->create(passwordLength, password);
		}
        RequiredParam(keychainRef)=KeychainRef::handle(keychain);

	END_SECAPI
}

OSStatus SecKeychainDelete(SecKeychainRef keychainRef)
{
    BEGIN_SECAPI
	
		Keychain keychain = Keychain::optional(keychainRef);
		keychain->database()->deleteDb();
		
        list<SecKeychainRef> SecKeychainRefToRemove;
		SecKeychainRefToRemove.push_back(keychainRef);
		KeychainCore::StorageManager &smgr=KeychainCore::globals().storageManager;
		smgr.remove(SecKeychainRefToRemove);
		return noErr;

	END_SECAPI


}
OSStatus SecKeychainSetSettings(SecKeychainRef keychainRef, const SecKeychainSettings *newSettings)
{
    BEGIN_SECAPI
        Keychain keychain = Keychain::optional(keychainRef);
		if(newSettings->version==SEC_KEYCHAIN_SETTINGS_VERS1)
		{
			UInt32 lockInterval=newSettings->lockInterval;
			bool lockOnSleep=newSettings->lockOnSleep;

			keychain->setSettings(lockInterval, lockOnSleep);
        }
	END_SECAPI
}


OSStatus SecKeychainCopySettings(SecKeychainRef keychainRef, SecKeychainSettings *outSettings)
{
    BEGIN_SECAPI
        Keychain keychain = Keychain::optional(keychainRef);
		if(outSettings->version==SEC_KEYCHAIN_SETTINGS_VERS1)
		{
			UInt32 lockInterval;
			bool lockOnSleep;
			
			keychain->getSettings(lockInterval, lockOnSleep);
			outSettings->lockInterval=lockInterval;
			outSettings->lockOnSleep=lockOnSleep;
        }
	END_SECAPI
}
 

OSStatus SecKeychainUnlock(SecKeychainRef keychainRef, UInt32 passwordLength, void *password, Boolean usePassword)
{
    BEGIN_SECAPI
        Keychain keychain = Keychain::optional(keychainRef);

		if(usePassword)
			keychain->unlock(CssmData(password,passwordLength));
		else
			keychain->unlock();
	END_SECAPI
}
 

OSStatus SecKeychainLock(SecKeychainRef	keychainRef)
{
    BEGIN_SECAPI
        Keychain keychain = Keychain::optional(keychainRef);
		keychain->lock();
	END_SECAPI
}


OSStatus SecKeychainLockAll()
{
    BEGIN_SECAPI
		globals().storageManager.lockAll();
	END_SECAPI
}


OSStatus SecKeychainRelease(SecKeychainRef keychainRef)
{
    BEGIN_SECAPI
		KeychainRef::release(keychainRef);
	END_SECAPI
}
 

OSStatus SecKeychainCopyDefault(SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI
        RequiredParam(keychainRef)=KeychainRef::handle(globals().defaultKeychain.keychain());
	END_SECAPI
}


OSStatus SecKeychainSetDefault(SecKeychainRef keychainRef)
{
    BEGIN_SECAPI
		globals().defaultKeychain.keychain(Keychain::optional(keychainRef));
 	END_SECAPI
}
 

OSStatus SecKeychainGetStatus(SecKeychainRef keychainRef, SecKeychainStatus *keychainStatus)
{
    BEGIN_SECAPI
		RequiredParam(keychainStatus) = (SecKeychainStatus)Keychain::optional(keychainRef)->status();
	END_SECAPI
}
  

OSStatus SecKeychainGetPath(SecKeychainRef keychainRef, UInt32 * ioPathLength, char *pathName)
{
    BEGIN_SECAPI
		RequiredParam(pathName);
		const char *name = Keychain::optional(keychainRef)->name();
		UInt32 nameLen = strlen(name);
		memcpy(pathName, name, *ioPathLength);
		if(nameLen < *ioPathLength)  // if the size is smaller then the buffer
			*ioPathLength=nameLen;   // set the length.  otherwise the size is clipped because
									 // the buffer is too small.
		
	END_SECAPI
}


UInt16 SecKeychainListGetCount(void)
{
    BEGIN_SECAPI
		return globals().storageManager.size();
	END_SECAPI
}
 

OSStatus SecKeychainListCopyKeychainAtIndex(UInt16 index, SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI
		KeychainCore::StorageManager &smgr=KeychainCore::globals().storageManager;
		RequiredParam(keychainRef)=KeychainRef::handle(smgr[index]);
	END_SECAPI
}
 
OSStatus SecKeychainItemCreateFromContent(SecItemClass itemClass, SecKeychainAttributeList *attrList, UInt32 length, const void *data, SecKeychainRef keychainRef, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI
		KCThrowParamErrIf_(length!=0 && data==NULL);
        Item item(itemClass, attrList, length, data);
        Keychain::optional(keychainRef)->add(item);
        if (itemRef)
        	*itemRef = ItemRef::handle(item);
	END_SECAPI
}
 
OSStatus SecKeychainItemModifyContent(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data)
{
    BEGIN_SECAPI
		Item item = ItemRef::required(itemRef);
		item->modifyContent(attrList, length, data);
	END_SECAPI
}
 
 
OSStatus SecKeychainItemCopyContent(SecKeychainItemRef itemRef, SecItemClass *itemClass, SecKeychainAttributeList *attrList, UInt32 *length, void **outData)
{
	BEGIN_SECAPI
		Item item = ItemRef::required(itemRef);
		item->getContent(itemClass, attrList, length, outData);
	END_SECAPI
}

OSStatus SecKeychainItemFreeContent(SecKeychainAttributeList *attrList, void *data)
{
	BEGIN_SECAPI
		ItemImpl::freeContent(attrList, data);
	END_SECAPI
}
 

OSStatus SecKeychainAttributeInfoForItemID(SecKeychainRef keychainRef, UInt32 itemID, SecKeychainAttributeInfo **info)
{
	BEGIN_SECAPI
		Keychain keychain = Keychain::optional(keychainRef);
		keychain->getAttributeInfoForItemID(itemID, info);
	END_SECAPI
}

OSStatus SecKeychainFreeAttributeInfo(SecKeychainAttributeInfo *info)
{
	BEGIN_SECAPI
		KeychainImpl::freeAttributeInfo(info);
	END_SECAPI
}

OSStatus SecKeychainItemModifyAttributesAndData(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList, UInt32 length, const void *data)
{
    BEGIN_SECAPI
		Item item = ItemRef::required(itemRef);
		item->modifyAttributesAndData(attrList, length, data);
	END_SECAPI
}

OSStatus SecKeychainItemCopyAttributesAndData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info, SecItemClass *itemClass, SecKeychainAttributeList **attrList, UInt32 *length, void **outData)
{
	BEGIN_SECAPI
		Item item = ItemRef::required(itemRef);
		item->getAttributesAndData(info, itemClass, attrList, length, outData);
	END_SECAPI
}

OSStatus SecKeychainItemFreeAttributesAndData(SecKeychainAttributeList *attrList, void *data)
{
	BEGIN_SECAPI
		ItemImpl::freeAttributesAndData(attrList, data);
	END_SECAPI
}

OSStatus SecKeychainItemDelete(SecKeychainItemRef itemRef)
{
    BEGIN_SECAPI
		Item item = ItemRef::required( itemRef );
		Keychain keychain = item->keychain();
		KCThrowIf_( !keychain, errSecInvalidItemRef );
		
        keychain->deleteItem( item ); // item must be persistant.
	END_SECAPI
}
 

OSStatus SecKeychainItemCopyKeychain(SecKeychainItemRef itemRef, SecKeychainRef* keychainRef)
{
    BEGIN_SECAPI
		Required(keychainRef) = KeychainRef::handle(ItemRef::required(itemRef)->keychain());
	END_SECAPI
}


OSStatus SecKeychainItemCreateCopy(SecKeychainItemRef itemRef, SecKeychainItemRef *itemCopy, SecKeychainRef destKeychainRef)
{
    BEGIN_SECAPI
		Item copy = ItemRef::required(itemRef)->copyTo(Keychain::optional(destKeychainRef));
		if (itemCopy)
			*itemCopy = ItemRef::handle(copy);
	END_SECAPI
}
 

OSStatus SecKeychainItemRelease(SecKeychainItemRef itemRef)
{
    BEGIN_SECAPI
 		ItemRef::release(itemRef);
	END_SECAPI
}

OSStatus SecKeychainSearchCreateFromAttributes(SecKeychainRef keychainRef, SecItemClass itemClass, const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef)
{
    BEGIN_SECAPI

		Required(searchRef); // Make sure that searchRef is an invalid SearchRef

		KCCursor cursor;
		if (keychainRef)
			cursor = Keychain::optional(keychainRef)->createCursor(itemClass, attrList);
		else
			cursor = globals().storageManager.createCursor(itemClass, attrList);

        *searchRef = KCCursorRef::handle(cursor);

	END_SECAPI
}
 

OSStatus SecKeychainCopySearchNextItem(SecKeychainSearchRef searchRef, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI
        RequiredParam(itemRef);
        Item item;
        if (!KCCursorRef::required(searchRef)->next(item))
            return errSecItemNotFound;

        *itemRef=ItemRef::handle(item);
	END_SECAPI
}
 
OSStatus SecKeychainSearchRelease(SecKeychainSearchRef searchRef)
{
    BEGIN_SECAPI
		KCCursorRef::release(searchRef);
	END_SECAPI
}


OSStatus SecKeychainListRemoveKeychain(SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI
        list<SecKeychainRef> SecKeychainRefToRemove;
		SecKeychainRefToRemove.push_back(RequiredParam(keychainRef));
		StorageManager &smgr = globals().storageManager;
		smgr.remove(SecKeychainRefToRemove);
		return noErr;
	END_SECAPI
}
 

pascal OSStatus SecKeychainAddCallback(SecKeychainCallbackProcPtr callbackFunction, SecKeychainEventMask eventMask, void* userContext)
{
    BEGIN_SECAPI
		RequiredParam(callbackFunction);
		CCallbackMgr::AddCallback(callbackFunction,eventMask,userContext);
	END_SECAPI
}	

OSStatus SecKeychainRemoveCallback(SecKeychainCallbackProcPtr callbackFunction)
{
    BEGIN_SECAPI
		RequiredParam(callbackFunction);
        CCallbackMgr::RemoveCallback(callbackFunction);
	END_SECAPI
}	


// --- Private API

OSStatus SecKeychainChangePassword(SecKeychainRef keychainRef, UInt32 oldPasswordLength, const void *oldPassword,  UInt32 newPasswordLength, const void *newPassword)
{
    BEGIN_SECAPI
	globals().storageManager.changeLoginPassword(oldPasswordLength, oldPassword,  newPasswordLength, newPassword);
    END_SECAPI
}

OSStatus SecKeychainCopyLogin(SecKeychainRef *keychainRef)
{
    BEGIN_SECAPI
	// NOTE: operates on default Keychain!  It shouldn't... we want to 
	//		 have code that operates of a login keychain.
        RequiredParam(keychainRef)=KeychainRef::handle(globals().defaultKeychain.keychain());
    END_SECAPI
}


OSStatus SecKeychainAddInternetPassword(SecKeychainRef keychainRef, UInt32 serverNameLength, char *serverName, 
										UInt32 securityDomainLength, char *securityDomain, UInt32 accountNameLength, char *accountName, 
										UInt32 pathLength, char *path, UInt16 port, OSType protocol, OSType authType,
										UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI
		KCThrowParamErrIf_(passwordLength!=0 && passwordData==NULL);
		// @@@ Get real itemClass
        Item item(kSecInternetPasswordItemClass, 'aapl', passwordLength, passwordData);
		
		if (serverName && serverNameLength)
			item->setAttribute(Schema::attributeInfo(kSecServerItemAttr),
				CssmData(serverName, serverNameLength));
			
		if (accountName && accountNameLength)
		{
			CssmData account(accountName, accountNameLength);
			item->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
			 // @@@ We should probably leave setting of label up to lower level code.
			item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), account);
		}

		if (securityDomain && securityDomainLength)
			item->setAttribute(Schema::attributeInfo(kSecSecurityDomainItemAttr),
				CssmData(securityDomain, securityDomainLength));
			
		item->setAttribute(Schema::attributeInfo(kSecPortItemAttr), UInt32(port));
		item->setAttribute(Schema::attributeInfo(kSecProtocolItemAttr), protocol);
		item->setAttribute(Schema::attributeInfo(kSecAuthTypeItemAttr), authType);
			
		if (path && pathLength)
			item->setAttribute(Schema::attributeInfo(kSecPathItemAttr),
				CssmData(path, pathLength));

		Keychain::optional(keychainRef)->add(item);
        if (itemRef)
        	*itemRef = ItemRef::handle(item);

    END_SECAPI
}

OSStatus SecKeychainFindInternetPassword(SecKeychainRef keychainRef, UInt32 serverNameLength, char *serverName, 
										UInt32 securityDomainLength, char *securityDomain, UInt32 accountNameLength, char *accountName,
										UInt32 pathLength, char *path, UInt16 port, OSType protocol, OSType authType,
										UInt32 *passwordLength, void **passwordData, SecKeychainItemRef *itemRef)
												
{
     BEGIN_SECAPI


		UInt32 attrCount = 0;
					
		// The number of attributes to search on depends on what was passed in
		if ( serverName && serverNameLength)
			attrCount++;
			
		if ( securityDomain && securityDomainLength )
			attrCount++;
			
		if ( accountName && accountNameLength)
			attrCount++;
			
		if ( port )
			attrCount++;
			
		if ( protocol )
			attrCount++;
			
		if ( authType )
			attrCount++;
			
		if ( path && pathLength )
			attrCount++;
			
		auto_array<SecKeychainAttribute> attrs(attrCount);
		attrCount = 0;

		if ( serverName && serverNameLength )
		{
			attrs[attrCount].tag = kSecServerItemAttr;
			attrs[attrCount].length = serverNameLength;
			attrs[attrCount].data = serverName;
			attrCount++;
		}
		if ( securityDomain && securityDomainLength )
		{
			attrs[attrCount].tag = kSecSecurityDomainItemAttr;
			attrs[attrCount].length = securityDomainLength;
			attrs[attrCount].data = securityDomain;
			attrCount++;
		}
		if ( accountName && accountNameLength )
		{
			attrs[attrCount].tag = kSecAccountItemAttr;
			attrs[attrCount].length = accountNameLength;
			attrs[attrCount].data = accountName;
			attrCount++;
		}
		
		if ( port )
		{
			attrs[attrCount].tag = kSecPortItemAttr;
			attrs[attrCount].length = sizeof( port );
			attrs[attrCount].data = &port;
			attrCount++;
		}
		if ( protocol )
		{
			attrs[attrCount].tag = kSecProtocolItemAttr;
			attrs[attrCount].length = sizeof( protocol );
			attrs[attrCount].data = &protocol;
			attrCount++;
		}
		if ( authType )
		{
			attrs[attrCount].tag = kSecAuthTypeItemAttr;
			attrs[attrCount].length = sizeof( authType );
			attrs[attrCount].data = &authType;
			attrCount++;
		}
			
		if ( path  && pathLength )
		{
			attrs[attrCount].tag = kSecPathItemAttr;
			attrs[attrCount].length = pathLength;
			attrs[attrCount].data = path;
			attrCount++;
		}

        SecKeychainAttributeList attrList;
		attrList.count = attrCount;
		attrList.attr = attrs.get();
		
		Item item;
	
		KCCursor cursor;
		if (keychainRef)
			cursor = Keychain::optional(keychainRef)->createCursor(kSecInternetPasswordItemClass, &attrList);
		else
			cursor = globals().storageManager.createCursor(kSecInternetPasswordItemClass, &attrList);

		if (!cursor->next(item))
			return errSecItemNotFound;

			
		// Get its data (only if necessary)
		if ( passwordData || passwordLength )
		{
			CssmDataContainer outData;
			item->getData(outData);
			*passwordLength=outData.length();
			outData.Length=NULL;
			*passwordData=outData.data();
			outData.Data=NULL;
		}
		
		if (itemRef)
			*itemRef=ItemRef::handle(item);
 
            
    END_SECAPI

	

}

OSStatus SecKeychainAddGenericPassword(SecKeychainRef keychainRef, UInt32 serviceNameLength, char *serviceName,
									   UInt32 accountNameLength, char *accountName, 
									   UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef)
										
{
   BEGIN_SECAPI
 
		KCThrowParamErrIf_(passwordLength!=0 && passwordData==NULL);
		// @@@ Get real itemClass
        Item item(kSecGenericPasswordItemClass, 'aapl', passwordLength, passwordData);
		
		if (serviceName && serviceNameLength)
			item->setAttribute(Schema::attributeInfo(kSecServiceItemAttr), CssmData(serviceName, serviceNameLength));
			
		if (accountName && accountNameLength)
		{
			CssmData account(accountName, accountNameLength);
			item->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
			 // @@@ We should probably leave setting of label up to lower level code.
			item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), account);
		}

        Keychain::optional(keychainRef)->add(item);
        if (itemRef)
        	*itemRef = ItemRef::handle(item);

    END_SECAPI
}

OSStatus SecKeychainFindGenericPassword(SecKeychainRef keychainRef,  UInt32 serviceNameLength, char *serviceName,
										UInt32 accountNameLength, char *accountName,
										UInt32 *passwordLength, void **passwordData, SecKeychainItemRef *itemRef)
																			   
{
    BEGIN_SECAPI
		UInt32 attrCount = 0;
					
		// The number of attributes to search on depends on what was passed in
		if (serviceName && serviceNameLength)
			attrCount++;
			
		if (accountName && accountNameLength)
			attrCount++;

		auto_array<SecKeychainAttribute> attrs(attrCount);
		attrCount = 0;

		if (serviceName && serviceNameLength)
		{
			attrs[attrCount].tag = kSecServiceItemAttr;
			attrs[attrCount].length = serviceNameLength;
			attrs[attrCount].data = serviceName;
			attrCount++;
		}
		if (accountName && accountNameLength)
		{
			attrs[attrCount].tag = kSecAccountItemAttr;
			attrs[attrCount].length = accountNameLength;
			attrs[attrCount].data = accountName;
			attrCount++;
		}
		
        SecKeychainAttributeList attrList;
		attrList.count = attrCount;
		attrList.attr = attrs.get();

 		Item item;
	
		KCCursor cursor;
		if (keychainRef)
			cursor = Keychain::optional(keychainRef)->createCursor(kSecGenericPasswordItemClass, &attrList);
		else
			cursor = globals().storageManager.createCursor(kSecGenericPasswordItemClass, &attrList);

  		if (!cursor->next(item))
			return errSecItemNotFound;

			
		// Get its data (only if necessary)
		if ( passwordData || passwordLength )
		{
			CssmDataContainer outData;
			item->getData(outData);
			*passwordLength=outData.length();
			outData.Length=NULL;
			*passwordData=outData.data();
			outData.Data=NULL;
		}
		
		if (itemRef)
			*itemRef=ItemRef::handle(item);
 
		           
    END_SECAPI
}

OSStatus SecKeychainLogin(UInt32 nameLength, void* name, UInt32 passwordLength, void* password)
{
    BEGIN_SECAPI
	globals().storageManager.login(nameLength, name,  passwordLength, password);
    END_SECAPI
}

OSStatus SecKeychainLogout()
{
    BEGIN_SECAPI
	globals().storageManager.logout();
    END_SECAPI
}

OSStatus SecKeychainSetUserInteractionAllowed(Boolean state) 
{
	BEGIN_SECAPI
	globals().setUserInteractionAllowed(state);
    END_SECAPI
	
}

OSStatus SecKeychainGetUserInteractionAllowed(Boolean *state) 
{
	BEGIN_SECAPI
	Required(state)=globals().getUserInteractionAllowed();
    END_SECAPI
	
}

