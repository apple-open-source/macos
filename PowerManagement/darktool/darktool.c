/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
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

/*
 
 cc -o darktool darktool.c -framework IOKit -framework CoreFoundation
 
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#include <IOKit/IOReturn.h>
#include <dispatch/dispatch.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#define PMTestLog(x...)  do {print_pretty_date(false); printf(x);} while(0);
#define PMTestPass  printf
#define PMTestFail  printf

/*************************************************************************/
static void usage(void);
static struct option *long_opts_from_darktool_opts(void);
static bool parse_it_all(int argc, char *argv[]);
static void print_the_plan(void);
static void print_pretty_date(bool newline);
static void print_everything_dark(void);

static void bringTheHeat(void);
static void executeTimedActions(const char *why);
static void createAssertion(CFStringRef type, long timeout);
static void makeTheCall(const char *callname);

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

/*************************************************************************/



/*************************************************************************/

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

static DTAssertionOption   *assertionTypes = NULL;
static int                 assertionsArgCount = 0;

static DTAssertionOption *createAssertionOptions(int *);

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
    kPrintDarkWakeResidencyTimeIndex,
    kCreateAssertionIndex,
    kCallIndex,
    kBringTheHeatIndex,
    kTCPKeepaliveOverride,
    kActionsCount
} DarkToolActions;

typedef enum {
    kIOPMConnectRequestBackgroundIndex,
    kIOPMConnectRequestSleepServiceIndex,
    kIOPMConnectRequestMaintenanceIndex,
    kRequestWakeCount
} DarkToolRequests;

typedef enum {
    kDoItNow                = (1<<0),
    kDoItUponDarkWake       = (1<<1),
    kDoItUponUserWake       = (1<<2)
} DoItWhenOptions;

struct args_struct {
    /* "Sec" suffix indicates value is seconds */
    long    dwIntervalSec;
    long    sleepIntervalSec;
    long    assertionTimeoutSec;
    long    sleepServiceCapTimeoutSec;
    int     doItWhen;
    int     sleepNow;
    
    /* If takeAssertionNamed != NULL; that implies our action is to take an assertion */
    CFStringRef         takeAssertionNamed;
    
    /* If callIOKit != NULL; that implies our action is to mame an SPI/API call */
    char                callIOKit[50];
    
    /* "do" prefix indicates 0/1 are the only acceptable values */
    int doAction[kActionsCount];
    int doRequestWake[kRequestWakeCount];
};
typedef struct args_struct args_struct;
static args_struct args;

static const long _default_dwintervalSec            = 1800;
static const long _default_sleepIntervalSec         = 1800;
static const long _default_assertionTimeoutSec      = 0;

/*
 * Options - caller may specify 0 or more
 */
#define kOptionDWInterval                               "darkwakeinterval"
#define kOptionSleepInterval                            "sleepinterval"
#define kOptionAssertionTimeout                         "assertiontimeout"
#define kOptionDoItNow                                  "now"
#define kOptionUponDarkWake                             "enterdarkwake"
#define kOptionUponUserWake                             "enteruserwake"
#define kOptionSleepServiceCapTimeout                   "sleepservicecap"
#define kOptionSleepNow                                 "sleepnow"

/*
 * Actions - caller may specify only one
 */
#define kActionGet                                      "get"
#define kActionCreateAssertion                          "createassertion"
#define kActionCall                                     "call"
//#define kActionTakePushAssertionUponDarkWake            "takepushupondarkwake"
//#define kActionDeclareUserActivityUponDarkWake          "declareuseractivityupondarkwake"
#define kActionPrintDarkWakeResidency                   "printdarkwakeresidencytime"
#define kActionExitIfNotPushWake                        "exitifnextwakeisnotpush"
#define kActionBringTheHeat                             "cpuheater"
#define kActionRequestBackgroundWake                    "backgroundwake"
#define kActionRequestSleepServiceWake                  "sleepservicewake"
#define kActionRequestMaintenanceWake                   "maintenancewake"
#define kActionSetTCPKeepAliveExpirationTimeout         "tcpkeepaliveexpiration"
#define kActionSetTCPWakeQuotaInterval                  "tcpwakequotainterval"
#define kActionSetTCPWakeQuota                          "tcpwakequota"

typedef enum {kNilType, kActionType, kOptionType} DTOptionType;

