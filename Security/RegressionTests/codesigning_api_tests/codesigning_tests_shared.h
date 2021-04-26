//
//  codesigning_tests_shared.h
//  Security
//
//  Copyright 2021 Apple Inc. All rights reserved.
//

//
// BATS test token helpers
//
#define TEST_START(name) \
    do { \
        printf("==================================================\n"); \
        printf("[TEST] %s\n", name); \
        printf("==================================================\n"); \
    } while(0)

#define TEST_CASE(cond, name) \
    do { \
        printf("[BEGIN] %s\n", (name)); \
        if ((cond)) \
            printf("[PASS] %s\n", (name)); \
        else \
            printf("[FAIL] %s\n", (name)); \
    } while (0)

#define TEST_CASE_EXPR(cond) TEST_CASE(cond, #cond)

#define TEST_CASE_JUMP(cond, block, name) \
    do { \
        printf("[BEGIN] %s\n", (name)); \
        if ((cond)) \
            printf("[PASS] %s\n", (name)); \
        else  {\
            printf("[FAIL] %s\n", (name)); \
            goto block; \
        } \
    } while (0)

#define TEST_CASE_EXPR_JUMP(cond, block) TEST_CASE_JUMP(cond, block, #cond)

#define TEST_CASE_BLOCK(name, block) \
    do { \
        printf("[BEGIN] %s\n", (name)); \
        if (block()) \
            printf("[PASS] %s\n", (name)); \
        else \
            printf("[FAIL] %s\n", (name)); \
    } while (0)

#define TEST_BEGIN printf("[BEGIN] %s\n", __FUNCTION__);
#define TEST_PASS printf("[PASS] %s\n", __FUNCTION__);
#define TEST_FAIL printf("[FAIL] %s\n", __FUNCTION__);

#define TEST_RESULT(cond) \
    (cond) ? TEST_PASS : TEST_FAIL

//
// Common output helpers
//
#define INFO(fmt, ...)                                      \
({                                                          \
    NSLog(fmt, ##__VA_ARGS__);                              \
})
