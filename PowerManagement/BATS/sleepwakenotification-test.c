//
//  main.c
//  SleepWakeNotification-BATS
//
//  Created by Mahdi Hamzeh on 3/10/17.
//  Original code by Ethan Bold 11/1/2011
//
/*
 This test
 1- Reads current PM setting
 2- Disable all assertion typess
 3- Changes PMsetting to allow the system to go to sleep quickly
 4- Listen to Sleep messages in the following order
 a-Expects CanSleep Message, and veto it after some delay (Risk of sleeping longer that requested sleep period)
 b-Expects Will not sleep next, schedules a wake in about 2 minutes
 c-Expects CanSleep (will not veto this time)
 d-Expects WillSleep
 e-Expects WillPowerOn
 f-Expects HasPowerOn, at this point all expected Sleep and wake messages are recieved
 5- Restores PMSetting and enables assertion types
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOReturn.h>
#include <SystemConfiguration/SCValidation.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "PMtests.h"


#define PMTestLog(x...)  do {print_pretty_date(false); printf(x);} while(0);
#define PMTestPass  printf
#define PMTestFail  printf

#define SYSTEM_POWER_MAX_DELAY 30
/*************************************************************************/

static int                  willsleep_delay = 0, cansleep_delay = 0;
static int                  gFailCnt = 0, gPassCnt =0;
static CFDictionaryRef             system_power_settings = NULL;

/*************************************************************************/
enum
{
    SystemWillSleep,
    CanSystemSleep_1st,
    CanSystemSleep_2nd,
    SystemHasPoweredOn,
    SystemWillNotSleep,
    SystemWillPowerOn,
} Expected_Notification;
static void print_pretty_date(bool newline);

void RestoreSystemCurrentSetting(void);

void EnableSystemSleep(void)
{

    IOReturn                        ret;
    CFTypeRef cfnum=NULL ;
    ret = IOPMCtlAssertionType("PreventUserIdleSystemSleep" , kIOPMDisableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not disable assertion type PreventUserIdleSystemSleep with error %d\n", ret);
        goto exit;
        
    }
    ret = IOPMCtlAssertionType("PreventUserIdleDisplaySleep" , kIOPMDisableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not disable assertion type PreventUserIdleDisplaySleep with error %d\n", ret);
        goto exit;
        
    }
    ret = IOPMCtlAssertionType("BackgroundTask" , kIOPMDisableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not disable assertion type BackgroundTask with error %d\n", ret);
        goto exit;
        
    }
    ret = IOPMCtlAssertionType("PreventSystemSleep" , kIOPMDisableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not disable assertion type PreventSystemSleep with error %d\n", ret);
        goto exit;
        
    }
    ret = IOPMCtlAssertionType("NetworkClientActive" , kIOPMDisableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not disable assertion type NetworkClientActive with error %d\n", ret);
        goto exit;
        
    }
    int32_t val32 = 1;
    cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val32);

    ret = IOPMSetPMPreference(CFSTR(kIOPMSystemSleepKey), cfnum, CFSTR(kIOPMACPowerKey));
    
    if(kIOReturnNotPrivileged == ret)
    {
        FAIL("Must be run as root %d\n", ret);
    
        goto exit;

    }
    
    else if (kIOReturnSuccess != ret)
    {
        FAIL("Could not update sleep settingwith error %d\n", ret);
        goto exit;
    }
    ret = IOPMSetPMPreference(CFSTR(kIOPMDisplaySleepKey), cfnum, CFSTR(kIOPMACPowerKey));
    
    
    if(kIOReturnNotPrivileged == ret)
    {
        FAIL("Must be run as root %d\n", ret);
        goto exit;
    }
    else if (kIOReturnSuccess != ret)
    {
        FAIL("Could not update sleep settingwith error %d\n", ret);
        goto exit;
    }
    ret = IOPMSetPMPreference(CFSTR(kIOPMDiskSleepKey), cfnum, CFSTR(kIOPMACPowerKey));

    if(kIOReturnNotPrivileged == ret)
    {
        FAIL("Must be run as root %d\n", ret);
        goto exit;
    }
    else if (kIOReturnSuccess != ret)
    {
        FAIL("Could not update sleep settingwith error %d\n", ret);
        goto exit;
    }
    CFRelease(cfnum);

    return;
exit:
    if (cfnum)
        CFRelease(cfnum);
    RestoreSystemCurrentSetting();

}

