/*
 * Copyright (c) 2019 Apple Computer, Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#include <Availability.h>

// Hack out availability macros...
#import <SkyLight/SkyLight.h>
#undef SL_AVAILABLE_STARTING
#define SL_AVAILABLE_STARTING(_mac) __AVAILABILITY_INTERNAL__MAC_10_15

#import <SkyLight/SLSDisplayControlClient.h>
#import <SkyLight/SLSDisplayPowerControl.h>

#include <libproc.h>
#include "PMDisplay.h"
#include "SystemLoad.h"
#include "PMConnection.h"

#include "PMAssertions.h"

os_log_t display_log = NULL;
#undef  LOG_STREAM
#define LOG_STREAM  display_log

bool gSLCheckIn = false;
bool gDesktopMode = false;
bool gDisplayOn = false;
pid_t gSLPid = -1;
dispatch_source_t gSLExit = 0;

enum {
    kPMClamshellOpen          = 1,
    kPMClamshellClosed        = 2,
    kPMClamshellUnknown       = 3,
    kPMClamshellDoesNotExist  = 4
};


uint32_t gClamshellState = kPMClamshellUnknown;
IOPMAssertionID gDisplayOnAssertionID = kIOPMNullAssertionID;
IOPMAssertionID gLidOpenAssertionID = kIOPMNullAssertionID;
CFMutableDictionaryRef    gAssertionDescription = NULL;
SLSDisplayPowerControlClient *gSLPowerClient = nil;

struct request_entry {
    SLSDisplayControlRequestUUID uuid;
    bool valid;
    STAILQ_ENTRY(request_entry) entries;
};

SLSDisplayControlRequestUUID gDimRequest = 0;

#if XCTEST
void xctSetDesktopMode(bool value) {
    gDesktopMode = value;
}

void xctSetClamshellState(uint32_t state) {
    gClamshellState = state;
}

uint32_t xctGetClamshellState() {
    return gClamshellState;
}
#endif

// list for storing display requests to WindowServer
STAILQ_HEAD( , request_entry) gRequestUUIDs = STAILQ_HEAD_INITIALIZER(gRequestUUIDs);

__private_extern__ void dimDisplay()
{
    requestDisplayState(kDisplaysDim, -1);
}

__private_extern__ void unblankDisplay()
{
    requestDisplayState(kDisplaysUnblank, -1);
}

__private_extern__ void blankDisplay()
{
    // turn off display with dim timer set to 0
    // called during software sleep
    requestDisplayState(kDisplaysDim, 0);
}

__private_extern__ bool canSustainFullWake()
{
    if (gClamshellState == kPMClamshellClosed) {
        if (_getPowerSource() != kACPowered && !(getClamshellSleepState() & kClamshellDisableAssertions)) {
            // device is on battery and no assertions preventing clamshell sleep
            return false;
        }
    }
    return true;
}

#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
__private_extern__ void updateDesktopMode(xpc_object_t connection, xpc_object_t msg) 
{
    // set DesktopMode
    if (!connection || !msg) {
        ERROR_LOG("Invalid args for DesktopMode update(%p, %p)\n", connection, msg);
        return;
    }
    if (!xpcConnectionHasEntitlement(connection, kIOPMDisplayServiceEntitlement)) {
        ERROR_LOG("Not entitled to update desktopmode");
        return;
    }

    bool desktop_mode = xpc_dictionary_get_bool(msg, kDesktopModeKey);
    INFO_LOG("DesktopMode set %u\n", desktop_mode);
    gDesktopMode = desktop_mode;

    dispatch_async(_getPMMainQueue(), ^{handleDesktopMode();
                                                });

}

__private_extern__ void skylightCheckIn(xpc_object_t connection, xpc_object_t msg)
{
    // set up logging
    display_log = os_log_create(PM_LOG_SYSTEM, DISPLAY_LOG);

    if (!connection || !msg) {
        ERROR_LOG("Invalid args for WindowServer check in update(%p, %p)\n", connection, msg);
        return;
    }
    
    if (!xpcConnectionHasEntitlement(connection, kIOPMDisplayServiceEntitlement)) {
        ERROR_LOG("Not entitled to call WindowServer checkin");
        return;
    }

    // skylight checked in
    if (xpc_dictionary_get_bool(msg, kSkylightCheckInKey)) {
        gSLPid = xpc_connection_get_pid(connection);
        INFO_LOG("WindowServer checked in with pid %u\n", gSLPid);
        if (gSLCheckIn == false) {
            gSLCheckIn = true;
            dispatch_async(_getPMMainQueue(), ^{handleSkylightCheckIn();
                                                });
        } else {
            INFO_LOG("Redundant check in from WindowServer");
        }
    } else {
        gSLCheckIn = false;
        gSLPid = -1;
        INFO_LOG("WindowServer checked out\n");
    }
}
#endif

__private_extern__ bool skylightDisplayOn()
{
    return gDisplayOn;
}

__private_extern__ bool isDesktopMode()
{
    return gDesktopMode;
}

__private_extern__ void evaluateClamshellSleepState()
{
    dispatch_async(_getPMMainQueue(), ^{
        setClamshellSleepState();
    });
}

__private_extern__ void updateClamshellState(void *message)
{
    uint32_t closed = (uint32_t)message & kClamshellStateBit;
    uint32_t new_state = closed ? kPMClamshellClosed : kPMClamshellOpen;
    if (gClamshellState != new_state) {
        gClamshellState = new_state;
        // update WindowServer
        INFO_LOG("Sending clamshell state closed: %u to WindowServer", closed);
        SLSClamshellState sls_state = kClamshellNotPresent;
        if (new_state == kPMClamshellClosed) {
            sls_state = kClamshellClosed;
        } else if (new_state == kPMClamshellOpen) {
            sls_state = kClamshellOpen;
        }
        requestClamshellState(sls_state);
        if (new_state == kPMClamshellOpen) {
            // clamshell open. send activity tickle to rootDomain
            CFStringRef description = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("com.apple.powermanagement.lidopen"));
            InternalDeclareUserActive(description, &gLidOpenAssertionID);
        }
    }
    INFO_LOG("ClamshellState. Closed : %u. ClamshellSleepState: isSleepDisabled : %d\n", closed, getClamshellSleepState());
}

__private_extern__ uint64_t inFlightDimRequest()
{
    return gDimRequest;
}

__private_extern__ void resetDisplayState()
{
    if (gDisplayOn) {
        INFO_LOG("Resetting display state to off on sleep");
        displayStateDidChange(kDisplaysUnpowered);
    }
}

#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
void handleDesktopMode()
{
    INFO_LOG("EvaluateClamshellSleepState on DesktopMode update");
    evaluateClamshellSleepState();
}
#endif

void validateRequestUUID(uint64_t uuid)
{
    // Called only for display state changes
    bool found = false;
    struct request_entry *entry;
    STAILQ_FOREACH(entry, &gRequestUUIDs, entries){
        if (entry->uuid == uuid) {
            found = true;
            break;
        } else {
            // When does an entry become invalid
            /* When powerd requests displays dim followed by a displays undim
             * before the dim timer expires, we will never get an ack for the
             * first request.
             * Since requests are inserted in order any pending request before
             * the current request becomes invalid
             */
            entry->valid = false;
        }
    }
    if (found) {
        if (!entry->valid) {
            INFO_LOG("Received callback for invalid uuid %llu\n", uuid);
        } else {
            INFO_LOG("Received callback for uuid %llu\n", uuid);
        }
        STAILQ_REMOVE(&gRequestUUIDs, entry, request_entry, entries);
        free(entry);
    } else {
        ERROR_LOG("Received callback for unknown uuid %llu", uuid);
    }
}

