//
//  property.m
//  IOHIDFamily
//
//  Created by YG on 4/14/16.
//
//

#import  <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include "hdutil.h"
#include <getopt.h>
#include "utility.h"

int property (int argc __unused, const char * argv[] __unused);
int propertySetOnEventSystem (IOHIDEventSystemClientRef client, NSDictionary * propertiesDicitonary);
int propertySetOnServices (IOHIDEventSystemClientRef client, NSDictionary * propertiesDicitonary);
int propertyGetEventSystemProperty (IOHIDEventSystemClientRef client, NSString* key);
int propertyGetServicesProperty (IOHIDEventSystemClientRef client, NSString* key);
void propertyPrint (NSString *str);

static const char propertyOptionShort[] = "hm:g:s:";
static const struct option propertyOptionLong[] =
{
    { "help",       0                ,  NULL,   'h' },
    { "matching",   required_argument,  NULL,   'm' },
    { "get",        required_argument,  NULL,   'g' },
    { "set",        required_argument,  NULL,   's' },
    { NULL,         0,                  NULL,    0  }
};

const char propertyUsage[] =
"\nRead/Write HID Event System property\n"
"\nUsage:\n\n"
"  hidutil property [ --matching <matching> ][ --get <key> ][ --set <key> ]\n"
"\nFlags:\n\n"
"  -g  --get...................Get property with key (where key is string value)\n"
"  -s  --set...................Set property (key/value pair JSON style dictionary)\n"
MATCHING_HELP
"\nExamples:\n\n"
"  hidutil property --matching '{\"ProductID\":0x54c}' --get \"HIDPointerAcceleration\"\n"
"  hidutil property --matching '{\"ProductID\":0x54c,\"VendorID\":746}' --set '{\"HIDPointerAcceleration\":0}'\n"
"  hidutil property --get \"HIDPointerAcceleration\"\n";

void propertyPrint (NSString *str) {
    const char * c_str = [str UTF8String];
    if (c_str) {
        printf ("%s" , [str UTF8String]);
    }
}

int propertySetOnEventSystem (IOHIDEventSystemClientRef client, NSDictionary * propertiesDicitonary) {
    
    [propertiesDicitonary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop __unused) {
        propertyPrint ([NSString stringWithFormat:@"%@:%@\n", key, value]);
        IOHIDEventSystemClientSetProperty (client,  (__bridge CFStringRef) key, (__bridge CFStringRef) value);
    }];
    return STATUS_SUCCESS;
}

int propertySetOnServices (IOHIDEventSystemClientRef client, NSDictionary * propertiesDicitonary) {

    NSArray *services = (NSArray *)CFBridgingRelease(IOHIDEventSystemClientCopyServices(client));
    if (services) {
        printf ("%-8s  %-20s  %s\n", "RegistryID", "Key", "Value");
        for (id service in services) {
            NSNumber *regID = (__bridge NSNumber *)IOHIDServiceClientGetRegistryID((__bridge IOHIDServiceClientRef)service);
            [propertiesDicitonary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop __unused) {
                propertyPrint ([NSString stringWithFormat:@"%-8lx   %-20@   %@\n", regID.unsignedLongValue, key, value]);
                IOHIDServiceClientSetProperty ((__bridge IOHIDServiceClientRef)service,  (__bridge CFStringRef) key, (__bridge CFStringRef) value);
            }];
        }
    }
   
    return STATUS_SUCCESS;
}

int propertyGetEventSystemProperty (IOHIDEventSystemClientRef client, NSString* key) {
    id value = CFBridgingRelease(IOHIDEventSystemClientCopyProperty (client, (__bridge CFStringRef) key));
    propertyPrint ([NSString stringWithFormat:@"%@\n",value]);
    return STATUS_SUCCESS;
}

int propertyGetServicesProperty (IOHIDEventSystemClientRef client, NSString* key) {
    NSArray *services = (NSArray *)CFBridgingRelease(IOHIDEventSystemClientCopyServices(client));
    if (services) {
        printf ("%-8s  %-20s  %s\n", "RegistryID", "Key", "Value");
        for (id service in services) {
            id value = CFBridgingRelease(IOHIDServiceClientCopyProperty ((__bridge IOHIDServiceClientRef)service, (__bridge CFStringRef) key));
            NSNumber *regID = (__bridge NSNumber *)IOHIDServiceClientGetRegistryID((__bridge IOHIDServiceClientRef)service);
            propertyPrint ([NSString stringWithFormat:@"%-8lx   %-20@   %@\n", regID.unsignedLongValue, key, value]);
        }
    }
    return STATUS_SUCCESS;
}

int property (int argc, const char * argv[]) {
    int                         arg;
    int                         status = STATUS_SUCCESS;
    IOHIDEventSystemClientRef   client = NULL;
    id                          propertyDicitonary = NULL;
    NSString                    *propertyKey = NULL;
    bool                        matching = false;
    
#ifdef INTERNAL
    client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
#else
    client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeSimple, NULL);
#endif
    if (!client) {
        status = STATUS_ERROR;
        goto exit;
    }
    
    while ((arg = getopt_long(argc, (char **) argv, propertyOptionShort, propertyOptionLong, NULL)) != -1) {
        switch (arg) {
             case 'h':
                printf ("%s", propertyUsage);
                goto exit;
            case 'm':
                matching = setClientMatching(client, optarg);
                if (!matching) {
                    printf ("bad matching string: %s\n", optarg);
                }
                break;
            case 's':
                propertyDicitonary = createPropertiesDicitonary (createPropertiesString(optarg));
                if (propertyDicitonary == NULL || ![propertyDicitonary isKindOfClass:[NSDictionary class]]) {
                    printf ("\nERROR!!!! Unable to create property object for \'%s\'\n", optarg);
                }
                break;
            case 'g':
                propertyKey = [NSString stringWithUTF8String:optarg];
                break;
            default:
                status = STATUS_ERROR;
                goto exit;
        }
    }
    
    if ((!propertyKey && !propertyDicitonary) ||
        (propertyKey && propertyDicitonary)) {
        status = STATUS_ERROR;
        goto exit;
    }
    
    if (propertyDicitonary) {
        if (matching) {
            status = propertySetOnServices (client, propertyDicitonary);
        } else {
            status = propertySetOnEventSystem (client, propertyDicitonary);
        }
    } else {
        if (!matching) {
            status = propertyGetEventSystemProperty (client, propertyKey);
        } else {
            status = propertyGetServicesProperty (client, propertyKey);
        }
    }

exit:
    if (client) {
        CFRelease (client);
    }
    return status;
}
