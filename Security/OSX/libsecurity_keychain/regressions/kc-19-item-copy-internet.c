#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <stdlib.h>
#include <unistd.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

static void tests()
{
	SecKeychainRef source, dest;
    source = createNewKeychain("source", "test");
    dest = createNewKeychain("dest", "test");
	SecKeychainItemRef original = NULL;
	ok_status(SecKeychainAddInternetPassword(source,
		19, "members.spamcop.net",
		0, NULL,
		5, "smith",
		0, NULL,
		80, kSecProtocolTypeHTTP,
		kSecAuthenticationTypeDefault,
		4, "test", &original), "add internet password");
	SecKeychainAttribute origAttrs[] = 
	{
		{ kSecCreationDateItemAttr },
		{ kSecModDateItemAttr }
	};
	SecKeychainAttributeList origAttrList =
	{
		sizeof(origAttrs) / sizeof(*origAttrs),
		origAttrs
	};
	ok_status(SecKeychainItemCopyContent(original, NULL, &origAttrList,
		NULL, NULL), "SecKeychainItemCopyContent");

	/* Must sleep 1 second to trigger mod date bug. */
	sleep(1);
	SecKeychainItemRef copy;
	ok_status(SecKeychainItemCreateCopy(original, dest, NULL, &copy),
		"copy item");

	SecKeychainAttribute copyAttrs[] = 
	{
		{ kSecCreationDateItemAttr },
		{ kSecModDateItemAttr }
	};
	SecKeychainAttributeList copyAttrList =
	{
		sizeof(copyAttrs) / sizeof(*copyAttrs),
		copyAttrs
	};
	ok_status(SecKeychainItemCopyContent(copy, NULL, &copyAttrList,
		NULL, NULL), "SecKeychainItemCopyContent");

	is(origAttrs[0].length, 16, "creation date length 16");
	is(origAttrs[1].length, 16, "mod date length 16");
	is(origAttrs[0].length, copyAttrs[0].length, "creation date length same");
	is(origAttrs[1].length, copyAttrs[1].length, "mod date length same");

    diag("original creation: %.*s copy creation: %.*s",
        (int)origAttrs[0].length, (const char *)origAttrs[0].data,
        (int)copyAttrs[0].length, (const char *)copyAttrs[0].data);
    ok(!memcmp(origAttrs[0].data, copyAttrs[0].data, origAttrs[0].length),
        "creation date same");

    diag("original mod: %.*s copy mod: %.*s",
        (int)origAttrs[1].length, (const char *)origAttrs[1].data,
        (int)copyAttrs[1].length, (const char *)copyAttrs[1].data);
    ok(!memcmp(origAttrs[1].data, copyAttrs[1].data, origAttrs[1].length),
        "mod date same");

	ok_status(SecKeychainItemFreeContent(&origAttrList, NULL),
		"SecKeychainItemCopyContent");
	ok_status(SecKeychainItemFreeContent(&copyAttrList, NULL),
		"SecKeychainItemCopyContent");

	is(CFGetRetainCount(original), 1, "original retaincount is 1");
	CFRelease(original);
	is(CFGetRetainCount(copy), 1, "copy retaincount is 1");
	CFRelease(copy);
	cmp_ok(CFGetRetainCount(source), >=, 1, "source keychain retaincount is 1");
	ok_status(SecKeychainDelete(source), "delete keychain source");
	CFRelease(source);
	ok_status(SecKeychainDelete(dest), "delete keychain dest");
	cmp_ok(CFGetRetainCount(dest), >=, 1, "dest retaincount is 1");
	CFRelease(dest);
}

int kc_19_item_copy_internet(int argc, char *const *argv)
{
	plan_tests(20);

	tests();

    deleteTestFiles();
	return 0;
}
