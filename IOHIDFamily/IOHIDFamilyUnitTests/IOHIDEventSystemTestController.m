//
//  IOHIDEventSystemTestController.m
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#import "IOHIDEventSystemTestController.h"
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include "IOHIDUnitTestUtility.h"

static void EventSystemServiceRemovedCallback (void *target, void *refcon, IOHIDServiceClientRef service);
static void EventSystemServiceAddedCallback  (void *target, void *refcon, IOHIDServiceClientRef service);
static void EventSystemEventCallback (void * _Nullable target, void * _Nullable refcon, void * _Nullable sender, IOHIDEventRef event);
static void EventSystemResetCallback(void * _Nullable target, void * _Nullable context __unused);
static boolean_t EventSystemEventFilterCallback (void * _Nullable target, void * _Nullable refcon, void * _Nullable sender, IOHIDEventRef event);
static void EventSystemPropertyChangedCallback (void * _Nullable target, void * _Nullable context, CFStringRef property, CFTypeRef value);

@implementation IOHIDEventSystemTestController  {
    dispatch_semaphore_t  _readySema;
}

-(nullable instancetype) initWithDeviceUniqueID: (nonnull id) uniqueID :(nonnull dispatch_queue_t) queue  :(IOHIDEventSystemClientType) type {
  
  NSDictionary *matching = @{@kIOHIDPhysicalDeviceUniqueIDKey : uniqueID};
  
  return [self initWithMatching:matching :queue :type];
}


-(nullable instancetype) initWithDeviceUniqueID: (nonnull id) uniqueID AndQueue:(nonnull dispatch_queue_t) queue {

    NSDictionary *matching = @{@kIOHIDPhysicalDeviceUniqueIDKey : uniqueID};
  
  return [self initWithMatching:matching :queue :kIOHIDEventSystemClientTypeMonitor];
}

-(nullable instancetype) initWithMatching: (nonnull NSDictionary *) matching AndQueue:(nonnull dispatch_queue_t) queue {

    return [self initWithMatching:matching :queue :kIOHIDEventSystemClientTypeMonitor];
}

-(nullable instancetype) initWithMatching: (nonnull NSDictionary *) matching :(nonnull dispatch_queue_t) queue :(IOHIDEventSystemClientType) type {

    self = [super init];
    if (!self) {
        return self;
    }
  
    self->_readySema = dispatch_semaphore_create(0);

    self->_eventSystemQueue = queue;
  
    self->_eventSystemClient =  IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, type, NULL);
    if (self.eventSystemClient == NULL) {
        return nil;
    }
  
    self.events = [[NSMutableArray alloc] init];
    self.otherEvents = [[NSMutableArray alloc] init];
    self.propertyObservers = [[NSMutableDictionary alloc] init];
  
    IOHIDEventSystemClientSetMatching (self.eventSystemClient, (CFDictionaryRef)matching);
    IOHIDEventSystemClientRegisterDeviceMatchingCallback(self.eventSystemClient, EventSystemServiceAddedCallback, (__bridge void * _Nullable)(self), NULL);
    IOHIDEventSystemClientRegisterEventCallback (self.eventSystemClient, EventSystemEventCallback, (__bridge void * _Nullable)(self), NULL);
    IOHIDEventSystemClientRegisterResetCallback(self.eventSystemClient, EventSystemResetCallback, (__bridge void * _Nullable)(self), NULL);
    IOHIDEventSystemClientScheduleWithDispatchQueue(self.eventSystemClient, self.eventSystemQueue);
    NSArray* services  = CFBridgingRelease(IOHIDEventSystemClientCopyServices(self.eventSystemClient));
    for (id service in services) {
        [self EventSystemServiceAdded: (__bridge IOHIDServiceClientRef)(service)];
    }
    if (dispatch_semaphore_wait (self->_readySema, dispatch_time(DISPATCH_TIME_NOW, kServiceMatchingTimeout * NSEC_PER_SEC))) {
        return nil;
    }
  
    return self;
}

-(void)dealloc {

    [self invalidate];

    if (self.eventSystemClient) {
        CFRelease (self.eventSystemClient);
    }

    if (self.eventService) {
        CFRelease (self.eventService);
    }

}

-(void) EventSystemServiceAdded: (IOHIDServiceClientRef) service {
    if (_eventService) {
        CFRelease(_eventService);
    }
    self->_eventService = (IOHIDServiceClientRef)CFRetain(service);
    TestLog("ServiceAdded: %@\n", self.eventService);
    IOHIDServiceClientRegisterRemovalCallback (service, EventSystemServiceRemovedCallback, (__bridge void * _Nullable) self, NULL);
    dispatch_semaphore_signal(self->_readySema);

}

-(void) EventSystemServiceRemoved: (IOHIDServiceClientRef) service {
    TestLog("EventSystemServiceRemoved: %@\n", service);
    if (self.eventService == service) {
        self.eventService = nil;
    }
}

-(void) EventCallback: (IOHIDEventRef) event For: (IOHIDServiceClientRef) service {
    if (self->_eventObserver) {
    
        [self->_eventObserver EventCallback:event For:service];
    
    } else {
        NSNumber *latency = [NSNumber numberWithUnsignedLongLong: IOHIDEventGetLatency (event, 1)];
        
        
        _IOHIDEventSetAttachment(event, CFSTR("latency"), (__bridge CFTypeRef _Nonnull)(latency), 0);

        if (IOHIDEventGetAttributeDataLength(event)) {
            @synchronized (self.otherEvents) {
                [self->_otherEvents addObject: (__bridge id _Nonnull)(event)];
            }
        } else {
            @synchronized (self.events) {
                [self->_events addObject: (__bridge id _Nonnull)(event)];
            }
        }
    }
}

