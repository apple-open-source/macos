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

#import <IOKit/hid/IOHIDLib.h>
#import "IOHIDTransactionClass.h"
#import "HIDLibElement.h"
#import <AssertMacros.h>
#import "IOHIDLibUserClient.h"
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <os/assumes.h>

@implementation IOHIDTransactionClass
/**
 *  IOHIDDeviceTransactionInterface         *_interface;
 *  Lifetime: Object, created in initWithDevice, released in dealloc
 *  IOHIDOutputTransactionInterface         *_outputInterface;  
 *  Lifetime: Object, created in initWithDevice (IOHIDOutputTransactionClass only), released in dealloc
 *  IOHIDDeviceClass                        *_device;
 *  Lifetime: Object, set during initWithDevice, doesn't change
 *  NSMutableArray                          *_elements;
 *  Lifetime: Object, created in initWithDevice, released in dealloc.
 *  IOHIDTransactionDirectionType           _direction;
 *  Lifetime: Object, set during initWithDevice, can change via setDirection 
 *  
 *  os_unfair_recursive_lock                _callbackLock;
 *  Lifetime: Object, created in initWithDevice, released in dealloc. This lock protects transaction state and callback members.
 *  NSMutableSet                            *_pendingCallbacks;
 *  Lifetime: First commit with callback call, released in dealloc
 */

- (HRESULT)queryInterface:(REFIID)uuidBytes
             outInterface:(LPVOID *)outInterface
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, uuidBytes);
    HRESULT result = E_NOINTERFACE;
    
    if (CFEqual(uuid, kIOHIDDeviceTransactionInterfaceID)) {
        *outInterface = (LPVOID *)&_interface;
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
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getAsyncEventSource:pSource];
}

- (IOReturn)getAsyncEventSource:(CFTypeRef *)pSource
{
    if (!pSource) {
        return kIOReturnBadArgument;
    }
    
    *pSource = _device.runLoopSource;
    
    return kIOReturnSuccess;
}

static IOReturn _setDirection(void *iunknown,
                              IOHIDTransactionDirectionType direction,
                              IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setDirection:direction];
}

- (IOReturn)setDirection:(IOHIDTransactionDirectionType)direction
{
    _direction = direction;
    return kIOReturnSuccess;
}

static IOReturn _getDirection(void *iunknown,
                              IOHIDTransactionDirectionType *pDirection)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getDirection:pDirection];
}

- (IOReturn)getDirection:(IOHIDTransactionDirectionType *)pDirection
{
    if (!pDirection) {
        return kIOReturnBadArgument;
    }
    
    *pDirection = _direction;
    return kIOReturnSuccess;
}

static IOReturn _addElement(void *iunknown,
                            IOHIDElementRef element,
                            IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me addElement:element];
}

- (IOReturn)addElement:(IOHIDElementRef)elementRef
{
    IOReturn ret = kIOReturnError;
    HIDLibElement *element = nil;
    
    __Require_Action(elementRef, exit, ret = kIOReturnBadArgument);
    
    element = [[HIDLibElement alloc] initWithElementRef:elementRef];
    __Require(element, exit);
    
    __Require(![_elements containsObject:element], exit);
    
    if (_direction == kIOHIDTransactionDirectionTypeOutput) {
        __Require(element.type == kIOHIDElementTypeOutput ||
                element.type == kIOHIDElementTypeFeature, exit);
    }
    
    [_elements addObject:element];
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

static IOReturn _removeElement(void *iunknown,
                               IOHIDElementRef element,
                               IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me removeElement:element];
}

- (IOReturn)removeElement:(IOHIDElementRef)elementRef
{
    IOReturn ret = kIOReturnError;
    HIDLibElement *element = nil;
    
    __Require_Action(elementRef, exit, ret = kIOReturnBadArgument);
    
    element = [[HIDLibElement alloc] initWithElementRef:elementRef];
    __Require(element && [_elements containsObject:element], exit);
    
    [_elements removeObject:element];
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

static IOReturn _containsElement(void *iunknown,
                                 IOHIDElementRef element,
                                 Boolean *pValue,
                                 IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me containsElement:element value:pValue];
}

