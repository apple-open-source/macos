/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
#include <sys/cdefs.h>

__BEGIN_DECLS
#include <ppc/proc_reg.h>
#include <ppc/machine_routines.h>

/* Map memory map IO space */
#include <mach/mach_types.h>
extern vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
__END_DECLS

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include "MacRISC2.h"
#include <IOKit/pci/IOPCIDevice.h>

static unsigned long macRISC2Speed[] = { 0, 1 };

#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOPMSlotsMacRISC2.h"
#include "IOPMUSBMacRISC2.h"
#include <IOKit/pwr_mgt/IOPMPagingPlexus.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>

extern char *gIOMacRISC2PMTree;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super ApplePlatformExpert

OSDefineMetaClassAndStructors(MacRISC2PE, ApplePlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool MacRISC2PE::start(IOService *provider)
{
    long            		machineType;
    OSData          		*tmpData;
    IORegistryEntry 		*uniNRegEntry;
    IORegistryEntry 		*powerMgtEntry;
    UInt32			   		*primInfo;
    UInt32			   		uniNArbCtrl, uniNBaseAddressTemp, uniNMPCIMemTimeout;
	UInt32					stepType;
	OSDictionary			*propTable;
	OSCollectionIterator	*propIter;
	OSString				*propKey;
	OSData					*propVal;
	SInt32					numFuncs, retval;
	
    setChipSetType(kChipSetTypeCore2001);
	
		
    // Set the machine type.
    provider_name = provider->getName();  

	machineType = kMacRISC2TypeUnknown;
	doPlatformPowerMonitor = false;
	if (provider_name != NULL) {
		if (0 == strncmp(provider_name, "PowerMac", strlen("PowerMac")))
			machineType = kMacRISC2TypePowerMac;
		else if (0 == strncmp(provider_name, "RackMac", strlen("RackMac")))
			machineType = kMacRISC2TypePowerMac;
		else if (0 == strncmp(provider_name, "PowerBook", strlen("PowerBook")))
			machineType = kMacRISC2TypePowerBook;
		else if (0 == strncmp(provider_name, "iBook", strlen("iBook")))
			machineType = kMacRISC2TypePowerBook;
		else	// kMacRISC2TypeUnknown
			IOLog ("AppleMacRISC2PE - warning: unknown machineType\n");
	}
	
	isPortable = (machineType == kMacRISC2TypePowerBook);
	
    setMachineType(machineType);
	
    // Get the bus speed from the Device Tree.
    tmpData = OSDynamicCast(OSData, provider->getProperty("clock-frequency"));
    if (tmpData == 0) return false;
    macRISC2Speed[0] = *(unsigned long *)tmpData->getBytesNoCopy();
   
    // Get a memory mapping for Uni-N's registers.
    uniNRegEntry = provider->childFromPath("uni-n", gIODTPlane);
    if (uniNRegEntry == 0) return false;
    tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("reg"));
    if (tmpData == 0) return false;
    uniNBaseAddressTemp = ((unsigned long *)tmpData->getBytesNoCopy())[0];
    uniNBaseAddress = (unsigned long *)ml_io_map(uniNBaseAddressTemp, 0x3200);
    if (uniNBaseAddress == 0) return false;
  
    // Set QAckDelay depending on the version of Uni-N.
    uniNVersion = readUniNReg(kUniNVersion);

    if (uniNVersion < kUniNVersion150)
    {
        uniNArbCtrl = readUniNReg(kUniNArbCtrl);
        uniNArbCtrl &= ~kUniNArbCtrlQAckDelayMask;

        if (uniNVersion < kUniNVersion107)
        {
            uniNArbCtrl |= kUniNArbCtrlQAckDelay105 << kUniNArbCtrlQAckDelayShift;
        } 
        else
        {
            uniNArbCtrl |= kUniNArbCtrlQAckDelay << kUniNArbCtrlQAckDelayShift;
        }
        writeUniNReg(kUniNArbCtrl, uniNArbCtrl);
    }

    // Set Max/PCI Memory Timeout for appropriate Uni-N.   
    if ( ((uniNVersion >= kUniNVersion150) && (uniNVersion <= kUniNVersion200)) || 
         (uniNVersion == kUniNVersionPangea) )
    {
        uniNMPCIMemTimeout = readUniNReg(kUniNMPCIMemTimeout);
        uniNMPCIMemTimeout &= ~kUniNMPCIMemTimeoutMask;
        uniNMPCIMemTimeout |= kUniNMPCIMemGrantTime;
        writeUniNReg(kUniNMPCIMemTimeout, uniNMPCIMemTimeout);
    }
	
	// Parse platform function commands for uni-n
	numFuncs = 0;
	
	if ( ((propTable = uniNRegEntry->dictionaryWithProperties()) == 0) ||
	     ((propIter = OSCollectionIterator::withCollection(propTable)) == 0) ) {
		if (propTable) propTable->release();
		return(false);
	}

	while ((propKey = OSDynamicCast(OSString, propIter->getNextObject())) != 0) {
		if (strncmp(kFunctionProvidedPrefix,
		            propKey->getCStringNoCopy(),
		            strlen(kFunctionProvidedPrefix)) == 0) {
			propVal = OSDynamicCast(OSData, propTable->getObject(propKey));

			if ((retval = parseProvidedFunction(propKey, propVal)) < 0) {
				// something went wrong, free resources and bail out
				releaseResources();
				numFuncs = 0;
				break;
 			} else
 				numFuncs += retval;
 		}
    }
	
	if (propTable) { propTable->release(); propTable = 0; }
	if (propIter) { propIter->release(); propIter = 0; }

	// publish on-demand and services in IOResources
	if (fOnDemand) publishStrings(fOnDemand);

   // Creates the nubs for the children of uni-n
    IOService *uniNServiceEntry = OSDynamicCast(IOService, uniNRegEntry);
    if (uniNServiceEntry != NULL)
        createNubs(this, uniNRegEntry->getChildIterator( gIODTPlane ));
  
    // Get PM features and private features
    powerMgtEntry = retrievePowerMgtEntry ();
    if (powerMgtEntry == 0)
    {
        kprintf ("didn't find power mgt node\n");
        return false;
    }

    tmpData  = OSDynamicCast(OSData, powerMgtEntry->getProperty ("prim-info"));
    if (tmpData != 0)
    {
        primInfo = (unsigned long *)tmpData->getBytesNoCopy();
        if (primInfo != 0)
        {
            _pePMFeatures            = primInfo[3];
            _pePrivPMFeatures        = primInfo[4];
            _peNumBatteriesSupported = ((primInfo[6]>>16) & 0x000000FF);
            kprintf ("Public PM Features: %0x.\n",_pePMFeatures);
            kprintf ("Privat PM Features: %0x.\n",_pePrivPMFeatures);
            kprintf ("Num Internal Batteries Supported: %0x.\n", _peNumBatteriesSupported);
        }
    }
  
    // This is to make sure that  is PMRegisterDevice reentrant
    mutex = IOLockAlloc();
    if (mutex == NULL)
		return false;
    else
		IOLockInit( mutex );
	
    // Set up processorSpeedChangeFlags depending on platform
	processorSpeedChangeFlags = kNoSpeedChange;
	stepType = 0;
    if (machineType == kMacRISC2TypePowerBook) {
		OSIterator 		*childIterator;
		IORegistryEntry *cpuEntry, *powerPCEntry;
		OSData			*cpuSpeedData, *stepTypeData;

		// locate the first PowerPC,xx cpu node so we can get clock properties
		cpuEntry = provider->childFromPath("cpus", gIODTPlane);
		if ((childIterator = cpuEntry->getChildIterator (gIODTPlane)) != NULL) {
			while ((powerPCEntry = (IORegistryEntry *)(childIterator->getNextObject ())) != NULL) {
				if (!strncmp ("PowerPC", powerPCEntry->getName(gIODTPlane), strlen ("PowerPC"))) {
					// Look for dynamic power step feature
					stepTypeData = OSDynamicCast( OSData, powerPCEntry->getProperty( "dynamic-power-step" ));
					if (stepTypeData)
						processorSpeedChangeFlags = kProcessorBasedSpeedChange | kProcessorFast | 
							kL3CacheEnabled | kL2CacheEnabled;
					else {	// Look for forced-reduced-speed case
						stepTypeData = OSDynamicCast( OSData, powerPCEntry->getProperty( "force-reduced-speed" ));
						cpuSpeedData = OSDynamicCast( OSData, powerPCEntry->getProperty( "max-clock-frequency" ));
						if (stepTypeData && cpuSpeedData) {
							UInt32 newCPUSpeed, newNum;
							
							doPlatformPowerMonitor = true;
				
							// At minimum disable L3 cache
							// Note that caches are enabled at this point, but the processor may not be at full speed.
							processorSpeedChangeFlags = kDisableL3SpeedChange | kL3CacheEnabled | kL2CacheEnabled;

							if (stepTypeData->getLength() > 0)
								stepType = *(UInt32 *) stepTypeData->getBytesNoCopy();
				
							newCPUSpeed = *(UInt32 *) cpuSpeedData->getBytesNoCopy();
							if (newCPUSpeed != gPEClockFrequencyInfo.cpu_clock_rate_hz) {
								// If max cpu speed is greater than what OF reported to us
								// then enable PMU speed change in addition to L3 speed change
								if ((_pePrivPMFeatures & (1 << 17)) != 0)
									processorSpeedChangeFlags |= kPMUBasedSpeedChange;
								processorSpeedChangeFlags |= kEnvironmentalSpeedChange;
								// Also fix up internal clock rates
								newNum = newCPUSpeed / (gPEClockFrequencyInfo.cpu_clock_rate_hz /
														gPEClockFrequencyInfo.bus_to_cpu_rate_num);
								gPEClockFrequencyInfo.bus_to_cpu_rate_num = newNum;		// Set new numerator
								gPEClockFrequencyInfo.cpu_clock_rate_hz = newCPUSpeed;	// Set new speed
							}
						} else // All other notebooks
							if ((_pePrivPMFeatures & (1 << 17)) != 0)
								processorSpeedChangeFlags = kPMUBasedSpeedChange | kProcessorFast | 
									kL3CacheEnabled | kL2CacheEnabled;
					}
					break;
				}
			}
			childIterator->release();
		}
	}
    
	// Init power monitor states.  This should be driven by data in the device-tree
	if (doPlatformPowerMonitor) {
		
		powerMonWeakCharger.bitsSet = kIOPMACInstalled | kIOPMACnoChargeCapability;
		powerMonWeakCharger.bitsClear = 0;
		powerMonWeakCharger.bitsMask = powerMonWeakCharger.bitsSet | powerMonWeakCharger.bitsClear;
		
		powerMonBatteryWarning.bitsSet = kIOPMRawLowBattery;
		powerMonBatteryWarning.bitsClear = 0;
		powerMonBatteryWarning.bitsMask = powerMonBatteryWarning.bitsSet | powerMonBatteryWarning.bitsClear;
		
		powerMonBatteryDepleted.bitsSet = kIOPMBatteryDepleted;
		powerMonBatteryDepleted.bitsClear = 0;
		powerMonBatteryDepleted.bitsMask = powerMonBatteryDepleted.bitsSet | powerMonBatteryDepleted.bitsClear;
		
		powerMonBatteryNotInstalled.bitsSet = 0;
		powerMonBatteryNotInstalled.bitsClear = kIOPMBatteryInstalled;
		powerMonBatteryNotInstalled.bitsMask = powerMonBatteryNotInstalled.bitsSet | powerMonBatteryNotInstalled.bitsClear;
		
		if ((stepType & 1) == 0) {
			powerMonClamshellClosed.bitsSet = kIOPMClosedClamshell;
			powerMonClamshellClosed.bitsClear = 0;
			powerMonClamshellClosed.bitsMask = powerMonClamshellClosed.bitsSet | powerMonClamshellClosed.bitsClear;
		} else {	// Don't do anything on clamshell closed
			powerMonClamshellClosed.bitsMask = powerMonClamshellClosed.bitsSet = 0xFFFFFFFF;
			powerMonClamshellClosed.bitsClear = 0;
		}

		powerMonForceLowPower.bitsSet = kIOPMForceLowSpeed;
		powerMonForceLowPower.bitsClear = 0;
		powerMonForceLowPower.bitsMask = powerMonForceLowPower.bitsSet | powerMonForceLowPower.bitsClear;

	} else { // Assume no power monitoring
		powerMonWeakCharger.bitsMask = powerMonWeakCharger.bitsSet = 0xFFFFFFFF;
		powerMonWeakCharger.bitsClear = 0;
		powerMonBatteryWarning.bitsMask = powerMonBatteryWarning.bitsSet = 0xFFFFFFFF;
		powerMonBatteryWarning.bitsClear = 0;
		powerMonBatteryDepleted.bitsMask = powerMonBatteryDepleted.bitsSet = 0xFFFFFFFF;
		powerMonBatteryDepleted.bitsClear = 0;
		powerMonBatteryNotInstalled.bitsMask = powerMonBatteryNotInstalled.bitsSet = 0xFFFFFFFF;
		powerMonBatteryNotInstalled.bitsClear = 0;
		powerMonClamshellClosed.bitsMask = powerMonClamshellClosed.bitsSet = 0xFFFFFFFF;
		powerMonClamshellClosed.bitsClear = 0;
		powerMonForceLowPower.bitsMask = 0xFFFFFFFF;
		powerMonForceLowPower.bitsSet = powerMonForceLowPower.bitsClear = 0;	// Assume we never set low power
	}

    return super::start(provider);
}

