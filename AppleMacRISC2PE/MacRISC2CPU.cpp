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
#include <sys/cdefs.h>

__BEGIN_DECLS
#include <ppc/proc_reg.h>
#include <ppc/machine_routines.h>
__END_DECLS

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOCPU.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#include "MacRISC2CPU.h"

#define kMacRISC_GPIO_DIRECTION_BIT	2

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOCPU

OSDefineMetaClassAndStructors(MacRISC2CPU, IOCPU);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOCPUInterruptController *gCPUIC;

bool MacRISC2CPU::start(IOService *provider)
{
    kern_return_t        result;
    IORegistryEntry      *cpusRegEntry, *uniNRegEntry, *mpicRegEntry, *devicetreeRegEntry;
    OSIterator           *cpusIterator;
    OSData               *tmpData;
    IOService            *service;
    const OSSymbol       *interruptControllerName;
    OSData               *interruptData;
    OSArray              *tmpArray;
    UInt32               maxCPUs, uniNVersion, physCPU;
    ml_processor_info_t  processor_info;
    
#if enableUserClientInterface    
	DFScontMode = 0;
    fWorkLoop = 0;
    DFS_Status = false;
    GPU_Status = kGPUHigh;
	vStepped = false;
#endif

    // callPlatformFunction symbols
    mpic_getProvider = OSSymbol::withCString("mpic_getProvider");
    mpic_getIPIVector= OSSymbol::withCString("mpic_getIPIVector");
    mpic_setCurrentTaskPriority = OSSymbol::withCString("mpic_setCurrentTaskPriority");
    mpic_setUpForSleep = OSSymbol::withCString("mpic_setUpForSleep");
    mpic_dispatchIPI = OSSymbol::withCString("mpic_dispatchIPI");
    keyLargo_restoreRegisterState = OSSymbol::withCString("keyLargo_restoreRegisterState");
    keyLargo_syncTimeBase = OSSymbol::withCString("keyLargo_syncTimeBase");
    keyLargo_saveRegisterState = OSSymbol::withCString("keyLargo_saveRegisterState");
    keyLargo_turnOffIO = OSSymbol::withCString("keyLargo_turnOffIO");
    keyLargo_writeRegUInt8 = OSSymbol::withCString("keyLargo_writeRegUInt8");
    keyLargo_getHostKeyLargo = OSSymbol::withCString("keyLargo_getHostKeyLargo");
    keyLargo_setPowerSupply = OSSymbol::withCString("setPowerSupply");
    uniN_setPowerState = OSSymbol::withCString(kUniNSetPowerState);
    uniN_setAACKDelay = OSSymbol::withCString(kUniNSetAACKDelay);
    
    macRISC2PE = OSDynamicCast(MacRISC2PE, getPlatform());
    if (macRISC2PE == 0) return false;
  
    if (!super::start(provider)) return false;

    // Get the Uni-N Version.
    uniNRegEntry = fromPath("/uni-n", gIODTPlane);
    if (uniNRegEntry == 0) return false;
    tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("device-rev"));
    if (tmpData == 0) return false;
    uniNVersion = *(long *)tmpData->getBytesNoCopy();
  
    // Count the CPUs.
    numCPUs = 0;
    cpusRegEntry = fromPath("/cpus", gIODTPlane);
    if (cpusRegEntry == 0) return false;
    cpusIterator = cpusRegEntry->getChildIterator(gIODTPlane);
    while (cpusIterator->getNextObject()) numCPUs++;
    cpusIterator->release();
  
    // Limit the number of CPUs to one if uniNVersion is 1.0.7 or less.
    if (uniNVersion < kUniNVersion107) numCPUs = 1;
  
    // Limit the number of CPUs by the cpu=# boot arg.
    if (PE_parse_boot_arg("cpus", &maxCPUs))
    {
        if (numCPUs > maxCPUs) numCPUs = maxCPUs;
    }
	
	ignoreSpeedChange = false;
	doSleep = false;
	topLevelPCIBridgeCount = 0;
  
    // Get the "flush-on-lock" property from the first cpu node.
    flushOnLock = false;
    cpusRegEntry = fromPath("/cpus/@0", gIODTPlane);
    if (cpusRegEntry == 0) return false;
    if (cpusRegEntry->getProperty("flush-on-lock") != 0) flushOnLock = true;
  
    // Set flushOnLock when numCPUs is not one.
    if (numCPUs != 1) flushOnLock = true;
  
    // If system is PowerMac3,5 (TowerG4), then set flushOnLock to disable nap
    devicetreeRegEntry = fromPath("/", gIODTPlane);
    tmpData = OSDynamicCast(OSData, devicetreeRegEntry->getProperty("model"));
    if (tmpData == 0) return false;
#if 0
    if(!strcmp((char *)tmpData->getBytesNoCopy(), "PowerMac3,5"))
        flushOnLock = true;
