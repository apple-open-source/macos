//
//  IOHIDEventSystemMonitor.c
//  IOHIDFamily
//
//  Created by Rob Yepez on 9/16/12.
//
//

#include <AssertMacros.h>
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDEventSystemClientPrivate.h>
#include <IOKit/hid/IOHIDEventSystemKeysPrivate.h>
#include <IOKit/hid/IOHIDServiceClient.h>
#include <IOKit/hid/IOHIDEventSystem.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDServicePrivate.h>
#include <IOKit/hid/IOHIDNotification.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/AppleHIDUsageTables.h>

static const char kAdded[]      = "ADDED";
static const char kRemoved[]    = "REMOVED";

static const char                   __version[]         = "2.00";
static uint32_t                     __persist           = 0;
static uint32_t                     __dispatchOnly      = 0;
static uint64_t                     __dispatchEventMask = 0;
static uint64_t                     __eventMask         = -1;
static boolean_t                    __filter            = false;
static boolean_t                    __virtualService    = false;
static IOHIDEventSystemClientType   __clientType        = kIOHIDEventSystemClientTypeMonitor;
static struct {
    uint32_t usagePage;
    uint32_t usage;
    uint32_t builtin;
} __matching[0xff]   = {};
static uint32_t                     __matchingCount                             = 0;
static uint32_t                     __matchingInterval                          = -1;
static uint64_t                     __eventLastTimestamps[kIOHIDEventTypeCount] = {};
static CFMutableArrayRef            __eventIntervals[kIOHIDEventTypeCount]      = {};
static uint64_t                     __eventCounts[kIOHIDEventTypeCount]         = {};
static uint64_t                     __eventCount                                = 0;
static uint64_t                     __eventLatencyTotal                         = 0;
static IOHIDEventSystemClientRef    __eventSystemRef                            = NULL;
static CFAbsoluteTime               __startTime                                 = 0;
static CFTimeInterval               __timeout                                   = 0;
static mach_timebase_info_data_t    __timeBaseinfo                              = {};
static bool                         __monitorServices                           = false;
static bool                         __monitorClients                            = false;


IOHIDEventBlock eventBlock = ^(void * target, void * refcon, void * sender, IOHIDEventRef event)
{
    IOHIDEventType  type        = IOHIDEventGetType(event);
    uint64_t        timestamp   = IOHIDEventGetTimeStamp(event);
    uint64_t        interval    = 0;
    
    // RY: This should really be tracked per service, but I'm lazy
    __eventCount++;
    __eventCounts[type]++;
    __eventLatencyTotal += IOHIDEventGetLatency(event, kMicrosecondScale);
    
    if ( __eventLastTimestamps[type] ) {
        
        CFNumberRef number = NULL;
        
        interval = timestamp - __eventLastTimestamps[type];
        
        interval *= __timeBaseinfo.numer;
        interval /= __timeBaseinfo.denom;
        interval /= kMicrosecondScale;
        
        if ( !__eventIntervals[type] )
            __eventIntervals[type] = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        
        number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &interval);
        if ( number ) {
            CFArrayAppendValue(__eventIntervals[type], number);
            CFRelease(number);
        }
    }
    
    __eventLastTimestamps[type] = timestamp;
    
    if ( ((1<<type) & __eventMask) != 0 ) {
    
        CFStringRef outputString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), event);
        
        if ( outputString ) {
            if (refcon)
                printf("<filter>\n");
            printf("*** %lld us since last %s event ***\n", interval, IOHIDEventGetTypeString(IOHIDEventGetType(event)));
            printf("%s", CFStringGetCStringPtr(outputString, kCFStringEncodingMacRoman));
            if (refcon)
                printf("</filter>\n");

            printf("\n");
            CFRelease(outputString);
        }
    }
};

IOHIDEventFilterBlock eventFilterBlock = ^ boolean_t (void * target, void * refcon, void * sender, IOHIDEventRef event)
{
    eventBlock(target, true, sender, event);
    
    return false;
};

static boolean_t filterEventCallback(void * target, void * refcon, void * sender, IOHIDEventRef event)
{
    return eventFilterBlock(target, refcon, sender, event);
}

