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


#include <sys/cdefs.h>

__BEGIN_DECLS
#if defined( __ppc__ )
#include <ppc/proc_reg.h>
#endif
#include <machine/machine_routines.h>
__END_DECLS

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOCPU.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#include "MacRISC4CPU.h"

#define kMacRISC_GPIO_DIRECTION_BIT	2

#ifndef kIOHibernateStateKey
#define kIOHibernateStateKey	"IOHibernateState"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOCPU

OSDefineMetaClassAndStructors(MacRISC4CPU, IOCPU);

/*
 * Platform-specific parameters needed for CPU timebase synchronization.  This
 * data is keyed by (ughh..) model property.
 */

#define kU3IIC "PPCI2CInterface.i2c-uni-n"

/* PowerMac7,2 - cypress CY28508 clock chip @ IIC_B 0xD2 */
static const cpu_timebase_params_t cypress =
	{ kU3IIC, /* port */ 0x00, /* addr */ 0xD0, /* subaddr */ 0x81,
	  /* mask */ 0x0C, /* enable */ 0x0C, /* disable */ 0x00 };

/* RackMac3,1 and PowerMac7,3 - pulsar clock chip @ IIC_B 0xD4 */
static const cpu_timebase_params_t pulsar =
	{ kU3IIC, /* port */ 0x00, /* addr */ 0xD4, /* subaddr */ 0x2E,
	  /* mask */ 0x77, /* enable */ 0x22, /* disable */ 0x11 };

/* RackMac3,1 and PowerMac7,3 - pulsar clock chip @ IIC_B 0xD2 */
static const cpu_timebase_params_t pulsarD2 =
        { kU3IIC, /* port */ 0x00, /* addr */ 0xD2, /* subaddr */ 0x2E,
          /* mask */ 0x77, /* enable */ 0x22, /* disable */ 0x11 };

/*
 * These global variables are used for I2C communication in enableCPUTimeBase
 * The process is initiated in startCPU for CPU 1 but is completed in
 * enableCPUTimeBase, called on CPU 0.  gI2CTransactionComplete is used to 
 * synchronize the operation and must be shared between the two instances.
 *
 * This is all necessary because enableCPUTimeBase is called on an interrupt
 * context and cannot block.  But we need to ensure exclusive access to the I2C
 * bus which is controlled by a lock in openI2Cbus.  So we grab the bus in 
 * startCPU(1), complete the transaction in enableCPUTimeBase and once
 * it has set gI2CTransactionComplete true, resume startCPU(1) and close the
 * I2C bus, releasing the lock.
 *
 * This will have to change if we have more than two CPUs.
 */
static bool								gI2CTransactionComplete;
static IOService						*gI2CDriver,
										*gTBDriver;
static const OSSymbol					*gTBFunctionNameSym;
static const OSSymbol					*gTBReadFunctionNameSym;		// XXX Temp!!!
static const cpu_timebase_params_t		*gTimeBaseParams;
static UInt32 							*gPHibernateState;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOCPUInterruptController *gCPUIC;

