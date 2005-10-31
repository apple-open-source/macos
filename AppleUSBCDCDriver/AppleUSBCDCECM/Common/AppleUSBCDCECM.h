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

#ifndef __APPLEUSBCDCECM__
#define __APPLEUSBCDCECM__

#include "AppleUSBCDCCommon.h"        

#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to Event LoG (via XTrace) - LDEBUG must also be set
#define USE_IOL		0			// to IOLog - LDEBUG must also be set
#define	LOG_DATA	0			// logs data to the appropriate log - LDEBUG must also be set
#define DUMPALL		0			// Dumps all the data to the log - LOG_DATA must also be set

#define Sleep_Time	20

#if LDEBUG
    #if USE_ELG
        #include "XTrace.h"
        #define XTRACE(id, x, y, msg)                    								\
        do														\
        {														\
            if (gXTrace)												\
            {														\
                static char *__xtrace = 0;              								\
                if (__xtrace)												\
                    gXTrace->LogAdd((UInt32)id, (UInt32)(x), (UInt32)(y), __xtrace);    				\
                else													\
                    __xtrace = gXTrace->LogAdd((UInt32)id, (UInt32)(x), (UInt32)(y), " " DEBUG_NAME ": " msg, false);	\
            }														\
        } while(0)
        #define XTRACE2(id, x, y, msg) XTRACE_HELPER(gXTrace, (UInt32)id, x, y, " " DEBUG_NAME": "  msg)
    #else /* not USE_ELG */
        #if USE_IOL
            #define XTRACE(ID,A,B,STRING) {IOLog("%8x %8x %8x %8x " DEBUG_NAME ": " STRING "\n",(unsigned int)(ID),(unsigned int)(A),(unsigned int)(B), (unsigned int)IOThreadSelf()); IOSleep(Sleep_Time);}
        #else
            #define XTRACE(id, x, y, msg)
        #endif /* USE_IOL */
    #endif /* USE_ELG */
    #if LOG_DATA
        #define LogData(D, C, b)	USBLogData((UInt8)D, (UInt32)C, (char *)b)
        #define meLogData(D, C, b)	me->USBLogData((UInt8)D, (UInt32)C, (char *)b)
        #define DumpData(b, C)		dumpData(char *)b, (UInt32)C)
        #define meDumpData(b, C)	me->dumpData(char *)b, (UInt32)C)
    #else /* not LOG_DATA */
        #define LogData(D, C, b)
        #define meLogData(D, C, b)
        #define DumpData(b, C)
        #define meDumpData(b, C)
    #endif /* LOG_DATA */
#else /* not LDEBUG */
    #define XTRACE(id, x, y, msg)
    #define LogData(D, C, b)
    #define meLogData(D, C, b)
    #define DumpData(b, C)
    #define meDumpData(b, C)
    #undef USE_ELG
    #undef USE_IOL
    #undef LOG_DATA
#endif /* LDEBUG */

#define ALERT(A,B,STRING)	IOLog("%8x %8x " DEBUG_NAME ": " STRING "\n", (unsigned int)(A), (unsigned int)(B))

enum
{
    kDataIn			= 0,
    kDataOut,
    kDataOther,
    kDataNone
};

	// Reset states

enum
{
    kResetNormal	= 0,
    kResetNeeded,
    kResetDone
};
#endif