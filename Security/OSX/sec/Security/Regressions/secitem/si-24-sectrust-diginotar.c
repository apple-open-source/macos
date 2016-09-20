/*
 * Copyright (c) 2011-2012 Apple Inc. All Rights Reserved.
 */

#include <Security/SecPolicyPriv.h>
#include <utilities/SecCFWrappers.h>

#include <test/testpolicy.h>

#include "shared_regressions.h"

static void tests(void)
{
    SecPolicyRef sslPolicy = SecPolicyCreateSSL(false, 0);

    /* Run the tests. */
    runCertificateTestForDirectory(sslPolicy, CFSTR("DigiNotar"), NULL);
    runCertificateTestForDirectory(sslPolicy, CFSTR("DigiNotar-Entrust"), NULL);
    runCertificateTestForDirectory(sslPolicy, CFSTR("DigiNotar-ok"), NULL);

    CFReleaseSafe(sslPolicy);
}

int si_24_sectrust_diginotar(int argc, char *const *argv)
{
	plan_tests(25);

	tests();

	return 0;
}
