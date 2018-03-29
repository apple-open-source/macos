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

/*************************************************************************/
/*
 
 Common invocations
 
 pmtool --maintenancewake 30 --createassertion maintenancewake --enterdarkwake --assertiontimeout 10 --iterations 10 --sleepnow
 
 pmtool --sleepservicewake 60 \
 --createassertion applepushservice --assertiontimeout 120 --enterdarkwake \
 --iterations 1 \
 --sleepnow
 
 */

#include "pmtool.h"

/*************************************************************************/

static bool gSleepWake = false;

struct args_struct {
    int     standardSleepIntervals; // Defines whether pmtool should accelerate btinternal and dwlinterval.
    long    sleepIntervalSec;
    long    assertionTimeoutSec;
    long    sleepServiceCapTimeoutSec;
    int     doItWhen;
    int     sleepNow;
    bool    ackIORegisterForSystemPower;
    bool    ackIOPMConnection;
    int     doIterations;
    int     iterationsCount;
    int     batteryLevel;
    long    inactivityWindowStart;
    long    inactivityWindowDuration;
    long    standbyAccelerationDelay;
    
    /* If takeAssertionNamed != NULL; that implies our action is to take an assertion */
    CFStringRef         takeAssertionNamed;
    
    /* If callIOKit != NULL; that implies our action is to mame an SPI/API call */
    char                callIOKit[50];
    char                exec[255];
    
    /* "do" prefix indicates 0/1 are the only acceptable values */
    int doAction[kActionsCount];
    int doRequestWake[kRequestWakeCount];
};
typedef struct args_struct args_struct;


typedef struct {
    CFMutableStringRef   invokedStr;
    DTAssertionOption   *assertionTypes;
    int                 assertionsArgCount;
} globals_struct;

static args_struct args;
static globals_struct g;

static DTOption pmtool_options[] =
{
    /* Actions
     */
    { {kActionExitIfNotPushWake,
        no_argument, &args.doAction[kExitIfNextWakeIsNotPushIndex], 1}, kActionType,
        "Waits, and exits if the next wake returns false for IOPMAllowsPushService()",
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
    
    { {kActionDoAckTimeout,
        required_argument, &args.doAction[kDoAckTimeoutIndex], 1}, kActionType,
        "pmtool acts as a negligent API client, and doesn't ack any notifications from IOPMConnection \
        or IORegisterForSytemPower. Caller should specify either 'iopmconnection' or 'ioregisterforsystempower' \
        as an argument.", { NULL }, {NULL}},
    
    { {kActionExec,
        required_argument, &args.doAction[kActionExecIndex], 1}, kActionType,
        "Runs the specified command.",
        { NULL }, { NULL }},
    
    { {kActionGet,
        no_argument, NULL, 'g'}, kActionType,
        "Print all pmtool system settings.",
        { NULL }, { NULL }},
    
    { {kActionClaim,
        no_argument, &args.doAction[kActionClaimIndex], 1}, kActionType,
        "For internal testing - Call IOPMClaimSystemWakeReason.",
        { NULL }, { NULL }},
    
    { {kActionCreatePowerSource,
        no_argument, &args.doAction[kActionCreatePowerSourceIndex], 1}, kActionType,
        "For internal testing - creates a power source object with IOPSCreatePowerSource.",
        { NULL }, { NULL }},
    
    { {kActionHibernateNow,
        no_argument, &args.doAction[kActionHibernateNowIndex], 1}, kActionType,
        "Puts the machine directly into hibernation.",
        { NULL }, { NULL }},
    
    { {kActionStandbyNow,
        no_argument, &args.doAction[kActionStandbyNowIndex], 1}, kActionType,
        "Puts the machine directly into standby.",
        { NULL }, { NULL }},
    
    { {kActionPowerOffNow,
        no_argument, &args.doAction[kActionPowerOffNowIndex], 1}, kActionType,
        "Puts the machine directly into poweroff.",
        { NULL }, { NULL }},
    
    { {kActionSetBatt,
        required_argument, &args.doAction[kActionSetBattIndex], 1}, kActionType,
        "Sets the percent charge for the battery. To reset the battery warning level, apply AC. To reset the percentage, call --resetbatt.",
        { NULL }, { NULL }},
    
    { {kActionResetBatt,
        no_argument, &args.doAction[kActionResetBattIndex], 1}, kActionType,
        "Resets the battery percentage to its true value.",
        { NULL }, { NULL }},
    
    /* Options
     */
    { {kActionRequestSleepServiceWake,
        required_argument, &args.doRequestWake[kIOPMConnectRequestSleepServiceIndex], 1}, kOptionType,
        "Use IOPMConnection API to emulate sleepservicesd",
        { NULL }, { kOptionSleepInterval, kOptionSleepServiceCapTimeout}},
    
    { {kActionRequestMaintenanceWake,
        required_argument, &args.doRequestWake[kIOPMConnectRequestMaintenanceIndex], 1}, kOptionType,
        "User IOPMConnection API to emulate mDNSResponder and SU Do It Later",
        { NULL }, { kOptionSleepInterval}},
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
        "Specifies to Act (e.g. create the assertion or make the IOKit call) when the system transitions into a DarkWake.",
        { NULL }, { NULL } },
    
    { {kOptionUponUserWake,
        no_argument, &args.doItWhen, kDoItUponUserWake}, kOptionType,
        "Specifies to Act (e.g. create the assertion or make the IOKit call) when the system transitions into a Full.",
        { NULL }, { NULL } },
    
    { {kOptionUponACAttach,
        no_argument, &args.doItWhen, kDoItUponACAttach}, kOptionType,
        "Specifies to Act (create assertion or make IOKit call) upon AC attach.",
        { NULL }, { NULL } },
    
    { {kOptionSleepNow,
        no_argument, &args.sleepNow, 1}, kOptionType,
        "Puts the system to sleep immediately, after setting up all other actions & options.",
        { NULL }, { NULL } },
    
    { {kOptionIterations,
        required_argument, &args.doIterations, 1}, kOptionType,
        "Repeats the wake/assertion/back to sleep for the specified <interval> wakes.",
        { NULL }, { NULL } },
    
    { {kOptionStandardSleepIntervals,
        no_argument, &args.standardSleepIntervals, 1}, kOptionType,
        "When set, pmtool will not accelerate DarkWake linger interval, or BackgroundTask interval. Usually testers want sleep & DarkWake cycles to occur as quickly as possible, without standard user delays; so if this setting isn't specified pmtool will call \'pmset btinterval 0\' and \'pmset dwlinterval 0\'.",
        { NULL }, { NULL } },

    // Adaptive Standby settings
    { {kActionSetUserInactivityStart,
          required_argument, &args.doAction[kActionInactivityWindowIndex], 1}, kActionType,
        "Sets the start of user inactivity window to be used to accelerate entry to standby. The value(in seconds) indicates the start of inactivity window from now.\n   Providing negative value resets the inactivity window to system predicated window.\n\
            Notes on setting the inactivity window:\n\
            - The remaining window when system enters sleep should be greater than standby acceleration delay\n\
            - The remaining window when system enters sleep should be greater than standby delay.\n\
            - The standbydelay should be at least twice the standby acceleration delay\n",
        { NULL }, { kOptionInactivityDuration, kOptionStandbyAccelerateDelay } },
     { {kOptionInactivityDuration,
          required_argument, &args.doAction[kOptionInactivityWindowDurationIndex], 1}, kActionType,
        "Sets the duration of user inactivity window to be used to accelerate entry to standby",
        { kActionSetUserInactivityStart }, { NULL } },
    { {kOptionStandbyAccelerateDelay,
        required_argument, &args.doAction[kOptionStandbyAccelerateDelayIndex], 1}, kActionType,
        "Sets the minimum duration spent in normal sleep before accelerating to Standby",
        { kActionSetUserInactivityStart }, { NULL } },

    { {"help", no_argument, NULL, 'h'}, kNilType, NULL, { NULL }, { NULL } },

    { {NULL, 0, NULL, 0}, kNilType, NULL, { NULL }, { NULL } }
};

