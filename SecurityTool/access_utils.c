/*
 * Copyright (c) 2008-2009 Apple Inc. All Rights Reserved.
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
 *
 *  access_utils.c
 */

#include "access_utils.h"
#include "security.h"
#include <stdio.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecAccess.h>
#include <Security/SecACL.h>

// create_access
//
// This function creates a SecAccessRef with an array of trusted applications.
//
int
create_access(const char *accessName, Boolean allowAny, CFArrayRef trustedApps, SecAccessRef *access)
{
	int result = 0;
	CFArrayRef appList = NULL;
	CFArrayRef aclList = NULL;
	CFStringRef description = NULL;
	const char *descriptionLabel = (accessName) ? accessName : "<unlabeled key>";
	CFStringRef promptDescription = NULL;
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector;
	SecACLRef aclRef;
	OSStatus status;
	
	if (accessName) {
		description = CFStringCreateWithCString(NULL, descriptionLabel, kCFStringEncodingUTF8);
	}

	status = SecAccessCreate(description, trustedApps, access);
	if (status)
	{
		sec_perror("SecAccessCreate", status);
		result = 1;
		goto cleanup;
	}
	
	// get the access control list for decryption operations (this controls access to an item's data)
	status = SecAccessCopySelectedACLList(*access, CSSM_ACL_AUTHORIZATION_DECRYPT, &aclList);
	if (status)
	{
		sec_perror("SecAccessCopySelectedACLList", status);
		result = 1;
		goto cleanup;
	}

	// get the first entry in the access control list
	aclRef = (SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
	status = SecACLCopySimpleContents(aclRef, &appList, &promptDescription, &promptSelector);
	if (status)
	{
		sec_perror("SecACLCopySimpleContents", status);
		result = 1;
		goto cleanup;
	}

	if (allowAny) // "allow all applications to access this item"
	{
		// change the decryption ACL to not require the passphrase, and have a nil application list.
		promptSelector.flags &= ~CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE;
		status = SecACLSetSimpleContents(aclRef, NULL, promptDescription, &promptSelector);
	}
	else // "allow access by these applications"
	{
		// modify the application list
		status = SecACLSetSimpleContents(aclRef, trustedApps, promptDescription, &promptSelector);
	}
	if (status)
	{
		sec_perror("SecACLSetSimpleContents", status);
		result = 1;
		goto cleanup;
	}

cleanup:
	if (description)
		CFRelease(description);
	if (promptDescription)
		CFRelease(promptDescription);
	if (appList)
		CFRelease(appList);
	if (aclList)
		CFRelease(aclList);

	return result;
}

// merge_access
//
// This function merges the contents of otherAccess into access.
// Simple ACL contents are assumed, and only the standard ACL
// for decryption operations is currently processed.
//
int
merge_access(SecAccessRef access, SecAccessRef otherAccess)
{
	OSStatus status;
	CFArrayRef aclList, newAclList;

	// get existing access control list for decryption operations (this controls access to an item's data)
	status = SecAccessCopySelectedACLList(access, CSSM_ACL_AUTHORIZATION_DECRYPT, &aclList);
	if (status) {
		return status;
	}
	// get desired access control list for decryption operations
	status = SecAccessCopySelectedACLList(otherAccess, CSSM_ACL_AUTHORIZATION_DECRYPT, &newAclList);
	if (status) {
		newAclList = nil;
	} else {
		SecACLRef aclRef=(SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
		SecACLRef newAclRef=(SecACLRef)CFArrayGetValueAtIndex(newAclList, 0);
		CFArrayRef appList=nil;
		CFArrayRef newAppList=nil;
		CFMutableArrayRef mergedAppList = nil;
		CFStringRef promptDescription=nil;
		CFStringRef newPromptDescription=nil;
		CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector;
		CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR newPromptSelector;
		
		status = SecACLCopySimpleContents(aclRef, &appList, &promptDescription, &promptSelector);
		if (!status) {
			status = SecACLCopySimpleContents(newAclRef, &newAppList, &newPromptDescription, &newPromptSelector);
		}
		if (!status) {
			if (appList) {
				mergedAppList = CFArrayCreateMutableCopy(NULL, 0, appList);
			}
			if (newAppList) {
				if (mergedAppList) {
					CFArrayAppendArray(mergedAppList, newAppList, CFRangeMake(0, CFArrayGetCount(newAppList)));
				} else {
					mergedAppList = CFArrayCreateMutableCopy(NULL, 0, newAppList);
				}
			}
			promptSelector.flags = newPromptSelector.flags;
			status = SecACLSetSimpleContents(aclRef, mergedAppList, newPromptDescription, &newPromptSelector);
		}

		if (appList) CFRelease(appList);
		if (newAppList) CFRelease(newAppList);
		if (mergedAppList) CFRelease(mergedAppList);
		if (promptDescription) CFRelease(promptDescription);
		if (newPromptDescription) CFRelease(newPromptDescription);
	}
	if (aclList) CFRelease(aclList);
	if (newAclList) CFRelease(newAclList);
	
	return status;
}

// modify_access
//
// This function updates the access for an existing item.
// The provided access is merged with the item's existing access.
//
int
modify_access(SecKeychainItemRef itemRef, SecAccessRef access)
{
	OSStatus status;
	SecAccessRef curAccess = NULL;
	// for historical reasons, we have to modify the item's existing access reference
	// (setting the item's access to a freshly created SecAccessRef instance doesn't behave as expected)
	status = SecKeychainItemCopyAccess(itemRef, &curAccess);
	if (status) {
		sec_error("SecKeychainItemCopyAccess: %s", sec_errstr(status));
	} else {
		status = merge_access(curAccess, access); // make changes to the existing access reference
		if (!status) {
			status = SecKeychainItemSetAccess(itemRef, curAccess); // will prompt!
			if (status) {
				sec_error("SecKeychainItemSetAccess: %s", sec_errstr(status));
			}
		}
	}
	if (curAccess) {
		CFRelease(curAccess);
	}
	return status;
}