bool MacRISC4CPU::start(IOService *provider)
{
#if defined( __ppc__ )
    kern_return_t        result;
#endif
    IORegistryEntry      *cpusRegEntry, *cpu0RegEntry, *mpicRegEntry;
    OSIterator           *cpusIterator;
    OSData               *tmpData;
    IOService            *service;
    const OSSymbol       *interruptControllerName, *mpicICSymbol;
    OSData               *interruptData, *parentICData;
    OSArray              *tmpArray;
    OSDictionary         *matchDict;
    char                 mpicICName[48];
	bool				 makeInterruptsProperties;
    UInt32               maxCPUs, physCPU, mpicPHandle;
    IOService            *tbResources;
    ml_processor_info_t  processor_info;
				
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
    keyLargo_setPowerSupply = OSSymbol::withCString("setPowerSupply");
    UniNSetPowerState = OSSymbol::withCString("UniNSetPowerState");
    UniNPrepareForSleep = OSSymbol::withCString("UniNPrepareForSleep");
    i2c_openI2CBus = OSSymbol::withCString("openI2CBus");
    i2c_closeI2CBus = OSSymbol::withCString("closeI2CBus");
    i2c_setCombinedMode = OSSymbol::withCString("setCombinedMode");
    i2c_setStandardSubMode = OSSymbol::withCString("setStandardSubMode");
    i2c_readI2CBus = OSSymbol::withCString("readI2CBus");
    i2c_writeI2CBus = OSSymbol::withCString("writeI2CBus");
	u3APIPhyDisableProcessor1 = OSSymbol::withCString("u3APIPhyDisableProcessor1");
    
    macRISC4PE = OSDynamicCast(MacRISC4PE, getPlatform());
    if (macRISC4PE == 0) return false;
  
    if (!super::start(provider)) return false;

    // Find out if this is the boot CPU.
    tmpData = OSDynamicCast(OSData, provider->getProperty("state"));
    if (tmpData == 0) return false;
    bootCPU = (strcmp((char *)tmpData->getBytesNoCopy(), "running") == 0);

    // Count the CPUs.
    numCPUs = 0;
    cpusRegEntry = fromPath("/cpus", gIODTPlane);
    if (cpusRegEntry == 0) return false;
    cpusIterator = cpusRegEntry->getChildIterator(gIODTPlane);
    while (cpusIterator->getNextObject()) numCPUs++;
    cpusIterator->release();
  
	// [3775206] - The bootCPU driver inits globals for all instances (like gCPUIC) so if we're not the
	// boot CPU driver we wait here for that driver to finish its initialization
	if ((numCPUs > 1) && !bootCPU)
		// Wait for bootCPU driver to say it's up and running
		(void) waitForService (resourceMatching ("BootCPU"));

    // Limit the number of CPUs by the cpu=# boot arg.
    if (PE_parse_boot_arg("cpus", &maxCPUs))
    {
        if (numCPUs > maxCPUs) numCPUs = maxCPUs;
    }

	// If we're MP, fetch the timebase parameters.  If we can't sync timebase, limit the number of CPUs to 1.
	if (numCPUs > 1 && (!(gTimeBaseParams || gTBDriver))) {
		// First try for standard platform function way.  This usually just requires toggling a GPIO so it
		// is very easy and we get a reference to the GPIO driver to do it for us
		if (cpusRegEntry->getProperty ("platform-cpu-timebase")) {
			// OK a platform function handler exists so go find it
			OSData *pHandle;
			char	functionName[64];
			
			if ((pHandle = OSDynamicCast( OSData, cpusRegEntry->getProperty("AAPL,phandle") )) != NULL) {
				// Build the call plaform function name by appending our phandle
				sprintf(functionName, "%s-%08lx", "platform-cpu-timebase", *((UInt32 *)pHandle->getBytesNoCopy()));
				//MYLOG ("4CPU: got tb function '%s'\n", functionName);
				gTBFunctionNameSym = OSSymbol::withCString(functionName);

				// XXX - This is temporary for Waveland bringup
				if (cpusRegEntry->getProperty ("platform-read-cpu-timebase")) {
					sprintf(functionName, "%s-%08lx", "platform-read-cpu-timebase", 
						*((UInt32 *)pHandle->getBytesNoCopy()));
					//MYLOG ("4CPU: got tb read function '%s'\n", functionName);
					gTBReadFunctionNameSym = OSSymbol::withCString(functionName);
				}
			}
		} else {
			/*
			 * Bummer, we don't have an easy way.  Instead we need to do an I2C transaction to the clock driver
			 * This turns out to be tricky because the disable code needs to run from an interrupt context
			 * See startCPU() for more explanation
			 */
			IORegistryEntry * clockchip;
	
			// PowerMac7,2 (CY28508) and PowerMac7,3 (Pulsar)
			if ((strcmp(macRISC4PE->provider_name, "PowerMac7,2") == 0) || (strcmp(macRISC4PE->provider_name, "PowerMac7,3") == 0)) {
				// look for cypress/pulsar at slave address 0xd2
				if ((clockchip = fromPath("/u3/i2c/i2c-hwclock@d2", gIODTPlane, 0, 0, 0)) != NULL) {
					OSString *pulsarCompat, *cypressCompat;
					
					pulsarCompat = OSString::withCString ("pulsar-legacy-slewing");
					cypressCompat = NULL;
					if (IODTCompareNubName(clockchip, pulsarCompat, NULL)) {
						gTimeBaseParams = &pulsarD2;
					} else {
						cypressCompat = OSString::withCString ("cy28508");
						if (IODTCompareNubName(clockchip, cypressCompat, NULL)) {
							gTimeBaseParams = &cypress;
						}
					}

					pulsarCompat->release();
					if (cypressCompat) cypressCompat->release();
				} else if ((clockchip = fromPath("/u3/i2c/i2c-hwclock@d4", gIODTPlane, 0, 0, 0)) != NULL) {
					// look for pulsar at slave address 0xd4
					gTimeBaseParams = &pulsar;
				}
				if (clockchip) clockchip->release();
			}
	
			// RackMac3,1 (Pulsar)
			else if (strcmp(macRISC4PE->provider_name, "RackMac3,1") == 0)
			{
				// look for pulsar at slave address 0xd4
				if ((clockchip = fromPath("/u3/i2c/i2c-hwclock@d4", gIODTPlane, 0, 0, 0)) != NULL)
				{
					clockchip->release();
					gTimeBaseParams = &pulsar;
				}
			}
		}

		// if we haven't figured out how to sync timebase, then limit the number of CPUs to 1
		if (!(gTimeBaseParams || gTBFunctionNameSym)) {
				IOLog("WARNING: don't know how to sync MP timebase, limiting to one CPU\n");
				kprintf("WARNING: don't know how to sync MP timebase, limiting to one CPU\n");
				numCPUs = 1;
			}
	}
	
	doSleep = false;
  
    // Get the "flush-on-lock" property from the first cpu node.  "flush-on-lock" mean do not "nap" the processor.
    flushOnLock = false;
    cpu0RegEntry = fromPath("/cpus/@0", gIODTPlane);
    if (cpu0RegEntry == 0) return false;
    if (cpu0RegEntry->getProperty("flush-on-lock") != 0) flushOnLock = true;
  
    // Set flushOnLock when numCPUs is not one.
    //if (numCPUs != 1) flushOnLock = true;
  
    // Get the physical CPU number from the "reg" property.
    tmpData = OSDynamicCast(OSData, provider->getProperty("reg"));
    if (tmpData == 0) return false;
    physCPU = *(long *)tmpData->getBytesNoCopy();
    setCPUNumber(physCPU);

	if (numCPUs > 1) {
		// Get the gpio offset for soft reset from the "soft-reset" property.
		tmpData = OSDynamicCast(OSData, provider->getProperty("soft-reset"));
		if (tmpData == 0) 
		{
			IOLog ("MacRISC4CPU::start - cpu(%ld) - no soft-reset property\n", physCPU);
			return false;
		} else
			soft_reset_offset = *(long *)tmpData->getBytesNoCopy();
	}
   
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
        l2crValue = *(long *)tmpData->getBytesNoCopy() & 0x7FFFFFFF;
#if defined( __ppc__ )
    else
        l2crValue = mfl2cr() & 0x7FFFFFFF;
#endif
  
	//kprintf ("MacRISC4CPU::start - waiting for KeyLargo\n");
    // Wait for KeyLargo to show up.
    keyLargo = waitForService(serviceMatching("KeyLargo"));
    if (keyLargo == 0) return false;
    
	// Set the IOInterruptController and IOInterruptSpecifier properties if they don't exist
	if (tmpArray = OSDynamicCast (OSArray, (cpuNub->getProperty(gIOInterruptControllersKey)))) {
		// The properties exist so all we need to do is locate the right MPIC
		makeInterruptsProperties = false;
		mpicICSymbol = OSSymbol::withString ((const OSString *)tmpArray->getObject(0));
	} else {
		makeInterruptsProperties = true;

		// Find the interrupt controller specified by the provider.
		parentICData = OSDynamicCast(OSData, provider->getProperty(kMacRISC4ParentICKey));
		if (parentICData) {
			mpicPHandle = *(UInt32 *)parentICData->getBytesNoCopy();
			sprintf(mpicICName, "IOInterruptController%08lX", mpicPHandle);
			mpicICSymbol = OSSymbol::withCString(mpicICName);
		} else {
			IOLog ("MacRISC4CPU::start - no cpu IOInterruptController!  Start failed!\n");
			return false;
		}
	}
	
	// Get the MPIC reference
    matchDict = serviceMatching("AppleMPICInterruptController");
    matchDict->setObject("InterruptControllerName", mpicICSymbol);
    mpic = waitForService(matchDict);
    
	if (makeInterruptsProperties) {
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
	}

  
    setCPUState(kIOCPUStateUninitalized);
  
    // Before to go to sleep we wish to disable the napping mode so that the PMU
    // will not shutdown the system while going to sleep:
    service = waitForService(serviceMatching("IOPMrootDomain"));
    pmRootDomain = OSDynamicCast(IOPMrootDomain, service);
    if (pmRootDomain != 0)
    {
        kprintf("Register MacRISC4CPU %d to acknowledge power changes\n", getCPUNumber());
        pmRootDomain->registerInterestedDriver(this);
        
        // Join the Power Management Tree to receive setAggressiveness calls.
        PMinit();
        provider->joinPMtree(this);
    }

    registerService();

	if (!(uniN = waitForService(serviceMatching("AppleU3")))) return false;
	/*
	 * If numCPUs is one, disable *any* second processor that might be present because it could
	 * be running and stealing cycles on the bus [3249029].  This also fixes a hang when 
	 * boot-args cpus=1 is set [3273619]  
	 */
	if (numCPUs == 1) 
		uniN->callPlatformFunction (u3APIPhyDisableProcessor1, false, (void *)0, (void *)0, (void *)0, (void *)0);

	// We have to get a pointer to the timebase driver now -- when we're syncing timebase, we'll be at
	// interrupt context and can't go looking for drivers
	if (numCPUs > 1) {
		if (gTimeBaseParams && !gI2CDriver) {
			//kprintf("MacRISC4CPU(%ld) looking for %s\n", getCPUNumber(), gTimeBaseParams->i2c_iface);
			tbResources = waitForService (resourceMatching ( gTimeBaseParams->i2c_iface ));
			//kprintf("MacRISC4CPU(%ld) done looking\n", getCPUNumber());
	
			if (tbResources) {
				gI2CDriver = OSDynamicCast (IOService, tbResources->getProperty ( gTimeBaseParams->i2c_iface ));
				if (!gI2CDriver)
				{
					kprintf ("MacRISC4CPU::start(%ld) - failed i2cDriver\n", getCPUNumber());
					return false;
				}
			}
		} else if (gTBFunctionNameSym && !gTBDriver) {
#if 0
			mach_timespec_t waitTimeout;
			
			waitTimeout.tv_sec = 15;
			waitTimeout.tv_nsec = 0;
	
			// MYLOG ("4CPU::startCPU - waiting for TBDriver with 15 second timeout\n");
			tbResources = waitForService(resourceMatching(gTBFunctionNameSym), &waitTimeout);
#else
			// MYLOG ("4CPU::startCPU - waiting for TBDriver\n");
			tbResources = waitForService(resourceMatching(gTBFunctionNameSym));
#endif
			if (tbResources) 
				gTBDriver = OSDynamicCast (IOService, tbResources->getProperty(gTBFunctionNameSym));
			// else
				// MYLOG ("4CPU::startCPU - timeout on TBDriver\n");
			
			// if (gTBDriver) MYLOG ("4CPU::startCPU - got TB service\n");
		}
	}

	// necessary bootCPU initialization is done, so release other CPU drivers to do their thing
	// other drivers need to be unblocked *before* we call processor_start otherwise we deadlock
	if (bootCPU)
		publishResource ("BootCPU", this);

    if (physCPU < numCPUs)
    {
        processor_info.cpu_id           = (cpu_id_t)this;
        processor_info.boot_cpu         = bootCPU;
        processor_info.start_paddr      = 0x0100;
        processor_info.l2cr_value       = l2crValue;
        processor_info.supports_nap     = !flushOnLock;
        processor_info.time_base_enable = MacRISC4CPU::sEnableCPUTimeBase;

#if defined( __ppc__ )
		OSData*							powerTuneData;

		// power_mode_0 and power_mode_1 are defined in a newer version of ppc/machine_routines.h.

		if ( ( powerTuneData = OSDynamicCast( OSData, cpu0RegEntry->getProperty( "power-mode-data" ) ) ) != NULL )
		{
			uint32_t*					powerTuneEntry;

			powerTuneEntry = ( uint32_t * ) powerTuneData->getBytesNoCopy();

			// Only copy power_mode_1 if it exists.

			if ( powerTuneData->getLength() >= ( 2 * sizeof( uint32_t ) ) )
				{
				processor_info.power_mode_0 = powerTuneEntry[ 0 ];
				processor_info.power_mode_1 = powerTuneEntry[ 1 ];
				}
		}
#endif

		// Register this CPU with mach.

#if defined( __ppc__ )
		kprintf("MacRISC4CPU::start(%ld) - registering with mach\n", getCPUNumber());
		result = ml_processor_register(&processor_info, &machProcessor,	&ipi_handler);
		if (result == KERN_FAILURE) return false;
#endif
	
		processor_start(machProcessor);
	}

    // Finds PMU so in quiesce we can put the machine to sleep.
    // I can not put these calls there because quiesce runs in interrupt
    // context and waitForService may block.
	if (!pmu) {

		service = waitForService(resourceMatching("IOPMU"));
		if (service) 
			if (!(pmu = OSDynamicCast (IOService, service->getProperty("IOPMU")))) return false;
	}

	// Some systems require special handling of Ultra-ATA at sleep.
	// Call UniN to prepare for that, if necessary
	uniN->callPlatformFunction ("setupUATAforSleep", false, (void *)0, (void *)0, (void *)0, (void *)0);

	if (cpusRegEntry) cpusRegEntry->release();
	if (cpu0RegEntry) cpu0RegEntry->release();
	
	kprintf ("MacRISC4CPU::start(%ld) - done\n", getCPUNumber());
    return true;
}

