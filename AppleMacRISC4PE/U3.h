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
//		$Log: U3.h,v $
//		Revision 1.7  2003/07/03 01:16:32  raddog
//		[3313953]U3 PwrMgmt register workaround
//		
//		Revision 1.6  2003/06/03 23:03:57  raddog
//		disable second cpu when unused - 3249029, 3273619
//		
//		Revision 1.5  2003/06/03 01:50:24  raddog
//		U3 sleep changes including calling SPU
//		
//		Revision 1.4  2003/05/07 00:14:55  raddog
//		[3125575] MacRISC4 initial sleep support
//		
//		Revision 1.3  2003/04/04 01:27:45  raddog
//		version 101.0.8
//		
//		Revision 1.2  2003/03/04 17:53:20  raddog
//		[3187811] P76: U3.2.0 systems don't boot
//		[3187813] MacRISC4CPU bridge saving code can block on interrupt stack
//		[3138343] Q37 Feature: remove platform functions for U3
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#ifndef _IOKIT_APPLE_U3_H
#define _IOKIT_APPLE_U3_H

#include <IOKit/IOService.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "IOPlatformFunction.h"

// Uni-North Register Information

#define kUniNVersion               (0x0000)
#define kUniNVersion107            (0x0003)
#define kUniNVersion10A            (0x0007)
#define kUniNVersion150            (0x0011)
#define kUniNVersion200            (0x0024)
#define kUniNVersionPangea         (0x00C0)
#define kUniNVersionIntrepid       (0x00D2)
#define kUniNVersion3			   (0x0030)		// U3

#define kUniNClockControl          (0x0020)
#define kUniNFirewireClockEnable   (1 << 2)
#define kUniNEthernetClockEnable   (1 << 1)
#define kUniNPCI2ClockEnable       (1 << 0)

#define kUniNPowerMngmnt           (0x0030)
#define kUniNNormal                (0x00)
#define kUniNIdle2                 (0x01)
#define kUniNSleep                 (0x02)

#define kUniNArbCtrl               (0x0040)
#define kUniNArbCtrlQAckDelayShift (15)
#define kUniNArbCtrlQAckDelayMask  (0x0e1f8000)
#define kUniNArbCtrlQAckDelay      (0x30)
#define kUniNArbCtrlQAckDelay105   (0x00)

#define kUniNHWInitState           (0x0070)
#define kUniNHWInitStateSleeping   (0x01)
#define kUniNHWInitStateRunning    (0x02)

// As far as I can tell the clock stop status registers are Intrepid only
// For K2, similar information is available in FCR9
#define kUniNClockStopStatus0		(0x150)
#define kUniNIsStopped49			(1 << (31 - 31))
#define kUniNIsStopped45			(1 << (31 - 30))
#define kUniNIsStopped32			(1 << (31 - 29))
#define kUniNIsStoppedUSB2			(1 << (31 - 28))
#define kUniNIsStoppedUSB1			(1 << (31 - 27))
#define kUniNIsStoppedUSB0			(1 << (31 - 26))
#define kUniNIsStoppedVEO1			(1 << (31 - 25))
#define kUniNIsStoppedVEO0			(1 << (31 - 24))
#define kUniNIsStoppedPCI_FB_CLK_OUT (1 << (31 - 23))
#define kUniNIsStoppedSlot2			(1 << (31 - 22))
#define kUniNIsStoppedSlot1			(1 << (31 - 21))
#define kUniNIsStoppedSlot0			(1 << (31 - 20))
#define kUniNIsStoppedVIA32			(1 << (31 - 19))
#define kUniNIsStoppedSCC_RTClk32or45 (1 << (31 - 18))
#define kUniNIsStoppedSCC_RTClk18	(1 << (31 - 17))
#define kUniNIsStoppedTimer			(1 << (31 - 16))
#define kUniNIsStoppedI2S1_18		(1 << (31 - 15))
#define kUniNIsStoppedI2S1_45or49	(1 << (31 - 14))
#define kUniNIsStoppedI2S0_18		(1 << (31 - 13))
#define kUniNIsStoppedI2S0_45or49	(1 << (31 - 12))
#define kUniNIsStoppedAGPDel		(1 << (31 - 11))
#define kUniNIsStoppedExtAGP		(1 << (31 - 10))

