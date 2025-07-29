/*
 * Copyright (c) 1998-2021 Apple Computer, Inc. All rights reserved.
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

#define __IOPCIDEVICE_INTERNAL__	1

#include <IOKit/system.h>

#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <PCIDriverKit/PCIDriverKitPrivate.h>
#include <IOKit/pci/IOAGPDevice.h>
#include <IOKit/pci/IOPCIConfigurator.h>
#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOUserServer.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOCommandGate.h>
#include <libkern/OSKextLib.h>

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include <libkern/c++/OSContainers.h>
#include <libkern/version.h>

#if ACPI_SUPPORT
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#endif

#define TARGET_OS_HAS_THUNDERBOLT __has_include(<IOKit/thunderbolt/IOThunderboltPort.h>)
#if TARGET_OS_HAS_THUNDERBOLT
#include <IOKit/thunderbolt/IOThunderboltPort.h>
#endif

#ifndef VERSION_MAJOR
#error VERSION_MAJOR
#endif

//#define PROT_DEVICE     "XGBE"

enum
{
    // reserved->pmSleepEnabled
    kPMEnable            = 0x01,
    kPMEOption           = 0x02,
    kPMEOptionS3Disable  = 0x04,
    kPMEOptionWakeReason = 0x08,
};

#define DLOG(fmt, args...)                   \
    do {                                                    \
        if ((gIOPCIFlags & kIOPCIConfiguratorIOLog) && !ml_at_interrupt_context())   \
            IOLog(fmt, ## args);                            \
        if (gIOPCIFlags & kIOPCIConfiguratorKPrintf)        \
            kprintf(fmt, ## args);                          \
    } while(0)

#ifdef PROT_DEVICE
extern "C"
kern_return_t
mach_vm_protect(
	vm_map_t		map,
	mach_vm_offset_t	start,
	mach_vm_size_t	size,
	boolean_t		set_maximum,
	vm_prot_t		new_protection);
#endif /* PROT_DEVICE */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClassAndStructors(IOPCIDevice, IOService)
OSMetaClassDefineReservedUnused(IOPCIDevice,  6);
OSMetaClassDefineReservedUnused(IOPCIDevice,  7);
OSMetaClassDefineReservedUnused(IOPCIDevice,  8);
OSMetaClassDefineReservedUnused(IOPCIDevice,  9);
OSMetaClassDefineReservedUnused(IOPCIDevice, 10);
OSMetaClassDefineReservedUnused(IOPCIDevice, 11);
OSMetaClassDefineReservedUnused(IOPCIDevice, 12);
OSMetaClassDefineReservedUnused(IOPCIDevice, 13);
OSMetaClassDefineReservedUnused(IOPCIDevice, 14);
OSMetaClassDefineReservedUnused(IOPCIDevice, 15);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//*********************************************************************************
// [public] maxCapabilityForDomainState
//
// Finds the highest power state in the array whose input power
// requirement is equal to the input parameter.  Where a more intelligent
// decision is possible, override this in the subclassed driver.
//*********************************************************************************

unsigned long 
IOPCIDevice::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
{
	if (domainState & kIOPMPowerOn)        return (kIOPCIDeviceOnState);
	if (domainState & kIOPMSoftSleep)      return (kIOPCIDeviceDozeState);
	if (domainState & kIOPMConfigRetained) return (kIOPCIDevicePausedState);
    return (kIOPCIDeviceOffState);
}

//*********************************************************************************
// [public] initialPowerStateForDomainState
//
// Finds the highest power state in the array whose input power
// requirement is equal to the input parameter.  Where a more intelligent
// decision is possible, override this in the subclassed driver.
//*********************************************************************************

unsigned long 
IOPCIDevice::initialPowerStateForDomainState ( IOPMPowerFlags domainState )
{
	if (domainState & kIOPMRootDomainState) return (kIOPCIDeviceOffState);
	if (domainState & kIOPMPowerOn)         return (kIOPCIDeviceOnState);
	if (domainState & kIOPMSoftSleep)       return (kIOPCIDeviceDozeState);
	if (domainState & kIOPMConfigRetained)  return (kIOPCIDevicePausedState);
    return (kIOPCIDeviceOffState);
}

//*********************************************************************************
// [public] powerStateForDomainState
//
// Finds the highest power state in the array whose input power
// requirement is equal to the input parameter.  Where a more intelligent
// decision is possible, override this in the subclassed driver.
//*********************************************************************************

