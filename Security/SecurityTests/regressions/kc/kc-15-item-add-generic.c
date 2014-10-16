#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <stdlib.h>
#include <unistd.h>

#include "testmore.h"
#include "testenv.h"
#include "testleaks.h"

void tests(void)
{
	SecKeychainRef keychain;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"create keychain");
	SecKeychainItemRef item = NULL;
	ok_status(SecKeychainAddGenericPassword(keychain, 7, "service", 7,
		"account", 4, "test", &item), "add generic password");
	ok(item, "is item non NULL");
	SecKeychainItemRef oldItem = item;
	is_status(SecKeychainAddGenericPassword(keychain, 7, "service", 7,
		"account", 4, "test", &oldItem),
		errSecDuplicateItem, "add generic password again");
	is((intptr_t)item, (intptr_t)oldItem, "item is unchanged");

	SecItemClass itemClass = 0;
	SecKeychainAttribute attrs[] = 
	{
		{ kSecAccountItemAttr },
		{ kSecServiceItemAttr }
	};
	SecKeychainAttributeList attrList = { sizeof(attrs) / sizeof(*attrs), attrs };
	UInt32 length = 0;
	void *data = NULL;
	ok_status(SecKeychainItemCopyContent(item, &itemClass, &attrList, &length, &data), "SecKeychainItemCopyContent");
	ok_status(SecKeychainItemFreeContent(&attrList, data), "SecKeychainItemCopyContent");

	is(CFGetRetainCount(item), 1, "item retaincount is 1");
	is(CFGetRetainCount(keychain), 2, "keychain retaincount is 2");
	CFRelease(item);
	is(CFGetRetainCount(keychain), 1, "keychain retaincount is 1");
	ok_status(SecKeychainDelete(keychain), "delete keychain");
	CFRelease(keychain);
}

int main(int argc, char *const *argv)
{
	plan_tests(13);

	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests();
	ok(tests_end(1), "cleanup");
	ok_leaks("no leaks");

	return 0;
}