- (IOReturn)containsElement:(IOHIDElementRef)elementRef
                      value:(Boolean *)pValue
{
    IOReturn ret = kIOReturnError;
    HIDLibElement *element = nil;
    
    __Require_Action(elementRef && pValue, exit, ret = kIOReturnBadArgument);
    
    element = [[HIDLibElement alloc] initWithElementRef:elementRef];
    __Require(element, exit);
    
    *pValue = [_elements containsObject:element];
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

static IOReturn _setValue(void *iunknown,
                          IOHIDElementRef element,
                          IOHIDValueRef value,
                          IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;

    return [me setValue:element value:value options:options];
}

- (IOReturn)setValue:(IOHIDElementRef)elementRef
               value:(IOHIDValueRef)valueRef
             options:(IOOptionBits)options
{
    IOReturn ret = kIOReturnError;
    HIDLibElement *element = nil;
    
    __Require_Action(elementRef && valueRef, exit, ret = kIOReturnBadArgument);
    __Require(_direction == kIOHIDTransactionDirectionTypeOutput, exit);
    
    element = [[HIDLibElement alloc] initWithElementRef:elementRef];
    __Require(element && [_elements containsObject:element], exit);
    
    if (options & kIOHIDTransactionOptionDefaultOutputValue) {
        [element setDefaultValueRef:valueRef];
    } else {
        [element setValueRef:valueRef];
    }
    
    [_elements replaceObjectAtIndex:[_elements indexOfObject:element]
                           withObject:element];
    
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

static IOReturn _getValue(void *iunknown,
                          IOHIDElementRef elementRef,
                          IOHIDValueRef *pValueRef,
                          IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getValue:elementRef value:pValueRef options:options];
}

- (IOReturn)getValue:(IOHIDElementRef)elementRef
               value:(IOHIDValueRef *)pValueRef
             options:(IOOptionBits)options
{
    IOReturn ret = kIOReturnError;
    HIDLibElement *element = nil;
    HIDLibElement *tmp = nil;
    
    __Require_Action(elementRef && pValueRef, exit, ret = kIOReturnBadArgument);
    
    tmp = [[HIDLibElement alloc] initWithElementRef:elementRef];
    __Require(tmp && [_elements containsObject:tmp], exit);
    
    element = [_elements objectAtIndex:[_elements indexOfObject:tmp]];
    
    if (options & kIOHIDTransactionOptionDefaultOutputValue) {
        *pValueRef = element.defaultValueRef;
    } else {
        *pValueRef = element.valueRef;
    }

    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

/*
 * Lifetime: malloc'd in commit when callback != NULL, lives until callback completion or dealloc.
 * Memory freed on normal completion in _asyncCallback() or on dealloc for aborted/pending callbacks.
 */
typedef struct {
    IOHIDCallback    callback;
    void           * context;
    void           * device;
    void           * transaction;
    void           * sender;
    NSArray        * elements;
} AsyncCommitContext;

static void _asyncCallback(void * context, IOReturn result, uint32_t bufferSize, uint64_t addr)
{
    HIDLibElement      * element;
    IOHIDValueRef        value;
    IOHIDElementValue  * elementVal;
    size_t               dataOffset   = 0;
    AsyncCommitContext * asyncContext = (AsyncCommitContext *)context;

    if (!asyncContext || !asyncContext->callback) {
        return;
    }

    if (asyncContext->elements && addr && bufferSize) {
        for (element in asyncContext->elements) {
            elementVal = (IOHIDElementValue *)((uint8_t *)addr + dataOffset);

            dataOffset += elementVal->totalSize;
            if (elementVal->totalSize < ELEMENT_VALUE_HEADER_SIZE(elementVal) || dataOffset > bufferSize) {
                HIDLogError("Unable to copy back value for element, unexpected size(%d)", elementVal->totalSize);
                break;
            } else if (elementVal->cookie != element.elementCookie) {
                HIDLogError("Unable to copy back value for element, unexpected cookie(%ld) expected:%d", (long)elementVal->cookie, element.elementCookie);
                break;
            }

            value = _IOHIDValueCreateWithElementValuePtr(kCFAllocatorDefault, element.elementRef, elementVal);
            [element setValueRef:value];
            if (value) {
                CFRelease(value);
            }
        }
        [(__bridge IOHIDDeviceClass *)asyncContext->device releaseReport:addr];
    }

    if (asyncContext->sender) {
        IOHIDTransactionClass *transactionClass = (__bridge IOHIDTransactionClass *)asyncContext->sender;
        os_unfair_recursive_lock_lock(&transactionClass->_callbackLock);
        [transactionClass->_pendingCallbacks removeObject:[NSValue valueWithPointer:asyncContext]];
        os_unfair_recursive_lock_unlock(&transactionClass->_callbackLock);
    }

    ((IOHIDCallback)asyncContext->callback)(asyncContext->context, result, asyncContext->transaction);

    asyncContext->elements = NULL;

    free(asyncContext);
}

static IOReturn _commit(void *iunknown,
                        uint32_t timeout,
                        IOHIDCallback callback,
                        void *context,
                        IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me commit:context timeout:timeout callback:callback options:options];
}

- (IOReturn)commit:(void *)context
           timeout:(uint32_t)timeout
          callback:(IOHIDCallback)callback
           options:(IOOptionBits)options
{
    uint64_t                  regID;
    IOReturn                  ret          = kIOReturnError;
    uint64_t                  input[3]     = {0};
    size_t                    dataSize     = 0;
    size_t                    dataOffset   = 0;
    void                    * cookies      = NULL;
    void                    * elementData  = NULL;
    uint32_t                  count        = (uint32_t)_elements.count;
    size_t                    cookiesSize  = count * sizeof(uint32_t);
    AsyncCommitContext      * asyncContext = NULL;
    HIDLibElement           * element;
    size_t                    elementSize;
    IOHIDValueRef             value;
    IOHIDElementValue       * elementVal;
    IOHIDElementValueHeader * elementValHeader;
    io_async_ref64_t          asyncRef;

    __Require(count, exit);

    IORegistryEntryGetRegistryEntryID(_device.service, &regID);

    input[2] = options;

    if (callback) {
        input[0] = timeout;
        __Require((asyncContext = (AsyncCommitContext *)calloc(1, sizeof(AsyncCommitContext))), exit);

        asyncContext->callback    = callback;
        asyncContext->context     = context;
        asyncContext->device      = (__bridge void *)_device;
        asyncContext->transaction = &_interface;
        asyncContext->sender      = (__bridge void *) self;
        asyncContext->elements    = NULL;

        asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)_asyncCallback;
        asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)asyncContext;

        os_unfair_recursive_lock_lock(&_callbackLock);
        if (!_pendingCallbacks) {
            _pendingCallbacks = [[NSMutableSet alloc] init];
        }
        [_pendingCallbacks addObject:[NSValue valueWithPointer:asyncContext]];
        os_unfair_recursive_lock_unlock(&_callbackLock);
    }

    if (_direction == kIOHIDTransactionDirectionTypeOutput) {
        for (uint32_t i = 0; i < count; i++) {
            element = [_elements objectAtIndex:i];

            ret = [_device setValue:element.elementRef value:element.valueRef timeout:0 callback:nil context:nil options:kHIDSetElementValuePendEvent];
            __Require_noErr_Action(ret, exit, HIDLogError("setValue(%#llx):%#x", regID, ret));
            
            elementSize = sizeof(IOHIDElementValueHeader) + IOHIDValueGetLength(element.valueRef);
            dataSize += elementSize;
        }

        __Require_Action((elementData = malloc(dataSize)), exit, ret = kIOReturnNoMemory);

        for (uint32_t i = 0; i < count; i++) {
            element = [_elements objectAtIndex:i];
            elementValHeader = (IOHIDElementValueHeader *)(elementData + dataOffset);
            _IOHIDValueCopyToElementValueHeader(element.valueRef, elementValHeader);
            dataOffset += elementValHeader->length + sizeof(IOHIDElementValueHeader);
        }

        if (callback) {
            ret = IOConnectCallAsyncMethod(_device.connect, kIOHIDLibUserClientPostElementValues, [_device getPort], asyncRef, kIOAsyncCalloutCount, input, 1, elementData, dataSize, NULL, NULL, NULL, NULL);
        } else {
            ret = IOConnectCallMethod(_device.connect, kIOHIDLibUserClientPostElementValues, input, 1, elementData, dataSize, NULL, NULL, NULL, NULL);
        }
        __Require_noErr_Action(ret, exit, HIDLogError("kIOHIDLibUserClientPostElementValues(%#llx):%#x", regID, ret));

    } else {
        for (uint32_t i = 0; i < count; i++) {
            element = [_elements objectAtIndex:i];
            elementSize = sizeof(IOHIDElementValue) + _IOHIDElementGetLength(element.elementRef);
            value = element.valueRef;
            dataSize += elementSize;

            ret = [_device getValue:element.elementRef value:&value timeout:0 callback:nil context:nil options:kHIDGetElementValuePendEvent];
            __Require_noErr_Action(ret, exit, HIDLogError("getValue(%#llx):%#x", regID, ret));
        }

        __Require_Action((cookies = malloc(cookiesSize)), exit, ret = kIOReturnNoMemory);

        for (uint32_t i = 0; i < count; i++) {
            element = [_elements objectAtIndex:i];
            *((uint32_t *)cookies + i) = (uint32_t)element.elementCookie;
        }

        if (callback) {
            input[1] = dataSize;
            __Require_Action((asyncContext->elements = [NSArray arrayWithArray:_elements]), exit, ret = kIOReturnNoMemory);
            ret = IOConnectCallAsyncMethod(_device.connect, kIOHIDLibUserClientUpdateElementValues, [_device getPort], asyncRef, kIOAsyncCalloutCount, input, 3, cookies, cookiesSize, NULL, NULL, NULL, NULL);
            __Require_noErr_Action(ret, exit, HIDLogError("kIOHIDLibUserClientUpdateElementValues(%#llx):%#x", regID, ret));
        } else {
            __Require_Action((elementData = calloc(1, dataSize)), exit, ret = kIOReturnNoMemory);
            ret = IOConnectCallMethod(_device.connect, kIOHIDLibUserClientUpdateElementValues, input, 3, cookies, cookiesSize, NULL, NULL, elementData, &dataSize);
            __Require_noErr_Action(ret, exit, HIDLogError("kIOHIDLibUserClientUpdateElementValues(%#llx):%#x", regID, ret));

            for (element in _elements) {
                elementVal = (IOHIDElementValue *)((uint8_t *)elementData + dataOffset);
                dataOffset += elementVal->totalSize;

                if (elementVal->totalSize < ELEMENT_VALUE_HEADER_SIZE(elementVal) || dataOffset > dataSize) {
                    HIDLogError("Unable to copy back value for element, unexpected size(%d)", elementVal->totalSize);
                    break;
                } else if (elementVal->cookie != element.elementCookie) {
                    HIDLogError("Unable to copy back value for element, unexpected cookie(%ld) expected:%d", (long)elementVal->cookie, element.elementCookie);
                    break;
                }

                value = _IOHIDValueCreateWithElementValuePtr(kCFAllocatorDefault, element.elementRef, elementVal);
                [element setValueRef:value];
                if (value) {
                    CFRelease(value);
                }
            }
        }
    }