struct DTOption {
    struct option   getopt_long;
    DTOptionType    type;
    const char      *desc;
    const char      *required_options[10];
    const char      *optional_options[10];

};
typedef struct DTOption DTOption;
static DTOption darktool_options[] =
{
/* Actions
 */
    { {kActionExitIfNotPushWake,
        no_argument, &args.doAction[kExitIfNextWakeIsNotPushIndex], 1}, kActionType,
        "Waits, and exits if the next wake returns false for IOPMAllowsPushService()",
        { NULL }, { NULL }},
    
    { {kActionPrintDarkWakeResidency,
        no_argument, &args.doAction[kPrintDarkWakeResidencyTimeIndex], 1}, kActionType,
        "Waits, and prints the time spent in DarkWake upon exiting DarkWake. Does not request wakeups or take assertions.",
        { NULL }, { NULL }},
    
    { {kActionCreateAssertion,
        required_argument, &args.doAction[kCreateAssertionIndex], 1}, kActionType,
        "Creates an IOKit assertion of the specified type.",
        { kOptionDoItNow, kOptionUponDarkWake, kOptionUponUserWake, NULL },
        { kOptionAssertionTimeout, NULL } },
    
    { {kActionCall,
        required_argument,&args.doAction[kCallIndex], 1}, kActionType,
        "Calls the specified IOKit API/SPI.",
        { kOptionDoItNow, kOptionUponDarkWake, kOptionUponUserWake, NULL },
        { kOptionAssertionTimeout, NULL } },
    
    { {kActionBringTheHeat,
        no_argument, &args.doAction[kBringTheHeatIndex], 1}, kActionType,
        "Immediately launches several threads to saturate the CPU's and generate heat.",
        { NULL }, { NULL }},

    { {kActionSetTCPKeepAliveExpirationTimeout,
        required_argument, NULL, 0}, kActionType,
        "Override the system default TCPKeepAliveExpiration override (in seconds). Please specify 1 second for fastest timeout (not 0). This setting does not persist across reboots.",
        { NULL }, { NULL }},

    { {kActionSetTCPWakeQuotaInterval,
        required_argument, NULL, 0}, kActionType,
        "Override the system default TCPKeepAlive wake quota interval (in seconds). Passing 0 will disable wake quota. This setting does not persist across reboots.",
        { NULL }, { NULL }},
    
    { {kActionSetTCPWakeQuota,
        required_argument, NULL, 0}, kActionType,
        "Override the system default TCPKeepAlive wake quota count (in number of non-user wakes to allow per WakeQuotaInterval). Passing 0 will disable wake quota. This setting does not persist across reboots.",
        { NULL }, { NULL }},
    
    { {kActionGet,
        no_argument, NULL, 'g'}, kActionType,
        "Print all darktool system settings.",
        { NULL }, { NULL }},

/* Options
 */
/*
    { {kActionRequestBackgroundWake,
        required_argument, &args.doRequestWake[kIOPMConnectRequestBackgroundIndex], 1}, kOptionType,
        "Use IOPMConnection API to emulate UserEventAgent",
        { NULL }, { kOptionSleepInterval}},
*/    
    { {kActionRequestSleepServiceWake,
        required_argument, &args.doRequestWake[kIOPMConnectRequestSleepServiceIndex], 1}, kOptionType,
        "Use IOPMConnection API to emulate sleepservicesd",
        { NULL }, { kOptionSleepInterval, kOptionSleepServiceCapTimeout}},
    
    { {kActionRequestMaintenanceWake,
        required_argument, &args.doRequestWake[kIOPMConnectRequestMaintenanceIndex], 1}, kOptionType,
        "User IOPMConnection API to emulate mDNSResponder and SU Do It Later",
        { NULL }, { kOptionSleepInterval}},

    
/*    { {kOptionDWInterval,
        required_argument, NULL, 0}, kOptionType,
        "Specifies how long to hold assertions in DarkWake. Takes an integer seconds argument.",
        { NULL }, { NULL } },
  */  
    { {kOptionSleepInterval,
        required_argument, NULL, 0}, kOptionType,
        "Specifies how long to stay asleep before entering a scheduled DarkWake. Takes an integer seconds argument.",
        { NULL }, { NULL } },
    
    { {kOptionAssertionTimeout,
        required_argument, NULL, 0}, kOptionType,
        "Specifies the how long hold an IOKit powerassertion. Takes an integer seconds argument.",
        { NULL }, { NULL } },
    
    { {kOptionDoItNow,
        no_argument, &args.doItWhen, kDoItNow}, kOptionType,
        "Specifies to Act immediately.",
        { NULL }, { NULL } },
    
    { {kOptionUponDarkWake,
        no_argument, &args.doItWhen, kDoItUponDarkWake}, kOptionType,
        "Specifies to Act when the system transitions into a DarkWake.",
        { NULL }, { NULL } },

    { {kOptionUponUserWake,
        no_argument, &args.doItWhen, kDoItUponUserWake}, kOptionType,
        "Specifies to Act when the system transitions into a Full.",
        { NULL }, { NULL } },

    { {kOptionSleepServiceCapTimeout,
        required_argument, NULL, 0}, kOptionType,
        "Specifies a SleepService Cap Timeout (for use with --requestiopmconnectionsleepservicewake).",
        { NULL }, { NULL } },
    
    { {kOptionSleepNow,
        no_argument, &args.sleepNow, 1}, kOptionType,
        "Puts the system to sleep immediately, after setting up all other actions & options.",
        { NULL }, { NULL } },    
    
    { {NULL, 0, NULL, 0}, kNilType, NULL, { NULL }, { NULL } }
};

