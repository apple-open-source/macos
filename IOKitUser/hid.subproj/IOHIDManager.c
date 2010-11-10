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
#include "IOHIDManager.h"
#include <IOKit/IOKitLib.h>
#include "IOHIDLibPrivate.h"
#include "IOHIDDevice.h"
#include "IOHIDLib.h"
#include "IOHIDManagerPersistentProperties.h"

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
static void             __IOHIDManagerMergeDictionaries(
                                    CFDictionaryRef             srcDict, 
                                    CFMutableDictionaryRef      dstDict);

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
}   DeviceApplierArgs;

typedef struct __IOHIDManager
{
    CFRuntimeBase                   cfBase;   // base CFType information
    
    CFMutableSetRef                 devices;
    CFMutableSetRef                 iterators;
    CFMutableDictionaryRef          properties;
    CFMutableDictionaryRef          deviceInputBuffers;

    IONotificationPortRef           notifyPort;
    CFRunLoopRef                    runLoop;
    CFStringRef                     runLoopMode;
    
    CFRunLoopSourceRef              initEnumRunLoopSource;
    CFMutableDictionaryRef          initRetVals;
    
    Boolean                         isOpen;
    IOOptionBits                    openOptions;
    IOOptionBits                    createOptions;
    
    void *                          inputContext;
    IOHIDValueCallback              inputCallback;
    
    void *                          reportContext;
    IOHIDReportCallback             reportCallback;
    
    void *                          matchContext;
    IOHIDDeviceCallback             matchCallback;
    
    void *                          removalContext;
    IOHIDDeviceCallback             removalCallback;
    
    CFArrayRef                      inputMatchingMultiple;
    Boolean                         isDirty;

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
    NULL,
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
    
    if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
        !(manager->createOptions & kIOHIDManagerOptionDoNotSaveProperties)) {
        __IOHIDManagerSaveProperties(manager, NULL);
    }
    
    if ( manager->isOpen )
        IOHIDManagerClose(manager, manager->openOptions);
        
    if ( manager->notifyPort ) {
        IONotificationPortDestroy(manager->notifyPort);
        manager->notifyPort = NULL;
    }
    
    if ( manager->devices ) {
        CFRelease(manager->devices);
        manager->devices = NULL;
    }
    
    if ( manager->deviceInputBuffers ) {
        CFRelease(manager->deviceInputBuffers);
        manager->deviceInputBuffers = NULL;
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
        
    __IOHIDManagerMergeDictionaries(matching, matchingDict);
    
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
        
    intptr_t temp = iterator;
    CFSetAddValue(manager->iterators, (void *)temp);

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
        
            if ( manager->devices ) {
                CFSetAddValue(manager->devices, device);
            }
                
            CFRelease(device);
            
            DeviceApplierArgs args;

            args.manager = manager;
            args.options = 0;

            IOHIDDeviceRegisterRemovalCallback(
                                            device,
                                            __IOHIDManagerDeviceRemoved,
                                            manager);
                                            
            retVal = kIOReturnSuccess;
            
            if ( manager->isOpen )
                args.options |= kDeviceApplierOpen;
 
            if ( manager->inputMatchingMultiple )
                args.options |= kDeviceApplierSetInputMatching;
            
            if ( manager->inputCallback )
                args.options |= kDeviceApplierSetInputCallback;

            if ( manager->reportCallback )
                args.options |= kDeviceApplierSetInputReportCallback;
                                                       
            if ( manager->runLoop ) {
                args.options |= kDeviceApplierScheduleRunLoop;

                // If this this is called using the iterator returned in 
                // IOServiceAddMatchingNotification, pend performing the
                // callback on the runLoop
                if ( !initial && manager->matchCallback )
                    args.options |= kDeviceApplierInitEnumCallback;
            }
            
            __IOHIDManagerDeviceApplier((const void *)device, &args);

            if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
                !(manager->createOptions & kIOHIDManagerOptionDoNotLoadProperties)) {
                __IOHIDDeviceLoadProperties(device);
            }
            if (manager->properties) {
                CFDictionaryApplyFunction(manager->properties, 
                                          __IOHIDApplyPropertiesToDeviceFromDictionary, 
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
    
    if ( manager->deviceInputBuffers )
        CFDictionaryRemoveValue(manager->deviceInputBuffers, sender);
        
    if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
        !(manager->createOptions & kIOHIDManagerOptionDoNotSaveProperties)) {
        if (CFSetContainsValue(manager->devices, sender))
            __IOHIDDeviceSaveProperties((IOHIDDeviceRef)sender, NULL);
    }

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
    intptr_t            retVal  = kIOReturnSuccess;
    
    if ( args->options & kDeviceApplierOpen ) {
        retVal = IOHIDDeviceOpen(           device,
                                            args->manager->openOptions);
        if ( args->manager->initRetVals )
            CFDictionarySetValue(args->manager->initRetVals, device, (void*)retVal);
    }

    if ( args->options & kDeviceApplierClose )
        retVal = IOHIDDeviceClose(          device,
                                            args->manager->openOptions);
                                            
    if ( args->options & kDeviceApplierInitEnumCallback ) {
        if ( args->manager->initRetVals )
            retVal = (intptr_t)CFDictionaryGetValue(
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

    if ( args->options & kDeviceApplierSetInputReportCallback ) {
        CFMutableDataRef dataRef = NULL;
        if ( !args->manager->deviceInputBuffers ) {
            args->manager->deviceInputBuffers = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        }

        dataRef = (CFMutableDataRef)CFDictionaryGetValue(args->manager->deviceInputBuffers, device);
        if ( !dataRef ) {
            CFNumberRef number = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDMaxInputReportSizeKey));
            CFIndex     length = 64;
            
            if ( number ) {
                CFNumberGetValue(number, kCFNumberCFIndexType, &length);
            }
            
            dataRef = CFDataCreateMutable(kCFAllocatorDefault, length);
            if ( dataRef ) {
                CFDataSetLength(dataRef, length);
                CFDictionarySetValue(args->manager->deviceInputBuffers, device, dataRef);
                CFRelease(dataRef);
            }
        }
    
        IOHIDDeviceRegisterInputReportCallback(  
                                            device,
                                            (uint8_t *)CFDataGetMutableBytePtr(dataRef),
                                            CFDataGetLength(dataRef),
                                            args->manager->reportCallback,
                                            args->manager->reportContext);
    }
    
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
                                    IOOptionBits            options)
{    
    IOHIDManagerRef manager;
    
    manager = __IOHIDManagerCreate(allocator, NULL);
    
    if (!manager)
        return NULL;
    
    manager->createOptions = options;
    
    if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
        !(manager->createOptions & kIOHIDManagerOptionDoNotLoadProperties))
    {
        __IOHIDManagerLoadProperties(manager);
    }
        
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
        manager->isOpen         = TRUE;
        manager->openOptions    = options;

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
        manager->isOpen         = FALSE;
        manager->openOptions    = options;

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

    if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
        !(manager->createOptions & kIOHIDManagerOptionDoNotSaveProperties)) {
        __IOHIDManagerSaveProperties(manager, NULL);
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
    __IOHIDApplyPropertyToSetContext context = {
        key, value
    };
    
    if (!manager->properties) {
        manager->properties = CFDictionaryCreateMutable(kCFAllocatorDefault, 
                                                        0,
                                                        &kCFTypeDictionaryKeyCallBacks,
                                                        &kCFTypeDictionaryValueCallBacks);
        if (!manager->properties)
            return FALSE;
    }
    
    manager->isDirty = TRUE;
    CFDictionarySetValue(manager->properties, key, value);
    if (manager->devices)
        CFSetApplyFunction(manager->devices, __IOHIDApplyPropertyToDeviceSet, &context);
    
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
    
    if ( manager->devices ) {
        if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
            !(manager->createOptions & kIOHIDManagerOptionDoNotSaveProperties)) {
            __IOHIDManagerSaveProperties(manager, NULL);
        }
        CFSetRemoveAllValues(manager->devices);
    }
        
    if ( manager->iterators )
        CFSetRemoveAllValues(manager->iterators);

    if ( manager->deviceInputBuffers )
        CFDictionaryRemoveAllValues(manager->deviceInputBuffers);
    
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

