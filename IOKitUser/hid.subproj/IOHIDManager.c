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
#include <pthread.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/IOKitLib.h>
#include "IOHIDLibPrivate.h"
#include "IOHIDDevice.h"
#include "IOHIDLib.h"

static IOHIDManagerRef  __IOHIDManagerCreate(
                                    CFAllocatorRef          allocator, 
                                    CFAllocatorContext *    context __unused);
static void             __IOHIDManagerRelease( CFTypeRef object );
static void             __IOHIDManagerSetDeviceMatching(
                                IOHIDManagerRef                 manager,
                                CFDictionaryRef                 matching);
static void             __IOHIDManagerDeviceAdded(
                                    void *                      context,
                                    io_iterator_t               iterator);
static void             __IOHIDManagerDeviceRemoved(
                                    void *                      context,
                                    IOReturn                    result,
                                    void *                      sender);
static void             __IOHIDManagerDeviceApplier(
                                    const void *                value,
                                    void *                      context);
static void             __IOHIDManagerInitialEnumCallback(
                                    void *                      info);
static void             __MergeDictionaries(
                                    CFDictionaryRef             srcDict, 
                                    CFMutableDictionaryRef *    pDstDict);

enum {
    kDeviceApplierOpen                      = 1 << 0,
    kDeviceApplierClose                     = 1 << 1,
    kDeviceApplierInitEnumCallback          = 1 << 2,
    kDeviceApplierSetInputMatching          = 1 << 3,
    kDeviceApplierSetInputCallback          = 1 << 4,
    kDeviceApplierSetInputReportCallback    = 1 << 5, 
    kDeviceApplierScheduleRunLoop           = 1 << 6,
    kDeviceApplierUnscheduleRunLoop         = 1 << 7
};

typedef struct __DeviceApplierArgs {
    IOHIDManagerRef     manager;
    IOOptionBits        options;
    IOReturn            retVal;
}DeviceApplierArgs;

typedef struct __IOHIDManager
{
    CFRuntimeBase                   cfBase;   // base CFType information
    
    CFMutableSetRef                 devices;
    CFMutableSetRef                 iterators;
    CFMutableDictionaryRef          properties;

    IONotificationPortRef           notifyPort;
    CFRunLoopRef                    runLoop;
    CFStringRef                     runLoopMode;
    
    CFRunLoopSourceRef              initEnumRunLoopSource;
    CFMutableDictionaryRef          initRetVals;
    
    Boolean                         isOpen;
    IOOptionBits                    options;
    
    void *                          inputContext;
    IOHIDValueCallback              inputCallback;
    
    void *                          reportContext;
    IOHIDReportCallback             reportCallback;
    
    void *                          matchContext;
    IOHIDDeviceCallback             matchCallback;
    
    void *                          removalContext;
    IOHIDDeviceCallback             removalCallback;
    
    CFArrayRef                      inputMatchingMultiple;

} __IOHIDManager, *__IOHIDManagerRef;

static const CFRuntimeClass __IOHIDManagerClass = {
    0,                      // version
    "IOHIDManager",         // className
    NULL,                   // init
    NULL,                   // copy
    __IOHIDManagerRelease,   // finalize
    NULL,                   // equal
    NULL,                   // hash
    NULL,                   // copyFormattingDesc
    NULL
};

static pthread_once_t __sessionTypeInit = PTHREAD_ONCE_INIT;
static CFTypeID __kIOHIDManagerTypeID = _kCFRuntimeNotATypeID;

//------------------------------------------------------------------------------
// __IOHIDManagerRegister
//------------------------------------------------------------------------------
void __IOHIDManagerRegister(void)
{
    __kIOHIDManagerTypeID = _CFRuntimeRegisterClass(&__IOHIDManagerClass);
}

