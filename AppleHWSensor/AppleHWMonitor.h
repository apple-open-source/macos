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
//		$Log: AppleHWMonitor.h,v $
//		Revision 1.3  2004/02/12 01:17:01  eem
//		Merge Rohan changes from tag MERGED-FROM-rohan-branch-TO-TOT-1
//		
//		Revision 1.2  2004/01/30 23:52:00  eem
//		[3542678] IOHWSensor/IOHWControl should use "reg" with version 2 thermal parameters
//		Remove AppleSMUSensor/AppleSMUFan since that code will be in AppleSMUDevice class.
//		Fix IOHWMonitor, AppleMaxim6690, AppleAD741x to use setPowerState() API instead of
//		unsynchronized powerStateWIllChangeTo() API.
//		
//		Revision 1.1.4.1  2004/02/10 09:58:01  eem
//		3548562, 3554178 - prevent extra OSNumber allocations
//		
//		Revision 1.1  2003/10/23 20:08:18  wgulland
//		Adding IOHWControl and a base class for IOHWSensor and IOHWControl
//		
//		

#ifndef _APPLEHWMONITOR_H
#define _APPLEHWMONITOR_H

#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
// #define APPLEHWMONITOR_DEBUG 1

#ifdef APPLEHWMONITOR_DEBUG
#define DLOG(fmt, args...)	kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define kHWMonitorPowerAckLimit 10000

// power state definitions
enum
{
	kIOHWMonitorOffState,
	kIOHWMonitorOnState,
	kIOHWMonitorNumPowerStates
};

/*
 * Base class of IOHWSensor and IOHWControl
 */
class IOHWMonitor : public IOService
{
    OSDeclareAbstractStructors(IOHWMonitor)

protected:

#ifdef APPLEHWMONITOR_DEBUG
	char fDebugID[64];
	void initDebugID(IOService *provider);
#endif

    UInt32					fID;
    UInt32					fChannel;
	IOService				*fIOPMon;
	IOPMrootDomain			*pmRootDomain;
	IOService				*powerPolicyMaker;
	IOService				*powerPolicyChanger;
	volatile bool			sleeping;
	volatile bool			busy;
    // Flag reflecting restart state
    static bool systemIsRestarting;
    
    IOReturn updateValue(const OSSymbol *func, const OSSymbol *key);
    void setNumber( const OSSymbol * key, UInt32 val );

public:

    // Generic IOService stuff:
    virtual bool start(IOService *provider);
    
    // get sleep/wake messages
#if !defined( __ppc__ )
	virtual IOReturn powerStateWillChangeTo (IOPMPowerFlags capabilities, 
                    unsigned long stateNumber, IOService* whatDevice);
	virtual IOReturn powerStateDidChangeTo (IOPMPowerFlags  capabilities, 
                    unsigned long stateNumber, IOService* whatDevice);
	virtual IOReturn changePowerState(unsigned long whatState, IOService *powerChanger);
#endif
	virtual IOReturn setPowerState(unsigned long, IOService *);
    static IOReturn sysPowerDownHandler(void *, void *, UInt32, IOService *, void *, vm_size_t);
};

#endif	// _APPLEHWMONITOR_H