void handleSkylightNotification(void *dict)
{
    /* Call back received from WindowServer
     The call back is dictionary containing the following keys
     1. kSLSDisplayControlNotificationUUID - UUID for this call back. Can be zero
     2. kSLSDisplayControlNotificationType - notification type.
     3. kSLSDisplayControlNotificationPayload - payload which contains display state and other information
     4. kSLSDisplayControlNotificationPayloadType - type of the payload data blob
     */

    NSDictionary *ns_dict = (__bridge NSDictionary *)dict;
    if (!ns_dict) {
        ERROR_LOG("Invalid call back data");
        return;
    }

    uint64_t uuid = [ns_dict[kSLSDisplayControlNotificationUUID] longLongValue];
    INFO_LOG("Received WindowServer call back %{public}@ for uuid %llu", ns_dict, uuid);
    if (uuid == gDimRequest) {
        // Our in flight dim request is complete. Let's clear the value
        gDimRequest = 0;
    }

    // notification type. Always present
    SLSDisplayControlNotificationType type = [ns_dict[kSLSDisplayControlNotificationType] longLongValue];
    
    // payload information
    SLSDisplayControlNotificationPayloadType payload_type = kSLDCNotificationPayloadTypeNone;
    payload_type = [ns_dict[kSLSDisplayControlNotificationPayloadType] longLongValue];
    
    switch (type) {
        case kSLDCNotificationTypeConnectionEstablished:
            // initial connection established
            INFO_LOG("Connection initialized");

            // send initial clamshell state to WindowServer
            SLSClamshellState sls_state = kClamshellNotPresent;
            if (gClamshellState == kPMClamshellClosed) {
                sls_state = kClamshellClosed;
            } else if (gClamshellState == kPMClamshellOpen) {
                sls_state = kClamshellOpen;
            }
            INFO_LOG("Sending initial clamshell state %d to WindowServer\n", gClamshellState);
            requestClamshellState(sls_state);
            break;
            
        case kSLDCNotificationTypeStateUpdate:
            // initial state
            INFO_LOG("Initial state update");
            if (payload_type == kSLDCNotificationPayloadTypeArray) {
                // payload. The second element is the logical display state
                NSArray *payload = (NSArray *)ns_dict[kSLSDisplayControlNotificationPayload];
                if (payload) {
                    // iterate through payload to find LogcalDisplayState
                    int i = 0;
                    for (i = 0; i < [payload count]; i++) {
                        if (payload[i][kSLSDisplayControlNotificationLogicalDisplaysState]) {
                            uint64_t state = [payload[i][kSLSDisplayControlNotificationLogicalDisplaysState] longLongValue];
                            displayStateDidChange(state);
                        }
                    }
                }
            }
            break;
        case kSLDCNotificationTypeError:
            // error
            if (payload_type == kSLDCNotificationPayloadTypeDictionary) {
                // error dictionary
                NSDictionary *error = (NSDictionary *)ns_dict[kSLSDisplayControlNotificationPayload];
                int error_code = [error[kSLSDisplayControlNotificationErrorCode] intValue];
                NSString *error_desc = error[kSLSDisplayControlNotificationErrorDescription];
                ERROR_LOG("Display request %llu failed with error code %d description: %@", uuid, error_code, error_desc);
            }
            break;
            
        case kSLDCNotificationTypeXPCError:
            // xpc error. Error details are directly in the call back dictionary
            {
                int error_code = [ns_dict[kSLSDisplayControlNotificationErrorCode] intValue];
                NSString *error_desc = ns_dict[kSLSDisplayControlNotificationErrorDescription];
                ERROR_LOG("Display request %llu failed with xpc erro code %d description: %@", uuid, error_code, error_desc);
            }
            break;
            
        case kSLDCNotificationTypeRequestCompleted:
            // acks for our requests
            if (payload_type == kSLDCNotificationPayloadTypeDictionary) {
                // ack dictionary
                NSDictionary *payload = (NSDictionary *)ns_dict[kSLSDisplayControlNotificationPayload];
                // display state
                uint64_t state = [payload[kSLSDisplayControlNotificationState] longLongValue];
                displayStateDidChange(state);
                validateRequestUUID(uuid);
            }
            break;
            
        case kSLDCNotificationTypeStateBroadcast:
            // broadcast notification. We dont need it. Just log it
            INFO_LOG("Recevied broadcast notification");
            break;
            
        case kSLDCNotificationTypeIdleRequest:
            // request idle forwarded from gui clients (LoginWindow, Dock ..)
            if (payload_type == kSLDCNotificationPayloadTypeDictionary) {
                NSDictionary *payload = (NSDictionary *)ns_dict[kSLSDisplayControlNotificationPayload];
                pid_t pid = [payload[kSLSDisplayControlNotificationIdleRequest] intValue];
                char p_name[kProcNameBufLen];
                if (proc_name(pid, p_name, sizeof(p_name)) == 0) {
                    INFO_LOG("Process %d is requesting display idle", pid);
                } else {
                    INFO_LOG("Process (%d, %s) is requesting display idle", pid, p_name);
                }
                requestDisplayState(kDisplaysDim, 0);
            }
            break;
            
        default:
            break;
    }
}