//------------------------------------------------------------------------------
// __IOHIDManagerCreate
//------------------------------------------------------------------------------
IOHIDManagerRef __IOHIDManagerCreate(   
                                CFAllocatorRef              allocator, 
                                CFAllocatorContext *        context __unused)
{
    IOHIDManagerRef manager = NULL;
    void *          offset  = NULL;
    uint32_t        size;

    /* allocate service */
    size    = sizeof(__IOHIDManager) - sizeof(CFRuntimeBase);
    manager = (IOHIDManagerRef)_CFRuntimeCreateInstance(
                                                allocator, 
                                                IOHIDManagerGetTypeID(), 
                                                size, 
                                                NULL);
    
    if (!manager)
        return NULL;

    offset = manager;
    bzero(offset + sizeof(CFRuntimeBase), size);
    
    return manager;
}

//------------------------------------------------------------------------------
// __IOHIDManagerRelease
//------------------------------------------------------------------------------
void __IOHIDManagerRelease( CFTypeRef object )
{
    IOHIDManagerRef manager = (IOHIDManagerRef)object;
    
    if ( manager->isOpen )
        IOHIDManagerClose(manager, manager->options);
        
    if ( manager->notifyPort ) {
        IONotificationPortDestroy(manager->notifyPort);
        manager->notifyPort = NULL;
    }
    
    if ( manager->devices ) {
        CFRelease(manager->devices);
        manager->devices = NULL;
    }

    if ( manager->iterators ) {
        CFRelease(manager->iterators);
        manager->iterators = NULL;
    }

    if ( manager->properties ) {
        CFRelease(manager->properties);
        manager->properties = NULL;
    }
    
    if ( manager->initEnumRunLoopSource ) {
        CFRelease(manager->initEnumRunLoopSource);
        manager->initEnumRunLoopSource = NULL;
    }
    
    if ( manager->initRetVals ) {
        CFRelease(manager->initRetVals);
        manager->initRetVals = NULL;
    }
    
    if ( manager->inputMatchingMultiple ) {
        CFRelease(manager->inputMatchingMultiple);
        manager->inputMatchingMultiple = NULL;
    }
}

//------------------------------------------------------------------------------
// __IOHIDManagerSetDeviceMatching
//------------------------------------------------------------------------------
void __IOHIDManagerSetDeviceMatching(
                                IOHIDManagerRef                 manager,
                                CFDictionaryRef                 matching)
{
    CFMutableDictionaryRef  matchingDict = NULL;
    io_iterator_t           iterator;
    IOReturn                kr;
    
    if (!manager->notifyPort) {
        manager->notifyPort = IONotificationPortCreate(kIOMasterPortDefault);
        
        if (manager->runLoop) 
            CFRunLoopAddSource(
                        manager->runLoop, 
                        IONotificationPortGetRunLoopSource(manager->notifyPort), 
                        manager->runLoopMode);
    }

    matchingDict = IOServiceMatching(kIOHIDDeviceKey);
    
    if ( !matchingDict )
        return;
        
    __MergeDictionaries(matching, &matchingDict);
    
    // Now set up a notification to be called when a device is first 
    // matched by I/O Kit.  Note that this will not catch any devices that were 
    // already plugged in so we take care of those later.
    kr = IOServiceAddMatchingNotification(manager->notifyPort,
                                          kIOFirstMatchNotification,
                                          matchingDict,
                                          __IOHIDManagerDeviceAdded,
                                          manager,
                                          &iterator
                                          );

    if ( kr != kIOReturnSuccess )
        return;
                
    // Add iterator to set for later destruction
    if ( !manager->iterators ) {
        CFSetCallBacks callbacks;
        
        bzero(&callbacks, sizeof(CFSetCallBacks));
        
        callbacks.retain    = _IOObjectCFRetain;
        callbacks.release   = _IOObjectCFRelease;
        
        manager->iterators  = CFSetCreateMutable(
                                            kCFAllocatorDefault, 
                                            0, 
                                            &callbacks);
        if ( !manager->iterators )
            return;
    }
        
    CFSetAddValue(manager->iterators, (void *)iterator);

    __IOHIDManagerDeviceAdded(manager, iterator);
}