/*************************************************************************/

int main(int argc, char *argv[])
{
    if (!parse_it_all(argc, argv)) {
        usage();
    }
    
    print_the_plan();
    
    
    if (args.doRequestWake[kIOPMConnectRequestBackgroundIndex]
        || args.doRequestWake[kIOPMConnectRequestSleepServiceIndex]
        || args.doRequestWake[kIOPMConnectRequestMaintenanceIndex]
        || args.doAction[kExitIfNextWakeIsNotPushIndex]
        || args.doAction[kPrintDarkWakeResidencyTimeIndex])
    {
        createPMConnectionListener();
    }
    
    if (args.doAction[kBringTheHeatIndex]) {
        bringTheHeat();
    }
        
    if (args.doAction[kCreateAssertionIndex]) {
        if (!args.doItWhen) {
            printf("You must specify a time option for your assertion. See usage(); pass \"--now\" or \"--upondarkwake\"\n");
            exit(1);
        }
    }
    
    if (args.doAction[kPrintDarkWakeResidencyTimeIndex]) {
        printf("Error - print dark wake residency is not implemetned.\n");
        return 1;
    }

    if (args.doItWhen & kDoItNow) {
        executeTimedActions("Now");
    }

    if (args.sleepNow) {
        double delayInSeconds = 5.0;
        printf("Will force sleep the system with IOPMSleepSystem in %d seconds.\n", (int)delayInSeconds);
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            io_connect_t connect = IOPMFindPowerManagement(kIOMasterPortDefault);
            IOReturn ret = IOPMSleepSystem(connect);
            if (kIOReturnSuccess != ret) {
                printf("Error: Couldn't put the system to sleep. IOPMSleepSystem() returns error 0x%08x\n", ret);
            }
        });
    }
    
    CFRunLoopRun();    
    return 0;
}

/*************************************************************************/

static void usage(void)
{
    printf("Usage: darktool [action] [options]\ndarktool is an OS X Core OS test tool to exercise DarkWake, Power Nap, and related API & SPI.\n");
    printf("[v0.1 BETA] This version of darktool was built on %s %s\n\n", __DATE__, __TIME__);
    printf("Options: Options affect the behavior of actions, below. You may specify zero or more options. \n");
    int optcount = sizeof(darktool_options) / sizeof(DTOption);
    for(int i=0; i<optcount; i++)
    {
        if ((NULL != darktool_options[i].getopt_long.name)
            && (kOptionType == darktool_options[i].type))
        {
            printf(" --%s\n   %s\n",
                   darktool_options[i].getopt_long.name,
                   darktool_options[i].desc);            

            if (i != optcount) {
                printf("\n");
            }
        }
    }

    printf("\nActions: Actions tell darktool what to do. You must specify exactly one action. \n");
    for(int i=0; i<optcount; i++)
    {
        if ((NULL != darktool_options[i].getopt_long.name)
            && (kActionType == darktool_options[i].type))
        {
            printf(" --%s\n   %s\n",
                   darktool_options[i].getopt_long.name,
                   darktool_options[i].desc);
            
            /* Print required options */
            if (darktool_options[i].required_options[0])
            {
                int oind = 0;
                const char *printopt = NULL;
                printf("   You must specify one of: ");
                while ((printopt = darktool_options[i].required_options[oind])) {
                    printf("--%s ", printopt);
                    oind++;
                }
                printf("\n");
            }
            
            /* Print required options */
            if (darktool_options[i].optional_options[0])
            {
                int oind = 0;
                const char *printopt = NULL;
                printf("   You may specify: ");
                while ((printopt = darktool_options[i].optional_options[oind])) {
                    printf("--%s ", printopt);
                    oind++;
                }
                printf("\n");
            }
            if (i != optcount) {
                printf("\n");
            }
            
            if (!strcmp(darktool_options[i].getopt_long.name, kActionCreateAssertion)) {
                for (int a_index = 0; a_index<assertionsArgCount; a_index++) {
                    printf("%25s for type %s\n", assertionTypes[a_index].arg, assertionTypes[a_index].desc);
                }
                printf("\n");
            }
            if (!strcmp(darktool_options[i].getopt_long.name, kActionCall)) {
                int c_index = 0;
                while (calls[c_index].arg) {
                    printf("%25s for call %s\n", calls[c_index].arg, calls[c_index].desc);
                    c_index++;
                }
                printf("\n");
            }
        }
    }
}