#define kUniNClockStopStatus1		(0x160)
#define kUniNIsStopped18			(1 << (31 - 31))
#define kUniNIsStoppedPCI0			(1 << (31 - 30))
#define kUniNIsStoppedAGP			(1 << (31 - 29))
#define kUniNIsStopped7PCI1			(1 << (31 - 28))
#define kUniNIsStoppedUSB2PCI		(1 << (31 - 26))
#define kUniNIsStoppedUSB1PCI		(1 << (31 - 25))
#define kUniNIsStoppedUSB0PCI		(1 << (31 - 24))
#define kUniNIsStoppedKLPCI			(1 << (31 - 23))
#define kUniNIsStoppedPCI1			(1 << (31 - 22))
#define kUniNIsStoppedMAX			(1 << (31 - 21))
#define kUniNIsStoppedATA100		(1 << (31 - 20))
#define kUniNIsStoppedATA66			(1 << (31 - 19))
#define kUniNIsStoppedGB			(1 << (31 - 18))
#define kUniNIsStoppedFW			(1 << (31 - 17))
#define kUniNIsStoppedPCI2			(1 << (31 - 16))
#define kUniNIsStoppedBUF_REF_CLK_OUT (1 << (31 - 15))
#define kUniNIsStoppedCPU			(1 << (31 - 14))
#define kUniNIsStoppedCPUDel		(1 << (31 - 13))
#define kUniNIsStoppedPLL4Ref		(1 << (31 - 12))

#define kUniNMPCIMemTimeout			(0x2160)
#define kUniNMPCIMemTimeoutMask		(0xFF000000)
#define kUniNMPCIMemGrantTime		(0x0 << 28)

#define kUniNUATAReset				(0x02000000)
#define kUniNUATAEnable				(0x01000000)

// U3 Control and Power Management registers and defines
#define kU3ToggleRegister			(0xE0)
#define kU3PMCStartStop				(1 << (31 - 31))
#define kU3MPICReset				(1 << (31 - 30))
#define kU3MPICEnableOutputs		(1 << (31 - 29))

#define kU3PMClockControl			(0xF00)
#define kU3APIDebugClockEnable		(1 << (31 - 31))
#define kU3APILogicStopEnable		(1 << (31 - 30))
#define kU3EnablePLL1Shutdown		(1 << (31 - 29))

#define kU3PMPwrSystem				(0xF10)
#define kU3PwrSleep					(1 << (31 - 30))
#define kU3PwrNormal				0

#define kU3PMPwrCPU					(0xF20)
#define kU3CPUPwrDnEnable0			(1 << (31 - 7))
#define kU3CPUPwrDnEnable1			(1 << (31 - 6))
#define kU3CPUPwrDnEnable2			(1 << (31 - 5))
#define kU3CPUPwrDnEnable3			(1 << (31 - 4))
#define kU3CPUSleep0				(1 << (31 - 15))
#define kU3CPUSleep1				(1 << (31 - 14))
#define kU3CPUSleep2				(1 << (31 - 13))
#define kU3CPUSleep3				(1 << (31 - 12))
#define kU3CPUTristateEnable		(1 << (31 - 31))

#define kU3CPUPwrDnEnableAll		(kU3CPUPwrDnEnable0 | kU3CPUPwrDnEnable1 | kU3CPUPwrDnEnable2 | kU3CPUPwrDnEnable3)
#define kU3CPUSleepAll				(kU3CPUSleep0 | kU3CPUSleep1 | kU3CPUSleep2 | kU3CPUSleep3)

#define kU3PMPwrCPUQuiesce			(0xF30)
#define kU3PMPwrHT					(0xF40)
#define kU3PMPLL1Control			(0xF50)
#define kU3PMPLL2Control			(0xF60)
#define kU3PMPLL3Control			(0xF70)
#define kU3PMPLL4Control			(0xF80)
#define kU3PMPLLVis					(0xF90)
#define kU3PMSBusArb				(0xFF0)
#define kU3PMSMax					(0xFFC)

// APIPhy registers
#define kU3APIPhyConfigRegister1	(0x23030)

// HyperTransport link register offsets
#define kU3HTLinkConfigRegister		(0x70110)
#define kU3HTLinkFreqRegister		(0x70120)

#define kIOPCICacheLineSize 	"IOPCICacheLineSize"
#define kIOPCITimerLatency		"IOPCITimerLatency"
#define kAAPLSuspendablePorts	"AAPL,SuspendablePorts"

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
	bool					hostIsMobile;
    const OSSymbol			*symGetHTLinkFrequency;
    const OSSymbol			*symSetHTLinkFrequency;
    const OSSymbol			*symGetHTLinkWidth;
    const OSSymbol			*symSetHTLinkWidth;
    const OSSymbol			*symSetSPUSleep;
    const OSSymbol			*symSetPMUSleep;
	const OSSymbol			*symU3APIPhyDisableProcessor1;

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

};

#endif /*  _IOKIT_APPLE_U3_H */