//------------------------------------------------------------------------------
// __IOHIDManagerDeviceAdded
//------------------------------------------------------------------------------
void __IOHIDManagerDeviceAdded(     void *                      refcon,
                                    io_iterator_t               iterator)
{
    IOHIDManagerRef manager = (IOHIDManagerRef)refcon;
    IOHIDDeviceRef  device;
    IOReturn        retVal;
    io_service_t    service;
    Boolean         initial = FALSE;
    
    while (( service = IOIteratorNext(iterator) )) {        
        
        device = IOHIDDeviceCreate(kCFAllocatorDefault, service);
        
        if ( device ) {
            if ( !manager->devices ) {
                manager->devices = CFSetCreateMutable(
                                                        kCFAllocatorDefault, 
                                                        0, 
                                                        &kCFTypeSetCallBacks);
                initial = TRUE;
                
                if ( manager->isOpen )
                    manager->initRetVals = CFDictionaryCreateMutable(
                                            kCFAllocatorDefault, 
                                            0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            NULL);
            }
        
            if ( manager->devices )
                CFSetAddValue(manager->devices, device);
                
            CFRelease(device);

            IOHIDDeviceRegisterRemovalCallback(
                                            device,
                                            __IOHIDManagerDeviceRemoved,
                                            manager);
                                            
            retVal = kIOReturnSuccess;
            
            if ( manager->isOpen ) {
                retVal = IOHIDDeviceOpen(   device,
                                            manager->options);
                                            
                if ( manager->initRetVals )
                    CFDictionarySetValue(   manager->initRetVals, 
                                            device, 
                                            (void *)retVal);
            }
            
            if ( manager->inputCallback )
                IOHIDDeviceRegisterInputValueCallback(
                                            device,
                                            manager->inputCallback,
                                            manager->inputContext);
                                            
            if ( manager->runLoop ) {
                IOHIDDeviceScheduleWithRunLoop(
                                            device,
                                            manager->runLoop,
                                            manager->runLoopMode);
                   
                // If this this is called using the iterator returned in 
                // IOServiceAddMatchingNotification, pend performing the
                // callback on the runLoop
                if ( !initial && manager->matchCallback )
                    (*manager->matchCallback)(  manager->matchContext, 
                                                retVal,
                                                manager,
                                                device);
            }

        }
        
        IOObjectRelease(service);
    }
    
    // Dispatch initial enumeration callback on runLoop
    if ( initial ) {
        
        CFRunLoopSourceContext  context;
        
        bzero(&context, sizeof(CFRunLoopSourceContext));
        
        context.info    = manager;
        context.perform = __IOHIDManagerInitialEnumCallback;
        
        manager->initEnumRunLoopSource = CFRunLoopSourceCreate( 
                                                        kCFAllocatorDefault, 
                                                        0, 
                                                        &context);
                                                        
        if ( manager->runLoop && manager->initEnumRunLoopSource ) {
            CFRunLoopAddSource(
                        manager->runLoop, 
                        manager->initEnumRunLoopSource, 
                        manager->runLoopMode);
                        
            CFRunLoopSourceSignal(manager->initEnumRunLoopSource);
        }
    }
}

//------------------------------------------------------------------------------
// __IOHIDManagerDeviceRemoved
//------------------------------------------------------------------------------
void __IOHIDManagerDeviceRemoved(   void *                      context,
                                    IOReturn                    result __unused,
                                    void *                      sender)
{
    IOHIDManagerRef manager = (IOHIDManagerRef)context;
    
    CFSetRemoveValue(manager->devices, sender);
    
    if ( manager->removalCallback )
        (*manager->removalCallback)(manager->removalContext,
                                    kIOReturnSuccess,
                                    manager,
                                    sender);
}

