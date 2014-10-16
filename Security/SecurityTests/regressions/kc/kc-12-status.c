#include <Security/SecKeychain.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "testmore.h"
#include "testenv.h"
#include "testleaks.h"

static void tests(void)
{
	char *home = getenv("HOME");
	char kcname1[256], kcname2[256];
	SecKeychainStatus status1, status2;

	if (!home || strlen(home) > 200)
		plan_skip_all("home too big");

	sprintf(kcname1, "%s/kc1/kc1", home);
	SecKeychainRef kc1 = NULL, kc2 = NULL;
	ok_status(SecKeychainCreate(kcname1, 4, "test", FALSE, NULL, &kc1),
		"SecKeychainCreate kc1");

	ok_status(SecKeychainGetStatus(kc1, &status1), "get kc1 status");
	is(status1, kSecUnlockStateStatus|kSecReadPermStatus|kSecWritePermStatus,
		"status unlocked readable writable");
	ok_status(SecKeychainLock(kc1), "SecKeychainLock kc1");
	ok_status(SecKeychainGetStatus(kc1, &status1), "get kc1 status");
	TODO: {
		todo("<rdar://problem/2668794> KeychainImpl::status() returns "
			"incorrect status (always writable?)");

		is(status1, kSecReadPermStatus|kSecWritePermStatus,
			"status (locked) readable writable");
	}

	/* Make keychain non writable. */
	char kcdir1[256];
	sprintf(kcdir1, "%s/kc1", home);
	ok_unix(chmod(kcdir1, 0555), "chmod kcdir1 0555");

	ok_status(SecKeychainGetStatus(kc1, &status1), "get kc1 status");
	is(status1, kSecReadPermStatus, "status (locked) readable");
	ok_status(SecKeychainUnlock(kc1, 4, "test", TRUE), "SecKeychainLock kc1");
	ok_status(SecKeychainGetStatus(kc1, &status1), "get kc1 status");
	TODO: {
		todo("<rdar://problem/2668794> KeychainImpl::status() returns "
			"incorrect status (always writable?)");

		is(status1, kSecUnlockStateStatus|kSecReadPermStatus,
			"status unlocked readable");
	}

	/* Reopen the keychain. */
	CFRelease(kc1);
	ok_status(SecKeychainOpen(kcname1, &kc1), "SecKeychainOpen kc1");

	ok_status(SecKeychainGetStatus(kc1, &status1), "get kc1 status");
	TODO: {
		todo("<rdar://problem/2668794> KeychainImpl::status() returns "
			"incorrect status (always writable?)");

		is(status1, kSecUnlockStateStatus|kSecReadPermStatus,
			"status unlocked readable");
	}

	sprintf(kcname2, "%s/kc2/kc2", home);
	ok_status(SecKeychainOpen(kcname2, &kc2), "SecKeychainOpen kc2");
	is_status(SecKeychainGetStatus(kc2, &status2), errSecNoSuchKeychain,
		"get kc2 status");
	ok_status(SecKeychainCreate(kcname2, 4, "test", FALSE, NULL, &kc2),
		"SecKeychainCreate kc2");
	ok_unix(chmod(kcname2, 0444), "chmod kc2 0444");
	ok_status(SecKeychainGetStatus(kc2, &status2), "get kc2 status");
	is(status2, kSecUnlockStateStatus|kSecReadPermStatus|kSecWritePermStatus,
		"status unlocked readable writable");

	/* Reopen the keychain. */
	CFRelease(kc2);
	ok_status(SecKeychainOpen(kcname2, &kc2), "SecKeychainOpen kc2");

	ok_status(SecKeychainGetStatus(kc2, &status2), "get kc2 status");
	is(status2, kSecUnlockStateStatus|kSecReadPermStatus|kSecWritePermStatus,
		"status unlocked readable writable");

	/* Restore dir to writable so cleanup code will work ok. */
	ok_unix(chmod(kcdir1, 0755), "chmod kcdir1 0755");
	CFRelease(kc1);
	CFRelease(kc2);

	bool testWithFreshlyCreatedKeychain = true;
	SecKeychainRef keychain;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"SecKeychainCreate");
	ok_status(SecKeychainLock(keychain), "SecKeychainLock");

	do {
		SecKeychainStatus keychainStatus = 0;
		is_status(SecKeychainUnlock(keychain, 0, NULL, true), -25293, "SecKeychainUnlock with NULL password (incorrect)");
		ok_status(SecKeychainGetStatus(keychain, &keychainStatus), "SecKeychainGetStatus");
		is( (keychainStatus & kSecUnlockStateStatus), 0, "Check it's not unlocked");

		keychainStatus = 0;
		ok_status(SecKeychainUnlock(keychain, strlen("test"), "test", true), "SecKeychainUnlock with correct password");
		ok_status(SecKeychainGetStatus(keychain, &keychainStatus), "SecKeychainGetStatus");
		is( (keychainStatus & kSecUnlockStateStatus), kSecUnlockStateStatus, "Check it's unlocked");
		
		ok_status(SecKeychainLock(keychain), "SecKeychainLock");
		CFRelease(keychain);
		
		if (testWithFreshlyCreatedKeychain)
		{
			testWithFreshlyCreatedKeychain = false;
			ok_status(SecKeychainOpen("test", &keychain), "SecKeychainOpen");
		}
		else
			testWithFreshlyCreatedKeychain = true;
		
	}
	while(!testWithFreshlyCreatedKeychain);

	tests_end(1);
}



int main(int argc, char *const *argv)
{
	plan_tests(43);
	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests();

	ok_leaks("leaks");

	return 0;
}
