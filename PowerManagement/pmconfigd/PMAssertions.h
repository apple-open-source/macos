/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 29-Aug-02 ebold created
 *
 */
#ifndef _PMAssertions_h_
#define _PMAssertions_h_

#include "XCTest_FunctionDefinitions.h"
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#include <sys/queue.h>

#define IOREPORT_ABORT(str...) \
do {    \
    asl_log(0,0,ASL_LEVEL_ERR, (str));  \
    abort(); \
} while(0)


#include <IOKit/IOReportMacros.h>
#include <IOKit/IOReportTypes.h>
#include <xpc/xpc.h>

/* ExternalMedia assertion
 * This assertion is only defined here in PM configd. 
 * It can only be asserted by PM configd; not by other user processes.
 */
#define _kIOPMAssertionTypeExternalMediaCStr    "ExternalMedia"
#define _kIOPMAssertionTypeExternalMedia        CFSTR(_kIOPMAssertionTypeExternalMediaCStr)

/* PreventStandby assertion
 * This assertion is only defined here in PM configd.
 */

#define _kIOPMAssertionTypePreventStandbyCStr   "PreventStandby"
#define _kIOPMAssertionTypePreventStandby       CFSTR(_kIOPMAssertionTypePreventStandbyCStr)

#ifndef     kIOPMRootDomainWakeTypeNetwork
#define     kIOPMRootDomainWakeTypeNetwork          CFSTR("Network")
#endif

#ifndef     kIOPMRootDomainWakeTypeAlarm
#define     kIOPMRootDomainWakeTypeAlarm            CFSTR("Alarm")
#endif

#ifndef     kIOPMRootDomainWakeTypeMaintenance  
#define     kIOPMRootDomainWakeTypeMaintenance      CFSTR("Maintenance")
#endif

#ifndef     kIOPMRootDomainWakeTypeSleepTimer
#define     kIOPMRootDomainWakeTypeSleepTimer       CFSTR("SleepTimer")
#endif

#ifndef kIOPMRootDomainWakeTypeSleepService
#define kIOPMRootDomainWakeTypeSleepService         CFSTR("SleepService")
#endif

#ifndef kIOPMRootDomainWakeReasonRTC
#define  kIOPMRootDomainWakeReasonRTC        CFSTR("RTC")
#endif

#ifndef kIORootDomainWakeReasonDarkPME
#define kIORootDomainWakeReasonDarkPME          CFSTR("EC.DarkPME")
#endif

#ifndef kIOPMrootDomainWakeTypeLowBattery
#define kIOPMrootDomainWakeTypeLowBattery   CFSTR("LowBattery")
#endif

#ifndef kIOPMRootDomainWakeTypeNotification
#define kIOPMRootDomainWakeTypeNotification CFSTR("Notification")
#endif

#ifndef kIOPMRootDomainWakeTypeUser
#define kIOPMRootDomainWakeTypeUser         CFSTR("User")
#endif

#define  kDisplayTickleDelay  30        // Mininum delay(in secs) between sending tickles

/*
 * Lower 16 bits are used for assertionID created by powerd.
 * Upper 16 bits are used for assertionID created by the client process creating
 * async assertions.
 */
#define ID_FROM_INDEX(idx)          (((idx) & 0x7fff) | 0x8000)
#define INDEX_FROM_ID(id)           ((id) & ~0x8000)

#define MAKE_UNIQAID(time, type, idx) \
    ((((uint64_t)time) & 0xffffffff) << 32) | ((type) & 0xffff) << 16 | ((idx) & 0xffff)
/*
 * kMaxAssertions  should be <= 0x7fff.
 * Then the 'idx' used ID_FROM_INDEX will be <= 0x7fff
 */
#define kMaxAssertions              10240

/*
 * A 'assertion_t' stucture is created for each assertion created by the processes.
 *
 * Each assertion will be associated with one of the pre-defined assertion types. All
 * assertion types are defined in kerAssertionType enum. A 'assertionType_t' strcture is 
 * created for each assertion type at the start of the process and can be accessed from 
 * 'gAssertionTypes' array.
 *
 * Each assertion type will have a pre-defined effect on the system. All possible effects are
 * listed in 'kerAssertionEffect' enum. A 'assertionEffect_t' structure is created for each 
 * assertion effect at the start of the process and can be accessed from 'gAssertionEffects' 
 * array.
 *
 * An assertion type can change the effect it has on the system based on various conditions 
 * like power source, wake type, user settings etc. When an assertion type's effect changes, 
 * it is moved from assertionEffect_t structure to another one.
 */
