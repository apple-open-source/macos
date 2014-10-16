/*
 *
 */

/****************************************************************/
/****************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOReturn.h>

/****************************************************************/
/****************************************************************/



static void populateAssertionForTestingStruct(void);

static void assertReleaseLots(void);
static void assertAllWithNames(void);
static void assertBogusNames(void);
static void assertAllAtOnce(void);
static void assertOneAtATime(void);




#define kSystemMaxAssertionsAllowed 64
#define kKnownGoodAssertionType     CFSTR("NoDisplaySleepAssertion")



int main()
{
    printf("Executing powerassertions-general\n");

    populateAssertionForTestingStruct();

    assertReleaseLots();
    assertAllWithNames();
    assertBogusNames();
    assertAllAtOnce();
    assertOneAtATime();

    return 0;
}


static bool assertionIsSupported(CFStringRef assertionname)
{
    // Assertion type EnableIdleSleep is unsupported on desktop. Do not run it.
    return !CFEqual(assertionname, kIOPMAssertionTypeEnableIdleSleep);
}

static bool assertionRequiresEntitlement(CFStringRef a)
{
    return (CFEqual(a, kIOPMAssertInteractivePushServiceTask)
            || CFEqual(a, kIOPMAssertDisplayWake));
}


typedef struct {
    CFArrayRef      all;
    CFIndex         allCount;
    CFArrayRef      supported;
    CFIndex         supCount;
    IOPMAssertionID     *idArray;
} AssertionStruct;
static AssertionStruct assertions;

static CFStringRef nextAssertionTypeForTesting(bool supportedOnly) {
    static int sup = 0;
    static int all = 0;

    if (supportedOnly) {
        return CFArrayGetValueAtIndex(assertions.supported,
                                      sup++ % assertions.supCount);;
    } else {
        return CFArrayGetValueAtIndex(assertions.all,
                                      all++ % assertions.allCount);;
    }
}

static void populateAssertionForTestingStruct(void)
{
    CFMutableDictionaryRef      editedAssertionsStatus = NULL;
    IOReturn                    ret;
    CFStringRef                 *listAssertions = NULL;
    CFDictionaryRef             currentAssertionsStatus = NULL;

    ret = IOPMCopyAssertionsStatus(&currentAssertionsStatus);
    if ((ret != kIOReturnSuccess)
        || (NULL == currentAssertionsStatus))
    {
        printf("[FAIL] Error return ret = 0x%08x currentAssertionsStatus dictionary = %p\n",
                ret, currentAssertionsStatus);
        exit(1);
    }

    editedAssertionsStatus = CFDictionaryCreateMutableCopy(0, 0, currentAssertionsStatus);
    CFRelease(currentAssertionsStatus);

    if (editedAssertionsStatus) {
        CFDictionaryRemoveValue(editedAssertionsStatus, kIOPMAssertionTypeEnableIdleSleep);

        assertions.allCount = CFDictionaryGetCount(editedAssertionsStatus);

        listAssertions = (CFStringRef *)calloc(assertions.allCount, sizeof(void *));

        CFDictionaryGetKeysAndValues(editedAssertionsStatus,
                                     (const void **)listAssertions,
                                     (const void **)NULL);
        assertions.all = CFArrayCreate(0,
                                       (const void **)listAssertions,
                                       assertions.allCount,
                                       &kCFTypeArrayCallBacks);

        if (assertions.all)
        {
            CFMutableArrayRef tmp;
            tmp = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);

            for (int i=0; i<assertions.allCount; i++)
            {
                CFStringRef arr = CFArrayGetValueAtIndex(assertions.all, i);
                if (assertionIsSupported(arr)
                    && !assertionRequiresEntitlement(arr))
                {
                    CFArrayAppendValue(tmp, arr);
                }
            }

            assertions.supported = CFArrayCreateCopy(0, tmp);
            assertions.supCount = CFArrayGetCount(assertions.supported);

            assertions.idArray = (IOPMAssertionID *)calloc(assertions.allCount,
                                                           sizeof(IOPMAssertionID));

            CFRelease(tmp);
        }
    }

    if (!assertions.all || !assertions.supported) {
        printf("[FAIL] Pre-test initializiation failed.\n");
        exit(1);
    }
}




