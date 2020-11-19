/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
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
#import <IOKit/IODataQueueClient.h>
#import "IOHIDQueueClass.h"
#import "HIDLibElement.h"
#import <AssertMacros.h>
#import "IOHIDLibUserClient.h"
#import <IOKit/hid/IOHIDLibPrivate.h>
#import "IOHIDDebug.h"
#import <IOKit/hid/IOHIDAnalytics.h>

@implementation IOHIDQueueClass

- (HRESULT)queryInterface:(REFIID)uuidBytes
             outInterface:(LPVOID *)outInterface
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, uuidBytes);
    HRESULT result = E_NOINTERFACE;
    
    if (CFEqual(uuid, kIOHIDDeviceQueueInterfaceID)) {
        *outInterface = (LPVOID *)&_queue;
        CFRetain((CFTypeRef)self);
        result = S_OK;
    }
    
    if (uuid) {
        CFRelease(uuid);
    }
    
    return result;
}

static IOReturn _getAsyncEventSource(void *iunknown, CFTypeRef *pSource)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getAsyncEventSource:pSource];
}

- (IOReturn)getAsyncEventSource:(CFTypeRef *)pSource
{
    if (!pSource) {
        return kIOReturnBadArgument;
    }
    
    *pSource = _runLoopSource;
    
    return kIOReturnSuccess;
}

static IOReturn _setDepth(void *iunknown,
                          uint32_t depth,
                          IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setDepth:depth];
}

- (IOReturn)setDepth:(uint32_t)depth
{
    _depth = depth;
    return kIOReturnSuccess;
}

static IOReturn _getDepth(void *iunknown, uint32_t *pDepth)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getDepth:pDepth];
}

- (IOReturn)getDepth:(uint32_t *)pDepth
{
    if (!pDepth) {
        return kIOReturnBadArgument;
    }
    
    *pDepth = _depth;
    
    return kIOReturnSuccess;
}

static IOReturn _addElement(void *iunknown,
                            IOHIDElementRef element,
                            IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me addElement:element];
}

- (IOReturn)addElement:(IOHIDElementRef)element
{
    IOReturn ret = kIOReturnError;
    uint64_t input[3] = { 0 };
    uint64_t sizeChange;
    uint32_t outputCount = 1;
    
    if (!element) {
        return kIOReturnBadArgument;
    }
    
    input[0] = _queueToken;
    input[1] = (uint64_t)IOHIDElementGetCookie(element);
    
    ret = IOConnectCallScalarMethod(_device.connect,
                                    kIOHIDLibUserClientAddElementToQueue,
                                    input,
                                    3,
                                    &sizeChange,
                                    &outputCount);
    _queueSizeChanged |= sizeChange;
    
    return ret;
}

static IOReturn _removeElement(void *iunknown,
                               IOHIDElementRef element,
                               IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me removeElement:element];
}

- (IOReturn)removeElement:(IOHIDElementRef)element
{
    IOReturn ret = kIOReturnError;
    uint64_t input[2];
    uint64_t sizeChange;
    uint32_t outputCount = 1;
    
    if (!element) {
        return kIOReturnBadArgument;
    }
    
    input[0] = _queueToken;
    input[1] = (uint64_t)IOHIDElementGetCookie(element);
    
    ret = IOConnectCallScalarMethod(_device.connect,
                                    kIOHIDLibUserClientRemoveElementFromQueue,
                                    input,
                                    2,
                                    &sizeChange,
                                    &outputCount);
    _queueSizeChanged |= sizeChange;
    
    return ret;
}

static IOReturn _containsElement(void *iunknown,
                                 IOHIDElementRef element,
                                 Boolean *pValue,
                                 IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me containsElement:element pValue:pValue];
}

- (IOReturn)containsElement:(IOHIDElementRef)element pValue:(Boolean *)pValue
{
    IOReturn ret = kIOReturnError;
    uint64_t input[2];
    uint64_t containsElement;
    uint32_t outputCount = 1;
    
    if (!element || !pValue) {
        return kIOReturnBadArgument;
    }
    
    input[0] = _queueToken;
    input[1] = (uint64_t)IOHIDElementGetCookie(element);
    
    ret = IOConnectCallScalarMethod(_device.connect,
                                    kIOHIDLibUserClientQueueHasElement,
                                    input,
                                    2,
                                    &containsElement,
                                    &outputCount);
    
    *pValue = containsElement;
    
    return ret;
}

