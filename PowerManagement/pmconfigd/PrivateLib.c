/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/ps/IOPSKeys.h>
#include <mach/mach.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#include "PrivateLib.h"

enum
{
    PowerManagerScheduledShutdown = 1,
    PowerManagerScheduledSleep,
    PowerManagerScheduledRestart
};

/* If the battery doesn't specify an alternative time, we wait 16 seconds
   of ignoring the battery's (or our own) time remaining estimate. We choose
   the number 16 seconds because PMU based PPC machines have a 15 second
   battery polling cycle, and this 16 second timer should guarantee a 
   valid time remaining estimate on PPC.
*/   
enum
{
    kInvalidWakeSecsDefault = 16
};

#define kPowerManagerActionNotificationName "com.apple.powermanager.action"
#define kPowerManagerActionKey "action"
#define kPowerManagerValueKey "value"

// Tracks system battery state
static int batCount = 0;
static IOPMBattery **batteries = NULL;

/******
 * Do not remove DUMMY macros
 *
 * The following DUMMY_* macros aren't used in the source code, but they're
 * here as a dummy code for our localization pre-processor scripts to see the strings
 * we're using in CFCopyLocalizedStringWithDefaultValue. Several localization
 * tools analyze the arguments to these calls to auto-generate loc files.
 ******/

#define DUMMY_UPS_HEADER(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("WARNING!"), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("Warning!"), \
            NULL);
            
#define DUMMY_UPS_BODY(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("YOUR COMPUTER IS NOW RUNNING ON UPS BACKUP BATTERY. SAVE YOUR DOCUMENTS AND SHUTDOWN SOON."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("Your computer is now running on UPS backup battery. Save your documents and shutdown soon."), \
            NULL);

#define DUMMY_BATT_HEADER(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("YOU ARE NOW RUNNING ON RESERVE BATTERY POWER."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("You are now running on reserve battery power."), \
            NULL);

#define DUMMY_BATT_BODY(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("PLEASE CONNECT YOUR COMPUTER TO AC POWER. IF YOU DO NOT, YOUR COMPUTER WILL GO TO SLEEP IN A FEW MINUTES TO PRESERVE THE CONTENTS OF MEMORY."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("Please connect your computer to AC power. If you do not, your computer will go to sleep in a few minutes to preserve the contents of memory."), \
            NULL);


__private_extern__ IOReturn 
_setRootDomainProperty(
    CFStringRef                 key, 
    CFTypeRef                   val) 
{
    io_registry_entry_t         root_domain;
    IOReturn                    ret;

    root_domain = IORegistryEntryFromPath( kIOMasterPortDefault, 
                        kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
 
    ret = IORegistryEntrySetCFProperty(root_domain, key, val);

    IOObjectRelease(root_domain);
    return ret;
}


static void sendNotification(int command)
{
    CFMutableDictionaryRef   dict = NULL;
    int numberOfSeconds = 600;
    
    CFNumberRef secondsValue = CFNumberCreate( NULL, kCFNumberIntType, &numberOfSeconds );
    CFNumberRef commandValue = CFNumberCreate( NULL, kCFNumberIntType, &command );

    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, 
                                    &kCFTypeDictionaryKeyCallBacks, 
                                    &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(dict, CFSTR(kPowerManagerActionKey), commandValue);
    CFDictionarySetValue(dict, CFSTR(kPowerManagerValueKey), secondsValue);

    CFNotificationCenterPostNotificationWithOptions ( CFNotificationCenterGetDistributedCenter(),
                                            CFSTR(kPowerManagerActionNotificationName), 
                                            NULL, dict, 
                                            (kCFNotificationPostToAllSessions | kCFNotificationDeliverImmediately));
    CFRelease(dict);
    CFRelease(secondsValue);
    CFRelease(commandValue);
}


__private_extern__ void _askNicelyThenShutdownSystem(void)
{
    sendNotification( PowerManagerScheduledShutdown );
}

__private_extern__ void _askNicelyThenSleepSystem(void)
{
    sendNotification( PowerManagerScheduledSleep );
}

__private_extern__ void _askNicelyThenRestartSystem(void)
{
    sendNotification( PowerManagerScheduledRestart );
}

// Accessor for internal battery structs
__private_extern__ IOPMBattery **_batteries(void)
{
    return batteries;
}

__private_extern__ bool _batterySupports(
    io_registry_entry_t which, 
    CFStringRef what)
{
    int         i = 0;
    bool        found = false;
    
    for(i=0; i<batCount; i++) 
    {
        if(which != batteries[i]->me) continue;
        if(CFDictionaryGetValue(batteries[i]->properties, what)) 
        {
            found = true;
            break;
        }
    }
    
    return found;
}

static void _unpackBatteryState(IOPMBattery *b, CFDictionaryRef prop)    
{
    CFBooleanRef    boo;
    CFNumberRef     n;
    
    if(!isA_CFDictionary(prop)) return;
    
    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalConnectedKey));
    b->externalConnected = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalChargeCapableKey));
    b->externalChargeCapable = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryInstalledKey));
    b->isPresent = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSIsChargingKey));
    b->isCharging = (kCFBooleanTrue == boo);

    b->failureDetected = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSErrorConditionKey));

    b->chargeStatus = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryChargeStatusKey));

    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCurrentCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->currentCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->maxCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSDesignCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->designCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSTimeRemainingKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->hwAverageTR);
    }
    

    n = CFDictionaryGetValue(prop, CFSTR("InstantAmperage"));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->instantAmperage);
    }    
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSAmperageKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->avgAmperage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxErrKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->maxerr);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCycleCountKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->cycleCount);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSLocationKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->location);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSInvalidWakeSecondsKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->invalidWakeSecs);
    } else {
        b->invalidWakeSecs = kInvalidWakeSecsDefault;
    }

    return;
}