static void doCreatePowerSource(void);

static void accelerate_sleep_intervals();
static void init_args(args_struct *a);
void init_globals(globals_struct *g2);
/*************************************************************************/

int main(int argc, char *argv[])
{
    init_globals(&g);
    
    cacheArgvString(argc, argv);
    
    if ((argc < 2)) {
        usage();
        exit(1);
    }
    
    if (!parse_it_all(argc, argv)) {
        printf("Exiting because the command line arguments weren't correct.\n");
        exit(1);
    }
    
    if (args.doAction[kActionSetBattIndex]) {
        sendSmartBatteryCommand(kSBSetOverrideCapacity, args.batteryLevel);
        exit(1);
    }
    
    if (args.doAction[kActionResetBattIndex]) {
        sendSmartBatteryCommand(kSBSwitchToTrueCapacity, 0);
        exit(1);
    }

    if (args.doAction[kActionInactivityWindowIndex]) {
        sendInactivityWindowCommand(args.inactivityWindowStart,
                                    args.inactivityWindowDuration, args.standbyAccelerationDelay);
        exit(1);
    }

    if (args.doAction[kActionHibernateNowIndex] ||
        args.doAction[kActionStandbyNowIndex] ||
        args.doAction[kActionPowerOffNowIndex]) {
        
        gSleepWake = true;
        createPMConnectionListener();
        
        if (args.doAction[kActionHibernateNowIndex]) {
            sleepHandler(kSleepTypeHibernate);
        } else if (args.doAction[kActionStandbyNowIndex]) {
            sleepHandler(kSleepTypeStandby);
        } else {
            sleepHandler(kSleepTypePowerOff);
        }
        
        CFRunLoopRun();
    }
    
    print_the_plan();
    
    if (!args.standardSleepIntervals) {
        accelerate_sleep_intervals();
    }
    
    if (args.doRequestWake[kIOPMConnectRequestBackgroundIndex]
        || args.doRequestWake[kIOPMConnectRequestSleepServiceIndex]
        || args.doRequestWake[kIOPMConnectRequestMaintenanceIndex]
        || args.doAction[kExitIfNextWakeIsNotPushIndex])
        //        || args.doAction[kPrintDarkWakeResidencyTimeIndex])
    {
        createPMConnectionListener();
    }
    
    if (args.doItWhen & kDoItUponACAttach) {
        int out_token;
        int status;
        status = notify_register_dispatch(kIOPSNotifyPowerSource, &out_token,
                                          dispatch_get_main_queue(), ^(int t) {
                                              executeTimedActions("ACAttach");
                                          });
        if (NOTIFY_STATUS_OK != status) {
            printf("Error %d registering for ACAttach dispatch notification; exiting.\n", status);
            exit(1);
        }
    }
    
    if (args.doAction[kBringTheHeatIndex]) {
        bringTheHeat();
    }
    
    if (args.doAction[kActionClaimIndex]) {
        printf("Claiming system wake event.\n");
        IOPMClaimSystemWakeEvent(CFSTR("pmtool"), CFSTR("Invoked with --claim"), NULL);
        
        /* Delay for a bit to allow the claim to process */
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            printf("Exit\n");
            exit(0);
        });
        
        dispatch_main();
    }
    
    if (args.doAction[kActionCreatePowerSourceIndex]) {
        printf("Creating a debug power source.\n");
        doCreatePowerSource();
        printf("Press control-C to exit\n");
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
    
    if (!(args.doItWhen & kDoItNow)) {
        CFRunLoopRun();
    }

    return 0;
}

