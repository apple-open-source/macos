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
//		$Log: MacRISC4CPU.cpp,v $
//		Revision 1.15  2003/07/03 01:16:32  raddog
//		[3313953]U3 PwrMgmt register workaround
//		
//		Revision 1.14  2003/06/27 00:45:07  raddog
//		[3304596]: remove unnecessary access to U3 Pwr registers on wake, [3249029]: Disable unused second process on wake, [3301232]: remove unnecessary PCI code from PE
//		
//		Revision 1.13  2003/06/04 20:21:41  raddog
//		Improved fix - disable second cpu when unused - 3249029, 3273619
//		
//		Revision 1.12  2003/06/03 23:03:57  raddog
//		disable second cpu when unused - 3249029, 3273619
//		
//		Revision 1.11  2003/06/03 01:49:44  raddog
//		Remove late sleep kprintfs
//		
//		Revision 1.10  2003/05/07 00:14:55  raddog
//		[3125575] MacRISC4 initial sleep support
//		
//		Revision 1.9  2003/04/27 23:13:30  raddog
//		MacRISC4PE.cpp
//		
//		Revision 1.8  2003/04/14 20:05:27  raddog
//		[3224952]AppleMacRISC4CPU must specify which MPIC to use (improved fix over that previously submitted)
//		
//		Revision 1.7  2003/04/04 01:26:25  raddog
//		[3217875] Q37: MacRISC4CPU - needs to be sure it locates host MPIC
//		
//		Revision 1.6  2003/03/04 17:53:20  raddog
//		[3187811] P76: U3.2.0 systems don't boot
//		[3187813] MacRISC4CPU bridge saving code can block on interrupt stack
//		[3138343] Q37 Feature: remove platform functions for U3
//		
//		Revision 1.5  2003/02/27 01:42:54  raddog
//		Better support for MP across sleep/wake [3146943]. This time we block in startCPU, rather than initCPU, which is safer.
//		
//		Revision 1.4  2003/02/19 22:35:44  raddog
//		Better support for MP across sleep/wake [3146943]
//		
//		Revision 1.3  2003/02/19 21:54:45  raddog
//		Support for MP across sleep/wake [3146943]
//		
//		Revision 1.2  2003/02/18 00:02:01  eem
//		3146943: timebase enable for MP, bump version to 1.0.1d3.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#include <sys/cdefs.h>

__BEGIN_DECLS
#include <ppc/proc_reg.h>
__END_DECLS

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOCPU.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#include "MacRISC4CPU.h"

#define kMacRISC_GPIO_DIRECTION_BIT	2

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOCPU

OSDefineMetaClassAndStructors(MacRISC4CPU, IOCPU);

// defines for CPU timebase enable
enum {
	kI2CPort = 0x0,			// U3 Master Port 0
	kI2CAddr = 0xD0,

	kI2CSubAddr = 0x81,		// byte transaction (0x80) flag, register 1
	kMask = 0xC,			// bits 2 & 3
	kValue = 0xC
};

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
static bool gI2CTransactionComplete;
static IOService *gI2CDriver;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOCPUInterruptController *gCPUIC;