static IOReturn _start(void *iunknown, IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me start];
}

- (IOReturn)start
{
    IOReturn ret = IOConnectCallScalarMethod(_device.connect,
                                             kIOHIDLibUserClientStartQueue,
                                             &_queueToken,
                                             1,
                                             NULL,
                                             NULL);
    
    // If the size of our queue changed after adding/remove elements, we will
    // have to remap our kernel memory.
    if (!_queueMemory || _queueSizeChanged) {
        [self mapMemory];
        _queueSizeChanged = false;
    }
    
    return ret;
}

static IOReturn _stop(void *iunknown, IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me stop];
}

- (IOReturn)stop
{
    return IOConnectCallScalarMethod(_device.connect,
                                     kIOHIDLibUserClientStopQueue,
                                     &_queueToken,
                                     1,
                                     NULL,
                                     NULL);
}

static IOReturn _setValueAvailableCallback(void *iunknown,
                                           IOHIDCallback callback,
                                           void *context)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setValueAvailableCallback:callback context:context];
}

- (IOReturn)setValueAvailableCallback:(IOHIDCallback)callback
                              context:(void *)context
{
    _valueAvailableCallback = callback;
    _valueAvailableContext = context;
    
    return kIOReturnSuccess;
}

static IOReturn _copyNextValue(void *iunknown,
                               IOHIDValueRef *pValue,
                               uint32_t timeout __unused,
                               IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me copyNextValue:pValue];
}

- (IOReturn)copyNextValue:(IOHIDValueRef *)pValue
{
    IOReturn ret = kIOReturnError;
    IODataQueueEntry *entry = NULL;
    IOHIDElementValue *elementValue = NULL;
    uint32_t cookie;
    uint32_t dataSize;
    
    require_action(pValue, exit, ret = kIOReturnBadArgument);

    [self updateUsageAnalytics];
    
    entry = IODataQueuePeek(_queueMemory);
    require_action(entry, exit, ret = kIOReturnUnderrun);
    
    elementValue = (IOHIDElementValue *)&(entry->data);
    cookie = (uint32_t)elementValue->cookie;
    
    *pValue = _IOHIDValueCreateWithElementValuePtr(kCFAllocatorDefault,
                                                   [_device getElement:cookie],
                                                   elementValue);
    if (*pValue && _IOHIDValueGetFlags(*pValue) & kIOHIDElementValueOOBReport) {
        uint64_t * reportAddress = (uint64_t *)elementValue->value;
        [_device releaseOOBReport:*reportAddress];
    }
    IODataQueueDequeue(_queueMemory, NULL, &dataSize);
    require(*pValue, exit);
    
    ret = kIOReturnSuccess;
exit:
    return ret;
}

static void _queueCallback(CFMachPortRef port,
                           mach_msg_header_t *msg,
                           CFIndex size,
                           void *info)
{
    IOHIDQueueClass *me = (__bridge id)info;
    
    [me queueCallback:port msg:msg size:size info:info];
}

- (void)queueCallback:(CFMachPortRef __unused)port
                  msg:(mach_msg_header_t * __unused)msg
                 size:(CFIndex __unused)size
                 info:(void * __unused)info
{
    if (_valueAvailableCallback) {
        (_valueAvailableCallback)(_valueAvailableContext,
                                  kIOReturnSuccess,
                                  (void *)&_queue);
    }
}

- (void)unmapMemory
{
#if !__LP64__
    vm_address_t        mappedMem = (vm_address_t)_queueMemory;
#else
    mach_vm_address_t   mappedMem = (mach_vm_address_t)_queueMemory;
#endif
    
    if (_queueMemory) {
        IOConnectUnmapMemory(_device.connect,
                             (uint32_t)_queueToken,
                             mach_task_self(),
                             mappedMem);
        
        _queueMemory = NULL;
        _queueMemorySize = 0;
    }

    if (_usageAnalytics) {
        IOHIDAnalyticsEventCancel(_usageAnalytics);
        CFRelease(_usageAnalytics);
        _usageAnalytics = NULL;
    }
}

