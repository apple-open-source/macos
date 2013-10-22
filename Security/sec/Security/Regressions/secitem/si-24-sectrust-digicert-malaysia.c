/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
 */

#include <Security/SecPolicyPriv.h>
#include <Security/SecInternal.h>

#include <test/testpolicy.h>

#include "Security_regressions.h"

static void tests(void)
{
    SecPolicyRef sslPolicy = SecPolicyCreateSSL(false, 0);

    /* Run the tests. */
    runCertificateTestForDirectory(sslPolicy, CFSTR("DigicertMalaysia"), NULL);

    CFReleaseSafe(sslPolicy);
}

int si_24_sectrust_digicert_malaysia(int argc, char *const *argv)
{
	plan_tests(2);

	tests();

	return 0;
}
