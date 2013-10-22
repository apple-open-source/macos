/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDUserDevice.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <AssertMacros.h>
#include "IOHIDReportDescriptorParser.h"

typedef struct GenericLEDKeyboardDescriptor {
    //05 01: Usage Page (Generic Desktop)
    UInt8 devUsagePageOp;
    UInt8 devUsagePageNum;
    //09 06: Usage (Keyboard)
    UInt8 devUsageOp;
    UInt8 devUsageNum;
    //A1 01: Collection (Application)
    UInt8 appCollectionOp;
    UInt8 appCollectionNum;
    //05 07:    Usage Page (Key Codes)
    UInt8 modUsagePageOp;
    UInt8 modUsagePageNum;
    //19 e0:    Usage Minimum...... (224) 
    UInt8 modUsageMinOp;
    UInt8 modUsageMinNum;
    //29 e7:    Usage Maximum...... (231) 
    UInt8 modUsageMaxOp;
    UInt8 modUsageMaxNum;
    //15 00:    Logical Minimum.... (0) 
    UInt8 modLogMinOp;
    UInt8 modLogMinNum;
    //25 01:    Logical Maximum.... (1) 
    UInt8 modLogMaxOp;
    UInt8 modLogMaxNum;
    //95 08:    Report Count....... (8) 
    UInt8 modRptCountOp;
    UInt8 modRptCountNum;
    //75 01:    Report Size........ (1) 
    UInt8 modRptSizeOp;
    UInt8 modRptSizeNum;
    //81 02:    Input (Data)
    UInt8 modInputOp;
    UInt8 modInputNum;

    //95 01:    Report Count....... (1) 
    UInt8 rsrvCountOp;
    UInt8 rsrvCountNum;
    //75 08:    Report Size........ (8) 
    UInt8 rsrvSizeOp;
    UInt8 rsrvSizeNum;
    //81 01:    Input (Constant)
    UInt8 rsrvInputOp;
    UInt8 rsrvInputNum;


    //95 02:    Report Count....... (2) 
    UInt8 ledRptCountOp;
    UInt8 ledRptCountNum;
    //75 01:    Report Size........ (1) 
    UInt8 ledRptSizeOp;
    UInt8 ledRptSizeNum; 
    //05 08:    Usage Page (LEDs)
    UInt8 ledUsagePageOp;
    UInt8 ledUsagePageNum;
    //19 01:    Usage Minimum...... (1) 
    UInt8 ledUsageMinOp;
    UInt8 ledUsageMinNum;
    //29 02:    Usage Maximum...... (2) 
    UInt8 ledUsageMaxOp;
    UInt8 ledUsageMaxNum;
    //91 02:    Output (Data)
    UInt8 ledInputOp;
    UInt8 ledInputNum;
    
    //95 01:    Report Count....... (1) 
    UInt8 fillRptCountOp;
    UInt8 fillRptCountNum;
    //75 03:    Report Size........ (3) 
    UInt8 fillRptSizeOp;
    UInt8 fillRptSizeNum;
    //91 01:    Output (Constant)
    UInt8 fillInputOp;
    UInt8 fillInputNum;


    //95 06:    Report Count....... (6) 
    UInt8 keyRptCountOp;
    UInt8 keyRptCountNum;
    //75 08:    Report Size........ (8) 
    UInt8 keyRptSizeOp;
    UInt8 keyRptSizeNum;
    //15 00:    Logical Minimum.... (0) 
    UInt8 keyLogMinOp;
    UInt8 keyLogMinNum;
    //26 ff 00:    Logical Maximum.... (255) 
    UInt8 keyLogMaxOp;
    UInt16 keyLogMaxNum;
    //05 07:    Usage Page (Key Codes)
    UInt8 keyUsagePageOp;
    UInt8 keyUsagePageNum;
    //19 00:    Usage Minimum...... (0) 
    UInt8 keyUsageMinOp;
    UInt8 keyUsageMinNum;
    //29 ff:    Usage Maximum...... (255) 
    UInt8 keyUsageMaxOp;
    UInt8 keyUsageMaxNum;
    //81 00:    Input (Array)
    UInt8 keyInputOp;
    UInt8 keyInputNum;

    //C0:    End Collection 
    UInt8 appCollectionEnd;
} GenericLEDKeyboardDescriptor;

typedef struct GenericKeyboardRpt {
    UInt8 modifiers;
    UInt8 reserved;
    UInt8 keys[6];
} GenericKeyboardRpt;

