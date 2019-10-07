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
#import "IOHIDDeviceClass.h"
#import "IOHIDQueueClass.h"
#import "IOHIDTransactionClass.h"
#import <AssertMacros.h>
#import "IOHIDLibUserClient.h"
#import "HIDLibElement.h"
#import <IOKit/IODataQueueClient.h>
#import <mach/mach_port.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import "IOHIDDebug.h"
#import "IOHIDDescriptorParser.h"
#import "IOHIDDescriptorParserPrivate.h"
#import <IOKit/hidsystem/IOHIDLib.h>
#import "IOHIDFamilyProbe.h"

#ifndef min
#define min(a, b) ((a < b) ? a : b)
#endif

@implementation IOHIDDeviceClass

@synthesize port = _port;
@synthesize runLoopSource = _runLoopSource;
@synthesize connect = _connect;
@synthesize service = _service;

- (HRESULT)queryInterface:(REFIID)uuidBytes
             outInterface:(LPVOID *)outInterface
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, uuidBytes);
    HRESULT result = E_NOINTERFACE;
    
    if (CFEqual(uuid, IUnknownUUID) || CFEqual(uuid, kIOCFPlugInInterfaceID)) {
        *outInterface = &self->_plugin;
        CFRetain((__bridge CFTypeRef)self);
        result = S_OK;
    } else if (CFEqual(uuid, kIOHIDDeviceDeviceInterfaceID) ||
               CFEqual(uuid, kIOHIDDeviceDeviceInterfaceID2)) {
        *outInterface = (LPVOID *)&_device;
        CFRetain((__bridge CFTypeRef)self);
        result = S_OK;
    } else if (CFEqual(uuid, kIOHIDDeviceQueueInterfaceID)) {
        [self initPort];
        [self initElements];
        
        IOHIDQueueClass *queue = [[IOHIDQueueClass alloc] initWithDevice:self];
        result = [queue queryInterface:uuidBytes outInterface:outInterface];
    } else if (CFEqual(uuid, kIOHIDDeviceTransactionInterfaceID)) {
        [self initPort];
        [self initElements];
        
        IOHIDTransactionClass *transaction;
        transaction = [[IOHIDTransactionClass alloc] initWithDevice:self];
        result = [transaction queryInterface:uuidBytes
                                outInterface:outInterface];
    }
    
    if (uuid) {
        CFRelease(uuid);
    }
    
    return result;
}

- (IOReturn)probe:(NSDictionary * __unused)properties
          service:(io_service_t)service
         outScore:(SInt32 * __unused)outScore
{
    if (IOObjectConformsTo(service, "IOHIDDevice")) {
        return kIOReturnSuccess;
    }
    
    return kIOReturnUnsupported;
}

- (IOHIDElementRef)getElement:(uint32_t)cookie
{
    IOHIDElementRef elementRef = NULL;
    
    if (cookie < _sortedElements.count) {
        id obj = [_sortedElements objectAtIndex:cookie];
        
        if (obj && [obj isKindOfClass:[HIDLibElement class]]) {
            elementRef = ((HIDLibElement *)obj).elementRef;
        }
    }
    
    return elementRef;
}

