#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "testmore.h"
#include "testenv.h"
#include "testleaks.h"

static void tests(void)
{
	char *home = getenv("HOME");
	char kcname1[256], kcname2[256];

	if (!home || strlen(home) > 200)
		plan_skip_all("home too big");

	sprintf(kcname1, "%s/kc1", home);
	SecKeychainRef kc1 = NULL, kc2 = NULL;
	Boolean kc1valid, kc2valid;
	ok_status(SecKeychainCreate(kcname1, 4, "test", FALSE, NULL, &kc1),
		"SecKeychainCreate kc1");
	ok_status(SecKeychainIsValid(kc1, &kc1valid), "SecKeychainIsValid kc1");
	is(kc1valid, TRUE, "is kc1 valid");
	CFRelease(kc1);

	int fd;
	sprintf(kcname2, "%s/kc2", home);
	ok_unix(fd = open(kcname2, O_CREAT|O_WRONLY|O_TRUNC, 0600),
		"create invalid kc2 file");
	ok_unix(close(fd), "close the kc2 file");
	ok_status(SecKeychainOpen(kcname2, &kc2), "SecKeychainOpen kc2");

	ok_status(SecKeychainIsValid(kc2, &kc2valid), "SecKeychainIsValid kc2");
	TODO: {
		todo("<rdar://problem/3795566> SecKeychainIsValid always returns "	
			"TRUE");
		is(kc2valid, FALSE, "is kc2 not valid");
	}
	CFRelease(kc2);

	tests_end(1);
}

int main(int argc, char *const *argv)
{
	plan_tests(9);
	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests();

	ok_leaks("leaks");

	return 0;
}
