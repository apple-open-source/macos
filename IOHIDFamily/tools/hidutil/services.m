//
//  services.m
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

#define PROPERTY_USAGE_LIST \
"                                  ProductID        - numeric value (decimal or hex)\n" \
"                                  VendorID         - numeric value (decimal or hex)\n" \
"                                  LocationID       - numeric value (decimal or hex)\n" \
"                                  PrimaryUsagePage - numeric value (decimal or hex)\n" \
"                                  PrimaryUsage     - numeric value (decimal or hex)\n" \
"                                  Trasport         - string value\n" \
"                                  Product          - string value\n"


int list (int argc, const char * argv[] __unused);
void listPrintService (IOHIDServiceClientRef service);


static const char listOptionShort[] = "hf:";
static const struct option listOptionLong[] =
{
    { "help",       0,                  NULL,   'h' },
    { "filter",     required_argument,  NULL,   'f' },
    { NULL,         0,                  NULL,    0  }
};

const char listUsage[] =
"\nList HID Event System services\n"
"\nUsage:\n\n"
"  hidutil list [object] [flags]\n"
"\nExamples:\n\n"
"  hidutil list\n"
"  hidutil list --filter '{\"ProductID\":0x54c}'\n"
"  hidutil list --filter '{\"ProductID\":0x54c,\"VendorID\":746}'\n"
"\nFlags:\n\n"
"  --filter dictionary.........Filter services by properties (key/value pair JSON style dictionary)\n"
"                              Supported property:\n"
                               PROPERTY_USAGE_LIST;

static const char propertyOptionShort[] = "hf:g:s:";
static const struct option propertyOptionLong[] =
{
    { "help",       0                ,  NULL,   'h' },
    { "filter",     required_argument,  NULL,   'f' },
    { "get",        required_argument,  NULL,   'g' },
    { "set",        required_argument,  NULL,   's' },
    { NULL,         0,                  NULL,    0  }
};

const char propertyUsage[] =
"\nRead/Write HID Event System property\n"
"\nUsage:\n\n"
"  hidutil property [--filter <value> ] <--get <key> |--write <dictionary>>\n"
"\nExamples:\n\n"
"  hidutil property --filter '{\"ProductID\":0x54c}' --get \"HIDPointerAcceleration\"\n"
"  hidutil property --filter '{\"ProductID\":0x54c,\"VendorID\":746}' --set '{\"HIDPointerAcceleration\":0}'\n"
"  hidutil property --get \"HIDPointerAcceleration\"\n"
"\nFlags:\n\n"
"  --filter dictionary.........Filter services by properties (key/value pair JSON style dictionary)\n"
"                              Supported property:\n"
                               PROPERTY_USAGE_LIST
"  --get key...................Get property with key (where key is string value)\n"
"  --set dictionary............Set property (key/value pair JSON style dictionary)\n";


void listPrintService (IOHIDServiceClientRef service) {

   NSDictionary *serviceInfo = createServiceInfoDictionary (service);
   if (!serviceInfo) {
        return ;
   }
    
   printf ("%-8d   %-8d  %-10d  %-10d  %-6d  %-10lx  %-12s  \"%s\"\n",
        ((NSNumber *)serviceInfo[@kIOHIDVendorIDKey]).unsignedIntValue,
        ((NSNumber *)serviceInfo[@kIOHIDProductIDKey]).unsignedIntValue,
        ((NSNumber *)serviceInfo[@kIOHIDLocationIDKey]).unsignedIntValue,
        ((NSNumber *)serviceInfo[@kIOHIDPrimaryUsagePageKey]).unsignedIntValue,
        ((NSNumber *)serviceInfo[@kIOHIDPrimaryUsageKey]).unsignedIntValue,
        ((NSNumber *)serviceInfo[@"RegistryID"]).unsignedLongValue,
        [((NSString *)serviceInfo[@kIOHIDTransportKey]) cStringUsingEncoding: NSASCIIStringEncoding],
        [((NSString *)serviceInfo[@kIOHIDProductKey]) cStringUsingEncoding: NSASCIIStringEncoding]
        );
  
}