static bool parse_it_all(int argc, char *argv[]) {
    int                 optind;
    char                ch = 0;
    struct option       *long_opts = long_opts_from_darktool_opts();
    long                temp_arg = 0;
    
    bzero(&args, sizeof(args));
    
    assertionTypes = createAssertionOptions(&assertionsArgCount);

    args.dwIntervalSec = _default_dwintervalSec;
    args.sleepIntervalSec = _default_sleepIntervalSec;
    args.assertionTimeoutSec = _default_assertionTimeoutSec;
    
    do {
        ch = getopt_long(argc, argv, "", long_opts, &optind);
        
        if (-1 == ch)
            break;
        
        if ('?' == ch)
            continue;
        
        if (!strcmp(long_opts[optind].name, kActionGet)) {
            print_everything_dark();
            exit(0);
        }
        
        if (!strcmp(long_opts[optind].name, kOptionDWInterval)) {
            args.dwIntervalSec = strtol(optarg, NULL, 10);
        }
        if (!strcmp(long_opts[optind].name, kActionRequestMaintenanceWake)
            || !strcmp(long_opts[optind].name, kActionRequestBackgroundWake)
            || !strcmp(long_opts[optind].name, kActionRequestSleepServiceWake)) {
            args.sleepIntervalSec = strtol(optarg, NULL, 10);
        }

        if (!strcmp(long_opts[optind].name, kOptionAssertionTimeout)) {
            args.assertionTimeoutSec = strtol(optarg, NULL, 10);
        }
        
        if (!strcmp(long_opts[optind].name, kActionSetTCPKeepAliveExpirationTimeout)) {
            temp_arg = strtol(optarg, NULL, 10);
            IOPMSetValueInt(kIOPMTCPKeepAliveExpirationOverride, (int)temp_arg);
            printf("Updated \"TCPKeepAliveExpiration\" to %lds\n", temp_arg);
            exit(0);
        }
        
        if (!strcmp(long_opts[optind].name, kActionSetTCPWakeQuotaInterval)) {
            temp_arg = strtol(optarg, NULL, 10);
            IOPMSetValueInt(kIOPMTCPWakeQuotaInterval, (int)temp_arg);
            printf("Updated \"TCPWakeQuotaInterval\" to %lds\n", temp_arg);
            exit(0);
        }
        
        if (!strcmp(long_opts[optind].name, kActionSetTCPWakeQuota)) {
            temp_arg = strtol(optarg, NULL, 10);
            IOPMSetValueInt(kIOPMTCPWakeQuota, (int)temp_arg);
            printf("Updated \"TCPWakeQuota\" to %ld\n", temp_arg);
            exit(0);
        }
        
        if (!strcmp(long_opts[optind].name, kOptionSleepServiceCapTimeout)) {
            args.sleepServiceCapTimeoutSec = strtol(optarg, NULL, 10);
        }
        
        if (!strcmp(long_opts[optind].name, kActionCreateAssertion))
        {
            for (int j=0; j<assertionsArgCount; j++)
            {
                if (!strcmp(assertionTypes[j].arg, optarg)) {
                    args.takeAssertionNamed = assertionTypes[j].assertionType;
                    break;
                }
            }
            if (!args.takeAssertionNamed) {
                printf("Unrecognized assertion type %s.\n", optarg);
                usage();
                exit(1);
            }
        }
        
        if (!strcmp(long_opts[optind].name, kActionCall)) {
            int j=0;
            while (calls[j].arg)
            {
                if (!strcmp(calls[j].arg, optarg))
                {
                    strncpy(args.callIOKit, calls[j].arg, sizeof(args.callIOKit));
                }
                j++;
            }
            
            if (!args.callIOKit[0]) {
                printf("Error: Unrecognized call %s.\n", optarg);
                usage();
                exit(1);
            }
        }
        
    } while (1);
    
    return true;
}

