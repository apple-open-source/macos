/*
 * Copyright (c) 2002-2004,2011-2014 Apple Inc. All Rights Reserved.
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

#include <SecBase.h>
#include <Security/SecAccess.h>
#include <Security/SecAccessPriv.h>
#include <Security/SecTrustedApplication.h>
#include <Security/SecTrustedApplicationPriv.h>
#include <security_keychain/Access.h>
#include "SecBridge.h"
#include <sys/param.h>

#undef secdebug
#include <utilities/SecCFWrappers.h>


/* No restrictions. Permission to perform all operations on
   the resource or available to an ACL owner.  */


CFTypeRef kSecACLAuthorizationAny = (CFTypeRef)(CFSTR("ACLAuthorizationAny"));

CFTypeRef kSecACLAuthorizationLogin = (CFTypeRef)(CFSTR("ACLAuthorizationLogin"));
CFTypeRef kSecACLAuthorizationGenKey = (CFTypeRef)(CFSTR("ACLAuthorizationGenKey"));
CFTypeRef kSecACLAuthorizationDelete = (CFTypeRef)(CFSTR("ACLAuthorizationDelete"));
CFTypeRef kSecACLAuthorizationExportWrapped = (CFTypeRef)(CFSTR("ACLAuthorizationExportWrapped"));
CFTypeRef kSecACLAuthorizationExportClear = (CFTypeRef)(CFSTR("ACLAuthorizationExportClear"));
CFTypeRef kSecACLAuthorizationImportWrapped = (CFTypeRef)(CFSTR("ACLAuthorizationImportWrapped"));
CFTypeRef kSecACLAuthorizationImportClear = (CFTypeRef)(CFSTR("ACLAuthorizationImportClear"));
CFTypeRef kSecACLAuthorizationSign = (CFTypeRef)(CFSTR("ACLAuthorizationSign"));
CFTypeRef kSecACLAuthorizationEncrypt = (CFTypeRef)(CFSTR("ACLAuthorizationEncrypt"));
CFTypeRef kSecACLAuthorizationDecrypt = (CFTypeRef)(CFSTR("ACLAuthorizationDecrypt"));
CFTypeRef kSecACLAuthorizationMAC = (CFTypeRef)(CFSTR("ACLAuthorizationMAC"));
CFTypeRef kSecACLAuthorizationDerive = (CFTypeRef)(CFSTR("ACLAuthorizationDerive"));

/* Defined authorization tag values for Keychain */



CFTypeRef kSecACLAuthorizationKeychainCreate = (CFTypeRef)(CFSTR("ACLAuthorizationKeychainCreate"));
CFTypeRef kSecACLAuthorizationKeychainDelete = (CFTypeRef)(CFSTR("ACLAuthorizationKeychainDelete"));
CFTypeRef kSecACLAuthorizationKeychainItemRead = (CFTypeRef)(CFSTR("ACLAuthorizationKeychainItemRead"));
CFTypeRef kSecACLAuthorizationKeychainItemInsert = (CFTypeRef)(CFSTR("ACLAuthorizationKeychainItemInsert"));
CFTypeRef kSecACLAuthorizationKeychainItemModify = (CFTypeRef)(CFSTR("ACLAuthorizationKeychainItemModify"));
CFTypeRef kSecACLAuthorizationKeychainItemDelete = (CFTypeRef)(CFSTR("ACLAuthorizationKeychainItemDelete"));

CFTypeRef kSecACLAuthorizationChangeACL = (CFTypeRef)(CFSTR("ACLAuthorizationChangeACL"));
CFTypeRef kSecACLAuthorizationChangeOwner = (CFTypeRef)(CFSTR("ACLAuthorizationChangeOwner"));


static CFArrayRef copyTrustedAppListFromBundle(CFStringRef bundlePath, CFStringRef trustedAppListFileName);

