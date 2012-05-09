/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include <TargetConditionals.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include "IOSystemConfiguration.h"
#include "IOPMKeys.h"
#include "IOPMLib.h"
#include "IOPMLibPrivate.h"
#include "IOPMPrivate.h"

#include "powermanagement_mig.h"
#include "powermanagement.h"

#include <servers/bootstrap.h>
#include <notify.h>

enum {
    kIOPMMaxScheduledEntries = 1000
};

#ifndef kIOPMMaintenanceScheduleImmediate
#define kIOPMMaintenanceScheduleImmediate   "MaintenanceImmediate"
#endif

#ifndef kPMSetMaintenanceWakeCalendar
#define kPMSetMaintenanceWakeCalendar 8
#endif

// Forward decls

static CFAbsoluteTime roundOffDate( 
    CFAbsoluteTime time);
static CFDictionaryRef _IOPMCreatePowerOnDictionary(
    CFAbsoluteTime the_time, 
    CFStringRef the_id, 
    CFStringRef type);
static bool inputsValid(
    CFDateRef time_to_wake, 
    CFStringRef my_id, 
    CFStringRef type);
static IOReturn _setRootDomainProperty(
    CFStringRef key,
    CFTypeRef val);
static void tellClockController(
    CFStringRef command,
    CFDateRef power_date);
IOReturn IOPMSchedulePowerEvent(
    CFDateRef time_to_wake, 
    CFStringRef my_id, 
    CFStringRef type);
IOReturn IOPMCancelScheduledPowerEvent(
    CFDateRef time_to_wake, 
    CFStringRef my_id, 
    CFStringRef wake_or_restart);    
CFArrayRef IOPMCopyScheduledPowerEvents( void );

static IOReturn doAMaintenanceWake(CFDateRef earliestRequest, int type);

__private_extern__ IOReturn _copyPMServerObject(int selector, int assertionID, CFTypeRef *objectOut);


IOReturn _pm_connect(mach_port_t *newConnection);
IOReturn _pm_disconnect(mach_port_t connection);

#if TARGET_OS_EMBEDDED
#define ROUND_SCHEDULE_TIME	(5.0)
#define MIN_SCHEDULE_TIME	(5.0)
#else
#define ROUND_SCHEDULE_TIME	(5.0)
#define MIN_SCHEDULE_TIME	(5.0)
#endif  /* TARGET_OS_EMBEDDED */


static CFAbsoluteTime roundOffDate(CFAbsoluteTime time)
{
    // round time down to the closest multiple of ROUND_SCHEDULE_TIME seconds
    // CFAbsoluteTimes are encoded as doubles
    return (CFAbsoluteTime) (trunc(time / ((double) ROUND_SCHEDULE_TIME)) * ((double) ROUND_SCHEDULE_TIME)); 
}

