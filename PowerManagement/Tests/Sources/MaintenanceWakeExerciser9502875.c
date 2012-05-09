
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOReturn.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "PMTestLib.h"


#ifndef kIOPMAcknowledgementOptionSleepServiceCapTimeout
#define kIOPMAcknowledgementOptionSleepServiceCapTimeout kIOPMAcknowledgeOptionSleepServiceCapTimeout
#endif

/*************************************************************************/
static int _sleepServiceWakePeriodSec = 15;

enum {
    kTestStageMaintenance = 0,
    kTestStageSleepServices = 1,
};

int _TestStage = kTestStageMaintenance;

static void print_pretty_date(bool newline);
void myPMConnectionHandler(void *param, IOPMConnection connection,
        IOPMConnectionMessageToken token, IOPMSystemPowerStateCapabilities capabilities);

void usage(char *name) {
    printf("usage: %s <minutes delta maintenance wake>\n", name);
    printf("\tUpon a notification that the system is going to sleep, we will request a maintenance wake in <minutes delta maintenance wake seconds.\n");
}

/*************************************************************************/

int main(int argc, char *argv[])
{
    IOReturn            ret;
    IOPMConnection      myConnection;
    
    PMTestInitialize("Maintenance & SleepService basic wake", "com.apple.iokit.power");
    /* 
     * Test PM Connection SLeepService arguments
     */


    ret = IOPMConnectionCreate(CFSTR("SleepWakeLogTool"), 
                kIOPMSystemPowerStateCapabilityDisk | kIOPMSystemPowerStateCapabilityNetwork
              | kIOPMSystemPowerStateCapabilityAudio | kIOPMSystemPowerStateCapabilityVideo,
                &myConnection);
    if (kIOReturnSuccess != ret) {
        PMTestFail("Error 0x%08x from IOPMConnectionCreate.\n", ret);
        exit(1);
    }

    ret = IOPMConnectionSetNotification(myConnection, NULL, (IOPMEventHandlerType)myPMConnectionHandler);
    if (kIOReturnSuccess != ret) {
        PMTestFail("Error 0x%08x from IOPMConnectionSetNotification.\n", ret);
        exit(1);
    }

    ret = IOPMConnectionScheduleWithRunLoop(myConnection, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    if (kIOReturnSuccess != ret) {
        PMTestFail("Error 0x%08x from IOPMConnectionScheduleWithRunloop.\n", ret);
        exit(1);
    }

    PMTestLog("%s - test maintenance wake & SleepService wake intervals", argv[0]);
    PMTestLog(" (1) Set a a maintenance wake interval in %d seconds", _sleepServiceWakePeriodSec);
    PMTestLog(" (2) Sleep the system (IOPMSleepSystem)");
    PMTestLog(" (3) [TEST] On wake; verify that system is in dark wake.");
    PMTestLog(" (4) Set a a sleepService wake interval in %d seconds", _sleepServiceWakePeriodSec);
    PMTestLog(" (5) Let the system return to sleep (natural maintenance timeout)");
    PMTestLog(" (6) [TEST] On wake; verify that the system is in dark wake");
    PMTestLog(" (7) The test is over. Create a user active assertion to light the display.");

    PMTestLog("Running with a sleepservice wakeup interval: %ds\n", _sleepServiceWakePeriodSec);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        io_connect_t fb;
        fb = IOPMFindPowerManagement(MACH_PORT_NULL);
        if ( MACH_PORT_NULL != fb ) {
            IOPMSleepSystem ( fb );
            IOServiceClose(fb);
        }
        });

    CFRunLoopRun();

    return 0;
}

