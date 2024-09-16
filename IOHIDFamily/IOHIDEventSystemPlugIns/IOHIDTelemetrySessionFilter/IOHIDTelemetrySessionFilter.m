//
//  IOHIDTelemetrySessionFilter.m
//  IOHIDTelemetrySessionFilter
//
//

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDSessionFilterPlugIn.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDEventData.h>
#include <IOKit/hid/IOHIDSession.h>
#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <IOKit/hid/AppleHIDUsageTables.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include "IOHIDTelemetrySessionFilter.h"
#include <IOKit/hidsystem/IOHIDParameter.h>
#include "IOHIDEventTranslation.h"
#include <IOKit/IOMessage.h>
#include <CoreAnalytics/CoreAnalytics.h>
#include <CoreUtils/CoreUtils.h>

#define HID_TELEMETRY_LOG_INTERVAL 43200 //sec


@interface HIDEventServiceTelemetry : NSObject {
    uint64_t _eventCount;
    NSMutableDictionary *_properties;
}

@property uint64_t eventCount;

@property NSMutableDictionary *properties;

@end

@implementation HIDEventServiceTelemetry

- (instancetype)initWithService: (HIDEventService *)service
{
    id property;
    
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _eventCount = 0;
    
    _properties = [[NSMutableDictionary alloc] init];
    
    property = (__bridge id)IOHIDServiceGetRegistryID((__bridge IOHIDServiceRef)service);
    _properties[@"serviceid"] = property ? property : 0;
    
    property = [service propertyForKey:@kIOHIDServiceDeviceUsagePairsKey];
    _properties[@"usagepairs"] = property ? property : @"None";
    
    property = [service propertyForKey:@kIOHIDTransportKey];
    _properties[@"transport"] = property ? property : @"None";
    
    property = [service propertyForKey:@kIOHIDServiceProductIDKey];
    _properties[@"productid"] = property ? property : 0;
    
    property = [service propertyForKey:@kIOHIDProductKey];
    _properties[@"productdescription"] = property ? property : @"None";
    
    property = [service propertyForKey:@kIOHIDServiceVendorIDKey];
    _properties[@"vendorid"] = property ? property : 0;
   
    property = [service propertyForKey:@kIOHIDManufacturerKey];
    _properties[@"manufacturer"] = property ? property : @"None";
   
    property = [service propertyForKey:@kIOHIDBuiltInKey];
    _properties[@"builtin"] = [property boolValue] ? @(YES) : @(NO);
   
    return self;
}

@end



@implementation IOHIDTelemetrySessionFilter {
    dispatch_queue_t            _queue;
    dispatch_source_t           _timer;
    CFMutableDictionaryRef      _serviceCounts;
}

static void logUsageInfo(const void *key __unused, const void *value, void *context)
{
    HIDLog("IOHIDTelemetrySessionFilter::logUsageInfo");
    HIDEventServiceTelemetry *serviceInfo = (__bridge HIDEventServiceTelemetry *)value;
    HIDEventService *service = (__bridge HIDEventService *)key;
    
    if (serviceInfo.eventCount) {
        NSMutableDictionary *usageData = [NSMutableDictionary dictionaryWithDictionary:serviceInfo.properties];
        
        usageData[@"eventcounter"] = [[NSNumber alloc] initWithLongLong:serviceInfo.eventCount];
        
        HIDLogInfo("IOHIDTelemetrySessionFilter dispatch com.apple.hid.device.usage event: %@", usageData);
        
        AnalyticsSendEventLazy(@"com.apple.hid.device.usage", ^NSDictionary<NSString *,NSObject *> *{
            return usageData;
        });
        
        serviceInfo.eventCount = 0;
    }
}

- (instancetype)initWithSession:(HIDSession *)session
{
    HIDLogDebug("IOHIDTelemetrySessionFilter::initWithSession: %@", session);
    
    self = [super init];
    if (!self) {
        return self;
    }
    
    _serviceCounts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    _queue = dispatch_queue_create("com.apple.hidtelemetry", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0));
    
    if (!_queue) {
        return nil;
    }
    
    _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    
    if (!_timer) {
        return nil;
    }
    
    //Event handler:
        //store mapping of serviceID->relevant service info + count
        //when event handler is called, log analytics info per service, and reset count value
    dispatch_source_set_event_handler(_timer, ^{
        CFDictionaryApplyFunction(self->_serviceCounts, logUsageInfo, NULL);
    });
    
    dispatch_source_set_timer(_timer, DISPATCH_TIME_FOREVER, 0, 0);
    dispatch_activate(_timer);
    
    return self;
}

