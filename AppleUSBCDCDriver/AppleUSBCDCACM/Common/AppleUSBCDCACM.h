/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef __APPLEUSBCDCACM__
#define __APPLEUSBCDCACM__

#include "AppleUSBCDCCommon.h"

    // Common Defintions

#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to Event LoG (via kprintf and Firewire) - LDEBUG must also be set
#define USE_IOL		0			// to IOLog - LDEBUG must also be set
#define	LOG_DATA	0			// logs data to the appropriate log - LDEBUG must also be set
#define DUMPALL		0			// Dumps all the data to the log - LOG_DATA must also be set

#define Sleep_Time	20

#if LDEBUG
#define Log IOLog
#else
	#define Log
#endif

#if USE_ELG
	#undef Log
	#define Log	kprintf
#endif

#if LDEBUG
    #if USE_ELG
		#define XTRACE(ID,A,B,STRING) {Log("%8x %8x %8x %8x " DEBUG_NAME ": " STRING "\n",(unsigned int)(ID),(unsigned int)(A),(unsigned int)(B), (unsigned int)IOThreadSelf());}
    #else /* not USE_ELG */
        #if USE_IOL
			#define XTRACE(ID,A,B,STRING) {Log("%8x %8x %8x %8x " DEBUG_NAME ": " STRING "\n",(unsigned int)(ID),(unsigned int)(A),(unsigned int)(B), (unsigned int)IOThreadSelf()); IOSleep(Sleep_Time);}
        #else
            #define XTRACE(id, x, y, msg)
        #endif /* USE_IOL */
    #endif /* USE_ELG */
    #if LOG_DATA
		#if DUMPALL
			#define LogData(D, C, b)	dumpData((UInt8)D, (char *)b, (SInt32)C)
			#define meLogData(D, C, b)	me->dumpData((UInt8)D, (char *)b, (SInt32)C)
		#else
			#define LogData(D, C, b)	USBLogData((UInt8)D, (SInt32)C, (char *)b)
			#define meLogData(D, C, b)	me->USBLogData((UInt8)D, (SInt32)C, (char *)b)
		#endif
		#define DumpData(D, C, b)	dumpData((UInt8)D, (char *)b, (SInt32)C)
    #else /* not LOG_DATA */
        #define LogData(D, C, b)
        #define meLogData(D, C, b)
		#define DumpData(D, C, b)
    #endif /* LOG_DATA */
#else /* not LDEBUG */
    #define XTRACE(id, x, y, msg)
    #define LogData(D, C, b)
    #define meLogData(D, C, b)
	#define DumpData(D, C, b)
    #undef USE_ELG
    #undef USE_IOL
    #undef LOG_DAT
	#undef DUMPALL
#endif /* LDEBUG */

#define ALERT(A,B,STRING)	IOLog("%8x %8x " DEBUG_NAME ": " STRING "\n", (unsigned int)(A), (unsigned int)(B))

enum
{
    kDataIn 		= 0,
    kDataOut,
    kDataOther,
	kDataNone
};

    // USB CDC ACM Defintions
	
#define kUSBbRxCarrier			0x01			// Carrier Detect
#define kUSBDCD				kUSBbRxCarrier
#define kUSBbTxCarrier			0x02			// Data Set Ready
#define kUSBDSR				kUSBbTxCarrier
#define kUSBbBreak			0x04
#define kUSBbRingSignal			0x08
#define kUSBbFraming			0x10
#define kUSBbParity			0x20
#define kUSBbOverRun			0x40

#define kDTROff				0
#define kRTSOff				0
#define kDTROn				1
#define kRTSOn				2
	
typedef struct
{	
    UInt32	dwDTERate;
    UInt8	bCharFormat;
    UInt8	bParityType;
    UInt8	bDataBits;
} LineCoding;
	
#define dwDTERateOffset	0

#define wValueOffset	2
#define wIndexOffset	4
#define wLengthOffset	6

#endif