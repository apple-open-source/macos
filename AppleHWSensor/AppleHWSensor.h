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
//		$Log: AppleHWSensor.h,v $
//		Revision 1.14  2008/04/18 23:25:31  raddog
//		<rdar://problem/5828356> AppleHWSensor - control code needs to deal with endian issues
//		
//		Revision 1.13  2007/03/16 21:40:09  raddog
//		[5056773]IOHWMonitor::updateValue() may call callPlatformFunction with NULL key
//		
//		Revision 1.12  2004/07/26 16:25:20  eem
//		Merge Strider changes from AppleHWSensor-130_1_2 to TOT. Bump version to
//		1.3.0a2.
//		
//		Revision 1.11.6.1  2004/07/23 22:16:26  eem
//		<rdar://problem/3725357> Add setPowerState() prototype to receive power
//		management notifications.
//		
//		Revision 1.11  2003/12/02 02:02:28  tsherman
//		3497295 - Q42 Task - AppleHWSensor's AppleLM8x (NEW) driver needs to be revised to comply with Thermal API
//		
//		Revision 1.10  2003/10/23 20:08:18  wgulland
//		Adding IOHWControl and a base class for IOHWSensor and IOHWControl
//		
//		Revision 1.9  2003/08/12 01:23:30  wgulland
//		Add code to handle notification via phandle in notify-xxx property
//		
//		Revision 1.8  2003/07/14 22:26:42  tsherman
//		3321185 - Q37: AppleHWSensor needs to stop polling at restart/shutdown
//		
//		Revision 1.7  2003/07/02 22:25:45  dirty
//		Fix warnings.
//		
//		Revision 1.6  2003/06/17 20:01:55  raddog
//		[3292803] Disable sensor reading across sleep
//		

#ifndef _APPLEHWSENSOR_H
#define _APPLEHWSENSOR_H

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
// #define APPLEHWSENSOR_DEBUG 1

#ifdef APPLEHWSENSOR_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#ifdef __ppc__
#	define ENABLENOTIFY
#endif

#define kNoThreshold -1
#define kNoPolling 0xffffffff

#define kLowThresholdHit 3
#define kHighThresholdHit 4
#define kRegisterSensor 1

class IOHWSensor : public IOHWMonitor
{
    OSDeclareDefaultStructors(IOHWSensor)

protected:

    SInt32					fLowThreshold;
    SInt32					fHighThreshold;
	thread_call_t			fCalloutEntry;
	UInt32					fPollingPeriod;
	UInt32					fPollingPeriodNS;
	bool					fInited;
#ifdef ENABLENOTIFY
    IORegistryEntry			*fNotifyObj;
    const OSSymbol			*fNotifySym;
 #endif
    
    static void timerCallback(void *self);
    void setLowThreshold(OSNumber *val);
    void setHighThreshold(OSNumber *val);
    void setPollingPeriod(OSNumber *val);
	void setPollingPeriodNS(OSNumber *val);
    void setTimeout();
    SInt32 updateCurrentValue();
    void sendMessage(UInt32 msg, SInt32 val, SInt32 threshold);
    
public:
    // Generic IOService stuff:
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
    
    // Override to allow setting of some properties
    // A dictionary is expected, containing the new properties
    virtual IOReturn setProperties( OSObject * properties );

	virtual IOReturn setPowerState(unsigned long, IOService *);
};

#endif	// _APPLEHWSENSOR_H