int list (int argc __unused, const char * argv[] __unused) {
    int                          status = STATUS_SUCCESS;
    int                          arg;
    IOHIDEventSystemClientRef    client = NULL;
    NSDictionary                 *filterDictionary = NULL;
    NSArray                      *services;
    while ((arg = getopt_long(argc, (char **) argv, listOptionShort, listOptionLong, NULL)) != -1) {
        switch (arg) {
            case 'h':
                printf ("%s", listUsage);
                return STATUS_SUCCESS;
            case 'f':
                filterDictionary = createFilterDictionary (createFilterString(optarg));
                if (filterDictionary == NULL) {
                    printf ("\nERROR!!!! Unable to create service filter for \'%s\'\n", optarg);
                }
                break;
            default:
                return STATUS_ERROR;
        }
    }
    
    client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    if (!client) {
        status = STATUS_ERROR;
        goto exit;
    }
    
    if (filterDictionary) {
        IOHIDEventSystemClientSetMatching (client, (__bridge CFDictionaryRef)(filterDictionary));
    }
    
    services = (NSArray *)CFBridgingRelease(IOHIDEventSystemClientCopyServices(client));
    if (services) {
        printf ("%-8s  %-8s  %-10s  %-10s  %-6s  %-10s  %-12s  %s\n", "VendorID", "ProductID", "LocationID", "PUsagePage", "PUsage", "RegistryID", "Transport", "Product" );
    
        for (id service in services) {
            listPrintService ((__bridge IOHIDServiceClientRef)service);
        }
    }
 
exit:
    
    if (client) {
        CFRelease (client);
    }
    return status;
}


void propertyPrint (NSString *str) {
//    [str writeToFile: @"/dev/stdout" atomically: NO];
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
            NSDictionary *serviceInfo = createServiceInfoDictionary((__bridge IOHIDServiceClientRef)service);
            [propertiesDicitonary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop __unused) {
                propertyPrint ([NSString stringWithFormat:@"%-8lx   %-20@   %@\n", ((NSNumber *)serviceInfo[@"RegistryID"]).unsignedLongValue, key, value]);
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
            NSDictionary *serviceInfo = createServiceInfoDictionary((__bridge IOHIDServiceClientRef)service);
            id value = CFBridgingRelease(IOHIDServiceClientCopyProperty ((__bridge IOHIDServiceClientRef)service, (__bridge CFStringRef) key));
            propertyPrint ([NSString stringWithFormat:@"%-8lx   %-20@   %@\n",  ((NSNumber *)serviceInfo[@"RegistryID"]).unsignedLongValue, key, value]);
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
    NSDictionary                *filterDictionary = NULL;
    
    while ((arg = getopt_long(argc, (char **) argv, propertyOptionShort, propertyOptionLong, NULL)) != -1) {
        switch (arg) {
             case 'h':
                printf ("%s", propertyUsage);
                return STATUS_SUCCESS;
            case 'f':
                filterDictionary = createFilterDictionary (createFilterString(optarg));
                if (filterDictionary == NULL) {
                    printf ("\nERROR!!!! Unable to create service filter for \'%s\'\n", optarg);
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
                return STATUS_ERROR;
        }
    }
    
    if ((!propertyKey && !propertyDicitonary) ||
        (propertyKey && propertyDicitonary)) {
        status = STATUS_ERROR;
        goto exit;
    }

    client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    if (!client) {
        status = STATUS_ERROR;
        goto exit;
    }

    if (filterDictionary) {
        IOHIDEventSystemClientSetMatching (client, (__bridge CFDictionaryRef)(filterDictionary));
    }
    
    if (propertyDicitonary) {
        if (filterDictionary) {
            status = propertySetOnServices (client, propertyDicitonary);
        } else {
            status = propertySetOnEventSystem (client, propertyDicitonary);
        }
    } else {
        if (filterDictionary == NULL) {
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
