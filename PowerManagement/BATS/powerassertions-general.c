/*
 *
 */

/****************************************************************/
/****************************************************************/
#include <SystemConfiguration/SCValidation.h>
#include <spawn.h>
#include <IOKit/IOReportTypes.h>
#include <IOReport.h>
#include <sys/types.h>
#include <libproc.h>
#include "PMtests.h"

/****************************************************************/
/****************************************************************/

// osx_xcr cc -o /tmp/powerassertions-general powerassertions-general.c  -framework IOKit -framework CoreFoundation -lIOReport


static void populateAssertionForTestingStruct(void);

static void assertReleaseLots(void);
static void assertAllWithNames(void);
static void assertBogusNames(void);
static void assertAllAtOnce(void);
static void assertOneAtATime(void);
static void assertionOnOff(void);
static void copyDeviceRestartPreventers(void);
static void test_IOPMCopyAssertionActivityAggregate();
static void test_assertionExceptions();
static void test_IOPMPerformBlockWithAssertion();
static void test_sysQualifiers();
static void testUserActivityAssertion();

int gPassCnt = 0, gFailCnt = 0;


#define kSystemMaxAssertionsAllowed 64
#define kKnownGoodAssertionType     CFSTR("NoDisplaySleepAssertion")


int main()
{

    START_TEST("Test Assertions APIs\n");

    dispatch_async(dispatch_get_main_queue(), ^{
        populateAssertionForTestingStruct();

        assertReleaseLots();
        assertAllWithNames();
        assertBogusNames();
        assertAllAtOnce();
        assertOneAtATime();
        copyDeviceRestartPreventers();

        IOPMAssertionSetBTCollection(true);

        // Start assertionOnOff() few seconds later to allow dispatch main queue process the
        // notification posted by the IOPMAssertionSetBTCollection
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2LL * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            assertionOnOff();
            IOPMAssertionSetBTCollection(false);
            SUMMARY("Test Assertions APIs");
            exit(0);
        });

    });
    test_IOPMCopyAssertionActivityAggregate();
    test_assertionExceptions();
    test_IOPMPerformBlockWithAssertion();
    test_sysQualifiers();
    testUserActivityAssertion();

    dispatch_main();
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

    START_TEST_CASE("Test IOPMCopyAssertionsStatus\n");

    ret = IOPMCopyAssertionsStatus(&currentAssertionsStatus);
    if ((ret != kIOReturnSuccess)
        || (NULL == currentAssertionsStatus))
    {
        FAIL("Error return ret = 0x%08x currentAssertionsStatus dictionary = %p\n",
                ret, currentAssertionsStatus);
        goto exit;
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

        CFRelease(editedAssertionsStatus);
    }

    if (!assertions.all || !assertions.supported) {
        FAIL("Pre-test initializiation failed.\n");
        goto exit;
    }
    PASS("IOPMCopyAssertionsStatus\n");
exit:
    return;
}



static CFTypeRef copyAssertionProperty(CFStringRef type, IOPMAssertionID id, CFStringRef prop)
{
    IOReturn ret;
    CFNumberRef numRef;
    IOPMAssertionID num;
    CFTypeRef value = NULL;
    CFTypeRef valueCopy = NULL;
    CFArrayRef  assertionsArray = NULL;
    CFIndex i;

    ret = IOPMCopyAssertionsByType(type, &assertionsArray);
    if (ret != kIOReturnSuccess) {
        FAIL("IOPMCopyAssertionsByType returned 0x%x\n", ret);
        goto exit;
    }

    for (i = 0; i < CFArrayGetCount(assertionsArray); i++) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex(assertionsArray, i);
        if (dict == NULL) {
            continue;
        }

        numRef = CFDictionaryGetValue(dict, kIOPMAssertionIdKey);
        if (isA_CFNumber(numRef)) {
            CFNumberGetValue(numRef, kCFNumberSInt32Type, &num);

            if (num == id) {
                value = CFDictionaryGetValue(dict, prop);
                goto exit;
            }
        }
    }

    if (i >= CFArrayGetCount(assertionsArray)) {
        goto exit;
    }