static UInt8 gGenLEDKeyboardDesc[] = {
    0x05, 0x01,
    0x09, 0x06,
    0xA1, 0x01,
    0x05, 0x07,
    0x19, 0xe0,
    0x29, 0xe7, 
    0x15, 0x00, 
    0x25, 0x01, 
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,
    0x95, 0x01, 
    0x75, 0x08, 
    0x81, 0x01,

    0x95, 0x02,
    0x75, 0x01,
    0x05, 0x08,
    0x19, 0x01,
    0x29, 0x02,
    0x91, 0x02,
    0x95, 0x01,
    0x75, 0x06,
    0x91, 0x01,

    0x95, 0x06,
    0x75, 0x08,
    0x15, 0x00,
    0x26, 0xff, 0x00,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0xff,
    0x81, 0x00,
    0xC0
};

static IOHIDUserDeviceRef   gDevice     = NULL;
static pthread_mutex_t      gMuxtex     = PTHREAD_MUTEX_INITIALIZER;

static void printReport(uint8_t * report, CFIndex reportLength, bool rcv)
{
    int index;

    printf("%s report: reportLength=%ld: ", rcv ? "Received" : "Dispatching", reportLength);
    for (index=0; index<reportLength; index++)
        printf("%02x ", report[index]);
    printf("\n");
}

#define kKeyboardInterval 0.008

static void dispatchKeyboardEvent(char c)
{
    GenericKeyboardRpt report;
    static CFAbsoluteTime sDeadline = 0;
    CFAbsoluteTime delta;
    
    bzero(&report, sizeof(report));
    if ( c < 'a' || c > 'z' )
        return;

    printf("dispatching keyboard event for '%c'\n", c);
        
    pthread_mutex_lock(&gMuxtex);
    
    delta = sDeadline - CFAbsoluteTimeGetCurrent();
    if ( delta > 0 )
        usleep(delta*1000000);

    report.keys[0] = 4 + c - 97;
    printReport((uint8_t*)&report, sizeof(report), 0);
    IOHIDUserDeviceHandleReport(gDevice, (uint8_t*)&report, sizeof(report));
    
    usleep(kKeyboardInterval*1000000);
    
    report.keys[0] = 0;
    printReport((uint8_t*)&report, sizeof(report), 0);
    IOHIDUserDeviceHandleReport(gDevice, (uint8_t*)&report, sizeof(report));

    sDeadline = CFAbsoluteTimeGetCurrent() + kKeyboardInterval;

    pthread_mutex_unlock(&gMuxtex);

}

static void * getKeyboardCharThread(void *arg)
{
    printf("This virtual keyboard supports dispatching typed characters within the range of 'a' - 'z'\n");

    while (1) {
        dispatchKeyboardEvent(getchar());
    }
    return arg;
}

static void * getReportCharThread(void *arg)
{
    char str[256];
    printf("Please enter report data: 00 11 22 33 ...\n");
    
    while (1) {
        bzero(str, sizeof(str));
        
        if ( fgets(str, sizeof(str), stdin) ) {
            
            CFStringRef rawString = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, (uint8_t *)str, sizeof(str), kCFStringEncodingMacRoman, false, kCFAllocatorNull);
            
            if ( rawString ) {
                CFArrayRef rawArray = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, rawString, CFSTR("\n"));
                
                if ( rawArray ) {
                    if ( CFArrayGetCount(rawArray) > 1 ) {
                        CFArrayRef array = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, CFArrayGetValueAtIndex(rawArray, 0), CFSTR(" "));
                        
                        if ( array ) {

                            uint8_t     report[64] = {};
                            uint32_t    reportIndex = 0;
                            
                            for ( int index=0; index<CFArrayGetCount(array) && reportIndex<sizeof(report); index++) {
                                CFStringRef substring = (CFStringRef)CFArrayGetValueAtIndex(array, index);
                                const char * substr;
                                
                                if ( !substring )
                                    continue;
                                
                                substr = CFStringGetCStringPtr(substring, kCFStringEncodingMacRoman);
                                if ( !substr )
                                    continue;
                                
                                report[reportIndex++] = strtoul(substr, NULL, 16);
                            }
                    
                            pthread_mutex_lock(&gMuxtex);
                            printReport(report, reportIndex, 0);
                            IOHIDUserDeviceHandleReport(gDevice, report, reportIndex);
                            pthread_mutex_unlock(&gMuxtex);
                            
                            CFRelease(array);
                        }
                    }
                 
                    CFRelease(rawArray);
                }
                
                CFRelease(rawString);
            }
        }
        
    }
    return arg;
}


