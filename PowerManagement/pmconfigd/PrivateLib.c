#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <syslog.h>
#include "PrivateLib.h"

enum
{
    PowerMangerScheduledShutdown = 1,
    PowerMangerScheduledSleep
};

#define kPowerManagerActionNotificationName "com.apple.powermanager.action"
#define kPowerManagerActionKey "action"
#define kPowerManagerValueKey "value"


static void sendNotification(int command)
{
    CFMutableDictionaryRef	dict = NULL;
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
    sendNotification(PowerMangerScheduledShutdown);
}

__private_extern__ void _askNicelyThenSleepSystem(void)
{
    sendNotification(PowerMangerScheduledSleep);
}

__private_extern__ void _doNiceShutdown(void)
{
}


__private_extern__ CFArrayRef _copyBatteryInfo(void) 
{
    static mach_port_t 		master_device_port = 0;
    kern_return_t       	kr;
    int				ret;
    CFArrayRef			battery_info = NULL;
    
    if(!master_device_port) kr = IOMasterPort(bootstrap_port,&master_device_port);
    
    // PMCopyBatteryInfo
    ret = IOPMCopyBatteryInfo(master_device_port, &battery_info);
    if(ret != kIOReturnSuccess || !battery_info)
    {
        return NULL;
    }
    
    return battery_info;
}