static CFStringRef gKeys[] =
{
	(CFStringRef)kSecACLAuthorizationAny,
	(CFStringRef)kSecACLAuthorizationLogin,
	(CFStringRef)kSecACLAuthorizationGenKey,
	(CFStringRef)kSecACLAuthorizationDelete,
	(CFStringRef)kSecACLAuthorizationExportWrapped,
	(CFStringRef)kSecACLAuthorizationExportClear,
	(CFStringRef)kSecACLAuthorizationImportWrapped,
	(CFStringRef)kSecACLAuthorizationImportClear,
	(CFStringRef)kSecACLAuthorizationSign,
	(CFStringRef)kSecACLAuthorizationEncrypt,
	(CFStringRef)kSecACLAuthorizationDecrypt,
	(CFStringRef)kSecACLAuthorizationMAC,
	(CFStringRef)kSecACLAuthorizationDerive,

	/* Defined authorization tag values for Keychain */
	(CFStringRef)kSecACLAuthorizationKeychainCreate,
	(CFStringRef)kSecACLAuthorizationKeychainDelete,
	(CFStringRef)kSecACLAuthorizationKeychainItemRead,
	(CFStringRef)kSecACLAuthorizationKeychainItemInsert,
	(CFStringRef)kSecACLAuthorizationKeychainItemModify,
	(CFStringRef)kSecACLAuthorizationKeychainItemDelete,

	(CFStringRef)kSecACLAuthorizationChangeACL,
	(CFStringRef)kSecACLAuthorizationChangeOwner

};

static sint32 gValues[] =
{
	CSSM_ACL_AUTHORIZATION_ANY,
	CSSM_ACL_AUTHORIZATION_LOGIN,
	CSSM_ACL_AUTHORIZATION_GENKEY,
	CSSM_ACL_AUTHORIZATION_DELETE,
	CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED,
	CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR,
	CSSM_ACL_AUTHORIZATION_IMPORT_WRAPPED,
	CSSM_ACL_AUTHORIZATION_IMPORT_CLEAR,
	CSSM_ACL_AUTHORIZATION_SIGN,
	CSSM_ACL_AUTHORIZATION_ENCRYPT,
	CSSM_ACL_AUTHORIZATION_DECRYPT,
	CSSM_ACL_AUTHORIZATION_MAC,
	CSSM_ACL_AUTHORIZATION_DERIVE,
	CSSM_ACL_AUTHORIZATION_DBS_CREATE,
	CSSM_ACL_AUTHORIZATION_DBS_DELETE,
	CSSM_ACL_AUTHORIZATION_DB_READ,
	CSSM_ACL_AUTHORIZATION_DB_INSERT,
	CSSM_ACL_AUTHORIZATION_DB_MODIFY,
	CSSM_ACL_AUTHORIZATION_DB_DELETE,
	CSSM_ACL_AUTHORIZATION_CHANGE_ACL,
	CSSM_ACL_AUTHORIZATION_CHANGE_OWNER
};

static
CFDictionaryRef CreateStringToNumDictionary()
{
	int numItems = (sizeof(gValues) / sizeof(sint32));
	CFMutableDictionaryRef tempDict = CFDictionaryCreateMutable(kCFAllocatorDefault, numItems, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for (int iCnt = 0; iCnt < numItems; iCnt++)
	{
		sint32 aNumber = gValues[iCnt];
		CFNumberRef aNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &aNumber);

		CFStringRef aString = gKeys[iCnt];
		CFDictionaryAddValue(tempDict, aString, aNum);
		CFRelease(aNum);
	}

	CFDictionaryRef result = CFDictionaryCreateCopy(kCFAllocatorDefault, tempDict);
	CFRelease(tempDict);
	return result;

}

