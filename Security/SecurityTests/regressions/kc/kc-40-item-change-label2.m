//
//  keychain_test.m
//	Keychain item access control example
//
//  Created by Perry Kiehtreiber on Wed Jun 19 2002
//	Modified by Ken McLeod, Mon Apr 21 2003 -- added "always allow" ACL support
//							Wed Jul 28 2004 -- add test code for persistent ref SPI
//							Mon Aug 02 2004 -- add test code to change label attributes
//
//	To build and run this example:
//		cc -framework Security -framework Foundation keychain_test.m ; ./a.out
//
//  Copyright (c) 2003-2005,2007 Apple Inc. All Rights Reserved.
//

#define TEST_PERSISTENT_REFS	0
#define USE_SYSTEM_KEYCHAIN		0


#import <Cocoa/Cocoa.h>

#include <Security/SecBase.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecAccess.h>
#include <Security/SecTrustedApplication.h>
#include <Security/SecACL.h>

#import "testmore.h"
#import "testenv.h"
#import "testleaks.h"

void renameItemViaModifyAttributesAndData(SecKeychainItemRef item)
{
	const char *labelSuffix = " [MAD]";

    // get the item's label attribute (allocated for us by
    // SecKeychainItemCopyAttributesAndData, must free later...)
    UInt32 itemTags[] = { kSecLabelItemAttr };
    UInt32 itemFmts[] = { CSSM_DB_ATTRIBUTE_FORMAT_STRING };
    SecKeychainAttributeInfo attrInfo = { 1, itemTags, itemFmts };
    SecKeychainAttributeList *attrList = NULL;
    SecItemClass itemClass;

    ok_status(SecKeychainItemCopyAttributesAndData(item, &attrInfo, &itemClass, &attrList, NULL, NULL),
        "get label attribute");
    
    ok(attrList && attrList->count == 1, "check that exactly one attribute was returned");

    // malloc enough space to hold our new label string
    // (length = old label string + suffix string + terminating NULL)
    CFIndex newLen = attrList->attr[0].length + strlen(labelSuffix);
    char *p = (char*) malloc(newLen);
    memcpy(p, attrList->attr[0].data, attrList->attr[0].length);
    memcpy(p + attrList->attr[0].length, labelSuffix, strlen(labelSuffix));

    // set up the attribute we want to change with its new value
    SecKeychainAttribute newAttrs[] = { { kSecLabelItemAttr, newLen, p } };
    SecKeychainAttributeList newAttrList =
    { sizeof(newAttrs) / sizeof(newAttrs[0]), newAttrs };

    // modify the attribute
    ok_status(SecKeychainItemModifyAttributesAndData(item, &newAttrList, 0, NULL),
        "SecKeychainItemModifyAttributesAndData");

    // free the memory we allocated for the new label string
    free(p);

    // free the attrList which was allocated by SecKeychainItemCopyAttributesAndData
    ok_status(SecKeychainItemFreeAttributesAndData(attrList, NULL),
        "SecKeychainItemFreeAttributesAndData");
}

void renameItemViaModifyContent(SecKeychainItemRef item)
{
	const char *labelSuffix = " [MC]";

    // get the item's label attribute (allocated for us by
    // SecKeychainItemCopyContent, must free later...)
    SecKeychainAttribute itemAttrs[] = { { kSecLabelItemAttr, 0, NULL } };
    SecKeychainAttributeList itemAttrList =
    { sizeof(itemAttrs) / sizeof(itemAttrs[0]), itemAttrs };

    ok_status(SecKeychainItemCopyContent(item, NULL, &itemAttrList,
        NULL, NULL), "get label");

    ok(itemAttrs[0].data != NULL, "check that attribute data was returned");

    // malloc enough space to hold our new label string
    // (length = old label string + suffix string + terminating NULL)
    CFIndex newLen = itemAttrs[0].length + strlen(labelSuffix);
    char *p = (char*) malloc(newLen);
    memcpy(p, itemAttrs[0].data, itemAttrs[0].length);
    memcpy(p + itemAttrs[0].length, labelSuffix, strlen(labelSuffix));

    // set up the attribute we want to change with its new value
    SecKeychainAttribute newAttrs[] = { { kSecLabelItemAttr, newLen, p } };
    SecKeychainAttributeList newAttrList =
    { sizeof(newAttrs) / sizeof(newAttrs[0]), newAttrs };

    // modify the attribute
    ok_status(SecKeychainItemModifyContent(item, &newAttrList,
        0, NULL), "modify label");

    // free the memory we allocated for the new label string
    free(p);

    // free the memory in the itemAttrList structure which was
    // allocated by SecKeychainItemCopyContent
    ok_status(SecKeychainItemFreeContent(&itemAttrList, NULL),
        "SecKeychainItemFreeContent");
}