__private_extern__ int  _batteryCount(void)
{
    return batCount;
}

__private_extern__ IOPMBattery *_newBatteryFound(io_registry_entry_t where)
{
    int             new_battery_index = batCount++;
    IOPMBattery     **new_batteries_holder = NULL;
    IOPMBattery     *new_battery = NULL;
    int             i;
    
    new_batteries_holder = (IOPMBattery **)malloc( 
                                batCount * sizeof(IOPMBattery *) );

    if( batteries && (batCount > 1) )
    {
        // Copy existing batteries into new array
        for(i=0; i<new_battery_index; i++)
        {
            new_batteries_holder[i] = batteries[i];
        }

        // Free older, smaller array
        free(batteries); batteries = NULL;
    }

    batteries = new_batteries_holder;

    // Populate new battery in array
    new_battery = calloc(1, sizeof(IOPMBattery));
    batteries[new_battery_index] = new_battery;
    
    new_battery->me = where;

    new_battery->name = CFStringCreateWithFormat(
                            kCFAllocatorDefault, 
                            NULL, 
                            CFSTR("InternalBattery-%d"), 
                            new_battery_index);                            
    new_battery->dynamicStoreKey = SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@/InternalBattery-%d"),
                            kSCDynamicStoreDomainState, 
                            CFSTR(kIOPSDynamicStorePath), 
                            new_battery_index);

    _batteryChanged(new_battery);

    return new_battery;
}


__private_extern__ void _batteryChanged(IOPMBattery *changed_battery)
{
    kern_return_t       kr;
    
    if(0 == batCount) return;
    
    if(!changed_battery) { 
        // This is unexpected; we're not tracking this battery
        return;
    }
    
    // Free the last set of properties
    if(changed_battery->properties) { 
        CFRelease(changed_battery->properties);
        changed_battery->properties = NULL;
    }
    
    kr = IORegistryEntryCreateCFProperties(
                            changed_battery->me, 
                            &(changed_battery->properties),
                            kCFAllocatorDefault, 0);
    if(KERN_SUCCESS != kr) {
        changed_battery->properties = NULL;
        goto exit;
    }

    _unpackBatteryState(changed_battery, changed_battery->properties);    
exit:
    return;
}

__private_extern__ bool _batteryHas(IOPMBattery *b, CFStringRef property)
{
    if(!property || !b->properties) return false;
    
    // If the battery's descriptior dictionary has an entry at all for the
    // given 'property' it is supported, i.e. the battery 'has' it.
    return CFDictionaryGetValue(b->properties, property) ? true : false;
}