/*************************************************************************/
CFDictionaryRef makeAcknowledgementDictionaryForSleepServices(void)
{
    CFDateRef               sleepServiceDate        = NULL;
    CFNumberRef             sleepCapNum             = NULL;
    int                     sleepServiceCapTimeout  = 0;

    CFMutableDictionaryRef  ackDictionary;

    PMTestLog("Acknowledgement dictionary requests SleepService in %d sec", _sleepServiceWakePeriodSec);

    
    ackDictionary = CFDictionaryCreateMutable(0, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (ackDictionary) 
    {
        if ((sleepServiceDate = CFDateCreate(0, CFAbsoluteTimeGetCurrent() + (CFTimeInterval)_sleepServiceWakePeriodSec))) 
        {
            CFDictionarySetValue(ackDictionary, kIOPMAcknowledgementOptionSleepServiceDate, sleepServiceDate);
            CFRelease(sleepServiceDate);
        }

        sleepServiceCapTimeout = 30*1000; // 30 sec = 30,000 msec
        if ((sleepCapNum = CFNumberCreate(0, kCFNumberSInt32Type, &sleepServiceCapTimeout))) 
        {
            CFDictionarySetValue(ackDictionary, kIOPMAcknowledgeOptionSleepServiceCapTimeout, sleepCapNum);
            CFRelease(sleepCapNum);
        }
    }
    return (CFDictionaryRef)ackDictionary;
}

/*************************************************************************/
CFDictionaryRef makeAcknowledgementDictionaryForMaintenanceWake(void)
{
    CFDateRef               maintenanceDate = NULL;
    CFNumberRef             maintenanceBits = NULL;

    CFMutableDictionaryRef  ackDictionary;
    
    PMTestLog("Acknowledgement dictionary requests MaintenanceWake in %d sec", _sleepServiceWakePeriodSec);
    
    ackDictionary = CFDictionaryCreateMutable(0, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (ackDictionary) 
    {
        maintenanceDate = CFDateCreate(0, CFAbsoluteTimeGetCurrent() + (CFTimeInterval)_sleepServiceWakePeriodSec);
        if (maintenanceDate) {
            CFDictionarySetValue(ackDictionary, kIOPMAcknowledgmentOptionWakeDate, maintenanceDate);
        }

        uint32_t _maintenanceBits = kIOPMSystemPowerStateCapabilityNetwork | kIOPMSystemPowerStateCapabilityDisk;
        maintenanceBits = CFNumberCreate(0, kCFNumberSInt32Type, &_maintenanceBits);
        if (maintenanceBits) {
            CFDictionarySetValue(ackDictionary, kIOPMAcknowledgmentOptionSystemCapabilityRequirements, maintenanceBits);
        }
    }
    return (CFDictionaryRef)ackDictionary;
}

/*************************************************************************/

#define CPU_CAPABILITY_SET(x)       (0 != (x & kIOPMSystemPowerStateCapabilityCPU))
#define CAPABILITY_CLEAR(x, b)      (0 == (x & b))
#define DARK_WAKE_CAPABILITIES(x)   (CPU_CAPABILITY_SET(x) \
                                        && CAPABILITY_CLEAR(x, kIOPMSystemPowerStateCapabilityVideo) \
                                        && CAPABILITY_CLEAR(x, kIOPMSystemPowerStateCapabilityAudio))

void myPMConnectionHandler(
    void *param, 
    IOPMConnection                      connection,
    IOPMConnectionMessageToken          token, 
    IOPMSystemPowerStateCapabilities    capabilities)
{
    IOReturn                ret         = kIOReturnSuccess;
    CFDictionaryRef  ackDictionary      = NULL;
    
    print_pretty_date(true);
    
    if (CPU_CAPABILITY_SET(capabilities))
    {
        PMTestPass("System wake - capabities = 0x%02x", capabilities);

        /*****************************************************
            Handle wake up
        *****************************************************/    

        if (kTestStageMaintenance == _TestStage)
        {
            // ASSERT: We are waking for a scheduled maintenance wake.

            if (DARK_WAKE_CAPABILITIES(capabilities)) {
                PMTestPass("MaintenanceStage - Woke up into dark wake for Maintenance wake.");
            } else {
                PMTestFail("MaintenanceStage - Expected to wake into a scheduled maintenance wake. Instead, system is fully awake.");
            }
            
            _TestStage = kTestStageSleepServices;

            ret = IOPMConnectionAcknowledgeEvent(connection, token);    
            if (kIOReturnSuccess != ret) {
                PMTestFail("-> AwakeToDarkWake Acknowledgement error 0x%08x\n", ret);    
            }            
        } else if (kTestStageSleepServices == _TestStage)
        {
            
            PMTestPass("Woke up for SleepServices wake.");
            
            /* END TEST: If we successfully woke into dark wake for SleepServices at the expected time;
                light the display, exit dark wake, and exit the app.
             */
            sleep(1);
            exit(1);
        }
        
    } else {
        PMTestLog("Going to sleep.\n");

        /*****************************************************
            Handle go to sleep
        *****************************************************/    

        if (0 != capabilities) {
            PMTestFail("\tError! Sleep capabilities=0x%02x; should be 0.\n", capabilities);
        }
        
        if (kTestStageMaintenance == _TestStage)
        {
            ackDictionary = makeAcknowledgementDictionaryForMaintenanceWake();
        } else if (kTestStageSleepServices == _TestStage)
        {
            ackDictionary = makeAcknowledgementDictionaryForSleepServices();
        }

        if (ackDictionary)
        {
            ret = IOPMConnectionAcknowledgeEventWithOptions(connection, token, ackDictionary);
            
            if (kIOReturnSuccess == ret) {
                PMTestPass("SUCCESS Acknowledged IOPMConnection notification with SleepService arguments.");
            } else {
                PMTestFail("FAILURE IOPMConnectionAcknowledgeEventWithOptions() returns 0x%08x", ret);
            }

            CFRelease(ackDictionary);
        }
    }

    return;
}


static void print_pretty_date(bool newline)
{
    CFAbsoluteTime      _showtime = CFAbsoluteTimeGetCurrent();
    CFDateFormatterRef  date_format;
    CFTimeZoneRef       tz;
    CFStringRef         time_date;
    CFLocaleRef         loc;
    char                _date[60];
 
   loc = CFLocaleCopyCurrent();
    date_format = CFDateFormatterCreate(kCFAllocatorDefault, loc,
        kCFDateFormatterShortStyle, kCFDateFormatterLongStyle);        
    CFRelease(loc);
    tz = CFTimeZoneCopySystem();
    CFDateFormatterSetProperty(date_format, kCFDateFormatterTimeZone, tz);
    CFRelease(tz);
    time_date = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault,
        date_format, _showtime);
    CFRelease(date_format);
    if(time_date)
    {
        CFStringGetCString(time_date, _date, 60, kCFStringEncodingUTF8);
        PMTestLog("%s ", _date); fflush(stdout);
        if(newline) printf("\n");
    }
    CFRelease(time_date); 
}
