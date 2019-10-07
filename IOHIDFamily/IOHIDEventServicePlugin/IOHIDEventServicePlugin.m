//
//  IOHIDEventServicePlugin.m
//  IOHIDEventServicePlugin
//
//  Created by dekom on 10/10/18.
//

#import <Foundation/Foundation.h>
#import "IOHIDEventServicePlugin.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import "IOHIDPrivateKeys.h"
#import "IOHIDevicePrivateKeys.h"
#import "IOHIDDebug.h"
#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFSerialize.h>
#import <IOKit/IODataQueueClient.h>
#import <os/log.h>
#import "IOHIDEventServiceUserClient.h"
#import <dispatch/private.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/hid/IOHIDEventData.h>
#import <mach/mach_vm.h>
#import <IOKit/hid/IOHIDAnalytics.h>
#if TARGET_OS_IPHONE
#import <IOKit/hid/AppleEmbeddedHIDKeys.h>
#endif



#define PluginLogError(fmt, ...)   os_log_error(_HIDLogCategory(kHIDLogServicePlugin), "0x%llx: " fmt, _regID, ##__VA_ARGS__)
#define PluginLog(fmt, ...)        os_log(_HIDLogCategory(kHIDLogServicePlugin), "0x%llx: " fmt, _regID, ##__VA_ARGS__)
#define PluginLogDebug(fmt, ...)   os_log_debug(_HIDLogCategory(kHIDLogServicePlugin), "0x%llx: " fmt, _regID, ##__VA_ARGS__)

#ifdef DEBUG_ASSERT_MESSAGE
#undef DEBUG_ASSERT_MESSAGE
#endif

#define DEBUG_ASSERT_MESSAGE(name, assertion, label, message, file, line, value) \
PluginLogError("AssertMacros: %s, %s", assertion, (message!=0) ? message : "");

#import <AssertMacros.h>

#define USAGE_BUCKETS 11
#define BUCKET_DENOM (100/(USAGE_BUCKETS-1))

@implementation IOHIDEventServicePlugin {
    io_service_t _service;
    io_connect_t _connect;
    uint64_t _regID;
    mach_port_t _queuePort;
    IODataQueueMemory *_sharedMemory;
    mach_vm_size_t _sharedMemorySize;
    uint32_t _lastTail;
    NSSet *_providerProps;
    HIDBlock _cancelHandler;
    dispatch_queue_t _queue;
    dispatch_mach_t _machChannel;
    uint64_t _eventCount;
    uint64_t _notificationCount;
    uint64_t _lastEventTimestamp;
    CFTypeRef _usageAnalytics;
    uint64_t _usageCounts[USAGE_BUCKETS];
}

+ (BOOL)matchService:(io_service_t)service
             options:(NSDictionary * __unused)options
               score:(NSInteger * __unused)score
{
    if(IOObjectConformsTo(service, "IOHIDEventService")) {
        return true;
    }
    
    return false;
}

- (void)unmapMemory
{
#if !__LP64__
    vm_address_t        mappedMem = (vm_address_t)_sharedMemory;
#else
    mach_vm_address_t   mappedMem = (mach_vm_address_t)_sharedMemory;
#endif
    
    if (_sharedMemory) {
        IOConnectUnmapMemory(_connect,
                             0,
                             mach_task_self(),
                             mappedMem);
        
        _sharedMemory = 0;
        _sharedMemorySize = 0;
    }

    if (_usageAnalytics) {
        IOHIDAnalyticsEventCancel(_usageAnalytics);
        CFRelease(_usageAnalytics);
        _usageAnalytics = NULL;
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
                             0,
                             mach_task_self(),
                             &mappedMem,
                             &memSize,
                             kIOMapAnywhere);
    require_noerr(ret, exit);
    
    _sharedMemory = (IODataQueueMemory *)mappedMem;
    _sharedMemorySize = (mach_vm_size_t)memSize;

    [self setupAnalytics];
    
exit:
    return ret;
}

