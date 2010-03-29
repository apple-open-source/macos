/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
#include <TargetConditionals.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/vm_map.h>
#include <servers/bootstrap.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include "IOSystemConfiguration.h"
#include "IOPMLibPrivate.h"
#include "powermanagement.h"

#include <unistd.h>
#include <stdlib.h>

#define kAssertionsArraySize        5

static const int kMaxNameLength = 128;


static IOReturn _pm_connect(mach_port_t *newConnection)
{
    kern_return_t       kern_result = KERN_SUCCESS;
    
    if(!newConnection) return kIOReturnBadArgument;

    // open reference to PM configd
    kern_result = bootstrap_look_up(bootstrap_port, 
            kIOPMServerBootstrapName, newConnection);
    if(KERN_SUCCESS != kern_result) {
	*newConnection = MACH_PORT_NULL;
        return kIOReturnError;
    }
    return kIOReturnSuccess;
}

static IOReturn _pm_disconnect(mach_port_t connection)
{
    if(!connection) return kIOReturnBadArgument;
    mach_port_deallocate(mach_task_self(), connection);
    return kIOReturnSuccess;
}

static bool _supportedAssertion(CFStringRef assertion)
{
    return (CFEqual(assertion, kIOPMAssertionTypeNoIdleSleep)
        || CFEqual(assertion, kIOPMAssertionTypeNoDisplaySleep)
        || CFEqual(assertion, kIOPMAssertionTypeDisableInflow)
        || CFEqual(assertion, kIOPMAssertionTypeInhibitCharging)

#if TARGET_EMBEDDED_OS
        // kIOPMAssertionTypeEnableIdleSleep is only supported on
        // embedded platforms. IOPMAssertionCreate returns an error
        // when a caller tries to assert it on user OS X.
        || CFEqual(assertion, kIOPMAssertionTypeEnableIdleSleep)
#endif

        || CFEqual(assertion, kIOPMAssertionTypeDisableLowBatteryWarnings) 
        || CFEqual(assertion, kIOPMAssertionTypeNeedsCPU) );
}


/******************************************************************************
 * IOPMAssertionCreate
 *
 ******************************************************************************/
IOReturn IOPMAssertionCreate(
    CFStringRef             AssertionType,
    IOPMAssertionLevel      AssertionLevel,
    IOPMAssertionID         *AssertionID)
{
    return IOPMAssertionCreateWithName(AssertionType, AssertionLevel,
                                    CFSTR("Nameless (via IOPMAssertionCreate)"), AssertionID);
}

/******************************************************************************
 * IOPMAssertionCreateWithName
 *
 ******************************************************************************/
IOReturn IOPMAssertionCreateWithName(
    CFStringRef          AssertionType, 
    IOPMAssertionLevel   AssertionLevel,
    CFStringRef          AssertionName,
    IOPMAssertionID      *AssertionID)
{
    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    char                    assertion_str[kMaxNameLength];
    char                    name_str[kMaxNameLength];
    mach_port_t             pm_server = MACH_PORT_NULL;
    mach_port_t             task_self = mach_task_self();
    IOReturn                err;

    if (!AssertionID)
        return kIOReturnBadArgument;

    // Set assertion_id to a known invalid setting. If successful, it will
    // get a valid value later on.
    *AssertionID = kIOPMNullAssertionID;

    if(!_supportedAssertion(AssertionType))
    {
        return kIOReturnUnsupported;
    }

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    CFStringGetCString( AssertionType, assertion_str, 
                        sizeof(assertion_str), kCFStringEncodingMacRoman);
    
    // Check validity of input name string
    if (!AssertionName || (kMaxNameLength < CFStringGetLength(AssertionName))) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    CFStringGetCString( AssertionName, name_str,
                        sizeof(name_str), kCFStringEncodingMacRoman);

    // io_pm_assertion_create mig's over to configd, and it's configd 
    // that actively tracks and manages the list of active power assertions.
    kern_result = io_pm_assertion_create(
            pm_server, 
            task_self,
            name_str,
            assertion_str,
            AssertionLevel, 
            (int *)AssertionID,
            &return_code);
            
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }

exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }
    return return_code;
}

/******************************************************************************
 * IOPMAssertionsRelease
 *
 ******************************************************************************/
IOReturn IOPMAssertionRelease(
    IOPMAssertionID         AssertionID)
{
    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    mach_port_t             pm_server = MACH_PORT_NULL;
    mach_port_t             task_self = mach_task_self();
    IOReturn                err;

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    kern_result = io_pm_assertion_release(
            pm_server, 
            task_self,
            AssertionID,
            &return_code);
            
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }

    _pm_disconnect(pm_server);
