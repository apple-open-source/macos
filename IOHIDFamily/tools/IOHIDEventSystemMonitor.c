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
//#include <IOKit/hid/AppleHIDUsageTables.h>
#include "AppleHIDUsageTables.h"
#include "IOHIDNXEventDescription.h"
#include <os/variant_private.h>

static const char kAdded[]      = "ADDED";
static const char kRemoved[]    = "REMOVED";

static const char                   __version[]         = "2.00";
static uint32_t                     __persist           = 0;
static uint32_t                     __dispatchOnly      = 0;
static uint64_t                     __dispatchEventMask = 0;
static uint64_t                     __eventMask         = -1;
static int64_t                      __nxTypeMask        = -1;
static int64_t                      __nxUsageMask       = -1;
static boolean_t                    __filter            = false;
static boolean_t                    __virtualService    = false;
static boolean_t                    __verboseMode       = true;
static IOHIDEventSystemClientType   __clientType        = kIOHIDEventSystemClientTypeMonitor;
static struct {
    uint32_t usagePage;
    uint32_t usage;
    uint32_t builtin;
} __matching[0xff]   = {};
static CFDictionaryRef              __matchingDictionary                        = NULL;
static uint32_t                     __matchingCount                             = 0;
static uint32_t                     __matchingInterval                          = -1;
static uint32_t                     __matchingBatchInterval                     = -1;
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
static CFStringRef                  __propertyKey                               = NULL;
static CFNumberRef                  __numericValue                              = NULL;
static CFBooleanRef                 __boolValue                                 = NULL;

boolean_t isNXEvent (IOHIDEventRef event);
void printNXEventInfo (IOHIDEventRef event);
void printNXEvents (IOHIDEventRef event);
void serviceRemovalCallback(void * target, void * refcon, IOHIDServiceRef service);



boolean_t isNXEvent (IOHIDEventRef event) {
  CFIndex usage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsage);
  if (IOHIDEventGetType(event) == kIOHIDEventTypeVendorDefined &&
      IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsagePage) == kHIDPage_AppleVendor &&
      (usage == kHIDUsage_AppleVendor_NXEvent ||
       usage == kHIDUsage_AppleVendor_NXEvent_Translated ||
       usage == kHIDUsage_AppleVendor_NXEvent_Diagnostic)) {
        return true;
      }
    return false;
}


void printNXEventInfo (IOHIDEventRef event) {
  CFIndex usage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsage);
  if (__nxUsageMask & (1 << usage))
  {
    
    NXEvent *nxEvent = NULL;
    CFIndex nxEventLength = 0;
    IOHIDEventGetVendorDefinedData (event, (uint8_t**)&nxEvent, &nxEventLength);
    if (nxEvent && (nxEventLength == sizeof(NXEvent) || nxEventLength == sizeof(NXEventExt)))
    {
      if (__nxTypeMask & (1 << nxEvent->type))
      {
         CFStringRef nxEventDescription;
         if (nxEventLength == sizeof(NXEventExt)) {
            nxEventDescription = NxEventExtCreateDescription ((NXEventExt *)nxEvent);
         } else {
            nxEventDescription = NxEventCreateDescription (nxEvent);
         }
        printf("[NXEvent Dispatch Type: %s]\n%s",
               (usage == kHIDUsage_AppleVendor_NXEvent) ? "Kernel" : (
               (usage == kHIDUsage_AppleVendor_NXEvent_Translated) ? "Translated" : "Diagnostic"),
               CFStringGetCStringPtr(nxEventDescription, kCFStringEncodingMacRoman)
               );
        CFRelease(nxEventDescription);
      }
    }
    else
    {
      printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
      printf("ERROR!!! NXEventData %p, nxEventLength = 0x%lx (expected length 0x%lx)\n", nxEvent, nxEventLength, sizeof(NXEvent));
      printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    }
  }
}