static void print_the_plan(void)
{
    int actions_count = 0;
    
    print_pretty_date(true);
    
    for (int i=0; i<kActionsCount; i++) {
        if (args.doAction[i]) {
            actions_count++;
            printf("Action: %s", darktool_options[i].getopt_long.name);            
            if (kCallIndex == i) {
                printf(" %s", args.callIOKit);
            }
            printf("\n");
        }
    }
    
    if (actions_count != 1 ) {
        printf("Error: Please specify one (and only one) action.\n");
        usage();
        exit(1);
    }
    
    
    if (args.doItWhen) {
        printf("When: ");
        if (args.doItWhen & kDoItNow) {
            printf("Now\n");
        }
        else if (args.doItWhen & kDoItUponDarkWake) {
            printf("Upon entering the next DarkWake\n");
        }
        else if (args.doItWhen & kDoItUponUserWake) {
            printf("Upon entering the next UserWake\n");
        }
    }

    if (args.doRequestWake[kIOPMConnectRequestBackgroundIndex])
    {
        printf("Request: BackgroundWake in %ld\n", args.sleepIntervalSec);
    }
    else if (args.doRequestWake[kIOPMConnectRequestMaintenanceIndex])
    {
        printf("Request: MaintenanceWake in %ld\n", args.sleepIntervalSec);
    }
    else if (args.doRequestWake[kIOPMConnectRequestSleepServiceIndex])
    {
        printf("Request: SleepServicesWake in %lds with cap %lds\n", args.sleepIntervalSec, args.assertionTimeoutSec);
    }
    printf("\n");

}

static struct option *long_opts_from_darktool_opts(void)
{
    int i;
    int count = sizeof(darktool_options)/sizeof(DTOption);
    struct option *retopt = NULL;
    
    retopt = calloc(count, sizeof(struct option));
    
    for (i=0; i<count; i++) {
        bcopy(&darktool_options[i].getopt_long, &retopt[i], sizeof(struct option));
    }
    
    return retopt;
}

static DTAssertionOption *createAssertionOptions(int *count)
{
    DTAssertionOption *assertions_heap = NULL;
    
    DTAssertionOption assertions_local[] =
    {
        {kIOPMAssertionTypeApplePushServiceTask,
            kAssertPushService,
            "kIOPMAssertionTypeApplePushServiceTask"},
        {kIOPMAssertInteractivePushServiceTask,
            kInteractivePushService,
            "kIOPMAssertInteractivePushServiceTask"},
        {kIOPMAssertionTypeBackgroundTask,
            kAssertBackground,
            "kIOPMAssertionTypeBackgroundTask"},
        {kIOPMAssertionUserIsActive,
            kAssertUserIsActive,
            "kIOPMAssertionUserIsActive"},
        {kIOPMAssertDisplayWake,
            kAssertDisplayWake,
            "kIOPMAssertDisplayWake"},
        {kIOPMAssertInternalPreventSleep,
            kAssertInternalPreventSleep,
            "kIOPMAssertInternalPreventSleep"},
        {kIOPMAssertMaintenanceActivity,
            kAssertMaintenanceWake,
            "kIOPMAssertMaintenanceActivity"},
        {kIOPMAssertionTypeSystemIsActive,
            kAssertSystemIsActive,
            "kIOPMAssertionTypeSystemIsActive"}
    };
    
    assertions_heap = calloc(1, sizeof(assertions_local));
    bcopy(assertions_local, assertions_heap, sizeof(assertions_local));

    
    *count = sizeof(assertions_local) / sizeof(DTAssertionOption);
    
    return assertions_heap;
}

