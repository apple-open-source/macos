//
//  sectask.m
//  secsecstaticcodeapitest
//

#import <Foundation/Foundation.h>
#include <security_utilities/cfutilities.h>
#import "codesigning_tests_shared.h"

static int
CheckErrorSecTaskCopyValuesForEntitlements(void)
{
    TEST_BEGIN

    // Create an SecTaskRef with an invalid token
    audit_token_t token = INVALID_AUDIT_TOKEN_VALUE;
    CFRef<SecTaskRef> task = SecTaskCreateWithAuditToken(kCFAllocatorDefault, token);

    CFRef<CFErrorRef> error = nil;
    NSArray *array = [[NSArray alloc] init];
    CFRef<CFDictionaryRef> values = SecTaskCopyValuesForEntitlements(task, (__bridge CFArrayRef)array, error.take());

    if (error == nil) {
        FAIL("Expecting to get an error asking about `INVALID_AUDIT_TOKEN_VALUE`");
        return -1;
    } else {
        CFStringRef domain = CFErrorGetDomain(error);
        CFIndex code = CFErrorGetCode(error);
        if (![(__bridge NSString*)domain isEqualTo:NSPOSIXErrorDomain]) {
            FAIL("domain != NSPOSIXErrorDomain");
            return -1;
        }
        if (code != ESRCH) {
            FAIL("code != ESRCH");
            return -1;
        }
    }

    PASS();
    return 0;
}

int main(void)
{
    TEST_START("secsectaskapitest");

    int i;
    int (*testList[])(void) = {
        CheckErrorSecTaskCopyValuesForEntitlements,
    };
    const int numberOfTests = sizeof(testList) / sizeof(*testList);
    int testResults[numberOfTests] = {0};

    for (i = 0; i < numberOfTests; i++) {
        testResults[i] = testList[i]();
    }

    fprintf(stdout, "[SUMMARY]\n");
    for (i = 0; i < numberOfTests; i++) {
        fprintf(stdout, "%d. %s\n", i+1, testResults[i] == 0 ? "Passed" : "Failed");
    }

    return 0;
}