/*************************************************************************/

static void usage(void)
{
    printf("Usage: pmtool [action] [options]\npmtool is an OS X Core OS test tool to exercise DarkWake, Power Nap, and related API & SPI.\n");
    printf("[v0.1 BETA] This version of pmtool was built on %s %s\n\n", __DATE__, __TIME__);
    printf("Options: Options affect the behavior of actions, below. You may specify zero or more options. \n");
    int optcount = sizeof(pmtool_options) / sizeof(DTOption);
    for(int i=0; i<optcount; i++)
    {
        if ((NULL != pmtool_options[i].getopt_long.name)
            && (kOptionType == pmtool_options[i].type))
        {
            printf(" --%s\n   %s\n",
                   pmtool_options[i].getopt_long.name,
                   pmtool_options[i].desc);
            
            if (i != optcount) {
                printf("\n");
            }
        }
    }
    
    printf("\nActions: Actions tell pmtool what to do. You must specify exactly one action. \n");
    for(int i=0; i<optcount; i++)
    {
        if ((NULL != pmtool_options[i].getopt_long.name)
            && (kActionType == pmtool_options[i].type))
        {
            printf(" --%s\n   %s\n",
                   pmtool_options[i].getopt_long.name,
                   pmtool_options[i].desc);
            
            /* Print required options */
            if (pmtool_options[i].required_options[0])
            {
                int oind = 0;
                const char *printopt = NULL;
                printf("   You must specify one of: ");
                while ((printopt = pmtool_options[i].required_options[oind])) {
                    printf("--%s ", printopt);
                    oind++;
                }
                printf("\n");
            }
            
            /* Print required options */
            if (pmtool_options[i].optional_options[0])
            {
                int oind = 0;
                const char *printopt = NULL;
                printf("   You may specify: ");
                while ((printopt = pmtool_options[i].optional_options[oind])) {
                    printf("--%s ", printopt);
                    oind++;
                }
                printf("\n");
            }
            if (i != optcount) {
                printf("\n");
            }
            
            if (!strcmp(pmtool_options[i].getopt_long.name, kActionCreateAssertion)) {
                for (int a_index = 0; a_index<g.assertionsArgCount; a_index++) {
                    printf("%25s for type %s\n", g.assertionTypes[a_index].arg, g.assertionTypes[a_index].desc);
                }
                printf("\n");
            }
            if (!strcmp(pmtool_options[i].getopt_long.name, kActionCall)) {
                int c_index = 0;
                while (calls[c_index].arg) {
                    printf("%25s for call %s\n", calls[c_index].arg, calls[c_index].desc);
                    c_index++;
                }
                printf("\n");
            }
        }
    }
    printf("\n");
    printf("Examples: You can use pmtool to write sentences.\n");
    printf(" -> Do network maintenance wakes at 30 second intervals, staying awake for 10 seconds. Stop after 10 times.\n");
    printf("    pmtool --maintenancewake 30 \n\
           --createassertion maintenancewake  --assertiontimeout 10 --enterdarkwake \n\
           --sleepnow --iterations 10\n");
    
    printf("\n -> Do push service DarkWakes using push service mechanisms & assertions.\n");
    printf("    pmtool --sleepservicewake 60 \n\
           --createassertion applepushservice --assertiontimeout 120 --enterdarkwake \n\
           --sleepnow --iterations 1");
    
    printf("\n");
    
}