static
CFDictionaryRef CreateNumToStringDictionary()
{
	int numItems = (sizeof(gValues) / sizeof(sint32));

	CFMutableDictionaryRef tempDict = CFDictionaryCreateMutable(kCFAllocatorDefault, numItems, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	for (int iCnt = 0; iCnt < numItems; iCnt++)
	{
		sint32 aNumber = gValues[iCnt];
		CFNumberRef aNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &aNumber);

		CFStringRef aString = gKeys[iCnt];
		CFDictionaryAddValue(tempDict, aNum, aString);
		CFRelease(aNum);

	}

	CFDictionaryRef result = CFDictionaryCreateCopy(kCFAllocatorDefault, tempDict);
	CFRelease(tempDict);
	return result;
}


/* TODO: This should be in some header */
sint32 GetACLAuthorizationTagFromString(CFStringRef aclStr);
sint32 GetACLAuthorizationTagFromString(CFStringRef aclStr)
{
	if (NULL == aclStr)
	{
#ifndef NDEBUG
		CFShow(CFSTR("GetACLAuthorizationTagFromString aclStr is NULL"));
#endif
		return 0;
	}

	static CFDictionaryRef gACLMapping = NULL;

	if (NULL == gACLMapping)
	{
		gACLMapping = CreateStringToNumDictionary();
	}

	sint32 result = 0;
	CFNumberRef valueResult = (CFNumberRef)CFDictionaryGetValue(gACLMapping, aclStr);
	if (NULL != valueResult)
	{
		if (!CFNumberGetValue(valueResult, kCFNumberSInt32Type, &result))
		{
			return 0;
		}

	}
	else
	{
		return 0;
	}

	return result;

}

/* TODO: This should be in some header */
CFStringRef GetAuthStringFromACLAuthorizationTag(sint32 tag);
CFStringRef GetAuthStringFromACLAuthorizationTag(sint32 tag)
{
	static CFDictionaryRef gTagMapping = NULL;
	CFNumberRef aNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &tag);

	if (NULL == gTagMapping)
	{
		gTagMapping = CreateNumToStringDictionary();
	}

	CFStringRef result = (CFStringRef)kSecACLAuthorizationAny;

	if (NULL != gTagMapping && CFDictionaryContainsKey(gTagMapping, aNum))
	{
		result = (CFStringRef)CFDictionaryGetValue(gTagMapping, aNum);
	}
	return result;
}

//
// CF boilerplate
//
CFTypeID SecAccessGetTypeID(void)
{
	BEGIN_SECAPI
	return gTypes().Access.typeID;
	END_SECAPI1(_kCFRuntimeNotATypeID)
}


//
// API bridge calls
//
/*!
 *	Create a new SecAccessRef that is set to the default configuration
 *	of a (newly created) security object.
 */
OSStatus SecAccessCreate(CFStringRef descriptor, CFArrayRef trustedList, SecAccessRef *accessRef)
{
	BEGIN_SECAPI
	Required(descriptor);
	SecPointer<Access> access;
	if (trustedList) {
		CFIndex length = CFArrayGetCount(trustedList);
		ACL::ApplicationList trusted;
		for (CFIndex n = 0; n < length; n++)
			trusted.push_back(TrustedApplication::required(
				SecTrustedApplicationRef(CFArrayGetValueAtIndex(trustedList, n))));
		access = new Access(cfString(descriptor), trusted);
	} else {
		access = new Access(cfString(descriptor));
	}
	Required(accessRef) = access->handle();
	END_SECAPI
}


/*!
 */
OSStatus SecAccessCreateFromOwnerAndACL(const CSSM_ACL_OWNER_PROTOTYPE *owner,
	uint32 aclCount, const CSSM_ACL_ENTRY_INFO *acls,
	SecAccessRef *accessRef)
{
	BEGIN_SECAPI
	Required(accessRef);	// preflight
	SecPointer<Access> access = new Access(Required(owner), aclCount, &Required(acls));
	*accessRef = access->handle();
	END_SECAPI
}