//------------------------------------------------------------------------------
// IOHIDManagerUnscheduleFromRunLoop
//------------------------------------------------------------------------------
void IOHIDManagerSaveToPropertyDomain(IOHIDManagerRef                 manager,
                                      CFStringRef                     applicationID,
                                      CFStringRef                     userName,
                                      CFStringRef                     hostName,
                                      IOOptionBits                    options)
{
    __IOHIDPropertyContext context = {
        applicationID, userName, hostName, options
    };
    
    if (manager && applicationID && userName && hostName) {
        __IOHIDManagerSaveProperties(manager, &context);        
    }
    else {
        // LOG AN ERROR?
    }
}

//------------------------------------------------------------------------------
CFStringRef __IOHIDManagerGetRootKey() 
{
    return CFSTR(kIOHIDManagerKey);
}

//------------------------------------------------------------------------------
void __IOHIDManagerSaveProperties(IOHIDManagerRef manager, __IOHIDPropertyContext *context)
{
    if (manager->isDirty && manager->properties) {
        __IOHIDPropertySaveToKeyWithSpecialKeys(manager->properties, 
                                               __IOHIDManagerGetRootKey(),
                                               NULL,
                                               context);
        manager->isDirty = FALSE;
    }
    
    if (manager->devices)
        CFSetApplyFunction(manager->devices, __IOHIDSaveDeviceSet, context);
}

