/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
 */

#include <Security/SecPolicyPriv.h>
#include <Security/SecInternal.h>

#include <test/testpolicy.h>

#include "Security_regressions.h"

static void tests(void)
{
    SecPolicyRef policy = SecPolicyCreateAppleIDAuthorityPolicy();

    /* Run the tests. */
    runCertificateTestForDirectory(policy, CFSTR("AppleID-certs"), NULL);

    CFReleaseSafe(policy);
}

int si_24_sectrust_appleid(int argc, char *const *argv)
{
	plan_tests(2);

	tests();

	return 0;
}