exit:
    if (value) {
        CFTypeID typeID = CFGetTypeID(value);
        if (typeID == CFStringGetTypeID()) {
            valueCopy = CFStringCreateCopy(NULL, value);
        }
        else if (typeID == CFArrayGetTypeID()) {
            valueCopy = CFArrayCreateCopy(NULL, value);
        }
        else {
            FAIL("Unexpected CF type %ld\n", typeID);
        }
    }

    if (assertionsArray) {
        CFRelease(assertionsArray);
    }

    return valueCopy;
}


/* Test: Assert one at a time *****/
static void assertOneAtATime(void)
{
    START_TEST_CASE("Test creating one assertion at a time\n");
    LOG("Assert and Release test will run %ld assertions.\n", assertions.allCount);

    IOReturn ret;
    for (int i=0; i<assertions.allCount; i++)
    {
        char    cStringName[100];

        CFStringRef testAssertion = nextAssertionTypeForTesting(false);

        CFStringGetCString(testAssertion, cStringName, 100, kCFStringEncodingMacRoman);

        LOG("(%d) Creating assertion with name %s\n", i, cStringName);

        ret = IOPMAssertionCreateWithName(
                                          testAssertion,
                                          kIOPMAssertionLevelOn,
                                          CFSTR("Test one assertion creation"),
                                          &assertions.idArray[i]);


        if (kIOReturnUnsupported == ret) {
            // This might be a valid, yet unsupported on this platform, assertion. Like EnableIdleSleep - it's
            // only supported on embedded. Let's just work around that for now.
            //            listAssertions[i] = kKnownGoodAssertionType;
            LOG("IOPMAssertionCreate #%d %s returns kIOReturnUnsupported(0x%08x) - skipping\n", i, cStringName, ret);
        } else if (kIOReturnNotPrivileged == ret
                   && assertionRequiresEntitlement(testAssertion))
        {
            // These assertions require an entitlement, but this tool doesn't have them. That's OK.
            LOG("Ignoring %s - this process isn't entitled.\n", cStringName);

        } else if (kIOReturnSuccess != ret)
        {
            FAIL("Create assertion #%d %s returns 0x%08x\n", i, cStringName, ret);
            goto exit;
        }

        if (kIOReturnSuccess == ret)
        {
            ret = IOPMAssertionRelease(assertions.idArray[i]);
            if (kIOReturnSuccess != ret) {
                FAIL("Release assertion #%d %s returns 0x%08x. id=%d\n", i, cStringName, ret, assertions.idArray[i]);
                goto exit;
            }
        }
    }

    PASS("AssertAndReleaseTest\n");
exit:
    return;
}

    /***** All at once *****/
static void assertAllAtOnce(void)
{
    IOReturn ret;

    bzero(assertions.idArray, assertions.allCount * sizeof(*assertions.idArray));
    START_TEST_CASE("Test creating multiple assertions\n");
    LOG("Creating all %ld assertions simultaneously, then releasing them.\n", assertions.allCount);

    for (int i=0; i<assertions.allCount; i++)
    {
        char    cStringName[100];
        CFStringRef     testAssertion = nextAssertionTypeForTesting(false);

        CFStringGetCString(testAssertion, cStringName, 100, kCFStringEncodingMacRoman);


        ret = IOPMAssertionCreateWithName(
                                          testAssertion,
                                          kIOPMAssertionLevelOn,
                                          CFSTR("Test all assertions creation"),
                                          &assertions.idArray[i]);

        if (kIOReturnSuccess != ret
            && !assertionRequiresEntitlement(testAssertion))
        {
            FAIL("Create assertion #%d %s returns 0x%08x\n", i, cStringName, ret);
            goto exit;
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
            FAIL("Release assertion #%d %s returns 0x%08x\n", i, cStringName, ret);
            goto exit;
        }
    }

    PASS("AssertAndReleaseSimultaneousTest\n");
exit:
    return;
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
    START_TEST_CASE("Test Creating assertions with long names\n");

    LOG("Creating %d assertions with long names.\n", kLongNamesCount);
    bzero(assertions.idArray, assertions.allCount * sizeof(*assertions.idArray));

    for (int i=0; i<kLongNamesCount; i++)
    {
        CFStringRef longName = createLongAssertionName();

        CFStringRef testAssertion = kKnownGoodAssertionType;

        LOG("(%d) Asserting bogus name\n", i);

        ret = IOPMAssertionCreateWithName(
                                          testAssertion,
                                          kIOPMAssertionLevelOn,
                                          longName,
                                          &assertions.idArray[i]);

        if (kIOReturnSuccess != ret) {
            FAIL("Create long named assertion #%d returns 0x%08x\n", i, ret);
            CFRelease(longName);
            goto exit;
        }
        CFRelease(longName);


        ret = IOPMAssertionRelease(assertions.idArray[i]);
        if (kIOReturnSuccess != ret) {
            FAIL("IOPMAssertionRelease LONG (%d) returns 0x%08x\n", i, ret);
            goto exit;
        }
    }
    PASS("Assert Long Names\n");
exit:
    return;
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
    START_TEST_CASE("Test IOPMAssertionCreateWithName\n");
    LOG("Creating %ld named assertions simultaneously, then releasing them.\n", assertions.allCount);
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
            FAIL("Create named assertion #%d name=%s type=%s returns 0x%08x\n", i, cfStringAssertionName, cfStringName, ret);
            CFRelease(funnyName);
            goto exit;
        }

        CFRelease(funnyName);
    }


    for (int i=0; i<assertions.allCount; i++)
    {
        ret = IOPMAssertionRelease(assertions.idArray[i]);

        if (kIOReturnSuccess != ret)
        {
            FAIL("Release assertion #%d returns 0x%08x\n", i, ret);
            goto exit;
        }
    }
    PASS("Assert several named assertions at once (creation calls are serialized)\n");

