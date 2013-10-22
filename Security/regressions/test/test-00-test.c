/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
 */

#include <stdlib.h>

#include "test_regressions.h"

int test_00_test(int argc, char *const *argv)
{
    int rv = 1;
    plan_tests(6);

    TODO: {
	todo("ok 0 is supposed to fail");

	rv = ok(0, "ok bad");
	if (!rv)
	    diag("ok bad not good today");
    }
    rv &= ok(1, "ok ok");
#if 0
    SKIP: {
	skip("is bad will fail", 1, 0);

	if (!is(0, 4, "is bad"))
	    diag("is bad not good today");
    }
    SKIP: {
	skip("is ok should not be skipped", 1, 1);

        is(3, 3, "is ok");
    }
#endif
    isnt(0, 4, "isnt ok");
    TODO: {
	todo("isnt bad is supposed to fail");

	isnt(3, 3, "isnt bad");
    }
    TODO: {
	todo("cmp_ok bad is supposed to fail");

	cmp_ok(3, &&, 0, "cmp_ok bad");
    }
    cmp_ok(3, &&, 3, "cmp_ok ok");

    return 0;
}
