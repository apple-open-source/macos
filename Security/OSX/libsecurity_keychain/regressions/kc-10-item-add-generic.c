#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <stdlib.h>
#include <unistd.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

static void tests(void)
{
    SecKeychainRef keychain = getPopulatedTestKeychain();
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
    eq_string(data, "test", "Item data is wrong");
    ok_status(SecKeychainItemFreeContent(&attrList, data), "SecKeychainItemCopyContent");

	is(CFGetRetainCount(item), 1, "item retaincount is 1");
	cmp_ok(CFGetRetainCount(keychain), >=, 2, "keychain retaincount is at least 2");
	CFRelease(item);
	cmp_ok(CFGetRetainCount(keychain), >=, 1, "keychain retaincount is at least 1");
	ok_status(SecKeychainDelete(keychain), "delete keychain");
	CFRelease(keychain);
}

int kc_10_item_add_generic(int argc, char *const *argv)
{
    initializeKeychainTests("kc-10-item-add-generic");
	plan_tests(13);

	tests();

    deleteTestFiles();
	return 0;
}