- (void)dealloc
{
    
}

- (NSString *)description
{
    return @"IOHIDTelemetrySessionFilter";
}

static void addDebugInfo(const void *key __unused, const void *value, void *context)
{
    HIDEventServiceTelemetry *serviceInfo = (__bridge HIDEventServiceTelemetry *)value;
    NSMutableArray *serviceIDsAndCounts = (__bridge NSMutableArray *)context;
    NSMutableDictionary *entryDict = [NSMutableDictionary new];
    
    entryDict[@"ServiceID"] = serviceInfo.properties[@"serviceid"];
    entryDict[@"Count"] = [[NSNumber alloc] initWithLongLong:serviceInfo.eventCount];
    
    [serviceIDsAndCounts addObject:entryDict];
}

- (id)propertyForKey:(NSString *)key
{
    id result = nil;
    
    if ([key isEqualToString:@(kIOHIDSessionFilterDebugKey)]) {
        NSMutableDictionary *debug = [NSMutableDictionary new];
        NSMutableArray *serviceIDsAndCounts = [NSMutableArray new];
            debug[@"Class"] = @"IOHIDTelemetrySessionFilter";
            CFDictionaryApplyFunction(self->_serviceCounts, addDebugInfo, (__bridge void *)serviceIDsAndCounts);
            debug[@"ServiceCounts"] = serviceIDsAndCounts;
            result = debug;
    }
    
    return result;
}

- (HIDEvent *)filterEvent:(HIDEvent *)event
               forService:(HIDEventService *)service
{
    //filter by non built-in services
    if(![event integerValueForField:kIOHIDEventFieldIsBuiltIn]) {
        
        IOHIDEventPolicyValue policy = IOHIDEventGetPolicy((__bridge IOHIDEventRef)event, kIOHIDEventPowerPolicy);
        
        if(policy == kIOHIDEventPowerPolicyWakeSystem) {
            dispatch_async(_queue, ^{
                HIDEventServiceTelemetry *serviceInfo = (__bridge HIDEventServiceTelemetry *)CFDictionaryGetValue(self->_serviceCounts, (__bridge IOHIDServiceRef)service);
                
                if(serviceInfo) {
                    serviceInfo.eventCount++;
                }
            });
        }
    }
    
    return event;
}

//Only include services which are considered relevant to usage
- (void)serviceNotification:(HIDEventService *)service added:(BOOL)added
{
    id builtIn = [service propertyForKey:@(kIOHIDBuiltInKey)];

    if(service && ![builtIn boolValue]) {
        if(added) {
            dispatch_async(_queue, ^{
                HIDEventServiceTelemetry *serviceInfo = [[HIDEventServiceTelemetry alloc] initWithService:service];
                CFDictionaryAddValue(self->_serviceCounts, (__bridge IOHIDServiceRef)service, (CFTypeRef)serviceInfo);
            });
        } else {
            //Send count to CA
            dispatch_async(_queue, ^{
                CFDictionaryApplyFunction(self->_serviceCounts, logUsageInfo, NULL);
                CFDictionaryRemoveValue(self->_serviceCounts, (__bridge IOHIDServiceRef)service);
            });
        }
    }
}


- (void)activate
{
    dispatch_source_set_timer(_timer, dispatch_time(DISPATCH_MONOTONICTIME_NOW, NSEC_PER_SEC * HID_TELEMETRY_LOG_INTERVAL), NSEC_PER_SEC * HID_TELEMETRY_LOG_INTERVAL, 0);
}


//Following methods are no-ops for this session filter
- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key {
    return false;
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    
}

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                      toConnection:(HIDConnection *)connection
                       fromService:(HIDEventService *)service
{
    return event;
}

@end