SecAccessRef SecAccessCreateWithOwnerAndACL(uid_t userId, gid_t groupId, SecAccessOwnerType ownerType, CFArrayRef acls, CFErrorRef *error)
{
	SecAccessRef result = NULL;

	CSSM_ACL_PROCESS_SUBJECT_SELECTOR selector =
	{
		CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION,	// selector version
		ownerType,
		userId,
		groupId
	};

	CSSM_LIST_ELEMENT subject2 = { NULL, 0 };
	subject2.Element.Word.Data = (UInt8 *)&selector;
	subject2.Element.Word.Length = sizeof(selector);
	CSSM_LIST_ELEMENT subject1 =
	{
		&subject2, CSSM_ACL_SUBJECT_TYPE_PROCESS, CSSM_LIST_ELEMENT_WORDID
	};

	CFIndex numAcls = 0;

	if (NULL != acls)
	{
		numAcls = CFArrayGetCount(acls);
	}

#ifndef NDEBUG
	CFStringRef debugStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
		CFSTR("SecAccessCreateWithOwnerAndACL: processing %d acls"), (int)numAcls);
	CFShow(debugStr);
	CFRelease(debugStr);
#endif

	CSSM_ACL_AUTHORIZATION_TAG rights[numAcls];
	memset(rights, 0, sizeof(rights));

	for (CFIndex iCnt = 0; iCnt < numAcls; iCnt++)
	{
		CFStringRef aclStr = (CFStringRef)CFArrayGetValueAtIndex(acls, iCnt);

#ifndef NDEBUG
		debugStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
			CFSTR("SecAccessCreateWithOwnerAndACL: acls[%d] = %@"), (int)iCnt, aclStr);

		CFShow(debugStr);
		CFRelease(debugStr);
#endif

		CSSM_ACL_AUTHORIZATION_TAG aTag = GetACLAuthorizationTagFromString(aclStr);

#ifndef NDEBUG
		debugStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
			CFSTR("SecAccessCreateWithOwnerAndACL: rights[%d] = %d"), (int)iCnt, aTag);

		CFShow(debugStr);
		CFRelease(debugStr);
#endif

		rights[iCnt] = aTag;
	}


	for (CFIndex iCnt = 0; iCnt < numAcls; iCnt++)
	{
#ifndef NDEBUG
		debugStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
			CFSTR("SecAccessCreateWithOwnerAndACL: rights[%d]  = %d"), (int)iCnt, rights[iCnt]);

		CFShow(debugStr);
		CFRelease(debugStr);
