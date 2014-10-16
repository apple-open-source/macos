/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


//
//  si-72-syncableitems.c
//  regressions
//
//
//
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#include <Security/SecInternal.h>
#include <utilities/array_size.h>

#include "Security_regressions.h"

static void tests(void)
{
    CFUUIDRef uuid = CFUUIDCreate(0);
	const CFStringRef uuidstr = CFUUIDCreateString(0, uuid);
	const CFStringRef account = CFStringCreateWithFormat(NULL, NULL, CFSTR("Test Account %@"), uuidstr);
	const CFStringRef service = CFSTR("Test Service");
	const CFStringRef label = CFSTR("Test Synchronizable Item");
	const CFStringRef comment = CFSTR("Test Comment");
	const CFDataRef passwordData = CFDataCreate(NULL, (const UInt8*)"Test", (CFIndex)5);

    CFReleaseSafe(uuid);
    CFReleaseSafe(uuidstr);
    
	CFDictionaryRef query = NULL;
	CFDictionaryRef attrs = NULL;
	CFDictionaryRef result = NULL;

	/* Test adding a synchronizable item */
	{
		const void *keys[] = {
			kSecClass, kSecAttrLabel, kSecAttrComment, kSecAttrAccount, kSecAttrService,
			kSecAttrSynchronizable,
			kSecValueData, kSecReturnAttributes };
		const void *values[] = {
			kSecClassGenericPassword, label, comment, account, service,
			kCFBooleanTrue,
			passwordData, kCFBooleanTrue };

		attrs = CFDictionaryCreate(NULL, keys, values,
			array_size(keys),
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

		is_status(SecItemAdd(attrs, (CFTypeRef *)&result),
				errSecSuccess, "SecItemAdd sync=true");

		CFReleaseSafe(attrs);
		CFReleaseNull(result);
	}

	/* Test finding the synchronizable item we just added, using sync=true */
	{
		const void *keys[] = {
			kSecClass, // class attribute is required
			kSecAttrAccount, kSecAttrService, // we'll look up by account and service, which determine uniqueness
			kSecAttrSynchronizable, // we want to get synchronizable results
			kSecReturnAttributes };
		const void *values[] = {
			kSecClassGenericPassword,
			account, service,
			kCFBooleanTrue, // only return synchronizable results
			kCFBooleanTrue };

		query = CFDictionaryCreate(NULL, keys, values,
			array_size(keys),
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

		is_status(SecItemCopyMatching(query, (CFTypeRef *)&result),
			errSecSuccess, "SecItemCopyMatching sync=true");

		CFReleaseSafe(query);
		CFReleaseNull(result);
	}

	/* Test finding the synchronizable item we just added, using sync=any */
	{
		const void *keys[] = {
			kSecClass, // class attribute is required
			kSecAttrAccount, kSecAttrService, // we'll look up by account and service, which determine uniqueness
			kSecAttrSynchronizable, // we want to get synchronizable results
			kSecReturnAttributes };
		const void *values[] = {
			kSecClassGenericPassword,
			account, service,
			kSecAttrSynchronizableAny, // return any match, regardless of whether it is synchronizable
			kCFBooleanTrue };

		query = CFDictionaryCreate(NULL, keys, values,
			array_size(keys),
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

		is_status(SecItemCopyMatching(query, (CFTypeRef *)&result),
			errSecSuccess, "SecItemCopyMatching sync=any");

		CFReleaseSafe(query);
		CFReleaseNull(result);
	}

	/* Test updating the synchronizable item */
	{
		const void *keys[] = {
			kSecClass, // class attribute is required
			kSecAttrAccount, kSecAttrService, // we'll look up by account and service, which determine uniqueness
			kSecAttrSynchronizable }; // we want synchronizable results
		const void *values[] = {
			kSecClassGenericPassword,
			account, service,
			kCFBooleanTrue }; // we only want to find the synchronizable item here, not a non-synchronizable one

		query = CFDictionaryCreate(NULL, keys, values,
			array_size(keys),
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

		const void *update_keys[] = { kSecAttrComment };
		const void *update_values[] = { CFSTR("Updated Comment") };
		attrs = CFDictionaryCreate(NULL, update_keys, update_values,
			array_size(update_keys),
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

		is_status(SecItemUpdate(query, attrs),
			errSecSuccess, "SecItemUpdate sync=true");

		CFReleaseSafe(query);
		CFReleaseSafe(attrs);
	}

	/* Test finding the updated item with its new attribute */
	{
		const void *keys[] = {
			kSecClass, // class attribute is required
			kSecAttrAccount, kSecAttrService, // we'll look up by account and service, which determine uniqueness
			kSecAttrComment, // also search on the attr we just changed, so we know we've found the updated item
			kSecAttrSynchronizable }; // we want synchronizable results
		const void *values[] = {
			kSecClassGenericPassword,
			account, service,
			CFSTR("Updated Comment"),
			kCFBooleanTrue }; // we only want to find the synchronizable item here, not a non-synchronizable one

		query = CFDictionaryCreate(NULL, keys, values,
			array_size(keys),
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

		is_status(SecItemCopyMatching(query, (CFTypeRef *)&result),
			errSecSuccess, "SecItemCopyMatching post-update");

		CFReleaseSafe(result);
		// re-use query in next test...
	}

	/* Test deleting the item */
	{
		is_status(SecItemDelete(query), errSecSuccess, "SecItemDelete sync=true");

		CFReleaseSafe(query);
	}
}

int si_72_syncableitems(int argc, char * const *argv)
{
	plan_tests(6);
	tests();

	return 0;
}
