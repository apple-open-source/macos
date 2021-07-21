//
//  pmtool.h
//  PowerManagement
//
//  Created by dekom on 8/13/15.
//
//

#ifndef pmtool_h
#define pmtool_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <CoreFoundation/CFPreferences_Private.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/pwr_mgt/powermanagement_mig.h>
#include <IOKit/IOReturn.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <dispatch/dispatch.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <notify.h>
#include <sysexits.h>
#include <xpc/xpc.h>
#include <IOKit/ps/IOPSKeysPrivate.h>
#include <IOKit/ps/IOPSKeys.h>
#include <SystemConfiguration/SCValidation.h>
#if TARGET_OS_OSX
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#endif

#define PMTestLog(x...)  do {print_pretty_date(false); printf(x);} while(0);
#define PMTestPass  printf
#define PMTestFail  printf


#define kSleepIntervalSec               1800
#define kAssertionTimeoutSec            0

/*
 * IOKit Assertions - command-line arguments for creating IOKit power assertions.
 *
 * callers should pass these strings as an argument to "pmtool --createassertion <assertionname>"
 */
#define kAssertPushService                              "applepushservice"
#define kInteractivePushService                         "interactivepush"
#define kAssertBackground                               "backgroundtask"
#define kAssertUserIsActive                             "userisactive"
#define kAssertRemoteUserIsActive                       "remoteuserisactive"
#define kAssertDisplayWake                              "displaywake"
#define kAssertInternalPreventSleep                     "internalpreventsleep"
#define kAssertMaintenanceWake                          "maintenancewake"
#define kAssertSystemIsActive                           "systemisactive"
#define kAssertPreventUserIdleSystemSleep               "preventuseridlesystemsleep"
#define kAssertPreventUserIdleDisplaySleep              "preventuseridledisplaysleep"

struct DTAssertionOption {
    CFStringRef         assertionType;
    const char          *arg;
    const char          *desc;
};
typedef struct DTAssertionOption DTAssertionOption;

#define POWERD_XPC_ID "com.apple.iokit.powerdxpc"

/*************************************************************************/
static void usage(void);
static struct option *long_opts_from_pmtool_opts(int *);
static bool parse_it_all(int argc, char *argv[]);
static void print_pretty_date(bool newline);

static void bringTheHeat(void);
static void executeTimedActions(const char *why);
static void execute_Assertion(CFStringRef type, long timeout);
static void execute_APICall(const char *callname);
static void exec_exec(char* run_command);
static DTAssertionOption *createAssertionOptions(int *);


#if TARGET_OS_OSX
static void print_everything_dark(void);
static void print_the_plan(void);
static void sleepHandler(int sleepType);
static void accelerate_sleep_intervals(void);
static void wakeHandler(void);
static void createPMConnectionListener(void);
static void myPMConnectionHandler(
                                  void *param, IOPMConnection,
                                  IOPMConnectionMessageToken, IOPMSystemPowerStateCapabilities);

static CFDictionaryRef HandleBackgroundTaskCapabilitiesChanged(
                                                               IOPMSystemPowerStateCapabilities cap);
static CFDictionaryRef HandleSleepServiceCapabilitiesChanged(
                                                             IOPMSystemPowerStateCapabilities cap);
static CFDictionaryRef HandleMaintenanceCapabilitiesChanged(
                                                            IOPMSystemPowerStateCapabilities cap);
static void sendInactivityWindowCommand(long start, long duration, long delay);

static void _CFDictionarySetLong(
                                 CFMutableDictionaryRef d,
                                 CFStringRef k,
                                 long val);
static void _CFDictionarySetDate(
                                 CFMutableDictionaryRef d,
                                 CFStringRef k,
                                 CFAbsoluteTime atime);
#endif

static void cacheArgvString(int argc, char *argv[]);

static void sendSmartBatteryCommand(uint32_t which, uint32_t level);
static void sendCustomBatteryProperties(char *path);
static void sendBHUpdateTimeDelta(long timeDelta);
static void sendBHDataFromCFPrefs(void);
static void sendAgingDataFromCFPrefs(void);
#if TARGET_OS_OSX
static void isVactSupported(void);
static void setPermFaultStatus(int64_t pfStatus);
#endif

/*************************************************************************/
/*
 * IOKit SPI/API calls
 *
 * callers should pass these strings as arguments to "pmtool --call <functionname>"
 */

#define kCallDeclareUserIsActive                        kAssertUserIsActive
#define kCallDeclareSystemIsActive                      kAssertSystemIsActive
#define kCallDeclareNotificationEvent                   "declarenotificationevent"

struct DTCallOption {
    const char *arg;
    const char *desc;
};
typedef struct DTCallOption DTCallOption;

DTCallOption calls[] = {
    {kCallDeclareUserIsActive, "IOPMAssertionDeclareUserActivity"},
    {kCallDeclareSystemIsActive, "IOPMAssertionDeclareSystemActivity"},
    {kCallDeclareNotificationEvent, "IOPMAssertionDeclareNotificationEvent"},
    { NULL, NULL}
};

/*************************************************************************/

