/* This file contains an array of all test functions, last element is NULL */

#undef ONE_TEST
#undef DISABLED_ONE_TEST
#undef OFF_ONE_TEST

#define ONE_TEST(x) {#x, x, 0, 0 , 0, 0 },
#define OFF_ONE_TEST(x) {#x, x, 1, 0 , 0, 0 },
#define DISABLED_ONE_TEST(x)
struct one_test_s testlist[] = {
