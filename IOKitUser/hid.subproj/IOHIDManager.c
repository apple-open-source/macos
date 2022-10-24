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
#include "IOHIDDevicePrivate.h"
#include "IOHIDLib.h"
#include "IOHIDManagerPersistentProperties.h"
#include <os/assumes.h>
#include <AssertMacros.h>
#include <os/lock_private.h>
#include <os/state_private.h>

static IOHIDManagerRef  __IOHIDManagerCreate(
                                    CFAllocatorRef          allocator, 
                                    CFAllocatorContext *    context __unused);
static void             __IOHIDManagerExtRelease( CFTypeRef object );
static void             __IOHIDManagerIntRelease( CFTypeRef object );
static void             __IOHIDManagerSetDeviceMatching(
                                IOHIDManagerRef                 manager,
                                CFDictionaryRef                 matching);
static void             __IOHIDManagerDeviceAdded(
                                    void *                      context,
                                    io_iterator_t               iterator);
static void             __IOHIDManagerDeviceRemoved(void *refcon,
                                                    io_service_t service,
                                                    uint32_t messageType,
                                                    void *messageArgument);
static IOReturn         __ApplyToDevices(IOHIDManagerRef manager,
                                         IOOptionBits options);
static void             __IOHIDManagerDeviceApplier(
                                    const void *                value,
                                    void *                      context);
static void             __IOHIDManagerInitialEnumCallback(
                                    void *                      info);
static void             __IOHIDManagerMergeDictionaries(
                                    CFDictionaryRef             srcDict, 
                                    CFMutableDictionaryRef      dstDict);
static  void __IOHIDManagerFinalizeStateHandler (void *context);
static  os_state_data_t __IOHIDManagerStateHandler (IOHIDManagerRef device, os_state_hints_t hints);
static  CFMutableDictionaryRef __IOHIDManagerSerializeState(IOHIDManagerRef device);

enum {
    kDeviceApplierOpen                      = 1 << 0,
    kDeviceApplierClose                     = 1 << 1,
    kDeviceApplierInitEnumCallback          = 1 << 2,
    kDeviceApplierSetInputMatching          = 1 << 3,
    kDeviceApplierSetInputCallback          = 1 << 4,
    kDeviceApplierSetInputReportCallback    = 1 << 5, 
    kDeviceApplierScheduleRunLoop           = 1 << 6,
    kDeviceApplierUnscheduleRunLoop         = 1 << 7,
    kDeviceApplierSetDispatchQueue          = 1 << 8,
    kDeviceApplierSetCancelHandler          = 1 << 9,
    kDeviceApplierActivate                  = 1 << 10,
    kDeviceApplierCancel                    = 1 << 11,
    kDeviceApplierSetInputTSReportCallback  = 1 << 12,
};

typedef struct __DeviceApplierArgs {
    IOHIDManagerRef     manager;
    IOOptionBits        options;
    IOReturn            retVal;
}   DeviceApplierArgs;

typedef struct __DeviceInitialEnumArgs {
    IOHIDManagerRef manager;
    CFSetRef devices;
} DeviceInitialEnumArgs;

typedef struct __IOHIDManager
{
    IOHIDObjectBase                 hidBase;
    
    // Lifetime: Object, persists as long as IOHIDManager. Used for synchronization of general manager data.
    os_unfair_recursive_lock        managerLock;

    // Lifetime: Created when device is first added. Must be accessed under the managerLock.
    CFMutableSetRef                 devices;
    
    // Lifetime: Created when device is first added. Must be accessed under the managerLock.
    CFMutableSetRef                 iterators;
    // Lifetime: Created when device is first added. Must be accessed under the managerLock.
    CFMutableDictionaryRef          removalNotifiers;
    
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    CFMutableDictionaryRef          properties;
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    CFMutableDictionaryRef          deviceInputBuffers;

    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    IONotificationPortRef           notifyPort;
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    CFRunLoopRef                    runLoop;
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    CFStringRef                     runLoopMode;
    
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    dispatch_queue_t                dispatchQueue;
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    dispatch_block_t                cancelHandler;
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    uint32_t                        cancelCount;
    
    // Lifetime: Object. Use the atomic functions for access.
    _Atomic uint32_t                dispatchStateMask;
    
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    CFSetRef                        initDeviceSet;
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    CFRunLoopSourceRef              initialEnumSource;
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    CFMutableDictionaryRef          initRetVals;
    
    // Lifetime: Created when property is first added. Must be accessed under the managerLock.
    Boolean                         isOpen;
    // Lifetime: Object, created when first opened. Once set it doesn't change, safe to access without lock.
    IOOptionBits                    openOptions;
    // Lifetime: Object, created when manager is created. Once set it doesn't change, safe to access without lock.
    IOOptionBits                    createOptions;
    // Lifetime: Object, created when manager is created. Once set it doesn't change, safe to access without lock.
    Boolean                         isDirty;

    // Lifetime: Object, persists as long as IOHIDManager. Used for synchronization of callback data.
    os_unfair_recursive_lock        callbackLock;
    
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    void *                          inputContext;
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    IOHIDValueCallback              inputCallback;
    
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    void *                              reportContext;
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    IOHIDReportCallback                 reportCallback;
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    IOHIDReportWithTimeStampCallback    reportTimestampCallback;
    
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    void *                          matchContext;
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    IOHIDDeviceCallback             matchCallback;
    
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    void *                          removalContext;
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    IOHIDDeviceCallback             removalCallback;
    
    // Lifetime: Set when input handler is added. Requires callbackLock for synchronization.
    CFArrayRef                      inputMatchingMultiple;

    // Lifetime: Object, persists as long as IOHIDManager. Does not require locking for synchronization
    os_state_handle_t               stateHandler;
    // Lifetime: Object, persists as long as IOHIDManager. Does not require locking for synchronization
    dispatch_queue_t                stateQueue;

} __IOHIDManager, *__IOHIDManagerRef;

static const IOHIDObjectClass __IOHIDManagerClass = {
    {
        _kCFRuntimeCustomRefCount,  // version
        "IOHIDManager",             // className
        NULL,                       // init
        NULL,                       // copy
        __IOHIDManagerExtRelease,   // finalize
        NULL,                       // equal
        NULL,                       // hash
        NULL,                       // copyFormattingDesc
        NULL,                       // copyDebugDesc
        NULL,                       // reclaim
        _IOHIDObjectExtRetainCount, // refcount
        NULL                        // requiredAlignment
    },
    _IOHIDObjectIntRetainCount,
    __IOHIDManagerIntRelease
};

