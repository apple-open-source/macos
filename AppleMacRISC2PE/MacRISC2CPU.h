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
 * Copyright (c) 1999-2000 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Tom Sherman
 *
 */

#ifndef _IOKIT_MACRISC2CPU_H
#define _IOKIT_MACRISC2CPU_H

#include <IOKit/IOCPU.h>

#include "MacRISC2.h"

class MacRISC2CPUInterruptController;

class MacRISC2CPU : public IOCPU
{
    OSDeclareDefaultStructors(MacRISC2CPU);
  
private:
    bool                         bootCPU;
    bool                         flushOnLock;
    UInt32                       l2crValue;
    MacRISC2PE                   *macRISC2PE;
    IOPCI2PCIBridge              *decBridge;
    UInt32                       numCPUs;
    bool                         rememberNap;
    IOService                    *mpic;
    IOService                    *keyLargo;
    IOService                    *pmu;

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

public:
    virtual const OSSymbol *getCPUName(void);
  
    virtual bool           start(IOService *provider);
    virtual IOReturn       powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);
    virtual void           initCPU(bool boot);
    virtual void           quiesceCPU(void);
    virtual kern_return_t  startCPU(vm_offset_t start_paddr,
				  vm_offset_t arg_paddr);
    virtual void           haltCPU(void);
    virtual void           signalCPU(IOCPU *target);
    virtual void           enableCPUTimeBase(bool enable);
};

#endif /* ! _IOKIT_MACRISC2CPU_H */