#endif

    // Get the physical CPU number from the "reg" property.
    tmpData = OSDynamicCast(OSData, provider->getProperty("reg"));
    if (tmpData == 0) return false;
    physCPU = *(long *)tmpData->getBytesNoCopy();
    setCPUNumber(physCPU);

    // Get the gpio offset for soft reset from the "soft-reset" property.
    tmpData = OSDynamicCast(OSData, provider->getProperty("soft-reset"));
    if (tmpData == 0) 
    {
        if (physCPU == 0)
            soft_reset_offset = 0x5B;
        else
            soft_reset_offset = 0x5C;
    }
    else
        soft_reset_offset = *(long *)tmpData->getBytesNoCopy();
   
    // Get the gpio offset for timebase enable from the "timebase-enable" property.
    tmpData = OSDynamicCast(OSData, provider->getProperty("timebase-enable"));
    if (tmpData == 0) 
        timebase_enable_offset = 0x73;
    else
        timebase_enable_offset = *(long *)tmpData->getBytesNoCopy();
  
    // On macines with a 'vmin' property in the CPU Node we need to make sure to tell the kernel to 
    // ml_set_processor_voltage on needed processors.
    needVSetting = (provider->getProperty( "vmin" ) != 0);

    // While techincally the Apollo7PM machines do need AACK delay, it is already set in the bootROM
    // since we boot slow.  We don't want the machine to switch AACKdelay off when we run DFS high so
    // setting this to false will take care of the issue.
        
    needAACKDelay = false;
    
    // Find out if this is the boot CPU.
    bootCPU = false;
    tmpData = OSDynamicCast(OSData, provider->getProperty("state"));
    if (tmpData == 0) return false;
    if (!strcmp((char *)tmpData->getBytesNoCopy(), "running")) bootCPU = true;
  
    if (bootCPU)
    {
        gCPUIC = new IOCPUInterruptController;
        if (gCPUIC == 0) return false;
        if (gCPUIC->initCPUInterruptController(numCPUs) != kIOReturnSuccess)
            return false;
        gCPUIC->attach(this);
        gCPUIC->registerCPUInterruptController();
    }
  
    // Get the l2cr value from the property list.
    tmpData = OSDynamicCast(OSData, provider->getProperty("l2cr"));
    if (tmpData != 0)
    {
        l2crValue = *(long *)tmpData->getBytesNoCopy() & 0x7FFFFFFF;
    }
    else
    {
        l2crValue = mfl2cr() & 0x7FFFFFFF;
    }
  
    // Wait for KeyLargo to show up.
    keyLargo = waitForService(serviceMatching("KeyLargo"));
    if (keyLargo == 0) return false;
  
    keyLargo->callPlatformFunction (keyLargo_getHostKeyLargo, false, &keyLargo, 0, 0, 0);
    if (keyLargo == 0)
    {
        kprintf ("MacRISC2CPU::start - getHostKeyLargo returned nil\n");
        return false;
    }

    // Wait for MPIC to show up.
    mpic = waitForService(serviceMatching("AppleMPICInterruptController"));
    if (mpic == 0) return false;
  
    // Set the Interrupt Properties for this cpu.
    mpic->callPlatformFunction(mpic_getProvider, false, (void *)&mpicRegEntry, 0, 0, 0);
    interruptControllerName = IODTInterruptControllerName(mpicRegEntry);
    mpic->callPlatformFunction(mpic_getIPIVector, false, (void *)&physCPU, (void *)&interruptData, 0, 0);
    if ((interruptControllerName == 0) || (interruptData == 0)) return false;
  
    tmpArray = OSArray::withCapacity(1);
    tmpArray->setObject(interruptControllerName);
    cpuNub->setProperty(gIOInterruptControllersKey, tmpArray);
    tmpArray->release();
  
    tmpArray = OSArray::withCapacity(1);
    tmpArray->setObject(interruptData);
    cpuNub->setProperty(gIOInterruptSpecifiersKey, tmpArray);
    tmpArray->release();
  
    setCPUState(kIOCPUStateUninitalized);
  
    if (physCPU < numCPUs)
    {
        processor_info.cpu_id           = (cpu_id_t)this;
        processor_info.boot_cpu         = bootCPU;
        processor_info.start_paddr      = 0x0100;
        processor_info.l2cr_value       = l2crValue;
        processor_info.supports_nap     = !flushOnLock;
        processor_info.time_base_enable =
        (time_base_enable_t)&MacRISC2CPU::enableCPUTimeBase;
    
        // Register this CPU with mach.
        result = ml_processor_register(&processor_info, &machProcessor,	&ipi_handler);
        if (result == KERN_FAILURE) return false;
    
        processor_start(machProcessor);
    }

    // Before to go to sleep we wish to disable the napping mode so that the PMU
    // will not shutdown the system while going to sleep:
    service = waitForService(serviceMatching("IOPMrootDomain"));
    pmRootDomain = OSDynamicCast(IOPMrootDomain, service);
    if (pmRootDomain != 0)
    {
        kprintf("Register MacRISC2CPU %d to acknowledge power changes\n", getCPUNumber());
        pmRootDomain->registerInterestedDriver(this);
        
        // Join the Power Management Tree to receive setAggressiveness calls.
        PMinit();
        provider->joinPMtree(this);
    }

    // Finds PMU and UniN so in quiesce we can put the machine to sleep.
    // I can not put these calls there because quiesce runs in interrupt
    // context and waitForService may block.
    pmu = waitForService(serviceMatching("ApplePMU"));
	uniN = waitForService(serviceMatching("AppleUniN"));
    if ((pmu == 0) || (uniN == 0)) return false;
	

    if (macRISC2PE->hasPMon) {
            // Find the platform monitor, if present
            service = waitForService(resourceMatching("IOPlatformMonitor"));
            ioPMon = OSDynamicCast (IOPlatformMonitor, service->getProperty("IOPlatformMonitor"));
            if (!ioPMon) 
                    return false;
            
            ioPMonDict = OSDictionary::withCapacity(2);
            if (!ioPMonDict) {
                    ioPMon = NULL;
            } else {
                    ioPMonDict->setObject (kIOPMonTypeKey, OSSymbol::withCString (kIOPMonTypeCPUCon));
                    ioPMonDict->setObject (kIOPMonCPUIDKey, OSNumber::withNumber ((long long)getCPUNumber(), 32));

                    if (messageClient (kIOPMonMessageRegister, ioPMon, (void *)ioPMonDict) != kIOReturnSuccess) {
                            // IOPMon doesn't need to know about us, so don't bother with it
                            IOLog ("MacRISC2CPU::start - failed to register cpu with IOPlatformMonitor\n");
                            ioPMonDict->release();
                            ioPMon = NULL;
                    }
            }
    }

#if enableUserClientInterface    
//   
// UserClient stuff...
//  
    fWorkLoop = getWorkLoop();
    if(!fWorkLoop)
    {
            IOLog("MacRISC2CPU::start ERROR: failed to find a fWorkLoop\n");
    }
    if(!initTimers()) 
    {
            IOLog("MacRISC2CPU::start ERROR: failed to init the timers\n");
    }
    
#endif

    registerService();
  
    return true;
}

