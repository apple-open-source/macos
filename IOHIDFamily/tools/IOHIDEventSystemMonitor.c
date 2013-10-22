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
#include <IOKit/hid/IOHIDServiceClient.h>
#include <IOKit/hid/IOHIDEventSystem.h>
#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDNotification.h>
#include <IOKit/hid/IOHIDKeys.h>

static const char kServiceAdded[] = "ADDED";
static const char kServiceRemoved[] = "REMOVED";

static uint32_t                     __persist           = 0;
static uint32_t                     __dispatchOnly      = 0;
static uint32_t                     __dispatchEventMask = 0;
static uint32_t                     __eventMask         = -1;
static IOHIDEventSystemClientType   __clientType        = kIOHIDEventSystemClientTypeMonitor;
static uint32_t                     __matchingUsagePage = 0;
static uint32_t                     __matchingUsage     = 0;
static uint32_t                     __matchingInterval  = 0;
static uint32_t                     __timeout           = 0;
static uint64_t                     __eventCounts[kIOHIDEventTypeCount] = {};
static uint64_t                     __eventCount        = 0;
static uint64_t                     __eventLatencyTotal = 0;

static boolean_t eventCallback(void * target, void * refcon, void * sender, IOHIDEventRef event)
{
    __eventCount++;
    __eventCounts[IOHIDEventGetType(event)]++;
    __eventLatencyTotal += IOHIDEventGetLatency(event, kMicrosecondScale);
    
    if ( ((1<<IOHIDEventGetType(event)) & __eventMask) != 0 ) {
    
        CFStringRef outputString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@\n"), event);
        
        if ( outputString ) {
            printf("%s", CFStringGetCStringPtr(outputString, kCFStringEncodingMacRoman));
            CFRelease(outputString);
        }
    }
    
    return false;
}

static void serviceClientCallback(void * target, void * refcon, IOHIDServiceClientRef service)
{
    CFStringRef string;
    
    if ( refcon == kServiceAdded ) {
        IOHIDServiceClientRegisterRemovalCallback(service, serviceClientCallback, NULL, (void*)kServiceRemoved);
    }
    
    printf("SERVICE %s:\n", (char *)refcon);
    
    string = CFCopyDescription(service);
    if ( string ) {
        printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
        CFRelease(string);
    }

    if ( __clientType == kIOHIDEventSystemClientTypeRateControlled ) {
        CFNumberRef number;
        uint32_t    primaryUsagePage=0, primaryUsage=0;
        
        number = IOHIDServiceClientCopyProperty(service, CFSTR(kIOHIDServicePrimaryUsagePageKey));
        if ( number ) {
            CFNumberGetValue(number, kCFNumberSInt32Type, &primaryUsagePage);
            CFRelease(number);
        }

        number = IOHIDServiceClientCopyProperty(service, CFSTR(kIOHIDServicePrimaryUsageKey));
        if ( number ) {
            CFNumberGetValue(number, kCFNumberSInt32Type, &primaryUsage);
            CFRelease(number);
        }
        
        if ( primaryUsagePage == __matchingUsagePage && primaryUsage == __matchingUsage ) {
            number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &__matchingInterval);
            if ( number ) {
                IOHIDServiceClientSetProperty(service, CFSTR(kIOHIDServiceReportIntervalKey), number);
                CFRelease(number);
            }
        }
    }

}

static CFMutableDictionaryRef __serviceNotifications = NULL;
void serviceRemovalCallback(void * target, void * refcon, IOHIDServiceRef service)
{
    CFStringRef string;

    CFDictionaryRemoveValue(__serviceNotifications, service);
    
    printf("SERVICE %s:\n", (char *)kServiceRemoved);
    
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
        
        printf("SERVICE %s:\n", (char *)kServiceAdded);
        
        string = CFCopyDescription(service);
        if ( string ) {
            printf("%s\n", CFStringGetCStringPtr(string, kCFStringEncodingMacRoman));
            CFRelease(string);
        }
    }
    
}

static void dispatchClientEvents(IOHIDEventSystemClientRef system, uint32_t mask)
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


static void runClient()
{
    IOHIDEventSystemClientRef eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, __clientType, NULL);
    
    require_action(eventSystem, exit, printf("Unable to create client"));
    
    require_action(!__dispatchOnly, exit, dispatchClientEvents(eventSystem, __dispatchEventMask));

    if ( __dispatchEventMask ) {
        pthread_attr_t  attr;
        pthread_t       tid;
        
        assert(!pthread_attr_init(&attr));
        assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));
        assert(!pthread_create(&tid, &attr, &dispatchClientThread, eventSystem));
        assert(!pthread_attr_destroy(&attr));
    }
            
    IOHIDEventSystemClientScheduleWithRunLoop(eventSystem, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    
    IOHIDEventSystemClientRegisterEventCallback(eventSystem, (IOHIDEventCallback)eventCallback, NULL, NULL);
    
    IOHIDEventSystemClientRegisterDeviceMatchingCallback(eventSystem, serviceClientCallback, NULL, (void*)kServiceAdded);
    
    CFArrayRef services = IOHIDEventSystemClientCopyServices(eventSystem);
    if ( services ) {
        CFIndex index, count;
        
        for(index=0, count = CFArrayGetCount(services); index<count; index++)
            serviceClientCallback(NULL, (void*)kServiceAdded, (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, index));
        
        CFRelease(services);
        
    }

    CFRunLoopRun();
exit:
    if ( eventSystem )
        CFRelease(eventSystem);
}

