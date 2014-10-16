#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>
#include <stdlib.h>
#include <unistd.h>

#include "testenv.h"
#include "testleaks.h"
#include "testmore.h"
#include "testsecevent.h"

void tests(int dont_skip)
{
	const char *user = getenv("USER");
	ok((user != NULL && strlen(user) != 0), "USER must be non-nil and non-zero length"); 
	fprintf(stdout, "Testing login for user \"%s\"\n", user);

	ok_status(test_sec_event_register(kSecEveryEventMask),
		"register for all events");

	// test SecKeychainLogin for $USER with password "test"
	ok_status(SecKeychainLogin(strlen(user), user, 4, "test"), "login user");

	// wait for a list changed event
	is_sec_event(kSecKeychainListChangedEvent, NULL, NULL, NULL,
		"list changed event");
	no_sec_event("no event");

	// get the default keychain (should be login.keychain if none is explicitly set)
	SecKeychainRef default_keychain = NULL;
	ok_status(SecKeychainCopyDefault(&default_keychain), "get default");
	
	// test status of default keychain (should be read/write and unlocked)
	SecKeychainStatus status = 0;
	ok_status(SecKeychainGetStatus(default_keychain, &status), "get status");
	is(status, kSecUnlockStateStatus|kSecReadPermStatus|kSecWritePermStatus,
		"default should be read/write/unlocked");

	// get the path for the default keychain
	char path[1024];
	UInt32 path_len = sizeof(path) - 1;
	ok_status(SecKeychainGetPath(default_keychain, &path_len, path),
		"get path");
	fprintf(stdout, "Default keychain path is %s\n", path);
	path[path_len] = 0;
	const char *login_path = "Library/Keychains/login.keychain";
	cmp_ok(path_len, >, strlen(login_path), "path len is enough");
	eq_string(path + path_len - strlen(login_path), login_path, "check path");

	// check retain count on default keychain (why??)
	is(CFGetRetainCount(default_keychain), 1, "default retain count is 1");
	CFRelease(default_keychain);
	default_keychain = NULL;

	// lock and unlock events have been removed because they can't be made reliable

	ok_status(test_sec_event_deregister(), "deregister events.");

	// rename login.keychain to $USER to simulate a Panther-style keychain
	char testuser_path[1024];
	sprintf(testuser_path, "Library/Keychains/%s", user);
	ok_unix(rename(login_path, testuser_path),
		"rename login.keychain to $USER");

	// login and verify that SecKeychainLogin cleans up the $USER keychain
	// (either by renaming to $USER.keychain, or renaming to login.keychain)
    ok_status(SecKeychainLogin(strlen(user), user, 4, "test"), "login again");

	// get the default keychain (should be login.keychain if none is explicitly set)
	ok_status(SecKeychainCopyDefault(&default_keychain), "get default");
	path_len = sizeof(path) - 1;
	ok_status(SecKeychainGetPath(default_keychain, &path_len, path),
		"get path");
	path[path_len] = 0;
	cmp_ok(path_len, >, strlen(testuser_path), "path len is enough");

	// lock the default keychain
	ok_status(SecKeychainLock(default_keychain), "lock default");
	CFRelease(default_keychain);

	ok(tests_end(1), "cleanup");
}

int main(int argc, char *const *argv)
{
	int dont_skip = argc > 1 && !strcmp(argv[1], "-s");
	plan_tests(21);

	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests(dont_skip);
	ok_leaks("no leaks");

	return 0;
}
