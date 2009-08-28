/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Tom Sherman
 *
 */

#ifndef _IOKIT_AppleKeyswitch_H
#define _IOKIT_AppleKeyswitch_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <IOKit/acpi/IOACPIPlatformDevice.h>

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
//#define APPLEKEYSWITCH_DEBUG 1

#ifdef APPLEKEYSWITCH_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

enum
{
	kKeyLargoGPIOBaseAddr		= 0x50, // ExtIntGPIO base offset
};

enum
{
	KEYSWITCH_VALUE_LOCKED		= 0,	
	KEYSWITCH_VALUE_UNLOCKED	= 1
};
enum
{
	kSWITCH_STATE_IS_UNLOCKED	= 0,
	kSWITCH_STATE_IS_LOCKED		= 1
};

class AppleKeyswitch : public IOService
{
    OSDeclareDefaultStructors(AppleKeyswitch);

private:
    UInt8 					state;
    UInt32 					*extIntGPIO;
    
    const OSSymbol 			*keyLargo_safeWriteRegUInt8;
    const OSSymbol 			*keyLargo_safeReadRegUInt8;

#if defined( __i386__ ) || defined( __x86_64 )
	IOACPIPlatformDevice	* myProvider;
	IONotifier				* switchEventNotify;
#elif defined( __ppc__ )
	void					* reserved1;	// make sure we remain the same size
	void					* reserved2;
#endif
	
public:
    IOWorkLoop 				*myWorkLoop;
    IOInterruptEventSource 	*interruptSource;

    virtual bool 			start(IOService *provider);
    virtual void 			stop(IOService *provider);
    static void 			interruptOccurred(OSObject *obj, IOInterruptEventSource *src, int count);

    virtual void 			toggle(bool disableInts);

	static IOReturn			keyswitchNotification(	void *      target,
													void *      refCon,
													UInt32      messageType,
													IOService * provider,
													void *      messageArgument,
													vm_size_t   argSize );
	void					getKeyswitchState( UInt8 * switchState );
};

#endif /* ! _IOKIT_AppleKeyswitch_H */