- (instancetype)initWithService:(io_service_t)service
{
    IOReturn ret = kIOReturnError;
    bool result = false;
    uint64_t input = 0;
    boolean_t createQueue = true;
    NSNumber *queueSize = nil;
    
    self = [super init];
    if (!self) {
        return self;
    }
    

    ret = IOObjectRetain(service);
    require_noerr_action(ret, exit,
                             PluginLogError("Failed to retain service object 0x%x", ret));
    
    _service = service;
    
    IORegistryEntryGetRegistryEntryID(_service, &_regID);
    
    ret = IOServiceOpen(_service,
                        mach_task_self(),
                        kIOHIDEventServiceUserClientType,
                        &_connect);
    require_noerr_action(ret, exit,
                         PluginLogError("IOServiceOpen failed: 0x%x", ret));
    
    ret = IOConnectCallScalarMethod(_connect,
                                    kIOHIDEventServiceUserClientOpen,
                                    &input,
                                    1,
                                    0,
                                    0);
    require_noerr_action(ret, exit,
                         PluginLogError("user client open failed: 0x%x", ret));
    
    queueSize = (NSNumber *)CFBridgingRelease(IORegistryEntryCreateCFProperty(
                                            _service,
                                            CFSTR(kIOHIDEventServiceQueueSize),
                                            kCFAllocatorDefault,
                                            0));
    
    if (queueSize && queueSize.unsignedIntegerValue == 0) {
        createQueue = false;
    }
    
    if (createQueue) {
        ret = [self mapMemory];
        require_noerr_action(ret, exit,
                             PluginLogError("Failed to map memory: 0x%x", ret));
        
        _queuePort = IODataQueueAllocateNotificationPort();
        require_action(_queuePort, exit,
                       PluginLogError("Failed to create queue port"));
    }
    
    // Set of properties to be queried on the provider only.
    _providerProps = [NSSet setWithObjects:
                      @(kIOHIDScrollAccelerationTypeKey),
                      @(kIOHIDPointerAccelerationTypeKey),
                      @(kIOHIDPointerAccelerationTableKey),
                      @(kIOHIDScrollAccelerationTableKey),
                      @(kIOHIDScrollAccelerationTableXKey),
                      @(kIOHIDScrollAccelerationTableYKey),
                      @(kIOHIDScrollAccelerationTableZKey),
                      @(kIOHIDBuiltInKey),
                      @("IOUserClass"),
                      nil];
    
    result = true;
    
exit:
    if (!result) {
        PluginLogError("initWithService failed");
        return nil;
    }
    
    return self;
}

- (void)dealloc
{
    if (_connect) {
        uint64_t input = 0;
        
        IOConnectCallScalarMethod(_connect,
                                  kIOHIDEventServiceUserClientClose,
                                  &input,
                                  1,
                                  0,
                                  0);
        
        IOConnectSetNotificationPort(_connect, 0, MACH_PORT_NULL, 0);
        [self unmapMemory];
        IOServiceClose(_connect);
    }
    
    if (_queuePort) {
        mach_port_mod_refs(mach_task_self(),
                           _queuePort,
                           MACH_PORT_RIGHT_RECEIVE,
                           -1);
    }
    
    if (_service) {
        IOObjectRelease(_service);
    }
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"IOHIDEventServicePlugin: 0x%llx",
            _regID];
}

- (nullable id)propertyForKey:(NSString *)key
                       client:(HIDConnection * __unused)client
{
    id result = nil;
    
    if ([key isEqualToString:@(kIOHIDServicePluginDebugKey)]) {
        // debug dictionary that gets captured by hidutil
        NSMutableDictionary *debug = [NSMutableDictionary new];

        debug[@"PluginName"] = @"IOHIDEventServicePlugin";
        debug[@"EventCount"] = @(_eventCount);
        debug[@"LastEventTimestamp"] = @(_lastEventTimestamp);
        debug[@"QueueUsagePercentHist"] = [self usageCountsDict];
        debug[@"NotificationCount"] = @(_notificationCount);
        result = debug;
    } else {
        NSDictionary *props = CFBridgingRelease(IORegistryEntryCreateCFProperty(
                                        _service,
                                        CFSTR(kIOHIDEventServicePropertiesKey),
                                        kCFAllocatorDefault,
                                        0));
        if (props) {
            result = props[key];
        }
    }
    
    if (!result) {
        if ([_providerProps containsObject:key]) {
            result = CFBridgingRelease(IORegistryEntryCreateCFProperty(
                                                    _service,
                                                    (__bridge CFStringRef)key,
                                                    kCFAllocatorDefault,
                                                    0));
        } else {
            result = CFBridgingRelease(IORegistryEntrySearchCFProperty(
                                                _service,
                                                kIOServicePlane,
                                                (__bridge CFStringRef)key,
                                                kCFAllocatorDefault,
                                                kIORegistryIterateRecursively |
                                                kIORegistryIterateParents));
        }
    }
    
    return result;
}

