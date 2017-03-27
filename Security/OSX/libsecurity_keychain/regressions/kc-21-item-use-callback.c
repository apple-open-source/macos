#include <Security/SecKeychainItem.h>
#include <Security/SecKeychain.h>
#include <CoreFoundation/CFRunLoop.h>
#include <unistd.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

/*
 Note: to force a failure, run this as root:
    chmod o-r /var/db/mds/messages/se_SecurityMessages
 Restore with
    chmod o+r /var/db/mds/messages/se_SecurityMessages
 */

static char account[] = "account";
static char service[] = "service";
static char password[] = "password";
static bool callbackCalled = false;

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

	ok_status(SecKeychainItemCopyContent(itemRef, &itemClass, &attrList,
		&length, &data), "get item data in callback");
    is(length, sizeof(password), "<rdar://problem/3867900> "
        "SecKeychainItemCopyContent() returns bad data on items "
        "from notifications");
    ok(data, "received data from item");

    ok(!memcmp(password, data, length), "password data matches.");

	ok_status(SecKeychainItemFreeContent(&attrList, data),
		"free item data in callback");
}

static OSStatus callbackFunction(SecKeychainEvent keychainEvent,
	SecKeychainCallbackInfo *info, void *context)
{
    is(keychainEvent, kSecAddEvent, "Got an incorrect keychain event");
    ok(context != NULL, "context is null");
    ok(info != NULL, "info is NULL");
    ok(info->item != NULL, "info-<item is NULL");

	checkContent(info->item);
	*((UInt32 *)context) = 1;

	ok_status(SecKeychainItemDelete(info->item), "delete item");

    // We processed an item, quit the run loop
    callbackCalled = true;
    CFRunLoopStop(CFRunLoopGetCurrent());
	return 0;
}

int
kc_21_item_use_callback(int argc, char *const *argv)
{
	plan_tests(16);

    // Run the CFRunLoop to clear out existing notifications
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);

	UInt32 didGetNotification = 0;
	ok_status(SecKeychainAddCallback(callbackFunction, kSecAddEventMask,
		&didGetNotification), "add callback");

    SecKeychainRef keychain = createNewKeychain("test", "test");
	SecKeychainItemRef itemRef;
	ok_status(SecKeychainAddGenericPassword(keychain,
		sizeof(account), account,
		sizeof(service), service,
		sizeof(password), password,
		&itemRef),
		"add generic password, release and wait for callback");

    // Run the CFRunLoop to process events (and call our callback)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10.0, false);
    is(callbackCalled, true, "Keychain callback function was not called or did not finish");

	CFRelease(itemRef);

    ok_status(SecKeychainRemoveCallback(callbackFunction), "Remove callback");

    ok_status(SecKeychainDelete(keychain), "%s: SecKeychainDelete", testName);
	CFRelease(keychain);

    deleteTestFiles();
	return 0;
}