// Returns 10.0 - 10.4 style IOPMCopyBatteryInfo dictionary, when possible.
__private_extern__ CFArrayRef _copyLegacyBatteryInfo(void) 
{
    CFArrayRef          battery_info = NULL;
    IOReturn            ret;
    
    // PMCopyBatteryInfo
    ret = IOPMCopyBatteryInfo(MACH_PORT_NULL, &battery_info);
    if(ret != kIOReturnSuccess || !battery_info)
    {
        return NULL;
    }
    
    return battery_info;
}

#if HAVE_CF_USER_NOTIFICATION

__private_extern__ CFUserNotificationRef _showUPSWarning(void)
{
#ifndef STANDALONE
    CFMutableDictionaryRef      alert_dict;
    SInt32                      error;
    CFUserNotificationRef       note_ref;
    CFBundleRef                 myBundle;
    CFStringRef                 header_unlocalized;
    CFStringRef                 message_unlocalized;
    CFURLRef                    bundle_url;

    myBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.SystemConfiguration.PowerManagement"));

    // Create alert dictionary
    alert_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(!alert_dict) return NULL;

    bundle_url = CFBundleCopyBundleURL(myBundle);
    CFDictionarySetValue(alert_dict, kCFUserNotificationLocalizationURLKey, bundle_url);
    CFRelease(bundle_url);

    header_unlocalized = CFSTR("WARNING!");
    message_unlocalized = CFSTR("YOUR COMPUTER IS NOW RUNNING ON UPS BACKUP BATTERY. SAVE YOUR DOCUMENTS AND SHUTDOWN SOON.");

    CFDictionaryAddValue(alert_dict, kCFUserNotificationAlertHeaderKey, header_unlocalized);
    CFDictionaryAddValue(alert_dict, kCFUserNotificationAlertMessageKey, message_unlocalized);
    
    note_ref = CFUserNotificationCreate(kCFAllocatorDefault, 0, 0, &error, alert_dict);
    CFRelease(alert_dict);
    if(0 != error)
    {
        syslog(LOG_INFO, "pmcfgd: UPS warning error = %d\n", error);
        return NULL;    
    }

    return note_ref;
#else 
    return NULL;
#endif
}

#endif

/************************* One off hack for AppleSMC
 *************************
 ************************* Send AppleSMC a kCFPropertyTrue
 ************************* on time discontinuities.
 *************************/

static CFMachPortRef        calChangeReceivePort = NULL;

static void setSMCProperty(void)
{
    static io_registry_entry_t     _smc = MACH_PORT_NULL;
    IOReturn                ret;

    if(MACH_PORT_NULL == _smc) {
        _smc = IOServiceGetMatchingService( MACH_PORT_NULL,
                        IOServiceMatching("AppleSMCFamily"));
    }
    
    if(!_smc) {
        return;
    }
    
    // And simply AppleSMC with kCFBooleanTrue to let them know time is changed.
    // We don't pass any information down.
    ret = IORegistryEntrySetCFProperty( _smc, 
                        CFSTR("TheTimesAreAChangin"), 
                        kCFBooleanTrue);
}

static void handleMachCalendarMessage(CFMachPortRef port, void *msg, 
                                            CFIndex size, void *info)
{
	kern_return_t  result;
    mach_port_t    mport = CFMachPortGetPort(port); 
	
	// Re-register for notification
	result = host_request_notification(mach_host_self(), HOST_NOTIFY_CALENDAR_CHANGE, mport);
	if (result != KERN_SUCCESS) {
        // Pretty fatal error. Oh well.
        return;
	}

    setSMCProperty();
}


static void registerForCalendarChangedNotification(void)
{
	mach_port_t tport;
	kern_return_t result;
	CFRunLoopSourceRef rls;

	// allocate the mach port we'll be listening to
	result = mach_port_allocate(mach_task_self(),MACH_PORT_RIGHT_RECEIVE, &tport);
	if (result != KERN_SUCCESS) {
        return;
    }

    calChangeReceivePort = CFMachPortCreateWithPort(
            kCFAllocatorDefault,
            tport,
            (CFMachPortCallBack)handleMachCalendarMessage,
            NULL, /* context */
            false); /* shouldFreeInfo */

    rls = CFMachPortCreateRunLoopSource(
            kCFAllocatorDefault, 
            calChangeReceivePort,
            0); /* index Order */

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

	// register for notification
	result = host_request_notification(mach_host_self(),HOST_NOTIFY_CALENDAR_CHANGE, tport);
}