- (BOOL)setProperty:(id)value
             forKey:(NSString *)key
             client:(HIDConnection * __unused)client
{
#if TARGET_OS_IPHONE
    if ([key isEqualToString:@(kIOHIDAccelerometerShakeKey)] &&
        [value isKindOfClass:[NSDictionary class]]) {
        NSMutableDictionary *newVal = [NSMutableDictionary new];
        
        [value enumerateKeysAndObjectsUsingBlock:^(NSString *aKey,
                                                   NSNumber *val,
                                                   BOOL *stop __unused) {
            if (CFNumberIsFloatType((__bridge CFNumberRef)val)) {
                IOFixed fixed = 0;
                fixed = val.doubleValue * 65535;
                val = [NSNumber numberWithInteger:fixed];
            }
            
            newVal[aKey] = val;
        }];
        
        value = newVal;
    }
#endif
    
    return (IOConnectSetCFProperty(
                                _connect,
                                (__bridge CFStringRef)key,
                                (__bridge CFTypeRef)value) == kIOReturnSuccess);
}

- (HIDEvent *)eventMatching:(NSDictionary *)matching
                  forClient:(HIDConnection * __unused)client
{
    HIDEvent *event = nil;
    IOReturn ret = kIOReturnError;
    NSData *matchingData = nil;
    void *output = nil;
    size_t outputSize = kIOConnectMethodVarOutputSize;
    
    if (matching) {
        matchingData = (NSData *)CFBridgingRelease(IOCFSerialize(
                                            (__bridge CFDictionaryRef)matching,
                                            kIOCFSerializeToBinary));
        require_action(matchingData, exit, PluginLogError("serialize fail"));
    }
    
    ret = IOConnectCallStructMethod(_connect,
                                    kIOHIDEventServiceUserClientCopyMatchingEvent,
                                    [matchingData bytes],
                                    [matchingData length],
                                    &output,
                                    &outputSize);
    require_action(ret == kIOReturnSuccess && output, exit,
                   PluginLogError("kIOHIDEventServiceUserClientCopyMatchingEvent:0x%x matching:%@ ", ret, matching));
    
    event = [[HIDEvent alloc] initWithBytes:output length:outputSize];
    
exit:
    if (output) {
        mach_vm_deallocate(mach_task_self(),
                           (mach_vm_address_t)output,
                           outputSize);
    }
    
    return event;
}

- (HIDEvent *)copyEvent:(IOHIDEventType)type
               matching:(HIDEvent *)matching
                options:(IOOptionBits)options
{
    HIDEvent *event = nil;
    NSData *eventData = nil;
    NSMutableData *outData = nil;
    size_t outSize = 0;
    uint64_t input[2];
    IOReturn ret = kIOReturnError;
    
    if (matching) {
        eventData = [matching serialize:HIDEventSerializationTypeFast
                                  error:nil];
    }
    
    input[0] = type;
    input[1] = options;
    
    IOHIDEventGetQueueElementSize(type, outSize);
    require(outSize, exit);

    if (matching) {
        if (matching.type == kIOHIDEventTypeVendorDefined) {
            outSize += [matching integerValueForField:kIOHIDEventFieldVendorDefinedDataLength];
        }
    }
    
    outData = [NSMutableData dataWithLength:outSize];
    require(outData, exit);
    
    ret = IOConnectCallMethod(_connect,
                              kIOHIDEventServiceUserClientCopyEvent,
                              input, 2, [eventData bytes], [eventData length],
                              NULL, NULL, [outData mutableBytes], &outSize);
    require_noerr_action(ret, exit, {
        if (ret != kIOReturnUnsupported) {
            PluginLogError("kIOHIDEventServiceUserClientCopyEvent:0x%x type:%d event:%@",
                           ret,
                           (int)type,
                           event);
        }
    });
    
    outData.length = outSize;
    event = [[HIDEvent alloc] initWithData:outData];
    
exit:
    return event;
}

- (IOReturn)setOutputEvent:(HIDEvent *)event
{
    IOReturn ret = kIOReturnUnsupported;
    NSUInteger usage = 0;
    uint64_t input[3] = { 0 };
    
    require(event.type == kIOHIDEventTypeLED, exit);
    
    usage = [event integerValueForField:kIOHIDEventFieldLEDNumber];
    
    input[0] = kHIDPage_LEDs;
    input[1] = usage;
    input[2] = [event integerValueForField:kIOHIDEventFieldLEDState];
    
    ret = IOConnectCallMethod(_connect,
                              kIOHIDEventServiceUserClientSetElementValue,
                              input, 3, NULL, 0,
                              NULL, NULL, NULL, NULL);
    
    if (ret == kIOReturnUnsupported &&
        usage >= kHIDUsage_LED_Player1 &&
        usage <= kHIDUsage_LED_Player8) {
        input[1] = kHIDPage_AppleVendor | usage - kHIDUsage_LED_Player1;
        
        ret = IOConnectCallMethod(_connect,
                                  kIOHIDEventServiceUserClientSetElementValue,
                                  input, 3, NULL, 0,
                                  NULL, NULL, NULL, NULL);
    }
    
exit:
    return ret;
}

