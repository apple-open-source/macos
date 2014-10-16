#include <stdlib.h>
#include <Security/SecKeychain.h>

#include "testmore.h"
#include "testenv.h"

int main(int argc, char *const *argv)
{
	plan_tests(8);

	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	ok_status(SecKeychainSetUserInteractionAllowed(FALSE), "disable ui");
	SecKeychainRef keychain = NULL;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"SecKeychainCreate");
	SKIP: {
		skip("can't continue without keychain", 2, ok(keychain, "keychain not NULL"));
		ok_status(SecKeychainDelete(keychain), "SecKeychainDelete");
		
		is_status(CFGetRetainCount(keychain), 1, "retaincount is 1");
		CFRelease(keychain);
	}

	keychain = NULL;
	ok_status(SecKeychainOpen("test", &keychain), "SecKeychainOpen");
	SKIP: {
		skip("can't continue without keychain", 2, ok(keychain, "keychain not NULL"));
		CFIndex retCount = CFGetRetainCount(keychain);
		is_status(retCount, 1, "retaincount is 1");
		CFRelease(keychain);
	}

	return !tests_end(1);
}
