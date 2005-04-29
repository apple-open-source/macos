/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 *  DRI: Dave Radcliffe
 *
 */


#ifndef _IOKIT_APPLE_UNIN_H
#define _IOKIT_APPLE_UNIN_H

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

#define kUniNClockControl          (0x0020)
#define kUniNFirewireClockEnable   (1 << 2)
#define kUniNEthernetClockEnable   (1 << 1)
#define kUniNPCI2ClockEnable       (1 << 0)

#define kUniNPowerMngmnt           (0x0030)
#define kUniNNormal                (0x00)
#define kUniNIdle2                 (0x01)
#define kUniNSleep                 (0x02)
#define kUniNSave                  (0x03)

#define kUniNArbCtrl               (0x0040)
#define kUniNArbCtrlQAckDelayShift (15)
#define kUniNArbCtrlQAckDelayMask  (0x0e1f8000)
#define kUniNArbCtrlQAckDelay      (0x30)
#define kUniNArbCtrlQAckDelay105   (0x00)

#define kUniNAACKCtrl                  (0x0100)
#define kUniNAACKCtrlDelayEnableMask   (0x00000001)
#define kUniNAACKCtrlDelayEnable       (0x00000001)

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

#define kUniNVSPSoftReset			(0x170)

#define kUniNMPCIMemTimeout			(0x2160)
#define kUniNMPCIMemTimeoutMask		(0xFF000000)
#define kUniNMPCIMemGrantTime		(0x0 << 28)

#define kUniNUATAReset				(0x02000000)
#define kUniNUATAEnable				(0x01000000)

// Uni-N 1.5 Performance Monitoring Registers
#define kUniNMMCR					(0x0F00)
#define kUniNMCMDR					(0x0F10)
#define kUniNMPMC1					(0x0F20)
#define kUniNMPMC2					(0x0F30)
#define kUniNMPMC3					(0x0F40)
#define kUniNMPMC4					(0x0F50)	

#define kIOPCICacheLineSize 	"IOPCICacheLineSize"
#define kIOPCITimerLatency		"IOPCITimerLatency"
#define kAAPLSuspendablePorts	"AAPL,SuspendablePorts"

#define kUniNSetPowerState		"UniNSetPowerState"
#define kUniNSetAACKDelay		"UniNSetAACKDelay"

class AppleUniN: public ApplePlatformExpert
{
    OSDeclareDefaultStructors(AppleUniN)

public:

    virtual  bool start( IOService * nub );
    virtual void free();
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, 
		void *param1, void *param2, void *param3, void *param4);
    virtual IOReturn callPlatformFunction(const char *functionName, bool waitForFunction, 
		void *param1, void *param2, void *param3, void *param4);

	virtual IOReturn setupUATAforSleep ();
	virtual IOReturn readIntrepidClockStopStatus (UInt32 *status0, UInt32 *status1);
	virtual void enableUniNEthernetClock(bool enable, IOService *nub);
	virtual void enableUniNFireWireClock(bool enable, IOService *nub);
	virtual IOReturn accessUniN15PerformanceRegister(bool write, long regNumber, UInt32 *data);

private:
	IOMemoryMap		* uniNMemory;
	UInt32			* uniNBaseAddress;
    UInt32					uniNVersion;
	bool					uataBusWasReset;
    IOService				*provider;
	// this is to ensure mutual exclusive access to the Uni-N registers:
	IOSimpleLock 			*mutex;
	OSArray 				*platformFuncArray;
    IOMemoryMap				*uATABaseAddressMap;
	volatile UInt32			*uATABaseAddress;
    UInt32					uATAFCR;
    UInt32					saveVSPSoftReset;
	bool					hostIsMobile;
	const OSSymbol			*symReadIntrepidClockStopStatus;

    virtual UInt32 readUniNReg(UInt32 offset);
    virtual void writeUniNReg(UInt32 offset, UInt32 data);
	virtual UInt32 safeReadRegUInt32(UInt32 offset);
	virtual void safeWriteRegUInt32(UInt32 offset, UInt32 mask, UInt32 data);
	virtual void configureUniNPCIDevice (IOService *nub);
	virtual void uniNSetPowerState (UInt32 state);
	virtual void setAACKDelay (UInt32 setDelayBit);

	virtual bool performFunction(const IOPlatformFunction *func, void *param1 = 0,
			void *param2 = 0, void *param3 = 0, void *param4 = 0);
	virtual IOPCIDevice* findNubForPHandle( UInt32 pHandleValue );

};

#endif /*  _IOKIT_APPLE_UNIN_H */