// This is called before to start the sleep process and after waking up before to
// start the wake process. We wish to disable the CPU nap mode going down and
// re-enable it before to go up.  For machines that support speed changing, we do
// some bookkeeping to make sure we end up in the right state coming out of sleep
IOReturn MacRISC2CPU::powerStateWillChangeTo ( IOPMPowerFlags theFlags, unsigned long, IOService*)
{
    if ( ! (theFlags & IOPMPowerOn) ) {
        // Sleep sequence:
        kprintf("MacRISC2CPU %d powerStateWillChangeTo to acknowledge power changes (DOWN) we set napping %d\n", getCPUNumber(), false);
        rememberNap = ml_enable_nap(getCPUNumber(), false);        // Disable napping (the function returns the previous state)

		// If processor based and currently slow, kick it back up so we safely come out of sleep
		if (macRISC2PE->processorSpeedChangeFlags & kProcessorBasedSpeedChange &&
			!(macRISC2PE->processorSpeedChangeFlags & kProcessorFast)) {
 				setAggressiveness (kPMSetProcessorSpeed, 0);				// Make it so
				macRISC2PE->processorSpeedChangeFlags &= ~kProcessorFast;	// Remember slow so we can set it after sleep
			}

		// on machines that use the processor based speed change, once we start the sleep process, we don't want to change the 
		// speed again until we wake. setting this true causes us to ignore all speed change requests.
		if(macRISC2PE->processorSpeedChangeFlags & kProcessorBasedSpeedChange)
		{	
			//IOLog("*** setting ignoreSpeedChange = true\n");
			ignoreSpeedChange = true;
		}
			
   } else {
        // Wake sequence:

		// on machines that use the processor based speed change, now that we're awake, we can again start accepting speed change requests.
		if(macRISC2PE->processorSpeedChangeFlags & kProcessorBasedSpeedChange)
		{	
			//IOLog("*** setting ignoreSpeedChange = false\n");
			ignoreSpeedChange = false;
		}

        kprintf("MacRISC2CPU %d powerStateWillChangeTo to acknowledge power changes (UP) we set napping %d\n", getCPUNumber(), rememberNap);
        ml_enable_nap(getCPUNumber(), rememberNap); 		   // Re-set the nap as it was before.
		
		// If we have an ioPMon, it will handle this
		if (!ioPMon && (macRISC2PE->processorSpeedChangeFlags & kEnvironmentalSpeedChange)) {
			// Coming out of sleep we will be slow, so go to fast if necessary
			if (macRISC2PE->processorSpeedChangeFlags & kProcessorFast) {
				macRISC2PE->processorSpeedChangeFlags &= ~kProcessorFast;	// Clear fast flag so we know to speed up
				setAggressiveness (kPMSetProcessorSpeed, 0);				// Make it so
			} else
				doSleep = true;		// force delay on next call
		}
		
		// If processor based and flag indicates slow, we boosted it before going to sleep,
		// 		so set it back to slow
		if (macRISC2PE->processorSpeedChangeFlags & kProcessorBasedSpeedChange &&
			!(macRISC2PE->processorSpeedChangeFlags & kProcessorFast)) {
				macRISC2PE->processorSpeedChangeFlags |= kProcessorFast;	// Set fast so we know to go slow
 				setAggressiveness (kPMSetProcessorSpeed, 1);				// Make it so
			}

	}
	
    return IOPMAckImplied;
}

/*
 * MacRISC2CPU::setAggressiveness - respond to messages regarding power conservation aggressiveness
 *
 * For the case we care about (kPMSetProcessorSpeed), newLevel means:
 * 		newLevel == 0 => run fast => cache on => true
 *		newLevel == 1 => run slow => cache off => false
 */