exit:
    return;
}


    /****** Assert & Release 300 assertions **********/
static void assertReleaseLots(void)
{
    #define kDoManyManyAssertionsCount  300

    START_TEST_CASE("Create and release multiple assertions\n");
    LOG("Creating %d assertions simultaneously.\n",
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
            CFRelease(funnyName);
			FAIL("Asserting many assertions simultaneously failed: (i=%d)<(max=%d), ret=0x%08x, AssertionID=%d type=%s\n",
                       i, kSystemMaxAssertionsAllowed, ret, manyAssertions[i], buf);
            goto exit;
		}

        CFRelease(funnyName);
    }

    for (int i=0; i<kDoManyManyAssertionsCount; i++)
    {
        IOReturn        ret;

        ret = IOPMAssertionRelease(manyAssertions[i]);

        if (kIOReturnSuccess != ret)
        {
            FAIL("Assert many assertions simultaneously assertion-release failed: i=%d, ret=0x%08x, AssertionID=%d\n",
                       i, ret, manyAssertions[i]);
            goto exit;
        }
    }
    PASS("Assert more assertions than allowed per-process\n");

exit:
    return;
}


/*
 * Looks for the assertion with specified in the assertionsDict, under the specified pid.
 * The assertionsDict is obtained by calling IOPMCopyAsserionysByProcess() or IOPMCopyInactiveAssertionsByProcess.
 */
static IOReturn checkForAssertion(CFDictionaryRef assertionsDict, IOPMAssertionID id, pid_t pid)
{
    IOReturn rc = kIOReturnSuccess;
    CFIndex i;
    CFNumberRef pidCF = NULL;

    if (!isA_CFDictionary(assertionsDict)) {
        FAIL("assertionsDict passed to checkForAssertion() is invalid\n");
        rc = kIOReturnBadArgument;
        goto exit;
    }

    pidCF = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
    if (!isA_CFNumber(pidCF)) {
        FAIL("assertionsDict failed to create CFNumber for pid %d\n", pid);
        rc = kIOReturnError;
        goto exit;
    }

    CFArrayRef pidAssertions = CFDictionaryGetValue(assertionsDict, pidCF);

    for (i = 0; i < CFArrayGetCount(pidAssertions); i++ ) {

        IOPMAssertionID currId;
        CFDictionaryRef assertion = CFArrayGetValueAtIndex(pidAssertions, i);

        CFNumberRef idCF = CFDictionaryGetValue(assertion, kIOPMAssertionIdKey);
        CFNumberGetValue(idCF, kCFNumberSInt32Type, &currId);

        if (currId == id) {
            break;
        }

    }

    if (i >= CFArrayGetCount(pidAssertions)) {
        FAIL("Assertion with id 0x%x is not found in assertion Dictionary\n", id);
        rc = kIOReturnError;
    }
exit:
    if (pidCF) {
        CFRelease(pidCF);
    }

    return rc;
}


