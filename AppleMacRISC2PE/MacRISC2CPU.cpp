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

extern "C" {
#include <ppc/proc_reg.h>
}

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOCPU.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#include "MacRISC2CPU.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOCPU

OSDefineMetaClassAndStructors(MacRISC2CPU, IOCPU);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOCPUInterruptController *gCPUIC;

bool MacRISC2CPU::start(IOService *provider)
{
    kern_return_t        result;
    IORegistryEntry      *cpusRegEntry, *uniNRegEntry, *mpicRegEntry;
    OSIterator           *cpusIterator;
    OSData               *tmpData;
    IOService            *service;
    const OSSymbol       *interruptControllerName;
    OSData               *interruptData;
    OSArray              *tmpArray;
    UInt32               maxCPUs, uniNVersion, physCPU;
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
  
    // Get the "flush-on-lock" property from the fisrt cpu node.
    flushOnLock = false;
    cpusRegEntry = fromPath("/cpus/@0", gIODTPlane);
    if (cpusRegEntry == 0) return false;
    if (cpusRegEntry->getProperty("flush-on-lock") != 0) flushOnLock = true;
  
    // Set flushOnLock when numCPUs is not one.
    if (numCPUs != 1) flushOnLock = true;
  
    // Get the physical CPU number from the "reg" property.
    tmpData = OSDynamicCast(OSData, provider->getProperty("reg"));
    if (tmpData == 0) return false;
    physCPU = *(long *)tmpData->getBytesNoCopy();
    setCPUNumber(physCPU);
  
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
    IOPMrootDomain *pmRootDomain = OSDynamicCast(IOPMrootDomain, service);
    if (pmRootDomain != 0)
    {
        kprintf("Register MacRISC2CPU %d to acknowledge power changes\n", getCPUNumber());
        pmRootDomain->registerInterestedDriver(this);
    }
  
    registerService();
  
    return true;
}

// This is called before to start the sleep process and after waking up before to
// start the wake process. We wish to disble the CPU nap mode going down and
// re-enable it before to go up:
IOReturn MacRISC2CPU::powerStateWillChangeTo ( IOPMPowerFlags theFlags, unsigned long, IOService*)
{
    if ( ! (theFlags & IOPMPowerOn) ) {
        // Sleep sequence:
        kprintf("MacRISC2CPU %d powerStateWillChangeTo to acknowledge power changes (DOWN) we set napping %d\n", getCPUNumber(), false);
        rememberNap = ml_enable_nap(getCPUNumber(), false);        // Disable napping (the function returns the previous state)
    }
    else
    {
        // Wake sequence:
        kprintf("MacRISC2CPU %d powerStateWillChangeTo to acknowledge power changes (UP) we set napping %d\n", getCPUNumber(), rememberNap);
        ml_enable_nap(getCPUNumber(), rememberNap); 		   // Re-set the nap as it was before.
    }
    return IOPMAckImplied;
}

void MacRISC2CPU::initCPU(bool boot)
{
    if (!boot && bootCPU)
    {
        // Tell Uni-N to enter normal mode.
        macRISC2PE->writeUniNReg(kUniNPowerMngmnt, kUniNNormal);
    
        // Set the running state for HWInit.
        macRISC2PE->writeUniNReg(kUniNHWInitState, kUniNHWInitStateRunning);
    
        // Restore the PCI-PCI Bridge.
        if (decBridge) decBridge->restoreBridgeState();
    
        keyLargo->callPlatformFunction(keyLargo_restoreRegisterState, false, 0, 0, 0, 0);

        // Disables the interrupts for this CPU.
        if (macRISC2PE->getMachineType() == kMacRISC2TypePowerMac)
        {
            kprintf("MacRISC2CPU::initCPU %d -> mpic->setUpForSleep on", getCPUNumber());
            mpic->callPlatformFunction(mpic_setUpForSleep, false, (void *)false, (void *)getCPUNumber(), 0, 0);
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
        // Send PMU command to shutdown system before io is turned off
        if (pmu != 0)
            pmu->callPlatformFunction("sleepNow", false, 0, 0, 0, 0);
        else
            kprintf("MacRISC2CPU::quiesceCPU can't find ApplePMU\n");

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

        // Set the wake vector to point to the reset vector
        ml_phys_write(0x0080, 0x100);

        // Set the sleeping state for HWInit.
        macRISC2PE->writeUniNReg(kUniNHWInitState, kUniNHWInitStateSleeping);

        // Tell Uni-N to enter sleep mode.
        macRISC2PE->writeUniNReg(kUniNPowerMngmnt, kUniNSleep);
    }

    ml_ppc_sleep();
}

kern_return_t MacRISC2CPU::startCPU(vm_offset_t /*start_paddr*/, vm_offset_t /*arg_paddr*/)
{
    long gpioOffset;
  
    switch (getCPUNumber())
    {
        case 0 : gpioOffset = 0x5B; break;
        case 1 : gpioOffset = 0x5C; break;
        case 2 : gpioOffset = 0x67; break;
        case 3 : gpioOffset = 0x68; break;
        default : return KERN_FAILURE;
    }
  
    // Strobe the reset line for this CPU.
    keyLargo->callPlatformFunction(keyLargo_writeRegUInt8, false, (void *)&gpioOffset, (void *)4, 0, 0);
    keyLargo->callPlatformFunction(keyLargo_writeRegUInt8, false, (void *)&gpioOffset, (void *)5, 0, 0);
  
    return KERN_SUCCESS;
}

void MacRISC2CPU::haltCPU(void)
{
    IORegistryEntry *decBridgeEntry;
    IOService       *decBridgeNub;
  
    setCPUState(kIOCPUStateStopped);
  
    if (bootCPU)
    {
        // Find the DEC Bridge if it is there.
        decBridge = 0;
        decBridgeEntry = fromPath("/pci@f2000000/@d", gIODTPlane);
        decBridgeNub = OSDynamicCast(IOService, decBridgeEntry);
        if (decBridgeNub != 0)
        {
            decBridge = OSDynamicCast(IOPCI2PCIBridge, decBridgeNub->getClient());
        }
    }

   // Finds the PMU so in quience we can put the machine to sleep.
   // I can not put this call there because quience runs in interrupt
   // context and waitForService may block.
   pmu = waitForService(serviceMatching("ApplePMU"));
  
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
    long gpioOffset;
    UInt8 value;
  
    gpioOffset = 0x73;
    value = enable ? 5 : 4;

    keyLargo->callPlatformFunction(keyLargo_writeRegUInt8, false, (void *)&gpioOffset, (void *)value, 0, 0);
}

void MacRISC2CPU::ipiHandler(void *refCon, void *nub, int source)
{
    // Call mach IPI handler for this CPU.
    if (ipi_handler) ipi_handler();
}

const OSSymbol *MacRISC2CPU::getCPUName(void)
{
    char tmpStr[256];
  
    sprintf(tmpStr, "Primary%d", getCPUNumber());
  
    return OSSymbol::withCString(tmpStr);
}