exit:
    if (cookies) {
        free(cookies);
    }
    if (elementData) {
        free(elementData);
    }
    if (asyncContext && ret) {
        asyncContext->elements = NULL;
        os_unfair_recursive_lock_lock(&_callbackLock);
        [_pendingCallbacks removeObject:[NSValue valueWithPointer:asyncContext]];
        os_unfair_recursive_lock_unlock(&_callbackLock);
        free(asyncContext);
    }

    return ret;
}

static IOReturn _clear(void *iunknown, IOOptionBits options __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me clear];
}

- (IOReturn)clear
{
    [_elements removeAllObjects];
    return kIOReturnSuccess;
}

- (instancetype)initWithDevice:(IOHIDDeviceClass *)device
{
    self = [super init];
    
    if (!self) {
        return nil;
    }
    
    _device = device;
    
    _interface = (IOHIDDeviceTransactionInterface *)malloc(sizeof(*_interface));
    
    *_interface = (IOHIDDeviceTransactionInterface) {
        // IUNKNOWN_C_GUTS
        ._reserved = (__bridge void *)self,
        .QueryInterface = self->_vtbl->QueryInterface,
        .AddRef = self->_vtbl->AddRef,
        .Release = self->_vtbl->Release,
        
        // IOHIDDeviceTransactionInterface
        .getAsyncEventSource = _getAsyncEventSource,
        .setDirection = _setDirection,
        .getDirection = _getDirection,
        .addElement = _addElement,
        .removeElement = _removeElement,
        .containsElement = _containsElement,
        .setValue = _setValue,
        .getValue = _getValue,
        .commit = _commit,
        .clear = _clear,
    };
    
    _elements = [[NSMutableArray alloc] init];
    _callbackLock = OS_UNFAIR_RECURSIVE_LOCK_INIT;

    return self;
}