static bool parse_it_all(int argc, char *argv[]) {
    int                 optind;
    char                ch = 0;
    struct option       *long_opts = NULL;
    int                 long_opts_count = 0;
    long                temp_arg = 0;
    char                *arg;
    
    init_args(&args);
    
    g.assertionTypes = createAssertionOptions(&g.assertionsArgCount);
    
    args.sleepIntervalSec = kSleepIntervalSec;
    args.assertionTimeoutSec = kAssertionTimeoutSec;
    
    long_opts = long_opts_from_pmtool_opts(&long_opts_count);
    
    do {
        ch = getopt_long(argc, argv, "hg", long_opts, &optind);
        
        if (optind > 0 && optind < long_opts_count) {
            arg = (char *)long_opts[optind].name;
        } else {
            arg = NULL;
        }
        
        if (-1 == ch)
            break;
        
        if ('?' == ch || 1 == ch || ':' == ch) {
            return false;
        }
        
        if (('h' == ch)
            || (arg && !strcmp(arg, "help"))) {
            usage();
            exit(0);
        }
        
        if (('g' == ch)
            || (arg && !strcmp(arg, kActionGet))) {
            print_everything_dark();
            exit(0);
        }
        else if (arg && (!strcmp(arg, kActionRequestMaintenanceWake)
                         || !strcmp(arg, kActionRequestBackgroundWake)
                         || !strcmp(arg, kActionRequestSleepServiceWake))) {
            args.sleepIntervalSec = strtol(optarg, NULL, 10);
        }
        else if (arg && !strcmp(arg, kOptionAssertionTimeout)) {
            args.assertionTimeoutSec = strtol(optarg, NULL, 10);
        }
        else if (arg && !strcmp(arg, kActionSetTCPKeepAliveExpirationTimeout)) {
            temp_arg = strtol(optarg, NULL, 10);
            IOPMSetValueInt(kIOPMTCPKeepAliveExpirationOverride, (int)temp_arg);
            printf("Updated \"TCPKeepAliveExpiration\" to %lds\n", temp_arg);
            exit(0);
        }
        else if (arg && !strcmp(arg, kActionSetBatt)) {
            args.batteryLevel = (int)strtol(optarg, NULL, 10);
        }
        else if (arg && !strcmp(arg, kOptionIterations)) {
            args.iterationsCount = (int)strtol(optarg, NULL, 10);
            strtol(optarg, NULL, 10);
        }
        else if (arg && !strcmp(arg, kOptionSleepServiceCapTimeout)) {
            args.sleepServiceCapTimeoutSec = strtol(optarg, NULL, 10);
        }
        else if (arg && !strcmp(arg, kActionDoAckTimeout)) {
            args.doAction[kDoAckTimeoutIndex] = 1;
            if (!strcmp(kArgIOPMConnection, optarg)) {
                args.ackIOPMConnection = false;
            } else if (!strcmp(kArgIORegisterForSystemPower, optarg)) {
                args.ackIORegisterForSystemPower = false;
            } else {
                printf("Unrecognized ackTimeout argument: %s", optarg);
                exit(1);
            }
        }
        else if (arg && !strcmp(arg, kActionCreateAssertion)) {
            for (int j=0; j<g.assertionsArgCount; j++) {
                if (!strcmp(g.assertionTypes[j].arg, optarg)) {
                    args.takeAssertionNamed = g.assertionTypes[j].assertionType;
                    break;
                }
            }
            if (!args.takeAssertionNamed) {
                printf("Unrecognized assertion type %s.\n", optarg);
                exit(1);
            }
        }
        else if (arg && !strcmp(arg, kActionCall)) {
            int j=0;
            while (calls[j].arg) {
                if (!strcmp(calls[j].arg, optarg)) {
                    strlcpy(args.callIOKit, calls[j].arg, sizeof(args.callIOKit));
                }
                j++;
            }
            
            if (!args.callIOKit[0]) {
                printf("Error: Unrecognized call %s.\n", optarg);
                exit(1);
            }
        }
        else if (arg && !strcmp(arg, kActionExec)) {
            strlcpy(args.exec, optarg, sizeof(args.exec));
            if (!args.exec[0]) {
                printf("Error: Unrecognized exec string specified %s.\n", optarg);
                exit(1);
            }
        }
        else if (arg && !strcmp(arg, kActionSetUserInactivityStart)) {
            args.inactivityWindowStart = strtol(optarg, NULL, 0);
        }
        else if (arg && !strcmp(arg, kOptionInactivityDuration)) {
            args.inactivityWindowDuration = strtol(optarg, NULL, 0);
        }
        else if (arg && !strcmp(arg, kOptionStandbyAccelerateDelay)) {
            args.standbyAccelerationDelay = strtol(optarg, NULL, 0);
        }
        
    } while (1);
    
    
    
    if (args.doAction[kCreateAssertionIndex]) {
        if (!args.doItWhen) {
            printf("You must specify a time option for your assertion. See usage(); pass \"--now\" or \"--upondarkwake\"\n");
            exit(1);
        }
    }
    else if ((args.doAction[kOptionInactivityWindowDurationIndex] || args.doAction[kOptionStandbyAccelerateDelayIndex])
             && (args.doAction[kActionInactivityWindowIndex] == 0)) {
        printf("Option \'--%s\' is missing.\n", kActionSetUserInactivityStart);
        exit(1);
    }
    
    return true;
}