IORegistryEntry * MacRISC2PE::retrievePowerMgtEntry (void)
{
    IORegistryEntry *     theEntry = 0;
    IORegistryEntry *     anObj = 0;
    IORegistryIterator *  iter;
    OSString *            powerMgtNodeName;

    iter = IORegistryIterator::iterateOver (IORegistryEntry::getPlane(kIODeviceTreePlane), kIORegistryIterateRecursively);
    if (iter)
    {
        powerMgtNodeName = OSString::withCString("power-mgt");
        anObj = iter->getNextObject ();
        while (anObj)
        {
            if (anObj->compareName(powerMgtNodeName))
            {
                theEntry = anObj;
                break;
            }
            anObj = iter->getNextObject();
        }
    
        powerMgtNodeName->release();
        iter->release ();
    }

    return theEntry;
}

bool MacRISC2PE::platformAdjustService(IOService *service)
{
    const OSSymbol *tmpSymbol, *keySymbol;
    bool           result;
  
    if (IODTMatchNubWithKeys(service, "open-pic"))
    {
        keySymbol = OSSymbol::withCStringNoCopy("InterruptControllerName");
        tmpSymbol = IODTInterruptControllerName(service);
        result = service->setProperty(keySymbol, tmpSymbol);
        return true;
    }

    if (!strcmp(service->getName(), "programmer-switch"))
    {
        // Set property to tell AppleNMI to mask/unmask NMI @ sleep/wake
        service->setProperty("mask_NMI", service); 
        return true;
    }
  
    if (!strcmp(service->getName(), "pmu"))
    {
        // Change the interrupt mapping for pmu source 4.
        OSArray              *tmpArray;
        OSCollectionIterator *extIntList;
        IORegistryEntry      *extInt;
        OSObject             *extIntControllerName;
        OSObject             *extIntControllerData;
    
        // Set the no-nvram property.
        service->setProperty("no-nvram", service);
    
        // Find the new interrupt information.
        extIntList = IODTFindMatchingEntries(getProvider(), kIODTRecursive, "'extint-gpio1'");
        extInt = (IORegistryEntry *)extIntList->getNextObject();
    
        tmpArray = (OSArray *)extInt->getProperty(gIOInterruptControllersKey);
        extIntControllerName = tmpArray->getObject(0);
        tmpArray = (OSArray *)extInt->getProperty(gIOInterruptSpecifiersKey);
        extIntControllerData = tmpArray->getObject(0);
    
        // Replace the interrupt infomation for pmu source 4.
        tmpArray = (OSArray *)service->getProperty(gIOInterruptControllersKey);
        tmpArray->replaceObject(4, extIntControllerName);
        tmpArray = (OSArray *)service->getProperty(gIOInterruptSpecifiersKey);
        tmpArray->replaceObject(4, extIntControllerData);
    
        extIntList->release();
        
        return true;
    }

    if (!strcmp(service->getName(), "via-pmu"))
    {
        service->setProperty("BusSpeedCorrect", this);
        return true;
    }
    
	/*
	 * For uni-n pci version 1.5 which services mac-io, tell pci driver to disable read data gating
	 * -- Note that previous versions of this driver defined version 1.5 as 0x0010.  This is not 
	 * correct and we now use 0x0011 for uni-n 1.5.
	 */
	if ((uniNVersion == kUniNVersion150) && IODTMatchNubWithKeys(service, "('pci', 'uni-north')") && 
		(service->childFromPath("mac-io", gIODTPlane) != NULL)) {
			service->setProperty ("DisableRDG", true);
			return true;
	}
    
	/* 
	 * For Firewire on uni-n Pangea & uni-n 1.5 through 2.0, adjust the cache line size and timer latency
	 */
	if (((uniNVersion >= kUniNVersion150) && (uniNVersion <= kUniNVersion200) || (uniNVersion == kUniNVersionPangea)) &&
		(!strcmp(service->getName(gIODTPlane), "firewire")) &&
		IODTMatchNubWithKeys(service->getParentEntry(gIODTPlane), "('pci', 'uni-north')")) {
			char data;
			
			data = 0x08;		// 32 byte cache line
			service->setProperty (kIOPCICacheLineSize, &data, 1);
			data = 0x40;		// latency timer to 40
			service->setProperty (kIOPCITimerLatency, &data, 1);
			return true;
	}
    
	/* 
	 * For Ethernet on uni-n 2.0, adjust the cache line size and timer latency
	 */
	if ((uniNVersion == kUniNVersion200) &&
		IODTMatchNubWithKeys(service, "('ethernet', 'gmac')") &&
		IODTMatchNubWithKeys(service->getParentEntry(gIODTPlane), "('pci', 'uni-north')")) {
			char 	data;
			long	cacheSize;
			OSData 	*cacheData;
			
			data = 0x08; 	// assume default of 32 byte cache line
			cacheData = OSDynamicCast( OSData, service->getProperty( "cache-line-size" ) );
			if (cacheData) {
				cacheSize = *(long *)cacheData->getBytesNoCopy();
				data = (cacheSize >> 2);
			}
			service->setProperty (kIOPCICacheLineSize, &data, 1);
			data = 0x20;		// latency timer to 20
			service->setProperty (kIOPCITimerLatency, &data, 1);
			return true;
	}
	
	// For usb@18 on PowerBook4,x machines add an "AAPL,SuspendablePorts" property if it doesn't exist
	if (!strcmp(service->getName(), "usb") && 
		(0 == strncmp(provider_name, "PowerBook4,", strlen("PowerBook4,"))) &&
		IODTMatchNubWithKeys(service->getParentEntry(gIODTPlane), "('pci', 'uni-north')")) {
		
		OSData				*regProp;
		IOPCIAddressSpace	*pciAddress;
		UInt32				ports;
	
		if( (regProp = (OSData *) service->getProperty("reg"))) {
			pciAddress = (IOPCIAddressSpace *) regProp->getBytesNoCopy();
			// Only for usb@18
			if (pciAddress->s.deviceNum == 0x18) {
				// If property doesn't exist, create it
				if(!((OSData *) service->getProperty(kAAPLSuspendablePorts))) {
					ports = 4;
					service->setProperty (kAAPLSuspendablePorts, &ports, sizeof(UInt32));
				}
				return true;
			}
		}
	}
    
    return true;
}