-(BOOL) FilterCallback: (IOHIDEventRef) event For: (IOHIDServiceClientRef) service {
    return [self->_eventObserver FilterCallback: event For:service];
}


-(void) setEventObserver : (id <IOHIDEventObserver>) observer {

    self->_eventObserver = observer;
    if ( self->_eventObserver &&  [self->_eventObserver respondsToSelector:@selector(FilterCallback:For:)]) {
        IOHIDEventSystemClientRegisterEventFilterCallback (self->_eventSystemClient, EventSystemEventFilterCallback, (__bridge void * _Nullable)(self), NULL);
    } else {
        IOHIDEventSystemClientUnregisterEventFilterCallback (self->_eventSystemClient, EventSystemEventFilterCallback, (__bridge void * _Nullable)(self), NULL);
    }
}

-(void) addPropertyObserver : (id <IOHIDPropertyObserver>) observer   For:(NSString *) key {
  
    NSMutableArray<id <IOHIDPropertyObserver>> *observers = self.propertyObservers[key];
    if (observers) {
        [observers addObject: observer];
    } else {
        observers = [[NSMutableArray alloc ] init];
        [observers addObject: observer];
        self.propertyObservers[key] = observers;
        IOHIDEventSystemClientRegisterPropertyChangedCallback (
                                                               self->_eventSystemClient,
                                                               (CFStringRef)key,
                                                               EventSystemPropertyChangedCallback,
                                                               (__bridge void * _Nullable)(self),
                                                               NULL);
    }
}

-(void) removePropertyObserver : (id <IOHIDPropertyObserver>) observer   For:(NSString *) key {
  
    NSMutableArray<id <IOHIDPropertyObserver>> *observers = self.propertyObservers[key];
    if (!observers) {
        return;
    }
    [observers removeObjectIdenticalTo:observer];
    if (observers.count == 0) {
        self.propertyObservers[key] = nil;
        IOHIDEventSystemClientUnregisterPropertyChangedCallback (
                                                                 self->_eventSystemClient,
                                                                 (CFStringRef)key,
                                                                 EventSystemPropertyChangedCallback,
                                                                 (__bridge void * _Nullable)(self),
                                                                 NULL);
    }
}

+(EVENTS_STATS) getEventsStats : (NSArray *) events {
    EVENTS_STATS stats;
    memset(&stats, 0, sizeof(stats));
    stats.minLatency = 0xffffffffffffffff;
    stats.totalCount = (uint32_t)events.count;
    for (id e in events) {
        IOHIDEventRef event = (__bridge IOHIDEventRef) e;
        NSNumber *eventLatency = CFBridgingRelease(_IOHIDEventCopyAttachment(event, CFSTR("latency"), 0));
        if (eventLatency) {
            if ((uint64_t)eventLatency.longLongValue > stats.maxLatency) {
                stats.maxLatency = (uint64_t)eventLatency.longLongValue;
            }
            if ((uint64_t)eventLatency.longLongValue < stats.minLatency) {
                stats.minLatency = (uint64_t)eventLatency.longLongValue;
            }
            stats.averageLatency += (uint64_t)eventLatency.longLongValue;
        }
      
        IOHIDEventType type = IOHIDEventGetType(event);
        stats.counts[type] += 1;
    }
    if (stats.totalCount) {
        stats.averageLatency = stats.averageLatency / stats.totalCount;
    }
    return stats;
}

-(void)invalidate {
    if (self.eventSystemQueue) {
        dispatch_sync( self.eventSystemQueue, ^{
            IOHIDEventSystemClientUnscheduleFromDispatchQueue(self.eventSystemClient, self.eventSystemQueue);
            self->_eventSystemQueue = nil;
        });
    }
}

@end


boolean_t EventSystemEventFilterCallback (void * _Nullable target, void * _Nullable refcon __unused, void * _Nullable sender, IOHIDEventRef event) {
    IOHIDEventSystemTestController *self = (__bridge IOHIDEventSystemTestController *)target;
     return [self FilterCallback : event For:(IOHIDServiceClientRef)sender];
}

void EventSystemResetCallback(void * _Nullable target, void * _Nullable context __unused) {
    IOHIDEventSystemTestController *self = (__bridge IOHIDEventSystemTestController *)target;
    ++self.eventSystemResetCount;
}

void EventSystemServiceRemovedCallback (void *target, void *refcon __unused, IOHIDServiceClientRef service) {
    IOHIDEventSystemTestController *self = (__bridge IOHIDEventSystemTestController *)target;
    [self EventSystemServiceRemoved: service];
}

void EventSystemServiceAddedCallback  (void *target, void *refcon __unused, IOHIDServiceClientRef service) {
    IOHIDEventSystemTestController *self = (__bridge IOHIDEventSystemTestController *)target;
    [self EventSystemServiceAdded : service];
}

void EventSystemEventCallback (void * _Nullable target, void * _Nullable refcon __unused, void * _Nullable sender, IOHIDEventRef event) {
    IOHIDEventSystemTestController *self = (__bridge IOHIDEventSystemTestController *)target;
    [self EventCallback : event For:(IOHIDServiceClientRef)sender];
}

void EventSystemPropertyChangedCallback (void * _Nullable target, void * _Nullable context __unused, CFStringRef property, CFTypeRef value) {
    IOHIDEventSystemTestController *self = (__bridge IOHIDEventSystemTestController *)target;
    NSMutableArray<id <IOHIDPropertyObserver>> *observers = self.propertyObservers[(__bridge NSString *)property];
    for (id <IOHIDPropertyObserver> observer in observers) {
      [observer PropertyCallback:property And:value];
    }
}