static pthread_once_t __sessionTypeInit = PTHREAD_ONCE_INIT;
static CFTypeID __kIOHIDManagerTypeID = _kCFRuntimeNotATypeID;

//------------------------------------------------------------------------------
// __IOHIDManagerRegister
//------------------------------------------------------------------------------
void __IOHIDManagerRegister(void)
{
    __kIOHIDManagerTypeID = _CFRuntimeRegisterClass(&__IOHIDManagerClass.cfClass);
}

//------------------------------------------------------------------------------
// __IOHIDManagerCreate
//------------------------------------------------------------------------------
IOHIDManagerRef __IOHIDManagerCreate(   
                                CFAllocatorRef              allocator, 
                                CFAllocatorContext *        context __unused)
{
    uint32_t        size;
    
    /* allocate service */
    size = sizeof(__IOHIDManager) - sizeof(CFRuntimeBase);
    
    return (IOHIDManagerRef)_IOHIDObjectCreateInstance(allocator, IOHIDManagerGetTypeID(), size, NULL);
}

//------------------------------------------------------------------------------
// __IOHIDManagerExtRelease
//------------------------------------------------------------------------------
void __IOHIDManagerExtRelease( CFTypeRef object )
{
    IOHIDManagerRef manager = (IOHIDManagerRef)object;
    
    os_unfair_recursive_lock_lock(&manager->managerLock);

    if (manager->dispatchQueue) {
        // enforce the call to activate/cancel
        os_assert(manager->dispatchStateMask == (kIOHIDDispatchStateActive | kIOHIDDispatchStateCancelled),
                  "Invalid dispatch state: 0x%x", manager->dispatchStateMask);
    }
    
    if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
        !(manager->createOptions & kIOHIDManagerOptionDoNotSaveProperties)) {
        __IOHIDManagerSaveProperties(manager, NULL);
    }
    
    if ( manager->isOpen ) {
        // This will unschedule the manager if it was opened
        IOHIDManagerClose(manager, manager->openOptions);
    }
    
    if ( manager->runLoop ) {
        // This will unschedule the manager if it wasn't
        IOHIDManagerUnscheduleFromRunLoop(manager, manager->runLoop, manager->runLoopMode);
    }
        
    // Remove the notification
    if (manager->notifyPort) {
        CFRunLoopSourceRef source = IONotificationPortGetRunLoopSource(manager->notifyPort);
        if (source) {
            CFRunLoopSourceInvalidate(source);
        }
    }

    if (manager->initialEnumSource) {
        CFRunLoopSourceInvalidate(manager->initialEnumSource);
    }
    
    if (manager->stateHandler) {
        os_state_remove_handler(manager->stateHandler);
    }
    
    if (manager->stateQueue) {
        dispatch_set_context(manager->stateQueue, manager);
        dispatch_set_finalizer_f(manager->stateQueue, __IOHIDManagerFinalizeStateHandler);
        _IOHIDObjectInternalRetain(manager);
        dispatch_release(manager->stateQueue);
    }

    os_unfair_recursive_lock_unlock(&manager->managerLock);
}