exit:
    return return_code;
}

/******************************************************************************
 * IOPMCopyAssertionsByProcess
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionsByProcess(
    CFDictionaryRef         *AssertionsByPid)
{
    SCDynamicStoreRef   dynamicStore = NULL;
    CFStringRef         dataKey = NULL;
    IOReturn            returnCode = kIOReturnError;
    int                 flattenedArrayCount = 0;
    CFArrayRef          flattenedDictionary = NULL;    
    CFNumberRef         *newDictKeys = NULL;
    CFArrayRef          *newDictValues = NULL;
    
    if (!AssertionsByPid)
        return kIOReturnBadArgument;
        
    *AssertionsByPid = NULL;
    
    dynamicStore = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("PM IOKit User Library"),
                                         NULL, NULL);

    if (NULL == dynamicStore) {
        goto exit;
    }
    
    dataKey = IOPMAssertionCreatePIDMappingKey();

    flattenedDictionary = SCDynamicStoreCopyValue(dynamicStore, dataKey);

    if (!flattenedDictionary) {
        returnCode = kIOReturnSuccess;
        goto exit;
    }

    /*
     * This API returns a dictionary whose keys are process ID's.
     * This is perfectly acceptable in CoreFoundation, EXCEPT that you cannot
     * serialize a dictionary with CFNumbers for keys using CF or IOKit
     * serialization.
     *
     * To serialize this dictionary and pass it from configd to the caller's process,
     * we re-formatted it as a "flattened" array of dictionaries in configd, 
     * and we will re-constitute with pid's for keys here.
     *
     * Next time around, I will simply not use CFNumberRefs for keys in API.
     */

    flattenedArrayCount = CFArrayGetCount(flattenedDictionary);
    
    newDictKeys = (CFNumberRef *)malloc(sizeof(CFTypeRef) * flattenedArrayCount);
    newDictValues = (CFArrayRef *)malloc(sizeof(CFTypeRef) * flattenedArrayCount);
    
    if (!newDictKeys || !newDictValues)
        goto exit;
    
    for (int i=0; i < flattenedArrayCount; i++)
    {
        CFDictionaryRef         dictionaryAtIndex = NULL;

        dictionaryAtIndex = CFArrayGetValueAtIndex(flattenedDictionary, i);

        if (!dictionaryAtIndex)
            continue;

        newDictKeys[i] = CFDictionaryGetValue(
                                dictionaryAtIndex,
                                kIOPMAssertionPIDKey);
        newDictValues[i] = CFDictionaryGetValue(
                                dictionaryAtIndex,
                                CFSTR("PerTaskAssertions"));    
    }
    

    *AssertionsByPid = CFDictionaryCreate(
                                kCFAllocatorDefault,
                                (const void **)newDictKeys,
                                (const void **)newDictValues,
                                flattenedArrayCount,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);

    returnCode = kIOReturnSuccess;
exit:
    if (newDictKeys)
        free(newDictKeys);
    if (newDictValues)
        free(newDictValues);
    if (dynamicStore)
        CFRelease(dynamicStore);
    if (flattenedDictionary)
        CFRelease(flattenedDictionary);
    if (dataKey)
        CFRelease(dataKey);
    return returnCode;
}


/******************************************************************************
 * IOPMCopyAssertionsStatus
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionsStatus(
    CFDictionaryRef         *AssertionsStatus)
{
    SCDynamicStoreRef   dynamicStore = NULL;
    CFStringRef         dataKey = NULL;
    IOReturn            returnCode = kIOReturnError;
    CFDictionaryRef     returnDictionary = NULL;
    
    if (!AssertionsStatus)
        return kIOReturnBadArgument;

    *AssertionsStatus = NULL;
    
    dynamicStore = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("PM IOKit User Library"),
                                         NULL, NULL);

    if (NULL == dynamicStore) {
        goto exit;
    }
    
    dataKey = IOPMAssertionCreateAggregateAssertionKey();

    returnDictionary = SCDynamicStoreCopyValue(dynamicStore, dataKey);
    
    // TODO: check return of SCDynamicStoreCopyVale

    *AssertionsStatus = returnDictionary;

    returnCode = kIOReturnSuccess;
exit:
    if (dynamicStore)
        CFRelease(dynamicStore);
    if (dataKey)
        CFRelease(dataKey);
    return returnCode;
}

/******************************************************************************
 * IOPMAssertionSetTimeout
 *
 ******************************************************************************/