bool MacRISC4CPU::start(IOService *provider)
{
    kern_return_t        result;
    IORegistryEntry      *cpusRegEntry, *mpicRegEntry;
    OSIterator           *cpusIterator;
    OSData               *tmpData;
    IOService            *service;
    const OSSymbol       *interruptControllerName, *mpicICSymbol;
    OSData               *interruptData, *parentICData;
    OSArray              *tmpArray;
    OSDictionary         *matchDict;
    char                 mpicICName[48];
    UInt32               maxCPUs, physCPU, mpicPHandle;
    IOService            *i2cresources;
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
    i2c_readI2CBus = OSSymbol::withCString("readI2CBus");
    i2c_writeI2CBus = OSSymbol::withCString("writeI2CBus");
	u3APIPhyDisableProcessor1 = OSSymbol::withCString("u3APIPhyDisableProcessor1");
    
    macRISC4PE = OSDynamicCast(MacRISC4PE, getPlatform());
    if (macRISC4PE == 0) return false;
  
    if (!super::start(provider)) return false;

    // Count the CPUs.
    numCPUs = 0;
    cpusRegEntry = fromPath("/cpus", gIODTPlane);
    if (cpusRegEntry == 0) return false;
    cpusIterator = cpusRegEntry->getChildIterator(gIODTPlane);
    while (cpusIterator->getNextObject()) numCPUs++;
    cpusIterator->release();
  
    // Limit the number of CPUs by the cpu=# boot arg.
    if (PE_parse_boot_arg("cpus", &maxCPUs))
    {
        if (numCPUs > maxCPUs) numCPUs = maxCPUs;
    }
	
	doSleep = false;
  
    // Get the "flush-on-lock" property from the first cpu node.  "flush-on-lock" mean do no "nap" the processor.
    flushOnLock = false;
    cpusRegEntry = fromPath("/cpus/@0", gIODTPlane);
    if (cpusRegEntry == 0) return false;
    if (cpusRegEntry->getProperty("flush-on-lock") != 0) flushOnLock = true;
  
    // Set flushOnLock when numCPUs is not one.
    //if (numCPUs != 1) flushOnLock = true;
  
    // Get the physical CPU number from the "reg" property.
    tmpData = OSDynamicCast(OSData, provider->getProperty("reg"));
    if (tmpData == 0) return false;
    physCPU = *(long *)tmpData->getBytesNoCopy();
    setCPUNumber(physCPU);

    // Get the gpio offset for soft reset from the "soft-reset" property.
    tmpData = OSDynamicCast(OSData, provider->getProperty("soft-reset"));
    if (tmpData == 0) 
    {
		IOLog ("MacRISC4CPU::start - cpu(%ld) - no soft-reset property\n", physCPU);
		return false;
    } else
        soft_reset_offset = *(long *)tmpData->getBytesNoCopy();
   
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
        l2crValue = *(long *)tmpData->getBytesNoCopy() & 0x7FFFFFFF;
    else
        l2crValue = mfl2cr() & 0x7FFFFFFF;
  
	kprintf ("MacRISC4CPU::start - waiting for KeyLargo\n");
    // Wait for KeyLargo to show up.
    keyLargo = waitForService(serviceMatching("KeyLargo"));
    if (keyLargo == 0) return false;
    
    // Find the interrupt controller specified by the provider.
    parentICData = OSDynamicCast(OSData, provider->getProperty(kMacRISC4ParentICKey));
    mpicPHandle = *(UInt32 *)parentICData->getBytesNoCopy();
    sprintf(mpicICName, "IOInterruptController%08lX", mpicPHandle);
    mpicICSymbol = OSSymbol::withCString(mpicICName);
    matchDict = serviceMatching("AppleMPICInterruptController");
    matchDict->setObject("InterruptControllerName", mpicICSymbol);
    mpic = waitForService(matchDict);
    
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

#if 0		// XXX - incomplete
	if (macRISC4PE->ioPPluginNub) {
		// Find the platform monitor, if present
		service = waitForService(resourceMatching("IOPlatformPlugin"));
		ioPPlugin = OSDynamicCast (IOService, service->getProperty("IOPlatformPlugin"));
		if (!ioPPlugin) 
			return false;
		
		ioPPluginDict = OSDictionary::withCapacity(2);
		if (!ioPPluginDict) {
			ioPPlugin = NULL;
		} else {
			ioPPluginDict->setObject (kIOPPluginTypeKey, OSSymbol::withCString (kIOPPluginTypeCPUCon));
			ioPPluginDict->setObject (kIOPPluginCPUIDKey, OSNumber::withNumber ((long long)getCPUNumber(), 32));

			if (messageClient (kIOPPluginMessageRegister, ioPPlugin, (void *)ioPPluginDict) != kIOReturnSuccess) {
				IOLog ("MacRISC4CPU::start - failed to register cpu with IOPlatformPlugin\n");
				ioPPluginDict->release();
				ioPPlugin = NULL;
				return false;
			}
		}
	}
#endif

    registerService();

	if (!(uniN = waitForService(serviceMatching("AppleU3")))) return false;
	/*
	 * If numCPUs is one, disable *any* second processor that might be present because it could
	 * be running and stealing cycles on the bus [3249029].  This also fixes a hang when 
	 * boot-args cpus=1 is set [3273619]  
	 */
	if (numCPUs == 1) 
		uniN->callPlatformFunction (u3APIPhyDisableProcessor1, false, (void *)0, (void *)0, (void *)0, (void *)0);
	
    if (physCPU < numCPUs)
    {
		if (numCPUs > 1 && !gI2CDriver) {	// MP only
			// We need to wait for our I2C resources to load before registering the CPU
			// Otherwise we won't be prepared to enableCPUTimeBase
			i2cresources = waitForService (resourceMatching ("PPCI2CInterface.i2c-uni-n"));
			if (i2cresources) {
				gI2CDriver = OSDynamicCast (IOService, i2cresources->getProperty ("PPCI2CInterface.i2c-uni-n"));
				if (!gI2CDriver) {
					kprintf ("MacRISC4CPU::start(%ld) - failed i2cDriver\n", getCPUNumber());
					return false;
				}
			} 
		}
		
        processor_info.cpu_id           = (cpu_id_t)this;
        processor_info.boot_cpu         = bootCPU;
        processor_info.start_paddr      = 0x0100;
        processor_info.l2cr_value       = l2crValue;
        processor_info.supports_nap     = !flushOnLock;
        processor_info.time_base_enable =
        (time_base_enable_t)&MacRISC4CPU::enableCPUTimeBase;

		// Register this CPU with mach.
		result = ml_processor_register(&processor_info, &machProcessor,	&ipi_handler);
		if (result == KERN_FAILURE) return false;
	
		processor_start(machProcessor);
	}

    // Finds PMU so in quiesce we can put the machine to sleep.
    // I can not put these calls there because quiesce runs in interrupt
    // context and waitForService may block.
    if (!(pmu = waitForService(serviceMatching("ApplePMU")))) return false;
	
	// Some systems require special handling of Ultra-ATA at sleep.
	// Call UniN to prepare for that, if necessary
	uniN->callPlatformFunction ("setupUATAforSleep", false, (void *)0, (void *)0, (void *)0, (void *)0);

	kprintf ("MacRISC4CPU::start - done\n");
    return true;
}