static void assertionOnOff(void)
{

    IOReturn ret;
    IOPMAssertionID id;

    CFStringRef name = CFSTR("Test assertion on off");
    CFStringRef nameInProps = NULL;
    CFArrayRef backtrace1 = NULL;
    CFArrayRef backtrace2 = NULL;
    CFNumberRef numRef = NULL;
    bool differs = true;
    CFDictionaryRef assertionsDict = NULL;

    START_TEST_CASE("Test changing assertion level\n");

    ret = IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleSystemSleep, kIOPMAssertionLevelOff, name, &id);

    if (ret != kIOReturnSuccess) {
        FAIL("assertOnOff: Failed to create assertion with initial state off. ret:0x%x\n", ret);
        return;
    }

    if ((nameInProps = copyAssertionProperty(kIOPMAssertionTypePreventUserIdleSystemSleep, id, kIOPMAssertionNameKey)) != NULL) {
        FAIL("assertOnOff: Assertion detail is unexpected when it is off\n");
        goto exit;
    }

    INT_TO_CFNUMBER(numRef, kIOPMAssertionLevelOn);
    ret = IOPMAssertionSetProperty(id, kIOPMAssertionLevelKey, numRef);
    if (ret != kIOReturnSuccess) {
        FAIL("assertOnOff: Failed to set the assertion level property. ret:0x%x\n", ret);
        goto exit;
    }
    CFRelease(numRef); numRef = NULL;

    nameInProps = copyAssertionProperty(kIOPMAssertionTypePreventUserIdleSystemSleep, id, kIOPMAssertionNameKey);
    if ((nameInProps == NULL) || (CFStringCompare(name, nameInProps, 0) != kCFCompareEqualTo)) {
        FAIL("assertOnOff: Assertion name is not found or unexpected value after turning the assertion On\n");
        goto exit;
    }

    backtrace1 = copyAssertionProperty(kIOPMAssertionTypePreventUserIdleSystemSleep, id, kIOPMAssertionCreatorBacktrace);
    if (backtrace1 == NULL) {
        FAIL("assertOnOff: Assertion backtrace is not found after turning the assertion On\n");
        goto exit;
    }

    // Turn it off and on once more
    INT_TO_CFNUMBER(numRef, kIOPMAssertionLevelOff);
    ret = IOPMAssertionSetProperty(id, kIOPMAssertionLevelKey, numRef);
    if (ret != kIOReturnSuccess) {
        FAIL("assertOnOff: Failed to set the assertion level property. ret:0x%x\n", ret);
        goto exit;
    }
    CFRelease(numRef); numRef = NULL;

    // Check and make sure IOPMCopyInactiveAssertionsByProcess returns this assertion
    ret = IOPMCopyInactiveAssertionsByProcess(&assertionsDict);
    if (ret != kIOReturnSuccess) {
        FAIL("assertOnOff: IOPMCopyInactiveAssertionsByProcess returned 0x%x\n", ret);
        goto exit;
    }

    ret = checkForAssertion(assertionsDict, id, getpid());
    if (ret != kIOReturnSuccess) {
        FAIL("assertOnOff: Failed to find the inactive asseertion in dictionary returned by IOPMCopyInactiveAssertionsByProcess\n");
        goto exit;
    }

    INT_TO_CFNUMBER(numRef, kIOPMAssertionLevelOn);
    ret = IOPMAssertionSetProperty(id, kIOPMAssertionLevelKey, numRef);
    if (ret != kIOReturnSuccess) {
        FAIL("assertOnOff: Failed to set the assertion level property. ret:0x%x\n", ret);
        goto exit;
    }
    CFRelease(numRef); numRef = NULL;

    // The new backtrace must be different from the previous one
    backtrace2 = copyAssertionProperty(kIOPMAssertionTypePreventUserIdleSystemSleep, id, kIOPMAssertionCreatorBacktrace);
    if (backtrace2 == NULL) {
        FAIL("assertOnOff: Assertion backtrace is not found turning the assertion On\n");
        goto exit;
    }

    // Compare both the backtraces. They should be different
    if (CFArrayGetCount(backtrace1) == CFArrayGetCount(backtrace2)) {
        differs = false;
        for (CFIndex i = 0; i < CFArrayGetCount(backtrace1); i++) {
            CFStringRef fr1, fr2;
            fr1 = CFArrayGetValueAtIndex(backtrace1, i);
            fr2 = CFArrayGetValueAtIndex(backtrace2, i);

            if (CFStringCompare(fr1, fr2, 0) != kCFCompareEqualTo) {
                differs = true;
                break;
            }
        }
    }
    if (differs == false) {
        FAIL("assertOnOff: Assertion backtrace string hasn't changed after assertion is truned off and on\n");
        goto exit;
    }

    PASS("AssertOnOff test\n");