/* Test: Assert one at a time *****/
static void assertOneAtATime(void)
    {
    printf("Assert and Release test will run %ld assertions.\n", assertions.allCount);

    IOReturn ret;
    for (int i=0; i<assertions.allCount; i++)
    {
        char    cStringName[100];

        CFStringRef testAssertion = nextAssertionTypeForTesting(false);

        CFStringGetCString(testAssertion, cStringName, 100, kCFStringEncodingMacRoman);

        printf("(%d) Creating assertion with name %s\n", i, cStringName);

        ret = IOPMAssertionCreate(
                                  testAssertion,
                                  kIOPMAssertionLevelOn,
                                  &assertions.idArray[i]);


        if (kIOReturnUnsupported == ret) {
            // This might be a valid, yet unsupported on this platform, assertion. Like EnableIdleSleep - it's
            // only supported on embedded. Let's just work around that for now.
//            listAssertions[i] = kKnownGoodAssertionType;
            printf("IOPMAssertionCreate #%d %s returns kIOReturnUnsupported(0x%08x) - skipping\n", i, cStringName, ret);
        } else if (kIOReturnNotPrivileged == ret
                   && assertionRequiresEntitlement(testAssertion))
        {
            // These assertions require an entitlement, but this tool doesn't have them. That's OK.
            printf("Ignoring %s - this process isn't entitled.\n", cStringName);

        } else if (kIOReturnSuccess != ret)
        {
            printf("[FAIL] Create assertion #%d %s returns 0x%08x\n", i, cStringName, ret);
        }

        if (kIOReturnSuccess == ret)
        {
            ret = IOPMAssertionRelease(assertions.idArray[i]);
            if (kIOReturnSuccess != ret) {
                printf("[FAIL] Release assertion #%d %s returns 0x%08x. id=%d\n", i, cStringName, ret, assertions.idArray[i]);
            }
        }
    }

    printf("[PASS] AssertAndReleaseTest\n");
    }

    /***** All at once *****/
static void assertAllAtOnce(void)
{
    IOReturn ret;

    bzero(assertions.idArray, sizeof(assertions.idArray));
    printf("Creating all %ld assertions simultaneously, then releasing them.\n", assertions.allCount);

    for (int i=0; i<assertions.allCount; i++)
    {
        char    cStringName[100];
        CFStringRef     testAssertion = nextAssertionTypeForTesting(false);

        CFStringGetCString(testAssertion, cStringName, 100, kCFStringEncodingMacRoman);


        ret = IOPMAssertionCreate(
                                  testAssertion,
                                  kIOPMAssertionLevelOn,
                                  &assertions.idArray[i]);

        if (kIOReturnSuccess != ret
            && !assertionRequiresEntitlement(testAssertion))
        {
            printf("[FAIL] Create assertion #%d %s returns 0x%08x\n", i, cStringName, ret);
        }
    }

    for (int i=0; i<assertions.allCount; i++)
    {
        CFStringRef     testAssertion = nextAssertionTypeForTesting(false);;
        char    cStringName[100];

        if (assertions.idArray) {
            continue;
        }
        CFStringGetCString(testAssertion, cStringName, 100, kCFStringEncodingMacRoman);

        ret = IOPMAssertionRelease(assertions.idArray[i]);

        if (kIOReturnSuccess != ret)
        {
            printf("[FAIL] Release assertion #%d %s returns 0x%08x\n", i, cStringName, ret);
        }
    }

    printf("[PASS] AssertAndReleaseSimultaneousTest\n");
}




static const char *LongNames[] =
{
"••000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000•••••••••••••••00000",
"00005555555566666666666666667777777777777777700000000000AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA000000000000000XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
};
static const int kLongNamesCount = sizeof(LongNames) / sizeof(char *);

static CFStringRef createLongAssertionName(void)
{
    static int i = 0;

    return CFStringCreateWithCString(0,
                                     LongNames[i++ % kLongNamesCount],
                                     kCFStringEncodingMacRoman);
}


/***** Assert Bogus Names *****/
static void assertBogusNames(void)
{
    IOReturn ret;
    printf("Creating %d assertions with long names.\n", kLongNamesCount);
    bzero(assertions.idArray, sizeof(assertions.idArray));

    for (int i=0; i<kLongNamesCount; i++)
    {
        CFStringRef longName = createLongAssertionName();

        CFStringRef testAssertion = kKnownGoodAssertionType;

        printf("(%d) Asserting bogus name\n", i);

        ret = IOPMAssertionCreateWithName(
                                          testAssertion,
                                          kIOPMAssertionLevelOn,
                                          longName,
                                          &assertions.idArray[i]);

        if (kIOReturnSuccess != ret) {
            printf("[FAIL] Create long named assertion #%d returns 0x%08x\n", i, ret);
        }
        CFRelease(longName);


        ret = IOPMAssertionRelease(assertions.idArray[i]);
        if (kIOReturnSuccess != ret) {
            printf("[FAIL] IOPMAssertionRelease LONG (%d) returns 0x%08x\n", i, ret);
        }
    }
    printf("[PASS] Assert Long Names\n");
}