IOHIDServiceClientBlock serviceClientBlock =  ^(void * target, void * refcon, IOHIDServiceClientRef service)
{
    CFStringRef string;
    
    if ( refcon == kAdded ) {
        IOHIDServiceClientRegisterRemovalBlock(service, serviceClientBlock, NULL, (void*)kRemoved);

        if ( __clientType == kIOHIDEventSystemClientTypeRateControlled ) {
            CFNumberRef number;
            
            if ( __matchingInterval != -1 ) {
                number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &__matchingInterval);
                if ( number ) {
                    IOHIDServiceClientSetProperty(service, CFSTR(kIOHIDServiceReportIntervalKey), number);
                    CFRelease(number);
                }
            }
        }
    }
    printf("SERVICE %s:\n", (char *)refcon);
    
    string = CFCopyDescription(service);
    if ( string ) {
        printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
        CFRelease(string);
    }
};

static CFMutableDictionaryRef __serviceNotifications = NULL;
void serviceRemovalCallback(void * target, void * refcon, IOHIDServiceRef service)
{
    CFStringRef string;

    CFDictionaryRemoveValue(__serviceNotifications, service);
    
    printf("SERVICE %s:\n", (char *)kRemoved);
    
    string = CFCopyDescription(service);
    if ( string ) {
        printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
        CFRelease(string);
    }
}

static void servicesAddedCallback(void * target, void * refcon, void * sender, CFArrayRef services)
{
    CFIndex index, count;
    
    for(index=0, count = CFArrayGetCount(services); index<count; index++) {
        IOHIDServiceRef service = (IOHIDServiceRef)CFArrayGetValueAtIndex(services, index);
        CFStringRef string;
        
        IOHIDNotificationRef notification = IOHIDServiceCreateRemovalNotification(service, serviceRemovalCallback, NULL, NULL);
        if ( notification ) {
            CFDictionaryAddValue(__serviceNotifications, service, notification);
            CFRelease(notification);
        }
        
        printf("SERVICE %s:\n", (char *)kAdded);
        
        string = CFCopyDescription(service);
        if ( string ) {
            printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
            CFRelease(string);
        }
    }
}

static void connectionAddedCallback(void * target, void * refcon, IOHIDEventSystemConnectionRef connection)
{
    CFStringRef string;

    printf("CONNECTION %s:\n", (char *)kAdded);
    
    string = CFCopyDescription(connection);
    if ( string ) {
        printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
        CFRelease(string);
    }
}

static void connectionRemovedCallback(void * target, void * refcon, IOHIDEventSystemConnectionRef connection)
{
    CFStringRef string;
    
    printf("CONNECTION %s:\n", (char *)kRemoved);
    
    string = CFCopyDescription(connection);
    if ( string ) {
        printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
        CFRelease(string);
    }
}


static void dispatchClientEvents(IOHIDEventSystemClientRef system, uint64_t mask)
{
    do {
        
        for ( uint32_t index=0; index<kIOHIDEventTypeCount; index++ ) {
            IOHIDEventRef event;
            
            if ( ((1<<index) & mask) == 0 )
                continue;
                    
            event = IOHIDEventCreate(kCFAllocatorDefault, index, mach_absolute_time(), 0);
            if ( !event )
                continue;
            
            IOHIDEventSetSenderID(event, 0xDEFACEDBEEFFECE5);
            
            printf("Dispatch %s event\n", IOHIDEventGetTypeString(index));
            
            IOHIDEventSystemClientDispatchEvent(system, event);
            
            CFRelease(event);
        }
        
        if ( !__persist )
            continue;
        
        printf("hit return to redispatch\n");
        
        while (getchar() != '\n');
        
    } while (__persist);
}

static void * dispatchClientThread(void * context)
{
    IOHIDEventSystemClientRef eventSystem = (IOHIDEventSystemClientRef)context;
    
    dispatchClientEvents(eventSystem, __dispatchEventMask);
    
    return NULL;
}

static boolean_t VirtualOpen(void * target, void * refcon, IOOptionBits options)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    return true;
}

static void VirtualClose(void * target, void * refcon, IOOptionBits options)
{
    printf("%s\n", __PRETTY_FUNCTION__);
}

static CFTypeRef VirtualCopyProperty(void * target, void * refcon, CFStringRef key)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    
    if (CFEqual(key, CFSTR(kIOHIDTransportKey))) {
        CFStringRef value = CFSTR("Virtual");
        CFRetain(value);
        return value;
    }
    
    return NULL;
}

static boolean_t VirtualSetProperty(void * target, void * refcon, CFStringRef key, CFTypeRef value)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    return false;
}

static void VirtualSetEventCallback(void * target, void * refcon, IOHIDServiceEventCallback callback, void *callbackTarget, void *callbackRefcon)
{
    printf("%s\n", __PRETTY_FUNCTION__);
}

static void VirtualScheduleWithDispatchQueue(void * target, void * refcon, dispatch_queue_t queue)
{
    printf("%s\n", __PRETTY_FUNCTION__);
}