#endif


	}

	CSSM_ACL_OWNER_PROTOTYPE owner =
	{
		// TypedSubject
		{ CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
		// Delegate
		false
	};


	// ACL entries (any number, just one here)
	CSSM_ACL_ENTRY_INFO acl_rights[] =
	{
		{
			// prototype
			{
				// TypedSubject
				{ CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
				false,	// Delegate
				// rights for this entry
				{ (uint32)(sizeof(rights) / sizeof(rights[0])), rights },
				// rest is defaulted
			}
		}
	};

	OSStatus err = SecAccessCreateFromOwnerAndACL(&owner,
		sizeof(acl_rights) / sizeof(acl_rights[0]), acl_rights, &result);

	if (errSecSuccess != err)
	{
		result = NULL;
		if (NULL != error)
		{
			*error  = CFErrorCreate(kCFAllocatorDefault, CFSTR("FIX ME"), err, NULL);
   		}
	}
	return result;
}


/*!
 */
OSStatus SecAccessGetOwnerAndACL(SecAccessRef accessRef,
	CSSM_ACL_OWNER_PROTOTYPE_PTR *owner,
	uint32 *aclCount, CSSM_ACL_ENTRY_INFO_PTR *acls)
{
	BEGIN_SECAPI
	Access::required(accessRef)->copyOwnerAndAcl(
		Required(owner), Required(aclCount), Required(acls));
	END_SECAPI
}

OSStatus SecAccessCopyOwnerAndACL(SecAccessRef accessRef, uid_t* userId, gid_t* groupId, SecAccessOwnerType* ownerType, CFArrayRef* aclList)
{
	CSSM_ACL_OWNER_PROTOTYPE_PTR owner = NULL;
	CSSM_ACL_ENTRY_INFO_PTR acls = NULL;
	uint32 aclCount = 0;
	OSStatus result = SecAccessGetOwnerAndACL(accessRef, &owner, &aclCount, &acls);
	if (errSecSuccess != result )
	{
		return result;
	}

	if (NULL != owner)
	{
		CSSM_LIST_ELEMENT_PTR listHead = owner->TypedSubject.Head;
		if (listHead != NULL && listHead->ElementType == CSSM_LIST_ELEMENT_WORDID)
		{
			CSSM_LIST_ELEMENT_PTR nextElement = listHead->NextElement;
			if (listHead->WordID == CSSM_ACL_SUBJECT_TYPE_PROCESS && listHead->ElementType == CSSM_LIST_ELEMENT_WORDID)
			{
				// nextElement contains the required data
				CSSM_ACL_PROCESS_SUBJECT_SELECTOR* selectorPtr = (CSSM_ACL_PROCESS_SUBJECT_SELECTOR*)nextElement->Element.Word.Data;
				if (NULL != selectorPtr)
				{
					if (NULL != userId)
					{
						*userId = (uid_t)selectorPtr->uid;
					}

					if (NULL != groupId)
					{
						*groupId = (gid_t)selectorPtr->gid;
					}

					if (NULL != ownerType)
					{
						*ownerType = (SecAccessOwnerType)selectorPtr->mask;
					}
				}
			}

		}

	}

	if (NULL != aclList)
	{
#ifndef NDEBUG
		CFShow(CFSTR("SecAccessCopyOwnerAndACL: processing the ACL list"));
#endif

		CFMutableArrayRef stringArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		CSSM_ACL_OWNER_PROTOTYPE_PTR protoPtr = NULL;
		uint32 numAcls = 0L;
		CSSM_ACL_ENTRY_INFO_PTR aclEntry = NULL;

		result = SecAccessGetOwnerAndACL(accessRef, &protoPtr, &numAcls, &aclEntry);
		if (errSecSuccess == result)
		{
#ifndef NDEBUG
			CFStringRef tempStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("SecAccessCopyOwnerAndACL: numAcls = %d"), numAcls);
			CFShow(tempStr);
			CFRelease(tempStr);
#endif

			for (uint32 iCnt = 0; iCnt < numAcls; iCnt++)
			{
				CSSM_ACL_ENTRY_PROTOTYPE prototype = aclEntry[iCnt].EntryPublicInfo;
				CSSM_AUTHORIZATIONGROUP authGroup = prototype.Authorization;
				int numAuthTags = (int)authGroup.NumberOfAuthTags;

				for (int jCnt = 0; jCnt < numAuthTags; jCnt++)
				{

					sint32 aTag = authGroup.AuthTags[jCnt];
					CFStringRef aString = GetAuthStringFromACLAuthorizationTag(aTag);

					CFArrayAppendValue(stringArray, aString);
				}
			}
		}

		if (NULL != stringArray)
		{
			if (0 < CFArrayGetCount(stringArray))
			{
				*aclList = CFArrayCreateCopy(kCFAllocatorDefault, stringArray);
			}
			CFRelease(stringArray);
		}
	}

	return result;
}

/*!
 */
OSStatus SecAccessCopyACLList(SecAccessRef accessRef,
	CFArrayRef *aclList)
{
	BEGIN_SECAPI
	Required(aclList) = Access::required(accessRef)->copySecACLs();
	END_SECAPI
}


/*!
 */
OSStatus SecAccessCopySelectedACLList(SecAccessRef accessRef,
	CSSM_ACL_AUTHORIZATION_TAG action,
	CFArrayRef *aclList)
{
	BEGIN_SECAPI
	Required(aclList) = Access::required(accessRef)->copySecACLs(action);
	END_SECAPI
}