- (void)mapMemory
{
#if !__LP64__
    vm_address_t        mappedMem = (vm_address_t)0;
    vm_size_t           memSize = 0;
#else
    mach_vm_address_t   mappedMem = (mach_vm_address_t)0;
    mach_vm_size_t      memSize = 0;
#endif
    
    [self unmapMemory];
    
    IOConnectMapMemory(_device.connect,
                       (uint32_t)_queueToken,
                       mach_task_self(),
                       &mappedMem,
                       &memSize,
                       kIOMapAnywhere);
    
    _queueMemory = (IODataQueueMemory *)mappedMem;
    _queueMemorySize = memSize;

    [self setupAnalytics];
}

- (nullable instancetype)initWithDevice:(IOHIDDeviceClass *)device
{
    return [self initWithDevice:device port:MACH_PORT_NULL source:nil];
}

- (instancetype)initWithDevice:(IOHIDDeviceClass *)device
                          port:(mach_port_t)port
                        source:(CFRunLoopSourceRef)source
{
    IOReturn ret = kIOReturnError;
    uint64_t input[2] = { 0 };
    uint64_t output;
    uint32_t outputCount = 1;
    io_async_ref64_t async;
    CFMachPortContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };
    
    self = [super init];
    
    if (!self) {
        return nil;
    }
    
    _device = device;
    
    _queue = (IOHIDDeviceQueueInterface *)malloc(sizeof(*_queue));
    
    *_queue = (IOHIDDeviceQueueInterface) {
        // IUNKNOWN_C_GUTS
        ._reserved = (__bridge void *)self,
        .QueryInterface = self->_vtbl->QueryInterface,
        .AddRef = self->_vtbl->AddRef,
        .Release = self->_vtbl->Release,
        
        // IOHIDDeviceQueueInterface
        .getAsyncEventSource = _getAsyncEventSource,
        .setDepth = _setDepth,
        .getDepth = _getDepth,
        .addElement = _addElement,
        .removeElement = _removeElement,
        .containsElement = _containsElement,
        .start = _start,
        .stop = _stop,
        .setValueAvailableCallback = _setValueAvailableCallback,
        .copyNextValue = _copyNextValue
    };
    
    ret = IOConnectCallScalarMethod(_device.connect,
                                    kIOHIDLibUserClientCreateQueue,
                                    input,
                                    2,
                                    &output,
                                    &outputCount);
    require_noerr(ret, exit);
    
    _queueToken = output;
    
    if (port == MACH_PORT_NULL) {
    
        _port = IODataQueueAllocateNotificationPort();
        require_action(_port, exit, ret = kIOReturnNoMemory);
        
        _machPort = CFMachPortCreateWithPort(kCFAllocatorDefault,
                                             _port,
                                             (CFMachPortCallBack)_queueCallback,
                                             &context, NULL);
        require_action(_machPort, exit, ret = kIOReturnNoMemory);
    } else {
        _port = port;
    }
    
    if (!source) {
        _runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault,
                                                       _machPort,
                                                       0);
        require_action(_runLoopSource, exit, ret = kIOReturnNoMemory);
    } else {
        _runLoopSource = source;
        CFRetain(_runLoopSource);
    }
    
    ret = IOConnectCallAsyncScalarMethod(_device.connect,
                                         kIOHIDLibUserClientSetQueueAsyncPort,
                                         _port,
                                         async,
                                         1,
                                         &_queueToken,
                                         1,
                                         NULL,
                                         NULL);
    require_noerr(ret, exit);
    
exit:
    if (ret != kIOReturnSuccess) {
        HIDLogInfo("Failed to create IOHIDQueue plugin result: 0x%x", ret);
        return nil;
    }
    
    return self;
}

- (void)dealloc
{
    IOConnectCallScalarMethod(_device.connect,
                              kIOHIDLibUserClientDisposeQueue,
                              &_queueToken,
                              1,
                              NULL,
                              NULL);
    
    if (_queue) {
        free(_queue);
    }
    
    if (_runLoopSource) {
        CFRelease(_runLoopSource);
    }
    
    if (_machPort) {
        CFMachPortInvalidate(_machPort);
        CFRelease(_machPort);
        
        // presence of mach port indicates we had to create our own
        // mach_port_t rather than using the device's, so deallocate it.
        if (_port) {
            mach_port_mod_refs(mach_task_self(),
                               _port,
                               MACH_PORT_RIGHT_RECEIVE,
                               -1);
        }
    }
    
    [self unmapMemory];
}

