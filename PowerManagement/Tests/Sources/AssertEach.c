/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "PMTestLib.h"


/*
 * IOPMAssertions test case.
 * Assert each known assertion once, then release it.
 * Then assert all assertions together, and release them.
 *
 */

#define kSystemMaxAssertionsAllowed 64

#define kLongNamesCount  2

#define kKnownGoodAssertionType     CFSTR("NoDisplaySleepAssertion")

const char *LongNames[] =
    {
"••000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000•••••••••••••••00000",
        "00005555555566666666666666667777777777777777700000000000AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA000000000000000XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
    };


#define kFunnyNamesCount    8

const char *funnyAssertionNames[] = 
    {
        "AssertionZero",
        "AssertionThatIsVeryLong.WhatTheHellAreYouLookingAt.WaitItGetsEvenLonger.",
        "???????????????????????????????????????????????????????????????????????????????????????????",
        "blah",
        "test one, test two. uh huh uh huh.",
        "hi.",
        "aura real",
        "test test"
    };

static CFStringRef createFunnyAssertionName(void)
{      
    static int i = 0;
    
    return  CFStringCreateWithCString(0, 
                        funnyAssertionNames[i++ % kFunnyNamesCount], 
                        kCFStringEncodingMacRoman);
}

static CFStringRef createLongAssertionName(void)
{
    static int i = 0;

    return CFStringCreateWithCString(0, 
                        LongNames[i++ % kLongNamesCount],    
                        kCFStringEncodingMacRoman);
}

static bool AssertionIsSupported(CFStringRef assertionname)
{
    // Assertion type EnableIdleSleep is unsupported on desktop. Do not run it.
    return !CFEqual(assertionname, kIOPMAssertionTypeEnableIdleSleep);
}

void Test_runthroughonce(void);

int main()
{
    IOReturn        ret = 0;
    
     ret = PMTestInitialize("PMAssertions", "com.apple.iokit.powertesting");
     if(kIOReturnSuccess != ret)
     {
         fprintf(stderr,"PMTestInitialize failed with IOReturn error code 0x%08x\n", ret);
         exit(-1);
     }

    Test_runthroughonce();

    return 0;
}


