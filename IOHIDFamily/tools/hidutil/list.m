//
//  list.m
//  IOHIDFamily
//
//  Created by dekom on 8/15/17.
//

#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <getopt.h>
#include "utility.h"

int list(int argc, const char * argv[]);

static const char listOptionShort[] = "hm:";
static const struct option listOptionLong[] =
{
    { "help",       0,                  NULL,   'h' },
    { "matching",   required_argument,  NULL,   'm' },
    { NULL,         0,                  NULL,    0  }
};

const char listUsage[] =
"\nList HID Event System services and devices\n"
"\nUsage:\n\n"
"  hidutil list [ --matching <matching> ]\n"
"\nFlags:\n\n"
MATCHING_HELP
"\nExamples:\n\n"
"  hidutil list\n"
"  hidutil list --matching '{\"ProductID\":0x54c}'\n"
"  hidutil list --matching '{\"ProductID\":0x54c,\"VendorID\":746}'\n";

static void listServices(IOHIDEventSystemClientRef client) {
    NSArray *services = (NSArray *)CFBridgingRelease(IOHIDEventSystemClientCopyServices(client));
    
    if (!services) {
        return;
    }
    
    printf("Services:\n");
    printf("%-8s   %-8s   %-10s   %-10s  %-6s  %-12s  %-12s  %s\n", "VendorID", "ProductID", "LocationID", "UsagePage", "Usage", "RegistryID", "Transport", "Class/Product");
    
    for (id service in services) {
        NSDictionary *info = createServiceInfoDictionary((__bridge IOHIDServiceClientRef)service);
        if (info) {
            printf ("0x%-6x   0x%-7x   0x%-8x   %-10d  %-6d  0x%-10lx  %-12s  <%s> \"%s\"\n",
                    ((NSNumber *)info[@kIOHIDVendorIDKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIOHIDProductIDKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIOHIDLocationIDKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIOHIDPrimaryUsagePageKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIOHIDPrimaryUsageKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIORegistryEntryIDKey]).unsignedLongValue,
                    [((NSString *)info[@kIOHIDTransportKey]) cStringUsingEncoding: NSASCIIStringEncoding],
                    [((NSString *)info[@kIOClassKey]) cStringUsingEncoding: NSASCIIStringEncoding],
                    [((NSString *)info[@kIOHIDProductKey]) cStringUsingEncoding: NSASCIIStringEncoding]
                    );
        }
    }
}

static void listDevices(IOHIDManagerRef manager) {
    NSSet *devices = CFBridgingRelease(IOHIDManagerCopyDevices(manager));
    
    if (!devices) {
        return;
    }
    
    printf("Devices:\n");
    printf("%-8s   %-8s   %-10s   %-10s  %-6s  %-12s  %-12s  %s\n", "VendorID", "ProductID", "LocationID", "UsagePage", "Usage", "RegistryID", "Transport", "Class/Product");
    
    for (id device in devices) {
        NSDictionary *info = createDeviceInfoDictionary((__bridge IOHIDDeviceRef)device);
        if (info) {
            printf ("0x%-6x   0x%-7x   0x%-8x   %-10d  %-6d  0x%-10lx  %-12s  <%s> \"%s\"\n",
                    ((NSNumber *)info[@kIOHIDVendorIDKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIOHIDProductIDKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIOHIDLocationIDKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIOHIDPrimaryUsagePageKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIOHIDPrimaryUsageKey]).unsignedIntValue,
                    ((NSNumber *)info[@kIORegistryEntryIDKey]).unsignedLongValue,
                    [((NSString *)info[@kIOHIDTransportKey]) cStringUsingEncoding: NSASCIIStringEncoding],
                    [((NSString *)info[@kIOClassKey]) cStringUsingEncoding: NSASCIIStringEncoding],
                    [((NSString *)info[@kIOHIDProductKey]) cStringUsingEncoding: NSASCIIStringEncoding]
                    );
        }
    }
}

int list (int argc, const char * argv[]) {
    int                         status      = STATUS_SUCCESS;
    int                         arg;
    IOHIDEventSystemClientRef   client      = NULL;
    IOHIDManagerRef             manager     = NULL;
    char                        *matching   = NULL;
    
    while ((arg = getopt_long(argc, (char **) argv, listOptionShort, listOptionLong, NULL)) != -1) {
        switch (arg) {
            case 'h':
                printf ("%s", listUsage);
                goto exit;
            case 'm':
                matching = optarg;
                break;
            default:
                status = STATUS_ERROR;
                goto exit;
        }
    }
    
    client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    if (!client) {
        status = kUnknownErr;
        goto exit;
    }
    
    manager = IOHIDManagerCreate(kCFAllocatorDefault, 0);
    if (!manager) {
        status = kUnknownErr;
        goto exit;
    }
    
    if (matching && !setClientMatching(client, matching)) {
        printf("bad matching string: %s\n", matching);
    }
    
    listServices(client);
    printf("\n");
    
    if (matching) {
        if (!setManagerMatching(manager, matching)) {
            printf("bad matching string: %s\n", matching);
            IOHIDManagerSetDeviceMatching(manager, NULL);
        }
    } else {
        IOHIDManagerSetDeviceMatching(manager, NULL);
    }
    
    listDevices(manager);
    
exit:
    if (client) {
        CFRelease(client);
    }
    
    if (manager) {
        CFRelease(manager);
    }
    
    return status;
}
