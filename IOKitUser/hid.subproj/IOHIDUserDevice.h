/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_HID_IOHIDUSERDEVICE_USER_H
#define _IOKIT_HID_IOHIDUSERDEVICE_USER_H

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>

__BEGIN_DECLS

typedef struct __IOHIDUserDevice * IOHIDUserDeviceRef;


typedef IOReturn (*IOHIDUserDeviceReportCallback)(void * refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength);
typedef IOReturn (*IOHIDUserDeviceHandleReportAsyncCallback)(void * refcon, IOReturn result);

/*!
	@function   IOHIDUserDeviceGetTypeID
	@abstract   Returns the type identifier of all IOHIDUserDevice instances.
*/
CF_EXPORT
CFTypeID IOHIDUserDeviceGetTypeID(void);

/*!
	@function   IOHIDUserDeviceCreate
	@abstract   Creates an virtual IOHIDDevice in the kernel.
    @discussion The io_service_t passed in this method must reference an object 
                in the kernel of type IOHIDUserDevice.
    @param      allocator Allocator to be used during creation.
    @param      properties CFDictionaryRef containing device properties index by keys defined in IOHIDKeys.h.
    @result     Returns a new IOHIDUserDeviceRef.
*/
CF_EXPORT
IOHIDUserDeviceRef IOHIDUserDeviceCreate(CFAllocatorRef allocator, CFDictionaryRef properties);

/*!
	@function   IOHIDUserDeviceScheduleWithRunLoop
	@abstract   Schedules the IOHIDUserDevice with a run loop
    @discussion This is necessary to receive asynchronous events from the kernel
    @param      device Reference to IOHIDUserDevice 
    @param      runLoop Run loop to be scheduled with
    @param      runLoopMode Run loop mode to be scheduled with
*/
CF_EXPORT
void IOHIDUserDeviceScheduleWithRunLoop(IOHIDUserDeviceRef device, CFRunLoopRef runLoop, CFStringRef runLoopMode);

/*!
	@function   IOHIDUserDeviceUnscheduleFromRunLoop
	@abstract   Unschedules the IOHIDUserDevice from a run loop
    @param      device Reference to IOHIDUserDevice 
    @param      runLoop Run loop to be scheduled with
    @param      runLoopMode Run loop mode to be scheduled with
*/
CF_EXPORT
void IOHIDUserDeviceUnscheduleFromRunLoop(IOHIDUserDeviceRef device, CFRunLoopRef runLoop, CFStringRef runLoopMode);

/*!
	@function   IOHIDUserDeviceRegisterGetReportCallback
	@abstract   Register a callback to receive get report requests
    @param      device Reference to IOHIDUserDevice 
    @param      callback Callback to be used
    @param      refcon pointer to a reference object of your choosing
*/
CF_EXPORT
void IOHIDUserDeviceRegisterGetReportCallback(IOHIDUserDeviceRef device, IOHIDUserDeviceReportCallback callback, void * refcon);

/*!
	@function   IOHIDUserDeviceRegisterSetReportCallback
	@abstract   Register a callback to receive set report requests
    @param      device Reference to IOHIDUserDevice 
    @param      callback Callback to be used
    @param      refcon pointer to a reference object of your choosing
*/
CF_EXPORT
void IOHIDUserDeviceRegisterSetReportCallback(IOHIDUserDeviceRef device, IOHIDUserDeviceReportCallback callback, void * refcon);

/*!
	@function   IOHIDUserDeviceHandleReport
	@abstract   Dispatch a report to the IOHIDUserDevice.
    @param      device Reference to IOHIDUserDevice 
    @param      report Buffer containing formated report being issued to HID stack
    @param      reportLength Report buffer length
    @result     Returns kIOReturnSuccess when report is handled successfully.
*/
CF_EXPORT
IOReturn IOHIDUserDeviceHandleReport(IOHIDUserDeviceRef device, uint8_t * report, CFIndex reportLength);

/*!
 @function   IOHIDUserDeviceHandleReportAsync
 @abstract   Dispatch a report to the IOHIDUserDevice.
 @param      device Reference to IOHIDUserDevice 
 @param      report Buffer containing formated report being issued to HID stack
 @param      reportLength Report buffer length
 @param      callback Callback to be used (optional)
 @param      refcon pointer to a reference object of your choosing (optional)
 @result     Returns kIOReturnSuccess when report is handled successfully.
 */
CF_EXPORT
IOReturn IOHIDUserDeviceHandleReportAsync(IOHIDUserDeviceRef device, uint8_t *report, CFIndex reportLength, IOHIDUserDeviceHandleReportAsyncCallback callback, void * refcon);

__END_DECLS

#endif /* _IOKIT_HID_IOHIDUSERDEVICE_USER_H */