/* initCPU - not called at interrupt context but must not block */
void MacRISC4CPU::initCPU(bool boot)
{
	IOPCIBridge		*pciDriver;
	UInt32			i;

    if (!boot && bootCPU) {
		// Tell Uni-N to enter normal mode.
		uniN->callPlatformFunction (UniNSetPowerState, false, (void *)kUniNNormal,
			(void *)0, (void *)0, (void *)0);
    
		/*
		* If numCPUs is one, disable *any* second processor that might be present because it could
		* be running and stealing cycles on the bus [3249029].  This also fixes a hang when 
		* boot-args cpus=1 is set [3273619].  This is called during start() but is called again here
		* to cover the wake from sleep case.
		*/
		if (numCPUs == 1) 
			uniN->callPlatformFunction (u3APIPhyDisableProcessor1, false, (void *)0, (void *)0, (void *)0, (void *)0);
		
        if (!processorSpeedChange) {
			// Notify our pci children to restore their state
			for (i = 0; i < topLevelPCIBridgeCount; i++)
				if (pciDriver = topLevelPCIBridges[i])
					// Got the driver - send the message
					pciDriver->setDevicePowerState (NULL, 3);

			keyLargo->callPlatformFunction(keyLargo_restoreRegisterState, false, 0, 0, 0, 0);
	
			// Enables the interrupts for this CPU.
			if (macRISC4PE->getMachineType() == kMacRISC4TypePowerMac) {
				haveSleptMPIC = false;
				kprintf("MacRISC4CPU::initCPU %d -> mpic->setUpForSleep off", getCPUNumber());
				mpic->callPlatformFunction(mpic_setUpForSleep, false, (void *)false, (void *)getCPUNumber(), 0, 0);
			}
		}
    }

    kprintf("MacRISC4CPU::initCPU %d Here!\n", getCPUNumber());
 
    // Set time base.
    if (bootCPU)
        keyLargo->callPlatformFunction(keyLargo_syncTimeBase, false, 0, 0, 0, 0);
  
    if (boot) {
		if (gCPUIC)
        	gCPUIC->enableCPUInterrupt(this);
		else
			panic ("MacRISC4CPU: gCPUIC uninitialized for CPU %d\n", getCPUNumber());
    
        // Register and enable IPIs.
        cpuNub->registerInterrupt(0, this, MacRISC4CPU::sIPIHandler, 0);
        cpuNub->enableInterrupt(0);
    } else {
        long priority = 0;
        mpic->callPlatformFunction(mpic_setCurrentTaskPriority, false, (void *)&priority, 0, 0, 0);
    }
  
    setCPUState(kIOCPUStateRunning);
		
	kprintf ("MacRISC4CPU::initCPU(%ld) - done\n", getCPUNumber());
	return;
}

