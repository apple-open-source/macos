
/* This file contains all the prototypes for all the test functions */
#ifndef __TESTLIST_H__
#define __TESTLIST_H__


#define ONE_TEST(x) int x(int argc, char *const *argv);
#define DISABLED_ONE_TEST(x) ONE_TEST(x)
#include "testlistInc.h"
#undef ONE_TEST
#undef DISABLED_ONE_TEST


#endif /* __TESTLIST_H__ */

