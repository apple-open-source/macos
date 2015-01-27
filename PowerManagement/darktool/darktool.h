//
//  darktool.h
//  PowerManagement
//
//  Created by Ethan Bold on 9/19/13.
//
//

#ifndef PowerManagement_darktool_h
#define PowerManagement_darktool_h

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/platform/IOPlatformSupportPrivate.h>
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

#define PMTestLog(x...)  do {print_pretty_date(false); printf(x);} while(0);
#define PMTestPass  printf
#define PMTestFail  printf


#define kSleepIntervalSec               1800
#define kAssertionTimeoutSec            0

/*
 * IOKit Assertions - command-line arguments for creating IOKit power assertions.
 *
 * callers should pass these strings as an argument to "darktool --createassertion <assertionname>"
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

struct DTAssertionOption {
    CFStringRef         assertionType;
    const char          *arg;
    const char          *desc;
};
typedef struct DTAssertionOption DTAssertionOption;

/*************************************************************************/
static void usage(void);
static struct option *long_opts_from_darktool_opts(int *);
static bool parse_it_all(int argc, char *argv[]);
static void print_the_plan(void);
static void print_pretty_date(bool newline);
static void print_everything_dark(void);

static void bringTheHeat(void);
static void executeTimedActions(const char *why);
static void execute_Assertion(CFStringRef type, long timeout);
static void execute_APICall(const char *callname);
static void exec_exec(char* run_command);
static DTAssertionOption *createAssertionOptions(int *);


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

static void _CFDictionarySetLong(
                                 CFMutableDictionaryRef d,
                                 CFStringRef k,
                                 long val);
static void _CFDictionarySetDate(
                                 CFMutableDictionaryRef d,
                                 CFStringRef k,
                                 CFAbsoluteTime atime);

static void cacheArgvString(int argc, char *argv[]);


/*************************************************************************/
/*
 * IOKit SPI/API calls
 *
 * callers should pass these strings as arguments to "darktool --call <functionname>"
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
//  kPrintDarkWakeResidencyTimeIndex,
    kCreateAssertionIndex,
    kCallIndex,
    kBringTheHeatIndex,
    kTCPKeepaliveOverride,
    kActionSetTCPWakeQuota, // unused
    kActionSeTTCPWakeQuota, // unused
    kDoAckTimeoutIndex,
    kActionExecIndex,
    kActionClaimIndex,
    kActionCreatePowerSourceIndex,
    kActionsCount   // kActionsCount must always be the last item in this list
} DarkToolActions;

typedef enum {
    kIOPMConnectRequestBackgroundIndex,
    kIOPMConnectRequestSleepServiceIndex,
    kIOPMConnectRequestMaintenanceIndex,
    // kRequestWakeCount must always be the last item in this list
    kRequestWakeCount
} DarkToolRequests;

typedef enum {
    kDoItNow                = (1<<0),
    kDoItUponDarkWake       = (1<<1),
    kDoItUponUserWake       = (1<<2),
    kDoItUponACAttach       = (1<<3)
} DoItWhenOptions;




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



#endif
