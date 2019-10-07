#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

int kc_45_change_password(int argc, char *const *argv)
{
    plan_tests(16);

    initializeKeychainTests(__FUNCTION__);

    ok_status(SecKeychainSetUserInteractionAllowed(FALSE), "SecKeychainSetUserInteractionAllowed(FALSE)");

    SecKeychainRef keychain = createNewKeychain("test", "before");
    ok_status(SecKeychainLock(keychain), "SecKeychainLock");
    is_status(SecKeychainChangePassword(keychain, 0, NULL, 5, "after"), errSecInteractionNotAllowed, "Change PW w/ null pw while locked");  // We're not responding to prompt so we can't do stuff while locked
    checkPrompts(1, "Prompt to unlock keychain before password change");
    is_status(SecKeychainChangePassword(keychain, 5, "badpw", 5, "after"), errSecAuthFailed, "Change PW w/ bad pw while locked");
    ok_status(SecKeychainUnlock(keychain, 6, "before", true), "SecKeychainUnlock");
    is_status(SecKeychainChangePassword(keychain, 0, NULL, 5, "after"), errSecAuthFailed, "Change PW w/ null pw while unlocked");
    is_status(SecKeychainChangePassword(keychain, 5, "badpw", 5, "after"), errSecAuthFailed, "Change PW w/ bad pw while unlocked");
    ok_status(SecKeychainChangePassword(keychain, 6, "before", 7, "between"), "Change PW w/ good pw while unlocked");
    ok_status(SecKeychainLock(keychain), "SecKeychainLock");
    ok_status(SecKeychainChangePassword(keychain, 7, "between", 7, "after"), "Change PW w/ good pw while locked");
    checkPrompts(0, "Unexpected keychain access prompt");

    ok_status(SecKeychainDelete(keychain), "%s: SecKeychainDelete", testName);
    CFRelease(keychain);

    deleteTestFiles();
    return 0;
}