IOReturn MacRISC2PE::callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
					void *param1, void *param2,
					void *param3, void *param4)
{
	OSData		*func;

    if (functionName == gGetDefaultBusSpeedsKey)
    {
        getDefaultBusSpeeds((long *)param1, (unsigned long **)param2);
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("EnableUniNEthernetClock"))
    {
        enableUniNEthernetClock((bool)param1, (IOService *)param2);
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo("EnableFireWireClock")) {
        enableUniNFireWireClock((bool)param1, (IOService *)param2);
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("EnableFireWireCablePower")) {
        enableUniNFireWireCablePower((bool)param1);
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("AccessUniN15PerformanceRegister"))
    {
        return accessUniN15PerformanceRegister((bool)param1, (long)param2, (unsigned long *)param3);
    }
  
    if (functionName->isEqualTo("PlatformIsPortable")) {
		*(bool *) param1 = isPortable;
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("PlatformPowerMonitor")) {
		return platformPowerMonitor ((UInt32 *) param1);
    }
  
	// Is this an on-demand platform function?
	if (fOnDemand && (func = OSDynamicCast(OSData, fOnDemand->getObject(functionName))) != 0) {
		if (performFunction(func, param1, param2, param3, param4))
			return kIOReturnSuccess;
		else
			return kIOReturnError;
	}

    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

unsigned long MacRISC2PE::readUniNReg(unsigned long offset)
{
    return uniNBaseAddress[offset / 4];
}

void MacRISC2PE::writeUniNReg(unsigned long offset, unsigned long data)
{
    uniNBaseAddress[offset / 4] = data;
    eieio();
}

void MacRISC2PE::getDefaultBusSpeeds(long *numSpeeds, unsigned long **speedList)
{
    if ((numSpeeds == 0) || (speedList == 0)) return;
  
    *numSpeeds = 1;
    *speedList = macRISC2Speed;
}

void MacRISC2PE::enableUniNEthernetClock(bool enable, IOService *nub)
{
    unsigned long 	regTemp;

	if (enable)
		configureUniNPCIDevice(nub);		

    if (mutex != NULL)
        IOLockLock(mutex);
  
    regTemp = readUniNReg(kUniNClockControl);
  
    if (enable)
    {
        regTemp |= kUniNEthernetClockEnable;
    } 
    else
    {
        regTemp &= ~kUniNEthernetClockEnable;
    }
  
    writeUniNReg(kUniNClockControl, regTemp);

    if (mutex != NULL)
        IOLockUnlock(mutex);
}

void MacRISC2PE::enableUniNFireWireClock(bool enable, IOService *nub)
{
    unsigned long 	regTemp;

	if (enable)
		configureUniNPCIDevice(nub);		

    //IOLog("FWClock, enable = %d kFW = %x\n", enable, kUniNFirewireClockEnable);
  
    if (mutex != NULL)
        IOLockLock(mutex);

    regTemp = readUniNReg(kUniNClockControl);

    if (enable)
    {
        regTemp |= kUniNFirewireClockEnable;
    } 
    else
    {
        regTemp &= ~kUniNFirewireClockEnable;
    }
  
    writeUniNReg(kUniNClockControl, regTemp);

    if (mutex != NULL)
        IOLockUnlock(mutex);
}

void MacRISC2PE::enableUniNFireWireCablePower(bool enable)
{
    // Turn off cable power supply on mid/merc/pismo(on pismo only, this kills the phy)

    if(getMachineType() == kMacRISC2TypePowerBook)
    {
        IOService *keyLargo;
        keyLargo = waitForService(serviceMatching("KeyLargo"));
        
        if(keyLargo)
        {
            UInt32 gpioOffset = 0x73;
            
            keyLargo->callPlatformFunction(OSSymbol::withCString("keyLargo_writeRegUInt8"),
                    true, (void *)&gpioOffset, (void *)(enable ? 0:4), 0, 0);
        }
    }
}

void MacRISC2PE::configureUniNPCIDevice (IOService *nub)
{
	OSData			*cacheData, *latencyData;
	IOPCIDevice		*provider;

	if (nub) provider = OSDynamicCast(IOPCIDevice, nub);
	else provider = NULL;
	
	if (provider) {
		cacheData = (OSData *)provider->getProperty (kIOPCICacheLineSize);
		latencyData = (OSData *)provider->getProperty (kIOPCITimerLatency);
	
		if (cacheData || latencyData) {
			UInt32				configData;	

			configData = provider->configRead32 (kIOPCIConfigCacheLineSize);
				
			if (cacheData) 
				configData = (configData & 0xFFFFFF00) | *(char *) cacheData->getBytesNoCopy();
			
			if (latencyData) 
				configData = (configData & 0xFFFF00FF) | ((*(char *) latencyData->getBytesNoCopy()) << 8);
							
			provider->configWrite32 (kIOPCIConfigCacheLineSize, configData);
		}
	}
	return;
}

enum
{
  kMCMonitorModeControl = 0,
  kMCCommand,
  kMCPerformanceMonitor0,
  kMCPerformanceMonitor1,
  kMCPerformanceMonitor2,
  kMCPerformanceMonitor3
};

IOReturn MacRISC2PE::accessUniN15PerformanceRegister(bool write, long regNumber, unsigned long *data)
{
    unsigned long offset;
  
    if (uniNVersion < kUniNVersion150) return kIOReturnUnsupported;
  
    switch (regNumber)
    {
    case kMCMonitorModeControl  : offset = kUniNMMCR; break;
    case kMCCommand             : offset = kUniNMCMDR; break;
    case kMCPerformanceMonitor0 : offset = kUniNMPMC1; break;
    case kMCPerformanceMonitor1 : offset = kUniNMPMC2; break;
    case kMCPerformanceMonitor2 : offset = kUniNMPMC3; break;
    case kMCPerformanceMonitor3 : offset = kUniNMPMC4; break;
    default                     : return kIOReturnBadArgument;
    }
  
    if (data == 0) return kIOReturnBadArgument;
  
    if (write)
    {
        writeUniNReg(offset, *data);
    } 
    else 
    {
        *data = readUniNReg(offset);
    }
  
    return kIOReturnSuccess;
}

//*********************************************************************************
// platformPowerMonitor
//
// A call platform function call called by the ApplePMU driver.  ApplePMU call us
// with a set of power flags.  We examine those flags and modify the state
// according to the characteristics of the platform. 
//
// If necessary, we force an immediate change in the power state
//*********************************************************************************
IOReturn MacRISC2PE::platformPowerMonitor(UInt32 *powerFlags)
{
	IOReturn	result;
	
	if (doPlatformPowerMonitor) {
		// First check primary power conditions
		if (((*powerFlags & powerMonWeakCharger.bitsMask) == 
				(powerMonWeakCharger.bitsMask & powerMonWeakCharger.bitsSet & ~powerMonWeakCharger.bitsClear)) ||
			((*powerFlags & powerMonBatteryWarning.bitsMask) == 
				(powerMonBatteryWarning.bitsMask & powerMonBatteryWarning.bitsSet & ~powerMonBatteryWarning.bitsClear)) ||
			((*powerFlags & powerMonBatteryDepleted.bitsMask) == 
				(powerMonBatteryDepleted.bitsMask & powerMonBatteryDepleted.bitsSet & ~powerMonBatteryDepleted.bitsClear)) ||
			((*powerFlags & powerMonBatteryNotInstalled.bitsMask) == 
				(powerMonBatteryNotInstalled.bitsMask & powerMonBatteryNotInstalled.bitsSet & ~powerMonBatteryNotInstalled.bitsClear))) {
					/*
					 * For these primary power conditions we signal the power manager to force low power state
					 * This includes both reduced processor speed and disabled L3 cache.
					 */
					*powerFlags |= (powerMonForceLowPower.bitsMask & powerMonForceLowPower.bitsSet);
					/*
					 * If we previously speed changed due to a closed clamshell and the L3 cache is still enabled
					 * we must call through to get the L3 cache disabled as well
					 */
					if (processorSpeedChangeFlags & kL3CacheEnabled) {
						if (!macRISC2CPU)
							macRISC2CPU = waitForService (serviceMatching("MacRISC2CPU"));
						if (macRISC2CPU) {
							processorSpeedChangeFlags &= ~kClamshellClosedSpeedChange;
							macRISC2CPU->setAggressiveness (kPMSetProcessorSpeed, 1); // Force slow now so cache state is right
						}
					}
		} else if ((*powerFlags & powerMonClamshellClosed.bitsMask) == 
			(powerMonClamshellClosed.bitsMask & powerMonClamshellClosed.bitsSet & ~powerMonClamshellClosed.bitsClear)) {
				/*
				 * clamShell closed with no other power conditions is a special case --
				 * leave L3 cache enabled
				 */
				*powerFlags |= (powerMonForceLowPower.bitsMask & powerMonForceLowPower.bitsSet);
				
				if (!(processorSpeedChangeFlags & kL3CacheEnabled)) {
					if (!macRISC2CPU)
						macRISC2CPU = waitForService (serviceMatching("MacRISC2CPU"));
					if (macRISC2CPU) {
						if (processorSpeedChangeFlags & kPMUBasedSpeedChange) {
							// Only want setAggressiveness to enable cache
							processorSpeedChangeFlags &= ~kPMUBasedSpeedChange;
							macRISC2CPU->setAggressiveness (kPMSetProcessorSpeed, 0); // Force fast now so cache state is right
							processorSpeedChangeFlags |= kPMUBasedSpeedChange;
						}
					}
				}
				processorSpeedChangeFlags |= kClamshellClosedSpeedChange;	// Show clamshell state
		} else {
			/*
			 * No low power conditions exist, clear all flags
			 */
			*powerFlags &= ~(powerMonForceLowPower.bitsMask & powerMonForceLowPower.bitsClear);
			processorSpeedChangeFlags &= ~kClamshellClosedSpeedChange;
		}

		result = kIOReturnSuccess;
	} else
		result = kIOReturnUnsupported;		// Not supported on this platform
		
    return result;
}

//*********************************************************************************
// PMInstantiatePowerDomains
//
// This overrides the vanilla implementation in IOPlatformExpert.  It instantiates
// a root domain with two children, one for the USB bus (to handle the USB idle
// power budget), and one for the expansions slots on the PCI bus (to handle
// the idle PCI power budget)
//*********************************************************************************

void MacRISC2PE::PMInstantiatePowerDomains ( void )
{    
    OSString * errorStr = new OSString;
    OSObject * obj;
    IOPMUSBMacRISC2 * usbMacRISC2;

    obj = OSUnserializeXML (gIOMacRISC2PMTree, &errorStr);

    if( 0 == (thePowerTree = ( OSArray * ) obj) )
    {
        kprintf ("error parsing power tree: %s", errorStr->getCStringNoCopy());
    }

    getProvider()->setProperty ("powertreedesc", thePowerTree);

#if CREATE_PLEXUS
   plexus = new IOPMPagingPlexus;
   if ( plexus ) {
        plexus->init();
        plexus->attach(this);
        plexus->start(this);
    }
#endif
         
    root = IOPMrootDomain::construct();
    root->attach(this);
    root->start(this);

    if ( plexus ) {
        root->addPowerChild(plexus);
    }

    root->setSleepSupported(kRootDomainSleepSupported);
   
    if (NULL == root)
    {
        kprintf ("PMInstantiatePowerDomains - null ROOT\n");
        return;
    }

    PMRegisterDevice (NULL, root);

    usbMacRISC2 = new IOPMUSBMacRISC2;
    if (usbMacRISC2)
    {
        usbMacRISC2->init ();
        usbMacRISC2->attach (this);
        usbMacRISC2->start (this);
        PMRegisterDevice (root, usbMacRISC2);
        if ( plexus ) {
            plexus->addPowerChild (usbMacRISC2);
        }
    }

    slotsMacRISC2 = new IOPMSlotsMacRISC2;
    if (slotsMacRISC2)
    {
        slotsMacRISC2->init ();
        slotsMacRISC2->attach (this);
        slotsMacRISC2->start (this);
        PMRegisterDevice (root, slotsMacRISC2);
        if ( plexus ) {
            plexus->addPowerChild (slotsMacRISC2);
        }
    }

    if (processorSpeedChangeFlags != kNoSpeedChange) {
        // Any system that support Speed change supports Reduce Processor Speed.
        root->publishFeature("Reduce Processor Speed");
        
        // Enable Dynamic Power Step for low latency systems.
        if (processorSpeedChangeFlags & kProcessorBasedSpeedChange) {
            root->publishFeature("Dynamic Power Step");
        }
    }
    
    return;
}


//*********************************************************************************
// PMRegisterDevice
//
// This overrides the vanilla implementation in IOPlatformExpert.  We try to 
// put a device into the right position within the power domain hierarchy.
//*********************************************************************************
extern const IORegistryPlane * gIOPowerPlane;

void MacRISC2PE::PMRegisterDevice(IOService * theNub, IOService * theDevice)
{
    bool            nodeFound  = false;
    IOReturn        err        = -1;
    OSData *	    propertyPtr = 0;
    const char *    theProperty;

    // Starts the protected area, we are trying to protect numInstancesRegistered
    if (mutex != NULL)
      IOLockLock(mutex);
     
    // reset our tracking variables before we check the XML-derived tree
    multipleParentKeyValue = NULL;
    numInstancesRegistered = 0;

    // try to find a home for this registrant in our XML-derived tree
    nodeFound = CheckSubTree (thePowerTree, theNub, theDevice, NULL);

    if (0 == numInstancesRegistered)
    {
        // make sure the provider is within the Power Plane...if not, 
        // back up the hierarchy until we find a grandfather or great
        // grandfather, etc., that is in the Power Plane.

        while( theNub && (!theNub->inPlane(gIOPowerPlane)))
            theNub = theNub->getProvider();
    }

    // Ends the protected area, we are trying to protect numInstancesRegistered
    if (mutex != NULL)
       IOLockUnlock(mutex);
     
    // try to register with the given (or reassigned in the case above) provider.
    if ( NULL != theNub )
        err = theNub->addPowerChild (theDevice);

    // failing that then register with root (but only if we didn't register in the 
    // XML-derived tree and only if the device we're registering is not the root).
    if ((err != IOPMNoErr) && (0 == numInstancesRegistered) && (theDevice != root)) {
        root->addPowerChild (theDevice);
        if ( plexus ) {
            plexus->addPowerChild (theDevice);
        }
    }

    // in addition, if it's in a PCI slot, give it to the Aux Power Supply driver
    
    propertyPtr = OSDynamicCast(OSData,theDevice->getProperty("AAPL,slot-name"));
    if ( propertyPtr ) {
	theProperty = (const char *) propertyPtr->getBytesNoCopy();
        if ( strncmp("SLOT-",theProperty,5) == 0 ) {
            slotsMacRISC2->addPowerChild (theDevice);
	}
    }
}

// -- doPlatformFunction support
void MacRISC2PE::publishStrings(OSCollection *strings)
{
	OSCollectionIterator	*strIter;
	OSSymbol				*key;

	if (!strings) return;

	strIter = OSCollectionIterator::withCollection(strings);

	if (strIter){
		while ((key = OSDynamicCast(OSSymbol, strIter->getNextObject())) != 0)
			publishResource(key, this);

		strIter->release();
	}
}

void MacRISC2PE::releaseResources(void)
{
	UInt32 i;

	if (fOnDemand) {
		fOnDemand->release();
		fOnDemand = 0;
	}

	for (i=0; i<kListNumLists; i++) {
		if (fFuncList[i]) {
			fFuncList[i]->release();
			fFuncList[i] = 0;
		}
	}

	return;
}

SInt32 MacRISC2PE::parseProvidedFunction(OSString *key, OSData *value)
{
	SInt32			numFuncs;
	UInt32			wordsLeft, phandle, flags, cmdCount, cmdLen, maskLen, valueLen;
	UInt32			*quadlet;
	OSData			*cmd;
	const OSSymbol	*aKey;
	char			funcNameWithPrefix[160], funcName[128];
    const char		*myFunc;
	bool			processingCmdList;
	
	processingCmdList = false;

	numFuncs = 0;
    cmd = 0;
	cmdCount = 0;
    	
	wordsLeft = value->getLength() / sizeof(UInt32);

	quadlet = (UInt32 *)value->getBytesNoCopy();
	if (quadlet == 0) return(-1);
	
	while (wordsLeft >= 3) { // need at least (phandle,flags,cmd) -> 3 words
		if (processingCmdList)
			// if cmdCount is zero, done processing list
			processingCmdList = (cmdCount != 0);
		else {
			numFuncs++;
			phandle = *quadlet++;
			flags = *quadlet++;
			wordsLeft -= 2;
		}

		// If no flags are set this is a useless command
		if (!flags) return(-1);
		
		// Examine the command - not all commands are supported
		switch (*quadlet) {
			case kCommandCommandList:
				DLOG ("parseProvidedFunction got command kCommandCommandList,");
				processingCmdList = true;
				cmd = OSData::withBytes ((void *)&phandle, sizeof(UInt32));
				cmdLen = 1 + kCommandCommandListLength;
				cmd->appendBytes((void *)quadlet, cmdLen * sizeof(UInt32));
				quadlet++;
				cmdCount = *quadlet++;
				wordsLeft -= cmdLen;
				DLOG (" cmdCount = %ld\n", cmdCount);
				break;
			
			case kCommandWriteReg32:
				DLOG ("parseProvidedFunction got command kCommandWriteReg32\n");
				if (wordsLeft < kCommandWriteReg32Length) return(-1);

				if (processingCmdList)
					cmdCount--;
				else
					cmd = OSData::withBytes ((void *)&phandle, sizeof(UInt32));
				cmdLen = 1 + kCommandWriteReg32Length;
				cmd->appendBytes((void *)quadlet, cmdLen * sizeof(UInt32));
				quadlet += cmdLen;
				wordsLeft -= cmdLen;
				break;

			case kCommandConfigRead:
				DLOG ("parseProvidedFunction got command kCommandConfigRead\n");
				if (wordsLeft < kCommandConfigReadLength) return(-1);

				if (processingCmdList)
					cmdCount--;
				else
					cmd = OSData::withBytes ((void *)&phandle, sizeof(UInt32));
				cmdLen = 1 + kCommandConfigReadLength;
				cmd->appendBytes((void *)quadlet, cmdLen * sizeof(UInt32));
				quadlet += cmdLen;
				wordsLeft -= cmdLen;
				break;

			case kCommandConfigRMW:
				DLOG ("parseProvidedFunction got command kCommandConfigRMW\n");
				if (wordsLeft < kCommandConfigRMWLength) return(-1);

				if (processingCmdList)
					cmdCount--;
				else
					cmd = OSData::withBytes ((void *)&phandle, sizeof(UInt32));
				
				// Calculate correct length of command, taking into account mask and data length
				maskLen = quadlet[2];	// get mask array length
				valueLen = quadlet[3];	// get data array length
				cmdLen = 1 + kCommandConfigRMWLength + (maskLen / sizeof(UInt32)) + (valueLen / sizeof(UInt32));

				cmd->appendBytes((void *)quadlet, cmdLen * sizeof (UInt32));

				quadlet += cmdLen;
				wordsLeft -= cmdLen;
				break;

			default:
				// unsupported command
				DLOG("MacRISC2PE got unsupported command\n" \
				      "\tphandle = %08lx\n" \
				      "\t  flags = %08lx\n" \
				      "\tcommand = %08lx\n", phandle, flags, *quadlet);
				return(-1);
		}

		if (!cmd) return(-1);

		/*
		 * If we're not processing a commandList or there aren't enought wordsLeft
		 * for a full command, then assume we have a complete command and process it.
		 */
		if (!processingCmdList || (wordsLeft < 3)) {
			// Examine flags, add command to appropriate execution list(s)
			if (flags & kFlagOnInit) ADD_OBJ_TO_SET(cmd, fFuncList[kListOnInit]);
			if (flags & kFlagOnTerm) ADD_OBJ_TO_SET(cmd, fFuncList[kListOnTerm]);
	
			// sleep and wake to be added in later release
			//if (flags & kFlagOnSleep) ADD_OBJ_TO_SET(cmd, fFuncList[kListOnSleep], -1);
			//if (flags & kFlagOnWake) ADD_OBJ_TO_SET(cmd, fFuncList[kListOnWake], -1);
			
			// On-demand functions need special attention
			if (flags & kFlagOnDemand) {
				// Build an OSString to represent this function
				myFunc = key->getCStringNoCopy() + strlen(kFunctionProvidedPrefix);
				sprintf(funcName, "%s-%08lx", myFunc, phandle);
				sprintf(funcNameWithPrefix, "%s%s", kFunctionRequiredPrefix,
						funcName);
	
				if ((aKey = OSSymbol::withCString(funcNameWithPrefix)) == 0)
					return(-1);
				
				DLOG ("parseProvidedFunction registering demand function '%s\n", funcNameWithPrefix);
				// register for on-demand execution
				if (fOnDemand) {
					if (!fOnDemand->setObject(aKey,cmd))
						return(-1);
				} else 
					if ((fOnDemand = OSDictionary::withObjects(
							&(const OSObject *)cmd,	&aKey, 1, 1)) == 0)
						return(-1);
			}
			cmd->release();
		}
	}

	return(numFuncs);
}

bool MacRISC2PE::performFunctionList(const OSSet *funcList)
{
	bool ret;
	OSCollectionIterator	*iter;
	const OSData			*aFunc;
	
	// Sanity check
	if (funcList == 0) return(false);

	iter = OSCollectionIterator::withCollection(funcList);
	if (iter == 0) return(false);
	
	ret = true;
	
	// Attempt every function in the list.  If any of them fail, return false.
	while((aFunc = OSDynamicCast(OSData, iter->getNextObject())) != 0)
		if (!performFunction(aFunc, (void *)1, (void *)0, (void *)0, (void *)0)) ret = false;

	iter->release();
	return(ret);
}

bool MacRISC2PE::performFunction(const OSData *func, void *param1 = 0,
			void *param2 = 0, void *param3 = 0, void *param4 = 0)
{
	UInt32		*quadlet, pHandle, offset, data, value, valueLen, mask, maskLen, writeLen, wordsLeft, cmdCount;
	SInt32		lastCmd;
	bool		processingCmdList;
	IOPCIDevice	*nub;
	
	if (func == 0) return(false);

	processingCmdList = false;

	cmdCount = 0;
    	
	wordsLeft = func->getLength() / sizeof(UInt32);
	
	data = 0;
	nub = NULL;
	lastCmd = -1;
	quadlet = (UInt32 *)func->getBytesNoCopy();
	if (quadlet == 0) return(-1);
	
	while (wordsLeft >= 2) { // need at least (phandle,cmd) -> 2 words
		if (processingCmdList)
			// if cmdCount is zero, done processing list
			processingCmdList = (cmdCount != 0);
		else {
			pHandle = *quadlet++;
			wordsLeft--;
		}
		
		// Examine the command - not all commands are supported
		switch (*quadlet) {
			case kCommandCommandList:
				DLOG ("MacRISC2PE::performFunction got command kCommandCommandList,");
				processingCmdList = true;
				quadlet++;
				cmdCount = *quadlet++;
				wordsLeft -= (1 + kCommandCommandListLength);
				DLOG (" cmdCount = %ld\n", cmdCount);
				break;
			
			case kCommandWriteReg32:
				DLOG ("MacRISC2PE::performFunction command kCommandWriteReg32\n");
				if (processingCmdList)
					cmdCount--;

				quadlet++;
				offset = *quadlet++;
				value = *quadlet++;
				mask  = *quadlet++;
				wordsLeft -= (1 + kCommandWriteReg32Length);
	
				// If mask isn't all ones, read data and mask it
				if (mask != 0xFFFFFFFF) {
					data = readUniNReg (offset);
					data &= mask;
					data |= value;
				} else	// just write the data
					data = value;
				
				DLOG ("MacRISC2PE::performFunction - writing data 0x%lx to Uni-N register offset 0x%lx\n", data, offset);
				// write the result to the Uni-N register
				writeUniNReg(offset, data);
				
				break;
	
			// Currently only handle config reads of 4 bytes or less
			case kCommandConfigRead:
				DLOG ("MacRISC2PE::performFunction command kCommandConfigRead\n");
				if (processingCmdList)
					cmdCount--;

				quadlet++;
				offset = *quadlet++;
				valueLen = *quadlet++;
				wordsLeft -= (1 + kCommandConfigReadLength);
				
				if (valueLen != 4) {
					IOLog ("MacRISC2PE::performFunction config reads cannot handle anything other than 4 bytes, found length %ld\n", valueLen);
					return false;
				}
	
				if (!nub) {
					if (!pHandle) {
						IOLog ("MacRISC2PE::performFunction config read requires pHandle to locate nub\n");
						return false;
					}
					nub = findNubForPHandle (pHandle);
					if (!nub) {
						IOLog ("MacRISC2PE::performFunction config read cannot find nub for pHandle 0x%08lx\n", pHandle);
						return false;
					}
				}
				
				// NOTE - code below assumes read of 4 bytes, i.e., valueLen == 4!!
				data = nub->configRead32 (offset);
				DLOG ("MacRISC2PE::performFunction config read 0x%lx at offset 0x%lx\n", data, offset);
				if (param1)
					*(UInt32 *)param1 = data;
				
				lastCmd = kCommandConfigRead;
				break;
				
			// Currently only handle config reads/writes of 4 bytes
			case kCommandConfigRMW:
				DLOG ("MacRISC2PE::performFunction command kCommandConfigRMW\n");
				if (processingCmdList)
					cmdCount--;

				// data must have been read above
				if (lastCmd != kCommandConfigRead) {
					IOLog ("MacRISC2PE::performFunction - config modify/write requires prior read\n");
					return false;
				}
				
				quadlet++;
				offset = *quadlet++;
				maskLen = *quadlet++;
				valueLen = *quadlet++;
				writeLen = *quadlet++;
				wordsLeft -= (1 + kCommandConfigRMWLength + (maskLen / sizeof(UInt32)) + (valueLen / sizeof(UInt32)));

				if (writeLen != 4) {
					IOLog ("MacRISC2PE::performFunction config read/modify/write cannot handle anything other than 4 bytes, found length %ld\n", writeLen);
					return false;
				}
				
				// NOTE - code below makes implicit assumption that mask and value are each 4 bytes!!
				mask = *quadlet++;
				value = *quadlet++;
	
				DLOG("\nMacRISC2PE::performFunction config read/modify/write offset = %08lx maskLen = %08lx dataLen = %08lx writeLen = %08lx, mask = %08lx, data = %08lx\n",
						offset, maskLen, valueLen, writeLen, mask, value);
	
				if (!nub) {
					if (!pHandle) {
						IOLog ("MacRISC2PE::performFunction config read/modify/write requires pHandle to locate nub\n");
						return false;
					}
					nub = findNubForPHandle (pHandle);
					if (!nub) {
						IOLog ("MacRISC2PE::performFunction config read/modify/write cannot find nub for pHandle 0x%08lx\n", pHandle);
						return false;
					}
				}
				
				// data must have been previously read
				data &= mask;
				data |= value;
				
				DLOG ("MacRISC2PE::performFunction config write data 0x%lx to offset 0x%lx\n", data, offset);
				nub->configWrite32 (offset, data);
	
				break;
	
			default:
				IOLog("MacRISC2PE::performFunction got unsupported command %08lx\n", *quadlet);
				return(false);
		}
	}
	
	return(true);
}

IOPCIDevice* MacRISC2PE::findNubForPHandle( UInt32 pHandleValue )
{
	IORegistryIterator*								iterator;
	IORegistryEntry*								matchingEntry = NULL;

	iterator = IORegistryIterator::iterateOver( gIODTPlane, kIORegistryIterateRecursively );

	if ( iterator == NULL )
		return( NULL );

	while ( ( matchingEntry = iterator->getNextObject() ) != NULL ) {
		OSData*						property;

		if ( ( property = OSDynamicCast( OSData, matchingEntry->getProperty( "AAPL,phandle" ) ) ) != NULL )
			if ( pHandleValue == *( ( UInt32 * ) property->getBytesNoCopy() ) )
				break;
	}
	
	iterator->release();
	
	DLOG ("MacRISC2PE::findNubForPHandle - found matchingEntry '%s'\n", matchingEntry->getName());

	return( OSDynamicCast (IOPCIDevice, matchingEntry));
}

