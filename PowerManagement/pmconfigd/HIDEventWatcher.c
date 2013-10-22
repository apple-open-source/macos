/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
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

#include <asl.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <libproc.h>
#include <bsm/libbsm.h>
#include "HIDEventWatcher.h"

static CFMutableArrayRef   gHIDEventHistory = NULL;

static const CFTimeInterval kFiveMinutesInSeconds   = (double)300.0;
static const int kMaxFiveMinutesWindowsCount        = 12;
static const int kMaxPIDRecorded                    = 10;

#define __NX_NULL_EVENT     0

/*
 * gHIDEventHistory = One big CFArrayRef
 *   full of CFDictionaries
 *       pid is at kIOPMHIDAppPIDKey
 *       path is at kIOPMHIDAppPathKey
 *       CFArray of buckets is at kIOPMHIDHistoryArrayKey
 *           Each bucket is a IOPMHIDPostEventActivityWindow
 */

__private_extern__ kern_return_t _io_pm_hid_event_report_activity(
    mach_port_t server,
    audit_token_t token,                                                        
    int         _action)
{
    pid_t                               callerPID;
    CFNumberRef                         appPID = NULL;
    int                                 _app_pid_;
    CFMutableDictionaryRef              foundDictionary = NULL;
    CFMutableArrayRef                   bucketsArray = NULL;
    CFDataRef                           dataEvent = NULL;
    IOPMHIDPostEventActivityWindow      *ev = NULL;
    CFAbsoluteTime                      timeNow = CFAbsoluteTimeGetCurrent();
    int                                 arrayCount = 0;
    int                                 i = 0;
    
    // Unwrapping big data structure...
    if (!gHIDEventHistory) {
        gHIDEventHistory = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks);
    }
    if (!gHIDEventHistory) {
        goto exit;
    }
    
    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

    if (0 !=(arrayCount = CFArrayGetCount(gHIDEventHistory)))
    {
        // Scan through the array to find an existing dictionary for the given pid
        for (i=0; i<arrayCount; i++)
        {
            CFMutableDictionaryRef dictionaryForApp = NULL;
            dictionaryForApp = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(gHIDEventHistory, i);
            if (!dictionaryForApp) {
                break;
            }
            appPID = CFDictionaryGetValue(dictionaryForApp, kIOPMHIDAppPIDKey);
            if (appPID) {
                CFNumberGetValue(appPID, kCFNumberIntType, &_app_pid_);
                if (callerPID == _app_pid_) {
                    foundDictionary = dictionaryForApp;
                    break;
                }
            }
        }
    }
    
    // Unwrapping big data structure...
    if (!foundDictionary) {
        foundDictionary = CFDictionaryCreateMutable(0, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFArrayAppendValue(gHIDEventHistory, foundDictionary);
        CFRelease(foundDictionary);
        
        if (CFArrayGetCount(gHIDEventHistory) > kMaxPIDRecorded) {
            // Limit number of PID's tracked at one time.
            CFArrayRemoveValueAtIndex(gHIDEventHistory, 0);
        }
        
        /* Tag our pid */
        appPID = CFNumberCreate(0, kCFNumberIntType, &callerPID);
        if (appPID) {
            CFDictionarySetValue(foundDictionary, kIOPMHIDAppPIDKey, appPID);
            CFRelease(appPID);
        }

        /* Tag the process name */
        CFStringRef appName = NULL;
        char    appBuf[MAXPATHLEN];
        int     len = proc_name(callerPID, appBuf, MAXPATHLEN);
        if (0 != len) {
            appName = CFStringCreateWithCString(0, appBuf, kCFStringEncodingMacRoman);
            if (appName) {
                CFDictionarySetValue(foundDictionary, kIOPMHIDAppPathKey, appName);
                CFRelease(appName);
            }
        }
    }

    if (!foundDictionary)
        goto exit;

    // Unwrapping big data structure...
    bucketsArray = (CFMutableArrayRef)CFDictionaryGetValue(foundDictionary, kIOPMHIDHistoryArrayKey);
    if (!bucketsArray) {
        bucketsArray = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks);
        CFDictionarySetValue(foundDictionary, kIOPMHIDHistoryArrayKey, bucketsArray);
        CFRelease(bucketsArray);
    }
    if (!bucketsArray) 
        goto exit;

    // Check last HID event bucket timestamp - is it more than 5 minutes old?
    bool foundWindowForEvent = false;
    if (0 < CFArrayGetCount(bucketsArray)) {
        dataEvent = (CFDataRef)CFArrayGetValueAtIndex(bucketsArray, 0);
    }
    if (dataEvent) {
        ev = (IOPMHIDPostEventActivityWindow *)CFDataGetBytePtr(dataEvent);
        if (timeNow < (ev->eventWindowStart + kFiveMinutesInSeconds))
        {
            // This HID event gets dropped into this existing 5 minute bucket.
            // We bump the count for HID activity!
            if (__NX_NULL_EVENT == _action) {
                ev->nullEventCount++;
            } else {
                ev->hidEventCount++;
            }
            foundWindowForEvent = true;
        }
    }

    if (!foundWindowForEvent) {
        IOPMHIDPostEventActivityWindow  newv;

        // We align the starts of our windows with 5 minute intervals
        newv.eventWindowStart = ((int)timeNow / (int)kFiveMinutesInSeconds) * kFiveMinutesInSeconds;
        newv.nullEventCount = newv.hidEventCount = 0;
        if (__NX_NULL_EVENT == _action) {
            newv.nullEventCount++;
        } else {
            newv.hidEventCount++;
        }
    
        dataEvent = CFDataCreate(0, (const UInt8 *)&newv, sizeof(IOPMHIDPostEventActivityWindow));
        if (dataEvent) {
            CFArrayInsertValueAtIndex(bucketsArray, 0, dataEvent);
            CFRelease(dataEvent);
        }
        
        // If we've recorded more than kMaxFiveMinutesWindowsCount activity windows for this process, delete the old ones.
        while (kMaxFiveMinutesWindowsCount < CFArrayGetCount(bucketsArray)) {
            CFArrayRemoveValueAtIndex(bucketsArray, CFArrayGetCount(bucketsArray) - 1);
        }
        
    }

exit:
    return KERN_SUCCESS;
}

__private_extern__ kern_return_t _io_pm_hid_event_copy_history(
            mach_port_t     server,
            vm_offset_t     *array_data,
            mach_msg_type_number_t  *array_dataLen,
            int             *return_val)
{
    CFDataRef   sendData = NULL;

    sendData = CFPropertyListCreateData(0, gHIDEventHistory, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    if (!sendData) {
        *return_val = kIOReturnError;
        goto exit;
    }

    *array_data = (vm_offset_t)CFDataGetBytePtr(sendData);
    *array_dataLen = (size_t)CFDataGetLength(sendData);
    vm_allocate(mach_task_self(), (vm_address_t *)array_data, *array_dataLen, TRUE);
    if (*array_data) {
        memcpy((void *)*array_data, CFDataGetBytePtr(sendData), *array_dataLen);
        *return_val = kIOReturnSuccess;
    }

    CFRelease(sendData);

    *return_val = kIOReturnSuccess;
exit:
    return KERN_SUCCESS;
}
