/*
 * Copyright (c) 1999-2008 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_HID_IOHIDLIBPRIVATE_H
#define _IOKIT_HID_IOHIDLIBPRIVATE_H

#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDDevicePlugIn.h>
#include <IOKit/hid/IOHIDLibUserClient.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <Availability.h>
#include <os/log.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

typedef struct _IOHIDCalibrationStruct {
    CFIndex     satMin;
    CFIndex     satMax;
    CFIndex     dzMin;
    CFIndex     dzMax;
    CFIndex     min;
    CFIndex     max;
    double_t    gran;
} IOHIDCalibrationInfo;

typedef struct _IOHIDCallbackApplierContext {
    IOReturn                result;
    void *                  sender;
} IOHIDCallbackApplierContext;

typedef enum {
    kIOHIDLogCategoryDefault,
    kIOHIDLogCategoryTrace,
    kIOHIDLogCategoryProperty,
    kIOHIDLogCategoryActivity,
    kIOHIDLogCategoryCount
} IOHIDLogCategory;

extern uint32_t gIOHIDLogLevel;

#define kIOHIDLogSubsytem   "com.apple.iohid"

#define IOHIDLog(fmt, ...)        os_log(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)
#define IOHIDLogError(fmt, ...)   os_log_error(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)
#define IOHIDLogDebug(fmt, ...)   os_log_debug(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)

#define IOHIDLogTrace(fmt, ...) do {                                            \
    if (gIOHIDLogLevel >= kIOHIDLogLevelTypeTrace) {                            \
        os_log(_IOHIDLogCategory(kIOHIDLogCategoryTrace), fmt, ##__VA_ARGS__);  \
    }                                                                           \
} while (0)

extern void _IOObjectCFRelease(CFAllocatorRef _Nullable allocator, const void * value);

extern const void * _Nullable _IOObjectCFRetain(CFAllocatorRef _Nullable allocator, const void *value);

CF_EXPORT
IOHIDElementRef _IOHIDElementCreateWithParentAndData(CFAllocatorRef _Nullable allocator, IOHIDElementRef _Nullable parent, CFDataRef dataStore, IOHIDElementStruct * elementStruct, uint32_t index);

CF_EXPORT
void _IOHIDElementSetDevice(IOHIDElementRef element, IOHIDDeviceRef device);

CF_EXPORT
void _IOHIDElementSetDeviceInterface(IOHIDElementRef element, IOHIDDeviceDeviceInterface * _Nonnull * _Nonnull interface);

CF_EXPORT
uint32_t _IOHIDElementGetFlags(IOHIDElementRef element);

CF_EXPORT
CFIndex _IOHIDElementGetLength(IOHIDElementRef element);

CF_EXPORT
IOHIDValueRef _IOHIDElementGetValue(IOHIDElementRef element);

CF_EXPORT
IOHIDCalibrationInfo * _IOHIDElementGetCalibrationInfo(IOHIDElementRef element);

CF_EXPORT
void _IOHIDElementSetValue(IOHIDElementRef element, _Nullable IOHIDValueRef value);

CF_EXPORT
IOHIDValueRef _IOHIDValueCreateWithStruct(CFAllocatorRef _Nullable allocator, IOHIDElementRef element, IOHIDEventStruct * pEventStruct);

CF_EXPORT
IOHIDValueRef _IOHIDValueCreateWithElementValuePtr(CFAllocatorRef _Nullable allocator, IOHIDElementRef element, IOHIDElementValue * pEventValue);

CF_EXPORT
void _IOHIDValueCopyToElementValuePtr(IOHIDValueRef value, IOHIDElementValue * pElementValue);

CF_EXPORT
IOCFPlugInInterface * _Nonnull * _Nonnull _IOHIDDeviceGetIOCFPlugInInterface(
                                IOHIDDeviceRef                  device);

CF_EXPORT
CFArrayRef _Nullable _IOHIDQueueCopyElements(IOHIDQueueRef queue);

CF_EXPORT 
void _IOHIDCallbackApplier(const void * callback, const void * _Nullable callbackContext, void *applierContext);

CF_EXPORT
os_log_t _IOHIDLog(void);

CF_EXPORT
os_log_t _IOHIDLogCategory(IOHIDLogCategory category);

CF_EXPORT
kern_return_t IOHIDSetFixedMouseLocation(io_connect_t connect,
                                         int32_t x, int32_t y)                                      AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER;

CF_EXPORT
CFStringRef _IOHIDCreateTimeString(struct timeval *tv);

CF_EXPORT
uint64_t  _IOHIDGetMonotonicTime ();

CF_EXPORT
kern_return_t IOHIDSetCursorBounds( io_connect_t connect, const IOGBounds * bounds );

CF_EXPORT
kern_return_t IOHIDSetOnScreenCursorBounds( io_connect_t connect, const IOGPoint * point, const IOGBounds * bounds );


CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* _IOKIT_HID_IOHIDLIBPRIVATE_H */