//------------------------------------------------------------------------------
// __IOHIDManagerIntRelease
//------------------------------------------------------------------------------
void __IOHIDManagerIntRelease( CFTypeRef object )
{
    IOHIDManagerRef manager = (IOHIDManagerRef)object;
    
    // Destroy the notification
    if ( manager->notifyPort ) {
        IONotificationPortDestroy(manager->notifyPort);
        manager->notifyPort = NULL;
    }

    if ( manager->initDeviceSet ) {
        CFRelease(manager->initDeviceSet);
        manager->initDeviceSet = NULL;
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
    
    if ( manager->initRetVals ) {
        CFRelease(manager->initRetVals);
        manager->initRetVals = NULL;
    }
    
    if ( manager->inputMatchingMultiple ) {
        CFRelease(manager->inputMatchingMultiple);
        manager->inputMatchingMultiple = NULL;
    }
    
    if (manager->dispatchQueue) {
        dispatch_release(manager->dispatchQueue);
    }
    
    if (manager->removalNotifiers) {
        CFRelease(manager->removalNotifiers);
    }

    if (manager->initialEnumSource) {
        CFRelease(manager->initialEnumSource);
        manager->initialEnumSource = NULL;
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
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    if (!manager->notifyPort) {
        manager->notifyPort = IONotificationPortCreate(kIOMainPortDefault);
        
        if (manager->runLoop)  {
            CFRunLoopSourceRef source = IONotificationPortGetRunLoopSource(manager->notifyPort);
            if (source) {
                CFRunLoopAddSource(
                            manager->runLoop,
                            source,
                            manager->runLoopMode);
            }
        }
    }
    os_unfair_recursive_lock_unlock(&manager->managerLock);

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

    if (kr != kIOReturnSuccess) {
        IOHIDLogError("IOServiceAddMatchingNotification:0x%x", kr);
        return;
    }
                
    os_unfair_recursive_lock_lock(&manager->managerLock);
    // Add iterator to set for later destruction
    if ( !manager->iterators ) {
        CFSetCallBacks callbacks;
        
        bzero(&callbacks, sizeof(CFSetCallBacks));
        
        callbacks.retain    = _IOObjectCFRetain;
        callbacks.release   = _IOObjectCFRelease;
        
        manager->iterators  = CFSetCreateMutable(
                                            CFGetAllocator(manager),
                                            0, 
                                            &callbacks);
        if ( !manager->iterators ) {
            os_unfair_recursive_lock_unlock(&manager->managerLock);
            return;
        }
    }
    
    intptr_t temp = iterator;
    CFSetAddValue(manager->iterators, (void *)temp);
    IOObjectRelease(iterator);
    os_unfair_recursive_lock_unlock(&manager->managerLock);

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
    io_object_t     notification;
    
    while (( service = IOIteratorNext(iterator) )) {
        
        device = IOHIDDeviceCreate(CFGetAllocator(manager), service);
        
        if ( device ) {
            os_unfair_recursive_lock_lock(&manager->managerLock);
            if ( !manager->devices ) {                
                manager->devices = CFSetCreateMutable(
                                                        CFGetAllocator(manager),
                                                        0, 
                                                        &kCFTypeSetCallBacks);
                initial = TRUE;
                
                if ( manager->isOpen )
                    manager->initRetVals = CFDictionaryCreateMutable(
                                            CFGetAllocator(manager),
                                            0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            NULL);
            }
            
            DeviceApplierArgs args;

            args.manager = manager;
            args.options = 0;
            
            retVal = IOServiceAddInterestNotification(manager->notifyPort,
                                                      service,
                                                      kIOGeneralInterest,
                                                      __IOHIDManagerDeviceRemoved,
                                                      manager,
                                                      &notification);
            
            if (!manager->removalNotifiers) {
                CFDictionaryValueCallBacks cb = kCFTypeDictionaryValueCallBacks;
                
                cb.retain = _IOObjectCFRetain;
                cb.release = _IOObjectCFRelease;
                
                manager->removalNotifiers = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                      0,
                                                                      &kCFTypeDictionaryKeyCallBacks,
                                                                      &cb);
            }
            
            if (retVal == kIOReturnSuccess) {
                CFDictionarySetValue(manager->removalNotifiers,
                                     device,
                                     (void *)(intptr_t)notification);
                
                IOObjectRelease(notification);
            } else {
                IOHIDLogError("IOServiceAddInterestNotification: 0x%x", retVal);
                CFRelease(device);
                IOObjectRelease(service);
                os_unfair_recursive_lock_unlock(&manager->managerLock);
                continue;
            }

            if ( manager->devices ) {
                CFSetAddValue(manager->devices, device);
            }
            os_unfair_recursive_lock_unlock(&manager->managerLock);
            
            retVal = kIOReturnSuccess;
            
            os_unfair_recursive_lock_lock(&manager->callbackLock);
            if ( manager->inputMatchingMultiple ) {
                args.options |= kDeviceApplierSetInputMatching;
            }
            
            if ( manager->inputCallback ) {
                args.options |= kDeviceApplierSetInputCallback;
            }

            if ( manager->reportCallback ) {
                args.options |= kDeviceApplierSetInputReportCallback;
            }
            
            if (manager->reportTimestampCallback) {
                args.options |= kDeviceApplierSetInputTSReportCallback;
            }

            if (manager->cancelHandler) {
                args.options |= kDeviceApplierSetCancelHandler;
            }
            os_unfair_recursive_lock_unlock(&manager->callbackLock);

            os_unfair_recursive_lock_lock(&manager->managerLock);
            if ( manager->isOpen ) {
                args.options |= kDeviceApplierOpen;
            }

            if ( manager->runLoop ) {
                args.options |= kDeviceApplierScheduleRunLoop;

                // If this this is called using the iterator returned in 
                // IOServiceAddMatchingNotification, pend performing the
                // callback on the runLoop
                if ( !initial && manager->matchCallback )
                    args.options |= kDeviceApplierInitEnumCallback;
            }
            
            if (manager->dispatchQueue) {
                args.options |= kDeviceApplierSetDispatchQueue;
                
                if (manager->matchCallback &&
                    manager->dispatchStateMask == kIOHIDDispatchStateActive)
                    args.options |= kDeviceApplierInitEnumCallback;
            }
            
            if (manager->dispatchStateMask & kIOHIDDispatchStateActive) {
                args.options |= kDeviceApplierActivate;
            }
            os_unfair_recursive_lock_unlock(&manager->managerLock);

            __IOHIDManagerDeviceApplier((const void *)device, &args);

            if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
                !(manager->createOptions & kIOHIDManagerOptionDoNotLoadProperties)) {
                __IOHIDDeviceLoadProperties(device);
            }

            os_unfair_recursive_lock_lock(&manager->managerLock);
            if (manager->properties) {
                CFDictionaryApplyFunction(manager->properties, 
                                          __IOHIDApplyPropertiesToDeviceFromDictionary, 
                                          device);
            }
            os_unfair_recursive_lock_unlock(&manager->managerLock);

            CFRelease(device);
        }
        
        IOObjectRelease(service);
    }
    
    // Dispatch initial enumeration callback on runLoop
    if ( initial ) {
        CFRunLoopSourceContext context;

        memset(&context, 0, sizeof(context));

        os_unfair_recursive_lock_lock(&manager->managerLock);

        context.info = manager;
        context.perform = __IOHIDManagerInitialEnumCallback;

        manager->initDeviceSet = CFSetCreateCopy(CFGetAllocator(manager), manager->devices);

        manager->initialEnumSource = CFRunLoopSourceCreate(CFGetAllocator(manager), 0, &context);

        if ( manager->runLoop && manager->runLoopMode ) {
            CFRunLoopAddSource(manager->runLoop, manager->initialEnumSource, manager->runLoopMode);
            CFRunLoopSourceSignal(manager->initialEnumSource);
            CFRunLoopWakeUp(manager->runLoop);
        }
        os_unfair_recursive_lock_unlock(&manager->managerLock);
    }
}

//------------------------------------------------------------------------------
// __IOHIDManagerInitialEnumCallback
//------------------------------------------------------------------------------
void __IOHIDManagerInitialEnumCallback(void * info)
{
    IOHIDManagerRef manager = (IOHIDManagerRef)info;
    DeviceApplierArgs args = { manager, kDeviceApplierInitEnumCallback, kIOReturnSuccess };
    
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    require_action(manager->matchCallback, exit, os_unfair_recursive_lock_unlock(&manager->callbackLock));
    os_unfair_recursive_lock_unlock(&manager->callbackLock);

    os_unfair_recursive_lock_lock(&manager->managerLock);
    CFSetApplyFunction(manager->initDeviceSet, __IOHIDManagerDeviceApplier, &args);
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    
exit:
    // After we have dispatched all of the enum callbacks, kill the source
    os_unfair_recursive_lock_lock(&manager->managerLock);
    
    if (manager->initialEnumSource) {
        CFRunLoopSourceInvalidate(manager->initialEnumSource);
        CFRelease(manager->initialEnumSource);
        manager->initialEnumSource = NULL;
    }

    if (manager->initDeviceSet) {
        CFRelease(manager->initDeviceSet);
        manager->initDeviceSet = NULL;
    }
    
    if (manager->initRetVals) {
        CFRelease(manager->initRetVals);
        manager->initRetVals = NULL;
    }
    os_unfair_recursive_lock_unlock(&manager->managerLock);
}

