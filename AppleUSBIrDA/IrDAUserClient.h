/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
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
/*
 * IrDAUserClient.h
 *
 * to be included by both sides of the IrDA user-client interface.
 * keep free of kernel-only or C++ only includes.
 */
 
#ifndef __IrDAUserClient__
#define __IrDAUserClient__

#include "IrDAStats.h"

enum {
    kIrDAUserClientCookie   = 123       // pass to IOServiceOpen
};

enum {                                  // command bytes to send to the user client
    kIrDAUserCmd_GetLog     = 0x12,     // return irdalog buffers
    kIrDAUserCmd_GetStatus  = 0x13,     // return connection status and counters
    kIrDAUserCmd_Enable     = 0x14,     // Enable the hardware and the IrDA stack
    kIrDAUserCmd_Disable    = 0x15      // Disable the hardware and the IrDA stack
};

enum {                                  // messageType for the callback routines
    kIrDACallBack_Status    = 0x1000,   // Status Information is coming
    kIrDACallBack_Unplug    = 0x1001    // USB Device is unplugged
};

// This is the way the messages are sent from user space to kernel space:
typedef struct IrDACommand
{
    unsigned char commandID;    // one of the commands above (tbd)
    char data[1];               // this is not really one byte, it is as big as I like
				// I set it to 1 just to make the compiler happy
} IrDACommand;
typedef IrDACommand *IrDACommandPtr;

#endif // __IrDAUserClient__
