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

#include <XILog/XILog.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>


/*
 * IOPMAssertions test case.
 * Assert each known assertion once, then release it.
 * Then assert all assertions together, and release them.
 *
 */

#define kSystemMaxAssertionsAllowed 64

#define kInvalidNamesCount  2

const char *invalidNames[] =
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

static CFStringRef createInvalidAssertionName(void)
{
    static int i = 0;

    return CFStringCreateWithCString(0, 
                        invalidNames[i++ % kInvalidNamesCount],    
                        kCFStringEncodingMacRoman);
};

int main()
{
     char *XIconfig = NULL;
     int XIecho = true;
     int XIxml = false;

	char *XILogPath = NULL; //"~/Desktop/AssertEach.log";

    // 
    // XILog Initialization
    //
     XILogRef logRef = XILogOpenLog(XILogPath, "PMAssertions", "com.apple.iokit.ethan", XIconfig, XIxml, XIecho);
     if(logRef == NULL)
     {
         fprintf(stderr,"Couldn't create log: %s", XILogPath ? XILogPath : "(NULL)");
         exit(-1);
     }

    Test_runthroughonce(logRef);

    Test_loderunner();

    return 0;
}


void Test_runthrughonce(XILogRef logRef)
{
    CFDictionaryRef     currentAssertionsStatus = NULL;
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
    
    assertionsCount = CFDictionaryGetCount(currentAssertionsStatus);

    listAssertions = (CFStringRef *)calloc(assertionsCount, sizeof(void *));

    assertionIDArray = (IOPMAssertionID *)calloc(assertionsCount, sizeof(IOPMAssertionID));

    CFDictionaryGetKeysAndValues(
                        currentAssertionsStatus, 
                        (const void **)listAssertions,
                        (const void **)NULL);


    /**************************************************************************/
    /**************************************************************************/
    /* Test: Assert one at a time *****/

    XILogBeginTestCase(logRef, "AssertAndReleaseTest", "Can we assert and release assertions individually.");
    
    XILogMsg("Assert and Release test will run %d assertions.", assertionsCount);
    
    for (i=0; i<assertionsCount; i++)
    {
        char    cStringName[100];
        
        CFStringGetCString(listAssertions[i], cStringName, 100, kCFStringEncodingMacRoman);

        XILogMsg("(%d) Creating assertion with name %s", i, cStringName);

        ret = IOPMAssertionCreate(
                            listAssertions[i],
                            kIOPMAssertionLevelOn, 
                            &assertionIDArray[i]);

        if (kIOReturnSuccess != ret)
        {
            XILogErr("Create assertion #%d %s returns 0x%08x", i, cStringName, ret);
        }

        ret = IOPMAssertionRelease(assertionIDArray[i]);
        if (kIOReturnSuccess != ret) {
            XILogErr("Release assertion #%d %s returns 0x%08x", i, cStringName, ret);
        }
		
    }

    XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);
    
/***** All at once *****/

    XILogBeginTestCase(logRef, "AssertAndReleaseSimultaneousTest", "Can we assert and release assertions individually.");
    
    XILogMsg("Creating all %d assertions simultaneously, then releasing them.", assertionsCount);

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
            XILogErr("Create assertion #%d %s returns 0x%08x", i, cStringName, ret);
        }
    }

    for (i=0; i<assertionsCount; i++)
    {
        char    cStringName[100];
        
        CFStringGetCString(listAssertions[i], cStringName, 100, kCFStringEncodingMacRoman);

        ret = IOPMAssertionRelease(assertionIDArray[i]);

        if (kIOReturnSuccess != ret) 
        {
            XILogErr("Release assertion #%d %s returns 0x%08x", i, cStringName, ret);    
        }
    }
    
    XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);

#if 1
        return 0;
    }
#else

/***** Assert Bogus Names *****/
    XILogBeginTestCase(logRef, "AssertBogus", "Assert invalid assertion names; expecting errors.");
    XILogMsg("Creating %d assertions with invalid names.", kInvalidNamesCount);

    for (i=0; i<kInvalidNamesCount; i++)
    {
        CFStringRef invalidName = createInvalidAssertionName();
    
		XILogMsg("(%d) Asserting bogus name %s", i, CFStringGetCStringPtr(invalidName, kCFStringEncodingMacRoman));
	
        ret = IOPMAssertionCreateWithName(
                            listAssertions[0],
                            kIOPMAssertionLevelOn, 
                            invalidName,
                            &assertionIDArray[i]);
    
        if (kIOReturnBadArgument != ret) {
            XILogErr("Create invalid named assertion #%d returns 0x%08x", i, ret);
        }
        CFRelease(invalidName);
    }
    XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);


/***** All at once with names *****/

    XILogBeginTestCase(logRef, "AssertWithNameSimultaneous", "Assert several assertions with names simultaneously.");
    XILogMsg("Creating %d named assertions simultaneously, then releasing them.", assertionsCount);
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
            XILogErr("Create named assertion #%d name=%s type=%s returns 0x%08x", i, cfStringAssertionName, cfStringName, ret);
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
            XILogErr("Release assertion #%d %s returns 0x%08x", i, cStringName, ret);    
        }
    }
    XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);



/****** Assert & Release 70>Max assertions **********/

#define kDoManyManyAssertionsCount  70

    XILogBeginTestCase(logRef, "AssertMoreThanAllowed", "Assert greater than MAX_ASSERTIONS = 64.");
    XILogMsg("Creating %d assertions to exceed the system's per-process PM assertion limit.",
            kDoManyManyAssertionsCount);

    IOPMAssertionID     manyAssertions[kDoManyManyAssertionsCount];
    
    bzero(manyAssertions, sizeof(manyAssertions));

    for (i=0; i<kDoManyManyAssertionsCount; i++)
    {
        IOReturn        ret;
        CFStringRef     funnyName = createFunnyAssertionName();

        ret = IOPMAssertionCreateWithName(
                            listAssertions[ i % assertionsCount ],
                            kIOPMAssertionLevelOn,
                            funnyName,
                            &manyAssertions[i]);

        if (i < kSystemMaxAssertionsAllowed)
        {

            if (kIOReturnSuccess != ret)
            {
                XILogErr("Assert 70+ assertions failed: i=%d, ret=0x%08x, AssertionID=%d",
                        i, ret, manyAssertions[i]);
            }      
        } else {
            if (kIOReturnNoMemory != ret) 
            {
                XILogErr("Assert 70+ assertions failed (ret != 0x%08x): i=%d, ret=0x%08x, AssertionID=%d",
                    kIOReturnNoMemory, i, ret, manyAssertions[i]);
            }
        }

        CFRelease(funnyName);
    }

    for (i=0; i<kDoManyManyAssertionsCount; i++)
    {
        IOReturn        ret;

        ret = IOPMAssertionRelease(manyAssertions[i]);

        if (i < kSystemMaxAssertionsAllowed)
        {

            if (kIOReturnSuccess != ret)
            {
                XILogErr("Assert 70+ assertion-release failed: i=%d, ret=0x%08x, AssertionID=%d",
                        i, ret, manyAssertions[i]);
            }      
        } else {
            if (kIOReturnNotFound != ret)
            {
                XILogErr("Assert 70+ assertion-release failed (ret != 0x%08x): i=%d, ret=0x%08x, AssertionID=%d",
                        kIOReturnNoMemory, i, ret, manyAssertions[i]);
            }
        }
    }
    XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);
}



#endif // MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
