/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 */
//		$Log: AppleHWControl.h,v $
//		Revision 1.2  2008/04/18 23:25:31  raddog
//		<rdar://problem/5828356> AppleHWSensor - control code needs to deal with endian issues
//		
//		Revision 1.1  2003/10/23 20:08:18  wgulland
//		Adding IOHWControl and a base class for IOHWSensor and IOHWControl
//		
//		

#ifndef _APPLEHWCONTROLLER_H
#define _APPLEHWCONTROLLER_H

#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPowerConnection.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <string.h>
#include "AppleHWMonitor.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
// #define APPLEHWCONTROLLER_DEBUG 1

#ifdef APPLEHWCONTROLLER_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define kRegisterControl 1

class IOHWControl : public IOHWMonitor
{
    OSDeclareDefaultStructors(IOHWControl)

private:
	UInt32	lastValue;

protected:

#ifdef APPLEHWCONTROLLER_DEBUG
	char fDebugID[64];
	void initDebugID(IOService *provider);
#endif

    
    IOReturn setTargetValue(OSNumber *val);
    IOReturn updateCurrentValue();
    IOReturn updateTargetValue();

public:
    // Generic IOService stuff:
    virtual bool start(IOService *provider);
    
    // Override to allow setting of some properties
    // A dictionary is expected, containing the new properties
    virtual IOReturn setProperties( OSObject * properties );

#if !defined( __ppc__ )
    // get sleep/wake messages
	virtual IOReturn setPowerState(unsigned long, IOService *);
#endif	
};

#endif	// _APPLEHWCONTROLLER_H
