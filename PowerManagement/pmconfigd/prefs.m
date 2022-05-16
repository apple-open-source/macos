#import <Foundation/NSUserDefaults.h>
#import <Foundation/Foundation.h>
#import "AppleSmartBatteryKeysPrivate.h"
#import "PrivateLib.h"

#undef   LOG_STREAM
#define  LOG_STREAM   prefsLogger

static const CFStringRef kPath = CFSTR("/Library/Managed Preferences/mobile");
static const CFStringRef kDomain = CFSTR("com.apple.powerd.managed");
static os_log_t prefsLogger = NULL;

static void createLogger(void)
{
    if (!prefsLogger) {
        prefsLogger = os_log_create(PM_LOG_SYSTEM, PREFS_LOG);
    }
}

IOReturn initializeGeneralPreferences(void)
{
    CFStringRef managedPrefsDictPath = NULL;
    io_service_t service = MACH_PORT_NULL;
    IOReturn ret = kIOReturnSuccess;
    CFStringRef name = NULL;
    CFBooleanRef param = NULL;
    CFDictionaryRef dict = NULL;

    createLogger();

    INFO_LOG("Looking for MDM prefs\n");
    managedPrefsDictPath = CFStringCreateWithFormat(kCFAllocatorDefault,
                                                    NULL,
                                                    CFSTR("%@/%@.plist"),
                                                    kPath,
                                                    kDomain);
    if (managedPrefsDictPath == NULL) {
        INFO_LOG("No managed preferences found\n");
        return kIOReturnUnsupported;
    }

    CFDictionaryRef prefs = (__bridge CFDictionaryRef) [[NSDictionary alloc] initWithContentsOfFile:(__bridge id)managedPrefsDictPath];
    if (prefs == NULL) {
        ERROR_LOG("Failed to get prefs from path\n");
        CFRelease(managedPrefsDictPath);
        return kIOReturnNoDevice;
    }

    if (CFDictionaryContainsKey(prefs, CFSTR(kAsbCriticalAcOverrideKey))) {
        int val = -1;
        CFDictionaryGetIntValue(prefs, CFSTR(kAsbCriticalAcOverrideKey), val);
        INFO_LOG("CriticalAcOverrideKey found %d in prefs\n", val);
        name = CFStringCreateWithCString(kCFAllocatorDefault, kAsbCriticalAcOverrideKey, kCFStringEncodingUTF8);
        param = val <= 0 ? kCFBooleanFalse : kCFBooleanTrue;
    } else {
        ret = kIOReturnUnsupported;
        goto bail;
    }
 
    dict = CFDictionaryCreate(kCFAllocatorDefault, (const void **) &name, (const void **) &param, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict) {
        ERROR_LOG("failed to create request");
        ret = kIOReturnNoMemory;
        goto bail;
    }

    service = getIOPMPowerSource();
    if (MACH_PORT_NULL == service) {
        ret = kIOReturnNoDevice;
        ERROR_LOG("Failed to get power source handle");
        goto bail;
    }
    
    ret = IORegistryEntrySetCFProperties(service, dict);
    if (ret) {
        ERROR_LOG("Failed to send request to service: 0x%x", ret);
        goto bail;
    }

bail:
    CFRelease(managedPrefsDictPath);
    CFRelease(prefs);
    if (name)
        CFRelease(name);
    if (param)
        CFRelease(param);
    if (dict)
        CFRelease(dict);
    return ret;
}