- (void)setDevice:(IOHIDDeviceClass *)device
{
    _device = device;
}

- (IOHIDDeviceClass *)device
{
    return _device;
}

- (void)dealloc
{
    for (NSValue *contextValue in [_pendingCallbacks copy]) {
        AsyncCommitContext *context = [contextValue pointerValue];
        context->elements = NULL;
        free(context);
    }
    [_pendingCallbacks removeAllObjects];

    free(_interface);
}

@end

@implementation IOHIDOutputTransactionClass

- (HRESULT)queryInterface:(REFIID)uuidBytes
             outInterface:(LPVOID *)outInterface
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, uuidBytes);
    HRESULT result = E_NOINTERFACE;
    
    if (CFEqual(uuid, kIOHIDOutputTransactionInterfaceID)) {
        *outInterface = (LPVOID *)&_outputInterface;
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
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    if (!source) {
        return kIOReturnBadArgument;
    }
    
    *source = me->_device.runLoopSource;
    CFRetain(*source);
    
    return kIOReturnSuccess;
}

static CFRunLoopSourceRef _getOutputAsyncEventSource(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return me->_device.runLoopSource;
}

static IOReturn _createAsyncPort(void *iunknown, mach_port_t *port)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    *port = me->_device.port;
    return kIOReturnSuccess;
}

