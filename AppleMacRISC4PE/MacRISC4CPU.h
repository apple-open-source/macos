/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */


#ifndef _IOKIT_MACRISC4CPU_H
#define _IOKIT_MACRISC4CPU_H

#include <IOKit/IOCPU.h>

#include "MacRISC4PE.h"
#include "IOPlatformPlugin.h"

class MacRISC4PE;
class MacRISC4CPUInterruptController;

// data structure to hold platform-specific timebase sync parameters
typedef struct
{
	const char * i2c_iface;		// string identifier for desired PPCI2CInterface in IOResources
	UInt8 i2c_port;				// i2c port 0 or 1
	UInt8 i2c_addr;				// 8-bit i2c slave address
	UInt8 i2c_subaddr;			// 8-bit i2c register subaddress
	UInt8 mask;					// mask to be applied during writes to device register
	UInt8 enable_value;			// value for enabling timebase clocks
	UInt8 disable_value;		// value for stopping timebase clocks
} cpu_timebase_params_t;

enum {
	kMaxPCIBridges = 32
};

class MacRISC4CPU : public IOCPU
{
    OSDeclareDefaultStructors(MacRISC4CPU);
  
private:
    bool				bootCPU;
    bool				flushOnLock;
    bool				haveSleptMPIC;
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

	static	void		sIPIHandler( OSObject* self, void* refCon, IOService* nub, int source );
	virtual	void		ipiHandler(void *refCon, void *nub, int source);

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

	static	void			sEnableCPUTimeBase( cpu_id_t self, boolean_t enable );

public:
    virtual const OSSymbol *getCPUName(void);
  
    virtual bool           start(IOService *provider);

    virtual void           initCPU(bool boot);
    virtual void           quiesceCPU(void);
    virtual kern_return_t  startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr);
    virtual void           haltCPU(void);
    virtual void           signalCPU(IOCPU *target);
    virtual void           enableCPUTimeBase(bool enable);
};

#endif /* ! _IOKIT_MACRISC4CPU_H */
