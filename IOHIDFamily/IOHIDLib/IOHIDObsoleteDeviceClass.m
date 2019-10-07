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
#import "IOHIDObsoleteDeviceClass.h"
#import "IOHIDQueueClass.h"
#import "HIDLibElement.h"
#import <IOKit/hid/IOHIDLibPrivate.h>
#import "IOHIDDebug.h"
#import <AssertMacros.h>
#import "IOHIDTransactionClass.h"

@implementation IOHIDObsoleteDeviceClass

- (HRESULT)queryInterface:(REFIID)uuidBytes
             outInterface:(LPVOID *)outInterface
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, uuidBytes);
    HRESULT result = E_NOINTERFACE;
    
    if (CFEqual(uuid, IUnknownUUID) || CFEqual(uuid, kIOCFPlugInInterfaceID)) {
        *outInterface = &self->_plugin;
        CFRetain((__bridge CFTypeRef)self);
        result = S_OK;
    } else if (CFEqual(uuid, kIOHIDDeviceInterfaceID) ||
               CFEqual(uuid, kIOHIDDeviceInterfaceID122)) {
        *outInterface = (LPVOID *)&_interface;
        CFRetain((__bridge CFTypeRef)self);
        result = S_OK;
    } else if (CFEqual(uuid, kIOHIDQueueInterfaceID)) {
        IOHIDObsoleteQueueClass *queue = [[IOHIDObsoleteQueueClass alloc]
                                          initWithDevice:self];
        result = [queue queryInterface:uuidBytes outInterface:outInterface];
    } else if (CFEqual(uuid, kIOHIDOutputTransactionInterfaceID)) {
        IOHIDOutputTransactionClass *transaction;
        transaction = [[IOHIDOutputTransactionClass alloc] initWithDevice:self];
        result = [transaction queryInterface:uuidBytes
                                outInterface:outInterface];
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
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
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
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return me->_runLoopSource;
}

static IOReturn _createAsyncPort(void *iunknown, mach_port_t *port)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    *port = me->_port;
    return kIOReturnSuccess;
}

static mach_port_t _getAsyncPort(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return me->_port;
}

static IOReturn _open(void *iunknown, IOOptionBits flags)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me open:flags];
}

static IOReturn _close(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me close:0];
}

static void interestCallback(void *refcon,
                             io_service_t service __unused,
                             uint32_t messageType,
                             void *messageArgument __unused)
{
    IOHIDObsoleteDeviceClass *me = (__bridge id)refcon;
    
    if (!me || messageType != kIOMessageServiceIsTerminated) {
        return;
    }
    
    if (me->_removalCallback) {
        (*me->_removalCallback)(me->_removalTarget,
                                kIOReturnSuccess,
                                me->_removalRefcon,
                                (void *)&me->_interface);
    }
}

static IOReturn _setRemovalCallback(void *iunknown,
                                    IOHIDCallbackFunction removalCallback,
                                    void *removalTarget,
                                    void *removalRefcon)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setRemovalCallback:removalCallback
                    removalTarget:removalTarget
                    removalRefcon:removalRefcon];
}

- (IOReturn)setRemovalCallback:(IOHIDCallbackFunction)removalCallback
                 removalTarget:(void *)removalTarget
                 removalRefcon:(void *)removalRefcon
{
    IOReturn ret = kIOReturnSuccess;
    
    _removalTarget = removalTarget;
    _removalRefcon = removalRefcon;
    _removalCallback = removalCallback;
    
    if (!_notification) {
        ret = IOServiceAddInterestNotification(_notifyPort,
                                               _service,
                                               kIOGeneralInterest,
                                               interestCallback,
                                               (__bridge void *)self,
                                               &_notification);
    }
    
    return ret;
}

static IOReturn _getElementValue(void *iunknown,
                                 IOHIDElementCookie elementCookie,
                                 IOHIDEventStruct *valueEvent)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getElementValue:elementCookie value:valueEvent options:0];
}