static void VirtualUnscheduleFromDispatchQueue(void * target, void * refcon, dispatch_queue_t queue)
{
    printf("%s\n", __PRETTY_FUNCTION__);
}

static IOHIDEventRef VirtualCopyEvent(void * target, void * refcon, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    return NULL;
}

static IOReturn VirtualSetOutputEvent(void * target, void * refcon, IOHIDEventRef event)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    return kIOReturnUnsupported;
}


static void runServer()
{
    boolean_t                       result;
    IOHIDEventSystemRef             eventSystem         = NULL;
    IOHIDServiceRef                 virtualService      = NULL;
    IOHIDServiceVirtualCallbacks    virtualCallbacks    = {
                                                            VirtualOpen,
                                                            VirtualClose,
                                                            VirtualCopyProperty,
                                                            VirtualSetProperty,
                                                            VirtualSetEventCallback,
                                                            VirtualScheduleWithDispatchQueue,
                                                            VirtualUnscheduleFromDispatchQueue,
                                                            VirtualCopyEvent,
                                                            VirtualSetOutputEvent
                                                        };
    
    eventSystem = IOHIDEventSystemCreate(kCFAllocatorDefault);
    require(eventSystem, exit);
        
    result = IOHIDEventSystemOpen(eventSystem, filterEventCallback, NULL, NULL, 0);
    require(result, exit);

    if ( __virtualService ) {
        virtualService = _IOHIDServiceCreateVirtual(kCFAllocatorDefault, 0xb0b0000000000000, &virtualCallbacks, NULL, NULL); // Last 2 args target, refcon
        printf("Virtual Service = %p\n", virtualService);
        _IOHIDEventSystemAddService(eventSystem, virtualService);
    }
    
    if ( !__serviceNotifications )
        __serviceNotifications = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    for ( int index=kIOHIDEventSystemConnectionTypeAdmin; index<kIOHIDEventSystemConnectionTypeCount; index++) {
        IOHIDEventSystemRegisterConnectionAdditionCallback(eventSystem, (IOHIDEventSystemConnectionType)index, connectionAddedCallback, NULL, NULL);
        IOHIDEventSystemRegisterConnectionRemovalCallback(eventSystem, (IOHIDEventSystemConnectionType)index, connectionRemovedCallback, NULL, NULL);
    }

    IOHIDEventSystemRegisterServicesCallback(eventSystem, NULL, servicesAddedCallback, NULL, NULL);
    CFArrayRef services = IOHIDEventSystemCopyServices(eventSystem, NULL);
    if ( services ) {
        servicesAddedCallback(NULL, NULL, NULL, services);
        CFRelease(services);
        
    }

    CFRunLoopRun();
    
exit:
    if ( virtualService )
        CFRelease(virtualService);
    
    if ( eventSystem )
        CFRelease(eventSystem);

}

static void printIndentation(CFIndex indentationLevel)
{
    for ( CFIndex indentationIndex=0; indentationIndex<indentationLevel; indentationIndex++)
        printf("\t");
    
}

static void printBorder(CFIndex indentationLevel)
{
    printIndentation(indentationLevel);

    if ( indentationLevel )
        printf("-----------------------------------------------------------------------\n");
    else 
        printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
}

static void printStringValue(CFStringRef key, CFStringRef string, CFIndex indentationLevel, bool newline)
{
    if ( !key || !string )
        return;

    printIndentation(indentationLevel);
    printf("%25.25s: %-32.32s ", CFStringGetCStringPtr(key, CFStringGetSystemEncoding()), CFStringGetCStringPtr(string, CFStringGetSystemEncoding()));
    if ( newline ) printf("\n");
}

static void printBooleanValue(CFStringRef key, CFBooleanRef value, CFIndex indentationLevel, bool newline)
{
    if ( !key || !value )
        return;
    
    printIndentation(indentationLevel);
        
    printf("%25.25s: %-3.3s ", CFStringGetCStringPtr(key, CFStringGetSystemEncoding()), value == kCFBooleanTrue ? "YES" : "NO");
    if ( newline ) printf("\n");
}

static void printNumberValue(CFStringRef key, CFNumberRef number, CFIndex indentationLevel, bool newline)
{
    uint64_t value;
    
    if ( !key || !number )
        return;

    printIndentation(indentationLevel);

    CFNumberGetValue(number, kCFNumberSInt64Type, &value);
    
    if ( CFEqual(key, CFSTR(kIOHIDServiceRegistryIDKey)) )
        printf("%25.25s: 0x%016llx ", CFStringGetCStringPtr(key, CFStringGetSystemEncoding()), value);
    else if ( CFEqual(key, CFSTR(kIOHIDServiceNextEventTimeStampDeltaKey)) )
        printf("%25.25s: %-5lld us ", CFStringGetCStringPtr(key, CFStringGetSystemEncoding()), value);
    else
        printf("%25.25s: %-5lld ", CFStringGetCStringPtr(key, CFStringGetSystemEncoding()), value);
    
    if ( newline ) printf("\n");
}