static IOReturn setReportCallback(void * refcon __unused, IOHIDReportType type __unused, uint32_t reportID __unused, uint8_t * report, CFIndex reportLength)
{
    printReport(report, reportLength, 1);
    return kIOReturnSuccess;
}

typedef void * (*UserInputCallback)(void * refcon);


static void startDevice(CFMutableDictionaryRef properties, const uint8_t * descriptor, uint32_t descriptorLength, UserInputCallback userInputCallback, IOHIDUserDeviceReportCallback outputReportCallback)
{
    CFDataRef descriptorData  = NULL;
    
    descriptorData = CFDataCreate(kCFAllocatorDefault, descriptor, descriptorLength);
    require(descriptorData, finish);
    
    require(properties, finish);
    
    CFDictionarySetValue(properties, CFSTR(kIOHIDReportDescriptorKey), descriptorData);
    
    gDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, properties);
    require(gDevice, finish);
    
    IOHIDUserDeviceScheduleWithRunLoop(gDevice, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    
    if ( outputReportCallback )
        IOHIDUserDeviceRegisterSetReportCallback(gDevice, outputReportCallback, NULL);
    
    if ( userInputCallback ) {
        pthread_t tid;
        pthread_attr_t attr;
        
        assert(!pthread_attr_init(&attr));
        assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
        assert(!pthread_create( &tid, &attr, userInputCallback, NULL));
        assert(!pthread_attr_destroy(&attr));
    }
    
    CFRunLoopRun();
    
finish:
    if ( gDevice )
        CFRelease(gDevice);
    
    if ( descriptorData )
        CFRelease(descriptorData);
}

static void printHelp()
{
    printf("\n");
    printf("hidUserDeviceTest usage:\n\n");
    printf("\t-d    <descriptor data: 00 11 22...>\t: create device with descriptor data\n");
    printf("\t-p    Parse descriptor data\n");
    printf("\t-k    Create generic keyboard device\n");
    printf("\t--vid <vendor id>\n");
    printf("\t--pid <product id>\n");
    printf("\n");
}

int main (int argc, const char * argv[])
{
    bool                            handled                 = false;
    bool                            parse                   = false;
    uint8_t *                       data                    = NULL;
    uint32_t                        dataSize                = 0;
    uint32_t                        dataIndex               = 0;
    UserInputCallback               userInputCallback       = NULL;
    IOHIDUserDeviceReportCallback   outputReportCallback    = NULL;
    CFMutableDictionaryRef          properties              = NULL;

    properties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    for ( int argi=1; argi<argc; argi++ ) {
        // command
        if ( argv[argi][0] == '-' ) {
            if ( !strcmp("-k", argv[argi] ) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gGenLEDKeyboardDesc);
                data                    = malloc(dataSize);
                userInputCallback       = getKeyboardCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                
                bcopy(gGenLEDKeyboardDesc, data, dataSize);
                dataIndex = dataSize;
            }
            else if ( !strcmp("-d", argv[argi]) ) {
                if ( !data ) {
                    dataSize                = argc-argi+1;
                    data                    = malloc(dataSize);
                }
                userInputCallback       = getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
            }
            else if ( !strcmp("-p", argv[argi]) ) {
                parse = true;
                
                if ( !data ) {
                    dataSize    = argc-argi+1;
                    data        = malloc(argc-argi+1);
                }
            }
            else if ( !strcmp("--vid", argv[argi]) && (argi+1) < argc) {
                int value = strtol(argv[++argi], NULL, 10);
                CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
                if ( number ) {
                    CFDictionarySetValue(properties, CFSTR(kIOHIDVendorIDKey), number);
                    CFRelease(number);
                }
            }
            else if ( !strcmp("--pid", argv[argi]) && (argi+1) < argc) {
                int value = strtol(argv[++argi], NULL, 10);
                CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
                if ( number ) {
                    CFDictionarySetValue(properties, CFSTR(kIOHIDProductIDKey), number);
                    CFRelease(number);
                }
            }
        }
        // data
        else if ( data && dataIndex < dataSize ) {
            data[dataIndex++] = strtoul(argv[argi], NULL, 16);
        }
        
    }

    if ( data ) {
        if ( parse )
            PrintHIDReport(data, dataSize);
        
        if ( handled ) {
            startDevice(properties, data, dataIndex, userInputCallback, outputReportCallback);
        }
        
        free(data);
        
    } else {
        printHelp();
    }
    
    if ( properties )
        CFRelease(properties);
    
    return 0;
}
