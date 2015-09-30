/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
 */

#include <stdlib.h>

#include "test_regressions.h"

int test_00_test(int argc, char *const *argv)
{
    plan_tests(6);

    ok(1, "ok ok");

    TODO: {
        todo("ok bad is supposed to fail");
        ok(0, "ok bad");
    }

    isnt(0, 4, "isnt ok");

    TODO: {
        todo("isnt bad is supposed to fail");
        isnt(3, 3, "isnt bad");
    }

    cmp_ok(3, &&, 3, "cmp_ok ok");

    TODO: {
        todo("cmp_ok bad is supposed to fail");
        cmp_ok(3, &&, 0, "cmp_ok bad");
    }

    return 0;
}