//------------------------------------------------------------------------------
void __IOHIDManagerLoadProperties(IOHIDManagerRef manager)
{
    // Convert to __IOHIDPropertyLoadFromKeyWithSpecialKeys if we identify special keys
    CFMutableDictionaryRef properties = __IOHIDPropertyLoadDictionaryFromKey(__IOHIDManagerGetRootKey());
    
    if (properties) {
        CFRELEASE_IF_NOT_NULL(manager->properties);
        manager->properties = properties;
        manager->isDirty = FALSE;
    }
    
    // We do not load device properties here, since the devices are not present when this is called.
}

//------------------------------------------------------------------------------
void __IOHIDPropertySaveWithContext(CFStringRef key, CFPropertyListRef value, __IOHIDPropertyContext *context)
{
    if (key && value) {
        if (context && context->applicationID && context->userName && context->hostName) {
            CFPreferencesSetValue(key, value, context->applicationID, context->userName, context->hostName);
        }
        else {
            CFPreferencesSetAppValue(key, value, kCFPreferencesCurrentApplication);
        }
    }
}

//------------------------------------------------------------------------------
void __IOHIDPropertySaveToKeyWithSpecialKeys(CFDictionaryRef dictionary, CFStringRef key, CFStringRef *specialKeys, __IOHIDPropertyContext *context)
{
    CFMutableDictionaryRef temp = CFDictionaryCreateMutableCopy(NULL, 0, dictionary);
    
    if (specialKeys) {
        while (*specialKeys) {
            CFTypeRef value = CFDictionaryGetValue(temp, *specialKeys);
            if (value) {
                CFStringRef subKey = CFStringCreateWithFormat(NULL, NULL, 
                                                              CFSTR("%@#%@"), 
                                                              key,
                                                              *specialKeys);
                __IOHIDPropertySaveWithContext(subKey, value, context);
                CFDictionaryRemoveValue(temp, *specialKeys);
                CFRelease(subKey);
            }
            specialKeys++;
        }
    }
    
    CFAbsoluteTime time = CFAbsoluteTimeGetCurrent();
    CFNumberRef timeNum = CFNumberCreate(NULL, kCFNumberDoubleType, &time);
    CFDictionaryAddValue(temp, CFSTR("time of last save"), timeNum);
    CFRelease(timeNum);
    __IOHIDPropertySaveWithContext(key, temp, context);
    CFRelease(temp);
}