typedef enum {
    kNoEffect                       = 0,
    kPrevIdleSlpEffect,
    kPrevDemandSlpEffect,
    kPrevDisplaySlpEffect,  // Stats are maintained for effects up to this one

    kExternalMediaEffect,
    kPreventDiskSleepEffect,
    kTicklessDisplayWakeEffect,

    kEnableIdleEffect,
    kHighPerfEffect,
    kDisableInflowEffect,
    kInhibitChargeEffect,
    kDisableWarningsEffect,
    kNoRealPowerSourcesDebugEffect,

    kMaxAssertionEffects
} kerAssertionEffect;

// Stats are maintained only for affects up to kPrevDisplaySlpEffect
#define kMaxEffectStats     kPrevDisplaySlpEffect+1


/* IOPMAssertion levels
 * 
 * Each assertion type has a corresponding bitfield index, here.
 * Also as found under the "Bitfields" property in assertion CFDictionaries.
 */
typedef enum {
    // These must be consecutive integers beginning at 0
    kHighPerfType                   = 0,
    kPreventIdleType                = 1,
    kDisableInflowType              = 2,
    kInhibitChargeType              = 3,
    kDisableWarningsType            = 4,
    kPreventDisplaySleepType        = 5,
    kEnableIdleType                 = 6,
    kPreventSleepType               = 7,
    kExternalMediaType              = 8,
    kDeclareUserActivityType        = 9,
    kPushServiceTaskType            = 10,
    kBackgroundTaskType             = 11,
    kDeclareSystemActivityType      = 12,
    kSRPreventSleepType             = 13, /* Silent running Prevent Sleep, used as internal proxy */
    kTicklessDisplayWakeType        = 14, /* Display wake without HID tick */
    kPreventDiskSleepType           = 15,
    kIntPreventDisplaySleepType     = 16, /* Prevent display sleep, used as internal proxy */
    kNetworkAccessType              = 17, /* Prevent demand sleep on AC, prevent idle sleep on batt */
    kInteractivePushServiceType     = 18,
    kReservePwrPreventIdleType      = 19, /* Prevents idle sleep in reserve power mode */


    // Make sure this is the last enum element, as it tells us the total
    // number of elements in the enum definition
    kIOPMNumAssertionTypes      
} kerAssertionType;

/*
 * effectStats_t
 *
 * This structure accumulates the total duration for which a process is 
 * holding each assertion effect.
 */
typedef struct {
    uint32_t    cnt;            // Number of assertions of this effect currently held
    uint64_t    startTime;      // Time at which first assertion is taken after last reset
} effectStats_t;

typedef struct {
    uint8_t    assert_cnt [kIOPMNumAssertionTypes];  // Number of assertions of each type.
                                                     // Set only for app sleep preventing assertions
    uint32_t   aggTypes;                             // Aggregate assertion types of this proc. 
                                                     // Set only for app sleep preventing assertions
    effectStats_t       stats[kMaxEffectStats]; // Stats per assertion effect
    void                *reportBuf;                  // Stats buffer for IOReporter
                                  
    uint32_t            retain_cnt;     // Retain cnt of this structure
    CFStringRef         name;           // Process name
    

    XCT_UNSAFE_UNRETAINED dispatch_source_t   disp_src;       // Dispatch src to handle process exit
    
    pid_t               pid;            // PID
    uint32_t            create_seq;

    CFStringRef         assertionExceptionAggdKey;     // Aggd keys for updating stats for
    CFStringRef         aggregateExceptionAggdKey;     // assertion exception on this process

    XCT_UNSAFE_UNRETAINED xpc_object_t        remoteConnection;   // Connection for xpc based assertions

    uint32_t            maxAssertLength;    // Max assertion duration expected by this process
    uint32_t            aggAssertLength;    // Total duration assertions held since last reset

    uint32_t            anychange:1;    // Interested in any assertion changes notification
    uint32_t            aggchange:1;    // Interested in assertion aggregates change notifications
    uint32_t            timeoutchange:1;    // Interested in assertion timeout notification
    uint32_t            disableAS_pend:1;   // Disable AppSleep notification need to be sent
    uint32_t            enableAS_pend:1;    // Enable AppSleep notification need to be sent
    uint32_t            proc_exited:1;      // True if PROC_EXIT notification is received
    uint32_t            aggactivity:1;      // Contributed to gActivityAggCnt. Subscribed to AssertionActivityAggregate
    uint32_t            isSuspended:1;      // Process assertions are suspended
} ProcessInfo;