static void executeTimedActions(const char *why)
{
    print_pretty_date(false);
    printf("%s ", why);
    
    if (args.takeAssertionNamed)
    {
        printf("Create assertion %s\n", CFStringGetCStringPtr(args.takeAssertionNamed, kCFStringEncodingUTF8));
        createAssertion(args.takeAssertionNamed, args.assertionTimeoutSec);
    }
    else if (args.callIOKit[0]) {
        printf("Call IOKit function %s\n", args.callIOKit);
        makeTheCall(args.callIOKit);
    }
}

/*************************************************************************/

static void createAssertion(CFStringRef type, long timeout)
{
    IOReturn  ret;
    IOPMAssertionID id = kIOPMNullAssertionID;
    CFDictionaryRef     d = NULL;
    CFNumberRef         obj = NULL;
    int                 level;

    ret = IOPMAssertionCreateWithDescription(
            type, CFSTR("com.apple.darkmaintenance"),
            NULL, NULL, NULL,
            (CFTimeInterval)timeout, kIOPMAssertionTimeoutActionRelease,
            &id);

    if (kIOReturnSuccess != ret) {
        PMTestLog("Create \'%s\' assertion returns error=0x%08x\n",
                CFStringGetCStringPtr(type, kCFStringEncodingMacRoman), ret);
        exit(1);
    }
    PMTestLog("Created  \'%s\' assertion with timeout of %d secs. ID:0x%x\n",
            CFStringGetCStringPtr(type, kCFStringEncodingMacRoman), (int)timeout, id);

    IOPMCopyAssertionsStatus(&d);

    if (!d || !(obj = CFDictionaryGetValue(d, type)))
    {
        PMTestFail("Failed to get information about status of assertion type \'%s\'\n",
                CFStringGetCStringPtr(type, kCFStringEncodingMacRoman));
    }
    if (obj) {
        CFNumberGetValue(obj, kCFNumberIntType, &level);
        if (0 != level) {
            PMTestPass("Assertion level is ON for type \'%s\'\n", CFStringGetCStringPtr(type, kCFStringEncodingMacRoman));
        } else {
            PMTestFail("Assertion level is OFF for type \'%s\'\n", CFStringGetCStringPtr(type, kCFStringEncodingMacRoman));
        }
    }
    if (d) CFRelease(d);
    
}
/*************************************************************************/

