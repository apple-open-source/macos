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

#ifndef _IOKIT_IOGRAPHICSPRIVATE_H
#define _IOKIT_IOGRAPHICSPRIVATE_H

#include <mach/vm_param.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>

#if 0
#warning **LOGS**
#define RLOG 1
#define DEBG(idx, fmt, args...)  	\
do { 					\
    AbsoluteTime    now;		\
    UInt64	    nano;		\
    clock_get_uptime( &now);		\
    absolutetime_to_nanoseconds( now, &nano );				\
    kprintf("%08ld [%d]::", (UInt32) (nano / 1000000ULL), idx);		\
    kprintf(__FUNCTION__);		\
    kprintf(fmt, ## args);		\
} while( false )

#elif 0
#warning **LOGS**
#define RLOG 1
#define DEBG(idx, fmt, args...)		\
do { 					\
    AbsoluteTime    now;		\
    UInt64	    nano;		\
    clock_get_uptime( &now);		\
    absolutetime_to_nanoseconds( now, &nano );			\
    IOLog("%08ld [%d]::", (UInt32) (nano / 1000000ULL), idx);		\
    IOLog(__FUNCTION__);		\
    IOLog(fmt, ## args);		\
} while( false )

#else
#define DEBG(idx, fmt, args...)  {}
#endif

#define kIOFBBootGrayValue		0x00648cc3
// blue actual:0x00426bad gamma:0x00648cc3 bootx:0x00bfbfbf
#define kIOFBGrayValue			0x00648cc3

#ifndef kAppleAudioVideoJackStateKey
#define kAppleAudioVideoJackStateKey	"AppleAudioVideoJackState"
#endif
#ifndef kIOPMIsPowerManagedKey
#define kIOPMIsPowerManagedKey		"IOPMIsPowerManaged"
#endif
#ifndef kIOAGPCommandValueKey
#define kIOAGPCommandValueKey		"IOAGPCommandValue"
#endif
#ifndef kAppleClamshellStateKey
#define kAppleClamshellStateKey		"AppleClamshellState"
#endif
#ifndef kIOFBWaitCursorFramesKey
#define kIOFBWaitCursorFramesKey	"IOFBWaitCursorFrames"
#endif
#ifndef kIOFBWaitCursorPeriodKey
#define kIOFBWaitCursorPeriodKey	"IOFBWaitCursorPeriod"
#endif

extern "C" vm_map_t IOPageableMapForAddress( vm_address_t address );

#if __ppc__
extern "C" void bcopy_nc( void * from, void * to, UInt32 l );
extern "C" void bzero_nc( void * p, UInt32 l );
#else
inline void bcopy_nc( void * from, void * to, UInt32 l)	{ bcopy( from, to, l ); }
inline void bzero_nc( void * p, UInt32 l )		{ bzero( p, l ); }
#endif

#ifndef round_page_32
#define round_page_32(x) round_page(x)
#endif

#ifndef trunc_page_32(x)
#define trunc_page_32(x) trunc_page(x)
#endif

#define thisIndex		_IOFramebuffer_reserved[4]

#endif /* ! _IOKIT_IOGRAPHICSPRIVATE_H */