- (IOReturn)initElements
{
    IOReturn ret = kIOReturnError;
    uint64_t output[2];
    uint32_t outputCount = 2;
    uint64_t input = kHIDElementType;
    uint32_t elementCount;
    uint32_t reportCount;
    size_t bufferSize;
    NSMutableData *data = nil;
    uint32_t maxCookie = 0;
    
    require_action_quiet(!_elements, exit, ret = kIOReturnSuccess);
    
    ret = [self initConnect];
    require_noerr(ret, exit);
    
    ret = IOConnectCallScalarMethod(_connect,
                                    kIOHIDLibUserClientGetElementCount,
                                    0,
                                    0,
                                    output,
                                    &outputCount);
    require_noerr_action(ret, exit, HIDLogError("IOConnectCallScalarMethod(kIOHIDLibUserClientGetElementCount):%x", ret));
    
    elementCount = (uint32_t)output[0];
    reportCount = (uint32_t)output[1];
    bufferSize = sizeof(IOHIDElementStruct) * elementCount;
    data = [[NSMutableData alloc] initWithLength:bufferSize];
    
    ret = IOConnectCallMethod(_connect,
                              kIOHIDLibUserClientGetElements,
                              &input,
                              1,
                              0,
                              0,
                              0,
                              0,
                              [data mutableBytes],
                              &bufferSize);
    require_noerr_action(ret, exit, HIDLogError("IOConnectCallMethod(kIOHIDLibUserClientGetElements):%x", ret));
    
    _elements = [[NSMutableArray alloc] init];
    
    for (uint32_t i = 0; i < bufferSize; i += sizeof(IOHIDElementStruct)) {
        IOHIDElementStruct *elementStruct = &[data mutableBytes][i];
        IOHIDElementRef parentRef = NULL;
        HIDLibElement *element;
        uint32_t cookieCount;
        
        if (elementStruct->cookieMax > maxCookie) {
            maxCookie = elementStruct->cookieMax;
        }
        
        cookieCount = elementStruct->cookieMax - elementStruct->cookieMin + 1;
        
        // Find the parent element, if any
        if (elementStruct->parentCookie) {
            for (HIDLibElement *ele in _elements) {
                if (elementStruct->parentCookie == ele.elementCookie) {
                    parentRef = ele.elementRef;
                }
            }
        }
        
        /*
         * The element structs that are provided to us from the IOConnect call
         * may contain a range of cookies. It's up to us to turn each of those
         * cookies into an element. If cookieMin == cookieMax, then there is
         * only one element.
         */
        if (elementStruct->cookieMin == elementStruct->cookieMax) {
            element = [[HIDLibElement alloc] initWithElementStruct:elementStruct
                                                            parent:parentRef
                                                             index:0];
            _IOHIDElementSetDeviceInterface(element.elementRef,
                                            (IOHIDDeviceDeviceInterface **)&_device);
            [_elements addObject:element];
            continue;
        } else {
            /*
             * Iterate through the cookies and generate elements for each one.
             * The index that we pass in will determine the element's usage,
             * among other things.
             */
            for (uint32_t j = 0; j < cookieCount; j++) {
                element = [[HIDLibElement alloc] initWithElementStruct:elementStruct
                                                                parent:parentRef
                                                                 index:j];
                _IOHIDElementSetDeviceInterface(element.elementRef,
                                                (IOHIDDeviceDeviceInterface **)&_device);
                [_elements addObject:element];
                
                // Set the correct location for the value
                if (elementStruct->duplicateValueSize) {
                    elementStruct->valueLocation -= elementStruct->duplicateValueSize;
                } else {
                    elementStruct->valueLocation -= elementStruct->valueSize;
                }
            }
        }
    }
    
    input = kHIDReportHandlerType;
    bufferSize = sizeof(IOHIDElementStruct) * reportCount;
    data = [[NSMutableData alloc] initWithLength:bufferSize];
    
    ret = IOConnectCallMethod(_connect,
                              kIOHIDLibUserClientGetElements,
                              &input,
                              1,
                              0,
                              0,
                              0,
                              0,
                              [data mutableBytes],
                              &bufferSize);
    
    if (ret == kIOReturnSuccess) {
        /*
         * These report handler elements are by our IOHIDQueue for receiving
         * input reports.
         */
        _reportElements = [[NSMutableArray alloc] init];
        
        for (uint32_t i = 0; i < bufferSize; i += sizeof(IOHIDElementStruct)) {
            IOHIDElementStruct *elementStruct = &[data mutableBytes][i];
            HIDLibElement *element;
            
            element = [[HIDLibElement alloc] initWithElementStruct:elementStruct
                                                            parent:NULL
                                                             index:0];
            [_reportElements addObject:element];
            
            if (element.elementCookie > maxCookie) {
                maxCookie = element.elementCookie;
            }
        }
    }
    
    // Keep an array of elements sorted by cookie, for faster access in
    // getElement method.
    _sortedElements = [[NSMutableArray alloc] initWithCapacity:maxCookie + 1];
    for (uint32_t i = 0; i < maxCookie + 1; i++) {
        _sortedElements[i] = @NO;
    }
    
    for (HIDLibElement *element in _elements) {
        [_sortedElements replaceObjectAtIndex:element.elementCookie withObject:element];
    }
    
    for (HIDLibElement *element in _reportElements) {
        [_sortedElements replaceObjectAtIndex:element.elementCookie withObject:element];
    }
    
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

static void _portCallback(CFMachPortRef port,
                          void *msg,
                          CFIndex size,
                          void *info)
{
    IOHIDDeviceClass *me = (__bridge id)info;
    
    [me->_queue queueCallback:port msg:msg size:size info:info];
}

- (void)initPort
{
    CFMachPortContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };
    
    require_quiet(!_port, exit);
    
    _port = IODataQueueAllocateNotificationPort();
    require(_port, exit);
    
    _machPort = CFMachPortCreateWithPort(kCFAllocatorDefault,
                                         _port,
                                         (CFMachPortCallBack)_portCallback,
                                         &context, NULL);
    require(_machPort, exit);
    
    _runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault,
                                                   _machPort,
                                                   0);
    require(_runLoopSource, exit);
    
exit:
    return;
}

- (void)initQueue
{
    require_quiet(!_queue, exit);
    
    [self initPort];
    
    require_noerr([self initElements], exit);
    
    _queue = [[IOHIDQueueClass alloc] initWithDevice:self
                                                port:_port
                                              source:_runLoopSource];
    require_action(_queue, exit, HIDLogError("Failed to create queue"));
    
    [_queue setValueAvailableCallback:_valueAvailableCallback
                              context:(__bridge void *)self];
    
    for (HIDLibElement *element in _reportElements) {
        [_queue addElement:element.elementRef];
    }
    
exit:
    return;
}