IOReturn IOPMAssertionSetTimeout(
    IOPMAssertionID whichAssertion, 
    CFTimeInterval timeoutInterval)
{
    IOReturn            return_code = kIOReturnError;
    mach_port_t         pm_server = MACH_PORT_NULL;
    int                 seconds = lrint(timeoutInterval);
    kern_return_t       kern_result;
    IOReturn            err;
    
    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    kern_result = io_pm_assertion_settimeout(
                                    pm_server,
                                    mach_task_self(),
                                    whichAssertion, /* id */
                                    seconds,        /* interval */
                                    &return_code);

    if (KERN_SUCCESS != kern_result) {
        return_code = kern_result;
    }

exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }

    return return_code;
}

/******************************************************************************
 * IOPMCopyTimedOutAssertions
 *
 ******************************************************************************/
IOReturn IOPMCopyTimedOutAssertions(
    CFArrayRef *timedOutAssertions)
{
    SCDynamicStoreRef   dynamicStore = NULL;
    CFStringRef         dataKey = NULL;
    IOReturn            returnCode = kIOReturnError;
    CFArrayRef          returnArray = NULL;
    
    if (!timedOutAssertions)
        return kIOReturnBadArgument;

    *timedOutAssertions = NULL;
    
    dynamicStore = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("PM IOKit User Library"),
                                         NULL, NULL);

    if (NULL == dynamicStore) {
        goto exit;
    }
    
    dataKey = IOPMAssertionCreateTimeOutKey();

    returnArray = SCDynamicStoreCopyValue(dynamicStore, dataKey);
    
    // TODO: check return of SCDynamicStoreCopyVale

    *timedOutAssertions = returnArray;

    returnCode = kIOReturnSuccess;
exit:
    if (dynamicStore)
        CFRelease(dynamicStore);
    if (dataKey)
        CFRelease(dataKey);
    return returnCode;
}

/*
 * State:/IOKit/PowerManagement/Assertions/TimedOut
 */

CFStringRef IOPMAssertionCreateTimeOutKey(void)
{
    return SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@/%@"),
                            kSCDynamicStoreDomainState, 
                            CFSTR("/IOKit/PowerManagement/Assertions"), 
                            CFSTR("TimedOut"));
}

/*
 * State:/IOKit/PowerManagement/Assertions/ByProcess
 */

CFStringRef IOPMAssertionCreatePIDMappingKey(void)
{
    return SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@/%@"),
                            kSCDynamicStoreDomainState, 
                            CFSTR("/IOKit/PowerManagement/Assertions"), 
                            CFSTR("ByProcess"));
}


/*
 * State:/IOKit/PowerManagement/Assertions
 */

CFStringRef IOPMAssertionCreateAggregateAssertionKey(void)
{
    return SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@"),
                            kSCDynamicStoreDomainState, 
                            CFSTR("/IOKit/PowerManagement/Assertions"));
}


/******************************************************************************
 * IOPMGetLastWakeTime
 *
 ******************************************************************************/
static IOReturn _smcWakeTimerGetResults(uint16_t *mSec);
static IOReturn _smcReadKey(
    uint32_t key,
    uint8_t *outBuf,
    uint8_t *outBufMax);

// Todo: verify kSMCKeyNotFound
enum {
    kSMCKeyNotFound = 0x84
};

/* Do not modify - defined by AppleSMC.kext */
enum {
	kSMCSuccess	= 0,
	kSMCError	= 1
};
enum {
	kSMCUserClientOpen  = 0,
	kSMCUserClientClose = 1,
	kSMCHandleYPCEvent  = 2,	
    kSMCReadKey         = 5,
	kSMCWriteKey        = 6,
	kSMCGetKeyCount     = 7,
	kSMCGetKeyFromIndex = 8,
	kSMCGetKeyInfo      = 9
};
/* Do not modify - defined by AppleSMC.kext */
typedef struct SMCVersion 
{
    unsigned char    major;
    unsigned char    minor;
    unsigned char    build;
    unsigned char    reserved;
    unsigned short   release;
    
} SMCVersion;
/* Do not modify - defined by AppleSMC.kext */
typedef struct SMCPLimitData 
{
    uint16_t    version;
    uint16_t    length;
    uint32_t    cpuPLimit;
    uint32_t    gpuPLimit;
    uint32_t    memPLimit;

} SMCPLimitData;
/* Do not modify - defined by AppleSMC.kext */
typedef struct SMCKeyInfoData 
{
    IOByteCount         dataSize;
    uint32_t            dataType;
    uint8_t             dataAttributes;

} SMCKeyInfoData;
/* Do not modify - defined by AppleSMC.kext */
typedef struct {
    uint32_t            key;
    SMCVersion          vers;
    SMCPLimitData       pLimitData;
    SMCKeyInfoData      keyInfo;
    uint8_t             result;
    uint8_t             status;
    uint8_t             data8;
    uint32_t            data32;    
    uint8_t             bytes[32];
}  SMCParamStruct;

