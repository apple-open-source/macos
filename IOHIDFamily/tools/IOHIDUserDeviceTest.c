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
#include <IOKit/hid/IOHIDUsageTables.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <AssertMacros.h>
#include "IOHIDReportDescriptorParser.h"


typedef struct __attribute__((packed)) {
    UInt8 modifiers;
    UInt8 reserved;
    UInt8 keys[6];
} KeyboardInputReport;

typedef struct __attribute__((packed)) {
    UInt8 leds;
} KeyboardOutputReport;

static UInt8 gKeyboardDesc[] = {
    0x05, 0x01,                               // Usage Page (Generic Desktop)
    0x09, 0x06,                               // Usage (Keyboard)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x07,                               //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,                               //   Usage Minimum........... (224)
    0x29, 0xE7,                               //   Usage Maximum........... (231)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x08,                               //   Report Count............ (8)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x95, 0x01,                               //   Report Count............ (1)
    0x75, 0x08,                               //   Report Size............. (8)
    0x81, 0x01,                               //   Input...................(Constant)
    0x95, 0x02,                               //   Report Count............ (2)
    0x75, 0x01,                               //   Report Size............. (1)
    0x05, 0x08,                               //   Usage Page (LED)
    0x19, 0x01,                               //   Usage Minimum........... (1)
    0x29, 0x02,                               //   Usage Maximum........... (2)
    0x91, 0x02,                               //   Output..................(Data, Variable, Absolute)
    0x95, 0x01,                               //   Report Count............ (1)
    0x75, 0x06,                               //   Report Size............. (6)
    0x91, 0x01,                               //   Output..................(Constant)
    0x95, 0x06,                               //   Report Count............ (6)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x05, 0x07,                               //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x29, 0xFF,                               //   Usage Maximum........... (-1)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0xC0,                                     // End Collection  
};

static UInt8 gMediaButtonsDesc[] = {
    0x05, 0x0C,                               // Usage Page (Consumer)
    0x09, 0x01,                               // Usage 1 (0x1)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x09, 0xB5,                               //   Usage 181 (0xb5)
    0x09, 0xB6,                               //   Usage 182 (0xb6)
    0x09, 0xCD,                               //   Usage 205 (0xcd)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x04,                               //   Report Count............ (3)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x75, 0x04,                               //   Report Size............. (5)
    0x95, 0x01,                               //   Report Count............ (1)
    0x81, 0x01,                               //   Input...................(Constant)
    0xC0,                                     // End Collection  
};

static UInt8 gTelephonyButtonsDesc[] = {
    0x05, 0x0B,                               // Usage Page (Telephony Device)
    0x09, 0x01,                               // Usage 1 (0x1)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x0B,                               //   Usage Page (Telephony Device)
    0x09, 0x21,                               //   Usage 33 (0x21)
    0x09, 0xB0,                               //   Usage 176 (0xb0)
    0x09, 0xB1,                               //   Usage 177 (0xb1)
    0x09, 0xB2,                               //   Usage 178 (0xb2)
    0x09, 0xB3,                               //   Usage 179 (0xb3)
    0x09, 0xB4,                               //   Usage 180 (0xb4)
    0x09, 0xB5,                               //   Usage 181 (0xb5)
    0x09, 0xB6,                               //   Usage 182 (0xb6)
    0x09, 0xB7,                               //   Usage 183 (0xb7)
    0x09, 0xB8,                               //   Usage 184 (0xb8)
    0x09, 0xB9,                               //   Usage 185 (0xb9)
    0x09, 0xBA,                               //   Usage 186 (0xba)
    0x09, 0xBB,                               //   Usage 187 (0xbb)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x0D,                               //   Report Count............ (13)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x75, 0x03,                               //   Report Size............. (3)
    0x95, 0x01,                               //   Report Count............ (1)
    0x81, 0x01,                               //   Input...................(Constant)
    0xC0,                                     // End Collection  
};

typedef struct __attribute__((packed)) {
    UInt16 usage;
} UnicodeReport;

