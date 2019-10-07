/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _IOKIT_HID_IOHIDFAMILYPRIVATE_H
#define _IOKIT_HID_IOHIDFAMILYPRIVATE_H

#ifdef KERNEL
#include "IOHIDKeys.h"
#include "IOHIDDevice.h"
#endif

#include "IOHIDDebug.h"
#include <pexpert/pexpert.h>

__BEGIN_DECLS

#ifdef KERNEL
bool CompareProperty(IOService * owner, OSDictionary * matching, const char * key, SInt32 * score, SInt32 increment = 0);
bool CompareDeviceUsage( IOService * owner, OSDictionary * matching, SInt32 * score, SInt32 increment = 0);
bool CompareDeviceUsagePairs(IOService * owner, OSDictionary * matching, SInt32 * score, SInt32 increment = 0);
bool CompareProductID( IOService * owner, OSDictionary * matching, SInt32 * score);
bool MatchPropertyTable(IOService * owner, OSDictionary * table, SInt32 * score);
bool CompareNumberPropertyMask( IOService *owner, OSDictionary *matching, const char *key, const char *maskKey, SInt32 *score, SInt32 increment);
bool CompareNumberPropertyArray( IOService * owner, OSDictionary * matching, const char * arrayName, const char * key, SInt32 * score, SInt32 increment);
bool CompareNumberPropertyArrayWithMask( IOService * owner, OSDictionary * matching, const char * arrayName, const char * key, const char * maskKey, SInt32 * score, SInt32 increment);

#define     kEjectKeyDelayMS        0       // the delay for a dedicated eject key
#define     kEjectF12DelayMS        250     // the delay for an F12/eject key

void IOHIDSystemActivityTickle(SInt32 nxEventType, IOService *sender);

void handle_stackshot_keychord(uint32_t keycode);

#define NX_HARDWARE_TICKLE  (NX_LASTEVENT+1)
#endif

bool isSingleUser();

#define kHIDDtraceDebug "hid_dtrace_debug"

__attribute__((optnone)) __attribute__((unused)) static uint32_t gIOHIDFamilyDtraceDebug()
{
    static uint32_t debug = 0xffffffff;
    
    if (debug == 0xffffffff) {
        debug = 0;
        
        if (!PE_parse_boot_argn(kHIDDtraceDebug, &debug, sizeof (debug))) {
            debug = 0;
        }
    }
    
    return debug;
}

/*!
 used as dtrace probe
 funcType : 1(getReport), 2(setReport), 3(handleReport),
 */
void hid_trace(HIDTraceFunctionType functionType, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);

__END_DECLS

#endif /* !_IOKIT_HID_IOHIDFAMILYPRIVATE_H */
