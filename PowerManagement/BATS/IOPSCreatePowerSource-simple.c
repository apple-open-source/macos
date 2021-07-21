//
//  main.c
//  IOPSCreatePowerSource-simple
//
//
//

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/IOCFSerialize.h>
#include <spawn.h>
#include "PMtests.h"

static CFDictionaryRef      copyNextPSDictionary(CFStringRef type);
static CFStringRef          copyNextPSType(void);

static void iterateCreateSetRelease(int iterations);
static bool verifyThatAPublishedPowerSourceIsNamed(CFStringRef checkname, CFStringRef type);
static void createAndCheckForExistence(CFStringRef useName, CFStringRef type);
static void fillAndReleaseAllPowerSourceSlots(int count);

static const int kTryDictionaries = 5;
static const int kMaxPSCount = 7;

int gPassCnt = 0, gFailCnt = 0;

int main(int argc, const char * argv[])
{
    START_TEST("Test Power Source APIs\n");
    for (int i = 0; i< 3; i++)
    {
        iterateCreateSetRelease(kTryDictionaries);
        createAndCheckForExistence(CFSTR("Snaggletooth"), CFSTR(kIOPSUPSType));
        createAndCheckForExistence(CFSTR("Snaggletooth"), CFSTR(kIOPSAccessoryType));
        fillAndReleaseAllPowerSourceSlots(kMaxPSCount * 2);
    }
    SUMMARY("Test Power Source APIs");
}


//******************************************************************************
//******************************************************************************
//******************************************************************************

static void createAndCheckForExistence(CFStringRef useName, CFStringRef type)
{
    START_TEST_CASE("Create a UPS type power source\n");
    CFDictionaryRef         useDictionary = NULL;
    CFMutableDictionaryRef  setDictionary = NULL;
    IOReturn                ret;
    IOPSPowerSourceID         psid = 0;

    char buf[255];
    CFStringGetCString(useName, buf, sizeof(buf), kCFStringEncodingUTF8);


    useDictionary = copyNextPSDictionary(type);
    if (useDictionary) {
        setDictionary = CFDictionaryCreateMutableCopy(0, 0, useDictionary);
        if (setDictionary) {
            CFDictionarySetValue(setDictionary, CFSTR(kIOPSNameKey), useName);
        }
        CFRelease(useDictionary);
        useDictionary = NULL;
    }

    if (!setDictionary) {
        FAIL("createAndCheckForExistence couldn't create PS dictionary\n");
        goto exit;
    }

    ret = IOPSCreatePowerSource(&psid);
    if (kIOReturnSuccess != ret) {
        FAIL("createAndCheckForExistence couldn't create PS power source 0x%08x\n", ret);
        goto exit;
    }

    ret = IOPSSetPowerSourceDetails(psid, setDictionary);
    if (kIOReturnSuccess != ret) {
        FAIL("Failure return 0x%08x from IOPSSetPowerSourceDetails\n", ret);
        goto exit;
    }


    if (verifyThatAPublishedPowerSourceIsNamed(useName, type))
    {
        LOG("Successfully created, then found, a power source named %s\n", buf);
    } else {
        char * argv[] = {"pmset -g ps", NULL};
        posix_spawn(NULL, argv[0], NULL, NULL, argv, NULL);
        FAIL("createAndCheckForExistence couldn't locate a power source named %s\n", buf);
        goto exit;
    }

    IOPSReleasePowerSource(psid);
    psid = 0;

    // We want to wait a second to let the release power source percolate through powerd
    // before we check if the Release worked.
    sleep(1);

    if (!verifyThatAPublishedPowerSourceIsNamed(useName, type))
    {
        LOG("Successfully RELEASED (it's not published any more), a power source named %s\n", buf);
    } else {
        char * argv[] = {"pmset -g ps", NULL};
        posix_spawn(NULL, argv[0], NULL, NULL, argv, NULL);
        FAIL("createAndCheckForExistence just released a power source, but it's still published %s\n", buf);
        goto exit;
    }

    fflush(stdout);
    PASS("Create a UPS type power source\n");
exit:
    if (psid) {
        IOPSReleasePowerSource(psid);
    }
    if (useDictionary) {
        CFRelease(useDictionary);
    }
    if (setDictionary) {
        CFRelease(setDictionary);
    }
    return;
}

