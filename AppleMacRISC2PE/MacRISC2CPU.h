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
/*
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#ifndef _IOKIT_MACRISC2CPU_H
#define _IOKIT_MACRISC2CPU_H

#include <IOKit/IOCPU.h>

#include "MacRISC2.h"
#include "IOPlatformMonitor.h"

#define enableUserClientInterface   0

#if enableUserClientInterface    
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/platform/AppleMacIODevice.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#define kNormal				0
#define kNoVoltage			1
#define kDelayAACKOnly		2
#define kToggleDelayAACK	4

#define kDFSLow				1
#define kDFSHigh			0

#define kGPULow				1
#define kGPUHigh			0

#define kSteppedLow			1
#define kSteppedHigh		0

#ifndef sub_iokit_graphics
#	define sub_iokit_graphics           err_sub(5)
#endif
#ifndef kIOFBLowPowerAggressiveness
#	define kIOFBLowPowerAggressiveness iokit_family_err(sub_iokit_graphics,1)
#endif

#endif

class MacRISC2PE;
class MacRISC2CPUInterruptController;

enum {
	kMaxPCIBridges = 32
};

class MacRISC2CPU : public IOCPU
{
    OSDeclareDefaultStructors(MacRISC2CPU);
  
private:
    bool				bootCPU;
    bool				flushOnLock;
    UInt32				l2crValue;
    MacRISC2PE			*macRISC2PE;
	IOService			*uniN;
	IOPlatformMonitor	*ioPMon;
	OSDictionary		*ioPMonDict;
    UInt32				numCPUs;
    bool				rememberNap;
    IOService			*mpic;
    IOService			*keyLargo;
    IOService			*pmu;
    UInt32				soft_reset_offset;
    UInt32				timebase_enable_offset;
    IOPMrootDomain		*pmRootDomain;
	bool				doSleep;
    bool				processorSpeedChange;
    bool				ignoreSpeedChange;
    UInt32				currentProcessorSpeed;
    bool				needVSetting;
    bool				needAACKDelay;
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
    const OSSymbol 		*keyLargo_getHostKeyLargo;
    const OSSymbol 		*keyLargo_setPowerSupply;
    const OSSymbol 		*uniN_setPowerState;
    const OSSymbol		*uniN_setAACKDelay;
#if enableUserClientInterface
    IOWorkLoop			*fWorkLoop;
    IOTimerEventSource  *fDFSContTimer;
    IOTimerEventSource  *fGPUContTimer;
    IOTimerEventSource  *fVStepContTimer;
    bool				DFS_Status;
    bool				GPU_Status;
    bool				vStepped;
    UInt32				DFSTime;
    UInt32				GPUTime;
    UInt32				vStepTime;
	UInt32				DFScontMode;
    virtual bool		initTimers(void);
    virtual void 		DFSContTimerEventOccurred(IOTimerEventSource *sender);
    virtual void 		GPUContTimerEventOccurred(IOTimerEventSource *sender);
    virtual void 		vStepContTimerEventOccurred(IOTimerEventSource *sender);
#endif    

public:
    virtual const OSSymbol *getCPUName(void);
  
    virtual bool           start(IOService *provider);
    virtual IOReturn       powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);
    virtual IOReturn	   setAggressiveness(unsigned long selector, unsigned long newLevel);
	virtual void		   performPMUSpeedChange (UInt32 newLevel);

    virtual void           initCPU(bool boot);
    virtual void           quiesceCPU(void);
    virtual kern_return_t  startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr);
    virtual void           haltCPU(void);
    virtual void           signalCPU(IOCPU *target);
    virtual void           enableCPUTimeBase(bool enable);
#if enableUserClientInterface
    virtual IOReturn		DFS(UInt32 newLevel, UInt32 mode);
    virtual IOReturn		DFSCont(UInt32 delayTime, UInt32 mode);
    virtual IOReturn		DFSStopCont(void);
    virtual IOReturn		SetGPUPower(UInt32 GPUPowerLevel);
    virtual IOReturn		GPUCont(UInt32 delayTime);
    virtual IOReturn		GPUStopCont(void);
    virtual IOReturn		vStep(UInt32 newLevel);
    virtual IOReturn		vStepCont(UInt32 delayTime);
    virtual IOReturn		vStepStopCont(void);
    static  void			DFSTimerEventHandler(OSObject *self, IOTimerEventSource *sender);
    static  void			GPUTimerEventHandler(OSObject *self, IOTimerEventSource *sender);
    static  void			vStepTimerEventHandler(OSObject *self, IOTimerEventSource *sender);
#endif
};

#endif /* ! _IOKIT_MACRISC2CPU_H */