IOReturn MacRISC2CPU::setAggressiveness(UInt32 selector, UInt32 newLevel)
{
	bool		doChange = false;
	IOReturn	result;
        	    
	// on machines that use the processor based speed change, we check ignoreSpeedChange to see
	// if we should ignore speed change requests. this is to avoid changing speed in the middle of
	// a sleep/wake cycle.
	if((selector == kPMSetProcessorSpeed) && (macRISC2PE->processorSpeedChangeFlags & kProcessorBasedSpeedChange) && (ignoreSpeedChange == true))
	{	
		//IOLog("*** ignoreSpeedChange == true, ignoring\n");
		return IOPMNoErr;
	}
		
    result = super::setAggressiveness(selector, newLevel);
	
    if ((selector == kPMSetProcessorSpeed) && (macRISC2PE->processorSpeedChangeFlags != kNoSpeedChange))
    {
		/*
		 * If we're using the platform monitor, then we let the platform monitor handle the standard
		 * call (i.e., newLevel = 0 or 1).  If it wants a speed change, the platform monitor will
		 * call us directly with newLevel = 2 or newLevel = 3 (corresponding to 0 and 1) and then
		 * we will do the actual speed change
		 */
		if (ioPMon) {
			if (newLevel < 2)
				return result;		// Nothing to do just yet
			
			// OK, this is a call from the platform monitor, so adjust it to standard levels and carry on
			newLevel -= 2;
			doChange = true;		// Change should always be true under control of ioPMon
		}

		/*
		 * We get here if 1) there is no platform monitor, OR 2) this is a call from the platform
		 * monitor.
		 */
		if (doSleep) {
			IOSleep (1000);
			doSleep = false;
		}
	
		// Enable/Disable L2 if needed.
		if (macRISC2PE->processorSpeedChangeFlags & kDisableL2SpeedChange) {
			if (!(macRISC2PE->processorSpeedChangeFlags & kClamshellClosedSpeedChange)) {
				// newLevel == 0 => run fast => cache on => true
				// newLevel == 1 => run slow => cache off => false
				if (!newLevel) {
					// See if cache is disabled
					if (!(macRISC2PE->processorSpeedChangeFlags & kL2CacheEnabled)) {
						// Enable it
						ml_enable_cache_level(2, !newLevel);
						macRISC2PE->processorSpeedChangeFlags |= kL2CacheEnabled;
					}
				} else if (macRISC2PE->processorSpeedChangeFlags & kL2CacheEnabled) {
					// Disable it
					ml_enable_cache_level(2, !newLevel);
					macRISC2PE->processorSpeedChangeFlags &= ~kL2CacheEnabled;
				}
			}
		}

		// Enable/Disable L3 if needed.
		if (macRISC2PE->processorSpeedChangeFlags & kDisableL3SpeedChange) {
			if (!(macRISC2PE->processorSpeedChangeFlags & kClamshellClosedSpeedChange)) {
				// newLevel == 0 => run fast => cache on => true
				// newLevel == 1 => run slow => cache off => false
				if (!newLevel) {
					// See if cache is disabled
					if (!(macRISC2PE->processorSpeedChangeFlags & kL3CacheEnabled)) {
						// Enable it
						ml_enable_cache_level(3, !newLevel);
						macRISC2PE->processorSpeedChangeFlags |= kL3CacheEnabled;
					}
				}
                else if (macRISC2PE->processorSpeedChangeFlags & kL3CacheEnabled)
                {
					// Disable it
					ml_enable_cache_level(3, !newLevel);
					macRISC2PE->processorSpeedChangeFlags &= ~kL3CacheEnabled;
				}
			}
		}
        
		if (!newLevel)
        {
			// See if already running slow
			if (!(macRISC2PE->processorSpeedChangeFlags & kProcessorFast))
            {
				// Signal to switch
				doChange = true;
				macRISC2PE->processorSpeedChangeFlags |= kProcessorFast;
			}
		}
        else if (macRISC2PE->processorSpeedChangeFlags & kProcessorFast)
        {
			// Signal to switch
			doChange = true;
			macRISC2PE->processorSpeedChangeFlags &= ~kProcessorFast;
		}

		if (macRISC2PE->processorSpeedChangeFlags & kPMUBasedSpeedChange)
        {
			if (doChange) 
				performPMUSpeedChange (newLevel);
		} 
        
		if ((macRISC2PE->processorSpeedChangeFlags & kProcessorBasedSpeedChange) && doChange && ! (macRISC2PE->processorSpeedChangeFlags & kBusSlewBasedSpeedChange))
        {
			IOReturn cpfResult = kIOReturnSuccess;
			
			if (newLevel == 0)
				cpfResult = keyLargo->callPlatformFunction (keyLargo_setPowerSupply, false,
					(void *)1, (void *)0, (void *)0, (void *)0);
			
			if (cpfResult == kIOReturnSuccess)
            {  
				// Set processor to new speed setting.
                
                if (needVSetting && (newLevel == 0))
                            ml_set_processor_voltage(0);	// High
                
                if (needAACKDelay && (newLevel == 1))
                    uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)1, (void *)0, (void *)0, (void *)0);

				ml_set_processor_speed(newLevel ? 1 : 0);
                                
                if (needAACKDelay & (newLevel == 0))
                    uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)0, (void *)0, (void *)0, (void *)0);
                
                if (needVSetting && (newLevel != 0))
                        ml_set_processor_voltage(1);	// Low
                                
				if (newLevel != 0)
					cpfResult = keyLargo->callPlatformFunction (keyLargo_setPowerSupply, false,
						(void *)0, (void *)0, (void *)0, (void *)0);
			}
		}
	}
	
    return result;
}

void MacRISC2CPU::performPMUSpeedChange (UInt32 newLevel)
{
	bool tempRememberNap;

	// Note the current processor speed so quiesceCPU knows what to do
	currentProcessorSpeed = newLevel;
	
	// Disable nap to prevent PMU doing reset too soon.
	tempRememberNap = ml_enable_nap(getCPUNumber(), false);
	
	// Set flags for processor speed change.
	processorSpeedChange = true;
	
	// Ask PM to do the processor speed change.
	pmRootDomain->receivePowerNotification(kIOPMProcessorSpeedChange);
	
	// Set flags for system sleep.
	processorSpeedChange = false;
	
	// Enable nap as needed.
	ml_enable_nap(getCPUNumber(), tempRememberNap);
	
	return;
}

void MacRISC2CPU::initCPU(bool boot)
{
	IOPCIBridge		*pciDriver;
	UInt32			i;

    if (!boot && bootCPU) {
		// Tell Uni-N to enter normal mode.
		uniN->callPlatformFunction (uniN_setPowerState, false, (void *)kUniNNormal,
			(void *)0, (void *)0, (void *)0);
    
        if (!processorSpeedChange) {

			// Notify our pci children to restore their state
			for (i = 0; i < topLevelPCIBridgeCount; i++)
				if (pciDriver = topLevelPCIBridges[i])
					// Got the driver - send the message
					pciDriver->setDevicePowerState (NULL, 3);

			keyLargo->callPlatformFunction(keyLargo_restoreRegisterState, false, 0, 0, 0, 0);
	
			// Disables the interrupts for this CPU.
			if (macRISC2PE->getMachineType() == kMacRISC2TypePowerMac)
			{
				kprintf("MacRISC2CPU::initCPU %d -> mpic->setUpForSleep on", getCPUNumber());
				mpic->callPlatformFunction(mpic_setUpForSleep, false, (void *)false, (void *)getCPUNumber(), 0, 0);
			}
		}
    }

    kprintf("MacRISC2CPU::initCPU %d Here!\n", getCPUNumber());
 
    // Set time base.
    if (bootCPU)
        keyLargo->callPlatformFunction(keyLargo_syncTimeBase, false, 0, 0, 0, 0);
  
    if (boot)
    {
        gCPUIC->enableCPUInterrupt(this);
    
        // Register and enable IPIs.
        cpuNub->registerInterrupt(0, this, (IOInterruptAction)&MacRISC2CPU::ipiHandler, 0);
        cpuNub->enableInterrupt(0);
    }
    else
    {
        long priority = 0;
        mpic->callPlatformFunction(mpic_setCurrentTaskPriority, false, (void *)&priority, 0, 0, 0);
    }
  
    setCPUState(kIOCPUStateRunning);
}