/***** All at once with names *****/


const char *funnyAssertionNames[] =
{
    "AssertionZero",
    "AssertionThatIsVeryLong.AssertionThatIsVeryLong.WaitItGetsEvenLonger.AssertionThatIsVeryLong.",
    "???????????????????????????????????????????????????????????????????????????????????????????",
    "blah",
    "test one, test two. uh huh uh huh.",
    "hi.",
    ";;;;;LLLLLLLLPPPPPPPPPPPP    real",
    "test test"
};

static int kFunnyNamesCount = sizeof(funnyAssertionNames)/sizeof(char *);

static CFStringRef createFunnyAssertionName(void)
{
    static int i = 0;
    return  CFStringCreateWithCString(0,
                                      funnyAssertionNames[i++ % kFunnyNamesCount],
                                      kCFStringEncodingMacRoman);
}

static void assertAllWithNames(void)
{
    IOReturn ret;
    printf("Creating %ld named assertions simultaneously, then releasing them.\n", assertions.allCount);
    for (int i=0; i<assertions.allCount; i++)
    {

        CFStringRef     funnyName = createFunnyAssertionName();

        CFStringRef     testAssertion = kKnownGoodAssertionType;
        char             cfStringName[100];
        char             cfStringAssertionName[100];

        CFStringGetCString(testAssertion, cfStringAssertionName, 100, kCFStringEncodingMacRoman);
        CFStringGetCString(funnyName, cfStringName, 100, kCFStringEncodingMacRoman);

        ret = IOPMAssertionCreateWithName(
                                          testAssertion,
                                          kIOPMAssertionLevelOn,
                                          funnyName,
                                          &assertions.idArray[i]);

        if (kIOReturnSuccess != ret)
        {
            printf("[FAIL] Create named assertion #%d name=%s type=%s returns 0x%08x\n", i, cfStringAssertionName, cfStringName, ret);
        }

        CFRelease(funnyName);
    }


    for (int i=0; i<assertions.allCount; i++)
    {
        ret = IOPMAssertionRelease(assertions.idArray[i]);

        if (kIOReturnSuccess != ret)
        {
            printf("[FAIL] Release assertion #%d returns 0x%08x\n", i, ret);
        }
    }
    printf("[PASS] Assert several named assertions at once (creation calls are serialized)\n");
}


    /****** Assert & Release 300 assertions **********/
static void assertReleaseLots(void)
{
    #define kDoManyManyAssertionsCount  300

    printf("Creating %d assertions simultaneously.\n",
              kDoManyManyAssertionsCount);

    IOPMAssertionID     manyAssertions[kDoManyManyAssertionsCount];

    bzero(manyAssertions, sizeof(manyAssertions));

    for (int i=0; i<kDoManyManyAssertionsCount; i++)
    {
        IOReturn        ret;
        CFStringRef     funnyName = createFunnyAssertionName();


        // If we happen to assert an embedded only assertion, we don't want to get failures
        // out of our tests for it.
        CFStringRef useAssertionType = nextAssertionTypeForTesting(true);

        ret = IOPMAssertionCreateWithName(
                                          useAssertionType,
                                          kIOPMAssertionLevelOn,
                                          funnyName,
                                          &manyAssertions[i]);

		if (kIOReturnSuccess != ret)
		{
            char buf[100];
            CFStringGetCString(useAssertionType, buf, sizeof(buf), kCFStringEncodingUTF8);
			printf("[FAIL] Asserting many assertions simultaneously failed: (i=%d)<(max=%d), ret=0x%08x, AssertionID=%d type=%s\n",
                       i, kSystemMaxAssertionsAllowed, ret, manyAssertions[i], buf);
		}      
        
        CFRelease(funnyName);
    }
    
    for (int i=0; i<kDoManyManyAssertionsCount; i++)
    {
        IOReturn        ret;
        
        ret = IOPMAssertionRelease(manyAssertions[i]);
        
        if (kIOReturnSuccess != ret)
        {
            printf("[FAIL] Assert many assertions simultaneously assertion-release failed: i=%d, ret=0x%08x, AssertionID=%d\n",
                       i, ret, manyAssertions[i]);
        }      
    }
    printf("[PASS] Assert more assertions than allowed per-process\n");
}