void printNXEvents (IOHIDEventRef event) {
  if (isNXEvent (event)) {
    printNXEventInfo (event);
  }
  CFArrayRef childrens = IOHIDEventGetChildren (event);
  for (CFIndex index = 0 , count = childrens ? CFArrayGetCount(childrens) : 0 ; index < count ; index++) {
    IOHIDEventRef child = (IOHIDEventRef)CFArrayGetValueAtIndex(childrens, index);
    if (isNXEvent (child)) {
      printNXEventInfo (child);
    }
  }
}

IOHIDEventBlock eventBlock = ^(void * target __unused, void * refcon, void * sender __unused, IOHIDEventRef event)
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

    if ((__eventMask & (1ull << 63)) && IOHIDEventConformsTo (event, kIOHIDEventTypeVendorDefined)) {
      printNXEvents (event);
    }
    if  ((((uint64_t)1<<type) & __eventMask) != 0 ) {
    
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

IOHIDEventFilterBlock eventFilterBlock = ^ boolean_t (void * target, void * refcon __unused, void * sender, IOHIDEventRef event)
{
    bool val = true;
    eventBlock(target, &val, sender, event);
    
    return false;
};

static boolean_t filterEventCallback(void * target, void * refcon, void * sender, IOHIDEventRef event)
{
    return eventFilterBlock(target, refcon, sender, event);
}

IOHIDServiceClientBlock serviceClientBlock =  ^(void * target __unused, void * refcon, IOHIDServiceClientRef service)
{
    CFStringRef string;
    CFNumberRef number;
    
    if ( refcon == kAdded ) {
        IOHIDServiceClientRegisterRemovalBlock(service, serviceClientBlock, NULL, (void*)kRemoved);

        if ( __clientType == kIOHIDEventSystemClientTypeRateControlled ) {
            if ( __matchingInterval != (uint32_t)-1 ) {
                number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &__matchingInterval);
                if ( number ) {
                    IOHIDServiceClientSetProperty(service, CFSTR(kIOHIDServiceReportIntervalKey), number);
                    CFRelease(number);
                }
            }
        }

        if ( __matchingBatchInterval != (uint32_t)-1 ) {
            number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &__matchingBatchInterval);
            if ( number ) {
                IOHIDServiceClientSetProperty(service, CFSTR(kIOHIDServiceBatchIntervalKey), number);
                CFRelease(number);
            }
        }
        
        if ( __propertyKey != NULL ) {
            if ( __boolValue )
                IOHIDServiceClientSetProperty(service, __propertyKey, __boolValue);
            if ( __numericValue )
                IOHIDServiceClientSetProperty(service, __propertyKey, __numericValue);
        }
    }
    
    if (__verboseMode) {
        printf("SERVICE %s:\n", (char *)refcon);
        
        string = CFCopyDescription(service);
        if ( string ) {
            printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
            CFRelease(string);
        }
    }
};

static CFMutableDictionaryRef __serviceNotifications = NULL;
void serviceRemovalCallback(void * target __unused, void * refcon __unused, IOHIDServiceRef service)
{
    CFStringRef string;

    CFDictionaryRemoveValue(__serviceNotifications, service);
    
    if (__verboseMode) {
        printf("SERVICE %s:\n", (char *)kRemoved);
        
        string = CFCopyDescription(service);
        if ( string ) {
            printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
            CFRelease(string);
        }
    }
}

static void servicesAddedCallback(void * target __unused, void * refcon __unused, void * sender __unused, CFArrayRef services)
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
        
        if (__verboseMode) {
            printf("SERVICE %s:\n", (char *)kAdded);

            string = CFCopyDescription(service);
            if ( string ) {
                printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
                CFRelease(string);
            }
        }
    }
}

static void connectionAddedCallback(void * target __unused, void * refcon __unused, IOHIDEventSystemConnectionRef connection)
{
    CFStringRef string;

    if (__verboseMode) {
        printf("CONNECTION %s:\n", (char *)kAdded);
        
        string = CFCopyDescription(connection);
        if ( string ) {
            printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
            CFRelease(string);
        }
    }
}