/*************************************************************************/
static io_connect_t gPMAckPort = MACH_PORT_NULL;
static void
sleepWakeCallback(
                  void *refcon,
                  io_service_t y __unused,
                  natural_t messageType,
                  void * messageArgument)
{
    
    switch ( messageType ) {
        case kIOMessageSystemWillSleep:
            if (Expected_Notification != SystemWillSleep)
            {
                FAIL("Expected Notification is 0x%08x but received 0x%08x (kIOMessageSystemWillSleep)\n", Expected_Notification, kIOMessageSystemWillSleep);
                goto exit;
            }
            Expected_Notification = SystemWillPowerOn;
            PMTestLog("IORegisterForSystemPower: ...WillSleep...\n");
            fflush(stdout);
            sleep(willsleep_delay);
            IOCancelPowerChange(gPMAckPort, (long)messageArgument);
            break;
            
        case kIOMessageCanSystemSleep:
            if (Expected_Notification != CanSystemSleep_1st && Expected_Notification != CanSystemSleep_2nd )
            {
                FAIL("Expected Notification is 0x%08x but received 0x%08x (kIOMessageCanSystemSleep)\n", Expected_Notification, kIOMessageCanSystemSleep);
                goto exit;
            }

            if (Expected_Notification == CanSystemSleep_1st)
            {
                Expected_Notification = SystemWillNotSleep;
                printf("\n");
                PMTestLog("IORegisterForSystemPower: ...CanSystemSleep...\n");
                sleep(cansleep_delay);
                IOCancelPowerChange(gPMAckPort, (long)messageArgument);
            }
            else{
                Expected_Notification = SystemWillSleep;
                CFDateRef wake_time = CFDateCreate(0, CFAbsoluteTimeGetCurrent() + cansleep_delay + willsleep_delay + 60);
            

                PMTestLog("IORegisterForSystemPower: ...CanSystemSleep...Scheduled a wake in %d secs\n", cansleep_delay + willsleep_delay + 60);
                
                IOReturn ret = IOPMSchedulePowerEvent(wake_time, CFSTR("SleepWakeBATS"), CFSTR(kIOPMAutoWakeRelativeSeconds));
                if (kIOReturnSuccess != ret) {
                    FAIL("Could not schedule a wake 0x%08x", ret);
                    CFRelease(wake_time);
                }
                sleep(cansleep_delay);
                IOAllowPowerChange(gPMAckPort, (long)messageArgument);
            }
            break;
            
        case kIOMessageSystemHasPoweredOn:
            if (Expected_Notification != SystemHasPoweredOn)
            {
                FAIL("Expected Notification is 0x%08x but received 0x%08x (kIOMessageSystemHasPoweredOn)\n", Expected_Notification, kIOMessageSystemHasPoweredOn);
                goto exit;

            }
            Expected_Notification = SystemWillPowerOn;
            printf("\n");
            PMTestLog("IORegisterForSystemPower: ...HasPoweredOn...\n");
            fflush(stdout);
            PASS("All expected Sleep and Wake messages are received\n");
            CFRunLoopStop(CFRunLoopGetCurrent());
            break;
            
        case kIOMessageSystemWillNotSleep:
            if (Expected_Notification != SystemWillNotSleep)
            {
                FAIL("Expected Notification is 0x%08x but received 0x%08x (kIOMessageSystemHasPoweredOn)\n", Expected_Notification, kIOMessageSystemWillNotSleep);
                goto exit;
            }
            Expected_Notification = CanSystemSleep_2nd;
            printf("\n");
            PMTestLog("IORegisterForSystemPower: ...WillNotSleep...\n");
            fflush(stdout);
            break;
            
        case kIOMessageSystemWillPowerOn:
            if (Expected_Notification != SystemWillPowerOn)
            {
                FAIL("Expected Notification is 0x%08x but received 0x%08x (kIOMessageSystemWillPowerOn)\n", Expected_Notification, kIOMessageSystemWillPowerOn);
                goto exit;
            }
            Expected_Notification = SystemHasPoweredOn;
            printf("\n");
            PMTestLog("IORegisterForSystemPower: ...WillPowerOn...\n");
            fflush(stdout);
            break;
            
        default:
            PMTestLog("IORegisterForSystemPower: ...message: 0x%x\n", messageType);
            
    }
    
    return;
exit:
    CFRunLoopStop(CFRunLoopGetCurrent());
    return;
}