- (IOReturn)initConnect
{
    IOReturn ret = kIOReturnError;
    
    if (_connect) {
        return kIOReturnSuccess;
    }
    
#if TARGET_OS_OSX
    uint64_t regID;
    
    IORegistryEntryGetRegistryEntryID(_service, &regID);
    
    if (!_tccRequested) {
        NSNumber *tcc = CFBridgingRelease(IORegistryEntryCreateCFProperty(
                                    _service,
                                    CFSTR(kIOHIDRequiresTCCAuthorizationKey),
                                    kCFAllocatorDefault,
                                    0));
        
        if (tcc && [tcc isEqual:@YES]) {
            _tccGranted = IOHIDRequestAccess(kIOHIDRequestTypeListenEvent);
        } else {
            _tccGranted = true;
        }
        
        _tccRequested = true;
    }
    
    if (!_tccGranted) {
        HIDLogError("0x%llx: TCC deny IOHIDDeviceOpen", regID);
    }
    require_action(_tccGranted, exit, ret = kIOReturnNotPermitted);
#endif
    
    ret = IOServiceOpen(_service,
                        mach_task_self(),
                        kIOHIDLibUserClientConnectManager,
                        &_connect);
    require_action(ret == kIOReturnSuccess && _connect, exit,
                   HIDLogError("IOServiceOpen failed: 0x%x", ret));
    
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

- (void)unmapMemory
{
#if !__LP64__
    vm_address_t        mappedMem = (vm_address_t)_sharedMemory;
#else
    mach_vm_address_t   mappedMem = _sharedMemory;
#endif
    
    if (_sharedMemory) {
        IOConnectUnmapMemory(_connect,
                             kIOHIDLibUserClientElementValuesType,
                             mach_task_self(),
                             mappedMem);
        
        _sharedMemory = 0;
        _sharedMemorySize = 0;
    }
}

- (IOReturn)mapMemory
{
    IOReturn ret = kIOReturnError;
    
#if !__LP64__
    vm_address_t        mappedMem = (vm_address_t)0;
    vm_size_t           memSize = 0;
#else
    mach_vm_address_t   mappedMem = (mach_vm_address_t)0;
    mach_vm_size_t      memSize = 0;
#endif
    
    [self unmapMemory];
    
    ret = IOConnectMapMemory(_connect,
                             kIOHIDLibUserClientElementValuesType,
                             mach_task_self(),
                             &mappedMem,
                             &memSize,
                             kIOMapAnywhere);
    require_noerr_action(ret, exit, {
        HIDLogError("IOConnectCallMethod(kIOHIDLibUserClientElementValuesType):%x", ret);
    });
    
    _sharedMemory = (mach_vm_address_t)mappedMem;
    _sharedMemorySize = (mach_vm_size_t)memSize;
    
exit:
    return ret;
}

- (BOOL)validCheck
{
    BOOL valid = false;
    IOReturn ret = kIOReturnError;
    uint64_t output[2];
    uint32_t outputCount = 2;
    
    /*
     * The valid check insures that the device is still open, and that the
     * client has proper privileges in the case of secure input/console user.
     */
    ret = IOConnectCallScalarMethod(_connect,
                                    kIOHIDLibUserClientDeviceIsValid,
                                    0,
                                    0,
                                    output,
                                    &outputCount);
    require_noerr_action (ret, exit,  HIDLogError("IOConnectCallMethod(kIOHIDLibUserClientDeviceIsValid):%x", ret));
    
    valid = output[0];
    
exit:
    return valid;
}

- (IOReturn)start:(NSDictionary * __unused)properties
          service:(io_service_t)service
{
    IOReturn ret  = IOObjectRetain(service);
    require_noerr_action(ret, exit, HIDLogError("IOHIDDeviceClass failed to retain service object with err %x", ret));
    _service = service;
exit:
    return ret;
}

- (IOReturn)stop
{
    return kIOReturnSuccess;
}

static IOReturn _open(void *iunknown, IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me open:options];
}

- (IOReturn)open:(IOOptionBits)options
{
    IOReturn ret = kIOReturnError;
    uint64_t input = options;
    
    ret = [self initConnect];
    require_noerr(ret, exit);
    
    ret = IOConnectCallScalarMethod(_connect,
                                    kIOHIDLibUserClientOpen,
                                    &input,
                                    1,
                                    0,
                                    NULL);
    require_noerr_action(ret, exit, HIDLogError("IOConnectCallMethod(kIOHIDLibUserClientOpen):%x", ret));
    
    _opened = (ret == kIOReturnSuccess);
    
    ret = [self mapMemory];
    require_noerr(ret, exit);
    
    if (_inputReportCallback || _inputReportTimestampCallback) {
        [_queue start];
    }
    
exit:
    return ret;
}

static IOReturn _close(void * iunknown, IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me close:options];
}

- (IOReturn)close:(IOOptionBits __unused)options
{
    IOReturn ret;
    
    require_action(_opened, exit, ret = kIOReturnNotOpen);
    
    ret = [self initConnect];
    require_noerr(ret, exit);
    
    if (_inputReportCallback || _inputReportTimestampCallback) {
        [_queue stop];
    }
    
    [self unmapMemory];
    
    ret = IOConnectCallScalarMethod(_connect,
                                    kIOHIDLibUserClientClose,
                                    0,
                                    0,
                                    0,
                                    NULL);
    
    _opened = false;
    
exit:
    return ret;
}

static IOReturn _getProperty(void *iunknown,
                             CFStringRef key,
                             CFTypeRef *pProperty)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getProperty:(__bridge NSString *)key property:pProperty];
}