static void connectionRemovedCallback(void * target __unused, void * refcon __unused, IOHIDEventSystemConnectionRef connection)
{
    CFStringRef string;
    
    if (__verboseMode) {
        printf("CONNECTION %s:\n", (char *)kRemoved);
        
        string = CFCopyDescription(connection);
        if ( string ) {
            printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
            CFRelease(string);
        }
    }
}


static void dispatchClientEvents(IOHIDEventSystemClientRef system, uint64_t mask)
{
    do {
        
        for ( uint32_t index=0; index<kIOHIDEventTypeCount; index++ ) {
            IOHIDEventRef event;
            
            if ( (((uint64_t)1<<index) & mask) == 0 )
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

static boolean_t VirtualOpen(void * target __unused, void * refcon __unused, IOOptionBits options __unused)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    return true;
}

static void VirtualClose(void * target __unused, void * refcon __unused, IOOptionBits options __unused)
{
    printf("%s\n", __PRETTY_FUNCTION__);
}

static CFTypeRef VirtualCopyProperty(void * target __unused, void * refcon __unused, CFStringRef key)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    
    if (CFEqual(key, CFSTR(kIOHIDTransportKey))) {
        CFStringRef value = CFSTR("Virtual");
        CFRetain(value);
        return value;
    }
    
    return NULL;
}

static boolean_t VirtualSetProperty(void * target __unused, void * refcon __unused, CFStringRef key __unused, CFTypeRef value __unused)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    return false;
}

static void VirtualSetEventCallback(void * target __unused, void * refcon __unused, IOHIDServiceEventCallback callback __unused, void *callbackTarget __unused, void *callbackRefcon __unused)
{
    printf("%s\n", __PRETTY_FUNCTION__);
}

static void VirtualScheduleWithDispatchQueue(void * target __unused, void * refcon __unused, dispatch_queue_t queue __unused)
{
    printf("%s\n", __PRETTY_FUNCTION__);
}

static void VirtualUnscheduleFromDispatchQueue(void * target __unused, void * refcon __unused, dispatch_queue_t queue __unused)
{
    printf("%s\n", __PRETTY_FUNCTION__);
}

static IOHIDEventRef VirtualCopyEvent(void * target __unused, void * refcon __unused, IOHIDEventType type __unused, IOHIDEventRef matching __unused, IOOptionBits options __unused)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    return NULL;
}

static IOReturn VirtualSetOutputEvent(void * target __unused, void * refcon __unused, IOHIDEventRef event __unused)
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
        CFStringRef lkey;
        
        lkey = keys[index];
        if ( !lkey )
            continue;

        value = values[index];
        if ( !value )
            continue;

        if ( CFGetTypeID(value) == CFStringGetTypeID() )
            printStringValue(lkey, (CFStringRef)value, indentationLevel+1, true);
        else if ( CFGetTypeID(value) == CFNumberGetTypeID() )
            printNumberValue(lkey, (CFNumberRef)value, indentationLevel+1, true);
        else if ( CFGetTypeID(value) == CFBooleanGetTypeID() )
            printBooleanValue(lkey, (CFBooleanRef)value, indentationLevel+1, true);
    }
}