static bool verifyThatAPublishedPowerSourceIsNamed(CFStringRef checkname, CFStringRef type)
{
    CFTypeRef blob = NULL;
    CFArrayRef arr = NULL;
    CFDictionaryRef details = NULL;
    bool doMatch = false;

    if (CFStringCompare(type, CFSTR(kIOPSAccessoryType), 0) == kCFCompareEqualTo) {
        blob = IOPSCopyPowerSourcesByType(kIOPSSourceForAccessories);
    }
    else {
        blob = IOPSCopyPowerSourcesInfo();
    }
    if (blob) {
        arr = IOPSCopyPowerSourcesList(blob);
    }

    if (!arr) {
        goto bail;
    }

    for (int i=0; i<CFArrayGetCount(arr); i++) {
        details = CFArrayGetValueAtIndex(arr, i);
        if (!details) {
            continue;
        }

        CFStringRef hasName = NULL;
        hasName = CFDictionaryGetValue(details, CFSTR(kIOPSNameKey));
        if (hasName && CFEqual(checkname, hasName)) {
            doMatch = true;
            break;
        }
    }

bail:
    if (arr) {
        CFRelease(arr);
    }
    if (blob) {
        CFRelease(blob);
    }
    return doMatch;
}


static void fillAndReleaseAllPowerSourceSlots(int count)
{
    IOReturn ret;

    IOPSPowerSourceID *ids = calloc(count, sizeof(IOPSPowerSourceID));

    START_TEST_CASE("Fill and Release Power Source Slots\n");
    for (int i=0; i<count; i++)
    {

        CFStringRef     pstype = copyNextPSType();
        if (!pstype) {
            FAIL("internal error generating testing ps type");
            goto exit;
        }

        /*
         * Create
         */
        ret = IOPSCreatePowerSource(&ids[i]);

        LOG("Creating %d power sources to exceed limits (%d returns 0x%08x)\n", count, i, ret);
        if (ret != kIOReturnSuccess
            && ret != kIOReturnNoSpace)
        {
            FAIL("IOPSCreatePowerSource return value was 0x%08x, should have been Success or NoSpace.\n", ret);
            goto exit;
        }
    }


    for (int i=0; i<count; i++)
    {
        /*
         * Release
         */
        ret = IOPSReleasePowerSource(ids[i]);
        if (kIOReturnSuccess != ret) {
            FAIL("Failure return 0x%08x from IOPSReleasePowerSource\n", ret);
            goto exit;
        }
        LOG("Release %d power sources to exceed limits (%d returns 0x%08x)\n", count, i, ret);
    }
    
    PASS("Fill and Release Power Source Slots\n");
exit:
    if (ids) { free(ids); }
    return;
}

static void iterateCreateSetRelease(int iterations)
{
    IOReturn ret;

    IOPSPowerSourceID psid = NULL;

    START_TEST_CASE("Create, Set and Release Poweer Sources\n");
    for (int i=0; i<iterations; i++)
    {
        LOG("Big iteration %d of %d\n", i, iterations);

        CFStringRef     pstype = copyNextPSType();
        if (!pstype) {
            FAIL("internal error generating testing ps type");
            goto exit;
        }

        /*
         * Create
         */
        ret = IOPSCreatePowerSource(&psid);
        if (kIOReturnSuccess != ret) {
            FAIL("Failure return 0x%08x from IOPSCreatePowerSource\n", ret);
            goto exit;
        }

        /*
         * Set
         */
        CFDictionaryRef psdict = NULL;
        psdict = copyNextPSDictionary(NULL);

        if (!psdict) {
            FAIL("internal error generating testing dictionary");
            exit(1);
        }
        ret = IOPSSetPowerSourceDetails(psid, psdict);
        if (kIOReturnSuccess != ret) {
            FAIL("Failure return 0x%08x from IOPSSetPowerSourceDetails\n", ret);
            CFRelease(psdict);
            goto exit;
        }
        CFRelease(psdict);

        /*
         * Release
         */
        ret = IOPSReleasePowerSource(psid);
        if (kIOReturnSuccess != ret) {
            FAIL("Failure return 0x%08x from IOPSReleasePowerSource\n", ret);
            goto exit;
        }

        fflush(stdout);

    }
    
    PASS("Create, Set and Release Poweer Sources\n");
exit:
    return;
}