//------------------------------------------------------------------------------
// __IOHIDManagerDeviceRemoved
//------------------------------------------------------------------------------
void __IOHIDManagerDeviceRemoved(void *refcon,
                                 io_service_t service,
                                 uint32_t messageType,
                                 void *messageArgument __unused)
{
    IOHIDManagerRef manager = (IOHIDManagerRef)refcon;
    __block IOHIDDeviceRef sender = NULL;
    uint64_t regID = 0;
    
    IORegistryEntryGetRegistryEntryID(service, &regID);
    
    if (messageType != kIOMessageServiceIsTerminated) {
        return;
    }
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    _IOHIDCFSetApplyBlock(manager->devices, ^(CFTypeRef value) {
        IOHIDDeviceRef device = (IOHIDDeviceRef)value;
        
        if (!sender) {
            uint64_t deviceID = IOHIDDeviceGetRegistryEntryID(device);
            
            if (regID == deviceID) {
                sender = device;
            }
        }
    });
    
    if (!sender) {
        os_unfair_recursive_lock_unlock(&manager->managerLock);
        return;
    }
    
    CFDictionaryRemoveValue(manager->removalNotifiers, sender);
    
    if ( manager->deviceInputBuffers )
        CFDictionaryRemoveValue(manager->deviceInputBuffers, sender);


    if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
        !(manager->createOptions & kIOHIDManagerOptionDoNotSaveProperties)) {
        if (CFSetContainsValue(manager->devices, sender))
            __IOHIDDeviceSaveProperties((IOHIDDeviceRef)sender, NULL);
    }

    dispatch_queue_t dispatchQueue = manager->dispatchQueue;
    CFRunLoopRef runLoop = manager->runLoop;
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    
    if (!(manager->createOptions &kIOHIDManagerOptionIndependentDevices) &&
        dispatchQueue) {
        IOHIDDeviceCancel(sender);
        IOHIDDeviceActivate(sender);
    }
    
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    if (manager->removalCallback &&
        (runLoop|| manager->dispatchStateMask & kIOHIDDispatchStateActive)) {
        (*manager->removalCallback)(manager->removalContext,
                                    kIOReturnSuccess,
                                    manager,
                                    sender);
    }
    os_unfair_recursive_lock_unlock(&manager->callbackLock);
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    CFSetRemoveValue(manager->devices, sender);
    os_unfair_recursive_lock_unlock(&manager->managerLock);
}



//------------------------------------------------------------------------------
// __ApplyToDevices
//------------------------------------------------------------------------------
IOReturn __ApplyToDevices(IOHIDManagerRef manager, IOOptionBits options)
{
    CFSetRef devices = NULL;
    IOReturn ret = kIOReturnError;
    DeviceApplierArgs args = { manager, options, kIOReturnSuccess };
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    require_action(manager->devices, exit, os_unfair_recursive_lock_unlock(&manager->managerLock); ret = kIOReturnNoDevice);
    
    devices = CFSetCreateCopy(CFGetAllocator(manager), manager->devices);
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    
    require(devices, exit);
    
    CFSetApplyFunction(devices, __IOHIDManagerDeviceApplier, &args);
    ret = args.retVal;
    
    CFRelease(devices);
    
exit:
    return ret;
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
    IOHIDManagerRef     manager = args->manager;
    intptr_t            retVal  = kIOReturnSuccess;
    
    require_quiet((manager->createOptions & kIOHIDManagerOptionIndependentDevices) == 0, exit);
    
    if ( args->options & kDeviceApplierOpen ) {
        retVal = IOHIDDeviceOpen(           device,
                                            args->manager->openOptions);
        if ( args->manager->initRetVals )
            CFDictionarySetValue(args->manager->initRetVals, device, (void*)retVal);
    }

    if ( args->options & kDeviceApplierClose )
        retVal = IOHIDDeviceClose(          device,
                                            args->manager->openOptions);

    if ( args->options & kDeviceApplierSetInputMatching )
        IOHIDDeviceSetInputValueMatchingMultiple( 
                                            device,
                                            args->manager->inputMatchingMultiple);
    
    if ( args->options & kDeviceApplierSetInputCallback )
        IOHIDDeviceRegisterInputValueCallback( 
                                            device,
                                            args->manager->inputCallback,
                                            args->manager->inputContext);

    if ( args->options & kDeviceApplierSetInputReportCallback ||
         args->options & kDeviceApplierSetInputTSReportCallback ) {
        CFMutableDataRef dataRef = NULL;
        os_unfair_recursive_lock_lock(&args->manager->managerLock);
        if ( !args->manager->deviceInputBuffers ) {
            args->manager->deviceInputBuffers = CFDictionaryCreateMutable(CFGetAllocator(args->manager), 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        }

        dataRef = (CFMutableDataRef)CFDictionaryGetValue(args->manager->deviceInputBuffers, device);
        if ( !dataRef ) {
            CFNumberRef number = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDMaxInputReportSizeKey));
            CFIndex     length = 64;
            
            if ( number ) {
                CFNumberGetValue(number, kCFNumberCFIndexType, &length);
            }
            
            dataRef = CFDataCreateMutable(CFGetAllocator(args->manager), length);
            if ( dataRef ) {
                CFDataSetLength(dataRef, length);
                CFDictionarySetValue(args->manager->deviceInputBuffers, device, dataRef);
                CFRelease(dataRef);
            }
        }
        os_unfair_recursive_lock_unlock(&args->manager->managerLock);
        
        if (args->options & kDeviceApplierSetInputReportCallback) {
            IOHIDDeviceRegisterInputReportCallback(device,
                                                   (uint8_t *)CFDataGetMutableBytePtr(dataRef),
                                                   CFDataGetLength(dataRef),
                                                   args->manager->reportCallback,
                                                   args->manager->reportContext);
        } else {
            IOHIDDeviceRegisterInputReportWithTimeStampCallback(device,
                                                                (uint8_t *)CFDataGetMutableBytePtr(dataRef),
                                                                CFDataGetLength(dataRef),
                                                                args->manager->reportTimestampCallback,
                                                                args->manager->reportContext);
        }
    }
    
    if ( args->options & kDeviceApplierScheduleRunLoop )
        IOHIDDeviceScheduleWithRunLoop(     device,
                                            args->manager->runLoop,
                                            args->manager->runLoopMode);

    if ( args->options & kDeviceApplierUnscheduleRunLoop )
        IOHIDDeviceUnscheduleFromRunLoop(   device,
                                            args->manager->runLoop,
                                            args->manager->runLoopMode);
    
    if (args->options & kDeviceApplierSetDispatchQueue) {
        IOHIDDeviceSetDispatchQueue(device, manager->dispatchQueue);
    }
    
    if (args->options & kDeviceApplierSetCancelHandler) {
        manager->cancelCount++;
        
        IOHIDDeviceSetCancelHandler(device, ^{
            manager->cancelCount--;
            
            // once all of our devices have been cancelled
            // we call our manager's cancel handler
            if ((manager->dispatchStateMask & kIOHIDDispatchStateCancelled)
                && !manager->cancelCount) {
                if (manager->cancelHandler) {
                    (manager->cancelHandler)();
                    Block_release(manager->cancelHandler);
                    manager->cancelHandler = NULL;
                    _IOHIDObjectInternalRelease(manager);
                }
            }
        });
    }
    
    if (args->options & kDeviceApplierActivate) {
        IOHIDDeviceActivate(device);
    }
    
    if (args->options & kDeviceApplierCancel) {
        IOHIDDeviceCancel(device);
    }
    
    if ( args->retVal == kIOReturnSuccess && retVal != kIOReturnSuccess )
        args->retVal = retVal;
    