static UInt8 gUnicodeDesc[] = {
    0x05, 0x10,                               // Usage Page (Unicode)
    0x09, 0x00,                               // Usage 0 (0x0)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x10,                               //   Usage Page (Unicode)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x2A, 0xFF, 0xFF,                         //   Usage Maximum........... (65535)
    0x75, 0x10,                               //   Report Size............. (16)
    0x95, 0x01,                               //   Report Count............ (1)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0xC0,                                     // End Collection
};

typedef struct __attribute__((packed)) {
    uint8_t  state;
    uint16_t x;
    uint16_t y;
} SingleTouchDigitizerReport;

static UInt8 gSingleTouchDigitizerDesc[] = {
    0x05, 0x0D,                               // Usage Page (Digitizer)
    0x09, 0x04,                               // Usage (Touch Screen)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x0D,                               //   Usage Page (Digitizer)
    0x09, 0x22,                               //   Usage (Finger)
    0xA1, 0x02,                               //   Collection (Logical)
    0x05, 0x0D,                               //     Usage Page (Digitizer)
    0x09, 0x33,                               //     Usage (Touch)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x25, 0x01,                               //     Logical Maximum......... (1)
    0x75, 0x01,                               //     Report Size............. (1)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x75, 0x07,                               //     Report Size............. (7)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x01,                               //     Input...................(Constant)
    0x05, 0x01,                               //     Usage Page (Generic Desktop)
    0x09, 0x30,                               //     Usage (X)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x26, 0x20, 0x03,                         //     Logical Maximum......... (800)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x09, 0x31,                               //     Usage (Y)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x26, 0xE0, 0x01,                         //     Logical Maximum......... (480)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0xC0,                                     //   End Collection
    0xC0,                                     // End Collection  
};


typedef struct __attribute__((packed)) {
    struct {
        uint8_t  state;
        uint8_t  id;
        uint16_t x;
        uint16_t y;
    } tranducers[2];
} MultiTouchDigitizerReport;

static UInt8 gMultiTouchDigitizerDesc[] = {
    0x05, 0x0D,                               // Usage Page (Digitizer)
    0x09, 0x04,                               // Usage (Touch Screen)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x0D,                               //   Usage Page (Digitizer)
    0x09, 0x22,                               //   Usage (Finger)
    0xA1, 0x02,                               //   Collection (Logical)
    0x05, 0x0D,                               //     Usage Page (Digitizer)
    0x09, 0x38,                               //     Usage (Transducer Index)
    0x75, 0x08,                               //     Report Size............. (8)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x09, 0x33,                               //     Usage (Touch)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x25, 0x01,                               //     Logical Maximum......... (1)
    0x75, 0x01,                               //     Report Size............. (1)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x75, 0x07,                               //     Report Size............. (7)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x01,                               //     Input...................(Constant)
    0x05, 0x01,                               //     Usage Page (Generic Desktop)
    0x09, 0x30,                               //     Usage (X)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x26, 0x20, 0x03,                         //     Logical Maximum......... (800)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x09, 0x31,                               //     Usage (Y)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x26, 0xE0, 0x01,                         //     Logical Maximum......... (480)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0xC0,                                     //   End Collection
    0x05, 0x0D,                               //   Usage Page (Digitizer)
    0x09, 0x22,                               //   Usage (Finger)
    0xA1, 0x02,                               //   Collection (Logical)
    0x05, 0x0D,                               //     Usage Page (Digitizer)
    0x09, 0x38,                               //     Usage (Transducer Index)
    0x75, 0x08,                               //     Report Size............. (8)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x09, 0x33,                               //     Usage (Touch)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x25, 0x01,                               //     Logical Maximum......... (1)
    0x75, 0x01,                               //     Report Size............. (1)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x75, 0x07,                               //     Report Size............. (7)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x01,                               //     Input...................(Constant)
    0x05, 0x01,                               //     Usage Page (Generic Desktop)
    0x09, 0x30,                               //     Usage (X)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x26, 0x20, 0x03,                         //     Logical Maximum......... (800)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x09, 0x31,                               //     Usage (Y)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x26, 0xE0, 0x01,                         //     Logical Maximum......... (480)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute) 
    0xC0,                                     //   End Collection  
    0xC0,                                     // End Collection  
};

typedef struct __attribute__((packed)) {
    SingleTouchDigitizerReport touch;
    struct {
        UInt8 string[4];
        UInt8 length;
        UInt8 encoding;
        UInt8 quality;
    } chars[2];
    
} DigitizerUnicodeInputReport;