static void print_the_plan(void)
{
    int actions_count = 0;
    
    print_pretty_date(true);
    
    if (args.standardSleepIntervals) {
        printf("%-20sDidn't accelerate DWLInterval or BTInterval\n", "Intervals:");
    }
    
    for (int i=0; i<kActionsCount; i++) {
        if (args.doAction[i]) {
            actions_count++;
            printf("%-20s", "Action:");
            printf("%s", pmtool_options[i].getopt_long.name);
            
            if (kActionExecIndex == i) {
                printf(" \"%s\"", args.exec);
            } else
                if (kCallIndex == i)
                {
                    printf(" %s", args.callIOKit);
                } else
                    if ((kCreateAssertionIndex == i) && args.takeAssertionNamed)
                    {
                        char buf[120];
                        CFStringGetCString(args.takeAssertionNamed, buf, sizeof(buf), kCFStringEncodingUTF8);
                        printf(" %s", buf);
                        
                        if (0 != args.assertionTimeoutSec) {
                            printf(" with timeout=%lds", args.assertionTimeoutSec);
                        }
                    }
            
            printf("\n"); fflush(stdout);
        }
    }
    
    if (args.doItWhen) {
        printf("%-20s", "When:");
        if (args.doItWhen & kDoItNow) {
            printf("Now\n");
        }
        else if (args.doItWhen & kDoItUponDarkWake) {
            printf("Upon entering the next DarkWake\n");
        }
        else if (args.doItWhen & kDoItUponUserWake) {
            printf("Upon entering the next UserWake\n");
        }
        else if (args.doItWhen & kDoItUponACAttach) {
            printf("Upon AC Attach\n");
        }
    }
    
    if (args.doRequestWake[kIOPMConnectRequestBackgroundIndex])
    {
        printf("%-20sBackgroundWake in %ld\n", "Request:", args.sleepIntervalSec);
    }
    else if (args.doRequestWake[kIOPMConnectRequestMaintenanceIndex])
    {
        printf("%-20sMaintenanceWake in %ld\n", "Request:", args.sleepIntervalSec);
    }
    else if (args.doRequestWake[kIOPMConnectRequestSleepServiceIndex])
    {
        printf("%-20sSleepServicesWake in %lds with default cap\n", "Request:", args.sleepIntervalSec);
    }
    
    if (args.doIterations) {
        printf("%-20sFor %d iterations\n", "Repeat:", args.iterationsCount);
    } else {
        printf("%-20sForever (--iterations not specified)", "Repeat:");
        
    }
    printf("\n");
    
    if (actions_count == 0 ) {
        printf("Exiting: caller didn't specify an action.\n");
        exit(1);
    }
    if (actions_count > 1) {
        printf("Exiting: Caller specified too many actions.\n");
        exit(1);
    }
    
    
}

static struct option *long_opts_from_pmtool_opts(int *count)
{
    int i;
    struct option *retopt = NULL;
    
    *count = sizeof(pmtool_options)/sizeof(DTOption);
    
    retopt = calloc(*count, sizeof(struct option));
    