static void listServices(CFArrayRef services, CFIndex indentationLevel)
{
    static CFStringRef sServiceKeys[] = {CFSTR(kIOHIDServiceRegistryIDKey), CFSTR(kIOHIDServiceRegistryNameKey), CFSTR(kIOHIDBuiltInKey), CFSTR(kIOHIDDisplayIntegratedKey), CFSTR(kIOHIDServicePrimaryUsagePageKey), CFSTR(kIOHIDServicePrimaryUsageKey), CFSTR(kIOHIDServiceReportIntervalKey), CFSTR(kIOHIDServiceSampleIntervalKey), CFSTR(kIOHIDServiceNextEventTimeStampDeltaKey), CFSTR(kIOHIDCategoryKey), CFSTR(kIOHIDServiceTransportKey), CFSTR(kIOHIDServiceBatchIntervalKey)};
    CFIndex index;
    
    for ( index=0; index<CFArrayGetCount(services); index++, printf("\n") ) {
        
        CFDictionaryRef serviceRecord = (CFDictionaryRef)CFArrayGetValueAtIndex(services, index);
        
        if ( !serviceRecord )
            continue;        
        
        for ( UInt32 keyIndex=0; keyIndex<(sizeof(sServiceKeys)/sizeof(CFStringRef)); keyIndex++ ) {
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
    
    services = (CFArrayRef)IOHIDEventSystemClientCopyProperty(eventSystem, CFSTR(kIOHIDServiceRecordsKey));
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
    CFIndex     index;
    CFArrayRef  clients = NULL;

    require(eventSystem, exit);
    
    clients = (CFArrayRef)IOHIDEventSystemClientCopyProperty(eventSystem, CFSTR(kIOHIDClientRecordsKey));
    require(clients, exit);
    
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
    
exit:
    if (clients)
        CFRelease(clients);
    
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

static void serviceRecordsChangedCallback(void * target __unused, IOHIDEventSystemClientRef client, void * context __unused)
{
    listAllServicesWithSystem(client);
}

static void clientRecordsChangedCallback(void * target __unused, IOHIDEventSystemClientRef client, void * context __unused)
{
    listAllClientsWithSystem(client);
}

static void runClient()
{
    IOHIDEventSystemClientRef   eventSystem = NULL;
    CFMutableArrayRef           multiple    = NULL;
    CFMutableDictionaryRef      matching    = NULL;
    CFDictionaryRef             attribs     = NULL;
    uint32_t                    index       = 0;
    CFStringRef                 keys[]      = { CFSTR(kIOHIDEventSystemConnectionAttributeHighFrequency) };
    CFTypeRef                   values[]    = { kCFBooleanTrue };
    CFIndex                     keyCount    = sizeof(keys)/sizeof(keys[0]);

    _Static_assert(sizeof(keys) / sizeof(keys[0]) == sizeof(values) / sizeof(values[0]), "need same number of keys and values");

    attribs = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys, (const void **)values, keyCount, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(attribs, exit, printf("Unable to create attributes\n"));

    eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, __clientType, attribs);
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
        
        if ( __matchingCount || __matchingDictionary ) {
            multiple = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            require(multiple, exit);
            if (__matchingDictionary) {
              CFArrayAppendValue(multiple, __matchingDictionary);
              CFRelease(__matchingDictionary);
              __matchingDictionary = NULL;
            }
            for (index=0; index<__matchingCount; index++) {
                CFNumberRef number = NULL;
                
                matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                if ( matching ) {
                    number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &__matching[index].usagePage);
                    if ( number ) {
                        CFDictionaryAddValue(matching, CFSTR(kIOHIDServiceDeviceUsagePageKey), number);
                        CFRelease(number);
                    }
                    if ( __matching[index].usage != (uint32_t)-1 ) {
                        number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &__matching[index].usage);
                        if ( number ) {
                            CFDictionaryAddValue(matching, CFSTR(kIOHIDServiceDeviceUsageKey), number);
                            CFRelease(number);
                        }
                    }
                    if ( __matching[index].builtin != (uint32_t)-1 ) {
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
            CFIndex i, count;
            
            for(i=0, count = CFArrayGetCount(services); i<count; i++)
                serviceClientBlock(NULL, (void*)kAdded, (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, i));
            
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

static void exitTimerCallback(CFRunLoopTimerRef timer __unused, void *info __unused)
{
    if (__verboseMode) {
        printStatistics();
    }

    exit(0);
}

static void signalHandler(int type)
{
    if (type == SIGINT) {
        if (__verboseMode) {
            printStatistics();
        }
        exit(0);
    }
}

static void printHelp()
{
    printf("\n");
    printf("hidEventSystemMonitor usage:\n\n");
    printf("\t-up <usage page>\t\t: Device usage page\n");
    printf("\t-u <usage>\t\t\t: Device usage\n");
    printf("\t-b <1 | 0>\t\t\t: 1=built-in 0=not built-in\n");
    printf("\t-m <event type mask ...>\t: monitor all events contained in mask\n");
    printf("\t-e <event type number ...>\t: monitor all events of the passed type\n");
    printf("\t-nm <event type mask>\t\t: monitor all events except those contained in mask\n");
    printf("\t-ne <event type number>\t\t: monitor all events except those of the passed type\n");
    printf("\t-d <event type number>\t\t: dispatch event of the passed type\n");
    printf("\t-dm <event type number>\t\t: dispatch events of the passed mask\n");
    printf("\t-do \t\t\t\t: exit after dispatching events with -d, -dm\n");
    printf("\t-k <string>\t\t\t: Key to be used with -bp or -np options for setting properties\n");
    printf("\t-bp <0/1>\t\t\t: Boolean to use with -k option\n");
    printf("\t-np <number>\t\t\t: Numeric value, use with -k option\n");
#if !TARGET_OS_EMBEDDED
    printf("\t-nxtype <NX event type number> monitor all NX events with type\n");
    printf("\t-nxusage <NX event usage number> monitor all NX events with usage\n");
#endif
    printf("\t-p\t\t\t\t: persist event dispatch\n");
    printf("\n");
    printf("\t-a\t\t\t\t: Admin (Unfiltered event stream)\n");
    printf("\t-q\t\t\t\t: quiet (do not print services / event statistics\n");
    printf("\t-r\t\t\t\t: Rate Controlled\n");
    printf("\n");
    printf("\t-s\t\t\t\t: Instantiate HID event server\n");
    printf("\t-ms\t\t\t\t: Monitor services\n");
    printf("\t-mc\t\t\t\t: Monitor clients\n");
    printf("\n");
    printf("\t-lc\t\t\t\t: List clients\n");
    printf("\t-ls\t\t\t\t: List services\n");
    printf("\t-S <interval>\t\t\t: Set sample interval\n");
    printf("\t-V\t\t\t\t: Version\n");
    printf("\n\tAvailable Event Types:\n");
    
    for (int type = kIOHIDEventTypeNULL; type<kIOHIDEventTypeCount; type++) {
        printf("\t\t%2d: %s\n", type, CFStringGetCStringPtr(IOHIDEventTypeGetName(type), kCFStringEncodingMacRoman));
    }
#if !TARGET_OS_EMBEDDED
    printf("\t\t%2d: %s\n", 63, "Legacy NXEvents");
    printf("\n\tAvailable NXEvent Types:\n");
    printf("\t\t%2d: %s\n", NX_NULLEVENT, "NX_NULLEVENT");
    printf("\t\t%2d: %s\n", NX_LMOUSEDOWN, "NX_LMOUSEDOWN");
    printf("\t\t%2d: %s\n", NX_LMOUSEUP, "NX_LMOUSEUP");
    printf("\t\t%2d: %s\n", NX_RMOUSEDOWN, "NX_RMOUSEDOWN");
    printf("\t\t%2d: %s\n", NX_RMOUSEUP, "NX_RMOUSEUP");
    printf("\t\t%2d: %s\n", NX_MOUSEMOVED, "NX_MOUSEMOVED");
    printf("\t\t%2d: %s\n", NX_LMOUSEDRAGGED, "NX_LMOUSEDRAGGED");
    printf("\t\t%2d: %s\n", NX_RMOUSEDRAGGED, "NX_RMOUSEDRAGGED");
    printf("\t\t%2d: %s\n", NX_MOUSEENTERED, "NX_MOUSEENTERED");
    printf("\t\t%2d: %s\n", NX_MOUSEEXITED, "NX_MOUSEEXITED");
    printf("\t\t%2d: %s\n", NX_OMOUSEDOWN, "NX_OMOUSEDOWN");
    printf("\t\t%2d: %s\n", NX_OMOUSEUP, "NX_OMOUSEUP");
    printf("\t\t%2d: %s\n", NX_OMOUSEDRAGGED, "NX_OMOUSEDRAGGED");
    printf("\t\t%2d: %s\n", NX_KEYDOWN, "NX_KEYDOWN");
    printf("\t\t%2d: %s\n", NX_KEYUP, "NX_KEYUP");
    printf("\t\t%2d: %s\n", NX_FLAGSCHANGED, "NX_FLAGSCHANGED");
    printf("\t\t%2d: %s\n", NX_SYSDEFINED, "NX_SYSDEFINED");
    printf("\t\t%2d: %s\n", NX_SCROLLWHEELMOVED, "NX_SCROLLWHEELMOVED");
    printf("\t\t%2d: %s\n", NX_ZOOM, "NX_ZOOM");
    printf("\t\t%2d: %s\n", NX_TABLETPOINTER, "NX_TABLETPOINTER");
    printf("\t\t%2d: %s\n", NX_TABLETPROXIMITY, "NX_TABLETPROXIMITY");
    printf("\n\tAvailable NXEvent Usages:\n");
    printf("\t\t%2d: %s\n", kHIDUsage_AppleVendor_NXEvent, "kHIDUsage_AppleVendor_NXEvent");
    printf("\t\t%2d: %s\n", kHIDUsage_AppleVendor_NXEvent_Translated, "kHIDUsage_AppleVendor_NXEvent_Translated");
    printf("\t\t%2d: %s\n", kHIDUsage_AppleVendor_NXEvent_Diagnostic, "kHIDUsage_AppleVendor_NXEvent_Diagnostic");
#endif
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
    kEventRegistrationTypeBatch,
    kEventRegistrationTypePlist,
    kEventRegistrationTypeAddNxEventUsage,
    kEventRegistrationTypeAddNxEventType,
    kEventRegistrationTypePropertyKey,
    kEventRegistrationTypeBooleanProperty,
    kEventRegistrationTypeNumberProperty,
} EventRegistrationType;

int main (int argc __unused, const char * argv[] __unused)
{
    bool runAsClient = true;
    
    if(!os_variant_allows_internal_security_policies(NULL)) {
        return 0;
    }
    
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
            else if ( !strcmp("-q", arg ) ) {
                __verboseMode = false;
            }
            else if ( !strcmp("-r", arg ) ) {
                __clientType = kIOHIDEventSystemClientTypeRateControlled;
            }
            else if ( !strcmp("-plist", arg ) ) {
                registrationType = kEventRegistrationTypePlist;
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
            else if ( !strcmp("-B", arg) ) {
                registrationType = kEventRegistrationTypeBatch;
            }
            else if ( !strcmp("-V", arg) ) {
                printf("Version: %s\n", __version);
            }
            else if ( !strcmp("-k", arg) ) {
                registrationType = kEventRegistrationTypePropertyKey;
            }
            else if ( !strcmp("-np", arg) ) {
                registrationType = kEventRegistrationTypeNumberProperty;
            }
            else if ( !strcmp("-bp", arg) ) {
                registrationType = kEventRegistrationTypeBooleanProperty;
            }
#if !TARGET_OS_EMBEDDED
            else if ( !strcmp("-nxtype", arg) ) {
              if (__nxTypeMask == -1) {
                __nxTypeMask = 0;
              }
              registrationType = kEventRegistrationTypeAddNxEventType;
            }
            else if ( !strcmp("-nxusage", arg) ) {
              if (__nxUsageMask == -1) {
                __nxUsageMask = 0;
              }
              registrationType = kEventRegistrationTypeAddNxEventUsage;
            }
            else if ( registrationType == kEventRegistrationTypeAddNxEventType ) {
             __nxTypeMask |= ((uint64_t)1)<<(strtoull(arg, NULL, 10));
            }
            else if ( registrationType == kEventRegistrationTypeAddNxEventUsage ) {
              __nxUsageMask |= ((uint64_t)1)<<(strtoull(arg, NULL, 10));
            }
#endif
            else if ( registrationType == kEventRegistrationTypeReplaceMask ) {
                __eventMask = strtoull(arg, NULL, 16);
            }
            else if ( registrationType == kEventRegistrationTypeRemoveMask ) {
                __eventMask &= ~(strtoull(arg, NULL, 16));
            }
            else if ( registrationType == kEventRegistrationTypeAdd ) {
                __eventMask |= ((uint64_t)1)<<(strtoull(arg, NULL, 10));
            }
            else if ( registrationType == kEventRegistrationTypePlist ) {
              CFStringRef path = CFStringCreateWithCString(kCFAllocatorDefault, arg, kCFStringEncodingMacRoman);
              if (path) {
                  CFURLRef url = CFURLCreateWithFileSystemPath (kCFAllocatorDefault, path, kCFURLPOSIXPathStyle, false);
                  if (url) {
                      CFReadStreamRef plistReader = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
                      if (plistReader) {
                          if (CFReadStreamOpen(plistReader)) {
                              CFErrorRef error;
                              __matchingDictionary = CFPropertyListCreateWithStream(kCFAllocatorDefault, plistReader, 0, kCFPropertyListImmutable, NULL, &error);
                          }
                          CFRelease(plistReader);
                      }
                      CFRelease(url);
                  }
                  CFRelease(path);
              }
              if (__matchingDictionary == NULL) {
                    printf ("ERROR!!! Creating dictionary from plist file\n");
                    return 0;
              }
            }
            else if ( registrationType ==  kEventRegistrationTypeRemove ) {
                __eventMask &= ~(((uint64_t)1)<<(strtoull(arg, NULL, 10)));
            }
            else if ( registrationType ==  kEventRegistrationTypeDispatch ) {
                __dispatchEventMask |= ((uint64_t)1)<<(strtoull(arg, NULL, 10));
            }
            else if ( registrationType == kEventRegistrationTypeDispatchMask ) {
                __dispatchEventMask = strtoull(arg, NULL, 16);
            }
            else if ( registrationType == kEventRegistrationTypeUsagePage ) {
                uint32_t ix = __matchingCount++;
                __matching[ix].usagePage = (uint32_t)strtoul(arg, NULL, 10);
                __matching[ix].usage = -1;
                __matching[ix].builtin = -1;
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
            else if ( registrationType == kEventRegistrationTypeBatch ) {
                __matchingBatchInterval = (uint32_t)strtoul(arg, NULL, 10);
            }
            else if ( registrationType == kEventRegistrationTypePropertyKey ) {
                __propertyKey = CFStringCreateWithCString(kCFAllocatorDefault, arg, kCFStringEncodingUTF8);
            }
            else if ( registrationType == kEventRegistrationTypeNumberProperty ) {
                uint32_t num = (uint32_t)strtoul(arg, NULL, 10);
                __numericValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
            }
            else if ( registrationType == kEventRegistrationTypeBooleanProperty ) {
                uint32_t num = (uint32_t)strtoul(arg, NULL, 10);
                __boolValue = (0 == num ? kCFBooleanFalse : kCFBooleanTrue);
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
        if (__verboseMode) {
            printf("***************************************************************************\n");
            printf("Running as a %s client\n", IOHIDEventSystemClientGetTypeString(__clientType));
            printf("***************************************************************************\n");
        }
        runClient();
    } else {
        if (__verboseMode) {
            printf("***************************************************************************\n");
            printf("Running as server\n");
            printf("***************************************************************************\n");
        }
        runServer();
    }
        
exit:
    
    return 0;
}