/* quiesceCPU - may be called at interrupt context */
void MacRISC4CPU::quiesceCPU(void)
{
    if (bootCPU)
    {
        // Have U3 save state
		uniN->callPlatformFunction (UniNSetPowerState, false, (void *)(kUniNSave),
			(void *)0, (void *)0, (void *)0);

		if (!gPHibernateState || !*gPHibernateState) {
			// Tell U3 to enter sleep mode if not hibernating
			// For U3, this has to be done before telling the PMU to start going to sleep
			uniN->callPlatformFunction (UniNSetPowerState, false, (void *)(kUniNSleep),
				(void *)0, (void *)0, (void *)0);
        }

        if (processorSpeedChange) {
            // Send PMU command to speed the system
            pmu->callPlatformFunction("setSpeedNow", false, (void *)currentProcessorSpeed, 0, 0, 0);
        } else {
			/*
			 * Send PMU command to shutdown system before io is turned off
			 *
			 * After the command to PMU is sent we only have 100 milliseconds to quiesce
			 * the cpu, so unnecessary delays, like kprints must be avoided.
			 */
			if (!gPHibernateState || !*gPHibernateState)
				pmu->callPlatformFunction("sleepNow", false, 0, 0, 0, 0);
	
			// Disables the interrupts for this CPU.
			if (!haveSleptMPIC && (macRISC4PE->getMachineType() == kMacRISC4TypePowerMac))
			{
				haveSleptMPIC = true;
				mpic->callPlatformFunction(mpic_setUpForSleep, false, (void *)true, (void *)getCPUNumber(), 0, 0);
			}
	
			// Save KeyLargo's register state.
			keyLargo->callPlatformFunction(keyLargo_saveRegisterState, false, 0, 0, 0, 0);
	
			// Turn Off all KeyLargo I/O.
			if (!gPHibernateState || !*gPHibernateState) {
				keyLargo->callPlatformFunction(keyLargo_turnOffIO, false, (void *)false, 0, 0, 0);
        	}
        }
        
        // Set the wake vector to point to the reset vector
        ml_phys_write(0x0080, 0x100);
	}

#if defined( __ppc__ )
    ml_ppc_sleep();
#endif

	return;
}

