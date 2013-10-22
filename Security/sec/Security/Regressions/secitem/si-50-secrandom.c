/*
 *  si-50-secrandom.c
 *  Security
 *
 *  Created by Michael Brouwer on 5/8/07
 *  Copyright (c) 2007-2008,2010 Apple Inc. All Rights Reserved.
 *
 */

#include <Security/SecRandom.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
	UInt8 bytes[4096] = {};
	CFIndex size = 42;
	UInt8 *p = bytes + 23;
	ok_status(SecRandomCopyBytes(kSecRandomDefault, size, p), "generate some random bytes");
}

int si_50_secrandom(int argc, char *const *argv)
{
	plan_tests(1);


	tests();

	return 0;
}