//------------------------------------------------------------------------------
// __IOHIDManagerInitialEnumCallback
//------------------------------------------------------------------------------
void __IOHIDManagerInitialEnumCallback(void * info)
{
    IOHIDManagerRef manager = (IOHIDManagerRef)info;
    
    if ( manager->matchCallback && manager->devices ) {
        DeviceApplierArgs args;
        
        args.manager = manager;
        args.options = kDeviceApplierInitEnumCallback;
    
        CFSetApplyFunction( manager->devices, 
                            __IOHIDManagerDeviceApplier, 
                            &args);
    }
    
    // After we have dispatched all of the enum callbacks, kill the source
    CFRunLoopRemoveSource(  manager->runLoop, 
                            manager->initEnumRunLoopSource, 
                            manager->runLoopMode);

    CFRelease(manager->initEnumRunLoopSource);
    manager->initEnumRunLoopSource = NULL;
    
    if ( manager->initRetVals ) {
        CFRelease(manager->initRetVals);
        manager->initRetVals = NULL;
    }
}

//------------------------------------------------------------------------------
// __IOHIDManagerDeviceApplier
//------------------------------------------------------------------------------
void __IOHIDManagerDeviceApplier(
                                    const void *                value,
                                    void *                      context)
{
    DeviceApplierArgs * args    = (DeviceApplierArgs*)context;
    IOHIDDeviceRef      device  = (IOHIDDeviceRef)value;
    IOReturn            retVal  = kIOReturnSuccess;
    
    if ( args->options & kDeviceApplierOpen )
        retVal = IOHIDDeviceOpen(           device,
                                            args->manager->options);

    if ( args->options & kDeviceApplierClose )
        retVal = IOHIDDeviceClose(          device,
                                            args->manager->options);
                                            
    if ( args->options & kDeviceApplierInitEnumCallback ) {
        if ( args->manager->initRetVals )
            retVal = (IOReturn)CFDictionaryGetValue(
                                            args->manager->initRetVals, 
                                            device);
            
        (*args->manager->matchCallback)(    args->manager->matchContext,
                                            retVal,
                                            args->manager,
                                            device);
    }

    if ( args->options & kDeviceApplierSetInputMatching )
        IOHIDDeviceSetInputValueMatchingMultiple( 
                                            device,
                                            args->manager->inputMatchingMultiple);
    
    if ( args->options & kDeviceApplierSetInputCallback )
        IOHIDDeviceRegisterInputValueCallback( 
                                            device,
                                            args->manager->inputCallback,
                                            args->manager->inputContext);

    if ( args->options & kDeviceApplierSetInputReportCallback )
        IOHIDDeviceRegisterInputReportCallback(  
                                            device,
                                            NULL,
                                            0,
                                            args->manager->reportCallback,
                                            args->manager->reportContext);
    
    if ( args->options & kDeviceApplierScheduleRunLoop )
        IOHIDDeviceScheduleWithRunLoop(     device,
                                            args->manager->runLoop,
                                            args->manager->runLoopMode);

    if ( args->options & kDeviceApplierUnscheduleRunLoop )
        IOHIDDeviceUnscheduleFromRunLoop(   device,
                                            args->manager->runLoop,
                                            args->manager->runLoopMode);
                                            
    if ( args->retVal == kIOReturnSuccess && retVal != kIOReturnSuccess )
        args->retVal = retVal;
}

//------------------------------------------------------------------------------
// IOHIDManagerGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOHIDManagerGetTypeID(void)
{
    if ( _kCFRuntimeNotATypeID == __kIOHIDManagerTypeID )
        pthread_once(&__sessionTypeInit, __IOHIDManagerRegister);
        
    return __kIOHIDManagerTypeID;
}

//------------------------------------------------------------------------------
// IOHIDManagerCreate
//------------------------------------------------------------------------------
IOHIDManagerRef IOHIDManagerCreate(     
                                    CFAllocatorRef          allocator,
                                    IOOptionBits            options __unused)
{    
    IOHIDManagerRef manager;
    
    manager = __IOHIDManagerCreate(allocator, NULL);
    
    if (!manager)
        return NULL;
    
        
    return manager;
}