    for (i=0; i<*count; i++) {
        bcopy(&pmtool_options[i].getopt_long, &retopt[i], sizeof(struct option));
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
    printf("%s ", why);fflush(stdout);
    
    if (args.takeAssertionNamed)
    {
        printf("Create assertion %s\n", CFStringGetCStringPtr(args.takeAssertionNamed, kCFStringEncodingUTF8));
        execute_Assertion(args.takeAssertionNamed, args.assertionTimeoutSec);
    }
    else if (args.callIOKit[0]) {
        printf("Call IOKit function %s\n", args.callIOKit);
        execute_APICall(args.callIOKit);
    } else if (args.exec[0]) {
        
        exec_exec(args.exec);
    }
}

/*************************************************************************/

static void exec_exec(char* run_command) {
    pid_t pid;
    char *argv[45];
    int pipes[2];
    bool print_newline = false;
    char reading_buf[1];
    int status;
    
    pipe(pipes);
    bzero(argv, sizeof(argv));
    
    argv[0] = "/usr/bin/caffeinate";
    int  i=1;
    argv[i] = strtok(run_command, " ");
    printf ("Executing: %s %s ", argv[0], argv[1]);
    while (argv[i]) {
        argv[++i] = strtok(NULL, " ");
        if (argv[i])
            printf("%s ", argv[i]);
        if (i>=45) {
            break;
        }
    }
    fflush(stdout);
    printf("\n");
    fflush(stdout);
    
    pid = fork();
    if (-1 == pid) {
        PMTestLog("fork error");
        exit(1);
        // Not reached
    }
    else if (0 == pid) {
        dup2(pipes[STDOUT_FILENO], STDOUT_FILENO);
        close(pipes[STDIN_FILENO]);
        
        execvp(argv[0], argv);
        perror(*argv);
        _exit((errno == ENOENT) ? 127 : 126);
        // Not reached
    }
    
    /* parent */
    
    close(pipes[STDOUT_FILENO]);
    while(read(pipes[0], reading_buf, 1) > 0)
    {
        if (reading_buf[0] == '\n' || reading_buf[0] == '\r') {
            print_newline = true;
            continue;
        }
        
        if (print_newline) {
            write(1, "\n", 1);
            write(1, "output: ", strlen("output: "));
            print_newline = false;
        }
        write(1, reading_buf, 1); // 1 -> stdout
    }
    close(pipes[STDIN_FILENO]);
    fflush(stdout);
    
    printf("\n");
    if (waitpid(pid, &status, 0) < 0) {
        perror("");
        exit(1);
    }
    
    return;
}


static void execute_Assertion(CFStringRef type, long timeout)
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

static void execute_APICall(const char *callname)
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
        ret = IOPMAssertionDeclareNotificationEvent(CFSTR("pmtool"), args.assertionTimeoutSec, &dontcare);
    } else if (!strcmp(kCallDeclareSystemIsActive, callname))
    {
        ret = IOPMAssertionDeclareSystemActivity(CFSTR("pmtool-system"), &dontcare, &systemstate);
        if (kIOReturnSuccess == ret) {
            if (kIOPMSystemSleepReverted == systemstate) {
                printf("IOPMAssertionDeclareSystemActivity reverted system sleep.\n");
            } else {
                printf("IOPMAssertionDeclareSystemActivity did not revert system sleep.\n");
            }
        }
    } else if (!strcmp(kCallDeclareUserIsActive, callname)) {
        ret = IOPMAssertionDeclareUserActivity(CFSTR("pmtool-user"), kIOPMUserActiveLocal, &dontcare);
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
    static int wakeupcount = 0;
    char buf[100];
    int buf_size = sizeof(buf);
    
    if (gSleepWake) {
        if (IOPMIsAUserWake(capabilities)) {
            wakeHandler();
        }
        return;
    }

    IOPMGetCapabilitiesDescription(buf, buf_size, previousCapabilities);
    printf("Transition from %s", buf);
    IOPMGetCapabilitiesDescription(buf, buf_size, capabilities);
    printf(", to %s\n", buf);
    
    if(IOPMIsADarkWake(capabilities)
       && !IOPMIsADarkWake(previousCapabilities)
       && args.doItWhen & kDoItUponDarkWake)
    {
        executeTimedActions("DarkWake");
        
        wakeupcount += 1;
        
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
        
        wakeupcount += 1;
    }
    
    if (args.doIterations && wakeupcount > args.iterationsCount)
    {
        IOPMAssertionID     _id;
        printf("Completed %d wakeup iterations - exiting.\n", args.iterationsCount);
        IOPMAssertionDeclareUserActivity(g.invokedStr, kIOPMUserActiveLocal, &_id);
        exit(0);
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
    
    if (!args.ackIOPMConnection)
    {
        PMTestLog("Skipping IOPMConnection acknowlegement.");
        
    } else
    {
        
        if (ackDictionary) {
            ret = IOPMConnectionAcknowledgeEventWithOptions(connection, token, ackDictionary);
            CFRelease(ackDictionary);
        } else {
            ret = IOPMConnectionAcknowledgeEvent(connection, token);
        }
        
        if (kIOReturnSuccess != ret) {
            PMTestFail("IOPMConnectionAcknowledgement%s failed with 0x%08x", ackDictionary?"WithOptions ":" ", ret);
        }
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
    bool tcpka_activeConnectionsExist = false;
    
    IOPlatformCopyFeatureDefault(kIOPlatformTCPKeepAliveDuringSleep, &b);
    printf("  Platform: TCPKeepAliveDuringSleep = %s\n", (kCFBooleanTrue == b) ? "yes":"no");
    IOPlatformCopyFeatureDefault(CFSTR("NotificationWake"), &b);
    printf("  Platform: NotificationWake = %s\n", (kCFBooleanTrue == b) ? "yes":"no");
    IOPlatformCopyFeatureDefault(CFSTR("DNDWhileDisplaySleeps"), &b);
    printf("  Platform: DNDWhileDisplaySleeps = %s\n", (kCFBooleanTrue == b) ? "yes":"no");
    
    
    int tcpKeepAliveActive = IOPMGetValueInt(kIOPMTCPKeepAliveIsActive);
    int tcpKeepAliveExpires = IOPMGetValueInt(kIOPMTCPKeepAliveExpirationOverride);
    IOPMGetActivePushConnectionState(&tcpka_activeConnectionsExist);
    
    printf("  TCPKeepAlive Active = %d\n", tcpKeepAliveActive);
    printf("  TCPKeepAlive Expiration = %ds\n", tcpKeepAliveExpires);
    printf("  TCPKeepAlive ConnectionsExist = %s\n", (tcpka_activeConnectionsExist) ? "yes" : "no");
}

enum {
    kCurrentCapacity = 0,
    kMaxCapacity,
    kName,
    kTimeToEmpty,
    kTimeToFull,
    kTimeRemaining,
    kIsCharged,
    kIsCharging,
    kIsPresent,
    kDesignCap,
    kVoltage,
    kIsFinishingCharge,
    kTransportType,
    kType
};

static CFDictionaryRef copyNextPSDictionary(void)
{
    const CFStringRef   keys[] = {
        CFSTR(kIOPSCurrentCapacityKey),
        CFSTR(kIOPSMaxCapacityKey),
        CFSTR(kIOPSNameKey),
        CFSTR(kIOPSTimeToEmptyKey),
        CFSTR(kIOPSTimeToFullChargeKey),
        CFSTR(kIOPSTimeRemainingNotificationKey),
        CFSTR(kIOPSIsChargedKey),
        CFSTR(kIOPSIsChargingKey),
        CFSTR(kIOPSIsPresentKey),
        CFSTR(kIOPSDesignCapacityKey),
        CFSTR(kIOPSVoltageKey),
        CFSTR(kIOPSIsFinishingChargeKey),
        CFSTR(kIOPSTransportTypeKey),
        CFSTR(kIOPSTypeKey)
    };
    
    CFTypeRef *values = NULL;
    int tmpInt = 0;
    
    int count = sizeof(keys)/sizeof(CFTypeRef);
    values = (CFTypeRef *)calloc(1, count * sizeof(CFTypeRef));
    
    tmpInt = 4000;
    values[kCurrentCapacity] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    tmpInt *= 2;
    values[kMaxCapacity] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kName] = CFSTR("com.iokit.IOPSCreatePowerSource");
    tmpInt = 25;
    values[kTimeToEmpty] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    tmpInt = 0;
    values[kTimeToFull] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kTimeRemaining] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kIsCharged] = CFRetain(kCFBooleanFalse);
    values[kIsCharging] = CFRetain(kCFBooleanFalse);
    values[kIsPresent] = CFRetain(kCFBooleanTrue);
    tmpInt = 7000;
    values[kDesignCap] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    tmpInt = 1100;
    values[kVoltage] = CFNumberCreate(0, kCFNumberIntType, &tmpInt);
    values[kIsFinishingCharge] = CFRetain(kCFBooleanTrue);
    values[kTransportType] = CFSTR(kIOPSUSBTransportType);
    values[kType] = CFSTR(kIOPSUPSType);
    
    return CFDictionaryCreate(0, (const void **)keys, (const void **)values, count,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
    
}