static mach_port_t _getAsyncPort(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return me->_device.port;
}

static IOReturn _create(void *iunknown __unused)
{
    return kIOReturnSuccess;
}

static IOReturn _dispose(void *iunknown __unused)
{
    return kIOReturnSuccess;
}

static IOReturn _addOutputElement(void *iunknown,
                                  IOHIDElementCookie elementCookie)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me addElement:[me->_device getElement:(uint32_t)elementCookie]];
}

static IOReturn _removeOutputElement(void *iunknown,
                                     IOHIDElementCookie elementCookie)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me removeElement:[me->_device getElement:(uint32_t)elementCookie]];
}

static Boolean _hasElement(void *iunknown, IOHIDElementCookie elementCookie)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    Boolean contains = false;
    
    [me containsElement:[me->_device getElement:(uint32_t)elementCookie]
                  value:&contains];
    
    return contains;
}

static IOReturn _setElementDefault(void *iunknown,
                                   IOHIDElementCookie elementCookie,
                                   IOHIDEventStruct *valueEvent)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setElementValue:elementCookie
                         value:valueEvent
                       options:kIOHIDTransactionOptionDefaultOutputValue];
}

static IOReturn _getElementDefault(void *iunknown,
                                   IOHIDElementCookie elementCookie,
                                   IOHIDEventStruct *outValueEvent)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getElementValue:elementCookie
                         value:outValueEvent
                       options:kIOHIDTransactionOptionDefaultOutputValue];
}