//------------------------------------------------------------------------------
// IOHIDManagerOpen
//------------------------------------------------------------------------------
IOReturn IOHIDManagerOpen(
                                IOHIDManagerRef                 manager,
                                IOOptionBits                    options)
{
    IOReturn retVal = kIOReturnSuccess;
    
    if ( !manager->isOpen ) {
        manager->isOpen     = TRUE;
        manager->options    = options;

        if ( manager->devices ) {
            DeviceApplierArgs args;
            
            args.manager        = manager;
            args.options        = kDeviceApplierOpen;
            args.retVal         = kIOReturnSuccess;
            
            if ( manager->inputMatchingMultiple )
                args.options |= kDeviceApplierSetInputMatching;
            
            if ( manager->inputCallback )
                args.options |= kDeviceApplierSetInputCallback;

            if ( manager->reportCallback )
                args.options |= kDeviceApplierSetInputReportCallback;
                        
            CFSetApplyFunction( manager->devices, 
                                __IOHIDManagerDeviceApplier, 
                                &args);
                                
            retVal = args.retVal;
        }
    }

    return retVal;
}
                                
//------------------------------------------------------------------------------
// IOHIDManagerClose
//------------------------------------------------------------------------------
IOReturn IOHIDManagerClose(
                                IOHIDManagerRef                 manager,
                                IOOptionBits                    options)
{
    IOReturn retVal = kIOReturnSuccess;

    if ( manager->isOpen ) {
        manager->isOpen     = FALSE;
        manager->options    = options;

        if ( manager->devices ) {
            DeviceApplierArgs args;
            
            args.manager        = manager;
            args.options        = kDeviceApplierClose;
            args.retVal         = kIOReturnSuccess;

            CFSetApplyFunction( manager->devices, 
                                __IOHIDManagerDeviceApplier, 
                                &args);

            retVal = args.retVal;
        }
    }

    return retVal;
}

//------------------------------------------------------------------------------
// IOHIDManagerGetProperty
//------------------------------------------------------------------------------
CFTypeRef IOHIDManagerGetProperty(
                                IOHIDManagerRef                 manager,
                                CFStringRef                     key)
{
    if ( !manager->properties )
        return NULL;
        
    return CFDictionaryGetValue(manager->properties, key);
}
                                
//------------------------------------------------------------------------------
// IOHIDManagerSetProperty
//------------------------------------------------------------------------------
Boolean IOHIDManagerSetProperty(
                                IOHIDManagerRef                 manager,
                                CFStringRef                     key,
                                CFTypeRef                       value)
{
    if (!manager->properties || 
        !(manager->properties = CFDictionaryCreateMutable(
                                            kCFAllocatorDefault, 
                                            0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks)))
        return FALSE;
        
    CFDictionarySetValue(manager->properties, key, value);
    
    // RY: Should iterate through all devices and set properties too.
    return TRUE;
}
                                        
//------------------------------------------------------------------------------
// IOHIDManagerSetDeviceMatching
//------------------------------------------------------------------------------
void IOHIDManagerSetDeviceMatching(
                                IOHIDManagerRef                 manager,
                                CFDictionaryRef                 matching)
{
    CFArrayRef multiple = NULL;     
      
    if ( matching ) 
        multiple = CFArrayCreate(kCFAllocatorDefault,
                                (const void **)&matching,
                                1,
                                &kCFTypeArrayCallBacks);

    IOHIDManagerSetDeviceMatchingMultiple(manager, multiple);
                                        
    if ( multiple )
        CFRelease(multiple);
}

