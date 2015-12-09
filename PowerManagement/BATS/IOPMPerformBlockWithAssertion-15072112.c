/*
 * Copyright (c) 2014
  Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include <dispatch/dispatch.h>
#include <stdio.h>


/*

cc -o /tmp/IOPMPerformBlockWithAssertion-15072112  IOPMPerformBlockWithAssertion-15072112.c  -framework IOKit -framework CoreFoundation

Inspired by:
<rdar://problem/15072112> Sub-TLF: Investigating IOPMAssertion block API

*/



#define kAssertName "IOPMPerformBlockWithAssertion test"

int checkAssertionState()
{
    CFDictionaryRef dict = NULL;
    CFArrayRef  array = NULL;
    CFIndex i;
    IOReturn rc;
    int ret = 0;
    int val = 0;

    rc = IOPMCopyAssertionsStatus(&dict);
    if (rc != kIOReturnSuccess) {
        printf("FAIL: IOPMCopyAssertionsStatus returned 0x%x\n", rc);
        return -1;
    }

    CFNumberRef cfVal = CFDictionaryGetValue(dict, kIOPMAssertionTypePreventUserIdleSystemSleep);
    if (cfVal) {
        CFNumberGetValue(cfVal, kCFNumberIntType, &val);
    }
    if (val == 0) {
        printf("FAIL: Assertion Status reports that PreventUserIdle is not enabled\n");
        ret = -1;
        goto exit;
    }

    rc = IOPMCopyAssertionsByType(kIOPMAssertionTypePreventUserIdleSystemSleep, &array);
    if (rc != kIOReturnSuccess) {
        printf("FAIL: IOPMCopyAssertionsByType returned 0x%x\n", rc);
        ret = -1;
        goto exit;
    }
    for (i=0; i < CFArrayGetCount(array); i++) {
        CFDictionaryRef assertion = CFArrayGetValueAtIndex(array, i);
        if (!assertion) {
            ret = -1;
            printf("FAIL: IOPMCopyAssertionsByType returned empty element at index %ld\n", i);
            goto exit;
        }

        CFStringRef name = CFDictionaryGetValue(assertion, kIOPMAssertionNameKey);
        if (!name) {
            ret = -1;
            printf("FAIL: IOPMCopyAssertionsByType returned assertion without name at index %ld\n", i);
            goto exit;
        }
        if (CFStringCompare(name, CFSTR(kAssertName), 0) == kCFCompareEqualTo) {
            break;
        }
    }
    if (i >= CFArrayGetCount(array)) {
        printf("FAIL: Expected block assertion is not found in the output of IOPMCopyAssertionsByType\n");
        ret = -1;
        goto exit;
    }


exit:
    if (dict) {
        CFRelease(dict);
    }
    if (array) {
        CFRelease(array);
    }
    return ret;
}
int main(int argc, char *argv[])
{

    __block int ret = 0;
    IOReturn rc;

    CFMutableDictionaryRef properties = CFDictionaryCreateMutable(0, 3,
                                                           &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);

    if (properties) {
        CFDictionarySetValue(properties, kIOPMAssertionNameKey, CFSTR(kAssertName));

        CFDictionarySetValue(properties, kIOPMAssertionTypeKey, kIOPMAssertPreventUserIdleSystemSleep);
    }
    else {
        printf("FAIL: CFDictionaryCreateMutable failed\n");
        return -1;
    }

    rc = IOPMPerformBlockWithAssertion(properties, ^{
                                       ret = checkAssertionState( );
                                       });

    if (rc != kIOReturnSuccess) {
        printf("FAIL: IOPMPerformBlockWithAssertion returned 0x%x\n", rc);
        return -1;
    }
    else {
        if (ret == 0) {
            printf("PASS: Block assertion is taken properly\n");
        }
        else {
            printf("FAIL: Couldn't find block assertion\n");
        }
    }

    return 0;
}