CFArrayRef SecAccessCopyMatchingACLList(SecAccessRef accessRef, CFTypeRef authorizationTag)
{
	CFArrayRef result = NULL;
	CSSM_ACL_AUTHORIZATION_TAG tag = GetACLAuthorizationTagFromString((CFStringRef)authorizationTag);
	OSStatus err = SecAccessCopySelectedACLList(accessRef, tag, &result);
	if (errSecSuccess != err)
	{
		result = NULL;
	}
	return result;
}

CFArrayRef copyTrustedAppListFromBundle(CFStringRef bundlePath, CFStringRef trustedAppListFileName)
{
	CFStringRef errorString = nil;
    CFURLRef bundleURL,trustedAppsURL = NULL;
    CFBundleRef secBundle = NULL;
	CFPropertyListRef trustedAppsPlist = NULL;
	CFDataRef xmlDataRef = NULL;
	SInt32 errorCode;
    CFArrayRef trustedAppList = NULL;
	CFMutableStringRef trustedAppListFileNameWithoutExtension = NULL;

    // Make a CFURLRef from the CFString representation of the bundle’s path.
    bundleURL = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,bundlePath,kCFURLPOSIXPathStyle,true);

	CFRange wholeStrRange;

	if (!bundleURL)
        goto xit;

    // Make a bundle instance using the URLRef.
    secBundle = CFBundleCreate(kCFAllocatorDefault,bundleURL);
    if (!secBundle)
        goto xit;

	trustedAppListFileNameWithoutExtension =
		CFStringCreateMutableCopy(NULL,CFStringGetLength(trustedAppListFileName),trustedAppListFileName);
	wholeStrRange = CFStringFind(trustedAppListFileName,CFSTR(".plist"),0);

	CFStringDelete(trustedAppListFileNameWithoutExtension,wholeStrRange);

    // Look for a resource in the bundle by name and type
    trustedAppsURL = CFBundleCopyResourceURL(secBundle,trustedAppListFileNameWithoutExtension,CFSTR("plist"),NULL);
    if (!trustedAppsURL)
        goto xit;

    if ( trustedAppListFileNameWithoutExtension )
		CFRelease(trustedAppListFileNameWithoutExtension);

	if (!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,trustedAppsURL,&xmlDataRef,NULL,NULL,&errorCode))
        goto xit;

	trustedAppsPlist = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,xmlDataRef,kCFPropertyListImmutable,&errorString);
    trustedAppList = (CFArrayRef)trustedAppsPlist;

xit:
    if (bundleURL)
        CFRelease(bundleURL);
    if (secBundle)
        CFRelease(secBundle);
    if (trustedAppsURL)
        CFRelease(trustedAppsURL);
    if (xmlDataRef)
        CFRelease(xmlDataRef);
    if (errorString)
        CFRelease(errorString);

    return trustedAppList;
}

