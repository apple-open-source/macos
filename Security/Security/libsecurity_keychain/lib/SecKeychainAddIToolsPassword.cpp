/*
 *  Copyright (c) 2003-2013 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  SecKeychainAddIToolsPassword.c
 *  
 *	Based on Keychain item access control example
 *	  -- added "always allow" ACL support
 */

#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecAccess.h>
#include <Security/SecAccessPriv.h>
#include <Security/SecTrustedApplicationPriv.h>
#include <Security/SecACL.h>
#include "SecBridge.h"
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/cfutilities.h>


OSStatus SecKeychainAddIToolsPassword(SecKeychainRef keychain, UInt32 accountNameLength, const char *accountName,
    UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef)
{
	BEGIN_SECAPI

    const char *serviceUTF8 = "iTools";
	
	// create the initial ACL label string (use the account name, not "iTools")
	CFRef<CFStringRef> itemLabel = CFStringCreateWithBytes(kCFAllocatorDefault,
		(const UInt8 *)accountName, accountNameLength, kCFStringEncodingUTF8, FALSE);

	// accumulate applications in this list
	CFRef<CFMutableArrayRef> apps = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	
	// add entries for application groups
	CFRef<SecTrustedApplicationRef> dotMacGroup, accountsGroup;
	MacOSError::check(SecTrustedApplicationCreateApplicationGroup("dot-mac", NULL, &dotMacGroup.aref()));
	CFArrayAppendValue(apps, dotMacGroup);
	MacOSError::check(SecTrustedApplicationCreateApplicationGroup("InternetAccounts", NULL, &accountsGroup.aref()));
	CFArrayAppendValue(apps, accountsGroup);

	// now add "myself" as an ordinary application
	CFRef<SecTrustedApplicationRef> myself;
	MacOSError::check(SecTrustedApplicationCreateFromPath(NULL, &myself.aref()));
	CFArrayAppendValue(apps, myself);

	// now add the pre-cooked list of .Mac applications for systems that don't understand the group semantics
	if (CFRef<CFBundleRef> myBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.security")))
		if (CFRef<CFURLRef> url = CFBundleCopyResourceURL(myBundle,
				CFSTR("iToolsTrustedApps"), CFSTR("plist"), NULL)) {
			CFRef<CFDataRef> data;
			if (CFURLCreateDataAndPropertiesFromResource(NULL, url, &data.aref(), NULL, NULL, NULL))
				if (CFRef<CFArrayRef> list = 
					CFArrayRef(CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListImmutable, NULL))) {
					CFIndex size = CFArrayGetCount(list);
					for (CFIndex n = 0; n < size; n++) {
						CFStringRef path = (CFStringRef)CFArrayGetValueAtIndex(list, n);
						CFRef<SecTrustedApplicationRef> app;
						if (SecTrustedApplicationCreateFromPath(cfString(path).c_str(), &app.aref()) == errSecSuccess)
							CFArrayAppendValue(apps, app);
					}
				}
		}
    
	// form a SecAccess from this
	CFRef<SecAccessRef> access;
	MacOSError::check(SecAccessCreate(itemLabel, (CFArrayRef)apps, &access.aref()));
	
	// set up attribute vector (each attribute consists of {tag, length, pointer})
	SecKeychainAttribute attrs[] = {
		{ kSecLabelItemAttr, accountNameLength, (char *)accountName },	// use the account name as the label for display purposes [3787371]
		{ kSecAccountItemAttr, accountNameLength, (char *)accountName },
		{ kSecServiceItemAttr, (UInt32)strlen(serviceUTF8), (char *)serviceUTF8 }
	};
	SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };

	return SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass,
		&attributes,
		passwordLength,
		(const char *)passwordData,
		keychain,
		access,
		itemRef);
	
	END_SECAPI
}
