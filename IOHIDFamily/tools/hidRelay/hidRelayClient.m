//
//  client.m
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDKeys.h>
#import "DeviceClient.h"


static void printHelp()
{
    printf("\n");
    printf("hidRelayClient usage:\n\n");
    printf("\t--usage <usage>\n");
    printf("\t--usagepage <usage page>\n");
    printf("\t--transport <transport string value>\n");
    printf("\t--vid <vendor id>\n");
    printf("\t--pid <product id>\n");
    printf("\t--multicast\n");
    printf("\n");
}

int main(int argc, const char * argv[]) {
    CFMutableDictionaryRef matching = NULL;
    static BOOL useBonjour=YES;
    CFStringRef interface = NULL;
    int argi;
    for (argi=1; argi<argc; argi++) {
        if ( !strcmp("--usage", argv[argi]) && (argi+1) < argc) {
            long value = atol(argv[++argi]);
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
            if ( number ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDDeviceUsageKey), number);
                CFRelease(number);
            }
        }
        else if ( !strcmp("--usagepage", argv[argi]) && (argi+1) < argc) {
            long value = atol(argv[++argi]);
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
            if ( number ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDDeviceUsagePageKey), number);
                CFRelease(number);
            }
        }
        else if ( !strcmp("--vid", argv[argi]) && (argi+1) < argc) {
            long value = atol(argv[++argi]);
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
            if ( number ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDVendorIDKey), number);
                CFRelease(number);
            }
        }
        else if ( !strcmp("--pid", argv[argi]) && (argi+1) < argc) {
            long value = atol(argv[++argi]);
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
            if ( number ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDProductIDKey), number);
                CFRelease(number);
            }
        }
        else if ( !strcmp("--transport", argv[argi]) && (argi+1) < argc) {
            CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, argv[++argi], CFStringGetSystemEncoding());
            if ( string ) {
                if ( !matching )
                    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(matching, CFSTR(kIOHIDTransportKey), string);
                CFRelease(string);
            }
        }
        else if ( !strcmp("--multicast", argv[argi])) {
            useBonjour = NO;
        }
        else if ( !strcmp("--interface", argv[argi]) && (argi+1) < argc) {
            CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, argv[++argi], CFStringGetSystemEncoding());
            if ( string ) {
                interface = string;
            }
        }
        else {
            printHelp();
            return 0;
        }
    }

    @autoreleasepool {
        DeviceClient * client = [[DeviceClient alloc] initWithMatchingDictionary: (__bridge NSDictionary *)(matching) withBonjour:useBonjour withInterface: CFBridgingRelease(interface)];
        
        [[NSRunLoop currentRunLoop] run];
        
    }
    return 0;
}
