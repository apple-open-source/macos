//
//  monitor.m
//  IOHIDFamily
//
//  Created by dekom on 8/2/17.
//


#include <IOKit/hid/IOHIDEventSystemClient.h>
#include <getopt.h>
#include "utility.h"
#import "HIDEvent.h"
#import <objc/runtime.h>
#import <AssertMacros.h>
#import <os/variant_private.h>

#define kOptClientType  1000

Boolean clientTypeForString (NSString * string, IOHIDEventSystemClientType * type);
int monitor(int argc, const char * argv[]);

static const char MAIN_OPTIONS_SHORT[] = "p:cm:i:s:t:o:hv";
static const struct option MAIN_OPTIONS[] =
{
    { "predicate",  required_argument,  NULL,   'p' },
    { "children",   no_argument,        NULL,   'c' },
    { "matching",   required_argument,  NULL,   'm' },
    { "info",       required_argument,  NULL,   'i' },
    { "show",       required_argument,  NULL,   's' },
    { "type",       required_argument,  NULL,   't' },
    { "output",     required_argument,  NULL,   'o' },
    { "help",       0,                  NULL,   'h' },
    { "verbose",    0,                  NULL,   'v' },
    { "client",     required_argument,  NULL,    kOptClientType },
    { NULL,         0,                  NULL,    0  }
};

const char monitorUsage[] =
"\nMonitor HID Event System events\n"
"\nUsage:\n\n"
"  hidutil monitor [ --info <eventType | eventName> ][ --predicate <predicate> ][ --show <varibles> ][ --type <event type> ][ --matching <matching> ][ --children ]\n"
"\nFlags:\n\n"
"  -i  --info..................print predicate info for an event type\n"
"  -p  --predicate.............filter events output based on predicate\n"
"  -s  --show..................show only specified variables\n"
"  -t  --type..................filter by event type, takes integer or string\n"
"  -c  --children..............show child events\n"
"  -v  --verbose...............print service enumerated/terminated info\n"
"  -o  --output................output to file\n"
"      --client................event system clien type (admin, passive, ratecontrolled, simple, monitor)\n"

MATCHING_HELP
"\nExamples:\n\n"
"  hidutil monitor --info keyboard\n"
"  hidutil monitor --type keyboard\n"
"  hidutil monitor --predicate 'typestr contains \"digit\"' --children\n"
"  hidutil monitor --predicate 'usagepage == 7 and latency > 100'\n"
"  hidutil monitor --show 'timestamp sender typestr usagepage usage'\n"
"  hidutil monitor --matching '{\"ProductID\":0x54c,\"VendorID\":746}'\n";

static NSCompoundPredicate * _predicate = NULL;
static bool                  _children   = false;
static NSArray *             _variables = NULL;
static char *                _fileNameStr;
static char *                _clientTypeStr;
static FILE *                _file;
static const NSArray        *_debugKeys;

static void printClassInfo(Class cls) {
    objc_property_t *properties = NULL;
    unsigned int outCount, i;
    
    properties = class_copyPropertyList(cls, &outCount);
    
    for(i = 0; i < outCount; i++) {
        objc_property_t property = properties[i];
        printf("    %s\n", property_getName(property));
    }
    
    free(properties);
}

static void printEventInfo(char *arg) {
    NSString *eventString       = [NSString stringWithUTF8String:arg];
    NSNumberFormatter *nf       = [[NSNumberFormatter alloc] init];
    BOOL isNum                  = [nf numberFromString:eventString] != nil;
    IOHIDEventRef dummyEvent    = NULL;
    HIDEvent *event             = [[HIDEvent alloc] init];
    IOHIDEventType eventType    = kIOHIDEventTypeCount;
    
    // get event type from string
    if (isNum) {
        eventType = [eventString intValue];
    } else {
        for (uint32_t i = 0; i < kIOHIDEventTypeCount; i++) {
            NSString *eventTypeString = [NSString stringWithUTF8String:IOHIDEventGetTypeString(i)];
            
            if ([[eventTypeString lowercaseString] containsString:eventString]) {
                eventType = i;
                break;
            }
        }
    }
    
    dummyEvent = IOHIDEventCreate(kCFAllocatorDefault, eventType, 0, 0);
    
    printf("base event predicates:\n");
    printClassInfo([event class]);
    
    if (eventType < kIOHIDEventTypeCount) {
        event = createHIDEvent(dummyEvent);
        printf("\n%s event predicates:\n", IOHIDEventGetTypeString(eventType));
        printClassInfo([event class]);
    }
    
    CFRelease(dummyEvent);
}