//------------------------------------------------------------------------------
CFMutableDictionaryRef __IOHIDPropertyLoadDictionaryFromKey(CFStringRef key)
{
    CFMutableDictionaryRef result = NULL;
    CFDictionaryRef baseProperties = CFPreferencesCopyAppValue(key, kCFPreferencesCurrentApplication);
    if (baseProperties && (CFGetTypeID(baseProperties) == CFDictionaryGetTypeID())) {
        result = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        CFDictionaryRef properties = CFPreferencesCopyValue(key, kCFPreferencesAnyApplication, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
        if (properties && (CFGetTypeID(properties) == CFDictionaryGetTypeID()))
            __IOHIDManagerMergeDictionaries(properties, result);
        if (properties)
            CFRelease(properties);
        
        properties = CFPreferencesCopyValue(key, kCFPreferencesAnyApplication, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if (properties && (CFGetTypeID(properties) == CFDictionaryGetTypeID()))
            __IOHIDManagerMergeDictionaries(properties, result);
        if (properties)
            CFRelease(properties);
        
        properties = CFPreferencesCopyValue(key, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (properties && (CFGetTypeID(properties) == CFDictionaryGetTypeID()))
            __IOHIDManagerMergeDictionaries(properties, result);
        if (properties)
            CFRelease(properties);
        
        properties = CFPreferencesCopyValue(key, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
        if (properties && (CFGetTypeID(properties) == CFDictionaryGetTypeID()))
            __IOHIDManagerMergeDictionaries(properties, result);
        if (properties)
            CFRelease(properties);
        
        properties = CFPreferencesCopyValue(key, kCFPreferencesCurrentApplication, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
        if (properties && (CFGetTypeID(properties) == CFDictionaryGetTypeID()))
            __IOHIDManagerMergeDictionaries(properties, result);
        if (properties)
            CFRelease(properties);
        
        properties = CFPreferencesCopyValue(key, kCFPreferencesCurrentApplication, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        if (properties && (CFGetTypeID(properties) == CFDictionaryGetTypeID()))
            __IOHIDManagerMergeDictionaries(properties, result);
        if (properties)
            CFRelease(properties);
        
        properties = CFPreferencesCopyValue(key, kCFPreferencesCurrentApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (properties && (CFGetTypeID(properties) == CFDictionaryGetTypeID()))
            __IOHIDManagerMergeDictionaries(properties, result);
        if (properties)
            CFRelease(properties);
        
        properties = CFPreferencesCopyValue(key, kCFPreferencesCurrentApplication, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
        if (properties && (CFGetTypeID(properties) == CFDictionaryGetTypeID()))
            __IOHIDManagerMergeDictionaries(properties, result);
        if (properties)
            CFRelease(properties);
        
        __IOHIDManagerMergeDictionaries(baseProperties, result);
    }
    if (baseProperties)
        CFRelease(baseProperties);
    return result;
}

//------------------------------------------------------------------------------
CFMutableDictionaryRef __IOHIDPropertyLoadFromKeyWithSpecialKeys(CFStringRef key, CFStringRef *specialKeys)
{
    CFMutableDictionaryRef result = __IOHIDPropertyLoadDictionaryFromKey(key);
    
    if (!result)
        result = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    while (*specialKeys) {
        CFStringRef subKey = CFStringCreateWithFormat(NULL, NULL, 
                                                      CFSTR("%@#%@"), 
                                                      key,
                                                      *specialKeys);
        CFPropertyListRef value = CFPreferencesCopyAppValue(subKey, kCFPreferencesCurrentApplication);
        if (value) {
            CFDictionarySetValue(result, *specialKeys, value);
            CFRelease(value);
        }
        
        CFRelease(subKey);
        specialKeys++;
    }
    
    return result;
}

//===========================================================================
// Static Helper Definitions
//===========================================================================
//------------------------------------------------------------------------------
// __IOHIDManagerMergeDictionaries
//------------------------------------------------------------------------------
void __IOHIDManagerMergeDictionaries(CFDictionaryRef        srcDict, 
                                     CFMutableDictionaryRef dstDict)
{
    uint32_t        count;
    CFTypeRef *     values;
    CFStringRef *   keys;
    
    if ( !dstDict || !srcDict || !(count = CFDictionaryGetCount(srcDict)))
        return;
        
    values  = (CFTypeRef *)malloc(sizeof(CFTypeRef) * count);
    keys    = (CFStringRef *)malloc(sizeof(CFStringRef) * count);
    
    if ( values && keys ) {
        CFDictionaryGetKeysAndValues(srcDict, (const void **)keys, (const void **)values);
        
        for ( uint32_t i=0; i<count; i++) 
            CFDictionarySetValue(dstDict, keys[i], values[i]);
    }
        
    if ( values )
        free(values);
        
    if ( keys )
        free(keys);
}