- (IOReturn)getProperty:(NSString *)key property:(CFTypeRef *)pProperty
{
    if (!pProperty) {
        return kIOReturnBadArgument;
    }
    
    CFTypeRef prop = (__bridge CFTypeRef)_properties[key];
    
    if (!prop) {
        if ([key isEqualToString:@(kIOHIDUniqueIDKey)]) {
            uint64_t regID;
            IORegistryEntryGetRegistryEntryID(_service, &regID);
            prop = CFNumberCreate(kCFAllocatorDefault,
                                  kCFNumberLongLongType,
                                  &regID);
        } else {
            prop = IORegistryEntrySearchCFProperty(_service,
                                                   kIOServicePlane,
                                                   (__bridge CFStringRef)key,
                                                   kCFAllocatorDefault,
                                                   kIORegistryIterateRecursively
                                                   | kIORegistryIterateParents);
        }
        
        if (prop) {
            _properties[key] = (__bridge id)prop;
            CFRelease(prop);
        }
    }
    
    *pProperty = prop;
    
    return kIOReturnSuccess;
}

static IOReturn _setProperty(void *iunknown,
                             CFStringRef key,
                             CFTypeRef property)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setProperty:(__bridge NSString *)key
                  property:(__bridge id)property];
}

- (IOReturn)setProperty:(NSString *)key property:(id)property
{
    if ([key isEqualToString:@(kIOHIDDeviceSuspendKey)]) {
        require(_queue, exit);
        
        if ([property boolValue]) {
            [_queue stop];
        } else {
            [_queue start];
        }
    }
    
exit:
    _properties[key] = property;
    return kIOReturnSuccess;
}

static IOReturn _getAsyncEventSource(void *iunknown, CFTypeRef *pSource)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getAsyncEventSource:pSource];
}

- (IOReturn)getAsyncEventSource:(CFTypeRef *)pSource
{
    if (!pSource) {
        return kIOReturnBadArgument;
    }
    
    [self initPort];
    
    *pSource = _runLoopSource;
    
    return kIOReturnSuccess;
}

- (NSString *)propertyForElementKey:(NSString *)key
{
    /*
     * This will just convert the first letter in the kIOHIDElement key to
     * lowercase, so we can use it with NSPredicate.
     */
    
    NSString *firstChar = [[key substringToIndex:1] lowercaseString];
    NSString *prop = [key stringByReplacingCharactersInRange:NSMakeRange(0,1)
                                                  withString:firstChar];
    
    return prop;
}

- (NSMutableArray *)copyObsoleteDictionary:(NSArray *)elements
{
    /*
     * The IOHIDObsoleteDeviceClass's version of copyMatchingElements returns an
     * array of dictionaries that contains key/value pairs for each element's
     * values. We have to go through the arduous process of converting the
     * elements' properties into these dictionaries.
     */
    
    NSMutableArray *result = [[NSMutableArray alloc] init];
    
    for (HIDLibElement *element in elements) {
        IOHIDElementStruct eleStruct = element.elementStruct;
        NSMutableDictionary *props = [[NSMutableDictionary alloc] init];
        
        bool nullState = eleStruct.flags & kHIDDataNullStateBit;
        bool prefferedState = eleStruct.flags & kHIDDataNoPreferredBit;
        bool nonLinear = eleStruct.flags & kHIDDataNonlinearBit;
        bool relative = eleStruct.flags & kHIDDataRelativeBit;
        bool wrapping = eleStruct.flags & kHIDDataWrapBit;
        bool array = eleStruct.flags & kHIDDataArrayBit;
        
        props[@(kIOHIDElementCookieKey)] = @(element.elementCookie);
        props[@(kIOHIDElementCollectionCookieKey)] = @(eleStruct.parentCookie);
        props[@(kIOHIDElementTypeKey)] = @(element.type);
        props[@(kIOHIDElementUsageKey)] = @(element.usage);
        props[@(kIOHIDElementUsagePageKey)] = @(element.usagePage);
        props[@(kIOHIDElementReportIDKey)] = @(element.reportID);
        if (eleStruct.duplicateValueSize &&
            eleStruct.duplicateIndex != 0xFFFFFFFF) {
            props[@(kIOHIDElementDuplicateIndexKey)] = @(eleStruct.duplicateIndex);
        }
        props[@(kIOHIDElementSizeKey)] = @(eleStruct.size);
        props[@(kIOHIDElementReportSizeKey)] = @(eleStruct.reportSize);
        props[@(kIOHIDElementReportCountKey)] = @(eleStruct.reportCount);
        props[@(kIOHIDElementHasNullStateKey)] = @(nullState);
        props[@(kIOHIDElementHasPreferredStateKey)] = @(prefferedState);
        props[@(kIOHIDElementIsNonLinearKey)] = @(nonLinear);
        props[@(kIOHIDElementIsRelativeKey)] = @(relative);
        props[@(kIOHIDElementIsWrappingKey)] = @(wrapping);
        props[@(kIOHIDElementIsArrayKey)] = @(array);
        props[@(kIOHIDElementMaxKey)] = @(eleStruct.max);
        props[@(kIOHIDElementMinKey)] = @(eleStruct.min);
        props[@(kIOHIDElementScaledMaxKey)] = @(eleStruct.scaledMax);
        props[@(kIOHIDElementScaledMinKey)] = @(eleStruct.scaledMin);
        props[@(kIOHIDElementUnitKey)] = @(element.unit);
        props[@(kIOHIDElementUnitExponentKey)] = @(element.unitExponent);
        
        [result addObject:props];
    }
    
    return result;
}

