/*
 *  Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
#include <Security/SecTrustedApplication.h>
#include <Security/SecACL.h>
#include <CoreFoundation/CoreFoundation.h>


OSStatus SecKeychainAddIToolsPassword(SecKeychainRef keychain, UInt32 accountNameLength, const char *accountName,
    UInt32 passwordLength, const void *passwordData, SecKeychainItemRef *itemRef)
{
    OSStatus err;
    SecKeychainItemRef item = nil;
    const char *serviceUTF8 = "iTools";
	CFStringRef itemLabel = nil;
	const int allowAny = 0;
	
	// create the initial ACL label string (use the account name, not "iTools") [3787371]
	itemLabel = CFStringCreateWithBytes(kCFAllocatorDefault, accountName, accountNameLength, kCFStringEncodingUTF8, FALSE);
    
	// create initial access control settings for the item
	SecAccessRef access = NULL;
	err = SecAccessCreateWithTrustedApplications(CFSTR("/System/Library/Frameworks/Security.framework/Resources/iToolsTrustedApps.plist"), itemLabel, allowAny, &access);
	
	// below is the lower-layer equivalent to the SecKeychainAddGenericPassword() function;
	// it does the same thing (except specify the access controls)
	
	// set up attribute vector (each attribute consists of {tag, length, pointer})
	SecKeychainAttribute attrs[] =
    {
		{ kSecLabelItemAttr, accountNameLength, (char *)accountName },	// use the account name as the label for display purposes [3787371]
		{ kSecAccountItemAttr, accountNameLength, (char *)accountName },
		{ kSecServiceItemAttr, strlen(serviceUTF8), (char *)serviceUTF8 }
	};
	SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };

	err = SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass,
		&attributes,
		passwordLength,
		(const char *)passwordData,
		keychain,
		access,
		&item);
	
	if (access)
        CFRelease(access);
	if (item)	
        CFRelease(item);
    return noErr;
}

