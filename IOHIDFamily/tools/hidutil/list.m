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


struct fields {
    NSString * key;
    NSUInteger integerBase;
    NSUInteger width;
    NSString * name;
} serviceFields [] = {
    {@kIOHIDVendorIDKey,            16, 0, @kIOHIDVendorIDKey},
    {@kIOHIDProductIDKey,           16, 0, @kIOHIDProductIDKey},
    {@kIOHIDLocationIDKey,          16, 0, @kIOHIDLocationIDKey },
    {@kIOHIDPrimaryUsagePageKey,    10, 0, @"UsagePage"},
    {@kIOHIDPrimaryUsageKey,        10, 0, @"Usage"},
    {@kIORegistryEntryIDKey,        16, 0, @"RegistryID"},
    {@kIOHIDTransportKey,            0, 0, @kIOHIDTransportKey},
    {@kIOClassKey,                   0, 0, @"Class"},
    {@kIOHIDProductKey,              0, 0, @"Product"},
    {@"IOUserClass",                 0, 0, @"UserClass"},
    {@kIOHIDBuiltInKey,              0, 0, @kIOHIDBuiltInKey},
};


static void listPrint(NSArray * infoArray) {
    NSDictionary * info;

    for (NSUInteger index = 0; index < sizeof(serviceFields) / sizeof(serviceFields[0]); index++) {
        serviceFields[index].width = serviceFields[index].name.length;
    }
    
    for (info in infoArray) {
        for (NSUInteger index = 0; index < sizeof(serviceFields) / sizeof(serviceFields[0]); index++) {
            NSUInteger width = formatPropertyValue (info[serviceFields[index].key], serviceFields[index].integerBase).length;
            if (width > serviceFields[index].width) {
                serviceFields[index].width = width;
            }
        }
    }
    for (NSUInteger index = 0; index < sizeof(serviceFields) / sizeof(serviceFields[0]); index++) {
        printf("%-*s", (int)serviceFields[index].width + 1, [serviceFields[index].name cStringUsingEncoding: NSASCIIStringEncoding]);
    }
    printf ("\n");
    
    for (info in infoArray) {
        for (NSUInteger index = 0; index < sizeof(serviceFields) / sizeof(serviceFields[0]); index++) {
            printf("%-*s", (int)serviceFields[index].width + 1, [formatPropertyValue (info[serviceFields[index].key], serviceFields[index].integerBase) cStringUsingEncoding: NSASCIIStringEncoding]);
        }
        printf ("\n");
    }
}


static void listServices(IOHIDEventSystemClientRef client) {
    NSArray *services = (NSArray *)CFBridgingRelease(IOHIDEventSystemClientCopyServices(client));
    NSDictionary *info;
    
    if (!services) {
        return;
    }
    
    NSMutableArray * infoArray = [[NSMutableArray alloc] init];
    for (id service in services) {
        info = createServiceInfoDictionary((__bridge IOHIDServiceClientRef)service);
        if (info) {
            [infoArray addObject: info];
        }
    }

    printf("Services:\n");
 
    listPrint (infoArray);
}

static void listDevices(IOHIDManagerRef manager) {
    NSSet *devices = CFBridgingRelease(IOHIDManagerCopyDevices(manager));
    NSDictionary *info;
    if (!devices) {
        return;
    }
    
    
    NSMutableArray * infoArray = [[NSMutableArray alloc] init];
    for (id device in devices) {
        info = createDeviceInfoDictionary((__bridge IOHIDDeviceRef)device);
        if (info) {
            [infoArray addObject: info];
        }
    }
    
    printf("Devices:\n");

    listPrint (infoArray);
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
#ifdef INTERNAL
    client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
#else
    client = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeSimple, NULL);
#endif
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