typedef struct  {
    CFMachPortRef   configdConnection;
    int     psid;
} __OpaqueIOPSPowerSourceID;
static void doCreatePowerSource(void)
{
    CFDictionaryRef         useDictionary = NULL;
    IOReturn                ret;
    IOPSPowerSourceID         psid = 0;
    __OpaqueIOPSPowerSourceID *cast = NULL;
    
    useDictionary = copyNextPSDictionary();
    if (!useDictionary) {
        printf("doCreatePowerSource FAIL during setup: couldn't create descriptor dictionary.\n");
        exit(1);
    }
    
    
    ret = IOPSCreatePowerSource(&psid);
    if (kIOReturnSuccess != ret) {
        printf("FAIL: createAndCheckForExistence couldn't create PS power source 0x%08x\n", ret);
        return;
    }
    cast = (__OpaqueIOPSPowerSourceID *)psid;
    printf("[PASS] New power source with id=%d(@%p)\n", cast->psid, psid);
    
    ret = IOPSSetPowerSourceDetails(psid, useDictionary);
    
    if (kIOReturnSuccess == ret) {
        printf("[PASS] successfully set power source.\n");
    } else {
        printf("[FAILURE] IOPSSetPowerSourceDetails returns error code 0x%08x\n", ret);
        exit(1);
    }
    
    CFRelease(useDictionary);
    
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

static void sleepHandler(int sleepType)
{
    io_connect_t connect = IOPMFindPowerManagement(kIOMasterPortDefault);
    
    // Set pmset values according to sleep type
    if (sleepType == kSleepTypeHibernate) {
        system("/usr/bin/pmset standby 0 autopoweroff 0 hibernatemode 25");
    } else if (sleepType == kSleepTypeStandby) {
        system("/usr/bin/pmset standby 1 hibernatemode 25");
    } else if (sleepType == kSleepTypePowerOff) {
        system("/usr/bin/pmset standby 0 autopoweroff 1 hibernatemode 25");
    }
    
    IOReturn ret = IOPMSleepSystem(connect);
    
    if (ret != kIOReturnSuccess) {
        if (ret == kIOReturnNotPrivileged) {
            printf("Sleep error 0x%08x; You must run this as root.\n", ret);
            exit(EX_NOPERM);
        } else {
            printf("Unable to sleep system: error 0x%08x\n", ret);
            exit(EX_OSERR);
        }
    } else {
        printf("Sleeping now...\n");
    }
}

static void wakeHandler(void)
{
    CFTypeRef keys[] = { CFSTR(kIOPMDeepSleepEnabledKey), CFSTR(kIOPMAutoPowerOffEnabledKey), CFSTR(kIOHibernateModeKey) };
    CFArrayRef keysArray = CFArrayCreate(kCFAllocatorDefault, keys, sizeof(keys) / sizeof(CFTypeRef), &kCFTypeArrayCallBacks);
    
    printf("Reverting pm preferences...\n");
    
    if (keysArray) {
        IOReturn ret = IOPMRevertPMPreferences(keysArray);
        if (ret != kIOReturnSuccess) {
            printf("IOPMRevertPMPreferences failed.\n");
        }
        CFRelease(keysArray);
    }
    exit(1);
}

void _CFDictionarySetLong(CFMutableDictionaryRef d, CFStringRef k, long val)
{
    CFNumberRef     num = NULL;
    
    num = CFNumberCreate(0, kCFNumberLongType, (const void *)&val);
    if (num)
    {
        CFDictionarySetValue(d, k, num);
        CFRelease(num);
    }
}

void accelerate_sleep_intervals(void)
{
    system("/usr/bin/pmset btinterval 0");
    system("/usr/bin/pmset dwlinterval 0");
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


static void cacheArgvString(int argc, char *argv[])
{
    
    g.invokedStr = CFStringCreateMutable(0, 0);
    for (int i=1; i<argc; i++) {
        if (argv[i]) {
            CFStringAppendCString(g.invokedStr, argv[i], kCFStringEncodingUTF8);
            if (i+1 < argc) {
                CFStringAppendCString(g.invokedStr, " ", kCFStringEncodingUTF8);
            }
        }
    }
    
    if (g.invokedStr)
    {
        char tmp_buf[200];
        CFStringGetCString(g.invokedStr, tmp_buf, sizeof(tmp_buf), kCFStringEncodingUTF8);
        printf("- pmtool: %s\n", tmp_buf);
    }
}

void init_args(args_struct *a) {
    bzero(a, sizeof(args_struct));
    
    a->ackIORegisterForSystemPower = true;
    a->ackIOPMConnection = true;
}

void init_globals(globals_struct *g2) {
    bzero(g2, sizeof(globals_struct));
}

static void
sendSmartBatteryCommand(uint32_t which, uint32_t level)
{
    io_service_t    sbmanager = MACH_PORT_NULL;
    io_connect_t    sbconnection = MACH_PORT_NULL;
    kern_return_t   kret;
    uint32_t        output_count = 1;
    uint64_t        uc_return = kIOReturnError;
    uint64_t        input = 0;
    CFNumberRef     capacity = NULL;
    int             _capacity = 0;
    io_iterator_t   iterator = IO_OBJECT_NULL;
    
    // Find SmartBattery manager
    sbmanager = IOServiceGetMatchingService(MACH_PORT_NULL,
                                            IOServiceMatching("AppleSmartBatteryManager"));
    
    if (MACH_PORT_NULL == sbmanager) {
        goto bail;
    }
    
    kret = IOServiceOpen( sbmanager, mach_task_self(), 0, &sbconnection);
    if (kIOReturnSuccess != kret) {
        goto bail;
    }
    
    if (which == kSBSetOverrideCapacity) {
        // Get battery max capacity
        IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOPMPowerSource"), &iterator);
        if (iterator) {
            io_registry_entry_t entry = IO_OBJECT_NULL;
            while ((entry = IOIteratorNext(iterator)))
            {
                CFMutableDictionaryRef ioregDict = NULL;
                if (KERN_SUCCESS == IORegistryEntryCreateCFProperties(entry, &ioregDict, kCFAllocatorDefault, kNilOptions)) {
                    capacity = CFDictionaryGetValue(ioregDict, CFSTR(kIOPMPSMaxCapacityKey));
                    if(capacity) CFNumberGetValue(capacity, kCFNumberIntType, &_capacity);
                    break;
                }
            }
            IOObjectRelease(iterator);
        }
        
        // Convert user input to a percentage of total capacity
        input = _capacity * ((double)level / 100);
        printf("Setting capacity to %llu (Max=%d).\n", input, _capacity);
        
        IOConnectCallMethod(
                            sbconnection, // connection
                            which,      // selector
                            &input,      // uint64_t *input
                            1,          // input Count
                            NULL,       // input struct count
                            0,          // input struct count
                            &uc_return, // output
                            &output_count,  // output count
                            NULL,       // output struct
                            0);         // output struct count
    } else {
        printf("Resetting battery percentage.\n");
        IOConnectCallMethod(
                            sbconnection, // connection
                            which,      // selector
                            NULL,      // uint64_t *input
                            0,          // input Count
                            NULL,       // input struct count
                            0,          // input struct count
                            &uc_return, // output
                            &output_count,  // output count
                            NULL,       // output struct
                            0);         // output struct count
    }
    
bail:
    
    if (MACH_PORT_NULL != sbconnection) {
        IOServiceClose(sbconnection);
    }
    
    if (MACH_PORT_NULL != sbmanager) {
        IOObjectRelease(sbmanager);
    }
    
    return;
}

void processXpcEvent(xpc_object_t msg)
{
    xpc_type_t type = xpc_get_type(msg);

    if (type == XPC_TYPE_DICTIONARY) {
        printf("Unexpected xpc event\n");
    }
    else if (type == XPC_TYPE_ERROR) {
        printf("Received xpc error\n");
    }
}


static void sendInactivityWindowCommand(long start, long duration, long delay)
{
    xpc_object_t            desc = NULL;
    xpc_object_t            msg = NULL;
	xpc_object_t			connection;

    if (geteuid() != 0) {
        printf("Error: This command must be issued as root\n");
        return;
    }
    connection = xpc_connection_create_mach_service(POWERD_XPC_ID, dispatch_get_main_queue(), 0);
    if (!connection) {
        printf("Failed to open connection\n");
        return;
    }
    xpc_connection_set_target_queue(connection, dispatch_get_main_queue());

    xpc_connection_set_event_handler(connection,
        ^(xpc_object_t msg ) {processXpcEvent(msg); });

    xpc_connection_resume(connection);


    desc = xpc_dictionary_create(NULL, NULL, 0);
    msg = xpc_dictionary_create(NULL, NULL, 0);
    if (desc && msg) {
        xpc_dictionary_set_int64(desc, kInactivityWindowStart, start);
        xpc_dictionary_set_int64(desc, kInactivityWindowDuration, duration);
        xpc_dictionary_set_int64(desc, kStandbyAccelerationDelay, delay);
        xpc_dictionary_set_value(msg, kInactivityWindowKey, desc);


        xpc_connection_send_message_with_reply(connection, msg, dispatch_get_main_queue(), ^(xpc_object_t reply) {
            if (xpc_get_type(reply) == XPC_TYPE_DICTIONARY) {
                uint64_t err = xpc_dictionary_get_uint64(reply, kMsgReturnCode);
                if (err == 0) {
                    printf("Inactivity window set successfully\n");
                }
                else {
                    printf("Failed to set inactivity window(err:0x%llx\n", err);
                }
            }
            else {
                printf("Received unknown response\n");
            }
            xpc_connection_cancel(connection);
            exit(0);
        });

        dispatch_main();
    }
    else {
        printf("Failed to create xpc objects to send message\n");
    }
    if (msg) {
        xpc_release(msg);
    }
    if (desc) {
        xpc_release(desc);
    }

    xpc_release(connection);
}