exit:
    if (args->options & kDeviceApplierInitEnumCallback) {
        if (args->manager->initRetVals) {
            retVal = (intptr_t)CFDictionaryGetValue(args->manager->initRetVals,
                                                    device);
        }
        
        (*args->manager->matchCallback)(args->manager->matchContext,
                                        retVal,
                                        args->manager,
                                        device);
    }
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
        return (_Nonnull IOHIDManagerRef)NULL;
    
    manager->managerLock = OS_UNFAIR_RECURSIVE_LOCK_INIT;
    manager->callbackLock = OS_UNFAIR_RECURSIVE_LOCK_INIT;
    manager->createOptions = options;
    
    if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
        !(manager->createOptions & kIOHIDManagerOptionDoNotLoadProperties))
    {
        __IOHIDManagerLoadProperties(manager);
    }
    
    manager->stateQueue = dispatch_queue_create("IOHIDManagerStateQueue", DISPATCH_QUEUE_SERIAL);
    require(manager->stateQueue, error);
    
    manager->stateHandler = os_state_add_handler(manager->stateQueue,
                                                ^os_state_data_t(os_state_hints_t hints) {
                                                    return __IOHIDManagerStateHandler(manager, hints);
                                                });
error:
    return manager;
}

//------------------------------------------------------------------------------
// IOHIDManagerOpen
//------------------------------------------------------------------------------
IOReturn IOHIDManagerOpen(
                                IOHIDManagerRef                 manager,
                                IOOptionBits                    options)
{
    IOReturn ret = kIOReturnSuccess;
    
    require(!manager->isOpen, exit);
    
    manager->isOpen = true;
    manager->openOptions = options;
    
    if (manager->devices) {
        IOOptionBits deviceOptions = kDeviceApplierOpen;
        
        if (manager->inputMatchingMultiple) {
            deviceOptions |= kDeviceApplierSetInputMatching;
        }
        
        if (manager->inputCallback) {
            deviceOptions |= kDeviceApplierSetInputCallback;
        }
        
        if (manager->reportCallback) {
            deviceOptions |= kDeviceApplierSetInputReportCallback;
        }
        
        if (manager->reportTimestampCallback) {
            deviceOptions |= kDeviceApplierSetInputTSReportCallback;
        }
        
        ret = __ApplyToDevices(manager, deviceOptions);
    }
    
exit:
    return ret;
}
                                
//------------------------------------------------------------------------------
// IOHIDManagerClose
//------------------------------------------------------------------------------
IOReturn IOHIDManagerClose(
                                IOHIDManagerRef                 manager,
                                IOOptionBits                    options)
{
    IOReturn ret = kIOReturnError;
    
    if (manager->runLoop) {
        IOHIDManagerUnscheduleFromRunLoop(manager, manager->runLoop, manager->runLoopMode);
    }
    
    require_action(manager->isOpen, exit, ret = kIOReturnNotOpen);
    
    manager->isOpen = false;
    manager->openOptions = options;
    
    if (manager->devices) {
        ret = __ApplyToDevices(manager, kDeviceApplierClose);
    } else {
        ret = kIOReturnSuccess;
    }
    
exit:
    if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
        !(manager->createOptions & kIOHIDManagerOptionDoNotSaveProperties)) {
        __IOHIDManagerSaveProperties(manager, NULL);
    }
    
    return ret;
}

//------------------------------------------------------------------------------
// IOHIDManagerGetProperty
//------------------------------------------------------------------------------
CFTypeRef IOHIDManagerGetProperty(
                                IOHIDManagerRef                 manager,
                                CFStringRef                     key)
{
    CFTypeRef value;
    os_unfair_recursive_lock_lock(&manager->managerLock);
    if ( !manager->properties ) {
        os_unfair_recursive_lock_unlock(&manager->managerLock);
        return NULL;
    }
    
    value = CFDictionaryGetValue(manager->properties, key);
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    return value;
}
                                
//------------------------------------------------------------------------------
// IOHIDManagerSetProperty
//------------------------------------------------------------------------------
Boolean IOHIDManagerSetProperty(
                                IOHIDManagerRef                 manager,
                                CFStringRef                     key,
                                CFTypeRef                       value)
{
    CFSetRef devices = NULL;
    bool result = false;
    __IOHIDApplyPropertyToSetContext context = { key, value };
        
    os_unfair_recursive_lock_lock(&manager->managerLock);
    if (!manager->properties) {
        manager->properties = CFDictionaryCreateMutable(CFGetAllocator(manager),
                                                        0,
                                                        &kCFTypeDictionaryKeyCallBacks,
                                                        &kCFTypeDictionaryValueCallBacks);
        require_action(manager->properties, exit, os_unfair_recursive_lock_unlock(&manager->managerLock));
    }
    
    manager->isDirty = TRUE;
    CFDictionarySetValue(manager->properties, key, value);
    
    require_action(manager->devices, exit, os_unfair_recursive_lock_unlock(&manager->managerLock); result = true);
    
    devices = CFSetCreateCopy(CFGetAllocator(manager), manager->devices);
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    
    require(devices, exit);
    
    CFSetApplyFunction(devices, __IOHIDApplyPropertyToDeviceSet, &context);
    CFRelease(devices);
    
    result = true;
    
exit:
    return result;
}
                                        