static void printDictionaryValue(CFStringRef key, CFDictionaryRef dictionary, CFIndex indentationLevel)
{
    CFIndex index, count;
    
    if ( !dictionary )
        return;
    
    count = CFDictionaryGetCount(dictionary);
    if ( !count )
        return;
    
    printf("%20.20s:\n", CFStringGetCStringPtr(key, CFStringGetSystemEncoding()));

    CFStringRef keys[count];
    CFStringRef values[count];
    
    bzero(keys, sizeof(keys));
    
    CFDictionaryGetKeysAndValues(dictionary, (const void **)keys, (const void **)values);
    
    for (index=0; index<count; index++) {
        CFTypeRef   value;
        CFStringRef key;
        
        key = keys[index];
        if ( !keys )
            continue;

        value = values[index];
        if ( !value )
            continue;

        if ( CFGetTypeID(value) == CFStringGetTypeID() )
            printStringValue(key, (CFStringRef)value, indentationLevel+1, true);
        else if ( CFGetTypeID(value) == CFNumberGetTypeID() )
            printNumberValue(key, (CFNumberRef)value, indentationLevel+1, true);
        else if ( CFGetTypeID(value) == CFBooleanGetTypeID() )
            printBooleanValue(key, (CFBooleanRef)value, indentationLevel+1, true);
    }
}

static void listServices(CFArrayRef services, CFIndex indentationLevel)
{
    static CFStringRef sServiceKeys[] = {CFSTR(kIOHIDServiceRegistryIDKey), CFSTR(kIOHIDServiceRegistryNameKey), CFSTR(kIOHIDBuiltInKey), CFSTR(kIOHIDDisplayIntegratedKey), CFSTR(kIOHIDServicePrimaryUsagePageKey), CFSTR(kIOHIDServicePrimaryUsageKey), CFSTR(kIOHIDServiceReportIntervalKey), CFSTR(kIOHIDServiceSampleIntervalKey), CFSTR(kIOHIDServiceNextEventTimeStampDeltaKey), CFSTR(kIOHIDCategoryKey), CFSTR(kIOHIDServiceTransportKey)};
    CFIndex index;
    
    for ( index=0; index<CFArrayGetCount(services); index++, printf("\n") ) {
        
        CFDictionaryRef serviceRecord = (CFDictionaryRef)CFArrayGetValueAtIndex(services, index);
        
        if ( !serviceRecord )
            continue;        
        
        for ( CFIndex keyIndex=0; keyIndex<(sizeof(sServiceKeys)/sizeof(CFStringRef)); keyIndex++ ) {
            CFStringRef key;
            CFTypeRef   value;
            
            key = sServiceKeys[keyIndex];
            if ( !key )
                continue;
            
            value = CFDictionaryGetValue(serviceRecord, key);
            if ( !value )
                continue;
            
            if ( CFGetTypeID(value) == CFStringGetTypeID() )
                printStringValue(key, (CFStringRef)value, indentationLevel, false);
            else if ( CFGetTypeID(value) == CFNumberGetTypeID() )
                printNumberValue(key, (CFNumberRef)value, indentationLevel, false);
            else if ( CFGetTypeID(value) == CFBooleanGetTypeID() )
                printBooleanValue(key, (CFBooleanRef)value, indentationLevel, false);
        }
    }
}

static void listAllServicesWithSystem(IOHIDEventSystemClientRef eventSystem)
{
    CFArrayRef  services = NULL;
    
    require(eventSystem, exit);
    
    services = _IOHIDEventSystemClientCopyServiceRecords(eventSystem);
    require(services, exit);
    
    listServices(services, 0);
    
exit:
    if ( services )
        CFRelease(services);
    
}

static void listAllServices()
{
    IOHIDEventSystemClientRef eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeAdmin, NULL);
    
    require(eventSystem, exit);
    
    listAllServicesWithSystem(eventSystem);

exit:
    if (eventSystem)
        CFRelease(eventSystem);
}