static void install_listen_IORegisterForSystemPower(void)
{
    io_object_t                 root_notifier = MACH_PORT_NULL;
    IONotificationPortRef       notify = NULL;
    
    printf("Logging IORegisterForSystemPower sleep/wake messages\n");
    
    /* Log sleep/wake messages */
    gPMAckPort = IORegisterForSystemPower (
                                           NULL, &notify,
                                           sleepWakeCallback, &root_notifier);
    
    if( notify && (MACH_PORT_NULL != gPMAckPort) )
    {
        CFRunLoopAddSource(CFRunLoopGetCurrent(),
                           IONotificationPortGetRunLoopSource(notify),
                           kCFRunLoopDefaultMode);
    }
    
    return;
}
void RestoreSystemCurrentSetting(void)
{
    IOReturn ret;
    if (system_power_settings)
    {
        ret = IOPMSetPMPreferences(system_power_settings);
        if(kIOReturnNotPrivileged == ret)
        {
            FAIL("must be run as root...\n");
        }
        else if (kIOReturnSuccess != ret)
        {
            FAIL("failed to set the value %d.\n",ret);
        }

        CFRelease(system_power_settings);
    }
    
    ret = IOPMCtlAssertionType("PreventUserIdleSystemSleep" , kIOPMEnableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not enable assertion type PreventUserIdleSystemSleep with error %d\n", ret);
        
    }
    ret = IOPMCtlAssertionType("PreventUserIdleDisplaySleep" , kIOPMEnableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not enable assertion type PreventUserIdleDisplaySleep with error %d\n", ret);
        
    }
    ret = IOPMCtlAssertionType("BackgroundTask" , kIOPMEnableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not enable assertion type BackgroundTask with error %d\n", ret);
        
    }
    ret = IOPMCtlAssertionType("PreventSystemSleep" , kIOPMEnableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not enable assertion type PreventSystemSleep with error %d\n", ret);
        
    }
    ret = IOPMCtlAssertionType("NetworkClientActive" , kIOPMEnableAssertionType);
    if (ret !=  kIOReturnSuccess  )
    {
        FAIL("Could not enable assertion type NetworkClientActive with error %d\n", ret);
        
    }
}
void ReadSystemCurrentPMSetting()
{
    system_power_settings = IOPMCopyActivePMPreferences();
    if(!isA_CFDictionary(system_power_settings)) {
        FAIL("Problem with power system setting on the host machine\n");
        exit(1);
    }
}
/*************************************************************************/

int main(int argc, char *argv[])
{
    
    //delay power event acknowledge for 1 secs less than acceptable delay
    cansleep_delay = SYSTEM_POWER_MAX_DELAY - 1;
    
    willsleep_delay = SYSTEM_POWER_MAX_DELAY - 1;
    
    ReadSystemCurrentPMSetting();
    
    install_listen_IORegisterForSystemPower();

    Expected_Notification = CanSystemSleep_1st;

    EnableSystemSleep();
    
    PMTestLog("Test started\n");
    
    CFRunLoopRun();
    RestoreSystemCurrentSetting();
    SUMMARY("Sleep Wake Notification");
    return 0;

}
/*************************************************************************/

static void print_pretty_date(bool newline)
{
    CFDateFormatterRef  date_format         = NULL;
    CFTimeZoneRef       tz                  = NULL;
    CFStringRef         time_date           = NULL;
    CFLocaleRef         loc                 = NULL;
    char                _date[60];
    
    loc = CFLocaleCopyCurrent();
    if (loc) {
        date_format = CFDateFormatterCreate(0, loc, kCFDateFormatterShortStyle, kCFDateFormatterLongStyle);
        CFRelease(loc);
    }
    if (date_format) {
        tz = CFTimeZoneCopySystem();
        if (tz) {
            CFDateFormatterSetProperty(date_format, kCFDateFormatterTimeZone, tz);
            CFRelease(tz);
        }
        time_date = CFDateFormatterCreateStringWithAbsoluteTime(0, date_format, CFAbsoluteTimeGetCurrent());
        CFRelease(date_format);
    }
    if(time_date) {
        CFStringGetCString(time_date, _date, 60, kCFStringEncodingUTF8);
        printf("%s ", _date); fflush(stdout);
        if(newline) printf("\n");
        CFRelease(time_date);
    }
}