#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
void handleSkylightCheckIn()
{
    // monitor gSLPid for process exit
    if (!gSLExit) {
        gSLExit = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, gSLPid, DISPATCH_PROC_EXIT, _getPMMainQueue());
        dispatch_source_set_event_handler(gSLExit, ^{
                                        gSLCheckIn = false;
                                        gSLPid = -1;
                                        ERROR_LOG("WindowServer exited");
                                        dispatch_release(gSLExit);
                                        gSLExit = 0;
        });
    }
    dispatch_resume(gSLExit);

    // create ws power control client
    NSError *err = nil;
    gSLPowerClient = [[SLSDisplayPowerControlClient alloc] initPowerControlClient:&err notifyQueue:_getPMMainQueue() notificationType:kSLDCNotificationTypeNone notificationBlock:^(void *dict) {
        if (dict != nil) {
            handleSkylightNotification(dict);
        } else {
            ERROR_LOG("Received a nil dictionary from WindowServer callback");
        }
    }];

    if ([err code] != 0) {
        ERROR_LOG("Error registering with WindowServer %{public}@", err);
    } else {
        INFO_LOG("Registered for WindowServer callbacks");
    }
    if (gClamshellState == kPMClamshellUnknown) {
        // determine state
        gClamshellState = rootDomainClamshellState();
    }
    if (err) {
        [err release];
    }
    return;
}
#endif