exit:
    if (numRef) {
        CFRelease(numRef);
    }
    if (id) {
        IOPMAssertionRelease(id);
    }

    if (nameInProps) {
        CFRelease(nameInProps);
    }
    if (backtrace1) {
        CFRelease(backtrace1);
    }
    if (backtrace2) {
        CFRelease(backtrace2);
    }
    if (assertionsDict) {
        CFRelease(assertionsDict);
    }

    return;

}

static void copyDeviceRestartPreventers(void)
{
    IOReturn ret;
    CFArrayRef arr = NULL;
    
    START_TEST_CASE("Test IOPMCopyDeviceRestartPreventers\n");
    ret = IOPMCopyDeviceRestartPreventers(&arr);
    
    if (ret != kIOReturnSuccess) {
        FAIL("copyDeviceRestartPreventers: IOPMCopyDeviceRestartPreventers failed. ret:0x%x\n", ret);
        goto exit;
    }
    
    //CFShow(arr);
    PASS("copyDeviceRestartPreventers test\n");
    
exit:
    if (arr) {
        CFRelease(arr);
    }
    
    return;
}



static void test_IOPMCopyAssertionActivityAggregate()
{
#define kIterations 3
    IOReturn ret;
    CFDictionaryRef basis = NULL;
    CFDictionaryRef update = NULL;
    IOReportSampleRef   delta;
    __block int cnt = 0;
    unsigned int interval = 0;
    pid_t   pid[kIterations], *pidPtr;
    int     rc;

    START_TEST_CASE("IOPMSetAssertionActivityAggregate\n");
    pidPtr = pid;
    memset(pid, 0, sizeof(pid));
    interval = 5;

    ret = IOPMSetAssertionActivityAggregate(true);
    if (ret != kIOReturnSuccess) {
        FAIL("Failed to enable aggregation of assertion activity\n");
        return;
    }


    basis = IOPMCopyAssertionActivityAggregate( );

    /*
     * Do 'kIterations'. On each iteration, create an assertion for 3 secs
     * and sleep for 5 secs.
     * As we are sleeping longer than the assertion duraion, each update
     * data from IOPMCopyAssertionActivityAggregate should show ony one pid
     */
    for (int i = 0; i < kIterations; i++) {

        rc = posix_spawn(&pid[i], "/usr/bin/caffeinate", NULL, NULL, (char *[]){"/usr/bin/caffeinate", "-t", "3", NULL}, NULL);
        if (rc != 0) {
            FAIL("Failed to spawn a caffeinate cmd(%d)\n", rc);
            goto exit;
        }
        LOG("Iteration %d. Sleep for %d secs\n", i, interval);
        sleep(interval);
        update = IOPMCopyAssertionActivityAggregate( );

        if (basis && update) {
            delta = IOReportCreateSamplesDelta(basis, update, NULL);
        }
        else if (update) {
            delta = CFDictionaryCreateMutableCopy(NULL, 0, update);
        }
        else if (basis) {
            // Can't have basis without update unless there is some error
            FAIL("Failed to get updated aggregate assertion activity\n");
            goto exit;
        }
        else {
            FAIL("Found no sleep blockers. Expected pid %d\n", pid[i]);
            goto exit;
        }
        if (!delta) {
            FAIL("Failed to get delta\n");
            goto exit;
        }

        cnt = 0;
        __block bool found_pid = false;
        __block bool found_other_pids = false;
        IOReportIterate(delta, ^(IOReportChannelRef ch) {
            int64_t     eff1, eff2, eff3;
            uint64_t chid;

            eff1 = IOReportArrayGetValueAtIndex(ch, 1); // Idle Sleep
            eff2 = IOReportArrayGetValueAtIndex(ch, 2); // Demand Sleep
            eff3 = IOReportArrayGetValueAtIndex(ch, 3); // Display Sleep

            if (eff1 < 0 || eff2 < 0 ||eff3 < 0) {
                // We shouldn't be getting negative values
                return kIOReportIterOk;
            }
#if 0

            if (eff1 || eff2 || eff3) {
                char name[64];

                chid = IOReportChannelGetChannelID(ch);
                name[0] = 0;
                proc_name((pid_t)chid, name, sizeof(name));
                snprintf(name, sizeof(name), "%s(%d)", name, (pid_t)chid);
                LOG("%-25s ", name);
                LOG("%15lld %15lld %15lld\n", eff1, eff2, eff3);
                cnt++;
            }
#endif
            if (eff1) {

                chid = IOReportChannelGetChannelID(ch);
                for (int j = 0; j < kIterations; j++) {
                    if (chid == pidPtr[j]) {
                        if (i == j) {
                            found_pid = true;
                        }
                        else {
                            found_other_pids = true;
                        }
                    }
                }
            }
            return kIOReportIterOk;
        });

        if (!found_pid) {
            FAIL("Expected pid %d not found in delta\n", pid[i]);
        }
        if (found_other_pids) {
            FAIL("Found unexpected pids\n");
        }

        CFRelease(delta);
        if (basis) CFRelease(basis);
        basis = update;
        update = NULL;
    }

    PASS("IOPMSetAssertionActivityAggregate\n");
exit:

    if (basis) CFRelease(basis);
    if (update) CFRelease(update);
    IOPMSetAssertionActivityAggregate(false);
}