static IOReturn _copyMatchingElements(void *iunknown,
                                      CFDictionaryRef matchingDict,
                                      CFArrayRef *pElements,
                                      IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me copyMatchingElements:(__bridge NSDictionary *)matchingDict
                           elements:pElements
                            options:options];
}

- (IOReturn)copyMatchingElements:(NSDictionary *)matching
                        elements:(CFArrayRef *)pElements
                         options:(IOOptionBits __unused)options
{
    IOReturn ret;
    
    if (!pElements) {
        return kIOReturnBadArgument;
    }
    
    ret = [self initElements];
    if (ret != kIOReturnSuccess) {
        return ret;
    }
    
    NSMutableArray *elements = [[NSMutableArray alloc] initWithArray:_elements];
    NSMutableArray *result = nil;
    
    [matching enumerateKeysAndObjectsUsingBlock:^(NSString *key,
                                                  NSNumber *val,
                                                  BOOL *stop __unused)
    {
        @autoreleasepool {
            NSPredicate *predicate = nil;
            NSString *prop;
            NSPredicateOperatorType type = NSEqualToPredicateOperatorType;
            NSExpression *left;
            NSExpression *right;
            
            /*
             * Special case for usage/cookie min/max keys. We want to check
             * the actual usage/cookie key, and verify that is within the range.
             * We use >=/<= operators, rather than == here.
             */
            if ([key isEqualToString:@kIOHIDElementUsageMinKey]) {
                prop = [self propertyForElementKey:@kIOHIDElementUsageKey];
                type = NSGreaterThanOrEqualToPredicateOperatorType;
            } else if ([key isEqualToString:@kIOHIDElementUsageMaxKey]) {
                prop = [self propertyForElementKey:@kIOHIDElementUsageKey];
                type = NSLessThanOrEqualToPredicateOperatorType;
            } else if ([key isEqualToString:@kIOHIDElementCookieMinKey]) {
                prop = [self propertyForElementKey:@kIOHIDElementCookieKey];
                type = NSGreaterThanOrEqualToPredicateOperatorType;
            } else if ([key isEqualToString:@kIOHIDElementCookieMaxKey]) {
                prop = [self propertyForElementKey:@kIOHIDElementCookieKey];
                type = NSLessThanOrEqualToPredicateOperatorType;
            } else {
                prop = [self propertyForElementKey:key];
            }
            
            /*
             * This will continuously filter the elements until we are left with
             * only matching elements.
             */
            
            left = [NSExpression expressionForKeyPath:prop];
            right = [NSExpression expressionForConstantValue:val];
            
            predicate = [NSComparisonPredicate
                         predicateWithLeftExpression:left
                         rightExpression:right
                         modifier:NSDirectPredicateModifier
                         type:type
                         options:0];
            
            @try {
                [elements filterUsingPredicate:predicate];
            } @catch (NSException *e) {
                HIDLogError("Unsupported matching criteria: %@ %@", prop, e);
            }
        }
    }];
    
    require(elements.count, exit);
    
    if (options & kHIDCopyMatchingElementsDictionary) {
        // Handle IOHIDObsoleteDeviceClass's copyMatchingElements
        result = [self copyObsoleteDictionary:elements];
    } else {
        result = [[NSMutableArray alloc] init];
        
        for (HIDLibElement *element in elements) {
            [result addObject:(__bridge id)element.elementRef];
        }
    }
    
exit:
    *pElements = (CFArrayRef)CFBridgingRetain(result);
    
    return kIOReturnSuccess;
}

static IOReturn _setValue(void *iunknown,
                          IOHIDElementRef element,
                          IOHIDValueRef value,
                          uint32_t timeout,
                          IOHIDValueCallback callback,
                          void *context,
                          IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setValue:element
                  value:value
                timeout:timeout
               callback:callback
                context:context
                options:options];
}

- (IOReturn)setValue:(IOHIDElementRef)elementRef
               value:(IOHIDValueRef)value
             timeout:(uint32_t __unused)timeout
            callback:(IOHIDValueCallback __unused)callback
             context:(void * __unused)context
             options:(IOOptionBits)options
{
    IOReturn ret = kIOReturnError;
    HIDLibElement *element = nil;
    HIDLibElement *tmp = nil;
    IOHIDElementValue *elementValue = NULL;
    uint64_t input;
    NSUInteger elementIndex;
    
    require_action(_opened, exit, ret = kIOReturnNotOpen);
    
    ret = [self initElements];
    require_noerr(ret, exit);
    
    require_action([self validCheck], exit, ret = kIOReturnNotPermitted);
    
    tmp = [[HIDLibElement alloc] initWithElementRef:elementRef];
    require_action(tmp, exit, ret = kIOReturnError);
    
    elementIndex = [_elements indexOfObject:tmp];
    require_action(elementIndex != NSNotFound, exit, ret = kIOReturnBadArgument);
    
    element = [_elements objectAtIndex:elementIndex];
    require_action(element.type == kIOHIDElementTypeOutput ||
                   element.type == kIOHIDElementTypeFeature,
                   exit,
                   ret = kIOReturnBadArgument);
    
    // Make sure the location of the element value is within our shared memory
    require_action(element.valueLocation < _sharedMemorySize,
                   exit,
                   ret = kIOReturnError);
    
    // Copy the value to our shared kernel memory
    elementValue = (IOHIDElementValue *)(_sharedMemory + element.valueLocation);
    _IOHIDValueCopyToElementValuePtr(value, elementValue);
    
    require_action(!(options & kHIDSetElementValuePendEvent),
                   exit,
                   ret = kIOReturnSuccess);
    
    input = element.elementCookie;
    ret = IOConnectCallScalarMethod(_connect,
                                    kIOHIDLibUserClientPostElementValues,
                                    &input,
                                    1,
                                    0,
                                    NULL);
    if (ret) {
        HIDLogError("kIOHIDLibUserClientPostElementValues:%x",ret);
    }
exit:
    return ret;
}

