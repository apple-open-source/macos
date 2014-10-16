#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <stdlib.h>
#include <unistd.h>

#include "testenv.h"
#include "testleaks.h"
#include "testmore.h"
#include "testsecevent.h"

void tests(int dont_skip)
{
	SecKeychainRef keychain = NULL, default_keychain = NULL;
	is_status(SecKeychainCopyDefault(&default_keychain),
		errSecNoDefaultKeychain, "no default keychain");

	ok_status(test_sec_event_register(kSecEveryEventMask),
		"register for all events");
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"create keychain");
	is_sec_event(kSecKeychainListChangedEvent, NULL, NULL, NULL,
		"list changed event");
	SecKeychainRef notKc = NULL;

	is_sec_event(kSecDefaultChangedEvent, &notKc, NULL, NULL,
		 "default changed event");

	is((intptr_t)keychain, (intptr_t)notKc,
		"keychain in notification is keychain");

	no_sec_event("no event");

	ok_status(SecKeychainCopyDefault(&default_keychain),
		"get default keychain");
	is((intptr_t)default_keychain, (intptr_t)keychain,
		"default kc is just created kc");
	if (default_keychain)
	{
		CFRelease(default_keychain);
		default_keychain = NULL;
	}

	SecKeychainRef keychain2;
	ok_status(SecKeychainCreate("test2", 4, "test", FALSE, NULL, &keychain2),
		"create keychain2");
	is_sec_event(kSecKeychainListChangedEvent, NULL, NULL, NULL,
		"list changed event");
	no_sec_event("no event");

	ok_status(SecKeychainCopyDefault(&default_keychain),
		"get default keychain");

	is((intptr_t)default_keychain, (intptr_t)keychain,
		"default kc is first created kc");

	if (default_keychain)
	{
		CFRelease(default_keychain);
		default_keychain = NULL;
	}

	ok_status(SecKeychainDelete(keychain), "delete default keychain");
	is_sec_event(kSecKeychainListChangedEvent, NULL, NULL, NULL,
		"list changed event");

		is_sec_event(kSecDefaultChangedEvent, NULL, NULL, NULL,
			"default changed event");

	no_sec_event("no event");
	CFRelease(keychain);

	ok_status(SecKeychainDelete(keychain2), "delete keychain2");
	CFRelease(keychain2);
	is_sec_event(kSecKeychainListChangedEvent, NULL, NULL, NULL,
		"list changed event");
	no_sec_event("no event");

	ok_status(test_sec_event_deregister(), "deregister events.");


	ok(tests_end(1), "cleanup");
}

int main(int argc, char *const *argv)
{
#ifdef DEBUG
	int dont_skip = argc > 1 && !strcmp(argv[1], "-s");
	plan_tests(24);

	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests(dont_skip);
#endif
	plan_tests(1);
	ok_leaks("no leaks");

	return 0;
}
