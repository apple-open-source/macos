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


#ifndef _IOKIT_APPLE_U3_H
#define _IOKIT_APPLE_U3_H

#include <IOKit/IOService.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "IOPlatformFunction.h"


#define kIOPCICacheLineSize 	"IOPCICacheLineSize"
#define kIOPCITimerLatency		"IOPCITimerLatency"
#define kAAPLSuspendablePorts	"AAPL,SuspendablePorts"

// platform function link to chip fault GPIO
#define kChipFaultFuncName		"platform-chip-fault"

// internal data structure to track memory parity errors
typedef struct _u3_parity_error_record_t
{
	char	slotName[32];
	UInt32	count;
} u3_parity_error_record_t;

#define kU3MaxDIMMSlots			8	// max number of dimm slots we're prepared to handle

#define kU3ECCNotificationIntervalMS	500	// notify clients of outstanding ECC errors at this interval

// memory parity error message type
#ifndef sub_iokit_platform
#define sub_iokit_platform				err_sub(0x2A)	// chosen randomly...
#endif

#ifndef kIOPlatformMessageParityError
#define kIOPlatformMessageParityError	iokit_family_err( sub_iokit_platform, 0x100 )
#endif

// message format for client notifications.  MUST NOT EXCEED 64 BYTES!!!
typedef struct _u3_parity_error_msg_t
{
	UInt8	version;	// structure version - using 0x1 for now
	UInt8	slotIndex;	// the index of the dimm slot this message applies to
	char	slotName[32];	// an ascii string describing the slot
	UInt32	count;	// the number of errors encountered since the last notification was sent
} u3_parity_error_msg_t;

class AppleU3: public ApplePlatformExpert
{

    OSDeclareDefaultStructors(AppleU3)

public:

    virtual  bool start( IOService * nub );
    virtual void free();
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, 
		void *param1, void *param2, void *param3, void *param4);
    virtual IOReturn callPlatformFunction(const char *functionName, bool waitForFunction, 
		void *param1, void *param2, void *param3, void *param4);

	static void sHandleChipFault( void*, void*, void*, void* );
	static void sDispatchECCNotifier( void* self, void* refcon );

private:
	IOMemoryMap				*uniNMemory;
    volatile UInt32			*uniNBaseAddress;
    UInt32					uniNVersion;
	bool					uataBusWasReset;
    IOService				*provider;
	IORegistryEntry			*mpicRegEntry;
	IOService				*k2;
	IOService				*spu;
	IOService				*pmu;
	IOPCIDevice				*golem;
	// this is to ensure mutual exclusive access to the Uni-N registers:
	IOSimpleLock 			*mutex;
	OSArray 				*platformFuncArray;
    IOMemoryMap				*uATABaseAddressMap;
	volatile UInt32			*uATABaseAddress;
	UInt32					saveDARTCntl;
	UInt32					saveClockCntl;
	UInt32					saveVSPSoftReset;
	bool					hostIsMobile;
    const OSSymbol			*symGetHTLinkFrequency;
    const OSSymbol			*symSetHTLinkFrequency;
    const OSSymbol			*symGetHTLinkWidth;
    const OSSymbol			*symSetHTLinkWidth;
    const OSSymbol			*symSetSPUSleep;
    const OSSymbol			*symSetPMUSleep;
	const OSSymbol			*symU3APIPhyDisableProcessor1;

	// chip fault interrupt symbols
	const OSSymbol			*symChipFaultFunc;
	const OSSymbol			*symPFIntRegister;
	const OSSymbol			*symPFIntEnable;
	const OSSymbol			*symPFIntDisable;
	
        //get U3 version
        const OSSymbol 			*symreadUniNReg;

	// thread callout for servicing ecc errors without blocking the workloop
	thread_call_t				eccErrorCallout;

	// this array holds DIMM slot names if ECC is enabled
	UInt32						dimmCount;
	u3_parity_error_record_t	*dimmErrors;	// allocated in setupECC()
	IOSimpleLock				*dimmLock;
	UInt32						*dimmErrorCountsTotal;

    virtual UInt32 readUniNReg(UInt32 offset);
    virtual void writeUniNReg(UInt32 offset, UInt32 data);
	virtual UInt32 safeReadRegUInt32(UInt32 offset);
	virtual void safeWriteRegUInt32(UInt32 offset, UInt32 mask, UInt32 data);
	virtual void uniNSetPowerState (UInt32 state);

	virtual bool performFunction(const IOPlatformFunction *func, void *param1 = 0,
			void *param2 = 0, void *param3 = 0, void *param4 = 0);
	virtual IOPCIDevice* findNubForPHandle( UInt32 pHandleValue );

	virtual void prepareForSleep ( void );
	virtual bool getHTLinkFrequency (UInt32 *freqResult);
	virtual bool setHTLinkFrequency (UInt32 newFreq);
	virtual bool getHTLinkWidth (UInt32 *linkOutWidthResult, UInt32 *linkInWidthResult);
	virtual bool setHTLinkWidth (UInt32 newLinkOutWidth, UInt32 newLinkInWidth);
	virtual void u3APIPhyDisableProcessor1 ( void );

	virtual IOReturn	installChipFaultHandler ( IOService * provider );
	virtual void		eccNotifier( void * refcon );
	virtual void		setupECC( void );
	virtual void		setupDARTExcp( void );
};

#endif /*  _IOKIT_APPLE_U3_H */