typedef enum {
    kExitIfNextWakeIsNotPushIndex           = 0,
    kCreateAssertionIndex,
    kCallIndex,
    kBringTheHeatIndex,
    kTCPKeepaliveOverride,
    kDoAckTimeoutIndex,
    kActionExecIndex,
    kActionGetIndex,
    kActionClaimIndex,
    kActionCreatePowerSourceIndex,
    kActionHibernateNowIndex,
    kActionStandbyNowIndex,
    kActionPowerOffNowIndex,
    kActionSetBattIndex,
    kActionResetBattIndex,
    kActionInactivityWindowIndex,
    kOptionInactivityWindowDurationIndex,
    kOptionStandbyAccelerateDelayIndex,
    kSetCustomBatteryPropertiesIndex,
    kResetCustomBatteryPropertiesIndex,
    kSetBHUpdateDeltaIndex,
    kGetBHDataFromPrefsIndex,
    kGetAgingDataFromPrefsIndex,
    kGetVactSupportedIndex,
    kSetPermFaultStatusIndex,
    kActionsCount   // kActionsCount must always be the last item in this list
} pmtoolActions;

typedef enum {
    kIOPMConnectRequestBackgroundIndex,
    kIOPMConnectRequestSleepServiceIndex,
    kIOPMConnectRequestMaintenanceIndex,
    // kRequestWakeCount must always be the last item in this list
    kRequestWakeCount
} pmtoolRequests;

typedef enum {
    kDoItNow                = (1<<0),
    kDoItUponDarkWake       = (1<<1),
    kDoItUponUserWake       = (1<<2),
    kDoItUponACAttach       = (1<<3)
} DoItWhenOptions;

// Sleep types
enum {
    kSleepTypeNormal = 0,
    kSleepTypeHibernate,
    kSleepTypeStandby,
    kSleepTypePowerOff
};

// Battery manager commands
enum {
    kSBInflowDisable        = 0,
    kSBChargeInhibit        = 1,
    kSBSetPollingInterval   = 2,
    kSBSMBusReadWriteWord   = 3,
    kSBRequestPoll          = 4,
    kSBSetOverrideCapacity  = 5,
    kSBSwitchToTrueCapacity = 6
};

/*
 * Options - caller may specify 0 or more
 */
#define kOptionSleepInterval                            "sleepinterval"
#define kOptionAssertionTimeout                         "assertiontimeout"
#define kOptionDoItNow                                  "now"
#define kOptionUponDarkWake                             "enterdarkwake"
#define kOptionUponUserWake                             "enteruserwake"
#define kOptionUponACAttach                             "acattach"
#define kOptionSleepServiceCapTimeout                   "sleepservicecap"
#define kOptionSleepNow                                 "sleepnow"
#define kOptionIterations                               "iterations"
#define kOptionStandardSleepIntervals                   "standardsleepintervals"
#define kOptionInactivityDuration                       "duration"
#define kOptionStandbyAccelerateDelay                   "delay"

/*
 * Actions - caller may specify only one
 */
#define kActionGet                                      "get"
#define kActionCreateAssertion                          "createassertion"
#define kActionCall                                     "call"
#define kActionPrintDarkWakeResidency                   "printdarkwakeresidencytime"
#define kActionExitIfNotPushWake                        "exitifnextwakeisnotpush"
#define kActionBringTheHeat                             "cpuheater"
#define kActionRequestBackgroundWake                    "backgroundwake"
#define kActionRequestSleepServiceWake                  "sleepservicewake"
#define kActionRequestMaintenanceWake                   "maintenancewake"
#define kActionSetTCPKeepAliveExpirationTimeout         "tcpkeepaliveexpiration"
#define kActionDoAckTimeout                             "doacktimeout"
#define kActionExec                                     "exec"
#define kActionClaim                                    "claim"
#define kActionCreatePowerSource                        "createPowerSource"
#define kActionHibernateNow                             "hibernatenow"
#define kActionStandbyNow                               "standbynow"
#define kActionPowerOffNow                              "poweroffnow"
#define kActionSetBatt                                  "setbatt"
#define kActionResetBatt                                "resetbatt"
#define kActionSetUserInactivityStart                   "inactivitystart"
#define kActionSetBattProps                             "setbattprops"
#define kActionResetBattProps                           "resetbattprops"
#define kActionSetBHUpdateDelta                         "bhupdatedelta"
#define kActionGetBHDataFromPrefs                       "getbhdatafromprefs"
#define kActionGetAgingDataFromPrefs                    "getagingdatafromprefs"
#define kActionGetVactSupported                         "isvactsupported"
#define kActionSetPermFaultStatus                       "setpermfaultstatus"

#define kArgIOPMConnection                              "iopmconnection"
#define kArgIORegisterForSystemPower                    "ioregisterforsystempower"

typedef enum {kNilType, kActionType, kOptionType} DTOptionType;

struct DTOption {
    struct option   getopt_long;
    DTOptionType    type;
    const char      *desc;
    const char      *required_options[10];
    const char      *optional_options[10];
    
};
typedef struct DTOption DTOption;

#endif /* pmtool_h */
