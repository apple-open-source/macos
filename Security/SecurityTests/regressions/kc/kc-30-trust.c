#include <Security/SecTrust.h>

#include "testmore.h"
#include "testleaks.h"

static void tests(void)
{
	SecTrustRef trust = NULL;

	ok_status(SecTrustCreateWithCertificates(NULL, NULL, &trust),
		"create empty trust");
	ok(trust, "trust not NULL");
	CFRelease(trust);
}

int main(int argc, char *const *argv)
{
	plan_tests(3);

	tests();

	ok_leaks("leaks");

	return 0;
}