unsigned long 
IOPCIDevice::powerStateForDomainState ( IOPMPowerFlags domainState )
{
	if (domainState & kIOPMPowerOn)        return (kIOPCIDeviceOnState);
	if (domainState & kIOPMSoftSleep)      return (kIOPCIDeviceDozeState);
	if (domainState & kIOPMConfigRetained) return (kIOPCIDevicePausedState);
    return (kIOPCIDeviceOffState);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService *
IOPCIDeviceDMAOriginator(IOPCIDevice * device)
{
    IOService       * dmaOriginator = device;
    IORegistryEntry * parent        = 0;
    IORegistryEntry * child         = 0;
    IOPCIDevice     * funcZero;
    OSObject        * prop          = 0;
    OSData          * data;
    uint32_t          value         = 0;

    if (device->space.s.functionNum) do
    {
        parent = device->copyParentEntry(gIODTPlane);
        if (!parent)   break;
        child = parent->copyChildEntry(gIODTPlane);
        if (!child)    break;
        funcZero = OSDynamicCast(IOPCIDevice, child);
        if (!funcZero) break;
        prop = funcZero->copyProperty(kIOPCIFunctionsDependentKey);
        if ((data = OSDynamicCast(OSData, prop)))
        {
            value = ((uint32_t *) data->getBytesNoCopy())[0];
        }
        if (value) dmaOriginator = funcZero;
    }
    while (false);

    OSSafeReleaseNULL(prop);
    OSSafeReleaseNULL(child);
    OSSafeReleaseNULL(parent);

    return (dmaOriginator);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// attach
//
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIDevice::attach( IOService * provider )
{
    if (!super::attach(provider)) return (false);

#if ACPI_SUPPORT
	IOACPIPlatformDevice * device;
	uint32_t               idx;
	bool                   hasPS;

	device = 0;
	if (reserved->configEntry 
	 && (device = (IOACPIPlatformDevice *) reserved->configEntry->acpiDevice) 
	 && !device->metaCast("IOACPIPlatformDevice")) device = 0;

	for (idx = kIOPCIDeviceOffState, hasPS = false; idx <= kIOPCIDeviceOnState; idx++)
	{
		reserved->psMethods[idx] = -1;
		if (!device) continue;
		if (kIOReturnSuccess == device->validateObject(gIOPCIPSMethods[idx]))
		{
			reserved->psMethods[idx] = idx;
			hasPS = true;
		}
		else if (kIOPCIDeviceDozeState == idx)
		{
			reserved->psMethods[idx] = reserved->psMethods[kIOPCIDeviceOffState];
		}
	}
	reserved->psMethods[kIOPCIDevicePausedState] = reserved->psMethods[kIOPCIDeviceOnState];
	if (hasPS) DLOG("%s: _PSx %d, %d, %d, %d\n", getName(),
				  reserved->psMethods[0], reserved->psMethods[1], 
				  reserved->psMethods[2], reserved->psMethods[3]);
	reserved->lastPSMethod = reserved->psMethods[kIOPCIDeviceOnState];
#endif
	reserved->pciPMState = kIOPCIDeviceOnState;

	if (reserved->powerCapability)
	{
		uint16_t pmcsr;
		reserved->pmControlStatus = reserved->powerCapability + 4;
		pmcsr = extendedConfigRead16(reserved->pmControlStatus);
		if (pmcsr & kPCIPMCSPMEStatus)
		{
			// R/WC PME_Status
			extendedConfigWrite16(reserved->pmControlStatus, pmcsr);
		}
	}

	// kIOPCIConfiguratorWakeToOff is deprecated in rdar://problem/64949845
	IORegistryEntry* parent = getParentEntry(gIOServicePlane);
	if ((parent && (parent->getProperty(kIOPMResetPowerStateOnWakeKey) == kOSBooleanTrue)) ||
		(kIOPCIConfiguratorWakeToOff & gIOPCIFlags))
	{
		setProperty(kIOPMResetPowerStateOnWakeKey, kOSBooleanTrue);
	}

	// Apply IOPMResetPowerStateOnWake on self and on all parent IOPCIDevice
	// and IOPCIBridge objects.
	if (getProperty("manual-enable-s2r"))
	{
		setProperty(kIOPMResetPowerStateOnWakeKey, kOSBooleanTrue);

		parent = getParentEntry(gIOServicePlane);
		while (parent != NULL)
		{
			if (!OSDynamicCast(IOPCIDevice, parent) && !OSDynamicCast(IOPCIBridge, parent))
			{
				break;
			}

			parent = parent->getParentEntry(gIOServicePlane);
		}
	}

	IORegistryEntry* dtParent = getParentEntry(gIODTPlane);
	if (dtParent && (dtParent->getProperty("manual-enable-s2r-ep") != NULL))
	{
		setProperty(kIOPMResetPowerStateOnWakeKey, kOSBooleanTrue);
	}

	// initialize superclass variables
	PMinit();

	if (dtParent && (dtParent->getProperty("manual-enable-s2r") != nullptr || dtParent->getProperty("manual-enable-s2r-ep") != nullptr))
	{
		// rdar://95285826: Set wifi/bt's desired power before issuing a temporaryPowerClampOn()
		if (reserved->configEntry && (reserved->configEntry->vendorProduct & 0xFFFF) == 0x14e4) {
			changePowerStateToPriv(kIOPCIDeviceOnState);
			if(dtParent->getProperty("manual-enable-s2r-ep") != nullptr)
			{
				IOPCIDevice *parentDevice = OSDynamicCast(IOPCIDevice, dtParent);
				parentDevice->changePowerStateToPriv(kIOPCIDeviceOnState);
			}
		}
	}

	// clamp power on
	temporaryPowerClampOn();

    // register as controlling driver
    IOPCIRegisterPowerDriver(this, false);

    // join the tree
	reserved->pmState  = kIOPCIDeviceOnState;
	reserved->pmActive = true;

    IOService * originator = IOPCIDeviceDMAOriginator(this);
    IOService * powerProvider = (originator == this) ? provider : originator;
    powerProvider->joinPMtree( this);

	return (true);
}

void IOPCIDevice::detach( IOService * provider )
{
	IOPCIConfigShadow * shadow;

	if ((shadow = configShadow(this)) && shadow->tunnelRoot)
	{
		setTunnelL1Enable(this, true);
	}

    PMstop();

	IORecursiveLockLock(reserved->lock);
	while (false && reserved->pmActive)
	{
		reserved->pmWait = true;
		IORecursiveLockSleep(reserved->lock, &reserved->pmActive, THREAD_UNINT);
	}
	IORecursiveLockUnlock(reserved->lock);


	if (reserved->pauseTimer)
	{
		IOWorkLoop *tempWL = NULL;
		reserved->pauseTimer->cancelTimeout();
		if ((tempWL = reserved->pauseTimer->getWorkLoop()))
		{
			tempWL->removeEventSource(reserved->pauseTimer);
		}
	}

    if (parent) parent->removeDevice(this);

    super::detach(provider);

    detachAbove(gIODTPlane);
}

bool IOPCIDevice::shouldSkipReset(void)
{
    return false;
}

void IOPCIDevice::detachFromChild(IORegistryEntry *child, const IORegistryPlane *plane)
{
	IOService *childService = OSDynamicCast(IOService, child);

	// If the IOPCIDevice is not terminating (e.g. on an unplug), its driver
	// did not crash (the client crash handler will reset the function/device),
	// and this is not a detach as part of matching, mark this nub as needing
	// reset.
	if (   !isInactive()
		&& !reserved->clientCrashed
		&& !shouldSkipReset()
		&& (childService && childService->isInactive()))
	{
		// Set a flag to reset the function/device next time drivers match on this nub. Use
		// function level reset if supported, else fall back to hot reset.
		DLOG("[%s()] Marking %s as needing hardware reset\n", __func__, getName());
		reserved->hardwareResetNeeded = true;
	}

	super::detachFromChild(child, plane);
}

void
IOPCIDevice::detachAbove( const IORegistryPlane * plane )
{
	super::detachAbove(plane);

    if (plane == gIOPowerPlane)
	{
		IORecursiveLockLock(reserved->lock);
		reserved->pmActive = false;
		if (reserved->pmWait)
			IORecursiveLockWakeup(reserved->lock, &reserved->pmActive, true);
		IORecursiveLockUnlock(reserved->lock);
	}
}

bool 
IOPCIDevice::initReserved(void)
{
    // allocate our expansion data
    if (!reserved)
    {
        reserved = IOMallocType(IOPCIDeviceExpansionData);
        if (!reserved)
            return (false);
		reserved->lock = IORecursiveLockAlloc();
		reserved->expressMaxReadRequestSize = -1;
    }
    return (true);
}

bool 
IOPCIDevice::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return (false);
    return (initReserved());
}

bool IOPCIDevice::init( IORegistryEntry * from, const IORegistryPlane * inPlane )
{
    if (!super::init(from, inPlane))  return (false);
    return (initReserved());
}

void IOPCIDevice::free()
{
    if (savedConfig)
    {
		IOPCIConfigShadow *shadow = configShadow(this);
		if (shadow->link.next) panic("IOPCIDevice(%p) linked", this);
		if (shadow->linkFinish.next) panic("IOPCIDevice(%p) linked (linkFinish)", this);
        IOFreeType(shadow, IOPCIConfigShadow);
        savedConfig = 0;
    }
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (reserved)
	{
		if (reserved->lock)
			IORecursiveLockFree(reserved->lock);
		OSSafeReleaseNULL(reserved->pauseTimer);
		if (reserved->busyNotifier)
		{
			reserved->busyNotifier->remove();
			reserved->busyNotifier = NULL;
		}
        IOFreeType(reserved, IOPCIDeviceExpansionData);
	}

    super::free();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIDevice::powerStateWillChangeTo (IOPMPowerFlags  capabilities, 
                                              unsigned long   stateNumber, 
                                              IOService*      whatDevice)
{
#if ACPI_SUPPORT
	return powerStateWillChangeToGated(&capabilities, &stateNumber, whatDevice);
#else
    return parent->getConfiguratorWorkLoop()->runAction(
                OSMemberFunctionCast(IOCommandGate::Action, this, &IOPCIDevice::powerStateWillChangeToGated),
                this, &capabilities, &stateNumber, whatDevice);
#endif
}

IOReturn IOPCIDevice::powerStateWillChangeToGated (IOPMPowerFlags  *_capabilities,
                                              unsigned long   *_stateNumber,
                                              IOService*      whatDevice)
{
	IOPMPowerFlags capabilities = *_capabilities;
	unsigned long stateNumber = *_stateNumber;
    IOReturn ret;
    uint16_t pmcsr;

    if (stateNumber == kIOPCIDeviceOffState)
    {
        if ((kPMEOption & reserved->pmSleepEnabled) && reserved->pmControlStatus && (reserved->sleepControlBits & kPCIPMCSPMEStatus))
        {
            // if we would normally reset the PME_Status bit when going to sleep, do it now
            // at the beginning of the power change. that way any PME event generated from this point
            // until we go to sleep should wake the machine back up.
            pmcsr = extendedConfigRead16(reserved->pmControlStatus);
            if (pmcsr & kPCIPMCSPMEStatus)
            {
                // the the PME_Status bit is set at this point, we clear it but leave all other bits
                // untouched by writing the exact same value back to the register. This is because the
                // PME_Status bit is R/WC.
                DLOG("%s[%p]::powerStateWillChangeTo(OFF) - PMCS has PME set(0x%x) - CLEARING\n", getName(), this, pmcsr);
                extendedConfigWrite16(reserved->pmControlStatus, pmcsr);
                DLOG("%s[%p]::powerStateWillChangeTo(OFF) - PMCS now is(0x%x)\n", getName(), this, extendedConfigRead16(reserved->pmControlStatus));
            }
            else
            {
                DLOG("%s[%p]::powerStateWillChangeTo(OFF) - PMCS has PME clear(0x%x) - not touching\n", getName(), this, pmcsr);
            }
        }
        reserved->updateWakeReason = true;
    }
    else if ((stateNumber == kIOPCIDeviceOnState) && reserved->pmSleepEnabled && reserved->pmControlStatus && reserved->sleepControlBits)
    {
        ret = (kIOPCIDeviceOnState == reserved->pciPMState)
         ? kIOReturnNotReady : checkLink(kCheckLinkForPower);
		pmcsr = (kIOReturnSuccess == ret)
		 ?  extendedConfigRead16(reserved->pmControlStatus) : reserved->pmLastWakeBits;
		updateWakeReason(pmcsr);
		DLOG("%s[%p]::powerStateWillChangeTo(ON) - ps %d lnk(0x%x) - PMCS(0x%x, 0x%x)\n", getName(), this, reserved->pciPMState, ret, pmcsr, reserved->pmLastWakeBits);
    }

    return super::powerStateWillChangeTo(capabilities, stateNumber, whatDevice);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// PCI setPowerState
//
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIDevice::setPCIPowerState(uint8_t powerState, uint32_t options)
{
	uint16_t            pmeState;
	uint8_t             prevState;
	uint8_t             effectiveState;

	if ((powerState == kIOPCIDeviceOffState) && (kIOPCIConfiguratorDeepIdle & gIOPCIFlags))
	{
		 effectiveState = kIOPCIDeviceDozeState;
	}
	else effectiveState = powerState;

    DLOG("%s[%p]::setPCIPowerState(%d->%d,%d)\n", getName(), this, reserved->pciPMState, powerState, effectiveState);

#if ACPI_SUPPORT
	IOACPIPlatformDevice * device;
	int8_t  idx;
	IOReturn ret;
	uint64_t time;
	if ((idx = reserved->psMethods[effectiveState]) >= 0)
	{
		if ((idx != reserved->lastPSMethod) && !(kMachineRestoreDehibernate & options))
		{
			IOPCIConfigShadow * shadow = configShadow(this);
			if ((effectiveState >= kIOPCIDeviceOnState)
				&& !space.s.busNum 
				&& (shadow)
				&& (shadow->bridge)
				&& (kIOPCIConfigShadowValid 
					== ((kIOPCIConfigShadowValid | kIOPCIConfigShadowBridgeDriver) & shadow->flags))
				&& (!(0x00FFFFFF & extendedConfigRead32(kPCI2PCIPrimaryBus))))
			{
				DLOG("%s::restore bus(0x%x)\n", getName(), shadow->configSave.savedConfig[kPCI2PCIPrimaryBus >> 2]);
				extendedConfigWrite32(kPCI2PCIPrimaryBus, shadow->configSave.savedConfig[kPCI2PCIPrimaryBus >> 2]);
			}
			device = (IOACPIPlatformDevice *) reserved->configEntry->acpiDevice;
			DLOG("%s::evaluateObject(%s)\n", getName(), gIOPCIPSMethods[idx]->getCStringNoCopy());
			time = mach_absolute_time();
			ret = device->evaluateObject(gIOPCIPSMethods[idx]);
			time = mach_absolute_time() - time;
			absolutetime_to_nanoseconds(time, &time);
			DLOG("%s::evaluateObject(%s) ret 0x%x %qd ms\n", 
				getName(), gIOPCIPSMethods[idx]->getCStringNoCopy(), ret, time / 1000000ULL);

			if ((effectiveState < kIOPCIDeviceOnState) && shadow) shadow->restoreCount = 0;
		}
		reserved->lastPSMethod = idx;
	}

//    void * p3 = (void *) 0;
//    if (doMsg) device->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
//						  (void *) kIOMessageDeviceWillPowerOff, device, p3, (void *) 0);
//    if (doMsg) device->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
//						  (void *) kIOMessageDeviceHasPoweredOn, device, p3, (void *) 0);

#endif

	if (kMachineRestoreDehibernate & options) return (kIOReturnSuccess);
	prevState = reserved->pciPMState;
	if (powerState != prevState)
	{
		atomic_store((atomic_char*)&reserved->pciPMState, powerState);
		if (!isInactive()) switch (powerState)
		{
			case kIOPCIDeviceOffState:
			case kIOPCIDeviceDozeState:

				if (reserved->pmSleepEnabled && reserved->pmControlStatus && reserved->sleepControlBits)
				{
					UInt16 bits = reserved->sleepControlBits;
					if (kPMEOption & reserved->pmSleepEnabled)
					{
						if ((kPMEOptionS3Disable & reserved->pmSleepEnabled) && (kIOPCIDeviceOffState == powerState))
						{
							bits &= ~kPCIPMCSPMEEnable;
							bits |= kPCIPMCSPMEStatus;
							prevState = -1U;
						}
						else
						{
							// we don't clear the PME_Status at this time. Instead, we cleared it in powerStateWillChangeTo
							// so this write will change our power state to the desired state and will also set PME_En
							bits &= ~kPCIPMCSPMEStatus;
						}
					}

					if (effectiveState != prevState)
					{
						DLOG("%s[%p]::setPCIPowerState(OFF) - writing 0x%x to PMCS currently (0x%x)\n", getName(), this, bits, extendedConfigRead16(reserved->pmControlStatus));
						extendedConfigWrite16(reserved->pmControlStatus, bits);
						// PCIe base spec Table 5-13 "PCI Function State Transition Delays"
						IOSleep(10);
						DLOG("%s[%p]::setPCIPowerState(OFF) - did move PMCS to D3\n", getName(), this);
					}
				}
				break;
			
			case kIOPCIDevicePausedState:
			case kIOPCIDeviceOnState:
				pmeState = reserved->pmControlStatus ? extendedConfigRead16(reserved->pmControlStatus) : 0;
				if (reserved->pmSleepEnabled && reserved->pmControlStatus && reserved->sleepControlBits)
				{
					if ((pmeState & kPCIPMCSPowerStateMask) != kPCIPMCSPowerStateD0)
					{
						DLOG("%s[%p]::setPCIPowerState(ON) - moving PMCS from 0x%x to D0\n", 
							getName(), this, extendedConfigRead16(reserved->pmControlStatus));
							// the write below will clear PME_Status, clear PME_En, and set the Power State to D0
						extendedConfigWrite16(reserved->pmControlStatus, kPCIPMCSPMEStatus | kPCIPMCSPowerStateD0);
						// PCIe base spec Table 5-13 "PCI Function State Transition Delays"
						IOSleep(10);
						DLOG("%s[%p]::setPCIPowerState(ON) - did move PMCS to 0x%x\n", 
							getName(), this, extendedConfigRead16(reserved->pmControlStatus));
					}
					else
					{
						DLOG("%s[%p]::setPCIPowerState(ON) - PMCS already at D0 (0x%x)\n", 
							getName(), this, extendedConfigRead16(reserved->pmControlStatus));
							// the write below will clear PME_Status, clear PME_En, and set the Power State to D0
						extendedConfigWrite16(reserved->pmControlStatus, kPCIPMCSPMEStatus);
					}
					reserved->pmLastWakeBits = pmeState;
					reserved->pmeUpdate      = (kIOPCIDeviceOffState == prevState);
				}
				break;
		}
	}

	if (kIOPCIDeviceOnState == powerState)
    {
        if (reserved->pmeUpdate && !(kMachineRestoreDehibernate & options))
        {
            reserved->pmeUpdate = false;
            updateWakeReason(reserved->pmLastWakeBits);
        }
        reserved->updateWakeReason = false;
    }

    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIDevice::addPowerChild(IOService *theChild)
{
#if TARGET_CPU_ARM || TARGET_CPU_ARM64
    IOPCIHostBridgeData *vars = parent->reserved->hostBridgeData;
    vars->addPCIEPowerChild(theChild);
#endif

    return super::addPowerChild(theChild);
}

IOReturn IOPCIDevice::removePowerChild(IOPowerConnection *theChild)
{
#if TARGET_CPU_ARM || TARGET_CPU_ARM64
    IOPCIHostBridgeData *vars = parent->reserved->hostBridgeData;
    vars->removePCIEPowerChild(theChild);
#endif
	return super::removePowerChild(theChild);
}

void IOPCIDevice::updateWakeReason(uint16_t pmeState)
{
    OSNumber * num;

    if (0xFFFF == pmeState) removeProperty(kIOPCIPMCSStateKey);
    else
    {
        if (reserved->updateWakeReason)
        {
            uint32_t mask;
            if (kPMEOptionWakeReason & reserved->pmSleepEnabled) mask = kPCIPMCSPMEStatus;
            else                                                 mask = (kPCIPMCSPMEStatus | kPCIPMCSPMEEnable);

            if (mask == (mask & pmeState))
            {
                parent->updateWakeReason(this);
                reserved->updateWakeReason = false;
            }
        }
        num = OSNumber::withNumber(pmeState, 16);
        if (num)
        {
            setProperty(kIOPCIPMCSStateKey, num);
            num->release();
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// PM setPowerState
//
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIDevice::setPowerState( unsigned long newState,
                                     IOService * whatDevice )
{
#if ACPI_SUPPORT
	return setPowerStateGated(&newState, whatDevice);
#else
    return parent->getConfiguratorWorkLoop()->runAction(
                OSMemberFunctionCast(IOCommandGate::Action, this, &IOPCIDevice::setPowerStateGated),
                this, &newState, whatDevice);
#endif
}

IOReturn IOPCIDevice::setPowerStateGated( unsigned long *_newState,
                                          IOService * whatDevice )
{
	unsigned long newState = *_newState;
	IOReturn ret;
	unsigned long prevState;
	
    if (isInactive())
        return (kIOPMAckImplied);

	if ((getProperty(kIOPMResetPowerStateOnWakeKey) == kOSBooleanTrue) && (kIOPCIDeviceOffState == newState))
	{
		changePowerStateTo(kIOPCIDeviceOffState);
		changePowerStateToPriv(kIOPCIDeviceOffState);
	}

	prevState = reserved->pmState;
    ret = parent->setDevicePowerState(this, kIOPCIConfigShadowVolatile,
    								  prevState, newState);
    reserved->pmState = newState;

#ifdef PROT_DEVICE
    if (!strcmp(PROT_DEVICE, getName()))
    {
        IOMemoryMap * map;
        kern_return_t kr;
        map = mapDeviceMemoryWithIndex(0);
        kr = mach_vm_protect(kernel_map, map->getAddress(), map->getLength(), false,
                                (kIOPCIDeviceOnState == newState) ? VM_PROT_READ | VM_PROT_WRITE : VM_PROT_NONE);
        kprintf("%s mach_vm_protect %d\n", getName(), kr);
        map->release();
    }
#endif /* PROT_DEVICE */

	return (ret);
}	

IOReturn IOPCIDevice::saveDeviceState( IOOptionBits options )
{
	IOReturn ret;

	if (!reserved->interruptVectorsResolved)
	{
		// TODO: Remove the temporary workaround for legacy drivers
		(void)getProperty(gIOInterruptSpecifiersKey);
	}

	ret = parent->setDevicePowerState(this, kIOPCIConfigShadowPermanent & options, 
									  kIOPCIDeviceOnState, kIOPCIDeviceDozeState);

	return (ret);
}

IOReturn IOPCIDevice::restoreDeviceState( IOOptionBits options )
{
	IOReturn ret;
    ret = parent->setDevicePowerState(this, 0, 
    								  kIOPCIDeviceDozeState, kIOPCIDeviceOnState);
	return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIDevice::matchPropertyTable(OSDictionary* table)
{
    OSArray*      entitlementArray      = NULL;
    OSDictionary* entitlementDictionary = NULL;
    OSString*     builtInEntitlement    = NULL;
    bool          builtInEntitled       = false;
    bool          isMatch               = false;
    SInt32        matchScore            = 0;

    isMatch = matchPropertyTable(table, &matchScore);

    // Check if the entitlements match, but only if:
    // 1. The original matching dictionary matches
    // 2. The kIOPCITransportDextEntitlement key exists.
    // Otherwise return the value of 'isMatch' and don't do any entitlement validation.
    if (  (isMatch == true)
       && (table->getObject(kIOPCITransportDextEntitlement)))
    {
        isMatch = false;
        entitlementArray = OSDynamicCast(OSArray, table->getObject(kIOPCITransportDextEntitlement));
        if (entitlementArray != NULL)
        {
            for (unsigned int i = 0; i < entitlementArray->getCount(); i++)
            {
                OSObject* entitlement = entitlementArray->getObject(i);
                entitlementDictionary = OSDynamicCast(OSDictionary, entitlement);
                if (entitlementDictionary != NULL)
                {
                    isMatch = matchPropertyTable(entitlementDictionary, &matchScore);
                    if (isMatch == true)
                    {
                        DLOG("The %s entitlements array has matched at index %d \n", kIOPCITransportDextEntitlement, i);
                        break;
                    }
                }
                else if(   ((builtInEntitlement = OSDynamicCast(OSString, entitlement)) != NULL)
                        && (builtInEntitlement->isEqualTo(kIODriverKitTransportBuiltinEntitlementKey) == true))
                {
                    builtInEntitled = true;
                }
            }

            if(   (getProperty("built-in") != NULL)
               && (builtInEntitled != true))
            {
                isMatch = false;
            }

            if (isMatch == false)
            {
                DLOG("The %s entitlements property doesn't contain a matching entitlement\n", kIOPCITransportDextEntitlement);
            }
        }
        else if (table->getObject(kIOPCITransportDextEntitlement) == kOSBooleanTrue)
        {
            isMatch = true;
            DLOG("The %s entitlement is not a dictionary, this is intended for Apple internal use only\n", kIOPCITransportDextEntitlement);
        }
        else
        {
            DLOG("The %s entitlements property is malformed\n", kIOPCITransportDextEntitlement);
        }
    }

    return isMatch;
}

bool IOPCIDevice::matchPropertyTable( OSDictionary * table, SInt32 * score )
{
    bool result     = false;
    IORegistryEntry * regEntry;
    IOPCIBridge     * bridge;

    regEntry = copyParentEntry(gIOServicePlane);
    bridge   = OSDynamicCast(IOPCIBridge, regEntry);

    if (bridge) result = bridge->matchNubWithPropertyTable(this, table, score);

    OSSafeReleaseNULL(regEntry);

    return (result);
}

bool IOPCIDevice::compareName( OSString * name, OSString ** matched ) const
{
    bool result     = false;
    IORegistryEntry * regEntry;
    IOPCIBridge     * bridge;

    regEntry = copyParentEntry(gIOServicePlane);
    bridge   = OSDynamicCast(IOPCIBridge, regEntry);

    if (bridge) result = bridge->compareNubName(this, name, matched);

    OSSafeReleaseNULL(regEntry);

    return (result);
}

// Reset nub software state that could have been modified by a client driver
void IOPCIDevice::resetNubState(void)
{
	// TODO: Reset interrupt state (reserved->interruptVectorsResolved,
	// IOInterruptControllers, IOInterruptSpecifiers. This requires IOKit support
	// (rdar://118153788). For now, nubs cannot resize their first interrupt
	// configuration.

	// Remove crash reset type override property
	removeProperty(kIOPCIDeviceCrashResetType);

	// Reset the shadow config state
	configShadow(this)->flags &= ~kIOPCIConfigShadowPermanent;
	configShadow(this)->restoreCount = 0;

	reserved->clientCrashed = false;
}

// The grace period is the time (in seconds) after the nub's busy state
// becomes 0 before the nub's IOPM state can be transitioned to Paused.
#define NUB_SETTLED_GRACE_PERIOD_S 2

bool IOPCIDevice::isInitializing(void)
{
	uint64_t deltaNS = 0;

	absolutetime_to_nanoseconds(mach_absolute_time() - reserved->busyTimestamp, &deltaNS);

	return (getBusyState() > 0 || deltaNS < (NUB_SETTLED_GRACE_PERIOD_S * NSEC_PER_SEC));
}

#define PAUSE_TIMER_DURATION_MS 100
void IOPCIDevice::initiatePause(void)
{
	if (isInitializing()) {
		DLOG("%s(0x%qx) still initializing, deferring pause\n", getName(), getRegistryEntryID());
		reserved->pauseTimer->setTimeoutMS(PAUSE_TIMER_DURATION_MS);
	} else {
		DLOG("configOp:->deferredRequestPause: %s(0x%qx)\n", getName(), getRegistryEntryID());
		changePowerStateToPriv(kIOPCIDevicePausedState);
		powerOverrideOnPriv();
	}
}

void IOPCIDevice::pauseTimerHandler(IOTimerEventSource * es)
{
	initiatePause();
}

// Fires when the nub's busy state becomes busy or non-busy
IOReturn IOPCIDevice::busyStateChange(void * target, void * refCon,
									  UInt32 messageType, IOService * service,
									  void * messageArgument, vm_size_t argSize)
{
	IOPCIDevice *self = OSDynamicCast(IOPCIDevice, (IOService*)refCon);

	if (self) {
		DLOG("[%s()] nub %s(0x%qx) has busy state %u\n", __func__, self->getName(), self->getRegistryEntryID(), self->getBusyState());
		self->reserved->busyTimestamp = mach_absolute_time();
	}

	return kIOReturnSuccess;
}

IOReturn IOPCIDevice::getResources( void )
{
    if (getProperty(kIOPCIResourcedKey) && !getChildEntry(gIOServicePlane))
	{
		if (reserved->hardwareResetNeeded)
		{
			tIOPCIDeviceResetTypes type = supportsFLR() ? kIOPCIDeviceResetTypeFunctionReset : kIOPCIDeviceResetTypeHotReset;

			DLOG("[%s()] Performing %s on nub %s\n", __func__, (type == kIOPCIDeviceResetTypeFunctionReset) ? "FLR" : "hot reset", getName());

			IOReturn ret = reset(type);

			reserved->hardwareResetNeeded = false;

			if (ret != kIOReturnSuccess)
			{
				return ret;
			}
		}

		DLOG("[%s()] resetting nub %s's software state\n", __func__, getName());

		// resetNubState() is effectively a no-op on the first match
		resetNubState();
	}

	return parent->getNubResources(this);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Note that the filter is applied to all config writes, internal and external.
// This works for the MRRS setting, but if the filters are extended in the
// future we will likely need to distinguish internal accesses so they can
// bypass the filter.

uint32_t IOPCIDevice::configWrite32Filter(IOByteCount offset, uint32_t data)
{
	// Enforce the EP MRRS override
	if (reserved
		&& reserved->expressCapability != 0
		&& offset == (reserved->expressCapability + 0x8)
		&& reserved->expressMaxReadRequestSize != -1)
	{
		data &= ~(7 << 12);
		data |= reserved->expressMaxReadRequestSize << 12;
	}

	return data;
}

uint16_t IOPCIDevice::configWrite16Filter(IOByteCount offset, uint16_t data)
{
	// Enforce the EP MRRS override
	if (reserved
		&& reserved->expressCapability != 0
		&& offset == (reserved->expressCapability + 0x8)
		&& reserved->expressMaxReadRequestSize != -1)
	{
		data &= ~(7 << 12);
		data |= reserved->expressMaxReadRequestSize << 12;
	}

	return data;
}

uint8_t IOPCIDevice::configWrite8Filter(IOByteCount offset, uint8_t data)
{
	// Enforce the EP MRRS override
	if (reserved
		&& reserved->expressCapability != 0
		&& offset == (reserved->expressCapability + 0x9)
		&& reserved->expressMaxReadRequestSize != -1)
	{
		data &= ~(7 << 4);
		data |= reserved->expressMaxReadRequestSize << 4;
	}

	return data;
}

UInt32 IOPCIDevice::configRead32( IOPCIAddressSpace _space,
                                  UInt8 offset )
{
	if (!parent) return (0xFFFFFFFF);
    return (parent->configRead32(_space, offset));
}

void IOPCIDevice::configWrite32( IOPCIAddressSpace _space,
                                 UInt8 offset, UInt32 data )
{
	if (!parent) return;
	// The offset is relative to space's config space layout,
	// so only apply the filter if the operation is on this
	// IOPCIDevice.
	if (_space.bits == space.bits)
	{
		data = configWrite32Filter(offset, data);
	}
    parent->configWrite32( _space, offset, data );
}

UInt16 IOPCIDevice::configRead16( IOPCIAddressSpace _space,
                                  UInt8 offset )
{
	if (!parent) return (0xFFFF);
    return (parent->configRead16(_space, offset));
}

void IOPCIDevice::configWrite16( IOPCIAddressSpace _space,
                                 UInt8 offset, UInt16 data )
{
	if (!parent) return;
	if (_space.bits == space.bits)
	{
		data = configWrite16Filter(offset, data);
	}
    parent->configWrite16( _space, offset, data );
}

UInt8 IOPCIDevice::configRead8( IOPCIAddressSpace _space,
                                UInt8 offset )
{
	if (!parent) return (0xFF);
    return (parent->configRead8(_space, offset));
}

void IOPCIDevice::configWrite8( IOPCIAddressSpace _space,
                                UInt8 offset, UInt8 data )
{
	if (!parent) return;
	if (_space.bits == space.bits)
	{
		data = configWrite8Filter(offset, data);
	}
    parent->configWrite8( _space, offset, data );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIDevice::configAccess(bool write)
{
	bool ok = (!isInactive()
			&& reserved
			&& parent && !parent->reserved->childrenInReset
			&& (0 == ((write ? VM_PROT_WRITE : VM_PROT_READ) & reserved->configProt)));
	if (!ok && !ml_at_interrupt_context() && (gIOPCIFlags & kIOPCIConfiguratorIOLog))
	{
		OSReportWithBacktrace("config protect fail(2) for device %u:%u:%u\n",
								PCI_ADDRESS_TUPLE(this));
	}
	return (ok);
}

#if APPLE_KEXT_VTABLE_PADDING

UInt32 IOPCIDevice::configRead32( UInt8 offset )
{
	if (!configAccess(false)) return (0xFFFFFFFF);
    return (parent->configRead32(space, offset));
}

void IOPCIDevice::configWrite32( UInt8 offset, UInt32 data )
{
	if (!configAccess(true)) return;
	data = configWrite32Filter(offset, data);
    parent->configWrite32( space, offset, data );
}

UInt16 IOPCIDevice::configRead16( UInt8 offset )
{
	if (!configAccess(false)) return (0xFFFF);
    return (parent->configRead16(space, offset));
}

void IOPCIDevice::configWrite16( UInt8 offset, UInt16 data )
{
	if (!configAccess(true)) return;
	data = configWrite16Filter(offset, data);
    parent->configWrite16( space, offset, data );
}

UInt8 IOPCIDevice::configRead8( UInt8 offset )
{
	if (!configAccess(false)) return (0xFF);
    return (parent->configRead8(space, offset));
}

void IOPCIDevice::configWrite8( UInt8 offset, UInt8 data )
{
	if (!configAccess(true)) return;
	data = configWrite8Filter(offset, data);
    parent->configWrite8( space, offset, data );
}

#endif /* APPLE_KEXT_VTABLE_PADDING */

// --

UInt32 IOPCIDevice::extendedConfigRead32( IOByteCount offset )
{
    // Access must be within 4KB configuration space
	if (!configAccess(false) || offset > (0x1000 - sizeof(uint32_t))) return (0xFFFFFFFF);
    IOPCIAddressSpace _space = space;
    _space.es.registerNumExtended = ((offset >> 8) & 0xF);
    return (configRead32(_space, offset));
}

void IOPCIDevice::extendedConfigWrite32( IOByteCount offset, UInt32 data )
{
    // Access must be within 4KB configuration space
	if (!configAccess(true) || offset > (0x1000 - sizeof(uint32_t))) return;
	data = configWrite32Filter(offset, data);
    IOPCIAddressSpace _space = space;
    _space.es.registerNumExtended = ((offset >> 8) & 0xF);
    configWrite32(_space, offset, data);
}

UInt16 IOPCIDevice::extendedConfigRead16( IOByteCount offset )
{
    // Access must be within 4KB configuration space
	if (!configAccess(false) || offset > (0x1000 - sizeof(uint16_t))) return (0xFFFF);
    IOPCIAddressSpace _space = space;
    _space.es.registerNumExtended = ((offset >> 8) & 0xF);
    return (configRead16(_space, offset));
}

void IOPCIDevice::extendedConfigWrite16( IOByteCount offset, UInt16 data )
{
    // Access must be within 4KB configuration space
	if (!configAccess(true) || offset > (0x1000 - sizeof(uint16_t))) return;
	data = configWrite16Filter(offset, data);
    IOPCIAddressSpace _space = space;
    _space.es.registerNumExtended = ((offset >> 8) & 0xF);
    configWrite16(_space, offset, data);
}

UInt8 IOPCIDevice::extendedConfigRead8( IOByteCount offset )
{
    // Access must be within 4KB configuration space
	if (!configAccess(false) || offset > (0x1000 - sizeof(uint8_t))) return (0xFF);
    IOPCIAddressSpace _space = space;
    _space.es.registerNumExtended = ((offset >> 8) & 0xF);
    return (configRead8(_space, offset));
}

void IOPCIDevice::extendedConfigWrite8( IOByteCount offset, UInt8 data )
{
    // Access must be within 4KB configuration space
	if (!configAccess(true) || offset > (0x1000 - sizeof(uint8_t))) return;
	data = configWrite8Filter(offset, data);
    IOPCIAddressSpace _space = space;
    _space.es.registerNumExtended = ((offset >> 8) & 0xF);
    configWrite8(_space, offset, data);
}

// --

UInt32 IOPCIDevice::findPCICapability( UInt8 capabilityID, UInt8 * offset )
{
    if (!configAccess(true)) return 0;
    return (parent->findPCICapability(space, capabilityID, offset));
}

UInt32 IOPCIDevice::extendedFindPCICapability( UInt32 capabilityID, IOByteCount * offset )
{
    if (!configAccess(true)) return 0;
    return (parent->extendedFindPCICapability(reserved->configEntry, capabilityID, offset));
}

UInt32 IOPCIDevice::setConfigBits( UInt8 reg, UInt32 mask, UInt32 value )
{
    UInt32      was;
    UInt32      bits;

    bits = extendedConfigRead32( reg );
    was = (bits & mask);
    bits &= ~mask;
    bits |= (value & mask);
    extendedConfigWrite32( reg, bits );

    return (was);
}

bool IOPCIDevice::setBusLeadEnable( bool enable )
{
    return (0 != setConfigBits(kIOPCIConfigCommand, kIOPCICommandBusLead,
                               enable ? kIOPCICommandBusLead : 0));
}

bool IOPCIDevice::setBusMasterEnable( bool enable )
{
    return setBusLeadEnable(enable);
}

bool IOPCIDevice::setMemoryEnable( bool enable )
{
    return (0 != setConfigBits(kIOPCIConfigCommand, kIOPCICommandMemorySpace,
                               enable ? kIOPCICommandMemorySpace : 0));
}

bool IOPCIDevice::setIOEnable( bool enable, bool /* exclusive = false */ )
{
    // exclusive is TODO.
    return (0 != setConfigBits(kIOPCIConfigCommand, kIOPCICommandIOSpace,
                               enable ? kIOPCICommandIOSpace : 0));
}

UInt8 IOPCIDevice::getBusNumber( void )
{
    return (space.s.busNum);
}

UInt8 IOPCIDevice::getDeviceNumber( void )
{
    return (space.s.deviceNum);
}

UInt8 IOPCIDevice::getFunctionNumber( void )
{
    return (space.s.functionNum);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IODeviceMemory * IOPCIDevice::getDeviceMemoryWithIndex(unsigned int index)
{
	if (kTunnelL1NotSet == reserved->tunnelL1Allow) setTunnelL1Enable(this, false);

    return (super::getDeviceMemoryWithIndex(index));
}

IODeviceMemory * IOPCIDevice::getDeviceMemoryWithRegister( UInt8 reg )
{
    OSArray *           array;
    IODeviceMemory *    range;
    unsigned int        i = 0;

	if (kTunnelL1NotSet == reserved->tunnelL1Allow) setTunnelL1Enable(this, false);

    array = (OSArray *) getProperty( gIODeviceMemoryKey);
    if (0 == array)
        return (0);

    while ((range = (IODeviceMemory *) array->getObject(i++)))
    {
        if (reg == (range->getTag() & 0xff))
            break;
    }

    return (range);
}

IOMemoryMap * IOPCIDevice:: mapDeviceMemoryWithRegister( UInt8 reg,
        IOOptionBits options )
{
    IODeviceMemory *    range;
    IOMemoryMap *       map;

    range = getDeviceMemoryWithRegister( reg );
    if (range)
        map = range->map( options );
    else
        map = 0;

    return (map);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IODeviceMemory * IOPCIDevice::ioDeviceMemory( void )
{
    return (parent->ioDeviceMemory());
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService * IOPCIDevice::matchLocation( IOService * /* client */ )
{
    return (this);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSMetaClassDefineReservedUsed(IOPCIDevice,  0);
bool IOPCIDevice::hasPCIPowerManagement(IOOptionBits state)
{
    UInt16      pciPMCapReg, checkMask;
    OSData      *aString;

    reserved->sleepControlBits = 0;               // on a new query, we reset the proper sleep control bits
    if (!reserved->pmControlStatus) return (false);

    pciPMCapReg = extendedConfigRead16(reserved->pmControlStatus - sizeof(uint16_t));
//    DLOG("%s[%p]::hasPCIPwrMgmt found pciPMCapReg %x\n", 
//        getName(), this, pciPMCapReg);

    if (state)
    {
        checkMask = state;
        switch (state)
        {
            case kPCIPMCPMESupportFromD3Cold:
            case kPCIPMCPMESupportFromD3Hot:
                reserved->sleepControlBits = (kPCIPMCSPMEStatus | kPCIPMCSPMEEnable | kPCIPMCSPowerStateD3);
                break;
            case kPCIPMCPMESupportFromD2:
                reserved->sleepControlBits = (kPCIPMCSPMEStatus | kPCIPMCSPMEEnable | kPCIPMCSPowerStateD2);
                break;
            case kPCIPMCPMESupportFromD1:
                reserved->sleepControlBits = (kPCIPMCSPMEStatus | kPCIPMCSPMEEnable | kPCIPMCSPowerStateD1);
                break;
            case kPCIPMCD2Support:
                reserved->sleepControlBits = kPCIPMCSPowerStateD2;
                break;
            case kPCIPMCD1Support:
                reserved->sleepControlBits = kPCIPMCSPowerStateD1;
                break;
            case kPCIPMCD3Support:
                reserved->sleepControlBits = kPCIPMCSPowerStateD3;
                checkMask = 0;
                break;
            default:
                break;
        }
        if (checkMask && !(checkMask & pciPMCapReg))
            reserved->sleepControlBits = 0;
    }
    else
    {
        if ((aString = OSDynamicCast(OSData, getProperty("sleep-power-state"))))
        {
            DLOG("%s[%p]::hasPCIPwrMgmt found sleep-power-state string %p\n", getName(), this, aString);
            if (aString->isEqualTo("D3cold", static_cast<unsigned int>(strlen("D3cold"))))
                reserved->sleepControlBits = (kPCIPMCSPMEStatus | kPCIPMCSPMEEnable | kPCIPMCSPowerStateD3);
            else if (aString->isEqualTo("D3Hot", static_cast<unsigned int>(strlen("D3Hot"))))
                reserved->sleepControlBits = (kPCIPMCSPMEStatus | kPCIPMCSPMEEnable | kPCIPMCSPowerStateD3);
        }
    }

    return (reserved->sleepControlBits ? true : false);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSMetaClassDefineReservedUsed(IOPCIDevice,  1);
IOReturn IOPCIDevice::enablePCIPowerManagement(IOOptionBits state)
{
    IOReturn    ret = kIOReturnSuccess;

    if (0xffffffff == state) state = kPCIPMCSPowerStateD3;

    if (!reserved->pmControlStatus)
    {
        ret = kIOReturnBadArgument;
        return ret;
    }
        
    if ( state == kPCIPMCSPowerStateD0 )
    {
        reserved->sleepControlBits = 0;
        reserved->pmSleepEnabled = false;
        return ret;
    }
    else
    {
        UInt32  oldBits = reserved->sleepControlBits;
        
        reserved->sleepControlBits = state & kPCIPMCSPowerStateMask;
        
        if ( oldBits & kPCIPMCSPMEStatus )
            reserved->sleepControlBits |= kPCIPMCSPMEStatus;
        
        if ( oldBits & kPCIPMCSPMEEnable )
            reserved->sleepControlBits |= kPCIPMCSPMEEnable;
        
        if (!reserved->sleepControlBits)
        {
            DLOG("%s[%p] - enablePCIPwrMgmt - no sleep control bits - not enabling\n", getName(), this);
            ret = kIOReturnBadArgument;
        }
        else
        {
            DLOG("%s[%p] - enablePCIPwrMgmt, enabling\n", getName(), this);
            reserved->pmSleepEnabled = kPMEnable | kPMEOption;
			if (kPCIPMCSPMEDisableInS3 & state) reserved->pmSleepEnabled |= kPMEOptionS3Disable;
			if (kPCIPMCSPMEWakeReason & state)  reserved->pmSleepEnabled |= kPMEOptionWakeReason;
        }
    }
    return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn 
IOPCIDevice::callPlatformFunction(const OSSymbol * functionName,
                                          bool waitForFunction,
                                          void * p1, void * p2,
                                          void * p3, void * p4)
{
    IOReturn result;

    result = super::callPlatformFunction(functionName, waitForFunction,
                                         p1, p2, p3, p4);

    if ((kIOReturnUnsupported == result) 
     && (gIOPlatformDeviceASPMEnableKey == functionName)
     && getProperty(kIOPCIDeviceASPMSupportedKey))
    {
    	IOOptionBits state = (p2 != 0) ? reserved->expressASPMDefault : 0;
        result = parent->setDeviceASPMState(this, (IOService *) p1, state);
    }

    return (result);
}

IOReturn 
IOPCIDevice::callPlatformFunction(const char * functionName,
                                          bool waitForFunction,
                                          void * p1, void * p2,
                                          void * p3, void * p4)
{
    return (super::callPlatformFunction(functionName, waitForFunction,
                                         p1, p2, p3, p4));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIDevice::enableLTR(IOPCIDevice * device, bool enable)
{
	uint32_t reg;

	if (!reserved->expressCapability || !expressV2(this))   return (kIOReturnUnsupported);

	reg = extendedConfigRead32(reserved->expressCapability + 0x24);
	if (!((1 << 11) & reg))                                 return (kIOReturnUnsupported);

	reg = extendedConfigRead32(reserved->expressCapability + 0x28);
	reg &= ~(1 << 10);
	if (enable) reg |= ( 1 << 10);
	extendedConfigWrite32(reserved->expressCapability + 0x28, reg);

	return (kIOReturnSuccess);
}

IOReturn
IOPCIDevice::setLatencyTolerance(IOOptionBits type, uint64_t nanoseconds)
{
	static const uint32_t ltrScales[] = { 1, 32, 1024, 32768, 1048576, 33554432 };

	OSData * data;
	uint64_t ltrScale, ltrValue;
	uint32_t idx;
	uint32_t reg1;
	uint8_t  reg2;
	uint8_t  reg3;

	if (!reserved->ltrDevice)
	{
		IOPCIDevice * next;
		next = this;
		reserved->ltrDevice = (IOPCIDevice *) 1L;
		while (next)
		{
			if ((data = OSDynamicCast(OSData, next->getProperty("reg-ltrovr"))))
			{
				reserved->ltrDevice = next;
				uint64_t off = *((uint64_t *)data->getBytesNoCopy());
				reserved->ltrOffset = off;
				reserved->ltrReg1 = reserved->ltrDevice->extendedConfigRead32(reserved->ltrOffset);
				reserved->ltrReg2 = reserved->ltrDevice->extendedConfigRead8(reserved->ltrOffset + 4);
				break;
			}
			next = OSDynamicCast(IOPCIDevice, next->getParentEntry(gIODTPlane));
		}
	}

	if (((uintptr_t) reserved->ltrDevice) <= 1) return (kIOReturnUnsupported);

	for (ltrValue = idx = 0; idx < arrayCount(ltrScales); idx++)
	{
		ltrScale = ltrScales[idx];
		ltrValue = (nanoseconds / ltrScale);
		if (ltrValue < (1<<10)) break;
	}
	
	if (idx >= arrayCount(ltrScales)) return (kIOReturnMessageTooLarge);

    reg1 = reserved->ltrReg1;
    reg2 = reserved->ltrReg2;
    reg3 = reg2;

    if (kIOPCILatencySnooped & type)
    {
        reg1 &= 0x0000FFFF;
        reg1 |= (1 << 31) | (idx << 26) | (ltrValue << 16);
        reg2 &= ~(1 << 0);
        reg3 |= (1 << 3) | (1 << 0);
    }
    if (kIOPCILatencyUnsnooped & type)
    {
        reg1 &= 0xFFFF0000;
        reg1 |= (1 << 15) | (idx << 10) | (ltrValue << 0);
        reg2 &= ~(1 << 1);
        reg3 |= (1 << 3) | (1 << 1);
    }
    reserved->ltrDevice->extendedConfigWrite32(reserved->ltrOffset, reg1);
    reserved->ltrDevice->extendedConfigWrite8(reserved->ltrOffset + 4, reg2);
    reserved->ltrDevice->extendedConfigWrite8(reserved->ltrOffset + 4, reg3);
    reserved->ltrReg1 = reg1;
    reserved->ltrReg2 = reg3;

#if 0
    DLOG("%s: ltr 0x%x = 0x%x, 0x%x\n",
         reserved->ltrDevice->getName(),
         (int)reserved->ltrOffset,
         reserved->ltrDevice->extendedConfigRead32(reserved->ltrOffset),
         reserved->ltrDevice->extendedConfigRead8(reserved->ltrOffset + 4));
#endif

	return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIDevice::enableACS(IOPCIDevice * device, bool enable)
{
    uint16_t reg = 0;

    if (!reserved->acsCapability)   return (kIOReturnUnsupported);
    
    if (enable) {
        reg = extendedConfigRead16(reserved->acsCapability + 0x4);
        reg &= kIOPCIExpressACSDefault;
    }
    extendedConfigWrite16(reserved->acsCapability + 0x6, reg);

    return (kIOReturnSuccess);
}


IOReturn 
IOPCIDevice::setASPMState(IOService * client, IOOptionBits state)
{
	IOPCI2PCIBridge * pcib;

	if (!(pcib = OSDynamicCast(IOPCI2PCIBridge, parent))) return (kIOReturnUnsupported);

    return (pcib->setDeviceASPMState(this, client, state));
}

IOReturn
IOPCIDevice::setTunnelL1Enable(IOService * client, bool l1Enable)
{
	IOPCI2PCIBridge * pcib;

	if (!(pcib = OSDynamicCast(IOPCI2PCIBridge, parent))) return (kIOReturnUnsupported);

    return (pcib->setTunnelL1Enable(this, client, l1Enable));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOPCIEventSource * 
IOPCIDevice::createEventSource(OSObject * owner, IOPCIEventSource::Action action, uint32_t options)
{
    return (parent->createEventSource(this, owner, action, options));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSObject* IOPCIDevice::getProperty(const OSSymbol * aKey) const
{
    OSObject *value;

    if (aKey == gIOInterruptControllersKey || aKey == gIOInterruptSpecifiersKey)
    {
        IORecursiveLockLock(reserved->lock);
        value = super::getProperty(aKey);
        if (!value)
        {
            OSArray * controllers = OSArray::withCapacity(1);
            OSArray * specifiers  = OSArray::withCapacity(1);
            if (controllers && specifiers)
            {
                IOPCIDevice * self = const_cast<IOPCIDevice*>(this);
                self->setProperty(gIOInterruptControllersKey, controllers);
                self->setProperty(gIOInterruptSpecifiersKey,  specifiers);
            }
            OSSafeReleaseNULL(controllers);
            OSSafeReleaseNULL(specifiers);
        }
        if (!reserved->interruptVectorsResolved)
        {
            reserved->interruptVectorsResolved = 1;
            parent->resolveInterrupts(const_cast<IOPCIDevice*>(this));
        }
        IORecursiveLockUnlock(reserved->lock);
        if (!value)
        {
            value = super::getProperty(aKey);
        }
    } else if (aKey == gIOPCIExpressLinkStatusKey) {
		// Update cached Link Status register
		IOPCIDevice *device = (IOPCIDevice *)this; // strip 'const' qualifier
		device->checkLink(kCheckLinkForPower);
		value = super::getProperty(aKey);
    } else {
        value = super::getProperty(aKey);
    }
    return value;
}

IOReturn
IOPCIDevice::configureInterrupts(UInt32 interruptType, UInt32 numRequired, UInt32 numRequested, IOOptionBits options)
{
    IOReturn       ret = kIOReturnBadArgument;

    if (!numRequired || !numRequested || (numRequired > numRequested) || options) return ret;
    IORecursiveLockLock(reserved->lock);
    if (reserved->interruptVectorsResolved) // TODO add cleanup support on numRequired == 0 (if needed).
    {
        // Return success if this request is satisfied by the existing allocation.
        ret = kIOReturnUnsupported;

        switch (interruptType)
        {
        case kIOInterruptTypeLevel:
            if (reserved->legacyInterruptResolved)
            {
                ret = kIOReturnSuccess;
            }
            break;

        case kIOInterruptTypePCIMessaged:
            if (   !(reserved->msiMode & kMSIX)
                && (reserved->msiPhysVectorCount >= numRequired))
            {
                ret = kIOReturnSuccess;
            }
            break;
        case kIOInterruptTypePCIMessagedX:
            if (   (reserved->msiMode & kMSIX)
                && (reserved->msiPhysVectorCount >= numRequired))
            {
                ret = kIOReturnSuccess;
            }
            break;
        }

        IORecursiveLockUnlock(reserved->lock);
        return ret;
    }
    reserved->interruptVectorsResolved = 1;
    switch (interruptType)
    {
    case kIOInterruptTypeLevel:
        ret = parent->resolveLegacyInterrupts(parent->getProvider(), this);
        break;
    case kIOInterruptTypePCIMessaged:
        if (reserved->msiMode & kMSIX)
        {
            IOByteCount capa = 0;
            extendedFindPCICapability(kIOPCIMSICapability, &capa);
            if (!capa) break;
            reserved->msiMode &= ~kMSIX;
            reserved->msiCapability = capa;
        }
        // Round numRequested up to the power of 2:
        numRequested--;
        numRequested |= numRequested >> 1;
        numRequested |= numRequested >> 2;
        numRequested |= numRequested >> 4;
        numRequested++;
        ret = parent->resolveMSIInterrupts(parent->getProvider(), this, numRequired, numRequested);
        break;
    case kIOInterruptTypePCIMessagedX:
        if (~reserved->msiMode & kMSIX)
        {
            IOByteCount capa = 0;
            extendedFindPCICapability(kIOPCIMSIXCapability, &capa);
            if (!capa) break;
            reserved->msiMode |= kMSIX;
            reserved->msiCapability = capa;
        }
        ret = parent->resolveMSIInterrupts(parent->getProvider(), this, numRequired, numRequested);
    }
    if (ret)
    {
        reserved->interruptVectorsResolved = 0;
    }
    IORecursiveLockUnlock(reserved->lock);
    return ret;
}

IOReturn
IOPCIDevice::setProperties(OSObject * properties)
{
    IOReturn       ret = kIOReturnUnsupported;
    OSDictionary * dict;
    IOService *    ejectable;
    IOService *    topEjectable = NULL;

    dict = OSDynamicCast(OSDictionary, properties);
    if (dict)
    {
        if (kOSBooleanFalse == dict->getObject(kIOPCIOnlineKey))
        {
            ejectable = this;
            do
            {
                if (ejectable->getProperty(kIOPCIEjectableKey))
                {
                    ejectable->setProperty(kIOPCIOnlineKey, kOSBooleanFalse);
                    topEjectable = ejectable;
                }
                ejectable = ejectable->getProvider();
            }
            while (ejectable);
            if (topEjectable)
            {
                ret = topEjectable->requestProbe(kIOPCIProbeOptionEject | kIOPCIProbeOptionDone);
            }
            return (ret);
        }
    }

    return (super::setProperties(properties));
}

bool IOPCIDevice::setProperty(const OSSymbol * aKey, OSObject *anObject)
{
	return super::setProperty(aKey, anObject);
}

bool IOPCIDevice::setProperty(const OSString * aKey, OSObject *anObject)
{
	return super::setProperty(aKey, anObject);
}

bool IOPCIDevice::hasL1Errata(IOPCIDevice *nub)
{
	return false;
}

bool IOPCIDevice::setProperty(const char* aKey, OSObject* anObject)
{
#if TARGET_OS_HAS_THUNDERBOLT
	// Thunderbolt will not set its properties on the upstream bridge's IOPCIDevice until
	// the service is registered, so the logic to check CLx state in
	// IOPCIBridge::publishNub() will not run for the upstream bridge. Perform those checks
	// here instead.
	if (strncmp(aKey, "Thunderbolt Entry ID", 20) == 0)
	{
		OSNumber *tbEntryID = OSDynamicCast(OSNumber, anObject);
		if (tbEntryID)
		{
			OSDictionary *matching = registryEntryIDMatching(tbEntryID->unsigned64BitValue());
			IOService *tbNode = copyMatchingService(matching);
			OSSafeReleaseNULL(matching);
			if (tbNode)
			{
				OSNumber *clx = OSDynamicCast(OSNumber, tbNode->getProperty(kIOThunderboltPortCLxStateProperty, gIOServicePlane));
				tbNode->release();
				if (clx && clx->unsigned32BitValue()) setProperty(kIOCLxEnabledKey, kOSBooleanTrue);
			}
		}

		// If this is a switch upstream port whose upstream-facing thunderbolt port has CLx Enabled,
		// enable ASPM.
		if (   ((reserved->expressCapabilities >> 4) & 0xF) == 0x5
			&& propertyHasValue(kIOCLxEnabledKey, kOSBooleanTrue)
			&& !hasL1Errata(this))
		{
			setASPMState(this, 2);
		}
	}
#endif

    return super::setProperty(aKey, anObject);
}

bool IOPCIDevice::setProperty(const char* aKey, const char* aString)
{
    return super::setProperty(aKey, aString);
}

bool IOPCIDevice::setProperty(const char* aKey, bool aBoolean)
{
    return super::setProperty(aKey, aBoolean);
}

bool IOPCIDevice::setProperty(const char* aKey, unsigned long long aValue, unsigned int aNumberOfBits)
{
    return super::setProperty(aKey, aValue, aNumberOfBits);
}

bool IOPCIDevice::setProperty(const char* aKey, void* bytes, unsigned int length)
{
    return super::setProperty(aKey, bytes, length);
}

IOReturn IOPCIDevice::requestProbe(IOOptionBits options)
{
    if (kIOReturnSuccess != IOUserClient::clientHasPrivilege(current_task(), 
                                kIOClientPrivilegeLocalUser))
    {
        IOLog("IOPCIDevice requestProbe failed insufficient privileges\n");
        return (kIOReturnNotPrivileged);
    }

    // debug
    return (kernelRequestProbe(options));
}

IOReturn IOPCIDevice::kernelRequestProbe(uint32_t options)
{
    return (parent->busProbe(this, options));
}

IOReturn IOPCIDevice::protectDevice(uint32_t space, uint32_t prot)
{
	if (space != kIOPCIConfigSpace)
		return (kIOReturnUnsupported);

	reserved->configProt = prot;

    return (parent->protectDevice(this, space, prot));
}

IOReturn IOPCIDevice::checkLink(uint32_t options)
{
    return (parent->checkLink(options));
}

IOReturn IOPCIDevice::relocate(uint32_t options)
{
    return (parent->relocate(this, options));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOPCIDevice::setConfigHandler(IOPCIDeviceConfigHandler handler, void * ref,
                              IOPCIDeviceConfigHandler * currentHandler, void ** currentRef)
{
    if (!configShadow(this))
        return (kIOReturnError);

    if (currentHandler)
        *currentHandler = configShadow(this)->handler;
    if (currentRef)
        *currentRef = configShadow(this)->handlerRef;

	configShadow(this)->handler    = handler;
	configShadow(this)->handlerRef = ref;

    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIDevice::newUserClient(task_t owningTask, void * securityID,
                                    UInt32 type,  OSDictionary * properties,
                                    IOUserClient ** handler)
{
    return (kIOReturnUnsupported);
}

bool IOPCIDevice::handleOpen(IOService * forClient, IOOptionBits options, void * arg)
{
    OSObject *prop = NULL;

    bool result = super::handleOpen(forClient, options, arg);

    if (result == true)
    {
        reserved->sessionOptions = options;

#if ACPI_SUPPORT
        // Copy relevant keys for mapper selection
        if (forClient) {
            if (forClient->getProperty(kIOPCIUseDeviceMapperKey))
                setProperty(kIOPCIUseDeviceMapperKey, true);
            if ((prop = forClient->getProperty(kCFBundleIdentifierKey)) != NULL)
                setProperty(kIOPCIChildBundleIdentifierKey, prop);
        }
    
        getResources();
#endif

        if((options & kIOPCISessionOptionDriverkit) != 0)
        {
            // check if the task is entitled to disable the offload engine
#if 0
            OSObject* offloadEngineDisableEntitlement = IOUserClient::copyClientEntitlement(current_task(), kIOPCITransportDextEntitlementOffloadEngineDisable);
            if(offloadEngineDisableEntitlement != NULL)
            {
                reserved->offloadEngineMMIODisable = 1;
            }
            OSSafeReleaseNULL(offloadEngineDisableEntitlement);
#else
            reserved->offloadEngineMMIODisable = 1;
#endif
        }

    }

    return result;
}

uint16_t IOPCIDevice::getCloseCommandMask(uint32_t vendorDevice)
{
    uint16_t commandMask = kIOPCICommandBusLead | kIOPCICommandMemorySpace;

    if (IOPCIBridge::hasBusLeadCTOBug(vendorDevice))
    {
        commandMask &= ~kIOPCICommandBusLead;
    }

    return commandMask;
}

void IOPCIDevice::handleClose(IOService * forClient, IOOptionBits options)
{
    if ((forClient != NULL) && (isOpen(forClient) == true))
    {
        if ((reserved->sessionOptions & kIOPCISessionOptionDriverkit) != 0)
        {
			uint32_t viddid = savedConfigRead32(&configShadow(this)->configSave, 0);
            reserved->offloadEngineMMIODisable = 0;
            // Driverkit either called close or crashed. Turn off bus leading to prevent any further DMAs
            uint16_t command = extendedConfigRead16(kIOPCIConfigurationOffsetCommand);
            uint16_t commandMask = getCloseCommandMask(viddid);
            if ((command & commandMask) != 0)
            {
                DLOG("IOPCIDevice::handleClose: disabling memory%s for client %s\n", commandMask & kIOPCICommandBusLead ? " and bus leading" : "", (forClient) ? forClient->getName() : "unknown");
                extendedConfigWrite16(kIOPCIConfigurationOffsetCommand, command & ~commandMask);
            }

            configShadow(this)->configSave.savedConfig[1] &= 0xFFFF0000;
            configShadow(this)->configSave.savedConfig[1] |= (command & ~commandMask);

            // Reset the shadow config state for the next dext instance
            configShadow(this)->flags &= ~kIOPCIConfigShadowPermanent;
            configShadow(this)->restoreCount = 0;
        }

#if ACPI_SUPPORT
        removeProperty(kIOPCIChildBundleIdentifierKey);
        removeProperty(kIOPCIUseDeviceMapperKey);
        removeProperty("iommu-selection");
#endif
    }

    super::handleClose(forClient, options);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOPCIDevice::copyAERErrorDescriptionForBit(bool uncorrectable, uint32_t bit, char * string, size_t maxLength)
{
    const char * desc;

    if (uncorrectable) 
    {
        switch (bit)
        {
            case kIOPCIUncorrectableErrorBitDataLinkProtocol:
                desc = "DataLinkProtocol";
                break;
            case kIOPCIUncorrectableErrorBitSurpriseDown:
                desc = "SurpriseDown";
                break;
            case kIOPCIUncorrectableErrorBitPoisonedTLP:
                desc = "PoisonedTLP";
                break;
            case kIOPCIUncorrectableErrorBitFlowControlProtocol:
                desc = "FlowControlProtocol";
                break;
            case kIOPCIUncorrectableErrorBitCompletionTimeout:
                desc = "CompletionTimeout";
                break;
            case kIOPCIUncorrectableErrorBitCompleterAbort:
                desc = "CompleterAbort";
                break;
            case kIOPCIUncorrectableErrorBitUnexpectedCompletion:
                desc = "UnexpectedCompletion";
                break;
            case kIOPCIUncorrectableErrorBitReceiverOverflow:
                desc = "ReceiverOverflow";
                break;
            case kIOPCIUncorrectableErrorBitMalformedTLP:
                desc = "MalformedTLP";
                break;
            case kIOPCIUncorrectableErrorBitECRC:
                desc = "ECRC";
                break;
            case kIOPCIUncorrectableErrorBitUnsupportedRequest:
                desc = "UnsupportedRequest";
                break;
            case kIOPCIUncorrectableErrorBitACSViolation:
                desc = "ACSViolation";
                break;
            case kIOPCIUncorrectableErrorBitInternal:
                desc = "Internal";
                break;
            case kIOPCIUncorrectableErrorBitMCBlockedTLP:
                desc = "MCBlockedTLP";
                break;
            case kIOPCIUncorrectableErrorBitAtomicOpEgressBlocked:
                desc = "AtomicOpEgressBlocked";
                break;
            case kIOPCIUncorrectableErrorBitTLPPrefixBlocked:
                desc = "TLPPrefixBlocked";
                break;
            default:
                desc = NULL;
                break;
        }
        snprintf(string, maxLength, "IOPCIUncorrectableError(%d%s%s)", bit, desc ? ", " : "", desc ? desc : "");
    }
    else 
    {
        switch (bit)
        {
            case kIOPCICorrectableErrorBitReceiver:
                desc = "Receiver";
                break;
            case kIOPCICorrectableErrorBitBadTLP:
                desc = "BadTLP";
                break;
            case kIOPCICorrectableErrorBitBadDLLP:
                desc = "BadDLLP";
                break;
            case kIOPCICorrectableErrorBitReplayNumRollover:
                desc = "ReplayNumRollover";
                break;
            case kIOPCICorrectableErrorBitReplayTimerTimeout:
                desc = "ReplayTimerTimeout";
                break;
            case kIOPCICorrectableErrorBitAdvisoryNonFatal:
                desc = "AdvisoryNonFatal";
                break;
            case kIOPCICorrectableErrorBitCorrectedInternal:
                desc = "Internal";
                break;
            case kIOPCICorrectableErrorBitHeaderLogOverflow:
                desc = "HeaderLogOverflow";
                break;
            default:
                desc = NULL;
                break;
        }
        snprintf(string, maxLength, "IOPCICorrectableError(%d%s%s)", bit, desc ? ", " : "", desc ? desc : "");
    }
}

OSMetaClassDefineReservedUsed(IOPCIDevice,  4);
IOReturn IOPCIDevice::deviceMemoryRead(uint8_t   memoryIndex,
									   uint64_t  offset,
									   void*     data,
									   uint8_t   size,
									   IOOptionBits options)
{
	switch (size)
	{
	case sizeof(uint64_t):
		return deviceMemoryRead64(memoryIndex, offset, reinterpret_cast<uint64_t*>(data), options);
	case sizeof(uint32_t):
		return deviceMemoryRead32(memoryIndex, offset, reinterpret_cast<uint32_t*>(data), options);
	case sizeof(uint16_t):
		return deviceMemoryRead16(memoryIndex, offset, reinterpret_cast<uint16_t*>(data), options);
	case sizeof(uint8_t):
		return deviceMemoryRead8(memoryIndex, offset, reinterpret_cast<uint8_t*>(data), options);
	default:
		return kIOReturnBadArgument;
	}
}

IOReturn IOPCIDevice::deviceMemoryRead64(uint8_t      memoryIndex,
                                         uint64_t     offset,
                                         uint64_t*    readData,
                                         IOOptionBits options)
{
    IOMemoryMap* deviceMemoryMap = reserved->deviceMemoryMap[memoryIndex];
    IOReturn result = kIOReturnUnsupported;

    if(deviceMemoryMap != NULL)
    {
        IOByteCount length = deviceMemoryMap->getLength();
        uint64_t    sum = 0;

        if(   (offset + sizeof(uint64_t)) > length
           || (os_add_overflow(offset, sizeof(uint64_t), &sum)))
        {
            return kIOReturnOverrun;
        }
    }
    else
    {
        DLOG("IOPCIDevice::deviceMemoryRead64: index %u could not get mapping\n", memoryIndex);
        return kIOReturnNoMemory;
    }

#if TARGET_CPU_ARM || TARGET_CPU_ARM64
    if(   ((reserved->offloadEngineMMIODisable == 0) || (options & kIOPCIAccessLatencyTolerantHint))
       && (ml_get_interrupts_enabled() == true)
       && (ml_at_interrupt_context() == false))
    {
        IODeviceMemory* deviceMemoryDescriptor = reserved->deviceMemory[memoryIndex];
        if(deviceMemoryDescriptor == NULL)
        {
            DLOG("IOPCIDevice::deviceMemoryRead64: failed to get memory for index %u\n", memoryIndex);
            return kIOReturnBadArgument;
        }

        IOPCIAddressSpace addressSpace;
        addressSpace.bits = static_cast<uint32_t>(deviceMemoryDescriptor->getTag());

        if(   (addressSpace.s.space != kIOPCI32BitMemorySpace)
           && (addressSpace.s.space != kIOPCI64BitMemorySpace))
        {
            DLOG("IOPCIDevice::deviceMemoryRead64: index %u is not MMIO space\n", memoryIndex);
            return kIOReturnBadArgument;
        }


        result = reserved->hostBridge->deviceMemoryRead(deviceMemoryDescriptor,
                                                        offset,
                                                        readData,
                                                        sizeof(uint64_t));
    }
#endif

    // if the host bridge can't do the access through a DMA transaction, directly do the transaction ourself
    if(result == kIOReturnUnsupported)
    {
        *readData = ml_io_read64(deviceMemoryMap->getVirtualAddress() + offset);
        result = kIOReturnSuccess;
    }

    return result;
}

IOReturn IOPCIDevice::deviceMemoryRead64(uint8_t      memoryIndex,
                                         uint64_t     offset,
                                         uint64_t*    readData)
{
	return deviceMemoryRead64(memoryIndex, offset, readData, 0);
}

IOReturn IOPCIDevice::deviceMemoryRead32(uint8_t      memoryIndex,
                                         uint64_t     offset,
                                         uint32_t*    readData,
                                         IOOptionBits options)
{
    IOMemoryMap* deviceMemoryMap = reserved->deviceMemoryMap[memoryIndex];
    IOReturn result = kIOReturnUnsupported;

    if(deviceMemoryMap != NULL)
    {
        IOByteCount length = deviceMemoryMap->getLength();
        uint64_t    sum = 0;

        if(   (offset + sizeof(uint32_t)) > length
           || (os_add_overflow(offset, sizeof(uint32_t), &sum)))
        {
            return kIOReturnOverrun;
        }
    }
    else
    {
        DLOG("IOPCIDevice::deviceMemoryRead32: index %u could not get mapping\n", memoryIndex);
        return kIOReturnNoMemory;
    }

#if TARGET_CPU_ARM || TARGET_CPU_ARM64
    if(   ((reserved->offloadEngineMMIODisable == 0) || (options & kIOPCIAccessLatencyTolerantHint))
       && (ml_get_interrupts_enabled() == true)
       && (ml_at_interrupt_context() == false))
    {
        IODeviceMemory* deviceMemoryDescriptor = reserved->deviceMemory[memoryIndex];
        if(deviceMemoryDescriptor == NULL)
        {
            DLOG("IOPCIDevice::deviceMemoryRead32: failed to get memory for index %u\n", memoryIndex);
            return kIOReturnBadArgument;
        }

        IOPCIAddressSpace addressSpace;
        addressSpace.bits = static_cast<uint32_t>(deviceMemoryDescriptor->getTag());

        if(   (addressSpace.s.space != kIOPCI32BitMemorySpace)
           && (addressSpace.s.space != kIOPCI64BitMemorySpace))
        {
            DLOG("IOPCIDevice::deviceMemoryRead32: index %u is not MMIO space\n", memoryIndex);
            return kIOReturnBadArgument;
        }

        result = reserved->hostBridge->deviceMemoryRead(deviceMemoryDescriptor,
                                                        offset,
                                                        readData,
                                                        sizeof(uint32_t));
    }
#endif

    // if the host bridge can't do the access through a DMA transaction, directly do the transaction ourself
    if(result == kIOReturnUnsupported)
    {
        *readData = ml_io_read32(deviceMemoryMap->getVirtualAddress() + offset);
        result = kIOReturnSuccess;
    }

    return result;
}

IOReturn IOPCIDevice::deviceMemoryRead32(uint8_t      memoryIndex,
                                         uint64_t     offset,
                                         uint32_t*    readData)
{
	return deviceMemoryRead32(memoryIndex, offset, readData, 0);
}

IOReturn IOPCIDevice::deviceMemoryRead16(uint8_t      memoryIndex,
                                         uint64_t     offset,
                                         uint16_t*    readData,
                                         IOOptionBits options)
{
    IOMemoryMap* deviceMemoryMap = reserved->deviceMemoryMap[memoryIndex];
    IOReturn result = kIOReturnUnsupported;

    if(deviceMemoryMap != NULL)
    {
        IOByteCount length = deviceMemoryMap->getLength();
        uint64_t    sum = 0;

        if(   (offset + sizeof(uint16_t)) > length
           || (os_add_overflow(offset, sizeof(uint16_t), &sum)))
        {
            return kIOReturnOverrun;
        }
    }
    else
    {
        DLOG("IOPCIDevice::deviceMemoryRead16: index %u could not get mapping\n", memoryIndex);
        return kIOReturnNoMemory;
    }

#if TARGET_CPU_ARM || TARGET_CPU_ARM64
    if(   ((reserved->offloadEngineMMIODisable == 0) || (options & kIOPCIAccessLatencyTolerantHint))
       && (ml_get_interrupts_enabled() == true)
       && (ml_at_interrupt_context() == false))
    {
        IODeviceMemory* deviceMemoryDescriptor = reserved->deviceMemory[memoryIndex];
        if(deviceMemoryDescriptor == NULL)
        {
            DLOG("IOPCIDevice::deviceMemoryRead16: failed to get memory for index %u\n", memoryIndex);
            return kIOReturnBadArgument;
        }

        IOPCIAddressSpace addressSpace;
        addressSpace.bits = static_cast<uint32_t>(deviceMemoryDescriptor->getTag());

        if(   (addressSpace.s.space != kIOPCI32BitMemorySpace)
           && (addressSpace.s.space != kIOPCI64BitMemorySpace))
        {
            DLOG("IOPCIDevice::deviceMemoryRead16: index %u is not MMIO space\n", memoryIndex);
            return kIOReturnBadArgument;
        }

        result = reserved->hostBridge->deviceMemoryRead(deviceMemoryDescriptor,
                                                        offset,
                                                        readData,
                                                        sizeof(uint16_t));
    }
#endif

    // if the host bridge can't do the access through a DMA transaction, directly do the transaction ourself
    if(result == kIOReturnUnsupported)
    {
        *readData = ml_io_read16(deviceMemoryMap->getVirtualAddress() + offset);
        result = kIOReturnSuccess;
    }

    return result;
}

IOReturn IOPCIDevice::deviceMemoryRead16(uint8_t      memoryIndex,
                                         uint64_t     offset,
                                         uint16_t*    readData)
{
	return deviceMemoryRead16(memoryIndex, offset, readData, 0);
}

IOReturn IOPCIDevice::deviceMemoryRead8(uint8_t      memoryIndex,
                                        uint64_t     offset,
                                        uint8_t*     readData,
                                        IOOptionBits options)
{
    IOMemoryMap* deviceMemoryMap = reserved->deviceMemoryMap[memoryIndex];
    IOReturn result = kIOReturnUnsupported;

    if(deviceMemoryMap != NULL)
    {
        IOByteCount length = deviceMemoryMap->getLength();
        uint64_t    sum = 0;

        if(   (offset + sizeof(uint8_t)) > length
           || (os_add_overflow(offset, sizeof(uint8_t), &sum)))
        {
            return kIOReturnOverrun;
        }
    }
    else
    {
        DLOG("IOPCIDevice::deviceMemoryRead8: index %u could not get mapping\n", memoryIndex);
        return kIOReturnNoMemory;
    }

#if TARGET_CPU_ARM || TARGET_CPU_ARM64
    if(   ((reserved->offloadEngineMMIODisable == 0) || (options & kIOPCIAccessLatencyTolerantHint))
       && (ml_get_interrupts_enabled() == true)
       && (ml_at_interrupt_context() == false))
    {
        IODeviceMemory* deviceMemoryDescriptor = reserved->deviceMemory[memoryIndex];
        if(deviceMemoryDescriptor == NULL)
        {
            DLOG("IOPCIDevice::deviceMemoryRead8: failed to get memory for index %u\n", memoryIndex);
            return kIOReturnBadArgument;
        }

        IOPCIAddressSpace addressSpace;
        addressSpace.bits = static_cast<uint32_t>(deviceMemoryDescriptor->getTag());

        if(   (addressSpace.s.space != kIOPCI32BitMemorySpace)
           && (addressSpace.s.space != kIOPCI64BitMemorySpace))
        {
            DLOG("IOPCIDevice::deviceMemoryRead8: index %u is not MMIO space\n", memoryIndex);
            return kIOReturnBadArgument;
        }

        result = reserved->hostBridge->deviceMemoryRead(deviceMemoryDescriptor,
                                                        offset,
                                                        readData,
                                                        sizeof(uint8_t));
    }
#endif

    // if the host bridge can't do the access through a DMA transaction, directly do the transaction ourself
    if(result == kIOReturnUnsupported)
    {
        *readData = ml_io_read8(deviceMemoryMap->getVirtualAddress() + offset);
        result = kIOReturnSuccess;
    }

    return result;
}

IOReturn IOPCIDevice::deviceMemoryRead8(uint8_t      memoryIndex,
                                        uint64_t     offset,
                                        uint8_t*     readData)
{
	return deviceMemoryRead8(memoryIndex, offset, readData, 0);
}

OSMetaClassDefineReservedUsed(IOPCIDevice,  5);
IOReturn IOPCIDevice::deviceMemoryWrite(uint8_t      memoryIndex,
										uint64_t     offset,
										uint64_t     data,
										uint8_t      size,
										IOOptionBits options)
{
	switch (size)
	{
	case sizeof(uint64_t):
		return deviceMemoryWrite64(memoryIndex, offset, static_cast<uint64_t>(data), options);
	case sizeof(uint32_t):
		return deviceMemoryWrite32(memoryIndex, offset, static_cast<uint32_t>(data), options);
	case sizeof(uint16_t):
		return deviceMemoryWrite16(memoryIndex, offset, static_cast<uint16_t>(data), options);
	case sizeof(uint8_t):
		return deviceMemoryWrite8(memoryIndex, offset, static_cast<uint8_t>(data), options);
	default:
		return kIOReturnBadArgument;
	}
}

// TODO: support memory writes being routed to the host bridge?
IOReturn IOPCIDevice::deviceMemoryWrite64(uint8_t      memoryIndex,
                                          uint64_t     offset,
                                          uint64_t     data,
                                          IOOptionBits options)
{
    IOReturn result = kIOReturnUnsupported;

    IOMemoryMap* deviceMemoryMap = reserved->deviceMemoryMap[memoryIndex];
    if(deviceMemoryMap != NULL)
    {
        IOVirtualAddress address = deviceMemoryMap->getVirtualAddress();
        IOByteCount      length  = deviceMemoryMap->getLength();
        uint64_t         sum = 0;

        if(   (offset + sizeof(uint64_t)) > length
           || (os_add_overflow(offset, sizeof(uint64_t), &sum)))
        {
            return kIOReturnOverrun;
        }

        ml_io_write(address + offset, data, sizeof(uint64_t));
        result = kIOReturnSuccess;
    }
    else
    {
        DLOG("IOPCIDevice::deviceMemoryWrite64: index %u could not get mapping\n", memoryIndex);
        return kIOReturnNoMemory;
    }

    return result;
}

IOReturn IOPCIDevice::deviceMemoryWrite64(uint8_t      memoryIndex,
                                          uint64_t     offset,
                                          uint64_t     data)
{
	deviceMemoryWrite64(memoryIndex, offset, data, 0);
}

IOReturn IOPCIDevice::deviceMemoryWrite32(uint8_t      memoryIndex,
                                          uint64_t     offset,
                                          uint32_t     data,
                                          IOOptionBits options)
{
    IOReturn result = kIOReturnUnsupported;

    IOMemoryMap* deviceMemoryMap = reserved->deviceMemoryMap[memoryIndex];
    if(deviceMemoryMap != NULL)
    {
        IOVirtualAddress address = deviceMemoryMap->getVirtualAddress();
        IOByteCount      length  = deviceMemoryMap->getLength();
        uint64_t         sum = 0;

        if(   (offset + sizeof(uint32_t)) > length
           || (os_add_overflow(offset, sizeof(uint32_t), &sum)))
        {
            return kIOReturnOverrun;
        }

        ml_io_write(address + offset, data, sizeof(uint32_t));
        result = kIOReturnSuccess;
    }
    else
    {
        DLOG("IOPCIDevice::deviceMemoryWrite32: index %u could not get mapping\n", memoryIndex);
        return kIOReturnNoMemory;
    }

    return result;
}

IOReturn IOPCIDevice::deviceMemoryWrite32(uint8_t      memoryIndex,
                                          uint64_t     offset,
                                          uint32_t     data)
{
	deviceMemoryWrite32(memoryIndex, offset, data, 0);
}

IOReturn IOPCIDevice::deviceMemoryWrite16(uint8_t      memoryIndex,
                                          uint64_t     offset,
                                          uint16_t     data,
                                          IOOptionBits options)
{
    IOReturn result = kIOReturnUnsupported;

    IOMemoryMap* deviceMemoryMap = reserved->deviceMemoryMap[memoryIndex];
    if(deviceMemoryMap != NULL)
    {
        IOVirtualAddress address = deviceMemoryMap->getVirtualAddress();
        IOByteCount      length  = deviceMemoryMap->getLength();
        uint64_t         sum = 0;

        if(   (offset + sizeof(uint16_t)) > length
           || (os_add_overflow(offset, sizeof(uint16_t), &sum)))
        {
            return kIOReturnOverrun;
        }

        ml_io_write(address + offset, data, sizeof(uint16_t));
        result = kIOReturnSuccess;
    }
    else
    {
        DLOG("IOPCIDevice::deviceMemoryWrite16: index %u could not get mapping\n", memoryIndex);
        return kIOReturnNoMemory;
    }

    return result;
}

IOReturn IOPCIDevice::deviceMemoryWrite16(uint8_t      memoryIndex,
                                          uint64_t     offset,
                                          uint16_t     data)
{
	deviceMemoryWrite16(memoryIndex, offset, data, 0);
}

IOReturn IOPCIDevice::deviceMemoryWrite8(uint8_t      memoryIndex,
                                         uint64_t     offset,
                                         uint8_t      data,
                                         IOOptionBits options)
{
    IOReturn result = kIOReturnUnsupported;
    IOMemoryMap* deviceMemoryMap = reserved->deviceMemoryMap[memoryIndex];
    if(deviceMemoryMap != NULL)
    {
        IOVirtualAddress address = deviceMemoryMap->getVirtualAddress();
        IOByteCount      length  = deviceMemoryMap->getLength();
        uint64_t         sum = 0;

        if(   (offset + sizeof(uint8_t)) > length
           || (os_add_overflow(offset, sizeof(uint8_t), &sum)))
        {
            return kIOReturnOverrun;
        }

        ml_io_write(address + offset, data, sizeof(uint8_t));
        result = kIOReturnSuccess;
    }
    else
    {
        DLOG("IOPCIDevice::deviceMemoryWrite8: index %u could not get mapping\n", memoryIndex);
        return kIOReturnNoMemory;
    }

    return result;
}

IOReturn IOPCIDevice::deviceMemoryWrite8(uint8_t      memoryIndex,
                                         uint64_t     offset,
                                         uint8_t      data)
{
	deviceMemoryWrite8(memoryIndex, offset, data, 0);
}

IOReturn IOPCIDevice::setLinkSpeed(tIOPCILinkSpeed linkSpeed,
								   bool            retrain)
{
	return parent->setLinkSpeed(linkSpeed, retrain);
}

IOReturn IOPCIDevice::getLinkSpeed(tIOPCILinkSpeed *linkSpeed)
{
	return parent->getLinkSpeed(linkSpeed);
}

void IOPCIDevice::launchReprobeThread(void)
{
	thread_call_t threadCall = thread_call_allocate(OSMemberFunctionCast(thread_call_func_t,
																		 this,
																		 &IOPCIDevice::reprobeThreadCall),
													this);

	if(threadCall != NULL)
	{
		retain();
		parent->retain();
		if(thread_call_enter1(threadCall, threadCall /* so the call cleans itself up */) == TRUE)
		{
			thread_call_free(threadCall);
			parent->release();
			release();
		}
	}
}

void IOPCIDevice::prepareFLR(void)
{
	// Prepare for the FLR by preventing new upstream transactions and flushing
	// in-flight ones, in order to satisfy the requirement that software "must
	// not initialize the Function until allowing adequate time for any
	// associated Completions to arrive."

	// Clear the function's Bus Lead and SERR bits, and set the Interrupt Disable bit,
	// to prevent the function from initiating new transactions.
	uint16_t command = extendedConfigRead16(kIOPCIConfigCommand);
	command &= ~(kIOPCICommandBusLead | kIOPCICommandSERR);
	command |= kIOPCICommandInterruptDisable;
	extendedConfigWrite16(kIOPCIConfigCommand, command);

	// Poll the transactions pending bit for up to 50ms
	const uint32_t tpTimeoutMs = 50;
	AbsoluteTime deadline, now = 0;
	uint16_t deviceStatus = 0;

	clock_interval_to_deadline(tpTimeoutMs, kMillisecondScale, &deadline);
	do
	{
		deviceStatus = extendedConfigRead8(reserved->expressCapability + 0x0A);
		IOSleep(2);
		clock_get_uptime(&now);
	}
	while (    (deviceStatus & (1 << 5))
			&& (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline)));
}

void IOPCIDevice::flr(void)
{
	// Initiate FLR
	uint16_t control = extendedConfigRead16(reserved->expressCapability + 0x08);
	control |= (1 << 15);
	extendedConfigWrite16(reserved->expressCapability + 0x08, control);
}

void IOPCIDevice::completeFLR(void)
{
	// Wait 100ms
	IOSleep(100);
}

IOReturn IOPCIDevice::resetFunction(tIOPCIDeviceResetOptions options)
{
	if (!supportsFLR())
	{
		DLOG("[%s()] Function %u:%u:%u does not support FLR\n", __func__, PCI_ADDRESS_TUPLE(this));
		return kIOReturnUnsupported;
	}

	// Save device state
	if (!(options & kIOPCIDeviceResetOptionTerminate))
	{
		parent->saveDeviceState(this, kIOPCIConfigShadowVolatile);
	}

	prepareFLR();
	flr();
	completeFLR();

	// Restore device state
	if (!(options & kIOPCIDeviceResetOptionTerminate))
	{
		parent->restoreDeviceState(this, 0);
	}

	if (options & kIOPCIDeviceResetOptionTerminate)
	{
		terminate(kIOServiceTerminateNeedWillTerminate);

		launchReprobeThread();
	}

	return kIOReturnSuccess;
}

IOReturn IOPCIDevice::reset(tIOPCIDeviceResetTypes type, tIOPCIDeviceResetOptions options)
{
    DLOG("%s[%p]::%s(0x%x, 0x%x)\n", getName(), this, __func__, type, options);

	if (type == kIOPCIDeviceResetTypeFunctionReset)
	{
		// FLR is different from conventional reset and can be
		// handled almost entirely in IOPCIDevice.
		return resetFunction(options);
	}

	return parent->resetDevice(type, options);
}

IOReturn IOPCIDevice::reprobeThreadCall(thread_call_t threadCall)
{
    IOPCIDevice* bridgeDevice = OSDynamicCast(IOPCIDevice, parent->getParentEntry(gIOServicePlane));
    if (bridgeDevice != NULL)
    {
        DLOG("%s waiting for downstream devices to finish terminating\n", __PRETTY_FUNCTION__);
        // wait for the drivers and termination to settle
        IOReturn ret = parent->waitQuiet(60ULL * kSecondScale);
		if (ret == kIOReturnTimeout)
		{
			OSIterator* childIterator = parent->getChildIterator(gIOServicePlane);
			IOService* child = NULL;
			while (childIterator && (child = OSDynamicCast(IOService, childIterator->getNextObject())))
			{
				IOLog("%s child %s did not complete termination\n", __PRETTY_FUNCTION__, child->getName());
			}
			OSSafeReleaseNULL(childIterator);

			if (!PE_i_can_has_debugger(nullptr))
			{
				IOLog("%s waitQuiet() timed out waiting for downstream devices to finish terminating\n", __PRETTY_FUNCTION__);
			}
			else
			{
				panic("%s waitQuiet() timed out waiting for downstream devices to finish terminating", __PRETTY_FUNCTION__);
			}
		}

        DLOG("%s reprobing bus\n", __PRETTY_FUNCTION__);
        // re-scan the bridge for this device and its functions
        parent->busProbe(bridgeDevice, kIOPCIProbeOptionNeedsScan | kIOPCIProbeOptionDone);
    }

    // clean up threadcall
    parent->release();
    release();
    thread_call_free(threadCall);

    return kIOReturnSuccess;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if TARGET_OS_HAS_PCIDRIVERKIT_IOPCIDEVICE

#pragma mark Public DriverKit Methods

kern_return_t IOPCIDevice::SetProperties_Impl(OSDictionary* properties)
{
    kern_return_t result = kIOReturnBadArgument;

    if(properties == NULL)
    {
        return result;
    }

    OSObject* dictionaryValue = properties->getObject(kIOPMPCIConfigSpaceVolatileKey);
    if(   (dictionaryValue == kOSBooleanTrue)
       || (dictionaryValue == kOSBooleanFalse))
    {
        DLOG("%s setting property %s to device %u:%u:%u\n", __PRETTY_FUNCTION__, kIOPMPCIConfigSpaceVolatileKey, PCI_ADDRESS_TUPLE(this));
        result = kIOReturnSuccess;
        setProperty(kIOPMPCIConfigSpaceVolatileKey, dictionaryValue);
    }

    dictionaryValue = properties->getObject(kIOPMPCISleepLinkDisableKey);
    if(   (dictionaryValue == kOSBooleanTrue)
       || (dictionaryValue == kOSBooleanFalse))
    {
        DLOG("%s setting property %s to device %u:%u:%u\n", __PRETTY_FUNCTION__, kIOPMPCISleepLinkDisableKey, PCI_ADDRESS_TUPLE(this));
        result = kIOReturnSuccess;
        setProperty(kIOPMPCISleepLinkDisableKey, dictionaryValue);
    }

    dictionaryValue = properties->getObject(kIOPMPCISleepResetKey);
    if(   (dictionaryValue == kOSBooleanTrue)
       || (dictionaryValue == kOSBooleanFalse))
    {
        DLOG("%s setting property %s to device %u:%u:%u\n", __PRETTY_FUNCTION__, kIOPMPCISleepResetKey, PCI_ADDRESS_TUPLE(this));
        result = kIOReturnSuccess;
        setProperty(kIOPMPCISleepResetKey, dictionaryValue);
    }

    return result;
}

#pragma mark Private DriverKit Methods
kern_return_t
IMPL(IOPCIDevice, _ManageSession)
{
    IOReturn result = kIOReturnNotOpen;

    if(openClient == true)
    {
        // If the nub is published during sleep entry, xnu will defer dext matching.
        // Ensure the nub is powered on before allowing the client to proceed (with
        // a 10s timeout).
        const uint32_t powerTimeout = 10;
        AbsoluteTime deadline, now = 0;

        clock_interval_to_deadline(powerTimeout, kSecondScale, &deadline);

        while (   (atomic_load((atomic_char*)&reserved->pciPMState) != kIOPCIDeviceOnState)
               && (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline)))
        {
            if (isInactive())
            {
                return kIOReturnError;
            }
            DLOG("[%s()] Device %s is not powered, sleeping 100ms\n", __func__, getName());
            IOSleepWithLeeway(100, 10);
            clock_get_uptime(&now);
        }

        if(open(forClient, (openOptions & (~kIOServiceFamilyOpenOptions)) | kIOPCISessionOptionDriverkit, NULL) == true)
        {
            result = kIOReturnSuccess;
        }
    }
    else
    {
        close(forClient);
        result = kIOReturnSuccess;
    }


    return result;
}

kern_return_t IOPCIDevice::ClientCrashed_Impl(IOService *client, uint64_t options)
{
	DLOG("IOPCIDevice::ClientCrashed_Impl() for client %s\n", (client) ? client->getName() : "unknown");

    // only reset the device if the driver potentially changed the state of the device
    if(isOpen(client) == true)
    {
        IOLog("%s: PCIDriverKit client, %s, crashed for device %s[%u:%u:%u], attempting to recover\n",
              __PRETTY_FUNCTION__,
              (client != NULL) ? client->getName() : "unknown",
              getName(),
              PCI_ADDRESS_TUPLE(this));

        IOReturn ret = parent->childClientCrashRecovery(this);

		// rdar://122664049 (Remove terminate-on-crash option from PCIDriverKit crash recovery)
		if (ret == kIOReturnSuccess && getProperty("terminate-on-crash"))
		{
			launchReprobeThread();
		}
    }
	else
	{
        IOLog("%s: PCIDriverKit client %s does not have open session with device %s[%u:%u:%u], skipping recovery\n",
              __PRETTY_FUNCTION__,
              (client != NULL) ? client->getName() : "unknown",
              getName(),
              PCI_ADDRESS_TUPLE(this));
	}

    DLOG("IOPCIDevice::ClientCrashed_Impl() for client %s done\n", (client) ? client->getName() : "unknown");

    return kIOReturnSuccess;
}

kern_return_t
IMPL(IOPCIDevice, _MemoryAccess)
{
    if ((forClient == NULL) || (isOpen(forClient) == false))
    {
        DLOG("IOPCIDevice::%s: device not open for client %s\n", __FUNCTION__, (forClient != NULL) ? forClient->getName() : "unknown client");
        return kIOReturnNotOpen;
    }

	uint8_t memoryIndex = operation & kPCIDriverKitMemoryAccessOperationDeviceMemoryIndexMask;
	if(memoryIndex > kIOPCIRangeExpansionROM)
	{
		DLOG("IOPCIDevice::%s: invalid index %u for client %s\n", __FUNCTION__, memoryIndex, (forClient != NULL) ? forClient->getName() : "unknown client");
		return kIOReturnBadArgument;
	}

    IOReturn  result = kIOReturnSuccess;

#if TARGET_CPU_X86 || TARGET_CPU_X86_64
    if((operation & (kPCIDriverKitMemoryAccessOperationIORead | kPCIDriverKitMemoryAccessOperationIOWrite)) != 0)
    {
        uint16_t ioSpaceOffset = 0;

        if(os_convert_overflow(offset, &ioSpaceOffset) == true)
        {
            result = kIOReturnBadArgument;
            DLOG("%s::%s bad offset 0x%llx\n", "IOPCIDevice", __FUNCTION__, offset);
        }


        IOMemoryDescriptor* memoryDescriptor = reserved->deviceMemory[memoryIndex];
        if (   (memoryDescriptor != NULL)
            && (ioMap != NULL))
        {
            IOPCIAddressSpace addressSpace;
            addressSpace.bits = static_cast<uint32_t>(memoryDescriptor->getTag());

            IOPhysicalAddress ioSpaceStartAddress = ioMap->getPhysicalAddress();
            IOPhysicalAddress ioSpaceBarAddress   = memoryDescriptor->getPhysicalAddress();
            IOByteCount       accessSize          = 0;

            // take into account the size of the access so we're not going over the the Bar region
            switch (operation & kPCIDriverKitMemoryAccessOperationSizeMask)
            {
                case kPCIDriverKitMemoryAccessOperation8Bit:
                {
                    accessSize = sizeof(uint8_t);
                    break;
                }
                case kPCIDriverKitMemoryAccessOperation16Bit:
                {
                    accessSize = sizeof(uint16_t);
                    break;
                }
                case kPCIDriverKitMemoryAccessOperation32Bit:
                {
                    accessSize = sizeof(uint32_t);
                    break;
                }
                default:
                {
                    DLOG("%s::%s bad request with memeoryIndex %u, offset 0x%x, operation 0x%llx\n", "IOPCIDevice", __FUNCTION__, memoryIndex, ioSpaceOffset, operation);
                    break;
                }
            }
            if(   (addressSpace.s.space == kIOPCIIOSpace)
               && (accessSize > 0)
               && ((ioSpaceOffset + accessSize) <= memoryDescriptor->getLength())
               && (ioSpaceStartAddress <= ioSpaceBarAddress))
            {
                // I/O space is physically contiguous
                // The I/O accessor methods use offsets from the beginning of I/O space, so the total offset needs to be
                // calculated
                ioSpaceOffset += static_cast<uint16_t>(ioSpaceBarAddress - ioSpaceStartAddress);
            }
            else
            {
                result = kIOReturnBadArgument;
                DLOG("%s::%s bad request with memeoryIndex %u, offset 0x%x\n", "IOPCIDevice", __FUNCTION__, memoryIndex, ioSpaceOffset);
            }
            memoryDescriptor = NULL;
        }
        else
        {
            result = kIOReturnBadArgument;
        }

        if (result == kIOReturnSuccess)
        {
            switch (operation & (~kPCIDriverKitMemoryAccessOperationDeviceMemoryIndexMask))
            {
                case kPCIDriverKitMemoryAccessOperationIORead | kPCIDriverKitMemoryAccessOperation32Bit:
                {
                    *readData = ioRead32(ioSpaceOffset);
                    break;
                }
                case kPCIDriverKitMemoryAccessOperationIORead | kPCIDriverKitMemoryAccessOperation16Bit:
                {
                    *readData = ioRead16(ioSpaceOffset);
                    break;
                }
                case kPCIDriverKitMemoryAccessOperationIORead | kPCIDriverKitMemoryAccessOperation8Bit:
                {
                    *readData = ioRead8(ioSpaceOffset);
                    break;
                }
                case kPCIDriverKitMemoryAccessOperationIOWrite | kPCIDriverKitMemoryAccessOperation32Bit:
                {
                    ioWrite32(ioSpaceOffset, static_cast<uint32_t>(data));
                    break;
                }
                case kPCIDriverKitMemoryAccessOperationIOWrite | kPCIDriverKitMemoryAccessOperation16Bit:
                {
                    ioWrite16(ioSpaceOffset, static_cast<uint16_t>(data));
                    break;
                }
                case kPCIDriverKitMemoryAccessOperationIOWrite | kPCIDriverKitMemoryAccessOperation8Bit:
                {
                    ioWrite8(ioSpaceOffset,  static_cast<uint8_t>(data));
                    break;
                }
                default:
                {
                    DLOG("%s::%s bad request with memeoryIndex %u, offset 0x%x, operation 0x%llx\n",
                         "IOPCIDevice",
                         __FUNCTION__,
                         memoryIndex,
                         ioSpaceOffset,
                         operation);
                    result = kIOReturnUnsupported;
                    break;
                }
            }
        }
    }
    else
#endif
    {
        switch (operation & (~kPCIDriverKitMemoryAccessOperationDeviceMemoryIndexMask))
        {
            case kPCIDriverKitMemoryAccessOperationConfigurationRead | kPCIDriverKitMemoryAccessOperation32Bit:
            {
                *readData = extendedConfigRead32(offset);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationConfigurationRead | kPCIDriverKitMemoryAccessOperation16Bit:
            {
                *readData = extendedConfigRead16(offset);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationConfigurationRead | kPCIDriverKitMemoryAccessOperation8Bit:
            {
                *readData = extendedConfigRead8(offset);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationConfigurationWrite | kPCIDriverKitMemoryAccessOperation32Bit:
            {
                extendedConfigWrite32(offset, static_cast<uint32_t>(data));
                break;
            }
            case kPCIDriverKitMemoryAccessOperationConfigurationWrite | kPCIDriverKitMemoryAccessOperation16Bit:
            {
                extendedConfigWrite16(offset, static_cast<uint16_t>(data));
                break;
            }
            case kPCIDriverKitMemoryAccessOperationConfigurationWrite | kPCIDriverKitMemoryAccessOperation8Bit:
            {
                extendedConfigWrite8(offset, static_cast<uint8_t>(data));
                break;
            }
            case kPCIDriverKitMemoryAccessOperationDeviceRead | kPCIDriverKitMemoryAccessOperation64Bit:
            {
                result = deviceMemoryRead(memoryIndex, offset, static_cast<void *>(readData), sizeof(uint64_t), options);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationDeviceRead | kPCIDriverKitMemoryAccessOperation32Bit:
            {
                result = deviceMemoryRead(memoryIndex, offset, static_cast<void *>(readData), sizeof(uint32_t), options);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationDeviceRead | kPCIDriverKitMemoryAccessOperation16Bit:
            {
                result = deviceMemoryRead(memoryIndex, offset, static_cast<void *>(readData), sizeof(uint16_t), options);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationDeviceRead | kPCIDriverKitMemoryAccessOperation8Bit:
            {
                result = deviceMemoryRead(memoryIndex, offset, static_cast<void *>(readData), sizeof(uint8_t), options);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationDeviceWrite | kPCIDriverKitMemoryAccessOperation64Bit:
            {
                result = deviceMemoryWrite(memoryIndex, offset, static_cast<uint64_t>(data), sizeof(uint64_t), options);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationDeviceWrite | kPCIDriverKitMemoryAccessOperation32Bit:
            {
                result = deviceMemoryWrite(memoryIndex, offset, static_cast<uint32_t>(data), sizeof(uint32_t), options);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationDeviceWrite | kPCIDriverKitMemoryAccessOperation16Bit:
            {
                result = deviceMemoryWrite(memoryIndex, offset, static_cast<uint16_t>(data), sizeof(uint16_t), options);
                break;
            }
            case kPCIDriverKitMemoryAccessOperationDeviceWrite | kPCIDriverKitMemoryAccessOperation8Bit:
            {
                result = deviceMemoryWrite(memoryIndex, offset, static_cast<uint8_t>(data), sizeof(uint8_t), options);
                break;
            }
            default:
            {
                result = kIOReturnUnsupported;
                break;
            }
        }
    }

    return result;
}

kern_return_t
IMPL(IOPCIDevice, _CopyDeviceMemoryWithIndex)
{
    if ((forClient == NULL) || (isOpen(forClient) == false))
    {
        DLOG("IOPCIDevice::%s: device not open for client %s\n", __FUNCTION__, (forClient != NULL) ? forClient->getName() : "unknown client");
        return kIOReturnNotOpen;
    }

    if(memoryIndex > kIOPCIRangeExpansionROM)
    {
        DLOG("IOPCIDevice::%s: invalid index %llu for client %s\n", __FUNCTION__, memoryIndex, (forClient != NULL) ? forClient->getName() : "unknown client");
        return kIOReturnBadArgument;
    }

#if TARGET_CPU_ARM || TARGET_CPU_ARM64
    if (reserved->offloadEngineMMIODisable == 0)
    {
        DLOG("IOPCIDevice::%s: offload engine requires using _MemoryAccess()\n", __FUNCTION__);
        return kIOReturnError;
    }
#endif

    if (getProperty("no-user-memory-mapping"))
    {
#if TARGET_CPU_X86 || TARGET_CPU_X86_64
		panic("no-user-memory-mapping unsupported on x86");
#endif
        DLOG("IOPCIDevice::%s: no-user-memory-mapping property prevents copying device memory\n", __FUNCTION__);
        return kIOReturnError;
    }

    IOReturn result                       = kIOReturnUnsupported;
    IOMemoryDescriptor * memoryDescriptor = reserved->deviceMemory[memoryIndex];
    OSArray*  deviceMemoryArray           = NULL;
    OSObject* deviceMemoryObject          = copyProperty(gIODeviceMemoryKey);

    if((deviceMemoryArray = OSDynamicCast(OSArray, deviceMemoryObject)) != NULL)
    {
        if(memoryIndex <= deviceMemoryArray->getCount())
        {
            memoryDescriptor = OSDynamicCast(IOMemoryDescriptor, deviceMemoryArray->getObject(static_cast<uint32_t>(memoryIndex)));
        }

        if (memoryDescriptor != NULL)
        {
            // since we're bypassing getDeviceMemoryWithIndex, implement our subclass implementation as well.
            // Make sure L1 is not set
            if (kTunnelL1NotSet == reserved->tunnelL1Allow) setTunnelL1Enable(this, false);

            IOPCIAddressSpace addressSpace;
            addressSpace.bits = static_cast<uint32_t>(memoryDescriptor->getTag());

            if(addressSpace.s.space == kIOPCIIOSpace)
            {
                memoryDescriptor = NULL;
                *returnMemory    = NULL;
            }
            else
            {
                memoryDescriptor->retain();
                *returnMemory = memoryDescriptor;
                result = kIOReturnSuccess;
            }
        }
    }

    OSSafeReleaseNULL(deviceMemoryObject);

    return result;
}

#pragma mark Configuration Space helpers

kern_return_t
IMPL(IOPCIDevice, FindPCICapability)
{
    IOByteCount _foundCapabilityOffset = static_cast<IOByteCount>(searchOffset);
    uint32_t offset = 0;

    offset = extendedFindPCICapability(capabilityID, &_foundCapabilityOffset);
    *foundCapabilityOffset = _foundCapabilityOffset;

    return (offset != 0) ? kIOReturnSuccess : kIOReturnNotFound;
}

kern_return_t
IMPL(IOPCIDevice, GetBusDeviceFunction)
{
    *returnBusNumber      = getBusNumber();
    *returnDeviceNumber   = getDeviceNumber();
    *returnFunctionNumber = getFunctionNumber();
    return kIOReturnSuccess;
}

#pragma mark Power Management

kern_return_t
IMPL(IOPCIDevice, HasPCIPowerManagement)
{
    return hasPCIPowerManagement(static_cast<IOOptionBits>(state)) ? kIOReturnSuccess : kIOReturnUnsupported;
}

kern_return_t
IMPL(IOPCIDevice, EnablePCIPowerManagement)
{
    return enablePCIPowerManagement(static_cast<IOOptionBits>(state));
}

#pragma mark State Management

kern_return_t
IMPL(IOPCIDevice, SaveDeviceState)
{
    return saveDeviceState(options);
}

kern_return_t
IMPL(IOPCIDevice, RestoreDeviceState)
{
    return restoreDeviceState(options);
}

#pragma mark Memory Accessor helpers

kern_return_t
IMPL(IOPCIDevice, GetBARInfo)
{
    IOPCIConfigEntry *configEntry = reserved->configEntry;
    IOReturn result = kIOReturnNotFound;
    OSArray *deviceMemoryArray  = NULL;

    if (barIndex > kIOPCIRangeExpansionROM)
    {
        DLOG("IOPCIDevice::%s:%s invalid bar index %u\n", __FUNCTION__, getName(), barIndex);
        return kIOReturnBadArgument;
    }

    if ((deviceMemoryArray = OSDynamicCast(OSArray, getProperty(gIODeviceMemoryKey))) == NULL)
    {
        DLOG("IOPCIDevice::%s:%s unable to access device memory\n", __FUNCTION__, getName());
        return kIOReturnError;
    }

    if (configEntry == NULL)
    {
        DLOG("IOPCIDevice::%s:%s NULL config entry\n", __FUNCTION__, getName());
        return kIOReturnError;
    }

    for (unsigned int i = 0; i < deviceMemoryArray->getCount(); i++)
    {
        static const uint8_t barRegisters[kIOPCIRangeExpansionROM + 1] = {
            kIOPCIConfigBaseAddress0, kIOPCIConfigBaseAddress1, kIOPCIConfigBaseAddress2,
            kIOPCIConfigBaseAddress3, kIOPCIConfigBaseAddress4, kIOPCIConfigBaseAddress5,
            kIOPCIConfigExpansionROMBase
        };

        IOMemoryDescriptor *memoryDescriptor = OSDynamicCast(IOMemoryDescriptor, deviceMemoryArray->getObject(static_cast<uint32_t>(i)));

        if (memoryDescriptor == NULL)
        {
            continue;
        }

        IOPCIAddressSpace addressSpace;
        addressSpace.bits = static_cast<uint32_t>(memoryDescriptor->getTag());

        if (addressSpace.s.registerNum == barRegisters[barIndex])
        {
            IOPCIRange *range = configEntry->ranges[barIndex];

            *memoryIndex = i;
            *barSize = range->size;

            if (addressSpace.s.space == kIOPCIIOSpace)
            {
                *barType = kPCIBARTypeIO;
            }
            else if (range->flags & kIOPCIRangeFlagBar64)
            {
                *barType = (addressSpace.s.prefetch) ? kPCIBARTypeM64PF : kPCIBARTypeM64;
            }
            else
            {
                *barType = (addressSpace.s.prefetch) ? kPCIBARTypeM32PF : kPCIBARTypeM32;
            }

            result = kIOReturnSuccess;
            break;
        }
    }

    return result;
}

#pragma mark Link Management

kern_return_t
IMPL(IOPCIDevice, SetLinkSpeed)
{
    tIOPCILinkSpeed speed = static_cast<tIOPCILinkSpeed>(linkSpeed);
    if (speed < kIOPCILinkSpeed_2_5_GTs || speed > kIOPCILinkSpeed_32_GTs)
    {
        return kIOReturnBadArgument;
    }

    return setLinkSpeed(speed, retrain);
}

kern_return_t
IMPL(IOPCIDevice, GetLinkSpeed)
{
    tIOPCILinkSpeed speed = kIOPCILinkSpeed_2_5_GTs;

    IOReturn ret = getLinkSpeed(&speed);

    *linkSpeed = static_cast<IOPCILinkSpeed>(speed);

    return ret;
}

#pragma mark Interrupts Allocation

kern_return_t
IMPL(IOPCIDevice, ConfigureInterrupts)
{
    return configureInterrupts(interruptType, numRequired, numRequested, options);
}

#pragma mark Reset

kern_return_t
IMPL(IOPCIDevice, Reset)
{
    return reset(static_cast<tIOPCIDeviceResetTypes>(type), static_cast<tIOPCIDeviceResetOptions>(options));
}

kern_return_t
IMPL(IOPCIDevice, SetASPMState)
{
	/* 'client' argument is unused */
    return setASPMState(NULL, aspmLinkControl & 0x3);
}

#endif

void IOPCIDevice::registerCrashNotification(IOPCIDeviceCrashNotification_t handler, void *ref)
{
	reserved->crashNotification = handler;
	reserved->crashNotificationRef = ref;
}

void IOPCIDevice::unregisterCrashNotification(void)
{
	reserved->crashNotification = NULL;
	reserved->crashNotificationRef = NULL;
}

bool IOPCIDevice::supportsFLR(void)
{
	return !!(reserved->expressDeviceCapabilities & (1 << 28));
}

bool IOPCIDevice::isDownstreamFacing(void)
{
	uint8_t portType = (reserved->expressCapabilities >> 4) & 0xF;

	return (   (portType == 4)   // Root port
			|| (portType == 6)); // Switch downstream port
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOPCIDevice

OSDefineMetaClassAndStructors(IOAGPDevice, IOPCIDevice)
OSMetaClassDefineReservedUnused(IOAGPDevice,  0);
OSMetaClassDefineReservedUnused(IOAGPDevice,  1);
OSMetaClassDefineReservedUnused(IOAGPDevice,  2);
OSMetaClassDefineReservedUnused(IOAGPDevice,  3);
OSMetaClassDefineReservedUnused(IOAGPDevice,  4);
OSMetaClassDefineReservedUnused(IOAGPDevice,  5);
OSMetaClassDefineReservedUnused(IOAGPDevice,  6);
OSMetaClassDefineReservedUnused(IOAGPDevice,  7);
OSMetaClassDefineReservedUnused(IOAGPDevice,  8);
OSMetaClassDefineReservedUnused(IOAGPDevice,  9);
OSMetaClassDefineReservedUnused(IOAGPDevice, 10);
OSMetaClassDefineReservedUnused(IOAGPDevice, 11);
OSMetaClassDefineReservedUnused(IOAGPDevice, 12);
OSMetaClassDefineReservedUnused(IOAGPDevice, 13);
OSMetaClassDefineReservedUnused(IOAGPDevice, 14);
OSMetaClassDefineReservedUnused(IOAGPDevice, 15);
OSMetaClassDefineReservedUnused(IOAGPDevice, 16);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOAGPDevice::createAGPSpace( IOOptionBits options,
                                      IOPhysicalAddress * address,
                                      IOPhysicalLength * length )
{
    return (parent->createAGPSpace(this, options, address, length));
}

IOReturn IOAGPDevice::destroyAGPSpace( void )
{
    return (parent->destroyAGPSpace(this));
}

IORangeAllocator * IOAGPDevice::getAGPRangeAllocator( void )
{
    return (parent->getAGPRangeAllocator(this));
}

IOOptionBits IOAGPDevice::getAGPStatus( IOOptionBits options )
{
    return (parent->getAGPStatus(this, options));
}

IOReturn IOAGPDevice::resetAGP( IOOptionBits options )
{
    return (parent->resetAGPDevice(this, options));
}

IOReturn IOAGPDevice::getAGPSpace( IOPhysicalAddress * address,
                                   IOPhysicalLength * length )
{
    return (parent->getAGPSpace(this, address, length));
}

IOReturn IOAGPDevice::commitAGPMemory(  IOMemoryDescriptor * memory,
                                        IOByteCount agpOffset,
                                        IOOptionBits options )
{
    return (parent->commitAGPMemory(this, memory, agpOffset, options));
}

IOReturn IOAGPDevice::releaseAGPMemory( IOMemoryDescriptor * memory,
                                        IOByteCount agpOffset,
                                        IOOptionBits options )
{
    return (parent->releaseAGPMemory(this, memory, agpOffset, options));
}
