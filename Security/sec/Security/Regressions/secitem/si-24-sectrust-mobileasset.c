/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
 */

#include <Security/SecPolicyPriv.h>
#include <Security/SecInternal.h>

#include <test/testpolicy.h>

#include "Security_regressions.h"

static void tests(void)
{
    SecPolicyRef otaPolicy = SecPolicyCreateMobileAsset();

    /* Run the tests. */
    runCertificateTestForDirectory(otaPolicy, CFSTR("mobileasset-certs"), NULL);

    CFReleaseSafe(otaPolicy);
}

int si_24_sectrust_mobileasset(int argc, char *const *argv)
{
	plan_tests(2);

	tests();

	return 0;
}
