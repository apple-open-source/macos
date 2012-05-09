
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOReturn.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "PMTestLib.h"

#ifndef kIOPMAssertionTypeApplePushServiceTask
#define kIOPMAssertionTypeApplePushServiceTask kIOPMAssertionTypeDenySystemSleep
#endif


#ifndef kIOPMAcknowledgementOptionSleepServiceDate
#define kIOPMAcknowledgementOptionSleepServiceDate kIOPMAcknowledgmentOptionWakeDate
#endif

#ifndef kIOPMAcknowledgeOptionSleepServiceCapTimeout
#define kIOPMAcknowledgeOptionSleepServiceCapTimeout kIOPMAcknowledgmentOptionSystemCapabilityRequirements
#endif

/*************************************************************************/

static int _sleepServiceWakePeriodSec = 15;

/*************************************************************************/

static void print_pretty_date(bool newline);
static void print_pretty_abstime(bool newline, CFAbsoluteTime _showtime);

static void assertSleepServiceAssertionType(void);

void myPMConnectionHandler(
    void *param, IOPMConnection connection,
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
    
    /* 
     * Test assertion type kIOPMAssertionTypeApplePushServiceTask
     */
    assertSleepServiceAssertionType();

    /* 
     * Test PM Connection SLeepService arguments
     */


    ret = IOPMConnectionCreate(
                        CFSTR("SleepWakeLogTool"),
                        kIOPMSystemPowerStateCapabilityDisk 
                            | kIOPMSystemPowerStateCapabilityNetwork
                            | kIOPMSystemPowerStateCapabilityAudio 
                            | kIOPMSystemPowerStateCapabilityVideo,
                        &myConnection);

    if (kIOReturnSuccess != ret) {
        PMTestFail("Error 0x%08x from IOPMConnectionCreate.\n", ret);
        exit(1);
    }

    ret = IOPMConnectionSetNotification(
                        myConnection, NULL,
                        (IOPMEventHandlerType)myPMConnectionHandler);

    if (kIOReturnSuccess != ret) {
        PMTestFail("Error 0x%08x from IOPMConnectionSetNotification.\n", ret);
        exit(1);
    }

    ret = IOPMConnectionScheduleWithRunLoop(
                        myConnection, CFRunLoopGetCurrent(),
                        kCFRunLoopDefaultMode);

    if (kIOReturnSuccess != ret) {
        PMTestFail("Error 0x%08x from IOPMConnectionScheduleWithRunloop.\n", ret);
        exit(1);
    }

    PMTestLog("Running with a sleepservice wakeup interval: %ds\n", _sleepServiceWakePeriodSec);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        io_connect_t                    fb;
        fb = IOPMFindPowerManagement(MACH_PORT_NULL);
        if ( MACH_PORT_NULL != fb ) {
            IOPMSleepSystem ( fb );
            IOServiceClose(fb);
        }
        });

    CFRunLoopRun();

    return 0;
}

static void assertSleepServiceAssertionType(void)
{
    IOPMAssertionID     newAssertionType = kIOPMNullAssertionID;
    CFDictionaryRef     d = NULL;
    IOReturn            ret;
    CFNumberRef         obj = NULL;
    int                 level;
    
    ret = IOPMAssertionCreateWithName(kIOPMAssertionTypeApplePushServiceTask, kIOPMAssertionLevelOn, 
        CFSTR("com.apple.power.test"), &newAssertionType);
    if (kIOReturnSuccess == ret) {
        PMTestPass("Created assertion for ApplePushServiceTask\n");
    } else {
        PMTestFail("FAILURE: Created assertion for ApplePushServiceTask return 0x%08x", ret);
    }
    
    IOPMCopyAssertionsStatus(&d);
    CFShow(d);
    if (!d || !(obj = CFDictionaryGetValue(d, kIOPMAssertionTypePreventSystemSleep)))
    {
        PMTestFail("Could not get dictionary, or dictionary did not contain PreventSystemSleep key.");
    }
    if (obj) {
        CFNumberGetValue(obj, kCFNumberIntType, &level);
        if (0 != level) {
            PMTestPass("Assertion level is ON for PreventSystemSleep");
        } else {
            PMTestFail("Assertion level is OFF for PreventSystemSleep");
        }
    }
    if (d) CFRelease(d);



    IOPMAssertionRelease(newAssertionType);
}

/*************************************************************************/

static void print_pretty_date(bool newline)
{
    print_pretty_abstime(newline, CFAbsoluteTimeGetCurrent()); 
}

/*************************************************************************/

static void print_pretty_abstime(bool newline, CFAbsoluteTime _showtime)
{
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

/*************************************************************************/

void myPMConnectionHandler(
    void *param, 
    IOPMConnection                      connection,
    IOPMConnectionMessageToken          token, 
    IOPMSystemPowerStateCapabilities    capabilities)
{
    IOReturn                ret                     = kIOReturnSuccess;
    CFMutableDictionaryRef  ackDictionary           = NULL;
    CFDateRef               sleepServiceDate        = NULL;
    CFNumberRef             sleepCapNum             = NULL;
    int                     sleepServiceCapTimeout  = 0;
    
    print_pretty_date(true);
    
    if ( capabilities & kIOPMSystemPowerStateCapabilityCPU )
    {
        print_pretty_date(false);
        PMTestPass("System wake - capabities = 0x%02x", capabilities);

        /* END TEST: If we successfully woke into dark wake for SleepServices at the expected time;
            light the display, exit dark wake, and exit the app.
         */
//        IOPMAssertionID   wakeDisplay;
//        ret = IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn, 
//            CFSTR("com.apple.power.test"), &wakeDisplay);
        sleep(1);
        exit(1);
/*        
        ret = IOPMConnectionAcknowledgeEvent(connection, token);    
        if (kIOReturnSuccess != ret)
        {
            PMTestFail("\t-> Acknowledgement error 0x%08x\n", ret);    
        }
*/
    } else {
        PMTestLog("Going to sleep.\n");

        if (0 != capabilities) {
            PMTestFail("\tError! Sleep capabilities=0x%02x; should be 0.\n", capabilities);
            exit(1);
        }
        
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