static IOReturn _setElementValue(void *iunknown,
                                 IOHIDElementCookie elementCookie,
                                 IOHIDEventStruct *valueEvent)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setElementValue:elementCookie value:valueEvent options:0];
}

- (IOReturn)setElementValue:(IOHIDElementCookie)elementCookie
                      value:(IOHIDEventStruct *)eventStruct
                    options:(IOOptionBits)options
{
    if (!eventStruct) {
        return kIOReturnBadArgument;
    }
    
    IOReturn ret = kIOReturnError;
    IOHIDElementRef elementRef = [_device getElement:(uint32_t)elementCookie];
    IOHIDValueRef value = _IOHIDValueCreateWithStruct(kCFAllocatorDefault,
                                                      elementRef,
                                                      eventStruct);
    
    __Require(elementRef && value, exit);
    
    ret = [self setValue:elementRef value:value options:options];
    
exit:
    if (value) {
        CFRelease(value);
    }
    
    return ret;
}

static IOReturn _getElementValue(void *iunknown,
                                 IOHIDElementCookie elementCookie,
                                 IOHIDEventStruct *outValueEvent)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getElementValue:elementCookie value:outValueEvent options:0];
}

- (IOReturn)getElementValue:(IOHIDElementCookie)elementCookie
                      value:(IOHIDEventStruct *)eventStruct
                    options:(IOOptionBits)options
{
    if (!eventStruct) {
        return kIOReturnBadArgument;
    }
    
    IOReturn ret = kIOReturnError;
    IOHIDElementRef elementRef = [_device getElement:(uint32_t)elementCookie];
    HIDLibElement *element;
    IOHIDValueRef value;
    uint32_t length;
    
    ret = [self getValue:elementRef value:&value options:options];
    __Require_noErr(ret, exit);
    
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

static IOReturn _commitOutput(void *iunknown,
                              uint32_t timeoutMS __unused,
                              IOHIDCallbackFunction callback __unused,
                              void *callbackTarget __unused,
                              void *callbackRefcon __unused)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me commit:nil timeout:0 callback:nil options:0];
}

static IOReturn _clearOutput(void *iunknown)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDOutputTransactionClass *me = (__bridge id)vtbl->_reserved;
    
    return [me clear];
}

- (instancetype)initWithDevice:(IOHIDDeviceClass *)device
{
    self = [super initWithDevice:device];
    
    if (!self) {
        return nil;
    }
    
    _direction = kIOHIDTransactionDirectionTypeOutput;
    
    _outputInterface = (IOHIDOutputTransactionInterface *)malloc(sizeof(*_outputInterface));
    
    *_outputInterface = (IOHIDOutputTransactionInterface) {
        // IUNKNOWN_C_GUTS
        ._reserved = (__bridge void *)self,
        .QueryInterface = self->_vtbl->QueryInterface,
        .AddRef = self->_vtbl->AddRef,
        .Release = self->_vtbl->Release,
        
        // IOHIDOutputTransactionInterface
        .createAsyncEventSource = _createAsyncEventSource,
        .getAsyncEventSource = _getOutputAsyncEventSource,
        .createAsyncPort = _createAsyncPort,
        .getAsyncPort = _getAsyncPort,
        .create = _create,
        .dispose = _dispose,
        .addElement = _addOutputElement,
        .removeElement = _removeOutputElement,
        .hasElement = _hasElement,
        .setElementDefault = _setElementDefault,
        .getElementDefault = _getElementDefault,
        .setElementValue = _setElementValue,
        .getElementValue = _getElementValue,
        .commit = _commitOutput,
        .clear = _clearOutput,
    };
    
    return self;
}

- (void)dealloc
{
    for (NSValue *contextValue in [_pendingCallbacks copy]) {
        AsyncCommitContext *context = [contextValue pointerValue];
        context->elements = NULL;
        free(context);
    }
    [_pendingCallbacks removeAllObjects];

    free(_outputInterface);
}

@end