static IOReturn _getValue(void *iunknown,
                          IOHIDElementRef element,
                          IOHIDValueRef *pValue,
                          uint32_t timeout,
                          IOHIDValueCallback callback,
                          void *context,
                          IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getValue:element
                  value:pValue
                timeout:timeout
               callback:callback
                context:context
                options:options];
}

- (IOReturn)getValue:(IOHIDElementRef)elementRef
               value:(IOHIDValueRef *)pValue
             timeout:(uint32_t __unused)timeout
            callback:(IOHIDValueCallback __unused)callback
             context:(void * __unused)context
             options:(IOOptionBits)options
{
    IOReturn ret = kIOReturnError;
    HIDLibElement *element = nil;
    HIDLibElement *tmp = nil;
    IOHIDElementValue *elementValue = NULL;
    uint64_t timestamp;
    uint64_t input;
    NSUInteger elementIndex;
    
    if (!pValue) {
        return kIOReturnBadArgument;
    }
    
    require_action(_opened, exit, ret = kIOReturnNotOpen);
    
    ret = [self initElements];
    require_noerr(ret, exit);
    
    require_action([self validCheck], exit, ret = kIOReturnNotPermitted);
    
    tmp = [[HIDLibElement alloc] initWithElementRef:elementRef];
    require_action(tmp, exit, ret = kIOReturnError);
    
    elementIndex = [_elements indexOfObject:tmp];
    require_action(elementIndex != NSNotFound,
                   exit,
                   ret = kIOReturnBadArgument);
    
    element = [_elements objectAtIndex:elementIndex];
    require_action(element.type != kIOHIDElementTypeCollection,
                   exit,
                   ret = kIOReturnBadArgument);
    require_action(element.valueLocation < _sharedMemorySize,
                   exit,
                   ret = kIOReturnError);
    
    // Copy the value to our shared kernel memory
    elementValue = (IOHIDElementValue *)(_sharedMemory + element.valueLocation);
    timestamp = *((uint64_t *)&(elementValue->timestamp));
    
    // Check if we need to update our value
    if (!element.valueRef ||
        element.timestamp < timestamp ||
        element.type == kIOHIDElementTypeFeature) {
        IOHIDValueRef valueRef;
        
        valueRef = _IOHIDValueCreateWithElementValuePtr(kCFAllocatorDefault,
                                                        element.elementRef,
                                                        elementValue);
        
        if (valueRef) {
            element.valueRef = valueRef;
            CFRelease(valueRef);
        }
    }
    
    *pValue = element.valueRef;
    
    // Do not call to the kernel if options prevent poll
    require_action(!(options & kHIDGetElementValuePreventPoll),
                   exit,
                   ret = kIOReturnSuccess);
    
    // Only call to kernel if specified in options or this is a feature element
    require_action(options & kHIDGetElementValueForcePoll ||
                   element.type == kIOHIDElementTypeFeature,
                   exit,
                   ret = kIOReturnSuccess);
    
    input = element.elementCookie;
    ret = IOConnectCallScalarMethod(_connect,
                                    kIOHIDLibUserClientUpdateElementValues,
                                    &input,
                                    1,
                                    0,
                                    NULL);
    require_noerr(ret, exit);
    
    // Update our value after kernel call
    timestamp = *((uint64_t *)&(elementValue->timestamp));
    
    // Check if we need to update our value
    if (!element.valueRef ||
        element.timestamp < timestamp ||
        element.type == kIOHIDElementTypeFeature) {
        IOHIDValueRef valueRef;
        
        valueRef = _IOHIDValueCreateWithElementValuePtr(kCFAllocatorDefault,
                                                        element.elementRef,
                                                        elementValue);
        
        if (valueRef) {
            element.valueRef = valueRef;
            CFRelease(valueRef);
        }
    }
    
    *pValue = element.valueRef;
    
exit:
    return ret;
}

static void _valueAvailableCallback(void *context,
                                    IOReturn result,
                                    void *sender __unused)
{
    IOHIDDeviceClass *me = (__bridge IOHIDDeviceClass *)context;
    [me valueAvailableCallback:result];
}