void MacRISC2CPU::quiesceCPU(void)
{
    if (bootCPU)
    {
        if (processorSpeedChange) {
            // Send PMU command to speed the system
            pmu->callPlatformFunction("setSpeedNow", false, (void *)currentProcessorSpeed, 0, 0, 0);
        }
        else
        {
			// Send PMU command to shutdown system before io is turned off
				pmu->callPlatformFunction("sleepNow", false, 0, 0, 0, 0);
	
			// Enables the interrupts for this CPU.
			if (macRISC2PE->getMachineType() == kMacRISC2TypePowerMac) 
			{
				kprintf("MacRISC2CPU::quiesceCPU %d -> mpic->setUpForSleep off", getCPUNumber());
				mpic->callPlatformFunction(mpic_setUpForSleep, false, (void *)true, (void *)getCPUNumber(), 0, 0);
			}
	
			kprintf("MacRISC2CPU::quiesceCPU %d -> keyLargo->saveRegisterState()\n", getCPUNumber());
			// Save KeyLargo's register state.
			keyLargo->callPlatformFunction(keyLargo_saveRegisterState, false, 0, 0, 0, 0);
	
			kprintf("MacRISC2CPU::quiesceCPU %d -> keyLargo->turnOffIO", getCPUNumber());
			// Turn Off all KeyLargo I/O.
			keyLargo->callPlatformFunction(keyLargo_turnOffIO, false, (void *)false, 0, 0, 0);
        }
        
        kprintf("MacRISC2CPU::quiesceCPU %d -> here\n", getCPUNumber());

        // Set the wake vector to point to the reset vector
        ml_phys_write(0x0080, 0x100);

        // Set the sleeping state for HWInit.
		// Tell Uni-N to enter normal mode.
		uniN->callPlatformFunction (uniN_setPowerState, false, 
			(void *)(processorSpeedChange ? kUniNIdle2 : kUniNSleep),
			(void *)0, (void *)0, (void *)0);
    }

    ml_ppc_sleep();
}

kern_return_t MacRISC2CPU::startCPU(vm_offset_t /*start_paddr*/, vm_offset_t /*arg_paddr*/)
{
    long            gpioOffset = soft_reset_offset;
    unsigned long   strobe_value;

    // Strobe the reset line for this CPU.
    //
    // This process is not nearly as obvious as it seems.
    // This GPIO is supposed to be managed as an open collector.
    // Furthermore, the SRESET(0,1) is an active LOW signal.
    // And just complicate things more because they're not already,
    // the GPIO wants to be written with the INVERTED value of what you want to set things to.
    // 
    // According to our GPIO expert, when using this GPIO to invoke a soft reset,
    // you write a 1 to the DATA DIRECTION bit (bit 2) to make the GPIO be an output,
    // and leave the DATA VALUE bit 0.  This causes SRESET(0,1)_L to be strobed,
    // as SRESET(0,1) is an active low signal.
    // To complete the strobe, you make the DATA DIRECTION bit 0 to make it an open collector,
    // leaving the DATA VALUE bit 0.  The data bit value will then float to a 1 value
    // and the reset signal removed.
    strobe_value = ( 1 << kMacRISC_GPIO_DIRECTION_BIT );
    keyLargo->callPlatformFunction(keyLargo_writeRegUInt8, false,
                                    (void *)&gpioOffset, (void *)strobe_value, 0, 0);
    // Mac OS 9.x actually reads this value back to make sure it "takes".
    // Should we do that here?  It (appears to) work without doing that.
    strobe_value = ( 0 << kMacRISC_GPIO_DIRECTION_BIT );
    keyLargo->callPlatformFunction(keyLargo_writeRegUInt8, false,
                                    (void *)&gpioOffset, (void *)strobe_value, 0, 0);

    return KERN_SUCCESS;
}

void MacRISC2CPU::haltCPU(void)
{
	OSIterator 		*childIterator;
	IORegistryEntry *childEntry, *childDriver;
	IOPCIBridge		*pciDriver;
	OSData			*deviceTypeString;
	UInt32			i;

  
    setCPUState(kIOCPUStateStopped);
  
    if (bootCPU)
    {
		// Some systems require special handling of Ultra-ATA at sleep.
		// Call UniN to prepare for that, if necessary
		uniN->callPlatformFunction ("setupUATAforSleep", false, (void *)0, (void *)0, (void *)0, (void *)0);

		// Notify our pci children to save their state
		if (!topLevelPCIBridgeCount) {
			// First build list of top level bridges - only need to do once as these don't change
			if ((childIterator = macRISC2PE->getChildIterator (gIOServicePlane)) != NULL) {
				while ((childEntry = (IORegistryEntry *)(childIterator->getNextObject ())) != NULL) {
					deviceTypeString = OSDynamicCast( OSData, childEntry->getProperty( "device_type" ));
					if (deviceTypeString) {
						if (!strcmp((const char *)deviceTypeString->getBytesNoCopy(), "pci")) {
							childDriver = childEntry->copyChildEntry(gIOServicePlane);
							if (childDriver) {
								pciDriver = OSDynamicCast( IOPCIBridge, childDriver );
								if (pciDriver)
									if (topLevelPCIBridgeCount < kMaxPCIBridges)
										// Remember this driver
										topLevelPCIBridges[topLevelPCIBridgeCount++] = pciDriver;
									else
										kprintf ("MacRISC2CPU::haltCPU - warning, more than %ld PCI bridges - cannot save/restore them all\n");
								childDriver->release();
							}
						}
					}
				}
				childIterator->release();
			}
		}
		for (i = 0; i < topLevelPCIBridgeCount; i++)
			if (pciDriver = topLevelPCIBridges[i]) {
				// Got the driver - send the message
				pciDriver->setDevicePowerState (NULL, 2);
			}
    }

   kprintf("MacRISC2CPU::haltCPU %d Here!\n", getCPUNumber());

   processor_exit(machProcessor);
}

void MacRISC2CPU::signalCPU(IOCPU *target)
{
    UInt32 physCPU = getCPUNumber();
    MacRISC2CPU *targetCPU = OSDynamicCast(MacRISC2CPU, target);
  
    if (targetCPU == 0) return;
  
    mpic->callPlatformFunction(mpic_dispatchIPI, false, (void *)&physCPU, (void *)(1 << targetCPU->getCPUNumber()), 0, 0);
}

