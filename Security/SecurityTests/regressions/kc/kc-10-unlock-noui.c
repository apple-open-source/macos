#include <Security/SecKeychain.h>

#include "testmore.h"
#include "testenv.h"

int main(int argc, char *const *argv)
{
	int dont_skip = argc > 1 && !strcmp(argv[1], "-s");

	plan_tests(7);

	ok_status(SecKeychainSetUserInteractionAllowed(FALSE), "SecKeychainSetUserInteractionAllowed(FALSE)");
	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	SecKeychainRef keychain;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"SecKeychainCreate");
	ok_status(SecKeychainLock(keychain), "SecKeychainLock");
	SKIP: {
		skip("<rdar://problem/3916692> 8A325 SecKeychainSetUserInteractionAllowed() doesn't affect kc unlock dialogs", 1, dont_skip);

		is_status(SecKeychainUnlock(keychain, 0, NULL, FALSE),
			errSecInteractionNotAllowed, "SecKeychainUnlock");
	}
	ok_status(SecKeychainLock(keychain), "SecKeychainLock");
	CFRelease(keychain);

	ok_status(SecKeychainOpen("test", &keychain), "SecKeychainOpen locked kc");
	SKIP: {
		skip("<rdar://problem/3916692> 8A325 SecKeychainSetUserInteractionAllowed() doesn't affect kc unlock dialogs", 1, dont_skip);

		is_status(SecKeychainUnlock(keychain, 0, NULL, FALSE),
			errSecInteractionNotAllowed, "SecKeychainUnlock");
	}
	return !tests_end(1);
}
