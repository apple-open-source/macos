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


#ifndef _IOKIT_MACRISC2_H
#define _IOKIT_MACRISC2_H

#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "PlatformFunctions.h"

// Indices for accessing the init, term, sleep and wake platform command lists
enum {
	kListOnInit		= 0,
	kListOnTerm		= 1,
	// sleep and wake will be implemented in a later release
	//#define kListOnSleep	2
	//#define kListOnWake		3
	kListNumLists	= kListOnTerm + 1
};

enum
{
	kMacRISC2TypeUnknown = kMachineTypeUnknown,
    kMacRISC2TypePowerMac,
    kMacRISC2TypePowerBook,
};

#if IOPM_POWER_SOURCE_REV < 2
enum {
  kIOPMACInstalled      		= (1<<0),	// Delete all these when IOPM.h updated
  kIOPMBatteryCharging  		= (1<<1),
  kIOPMBatteryInstalled 		= (1<<2),
  kIOPMUPSInstalled     		= (1<<3),
  kIOPMBatteryAtWarn    		= (1<<4),
  kIOPMBatteryDepleted  		= (1<<5),
  kIOPMACnoChargeCapability 	= (1<<6),    
  kIOPMRawLowBattery    		= (1<<7),       
  kIOPMForceLowSpeed    		= (1<<8),        
  kIOPMClosedClamshell  		= (1<<9),       
  kIOPMClamshellStateOnWake 	= (1<<10)   
};
#endif
class MacRISC2PE : public ApplePlatformExpert
{
    OSDeclareDefaultStructors(MacRISC2PE);
  
    friend class MacRISC2CPU;
  
private:
	struct PlatformPowerBits {
		UInt32 bitsSet;
		UInt32 bitsClear;
		UInt32 bitsMask;
	};
	
    const char 				*provider_name;
    unsigned long			*uniNBaseAddress;
    unsigned long			uniNVersion;
    IOService				*usb1;
    IOService				*usb2;
    IOService				*keylargoUSB1;
    IOService				*keylargoUSB2;
	IOService				*macRISC2CPU;
    class IOPMPagingPlexus	*plexus;
    class IOPMSlotsMacRISC2	*slotsMacRISC2;
    IOLock					*mutex;
    UInt32					processorSpeedChangeFlags;
	bool					isPortable;
	bool					doPlatformPowerMonitor;
	// Possible power states we care about
    PlatformPowerBits		powerMonWeakCharger;
    PlatformPowerBits		powerMonBatteryWarning;
    PlatformPowerBits		powerMonBatteryDepleted;
    PlatformPowerBits		powerMonBatteryNotInstalled;
    PlatformPowerBits		powerMonClamshellClosed;
	// Possible power state we might set
    PlatformPowerBits		powerMonForceLowPower;
	OSDictionary *fOnDemand;	// on-demand services are stored as pairs of
    								// func-name:cmd-data in this dict
	OSSet *fFuncList[kListNumLists];	// lists of services to perform for events
	
    virtual unsigned long readUniNReg(unsigned long offset);
    virtual void writeUniNReg(unsigned long offset, unsigned long data);
  
    void getDefaultBusSpeeds(long *numSpeeds, unsigned long **speedList);
    void enableUniNEthernetClock(bool enable, IOService *nub);
    void enableUniNFireWireClock(bool enable, IOService *nub);
    void enableUniNFireWireCablePower(bool enable);
	void configureUniNPCIDevice (IOService *nub);
    IOReturn accessUniN15PerformanceRegister(bool write, long regNumber, unsigned long *data);
    IOReturn platformPowerMonitor(UInt32 *powerFlags);
  
    void PMInstantiatePowerDomains ( void );
    void PMRegisterDevice(IOService * theNub, IOService * theDevice);
    IORegistryEntry * retrievePowerMgtEntry (void);

	// helper functions
	void publishStrings(OSCollection *strings);
	void releaseResources(void);
	
	bool performFunction(const OSData *cmd, void *param1,
			void *param2, void *param3, void *param4);
			
	bool performFunctionList(const OSSet *funcList);

	SInt32 parseProvidedFunction(OSString *key,	OSData *value);
	IOPCIDevice *findNubForPHandle( UInt32 pHandleValue );
	
public:
    virtual bool start(IOService *provider);
    virtual bool platformAdjustService(IOService *service);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);
};

// Processor Speed Change Flags
enum {
	// Low bits reflect types of changes to make
    kNoSpeedChange 					= 0,
    kPMUBasedSpeedChange			= (1 << 0),
    kProcessorBasedSpeedChange		= (1 << 1),
    kDisableL2SpeedChange			= (1 << 2),
    kDisableL3SpeedChange			= (1 << 3),
	kClamshellClosedSpeedChange		= (1 << 4),
	kEnvironmentalSpeedChange		= (1 << 5),
	kForceDisableL3					= (1 << 6),
	// Hi bits reflect current state information
	kProcessorFast					= (1 << 31),
	kL3CacheEnabled					= (1 << 30),
	kL2CacheEnabled					= (1 << 29)
};

// Uni-North Register Information

#define kUniNVersion               (0x0000)
#define kUniNVersion107            (0x0003)
#define kUniNVersion10A            (0x0007)
#define kUniNVersion150            (0x0011)
#define kUniNVersion200            (0x0024)
#define kUniNVersionPangea         (0x00C0)

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

#define kUniNMPCIMemTimeout	   (0x2160)
#define kUniNMPCIMemTimeoutMask    (0xFF000000)
#define kUniNMPCIMemGrantTime      (0x0 << 28)

// Uni-N 1.5 Performance Monitoring Registers
#define kUniNMMCR                  (0x0F00)
#define kUniNMCMDR                 (0x0F10)
#define kUniNMPMC1                 (0x0F20)
#define kUniNMPMC2                 (0x0F30)
#define kUniNMPMC3                 (0x0F40)
#define kUniNMPMC4                 (0x0F50)	

#define kIOPCICacheLineSize 	"IOPCICacheLineSize"
#define kIOPCITimerLatency		"IOPCITimerLatency"
#define kAAPLSuspendablePorts	"AAPL,SuspendablePorts"

#endif /* ! _IOKIT_MACRISC2_H */