void MacRISC2CPU::enableCPUTimeBase(bool enable)
{
    long			gpioOffset = timebase_enable_offset;

    // the time-base enable GPIO is 8 bits, but will be passed to ->callPlatformFunction()
    // as a void *-sized value so we make it an unsigned long just to not get an annoying
    // compiler warning from passing in differently-sized arguments.

    unsigned long	value;
  
    // According to our GPIO expert, when modifying this GPIO to enable the time base,
    // you write a 1 to the DATA DIRECTION bit (bit 2) to make the GPIO be an output,
    // and leave the DATA VALUE bit 0.
    // To disable the time base, you make the DATA DIRECTION bit 0 to make it an open collector,
    // leaving the DATA VALUE bit 0.  The data bit value will then float to a 1 value.
    //
    // However, this is not how Mac OS 9 does it, nor does the operation described
    // work in practice. What IS done on Mac OS 9, and what appears to work in practice is
    // to set the DATA DIRECTION and the DATA VALUE bits to 0 to enable the time base.
    // To disable, set the DATA DIRECTION bit to 1 (output) and the DATA VALUE bit remains 0.
    //
    // This sounds a lot like TIMEBASE_EN is not an active low signal to me ...

#if 0
// What Mac OS 9 does, as performed in a Mac OS X context.
// Mac OS 9 actually does the I/Os to the GPIOs directly without having to call KeyLargo to do it.
    if ( enable )
    {
        keyLargo->callPlatformFunction(keyLargo_safeReadRegUInt8, false, (void *)&gpioOffset, (void *)&value,
                                        (void *)0, (void *)0);
        // set DATA DIRECTION bit to 0 (open collector) and set the DATA VALUE bit to 0 as well
        // (actually, in Mac OS 9, the disabling and enabling all happens in the same routine, SynchClock(),
        // so in the enabling, I believe their assumption is that the DATA VALUE bit is already 0.
        // I include the DATA VALUE zeroing in the below code just to be explicit about what's expected.
        value = ( 0 << kMacRISC_GPIO_DIRECTION_BIT );	// DATA_VALUE bit (0) is also zero implicitly
        keyLargo->callPlatformFunction(keyLargo_writeRegUInt8, false, (void *)&gpioOffset, (void *)value,
                                        (void *)0, (void *)0);
        // eieio();             // done in writeRegUInt8()
    }
    else        // disable
    {
        // this seems like overkill, but perhaps this is done this way due to the way KeyLargo works
        // (or for a given machine, it must be done this way and the other machines don't mind/care?).
        keyLargo->callPlatformFunction(keyLargo_safeReadRegUInt8, false,
                                        (void *)&gpioOffset, (void *)&value, (void *)0, (void *)0);
        value &= ~ 0x01;		// set DATA VALUE bit to 0
        keyLargo->callPlatformFunction(keyLargo_writeRegUInt8, false, (void *)&gpioOffset, (void *)value,
                                        (void *)0, (void *)0);
        // eieio();             // done in writeRegUInt8()
        value |= ( 1 << kMacRISC_GPIO_DIRECTION_BIT );		// set DATA DIRECTION bit to 1 (output)
        keyLargo->callPlatformFunction(keyLargo_writeRegUInt8, false, (void *)&gpioOffset, (void *)value,
                                        (void *)0, (void *)0);
       // eieio();              // done in writeRegUInt8()
        sync();
    }
#endif

    value = ( enable )?  ( 0 << kMacRISC_GPIO_DIRECTION_BIT )   // enable
                      :  ( 1 << kMacRISC_GPIO_DIRECTION_BIT );  // disable
    keyLargo->callPlatformFunction( keyLargo_writeRegUInt8, false,
                        (void *)&gpioOffset, (void *)value, (void *)0, (void *)0);
    if ( ! enable )	// let the processor instruction stream catch up
        sync();
}

void MacRISC2CPU::ipiHandler(void *refCon, void *nub, int source)
{
    // Call mach IPI handler for this CPU.
    if (ipi_handler) ipi_handler();
}

const OSSymbol *MacRISC2CPU::getCPUName(void)
{
    char tmpStr[256];
  
    sprintf(tmpStr, "Primary%ld", getCPUNumber());
  
    return OSSymbol::withCString(tmpStr);
}

#if enableUserClientInterface

// **********************************************************************************
//
// !!!!!!!  USER CLIENT ROUTINES  !!!!!!!! 
//
// **********************************************************************************

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
//  vSTEP code - vSTEP code - vSTEP code
//
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
IOReturn MacRISC2CPU::vStep(UInt32 newLevel)
{
	IOLog("MacRISC2CPU::vStep - START!\n");
        
	if (newLevel)
		vStepped = true;
	else
		vStepped = false;
		
	performPMUSpeedChange (newLevel);
	return kIOReturnSuccess;
}


IOReturn MacRISC2CPU::vStepCont(UInt32 delayTime)
{
	IOLog("MacRISC2CPU::vStepCont - START - delay time = %ld ms\n", delayTime);

	vStepTime = delayTime;

        fVStepContTimer->setTimeoutMS(vStepTime);

	IOLog("MacRISC2CPU::vStepCont - STOP\n");

	return kIOReturnSuccess;
}


// this is used to stop the continuous slewing started in slewContinuousWithTime
IOReturn MacRISC2CPU::vStepStopCont(void)
{
	IOLog("MacRISC2CPU::vStepStopCont START\n");

	// cancelling this timer stops slewContinuousWithTime 
	if(fVStepContTimer)
		fVStepContTimer->cancelTimeout();

	IOLog("MacRISC2CPU::vStepStopCont STOP\n");
	return kIOReturnSuccess;
}


void MacRISC2CPU::vStepContTimerEventOccurred(IOTimerEventSource *sender)
{
	//IOLog("\nMacRISC2CPU::vStepContTimerEventOccurred - START \n");
	
	// this timer is used for automatic slewing with a time interval in between slews.
	// this is for testing purposes only, and will not be called by the OS.
	if(vStepped == kSteppedLow)
	{
		//IOLog("MacRISC2CPU::vStepContTimerEventOccurred - vStep high\n");
		vStepped = kSteppedHigh;
		performPMUSpeedChange (kSteppedHigh);
		fVStepContTimer->setTimeoutMS(vStepTime);

	} else
	{
		//IOLog("MacRISC2CPU::vStepContTimerEventOccurred - vStep low\n");
		vStepped = kSteppedLow;
		performPMUSpeedChange (kSteppedHigh);		// ****** we only need to go thru the "motions" of vStepping so only go high *****
		//performPMUSpeedChange (kSteppedLow);
		fVStepContTimer->setTimeoutMS(vStepTime);
	}
		
	//IOLog("\nMacRISC2CPU::vStepContTimerEventOccurred - STOP \n");
}