static void listAllClientsWithSystem(IOHIDEventSystemClientRef eventSystem)
{
    CFIndex     types;

    require(eventSystem, exit);
    
    for ( types=kIOHIDEventSystemClientTypeAdmin; types<=kIOHIDEventSystemClientTypeRateControlled; types++ ) {
        CFArrayRef  clients = _IOHIDEventSystemClientCopyClientRecords(eventSystem, (IOHIDEventSystemClientType)types);
        CFIndex     index;
        
        if ( !clients )
            continue;
        
        for ( index=0, printBorder(0); index<CFArrayGetCount(clients); index++, printBorder(0) ) {
            CFDictionaryRef clientRecord    = (CFDictionaryRef)CFArrayGetValueAtIndex(clients, index);
            CFArrayRef      services        = NULL;
            CFNumberRef     number;
            
            
            number = CFDictionaryGetValue(clientRecord, CFSTR(kIOHIDEventSystemClientTypeKey));
            if ( number ) {
                uint32_t value;
                
                CFNumberGetValue(number, kCFNumberSInt32Type, &value);
                
                printf("%20.20s: %d (%s)\n", kIOHIDEventSystemClientTypeKey, value, IOHIDEventSystemClientGetTypeString((IOHIDEventSystemClientType)value));
            }
            printNumberValue(CFSTR(kIOHIDEventSystemClientPIDKey), CFDictionaryGetValue(clientRecord, CFSTR(kIOHIDEventSystemClientPIDKey)), 0, true);
            printBooleanValue(CFSTR(kIOHIDEventSystemClientIsInactiveKey), CFDictionaryGetValue(clientRecord, CFSTR(kIOHIDEventSystemClientIsInactiveKey)), 0, true);
            printBooleanValue(CFSTR(kIOHIDEventSystemClientProtectedServicesDisabledKey), CFDictionaryGetValue(clientRecord, CFSTR(kIOHIDEventSystemClientProtectedServicesDisabledKey)), 0, true);
            printStringValue(CFSTR(kIOHIDEventSystemClientCallerKey), CFDictionaryGetValue(clientRecord, CFSTR(kIOHIDEventSystemClientCallerKey)), 0, true);
            printStringValue(CFSTR(kIOHIDEventSystemClientExecutablePathKey), CFDictionaryGetValue(clientRecord, CFSTR(kIOHIDEventSystemClientExecutablePathKey)), 0, true);
            printDictionaryValue(CFSTR(kIOHIDEventSystemClientAttributesKey), CFDictionaryGetValue(clientRecord, CFSTR(kIOHIDEventSystemClientAttributesKey)), 0);
            
            services = CFDictionaryGetValue(clientRecord, CFSTR(kIOHIDEventSystemClientServicesKey));
            if ( services ) {
                printf("%20.20s:\n", kIOHIDEventSystemClientServicesKey);
                listServices(services, 1);
            }
            
        }
        
        CFRelease(clients);
    }
exit:
    return;
}


static void listAllClients()
{
    IOHIDEventSystemClientRef eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeAdmin, NULL);
    
    require(eventSystem, exit);
    
    listAllClientsWithSystem(eventSystem);
    
exit:
    if (eventSystem)
        CFRelease(eventSystem);
}

static void serviceRecordsChangedCallback(void * target, IOHIDEventSystemClientRef client, void * context)
{
    listAllServicesWithSystem(client);
}

static void clientRecordsChangedCallback(void * target, IOHIDEventSystemClientRef client, void * context)
{
    listAllClientsWithSystem(client);
}