int set_exceptionLimits()
{
    IOReturn ret;
    CFMutableDictionaryRef limits = NULL;
    CFMutableDictionaryRef proc1 = NULL, proc2 = NULL;
    CFNumberRef num30 = NULL, num15 = NULL;
    char name[64];
    CFStringRef procName = NULL;
    int rc = -1;

    name[0] = 0;
    proc_name(getpid(), name, sizeof(name));
    procName = CFStringCreateWithCString(0, name, kCFStringEncodingUTF8);
    if (!procName) {
        FAIL("Failed to create CFString for this process name\n");
        goto exit;
    }

    INT_TO_CFNUMBER(num30, 30);
    INT_TO_CFNUMBER(num15, 15);
    if (!num30 || !num15) {
        FAIL("Failed to create CFNumbers\n");
        goto exit;
    }
    limits = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!limits) {
        FAIL("Failed to create CFDictionary\n");
        goto exit;
    }
    proc1 = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!proc1) {
        FAIL("Failed to create CFDictionary\n");
        goto exit;
    }
    CFDictionarySetValue(proc1, kIOPMAssertionDurationLimit, num30);
    CFDictionarySetValue(proc1, kIOPMAggregateAssertionLimit, num15);

    proc2 = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!proc2) {
        FAIL("Failed to create CFDictionary\n");
        goto exit;
    }
    CFDictionarySetValue(proc2, kIOPMAssertionDurationLimit, num15);
    CFDictionarySetValue(proc2, kIOPMAggregateAssertionLimit, num30);

    CFDictionarySetValue(limits, kIOPMDefaultLimtsKey, proc1); // Limits for caffeinate
    CFDictionarySetValue(limits, procName, proc2); // Limits for this process

    ret = IOPMSetAssertionExceptionLimits(limits);
    if (ret != kIOReturnSuccess) {
        FAIL("IOPMSetAssertionExceptionLimits returned 0x%x\n", ret);
        goto exit;
    }

    rc = 0; // return success

exit:
    if (procName) CFRelease(procName);
    if (num30)  CFRelease(num30);
    if (num15)  CFRelease(num15);
    if (proc1)  CFRelease(proc1);
    if (proc2)  CFRelease(proc2);
    if (limits) CFRelease(limits);

    return rc;
}

void toggleExternalPower(bool disable)
{
    int rc = -1;


    if (rc!=0) {
        printf("Failed to turn %s the external power source\n", disable ? "off" : "on");
    }
}