void testRenameItemLabels(SecKeychainRef keychain)
{
    // Find each generic password item in the given keychain whose label
    // is "sample service", and modify the existing label attribute
    // by adding a " [label]" suffix.

	const char *searchString = "sample service";

	SecKeychainSearchRef searchRef = nil;
	SecKeychainAttribute sAttrs[] =
	{ { kSecServiceItemAttr, strlen(searchString), (char*)searchString } };
	SecKeychainAttributeList sAttrList =
	{ sizeof(sAttrs) / sizeof(sAttrs[0]), sAttrs };
	ok_status(SecKeychainSearchCreateFromAttributes(keychain,
		kSecGenericPasswordItemClass, &sAttrList, &searchRef),
		"SecKeychainSearchCreateFromAttributes");

	SecKeychainItemRef foundItemRef = NULL;
	int count;
	for (count = 0; count < 2; ++count)
	{
		ok_status(SecKeychainSearchCopyNext(searchRef, &foundItemRef),
			"SecKeychainSearchCopyNext");

        renameItemViaModifyAttributesAndData(foundItemRef); // 4
        renameItemViaModifyContent(foundItemRef); // 4

		if (foundItemRef)
			CFRelease(foundItemRef);
	}

	is_status(SecKeychainSearchCopyNext(searchRef, &foundItemRef),
		errSecItemNotFound, "SecKeychainSearchCopyNext at end");

	if (searchRef) CFRelease(searchRef);
}

SecAccessRef createAccess(NSString *accessLabel, BOOL allowAny)
{
	SecAccessRef access=nil;
	NSArray *trustedApplications=nil;
	
	if (!allowAny) // use default access ("confirm access")
	{
		// make an exception list of applications you want to trust, which
		// are allowed to access the item without requiring user confirmation
		SecTrustedApplicationRef myself, someOther;
		ok_status(SecTrustedApplicationCreateFromPath(NULL, &myself),
			"create trusted app for self");
		ok_status(SecTrustedApplicationCreateFromPath("/Applications/Mail.app",
			&someOther), "create trusted app for Mail.app");
		trustedApplications = [NSArray arrayWithObjects:(id)myself,
			(id)someOther, nil];
		CFRelease(myself);
		CFRelease(someOther);
	}

	ok_status(SecAccessCreate((CFStringRef)accessLabel,
		(CFArrayRef)trustedApplications, &access), "SecAccessCreate");

	if (allowAny)
	{
		// change access to be wide-open for decryption ("always allow access")
		// get the access control list for decryption operations
		CFArrayRef aclList=nil;
		ok_status(SecAccessCopySelectedACLList(access,
			CSSM_ACL_AUTHORIZATION_DECRYPT, &aclList),
			"SecAccessCopySelectedACLList");
		
		// get the first entry in the access control list
		SecACLRef aclRef=(SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
		CFArrayRef appList=nil;
		CFStringRef promptDescription=nil;
		CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector;
		ok_status(SecACLCopySimpleContents(aclRef, &appList,
			&promptDescription, &promptSelector), "SecACLCopySimpleContents");

		// modify the default ACL to not require the passphrase, and have a
		// nil application list
		promptSelector.flags &= ~CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE;
		ok_status(SecACLSetSimpleContents(aclRef, NULL, promptDescription,
			&promptSelector), "SecACLSetSimpleContents");

		if (appList) CFRelease(appList);
		if (promptDescription) CFRelease(promptDescription);
		if (aclList) CFRelease(aclList);
	}

	return access;
}

void addApplicationPassword(SecKeychainRef keychain, NSString *password,
	NSString *account, NSString *service, BOOL allowAny, SecKeychainItemRef *outItem)
{
    SecKeychainItemRef item = nil;
    const char *serviceUTF8 = [service UTF8String];
    const char *accountUTF8 = [account UTF8String];
    const char *passwordUTF8 = [password UTF8String];
	// use the service string as the name of this item for display purposes
	NSString *itemLabel = service;
	const char *itemLabelUTF8 = [itemLabel UTF8String];

#if USE_SYSTEM_KEYCHAIN
	const char *sysKeychainPath = "/Library/Keychains/System.keychain";	
	status = SecKeychainOpen(sysKeychainPath, &keychain);
	if (status) { NSLog(@"SecKeychainOpen returned %d", status); return; }
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	if (status) { NSLog(@"SecKeychainSetPreferenceDomain returned %d", status); return; }
#endif

	// create initial access control settings for the item
	SecAccessRef access = createAccess(itemLabel, allowAny);

    // Below is the lower-layer equivalent to the
    // SecKeychainAddGenericPassword() function; it does the same thing
    // (except specify the access controls) set up attribute vector
    // (each attribute consists of {tag, length, pointer})
	SecKeychainAttribute attrs[] = {
		{ kSecLabelItemAttr, strlen(itemLabelUTF8), (char *)itemLabelUTF8 },
		{ kSecAccountItemAttr, strlen(accountUTF8), (char *)accountUTF8 },
		{ kSecServiceItemAttr, strlen(serviceUTF8), (char *)serviceUTF8 }
	};
	SecKeychainAttributeList attributes =
	{ sizeof(attrs) / sizeof(attrs[0]), attrs };

	ok_status(SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass,
		&attributes, strlen(passwordUTF8), passwordUTF8, keychain, access,
		&item), "SecKeychainItemCreateFromContent");

	if (access) CFRelease(access);

    if (outItem) {
        *outItem = item;
    } else if (item) {
        CFRelease(item);
    }
}