- (bool)setupAnalytics
{
    bool                   result = false;
    NSMutableDictionary *  eventDesc = [@{ @"staticSize"    : @(_queueMemorySize),
                                           @"queueType"     : @"deviceQueue"
                                        } mutableCopy];
    IOHIDAnalyticsHistogramSegmentConfig analyticsConfig = {
        .bucket_count       = 8,
        .bucket_width       = 13,
        .bucket_base        = 0,
        .value_normalizer   = 1,
    };
    NSDictionary *pairs = CFBridgingRelease(IORegistryEntryCreateCFProperty(
                                                                            _device.service,
                                                                            CFSTR(kIOHIDDeviceUsagePairsKey),
                                                                            kCFAllocatorDefault,
                                                                            0));
    NSString *transport = CFBridgingRelease(IORegistryEntryCreateCFProperty(
                                                                            _device.service,
                                                                            CFSTR(kIOHIDTransportKey),
                                                                            kCFAllocatorDefault,
                                                                            0));

    if (pairs) {
        eventDesc[@"usagePairs"] = pairs;
    }
    if (transport) {
        eventDesc[@"transport"] = transport;
    }

    
    
    _usageAnalytics = IOHIDAnalyticsHistogramEventCreate(CFSTR("com.apple.hid.queueUsage"), (__bridge CFDictionaryRef)eventDesc, CFSTR("UsagePercent"), &analyticsConfig, 1);

    require_action(_usageAnalytics, exit, HIDLogError("Unable to create queue analytics"));
    
    IOHIDAnalyticsEventActivate(_usageAnalytics);
   

    result = true;

exit:
    return result;
}

- (void)updateUsageAnalytics
{
    uint32_t head;
    uint32_t tail;
    uint64_t queueUsage;

    require(_queueMemory, exit);
    require(_usageAnalytics, exit);

    head = (uint32_t)_queueMemory->head;
    tail = (uint32_t)_queueMemory->tail;

    // Submit queue usage at local maximum queue size.
    // (first call to dequeue in a series w/o enqueue)
    if (tail == _lastTail) {
        return;
    }

    if (head < tail) {
        queueUsage = tail - head;
    }
    else {
        queueUsage = _queueMemorySize - (head - tail);
    }
    queueUsage = (queueUsage * 100) / _queueMemorySize;

    IOHIDAnalyticsHistogramEventSetIntegerValue(_usageAnalytics, queueUsage);

    _lastTail = tail;

exit:
    return;
}


@end

@implementation IOHIDObsoleteQueueClass

- (HRESULT)queryInterface:(REFIID)uuidBytes
             outInterface:(LPVOID *)outInterface
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, uuidBytes);
    HRESULT result = E_NOINTERFACE;
    
    if (CFEqual(uuid, kIOHIDQueueInterfaceID)) {
        *outInterface = (LPVOID *)&_interface;
        CFRetain((CFTypeRef)self);
        result = S_OK;
    }
    
    if (uuid) {
        CFRelease(uuid);
    }
    
    return result;
}

static IOReturn _createAsyncEventSource(void *iunknown,
                                        CFRunLoopSourceRef *source)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    if (!source) {
        return kIOReturnBadArgument;
    }
    
    CFRetain(me->_runLoopSource);
    *source = me->_runLoopSource;
    
    return kIOReturnSuccess;
}

static CFRunLoopSourceRef _getAsyncEventSource(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return me->_runLoopSource;
}

static IOReturn _createAsyncPort(void *iunknown, mach_port_t *port)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    *port = me->_port;
    return kIOReturnSuccess;
}

static mach_port_t _getAsyncPort(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return me->_port;
}

static IOReturn _create(void *iunknown __unused,
                        uint32_t flags __unused,
                        uint32_t depth __unused)
{
    return kIOReturnSuccess;
}

static IOReturn _dispose(void *iunknown __unused)
{
    return kIOReturnSuccess;
}

static IOReturn _addElement(void *iunknown,
                            IOHIDElementCookie elementCookie,
                            uint32_t flags __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me addElement:[me->_device getElement:(uint32_t)elementCookie]];
}

static IOReturn _removeElement(void *iunknown, IOHIDElementCookie elementCookie)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me removeElement:[me->_device getElement:(uint32_t)elementCookie]];
}