//------------------------------------------------------------------------------
// IOHIDManagerSetDeviceMatching
//------------------------------------------------------------------------------
void IOHIDManagerSetDeviceMatching(
                                IOHIDManagerRef                 manager,
                                CFDictionaryRef                 matching)
{
    CFArrayRef multiple = NULL;
    
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");
      
    if ( matching ) 
        multiple = CFArrayCreate(CFGetAllocator(manager),
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
    
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");

    os_unfair_recursive_lock_lock(&manager->managerLock);
    if ( manager->devices ) {
        if ( (manager->createOptions & kIOHIDManagerOptionUsePersistentProperties) && 
            !(manager->createOptions & kIOHIDManagerOptionDoNotSaveProperties)) {
            __IOHIDManagerSaveProperties(manager, NULL);
        }
        
        CFSetRemoveAllValues(manager->devices);
    }
    
    if (manager->removalNotifiers) {
        CFDictionaryRemoveAllValues(manager->removalNotifiers);
    }
        
    if (manager->iterators) {
        CFSetRemoveAllValues(manager->iterators);
    }

    if ( manager->deviceInputBuffers ) {
        CFDictionaryRemoveAllValues(manager->deviceInputBuffers);
    }

    if ( manager->initDeviceSet ) {
        CFRelease(manager->initDeviceSet);
        manager->initDeviceSet = NULL;
    }

    if ( multiple ) {
        count = CFArrayGetCount(multiple);
        for ( i=0; i<count; i++ ) {
            value = CFArrayGetValueAtIndex(multiple, i);
            if (CFDictionaryGetTypeID() == CFGetTypeID(value))
                __IOHIDManagerSetDeviceMatching(manager, (CFDictionaryRef)value);
        }
    }
    else {
        __IOHIDManagerSetDeviceMatching(manager, NULL);
    }

    if ( manager->devices ) {
        if (manager->initDeviceSet) {
            CFRelease(manager->initDeviceSet);
        }
        manager->initDeviceSet = CFSetCreateCopy(CFGetAllocator(manager), manager->devices);
    }
    os_unfair_recursive_lock_unlock(&manager->managerLock);
}

//------------------------------------------------------------------------------
// IOHIDManagerCopyDevices
//------------------------------------------------------------------------------
CFSetRef IOHIDManagerCopyDevices(
                                IOHIDManagerRef                 manager)
{
    CFSetRef devices = NULL;
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    require_action(manager->devices, exit,     os_unfair_recursive_lock_unlock(&manager->managerLock));
    
    devices = CFSetCreateCopy(CFGetAllocator(manager), manager->devices);
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    
exit:
    return devices;
}

//------------------------------------------------------------------------------
// IOHIDManagerRegisterDeviceMatchingCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterDeviceMatchingCallback(
                                IOHIDManagerRef                 manager,
                                IOHIDDeviceCallback             callback,
                                void *                          context)
{
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");
    
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    manager->matchCallback  = callback;
    manager->matchContext   = context;
    os_unfair_recursive_lock_unlock(&manager->callbackLock);
}

//------------------------------------------------------------------------------
// IOHIDManagerRegisterDeviceRemovalCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterDeviceRemovalCallback(
                                IOHIDManagerRef                 manager,
                                IOHIDDeviceCallback             callback,
                                void *                          context)
{
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");
    
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    manager->removalCallback    = callback;
    manager->removalContext     = context;
    os_unfair_recursive_lock_unlock(&manager->callbackLock);

}
                                        
//------------------------------------------------------------------------------
// IOHIDManagerRegisterInputReportCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterInputReportCallback( 
                                    IOHIDManagerRef             manager,
                                    IOHIDReportCallback         callback,
                                    void *                      context)
{
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");
    
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    manager->reportCallback  = callback;
    manager->reportContext   = context;
    os_unfair_recursive_lock_unlock(&manager->callbackLock);

    os_unfair_recursive_lock_lock(&manager->managerLock);
    require_action(manager->isOpen && manager->devices, exit, os_unfair_recursive_lock_unlock(&manager->managerLock));
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    
    __ApplyToDevices(manager, kDeviceApplierSetInputReportCallback);
    
exit:
    return;
}

//------------------------------------------------------------------------------
// IOHIDManagerRegisterInputReportWithTimeStampCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterInputReportWithTimeStampCallback(
                                    IOHIDManagerRef                    manager,
                                    IOHIDReportWithTimeStampCallback   callback,
                                    void *                             context)
{
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");
    
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    manager->reportTimestampCallback = callback;
    manager->reportContext = context;
    os_unfair_recursive_lock_unlock(&manager->callbackLock);
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    require_action(manager->isOpen && manager->devices, exit, os_unfair_recursive_lock_unlock(&manager->managerLock));
    os_unfair_recursive_lock_unlock(&manager->managerLock);

    
    __ApplyToDevices(manager, kDeviceApplierSetInputTSReportCallback);
    
exit:
    return;
}

//------------------------------------------------------------------------------
// IOHIDManagerRegisterInputValueCallback
//------------------------------------------------------------------------------
void IOHIDManagerRegisterInputValueCallback( 
                                    IOHIDManagerRef             manager,
                                    IOHIDValueCallback          callback,
                                    void *                      context)
{
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");
    
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    manager->inputCallback  = callback;
    manager->inputContext   = context;
    os_unfair_recursive_lock_unlock(&manager->callbackLock);
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    require_action(manager->isOpen && manager->devices, exit, os_unfair_recursive_lock_unlock(&manager->managerLock));
    os_unfair_recursive_lock_unlock(&manager->managerLock);
        
    __ApplyToDevices(manager, kDeviceApplierSetInputCallback);
    
exit:
    return;
}

