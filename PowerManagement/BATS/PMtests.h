#include <stdlib.h>
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOReturn.h>

#define START_TEST(fmt,...) \
do { \
    printf("[TEST]"fmt, ##__VA_ARGS__ ); \
    printf("**************************\n"); \
} while(0)

#define START_TEST_CASE(fmt,...) \
do { \
    printf("[BEGIN]"fmt, ##__VA_ARGS__ ); \
} while(0)

#define FAIL(fmt,...) \
do { \
    printf("[FAIL]"fmt"\n", ##__VA_ARGS__ ); \
    gFailCnt++; \
} while(0)

#define PASS(fmt,...) \
do { \
    printf("[PASS]"fmt"\n", ##__VA_ARGS__ ); \
    gPassCnt++; \
} while (0)

#define SUMMARY(name) \
do { \
    printf("**************************\n"); \
    printf("[SUMMARY] "name": Passed: %d Fail:%d\n", gPassCnt, gFailCnt); \
} while(0)

#define LOG(fmt,...) \
do { \
    printf("\t"fmt, ##__VA_ARGS__ ); \
} while (0)


#define INT_TO_CFNUMBER(numRef, val) { \
    int __n = (val);  \
    numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &__n); \
}


