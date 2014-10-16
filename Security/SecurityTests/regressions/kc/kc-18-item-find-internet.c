#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <stdlib.h>
#include <unistd.h>

#include "testmore.h"
#include "testenv.h"
#include "testleaks.h"

void tests(int dont_skip)
{
	SecKeychainRef keychain;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"create keychain");
	SecKeychainItemRef item = NULL;
	ok_status(SecKeychainAddInternetPassword(keychain,
		19, "members.spamcop.net",
		0, NULL,
		5, "smith",
		0, NULL,
		80, kSecProtocolTypeHTTP,
		kSecAuthenticationTypeDefault,
		4, "test", &item), "add internet password");
	ok(item, "is item non NULL");
	SecKeychainItemRef oldItem = item;
	is_status(SecKeychainAddInternetPassword(keychain,
		19, "members.spamcop.net",
		0, NULL,
		5, "smith",
		0, NULL,
		80, kSecProtocolTypeHTTP,
		kSecAuthenticationTypeDefault,
		4, "test", &item), errSecDuplicateItem, "add internet password again");
	is((intptr_t)item, (intptr_t)oldItem, "item is unchanged");

	CFRelease(item);
	item = NULL;
	ok_status(SecKeychainFindInternetPassword(NULL,
		19, "members.spamcop.net",
		0, NULL,
		0, NULL,
		0, NULL,
		80, 
		kSecProtocolTypeHTTP,
		kSecAuthenticationTypeDefault,
		NULL, NULL,
		&item), "find internet password");

	SecItemClass itemClass = 0;
	SecKeychainAttribute attrs[] = 
	{
		{ kSecAccountItemAttr },
		{ kSecSecurityDomainItemAttr },
		{ kSecServerItemAttr }
	};
	SecKeychainAttributeList attrList =
	{
		sizeof(attrs) / sizeof(*attrs),
		attrs
	};
	ok_status(SecKeychainItemCopyContent(item, &itemClass, &attrList,
		NULL, NULL), "SecKeychainItemCopyContent");
	is(itemClass, kSecInternetPasswordItemClass, "is internet password?");
	is(attrs[0].length, 5, "account len");
	ok(!strncmp(attrs[0].data, "smith", 5), "account eq smith");
	is(attrs[1].length, 0, "security domain len");
	is(attrs[2].length, 19, "server len");
	ok(!strncmp(attrs[2].data, "members.spamcop.net", 19),
		"servername eq members.spamcop.net");
	ok_status(SecKeychainItemFreeContent(&attrList, NULL),
		"SecKeychainItemCopyContent");

	SecKeychainAttribute attrs2[] = 
	{
		{ kSecAccountItemAttr },
		{ kSecServiceItemAttr },
		{ kSecSecurityDomainItemAttr },
		{ kSecServerItemAttr }
	};
	SecKeychainAttributeList attrList2 =
	{
		(sizeof(attrs2) / sizeof(*attrs2)),
		attrs2
	};
	SKIP: {
		skip("<rdar://problem/3298182> 6L60 Malloc/free misuse in "
			"SecKeychainItemCopyContent()", 1, dont_skip);
		is_status(SecKeychainItemCopyContent(item, &itemClass, &attrList2,
			NULL, NULL), errSecNoSuchAttr, "SecKeychainItemCopyContent fails");
	}

	is(CFGetRetainCount(item), 1, "item retaincount is 1");
	is(CFGetRetainCount(keychain), 2, "keychain retaincount is 2");
	CFRelease(item);
	is(CFGetRetainCount(keychain), 1, "keychain retaincount is 1");
	ok_status(SecKeychainDelete(keychain), "delete keychain");
	CFRelease(keychain);
}

int main(int argc, char *const *argv)
{
	int dont_skip = argc > 1 && !strcmp(argv[1], "-s");

	plan_tests(21);

	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests(dont_skip);
	ok(tests_end(1), "cleanup");
	ok_leaks("no leaks");

	return 0;
}