typedef struct __attribute__((packed)) {
    UInt8 enable;
} DigitizerUnicodeOutputReport;

static UInt8 gDigitizerUnicodeDesc[] = {
    0x05, 0x0D,                               // Usage Page (Digitizer)
    0x09, 0x05,                               // Usage (Touch Pad)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x0D,                               //   Usage Page (Digitizer)
    0x09, 0x22,                               //   Usage (Finger)
    0xA1, 0x02,                               //   Collection (Logical)
    0x05, 0x0D,                               //     Usage Page (Digitizer)
    0x09, 0x33,                               //     Usage (Touch)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x25, 0x01,                               //     Logical Maximum......... (1)
    0x75, 0x01,                               //     Report Size............. (1)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x75, 0x07,                               //     Report Size............. (7)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x01,                               //     Input...................(Constant)
    0x05, 0x01,                               //     Usage Page (Generic Desktop)
    0x09, 0x30,                               //     Usage (X)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x26, 0x00, 0x04,                         //     Logical Maximum......... (1024)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x09, 0x31,                               //     Usage (Y)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x26, 0x00, 0x04,                         //     Logical Maximum......... (1024)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0xC0,                                     //   End Collection
    0x05, 0x0D,                               //   Usage Page (Digitizer)
    0x09, 0x23,                               //   Usage (Device Settings)
    0xA1, 0x02,                               //   Collection (Logical)
    0x05, 0x0D,                               //     Usage Page (Digitizer)
    0x09, 0x60,                               //     Usage (Gesture Character Enable)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x25, 0x01,                               //     Logical Maximum......... (1)
    0x95, 0x01,                               //     Report Count............ (1)
    0x75, 0x01,                               //     Report Size............. (1)
    0xB1, 0x02,                               //     Feature.................(Data, Variable, Absolute)
    0x95, 0x01,                               //     Report Count............ (1)
    0x75, 0x07,                               //     Report Size............. (7)
    0xB1, 0x01,                               //     Feature.................(Constant)
    0xC0,                                     //   End Collection
    0x05, 0x0D,                               //   Usage Page (Digitizer)
    0x09, 0x24,                               //   Usage (Gesture Character)
    0xA1, 0x02,                               //   Collection (Logical)
    0x05, 0x0D,                               //     Usage Page (Digitizer)
    0x09, 0x63,                               //     Usage (Gesture Character Data)
    0x75, 0x20,                               //     Report Size............. (32)
    0x95, 0x01,                               //     Report Count............ (1)
    0x82, 0x02, 0x01,                         //     Input...................(Data, Variable, Absolute, Buffered bytes)
    0x09, 0x62,                               //     Usage (Gesture Character Data Length)
    0x09, 0x66,                               //     Usage (Gesture Character Encoding UTF16 Little Endian)
    0x75, 0x08,                               //     Report Size............. (8)
    0x95, 0x02,                               //     Report Count............ (2)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x09, 0x61,                               //     Usage (Gesture Character Quality)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x25, 0x64,                               //     Logical Maximum......... (100)
    0x75, 0x08,                               //     Report Size............. (8)
    0x95, 0x01,                               //     Report Count............ (1)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0xC0,                                     //   End Collection
    0x05, 0x0D,                               //   Usage Page (Digitizer)
    0x09, 0x24,                               //   Usage (Gesture Character)
    0xA1, 0x02,                               //   Collection (Logical)
    0x05, 0x0D,                               //     Usage Page (Digitizer)
    0x09, 0x63,                               //     Usage (Gesture Character Data)
    0x75, 0x20,                               //     Report Size............. (32)
    0x95, 0x01,                               //     Report Count............ (1)
    0x82, 0x02, 0x01,                         //     Input...................(Data, Variable, Absolute, Buffered bytes)
    0x09, 0x62,                               //     Usage (Gesture Character Data Length)
    0x09, 0x66,                               //     Usage (Gesture Character Encoding UTF16 Little Endian)
    0x75, 0x08,                               //     Report Size............. (8)
    0x95, 0x02,                               //     Report Count............ (2)
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute)
    0x09, 0x61,                               //     Usage (Gesture Character Quality)
    0x15, 0x00,                               //     Logical Minimum......... (0)
    0x25, 0x64,                               //     Logical Maximum......... (100)
    0x75, 0x08,                               //     Report Size............. (8)
    0x95, 0x01,                               //     Report Count............ (1)  
    0x81, 0x02,                               //     Input...................(Data, Variable, Absolute) 
    0xC0,                                     //   End Collection  
    0xC0,                                     // End Collection  
};

