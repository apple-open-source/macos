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
 *  DRI: Dave Radcliffe
 *
 */
//		$Log: MacRISC4CPU.h,v $
//		Revision 1.8  2003/06/03 23:03:57  raddog
//		disable second cpu when unused - 3249029, 3273619
//		
//		Revision 1.7  2003/05/07 00:14:55  raddog
//		[3125575] MacRISC4 initial sleep support
//		
//		Revision 1.6  2003/04/27 23:13:30  raddog
//		MacRISC4PE.cpp
//		
//		Revision 1.5  2003/03/04 17:53:20  raddog
//		[3187811] P76: U3.2.0 systems don't boot
//		[3187813] MacRISC4CPU bridge saving code can block on interrupt stack
//		[3138343] Q37 Feature: remove platform functions for U3
//		
//		Revision 1.4  2003/02/27 01:42:54  raddog
//		Better support for MP across sleep/wake [3146943]. This time we block in startCPU, rather than initCPU, which is safer.
//		
//		Revision 1.3  2003/02/19 21:54:45  raddog
//		Support for MP across sleep/wake [3146943]
//		
//		Revision 1.2  2003/02/18 00:02:01  eem
//		3146943: timebase enable for MP, bump version to 1.0.1d3.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#ifndef _IOKIT_MACRISC4CPU_H
#define _IOKIT_MACRISC4CPU_H

#include <IOKit/IOCPU.h>

#include "MacRISC4PE.h"
#include "IOPlatformPlugin.h"

class MacRISC4PE;
class MacRISC4CPUInterruptController;

enum {
	kMaxPCIBridges = 32
};

class MacRISC4CPU : public IOCPU
{
    OSDeclareDefaultStructors(MacRISC4CPU);
  
private:
    bool				bootCPU;
    bool				flushOnLock;
    UInt32				l2crValue;
    MacRISC4PE			*macRISC4PE;
	IOService			*uniN;
	IOService			*ioPPlugin;
	OSDictionary		*ioPPluginDict;
    UInt32				numCPUs;
    bool				rememberNap;
    IOService			*mpic;
    IOService			*keyLargo;
    IOService			*pmu;
    UInt32				soft_reset_offset;
    IOPMrootDomain		*pmRootDomain;
	bool				doSleep;
    bool				processorSpeedChange;
    UInt32				currentProcessorSpeed;
	UInt32				topLevelPCIBridgeCount;
	IOPCIBridge			*topLevelPCIBridges[kMaxPCIBridges];
    
    virtual void ipiHandler(void *refCon, void *nub, int source);

    // callPlatformFunction symbols
    const OSSymbol 		*mpic_dispatchIPI;
    const OSSymbol 		*mpic_getProvider;
    const OSSymbol 		*mpic_getIPIVector;
    const OSSymbol 		*mpic_setCurrentTaskPriority;
    const OSSymbol 		*mpic_setUpForSleep;
    const OSSymbol 		*keyLargo_restoreRegisterState;
    const OSSymbol 		*keyLargo_syncTimeBase;
    const OSSymbol 		*keyLargo_saveRegisterState;
    const OSSymbol 		*keyLargo_turnOffIO;
    const OSSymbol 		*keyLargo_writeRegUInt8;
    const OSSymbol 		*keyLargo_setPowerSupply;
    const OSSymbol 		*UniNSetPowerState;
    const OSSymbol 		*UniNPrepareForSleep;

    const OSSymbol 		*i2c_openI2CBus;
    const OSSymbol 		*i2c_closeI2CBus;
    const OSSymbol 		*i2c_setCombinedMode;
    const OSSymbol 		*i2c_readI2CBus;
    const OSSymbol 		*i2c_writeI2CBus;
    const OSSymbol 		*u3APIPhyDisableProcessor1;

public:
    virtual const OSSymbol *getCPUName(void);
  
    virtual bool           start(IOService *provider);
	virtual void		   performPMUSpeedChange (UInt32 newLevel);

    virtual void           initCPU(bool boot);
    virtual void           quiesceCPU(void);
    virtual kern_return_t  startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr);
    virtual void           haltCPU(void);
    virtual void           signalCPU(IOCPU *target);
    virtual void           enableCPUTimeBase(bool enable);
};

#endif /* ! _IOKIT_MACRISC4CPU_H */