void test_assertionExceptions()
{

    int rc;
    IOReturn ret;
    __block int caffeinate_pid_exc, my_pid_exc;
    IOPMNotificationHandle hdl = 0;
    pid_t caffeinate_pid = 0, my_pid = 0;
    dispatch_queue_t queue;
    IOPMAssertionID id;

    START_TEST_CASE("Test Assertion Exceptions\n");
    caffeinate_pid_exc = 0;
    my_pid_exc = 0;

    // Disable external power source
    toggleExternalPower(true);
    if (set_exceptionLimits() != 0) {
        goto exit;
    }
    queue = dispatch_queue_create("PM test queue", NULL);
    if (!queue) {
        FAIL("Failed to create dispatch queue\n");
        goto exit;
    }

    my_pid = getpid();

    // Fork caffeinate for assertion creation
    rc = posix_spawn(&caffeinate_pid, "/usr/bin/caffeinate", NULL, NULL, (char *[]){"/usr/bin/caffeinate", "-t", "35", NULL}, NULL);
    if (rc != 0) {
        FAIL("posix_spawn failed with error code %d\n", rc);
        goto exit;
    }

    hdl = IOPMScheduleAssertionExceptionNotification(queue,
                                                     ^(IOPMAssertionException exc, pid_t pid){
                                                         if (pid == caffeinate_pid) {
                                                             caffeinate_pid_exc++;
                                                         }
                                                         else if (pid == my_pid) {
                                                             my_pid_exc++;
                                                         }
                                                         
                                                     });


    // Create assertion from this process
    ret = IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleSystemSleep, kIOPMAssertionLevelOn, CFSTR("Test assertion from test_assertionExceptions"), &id);
    if (ret != kIOReturnSuccess) {
        FAIL("Assertion Create failed\n");
        goto exit;
    }
    ret = IOPMAssertionSetTimeout(id, 30);
    if (ret != kIOReturnSuccess) {
        FAIL("Failed to set timeout on assertion. ret=0x%x\n", ret);
        goto exit;
    }

    // Sleep for 40 secs, waiting for assertion exception notifications
    LOG("Sleep for 45secs...\n");
    sleep(45);

    if (caffeinate_pid_exc != 1) {
        FAIL("Unexpected number of exception notification on caffeinate pid(Expected:1, Actual:%d)\n", caffeinate_pid_exc);
        goto exit;
    }
    if (my_pid_exc != 1) {
        FAIL("Unexpected number of exception notification on this test process(Expected:1, Actual:%d)\n", my_pid_exc);
        goto exit;
    }

    PASS("Test Assertion exceptions\n");