typedef struct __attribute__((packed)) {
    uint8_t     buttonMask;
    uint8_t     x;
    uint8_t     y;
    uint8_t     scroll;
    
} MultiAxisDeviceInputReport;

static UInt8 gMultiAxisInputDesc[] = {
    0x05, 0x01,                               // Usage Page (Generic Desktop)
    0x09, 0x08,                               // Usage (MultiAxisController)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x09,                               //   Usage Page (Button)
    0x09, 0x01,                               //   Usage 1 (0x1)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x01,                               //   Report Count............ (1)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x0A, 0x23, 0x02,                         //   Usage 547 (0x223)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x01,                               //   Report Count............ (1)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x0A, 0x24, 0x02,                         //   Usage 548 (0x224)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x01,                               //   Report Count............ (1)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x75, 0x05,                               //   Report Size............. (5)
    0x95, 0x01,                               //   Report Count............ (1)
    0x81, 0x01,                               //   Input...................(Constant)
    0x05, 0x01,                               //   Usage Page (Generic Desktop)
    0x09, 0x30,                               //   Usage (X)
    0x09, 0x31,                               //   Usage (Y)
    0x15, 0x81,                               //   Logical Minimum......... (-127)
    0x25, 0x7F,                               //   Logical Maximum......... (127)
    0x75, 0x08,                               //   Report Size............. (8)
    0x95, 0x02,                               //   Report Count............ (2)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0xC0,                                     // End Collection  
};


static IOHIDUserDeviceRef   gDevice             = NULL;
static pthread_mutex_t      gMuxtex             = PTHREAD_MUTEX_INITIALIZER;
static uint32_t             gGetReportDelay     = 0;

static void printReport(uint8_t * report, CFIndex reportLength, bool rcv)
{
    int index;
    
    printf("%s report: reportLength=%ld: ", rcv ? "Received" : "Dispatched", reportLength);
    for (index=0; index<reportLength; index++)
        printf("%02x ", report[index]);
    printf("\n");
}

#define kKeyboardInterval 0.008