static void printEventVariables(HIDEvent *event) {
    NSString *eventString = [[NSString alloc] init];
    
    for (NSString *variable in _variables) {
        @try {
            if (variable == _variables.lastObject) {
                eventString = [eventString stringByAppendingFormat:@"%@:%@", variable, [event valueForKey:variable]];
            } else {
                eventString = [eventString stringByAppendingFormat:@"%@:%@ ", variable, [event valueForKey:variable]];
            }
        } @ catch(...) {
        }
    }
    
    if (eventString == nil) {
        fprintf(_file, "%s\n", [[event description] UTF8String]);
    } else {
        fprintf(_file, "%s\n", [eventString UTF8String]);
    }
}

static void eventCallback(void *target __unused, void *refcon __unused, void *sender __unused, IOHIDEventRef event) {
    bool        predicated  = false;
    HIDEvent    *hidEvent   = createHIDEvent(event);
    
    if (_predicate) {
        @try {
            if (![_predicate evaluateWithObject:hidEvent]) {
                predicated = true;
            }
        } @ catch(...) {
            predicated = true;
        }
    }
    
    if (!predicated) {
        if (_variables) {
            printEventVariables(hidEvent);
        } else {
           fprintf(_file, "%s\n", [[hidEvent description] UTF8String]);
        }
        
        if (_children) {
            CFArrayRef children = IOHIDEventGetChildren(event);
            for (CFIndex index = 0, count = children ? CFArrayGetCount(children) : 0; index < count; index++) {
                IOHIDEventRef child = (IOHIDEventRef)CFArrayGetValueAtIndex(children, index);
                HIDEvent *childHIDEvent = createHIDEvent(child);
                
                if (_variables) {
                    fprintf(_file, "    ");
                    printEventVariables(childHIDEvent);
                } else {
                    fprintf(_file,"    %s\n", [[childHIDEvent description] UTF8String]);
                }
            }
        }
    }
}

static NSPredicate *predicateForEventType(NSString *eventType)
{
    if ([eventType rangeOfCharacterFromSet:[NSCharacterSet decimalDigitCharacterSet]].location == NSNotFound) {
        // event type string
        return [NSPredicate predicateWithFormat:@"typestr contains %@", eventType ];
    } else {
        // event type number
        return [NSPredicate predicateWithFormat:@"typeval == %d",  [eventType intValue]];
    }
}

Boolean clientTypeForString (NSString * string, IOHIDEventSystemClientType * type)
{
    typedef struct {
        IOHIDEventSystemClientType type;
        NSString *                 str;
    } ClientTypeName;

    ClientTypeName table [] = {
        {
            kIOHIDEventSystemClientTypeAdmin,
            @"admin"
        },
        {
            kIOHIDEventSystemClientTypeMonitor,
            @"monitor"
        },
        {
            kIOHIDEventSystemClientTypePassive,
            @"passive"
        },
        {
            kIOHIDEventSystemClientTypeRateControlled,
            @"ratecontrolled"
        },
        {
            kIOHIDEventSystemClientTypeSimple,
            @"simple"
        },
    };

    for (int index = 0 ; index < sizeof(table) / sizeof (table[0]); index++) {
        if ([table[index].str containsString:string]) {
            *type = table[index].type;
            return true;
        }
    }

    return false;
}

static void _serviceRemovedCallback(void *target __unused,
                                    void *refcon __unused,
                                    IOHIDServiceClientRef service)
{
    uint64_t regID = 0;
    
    id serviceID = (__bridge id)IOHIDServiceClientGetRegistryID(service);
    if (serviceID) {
        regID = [serviceID unsignedLongLongValue];
    }
    
    printf("service removed:0x%llx ", regID);
    for (NSString *key in _debugKeys) {
        id val = CFBridgingRelease(IOHIDServiceClientCopyProperty(service, (__bridge CFStringRef)key));
        printf("%s ", [[NSString stringWithFormat:@"%@:%@", key, val] UTF8String]);
    }
    printf("\n");
}

static void _serviceAddedCallback(void *target __unused,
                                  void *refcon __unused,
                                  IOHIDServiceClientRef service)
{
    uint64_t regID = 0;
    
    IOHIDServiceClientRegisterRemovalCallback(service, _serviceRemovedCallback, 0, 0);
    
    
    id serviceID = (__bridge id)IOHIDServiceClientGetRegistryID(service);
    if (serviceID) {
        regID = [serviceID unsignedLongLongValue];
    }
    
    printf("service added:0x%llx ", regID);
    for (NSString *key in _debugKeys) {
        id val = CFBridgingRelease(IOHIDServiceClientCopyProperty(service, (__bridge CFStringRef)key));
        printf("%s ", [[NSString stringWithFormat:@"%@:%@", key, val] UTF8String]);
    }
    printf("\n");
}