static IOReturn callSMCFunction(
    int which, 
    SMCParamStruct *inputValues, 
    SMCParamStruct *outputValues);

IOReturn IOPMGetLastWakeTime(
    CFAbsoluteTime      *lastWakeTimeOut,
    CFTimeInterval      *adjustedForPhysicalWakeOut)
{
    IOReturn            ret;
    CFTimeInterval      lastSMCS3S0WakeInterval;
    CFAbsoluteTime      lastWakeTime;
    struct timeval      rawLastWakeTime;
    size_t              rawLastWakeTimeSize = sizeof(rawLastWakeTime);
    uint16_t            wakeup_smc_result = 0;
    static bool         sSMCSupportsWakeupTimer = true;

    if (!lastWakeTimeOut || !adjustedForPhysicalWakeOut) {
        return kIOReturnBadArgument;
    }
    
    *lastWakeTimeOut = 0.0;
    *adjustedForPhysicalWakeOut = 0.0;

    ret = sysctlbyname("kern.waketime", &rawLastWakeTime, &rawLastWakeTimeSize, NULL, 0);
    if (ret || !rawLastWakeTime.tv_sec) {
        return kIOReturnNotReady;
    }

    // Convert the timeval, which is in UNIX time, to a CFAbsoluteTime
    lastWakeTime = rawLastWakeTime.tv_sec + (rawLastWakeTime.tv_usec / 1000000.0);
    lastWakeTime -= kCFAbsoluteTimeIntervalSince1970;

    // Read SMC key for precise timing between when the wake event physically occurred
    // and now (i.e. the moment we read the key).
    if (sSMCSupportsWakeupTimer) {
        ret = _smcWakeTimerGetResults(&wakeup_smc_result);
        if (kIOReturnSuccess == ret && 0 != wakeup_smc_result) {
            // - SMC key returns the delta in tens of milliseconds
            // convert 10x msecs to (double)seconds
            lastSMCS3S0WakeInterval = ((double)wakeup_smc_result / 100.0);  

            // And we adjust backwards to determine the real time of physical wake.
            lastWakeTime -= lastSMCS3S0WakeInterval;
        } else {
            if (kIOReturnNotFound == ret) {
                sSMCSupportsWakeupTimer = false;
            }
        }
    }

    *lastWakeTimeOut = lastWakeTime;
    *adjustedForPhysicalWakeOut = lastSMCS3S0WakeInterval;

    return kIOReturnSuccess;
}

static IOReturn _smcWakeTimerGetResults(uint16_t *mSec)
{
#if !TARGET_OS_EMBEDDED    
    uint8_t     size = 2;
    uint8_t     buf[2];
    IOReturn    ret;
    ret = _smcReadKey('CLWK', buf, &size);

    if (kIOReturnSuccess == ret) {
        *mSec = buf[0] | (buf[1] << 8);
    }

    return ret;
#else
    return kIOReturnNotReadable;
#endif
}

static IOReturn _smcReadKey(
    uint32_t key,
    uint8_t *outBuf,
    uint8_t *outBufMax)
{
    SMCParamStruct  stuffMeIn;
    SMCParamStruct  stuffMeOut;
    IOReturn        ret;
    int             i;

    if (key == 0 || outBuf == NULL) 
        return kIOReturnCannotWire;

    // Determine key's data size
    bzero(outBuf, *outBufMax);
    bzero(&stuffMeIn, sizeof(SMCParamStruct));
    bzero(&stuffMeOut, sizeof(SMCParamStruct));
    stuffMeIn.data8 = kSMCGetKeyInfo;
    stuffMeIn.key = key;

    ret = callSMCFunction(kSMCHandleYPCEvent, &stuffMeIn, &stuffMeOut);

    if (stuffMeOut.result == kSMCKeyNotFound) {
        ret = kIOReturnNotFound;
        goto exit;
    } else if (stuffMeOut.result != kSMCSuccess) {
        ret = kIOReturnInternalError;
        goto exit;
    }

    // Get Key Value
    stuffMeIn.data8 = kSMCReadKey;
    stuffMeIn.key = key;
    stuffMeIn.keyInfo.dataSize = stuffMeOut.keyInfo.dataSize;
    bzero(&stuffMeOut, sizeof(SMCParamStruct));
    ret = callSMCFunction(kSMCHandleYPCEvent, &stuffMeIn, &stuffMeOut);
    if (stuffMeOut.result == kSMCKeyNotFound) {
        ret = kIOReturnNotFound;
        goto exit;
    } else if (stuffMeOut.result != kSMCSuccess) {
        ret = kIOReturnInternalError;
        goto exit;
    }

    if (*outBufMax > stuffMeIn.keyInfo.dataSize)
        *outBufMax = stuffMeIn.keyInfo.dataSize;

    // Byte-swap data returning from the SMC.
    // The data at key 'ACID' are not provided by the SMC and do 
    // NOT need to be byte-swapped.
    for (i=0; i<*outBufMax; i++) 
    {
        if ('ACID' == key)
        {
            // Do not byte swap
            outBuf[i] = stuffMeOut.bytes[i];
        } else {
            // Byte swap
            outBuf[i] = stuffMeOut.bytes[*outBufMax - (i + 1)];
        }
    }
exit:
    return ret;
}

