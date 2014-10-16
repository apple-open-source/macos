#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <stdlib.h>
#include <unistd.h>

#include "testenv.h"
#include "testleaks.h"
#include "testmore.h"
#include "testsecevent.h"

void tests(void)
{
	SecKeychainRef keychain = NULL;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"create keychain");
	ok_status(test_sec_event_register(kSecEveryEventMask),
		"register for all events");
	int item_num;
	int item_count = 9;
	for (item_num = 0; item_num < item_count; ++item_num)
	{
		char account[64];
		sprintf(account, "account-%d", item_num);
		ok_status(SecKeychainAddGenericPassword(keychain, 7, "service",
			strlen(account), account, 4, "test", NULL),
			"add generic password");
	}
	SecKeychainAttribute attrs[] =
	{ { kSecAccountItemAttr } };
	SecKeychainAttributeList attrList =
	{ sizeof(attrs) / sizeof(*attrs), attrs };

	for (item_num = 0; item_num < item_count - 2; ++item_num)
	{
		char account[64];
		sprintf(account, "account-%d", item_num);
		SecKeychainItemRef item = NULL;
		is_sec_event(kSecAddEvent, NULL, &item, NULL, "got add event");

		SKIP: {
			skip("no item", 3, item != NULL);

			ok_status(SecKeychainItemCopyContent(item, NULL, &attrList, NULL,
				NULL), "get content");
			
			eq_stringn(account, strlen(account), attrs[0].data, attrs[0].length,
				"account name in notification matches");
			ok_status(SecKeychainItemFreeContent(&attrList, NULL),
				"free content");
		}
	}

	for (; item_num < item_count; ++item_num)
	{
		char account[64];
		sprintf(account, "account-%d", item_num);
		SecKeychainItemRef item = NULL;
		is_sec_event(kSecAddEvent, NULL, &item, NULL, "got add event");

		SKIP: {
			skip("no item", 3, item != NULL);

			ok_status(SecKeychainItemCopyContent(item, NULL, &attrList, NULL,
				NULL), "get content");
			eq_stringn(account, strlen(account), attrs[0].data, attrs[0].length,
				"account name in notification matches");
			ok_status(SecKeychainItemFreeContent(&attrList, NULL),
				"free content");
		}
	}
	
	ok(tests_end(1), "cleanup");
}

int main(int argc, char *const *argv)
{
#ifdef DEBUG
	plan_tests(49);

	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests();
	ok_leaks("no leaks");
#endif
	plan_tests(1);
	ok_leaks("no leaks");
	
	return 0;
}