void MacRISC4CPU::performPMUSpeedChange (UInt32 newLevel)
{
    bool				tempRememberNap;

	// Note the current processor speed so quiesceCPU knows what to do
	currentProcessorSpeed = newLevel;
	
	// Disable nap to prevent PMU doing reset too soon.
	tempRememberNap = ml_enable_nap(getCPUNumber(), false);
	
	kprintf ("performPMUSpeedChange - initiating speed change to level %ld, nap %d\n", newLevel, tempRememberNap);
	IOLog ("performPMUSpeedChange - initiating speed change to level %ld, nap %d\n", newLevel, tempRememberNap);
	
	// Set flags for processor speed change.
	processorSpeedChange = true;
	
	// Ask PM to do the processor speed change.
	pmRootDomain->receivePowerNotification(kIOPMProcessorSpeedChange);
	
	// Set flags for system sleep.
	processorSpeedChange = false;
	
	kprintf ("performPMUSpeedChange - completing speed change to level %ld, nap %d\n", newLevel, tempRememberNap);
	IOLog ("performPMUSpeedChange - completing speed change to level %ld, nap %d\n", newLevel, tempRememberNap);
	// Enable nap as needed.
	ml_enable_nap(getCPUNumber(), tempRememberNap);
	
	return;
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
	
			// Disables the interrupts for this CPU.
			if (macRISC4PE->getMachineType() == kMacRISC4TypePowerMac) {
				kprintf("MacRISC4CPU::initCPU %d -> mpic->setUpForSleep on", getCPUNumber());
				mpic->callPlatformFunction(mpic_setUpForSleep, false, (void *)false, (void *)getCPUNumber(), 0, 0);
			}
		}
    }

    kprintf("MacRISC4CPU::initCPU %d Here!\n", getCPUNumber());
 
    // Set time base.
    if (bootCPU)
        keyLargo->callPlatformFunction(keyLargo_syncTimeBase, false, 0, 0, 0, 0);
  
    if (boot) {
        gCPUIC->enableCPUInterrupt(this);
    
        // Register and enable IPIs.
        cpuNub->registerInterrupt(0, this, (IOInterruptAction)&MacRISC4CPU::ipiHandler, 0);
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
        // Set the sleeping state for HWInit.
		// Tell U3 to enter normal mode.
		// For U3, this has to be done before telling the PMU to start going to sleep
		uniN->callPlatformFunction (UniNSetPowerState, false, (void *)(kUniNSleep),
			(void *)0, (void *)0, (void *)0);

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
			pmu->callPlatformFunction("sleepNow", false, 0, 0, 0, 0);
	
			// Enables the interrupts for this CPU.
			if (macRISC4PE->getMachineType() == kMacRISC4TypePowerMac) 
				mpic->callPlatformFunction(mpic_setUpForSleep, false, (void *)true, (void *)getCPUNumber(), 0, 0);
	
			// Save KeyLargo's register state.
			keyLargo->callPlatformFunction(keyLargo_saveRegisterState, false, 0, 0, 0, 0);
	
			// Turn Off all KeyLargo I/O.
			keyLargo->callPlatformFunction(keyLargo_turnOffIO, false, (void *)false, 0, 0, 0);
        }
        
        // Set the wake vector to point to the reset vector
        ml_phys_write(0x0080, 0x100);
	}

    ml_ppc_sleep();
	
	return;
}