- (void)valueAvailableCallback:(IOReturn)result
{
    IOHIDValueRef value;
    CFIndex size = 0;
    
    while ((result = [_queue copyNextValue:&value]) == kIOReturnSuccess) {
        IOHIDElementRef element;
        uint32_t reportID;
        uint64_t timestamp;
        
        if (IOHIDValueGetBytePtr(value) && IOHIDValueGetLength(value)) {
            size = min(_inputReportBufferLength, IOHIDValueGetLength(value));
            bcopy(IOHIDValueGetBytePtr(value), _inputReportBuffer, size);
        }
        
        element = IOHIDValueGetElement(value);
        reportID = IOHIDElementGetReportID(element);
        timestamp = IOHIDValueGetTimeStamp(value);
        
        if (IOHIDFAMILY_HID_TRACE_ENABLED()) {
            
            uint64_t regID;
            IORegistryEntryGetRegistryEntryID(_service, &regID);
            
            IOHIDFAMILY_HID_TRACE(kHIDTraceHandleReport, (uintptr_t)regID, (uintptr_t)reportID, (uintptr_t)size, (uintptr_t)timestamp, (uintptr_t)_inputReportBuffer);
            
        }
        
        if (_inputReportCallback) {
            (_inputReportCallback)(_inputReportContext,
                                   result,
                                   &_device,
                                   kIOHIDReportTypeInput,
                                   reportID,
                                   _inputReportBuffer,
                                   size);
        }

        if (_inputReportTimestampCallback) {
            (_inputReportTimestampCallback)(_inputReportContext,
                                            result,
                                            &_device,
                                            kIOHIDReportTypeInput,
                                            reportID,
                                            _inputReportBuffer,
                                            size,
                                            timestamp);
        }
        
        CFRelease(value);
    }
}

static IOReturn _setInputReportCallback(void *iunknown,
                                        uint8_t *report,
                                        CFIndex reportLength,
                                        IOHIDReportCallback callback,
                                        void *context,
                                        IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setInputReportCallback:report
                         reportLength:reportLength
                             callback:callback
                              context:context
                              options:options];
}

- (IOReturn)setInputReportCallback:(uint8_t *)report
                      reportLength:(CFIndex)reportLength
                          callback:(IOHIDReportCallback)callback
                           context:(void *)context
                           options:(IOOptionBits __unused)options
{
    _inputReportBuffer = report;
    _inputReportBufferLength = reportLength;
    _inputReportContext = context;
    _inputReportCallback = callback;
    
    [self initQueue];
    
    if (_opened) {
        [_queue start];
    }
    
    return kIOReturnSuccess;
}

typedef struct {
    IOHIDReportType type;
    uint8_t *buffer;
    uint32_t reportID;
    IOHIDReportCallback callback;
    void *context;
    void *sender;
} AsyncReportContext;

static void _asyncCallback(void *context, IOReturn result, uint32_t bufferSize)
{
    AsyncReportContext *asyncContext = (AsyncReportContext *)context;
    
    if (!asyncContext || !asyncContext->callback) {
        return;
    }
    
    ((IOHIDReportCallback)asyncContext->callback)(asyncContext->context,
                                                  result,
                                                  asyncContext->sender,
                                                  asyncContext->type,
                                                  asyncContext->reportID,
                                                  asyncContext->buffer,
                                                  bufferSize);
    
    free(asyncContext);
}

static IOReturn _setReport(void *iunknown,
                           IOHIDReportType reportType,
                           uint32_t reportID,
                           const uint8_t *report,
                           CFIndex reportLength,
                           uint32_t timeout,
                           IOHIDReportCallback callback,
                           void *context,
                           IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setReport:reportType
                reportID:reportID
                  report:report
            reportLength:reportLength
                 timeout:timeout
                callback:callback
                 context:context
                 options:options];
}

- (IOReturn)setReport:(IOHIDReportType)reportType
             reportID:(uint32_t)reportID
               report:(const uint8_t *)report
         reportLength:(CFIndex)reportLength
              timeout:(uint32_t)timeout
             callback:(IOHIDReportCallback)callback
              context:(void *)context
              options:(IOOptionBits __unused)options
{
    IOReturn ret = kIOReturnError;
    uint64_t input[3] = { 0 };
    
    input[0] = reportType;
    input[1] = reportID;
    
    require_action(_opened, exit, ret = kIOReturnNotOpen);
    require_action([self validCheck], exit, ret = kIOReturnNotPermitted);
    
    if (callback) {
        io_async_ref64_t asyncRef;
        AsyncReportContext *asyncContext;
        
        input[2] = timeout;
        
        asyncContext = (AsyncReportContext *)malloc(sizeof(AsyncReportContext));
        require(asyncContext, exit);
        
        asyncContext->type = reportType;
        asyncContext->buffer = (uint8_t *)report;
        asyncContext->reportID = reportID;
        asyncContext->callback = callback;
        asyncContext->context = context;
        asyncContext->sender = &_device;
        
        asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)_asyncCallback;
        asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)asyncContext;
        
        [self initPort];
        
        ret = IOConnectCallAsyncMethod(_connect,
                                       kIOHIDLibUserClientSetReport,
                                       _port,
                                       asyncRef,
                                       kIOAsyncCalloutCount,
                                       input,
                                       3,
                                       report,
                                       reportLength,
                                       0,
                                       0,
                                       0,
                                       0);
        if (ret != kIOReturnSuccess) {
            free(asyncContext);
        }
    }
    else {
        ret = IOConnectCallMethod(_connect,
                                  kIOHIDLibUserClientSetReport,
                                  input,
                                  3,
                                  report,
                                  reportLength,
                                  0,
                                  0,
                                  0,
                                  0);
    }
    
exit:
    return ret;
}

static IOReturn _getReport(void *iunknown,
                           IOHIDReportType reportType,
                           uint32_t reportID,
                           uint8_t *report,
                           CFIndex *pReportLength,
                           uint32_t timeout,
                           IOHIDReportCallback callback,
                           void *context,
                           IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me getReport:reportType
                reportID:reportID
                  report:report
            reportLength:pReportLength
                 timeout:timeout
                callback:callback
                 context:context
                 options:options];
}