void requestDisplayState(uint64_t state, int timeout)
{
    if (!gSLCheckIn) {
        ERROR_LOG("WindowServer has not checked in. Refusing to change display state");
        return;
    }
    /* This function always targets all displays.
     Clamshell state is requested through requestClamshellState*/
    
    NSError *err = nil;
    NSMutableDictionary *request = [[NSMutableDictionary alloc] initWithCapacity:1];
    NSNumber * ns_state = [[NSNumber alloc] initWithUnsignedLongLong:state];
    [request setValue:ns_state forKey:kSLSDisplayControlRequestState];

    // set timeout if present
    if (timeout != -1) {
        NSNumber * ns_timeout = [[NSNumber alloc] initWithInt:timeout];
        [request setValue:ns_timeout forKey:kSLSDisplayControlRequestTimeout];
        if (ns_timeout) {
            [ns_timeout release];
        }
    }

    SLSDisplayControlRequestUUID uuid = [gSLPowerClient requestStateChange:(NSDictionary *const)request error:&err];
    if ([err code] != 0) {
        ERROR_LOG("Display requestStateChange returned error %{public}@", err);
    } else {
        INFO_LOG("requestDisplayState: state %llu, Received uuid %llu", state, uuid);
        struct request_entry *entry = (struct request_entry *)malloc(sizeof(struct request_entry));
        entry->uuid = uuid;
        entry->valid = true;
        STAILQ_INSERT_TAIL(&gRequestUUIDs, entry, entries);
        if (state == kDisplaysDim) {
            gDimRequest = uuid;
        }
    }
    if (request) {
        [ns_state release];
        [request release];
    }
    if (err) {
        [err release];
    }
}