//------------------------------------------------------------------------------
// IOHIDManagerSetDeviceMatchingMultiple
//------------------------------------------------------------------------------
void IOHIDManagerSetDeviceMatchingMultiple(
                                IOHIDManagerRef                 manager,
                                CFArrayRef                      multiple)
{
    CFIndex         i, count;
    CFTypeRef       value;
    
    if ( manager->devices )
        CFSetRemoveAllValues(manager->devices);
        
    if ( manager->iterators )
        CFSetRemoveAllValues(manager->iterators);
    
    if ( multiple ) {
        count = CFArrayGetCount(multiple);
        for ( i=0; i<count; i++ ) {
            value = CFArrayGetValueAtIndex(multiple, i);
            if (CFDictionaryGetTypeID() == CFGetTypeID(value))
                __IOHIDManagerSetDeviceMatching(manager, (CFDictionaryRef)value);
        }
    }
    else 
        __IOHIDManagerSetDeviceMatching(manager, NULL);

}

//------------------------------------------------------------------------------
// IOHIDManagerCopyDevices
//------------------------------------------------------------------------------
CFSetRef IOHIDManagerCopyDevices(
                                IOHIDManagerRef                 manager)
{
    return manager->devices ? 
                CFSetCreateCopy(kCFAllocatorDefault, manager->devices) : NULL;
}

//------------------------------------------------------------------------------
// IOHIDManagerRegisterDeviceMatchingCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterDeviceMatchingCallback(
                                IOHIDManagerRef                 manager,
                                IOHIDDeviceCallback             callback,
                                void *                          context)
{
    manager->matchCallback  = callback;
    manager->matchContext   = context;
}

//------------------------------------------------------------------------------
// IOHIDManagerRegisterDeviceRemovalCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterDeviceRemovalCallback(
                                IOHIDManagerRef                 manager,
                                IOHIDDeviceCallback             callback,
                                void *                          context)
{
    manager->removalCallback    = callback;
    manager->removalContext     = context;

}
                                        
//------------------------------------------------------------------------------
// IOHIDManagerRegisterInputReportCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterInputReportCallback( 
                                    IOHIDManagerRef             manager,
                                    IOHIDReportCallback         callback,
                                    void *                      context)
{
    manager->reportCallback  = callback;
    manager->reportContext   = context;
    
    if ( manager->isOpen && manager->devices ) {
        DeviceApplierArgs args;
        
        args.manager = manager;
        args.options = kDeviceApplierSetInputReportCallback;
    
        CFSetApplyFunction( manager->devices, 
                            __IOHIDManagerDeviceApplier, 
                            &args);
    }
}

//------------------------------------------------------------------------------
// IOHIDManagerRegisterInputValueCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterInputValueCallback( 
                                    IOHIDManagerRef             manager,
                                    IOHIDValueCallback          callback,
                                    void *                      context)
{
    manager->inputCallback  = callback;
    manager->inputContext   = context;
    
    if ( manager->isOpen && manager->devices ) {
        DeviceApplierArgs args;
        
        args.manager = manager;
        args.options = kDeviceApplierSetInputCallback;
    
        CFSetApplyFunction( manager->devices, 
                            __IOHIDManagerDeviceApplier, 
                            &args);
    }
}

//------------------------------------------------------------------------------
// IOHIDManagerSetInputValueMatching
//------------------------------------------------------------------------------
void IOHIDManagerSetInputValueMatching( 
                                    IOHIDManagerRef             manager,
                                    CFDictionaryRef             matching)
{
    if ( matching ) {
        CFArrayRef multiple = CFArrayCreate(kCFAllocatorDefault, (const void **)&matching, 1, &kCFTypeArrayCallBacks);
        
        IOHIDManagerSetInputValueMatchingMultiple(manager, multiple);
        
        CFRelease(multiple);
    } else {
        IOHIDManagerSetInputValueMatchingMultiple(manager, NULL);
    }
}