/* startCPU - not called at interrupt context */
kern_return_t MacRISC4CPU::startCPU(vm_offset_t /*start_paddr*/, vm_offset_t /*arg_paddr*/)
{
    long            gpioOffset = soft_reset_offset;
    unsigned long   strobe_value;

	/*
	 * On MP machines, once we enable the second CPU the kernel will want to sync the timebases
	 * between the CPUs and will call enableCPUTimeBase to disable then enable the timebase around
	 * the sync operation.  enableCPUTimeBase is called on an interrupt context and so it cannot block
	 * To ensure exclusive access to the I2C bus by enableCPUTimeBase, we open the I2C bus here, where
	 * it is safe to block, then wait for the I2C transaction to complete after which we can close out
	 * the bus.
	 */
	gI2CTransactionComplete = false;
	if (gI2CDriver && (kIOReturnSuccess == gI2CDriver->callPlatformFunction (i2c_openI2CBus, false, (void *) kI2CPort, (void *) 0, (void *) 0, (void *) 0))) {
		kprintf ("MacRISC4CPU::startCPU(%ld) i2c bus opened\n", getCPUNumber());
		// pre-set for combined mode
		gI2CDriver->callPlatformFunction (i2c_setCombinedMode, false, (void *) 0, (void *) 0, (void *) 0, (void *) 0);	//CY28510 does reads in combined mode
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
	while (!gI2CTransactionComplete) IOSleep (10);			// Spin until done

	gI2CDriver->callPlatformFunction (i2c_closeI2CBus, false, (void *) 0, (void *) 0, (void *) 0, (void *) 0);	//CY28510 does reads in combined mode

	kprintf ("MacRISC4CPU::startCPU(%ld) i2c bus closed\n", getCPUNumber());
	
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
										kprintf ("MacRISC4CPU::haltCPU - warning, more than %ld PCI bridges - cannot save/restore them all\n");
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

/* enableCPUTimeBase - IS called at interrupt context */
void MacRISC4CPU::enableCPUTimeBase(bool enable)
{
	UInt8 sevenBitAddr, buf, tmp;
	
	// read the byte register -- requires 7 bit slave address
	sevenBitAddr = kI2CAddr >> 1;
	if (kIOReturnSuccess == gI2CDriver->callPlatformFunction (i2c_readI2CBus, false, (void *)(UInt32)sevenBitAddr, (void *)kI2CSubAddr, (void *)(UInt32)&buf, (void *)1)) {
		// apply mask and value
		tmp = enable ? kValue : ~kValue; 
		buf = (buf & ~kMask) | (tmp & kMask);
		gI2CDriver->callPlatformFunction (i2c_writeI2CBus, false, (void *)(UInt32)sevenBitAddr,(void *) kI2CSubAddr, (void *)(UInt32)& buf, (void *)1);
	} else {
		kprintf ("MacRISC4CPU::enableCPUTimeBase - I2C read failed\n");
		return;
	}
	kprintf ("MacRISC4CPU::enableCPUTimeBase(%s) - I2C transaction complete\n", enable ? "enable" : "disable");
	/*
	 * We generally get called twice, the first time to disable the timebase and the second
	 * time to enable it.  We're only done after the second call, i.e., enable is true.
	 * Setting gI2CTransactionComplete to true signals startCPU that it is OK to close
	 * the I2C bus.
	 */
	gI2CTransactionComplete = enable;

	return;
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