void requestClamshellState(SLSClamshellState state)
{
    /* Forward clamshell state to WindowServer
     A) a request with a clamshell state of close in interpreted as a turn off clamshell display (clamshell close)
     B) a request with a clamshell state of open in interpreted as a turn on internal and ANY external displays (clamshell open)
     */
    
    if (!gSLCheckIn) {
        ERROR_LOG("WindowServer has not checked in. Refusing to change clamshell display state");
        return;
    }

    NSError *err = nil;
    NSMutableDictionary *request = [[NSMutableDictionary alloc] initWithCapacity:1];
    NSNumber *ns_state = [[NSNumber alloc] initWithUnsignedChar:state];
    [request setValue:ns_state forKey:kSLSDisplayControlRequestClamshellState];
    SLSDisplayControlRequestUUID uuid = [gSLPowerClient requestStateChange:(NSDictionary *const)request error:&err];
    if ([err code] != 0) {
       ERROR_LOG("Clamshell requestStateChange returned error %{public}@", err);
    } else {
       INFO_LOG("requestClamshellState: state %u, Received uuid %llu", state, uuid);
        struct request_entry *entry = (struct request_entry *)malloc(sizeof(struct request_entry));
        entry->uuid = uuid;
        entry->valid = true;
        STAILQ_INSERT_TAIL(&gRequestUUIDs, entry, entries);
    }
    if (request) {
       [ns_state release];
       [request release];
    }
    if (err) {
        [err release];
    }
}

void createProxyAssertion()
{
    if (!gAssertionDescription) {
        gAssertionDescription = _IOPMAssertionDescriptionCreate(
                        kIOPMAssertionTypePreventUserIdleSystemSleep,
                        CFSTR("Powerd - Prevent sleep while display is on"),
                        NULL, NULL, NULL,
                        0, 0);
    }
    if (gDisplayOnAssertionID == kIOPMNullAssertionID) {
        INFO_LOG("Creating assertion to keep device awake while display is on\n");
        InternalCreateAssertion(gAssertionDescription, &gDisplayOnAssertionID);
    }
}

void releaseProxyAssertion()
{
    if (gDisplayOnAssertionID != kIOPMNullAssertionID) {
        INFO_LOG("Releasing prevent sleep while display is on\n");
        InternalReleaseAssertionSync(gDisplayOnAssertionID);
        gDisplayOnAssertionID = kIOPMNullAssertionID;
    }

}

void displayStateDidChange(uint64_t state)
{
    // display state changed
    bool display_on = false;
    if (state == kDisplaysPowered) {
        display_on = true;
    } else if(state == kDisplaysUnpowered) {
        display_on = false;
    }
    if (gDisplayOn == display_on) {
        // no change in state
        return;
    }
    gDisplayOn = display_on;
    SystemLoadDisplayPowerStateHasChanged(!gDisplayOn);
    logASLDisplayStateChange();


    if (gDisplayOn) {
        // temporary workaround to prevent unexpected sleep
        // until rdar://problem/51729848 & rdar://problem/50457533
        // Keep the device awake as long is display is on
        createProxyAssertion();
    } else {
        // Display is off. release  assertion
        releaseProxyAssertion();
    }
}

uint32_t rootDomainClamshellState()
{
    CFBooleanRef clamshell_state = NULL;
    uint32_t result = kPMClamshellUnknown;
    clamshell_state = (CFBooleanRef)_copyRootDomainProperty(CFSTR(kAppleClamshellStateKey));
    if (clamshell_state != NULL) {
        if (kCFBooleanTrue == clamshell_state) {
            result = kPMClamshellClosed;
        } else {
            result = kPMClamshellOpen;
        }
        CFRelease(clamshell_state);
    } else {
        result = kPMClamshellDoesNotExist;
    }
    INFO_LOG("rootDomain clamshell state %u\n", result);
    return result;
}