typedef struct assertion {
    LIST_ENTRY(assertion) link;
    CFMutableDictionaryRef props;       // client provided properties
    uint32_t        state;              // assertion state bits
    uint64_t        createTime;         // Time at which assertion is created
    uint64_t        timeout;            // absolute time at which assertion will timeout

    kerAssertionType    kassert;        // Assertion type, also index into gAssertionTypes
    IOPMAssertionID     assertionId;    // Assertion Id returned to client    

    uint32_t        mods;               // Modifcation bits for most recent SetProperties call

    uint32_t        retainCnt;          // Number of retain calls

    int             enTrIntensity;      // Intensity parameter for energy tracing of the assertion

    ProcessInfo     *pinfo;             // ProcessInfo structure for process that created assertion

    pid_t           causingPid;         // PID for process on whose behalf this assertion is raised
    ProcessInfo     *causingPinfo;      // Corresponding ProcessInfo struct 

    
    XCT_UNSAFE_UNRETAINED dispatch_source_t   procTimer;      // Timer set based on the value provided for this process.
    // Ths timer triggers log collection
    // System Qualifiers
    uint32_t        audioin:1;
    uint32_t        audioout:1;
    uint32_t        gps:1;
    uint32_t        baseband:1;
    uint32_t        bluetooth:1;
    uint32_t        allowsDeviceRestart:1;
    uint32_t        budgetedActivity:1;
} assertion_t;

/* State bits for assertion_t structure */
#define kAssertionStateTimed                0x001
#define kAssertionStateInactive             0x002
#define kAssertionStateValidOnBatt          0x004
#define kAssertionLidStateModifier          0x008
#define kAssertionTimeoutIsSystemTimer      0x010  // Assertion timeout value changes with system idle/display sleep timer 
#define kAssertionSkipLogging               0x020  // Avoid logging this assertion, even if type is set to kAssertionTypeLogOnCreate
#define kAssertionStateLogged               0x040
#define kAssertionStateAddsToProcStats      0x080
#define kAssertionProcTimerActive           0x100
#define kAssertionExitSilentRunningMode     0x200
#define kAssertionStateSuspended            0x400


/* Mods bits for assertion_t structure */
#define kAssertionModTimer              0x1
#define kAssertionModLevel              0x2
#define kAssertionModType               0x4
#define kAssertionModPowerConstraint    0x8
#define kAssertionModLidState           0x10
#define kAssertionModName               0x20
#define kAssertionModResources          0x40
#define kAssertionModSilentRunning      0x80
#define kAssertionModCausingPid         0x100

typedef enum {
    kAssertionOpRaise,
    kAssertionOpRelease,
    kAssertionOpEval,    // Evaluate for any changes to be sent to kernel due to enviromental changes
    kAssertionOpGlobalTimeout
} assertionOps;


typedef struct assertionType assertionType_t;
typedef void (*assertionHandler_f)(assertionType_t *a, assertionOps op);

typedef struct {
    LIST_HEAD(, assertionType)  assertTypes;
    kerAssertionEffect  effectIdx;
} assertionEffect_t;

/* Structure per kernel assertion type */
struct assertionType {
    uint32_t        flags;              /* Specific to this assertion type */

    LIST_HEAD(, assertion) activeTimed;  /* Active assertions with timeout */
    LIST_HEAD(, assertion) active;       /* Active assertions without timeout */
    LIST_HEAD(, assertion) inactive;     /* timed out assertions/Level 0 assertions etc */
    LIST_HEAD(, assertion) suspended;    /* Assertions that are suspended */

    kerAssertionType    kassert;
    XCT_UNSAFE_UNRETAINED dispatch_source_t   timer;          /* dispatch source for Per assertion timer */
    
    XCT_UNSAFE_UNRETAINED dispatch_source_t   globalTimer;    /* dispatch source for all assertions of this type */

    CFStringRef     entitlement;        /* if set, caller must have this entitlement to create this assertion */
    uint64_t        globalTimeout;      /* Relative time at which assertion is timedout */
    uint32_t        forceTimedoutCnt;   /* Count of assertions turned off due to global timer */
    CFStringRef     uuid;               /* Assertion type specific UUID */
    uint64_t        autoTimeout;        /* Automatic timeout for each assertion;set with kAssertionTypeAutoTimed flag */