static void runServer()
{
    IOHIDEventSystemRef eventSystem = IOHIDEventSystemCreate(kCFAllocatorDefault);
    IOHIDNotificationRef notification = NULL;
    
    require(eventSystem, exit);
        
    IOHIDEventSystemOpen(eventSystem, eventCallback, NULL, NULL, 0);
            
    if ( !__serviceNotifications )
        __serviceNotifications = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFArrayRef services = IOHIDEventSystemCopyMatchingServices(eventSystem, NULL, servicesAddedCallback, NULL, NULL, &notification);
    if ( services ) {
        servicesAddedCallback(NULL, NULL, NULL, services);
        CFRelease(services);
        
    }

    CFRunLoopRun();
exit:
    if ( eventSystem )
        CFRelease(eventSystem);

}

static void listClients()
{
    IOHIDEventSystemClientRef eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeAdmin, NULL);
    CFIndex     types;
    
    require(eventSystem, exit);
    
    for ( types=kIOHIDEventSystemClientTypeAdmin; types<=kIOHIDEventSystemClientTypeRateControlled; types++ ) {
        CFArrayRef  clients = _IOHIDEventSystemClientCopyClientDescriptions(eventSystem, (IOHIDEventSystemClientType)types);
        CFIndex     index;
        
        if ( !clients )
            continue;
        
        for ( index=0; index<CFArrayGetCount(clients); index++ ) {
            CFStringRef clientDebugDesc = (CFStringRef)CFArrayGetValueAtIndex(clients, index);
            printf("%s\n", CFStringGetCStringPtr(clientDebugDesc, CFStringGetSystemEncoding()));
        }
        
        CFRelease(clients);
    }
    
exit:
    if (eventSystem)
        CFRelease(eventSystem);
}

static void listServices()
{
    IOHIDEventSystemClientRef eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeAdmin, NULL);
    CFArrayRef  services = NULL;
    CFIndex     index;
    
    require(eventSystem, exit);
    
    services = _IOHIDEventSystemClientCopyServiceDescriptions(eventSystem);
    require(services, exit);
    
    for ( index=0; index<CFArrayGetCount(services); index++ ) {
        CFStringRef serviceDebugDesc = (CFStringRef)CFArrayGetValueAtIndex(services, index);
        printf("%s\n", CFStringGetCStringPtr(serviceDebugDesc, CFStringGetSystemEncoding()));
    }
    
    
exit:
    if ( services )
        CFRelease(services);
    
    if (eventSystem)
        CFRelease(eventSystem);
}

static void exitTimerCallback(CFRunLoopTimerRef timer, void *info)
{
    printf("***************************************************************************\n");
    printf("Event Statistics over %d seconds\n", __timeout);
    printf("***************************************************************************\n");
    for (uint32_t index=0; index<kIOHIDEventTypeCount; index++) {
        printf("%-20.20s: %llu\n", IOHIDEventGetTypeString(index), __eventCounts[index]);
    }
    printf("\n");
    printf("Average latency: %10.3f us\n", __eventLatencyTotal ? (double)__eventLatencyTotal/__eventCount : 0);
    exit(0);
}


static void printHelp()
{
    printf("\n");
    printf("hidEventSystemMonitor usage:\n\n");
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
    printf("\n");
    printf("\t-lc\t\t\t\t: List clients\n");
    printf("\t-ls\t\t\t\t: List services\n");
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
    kEventRegistrationTypeInterval,
    kEventRegistrationTypeTimeout,
} EventRegistrationType;

int main (int argc __unused, const char * argv[] __unused)
{
    bool runAsClient = true;
    
    if ( argc > 1 ) {
        EventRegistrationType registrationType=kEventRegistrationTypeNone;
        
        for ( int index=1; index<argc; index++) {
            const char * arg = argv[index];
            if ( !strcmp("-a", arg ) ) {
                __clientType = kIOHIDEventSystemClientTypeAdmin;
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
                listClients();
                goto exit;
            }
            else if ( !strcmp("-ls", arg ) ) {
                listServices();
                goto exit;
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
            else if ( registrationType == kEventRegistrationTypeReplaceMask ) {
                __eventMask = (uint32_t)strtoul(arg, NULL, 16);
            }
            else if ( registrationType == kEventRegistrationTypeRemoveMask ) {
                __eventMask &= ~(strtoul(arg, NULL, 16));
            }
            else if ( registrationType == kEventRegistrationTypeAdd ) {
                __eventMask |= (1<<strtoul(arg, NULL, 10));
            }
            else if ( registrationType ==  kEventRegistrationTypeRemove ) {
                __eventMask &= ~(1<<strtoul(arg, NULL, 10));
            }
            else if ( registrationType ==  kEventRegistrationTypeDispatch ) {
                __dispatchEventMask = (1<<strtoul(arg, NULL, 10));
            }
            else if ( registrationType == kEventRegistrationTypeDispatchMask ) {
                __dispatchEventMask = (uint32_t)strtoul(arg, NULL, 16);
            }
            else if ( registrationType == kEventRegistrationTypeUsagePage ) {
                __matchingUsagePage = (uint32_t)strtoul(arg, NULL, 10);
            }
            else if ( registrationType == kEventRegistrationTypeUsage ) {
                __matchingUsage = (uint32_t)strtoul(arg, NULL, 10);
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
