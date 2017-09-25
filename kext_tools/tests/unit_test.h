/*
 *  unit_test.h
 *  kext_tools
 *
 *  Copyright 2017 Apple Inc. All rights reserved.
 *
 */
#pragma once

#define TEST_START(name) \
do { \
    printf("==================================================\n"); \
    printf("[TEST] %s\n", name); \
    printf("==================================================\n"); \
} while(0)

#define TEST_RESULT(name, cond) \
do { \
    if ((cond)) \
        printf("[PASS] %s\n", (name)); \
    else \
        printf("[FAIL] %s\n", (name)); \
} while (0)


#define TEST_CASE(name, cond) \
do { \
    printf("[BEGIN] %s\n", (name)); \
    TEST_RESULT(name, cond); \
} while (0)