    kerAssertionEffect   effectIdx;         
    LIST_ENTRY(assertionType)    link;
    assertionHandler_f  handler;        /* Function changing the required settings in the kernel for this assertion type */

    uint32_t            disableCnt;     /* Number of active disable requests for this type */
    CFArrayRef          procs;          /* ProcessInfo of processes holding this assertion type */

    // Fields changed by properties set on assertion. 
    // Not all fields are valid for all assertion types 
    uint32_t   validOnBattCount;        /* Count of assertions requesting to be active on Battery power */

    uint32_t   enTrQuality;             /* Quality or intensity for energy tracing */
} ;

typedef enum {
    kIndexActiveTimed = 0,
    kIndexActive = 1,
    kIndexInactive = 2,
    kIndexSuspended = 3,
    kIndexMaxCount
} listIndexType_t;

#define SELECT_MASK(idx) (0x1 << idx)

typedef enum {
    /* Only Active or ActiveTimed Assertions */
    kSelectActive = (SELECT_MASK(kIndexActiveTimed) | SELECT_MASK(kIndexActive)),
    /* Suspended Assertions as considered Inactive */
    kSelectInactive = (SELECT_MASK(kIndexInactive) | SELECT_MASK(kIndexSuspended)),
    kSelectSuspended = (SELECT_MASK(kIndexSuspended)),
    kSelectAllButSuspended = (SELECT_MASK(kIndexActiveTimed) | SELECT_MASK(kIndexActive) | SELECT_MASK(kIndexInactive)),
    kSelectAll = (SELECT_MASK(kIndexActiveTimed) | SELECT_MASK(kIndexActive) | SELECT_MASK(kIndexInactive) | SELECT_MASK(kIndexSuspended))
} listSelectType_t;

// Selectors for AppleSmartBatteryManagerUserClient
enum {
    kSBUCInflowDisable              = 0,
    kSBUCChargeInhibit              = 1
};

typedef enum {
    kTimerTypeTimedOut              = 0,
    kTimerTypeReleased              = 1
} TimerType;

enum {
    kIOPMSystemActivityAssertionDisabled = 0,
    kIOPMSystemActivityAssertionEnabled  = 1
};

enum {
    kIOPMActiveAssertions = 0,
    kIOPMInactiveAssertions
};

/* Flag bits for assertionType_t structure */
#define kAssertionTypeNotValidOnBatt        0x01     /* By default, this assertion type is not valid on battery power */
#define kAssertionTypeGloballyTimed         0x02     /* A global timer releases all assertions of this type */
#define kAssertionTypeDisabled              0x04     /* All assertion requests of this type are ignored */
#define kAssertionTypePreventAppSleep       0x08     /* App sleep is prevented when this assertion type is raised by app */
#define kAssertionTypeAutoTimed             0x10     /* Each assertion of this type automatically gets a timeout value */
#define kAssertionTypeLogOnCreate           0x20     /* Assertions of this type have to be logged on creation */

/* Assertion logging actions */
typedef enum {
    kACreateLog,
    kACreateRetain,
    kAReleaseLog,
    kAClientDeathLog,
    kATimeoutLog,
    kACapExpiryLog,
    kASummaryLog,
    kATurnOffLog,
    kATurnOnLog,
    kANameChangeLog,
    kAStateSuspend,
    kAStateResume
} assertLogAction;

typedef struct {
    // System wide count of assertions with each system qualifier
    int audioin;
    int audioout;
    int gps;
    int baseband;
    int bluetooth;
    int budgetedActivity;
    int restartPreventers;
} sysQualifier_t;

typedef struct {
    CFStringRef procName;
    int64_t     limit;      // Min threshold value for this bucket
    uint64_t    count;      // Number of occurrences
} exceptionStatsBucket_t;

/* Reasons for clamshell sleep disable */
enum {
    kClamshellDisableAssertions     = 0x1,
    kClamshellDisableDesktopMode    = 0x10,
    kClamshellDisableSystemSleep    = 0x100,
};

__private_extern__ void logKernelAssertions(CFNumberRef, CFArrayRef);
__private_extern__ void logChangedSleepPreventers(int preventerType);
__private_extern__ void PMAssertions_prime(void);
__private_extern__ void createOnBootAssertions(void);
__private_extern__ void PMAssertions_SettingsHaveChanged(void);
                        
__private_extern__ IOReturn _IOPMAssertionCreateRequiresRoot(
                                mach_port_t task_port, 
                                char *nameCStr,
                                char *assertionCStr,
                                int level, 
                                int *assertion_id);