static IOReturn callSMCFunction(
    int which, 
    SMCParamStruct *inputValues, 
    SMCParamStruct *outputValues) 
{
    IOReturn result = kIOReturnError;

    size_t         inStructSize = sizeof(SMCParamStruct);
    size_t         outStructSize = sizeof(SMCParamStruct);
    
    io_connect_t    _SMCConnect = IO_OBJECT_NULL;
    io_service_t    smc = IO_OBJECT_NULL;

    smc = IOServiceGetMatchingService(
        kIOMasterPortDefault, 
        IOServiceMatching("AppleSMC"));
    if (IO_OBJECT_NULL == smc) {
        return kIOReturnNotFound;
    }
    
    result = IOServiceOpen(smc, mach_task_self(), 1, &_SMCConnect);        
    if (result != kIOReturnSuccess || 
        IO_OBJECT_NULL == _SMCConnect) {
        _SMCConnect = IO_OBJECT_NULL;
        goto exit;
    }
    
    result = IOConnectCallMethod(_SMCConnect, kSMCUserClientOpen, 
                    NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
    if (result != kIOReturnSuccess) {
        goto exit;
    }
    
    result = IOConnectCallStructMethod(_SMCConnect, which, 
                        inputValues, inStructSize,
                        outputValues, &outStructSize);

exit:    
    if (IO_OBJECT_NULL != _SMCConnect) {
        IOConnectCallMethod(_SMCConnect, kSMCUserClientClose, 
                    NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
        IOServiceClose(_SMCConnect);    
    }

    return result;
}


/******************************************************************************
 * IOPMSleepWakeSetUUID
 *
 ******************************************************************************/
IOReturn IOPMSleepWakeSetUUID(CFStringRef newUUID)
{
    IOReturn        ret = kIOReturnSuccess;
    io_service_t    service = IO_OBJECT_NULL;
    CFTypeRef       setObject = NULL;
    
    if (!newUUID) {
        // Clear active UUID
        setObject = kCFBooleanFalse;
    } else {
        // cache the upcoming UUID
        setObject = newUUID;
    }
    
    service = IORegistryEntryFromPath(kIOMasterPortDefault, 
            kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    if (IO_OBJECT_NULL != service) {
        ret = IORegistryEntrySetCFProperty(
                    service, CFSTR(kIOPMSleepWakeUUIDKey),
                    setObject);
        IOObjectRelease(service);
    }
    return ret;
}
 
/******************************************************************************
 * IOPMSleepWakeCopyUUID
 *
 ******************************************************************************/
CFStringRef IOPMSleepWakeCopyUUID(void)
{
    CFStringRef     uuidString = NULL;
    io_service_t    service = IO_OBJECT_NULL;
    
    service = IORegistryEntryFromPath(kIOMasterPortDefault, 
            kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    if (IO_OBJECT_NULL != service) {
        uuidString = IORegistryEntryCreateCFProperty(
                       service,
                       CFSTR(kIOPMSleepWakeUUIDKey),
                       kCFAllocatorDefault, 0);
        IOObjectRelease(service);
    }
    
    // Caller must release uuidString, if non-NULL
    return uuidString;
}

/******************************************************************************
 * IOPMDebugTracePoint
 *
 ******************************************************************************/
IOReturn IOPMDebugTracePoint(CFStringRef facility, uint8_t *data, int dataCount)
{
    io_registry_entry_t gRoot = IO_OBJECT_NULL;
    CFNumberRef         setNum = NULL;
    IOReturn            ret = kIOReturnError;
    if (data == NULL || facility == NULL || 1 != dataCount)
        return kIOReturnBadArgument;

    gRoot = IORegistryEntryFromPath( kIOMasterPortDefault, 
        kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    if (IO_OBJECT_NULL == gRoot)
        return kIOReturnError;
        
    // We allow CF to treat this number as a signed integer. We're using it as a bitfield;
    // we shouldn't have to worry about sign extension when it gets unpacked.
    setNum = CFNumberCreate(0, kCFNumberSInt8Type, data);
    if (!setNum)
        goto exit;

    ret = IORegistryEntrySetCFProperty( gRoot, 
                        CFSTR(kIOPMLoginWindowSecurityDebugKey), 
                        setNum);
exit:
    if (setNum)
        CFRelease(setNum);
    if (gRoot)
        IOObjectRelease(gRoot);
    return ret;
}

/******************************************************************************
 * IOPMCopySleepWakeFailure
 *
 ******************************************************************************/
CFDictionaryRef IOPMCopySleepWakeFailure(void)
{
    CFStringRef         scFailureKey = NULL;
    CFDictionaryRef     scFailureDictionary = NULL;
    SCDynamicStoreRef   scDynStore = NULL;

    scDynStore = SCDynamicStoreCreate(0, CFSTR("IOPMSleepFailure"), NULL, NULL);
    if (!scDynStore)
        goto exit;

    scFailureKey = SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@/%@"),
                            kSCDynamicStoreDomainState, 
                            CFSTR("PowerManagement"), 
                            CFSTR(kIOPMDynamicStoreSleepFailureKey));
    if (!scFailureKey)
        goto exit;

    scFailureDictionary = isA_CFDictionary(SCDynamicStoreCopyValue(scDynStore, scFailureKey));

exit:
    if (scDynStore)
        CFRelease(scDynStore);    
    if (scFailureKey)
        CFRelease(scFailureKey);
    return scFailureDictionary;
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/* __IOPMConnection is the IOKit-tracking struct to keep track of
 * open connections.
 */
typedef struct {
    uint32_t                id;
    int                     pid;
    CFStringRef             connectionName;
    // Track the notifications
    CFMachPortRef           localNotifyPort;
    CFRunLoopSourceRef      localNotifyPortRLS;
    IOPMEventHandlerType    userCallout;
    void                    *userParam;
    int                     runLoopCount;
} __IOPMConnection;


/* iopm_mach_port_callback
 * The caller installed this callback on a runloop of their choice.
 * Note that this code is running in the caller's runloop context; we don't have
 * it serialized.
 */
#define kMsgPayloadCount    2

typedef struct {
    mach_msg_header_t   header;
    mach_msg_body_t     body;
    uint32_t            payload[kMsgPayloadCount];
} IOPMMessageStructure;

static void iopm_mach_port_callback(
    CFMachPortRef port __unused, 
    void *msg, 
    CFIndex size __unused, 
    void *info)
{
    IOPMMessageStructure *m  = (IOPMMessageStructure *)msg;
    
    __IOPMConnection        *connection = (__IOPMConnection *)info;

    if (!connection || !connection->userCallout) {
        return;
    }
/*
    CFAbsoluteTime          userCalloutInvoked = 0;
    userCalloutInvoked = CFAbsoluteTimeGetCurrent();
*/
    (*(connection->userCallout))(
                    connection->userParam,
                    (IOPMConnection)connection,
                    m->payload[1],      // messageToken argument
                    m->payload[0]);     // event DATA
    
 /*
    CFAbsoluteTime          userCalloutFinished = 0;
    CFTimeInterval          deltaSecs = 0;
    userCalloutFinished = CFAbsoluteTimeGetCurrent();
 
    deltaSecs = userCalloutFinished - userCalloutInvoked; 
    int secs = (int)deltaSecs;
    int hundredths = ((int)(deltaSecs * 100.0)) % 100;
 */
 
    return;
}

/*****************************************************************************/
/*****************************************************************************/

IOReturn IOPMConnectionSetNotification(
    IOPMConnection myConnection, 
    void *param, 
    IOPMEventHandlerType handler)
{
    __IOPMConnection *connection = (__IOPMConnection *)myConnection;

    if (!connection || !handler)
        return kIOReturnBadArgument;
    
    connection->userParam = param;
    connection->userCallout = handler;

    return kIOReturnSuccess;
}

/*****************************************************************************/
/*****************************************************************************/

IOReturn IOPMConnectionScheduleWithRunLoop(
    IOPMConnection myConnection, 
    CFRunLoopRef theRunLoop,
    CFStringRef runLoopMode)
{
    __IOPMConnection *connection = (__IOPMConnection *)myConnection;

    kern_return_t           kr = KERN_SUCCESS;
    IOReturn                return_code = kIOReturnError;
    mach_port_t             pm_server = MACH_PORT_NULL;
    mach_port_t             notify_mach_port = MACH_PORT_NULL;
    CFMachPortContext       mpContext = { 1, (void *)connection, NULL, NULL, NULL };

    if (!connection || !theRunLoop || !runLoopMode)
        return kIOReturnBadArgument;

    if (NULL == connection->localNotifyPort)
    {
        // Create the mach port on which we'll receive mach messages
        // from PM configd.
        connection->localNotifyPort = CFMachPortCreate(
                                            kCFAllocatorDefault,
                                            iopm_mach_port_callback,
                                            &mpContext, NULL);
        
        if (connection->localNotifyPort) {
            connection->localNotifyPortRLS = CFMachPortCreateRunLoopSource(
                                            kCFAllocatorDefault,
                                            connection->localNotifyPort,
                                            0);
        }   
    }

    if (!connection->localNotifyPortRLS)
        return kIOReturnInternalError;

    // Record our new run loop.
    connection->runLoopCount++;

    CFRunLoopAddSource(theRunLoop, connection->localNotifyPortRLS, runLoopMode);

    // We have a mapping of one mach_port connected to PM configd to as many
    // CFRunLoopSources that the caller originates.
    if (1 == connection->runLoopCount)
    {
        return_code = _pm_connect(&pm_server);
        if(kIOReturnSuccess != return_code) {
            goto exit;
        }

        if (connection->localNotifyPort) {
            notify_mach_port = CFMachPortGetPort(connection->localNotifyPort);
        }
    
        // If this is our first scheduled runloop source, 
        // send configd our mach_port, on which we'll listen patiently for
        // system power event messages.
        kr = io_pm_connection_schedule_notification(
                                        pm_server,
                                        connection->id,
                                        notify_mach_port,
                                        0,  // enable
                                        &return_code);

        _pm_disconnect(pm_server);
    }
    
    
    if (KERN_SUCCESS != kr) {
        return kIOReturnIPCError;
    }
exit:
    return return_code;
}

/*****************************************************************************/
/*****************************************************************************/

IOReturn IOPMConnectionUnscheduleFromRunLoop(
    IOPMConnection myConnection, 
    CFRunLoopRef theRunLoop,
    CFStringRef runLoopMode)
{
    __IOPMConnection *connection = (__IOPMConnection *)myConnection;
    mach_port_t             pm_server = MACH_PORT_NULL;
    kern_return_t       kr;
    IOReturn            return_code = kIOReturnSuccess;

    if (!connection || !theRunLoop || !runLoopMode)
        return kIOReturnBadArgument;

    if (connection->localNotifyPort) {
        CFRunLoopRemoveSource(theRunLoop, connection->localNotifyPortRLS, runLoopMode);
    }

    connection->runLoopCount--;

    if (0 == connection->runLoopCount) 
    {
        return_code = _pm_connect(&pm_server);
        if(kIOReturnSuccess != return_code) {
            goto exit;
        }
    
        // Tell PM configd that we are done receiving notifications
        kr = io_pm_connection_schedule_notification(
                                pm_server,
                                connection->id,
                                CFMachPortGetPort(connection->localNotifyPort),
                                1,  // disable
                                &return_code);

        _pm_disconnect(pm_server);

        if (kr != KERN_SUCCESS) {
            return_code = kIOReturnIPCError;
            goto exit;
        }
    }
exit:

    return return_code;
}

/*****************************************************************************/
/*****************************************************************************/

IOReturn IOPMConnectionCreate(
    CFStringRef myName, 
    IOPMSystemPowerStateCapabilities interests, 
    IOPMConnection *newConnection)
{
    __IOPMConnection        *connection = NULL;

    mach_port_t             pm_server = MACH_PORT_NULL;
    int                     return_code = kIOReturnError;
    IOReturn                err = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;

    char                    arg_name_str[kMaxNameLength];
    uint32_t                new_connection_id = 0;

    // * vet argument newConnection
    // * and create new connection
    if (!newConnection)
        return kIOReturnBadArgument;
        
    *newConnection = NULL;

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    // * vet argument 'interests'
    // A caller specifying 0 interests would get no notifications
    if (0 == interests) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    // * vet argument 'myName'
    if (!myName || (kMaxNameLength < CFStringGetLength(myName))) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    CFStringGetCString( myName, arg_name_str,
                        sizeof(arg_name_str), kCFStringEncodingMacRoman);


    connection = calloc(1, sizeof(__IOPMConnection));
    if (!connection) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    kern_result = io_pm_connection_create(
                                    pm_server,
                                    mach_task_self(),
                                    arg_name_str,
                                    interests,
                                    &new_connection_id,
                                    &return_code);

    if (KERN_SUCCESS != kern_result) {
        return_code = kern_result;
        goto exit;
    }
    
    connection->id = (int)new_connection_id;
    *newConnection = (void *)connection;    
    
    return_code = kIOReturnSuccess;

exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }

    return return_code;

}

/*****************************************************************************/
/*****************************************************************************/

IOReturn IOPMConnectionRelease(IOPMConnection connection)
{
    __IOPMConnection    *connection_private = (__IOPMConnection *)connection;
    IOReturn            return_code = kIOReturnError;
    mach_port_t         pm_server = MACH_PORT_NULL;
    kern_return_t       kern_result;
    IOReturn            err;
    
    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    kern_result = io_pm_connection_release(pm_server, 
                                            connection_private->id, 
                                            &return_code);
    if (kern_result != KERN_SUCCESS) {
        return_code = kern_result;
    }
exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }

    return return_code;
}

/*****************************************************************************/
/*****************************************************************************/

IOReturn IOPMConnectionAcknowledgeEvent(
    IOPMConnection connect, 
    IOPMConnectionMessageToken token)
{
#if TARGET_OS_EMBEDDED
    return kIOReturnUnsupported;
#else
    return IOPMConnectionAcknowledgeEventWithOptions(
                           connect, token, NULL);
#endif /* TARGET_OS_EMBEDDED */
}


/*****************************************************************************/
/*****************************************************************************/

IOReturn IOPMConnectionAcknowledgeEventWithOptions(
    IOPMConnection myConnection, 
    IOPMConnectionMessageToken token, 
    CFDictionaryRef options)
{
#if TARGET_OS_EMBEDDED
    return kIOReturnUnsupported;
#else
    __IOPMConnection    *connection = (__IOPMConnection *)myConnection;

    IOReturn            return_code = kIOReturnError;
    mach_port_t         pm_server = MACH_PORT_NULL;
    kern_return_t       kern_result;
    IOReturn            err;

    CFDataRef           serializedData = NULL;    
    vm_offset_t         buffer_ptr = 0;
    size_t              buffer_size = 0;
    
    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    if (options) 
    {
        serializedData = CFPropertyListCreateData(
                                    kCFAllocatorDefault,
                                    (CFPropertyListRef)options,
                                    kCFPropertyListBinaryFormat_v1_0, 0, NULL);
            
        if (serializedData)
        {
            buffer_ptr = (vm_offset_t)CFDataGetBytePtr(serializedData);
            buffer_size = (size_t)CFDataGetLength(serializedData);
        
        }
    }
    
    kern_result = io_pm_connection_acknowledge_event(pm_server, 
                                    (uint32_t)connection->id, 
                                    (uint32_t)token,
                                    buffer_ptr, 
                                    buffer_size,
                                    &return_code);

    if (kern_result != KERN_SUCCESS) {
        return_code = kern_result;
    }
exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }

    if (serializedData) CFRelease(serializedData);

    return return_code;
#endif /* TARGET_OS_EMBEDDED */
}

/*****************************************************************************/
/*****************************************************************************/

IOReturn IOPMCopyConnectionStatus(int statusSelector, CFTypeRef *output)
{
    int                 return_code = kIOReturnError;
    mach_port_t         pm_server = MACH_PORT_NULL;
    kern_return_t       kern_result;
    IOReturn            err;
    
    vm_offset_t         buffer_ptr = 0;
    mach_msg_type_number_t    buffer_size = 0;
 
    CFDictionaryRef     *dictionaryOutput = (CFDictionaryRef *)output;
 
    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    // TODO: serialize passed-in options into buffer_ptr & buffer_size
    *dictionaryOutput = NULL;
    
    
    kern_result = io_pm_connection_copy_status(pm_server, 
                                            (uint32_t)statusSelector, 
                                            &buffer_ptr, 
                                            &buffer_size,
                                            &return_code);

    if (kern_result != KERN_SUCCESS) {
        return_code = kern_result;
    }
exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }

    return return_code;
}


/*****************************************************************************/
/*****************************************************************************/

/*****************************************************************************/
/*****************************************************************************/

#if 0
bool IOPMSystemPowerStateSupportsAcknowledgementOption(
    IOPMSystemPowerStateCapabilities stateDescriptor,
    CFStringRef acknowledgementOption)
{
    if (!acknowledgementOption)
        return false;

    if ( 0 != (stateDescriptor & kIOPMSytemPowerStateCapabilitiesMask) )
    {
        // The flags date & requiredcapabilities are only valid on going to sleep transitions
        return false;    
    }

    if (CFEqual(acknowledgementOption, kIOPMAcknowledgmentOptionSystemCapabilityRequirements)
        || CFEqual(acknowledgementOption, kIOPMAcknowledgmentOptionWakeDate))
    {
        return true;
    }

    return false;
}
#endif