//------------------------------------------------------------------------------
// IOHIDManagerSetInputValueMatchingMultiple
//------------------------------------------------------------------------------
void IOHIDManagerSetInputValueMatchingMultiple( 
                                    IOHIDManagerRef             manager,
                                    CFArrayRef                  multiple)
{
    if ( manager->inputMatchingMultiple )
        CFRelease(manager->inputMatchingMultiple);
        
    if ( multiple )
        CFRetain(multiple);
        
    manager->inputMatchingMultiple = multiple;
        
    if ( manager->isOpen && manager->devices ) {
        DeviceApplierArgs args;
        
        args.manager = manager;
        args.options = kDeviceApplierSetInputMatching;
    
        CFSetApplyFunction( manager->devices, 
                            __IOHIDManagerDeviceApplier, 
                            &args);
    }
}

//------------------------------------------------------------------------------
// IOHIDManagerScheduleWithRunLoop
//------------------------------------------------------------------------------
void IOHIDManagerScheduleWithRunLoop(
                                        IOHIDManagerRef         manager,
                                        CFRunLoopRef            runLoop, 
                                        CFStringRef             runLoopMode)
{
    manager->runLoop        = runLoop;
    manager->runLoopMode    = runLoopMode;

    if ( manager->runLoop ) {
        // Schedule the notifyPort
        if (manager->notifyPort)
            CFRunLoopAddSource(
                        manager->runLoop, 
                        IONotificationPortGetRunLoopSource(manager->notifyPort), 
                        manager->runLoopMode);

        // schedule the initial enumeration routine
        if ( manager->initEnumRunLoopSource ) {
            CFRunLoopAddSource(
                        manager->runLoop, 
                        manager->initEnumRunLoopSource, 
                        manager->runLoopMode);

            CFRunLoopSourceSignal(manager->initEnumRunLoopSource);
        }
                        
        // Schedule the devices
        if ( manager->devices ) {
            DeviceApplierArgs args;
            
            args.manager = manager;
            args.options = kDeviceApplierScheduleRunLoop;
            
            CFSetApplyFunction( manager->devices, 
                                __IOHIDManagerDeviceApplier, 
                                &args);
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDManagerUnscheduleFromRunLoop
//------------------------------------------------------------------------------
void IOHIDManagerUnscheduleFromRunLoop(
                                        IOHIDManagerRef         manager,
                                        CFRunLoopRef            runLoop, 
                                        CFStringRef             runLoopMode)
{
    if (!CFEqual(manager->runLoop, runLoop) || 
        !CFEqual(manager->runLoopMode, runLoopMode)) 
        return;
        
    if ( manager->devices ) {
        // Schedule the devices
        DeviceApplierArgs args;
        
        args.manager = manager;
        args.options = kDeviceApplierUnscheduleRunLoop;
        
        CFSetApplyFunction( manager->devices, 
                            __IOHIDManagerDeviceApplier, 
                            &args);
    }
                            
    manager->runLoop        = NULL;
    manager->runLoopMode    = NULL;
}

//===========================================================================
// Static Helper Definitions
//===========================================================================
//------------------------------------------------------------------------------
// __MergeDictionaries
//------------------------------------------------------------------------------
void __MergeDictionaries(           CFDictionaryRef             srcDict, 
                                    CFMutableDictionaryRef *    pDstDict)
{
    uint32_t        count;
    CFTypeRef *     values;
    CFStringRef *   keys;
    
    if ( !pDstDict || !srcDict || !(count = CFDictionaryGetCount(srcDict)))
        return;
        
    if ( !*pDstDict )
        return;
                
    values  = (CFTypeRef *)malloc(sizeof(CFTypeRef) * count);
    keys    = (CFStringRef *)malloc(sizeof(CFStringRef) * count);
    
    if ( values && keys ) {
        CFDictionaryGetKeysAndValues(srcDict, (const void **)keys, (const void **)values);
        
        for ( uint32_t i=0; i<count; i++) 
            CFDictionarySetValue(*pDstDict, keys[i], values[i]);
    }
        
    if ( values )
        free(values);
        
    if ( keys )
        free(keys);
}

