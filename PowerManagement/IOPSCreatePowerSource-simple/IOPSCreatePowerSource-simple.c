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

static CFDictionaryRef      copyNextPSDictionary(void);
static CFStringRef          copyNextPSType(void);

static void iterateCreateSetRelease(int iterations);
static bool verifyThatAPublishedPowerSourceIsNamed(CFStringRef checkname);
static void createAndCheckForExistence(CFStringRef useName);
static void fillAndReleaseAllPowerSourceSlots(int count);

static const int kTryDictionaries = 5;
static const int kMaxPSCount = 7;

int main(int argc, const char * argv[])
{
    for (int i = 0; i< 3; i++)
    {
        iterateCreateSetRelease(kTryDictionaries);
        createAndCheckForExistence(CFSTR("Snaggletooth"));
        fillAndReleaseAllPowerSourceSlots(kMaxPSCount * 2);
    }
}


//******************************************************************************
//******************************************************************************
//******************************************************************************

static void createAndCheckForExistence(CFStringRef useName)
{
    CFDictionaryRef         useDictionary = NULL;
    CFMutableDictionaryRef  setDictionary = NULL;
    IOReturn                ret;
    IOPSPowerSourceID         psid = 0;

    char buf[255];
    CFStringGetCString(useName, buf, sizeof(buf), kCFStringEncodingUTF8);


    useDictionary = copyNextPSDictionary();
    if (useDictionary) {
        setDictionary = CFDictionaryCreateMutableCopy(0, 0, useDictionary);
        if (setDictionary) {
            CFDictionarySetValue(setDictionary, CFSTR(kIOPSNameKey), useName);
        }
        CFRelease(useDictionary);
    }

    if (!setDictionary) {
        printf("FAIL: createAndCheckForExistence couldn't create PS dictionary\n");
        return;
    }

    ret = IOPSCreatePowerSource(&psid);
    if (kIOReturnSuccess != ret) {
        printf("FAIL: createAndCheckForExistence couldn't create PS power source 0x%08x\n", ret);
        return;
    }

    ret = IOPSSetPowerSourceDetails(psid, setDictionary);
    if (kIOReturnSuccess != ret) {
        printf("[FAIL] Failure return 0x%08x from IOPSSetPowerSourceDetails\n", ret);
        exit(1);
    }
    CFRelease(setDictionary);

    if (verifyThatAPublishedPowerSourceIsNamed(useName))
    {
        printf("[PASS] Successfully created, then found, a power source named %s\n", buf);
    } else {
        printf("[FAIL] createAndCheckForExistence couldn't locate a power source named %s\n", buf);
        system("pmset -g ps");
    }

    IOPSReleasePowerSource(psid);

    // We want to wait a second to let the release power source percolate through powerd
    // before we check if the Release worked.
    sleep(1);

    if (!verifyThatAPublishedPowerSourceIsNamed(useName))
    {
        printf("[PASS] Successfully RELEASED (it's not published any more), a power source named %s\n", buf);
    } else {
        printf("[FAIL] createAndCheckForExistence just released a power source, but it's still published %s\n", buf);
        system("pmset -g ps");
    }

    fflush(stdout);
    return;
}

static bool verifyThatAPublishedPowerSourceIsNamed(CFStringRef checkname)
{
    CFTypeRef blob = NULL;
    CFArrayRef arr = NULL;
    CFDictionaryRef details = NULL;
    bool doMatch = false;

    blob = IOPSCopyPowerSourcesInfo();
    if (blob) {
        arr = IOPSCopyPowerSourcesList(blob);
    }

    if (!arr) {
        return false;
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

    for (int i=0; i<count; i++)
    {

        CFStringRef     pstype = copyNextPSType();
        if (!pstype) {
            printf("[FAIL] internal error generating testing ps type");
            exit(1);
        }

        /*
         * Create
         */
        ret = IOPSCreatePowerSource(&ids[i]);

        printf("Creating %d power sources to exceed limits (%d returns 0x%08x)\n", count, i, ret);
        if (ret != kIOReturnSuccess
            && ret != kIOReturnNoSpace)
        {
            printf("[FAIL] IOPSCreatePowerSource return value was 0x%08x, should have been Success or NoSpace.\n", ret);
        }
        fflush(stdout);
    }


    for (int i=0; i<count; i++)
    {
        /*
         * Release
         */
        ret = IOPSReleasePowerSource(ids[i]);
        if (kIOReturnSuccess != ret) {
            printf("[FAIL] Failure return 0x%08x from IOPSReleasePowerSource\n", ret);
        }
        printf("Release %d power sources to exceed limits (%d returns 0x%08x)\n", count, i, ret);
    fflush(stdout);
    }
    
}

static void iterateCreateSetRelease(int iterations)
{
    IOReturn ret;

    IOPSPowerSourceID psid = NULL;

    for (int i=0; i<iterations; i++)
    {
        printf("Big iteration %d of %d\n", i, iterations);

        CFStringRef     pstype = copyNextPSType();
        if (!pstype) {
            printf("[FAIL] internal error generating testing ps type");
            exit(1);
        }

        /*
         * Create
         */
        ret = IOPSCreatePowerSource(&psid);
        if (kIOReturnSuccess != ret) {
            printf("[FAIL] Failure return 0x%08x from IOPSCreatePowerSource\n", ret);
            exit(1);
        }

        /*
         * Set
         */
        CFDictionaryRef psdict = NULL;
        psdict = copyNextPSDictionary();

        if (!psdict) {
            printf("[FAIL] internal error generating testing dictionary");
            exit(1);
        }
        ret = IOPSSetPowerSourceDetails(psid, psdict);
        if (kIOReturnSuccess != ret) {
            printf("[FAIL] Failure return 0x%08x from IOPSSetPowerSourceDetails\n", ret);
            exit(1);
        }
        CFRelease(psdict);

        /*
         * Release
         */
        ret = IOPSReleasePowerSource(psid);
        if (kIOReturnSuccess != ret) {
            printf("[FAIL] Failure return 0x%08x from IOPSReleasePowerSource\n", ret);
            exit(1);
        }

        fflush(stdout);

    }
    
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
    kTransportType
};

//******************************************************************************
static CFDictionaryRef copyNextPSDictionary(void)
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
        CFSTR(kIOPSTransportTypeKey)
    };

    CFTypeRef *values = NULL;
    int tmpInt = 0;

    int count = sizeof(keys)/sizeof(CFTypeRef);
    values = calloc(1, sizeof(keys));

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

    return CFDictionaryCreate(0, (const void **)keys, (const void **)values, count,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);

}
