#include <Security/SecKeychainItem.h>
#include <Security/SecKeychain.h>
#include <CoreFoundation/CFRunLoop.h>
#include <assert.h>
#include <unistd.h>

#include "testmore.h"
#include "testenv.h"
#include "testleaks.h"

static char account[] = "account";
static char service[] = "service";
static char password[] = "password";

static void checkContent(SecKeychainItemRef itemRef)
{
	SecItemClass itemClass;

	SecKeychainAttribute attrs[] =
	{
		{ kSecLabelItemAttr, 0, NULL },
		{ kSecAccountItemAttr, 0, NULL },
		{ kSecServiceItemAttr, 0, NULL }
	};

	SecKeychainAttributeList attrList =
		{ sizeof(attrs) / sizeof(*attrs), attrs };
	UInt32 length;
	void *data;

#if 1
	ok_status(SecKeychainItemCopyContent(itemRef, &itemClass, &attrList,
		&length, &data), "get item data in callback");
	SKIP: {
		skip("length mismatch", 1,
			is(length, sizeof(password), "<rdar://problem/3867900> "
				"SecKeychainItemCopyContent() returns bad data on items "
				"from notifications"));

		ok(!memcmp(password, data, length), "password data matches.");
	}
#else
	if (length != sizeof(password) || memcmp(password, data, length))
	{
		fprintf(stderr, "password '%.*s' not same as '%.*s'\n",
			(int)sizeof(password), password,
			(int)length, (char *)data);
	}
#endif

	ok_status(SecKeychainItemFreeContent(&attrList, data),
		"free item data in callback");
}

static OSStatus callbackFunction(SecKeychainEvent keychainEvent,
	SecKeychainCallbackInfo *info, void *context)
{
	assert(keychainEvent == kSecAddEvent && context != NULL);
	assert(info != NULL);
	assert(info->item != NULL);

	checkContent(info->item);
	*((UInt32 *)context) = 1;

	ok_status(SecKeychainItemDelete(info->item), "delete item");
	return 0;
}

int
main(int argc, char *const *argv)
{
	plan_tests(6);

	ok(tests_begin(argc, argv), "setup");

	UInt32 didGetNotification = 0;
	ok_status(SecKeychainAddCallback(callbackFunction, kSecAddEventMask,
		&didGetNotification), "add callback");

	SecKeychainRef keychain;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
			"create keychain");
	SecKeychainItemRef itemRef;
	ok_status(SecKeychainAddGenericPassword(keychain,
		sizeof(account), account,
		sizeof(service), service,
		sizeof(password), password,
		&itemRef),
		"add generic password, release and wait for callback");
	//checkContent(itemRef);
	CFRelease(itemRef);
	CFRelease(keychain);

	if (argc > 1 && !strcmp(argv[1], "-l")) {
		printf("pid: %d\n", getpid());
		sleep(100);
	}
	ok(tests_end(1), "cleanup");
	ok_leaks("leaks");

	return 0;
}