void Test_runthroughonce(void)
{
    CFDictionaryRef     currentAssertionsStatus = NULL;
    CFMutableDictionaryRef  editedAssertionsStatus = NULL;
    IOReturn            ret;
    CFStringRef         *listAssertions = NULL;
    int                 assertionsCount = 0;
    IOPMAssertionID     *assertionIDArray = NULL;
    int                 i;

    // 
    // Test setup
    //

    ret = IOPMCopyAssertionsStatus(&currentAssertionsStatus);
    if ((ret != kIOReturnSuccess) && (NULL == currentAssertionsStatus)) 
	{
        fprintf(stderr, "Error return ret = 0x%08x currentAssertionsStatus dictionary = %p",
            ret, currentAssertionsStatus);
    }
    
    editedAssertionsStatus = CFDictionaryCreateMutableCopy(0, 0, currentAssertionsStatus);
    CFRelease(currentAssertionsStatus);
    currentAssertionsStatus = NULL;
    
    CFDictionaryRemoveValue(editedAssertionsStatus, kIOPMAssertionTypeEnableIdleSleep);
    
    assertionsCount = CFDictionaryGetCount(editedAssertionsStatus);

    listAssertions = (CFStringRef *)calloc(assertionsCount, sizeof(void *));

    assertionIDArray = (IOPMAssertionID *)calloc(assertionsCount, sizeof(IOPMAssertionID));

    CFDictionaryGetKeysAndValues(
                        editedAssertionsStatus, 
                        (const void **)listAssertions,
                        (const void **)NULL);


    /**************************************************************************/
    /**************************************************************************/
    /* Test: Assert one at a time *****/

    PMTestLog("Assert and Release test will run %d assertions.", assertionsCount);
    
    for (i=0; i<assertionsCount; i++)
    {
        char    cStringName[100];
        
        CFStringGetCString(listAssertions[i], cStringName, 100, kCFStringEncodingMacRoman);

        PMTestLog("(%d) Creating assertion with name %s", i, cStringName);

        ret = IOPMAssertionCreate(
                            listAssertions[i],
                            kIOPMAssertionLevelOn, 
                            &assertionIDArray[i]);

        if (kIOReturnUnsupported == ret) {
            // This might be a valid, yet unsupported on this platform, assertion. Like EnableIdleSleep - it's 
            // only supported on embedded. Let's just work around that for now.
            listAssertions[i] = kKnownGoodAssertionType;
            PMTestLog("IOPMAssertionCreate #%d %s returns kIOReturnUnsupported(0x%08x) - skipping this assertion type from now on\n", i, cStringName, ret);
        } else if (kIOReturnSuccess != ret)
        {
            PMTestFail("Create assertion #%d %s returns 0x%08x", i, cStringName, ret);
        }

        if (kIOReturnSuccess == ret)
        {
            ret = IOPMAssertionRelease(assertionIDArray[i]);
            if (kIOReturnSuccess != ret) {
                PMTestFail("Release assertion #%d %s returns 0x%08x", i, cStringName, ret);
            }
        }
    }

    PMTestPass("AssertAndReleaseTest");
    
    
/***** All at once *****/


    PMTestLog("Creating all %d assertions simultaneously, then releasing them.", assertionsCount);

    for (i=0; i<assertionsCount; i++)
    {
        char    cStringName[100];
        
        CFStringGetCString(listAssertions[i], cStringName, 100, kCFStringEncodingMacRoman);


        ret = IOPMAssertionCreate(
                            listAssertions[i],
                            kIOPMAssertionLevelOn, 
                            &assertionIDArray[i]);
    
        if (kIOReturnSuccess != ret)
        {
            PMTestFail("Create assertion #%d %s returns 0x%08x", i, cStringName, ret);
        }
    }

    for (i=0; i<assertionsCount; i++)
    {
        char    cStringName[100];
        
        CFStringGetCString(listAssertions[i], cStringName, 100, kCFStringEncodingMacRoman);

        ret = IOPMAssertionRelease(assertionIDArray[i]);

        if (kIOReturnSuccess != ret) 
        {
            PMTestFail("Release assertion #%d %s returns 0x%08x", i, cStringName, ret);    
        }
    }
    
    PMTestPass("AssertAndReleaseSimultaneousTest");


/***** Assert Bogus Names *****/
    PMTestLog("Creating %d assertions with long names.", kLongNamesCount);

    for (i=0; i<kLongNamesCount; i++)
    {
        CFStringRef longName = createLongAssertionName();
    
        PMTestLog("(%d) Asserting bogus name", i);
	
        ret = IOPMAssertionCreateWithName(
                            listAssertions[0],
                            kIOPMAssertionLevelOn, 
                            longName,
                            &assertionIDArray[i]);
    
        if (kIOReturnSuccess != ret) {
            PMTestFail("Create long named assertion #%d returns 0x%08x", i, ret);
        }
        CFRelease(longName);

    
        ret = IOPMAssertionRelease(assertionIDArray[i]);
        if (kIOReturnSuccess != ret) {
            PMTestFail("IOPMAssertionRelease LONG (%d) returns 0x%08x", i, ret);
        }
    }
    PMTestPass("Assert Long Names");


/***** All at once with names *****/

    PMTestLog("Creating %d named assertions simultaneously, then releasing them.", assertionsCount);
    for (i=0; i<assertionsCount; i++)
    {
    
        CFStringRef     funnyName = createFunnyAssertionName();
        
        char    cfStringName[100];
        char    cfStringAssertionName[100];
        
        CFStringGetCString(listAssertions[i], cfStringAssertionName, 100, kCFStringEncodingMacRoman);
        CFStringGetCString(funnyName, cfStringName, 100, kCFStringEncodingMacRoman);

        ret = IOPMAssertionCreateWithName(
                            listAssertions[0],
                            kIOPMAssertionLevelOn, 
                            funnyName,
                            &assertionIDArray[i]);
    
        if (kIOReturnSuccess != ret)
        {
            PMTestFail("Create named assertion #%d name=%s type=%s returns 0x%08x", i, cfStringAssertionName, cfStringName, ret);
        }
        
        CFRelease(funnyName);
    }

    for (i=0; i<assertionsCount; i++)
    {
        char    cStringName[100];
        
        CFStringGetCString(listAssertions[i], cStringName, 100, kCFStringEncodingMacRoman);

        ret = IOPMAssertionRelease(assertionIDArray[i]);

        if (kIOReturnSuccess != ret)
        {
            PMTestFail("Release assertion #%d %s returns 0x%08x", i, cStringName, ret);    
        }
    }
    PMTestPass("Assert several named assertions at once (creation calls are serialized)");



/****** Assert & Release 300 assertions **********/

#define kDoManyManyAssertionsCount  300

    PMTestLog("Creating %d assertions simultaneously.",
            kDoManyManyAssertionsCount);

    IOPMAssertionID     manyAssertions[kDoManyManyAssertionsCount];
    
    bzero(manyAssertions, sizeof(manyAssertions));

    for (i=0; i<kDoManyManyAssertionsCount; i++)
    {
        IOReturn        ret;
        CFStringRef     funnyName = createFunnyAssertionName();


        // If we happen to assert an embedded only assertion, we don't want to get failures 
        // out of our tests for it.
        int useListIndex = i % assertionsCount;
        while (!AssertionIsSupported(listAssertions[ useListIndex ]))
        {
            // So we'll try the next assertion in the list.
            useListIndex++;
        }

        ret = IOPMAssertionCreateWithName(
                            listAssertions[ i % assertionsCount ],
                            kIOPMAssertionLevelOn,
                            funnyName,
                            &manyAssertions[i]);

		if (kIOReturnSuccess != ret)
		{
			PMTestFail("Asserting many assertions simultaneously failed: (i=%d)<(max=%d), ret=0x%08x, AssertionID=%d",
					i, kSystemMaxAssertionsAllowed, ret, manyAssertions[i]);
		}      

        CFRelease(funnyName);
    }

    for (i=0; i<kDoManyManyAssertionsCount; i++)
    {
        IOReturn        ret;

        ret = IOPMAssertionRelease(manyAssertions[i]);

            if (kIOReturnSuccess != ret)
            {
                PMTestFail("Assert many assertions simultaneously assertion-release failed: i=%d, ret=0x%08x, AssertionID=%d",
                        i, ret, manyAssertions[i]);
            }      
    }
    PMTestPass("Assert more assertions than allowed per-process");
}