int monitor(int argc __unused, const char * argv[] __unused) {
    int                         arg;
    int                         status              = STATUS_SUCCESS;
    IOHIDEventSystemClientRef   client              = NULL;
    dispatch_source_t           source;
    bool                        verbose             = false;
    char *                      matchingStr         = NULL;
    _debugKeys = @[ @kIOHIDPrimaryUsagePageKey, @kIOHIDPrimaryUsageKey, @kIOHIDVendorIDKey, @kIOHIDProductIDKey,
                     @kIOHIDTransportKey, @kIOHIDLocationIDKey, @kIOHIDProductKey, @kIOHIDManufacturerKey ];
    
    if(!os_variant_allows_internal_security_policies(nil)) {
        return 0;
    }
    
    while ((arg = getopt_long(argc, (char **) argv, MAIN_OPTIONS_SHORT, MAIN_OPTIONS, NULL)) != -1) {
        switch (arg) {
                // --help
            case 'h':
                printf("%s", monitorUsage);
                goto exit;
                // --verbose
            case 'v':
                verbose = true;
                break;
                // --predicate
            case 'p': {
                NSPredicate *predicate = [NSPredicate predicateWithFormat:@(optarg)];
                if (_predicate) {
                    _predicate = [NSCompoundPredicate andPredicateWithSubpredicates:[NSArray arrayWithObjects:_predicate, predicate, nil]];
                } else {
                    _predicate = (NSCompoundPredicate *)predicate;
                }
                
                if (!_predicate) {
                    printf("Bad predicate: %s\n", optarg);
                    status = STATUS_ERROR;
                    goto exit;
                }
                break;
            }
                // --children
            case 'c':
                _children = true;
                break;
                // --output
            case 'o':
                _fileNameStr = optarg;
                break;
                // --matching
            case 'm':
                matchingStr = optarg;
                break;
                // --info
            case 'i':
                printEventInfo(optarg);
                goto exit;
                break;
                // --show
            case 's':
                _variables = [[NSString stringWithUTF8String:optarg] componentsSeparatedByString:@" "];
                break;
                // --type
            case 't':
                {
                    NSPredicate *predicate = predicateForEventType([NSString stringWithUTF8String:optarg]);
                
                    if (_predicate) {
                        _predicate = [NSCompoundPredicate andPredicateWithSubpredicates:[NSArray arrayWithObjects:_predicate, predicate, nil]];
                    } else {
                        _predicate = (NSCompoundPredicate *)predicate;
                    }
                }
                break;
            case kOptClientType:
                _clientTypeStr = optarg;
                break;
            default:
                printf("%s", monitorUsage);
                goto exit;
                break;
        }
    }
    
    if (_fileNameStr) {
        _file = fopen(_fileNameStr, "w+");
        require_action_quiet (_file, exit, printf("Unable open ouput file: %s", _fileNameStr));
    } else {
        _file = stdout;
    }

    signal(SIGINT, SIG_IGN);
    source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_global_queue(0, 0));
    dispatch_source_set_event_handler(source, ^{
        CFRunLoopRef runloop = CFRunLoopGetMain();
        CFRunLoopStop(runloop);
        CFRunLoopWakeUp(runloop);
        
    });
    dispatch_resume(source);

    
    IOHIDEventSystemClientType type = kIOHIDEventSystemClientTypeMonitor;
    
    if (_clientTypeStr && !clientTypeForString ([[NSString stringWithUTF8String:_clientTypeStr] lowercaseString], &type)) {
        printf("Unknowwn client type: %s\n", _clientTypeStr);
        status = kOptionErr;
        goto exit;
    }
    
    client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, type, NULL);
    require_action_quiet (client, exit, status = kUnknownErr; printf("ERROR!Unable to create event system client\n"));
    
    
    require_action_quiet (!matchingStr || setClientMatching(client, matchingStr) == true, exit, status = kParamErr; printf("ERROR! bad matching string:%s\n", matchingStr));
    
    if (verbose) {
        IOHIDEventSystemClientRegisterDeviceMatchingCallback(client, _serviceAddedCallback, 0, 0);
        NSArray *services = CFBridgingRelease(IOHIDEventSystemClientCopyServices(client));
        for (id service in services) {
            _serviceAddedCallback(0, 0, (__bridge IOHIDServiceClientRef)service);
        }
    }
    
    IOHIDEventSystemClientRegisterEventCallback(client, eventCallback, NULL, NULL);
    IOHIDEventSystemClientScheduleWithRunLoop(client, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    
    CFRunLoopRun();
    
    IOHIDEventSystemClientUnscheduleWithRunLoop(client, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDEventSystemClientUnregisterEventCallback(client, eventCallback, NULL, NULL);
    status = STATUS_SUCCESS;
    
    if (_file) {
        fclose(_file);
    }

exit:
    
    if (client) {
        CFRelease(client);
    }
    
    return status;
}