static void runClient()
{
    IOHIDEventSystemClientRef   eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, __clientType, NULL);
    CFMutableArrayRef           multiple    = NULL;
    CFMutableDictionaryRef      matching    = NULL;
    uint32_t                    index       = 0;
    
    require_action(eventSystem, exit, printf("Unable to create client"));
    
    IOHIDEventSystemClientScheduleWithRunLoop(eventSystem, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    
    if ( __monitorClients || __monitorServices ) {
        
        if ( __monitorServices ) {
            printf("Monitoring service records\n");
            _IOHIDEventSystemClientRegisterServiceRecordsChangedCallback(eventSystem, serviceRecordsChangedCallback, NULL, NULL);
        }
        
        if ( __monitorClients ) {
            printf("Monitoring client records\n");
            _IOHIDEventSystemClientRegisterClientRecordsChangedCallback(eventSystem, clientRecordsChangedCallback, NULL, NULL);
        }
        
    } else {
        require_action(!__dispatchOnly, exit, dispatchClientEvents(eventSystem, __dispatchEventMask));
        
        if ( __dispatchEventMask ) {
            pthread_attr_t  attr;
            pthread_t       tid;
            struct sched_param param;
            
            assert(!pthread_attr_init(&attr));
            assert(!pthread_attr_setschedpolicy(&attr, SCHED_RR));
            assert(!pthread_attr_getschedparam(&attr, &param));
            param.sched_priority = 63;
            assert(!pthread_attr_setschedparam(&attr, &param));
            assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));
            assert(!pthread_create(&tid, &attr, &dispatchClientThread, eventSystem));
            assert(!pthread_attr_destroy(&attr));
        }
        
        if ( __filter )
            IOHIDEventSystemClientRegisterEventFilterBlock(eventSystem, eventFilterBlock, NULL, NULL);
        else
            IOHIDEventSystemClientRegisterEventBlock(eventSystem, eventBlock, NULL, NULL);
        
        if ( __matchingCount ) {
            multiple = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            require(multiple, exit);
            
            for (index=0; index<__matchingCount; index++) {
                CFNumberRef number = NULL;
                
                matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                if ( matching ) {
                    number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &__matching[index].usagePage);
                    if ( number ) {
                        CFDictionaryAddValue(matching, CFSTR(kIOHIDServiceDeviceUsagePageKey), number);
                        CFRelease(number);
                    }
                    if ( __matching[index].usage != -1 ) {
                        number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &__matching[index].usage);
                        if ( number ) {
                            CFDictionaryAddValue(matching, CFSTR(kIOHIDServiceDeviceUsageKey), number);
                            CFRelease(number);
                        }
                    }
                    if ( __matching[index].builtin != -1 ) {
                        CFDictionaryAddValue(matching, CFSTR(kIOHIDBuiltInKey), kCFBooleanTrue);
                    }
                    
                    printf("Matching on UsagePage=%#x Usage=%#x Built-in=%#x\n", __matching[index].usagePage, __matching[index].usage, __matching[index].builtin);
                    
                    CFArrayAppendValue(multiple, matching);
                    CFRelease(matching);
                }
            }
        }
        
        IOHIDEventSystemClientSetMatchingMultiple(eventSystem, multiple);
        
        IOHIDEventSystemClientRegisterDeviceMatchingBlock(eventSystem, serviceClientBlock, NULL, (void*)kAdded);
        
        CFArrayRef services = IOHIDEventSystemClientCopyServices(eventSystem);
        if ( services ) {
            CFIndex index, count;
            
            for(index=0, count = CFArrayGetCount(services); index<count; index++)
                serviceClientBlock(NULL, (void*)kAdded, (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, index));
            
            CFRelease(services);
            
        }
    }
    
    __eventSystemRef = eventSystem;
    
    
    CFRunLoopRun();
exit:
    if ( multiple )
        CFRelease(multiple);
    if ( eventSystem )
        CFRelease(eventSystem);
}

static void printStatistics()
{
    if ( !__timeout ) {
        __timeout =  CFAbsoluteTimeGetCurrent() - __startTime;
    }
    
    printf("\n");
    printf("***************************************************************************\n");
    printf("Event Statistics over %10.3f s\n", __timeout);
    printf("***************************************************************************\n");
    for (uint32_t index=0; index<kIOHIDEventTypeCount; index++) {
        uint64_t stdDevInterval = 0;
        uint64_t avgInterval    = 0;
        uint64_t highInterval   = 0;
        uint64_t lowInterval    = 0;
        
        if ( __eventIntervals[index] ) {
            CFIndex     intervalCount, intervalIndex;
            uint64_t    intervalTotal=0;
            
            // calculate avg
            for (intervalIndex=0,intervalCount=CFArrayGetCount(__eventIntervals[index]); intervalIndex<intervalCount; intervalIndex++) {
                CFNumberRef number = (CFNumberRef)CFArrayGetValueAtIndex(__eventIntervals[index], intervalIndex);
                uint64_t value = 0;
                
                if ( !number )
                    continue;
                
                CFNumberGetValue(number, kCFNumberSInt64Type, &value);
                intervalTotal += value;
                
                if ( value > highInterval )
                    highInterval = value;
                
                if ( !lowInterval || value < lowInterval )
                    lowInterval = value;
                
                
            }
            
            avgInterval = intervalTotal/intervalCount;
            
            // calculate std dev
            for (intervalIndex=0,intervalCount=CFArrayGetCount(__eventIntervals[index]); intervalIndex<intervalCount; intervalIndex++) {
                CFNumberRef number = (CFNumberRef)CFArrayGetValueAtIndex(__eventIntervals[index], intervalIndex);
                int64_t value = 0;
                
                if ( !number )
                    continue;
                
                CFNumberGetValue(number, kCFNumberSInt64Type, &value);
                
                value = (value-avgInterval);
                value *= value;
                intervalTotal += value;
            }
            
            stdDevInterval = sqrt(intervalTotal/intervalCount);
        }
        
        printf("%-20.20s: EventCount = %10llu    AverageInterval = %10llu us    StandardDeviation = %10llu us   HighInterval = %10llu us    LowInterval = %10llu us\n", IOHIDEventGetTypeString(index), __eventCounts[index], avgInterval, stdDevInterval, highInterval, lowInterval);
    }
    printf("\n");
    printf("Average latency: %10llu us\n", __eventLatencyTotal ? __eventLatencyTotal/__eventCount : 0);
}

