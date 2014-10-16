#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <stdlib.h>
#include <unistd.h>

#include "testenv.h"
#include "testleaks.h"
#include "testmore.h"
#include "testsecevent.h"

static char account[] = "account";
static char service[] = "service";
static char password[] = "password";

void tests(int dont_skip)
{
	SecKeychainRef keychain = NULL;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"create keychain");
	ok_status(test_sec_event_register(kSecEveryEventMask),
		"register for all events");
	SecKeychainItemRef itemRef;
	ok_status(SecKeychainAddGenericPassword(keychain,
		sizeof(account), account,
		sizeof(service), service,
		sizeof(password), password,
		&itemRef),
		"add generic password, wait for callback");
	SecKeychainRef eventKeychain = NULL;
	SecKeychainItemRef eventItem = NULL;
	is_sec_event(kSecAddEvent, &eventKeychain, &eventItem, NULL,
		"add event");
    is(eventItem, itemRef, "add event item matches");
    is(eventKeychain, keychain, "add event keychain matches");
	CFRelease(eventKeychain);
	eventKeychain = NULL;
	CFRelease(eventItem);
	eventItem = NULL;

	ok_status(SecKeychainItemDelete(itemRef), "delete item");
	is_sec_event(kSecDeleteEvent, &eventKeychain, &eventItem, NULL,
		"delete event");
    is(eventItem, itemRef, "delete event item matches");
    is(eventKeychain, keychain, "delete event keychain matches");
	if (eventKeychain != NULL) // eventKeychain can be null if the test times out
	{
		CFRelease(eventKeychain);
		eventKeychain = NULL;
	}
	
	if (eventItem != NULL)
	{
		CFRelease(eventItem);
		eventItem = NULL;
	}
	
	no_sec_event("no event");
	ok_status(test_sec_event_deregister(), "deregister events.");

	CFRelease(itemRef);
	CFRelease(keychain);


	ok(tests_end(1), "cleanup");
}

int main(int argc, char *const *argv)
{
#ifdef DEBUG
	int dont_skip = argc > 1 && !strcmp(argv[1], "-s");
	plan_tests(14);

	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests(dont_skip);
	ok_leaks("no leaks");
#endif
	plan_tests(1);
	ok_leaks("no leaks");
	
	return 0;
}