static Boolean _hasElement(void *iunknown, IOHIDElementCookie elementCookie)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    Boolean contains = false;
    
    [me containsElement:[me->_device getElement:(uint32_t)elementCookie]
                 pValue:&contains];
    
    return contains;
}

static IOReturn _start(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me start];
}

static IOReturn _stop(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me stop];
}

static IOReturn _getNextEvent(void *iunknown,
                       IOHIDEventStruct *event,
                       AbsoluteTime maxTime __unused,
                       uint32_t timeoutMS __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getNextEvent:event];
}

- (IOReturn)getNextEvent:(IOHIDEventStruct *)event
{
    IOHIDValueRef value = NULL;
    IOReturn ret = kIOReturnBadArgument;
    IOHIDElementRef elementRef = NULL;
    HIDLibElement *element = nil;
    uint32_t length;
    
    if (!event) {
        return kIOReturnBadArgument;
    }
    
    ret = [self copyNextValue:&value];
    require(ret == kIOReturnSuccess && value, exit);
    
    elementRef = IOHIDValueGetElement(value);
    element = [[HIDLibElement alloc] initWithElementRef:elementRef];
    element.valueRef = value;

    length = (uint32_t)element.length;
    event->type = element.type;
    event->elementCookie = (IOHIDElementCookie)element.elementCookie;
    *(UInt64 *)&event->timestamp = element.timestamp;
    
    if (length > sizeof(uint32_t)) {
        event->longValueSize = length;
        event->longValue = malloc(length);
        bcopy(IOHIDValueGetBytePtr(value), event->longValue, length);
    } else {
        event->longValueSize = 0;
        event->longValue = NULL;
        event->value = (int32_t)element.integerValue;
    }
    
    CFRelease(value);
exit:
    return ret;
}

static void _eventCallout(void *context, IOReturn result, void *sender __unused)
{
    IOHIDObsoleteQueueClass *me = (__bridge IOHIDObsoleteQueueClass *)context;
    
    if (me->_eventCallback) {
        (*me->_eventCallback)(me->_eventCallbackTarget,
                              result,
                              me->_eventCallbackRefcon,
                              (void *)&me->_interface);
    }
}

static IOReturn _setEventCallout(void *iunknown,
                                 IOHIDCallbackFunction callback,
                                 void *callbackTarget,
                                 void *callbackRefcon)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteQueueClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setEventCallout:callback
                callbackTarget:callbackTarget
                callbackRefcon:callbackRefcon];
}

- (IOReturn)setEventCallout:(IOHIDCallbackFunction)callback
             callbackTarget:(void *)callbackTarget
             callbackRefcon:(void *)callbackRefcon
{
    _eventCallbackTarget = callbackTarget;
    _eventCallbackRefcon = callbackRefcon;
    _eventCallback = callback;
    
    return [self setValueAvailableCallback:_eventCallout context:(__bridge void *)self];
}

static IOReturn _getEventCallout(void *iunknown  __unused,
                                 IOHIDCallbackFunction *outCallback  __unused,
                                 void **outCallbackTarget  __unused,
                                 void **outCallbackRefcon  __unused)
{
    return kIOReturnUnsupported;
}

- (instancetype)initWithDevice:(IOHIDDeviceClass *)device
{
    self = [super initWithDevice:device];
    
    if (!self) {
        return nil;
    }
    
    _interface = (IOHIDQueueInterface *)malloc(sizeof(*_interface));
    
    *_interface = (IOHIDQueueInterface) {
        // IUNKNOWN_C_GUTS
        ._reserved = (__bridge void *)self,
        .QueryInterface = self->_vtbl->QueryInterface,
        .AddRef = self->_vtbl->AddRef,
        .Release = self->_vtbl->Release,
        
        // IOHIDDeviceQueueInterface
        .createAsyncEventSource = _createAsyncEventSource,
        .getAsyncEventSource = _getAsyncEventSource,
        .createAsyncPort = _createAsyncPort,
        .getAsyncPort = _getAsyncPort,
        .create = _create,
        .dispose = _dispose,
        .addElement = _addElement,
        .removeElement = _removeElement,
        .hasElement = _hasElement,
        .start = _start,
        .stop = _stop,
        .getNextEvent = _getNextEvent,
        .setEventCallout = _setEventCallout,
        .getEventCallout = _getEventCallout
    };
    
    return self;
}

- (void)dealloc
{
    free(_interface);
}

@end