/* startCPU - not called at interrupt context */
kern_return_t MacRISC4CPU::startCPU(vm_offset_t /*start_paddr*/, vm_offset_t /*arg_paddr*/)
{
    long            gpioOffset = soft_reset_offset;
    unsigned long   strobe_value;

	//kprintf("MacRISC4CPU::startCPU(%ld) - entered\n", getCPUNumber());

	// check for timebase sync info
	if (!(gTimeBaseParams || gTBDriver))
	{
		IOLog("MacRISC4CPU::startCPU(%ld) - cannot manage timebase synchronization\n", getCPUNumber() );
		return KERN_FAILURE;
	}

	/*
	 * On MP machines, once we enable the second CPU the kernel will want to sync the timebases
	 * between the CPUs and will call enableCPUTimeBase to disable then enable the timebase around
	 * the sync operation.  enableCPUTimeBase is called on an interrupt context and so it cannot block.
	 * First generation G5 machines required an I2C transaction to the clock chip to disable the timebase
	 * To ensure exclusive access to the I2C bus by enableCPUTimeBase, we open the I2C bus here, where
	 * it is safe to block, then wait for the I2C transaction to complete after which we can close out
	 * the bus.
	 *
	 * Later machines (those for which gTBDriver is non-NULL) use a GPIO instead and in enableCPUTimeBase
	 * we can make a non-blocking call into the GPIO driver so this nonsense isn't necessary.
	 */
	if (gTimeBaseParams) {
		gI2CTransactionComplete = false;
		if (gI2CDriver && (kIOReturnSuccess == gI2CDriver->callPlatformFunction (i2c_openI2CBus, false,
				(void *) (UInt32)gTimeBaseParams->i2c_port, (void *) 0, (void *) 0, (void *) 0))) {
			kprintf ("MacRISC4CPU::startCPU(%ld) i2c bus opened\n", getCPUNumber());
		}
	}
	
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

	// Now wait for I2C transaction, handled by enableCPUTimeBase, to complete
	if (gTimeBaseParams) {
		while (!gI2CTransactionComplete) IOSleep (10);			// Spin until done

		gI2CDriver->callPlatformFunction (i2c_closeI2CBus, false, (void *) 0, (void *) 0, (void *) 0, (void *) 0);	//CY28510 does reads in combined mode

		kprintf ("MacRISC4CPU::startCPU(%ld) i2c bus closed\n", getCPUNumber());
	}
	
    return KERN_SUCCESS;
}

