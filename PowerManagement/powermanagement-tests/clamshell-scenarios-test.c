#include <darwintest.h>
#include <darwintest_utils.h>
#include <unistd.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct {
    uint32_t ac;
    uint32_t desktop_mode;
    uint32_t lid_assertions;
    int result;
} conditions;

conditions default_cases_closed[] = 
{
    {1, 0, 0, 1},
    {0, 0, 0, 1},
    {1, 1, 0, 0},
    {1, 0, 1, 0},
    {0, 1, 0, 1},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {1, 1, 1, 0},
    
};

conditions current = {1, 0, 0, 1};
dispatch_queue_t sleepwake_q;
static io_connect_t gPort = MACH_PORT_NULL;
dispatch_source_t sleep_t;
IOPMAssertionID gLidAssertionID = 0;
IOPMAssertionID gHotPlugAssertionID = 0;

void waitForSleep() {
    // wait for SystemWillSleep
    // schedule a relative wake
    T_LOG("waiting for sleep");
    if (!sleep_t)
    {
        sleep_t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, sleepwake_q);
    }
    dispatch_source_set_event_handler(sleep_t,  ^{
        // if timer fires that means we did not recieve a will sleep.
        T_LOG("Did not receive a will sleep 30 seconds after closing clamshell");
        current.result = 0;
    });
    dispatch_source_set_cancel_handler(sleep_t, ^{
        T_LOG("cancelling sleep_t. Must have received willsleep");
        if (sleep_t) {
            dispatch_release(sleep_t);
            sleep_t = 0;
        }
    });
    dispatch_source_set_timer(sleep_t , dispatch_walltime(DISPATCH_TIME_NOW, 30*NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
    dispatch_resume(sleep_t);
    
    sleep(45);
}

void waitForNoSleep() {
    // wait for NoSleep
    sleep(30);
    if (current.result == -1) {
        current.result = 0;
    } else {
        T_LOG("Oops! we slept when we should not have");
    }
}

static io_registry_entry_t copyRootDomainRef(void)
{
    return (io_registry_entry_t)IOServiceGetMatchingService(
                                                            MACH_PORT_NULL, IOServiceNameMatching("IOPMrootDomain"));
}

static IOReturn setRootDomainProperty(CFStringRef key, CFTypeRef val)
{
    io_registry_entry_t         root_domain = copyRootDomainRef();
    IOReturn                    ret;
    
    if(!root_domain) return kIOReturnError;
    
    ret = IORegistryEntrySetCFProperty(root_domain, key, val);
    
    IOObjectRelease(root_domain);
    return ret;
}

static IOReturn takeDesktopModeAssertion()
{
    CFMutableDictionaryRef properties = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(properties, kIOPMAssertionTypeKey, kIOPMAssertionTypePreventSystemSleep);
    CFDictionarySetValue(properties, kIOPMAssertionNameKey, CFSTR("com.apple.clamshell.hotplug"));
    CFDictionarySetValue(properties, CFSTR("ProcessingHotPlug"), kCFBooleanTrue);
    IOReturn err = IOPMAssertionCreateWithProperties(properties, &gHotPlugAssertionID);
    if (err == kIOReturnSuccess) {
        T_LOG("Created hot plug assertion");
    }
    if (properties) {
        CFRelease(properties);
    }
}

static IOReturn releaseDesktopModeAssertion()
{
    IOReturn err = IOPMAssertionRelease(gHotPlugAssertionID);
    if (err != kIOReturnSuccess) {
        T_FAIL("Failed to release hot plug assertion");
    }
}

void attachAC(bool state) {
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
    char *cmd_to_run = "";
    if(state) {
        cmd_to_run = "/usr/local/bin/smcif -w AC-X 0x00 0x00; /usr/local/bin/smcif -w AC-E 1";
    } else {
        cmd_to_run = "/usr/local/bin/smcif -w AC-X 0x03 0x03; /usr/local/bin/smcif -w AC-E 1";
    }
    int s_err = system(cmd_to_run);
    if (s_err) {
        T_FAIL("failed to set ac attach to %u\n", state);
    } else {
        T_LOG("Set ac attach to %u\n", state);
        current.ac = state ? 1: 0;
    }
#else
    IOReturn kr;
    if (state) {
        kr = setRootDomainProperty(CFSTR("IOPMTestACAttach"), kCFBooleanTrue);
    } else {
        kr = setRootDomainProperty(CFSTR("IOPMTestACDetach"), kCFBooleanTrue);
    }
    
    if (kr != kIOReturnSuccess) {
        T_FAIL("Failed to set ac attach state to %u\n", state);
    } else {
        T_LOG("Set ac attach to %u\n", state);
        current.ac = state ? 1: 0;
        
    }
#endif
}

void setDesktopMode(bool state) {
    IOReturn kr;
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
    // take assertion first
    takeDesktopModeAssertion();
    kr = IOPMSetDesktopMode(state);
    releaseDesktopModeAssertion();
#else
    if (state) {
        kr = setRootDomainProperty(CFSTR("IOPMTestDesktopModeSet"), kCFBooleanTrue);
    } else {
        kr = setRootDomainProperty(CFSTR("IOPMTestDesktopModeRemove"), kCFBooleanTrue);
    }
#endif
    if (kr != kIOReturnSuccess) {
        T_FAIL("Failed to set desktop mode to %u\n", state);
    } else {
        T_LOG("Set desktop mode to %u\n", state);
        current.desktop_mode = state?1:0;
    }
}

void setAssertions(bool state) {
    if (state) {
        T_LOG("setting assertions");
        CFMutableDictionaryRef properties = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(properties, kIOPMAssertionTypeKey, kIOPMAssertionUserIsActive);
        CFDictionarySetValue(properties, kIOPMAssertionNameKey, CFSTR("com.apple.clamshell.test"));
        CFDictionarySetValue(properties, kIOPMAssertionAppliesOnLidClose, kCFBooleanTrue);
        IOReturn err = IOPMAssertionCreateWithProperties(properties, &gLidAssertionID);
        if (err == kIOReturnSuccess) {
            current.lid_assertions = 1;
        } else {
            T_FAIL("Failed to create assertion with lid modifier 0x%x", err);
        }
        CFRelease(properties);
    } else {
        if (gLidAssertionID) {
            T_LOG("releasing assertions");
            IOReturn err = IOPMAssertionRelease(gLidAssertionID);
            if (err == kIOReturnSuccess) {
                current.lid_assertions = 0;
            } else {
                T_FAIL("Failed to release assertion");
            }
        }
    }
}

void setClamshell(bool state) {
    IOReturn kr;
    if (state) {
        kr = setRootDomainProperty(CFSTR("IOPMTestClamshellClose"), kCFBooleanTrue);
    } else {
        kr = setRootDomainProperty(CFSTR("IOPMTestClamshellOpen"), kCFBooleanTrue);
    }
    
    if (kr != kIOReturnSuccess) {
        T_FAIL("Failed to set clamshell state to %u\n", state);
    } else {
        T_LOG("Set clamshell state to %u\n", state);
    }
}

void sleepWakeCallback( void *refcon, io_service_t y __unused, natural_t messageType, void *messageArgument) {
    switch (messageType) {
        case kIOMessageSystemWillSleep:
            T_LOG("Will sleep");
            // if there is a timer waiting cancel it
            if (sleep_t) {
                dispatch_source_cancel(sleep_t);
            }
            current.result = 1;
            // schedule a relative wake
            char *cmd_to_run = "/usr/bin/pmset relative wake 1";
            int s_err = system(cmd_to_run);
            if (s_err) {
                T_FAIL("failed to schedule a relative wake");
            }
            IOAllowPowerChange(gPort , messageArgument);
            break;
        case kIOMessageSystemWillPowerOn:
            T_LOG("Will power on");
            break;
        case kIOMessageSystemHasPoweredOn:
            T_LOG("Has powered on");
            break;
    }
}

void registerForSleepWakeNotifications() {
    sleepwake_q = dispatch_queue_create("sleep-wake-notification", DISPATCH_QUEUE_SERIAL);
    io_object_t  notifier = MACH_PORT_NULL;
    IONotificationPortRef notify = NULL;
    
    gPort = IORegisterForSystemPower( NULL, &notify, sleepWakeCallback, &notifier);
    if (notify && (MACH_PORT_NULL != gPort)) {
        IONotificationPortSetDispatchQueue(notify, sleepwake_q);
    }
}

void setupSleepSettings() {
    // disable idle sleep
    char *cmd_to_run = "/usr/bin/pmset sleep 0";
    int s_err = system(cmd_to_run);
    if (s_err) {
        T_FAIL("failed to disable idle sleep");
    } else {
        T_LOG("disabled idle sleep");
    }
    cmd_to_run = "/usr/bin/pmset dwlinterval 0";
    s_err = system(cmd_to_run);
    if (s_err) {
        T_FAIL("failed to set dwlinterval to 0");
    } else {
        T_LOG("set dwlinterval to 0");
    }
}

void restoreDefaults() {
    char *cmd_to_run = "/usr/bin/pmset restoredefaults";
    int s_err = system(cmd_to_run);
    if (s_err) {
        T_FAIL("failed to restore default settings");
    } else {
        T_LOG("restored default settings");
    }
    
    // set clamshell open
    setClamshell(false);
    
    // set ac attach
    attachAC(true);
    
    // set desktopmode 0
    setDesktopMode(false);
    
    // set lid assertions to false
    setAssertions(false);
}

bool clamshellPresent() {

    io_registry_entry_t rootDomain = copyRootDomainRef();
    if (rootDomain) {
        CFBooleanRef present = IORegistryEntryCreateCFProperty(rootDomain, CFSTR(kAppleClamshellStateKey), kCFAllocatorDefault, 0);
        if (present != NULL){
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

T_DECL(clamshell_scenarios, "test various clamshell cases") {
    T_SETUPBEGIN;

    // skip if there is no clamshell
    if (!clamshellPresent()) {
        T_SKIP("Not a clamshell device");
    }
    // register for sleep wake notifications
    registerForSleepWakeNotifications();
    
    // disable assertions and idle sleep
    setupSleepSettings();
    
    T_SETUPEND;
    
    for (int i = 0; i < 8; i++) {
        conditions x = default_cases_closed[i];
        
        
        T_LOG("========Testing condition {ac: %u, desktop mode: %u, lid_assertions: %u } result : %d =================", x.ac, x.desktop_mode, x.lid_assertions, x.result);
        if (x.ac != current.ac) {
            if (x.ac) {
                attachAC(true);
            } else {
                attachAC(false);
            }
            // wait for PS change notifications
            sleep(5);
        } else {
            T_LOG("Ac already in expected condition %u", x.ac);
        }
        if (x.desktop_mode != current.desktop_mode) {
            if (x.desktop_mode) {
                setDesktopMode(true);
            } else {
                setDesktopMode(false);
            }
        } else {
            T_LOG("desktop mode already in the expected condition %u", x.desktop_mode);
        }
        if (x.lid_assertions != current.lid_assertions) {
            if (x.lid_assertions) {
                setAssertions(true);
                T_LOG("wait for assertion to take effect");
                sleep(5);
            } else {
                setAssertions(false);
            }
        } else {
            T_LOG("Lid assertions already in the expected conditions %u", x.lid_assertions);
        }
        current.result = -1;
        
        T_LOG("Closing clamshell");
        T_LOG("Expected result for ac: %u, desktop mode: %u, lid_assertions: %u : sleep %d", current.ac, current.desktop_mode, current.lid_assertions, x.result);
        setClamshell(true);
        if (x.result) {
            waitForSleep();
        } else {
            waitForNoSleep();
        }
        if (current.result != x.result) {
            T_FAIL("current.result %d != expected.result %d", current.result, x.result);
        } else {
            T_PASS("current.result %d == expected.result %d", current.result, x.result);
        }
        T_LOG("==================Done==================");
        
        // open clamshell at end of each iteration
        setClamshell(false);
    }
    
    
    restoreDefaults();
}