__private_extern__ void _TaskPortInvalidatedCallout(CFMachPortRef port, void *info);

__private_extern__ void _PMAssertionsDriverAssertionsHaveChanged(uint32_t changedDriverAssertions);

__private_extern__ void _ProxyAssertions(const struct IOPMSystemCapabilityChangeParameters *capArgs);


__private_extern__ void PMAssertions_TurnOffAssertions_ApplePushServiceTask(void);

__private_extern__ IOReturn InternalDeclareUserActive(
                                CFStringRef properties,
                                IOPMAssertionID *outID);
__private_extern__ IOReturn InternalCreateAssertion(
                                CFMutableDictionaryRef properties, 
                                IOPMAssertionID *outID);

__private_extern__ void InternalReleaseAssertion(
                                IOPMAssertionID *outID);
__private_extern__ IOReturn InternalReleaseAssertionSync(IOPMAssertionID outID);
__private_extern__ IOReturn 
InternalCreateAssertionWithTimeout(CFStringRef type, CFStringRef name, int timerSecs, IOPMAssertionID *outID);
__private_extern__ IOReturn InternalSetAssertionTimeout(IOPMAssertionID id, CFTimeInterval timeout);

__private_extern__ void InternalEvaluateAssertions(void);

__private_extern__ void evalAllUserActivityAssertions(unsigned int dispSlpTimer);
__private_extern__ void evalAllNetworkAccessAssertions(void);

__private_extern__ CFMutableDictionaryRef	_IOPMAssertionDescriptionCreate(
                            CFStringRef AssertionType, 
                            CFStringRef Name, 
                            CFStringRef Details,
                            CFStringRef HumanReadableReason,
                            CFStringRef LocalizationBundlePath,
                            CFTimeInterval Timeout,
                            CFStringRef TimeoutBehavior);


__private_extern__ CFStringRef processInfoGetName(pid_t p);
__private_extern__ void setSleepServicesTimeCap(uint32_t  timeoutInMS);
__private_extern__ bool systemBlockedInS0Dark(void);
__private_extern__ bool checkForActivesByType(kerAssertionType type);
__private_extern__ bool checkForEntriesByType(kerAssertionType type);
__private_extern__ bool checkForActivesByEffect(kerAssertionEffect effectIdx);
__private_extern__ bool checkForAudioType(void);
__private_extern__ void disableAssertionType(kerAssertionType type);
__private_extern__ void enableAssertionType(kerAssertionType type);
__private_extern__ void applyToAssertionsSync(assertionType_t *assertType,
                                              listSelectType_t assertionListSelect,
                                              void (^performOnAssertion)(assertion_t *));
__private_extern__ void configAssertionType(kerAssertionType idx, bool initialConfig);
__private_extern__ void logAssertionEvent(assertLogAction assertionAction, assertion_t *assertion);
__private_extern__ uint8_t getAssertionLevel(kerAssertionType idx);
__private_extern__ void setAggregateLevel(kerAssertionType idx, uint8_t val);
__private_extern__ uint32_t getKerAssertionBits(void);
__private_extern__ void setAssertionActivityLog(int value);
__private_extern__ void setAssertionActivityAggregate(pid_t pid, int value);
__private_extern__ kern_return_t setReservePwrMode(int enable);
__private_extern__ void releaseStatsBufForDeadProcs(void);
__private_extern__ void sendActivityTickle (void);

__private_extern__ void logASLAllAssertions(void);
__private_extern__ IOReturn  copyAssertionActivityAggregate(CFDictionaryRef *data);

__private_extern__ void setClamshellSleepState(void);

__private_extern__ int getClamshellSleepState(void);

__private_extern__ void disableClamshellSleepState(void);

__private_extern__ void delayDisplayTurnOff(void);

void asyncAssertionCreate(xpc_object_t remoteConnection, xpc_object_t msg);
void asyncAssertionRelease(xpc_object_t remoteConnection, xpc_object_t msg);
void asyncAssertionProperties(xpc_object_t remoteConnection, xpc_object_t msg);
void releaseConnectionAssertions(xpc_object_t remoteConnection);
void checkForAsyncAssertions(void *acknowledgementToken);
void handleAssertionSuspend(pid_t pid);
void handleAssertionResume(pid_t pid);
void processSetAssertionState(xpc_connection_t peer, xpc_object_t msg);
void logAssertionCount(bool displayOn);


__private_extern__ void logASLAssertionTypeSummary( kerAssertionType type);



#endif