/* haltCPU - not called at interrupt context */
void MacRISC4CPU::haltCPU(void)
{  
	OSIterator 		*childIterator;
	IORegistryEntry *childEntry, *childDriver;
	IOPCIBridge		*pciDriver;
	OSData			*deviceTypeString;
	UInt32			i;

    setCPUState(kIOCPUStateStopped);
  
    if (bootCPU)
    {
		if (!gPHibernateState) {
			OSData * data = OSDynamicCast(OSData, getPMRootDomain()->getProperty(kIOHibernateStateKey));
			if (data)
				gPHibernateState = (UInt32 *) data->getBytesNoCopy();
		}
        
		uniN->callPlatformFunction (UniNPrepareForSleep, false, 
			(void *)0, (void *)0, (void *)0, (void *)0);
		// Notify our pci children to save their state
		if (!topLevelPCIBridgeCount) {
			// First build list of top level bridges - only need to do once as these don't change
			if ((childIterator = macRISC4PE->getChildIterator (gIOServicePlane)) != NULL) {
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
										kprintf ("MacRISC4CPU::haltCPU - warning, more than %ld PCI bridges - cannot save/restore them all\n", kMaxPCIBridges);
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

	kprintf("MacRISC4CPU::haltCPU %d Here!\n", getCPUNumber());

	processor_exit(machProcessor);
	
	return;
}

/* signalCPU - may be called at interrupt context */
void MacRISC4CPU::signalCPU(IOCPU *target)
{
    UInt32 physCPU = getCPUNumber();
    MacRISC4CPU *targetCPU = OSDynamicCast(MacRISC4CPU, target);
  
    if (targetCPU == 0) return;
  
    mpic->callPlatformFunction(mpic_dispatchIPI, false, (void *)&physCPU, (void *)(1 << targetCPU->getCPUNumber()), 0, 0);
	
	return;
}

void MacRISC4CPU::sEnableCPUTimeBase( cpu_id_t self, boolean_t enable )
{
	MacRISC4CPU*			pe = ( MacRISC4CPU * ) self;

	pe->enableCPUTimeBase( enable );
}

/*
 * Read the CPU timebase registers.  This is not normally necessary so it's compiled
 * out.  But I'm keeping it around because it's handy during development to make
 * sure we are enabling/disabling the timebase correctly
 */
#if 0
static void getCPUTimebase (UInt32 *tbuRet, UInt32 *tblRet)
{
	UInt32 tbu, tbu2, tbl;
		
	do {
		asm volatile("  mftbu %0" : "=r" (tbu));
		asm volatile("  mftb %0" : "=r" (tbl));
		asm volatile("  mftbu %0" : "=r" (tbu2));
	} while (tbu != tbu2);
	
	*tbuRet = tbu;
	*tblRet = tbl;
	
	return;
}
#endif

/* enableCPUTimeBase - IS called at interrupt context */
void MacRISC4CPU::enableCPUTimeBase(bool enable)
{
	if (gTBDriver) {
		UInt32		gpioValue;
		UInt32 		count = 0;
		
		// XXX - temp for Waveland bringup - for Waveland we always drive the GPIO low
		// SMU will set it high when it is done which is how we know it is complete - see
		// the poll loop below.
		// For normal systems, the GPIO itself drives the timebase signal so we just have
		// to set it high or low to control it.
		if (gTBReadFunctionNameSym) 
			gpioValue = 0;
		else
			gpioValue = enable ? 1 : 0;
		
		// GPIO driver can handle it
		gTBDriver->callPlatformFunction (gTBFunctionNameSym, false, (void *)gpioValue, 0, 0, 0);
		
		// XXX - temp for Waveland bringup
		if (gTBReadFunctionNameSym) {
			UInt32 gpioState;
				
			do {
								
				count++;
				// Wait for SMU to restore GPIO state
				gTBDriver->callPlatformFunction (gTBReadFunctionNameSym, false, (void *)&gpioState, 0, 0, 0);
				//getCPUTimebase (&tbu, &tbl);
				//kprintf ("4CPU::tb read tb gpio got value %d, current tb values %d, %ud\n", gpioState, tbu, tbl);
			} while ((gpioState == 0) && (count < 15));
			
			//getCPUTimebase (&tbu, &tbl);
		
			kprintf ("MacRISC4CPU::enableCPUTimeBase(%s) complete\n", enable ? "enable" : "disable");
		}
	} else {
		// Do it messy I2C way 
		UInt8 sevenBitAddr, buf, tmp;
			
		// Set combined mode for read
		gI2CDriver->callPlatformFunction (i2c_setCombinedMode, false, (void *) 0, (void *) 0, (void *) 0, (void *) 0);	//CY28510 does reads in combined mode
	
		// read the byte register -- requires 7 bit slave address
		sevenBitAddr = gTimeBaseParams->i2c_addr >> 1;
		if (kIOReturnSuccess == gI2CDriver->callPlatformFunction (i2c_readI2CBus, false, (void *)(UInt32)sevenBitAddr, (void *) (UInt32)gTimeBaseParams->i2c_subaddr, (void *)(UInt32)&buf, (void *)1)) {
			// apply mask and value
			tmp = enable ? gTimeBaseParams->enable_value : gTimeBaseParams->disable_value; 
			buf = (buf & ~gTimeBaseParams->mask) | (tmp & gTimeBaseParams->mask);
			
			// Set standard sub mode for write
			gI2CDriver->callPlatformFunction (i2c_setStandardSubMode, false, (void *)0,(void *) 0, (void *)0, (void *)0);

			// Issue write for enable/disable
			gI2CDriver->callPlatformFunction (i2c_writeI2CBus, false, (void *)(UInt32)sevenBitAddr,(void *) (UInt32)gTimeBaseParams->i2c_subaddr, (void *)(UInt32)& buf, (void *)1);
		} else {
			kprintf ("MacRISC4CPU::enableCPUTimeBase - I2C read failed\n");
			return;
		}
		/*
		* We generally get called twice, the first time to disable the timebase and the second
		* time to enable it.  We're only done after the second call, i.e., enable is true.
		* Setting gI2CTransactionComplete to true signals startCPU that it is OK to close
		* the I2C bus.
		*/
		gI2CTransactionComplete = enable;
		kprintf ("MacRISC4CPU::enableCPUTimeBase(%s) - I2C transaction complete\n", enable ? "enable" : "disable");
	}

	return;
}


void MacRISC4CPU::sIPIHandler( OSObject* self, void* refCon, IOService* nub, int source )
	{
	MacRISC4CPU*				pe = ( MacRISC4CPU * ) self;

	pe->ipiHandler( refCon, nub, source );
	}


void MacRISC4CPU::ipiHandler(void *refCon, void *nub, int source)
{
    // Call mach IPI handler for this CPU.
    if (ipi_handler) ipi_handler();
	
	return;
}

const OSSymbol *MacRISC4CPU::getCPUName(void)
{
    char tmpStr[256];
  
    sprintf(tmpStr, "Primary%ld", getCPUNumber());
  
    return OSSymbol::withCString(tmpStr);
}