- (IOReturn)getElementValue:(IOHIDElementCookie)elementCookie
                      value:(IOHIDEventStruct *)eventStruct
                    options:(IOOptionBits)options
{
    if (!eventStruct) {
        return kIOReturnBadArgument;
    }
    
    IOReturn ret = kIOReturnError;
    IOHIDElementRef elementRef = [super getElement:(uint32_t)elementCookie];
    HIDLibElement *element;
    IOHIDValueRef value;
    uint32_t length;
    
    ret = [super getValue:elementRef
                    value:&value
                  timeout:0
                 callback:nil
                  context:nil
                  options:options];
    require_noerr(ret, exit);
    
    elementRef = IOHIDValueGetElement(value);
    element = [[HIDLibElement alloc] initWithElementRef:elementRef];
    element.valueRef = value;
    
    length = (uint32_t)element.length;
    eventStruct->type = element.type;
    eventStruct->elementCookie = (IOHIDElementCookie)element.elementCookie;
    *(UInt64 *)&eventStruct->timestamp = element.timestamp;
    
    if (length > sizeof(uint32_t)) {
        eventStruct->longValueSize = length;
        eventStruct->longValue = malloc(length);
        bcopy(IOHIDValueGetBytePtr(value), eventStruct->longValue, length);
    } else {
        eventStruct->longValueSize = 0;
        eventStruct->longValue = NULL;
        eventStruct->value = (int32_t)element.integerValue;
    }
    
exit:
    return ret;
}

static IOReturn _setElementValue(void *iunknown,
                                 IOHIDElementCookie elementCookie,
                                 IOHIDEventStruct *valueEvent,
                                 uint32_t timeoutMS __unused,
                                 IOHIDElementCallbackFunction callback __unused,
                                 void *callbackTarget __unused,
                                 void *callbackRefcon __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setElementValue:elementCookie value:valueEvent];
}

- (IOReturn)setElementValue:(IOHIDElementCookie)elementCookie
                      value:(IOHIDEventStruct *)eventStruct
{
    if (!eventStruct) {
        return kIOReturnBadArgument;
    }
    
    IOReturn ret = kIOReturnError;
    IOHIDElementRef elementRef = [super getElement:(uint32_t)elementCookie];
    IOHIDValueRef value = _IOHIDValueCreateWithStruct(kCFAllocatorDefault,
                                                      elementRef,
                                                      eventStruct);
    
    require(elementRef && value, exit);
    
    ret = [super setValue:elementRef
                    value:value
                  timeout:0
                 callback:nil
                  context:nil
                  options:0];
    
exit:
    if (value) {
        CFRelease(value);
    }
    
    return ret;
}

static IOReturn _queryElementValue(void *iunknown,
                                   IOHIDElementCookie elementCookie,
                                   IOHIDEventStruct *valueEvent,
                                   uint32_t timeoutMS __unused,
                                   IOHIDElementCallbackFunction callback __unused,
                                   void *callbackTarget __unused,
                                   void *callbackRefcon __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getElementValue:elementCookie
                         value:valueEvent
                       options:kHIDGetElementValueForcePoll];
}

static IOReturn _startAllQueues(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me->_queue start];
}

static IOReturn _stopAllQueues(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me->_queue stop];
}

static IOHIDQueueInterface **_allocQueue(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me allocQueue];
}

- (IOHIDQueueInterface **)allocQueue
{
    IOHIDQueueInterface **queue = NULL;
    
    [self queryInterface:CFUUIDGetUUIDBytes(kIOHIDQueueInterfaceID)
            outInterface:(LPVOID *)&queue];
    
    return queue;
}

static IOHIDOutputTransactionInterface **_allocOutputTransaction(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me allocOutputTransaction];
}

- (IOHIDOutputTransactionInterface **)allocOutputTransaction
{
    IOHIDOutputTransactionInterface **transaction = NULL;
    
    [self queryInterface:CFUUIDGetUUIDBytes(kIOHIDOutputTransactionInterfaceID)
            outInterface:(LPVOID *)&transaction];
    
    return transaction;
}

static IOReturn _setReport(void *iunknown,
                           IOHIDReportType reportType,
                           uint32_t reportID,
                           void *reportBuffer,
                           uint32_t reportBufferSize,
                           uint32_t timeoutMS,
                           IOHIDReportCallbackFunction callback __unused,
                           void *callbackTarget __unused,
                           void *callbackRefcon __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setReport:reportType
                reportID:reportID
                  report:reportBuffer
            reportLength:reportBufferSize
                 timeout:timeoutMS
                callback:nil
                 context:nil
                 options:0];
}

static IOReturn _getReport(void *iunknown,
                           IOHIDReportType reportType,
                           uint32_t reportID,
                           void *reportBuffer,
                           uint32_t *reportBufferSize,
                           uint32_t timeoutMS,
                           IOHIDReportCallbackFunction callback __unused,
                           void *callbackTarget __unused,
                           void *callbackRefcon __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    IOReturn ret = kIOReturnError;
    CFIndex reportLength = reportBufferSize ? *reportBufferSize : 0;
    
    ret = [me getReport:reportType
               reportID:reportID
                 report:reportBuffer
           reportLength:&reportLength
                timeout:timeoutMS
               callback:nil
                context:nil
                options:0];
    
    if (reportBufferSize) {
        *reportBufferSize = (uint32_t)reportLength;
    }
    
    return ret;
}

