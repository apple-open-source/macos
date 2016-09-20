#include <stdlib.h>
#include <Security/SecKeychain.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

int kc_01_keychain_creation(int argc, char *const *argv)
{
	plan_tests(9);

	ok_status(SecKeychainSetUserInteractionAllowed(FALSE), "disable ui");
    SecKeychainRef keychain = createNewKeychain("test", "test");
	SKIP: {
		skip("can't continue without keychain", 2, ok(keychain, "keychain not NULL"));
		
		is(CFGetRetainCount(keychain), 1, "retaincount of created keychain is 1");
	}

	SecKeychainRef keychain2 = NULL;
	ok_status(SecKeychainOpen("test", &keychain2), "SecKeychainOpen");
	SKIP: {
		skip("can't continue without keychain", 2, ok(keychain, "keychain not NULL"));
		CFIndex retCount = CFGetRetainCount(keychain2);
		is(retCount, 2, "retaincount of created+opened keychain is 2"); // 2, because we opened and created the same keychain.
	}
    
    is(keychain, keychain2, "SecKeychainCreate and SecKeychainOpen returned a different handle for the same keychain");

	ok_status(SecKeychainDelete(keychain), "SecKeychainDelete");

    CFRelease(keychain);
    CFRelease(keychain2);

	return 0;
}
