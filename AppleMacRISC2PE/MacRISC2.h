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
#include <IOKit/pci/IOAGPDevice.h>

#include "UniN.h"
#include "MacRISC2CPU.h"
#include "IOPlatformFunction.h"

#include "IOPlatformMonitor.h"

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
  kIOPMClamshellStateOnWake 	= (1<<10),   
  kIOPMWrongACConnected 		= (1<<11) 
};
#endif

class MacRISC2CPU;
class MacRISC2PE : public ApplePlatformExpert
{
    OSDeclareDefaultStructors(MacRISC2PE);
  
    friend class MacRISC2CPU;
    friend class Portable_PlatformMonitor;
    friend class Portable2003_PlatformMonitor;
    friend class Portable2004_PlatformMonitor;
 
private:
	struct PlatformPowerBits {
		UInt32 bitsXor;
		UInt32 bitsMask;
	};
	
    const char 					*provider_name;
    unsigned long				uniNVersion;
    IOService					*usb1;
    IOService					*usb2;
    IOService					*keylargoUSB1;
    IOService					*keylargoUSB2;
	IOService					*gpuSensor;
	IOAGPDevice					*atiDriver;
	IOPCIBridge					*agpBridgeDriver;
	MacRISC2CPU					*macRISC2CPU;
	AppleUniN					*uniN;
    class IOPMSlotsMacRISC2		*slotsMacRISC2;
    IOLock						*mutex;
    UInt32						processorSpeedChangeFlags;
	bool						isPortable;
	bool						doPlatformPowerMonitor;
    bool						hasPMon, hasPPlugin;
    bool						hasEmbededThermals;
	bool						i2ResetOnWake;
    UInt32						pMonPlatformNumber;
    
	// Possible power states we care about
    PlatformPowerBits		powerMonWeakCharger;
    PlatformPowerBits		powerMonBatteryWarning;
    PlatformPowerBits		powerMonBatteryDepleted;
    PlatformPowerBits		powerMonBatteryNotInstalled;
    PlatformPowerBits		powerMonClamshellClosed;
	
	IOService				*ioPMonNub;
	IOPlatformMonitor		*ioPMon;
	IOService				*ioPPlugin;
	IOService				*plFuncNub;
    
    void determinePlatformNumber( void );

    void getDefaultBusSpeeds(long *numSpeeds, unsigned long **speedList);
    void enableUniNEthernetClock(bool enable, IOService *nub);
    void enableUniNFireWireClock(bool enable, IOService *nub);
    void enableUniNFireWireCablePower(bool enable);
    IOReturn accessUniN15PerformanceRegister(bool write, long regNumber, unsigned long *data);
    IOReturn platformPluginPowerMonitor(UInt32 *powerFlags);
    IOReturn platformPowerMonitor(UInt32 *powerFlags);
  
    void PMInstantiatePowerDomains ( void );
    void PMRegisterDevice(IOService * theNub, IOService * theDevice);
    IORegistryEntry * retrievePowerMgtEntry (void);

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
    kBusSlewBasedSpeedChange		= (1 << 7),
    kSupportsPowerPlay				= (1 << 8),
	// Hi bits reflect current state information
	kProcessorFast					= (1 << 31),
	kL3CacheEnabled					= (1 << 30),
	kL2CacheEnabled					= (1 << 29)
};


#endif /* ! _IOKIT_MACRISC2_H */