static void dispatchKeyboardEvent(char c)
{
    KeyboardInputReport report;
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

static void dispatchUnicodeEvent(char c)
{
    UnicodeReport report;
    static CFAbsoluteTime sDeadline = 0;
    CFAbsoluteTime delta;
    
    bzero(&report, sizeof(report));
    if ( c < 'a' || c > 'z' )
        return;
    
    printf("dispatching unicode event for '%c'\n", c);
    
    pthread_mutex_lock(&gMuxtex);
    
    delta = sDeadline - CFAbsoluteTimeGetCurrent();
    if ( delta > 0 )
        usleep(delta*1000000);
    
    report.usage = c;
    
    OSSwapHostToLittleInt16(report.usage);

    printReport((uint8_t*)&report, sizeof(report), 0);
    IOHIDUserDeviceHandleReport(gDevice, (uint8_t*)&report, sizeof(report));
    
    sDeadline = CFAbsoluteTimeGetCurrent() + kKeyboardInterval;
    
    pthread_mutex_unlock(&gMuxtex);
    
}

static void * getUnicodeCharThread(void *arg)
{
    printf("This virtual unicode supports dispatching typed character within the range of 'a' - 'z'\n");
    
    while (1) {
        dispatchUnicodeEvent(getchar());
    }
    return arg;
}

static void dispatchDigitizerUnicodeEvent(char c)
{
    DigitizerUnicodeInputReport report;
    static CFAbsoluteTime sDeadline = 0;
    CFAbsoluteTime delta;
    
    if ( c < 'a' || c > 'z' )
        return;
    
    printf("dispatching digitizer unicode event for '%c'\n", c);
    
    bzero(&report, sizeof(report));
    // first candidate
    report.chars[0].encoding = 1;
    report.chars[0].quality = 50;
    report.chars[0].length = 2;
    report.chars[0].string[0] = c;
    
    // second candidate
    report.chars[1].encoding = 1;
    report.chars[1].quality = 50;
    report.chars[1].length = 2;
    report.chars[1].string[0] = c-32;

    pthread_mutex_lock(&gMuxtex);
    
    delta = sDeadline - CFAbsoluteTimeGetCurrent();
    if ( delta > 0 )
        usleep(delta*1000000);
    
    printReport((uint8_t*)&report, sizeof(report), 0);
    IOHIDUserDeviceHandleReport(gDevice, (uint8_t*)&report, sizeof(report));
    
    sDeadline = CFAbsoluteTimeGetCurrent() + kKeyboardInterval;

    bzero(&report, sizeof(report));
    printReport((uint8_t*)&report, sizeof(report), 0);
    IOHIDUserDeviceHandleReport(gDevice, (uint8_t*)&report, sizeof(report));

    pthread_mutex_unlock(&gMuxtex);
    
}

static void * getDigitizerUnicodeCharThread(void *arg)
{
    printf("This virtual digitizer unicode supports dispatching typed character within the range of 'a' - 'z'\n");
    
    while (1) {
        dispatchDigitizerUnicodeEvent(getchar());
    }
    return arg;
}

static uint8_t  __report[256]     = {};
IOReturn getReportCallback(void * refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength);

IOReturn getReportCallback(void * refcon __unused, IOHIDReportType type __unused, uint32_t reportID __unused, uint8_t * report, CFIndex reportLength)
{
    if ( !report || !reportLength )
        return kIOReturnBadArgument;
    
    if (gGetReportDelay)
        usleep(gGetReportDelay);
        
    bcopy(__report, report, reportLength);
    
    return kIOReturnSuccess;
}

static void * getReportCharThread(void *arg)
{
    printf("Please enter report data: 00 11 22 33 ...\n");
    
    while (1) {
        char str[1024];
        bzero(str, sizeof(str));
        
        if ( fgets(str, sizeof(str), stdin) ) {
            
            CFStringRef rawString = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, (uint8_t *)str, sizeof(str), kCFStringEncodingMacRoman, false, kCFAllocatorNull);
            
            if ( rawString ) {
                CFArrayRef rawArray = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, rawString, CFSTR("\n"));
                
                if ( rawArray ) {
                    if ( CFArrayGetCount(rawArray) > 1 ) {
                        CFArrayRef array = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, CFArrayGetValueAtIndex(rawArray, 0), CFSTR(" "));
                        
                        if ( array ) {
                            
                            uint32_t reportIndex = 0;
                            
                            bzero(__report, sizeof(__report));
                            
                            for ( int index=0; index<CFArrayGetCount(array) && reportIndex<sizeof(__report); index++) {
                                CFStringRef substring = (CFStringRef)CFArrayGetValueAtIndex(array, index);
                                const char * substr;
                                
                                if ( !substring )
                                    continue;
                                
                                substr = CFStringGetCStringPtr(substring, kCFStringEncodingMacRoman);
                                if ( !substr )
                                    continue;
                                
                                __report[reportIndex++] = strtoul(substr, NULL, 16);
                            }
                            
                            pthread_mutex_lock(&gMuxtex);
                            printReport(__report, reportIndex, 0);
                            IOHIDUserDeviceHandleReport(gDevice, __report, reportIndex);
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


static void startDevice(CFMutableDictionaryRef properties, uint32_t reportinterval, const uint8_t * descriptor, uint32_t descriptorLength, UserInputCallback userInputCallback, IOHIDUserDeviceReportCallback outputReportCallback, IOHIDUserDeviceReportCallback inputReportCallback)
{
    CFDataRef   descriptorData  = NULL;
    CFNumberRef timeoutNumber   = NULL;
    CFNumberRef intervalNumber  = NULL;
    uint32_t    value           = 5000000;
    
    descriptorData = CFDataCreate(kCFAllocatorDefault, descriptor, descriptorLength);
    require(descriptorData, finish);
    
    timeoutNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    require(timeoutNumber, finish);
    
    intervalNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reportinterval);
    require(intervalNumber, finish);
    
    require(properties, finish);
    
    CFDictionarySetValue(properties, CFSTR(kIOHIDReportDescriptorKey), descriptorData);
    CFDictionarySetValue(properties, CFSTR(kIOHIDRequestTimeoutKey), timeoutNumber);
    CFDictionarySetValue(properties, CFSTR(kIOHIDReportIntervalKey), intervalNumber);
    
    gDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, properties);
    require(gDevice, finish);
    
    IOHIDUserDeviceScheduleWithRunLoop(gDevice, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    
    if ( outputReportCallback )
        IOHIDUserDeviceRegisterSetReportCallback(gDevice, outputReportCallback, NULL);
    
    if ( inputReportCallback )
        IOHIDUserDeviceRegisterGetReportCallback(gDevice, inputReportCallback, NULL);
    
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
    
    if ( timeoutNumber )
        CFRelease(timeoutNumber);
    if ( intervalNumber )
        CFRelease(intervalNumber);
}

static void printHelp()
{
    printf("\n");
    printf("hidUserDeviceTest usage:\n\n");
    printf("\t-d    <descriptor data: 00 11 22...>\t: create device with descriptor data\n");
    printf("\t-p    Parse descriptor data\n");
    printf("\t-k    Create generic keyboard device with user generated char input\n");
    printf("\t-kr   Create generic keyboard device with user generated reports\n");
    printf("\t--vid <vendor id>\n");
    printf("\t--pid <product id>\n");
    printf("\t--ri  <report interval us>\n");
    printf("\t--transport <transport string value>\n");
    printf("\t--rdelay <getReport delay in uS>");
    printf("\n");
}

int main (int argc, const char * argv[])
{
    bool                            handled                 = false;
    bool                            parse                   = false;
    bool                            dataString              = false;
    uint8_t *                       data                    = NULL;
    uint32_t                        dataSize                = 0;
    uint32_t                        dataIndex               = 0;
    uint32_t                        reportInterval          = 16000;
    UserInputCallback               userInputCallback       = NULL;
    IOHIDUserDeviceReportCallback   outputReportCallback    = NULL;
    IOHIDUserDeviceReportCallback   inputReportCallback     = getReportCallback;
    CFMutableDictionaryRef          properties              = NULL;
    
    properties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    for ( int argi=1; argi<argc; argi++ ) {
        // command
        if ( argv[argi][0] == '-' ) {
            if ( !strcmp("-k", argv[argi]) || !strcmp("-kr", argv[argi]) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gKeyboardDesc);
                data                    = malloc(dataSize);
                userInputCallback       = !strcmp("-k", argv[argi]) ? getKeyboardCharThread : getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                
                bcopy(gKeyboardDesc, data, dataSize);
                dataIndex = dataSize;
            }
            if ( !strcmp("-u", argv[argi]) || !strcmp("-ur", argv[argi]) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gUnicodeDesc);
                data                    = malloc(dataSize);
                userInputCallback       = !strcmp("-u", argv[argi]) ? getUnicodeCharThread : getReportCharThread;
                handled                 = true;
                
                bcopy(gUnicodeDesc, data, dataSize);
                dataIndex = dataSize;
            }
            if ( !strcmp("-mb", argv[argi]) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gMediaButtonsDesc);
                data                    = malloc(dataSize);
                userInputCallback       = getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                
                bcopy(gMediaButtonsDesc, data, dataSize);
                dataIndex = dataSize;
            }
            if ( !strcmp("-tb", argv[argi]) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gTelephonyButtonsDesc);
                data                    = malloc(dataSize);
                userInputCallback       = getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                
                bcopy(gTelephonyButtonsDesc, data, dataSize);
                dataIndex = dataSize;
            }
            if ( !strcmp("-std", argv[argi]) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gSingleTouchDigitizerDesc);
                data                    = malloc(dataSize);
                userInputCallback       = getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                
                bcopy(gSingleTouchDigitizerDesc, data, dataSize);
                dataIndex = dataSize;
            }
            if ( !strcmp("-mac", argv[argi]) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gMultiAxisInputDesc);
                data                    = malloc(dataSize);
                userInputCallback       = getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                
                bcopy(gMultiAxisInputDesc, data, dataSize);
                dataIndex = dataSize;
            }
            if ( !strcmp("-mtd", argv[argi]) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gMultiTouchDigitizerDesc);
                data                    = malloc(dataSize);
                userInputCallback       = getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                
                bcopy(gMultiTouchDigitizerDesc, data, dataSize);
                dataIndex = dataSize;
            }
            if ( !strcmp("-du", argv[argi]) || !strcmp("-dur", argv[argi]) ) {
                if ( data ) {
                    free(data);
                }
                
                dataSize                = sizeof(gDigitizerUnicodeDesc);
                data                    = malloc(dataSize);
                userInputCallback       = !strcmp("-du", argv[argi]) ? getDigitizerUnicodeCharThread : getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                
                bcopy(gDigitizerUnicodeDesc, data, dataSize);
                dataIndex = dataSize;
            }
            else if ( !strcmp("-d", argv[argi]) ) {
                if ( !data ) {
                    dataSize                = argc-argi+1;
                    data                    = malloc(dataSize);
                    bzero(data, dataSize);
                }
                userInputCallback       = getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
            }
            else if ( !strcmp("-ds", argv[argi]) ) {
                userInputCallback       = getReportCharThread;
                outputReportCallback    = setReportCallback;
                handled                 = true;
                dataString              = true;
                
                if ( data ) {
                    free(data);
                    data        = NULL;
                    dataSize    = 0;
                }
            }
            else if ( !strcmp("-p", argv[argi]) ) {
                parse = true;
                
                if ( !data ) {
                    dataSize    = argc-argi+1;
                    data        = malloc(argc-argi+1);
                }
            }
            else if ( !strcmp("--vid", argv[argi]) && (argi+1) < argc) {
                long value = strtol(argv[++argi], NULL, 10);
                CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
                if ( number ) {
                    CFDictionarySetValue(properties, CFSTR(kIOHIDVendorIDKey), number);
                    CFRelease(number);
                }
            }
            else if ( !strcmp("--pid", argv[argi]) && (argi+1) < argc) {
                long value = strtol(argv[++argi], NULL, 10);
                CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &value);
                if ( number ) {
                    CFDictionarySetValue(properties, CFSTR(kIOHIDProductIDKey), number);
                    CFRelease(number);
                }
            }
            else if ( !strcmp("--transport", argv[argi]) && (argi+1) < argc) {
                CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, argv[++argi], CFStringGetSystemEncoding());
                if ( string ) {
                    CFDictionarySetValue(properties, CFSTR(kIOHIDTransportKey), string);
                    CFRelease(string);
                }
            }
            else if ( !strcmp("--rdelay", argv[argi]) && (argi+1) < argc) {
                gGetReportDelay = (uint32_t)strtol(argv[++argi], NULL, 10);
            }
            else if ( !strcmp("--ri", argv[argi]) && (argi+1) < argc) {
                reportInterval = (uint32_t)strtol(argv[++argi], NULL, 10);
            }
        }
        // data
        else if ( !dataString && data && dataIndex < dataSize ) {
            data[dataIndex++] = strtoul(argv[argi], NULL, 16);
        }
        else if ( dataString && !data) {
            printf("parsing data string\n");
            uint32_t stringLength = (uint32_t)strlen(argv[argi]);
            uint32_t stringIndex;
            if ( !data ) {
                dataSize                = stringLength;
                data                    = malloc(dataSize);
                bzero(data, dataSize);
            }
            
            for ( stringIndex=0; stringIndex<stringLength; stringIndex+=2) {
                char byteStr[3] =  {};
                
                bcopy(&(argv[argi][stringIndex]), byteStr, 2);
                
                data[dataIndex++] = strtoul(byteStr, NULL, 16);
            }
        }
        
    }
    
    if ( data ) {
        if ( dataSize > dataIndex )
            dataSize = dataIndex;

        if ( parse )
            PrintHIDDescriptor(data, dataSize);
        
        if ( handled ) {
            startDevice(properties, reportInterval, data, dataIndex, userInputCallback, outputReportCallback, inputReportCallback);
        }
        
        free(data);
        
    } else {
        printHelp();
    }
    
    if ( properties )
        CFRelease(properties);
    
    return 0;
}