static void exitTimerCallback(CFRunLoopTimerRef timer, void *info)
{
    printStatistics();
    exit(0);
}

static void signalHandler(int type)
{
    if (type == SIGINT) {
        printStatistics();
        exit(0);
    }
}

static void printHelp()
{
    printf("\n");
    printf("hidEventSystemMonitor usage:\n\n");
    printf("\t-up <usage page>\t: Device usage page\n");
    printf("\t-u <usage>\t: Device usage\n");
    printf("\t-b <1 | 0>\t: 1=built-in 0=not built-in\n");
    printf("\t-m <event type mask ...>\t: monitor all events contained in mask\n");
    printf("\t-e <event type number ...>\t: monitor all events of the passed type\n");
    printf("\t-nm <event type mask>\t\t: monitor all events except those contained in mask\n");
    printf("\t-ne <event type number>\t\t: monitor all events except those of the passed type\n");
    printf("\t-d <event type number>\t\t: dispatch event of the passed type\n");
    printf("\t-dm <event type number>\t\t: dispatch events of the passed mask\n");
    printf("\t-p\t\t\t\t: persist event dispatch\n");
    printf("\n");
    printf("\t-a\t\t\t\t: Admin (Unfiltered event stream)\n");
    printf("\t-r\t\t\t\t: Rate Controlled\n");
    printf("\n");
    printf("\t-s\t\t\t\t: Instantiate HID event server\n");
    printf("\t-ms\t\t\t\t: Monitor services\n");
    printf("\t-mc\t\t\t\t: Monitor clients\n");
    printf("\n");
    printf("\t-lc\t\t\t\t: List clients\n");
    printf("\t-ls\t\t\t\t: List services\n");
    printf("\t-V\t\t\t\t: Version\n");
    printf("\n\tAvailable Event Types:\n");
    
    for (int type = kIOHIDEventTypeNULL; type<kIOHIDEventTypeCount; type++) {
        printf("\t\t%2d: %s\n", type, CFStringGetCStringPtr(IOHIDEventTypeGetName(type), kCFStringEncodingMacRoman));
    }
}

typedef enum {
    kEventRegistrationTypeNone,
    kEventRegistrationTypeReplaceMask,
    kEventRegistrationTypeRemoveMask,
    kEventRegistrationTypeAdd,
    kEventRegistrationTypeRemove,
    kEventRegistrationTypeDispatch,
    kEventRegistrationTypeDispatchMask,
    kEventRegistrationTypeUsagePage,
    kEventRegistrationTypeUsage,
    kEventRegistrationTypeBuiltIn,
    kEventRegistrationTypeInterval,
    kEventRegistrationTypeTimeout,
} EventRegistrationType;