__private_extern__ int
callerIsRoot(
    int uid,
    int gid
)
{
    return (0 == uid);
}

__private_extern__ int
callerIsAdmin(
    int uid,
    int gid
)
{
    int         ngroups = NGROUPS_MAX+1;
    int         group_list[NGROUPS_MAX+1];
    int         i;
    struct group    *adminGroup;
    struct passwd   *pw;
        
    
    pw = getpwuid(uid);
    if(!pw) return false;
    
    getgrouplist(pw->pw_name, pw->pw_gid, group_list, &ngroups);

    adminGroup = getgrnam("admin");
    if (adminGroup != NULL) {
        gid_t    adminGid = adminGroup->gr_gid;
        for(i=0; i<ngroups; i++)
        {
            if (group_list[i] == adminGid) {
                return TRUE;    // if a member of group "admin"
            }
        }
    }
    return false;
}

__private_extern__ int
callerIsConsole(
    int uid,
    int gid)
{
    CFStringRef                 user_name = NULL;
    uid_t                       console_uid;
    gid_t                       console_gid;
    
    user_name = (CFStringRef)SCDynamicStoreCopyConsoleUser(NULL, 
                                    &console_uid, &console_gid);

    if(user_name) {
        CFRelease(user_name);
        return ((uid == console_uid) && (gid == console_gid));
    } else {
        // no data returned re: console user's uid or gid; return "false"
        return false;
    }
    
}


void _oneOffHacksSetup(void) 
{
    registerForCalendarChangedNotification();
}


#if !TARGET_OS_EMBEDDED

/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/

// Code to read AppleSMC

/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/

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

// Forwards
__private_extern__ IOReturn getSMCKey(
    uint32_t key,
    uint8_t *outBuf,
    uint8_t outBufMax);
static IOReturn callSMCFunction(
    int which, 
    SMCParamStruct *inputValues, 
    SMCParamStruct *outputValues) ;


// Methods
__private_extern__ IOReturn getSMCKey(
    uint32_t key,
    uint8_t *outBuf,
    uint8_t outBufMax)
{
    SMCParamStruct  stuffMeIn;
    SMCParamStruct  stuffMeOut;
    IOReturn        ret;
    int             i;

    if (key == 0 || outBuf == NULL) 
        return kIOReturnCannotWire;

    bzero(outBuf, outBufMax);
    bzero(&stuffMeIn, sizeof(SMCParamStruct));
    bzero(&stuffMeOut, sizeof(SMCParamStruct));

    stuffMeIn.data8 = kSMCGetKeyInfo;
    stuffMeIn.key = key;

    ret = callSMCFunction(kSMCHandleYPCEvent, &stuffMeIn, &stuffMeOut);

    if (stuffMeOut.result != kSMCSuccess)
    {
        ret = kIOReturnInternalError;
        goto exit;
    }

    stuffMeIn.data8 = kSMCReadKey;
    stuffMeIn.key = key;
    stuffMeIn.keyInfo.dataSize = stuffMeOut.keyInfo.dataSize;

    bzero(&stuffMeOut, sizeof(SMCParamStruct));

    ret = callSMCFunction(kSMCHandleYPCEvent, &stuffMeIn, &stuffMeOut);

    if (stuffMeOut.result != kSMCSuccess)
    {
        ret = kIOReturnInternalError;
        goto exit;
    }

    if (outBufMax == 1) {
        *outBuf = stuffMeOut.data8;
    } else if (outBufMax == 4) {
        *outBuf = stuffMeOut.data32;
    } else {
        if (outBufMax > stuffMeIn.keyInfo.dataSize)
            outBufMax = stuffMeIn.keyInfo.dataSize;
        for (i=0; i<outBufMax; i++)
        {
            outBuf[i] = stuffMeOut.bytes[i];
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

    IOByteCount    inStructSize = sizeof(SMCParamStruct);
    IOByteCount    outStructSize = sizeof(SMCParamStruct);
    
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
        IO_OBJECT_NULL == _SMCConnect) 
    {
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

#endif /* TARGET_OS_EMBEDDED */



__private_extern__ IOReturn getSystemManagementKeyInt32(
    uint32_t key, 
    uint32_t *val)
{
#if !TARGET_OS_EMBEDDED    
    return getSMCKey(key, (void *)val, 4);
#else
    return kIOReturnNotReadable;
#endif
}