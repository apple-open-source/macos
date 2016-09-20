#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

int kc_02_unlock_noui(int argc, char *const *argv)
{
    plan_tests(12);

    initializeKeychainTests(__FUNCTION__);

	ok_status(SecKeychainSetUserInteractionAllowed(FALSE), "SecKeychainSetUserInteractionAllowed(FALSE)");

    SecKeychainRef keychain = createNewKeychain("test", "test");
	ok_status(SecKeychainLock(keychain), "SecKeychainLock");

    is_status(SecKeychainUnlock(keychain, 0, NULL, FALSE), errSecAuthFailed, "SecKeychainUnlock");

    checkPrompts(0, "Unexpected keychain access prompt unlocking after SecKeychainCreate");

	ok_status(SecKeychainLock(keychain), "SecKeychainLock");
	CFRelease(keychain);

	ok_status(SecKeychainOpen("test", &keychain), "SecKeychainOpen locked kc");

    is_status(SecKeychainUnlock(keychain, 0, NULL, FALSE), errSecAuthFailed, "SecKeychainUnlock");

    checkPrompts(0, "Unexpected keychain access prompt unlocking after SecKeychainCreate");

    ok_status(SecKeychainDelete(keychain), "%s: SecKeychainDelete", testName);
    CFRelease(keychain);

    deleteTestFiles();
    return 0;
}
