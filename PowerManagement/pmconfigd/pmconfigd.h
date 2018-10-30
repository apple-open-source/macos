/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
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
//  Created by Mahdi Hamzeh on 1/31/17.

#ifndef pmconfigd_h
#define pmconfigd_h

#include <CoreFoundation/CoreFoundation.h>

#include <syslog.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <notify.h>
#include <asl.h>
#include <uuid/uuid.h>
#include <pthread.h>
#include <bsm/libbsm.h>
#include <sys/sysctl.h>
#include <xpc/private.h>
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#include <AssertMacros.h>
#include <spawn.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/AppleHIDUsageTables.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>

#include <Security/SecTask.h>
#include <os/log.h>

#include <System/sys/kdebug.h>

// TODO: remove these once we have:
// <rdar://problem/33055720> daemon powerd subclass (DBG_DAEMON_POWERD)
#ifndef DBG_DAEMON_POWERD
#define DBG_DAEMON_POWERD 0x2
#endif
#ifndef POWERDBG_CODE
#define POWERDBG_CODE(code) DAEMONDBG_CODE(DBG_DAEMON_POWERD, code)
#endif

#define POWERD_CLWK_CODE 0x1


#include "powermanagementServer.h" // mig generated

#include "PMStore.h"
#include "PMSettings.h"
#include "UPSLowPower.h"
#include "BatteryTimeRemaining.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "PMAssertions.h"
#include "TTYKeepAwake.h"
#include "PMSystemEvents.h"
#include "SystemLoad.h"
#include "PMConnection.h"
#include "ExternalMedia.h"
#include "Platform.h"
#include "StandbyTimer.h"
#include "PrivateLib.h"


#include "adaptiveDisplay.h"

// To support importance donation across IPCs
#include <libproc_internal.h>


#define kIOPMAppName        "Power Management configd plugin"
#define kIOPMPrefsPath      "com.apple.PowerManagement.xml"
#define pwrLogDirName       "/System/Library/PowerEvents"

#ifndef kIOUPSDeviceKey
// Also defined in ioupsd/IOUPSPrivate.h
#define kIOUPSDeviceKey             "UPSDevice"
#define kIOPowerDeviceUsageKey      0x84
#define kIOBatterySystemUsageKey    0x85
#endif

/*
 * BSD notifications from loginwindow indicating shutdown
 */
// kLWShutdownInitiated
//   User clicked shutdown: may be aborted later
#define kLWShutdowntInitiated    "com.apple.system.loginwindow.shutdownInitiated"

// kLWRestartInitiated
//   User clicked restart: may be aborted later
#define kLWRestartInitiated     "com.apple.system.loginwindow.restartinitiated"

// kLWLogoutCancelled
//   A previously initiated shutdown, restart, or logout, has been cancelled.
#define kLWLogoutCancelled      "com.apple.system.loginwindow.logoutcancelled"

// kLWLogoutPointOfNoReturn
//   A previously initiated shutdown, restart, or logout has succeeded, and is
//   no longer abortable by anyone. Point of no return!
#define kLWLogoutPointOfNoReturn    "com.apple.system.loginwindow.logoutNoReturn"

// kLWSULogoutInitiated
//   Loginwindow is beginning a sequence of 1. logout, 2. software update, 3. then restart.
#define kLWSULogoutInitiated     "com.apple.system.loginwindow.sulogoutinitiated"

#define kDWTMsgHandlerDelay         10  // Time(in secs) for which DW Thermal msg handler is delayed

#define LogObjectRetainCount(x, y) do {} while(0)
/* #define LogObjectRetainCount(x, y) do { \
 asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: kernel retain = %d, user retain = %d\n", \
 x, IOObjectGetKernelRetainCount(y), IOObjectGetUserRetainCount(y)); } while(0)
 */
#define kSpindumpIOKitDir      	    "/Library/Logs/IOKit"

enum {
    kWranglerPowerStateMin   = 0,
    kWranglerPowerStateSleep = 2,
    kWranglerPowerStateDim   = 3,
    kWranglerPowerStateMax   = 4
};


typedef struct {
    int  shutdown;
    int  restart;
    int  cancel;
    int  pointofnoreturn;
    int  su;
} LoginWindowNotifyTokens;

// defined by MiG
extern boolean_t powermanagement_server(mach_msg_header_t *, mach_msg_header_t *);
extern uint32_t  gDebugFlags;


bool isDisplayAsleep( );

kern_return_t _io_pm_last_wake_time(
                                    mach_port_t             server,
                                    vm_offset_t             *out_wake_data,
                                    mach_msg_type_number_t  *out_wake_len,
                                    vm_offset_t             *out_delta_data,
                                    mach_msg_type_number_t  *out_delta_len,
                                    int                     *return_val);




// Callback is registered in PrivateLib.c
__private_extern__ void dynamicStoreNotifyCallBack(
                                                   SCDynamicStoreRef   store,
                                                   CFArrayRef          changedKeys,
                                                   void                *info);

__private_extern__ void ioregBatteryProcess(IOPMBattery *changed_batt,
                                            io_service_t batt);
#endif /* pmconfigd_h */