static IOReturn _copyMatchingElements(void *iunknown,
                                      CFDictionaryRef matchingDict,
                                      CFArrayRef *elements)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me copyMatchingElements:matchingDict element:elements];
}

- (IOReturn)copyMatchingElements:(CFDictionaryRef)matchingDict
                         element:(CFArrayRef *)elements
{
    return [super copyMatchingElements:(__bridge NSDictionary *)matchingDict
                             elements:elements
                              options:kHIDCopyMatchingElementsDictionary];
}

static void inputReportCallback(void *context,
                                IOReturn result,
                                void *sender,
                                IOHIDReportType type __unused,
                                uint32_t reportID __unused,
                                uint8_t *report __unused,
                                CFIndex reportLength)
{
    IOHIDObsoleteDeviceClass *me = (__bridge id)context;
    
    if (me->_reportCallback) {
        (*me->_reportCallback)(me->_reportCallbackTarget,
                               result,
                               me->_reportCallbackRefcon,
                               sender,
                               (uint32_t)reportLength);
    }
}

static IOReturn _setInterruptReportHandlerCallback(void *iunknown,
                                                   void *reportBuffer,
                                                   uint32_t reportBufferSize,
                                                   IOHIDReportCallbackFunction callback,
                                                   void *callbackTarget,
                                                   void *callbackRefcon)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDObsoleteDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setInterruptReportHandlerCallback:reportBuffer
                                reportBufferSize:reportBufferSize
                                        callback:callback
                                  callbackTarget:callbackTarget
                                  callbackRefcon:callbackRefcon];
}

- (IOReturn)setInterruptReportHandlerCallback:(void *)reportBuffer
                             reportBufferSize:(uint32_t)reportBufferSize
                                     callback:(IOHIDReportCallbackFunction)callback
                               callbackTarget:(void *)callbackTarget
                               callbackRefcon:(void *)callbackRefcon
{
    _reportCallbackTarget = callbackTarget;
    _reportCallbackRefcon = callbackRefcon;
    _reportCallback = callback;
    
    return [self setInputReportCallback:(uint8_t *)reportBuffer
                           reportLength:reportBufferSize
                               callback:inputReportCallback
                                context:(__bridge void *)self
                                options:0];
}

- (IOReturn)start:(NSDictionary *)properties
          service:(io_service_t)service
{
    [super start:properties service:service];
    [self initQueue];
    
    return kIOReturnSuccess;
}

- (instancetype)init
{
    self = [super init];
    
    if (!self) {
        return nil;
    }
    
    _interface = (IOHIDDeviceInterface122 *)malloc(sizeof(*_interface));
    
    *_interface = (IOHIDDeviceInterface122) {
        // IUNKNOWN_C_GUTS
        ._reserved = (__bridge void *)self,
        .QueryInterface = self->_vtbl->QueryInterface,
        .AddRef = self->_vtbl->AddRef,
        .Release = self->_vtbl->Release,
        
        // IOHIDDeviceInterface122
        .createAsyncEventSource = _createAsyncEventSource,
        .getAsyncEventSource = _getAsyncEventSource,
        .createAsyncPort = _createAsyncPort,
        .getAsyncPort = _getAsyncPort,
        .open = _open,
        .close = _close,
        .setRemovalCallback = _setRemovalCallback,
        .getElementValue = _getElementValue,
        .setElementValue = _setElementValue,
        .queryElementValue = _queryElementValue,
        .startAllQueues = _startAllQueues,
        .stopAllQueues = _stopAllQueues,
        .allocQueue = _allocQueue,
        .allocOutputTransaction = _allocOutputTransaction,
        .setReport = _setReport,
        .getReport = _getReport,
        .copyMatchingElements = _copyMatchingElements,
        .setInterruptReportHandlerCallback = _setInterruptReportHandlerCallback
    };
    
    _notifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    IONotificationPortSetDispatchQueue(_notifyPort, dispatch_get_main_queue());
    
    return self;
}

- (void)dealloc
{
    free(_interface);
    
    if (_notifyPort) {
        IONotificationPortDestroy(_notifyPort);
    }
    
    if (_notification) {
        IOObjectRelease(_notification);
    }
}

@end