// this routine is called every time fVStepContTimer fires. this is used for continous slewing (testing only).
void MacRISC2CPU::vStepTimerEventHandler(OSObject *self, IOTimerEventSource *sender)
{
	MacRISC2CPU*	mr2cpu = (MacRISC2CPU*)self;

	mr2cpu->vStepContTimerEventOccurred(sender);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//


IOReturn MacRISC2CPU::DFS(UInt32 newLevel, UInt32 mode)
{
	IOReturn cpfResult = kIOReturnSuccess;
	
	IOLog("MacRISC2CPU::DFS - START!\n");
        
	if (newLevel) {
		if (mode & kToggleDelayAACK)
			uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)1, (void *)0, (void *)0, (void *)0);
		else
			if (needAACKDelay)
				uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)1, (void *)0, (void *)0, (void *)0);

		DFS_Status = kDFSLow;
		if ( !( mode & kDelayAACKOnly)) {
			ml_set_processor_speed(newLevel ? 1 : 0);
			if (!( mode & kNoVoltage ))
				cpfResult = keyLargo->callPlatformFunction (keyLargo_setPowerSupply, false,
					(void *)0, (void *)0, (void *)0, (void *)0);
		}
	}
	else {
		if (!( mode & kNoVoltage ))
			cpfResult = keyLargo->callPlatformFunction (keyLargo_setPowerSupply, false,
				(void *)1, (void *)0, (void *)0, (void *)0);

		DFS_Status = kDFSHigh;
		if ( !( mode & kDelayAACKOnly)) 
			ml_set_processor_speed(newLevel ? 1 : 0);

		if (mode & kToggleDelayAACK)
			uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)0, (void *)0, (void *)0, (void *)0);
		else
			if (needAACKDelay)
				uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)0, (void *)0, (void *)0, (void *)0);
	}
	return kIOReturnSuccess;
}


IOReturn MacRISC2CPU::DFSCont(UInt32 delayTime, UInt32 mode)
{
	IOLog("MacRISC2CPU::DFSCont - START - delay time = %ld ms\n", delayTime);

	DFSTime = delayTime;

        fDFSContTimer->setTimeoutMS(DFSTime);

	DFScontMode = mode;
	
	IOLog("MacRISC2CPU::DFSCont - STOP\n");
	return kIOReturnSuccess;
}

IOReturn MacRISC2CPU::SetGPUPower(UInt32 GPUPowerLevel)
{
	IOLog("MacRISC2CPU::SetGPU - START - GPU Power level = %ld\n", GPUPowerLevel);

	pmRootDomain->setAggressiveness (kIOFBLowPowerAggressiveness, GPUPowerLevel);
	
	return kIOReturnSuccess;
}


IOReturn MacRISC2CPU::GPUCont(UInt32 delayTime)
{
	IOLog("MacRISC2CPU::GPUCont - START - delay time = %ld ms\n", delayTime);

	GPUTime = delayTime;

        fGPUContTimer->setTimeoutMS(GPUTime);

	DFScontMode = 0;
	
	return kIOReturnSuccess;
}


// this is used to stop the continuous slewing started in slewContinuousWithTime
IOReturn MacRISC2CPU::DFSStopCont(void)
{
	IOLog("MacRISC2CPU::DFSStopCont START\n");

	// cancelling this timer stops DFSContinuousWithTime 
	if(fDFSContTimer)
		fDFSContTimer->cancelTimeout();
	DFScontMode = 0;							// clear any special modes, i.e. AACK Only, no voltage change...
	IOLog("MacRISC2CPU::DFSStopCont STOP\n");
	return kIOReturnSuccess;
}

// this is used to stop the continuous slewing started in slewContinuousWithTime
IOReturn MacRISC2CPU::GPUStopCont(void)
{
	IOLog("MacRISC2CPU::GPUStopCont START\n");

	// cancelling this timer stops GPUContinuousWithTime 
	if(fGPUContTimer)
		fGPUContTimer->cancelTimeout();
	DFScontMode = 0;							// clear any special modes, i.e. AACK Only, no voltage change...
	IOLog("MacRISC2CPU::GPUStopCont STOP\n");
	return kIOReturnSuccess;
}


void MacRISC2CPU::DFSContTimerEventOccurred(IOTimerEventSource *sender)
{
	IOReturn cpfResult = kIOReturnSuccess;
	//IOLog("\nMacRISC2CPU::DFSContTimerEventOccurred - START \n");
	
	// this timer is used for automatic slewing with a time interval in between slews.
	// this is for testing purposes only, and will not be called by the OS.
	if(DFS_Status == kDFSLow)
	{
		//IOLog("MacRISC2CPU::DFSContTimerEventOccurred - set to DFS high\n");
		DFS_Status = kDFSHigh;
		if ( ! (DFScontMode & kDelayAACKOnly)) {
			if ( ! (DFScontMode & kNoVoltage)) 
				cpfResult = keyLargo->callPlatformFunction (keyLargo_setPowerSupply, false,
					(void *)1, (void *)0, (void *)0, (void *)0);
			
			ml_set_processor_speed(0);
		}

		if (DFScontMode & kToggleDelayAACK)
			uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)0, (void *)0, (void *)0, (void *)0);
		else
			if (needAACKDelay)
				uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)0, (void *)0, (void *)0, (void *)0);

		//uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)0, (void *)0, (void *)0, (void *)0);
		fDFSContTimer->setTimeoutMS(DFSTime);

	} else
	{
		//IOLog("MacRISC2CPU::DFSContTimerEventOccurred - set to DFS low\n");
		DFS_Status = kDFSLow;
		if (DFScontMode & kToggleDelayAACK)
			uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)1, (void *)0, (void *)0, (void *)0);
		else
			if (needAACKDelay)
				uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)1, (void *)0, (void *)0, (void *)0);

		//uniN->callPlatformFunction (uniN_setAACKDelay, false, (void *)1, (void *)0, (void *)0, (void *)0);
		
		if ( ! (DFScontMode & kDelayAACKOnly)) {
			ml_set_processor_speed(1);
			if ( ! (DFScontMode & kNoVoltage)) 
				cpfResult = keyLargo->callPlatformFunction (keyLargo_setPowerSupply, false,
					(void *)0, (void *)0, (void *)0, (void *)0);
		}
		fDFSContTimer->setTimeoutMS(DFSTime);
	}
		
	//IOLog("\nMacRISC2CPU::DFSContTimerEventOccurred - STOP \n");
}