exit:
    toggleExternalPower(false);
    if (hdl) {
        IOPMUnregisterExceptionNotification(hdl);
    }

    return;
}


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
        FAIL("IOPMCopyAssertionsStatus returned 0x%x\n", rc);
        return -1;
    }

    CFNumberRef cfVal = CFDictionaryGetValue(dict, kIOPMAssertionTypePreventUserIdleSystemSleep);
    if (cfVal) {
        CFNumberGetValue(cfVal, kCFNumberIntType, &val);
    }
    if (val == 0) {
        FAIL("Assertion Status reports that PreventUserIdle is not enabled\n");
        ret = -1;
        goto exit;
    }

    rc = IOPMCopyAssertionsByType(kIOPMAssertionTypePreventUserIdleSystemSleep, &array);
    if (rc != kIOReturnSuccess) {
        FAIL("IOPMCopyAssertionsByType returned 0x%x\n", rc);
        ret = -1;
        goto exit;
    }
    for (i=0; i < CFArrayGetCount(array); i++) {
        CFDictionaryRef assertion = CFArrayGetValueAtIndex(array, i);
        if (!assertion) {
            ret = -1;
            FAIL("IOPMCopyAssertionsByType returned empty element at index %ld\n", i);
            goto exit;
        }

        CFStringRef name = CFDictionaryGetValue(assertion, kIOPMAssertionNameKey);
        if (!name) {
            ret = -1;
            FAIL("IOPMCopyAssertionsByType returned assertion without name at index %ld\n", i);
            goto exit;
        }
        if (CFStringCompare(name, CFSTR(kAssertName), 0) == kCFCompareEqualTo) {
            break;
        }
    }
    if (i >= CFArrayGetCount(array)) {
        FAIL("Expected block assertion is not found in the output of IOPMCopyAssertionsByType\n");
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

static void test_IOPMPerformBlockWithAssertion()
{

    __block int ret = 0;
    IOReturn rc;

    START_TEST_CASE("Test IOPMPerformBlockWithAssertion\n");
    CFMutableDictionaryRef properties = CFDictionaryCreateMutable(0, 3,
                                                           &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);

    if (properties) {
        CFDictionarySetValue(properties, kIOPMAssertionNameKey, CFSTR(kAssertName));

        CFDictionarySetValue(properties, kIOPMAssertionTypeKey, kIOPMAssertPreventUserIdleSystemSleep);
    }
    else {
        FAIL("CFDictionaryCreateMutable failed\n");
        goto exit;
    }

    rc = IOPMPerformBlockWithAssertion(properties, ^{
                                       ret = checkAssertionState();
                                       });

    if (rc != kIOReturnSuccess) {
        printf("FAIL: IOPMPerformBlockWithAssertion returned 0x%x\n", rc);
    }
    else {
        if (ret == 0) {
            PASS("Block assertion is taken properly\n");
        }
        else {
            FAIL("Couldn't find block assertion\n");
        }
    }

exit:
    if (properties) {
        CFRelease(properties);
    }
    return;

}
static void test_sysQualifiers()
{
    IOPMAssertionID id;
    START_TEST_CASE("Test System Qualifier assertion properties\n");

    CFMutableDictionaryRef props = CFDictionaryCreateMutable(0, 0,
                                                             &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);

    CFMutableArrayRef resources = NULL;

    if (!props) {
        FAIL("Failed to create dictionary for properties\n");
        goto exit;
    }

    CFDictionarySetValue(props, kIOPMAssertionNameKey, CFSTR("Audio-in property test"));
    CFDictionarySetValue(props, kIOPMAssertionTypeKey, kIOPMAssertionTypePreventUserIdleSystemSleep);
    resources = CFArrayCreateMutable(0, 0, 0);

    CFArrayAppendValue(resources, kIOPMAssertionResourceAudioIn);
    CFArrayAppendValue(resources, kIOPMAssertionResourceAudioOut);

    CFDictionarySetValue(props, kIOPMAssertionResourcesUsed, resources);
    double timeout = 30;

    CFNumberRef numRef = CFNumberCreate(0, kCFNumberDoubleType, &timeout);
    if (numRef) {
        CFDictionarySetValue(props, kIOPMAssertionTimeoutKey, numRef);
        CFRelease(numRef);
    }

    IOPMAssertionCreateWithProperties(props, &id);

    // Don't bother releasing
    PASS("Test System Qualifier assertion properties\n");

exit:
    if (resources) {
        CFRelease(resources);
    }
    if (props) {
        CFRelease(props);
    }
    return;
}

static void testUserActivityAssertion()
{
    START_TEST_CASE("Test creating UserActivity assertion\n");
    IOReturn rc;
    IOPMAssertionID id;

    CFMutableDictionaryRef properties = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!properties)
    {
        FAIL("CFDictionaryCreateMutable failed\n");
        goto exit;
    }
    CFNumberRef assertionTimeout;
    INT_TO_CFNUMBER(assertionTimeout, 30);
    if ( assertionTimeout != nil )
    {
        CFDictionarySetValue(properties, kIOPMAssertionTimeoutKey, assertionTimeout);
        CFDictionarySetValue(properties, kIOPMAssertionTimeoutActionKey, kIOPMAssertionTimeoutActionRelease);
        CFRelease(assertionTimeout);
    }
    CFDictionarySetValue(properties, kIOPMAssertionTypeKey, kIOPMAssertionUserIsActive);
    CFDictionarySetValue(properties, kIOPMAssertionNameKey, CFSTR("Testing User activity assertion"));
    CFDictionarySetValue(properties, kIOPMAssertionAppliesOnLidClose, kCFBooleanTrue);

    rc = IOPMAssertionCreateWithProperties(properties, &id);
    if ( rc != kIOReturnSuccess )
    {
        FAIL("IOPMAssertionDeclareUserActivity returned 0x%x\n", rc);
        goto exit;
    }

    sleep(30);
    IOPMAssertionRelease(id);

    PASS("UserActivity assertion\n");
exit:
    if (properties) {
        CFRelease(properties);
    }
    return;
}