int main (int argc __unused, const char * argv[] __unused)
{
    bool runAsClient = true;
    
    mach_timebase_info(&__timeBaseinfo);
    
    signal(SIGINT, signalHandler);
        
    if ( argc > 1 ) {
        EventRegistrationType registrationType=kEventRegistrationTypeNone;
        
        for ( int index=1; index<argc; index++) {
            const char * arg = argv[index];
            if ( !strcmp("-a", arg ) ) {
                __clientType = kIOHIDEventSystemClientTypeAdmin;
            }
            else if ( !strcmp("-f", arg ) ) {
                __filter        = true;
            }
            else if ( !strcmp("-v", arg ) ) {
                __virtualService = true;
            }
            else if ( !strcmp("-r", arg ) ) {
                __clientType = kIOHIDEventSystemClientTypeRateControlled;
            }
            else if ( !strcmp("-up", arg ) ) {
                registrationType = kEventRegistrationTypeUsagePage;
            }
            else if ( !strcmp("-u", arg ) ) {
                registrationType = kEventRegistrationTypeUsage;
            }
            else if ( !strcmp("-b", arg) ) {
                registrationType = kEventRegistrationTypeBuiltIn;
            }
            else if ( !strcmp("-i", arg ) ) {
                registrationType = kEventRegistrationTypeInterval;
            }
            else if ( !strcmp("-s", arg ) ) {
                runAsClient = false;
            }
            else if ( !strcmp("-m", arg ) ) {
                registrationType = kEventRegistrationTypeReplaceMask;
            }
            else if ( !strcmp("-e", arg ) ) {
                registrationType = kEventRegistrationTypeAdd;
                __eventMask = 0;
            }
            else if ( !strcmp("-nm", arg ) ) {
                registrationType = kEventRegistrationTypeRemoveMask;
            }
            else if ( !strcmp("-ne", arg ) ) {
                registrationType = kEventRegistrationTypeRemove;
            }
            else if ( !strcmp("-do", arg ) ) {
                __dispatchOnly = 1;
            }
            else if ( !strcmp("-lc", arg ) ) {
                listAllClients();
                goto exit;
            }
            else if ( !strcmp("-ls", arg ) ) {
                listAllServices();
                goto exit;
            }
            else if ( !strcmp("-ms", arg) ) {
                __monitorServices   = true;
                __clientType        = kIOHIDEventSystemClientTypeAdmin;
            }
            else if ( !strcmp("-mc", arg) ) {
                __monitorClients    = true;
                __clientType        = kIOHIDEventSystemClientTypeAdmin;
            }
            else if ( !strcmp("-d", arg) ) {
                registrationType = kEventRegistrationTypeDispatch;
            }
            else if ( !strcmp("-dm", arg) ) {
                registrationType = kEventRegistrationTypeDispatchMask;
            }
            else if ( !strcmp("-p", arg) ) {
                __persist = 1;
            }
            else if ( !strcmp("-t", arg) ) {
                registrationType = kEventRegistrationTypeTimeout;
            }
            else if ( !strcmp("-V", arg) ) {
                printf("Version: %s\n", __version);
            }
            else if ( registrationType == kEventRegistrationTypeReplaceMask ) {
                __eventMask = strtoull(arg, NULL, 16);
            }
            else if ( registrationType == kEventRegistrationTypeRemoveMask ) {
                __eventMask &= ~(strtoull(arg, NULL, 16));
            }
            else if ( registrationType == kEventRegistrationTypeAdd ) {
                __eventMask |= (1<<strtoull(arg, NULL, 10));
            }
            else if ( registrationType ==  kEventRegistrationTypeRemove ) {
                __eventMask &= ~(1<<strtoull(arg, NULL, 10));
            }
            else if ( registrationType ==  kEventRegistrationTypeDispatch ) {
                __dispatchEventMask |= (1<<strtoull(arg, NULL, 10));
            }
            else if ( registrationType == kEventRegistrationTypeDispatchMask ) {
                __dispatchEventMask = strtoull(arg, NULL, 16);
            }
            else if ( registrationType == kEventRegistrationTypeUsagePage ) {
                uint32_t index = __matchingCount++;
                __matching[index].usagePage = (uint32_t)strtoul(arg, NULL, 10);
                __matching[index].usage = -1;
                __matching[index].builtin = -1;
            }
            else if ( registrationType == kEventRegistrationTypeUsage && __matchingCount ) {
                __matching[__matchingCount-1].usage = (uint32_t)strtoul(arg, NULL, 10);
            }
            else if ( registrationType == kEventRegistrationTypeBuiltIn && __matchingCount ) {
                __matching[__matchingCount-1].builtin = (uint32_t)strtoul(arg, NULL, 10);
            }
            else if ( registrationType == kEventRegistrationTypeInterval ) {
                __matchingInterval = (uint32_t)strtoul(arg, NULL, 10);
            }
            else if ( registrationType == kEventRegistrationTypeTimeout ) {
                __timeout = (uint32_t)strtoul(arg, NULL, 10);
            }
            else if ( !strcmp("-h", arg ) ) {
                printHelp();
                return 0;
            }
        }
    }
        
    if ( __timeout ) {
        CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + __timeout, 0, 0, 0, exitTimerCallback, NULL);
        if ( timer ) {
            CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
            CFRelease(timer);
        }
    } else {
        __startTime = CFAbsoluteTimeGetCurrent();
    }
    
    if ( runAsClient ) {
        printf("***************************************************************************\n");
        printf("Running as a %s client\n", IOHIDEventSystemClientGetTypeString(__clientType));
        printf("***************************************************************************\n");
        runClient();
    } else {
        printf("***************************************************************************\n");
        printf("Running as server\n");
        printf("***************************************************************************\n");
        runServer();
    }
        
exit:
    
    return 0;
}
