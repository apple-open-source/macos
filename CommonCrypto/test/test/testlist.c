/* This file contains an array of all test functions, last element is NULL */

#include "testmore.h"
#include "testlist.h"

#define ONE_TEST(x) {#x, x, 0 , 0, 0 },
#define DISABLED_ONE_TEST(x)
struct one_test_s testlist[] = {
#include "testlistInc.h"
    { NULL, NULL, 0, 0, 0}, 
};
#undef ONE_TEST
#undef DISABLED_ONE_TEST


