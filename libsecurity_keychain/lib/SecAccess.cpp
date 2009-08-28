/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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

#include <Security/SecAccess.h>
#include <Security/SecAccessPriv.h>
#include <security_keychain/Access.h>
#include "SecBridge.h"
#include <sys/param.h>

static CFArrayRef copyTrustedAppListFromBundle(CFStringRef bundlePath, CFStringRef trustedAppListFileName);

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
	OSStatus err = noErr;
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
		    int ix,top;
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
                err = SecTrustedApplicationCreateFromPath(buffer,&someOther);
                if (!err)
                    CFArrayAppendValue(trustedApplications,someOther); 
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
