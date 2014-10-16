/*
 * Copyright (c) 2011-2012 Apple Inc. All Rights Reserved.
 */

#include <Security/SecPolicyPriv.h>
#include <Security/SecInternal.h>

#include <test/testpolicy.h>

#include "Security_regressions.h"

static void tests(void)
{
    SecPolicyRef basicPolicy = SecPolicyCreateBasicX509();

    /* Run the tests. */
    runCertificateTestForDirectory(basicPolicy, CFSTR("nist-certs"), NULL);

    CFReleaseSafe(basicPolicy);
}

int si_24_sectrust_nist(int argc, char *const *argv)
{
	plan_tests(179);

	tests();

	return 0;
}