void MacRISC2CPU::GPUContTimerEventOccurred(IOTimerEventSource *sender)
{
	//IOReturn cpfResult = kIOReturnSuccess;
	//IOLog("\nMacRISC2CPU::GPUContTimerEventOccurred - START \n");
	
	// this timer is used for automatic slewing with a time interval in between slews.
	// this is for testing purposes only, and will not be called by the OS.
	if(GPU_Status == kGPULow)
	{
		IOLog("MacRISC2CPU::GPUContTimerEventOccurred - set to GPU high\n");
		GPU_Status = kGPUHigh;
		pmRootDomain->setAggressiveness (kIOFBLowPowerAggressiveness, 0);
		fGPUContTimer->setTimeoutMS(GPUTime);

	} else
	{
		IOLog("MacRISC2CPU::GPUContTimerEventOccurred - set to GPU low\n");
		GPU_Status = kGPULow;
		pmRootDomain->setAggressiveness (kIOFBLowPowerAggressiveness, 2);
		fGPUContTimer->setTimeoutMS(GPUTime);
	}
		
	//IOLog("\nMacRISC2CPU::DFSContTimerEventOccurred - STOP \n");
}

// this routine is called every time fDFSContTimer fires. this is used for continous slewing (testing only).
void MacRISC2CPU::DFSTimerEventHandler(OSObject *self, IOTimerEventSource *sender)
{
	MacRISC2CPU*	mr2cpu = (MacRISC2CPU*)self;

	mr2cpu->DFSContTimerEventOccurred(sender);
}

// this routine is called every time fDFSContTimer fires. this is used for continous slewing (testing only).
void MacRISC2CPU::GPUTimerEventHandler(OSObject *self, IOTimerEventSource *sender)
{
	MacRISC2CPU*	mr2cpu = (MacRISC2CPU*)self;

	mr2cpu->GPUContTimerEventOccurred(sender);
}


bool MacRISC2CPU::initTimers(void)
{
	//IOLog("MacRISC2CPU::initTimers START\n");
	
	// set up the "DFS continuous" timer
	fDFSContTimer = IOTimerEventSource::timerEventSource(this, DFSTimerEventHandler);
	if(!fDFSContTimer) {
		IOLog("MacRISC2CPU::initTimers ERROR: failed to create fDFSContTimer\n");
		goto MacRISC2CPU_RemoveBothTimers;
	}

	// add the "DFS continuous" timer to the workloop
	if(fWorkLoop->addEventSource(fDFSContTimer) != kIOReturnSuccess) {
		IOLog("MacRISC2CPU::initTimers ERROR: failed to add fDFSContTimer to fWorkLoop\n");
		fDFSContTimer->release();
		goto MacRISC2CPU_RemoveBothTimers;
	}

	// set up the "GPU continuous" timer
	fGPUContTimer = IOTimerEventSource::timerEventSource(this, GPUTimerEventHandler);
	if(!fGPUContTimer) {
		IOLog("MacRISC2CPU::initTimers ERROR: failed to create fGPUContTimer\n");
		goto MacRISC2CPU_RemoveBothTimers2;
	}

	// add the "GPU continuous" timer to the workloop
	if(fWorkLoop->addEventSource(fGPUContTimer) != kIOReturnSuccess) {
		IOLog("MacRISC2CPU::initTimers ERROR: failed to add fGPUContTimer to fWorkLoop\n");
		fGPUContTimer->release();
		goto MacRISC2CPU_RemoveBothTimers2;
	}

	// set up the "vStep continuous" timer
	fVStepContTimer = IOTimerEventSource::timerEventSource(this, vStepTimerEventHandler);
	if(!fVStepContTimer) {
		IOLog("MacRISC2CPU::initTimers ERROR: failed to create fVStepContTimer\n");
		goto MacRISC2CPU_RemoveBothTimers3;
	}

	// add the "vStep continuous" timer to the workloop
	if(fWorkLoop->addEventSource(fVStepContTimer) != kIOReturnSuccess) {
		IOLog("MacRISC2CPU::initTimers ERROR: failed to add fVStepContTimer to fWorkLoop\n");
		fVStepContTimer->release();
		goto MacRISC2CPU_RemoveBothTimers3;
	}
	//IOLog("MacRISC2CPU::initTimers STOP\n");
	return true;

MacRISC2CPU_RemoveBothTimers:

	// if we fail after we've created and added both the fSlewContTimer and fVoltageDoneTimer
	// to the workloop, we have to remove and release them here.
	fWorkLoop->removeEventSource(fDFSContTimer);
	fDFSContTimer->release();


	IOLog("MacRISC2CPU::initTimers STOP EARLY\n");
	return false;

MacRISC2CPU_RemoveBothTimers2:

	// if we fail after we've created and added both the fSlewContTimer and fVoltageDoneTimer
	// to the workloop, we have to remove and release them here.
	fWorkLoop->removeEventSource(fGPUContTimer);
	fGPUContTimer->release();


	IOLog("MacRISC2CPU::initTimers STOP EARLY\n");
	return false;


MacRISC2CPU_RemoveBothTimers3:

	// if we fail after we've created and added both the fSlewContTimer and fVoltageDoneTimer
	// to the workloop, we have to remove and release them here.
	fWorkLoop->removeEventSource(fVStepContTimer);
	fVStepContTimer->release();


	IOLog("MacRISC2CPU::initTimers STOP EARLY\n");
	return false;
}

#endif