static void makeTheCall(const char *callname)
{
    IOPMAssertionID     dontcare;
    IOPMSystemState     systemstate;
    IOReturn            ret;
    
    if (!callname) {
        printf("Error: trying to call a (null) IOKit function call\n");
        return;    
    }
    
    if (!strcmp(kCallDeclareNotificationEvent, callname))
    {
        ret = IOPMAssertionDeclareNotificationEvent(CFSTR("darktool"), args.assertionTimeoutSec, &dontcare);
    } else if (!strcmp(kCallDeclareSystemIsActive, callname))
    {
        ret = IOPMAssertionDeclareSystemActivity(CFSTR("darktool-system"), &dontcare, &systemstate);
        if (kIOReturnSuccess == ret) {
            if (kIOPMSystemSleepReverted == systemstate) {
                printf("IOPMAssertionDeclareSystemActivity reverted system sleep.\n");
            } else {
                printf("IOPMAssertionDeclareSystemActivity did not revert system sleep.\n");
            }
        }
    } else if (!strcmp(kCallDeclareUserIsActive, callname)) {
        ret = IOPMAssertionDeclareUserActivity(CFSTR("darktool-user"), kIOPMUserActiveLocal, &dontcare);
    } else {
        printf("Error: No recognized IOKit calls named \"%s\"\n", callname);
        return;
    }
    
    if (kIOReturnSuccess != ret) {
        printf("Fail: IOKit call %s returne 0x%08x\n", callname, ret);
    }

    return;
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

static void createPMConnectionListener(void)
{
    IOReturn            ret;
    IOPMConnection      myConnection;
    
    /*
     * Test PM Connection SleepService arguments
     */
    ret = IOPMConnectionCreate(
                               CFSTR("SleepWakeLogTool"),
                               kIOPMCapabilityDisk | kIOPMCapabilityNetwork
                               | kIOPMCapabilityAudio| kIOPMCapabilityVideo,
                               &myConnection);
    
    if (kIOReturnSuccess != ret) {
        PMTestFail("Error 0x%08x from IOPMConnectionCreate.\n", ret);
        exit(1);
    }
    
    ret = IOPMConnectionSetNotification(myConnection, NULL,
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
    
    print_pretty_date(false);
    printf("Created an IOPMConnection client.\n");
}



/*************************************************************************/


static void myPMConnectionHandler(
                           void *param,
                           IOPMConnection                      connection,
                           IOPMConnectionMessageToken          token,
                           IOPMSystemPowerStateCapabilities    capabilities)
{
    IOReturn                ret                     = kIOReturnSuccess;
    CFDictionaryRef         ackDictionary           = NULL;
    static IOPMSystemPowerStateCapabilities     previousCapabilities = kIOPMCapabilityCPU;
        
    char buf[100];
    int buf_size = sizeof(buf);
    IOPMGetCapabilitiesDescription(buf, buf_size, previousCapabilities);
    printf("Transition from \'%s\'\n", buf);
    IOPMGetCapabilitiesDescription(buf, buf_size, capabilities);
    printf(", to \'%s\'\n", buf);
    
    
    if(IOPMIsADarkWake(capabilities)
       && !IOPMIsADarkWake(previousCapabilities)
       && args.doItWhen & kDoItUponDarkWake)
    {
        executeTimedActions("DarkWake");

        
        if (args.doAction[kExitIfNextWakeIsNotPushIndex])
        {
            if (!IOPMAllowsPushServiceTask(capabilities)) {
                printf("Woke to DarkWake; but Push was not allowed. Exiting with error 1.\n");
                exit(1);
            } else {
                printf("Push bit is set; push is allowed. Exiting with status 0.");
                exit(0);
            }
        }

    }
    else if(IOPMIsAUserWake(capabilities)
            && !IOPMIsAUserWake(previousCapabilities)
            && args.doItWhen & kDoItUponUserWake)
    {
        executeTimedActions("FullWake");
    }
    
    previousCapabilities = capabilities;
    

    /* Acknowledge the IOPMConnection event; possibly with a wakeup */
    if (args.doRequestWake[kIOPMConnectRequestSleepServiceIndex]) {
        ackDictionary = HandleSleepServiceCapabilitiesChanged(capabilities);
        
    } else if (args.doRequestWake[kIOPMConnectRequestMaintenanceIndex]) {
        ackDictionary = HandleMaintenanceCapabilitiesChanged(capabilities);
    }
    else if (args.doRequestWake[kIOPMConnectRequestBackgroundIndex]) {
        ackDictionary = HandleBackgroundTaskCapabilitiesChanged(capabilities);
    }
    
    if (ackDictionary) {
        ret = IOPMConnectionAcknowledgeEventWithOptions(connection, token, ackDictionary);
        CFRelease(ackDictionary);
    } else {
        ret = IOPMConnectionAcknowledgeEvent(connection, token);
    }
    
    if (kIOReturnSuccess != ret) {
        PMTestFail("IOPMConnectionAcknowledgement%s failed with 0x%08x", ackDictionary?"WithOptions ":" ", ret);
    }
    
    return;
}


static CFDictionaryRef HandleBackgroundTaskCapabilitiesChanged(IOPMSystemPowerStateCapabilities cap)
{
    CFMutableDictionaryRef ackDictionary = NULL;
    
    ackDictionary = CFDictionaryCreateMutable(0, 1,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
    if (ackDictionary) {
        return NULL;
    }

    if (IOPMIsASleep(cap))
    {
        PMTestLog("To Sleep: Ack'ing with request for BackgroundTask Wake.\n");
        /* Sleep */
        _CFDictionarySetDate(ackDictionary,
                             kIOPMAckTimerPluginWakeDate,
                             CFAbsoluteTimeGetCurrent() + (CFTimeInterval)args.sleepIntervalSec);
    }
    return ackDictionary;
}

static CFDictionaryRef HandleSleepServiceCapabilitiesChanged(IOPMSystemPowerStateCapabilities cap)
{
    CFMutableDictionaryRef      ackDictionary = NULL;
    
    ackDictionary = CFDictionaryCreateMutable(0, 1,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
    
    if (!ackDictionary)
        return NULL;
    
    if (IOPMAllowsPushServiceTask(cap))
    {
        PMTestLog("DarkWake with Push: Ack'ing with sleepService cap time %ld sec.\n", args.sleepServiceCapTimeoutSec);
     
        _CFDictionarySetLong(ackDictionary,
                             kIOPMAckSleepServiceCapTimeout,
                             args.sleepServiceCapTimeoutSec);
        
    }
    else if (IOPMIsASleep(cap))
    {
         PMTestLog("To Sleep: Ack'ing with request for SleepService in %ld sec.\n", args.sleepIntervalSec);

        _CFDictionarySetDate(ackDictionary,
                             kIOPMAckSleepServiceDate,
                             CFAbsoluteTimeGetCurrent() + (CFTimeInterval)args.sleepIntervalSec);
    }

    return ackDictionary;
}

static CFDictionaryRef HandleMaintenanceCapabilitiesChanged(IOPMSystemPowerStateCapabilities cap)
{
    CFMutableDictionaryRef  ackDictionary = NULL;

    ackDictionary = CFDictionaryCreateMutable(0, 1,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
    if (!ackDictionary)
        return NULL;
    
    if (IOPMIsASleep(cap))
    {
        PMTestLog("To Sleep: Ack'ing with request for maintenance in %ld sec.\n", args.sleepIntervalSec);

        _CFDictionarySetDate(ackDictionary,
                             kIOPMAckWakeDate,
                             CFAbsoluteTimeGetCurrent() + (CFTimeInterval)args.sleepIntervalSec);

        _CFDictionarySetLong(ackDictionary,
                             kIOPMAckSystemCapabilityRequirements,
                             (kIOPMCapabilityDisk | kIOPMCapabilityNetwork));
    }
        
    return ackDictionary;
}

static void print_everything_dark(void)
{
    
    CFTypeRef           b;
    
    IOPlatformCopyFeatureDefault(kIOPlatformTCPKeepAliveDuringSleep, &b);
    printf("  Platform: TCPKeepAliveDuringSleep = %s\n", (kCFBooleanTrue == b) ? "yes":"no");
    IOPlatformCopyFeatureDefault(CFSTR("NotificationWake"), &b);
    printf("  Platform: NotificationWake = %s\n", (kCFBooleanTrue == b) ? "yes":"no");
    IOPlatformCopyFeatureDefault(CFSTR("DNDWhileDisplaySleeps"), &b);
    printf("  Platform: DNDWhileDisplaySleeps = %s\n", (kCFBooleanTrue == b) ? "yes":"no");
    
    
    int tcpKeepAliveActive = IOPMGetValueInt(kIOPMTCPKeepAliveIsActive);
    int tcpKeepAliveExpires = IOPMGetValueInt(kIOPMTCPKeepAliveExpirationOverride);
    int tcpWakeQuotaInterval = IOPMGetValueInt(kIOPMTCPWakeQuotaInterval);
    int tcpWakeQuotaCount = IOPMGetValueInt(kIOPMTCPWakeQuota);
    
    printf("  TCPKeepAlive Active = %d\n", tcpKeepAliveActive);
    printf("  TCPKeepAlive Expiration = %ds\n", tcpKeepAliveExpires);
    printf("  TCPKeepAlive WakeQuotaInterval = %ds\n", tcpWakeQuotaInterval);
    printf("  TCPKeepAlive WakeQuotaCount = %d\n", tcpWakeQuotaCount);
}



#define kHeatersCount       32
static void bringTheHeat(void)
{
    int i;
 
    printf("Generating some CPU heat! %d parallel queues are doing busy work.\n", kHeatersCount);
    
    for (i=0; i<kHeatersCount; i++)
    {
        dispatch_queue_t    q;
        q = dispatch_queue_create("heater queue", DISPATCH_QUEUE_SERIAL);
        dispatch_async(q, ^{ while(1); });
    }
    dispatch_main();
}
        
void _CFDictionarySetLong(CFMutableDictionaryRef d, CFStringRef k, long val)
{
    CFNumberRef     num = NULL;
    
    
    num = CFNumberCreate(0, kCFNumberSInt32Type, (const void *)&val);
    if (num)
    {
        CFDictionarySetValue(d, k, num);
        CFRelease(num);
    }
}


void _CFDictionarySetDate(CFMutableDictionaryRef d, CFStringRef k, CFAbsoluteTime atime)
{
    CFDateRef     thedate = NULL;

    thedate = CFDateCreate(0, atime);
    if (thedate) {
        CFDictionarySetValue(d, k, thedate);
        CFRelease(thedate);
    }
}
        