OSStatus SecAccessCreateWithTrustedApplications(CFStringRef trustedApplicationsPListPath, CFStringRef accessLabel, Boolean allowAny, SecAccessRef* returnedAccess)
{
	OSStatus err = errSecSuccess;
	SecAccessRef accessToReturn=nil;
	CFMutableArrayRef trustedApplications=nil;

	if (!allowAny) // use default access ("confirm access")
	{
		// make an exception list of applications you want to trust,
		// which are allowed to access the item without requiring user confirmation
		SecTrustedApplicationRef myself=NULL, someOther=NULL;
        CFArrayRef trustedAppListFromBundle=NULL;

        trustedApplications=CFArrayCreateMutable(kCFAllocatorDefault,0,&kCFTypeArrayCallBacks);
        err = SecTrustedApplicationCreateFromPath(NULL, &myself);
        if (!err)
            CFArrayAppendValue(trustedApplications,myself);

		CFURLRef url = CFURLCreateWithFileSystemPath(NULL, trustedApplicationsPListPath, kCFURLPOSIXPathStyle, 0);
		CFStringRef leafStr = NULL;
		leafStr = CFURLCopyLastPathComponent(url);

		CFURLRef bndlPathURL = NULL;
		bndlPathURL = CFURLCreateCopyDeletingLastPathComponent(NULL, url);
		CFStringRef bndlPath = NULL;
		bndlPath = CFURLCopyFileSystemPath(bndlPathURL, kCFURLPOSIXPathStyle);
        trustedAppListFromBundle=copyTrustedAppListFromBundle(bndlPath, leafStr);
		if ( leafStr )
			CFRelease(leafStr);
		if ( bndlPath )
			CFRelease(bndlPath);
		if ( url )
			CFRelease(url);
		if ( bndlPathURL )
			CFRelease(bndlPathURL);
        if (trustedAppListFromBundle)
        {
            CFIndex ix,top;
            char buffer[MAXPATHLEN];
            top = CFArrayGetCount(trustedAppListFromBundle);
            for (ix=0;ix<top;ix++)
            {
                CFStringRef filename = (CFStringRef)CFArrayGetValueAtIndex(trustedAppListFromBundle,ix);
                CFIndex stringLength = CFStringGetLength(filename);
                CFIndex usedBufLen;

                if (stringLength != CFStringGetBytes(filename,CFRangeMake(0,stringLength),kCFStringEncodingUTF8,0,
                    false,(UInt8 *)&buffer,MAXPATHLEN, &usedBufLen))
                    break;
                buffer[usedBufLen] = 0;
				//
				// Support specification of trusted applications by either
				// a full pathname or a code requirement string.
				//
				if (buffer[0]=='/')
				{
					err = SecTrustedApplicationCreateFromPath(buffer,&someOther);
				}
				else
				{
					char *buf = NULL;
					CFStringRef reqStr = filename;
					CFArrayRef descArray = CFStringCreateArrayBySeparatingStrings(NULL, reqStr, CFSTR("\""));
					if (descArray && (CFArrayGetCount(descArray) > 1))
					{
						CFStringRef descStr = (CFStringRef) CFArrayGetValueAtIndex(descArray, 1);
						if (descStr)
							buf = CFStringToCString(descStr);
					}
					SecRequirementRef reqRef = NULL;
					err = SecRequirementCreateWithString(reqStr, kSecCSDefaultFlags, &reqRef);
					if (!err)
						err = SecTrustedApplicationCreateFromRequirement((const char *)buf, reqRef, &someOther);
					if (buf)
						free(buf);
					CFReleaseSafe(reqRef);
					CFReleaseSafe(descArray);
				}
                if (!err)
                    CFArrayAppendValue(trustedApplications,someOther);

				if (someOther)
					CFReleaseNull(someOther);
            }
            CFRelease(trustedAppListFromBundle);
        }
	}

	err = SecAccessCreate((CFStringRef)accessLabel, (CFArrayRef)trustedApplications, &accessToReturn);
    if (!err)
	{
		if (allowAny) // change access to be wide-open for decryption ("always allow access")
		{
			// get the access control list for decryption operations
			CFArrayRef aclList=nil;
			err = SecAccessCopySelectedACLList(accessToReturn, CSSM_ACL_AUTHORIZATION_DECRYPT, &aclList);
			if (!err)
			{
				// get the first entry in the access control list
				SecACLRef aclRef=(SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
				CFArrayRef appList=nil;
				CFStringRef promptDescription=nil;
				CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector;
				err = SecACLCopySimpleContents(aclRef, &appList, &promptDescription, &promptSelector);

				// modify the default ACL to not require the passphrase, and have a nil application list
				promptSelector.flags &= ~CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE;
				err = SecACLSetSimpleContents(aclRef, NULL, promptDescription, &promptSelector);

				if (appList) CFRelease(appList);
				if (promptDescription) CFRelease(promptDescription);
			}
		}
	}
	*returnedAccess = accessToReturn;
	return err;
}
