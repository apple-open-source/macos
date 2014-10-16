#include <stdlib.h>
#include <Security/SecKeychain.h>

#include "testmore.h"
#include "testenv.h"

int main(int argc, char *const *argv)
{
	int dont_skip = argc > 1 && !strcmp(argv[1], "-s");

	plan_tests(10);

	ok_status(SecKeychainSetUserInteractionAllowed(FALSE), "disable ui");
	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	char *home = getenv("HOME");
	char source[256], dest[256];
	if (!home || strlen(home) > 200)
		plan_skip_all("home too big");

	sprintf(source, "%s/source", home);
	sprintf(dest, "%s/dest", home);
	SecKeychainRef sourcekc = NULL, destkc = NULL;
	ok_status(SecKeychainCreate(source, 4, "test", FALSE, NULL, &sourcekc),
		"SecKeychainCreate source");
	ok_status(SecKeychainCreate(dest, 4, "test", FALSE, NULL, &destkc),
		"SecKeychainCreate dest");
	char cmdbuf[1024];
	ok_unix(sprintf(cmdbuf, "systemkeychain -k '%s' -s '%s'", dest, source),
		"sprintf");
	SKIP: {
		skip("systemkeychain brings up UI", 1, dont_skip);

		ok_unix(system(cmdbuf), "systemkeychain");
	}
	ok_status(SecKeychainLock(sourcekc), "SecKeychainLock source");
	SKIP: {
		skip("unlocking a source keychain w/ referal before reopen brings "
			"up ui", 1, dont_skip);
		TODO: {
			todo("<rdar://problem/4066169> Unlocking a source "
				"keychain w/ referal before reopen fails");

			ok_status(SecKeychainUnlock(sourcekc, 0, NULL, FALSE),
				"SecKeychainUnlock source");
		}
	}
	CFRelease(sourcekc);
	sourcekc = NULL;
	char source2[256];
	sprintf(source2, "%s/source2", home);
	ok_unix(rename(source, source2), "rename source -> source2");
	ok_status(SecKeychainOpen(source2, &sourcekc), "SecKeychainOpen source2");
	SKIP: {
		skip("systemkeychain brings up UI", 1, dont_skip);

		ok_status(SecKeychainUnlock(sourcekc, 0, NULL, FALSE),
			"SecKeychainUnlock source2");
	}
	CFRelease(sourcekc);

	return !tests_end(1);
}