//------------------------------------------------------------------------------
// IOHIDManagerSetInputValueMatching
//------------------------------------------------------------------------------
void IOHIDManagerSetInputValueMatching( 
                                    IOHIDManagerRef             manager,
                                    CFDictionaryRef             matching)
{
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");
    
    if ( matching ) {
        CFArrayRef multiple = CFArrayCreate(CFGetAllocator(manager), (const void **)&matching, 1, &kCFTypeArrayCallBacks);
        
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
    os_assert(manager->dispatchStateMask == kIOHIDDispatchStateInactive, "Manager has already been activated/cancelled.");
    
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    if ( manager->inputMatchingMultiple )
        CFRelease(manager->inputMatchingMultiple);
        
    if ( multiple )
        CFRetain(multiple);
        
    manager->inputMatchingMultiple = multiple;
    os_unfair_recursive_lock_unlock(&manager->callbackLock);

    os_unfair_recursive_lock_lock(&manager->managerLock);
    require_action(manager->isOpen && manager->devices, exit, os_unfair_recursive_lock_unlock(&manager->managerLock));
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    
    __ApplyToDevices(manager, kDeviceApplierSetInputMatching);
    
exit:
    return;
}

//------------------------------------------------------------------------------
// IOHIDManagerScheduleWithRunLoop
//------------------------------------------------------------------------------
void IOHIDManagerScheduleWithRunLoop(
                                        IOHIDManagerRef         manager,
                                        CFRunLoopRef            runLoop, 
                                        CFStringRef             runLoopMode)
{
    os_unfair_recursive_lock_lock(&manager->managerLock);
    os_assert(!manager->runLoop && !manager->dispatchQueue,
              "Schedule failed queue: %p runLoop: %p", manager->dispatchQueue, manager->runLoop);
    
    manager->runLoop        = runLoop;
    manager->runLoopMode    = runLoopMode;

    if ( manager->runLoop ) {

        // schedule the initial enumeration routine
        if ( manager->initDeviceSet && manager->initialEnumSource) {
            CFRunLoopAddSource(manager->runLoop, manager->initialEnumSource, manager->runLoopMode);
            CFRunLoopSourceSignal(manager->initialEnumSource);
            CFRunLoopWakeUp(manager->runLoop);
        }

        // Schedule the notifyPort
        if (manager->notifyPort) {
            CFRunLoopSourceRef source = IONotificationPortGetRunLoopSource(manager->notifyPort);
            if (source) {
                CFRunLoopAddSource(
                            manager->runLoop,
                            source,
                            manager->runLoopMode);
            }
        }
        os_unfair_recursive_lock_unlock(&manager->managerLock);
        
        // Schedule the devices
        __ApplyToDevices(manager, kDeviceApplierScheduleRunLoop);
    } else {
        os_unfair_recursive_lock_unlock(&manager->managerLock);
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
    os_unfair_recursive_lock_lock(&manager->managerLock);
    if (!manager->runLoop) {
        os_unfair_recursive_lock_unlock(&manager->managerLock);
        return;
    }
    
    if (!CFEqual(manager->runLoop, runLoop) || 
        !CFEqual(manager->runLoopMode, runLoopMode)) {
        os_unfair_recursive_lock_unlock(&manager->managerLock);
        return;
    }
    
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    // Unschedule the devices
    __ApplyToDevices(manager, kDeviceApplierUnscheduleRunLoop);
    
    // Unschedule the initial enumeration routine
    os_unfair_recursive_lock_lock(&manager->managerLock);
    // stop receiving device notifications
    if (manager->iterators) {
        CFSetRemoveAllValues(manager->iterators);
    }
    
    if (manager->removalNotifiers) {
        CFDictionaryRemoveAllValues(manager->removalNotifiers);
    }

    manager->runLoop        = NULL;
    manager->runLoopMode    = NULL;
    os_unfair_recursive_lock_unlock(&manager->managerLock);
}

//------------------------------------------------------------------------------
// IOHIDManagerSetDispatchQueue
//------------------------------------------------------------------------------

void IOHIDManagerSetDispatchQueue(IOHIDManagerRef manager, dispatch_queue_t queue)
{
    os_unfair_recursive_lock_lock(&manager->managerLock);
    os_assert(!manager->runLoop && !manager->dispatchQueue);
    
    char label[256] = {0};
    
    snprintf(label, sizeof(label), "%s.IOHIDManagerRef", dispatch_queue_get_label (queue) ? dispatch_queue_get_label (queue) : "");

    manager->dispatchQueue = dispatch_queue_create_with_target(label, DISPATCH_QUEUE_SERIAL, queue);
    require_action(manager->dispatchQueue, exit, os_unfair_recursive_lock_unlock(&manager->managerLock));
    
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    __ApplyToDevices(manager, kDeviceApplierSetDispatchQueue);

exit:
    return;
}

//------------------------------------------------------------------------------
// IOHIDManagerSetCancelHandler
//------------------------------------------------------------------------------
void IOHIDManagerSetCancelHandler(IOHIDManagerRef manager, dispatch_block_t handler)
{
    os_unfair_recursive_lock_lock(&manager->callbackLock);
    os_assert(!manager->cancelHandler && handler);
    
    _IOHIDObjectInternalRetain(manager);
    manager->cancelHandler = Block_copy(handler);
    os_unfair_recursive_lock_unlock(&manager->callbackLock);
    
    __ApplyToDevices(manager, kDeviceApplierSetCancelHandler);
}

//------------------------------------------------------------------------------
// IOHIDManagerActivate
//------------------------------------------------------------------------------
void IOHIDManagerActivate(IOHIDManagerRef manager)
{
    IOOptionBits deviceOptions = 0;

    os_assert(manager->dispatchQueue && !manager->runLoop,
              "Activate failed queue: %p runLoop: %p", manager->dispatchQueue, manager->runLoop);

    if (atomic_fetch_or(&manager->dispatchStateMask, kIOHIDDispatchStateActive) & kIOHIDDispatchStateActive) {
        return;
    }

    deviceOptions = kDeviceApplierActivate;

    os_unfair_recursive_lock_lock(&manager->callbackLock);
    if (manager->matchCallback) {
        deviceOptions |= kDeviceApplierInitEnumCallback;
    }
    os_unfair_recursive_lock_unlock(&manager->callbackLock);

    __ApplyToDevices(manager, deviceOptions);

    os_unfair_recursive_lock_lock(&manager->managerLock);
    if (manager->notifyPort) {
        IONotificationPortSetDispatchQueue(manager->notifyPort, manager->dispatchQueue);
    }
    os_unfair_recursive_lock_unlock(&manager->managerLock);
}

//------------------------------------------------------------------------------
// IOHIDManagerCancel
//------------------------------------------------------------------------------
void IOHIDManagerCancel(IOHIDManagerRef manager)
{
    os_assert(manager->dispatchQueue && !manager->runLoop,
              "Cancel failed queue: %p runLoop: %p", manager->dispatchQueue, manager->runLoop);
    
    if (atomic_fetch_or(&manager->dispatchStateMask, kIOHIDDispatchStateCancelled) & kIOHIDDispatchStateCancelled) {
        return;
    }
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    if (manager->notifyPort) {
        IONotificationPortDestroy(manager->notifyPort);
        manager->notifyPort = NULL;
    }
    
    // stop receiving device notifications
    if (manager->iterators) {
        CFSetRemoveAllValues(manager->iterators);
    }
    
    if (manager->removalNotifiers) {
        CFDictionaryRemoveAllValues(manager->removalNotifiers);
    }
    
    if (manager->devices &&
        CFSetGetCount(manager->devices) &&
        manager->cancelCount) {
        __ApplyToDevices(manager, kDeviceApplierCancel);
    } else if (manager->cancelHandler) {
        (manager->cancelHandler)();
        Block_release(manager->cancelHandler);
        manager->cancelHandler = NULL;
        _IOHIDObjectInternalRelease(manager);
    }
    os_unfair_recursive_lock_unlock(&manager->managerLock);
}

//------------------------------------------------------------------------------
// IOHIDManagerSaveToPropertyDomain
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
    os_unfair_recursive_lock_lock(&manager->managerLock);
    if (manager->isDirty && manager->properties) {
        __IOHIDPropertySaveToKeyWithSpecialKeys(manager->properties, 
                                               __IOHIDManagerGetRootKey(),
                                               NULL,
                                               context);
        manager->isDirty = FALSE;
    }
    
    if (manager->devices) {
        CFSetRef devices = NULL;
        
        devices = CFSetCreateCopy(CFGetAllocator(manager), manager->devices);
        
        require(devices, exit);
        
        CFSetApplyFunction(devices, __IOHIDSaveDeviceSet, context);
        CFRelease(devices);
    }
    
exit:
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    return;
}

//------------------------------------------------------------------------------
void __IOHIDManagerLoadProperties(IOHIDManagerRef manager)
{
    // Convert to __IOHIDPropertyLoadFromKeyWithSpecialKeys if we identify special keys
    CFMutableDictionaryRef properties = __IOHIDPropertyLoadDictionaryFromKey(__IOHIDManagerGetRootKey());
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    if (properties) {
        CFRELEASE_IF_NOT_NULL(manager->properties);
        manager->properties = properties;
        manager->isDirty = FALSE;
    }
    os_unfair_recursive_lock_unlock(&manager->managerLock);
    
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

//------------------------------------------------------------------------------
// __IOHIDManagerFinalizeStateHandler
//------------------------------------------------------------------------------
void __IOHIDManagerFinalizeStateHandler (void *context)
{
    IOHIDManagerRef manager = (IOHIDManagerRef)context;
    _IOHIDObjectInternalRelease(manager);
}

//------------------------------------------------------------------------------
// __IOHIDManagerStateHandler
//------------------------------------------------------------------------------
os_state_data_t __IOHIDManagerStateHandler (IOHIDManagerRef device, os_state_hints_t hints)
{
    os_state_data_t stateData = NULL;
    CFMutableDictionaryRef deviceState = NULL;
    CFDataRef serializedDeviceState = NULL;
    
    if (hints->osh_api != OS_STATE_API_FAULT &&
        hints->osh_api != OS_STATE_API_REQUEST) {
        return NULL;
    }
    
    deviceState = __IOHIDManagerSerializeState(device);
    require(deviceState, exit);
    
    serializedDeviceState = CFPropertyListCreateData(kCFAllocatorDefault, deviceState, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    require(serializedDeviceState, exit);
    
    uint32_t serializedDeviceStateSize = (uint32_t)CFDataGetLength(serializedDeviceState);
    stateData = calloc(1, OS_STATE_DATA_SIZE_NEEDED(serializedDeviceStateSize));
    require(stateData, exit);
    
    strlcpy(stateData->osd_title, "IOHIDManager State", sizeof(stateData->osd_title));
    stateData->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
    stateData->osd_data_size = serializedDeviceStateSize;
    CFDataGetBytes(serializedDeviceState, CFRangeMake(0, serializedDeviceStateSize), stateData->osd_data);
    
exit:
    if (deviceState) {
        CFRelease(deviceState);
    }
    
    if (serializedDeviceState) {
        CFRelease(serializedDeviceState);
    }
    
    return stateData;
}

//------------------------------------------------------------------------------
// __IOHIDUserDeviceSerializeState
//------------------------------------------------------------------------------
CFMutableDictionaryRef __IOHIDManagerSerializeState(IOHIDManagerRef manager)
{
    CFMutableDictionaryRef state = NULL;
    CFSetRef devices = NULL;
    state = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                      0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
    require(state, exit);
    
    os_unfair_recursive_lock_lock(&manager->managerLock);
    CFDictionarySetValue(state, CFSTR("DispatchQueue"), manager->dispatchQueue ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(state, CFSTR("RunLoop"), manager->runLoop ? kCFBooleanTrue : kCFBooleanFalse);
    _IOHIDDictionaryAddSInt32(state, CFSTR("openOptions"), manager->openOptions);
    _IOHIDDictionaryAddSInt32(state, CFSTR("createOptions"), manager->createOptions);
    CFDictionarySetValue(state, CFSTR("isOpen"), manager->isOpen ? kCFBooleanTrue : kCFBooleanFalse);
    
    if (manager->devices) {
        devices = CFSetCreateCopy(CFGetAllocator(manager->devices), manager->devices);
    }
    
    if (devices) {
        CFMutableArrayRef deviceList = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if (deviceList) {
            _IOHIDCFSetApplyBlock (devices, ^(CFTypeRef value) {
                IOHIDDeviceRef device =  (IOHIDDeviceRef) value;
                uint64_t regID  = 0;
                io_service_t service = IOHIDDeviceGetService (device);
                if (service) {
                    IORegistryEntryGetRegistryEntryID(service, &regID);
                    _IOHIDArrayAppendSInt64 (deviceList, regID);
                }
            });
            CFDictionarySetValue(state, CFSTR("devices"), deviceList);
            CFRelease(deviceList);
        }
        CFRelease(devices);
    }
    os_unfair_recursive_lock_unlock(&manager->managerLock);

exit:
    
    return state;
}