- (void)setEventDispatcher:(id<HIDEventDispatcher>)dispatcher
{
    _dispatcher = dispatcher;
}

- (void)setCancelHandler:(HIDBlock)handler
{
    _cancelHandler = handler;
}

- (void)dequeueEvents
{
    IODataQueueEntry *entry;

    _notificationCount++;
    
    require(_sharedMemory, exit);

    [self updateUsageAnalytics];
    
    while ((entry = IODataQueuePeek(_sharedMemory))) {
        HIDEvent *event = [[HIDEvent alloc] initWithBytes:&(entry->data)
                                                   length:entry->size];
        
        if (event) {
            [_dispatcher dispatchEvent:event];
            _eventCount++;
            _lastEventTimestamp = event.timestamp;
        }
        
        IODataQueueDequeue(_sharedMemory, NULL, NULL);
    }
    
exit:
    return;
}

static void machHandler(void *context,
                        dispatch_mach_reason_t reason,
                        dispatch_mach_msg_t message __unused,
                        mach_error_t error __unused)
{
    IOHIDEventServicePlugin *me = (__bridge IOHIDEventServicePlugin *)context;
    
    switch (reason) {
        case DISPATCH_MACH_MESSAGE_RECEIVED:
            [me dequeueEvents];
            break;
        case DISPATCH_MACH_CANCELED:
            me->_cancelHandler();
            break;
        default:
            break;
    }
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    _queue = queue;
    
    _machChannel = dispatch_mach_create_f("IOHIDEventServicePluginMachChannel",
                                          _queue,
                                          (__bridge void *)self,
                                          machHandler);
}

- (void)activate
{
    IOConnectSetNotificationPort(_connect, 0, _queuePort, 0);
    dispatch_mach_connect(_machChannel, _queuePort, MACH_PORT_NULL, 0);
}

- (void)cancel
{
    uint64_t input = 0;
    
    IOConnectCallScalarMethod(_connect,
                              kIOHIDEventServiceUserClientClose,
                              &input,
                              1,
                              0,
                              0);
    
    IOConnectSetNotificationPort(_connect, 0, MACH_PORT_NULL, 0);
    [self unmapMemory];
    IOServiceClose(_connect);
    _connect = MACH_PORT_NULL;
    
    dispatch_mach_cancel(_machChannel);
}

- (bool)setupAnalytics
{
    bool                    result = false;
    NSMutableDictionary *   eventDesc = [@{ @"staticSize"   : @(_sharedMemorySize),
                                            @"queueType"    : @"serviceQueue"
                                        } mutableCopy];
    IOHIDAnalyticsHistogramSegmentConfig analyticsConfig = {
        .bucket_count       = 8,
        .bucket_width       = 13,
        .bucket_base        = 0,
        .value_normalizer   = 1,
    };
    NSDictionary *pairs = CFBridgingRelease(IORegistryEntryCreateCFProperty(
                                                                           _service,
                                                                           CFSTR(kIOHIDDeviceUsagePairsKey),
                                                                           kCFAllocatorDefault,
                                                                           0));
    NSString *transport = CFBridgingRelease(IORegistryEntryCreateCFProperty(
                                                                            _service,
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
    
    require_action(_usageAnalytics, exit, PluginLogError("Unable to create queue analytics"));


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

    require(_sharedMemory, exit);
    require(_usageAnalytics, exit);

    head = (uint32_t)_sharedMemory->head;
    tail = (uint32_t)_sharedMemory->tail;

    // Submit queue usage at local maximum queue size.
    // (first call to dequeue in a series w/o enqueue)
    if (tail == _lastTail) {
        return;
    }

    if (head < tail) {
        queueUsage = tail - head;
    }
    else {
        queueUsage = _sharedMemorySize - (head - tail);
    }
    queueUsage = (queueUsage * 100) / _sharedMemorySize;

    IOHIDAnalyticsHistogramEventSetIntegerValue(_usageAnalytics, queueUsage);

    // Bucket the % usage into 0, 1, 2, 3, ...
    queueUsage /= BUCKET_DENOM;
    _usageCounts[queueUsage]++;
    
    _lastTail = tail;

exit:
    return;
}

- (NSDictionary *)usageCountsDict
{
    NSMutableDictionary * dict = [NSMutableDictionary new];

    for (size_t i = 0; i < USAGE_BUCKETS; i++) {
        dict[@((i*BUCKET_DENOM)).stringValue] = @(_usageCounts[i]);
    }

    return dict;
}

@end