static CFDictionaryRef 
_IOPMCreatePowerOnDictionary(
    CFAbsoluteTime the_time, 
    CFStringRef the_id, 
    CFStringRef type)
{
    CFMutableDictionaryRef          d;
    CFDateRef                       the_date;

    // make sure my_id is valid or NULL
    the_id = isA_CFString(the_id);        
    // round wakeup time to last ROUND_SCHEDULE_TIME second increment
    the_time = roundOffDate(the_time);
    // package AbsoluteTime as a date for CFType purposes
    the_date = CFDateCreate(0, the_time);
    d = CFDictionaryCreateMutable(0, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(!d) return NULL;
    CFDictionaryAddValue(d, CFSTR(kIOPMPowerEventTimeKey), the_date);
    if(!the_id) the_id = CFSTR("");
    CFDictionaryAddValue(d, CFSTR(kIOPMPowerEventAppNameKey), the_id);
    CFDictionaryAddValue(d, CFSTR(kIOPMPowerEventTypeKey), type);
    CFRelease(the_date);
    return d;
}

static bool 
inputsValid(
    CFDateRef time_to_wake, 
    CFStringRef my_id __unused, 
    CFStringRef type)
{
    // NULL is an acceptable input for my_id
    
    // NULL is only an accetable input to IOPMSchedulePowerEvent
    // if the type of event being scheduled is Immediate; in which
    // case NULL means "zero out current settings" to the hardware.
    if (!isA_CFDate(time_to_wake)
        && !CFEqual(type, CFSTR(kIOPMAutoWakeScheduleImmediate))
        && !CFEqual(type, CFSTR(kIOPMAutoPowerScheduleImmediate)))
    {
        return false;
    }
    
    if(!isA_CFString(type)) return false;
    if(!(CFEqual(type, CFSTR(kIOPMAutoWake)) || 
        CFEqual(type, CFSTR(kIOPMAutoPowerOn)) ||
        CFEqual(type, CFSTR(kIOPMAutoWakeOrPowerOn)) ||
        CFEqual(type, CFSTR(kIOPMAutoSleep)) ||
        CFEqual(type, CFSTR(kIOPMAutoShutdown)) ||
        CFEqual(type, CFSTR(kIOPMAutoRestart)) ||
        CFEqual(type, CFSTR(kIOPMAutoWakeScheduleImmediate)) ||
        CFEqual(type, CFSTR(kIOPMAutoPowerScheduleImmediate)) ||
        CFEqual(type, CFSTR(kIOPMAutoWakeRelativeSeconds)) ||
        CFEqual(type, CFSTR(kIOPMAutoPowerRelativeSeconds)) ||
        CFEqual(type, CFSTR(kIOPMMaintenanceScheduleImmediate)) ||
        CFEqual(type, CFSTR(kIOPMSleepServiceScheduleImmediate)) ))
    {
        return false;
    }

    return true;
}


static IOReturn 
_setRootDomainProperty(
    CFStringRef                 key, 
    CFTypeRef                   val) 
{
    io_registry_entry_t         root_domain;
    IOReturn                    ret;

    root_domain = IORegistryEntryFromPath( kIOMasterPortDefault, 
                kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    if(!root_domain) return kIOReturnError;
 
    ret = IORegistryEntrySetCFProperty(root_domain, key, val);

    IOObjectRelease(root_domain);
    return ret;
}

static IOReturn doAMaintenanceWake(
    CFDateRef                   earliestRequest,
    int                         type)
 {
    CFGregorianDate         maintGregorian;
    IOPMCalendarStruct      pmMaintenanceDate;
    IOReturn                connectReturn = 0;
    size_t                  connectReturnSize = sizeof(IOReturn);
    io_connect_t            root_domain_connect = IO_OBJECT_NULL;
    io_registry_entry_t     root_domain = IO_OBJECT_NULL;
    IOReturn                ret = kIOReturnError;
    kern_return_t           kr = -1;

    // Package maintenance time as a PMCalendarType and pass it into IOPMrootDomain
    CFTimeZoneRef  myTimeZone = NULL;
    
    myTimeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(0, 0.0);
    if (!myTimeZone)
        goto exit;

    maintGregorian = CFAbsoluteTimeGetGregorianDate(CFDateGetAbsoluteTime(earliestRequest), myTimeZone);

    CFRelease(myTimeZone);
    
    // Stuff into PM Calendar struct
    bzero(&pmMaintenanceDate, sizeof(pmMaintenanceDate));
    pmMaintenanceDate.year      = maintGregorian.year;
    pmMaintenanceDate.month     = maintGregorian.month;
    pmMaintenanceDate.day       = maintGregorian.day;
    pmMaintenanceDate.hour      = maintGregorian.hour;
    pmMaintenanceDate.minute    = maintGregorian.minute;
    pmMaintenanceDate.second    = maintGregorian.second;
    pmMaintenanceDate.selector  = type;

    // Open up RootDomain
    
    root_domain = IORegistryEntryFromPath( kIOMasterPortDefault, 
                kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    if (root_domain != IO_OBJECT_NULL)
    {
        kr = IOServiceOpen(root_domain, mach_task_self(), 0, &root_domain_connect);        
        
        if (KERN_SUCCESS == kr)
        {
            kr = IOConnectCallStructMethod(
                root_domain_connect, 
                kPMSetMaintenanceWakeCalendar,
                &pmMaintenanceDate, sizeof(pmMaintenanceDate),      // inputs struct
                &connectReturn, &connectReturnSize);                // outputs struct
    
            IOServiceClose(root_domain_connect);
        }
        
        IOObjectRelease(root_domain);
    }

    if ((KERN_SUCCESS != kr) || (kIOReturnSuccess != connectReturn))
    {
        ret = kIOReturnError;
        goto exit;
    }

    ret = kIOReturnSuccess;
exit:
    return ret;
}

static void 
tellClockController(
    CFStringRef command, 
    CFDateRef power_date)
{
    CFAbsoluteTime          now, wake_time;
    CFGregorianDate         gmt_calendar;
    CFTimeZoneRef           gmt_tz = NULL;
    long int                diff_secs;
    IOReturn                ret;
    CFNumberRef             seconds_delta = NULL;
    IOPMCalendarStruct      *cal_date = NULL;
    CFMutableDataRef        date_data = NULL;

    if(!command) goto exit;
    
    // We broadcast the wakeup time both as calendar date struct and as seconds.
    //  * AppleRTC hardware needs the date in a structured calendar format

    // ******************** Calendar struct ************************************
    
    date_data = CFDataCreateMutable(NULL, sizeof(IOPMCalendarStruct));
    CFDataSetLength(date_data, sizeof(IOPMCalendarStruct));
    cal_date = (IOPMCalendarStruct *)CFDataGetBytePtr(date_data);
    bzero(cal_date, sizeof(IOPMCalendarStruct));

    if(!power_date) {
    
        // Zeroed out calendar means "clear wakeup timer"

    } else {

        // A calendar struct stuffed with meaningful date and time
        // schedules a wake or power event for then.

        wake_time = CFDateGetAbsoluteTime(power_date);

        gmt_tz = CFTimeZoneCreateWithTimeIntervalFromGMT(0, 0.0);
        gmt_calendar = CFAbsoluteTimeGetGregorianDate(wake_time, gmt_tz);
        CFRelease(gmt_tz);

        cal_date->second    = lround(gmt_calendar.second);
        if (60 == cal_date->second)
            cal_date->second = 59;
        cal_date->minute    = gmt_calendar.minute;
        cal_date->hour      = gmt_calendar.hour;
        cal_date->day       = gmt_calendar.day;
        cal_date->month     = gmt_calendar.month;
        cal_date->year      = gmt_calendar.year;
    }
    
    if(CFEqual(command, CFSTR(kIOPMAutoWake))) {

        // Set AutoWake calendar property
        ret = _setRootDomainProperty(CFSTR(kIOPMSettingAutoWakeCalendarKey), date_data);
    } else {

        // Set AutoPower calendar property
        ret = _setRootDomainProperty(CFSTR(kIOPMSettingAutoPowerCalendarKey), date_data);    
    }

    if(kIOReturnSuccess != ret) {
        goto exit;
    }


    // *************************** Seconds *************************************
    // ApplePMU/AppleSMU seconds path
    // Machine needs to be told alarm in seconds relative to current time.

    if(!power_date) {
        // NULL dictionary argument, clear wakeup timer
        diff_secs = 0;
    } else {
        // Assume a well-formed entry since we've been doing thorough 
        // type-checking in the find & purge functions
        now = CFAbsoluteTimeGetCurrent();
        wake_time = CFDateGetAbsoluteTime(power_date);
        
        diff_secs = lround(wake_time - now);
        if(diff_secs < 0) goto exit;
    }
    
    // Package diff_secs as a CFNumber
    seconds_delta = CFNumberCreate(0, kCFNumberLongType, &diff_secs);
    if(!seconds_delta) goto exit;
    
    if(CFEqual(command, CFSTR(kIOPMAutoWake))) {

        // Set AutoWake seconds property
        ret = _setRootDomainProperty(CFSTR(kIOPMSettingAutoWakeSecondsKey), seconds_delta);
    } else {

        // Set AutoPower seconds property
        ret = _setRootDomainProperty(CFSTR(kIOPMSettingAutoPowerSecondsKey), seconds_delta);
    }

    if(kIOReturnSuccess != ret) {
        goto exit;
    }

exit:
    if(date_data) CFRelease(date_data);
    if(seconds_delta) CFRelease(seconds_delta);
    return;
}


IOReturn IOPMSchedulePowerEvent(
    CFDateRef time_to_wake, 
    CFStringRef my_id,
    CFStringRef type)
{
    CFDictionaryRef         package = 0;
    IOReturn                ret = kIOReturnError;
    CFAbsoluteTime          abs_time_to_wake;
    CFDataRef               flatPackage = NULL;
    kern_return_t           rc = KERN_SUCCESS;

    //  verify inputs
    if(!inputsValid(time_to_wake, my_id, type))
    {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    if( CFEqual(type, CFSTR(kIOPMMaintenanceScheduleImmediate)) )
    {
        ret = doAMaintenanceWake(time_to_wake, kPMCalendarTypeMaintenance);
        goto exit;
    } else if (CFEqual(type, CFSTR(kIOPMSleepServiceScheduleImmediate)) )
    {
        ret = doAMaintenanceWake(time_to_wake, kPMCalendarTypeSleepService);
        goto exit;
    } else if( CFEqual(type, CFSTR(kIOPMAutoWakeScheduleImmediate)) )
    {

        // Just send down the wake event immediately
        tellClockController(CFSTR(kIOPMAutoWake), time_to_wake);
        ret = kIOReturnSuccess;
        goto exit;

    } else if( CFEqual(type, CFSTR(kIOPMAutoPowerScheduleImmediate)) )
    {

        // Just send down the power on event immediately
        tellClockController(CFSTR(kIOPMAutoPowerOn), time_to_wake);
        ret = kIOReturnSuccess;
        goto exit;

    } else if( CFEqual( type, CFSTR( kIOPMAutoWakeRelativeSeconds) ) 
            || CFEqual( type, CFSTR( kIOPMAutoPowerRelativeSeconds) ) )
    {

        // Immediately send down a relative seconds argument
        // Seconds are relative to "right now" in CFAbsoluteTime
        CFAbsoluteTime      now_secs;
        CFAbsoluteTime      event_secs;
        CFNumberRef         diff_secs = NULL;
        int                 diff;

        if(time_to_wake)
        {
            now_secs = CFAbsoluteTimeGetCurrent();
            event_secs = CFDateGetAbsoluteTime(time_to_wake);
            diff = (int)event_secs - (int)now_secs;
            if(diff <= 0)
            {
                // Only positive diffs are meaningful
                return kIOReturnIsoTooOld;
            }
        } else {
            diff = 0;
        }
                
        diff_secs = CFNumberCreate(0, kCFNumberIntType, &diff);
        if(!diff_secs) goto exit;
        
        _setRootDomainProperty( type, (CFTypeRef)diff_secs );

        CFRelease(diff_secs);

        ret = kIOReturnSuccess;
        goto exit;
    }

    abs_time_to_wake = CFDateGetAbsoluteTime(time_to_wake);
    if(abs_time_to_wake < (CFAbsoluteTimeGetCurrent() + MIN_SCHEDULE_TIME))
    {
        ret = kIOReturnNotReady;
        goto exit;
    }
    
    mach_port_t         pm_server = MACH_PORT_NULL;
    
    if(kIOReturnSuccess != _pm_connect(&pm_server)) {
        ret = kIOReturnInternalError;
        goto exit;
    }
    
    // Package the event in a CFDictionary
    package = _IOPMCreatePowerOnDictionary(abs_time_to_wake, my_id, type);
    flatPackage = CFPropertyListCreateData(0, package,
                          kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
    if ( !flatPackage ) {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    rc = io_pm_schedule_power_event(pm_server, (vm_offset_t)CFDataGetBytePtr(flatPackage), 
                CFDataGetLength(flatPackage), 1, &ret);
    if (rc != KERN_SUCCESS)
        ret = kIOReturnInternalError;
    
    notify_post(kIOPMSchedulePowerEventNotification);

exit:
    
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }
    if(package) CFRelease(package);
    if(flatPackage) CFRelease(flatPackage);
    return ret;
}


IOReturn IOPMCancelScheduledPowerEvent(
    CFDateRef time_to_wake, 
    CFStringRef my_id, 
    CFStringRef wake_or_restart)
{
    CFDictionaryRef         package = 0;
    IOReturn                ret = kIOReturnSuccess;
    mach_port_t             pm_server = MACH_PORT_NULL;
    kern_return_t           err = KERN_SUCCESS;
    CFAbsoluteTime          abs_time_to_wake;
    CFDataRef               flatPackage = NULL;
    
    if(!inputsValid(time_to_wake, my_id, wake_or_restart)) 
    {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        ret = kIOReturnInternalError;
        goto exit;
    }

    abs_time_to_wake = CFDateGetAbsoluteTime(time_to_wake);
    package = _IOPMCreatePowerOnDictionary(abs_time_to_wake, my_id, wake_or_restart);
    flatPackage = CFPropertyListCreateData(0, package,
                          kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
    if ( !flatPackage ) {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    io_pm_schedule_power_event(pm_server, 
            (vm_offset_t)CFDataGetBytePtr(flatPackage), CFDataGetLength(flatPackage), 0, &ret);


exit:

    if (pm_server != MACH_PORT_NULL)
        _pm_disconnect(pm_server);
    if(package) CFRelease(package);
    if(flatPackage) CFRelease(flatPackage);
    return ret;
}

CFArrayRef IOPMCopyScheduledPowerEvents(void)
{
    CFMutableArrayRef           new_arr = NULL;

    _copyPMServerObject(kIOPMPowerEventsMIGCopyScheduledEvents, 0, (CFTypeRef *)&new_arr);

    return (CFArrayRef)new_arr;
}