// 1
void addInternetPassword(SecKeychainRef keychain, NSString *password,
	NSString *account, NSString *server, NSString *path,
	SecProtocolType protocol, int port, BOOL allowAny, SecKeychainItemRef *outItem)
{
    SecKeychainItemRef item = nil;
	const char *pathUTF8 = [path UTF8String];
    const char *serverUTF8 = [server UTF8String];
    const char *accountUTF8 = [account UTF8String];
    const char *passwordUTF8 = [password UTF8String];
	// use the server string as the name of this item for display purposes
	NSString *itemLabel = server;
	const char *itemLabelUTF8 = [itemLabel UTF8String];

#if USE_SYSTEM_KEYCHAIN
	const char *sysKeychainPath = "/Library/Keychains/System.keychain";
	status = SecKeychainOpen(sysKeychainPath, &keychain);
	if (status) { NSLog(@"SecKeychainOpen returned %d", status); return 1; }
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	if (status) { NSLog(@"SecKeychainSetPreferenceDomain returned %d", status); return 1; }
#endif

	// create initial access control settings for the item
	SecAccessRef access = createAccess(itemLabel, allowAny);

    // below is the lower-layer equivalent to the
    // SecKeychainAddInternetPassword() function; it does the same
    // thing (except specify the access controls) set up attribute
    // vector (each attribute consists of {tag, length, pointer})
	SecKeychainAttribute attrs[] = {
		{ kSecLabelItemAttr, strlen(itemLabelUTF8), (char *)itemLabelUTF8 },
		{ kSecAccountItemAttr, strlen(accountUTF8), (char *)accountUTF8 },
		{ kSecServerItemAttr, strlen(serverUTF8), (char *)serverUTF8 },
		{ kSecPortItemAttr, sizeof(int), (int *)&port },
		{ kSecProtocolItemAttr, sizeof(SecProtocolType),
			(SecProtocolType *)&protocol },
		{ kSecPathItemAttr, strlen(pathUTF8), (char *)pathUTF8 }
	};
	SecKeychainAttributeList attributes =
	{ sizeof(attrs) / sizeof(attrs[0]), attrs };

	ok_status(SecKeychainItemCreateFromContent(kSecInternetPasswordItemClass,
		&attributes, strlen(passwordUTF8), passwordUTF8, keychain, access,
		&item), "SecKeychainItemCreateFromContent");

	if (access) CFRelease(access);

    if (outItem) {
        *outItem = item;
    } else if (item) {
        CFRelease(item);
    }
}

void tests(void)
{
	SecKeychainRef keychain = NULL;
	ok_status(SecKeychainCreate("login.keychain", 4, "test", NO, NULL,
		&keychain), "SecKeychainCreate");

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	// add some example passwords to the keychain
	addApplicationPassword(keychain, @"sample password",
		@"sample account", @"sample service", NO, NULL);
	addApplicationPassword(keychain, @"sample password",
		@"different account", @"sample service", NO, NULL);
	addApplicationPassword(keychain, @"sample password",
		@"sample account", @"sample unprotected service", YES, NULL);
	addInternetPassword(keychain, @"sample password",
		@"sample account", @"samplehost.apple.com",
		@"cgi-bin/bogus/testpath", kSecProtocolTypeHTTP, 8080, NO, NULL);

	// test searching and changing item label attributes
	testRenameItemLabels(keychain);

	[pool release];

	SKIP: {
		skip("no keychain", 1, keychain);
	    ok_status(SecKeychainDelete(keychain), "SecKeychainDelete");
	    CFRelease(keychain);
	}

	tests_end(1);
}

int main(int argc, char * const *argv)
{
	plan_tests(40);
	tests_begin(argc, argv);

	tests();

	ok_leaks("leaks");

	return 0;
}
