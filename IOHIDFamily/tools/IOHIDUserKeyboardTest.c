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
#include <pthread.h>
#include <AssertMacros.h>
#include <stdio.h>

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

static IOHIDUserDeviceRef   gKeyboard   = NULL;
static pthread_mutex_t      gMuxtex     = PTHREAD_MUTEX_INITIALIZER;

static void printReport(uint8_t * report, CFIndex reportLength, bool rcv)
{
    int index;

    printf("%s report: reportLength=%ld: ", rcv ? "Received" : "Dispatching", reportLength);
    for (index=0; index<reportLength; index++)
        printf("%02x ", report[index]);
    printf("\n");
}

static void dispatchKeyboardEvent(char c)
{
    GenericKeyboardRpt  report;
    IOReturn            ret;
    
    bzero(&report, sizeof(report));
    if ( c < 'a' || c > 'z' )
        return;

    printf("dispatching keyboard event for '%c'\n", c);
        
    pthread_mutex_lock(&gMuxtex);
    
    report.keys[0] = 4 + c - 97;
    printReport((uint8_t*)&report, sizeof(report), 0);
    ret = IOHIDUserDeviceHandleReport(gKeyboard, (uint8_t*)&report, sizeof(report));

    report.keys[0] = 0;
    printReport((uint8_t*)&report, sizeof(report), 0);
    ret = IOHIDUserDeviceHandleReport(gKeyboard, (uint8_t*)&report, sizeof(report));

    pthread_mutex_unlock(&gMuxtex);

}

static void * getcThread(void *arg)
{
    printf("This virtual keyboard supports dispatching typed characters within the range of 'a' - 'z'\n");

    while (1) {
        dispatchKeyboardEvent(getchar());
    }
    return arg;
}

static IOReturn setReportCallback(void * refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength)
{
    printReport(report, reportLength, 1);
    return kIOReturnSuccess;
}


int main (int argc, const char * argv[]) 
{
    CFDataRef           descriptor  = NULL;
    CFStringRef         key         = CFSTR(kIOHIDReportDescriptorKey);
    CFDictionaryRef     properties  = NULL;
    
    descriptor = CFDataCreate(kCFAllocatorDefault, gGenLEDKeyboardDesc, sizeof(gGenLEDKeyboardDesc));
    require(descriptor, finish);
    
    properties = CFDictionaryCreate(kCFAllocatorDefault, (const void **)&key, (const void **)&descriptor, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(properties, finish);
    
    gKeyboard = IOHIDUserDeviceCreate(kCFAllocatorDefault, properties);
    require(gKeyboard, finish);
    
    IOHIDUserDeviceScheduleWithRunLoop(gKeyboard, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDUserDeviceRegisterSetReportCallback(gKeyboard, setReportCallback, NULL);

	pthread_t tid;
    pthread_attr_t attr;
    
    assert(!pthread_attr_init(&attr));
    assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
    assert(!pthread_create( &tid, &attr, getcThread, NULL));
    assert(!pthread_attr_destroy(&attr));
    
    CFRunLoopRun();
    
finish:
    if ( gKeyboard )
        CFRelease(gKeyboard);
        
    if ( properties )
        CFRelease(properties);
        
    if ( descriptor )
        CFRelease(descriptor);
        
    return 0;
}
