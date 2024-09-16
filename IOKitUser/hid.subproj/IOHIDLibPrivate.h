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
#include "IOHIDEvent.h"
#include <CoreFoundation/CFRuntime.h>

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
    kIOHIDDispatchStateInactive     = 0,
    kIOHIDDispatchStateActive       = 1 << 0,
    kIOHIDDispatchStateCancelled    = 1 << 1
} IOHIDDispatchState;

typedef enum {
    kIOHIDLogCategoryDefault,
    kIOHIDLogCategoryTrace,
    kIOHIDLogCategoryProperty,
    kIOHIDLogCategoryActivity,
    kIOHIDLogCategoryFastPath,
    kIOHIDLogCategoryUserDevice,
    kIOHIDLogCategoryService,
    kIOHIDLogCategoryServicePlugin,
    kIOHIDServiceLogCategoryCarplay,
    kIOHIDLogCategoryConnection,
    kIOHIDLogCategoryCursor,
    kIOHIDLogCategorySignpost,
    kIOHIDLogCategoryUPS,
    kIOHIDLogCategoryClient,
    kIOHIDLogCategoryOversized,
    kIOHIDLogCategoryCount
} IOHIDLogCategory;


extern uint32_t gIOHIDDebugConfig;

#define kIOHIDLogSubsytem   "com.apple.iohid"

#define IOHIDLog(fmt, ...)            os_log(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)
#define IOHIDLogInfo(fmt, ...)        os_log_info(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)
#define IOHIDLogError(fmt, ...)       os_log_error(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)
#define IOHIDLogDebug(fmt, ...)       os_log_debug(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)
#define IOHIDLogInfo(fmt, ...)        os_log_info(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)
#define IOHIDLogFault(fmt, ...)       os_log_fault(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt, ##__VA_ARGS__)

#define HIDLog(fmt, ...)              os_log(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)
#define HIDLogError(fmt, ...)         os_log_error(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)
#define HIDLogDebug(fmt, ...)         os_log_debug(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)
#define HIDLogInfo(fmt, ...)          os_log_info(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)
#define HIDLogFault(fmt, ...)         os_log_fault(_IOHIDLogCategory(kIOHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)

#define UPSLog(fmt, ...)              os_log(_IOHIDLogCategory(kIOHIDLogCategoryUPS), fmt, ##__VA_ARGS__)
#define UPSLogError(fmt, ...)         os_log_error(_IOHIDLogCategory(kIOHIDLogCategoryUPS), fmt, ##__VA_ARGS__)
#define UPSLogDebug(fmt, ...)         os_log_debug(_IOHIDLogCategory(kIOHIDLogCategoryUPS), fmt, ##__VA_ARGS__)

#define HIDOversizedLog(fmt, ...)     os_log(_IOHIDLogCategory(kIOHIDLogCategoryOversized), fmt, ##__VA_ARGS__)
#define HIDOversizedLogInfo(fmt, ...) os_log_info(_IOHIDLogCategory(kIOHIDLogCategoryOversized), fmt, ##__VA_ARGS__)

#define HIDLogActivityDebug(fmt, ...) os_log_debug(_IOHIDLogCategory(kIOHIDLogCategoryActivity), fmt, ##__VA_ARGS__)

// Creates a function that is properly typecasted for objc_msgSend. Takes return
// type, function name, and a list of function arguments.
#define TYPECAST_MSGSEND(returnType, funcName, ...) \
    static returnType (*funcName)(id, SEL, ##__VA_ARGS__) = (returnType (*)(id, SEL, ##__VA_ARGS__))objc_msgSend;

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
void _IOHIDValueCopyToElementValueHeader(IOHIDValueRef value, IOHIDElementValueHeader * pElementHeader);

CF_EXPORT
IOHIDValueRef _Nullable _IOHIDValueCreateWithValue(CFAllocatorRef _Nullable allocator, IOHIDValueRef value, IOHIDElementRef element);

CF_EXPORT
uint8_t _IOHIDValueGetFlags(IOHIDValueRef value);

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
                                         int32_t x, int32_t y) __attribute__((availability(macos,introduced=10.7,deprecated=10.16)));

CF_EXPORT
kern_return_t IOHIDSetFixedMouseLocationWithTimeStamp(io_connect_t connect, int32_t x, int32_t y, uint64_t timestamp)  __attribute__((availability(macos,introduced=10.15,deprecated=10.16)));

CF_EXPORT
CFStringRef _IOHIDCreateTimeString(CFAllocatorRef _Nullable allocator, struct timeval *tv);

CF_EXPORT
uint64_t  _IOHIDGetMonotonicTime (void);

CF_EXPORT
uint64_t _IOHIDGetTimestampDelta(uint64_t timestampA, uint64_t timestampB, uint32_t scaleFactor);

CF_EXPORT
kern_return_t IOHIDSetCursorBounds( io_connect_t connect, const IOGBounds * bounds ) __attribute__((availability(macos,deprecated=10.16)));

CF_EXPORT
kern_return_t IOHIDSetOnScreenCursorBounds( io_connect_t connect, const IOGPoint * point, const IOGBounds * bounds ) __attribute__((availability(macos,deprecated=10.16)));