//******************************************************************************
//******************************************************************************
//******************************************************************************

static CFStringRef  copyNextPSType(void)
{
    static int t = 0;
    CFStringRef _types[] = {
                            CFSTR(kIOPSInternalBatteryType),
                            CFSTR(kIOPSUPSType)
//                            CFSTR("HenryThe8thBogus")
                            };

    return _types[t++ % (sizeof(_types)/sizeof(_types[0]))];
}

//******************************************************************************
enum {
    kCurrentCapacity = 0,
    kMaxCapacity,
    kName,
    kTimeToEmpty,
    kTimeToFull,
    kTimeRemaining,
    kIsCharged,
    kIsCharging,
    kIsPresent,
    kDesignCap,
    kVoltage,
    kIsFinishingCharge,
    kTransportType,
    kPSType,
    kPSState
};

//******************************************************************************
static CFDictionaryRef copyNextPSDictionary(CFStringRef type)
{
    const CFStringRef   keys[] = {
        CFSTR(kIOPSCurrentCapacityKey),
        CFSTR(kIOPSMaxCapacityKey),
        CFSTR(kIOPSNameKey),
        CFSTR(kIOPSTimeToEmptyKey),
        CFSTR(kIOPSTimeToFullChargeKey),
        CFSTR(kIOPSTimeRemainingNotificationKey),
        CFSTR(kIOPSIsChargedKey),
        CFSTR(kIOPSIsChargingKey),
        CFSTR(kIOPSIsPresentKey),
        CFSTR(kIOPSDesignCapacityKey),
        CFSTR(kIOPSVoltageKey),
        CFSTR(kIOPSIsFinishingChargeKey),
        CFSTR(kIOPSTransportTypeKey),
        CFSTR(kIOPSTypeKey),
        CFSTR(kIOPSPowerSourceStateKey)
    };

    CFTypeRef *values = NULL;
    int tmpInt = 0;

    int count = sizeof(keys)/sizeof(CFTypeRef);
    values = (CFTypeRef *)calloc(1, count * sizeof(CFTypeRef));

    tmpInt = 6000;
    values[kCurrentCapacity] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kMaxCapacity] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kName] = CFSTR("com.iokit.IOPSCreatePowerSource");
    tmpInt = 25;
    values[kTimeToEmpty] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    tmpInt = 0;
    values[kTimeToFull] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kTimeRemaining] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kIsCharged] = CFRetain(kCFBooleanTrue);
    values[kIsCharging] = CFRetain(kCFBooleanTrue);
    values[kIsPresent] = CFRetain(kCFBooleanTrue);
    tmpInt = 7000;
    values[kDesignCap] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    tmpInt = 1100;
    values[kVoltage] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kIsFinishingCharge] = CFRetain(kCFBooleanTrue);
    values[kTransportType] = CFSTR(kIOPSUSBTransportType);
    if (type != NULL)
        values[kPSType] = type;
    else
        values[kPSType] = CFSTR(kIOPSUPSType);

    values[kPSState] = CFSTR(kIOPSACPowerValue);

    return CFDictionaryCreate(0, (const void **)keys, (const void **)values, count,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);

}