- (IOReturn)getReport:(IOHIDReportType)reportType
             reportID:(uint32_t)reportID
               report:(uint8_t *)report
         reportLength:(CFIndex *)pReportLength
              timeout:(uint32_t)timeout
             callback:(IOHIDReportCallback)callback
              context:(void *)context
              options:(IOOptionBits __unused)options
{
    IOReturn ret = kIOReturnError;
    uint64_t input[3] = { 0 };
    size_t reportLength = *pReportLength;
    
    if (!pReportLength || *pReportLength <= 0) {
        return kIOReturnBadArgument;
    }
    
    require_action(_opened, exit, ret = kIOReturnNotOpen);
    require_action([self validCheck], exit, ret = kIOReturnNotPermitted);
    
    input[0] = reportType;
    input[1] = reportID;
    
    if (callback) {
        io_async_ref64_t asyncRef;
        AsyncReportContext *asyncContext;
        
        input[2] = timeout;
        
        asyncContext = (AsyncReportContext *)malloc(sizeof(AsyncReportContext));
        require(asyncContext, exit);
        
        asyncContext->type = reportType;
        asyncContext->buffer = (uint8_t *)report;
        asyncContext->reportID = reportID;
        asyncContext->callback = callback;
        asyncContext->context = context;
        asyncContext->sender = &_device;
        
        asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)_asyncCallback;
        asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)asyncContext;
        
        [self initPort];
        
        ret = IOConnectCallAsyncMethod(_connect,
                                       kIOHIDLibUserClientGetReport,
                                       _port,
                                       asyncRef,
                                       kIOAsyncCalloutCount,
                                       input,
                                       3,
                                       0,
                                       0,
                                       0,
                                       0,
                                       report,
                                       &reportLength);
        if (ret != kIOReturnSuccess) {
            free(asyncContext);
        }
    }
    else {
        ret = IOConnectCallMethod(_connect,
                                  kIOHIDLibUserClientGetReport,
                                  input,
                                  3,
                                  0,
                                  0,
                                  0,
                                  0,
                                  report,
                                  &reportLength);
    }
    
    *pReportLength = reportLength;
    
exit:
    return ret;
}

static IOReturn _setInputReportWithTimeStampCallback(void *iunknown,
                                    uint8_t *report,
                                    CFIndex reportLength,
                                    IOHIDReportWithTimeStampCallback callback,
                                    void *context,
                                    IOOptionBits options)
{
    IUnknownVTbl *vtbl = *((IUnknownVTbl**)iunknown);
    IOHIDDeviceClass *me = (__bridge id)vtbl->_reserved;
    
    return [me setInputReportWithTimeStampCallback:report
                                      reportLength:reportLength
                                          callback:callback
                                           context:context
                                           options:options];
}

- (IOReturn)setInputReportWithTimeStampCallback:(uint8_t *)report
                        reportLength:(CFIndex)reportLength
                            callback:(IOHIDReportWithTimeStampCallback)callback
                            context:(void *)context
                            options:(IOOptionBits __unused)options
{
    _inputReportBuffer = report;
    _inputReportBufferLength = reportLength;
    _inputReportContext = context;
    _inputReportTimestampCallback = callback;
    
    [self initQueue];
    
    if (_opened) {
        [_queue start];
    }
    
    return kIOReturnSuccess;
}

- (instancetype)init
{
    self = [super init];
    
    if (!self) {
        return nil;
    }
    
    _device = (IOHIDDeviceTimeStampedDeviceInterface *)malloc(sizeof(*_device));
    
    *_device = (IOHIDDeviceTimeStampedDeviceInterface) {
        // IUNKNOWN_C_GUTS
        ._reserved = (__bridge void *)self,
        .QueryInterface = self->_vtbl->QueryInterface,
        .AddRef = self->_vtbl->AddRef,
        .Release = self->_vtbl->Release,
        
        // IOHIDDeviceTimeStampedDeviceInterface
        .open = _open,
        .close = _close,
        .getProperty = _getProperty,
        .setProperty = _setProperty,
        .getAsyncEventSource = _getAsyncEventSource,
        .copyMatchingElements = _copyMatchingElements,
        .setValue = _setValue,
        .getValue = _getValue,
        .setInputReportCallback = _setInputReportCallback,
        .setReport = _setReport,
        .getReport = _getReport,
        .setInputReportWithTimeStampCallback = _setInputReportWithTimeStampCallback
    };
    
    _properties = [[NSMutableDictionary alloc] init];
    
    return self;
}

- (void)dealloc
{
    free(_device);
    
    [self unmapMemory];
    
    if (_runLoopSource) {
        CFRelease(_runLoopSource);
    }
    
    if (_machPort) {
        CFMachPortInvalidate(_machPort);
        CFRelease(_machPort);
    }
    
    if (_port) {
        mach_port_mod_refs(mach_task_self(),
                           _port,
                           MACH_PORT_RIGHT_RECEIVE,
                           -1);
    }
    
    if (_connect) {
        IOServiceClose(_connect);
    }
    
    if (_service) {
        IOObjectRelease(_service);
    }
}

@end