CF_EXPORT
void _IOHIDDebugTrace(uint32_t code, uint32_t func, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

void _IOHIDDebugEventAddPerfData(IOHIDEventRef event, int timepoint, uint64_t timestamp);


typedef CFDataRef IOHIDSimpleQueueRef;

typedef void (^IOHIDSimpleQueueBlock) (void * entry, void * _Nullable ctx);

CF_EXPORT
IOHIDSimpleQueueRef _IOHIDSimpleQueueCreate (CFAllocatorRef _Nullable allocator, size_t entrySize, size_t count);

CF_EXPORT
IOReturn  _IOHIDSimpleQueueEnqueue (IOHIDSimpleQueueRef buffer, void * entry, boolean_t doOverride);

CF_EXPORT
boolean_t  _IOHIDSimpleQueueDequeue (IOHIDSimpleQueueRef buffer, void * _Nullable entry);

CF_EXPORT
void * _Nullable _IOHIDSimpleQueuePeek (IOHIDSimpleQueueRef buffer);

CF_EXPORT
void _IOHIDSimpleQueueApplyBlock (IOHIDSimpleQueueRef buffer, IOHIDSimpleQueueBlock applier, void * _Nullable ctx);

CF_EXPORT
void _IOHIDDictionaryAddSInt32 (CFMutableDictionaryRef dict, CFStringRef key, SInt32 value);

CF_EXPORT
void _IOHIDDictionaryAddSInt64 (CFMutableDictionaryRef dict, CFStringRef key, SInt64 value);

CF_EXPORT
void _IOHIDDictionaryAddCStr (CFMutableDictionaryRef dict, CFStringRef key, const char * cStr);


CF_EXPORT
void _IOHIDArrayAppendSInt64 (CFMutableArrayRef array, SInt64 value);

typedef struct {
    CFRuntimeBase               cfBase;
    volatile uint32_t           ref;
    volatile uint32_t           xref;
} IOHIDObjectBase;

typedef struct {
    CFRuntimeClass              cfClass;
    uint32_t (*intRetainCount)(intptr_t op, CFTypeRef cf);
    void (*intFinalize)(CFTypeRef cf);
} IOHIDObjectClass;

CF_EXPORT
CFTypeRef _IOHIDObjectInternalRetain (CFTypeRef cf);

CF_EXPORT
void _IOHIDObjectInternalRelease (CFTypeRef cf);

CF_EXPORT
uint32_t _IOHIDObjectExtRetainCount (intptr_t op, CFTypeRef cf);

CF_EXPORT
uint32_t _IOHIDObjectIntRetainCount (intptr_t op, CFTypeRef cf);

uint32_t _IOHIDObjectRetainCount (intptr_t op, CFTypeRef cf,  boolean_t isInternal);

CF_EXPORT
CFTypeRef _IOHIDObjectCreateInstance (CFAllocatorRef _Nullable allocator, CFTypeID typeID, CFIndex extraBytes, unsigned char * _Nullable category);

CF_EXPORT
bool _IOHIDIsRestrictedRemappingProperty(CFTypeRef property);

typedef void (^IOHIDCFSetBlock) (CFTypeRef value);

void _IOHIDCFSetApplyBlock (CFSetRef set, IOHIDCFSetBlock block);

typedef void (^IOHIDCFDictionaryBlock) (const void * key, const void * value);

void _IOHIDCFDictionaryApplyBlock (CFDictionaryRef set, IOHIDCFDictionaryBlock block);

typedef void (^IOHIDCFArrayBlock)(const void *value);

void _IOHIDCFArrayApplyBlock(CFArrayRef array, IOHIDCFArrayBlock block);

const void * _IOHIDObjectInternalRetainCallback (CFAllocatorRef allocator, const void * cf);

void _IOHIDObjectInternalReleaseCallback (CFAllocatorRef allocator, const void * cf);


CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS

#include <dlfcn.h>

#ifdef TARGET_OS_OSX
#define IOHID_DYN_LINK_DYLIB(directory, lib) \
static void* lib##Library() \
{ \
    static void* libLibrary = nil; \
    if (!libLibrary) libLibrary = dlopen(#directory "/lib" #lib ".dylib", RTLD_NOW); \
    return libLibrary; \
}
#else
#define IOHID_DYN_LINK_DYLIB(directory, lib) \
static void* lib##Library() \
{  \
    return nil; \
}
#endif

#define IOHID_DYN_LINK_FUNCTION(framework, functionName, localNameForFunction, resultType, defaultResult, parameterDeclarations, parameterNames) \
static resultType init##functionName parameterDeclarations; \
static resultType (*dynLink##functionName) parameterDeclarations = init##functionName; \
\
static resultType init##functionName parameterDeclarations \
{ \
    dynLink##functionName = (resultType (*) parameterDeclarations) dlsym(framework##Library(), #functionName); \
    if (dynLink##functionName != NULL) {  \
        return dynLink##functionName parameterNames; \
    } \
    else { \
        return (resultType)defaultResult; \
    }\
}\
\
__unused static inline resultType localNameForFunction parameterDeclarations \
{\
    return (dynLink##functionName != NULL) ? (resultType)(dynLink##functionName parameterNames) : (resultType)defaultResult; \
}

#endif /* _IOKIT_HID_IOHIDLIBPRIVATE_H */
