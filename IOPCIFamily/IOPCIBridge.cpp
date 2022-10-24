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

#include <IOKit/system.h>
#include <IOKit/IOCommandGate.h>

#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOAGPDevice.h>
#include <IOKit/pci/IOPCIConfigurator.h>
#if ACPI_SUPPORT
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include "AppleVTD.h"
#elif TARGET_CPU_ARM64 || TARGET_CPU_ARM
#include <IOKit/dart/IODARTKeys.h>
#include <IOKit/IOMapper.h>
#endif

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOSubMemoryDescriptor.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/pwr_mgt/IOPowerConnection.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/assert.h>
#include <IOKit/IOCatalogue.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOPolledInterface.h>
#include <IOKit/IOUserClient.h>

#include <libkern/c++/OSContainers.h>
#include <libkern/OSKextLib.h>

#define TARGET_OS_HAS_THUNDERBOLT __has_include(<IOKit/thunderbolt/IOThunderboltPort.h>)
#if TARGET_OS_HAS_THUNDERBOLT
#include <IOKit/thunderbolt/IOThunderboltPort.h>
#endif

extern "C"
{
#include <machine/machine_routines.h>
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef kIOPolledInterfaceActiveKey
#define kIOPolledInterfaceActiveKey  "IOPolledInterfaceActive"
#endif

// #define DEADTEST		"UPS0"
// #define DEFERTEST	1

enum { kAERISRNum     = 4 };
enum { kIOPCIEventNum = 8 };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__exported_push
__kpi_unavailable const OSSymbol *		   gIOPCITunnelIDKey;
__kpi_unavailable const OSSymbol *           gIOPCITunnelControllerKey;
__kpi_unavailable const OSSymbol *		   gIOPCITunnelledKey;
__kpi_unavailable const OSSymbol *		   gIOPCIHPTypeKey;
__kpi_unavailable const OSSymbol *		   gIOPCIThunderboltKey;
__kpi_unavailable const OSSymbol *		   gIOPCIHotplugCapableKey;
__kpi_unavailable const OSSymbol *		   gIOPCITunnelL1EnableKey;

__kpi_unavailable const OSSymbol *           gIOPlatformDeviceMessageKey;
__kpi_unavailable const OSSymbol *           gIOPlatformDeviceASPMEnableKey;
__kpi_unavailable const OSSymbol *           gIOPlatformSetDeviceInterruptsKey;
__kpi_unavailable const OSSymbol *           gIOPlatformResolvePCIInterruptKey;
__kpi_unavailable const OSSymbol *           gIOPlatformFreeDeviceResourcesKey;
__kpi_unavailable const OSSymbol *           gIOPlatformGetMessagedInterruptControllerKey;
__kpi_unavailable const OSSymbol *           gIOPlatformGetMessagedInterruptAddressKey;
__kpi_unavailable const OSSymbol *           gIOPlatformDeviceRelocatedKey;
__kpi_unavailable const OSSymbol *           gIOPolledInterfaceActiveKey;
__kpi_unavailable const OSSymbol *           gIOPCIDeviceHiddenKey;
__kpi_unavailable const OSSymbol *           gIOPCIDeviceChangedKey;
__exported_pop

uint32_t gIOPCIFlags = 0
             | 0*kIOPCIConfiguratorFPBEnable
             | kIOPCIConfiguratorPFM64
             | kIOPCIConfiguratorCheckTunnel
             | kIOPCIConfiguratorTBMSIEnable
#if ACPI_SUPPORT
             | 0*kIOPCIConfiguratorDeviceMap
#else
             | kIOPCIConfiguratorAER
#endif
//           | kIOPCIConfiguratorTBUSBCPanics
//           | kIOPCIConfiguratorDeepIdle
//           | kIOPCIConfiguratorNoSplay
//			 | kIOPCIConfiguratorNoTB
//           | kIOPCIConfiguratorIOLog | kIOPCIConfiguratorKPrintf
;

#define DLOG(fmt, args...)                   \
    do {                                                    \
        if ((gIOPCIFlags & kIOPCIConfiguratorIOLog) && !ml_at_interrupt_context())   \
            IOLog(fmt, ## args);                            \
        if (gIOPCIFlags & kIOPCIConfiguratorKPrintf)        \
            kprintf(fmt, ## args);                          \
    } while(0)

#if ACPI_SUPPORT
extern IOPCIHostBridgeData *gBridgeData;
#endif
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum
{
	// data link change, hot plug, presence detect change
	kSlotControlEnables = ((1 << 12) | (1 << 5) | (1 << 3) | (1 << 0))
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct IOPCIAERISREntry
{
    uint32_t source;
    uint32_t status;
};

struct IOPCIAERRoot
{
    IOPCIAERISREntry * fISRErrors;
	uint8_t            fAERReadIndex;
	uint8_t            fAERWriteIndex;
};

#undef super

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define super IOService
OSDefineMetaClassAndStructors(IOPCIHostBridgeData, super);

bool IOPCIHostBridgeData::init(void)
{
    uint32_t debug;

    _wakeCount = 0x100000001ULL;

    _allPCI2PCIBridgeState = 0;
    _allPCI2PCIBridgesLock = IOSimpleLockAlloc();
    _eventSourceLock = IOSimpleLockAlloc();
    queue_init(&_allPCIDeviceRestoreQ);
    queue_init(&_eventSourceQueue);
    _wakeReasonLock = IOLockAlloc();

    _configWorkLoop = IOWorkLoop::workLoop();
    thread_set_thread_name(_configWorkLoop->getThread(), "IOPCIConfigurator");

    _waitingPauseSet = OSSet::withCapacity(4);
    _pausedSet       = OSSet::withCapacity(4);
    _probeSet        = OSSet::withCapacity(4);

    _isUSBCSystem = false;
    _tunnelSleep  = 0;
    _tunnelWait   = 0;
#if ACPI_SUPPORT
    _vtdInterruptsInstalled = false;
#else
    _powerChildrenLock = IOLockAlloc();
    if (_powerChildrenLock == NULL)
    {
        panic("Cannot allocate _powerChildrenLock");
    }
    _powerChildren = OSSet::withCapacity(20);
    if (_powerChildren == NULL) {
        panic("Cannot allocate _powerChildren");
    }
    _activePowerChildren = OSSet::withCapacity(20);
    if (_activePowerChildren == NULL) {
        panic ("Cannot allocate _activePowerChildren");
    }

#endif

	_aspmDefault = 0xFFFF;
	if (PE_parse_boot_argn("pci-aspm-default", &debug, sizeof(debug)))
		_aspmDefault = debug;

    IOService::getPMRootDomain()->registerInterest(gIOPriorityPowerStateInterest, &IOPCIHostBridgeData::systemPowerChange, 0, this);
    if (IOService::getPMRootDomain()->getProperty(kIOPMDeepIdleSupportedKey))
    {
        if (!(PE_parse_boot_argn("acpi", &debug, sizeof(debug)) && (0x10000 & debug)))
        {
            gIOPCIFlags |= kIOPCIConfiguratorDeepIdle;
        }
    }

    _configurator = OSTypeAlloc(IOPCIConfigurator);
    if (!_configurator || !_configurator->init(_configWorkLoop, gIOPCIFlags))
    {
        panic("!IOPCIConfigurator");
    }

    return super::init();
}

void IOPCIHostBridgeData::free(void)
{
    if (_allPCI2PCIBridgesLock)
    {
        IOSimpleLockFree(_allPCI2PCIBridgesLock);
        _allPCI2PCIBridgesLock = nullptr;
    }
    if (_eventSourceLock)
    {
        IOSimpleLockFree(_eventSourceLock);
        _eventSourceLock = nullptr;
    }
    if (_wakeReasonLock)
    {
        IOLockFree(_wakeReasonLock);
        _wakeReasonLock = nullptr;
    }
    OSSafeReleaseNULL(_configurator);
    OSSafeReleaseNULL(_configWorkLoop);
    OSSafeReleaseNULL(_waitingPauseSet);
    OSSafeReleaseNULL(_pausedSet);
    OSSafeReleaseNULL(_probeSet);

#if !ACPI_SUPPORT
    OSSafeReleaseNULL(_powerChildren);
    OSSafeReleaseNULL(_activePowerChildren);
    IOLockFree(_powerChildrenLock);
#endif

    super::free();
}

void IOPCIHostBridgeData::lockWakeReasonLock(void)
{
    IOLockLock(_wakeReasonLock);
}

void IOPCIHostBridgeData::unlockWakeReasonLock(void)
{
    IOLockUnlock(_wakeReasonLock);
}

void IOPCIHostBridgeData::tunnelSleepIncrement(const char * deviceName, bool increment)
{
    lockWakeReasonLock();
    if (increment)
    {
        _tunnelSleep++;
        DLOG("%s: tunnel sleep %d\n", deviceName, _tunnelSleep);
    }
    else
    {
        DLOG("%s: tunnel wake %d\n", deviceName, _tunnelSleep);
        _tunnelSleep--;
        if (_tunnelWait && !--_tunnelWait)
        {
            IOLockWakeup(_wakeReasonLock, &_tunnelWait, false);
        }
    }
    unlockWakeReasonLock();
}

void IOPCIHostBridgeData::tunnelsWait(IOPCIDevice * device)
{
    lockWakeReasonLock();
    DLOG("%s: tunnel stall(%d, %d)\n", device->getName(), _tunnelWait, _tunnelSleep);
    if (_tunnelWait)
    {
        IOLockSleep(_wakeReasonLock, &_tunnelWait, THREAD_UNINT);
        DLOG("%s: tunnels done\n", device->getName());
    }
    unlockWakeReasonLock();
}

IOReturn IOPCIHostBridgeData::addPCIEPowerChild(IOService *theChild)
{
#if !ACPI_SUPPORT
    IOPCIDevice *childDevice = OSDynamicCast(IOPCIDevice, theChild);
    IOPCIBridge *childBridge = OSDynamicCast(IOPCIBridge, theChild);

    if (childBridge || childDevice)
    {
        IOLockLock(_powerChildrenLock);
        _powerChildren->setObject(theChild);
        _activePowerChildren->setObject(theChild);
        IOLockUnlock(_powerChildrenLock);

        theChild->registerInterestedDriver(this);

        DLOG("%s: %p %s (%s): now we have %d/%d children\n", __PRETTY_FUNCTION__, theChild, theChild->getName(),
              childBridge ? "bridge" : "device",
              _powerChildren->getCount(), _activePowerChildren->getCount());
    }
    else
    {
        DLOG("%s: %p %s: nis not PCIe device or bridge, so no monitoring is intended\n", __PRETTY_FUNCTION__, theChild, theChild->getName());
    }
#endif
    return kIOReturnSuccess;
}

IOReturn IOPCIHostBridgeData::removePCIEPowerChild(IOPowerConnection *theChild)
{
#if !ACPI_SUPPORT
    IOService *childPower = OSDynamicCast(IOService, theChild->getChildEntry(gIOPowerPlane));
    if (childPower)
    {
        childPower->deRegisterInterestedDriver(this);
    }

    IOLockLock(_powerChildrenLock);
    _powerChildren->removeObject(childPower);
    _activePowerChildren->removeObject(childPower);
    IOLockUnlock(_powerChildrenLock);

    DLOG("%s: %p %s: now we have %d/%d children\n", __PRETTY_FUNCTION__, childPower, childPower->getName(), _powerChildren->getCount(), _activePowerChildren->getCount());
#endif
    return kIOReturnSuccess;
}

IOReturn IOPCIHostBridgeData::powerStateDidChangeTo(IOPMPowerFlags capabilities, unsigned long stateNumber, IOService *whatDevice)
{
#if !ACPI_SUPPORT
    IOLockLock(_powerChildrenLock);
    if (_powerChildren->member(whatDevice))
    {
        switch (stateNumber) {
            case kIOPCIDeviceOnState:
                _activePowerChildren->setObject(whatDevice);
                break;
            default:
                _activePowerChildren->removeObject(whatDevice);
                break;
        }
    }
    IOLockUnlock(_powerChildrenLock);
    DLOG("%s: %p %s -> state %lu: now we have %d/%d children\n", __PRETTY_FUNCTION__, whatDevice, whatDevice->getName(), stateNumber, _powerChildren->getCount(), _activePowerChildren->getCount());
#endif
    return super::powerStateDidChangeTo(capabilities, stateNumber, whatDevice);
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#undef super
#define super IOPCIBridge

OSDefineMetaClassAndAbstractStructors(IOPCIHostBridge, super);

IOService* IOPCIHostBridge::probe(IOService * provider, SInt32 *score)
{
    super::probe(provider, score);

#if ACPI_SUPPORT
    InitSharedBridgeData();
    bridgeData = gBridgeData;
    gBridgeData->retain();
    IOSimpleLockLock(gBridgeData->_allPCI2PCIBridgesLock);
    if (gBridgeData->_vtdInterruptsInstalled == false)
    {
        gBridgeData->_vtdInterruptsInstalled = true;
        AppleVTD::installInterrupts();
    }
    IOSimpleLockUnlock(gBridgeData->_allPCI2PCIBridgesLock);
#else
    bridgeData = OSTypeAlloc(IOPCIHostBridgeData);
    if (bridgeData == NULL || bridgeData->init() == false)
    {
        panic("Failed to initialize host bridge data structure");
        return false;
    }
#endif

    return this;
}

void IOPCIHostBridge::free(void)
{
    OSSafeReleaseNULL(bridgeData);
    super::free();
}

bool IOPCIHostBridge::configure(IOService * provider)
{
    reserved->hostBridgeData = bridgeData;
    reserved->hostBridgeData->retain();

    return super::configure(provider);
}

IOReturn IOPCI2PCIBridge::addPowerChild(IOService *theChild)
{
#if !ACPI_SUPPORT
    IOPCIHostBridgeData *vars = ((IOPCIBridge*)this)->reserved->hostBridgeData;
    vars->addPCIEPowerChild(theChild);
#endif
    return IOPCIBridge::addPowerChild(theChild);
}

IOReturn IOPCI2PCIBridge::removePowerChild(IOPowerConnection * theChild)
{
#if !ACPI_SUPPORT
    IOPCIHostBridgeData *vars = ((IOPCIBridge*)this)->reserved->hostBridgeData;
    vars->removePCIEPowerChild(theChild);
#endif
    return IOPCIBridge::removePowerChild(theChild);
}

bool IOPCIHostBridge::allChildrenPoweredOn(void)
{
#if ACPI_SUPPORT
    return true;
#else
    IOPCIHostBridgeData *vars = reserved->hostBridgeData;
    bool result = true;
    IOLockLock(vars->_powerChildrenLock);
    result = (vars->_powerChildren->getCount() == vars->_activePowerChildren->getCount());
    IOLockUnlock(vars->_powerChildrenLock);
    return result;
#endif
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService
OSDefineMetaClassAndAbstractStructorsWithInit( IOPCIBridge, IOService, IOPCIBridge::initialize() )

OSMetaClassDefineReservedUsed(IOPCIBridge, 0);
OSMetaClassDefineReservedUsed(IOPCIBridge, 1);
OSMetaClassDefineReservedUsed(IOPCIBridge, 2);
OSMetaClassDefineReservedUsed(IOPCIBridge, 3);
OSMetaClassDefineReservedUsed(IOPCIBridge, 4);
OSMetaClassDefineReservedUsed(IOPCIBridge, 5);
OSMetaClassDefineReservedUsed(IOPCIBridge,  6);
OSMetaClassDefineReservedUsed(IOPCIBridge,  7);
OSMetaClassDefineReservedUsed(IOPCIBridge,  8);
OSMetaClassDefineReservedUsed(IOPCIBridge,  9);
OSMetaClassDefineReservedUsed(IOPCIBridge, 10);
OSMetaClassDefineReservedUsed(IOPCIBridge, 11);
OSMetaClassDefineReservedUsed(IOPCIBridge, 12);
OSMetaClassDefineReservedUnused(IOPCIBridge, 13);
OSMetaClassDefineReservedUnused(IOPCIBridge, 14);
OSMetaClassDefineReservedUnused(IOPCIBridge, 15);
OSMetaClassDefineReservedUnused(IOPCIBridge, 16);
OSMetaClassDefineReservedUnused(IOPCIBridge, 17);
OSMetaClassDefineReservedUnused(IOPCIBridge, 18);
OSMetaClassDefineReservedUnused(IOPCIBridge, 19);
OSMetaClassDefineReservedUnused(IOPCIBridge, 20);
OSMetaClassDefineReservedUnused(IOPCIBridge, 21);
OSMetaClassDefineReservedUnused(IOPCIBridge, 22);
OSMetaClassDefineReservedUnused(IOPCIBridge, 23);
OSMetaClassDefineReservedUnused(IOPCIBridge, 24);
OSMetaClassDefineReservedUnused(IOPCIBridge, 25);
OSMetaClassDefineReservedUnused(IOPCIBridge, 26);
OSMetaClassDefineReservedUnused(IOPCIBridge, 27);
OSMetaClassDefineReservedUnused(IOPCIBridge, 28);
OSMetaClassDefineReservedUnused(IOPCIBridge, 29);
OSMetaClassDefineReservedUnused(IOPCIBridge, 30);
OSMetaClassDefineReservedUnused(IOPCIBridge, 31);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef kIOPlatformDeviceMessageKey
#define kIOPlatformDeviceMessageKey     			"IOPlatformDeviceMessage"
#endif

#ifndef kIOPlatformSetDeviceInterruptsKey
#define kIOPlatformSetDeviceInterruptsKey			"SetDeviceInterrupts"
#endif

#ifndef kIOPlatformResolvePCIInterruptKey
#define kIOPlatformResolvePCIInterruptKey			"ResolvePCIInterrupt"
#endif

#ifndef kIOPlatformFreeDeviceResourcesKey
#define kIOPlatformFreeDeviceResourcesKey			"IOPlatformFreeDeviceResources"
#endif

#ifndef kIOPlatformGetMessagedInterruptAddressKey
#define kIOPlatformGetMessagedInterruptAddressKey	"GetMessagedInterruptAddress"
#endif

#ifndef kIOPlatformGetMessagedInterruptControllerKey
#define kIOPlatformGetMessagedInterruptControllerKey	"GetMessagedInterruptController"
#endif

#ifndef kIOPlatformDeviceRelocatedKey
#define kIOPlatformDeviceRelocatedKey			"IOPlatformDeviceRelocatedKey"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void IOPCIBridge::initialize(void)
{
	uint32_t debug;

	gIOPlatformDeviceMessageKey
		= OSSymbol::withCStringNoCopy(kIOPlatformDeviceMessageKey);
	gIOPlatformDeviceASPMEnableKey
		= OSSymbol::withCStringNoCopy(kIOPlatformDeviceASPMEnableKey);
	gIOPlatformSetDeviceInterruptsKey
		= OSSymbol::withCStringNoCopy(kIOPlatformSetDeviceInterruptsKey);
	gIOPlatformResolvePCIInterruptKey
		= OSSymbol::withCStringNoCopy(kIOPlatformResolvePCIInterruptKey);
	gIOPlatformFreeDeviceResourcesKey
		= OSSymbol::withCStringNoCopy(kIOPlatformFreeDeviceResourcesKey);
	gIOPlatformDeviceRelocatedKey
		= OSSymbol::withCStringNoCopy(kIOPlatformDeviceRelocatedKey);
	gIOPCIDeviceChangedKey
		= OSSymbol::withCStringNoCopy(kIOPCIDeviceChangedKey);
	gIOPlatformGetMessagedInterruptAddressKey
		= OSSymbol::withCStringNoCopy(kIOPlatformGetMessagedInterruptAddressKey);
	gIOPlatformGetMessagedInterruptControllerKey
		= OSSymbol::withCStringNoCopy(kIOPlatformGetMessagedInterruptControllerKey);
#if ACPI_SUPPORT
	gIOPCIPSMethods[kIOPCIDeviceOffState]  = OSSymbol::withCStringNoCopy("_PS3");
	gIOPCIPSMethods[kIOPCIDeviceDozeState] = OSSymbol::withCStringNoCopy("RPS3");
	gIOPCIPSMethods[kIOPCIDeviceOnState]   = OSSymbol::withCStringNoCopy("_PS0");

	gIOPCIACPIPlane             = IORegistryEntry::getPlane("IOACPIPlane");
	gIOPCIFlags |= kIOPCIConfiguratorMapInterrupts;
#endif
	gIOPCIFlags |= kIOPCIConfiguratorUsePause;

	if (PE_parse_boot_argn("pci", &debug, sizeof(debug)))
		gIOPCIFlags |= debug;
	if (PE_parse_boot_argn("npci", &debug, sizeof(debug)))
		gIOPCIFlags &= ~debug;

	gIOPCITunnelIDKey           = OSSymbol::withCStringNoCopy(kIOPCITunnelIDKey);
	gIOPCITunnelControllerKey   = OSSymbol::withCStringNoCopy(kIOPCITunnelControllerIDKey);
	gIOPCITunnelledKey          = OSSymbol::withCStringNoCopy(kIOPCITunnelledKey);
	gIOPCIHPTypeKey             = OSSymbol::withCStringNoCopy(kIOPCIHPTypeKey);
	gIOPCITunnelL1EnableKey     = OSSymbol::withCStringNoCopy(kIOPCITunnelL1EnableKey);
	gIOPCIThunderboltKey        = OSSymbol::withCStringNoCopy("PCI-Thunderbolt");
	gIOPCIHotplugCapableKey     = OSSymbol::withCStringNoCopy("PCIHotplugCapable");
	gIOPolledInterfaceActiveKey = OSSymbol::withCStringNoCopy(kIOPolledInterfaceActiveKey);
	gIOPCIDeviceHiddenKey       = OSSymbol::withCStringNoCopy(kIOPCIDeviceHiddenKey);
}

//*********************************************************************************

IOWorkLoop * IOPCIBridge::getConfiguratorWorkLoop(void) const
{
    IOPCIHostBridge *selfBridge = OSDynamicCast(IOPCIHostBridge, this);
    IOPCIBridge *self = (IOPCIBridge*)this;

    if (selfBridge)
    {
        // We are the host bridge, so use own instance as ExpansionData may not be ready yet.
        return selfBridge->bridgeData->_configWorkLoop;
    }
    else
    {
        assert(self != NULL);
        assert(self->reserved->hostBridgeData != NULL);
        return self->reserved->hostBridgeData->_configWorkLoop;
    }
}

//*********************************************************************************

IOReturn IOPCIHostBridgeData::systemPowerChange(void * target, void * refCon,
										UInt32 messageType, IOService * service,
										void * messageArgument, vm_size_t argSize)
{
	IOPCIHostBridgeData *self = (IOPCIHostBridgeData*) refCon;

	switch (messageType)
	{
		case kIOMessageSystemCapabilityChange:
		{
			IOPMSystemCapabilityChangeParameters * params = (typeof params) messageArgument;

			if (params->changeFlags & kIOPMSystemCapabilityDidChange)
			{
				if (((params->fromCapabilities & kIOPMSystemCapabilityCPU) == 0) &&
					(params->toCapabilities & kIOPMSystemCapabilityCPU))
				{
					self->finishMachineState(0);
				}
#if !ACPI_SUPPORT
				else if ((params->fromCapabilities & kIOPMSystemCapabilityCPU) &&
					((params->toCapabilities & kIOPMSystemCapabilityCPU) == 0))
				{
					self->_wakeCount++;
				}
#endif
			}
			break;
		}
	}

	return (kIOReturnSuccess);
}

//*********************************************************************************

void IOPCI2PCIBridge::systemWillShutdown(IOOptionBits specifier)
{
    if (kIOPCIDeviceOnState == fPowerState) disableBridgeInterrupts();
    super::systemWillShutdown(specifier);
}

//*********************************************************************************

IOReturn IOPCIBridge::configOp(configOpParams *params)
{
	IOReturn       ret = kIOReturnSuccess;
	OSSet *        changed = nullptr;
	IOPCIDevice *  next = nullptr;
	uint32_t       state;
	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	// Make sure we are running in gate.
	if (!vars->_configWorkLoop->inGate())
	{
		return (vars->_configWorkLoop->runAction(
						OSMemberFunctionCast(IOCommandGate::Action, this, &IOPCIBridge::configOp),
						this, params));
	}
	
	if (kConfigOpScan != params->op)
	{
		ret = vars->_configurator->configOp(params->device, params->op, params->result, params->arg);
		if (kIOReturnSuccess != ret) return (ret);

		next = (IOPCIDevice *) params->device;
		if (kConfigOpTerminated == params->op)
		{
			vars->_waitingPauseSet->removeObject(next);
			vars->_pausedSet->removeObject(next);
			vars->_probeSet->removeObject(next);
		}
		else if (kConfigOpTestPause == params->op)
		{
			if (vars->_waitingPauseSet->setObject(next))
			{
				next->changePowerStateToPriv(kIOPCIDevicePausedState);
				next->powerOverrideOnPriv();
			}
		}

		if (params->op != kConfigOpPaused)
		{
			params->op = 0;
		}
		else
		{
			params->op = 0;

			DLOG("configOp:->pause: %s(0x%qx)\n", params->device->getName(), params->device->getRegistryEntryID());
			if (vars->_waitingPauseSet->containsObject(params->device))
			{
				vars->_pausedSet->setObject(params->device);
				vars->_waitingPauseSet->removeObject(params->device);
				if (!vars->_waitingPauseSet->getCount())
				{
					params->op = kConfigOpRealloc;
				}
			}
		}
	}

	while (params->op)
	{
		ret = vars->_configurator->configOp(params->device, params->op, &changed);
		params->op = 0;
		if (kIOReturnSuccess != ret) break;
		if (!changed) break;

		while ((next = (IOPCIDevice *) changed->getAnyObject()))
		{
			ret = vars->_configurator->configOp(next, kConfigOpGetState, &state);
			if (kIOReturnSuccess == ret)
			{
				if (kPCIDeviceStateDead & state)
				{
					DLOG("configOp:->dead: %s(0x%qx), 0x%x\n", next->getName(), next->getRegistryEntryID(), state);
					next->terminate();
				}
				else if (kPCIDeviceStateRequestPause & state)
				{
					DLOG("configOp:->pause: %s(0x%qx), 0x%x\n", next->getName(), next->getRegistryEntryID(), state);
					if (vars->_waitingPauseSet->setObject(next))
					{
						next->changePowerStateToPriv(kIOPCIDevicePausedState);
						next->powerOverrideOnPriv();
					}
				}
				else
				{
					DLOG("configOp:->probe: %s(0x%qx), 0x%x\n", next->getName(), next->getRegistryEntryID(), state);
					vars->_probeSet->setObject(next);
				}
			}
			changed->removeObject(next);
		}
		changed->release();
	}

	if (!vars->_waitingPauseSet->getCount())
	{
		while ((next = (IOPCIDevice *) vars->_pausedSet->getAnyObject()))
		{
			DLOG("configOp:<-unpause: %s(0x%qx)\n", next->getName(), next->getRegistryEntryID());
			if (2 != next->reserved->pauseFlags)
			{
				next->changePowerStateToPriv(kIOPCIDeviceOnState);
				next->powerOverrideOffPriv();
			}
			next->reserved->pauseFlags = 0;
			vars->_pausedSet->removeObject(next);
		}
		while ((next = (IOPCIDevice *) vars->_probeSet->getAnyObject()))
		{
			DLOG("configOp:<-probe: %s(0x%qx), pm %d\n", next->getName(), next->getRegistryEntryID(), next->reserved->pciPMState);
			if (kIOPCIDeviceOnState == next->reserved->pciPMState) deferredProbe(next);
			else                                     next->reserved->needsProbe = true;
			vars->_probeSet->removeObject(next);
		}
	}

	return (ret);
}

//*********************************************************************************

void IOPCIBridge::deferredProbe(IOPCIDevice * device)
{
	IOService *   client;
	IOPCIBridge * bridge;

	client = device->copyClientWithCategory(gIODefaultMatchCategoryKey);
	if ((bridge = OSDynamicCast(IOPCIBridge, client)))
	{
		DLOG("configOp:<-probe: %s(0x%qx)\n", device->getName(), device->getRegistryEntryID());

		// If IOPCIBridge::start() hasn't initialized the ivars needed for probeBus(), then there's no need to it here, it will run during start().
		if (atomic_load(&bridge->reserved->readyToProbe))
		{
			bridge->probeBus(device, bridge->firstBusNum());
		}
		else
		{
			DLOG("Device %s not started, skipping probe\n", device->getName());
		}
	}
	if (client) client->release();

	device->reserved->needsProbe = false;
}

//*********************************************************************************

static const IOPMPowerState gIOPCIPowerStates[kIOPCIDevicePowerStateCount] = {
    // version, capabilityFlags, outputPowerCharacter, inputPowerRequirement, staticPower, stateOrder
	{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 2, 0, kIOPMSoftSleep, kIOPMSoftSleep, 0, 1, 0, 0, 0, 0, 0, 0 },
	{ 2, kIOPMPowerOn|kIOPMInitialDeviceState, kIOPMPowerOn, kIOPMPowerOn, 0, 3, 0, 0, 0, 0, 0, 0 },
	{ 2, kIOPMConfigRetained, kIOPMConfigRetained, kIOPMConfigRetained, 0, 2, 0, 0, 0, 0, 0, 0 }
};

static const IOPMPowerState gIOPCIHostPowerStates[kIOPCIDevicePowerStateCount] = {
    // version, capabilityFlags, outputPowerCharacter, inputPowerRequirement, staticPower, stateOrder
	{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 2, 0, kIOPMSoftSleep, kIOPMSoftSleep, 0, 1, 0, 0, 0, 0, 0, 0 },
	{ 2, kIOPMPowerOn|kIOPMInitialDeviceState, kIOPMPowerOn, kIOPMPowerOn, 0, 3, 0, 0, 0, 0, 0, 0 },
	{ 2, kIOPMConfigRetained, kIOPMConfigRetained, kIOPMPowerOn, 0, 2, 0, 0, 0, 0, 0, 0 }
};


// version without kIOPCIDevicePausedState
static const IOPMPowerState gIOPCIPowerStatesV1[kIOPCIDevicePowerStateCount - 1] = {
    // version, capabilityFlags, outputPowerCharacter, inputPowerRequirement,
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 0, kIOPMSoftSleep, kIOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, kIOPMPowerOn|kIOPMInitialDeviceState, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};

//*********************************************************************************

IOReturn
IOPCIRegisterPowerDriver(IOService * service, bool hostbridge)
{
	IOReturn ret;
	IOPMPowerState * powerStates = hostbridge 
		? (IOPMPowerState *) gIOPCIHostPowerStates 
		: (IOPMPowerState *) gIOPCIPowerStates;

	ret = service->registerPowerDriver(service, powerStates, kIOPCIDevicePowerStateCount);
	if (kIOReturnSuccess != ret)
	{
		ret = service->registerPowerDriver(service,
										   (IOPMPowerState *) gIOPCIPowerStatesV1,
										   arrayCount(gIOPCIPowerStatesV1));
	}
	return (ret);
}

//*********************************************************************************
// [public] maxCapabilityForDomainState
//
// Finds the highest power state in the array whose input power
// requirement is equal to the input parameter.  Where a more intelligent
// decision is possible, override this in the subclassed driver.
//*********************************************************************************

unsigned long 
IOPCIBridge::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
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
IOPCIBridge::initialPowerStateForDomainState ( IOPMPowerFlags domainState )
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
IOPCIBridge::powerStateForDomainState ( IOPMPowerFlags domainState )
{
	if (domainState & kIOPMPowerOn)        return (kIOPCIDeviceOnState);
	if (domainState & kIOPMSoftSleep)      return (kIOPCIDeviceDozeState);
	if (domainState & kIOPMConfigRetained) return (kIOPCIDevicePausedState);
    return (kIOPCIDeviceOffState);
}

//*********************************************************************************

bool IOPCIBridge::start( IOService * provider )
{
	IOPCIDevice * pciDevice;

    if (!super::start(provider))
        return (false);

    if (!configure(provider))
    {
        stop(provider);
        return (false);
    }
    pciDevice = OSDynamicCast(IOPCIDevice, provider);

	// kIOPCIConfiguratorWakeToOff is deprecated in rdar://problem/64949845
	IORegistryEntry* parent = getParentEntry(gIOServicePlane);
	if ((parent && (parent->getProperty(kIOPMResetPowerStateOnWakeKey) == kOSBooleanTrue)) ||
		(kIOPCIConfiguratorWakeToOff & gIOPCIFlags))
	{
		setProperty(kIOPMResetPowerStateOnWakeKey, kOSBooleanTrue);
	}

    // initialize superclass variables
    PMinit();
    // clamp power on
//    temporaryPowerClampOn();
    // register as controlling driver
    IOPCIRegisterPowerDriver(this, !pciDevice);

    // join the tree
    provider->joinPMtree(this);

    if (!pciDevice)
    {
        configOpParams cp = {.device = this, .op = kConfigOpAddHostBridge, .result = nullptr};
        IOReturn ret = configOp(&cp);
        if (kIOReturnSuccess != ret)
        {
            stop(provider);
            return (false);
        }
    }

    atomic_store(&reserved->readyToProbe, true);

    probeBus( provider, firstBusNum() );

    if ((kIOPCIConfiguratorDeepIdle & gIOPCIFlags)
	  && (!provider->getProperty(kIOPCIHotPlugKey)) 
	  && (!provider->getProperty(kIOPCITunnelLinkChangeKey)) 
	  && !(getChildEntry(gIOServicePlane)))
    {
		DLOG("%s: no child D3\n", provider->getName());

		if (pciDevice
		 && pciDevice->hasPCIPowerManagement(kPCIPMCPMESupportFromD3Hot))
		{
			pciDevice->enablePCIPowerManagement(kPCIPMCSPowerStateD3);
		}
		powerOverrideOnPriv();
		changePowerStateToPriv(kIOPCIDeviceOffState);
		changePowerStateTo(kIOPCIDeviceOffState);
	}
    
    if (provider->getProperty(kIOPCISlotCommandCompleted))
    {
        reserved->commandCompletedSupport = true;
        setProperty(kIOPCISlotCommandCompleted, kOSBooleanTrue);
    }

    registerService();

    return (true);
}

void IOPCIBridge::stop( IOService * provider )
{
    if (initialized)
    {
        PMstop();
    }
    super::stop( provider);
}

bool IOPCIBridge::init( OSDictionary *  propTable )
{
    if (!super::init(propTable))
        return false;

    reserved = IOMallocType(ExpansionData);
    if (reserved == nullptr)
    {
        return false;
    }

    return true;
}

void IOPCIBridge::free( void )
{
    if (reserved)
    {
        OSSafeReleaseNULL(reserved->hostBridgeData);
        IOFreeType(reserved, ExpansionData);
    }

    super::free();
}

IOReturn IOPCIBridge::setDeviceCLKREQBits(IOPCIDevice * device, uint32_t bits)
{
    if (!device->reserved->expressCapability) return (kIOReturnUnsupported);

    uint16_t control;
    control = device->configRead16(device->reserved->expressCapability + 0x10);
    control &= ~kIOPCIExpressClkReq;
    control |= (kIOPCIExpressClkReq & bits);
    device->configWrite16(device->reserved->expressCapability + 0x10, control);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::setDeviceASPMBits(IOPCIDevice * device, uint32_t bits)
{
    if (!device->reserved->expressCapability) return (kIOReturnUnsupported);

    uint16_t control;
    control = device->configRead16(device->reserved->expressCapability + 0x10);
    control &= ~(kIOPCIExpressASPML0s | kIOPCIExpressASPML1);
    control |= bits;
    device->configWrite16(device->reserved->expressCapability + 0x10, control);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::setDeviceASPML1Bit(IOPCIDevice * device, uint32_t bit)
{
    if (!device->reserved->expressCapability) return (kIOReturnUnsupported);

    uint16_t control;
    control = device->configRead16(device->reserved->expressCapability + 0x10);
    control &= ~(kIOPCIExpressASPML1);
    control |= bit;
    device->configWrite16(device->reserved->expressCapability + 0x10, control);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::setDeviceL1PMBits(IOPCIDevice * device, uint32_t bits)
{
    OSData * data;

    if (!device->reserved->l1pmCapability) return (kIOReturnUnsupported);

	if (bits
	 && (data = OSDynamicCast(OSData, device->getProperty(kIOPCIExpressL1PMControlKey)))
	 && (data->getLength() >= 2*sizeof(uint32_t)))
	{
		uint32_t * l1bits = (typeof(l1bits)) data->getBytesNoCopy();
		device->configWrite32(device->reserved->l1pmCapability + 0x0C, l1bits[1]);
		device->configWrite32(device->reserved->l1pmCapability + 0x08, l1bits[0] & bits);
	}
	else
	{
		device->configWrite32(device->reserved->l1pmCapability + 0x08, 0);
		device->configWrite32(device->reserved->l1pmCapability + 0x0C, 0);
	}

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::setDeviceASPMState(IOPCIDevice * device,
                                            IOService * client, IOOptionBits state)
{
    IOOptionBits aspmBits, l1pmBits;

	if (state)
	{
		aspmBits = (device->reserved->aspmCaps & state);
		l1pmBits = (device->reserved->l1pmCaps);
	}
	else
	{
        aspmBits = l1pmBits = 0;
	}

    if (0 == device->space.s.functionNum)
    {
        // PCIe Base Spec section 5.5.4, L1 PM Substates Configuration:
        // "If setting either or both of the enable bits for ASPM L1 PM Substates, both ports must
        // be configured as described in this section while ASPM L1 is disabled."
        //
        // For certain PCIe IP, "setting" is interpreted as writing 1 or 0, so always clear the L1
        // Entry Enabled bit prior to writing the L1 PM Substates Control 1 register.
        setDeviceASPML1Bit(device, 0);

        setDeviceL1PMBits(device, l1pmBits);
        setDeviceASPMBits(device, aspmBits);
    }

    return (kIOReturnSuccess);
}

IOReturn IOPCI2PCIBridge::setDeviceASPMState(IOPCIDevice * device,
                                            IOService * client, IOOptionBits state)
{
    uint32_t aspmBits, l1pmBits;

    // PCIe Base Spec section 5.5.4, L1 PM Substates Configuration:
    // "If setting either or both of the enable bits for ASPM L1 PM Substates, both ports must be
    // configured as described in this section while ASPM L1 is disabled."
    //
    // For certain PCIe IP, "setting" is interpreted as writing 1 or 0, so always clear the L1
    // Entry Enabled bit prior to writing the L1 PM Substates Control 1 register.

    // Need to enable upstream first then downstream, reverse for disable
    if (state)
    {
        if (0 == device->space.s.functionNum)
        {
            setDeviceASPML1Bit(fBridgeDevice, 0);
            setDeviceASPML1Bit(device, 0);

            l1pmBits = (fBridgeDevice->reserved->l1pmCaps & device->reserved->l1pmCaps);
            setDeviceL1PMBits(fBridgeDevice, l1pmBits);
            setDeviceL1PMBits(device,       l1pmBits);
        }
		// L1 and L0s need to be supported on both ends to enable
		aspmBits = (state
				   & fBridgeDevice->reserved->aspmCaps
				   & device->reserved->aspmCaps);

        setDeviceASPMBits(fBridgeDevice, aspmBits);
        setDeviceASPMBits(device,        aspmBits);
        setDeviceCLKREQBits(device, device->reserved->aspmCaps & device->reserved->expressASPMDefault);
    }
    else 
    {
        aspmBits = l1pmBits = 0;
        if (0 == device->space.s.functionNum)
        {
            setDeviceASPML1Bit(device, 0);
            setDeviceASPML1Bit(fBridgeDevice, 0);

            setDeviceL1PMBits(device,       l1pmBits);
            setDeviceL1PMBits(fBridgeDevice, l1pmBits);
        }
        setDeviceASPMBits(device,       aspmBits);
        setDeviceASPMBits(fBridgeDevice, aspmBits);
    }

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::enableLTR(IOPCIDevice * device, bool enable)
{
	return (kIOReturnSuccess);
}

IOReturn IOPCI2PCIBridge::enableLTR(IOPCIDevice * device, bool enable)
{
	IOReturn status;

	status = fBridgeDevice->enableLTR(device, enable);
	if (status != kIOReturnSuccess) 										return status;
	return (device->enableLTR(device, enable));
}

IOReturn
IOPCI2PCIBridge::setTunnelL1Enable(IOPCIDevice * device, IOService * client, bool l1Enable)
{
	IOReturn            ret;
    IOPCIConfigShadow * shadow;
    bool                was, now;
	int32_t             incr;

	if (l1Enable == device->reserved->tunnelL1Allow)        return (kIOReturnSuccess);

	shadow = configShadow(device);

	DLOG("setTunnelL1Enable(0x%llx) %d->%d\n", 
		 device->getRegistryEntryID(), device->reserved->tunnelL1Allow, l1Enable);

	if (!shadow->sharedRoot)                                return (kIOReturnUnsupported);

	if ((client != device) 
		&& !device->isChild(client, gIOServicePlane))       return (kIOReturnNotAttached);

	device->setProperty(gIOPCITunnelL1EnableKey, l1Enable ? kOSBooleanTrue : kOSBooleanFalse);

	((IOPCIBridge*)this)->reserved->hostBridgeData->lockWakeReasonLock();
	incr  = l1Enable;
	incr -= (kTunnelL1Disable != device->reserved->tunnelL1Allow);
	device->reserved->tunnelL1Allow = l1Enable;
	was = (fTunnelL1EnableCount >= 0);
	fTunnelL1EnableCount += incr;
	now = (fTunnelL1EnableCount >= 0);
	((IOPCIBridge*)this)->reserved->hostBridgeData->unlockWakeReasonLock();

	if (was == now)	ret = kIOReturnSuccess;
	else
	{
		if (device == shadow->sharedRoot)
		{
			IOOptionBits state = 0;
			if (now && !(kIOPCIConfiguratorNoL1 & gIOPCIFlags)) state |= kIOPCIExpressASPML1;
			DLOG("set tunnel ASPM %s -> %d\n", device->getName(), state);
            shadow->sharedRootASPMState = state;
            ret = (kIOPCIDeviceOnState == device->reserved->pmState)
                ? device->setASPMState(this, state) : kIOReturnSuccess;
		}
		else
		{
			ret = fBridgeDevice->setTunnelL1Enable(this, now);
		}
	}
	
	return (ret);
}

IOReturn IOPCIBridge::setDevicePowerState(IOPCIDevice * device, IOOptionBits options,
                                          unsigned long prevState, unsigned long newState)
{
    IOReturn ret;
    bool noSave;

	noSave = ((kIOPCIConfigShadowVolatile & options) 
		    && (kOSBooleanFalse == device->getProperty(kIOPMPCIConfigSpaceVolatileKey)));

    DLOG("%s[%p]::setDevicePowerState(%ld, %ld, %d)\n", device->getName(), device, prevState, newState, noSave);
    
	if (newState == prevState) return (kIOReturnSuccess);

    ret = kIOReturnSuccess;
    switch (newState)
    {
        case kIOPCIDeviceOffState:
			if (noSave) break;
			ret = saveDeviceState(device, options);
			if (kIOPCIConfigShadowPermanent & options) break;

		    if (kOSBooleanTrue == device->getProperty(kIOPolledInterfaceActiveKey))
		    {
		    	newState = kIOPCIDeviceOnState;
			}
			device->setPCIPowerState(newState, 0);
            break;
            
        case kIOPCIDeviceDozeState:
			if (noSave) break;
			ret = saveDeviceState(device, options);
			if (kIOPCIConfigShadowPermanent & options) break;
			device->setPCIPowerState(newState, 0);
            break;

        case kIOPCIDeviceOnState:
		{
			configOpParams cp = {.device = device, .op = kConfigOpUnpaused, .result = nullptr};
			configOp(&cp);
			if (noSave) break;
			if (kIOPCIDevicePausedState != prevState)
			{
				if ((kIOPCIDeviceOffState == prevState) 
				 && !configShadow(device)->tunnelRoot
				 && ((kIOPCIClassGraphics == (device->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24))
				  || (kIOPCIClassMultimedia == (device->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24))))
				{
					tunnelsWait(device);
				}
                if ((kIOPCIDeviceOffState == prevState) &&
                    !configShadow(device)->tunnelRoot &&
                    device->getProperty(gIOPCITunnelIDKey, gIOServicePlane, 0) )
                {
                    // if we don't have a tunnel root but we are a tunnel
                    // then we know that we are an integrated solution
                    // block until the tunnel controller is restored

                    tunnelsWait(device);
                }
			}
			device->setPCIPowerState(newState, 0);
			if (kIOPCIDevicePausedState == prevState) break;
            restoreDeviceState(device, options);
            break;
		}

        case kIOPCIDevicePausedState:
		{
			configOpParams cp = {.device = device, .op = kConfigOpPaused, .result = nullptr};
			IOLog("pci pause: %s\n", device->getName());
			configOp(&cp);
			if (noSave) break;
			device->setPCIPowerState(newState, 0);
            restoreDeviceState(device, options);
            break;
		}
    }
    
    return (ret);
}

IOReturn IOPCIBridge::setPowerState(unsigned long powerStateOrdinal, IOService * whatDevice)
{
	return super::setPowerState(powerStateOrdinal, whatDevice);
}

IOReturn IOPCIBridge::setDevicePowerState( IOPCIDevice * device,
        unsigned long whatToDo )
{
	// Special for pci/pci-bridge devices - 
	// kSaveBridgeState(2) to save immediately, kRestoreBridgeState(3) to restore immediately

	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	if (kRestoreBridgeState == whatToDo)
	{
		if (kSaveBridgeState == vars->_allPCI2PCIBridgeState)
		{
			restoreMachineState(kMachineRestoreBridges, 0);
			restoreMachineState(kMachineRestoreEarlyDevices, 0);
		}
		vars->_allPCI2PCIBridgeState = kRestoreBridgeState;
		vars->_tunnelWait = vars->_tunnelSleep;
	}
	else if (kSaveBridgeState == whatToDo)
	{
		vars->_allPCI2PCIBridgeState = (uint32_t)whatToDo;
		vars->_wakeCount++;
	}
	else
	{
		panic("setDevicePowerState");
	}

	return (kIOReturnSuccess);
}

static void IOPCILogDevice(const char * log, IOPCIDevice * device, bool dump)
{
	int      slen, len, pos;
	char *   string;
	uint32_t offset, data = 0;
	
	slen = 2048;
	pos  = 0;
	string = IONewData(char, slen);
	if (!string) return;
	len = 256;
	pos = snprintf(string, slen - pos, "%s : ints(%d) ", log, ml_get_interrupts_enabled());
	if (device->getPath(string + pos, &len, gIOServicePlane)) pos += len;
    if (dump)
    {
		pos += snprintf(string + pos, slen - pos, "\n        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
		for (offset = 0; offset < 256; offset++)
		{
			if (0 == (offset & 3))  data = device->configRead32(offset);
			if (0 == (offset & 15)) pos += snprintf(string + pos, slen - pos, "\n    %02X:", offset);
			pos += snprintf(string + pos, slen - pos, " %02x", data & 0xff);
			data >>= 8;
		}
	}
	pos += snprintf(string + pos, slen - pos, "\n");
	DLOG("%s", string);
	IODeleteData(string, char, slen);
}

void IOPCIBridge::slotControlWrite(IOPCIDevice *device,
								   uint16_t data,
								   uint16_t mask)
{
	// Writes to the slot control register are considered "commands" with unique semantics:
	// unless the slot has no-command-completed support, software must wait for the command
	// to complete before issuing another. Failure to do so may cause hardware to silently
	// discard the early write.
	// See PCIe base spec rev 5.0, sec 6.7.3.2 "Command Completed Events", for more details.

	// 1. Wait for up to 1s for the previous command, if any, to complete.
	if (reserved->commandCompletedSupport && reserved->commandSent)
	{
		int i, bound = 1000;
		for (i = 0; i < bound; i++)
		{
			uint16_t slotStatus = device->configRead16(device->reserved->expressCapability + 0x1a);

			if ((slotStatus & (1 << 4)) == 1)
			{
				break;
			}
			IOSleep(1);
		}

		if (i == bound)
		{
			IOLog("[%s()] Timed out waiting for command completed to be set for %s\n", __func__, device->getName());
		}

		// W1C the command-completed bit
		device->configWrite16(device->reserved->expressCapability + 0x1a, 1 << 4);
		reserved->commandSent = false;
	}

	// 2. RMW the slot control register
	uint16_t slotCtrl = device->configRead16(device->reserved->expressCapability + 0x18);

	slotCtrl &= ~mask;
	slotCtrl |= data;

	device->configWrite16(device->reserved->expressCapability + 0x18, slotCtrl);

	reserved->commandSent = true;
}

IOReturn IOPCIBridge::saveDeviceState( IOPCIDevice * device,
                                       IOOptionBits options )
{
    IOPCIConfigShadow * shadow;
    IOPCIConfigSave     _saved;
    IOPCIConfigSave *   saved;
	IOReturn ret;
    UInt32   flags;
	uint32_t data;
    int      i;
	bool     ok;
	uint64_t time;

    if (!device->savedConfig) return (kIOReturnNotReady);

    shadow = configShadow(device);

    // device flags
    if (device->getProperty(kIOPMPCIWakeL1PMDisableKey))
        shadow->flags |= kIOPCIConfigShadowWakeL1PMDisable;
    else
        shadow->flags &= ~kIOPCIConfigShadowWakeL1PMDisable;

    flags = shadow->flags;
	_saved = shadow->configSave;
    saved = &_saved;

    if (kIOPCIConfigShadowValid & flags)
    {
		if (kIOPCIConfigShadowPermanent & shadow->flags) shadow->restoreCount = 0;
		if (kIOPCIConfigShadowVolatile & options) restoreQEnter(device);
        if (!(kIOPCIConfigShadowPermanent & options)) return (kIOReturnSuccess);
	}

    ret = kIOReturnSuccess;
	DLOG("%s::saveDeviceState(0x%x)\n", device->getName(), options);
    flags |= kIOPCIConfigShadowValid | options;
    shadow->flags = flags;
    shadow->restoreCount = 0;

	if (shadow->tunnelled)
	{
		shadow->tunnelID = device->copyProperty(gIOPCITunnelIDKey, gIOServicePlane);
	}

    if (shadow->handler)
    {
		time = mach_absolute_time();
        (*shadow->handler)(shadow->handlerRef, kIOMessageDeviceWillPowerOff, device, 3);
		time = mach_absolute_time() - time;
		absolutetime_to_nanoseconds(time, &time);
		DLOG("%s::configHandler(kIOMessageDeviceWillPowerOff) %lld ms\n", device->getName(), time / 1000000ULL);
    }

	if (kIOPCIConfiguratorLogSaveRestore & gIOPCIFlags)
		IOPCILogDevice("save device", device, true);
    else if ((kIOPCIConfiguratorIOLog | kIOPCIConfiguratorKPrintf) & gIOPCIFlags)
		IOPCILogDevice("save device", device, false);

    if (kIOPCIConfigShadowHostBridge & flags) {}
    else 
	{
		uint32_t regCount = kIOPCIConfigEPShadowRegs;
		if (shadow->bridge)
		{
			regCount = kIOPCIConfigBridgeShadowRegs;
			shadow->bridge->saveBridgeState();
		}
        for (i = 0; i < regCount; i++)
        {
            if (kIOPCISaveRegsMask & (1 << i)) saved->savedConfig[i] = device->configRead32(i * 4);
        }
    }

	if (device->reserved->l1pmCapability)
	{
        saved->savedL1PM0 
        	= device->configRead32(device->reserved->l1pmCapability + 0x08);
        saved->savedL1PM1 
        	= device->configRead32(device->reserved->l1pmCapability + 0x0C);
		if (kIOPCIConfigShadowWakeL1PMDisable & shadow->flags) saved->savedL1PM0 &= ~(0xF);
    }

	if (device->reserved->latencyToleranceCapability)
	{
        saved->savedLTR 
        	= device->configRead32(device->reserved->latencyToleranceCapability + 0x04);
    }

    if (device->reserved->acsCapability)
    {
        saved->savedACS = device->configRead16(device->reserved->acsCapability + 0x06);
    }

	if (device->reserved->aerCapability)
	{
        saved->savedAERCapsControl
            = device->configRead32(device->reserved->aerCapability + 0x18);
        saved->savedAERSeverity
            = device->configRead32(device->reserved->aerCapability + 0x0C);
        saved->savedAERUMask
            = device->configRead32(device->reserved->aerCapability + 0x08);
        saved->savedAERCMask
            = device->configRead32(device->reserved->aerCapability + 0x14);
		if (device->reserved->rootPort) saved->savedAERRootCommand
            = device->configRead32(device->reserved->aerCapability + 0x2c);
    }

    if (device->reserved->expressCapability)
    {
        saved->savedDeviceControl
            = device->configRead16( device->reserved->expressCapability + 0x08 );
        saved->savedLinkControl
            = device->configRead16( device->reserved->expressCapability + 0x10 );
		if ((kIOPCIConfigShadowBridgeInterrupts & shadow->flags)
		 || (0x100 & device->reserved->expressCapabilities))
        {
            saved->savedSlotControl
                = device->configRead16( device->reserved->expressCapability + 0x18 );
        }
		if (expressV2(device))
		{
			saved->savedDeviceControl2
				= device->configRead16( device->reserved->expressCapability + 0x28 );
			saved->savedLinkControl2
				= device->configRead16( device->reserved->expressCapability + 0x30 );
			saved->savedSlotControl2
				= device->configRead16( device->reserved->expressCapability + 0x38 );
		}
		if (kIOPCIConfigShadowSleepLinkDisable & shadow->flags)
		{
            device->configWrite16(device->reserved->expressCapability + 0x10,
            						(1 << 4) | saved->savedLinkControl);
		}
		if (kIOPCIConfigShadowSleepReset & shadow->flags)
		{
			UInt16 bridgeControl;
			bridgeControl = device->configRead16(kPCI2PCIBridgeControl);
			device->configWrite16(kPCI2PCIBridgeControl, bridgeControl | 0x40);
			IOSleep(10);
			device->configWrite16(kPCI2PCIBridgeControl, bridgeControl);
		}
		if (kIOPCIConfigShadowWakeL1PMDisable & shadow->flags) saved->savedLinkControl &= ~(0x100);
    }

    if (device->reserved->fpbCapability)
    {
        saved->savedFPBControl1
            = device->configRead32(device->reserved->fpbCapability + 0x08);
        saved->savedFPBControl2
            = device->configRead32(device->reserved->fpbCapability + 0x0C);
		device->configWrite32(device->reserved->fpbCapability + 0x1C, 0);
        saved->savedFPBRIDVector0
            = device->configRead32(device->reserved->fpbCapability + 0x20);
	}

    if (device->reserved->ptmCapability)
    {
        saved->savedPTMControl
            = device->configRead32(device->reserved->ptmCapability + 0x08);
	}

	IOPCIMessagedInterruptController::saveDeviceState(device, saved);

    if (shadow->handler)
    {
		time = mach_absolute_time();
        (*shadow->handler)(shadow->handlerRef, kIOMessageDeviceHasPoweredOff, device, 3);
		time = mach_absolute_time() - time;
		absolutetime_to_nanoseconds(time, &time);
		DLOG("%s::configHandler(kIOMessageDeviceHasPoweredOff) %lld ms\n", device->getName(), time / 1000000ULL);
    }

	data = device->configRead32(kIOPCIConfigVendorID);
#ifdef DEADTEST
	if (!strcmp(DEADTEST, device->getName())) data = 0xFFFFFFFF;
#endif
	ok = (data && (data != 0xFFFFFFFF));
	if (ok)
	{
		shadow->configSave = *saved;
	}
	else
	{
		if (kIOPCIConfigShadowHotplug & shadow->flags)
		{
			configOpParams cp = {.device = device, .op = kConfigOpKill, .result = nullptr};
			DLOG("saveDeviceState kill device %s\n", device->getName());
			ret = configOp(&cp);
			shadow->flags &= ~kIOPCIConfigShadowValid;
		}
        if (kIOPCIConfigShadowPermanent & options) ret = kIOReturnOffline;
	}

    if ((kIOPCIConfigShadowValid & flags)
     && (!(kIOPCIConfigShadowPermanent & options)))
	{
		configOpParams cp = {.device = device, .op = kConfigOpShadowed, .result = &shadow->configSave.savedConfig[0]};
		configOp(&cp);
		restoreQEnter(device);
	}

    return (ret);
}

IOReturn IOPCIBridge::_restoreDeviceState(IOPCIDevice * device, IOOptionBits options)
{
	AbsoluteTime deadline, start, now = 0;
	IOPCIConfigShadow * shadow;
	IOPCIConfigSave   * saved;
	uint32_t     retries;
	uint32_t     data;
	bool         ok;
	uint8_t      dead;
	UInt32       flags;
	int          i;
	uint64_t     time;
	IOReturn     ret;
	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	shadow = configShadow(device);
	saved = &shadow->configSave;
	flags = shadow->flags;

	if (!(kIOPCIConfigShadowValid & flags))      return (kIOReturnNoResources);
	if (shadow->restoreCount == vars->_wakeCount) return (kIOReturnNoResources);
	shadow->restoreCount = vars->_wakeCount;

	shadow->device->reserved->pmHibernated = false;

	if (shadow->handler)
	{
		time = mach_absolute_time();
		ret = (*shadow->handler)(configShadow(device)->handlerRef, 
                                               kIOMessageDeviceWillPowerOn, device, 3);
		time = mach_absolute_time() - time;
		absolutetime_to_nanoseconds(time, &time);
		DLOG("%s::configHandler(kIOMessageDeviceWillPowerOn) %lld ms\n", device->getName(), time / 1000000ULL);
	}

	data = 0xEEEEEEEE;
	dead = device->reserved->dead;
	if (!dead)
	{
		ret = device->parent->checkLink(kCheckLinkParents);
		if (kIOReturnSuccess != ret)
		{
			DLOG("%s: pci restore no link\n", device->getName());
			dead = true;
			data = ret;
		}
	}

	if ((!dead) && !(kIOPCIConfigShadowBridgeDriver & flags))
	{
		retries = 0;
		clock_get_uptime(&start);

		// some devices take 600ms+ to be available rdar://problem/58030724
		clock_interval_to_deadline(1000, kMillisecondScale, &deadline);
		do
		{
			if (retries) IOSleep(2);
			data = device->configRead32(kIOPCIConfigVendorID);
			// Section 2.3.2 of PCIe spec. If the root complex implements Configuration Request Retry Status
			// "For a Configuration Read Request that includes both bytes of the VendorID field of a device Function's
			// Configuration Space Header, the Root Complex must complete the Request to the host by returning a
			// read-data value of 0001h for the Vendor ID field and all '1's for any additional bytes included in the request."
			ok = (data && (data != 0xFFFFFFFF) && (data != 0xffff0001));
			if (data != device->savedConfig[kIOPCIConfigVendorID >> 2])
			{
				// A device in the process of loading their configuration from EEPROM could return
				// an incorrect VID/DID. Just in case, retry (until the timeout).
				DLOG("%s: pci restore invalid deviceid 0x%08x\n", device->getName(), data);
				ok = false;
			}
			if (ok) break;
			retries++;
			clock_get_uptime(&now);
		}
		while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));

		if (retries)
		{
			absolutetime_to_nanoseconds(now - start, &now);
			DLOG("%s: pci restore waited for %qd ms %s\n", device->getName(), now / 1000000ULL, ok ? "ok" : "fail");
		}

		if (data != device->savedConfig[kIOPCIConfigVendorID >> 2])
		{
			DLOG("%s: pci restore invalid deviceid 0x%08x\n", device->getName(), data);
			if (kIOPCIConfigShadowPermanent & flags) IOLog("%s: pci restore invalid deviceid 0x%08x\n", device->getName(), data);
			dead = true;
#if !ACPI_SUPPORT
			if (data && (data != 0xFFFFFFFF)) panic("%s: pci restore invalid deviceid 0x%08lx\n", device->getName(), data);
#endif
		}
	}

	if (dead)
	{
		if ((kPCIHotPlugTunnelRoot == shadow->hpType)
			|| (kPCIStaticTunnel == shadow->hpType)
			|| (kPCIStaticShared == shadow->hpType))
		{
			if (((kIOPCIConfiguratorTBPanics    & gIOPCIFlags) && !vars->_isUSBCSystem)
				|| ((kIOPCIConfiguratorTBUSBCPanics & gIOPCIFlags) && vars->_isUSBCSystem))
			{
				panic("%s(%s): thunderbolt power on failed 0x%08x\n",
							device->getName(),
							getPlatform()->getProvider()->getName(),
							(int)data);
			}
			else
			{
				IOLog("%s(%s): thunderbolt power on failed 0x%08x\n",
							device->getName(),
							getPlatform()->getProvider()->getName(),
							(int)data);
			}
		}
	}
	else
	{
		if (kIOPCIConfiguratorLogSaveRestore & gIOPCIFlags)
			IOPCILogDevice("before restore", device, true);
		else if ((kIOPCIConfiguratorIOLog | kIOPCIConfiguratorKPrintf) & gIOPCIFlags)
			IOPCILogDevice("restore device", device, false);

		if (kIOPCIConfigShadowHostBridge & flags) {}
		else
		{
			uint32_t regCount = kIOPCIConfigEPShadowRegs;
			if (shadow->bridge)
			{
				regCount = kIOPCIConfigBridgeShadowRegs;
				shadow->bridge->restoreBridgeState();
			}
			for (i = (kIOPCIConfigRevisionID >> 2); i < regCount; i++)
			{
				if (kIOPCISaveRegsMask & (1 << i))
				    device->configWrite32( i * 4, saved->savedConfig[ i ]);
			}
			device->configWrite32(kIOPCIConfigCommand, saved->savedConfig[1]);
		}

		if (device->reserved->l1pmCapability)
		{
			device->configWrite32(device->reserved->l1pmCapability + 0x0C, 
								  saved->savedL1PM1);
			device->configWrite32(device->reserved->l1pmCapability + 0x08,
								  saved->savedL1PM0);
		}

		if (device->reserved->latencyToleranceCapability)
		{
			device->configWrite32(device->reserved->latencyToleranceCapability + 0x04, 
								  saved->savedLTR);
		}

		if (device->reserved->acsCapability)
		{
			device->configWrite16(device->reserved->acsCapability + 0x06, saved->savedACS);
		}

		if (device->reserved->aerCapability)
		{
			device->configWrite32(device->reserved->aerCapability + 0x18, 
								  saved->savedAERCapsControl);
			device->configWrite32(device->reserved->aerCapability + 0x0C, 
								  saved->savedAERSeverity);
			device->configWrite32(device->reserved->aerCapability + 0x08, 
								  saved->savedAERUMask);
			device->configWrite32(device->reserved->aerCapability + 0x14, 
								  saved->savedAERCMask);
			if (device->reserved->rootPort) 
			{
				device->configWrite32(device->reserved->aerCapability + 0x30, 0xFF);
				device->configWrite32(device->reserved->aerCapability + 0x2c,
									  saved->savedAERRootCommand);
			}
		}

		if (device->reserved->expressCapability)
		{
			device->configWrite16(device->reserved->expressCapability + 0x08,
									saved->savedDeviceControl);

			if (expressV2(device))
			{
				// PCI Spec Table 7-25 LTR Mechanism Enable:
				// For Downstream Ports, this bit must be reset to the default value if the Port goes to DL_Down status.
				// enable LTR for upstream bridge if the LTR Enable was set in case the port had a DL_Down event
				if((saved->savedDeviceControl2 & (1 << 10)) != 0)
				{
					device->parent->enableLTR(device, true);
				}

				device->configWrite16(device->reserved->expressCapability + 0x28,
									  saved->savedDeviceControl2);
			}
			device->configWrite16(device->reserved->expressCapability + 0x10,
									saved->savedLinkControl);
			if ((kIOPCIConfigShadowBridgeInterrupts & configShadow(device)->flags)
			 || (0x100 & device->reserved->expressCapabilities))
			{
				slotControlWrite(device, saved->savedSlotControl, 0xFFFF);
			}
			if (expressV2(device))
			{
				device->configWrite16(device->reserved->expressCapability + 0x30,
									  saved->savedLinkControl2);
				device->configWrite16(device->reserved->expressCapability + 0x38,
									  saved->savedSlotControl2);
			}
		}

		if (device->reserved->fpbCapability)
		{
			device->configWrite32(device->reserved->fpbCapability + 0x08,
									saved->savedFPBControl1);
			device->configWrite32(device->reserved->fpbCapability + 0x0C,
									saved->savedFPBControl2);
			device->configWrite32(device->reserved->fpbCapability + 0x1C,
									0);
			device->configWrite32(device->reserved->fpbCapability + 0x20,
									saved->savedFPBRIDVector0);
		}

		if (device->reserved->ptmCapability)
		{
			device->configWrite32(device->reserved->ptmCapability + 0x08,
									saved->savedPTMControl);
		}

		IOPCIMessagedInterruptController::restoreDeviceState(device, saved);

		if (kIOPCIConfiguratorLogSaveRestore & gIOPCIFlags)
			IOPCILogDevice("after restore", device, true);

		if (configShadow(device)->handler)
		{
			time = mach_absolute_time();
			(*configShadow(device)->handler)(configShadow(device)->handlerRef, 
											 kIOMessageDeviceHasPoweredOn, device, 3);
			time = mach_absolute_time() - time;
			absolutetime_to_nanoseconds(time, &time);
			DLOG("%s::configHandler(kIOMessageDeviceHasPoweredOn) %lld ms\n", device->getName(), time / 1000000ULL);
		}
	}

	if (!(kIOPCIConfigShadowPermanent & flags)) device->reserved->dead = dead;

	configOpParams cp = {.device = device, .op = kConfigOpShadowed, .result = nullptr};
	configOp(&cp);

	return (kIOReturnSuccess);
}

void IOPCIBridge::restoreQEnter(IOPCIDevice * device)
{
	bool                enqueued = false;
	queue_head_t *      que = NULL;
	IOPCIConfigShadow * shadow = NULL;
	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	shadow = configShadow(device);
	if (shadow->tunnelRoot) 
	{
		DLOG("%s: queued on %s\n", device->getName(), shadow->tunnelRoot->getName());
		que = &configShadow(shadow->tunnelRoot)->dependents;
	}
	else
	{
		que = &vars->_allPCIDeviceRestoreQ;
	}

	IOSimpleLockLock(vars->_allPCI2PCIBridgesLock);
	if (!device->isInactive() && shadow->link.next == NULL)
	{
		queue_enter_first(que,
					shadow,
					IOPCIConfigShadow *,
					link);

		enqueued = true;
	}
	IOSimpleLockUnlock(vars->_allPCI2PCIBridgesLock);

	if (!enqueued)
	{
		return;
	}

	if (shadow->tunnelRoot)
	{
		if ((kPCIHeaderType0 == device->reserved->headerType) && (kPCIStaticTunnel == shadow->hpType))
		{
			// tunnel controller
			vars->tunnelSleepIncrement(device->getName(), true);
		}
	}
	else if (device->getProperty(gIOPCITunnelControllerKey, gIOServicePlane, 0))
	{
		// if we don't have a tunnel root but we are a tunnel controller
		// then we know that we are an integrated solution
		// block the tunnelWait people
		vars->tunnelSleepIncrement(device->getName(), true);
	}
}

void IOPCIBridge::restoreQRemove(IOPCIDevice * device)
{
	queue_head_t *      que = NULL;
	IOPCIConfigShadow * shadow = nullptr;
	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	shadow = configShadow(device);

	if (shadow->tunnelRoot)
	{
		que = &configShadow(shadow->tunnelRoot)->dependents;
	}
	else
	{
		que = &vars->_allPCIDeviceRestoreQ;
	}

	IOSimpleLockLock(vars->_allPCI2PCIBridgesLock);
	if (shadow->link.next)
	{
		queue_remove(que,
					shadow,
					IOPCIConfigShadow *,
					link);
	}
	IOSimpleLockUnlock(vars->_allPCI2PCIBridgesLock);
}

void IOPCIBridge::tunnelsWait(IOPCIDevice * device)
{
	reserved->hostBridgeData->tunnelsWait(device);
}

IOReturn IOPCIBridge::restoreTunnelState(IOPCIDevice * rootDevice, IOOptionBits options, 
                                         bool * didTunnelController)
{
	IOReturn            ret;
	IOPCIConfigShadow * root = nullptr;
	IOPCIConfigShadow * shadow = nullptr;
	IOPCIConfigShadow * next = nullptr;
	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	DLOG("restoreTunnelState(%s, %d)\n", rootDevice->getName(), options);
	root = configShadow(rootDevice);
	IOSimpleLockLock(vars->_allPCI2PCIBridgesLock);

	next = (IOPCIConfigShadow *) queue_first(&root->dependents);
	while (!queue_end(&root->dependents, (queue_entry_t) next))
	{
		shadow = next;
		next = (IOPCIConfigShadow *) queue_next(&shadow->link);

		if (kMachineRestoreBridges & options) 
		{
			if (!(kIOPCIConfigShadowBridge & shadow->flags))
			{
				continue;
			}
		}

		if (!(kMachineRestoreTunnels & options))
		{
			if (shadow->tunnelID)
			{
				continue;
			}
		}

		IOSimpleLockUnlock(vars->_allPCI2PCIBridgesLock);

		if (kIOPCIConfigShadowValid & shadow->flags)
		{
			shadow->device->setPCIPowerState(kIOPCIDeviceOnState, options);
			ret = _restoreDeviceState(shadow->device, 0);
		}
		if (shadow->tunnelID)
		{
			shadow->tunnelID->release();
			shadow->tunnelID = 0;
		}

		IOSimpleLockLock(vars->_allPCI2PCIBridgesLock);

		next   = (IOPCIConfigShadow *) queue_next(&shadow->link);

		// Check if shadow's device terminated while restoreTunnelState()
		// released _allPCI2PCIBridgesLock.
		if (next == NULL)
		{
			next = (IOPCIConfigShadow *) queue_first(&root->dependents);
			continue;
		}

		queue_remove(&root->dependents,
					 shadow,
					 IOPCIConfigShadow *,
					 link);

		if (didTunnelController 
			&& (kPCIHeaderType0 == shadow->device->reserved->headerType)
			&& (kPCIStaticTunnel == shadow->hpType))
		{
			*didTunnelController = true;
		}
	}


	IOSimpleLockUnlock(vars->_allPCI2PCIBridgesLock);

	return (kIOReturnSuccess);
}

IOReturn IOPCIHostBridgeData::finishMachineState(IOOptionBits options)
{
    IOPCIConfigShadow * shadow;
    IOPCIConfigShadow * next;
    uint8_t             prevState;
    queue_head_t        q;

    DLOG("IOPCIBridgeVariables::finishMachineState(%d)\n", options);

    queue_init(&q);
    IOSimpleLockLock(_allPCI2PCIBridgesLock);
    next = (IOPCIConfigShadow *) queue_first(&_allPCIDeviceRestoreQ);
    while (!queue_end(&_allPCIDeviceRestoreQ, (queue_entry_t) next))
    {
		assert(next);
        shadow = next;
        next   = (IOPCIConfigShadow *) queue_next(&shadow->link);

        if (shadow->device->reserved->pmHibernated)
        {
            shadow->device->reserved->pmHibernated = false;
            queue_enter(&q, shadow, IOPCIConfigShadow *, linkFinish);
        }
    }
    IOSimpleLockUnlock(_allPCI2PCIBridgesLock);

    next = (IOPCIConfigShadow *) queue_first(&q);
    while (!queue_end(&q, (queue_entry_t) next))
    {
        shadow = next;
        next   = (IOPCIConfigShadow *) queue_next(&shadow->linkFinish);

        shadow->linkFinish.next = shadow->linkFinish.prev = NULL;
        if ((prevState = shadow->device->reserved->pciPMState) <= kIOPCIDeviceDozeState)
        {
            DLOG("%s: reset PCIPM\n", shadow->device->getName());
#if ACPI_SUPPORT
            shadow->device->reserved->lastPSMethod = shadow->device->reserved->psMethods[kIOPCIDeviceOnState];
#endif
            shadow->device->reserved->pciPMState   = kIOPCIDeviceOnState;
            shadow->device->setPCIPowerState(prevState, 0);
        }
    }

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::restoreMachineState(IOOptionBits options, IOPCIDevice * device)
{
	IOReturn            ret;
	IOPCIConfigShadow * shadow = nullptr;
	IOPCIConfigShadow * next = nullptr;
	bool                skip, disable;
	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	DLOG("restoreMachineState(%d)\n", options);

	IOSimpleLockLock(vars->_allPCI2PCIBridgesLock);

	next = (IOPCIConfigShadow *) queue_first(&vars->_allPCIDeviceRestoreQ);
	while (!queue_end(&vars->_allPCIDeviceRestoreQ, (queue_entry_t) next))
	{
		shadow  = next;
		next    = (IOPCIConfigShadow *) queue_next(&shadow->link);
		disable = skip = false;

		if (kMachineRestoreDehibernate & options)
		{
			shadow->device->reserved->pmHibernated = true;
		}

		if (!queue_empty(&shadow->dependents))                      continue;
		if (shadow->sharedRoot)                                     continue;
		if (shadow->tunnelRoot || shadow->tunnelID)                 panic("tunnel");

		if (shadow->device != device)
		{
			if (kMachineRestoreBridges & options) 
			{
				if (!(kIOPCIConfigShadowBridge & shadow->flags))
				{
					skip = true;
					disable = (0 != (kMachineRestoreDehibernate & options));
				}
			}
			if (!skip && !(kIOPCIConfigShadowVolatile & shadow->flags))
			{
				skip = disable = true;
			}
#if ACPI_SUPPORT
			if (!skip
				&& !(kMachineRestoreDehibernate & options)
				// skip any slow PS methods
				&& (shadow->device->reserved->psMethods[0] >= 0)
				// except for nvidia bus zero devices
				&& (shadow->device->space.s.busNum 
					|| (0x10de != (shadow->configSave.savedConfig[kIOPCIConfigVendorID >> 2] & 0xffff))))
			{
				disable = skip = true;
			}
#endif
			if (skip)
			{
				if (disable && !shadow->device->space.s.busNum)
				{
					DLOG("disable %s\n", shadow->device->getName());
					shadow->device->configWrite16(kIOPCIConfigCommand,
						shadow->configSave.savedConfig[kIOPCIConfigCommand >> 2] & ~(kIOPCICommandIOSpace|kIOPCICommandMemorySpace|kIOPCICommandBusLead));

					if (shadow->bridge
					 && (!(kIOPCIConfigShadowBridgeDriver & shadow->flags))
					 && (0xFF != shadow->device->configRead8(kPCI2PCISecondaryBus)))
					{
						DLOG("%s::restore bus(0x%x->0x%x)\n", shadow->device->getName(),
							shadow->device->configRead32(kPCI2PCIPrimaryBus),
							shadow->configSave.savedConfig[kPCI2PCIPrimaryBus >> 2]);
						shadow->device->configWrite32(kPCI2PCIPrimaryBus, shadow->configSave.savedConfig[kPCI2PCIPrimaryBus >> 2]);
					}
				}
				continue;
			}

			if (kMachineRestoreEarlyDevices & options)
			{
				if (shadow->device->space.s.busNum)                 continue;
				if (shadow->handler)                                continue;
				if (shadow->device->reserved->pmSleepEnabled)       continue;
			}
		}

		IOSimpleLockUnlock(vars->_allPCI2PCIBridgesLock);

		shadow->device->setPCIPowerState(kIOPCIDeviceOnState, options);

		ret = _restoreDeviceState(shadow->device, 0);

		IOSimpleLockLock(vars->_allPCI2PCIBridgesLock);

		next   = (IOPCIConfigShadow *) queue_next(&shadow->link);

		// Check if shadow's device terminated while restoreMachineState()
		// released _allPCI2PCIBridgesLock.
		if (next == NULL)
		{
			next = (IOPCIConfigShadow *) queue_first(&vars->_allPCIDeviceRestoreQ);
			continue;
		}

		queue_remove(&vars->_allPCIDeviceRestoreQ,
					 shadow,
					 IOPCIConfigShadow *,
					 link);
		shadow->link.next = shadow->link.prev = NULL;
	}

	IOSimpleLockUnlock(vars->_allPCI2PCIBridgesLock);

	return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::_restoreDeviceDependents(IOPCIDevice * device, IOOptionBits options, IOPCIDevice * forDependent)
{
	IOReturn ret = kIOReturnNotFound;
	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	if (!device->savedConfig) return (kIOReturnNotReady);

	IOLockLock(configShadow(device)->dependentsLock);

	if (!queue_empty(&configShadow(device)->dependents))
	{
		DLOG("%s: waking deps for %s\n", device->getName(), forDependent->getName());
		do
		{
			bool didTunnelController = false;

			ret = restoreTunnelState(device, kMachineRestoreBridges, &didTunnelController);
			ret = restoreTunnelState(device, 0,                      &didTunnelController);
			ret = restoreTunnelState(device, kMachineRestoreTunnels, &didTunnelController);

			if (didTunnelController)
			{
				IOOptionBits state;

				state = configShadow(device)->sharedRootASPMState;
				DLOG("wake tunnel ASPM %s -> %d\n", device->getName(), state);
				device->setASPMState(this, state);

				vars->tunnelSleepIncrement(device->getName(), false);
			}
		}
		while (false);
	}

	IOLockUnlock(configShadow(device)->dependentsLock);

	return (ret);
}

// public
IOReturn IOPCIBridge::restoreDeviceState(IOPCIDevice * device, IOOptionBits options)
{
	IOReturn      ret = kIOReturnNotFound;
	IOPCIDevice * root = nullptr;
	IOPCIHostBridgeData *vars = reserved->hostBridgeData;

	if (!device->savedConfig) return (kIOReturnNotReady);

	if (kSaveBridgeState == vars->_allPCI2PCIBridgeState)
	{
		ret = restoreMachineState(kMachineRestoreDehibernate | kMachineRestoreBridges, device);
	}

	if (kIOReturnSuccess != ret)
	{
		if ((root = configShadow(device)->tunnelRoot))
		{
			if (options & kIOPCIConfigShadowVolatile)
			{
				ret = _restoreDeviceDependents(root, options, device);
			}
			else
			{
				restoreQRemove(device);
				ret = _restoreDeviceState(device, 0);
			}
		}
		else
		{
			restoreQRemove(device);
			ret = _restoreDeviceState(device, 0);

			if( device->getProperty(gIOPCITunnelControllerKey, gIOServicePlane, 0) )
			{
				// if we don't have a tunnel root but we are a tunnel controller
				// then we know that we are an integrated solution
				// unblock the tunnelWait people
				vars->tunnelSleepIncrement(device->getName(), false);
			}
		}
	}

	if (!(kIOPCIConfigShadowPermanent & configShadow(device)->flags)) 
	{
		configShadow(device)->flags &= ~kIOPCIConfigShadowValid;
	}

	// callers expect success
	return (kIOReturnSuccess);
}

#if ACPI_SUPPORT
// See IOPCIBridge_Legacy.cpp
#else
IOReturn
IOPCIBridge::callPlatformFunction(const OSSymbol * functionName,
								  bool waitForFunction,
								  void * p1, void * p2,
								  void * p3, void * p4)
{
	return super::callPlatformFunction(functionName, waitForFunction, p1, p2, p3, p4);
}
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIBridge::configure( IOService * provider )
{
    return (true);
}

#if !defined(__arm64__)
SInt32 IOPCIBridge::compareAddressCell( UInt32 /* cellCount */, UInt32 cleft[], UInt32 cright[] )
{
     IOPCIPhysicalAddress *  left        = (IOPCIPhysicalAddress *) cleft;
     IOPCIPhysicalAddress *  right       = (IOPCIPhysicalAddress *) cright;
     static const UInt8      spacesEq[]  = { 0, 1, 2, 2 };
     if (spacesEq[ left->physHi.s.space ] != spacesEq[ right->physHi.s.space ])
         return (-1);

    return (left->physLo - right->physLo);
}
#else
SInt64 IOPCIBridge::compareAddressCell( UInt32 /* cellCount */, UInt32 cleft[], UInt32 cright[] )
{
    IOPCIPhysicalAddress *  left        = (IOPCIPhysicalAddress *) cleft;
    IOPCIPhysicalAddress *  right       = (IOPCIPhysicalAddress *) cright;
    static const UInt8      spacesEq[]  = { 0, 1, 2, 2 };

    if (spacesEq[ left->physHi.s.space ] != spacesEq[ right->physHi.s.space ])
        return (-1);

    return IOPhysical32(left->physMid, left->physLo) - IOPhysical32(right->physMid, right->physLo);
}
#endif

void IOPCIBridge::spaceFromProperties( OSDictionary * propTable,
                                       IOPCIAddressSpace * space )
{
    OSData *                    regProp;
    IOPCIAddressSpace *         inSpace;

    space->bits = 0;

    if ((regProp = (OSData *) propTable->getObject("reg")))
    {
        inSpace = (IOPCIAddressSpace *) regProp->getBytesNoCopy();
        space->s.busNum = inSpace->s.busNum;
        space->s.deviceNum = inSpace->s.deviceNum;
        space->s.functionNum = inSpace->s.functionNum;
    }
}

void IOPCIBridge::spaceFromProperties( IORegistryEntry * regEntry,
                                       IOPCIAddressSpace * space )
{
    OSData *                    regProp;
    IOPCIAddressSpace *         inSpace;

    space->bits = 0;

    if ((regProp = (OSData *) regEntry->copyProperty("reg")))
    {
        inSpace = (IOPCIAddressSpace *) regProp->getBytesNoCopy();
        space->s.busNum = inSpace->s.busNum;
        space->s.deviceNum = inSpace->s.deviceNum;
        space->s.functionNum = inSpace->s.functionNum;
        regProp->release();
    }
}

IORegistryEntry * IOPCIBridge::findMatching( OSIterator * kids,
        IOPCIAddressSpace space )
{
    IORegistryEntry *           found = 0;
    IOPCIAddressSpace           regSpace;

    if (kids)
    {
        kids->reset();
        while ((0 == found)
                && (found = (IORegistryEntry *) kids->getNextObject()))
        {
            spaceFromProperties(found, &regSpace);
            if (space.bits != regSpace.bits)
                found = 0;
        }
    }
    return (found);
}

bool IOPCIBridge::checkProperties( IOPCIDevice * entry )
{
    uint32_t    vendor, product, classCode, revID;
    uint32_t    subVendor = 0, subProduct = 0;
    IOByteCount offset;
    OSData *    data;

    if ((data = OSDynamicCast(OSData, entry->getProperty("vendor-id"))))
        vendor = *((uint32_t *) data->getBytesNoCopy());
    else
        return (false);
    if ((data = OSDynamicCast(OSData, entry->getProperty("device-id"))))
        product = *((uint32_t *) data->getBytesNoCopy());
    else
        return (false);
    if ((data = OSDynamicCast(OSData, entry->getProperty("class-code"))))
        classCode = *((uint32_t *) data->getBytesNoCopy());
    else
        return (false);
    if ((data = OSDynamicCast(OSData, entry->getProperty("revision-id"))))
        revID = *((uint32_t *) data->getBytesNoCopy());
    else
        return (false);
    if ((data = OSDynamicCast(OSData, entry->getProperty("subsystem-vendor-id"))))
        subVendor = *((uint32_t *) data->getBytesNoCopy());
    if ((data = OSDynamicCast(OSData, entry->getProperty("subsystem-id"))))
        subProduct = *((uint32_t *) data->getBytesNoCopy());

    if (entry->savedConfig)
    {
        // update matching config space regs from properties
        entry->savedConfig[kIOPCIConfigVendorID >> 2] = (product << 16) | vendor;
        entry->savedConfig[kIOPCIConfigRevisionID >> 2] = (classCode << 8) | revID;
        if (subVendor && subProduct)
            entry->savedConfig[kIOPCIConfigSubSystemVendorID >> 2] = (subProduct << 16) | subVendor;
    }

    if ((offset = entry->reserved->expressCapability))
    {
        uint32_t value, expressCaps;

		expressCaps = entry->configRead16(offset + 0x02);
        entry->setProperty(kIOPCIExpressCapabilitiesKey, expressCaps, 32);
        value = entry->configRead16(offset + 0x12);
        entry->setProperty(kIOPCIExpressLinkStatusKey, value, 32);
        value = entry->configRead32(offset + 0x0c);
        entry->setProperty(kIOPCIExpressLinkCapabilitiesKey, value, 32);
		if (0x100 & expressCaps)
		{
			value = entry->configRead16(offset + 0x1a);
			entry->setProperty(kIOPCIExpressSlotStatusKey, value, 32);
			value = entry->configRead32(offset + 0x14);
			entry->setProperty(kIOPCIExpressSlotCapabilitiesKey, value, 32);
		}
    }

    return (true);
}

#if ACPI_SUPPORT
#ifndef kIOPMRootDomainWakeTypeNetwork
#define kIOPMRootDomainWakeTypeNetwork      "Network"
#endif
#endif

void IOPCIBridge::updateWakeReason(IOPCIDevice * device)
{
	const char * reason;

	reason = device->getName();
	getPMRootDomain()->claimSystemWakeEvent(this, kIOPMWakeEventSource, reason);

#if ACPI_SUPPORT
	reserved->hostBridgeData->lockWakeReasonLock();
	if ((kIOPCIClassNetwork == (device->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24))
			&& (!getPMRootDomain()->getProperty(kIOPMRootDomainWakeTypeKey)))
	{
		getPMRootDomain()->setProperty(kIOPMRootDomainWakeTypeKey, kIOPMRootDomainWakeTypeNetwork);
	}
	reserved->hostBridgeData->unlockWakeReasonLock();
#endif
}

OSDictionary * IOPCIBridge::constructProperties( IOPCIAddressSpace space )
{
    return (0);
}

IOPCIDevice * IOPCIBridge::createNub( OSDictionary * from )
{
    return (new IOPCIDevice);
}

bool IOPCIBridge::initializeNub( IOPCIDevice * nub,
                                 OSDictionary * from )
{
    spaceFromProperties( from, &nub->space);
    nub->parent = this;

    if (ioDeviceMemory())
        nub->ioMap = ioDeviceMemory()->map();

    return (true);
}

void IOPCIBridge::removeDevice( IOPCIDevice * device, IOOptionBits options )
{
    IOReturn ret = kIOReturnSuccess;

#if USE_MSI
    if (device->reserved->msiCapability && reserved->messagedInterruptController)
        ret = reserved->messagedInterruptController->deallocateDeviceInterrupts(device);
#endif /* USE_MSI */

    getPlatform()->callPlatformFunction(gIOPlatformFreeDeviceResourcesKey,
                                          /* waitForFunction */ false,
                                          /* nub             */ device, NULL, NULL, NULL);

    device->callPlatformFunction(gIOPCIDeviceChangedKey,
                                  /* waitForFunction */ false,
                                  /* nub             */ device,
                                  kOSBooleanFalse, NULL, NULL);

#if ACPI_SUPPORT
    AppleVTD::removeDevice(device);
#endif

    restoreQRemove(device);
    configOpParams cp = {.device = device, .op = kConfigOpTerminated, .result = nullptr};
    configOp(&cp);
}

bool IOPCIBridge::publishNub( IOPCIDevice * nub, UInt32 /* index */ )
{
	IOPCIDevice *               root;
    char                        location[ 24 ];
    bool                        ok;
    OSNumber *                  num;

    if (nub)
    {
        if (nub->space.s.functionNum)
            snprintf( location, sizeof(location), "%X,%X", nub->space.s.deviceNum,
                     nub->space.s.functionNum );
        else
            snprintf( location, sizeof(location), "%X", nub->space.s.deviceNum );
        nub->setLocation( location );
        IODTFindSlotName( nub, nub->space.s.deviceNum );

        // set up config space shadow

        IOPCIConfigShadow * shadow = IOMallocType(IOPCIConfigShadow);
        if (shadow)
        {
            queue_init(&shadow->dependents);
            shadow->device = nub;

			shadow->tunnelled = (0 != nub->getProperty(gIOPCITunnelledKey));
			if ((num = (OSNumber *) nub->copyProperty(gIOPCIHPTypeKey)))
			{
				shadow->hpType = num->unsigned8BitValue();
				num->release();
			}
			if (kPCIStaticShared == shadow->hpType) reserved->hostBridgeData->_isUSBCSystem = true;

			for (root = nub;
				 (!root->getProperty(gIOPCIThunderboltKey)) 
				 	&& (root = OSDynamicCast(IOPCIDevice, root->getParentEntry(gIODTPlane)));) 
			    {}
			shadow->sharedRoot = root;
			if (root == nub)
			{
			    shadow->dependentsLock = IOLockAlloc();
			    assert(shadow->dependentsLock);
			}
			else if ((kPCIStatic != shadow->hpType)
			     && (kPCIStaticShared != shadow->hpType))
			{
			    shadow->tunnelRoot = root;
			}

			nub->reserved->tunnelL1Allow = kTunnelL1NotSet;
            nub->savedConfig = (UInt32 *) &shadow->configSave.savedConfig[0];
            for (int i = 0; i < kIOPCIConfigEPShadowRegs; i++)
            {
                if (kIOPCISaveRegsMask & (1 << i))
                	nub->savedConfig[i] = nub->configRead32( i << 2 );
			}
        }
        nub->setProperty(kIOServiceDEXTEntitlementsKey, kIOPCITransportDextEntitlement);

        // Add DriverKit entitlement property
        OSArray* entitlementArray    = OSArray::withCapacity(1);
        if(entitlementArray == NULL)
        {
            DLOG("unable to allocate entitlmentArray");
            return false;
        }

        OSArray* entitlementSubArray = OSArray::withCapacity(3);
        if(entitlementSubArray == NULL)
        {
            DLOG("unable to allocate entitlementSubArray");
            OSSafeReleaseNULL(entitlementArray);
            return false;
        }

        OSString* entitlementString  = OSString::withCString(kIOPCITransportDextEntitlement);
        if(entitlementString)
        {
            entitlementSubArray->setObject(entitlementString);
            OSSafeReleaseNULL(entitlementString);
        }

        if (nub->reserved->headerType != kPCIHeaderType0)
        {
            entitlementString = OSString::withCString(kIOPCITransportBridgeDextEntitlement);
            if (entitlementString != NULL)
            {
                entitlementSubArray->setObject(entitlementString);
                OSSafeReleaseNULL(entitlementString);
            }
        }

        // Note: built-in entitlement check is done in IOPCIDevice::matchPropertyTable(OSDictionary* table)
        // this is because the platform expert determines if a device is "built-in" or not
        // when the PCI device is registered for service matching
        entitlementArray->setObject(entitlementSubArray);
        nub->setProperty(kIOServiceDEXTEntitlementsKey, entitlementArray);
        OSSafeReleaseNULL(entitlementSubArray);
        OSSafeReleaseNULL(entitlementArray);

#if TARGET_OS_HAS_THUNDERBOLT
        if (kPCIHeaderType0 == nub->reserved->headerType)
        {
            // the tunnel VID/DID property is set by the thunderbolt stack on the new tunnel bridge.
            // Only add the properties for non-bridges until rdar://74079249 (Thunderbolt Model ID & Vendor ID properties needed on tunnelled  IOPCIDevices)
            nub->setProperty(kIOPCITunnelRootDeviceVendorIDKey, nub->getProperty(kIOThunderboltTunnelEndpointDeviceVIDProperty, gIODTPlane));
            nub->setProperty(kIOPCITunnelRootDeviceModelIDKey, nub->getProperty(kIOThunderboltTunnelEndpointDeviceMIDProperty, gIODTPlane));

            if (shadow && shadow->tunnelled)
            {
                OSNumber *tbEntryID = OSDynamicCast(OSNumber, getProperty("Thunderbolt Entry ID", gIOServicePlane));
                if (tbEntryID)
                {
                    OSDictionary *matching = registryEntryIDMatching(tbEntryID->unsigned64BitValue());
                    IOService *tbNode = copyMatchingService(matching);
                    OSSafeReleaseNULL(matching);
                    if (tbNode)
                    {
                        OSNumber *clx = OSDynamicCast(OSNumber, tbNode->getProperty(kIOThunderboltPortCLxStateProperty, gIOServicePlane));
                        tbNode->release();
                        if (clx && clx->unsigned32BitValue()) nub->setProperty(kIOCLxEnabledKey, kOSBooleanTrue);
                    }
                }
            }
        }
#endif

        checkProperties( nub );

        if (shadow && (kIOPCIClassBridge == (nub->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24)))
        {
            shadow->flags |= kIOPCIConfigShadowBridge;
#if 0
            if (kIOPCISubClassBridgeMCA >= (0xff & (nub->savedConfig[kIOPCIConfigRevisionID >> 2] >> 16)))
            {
                shadow->flags |= kIOPCIConfigShadowHostBridge;
            }
#endif
        }

        ok = nub->attach( this );

        if (ok)
        {
            nub->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
                    (void *) kIOMessageDeviceWillPowerOff, nub, (void *) 0, (void *) 0);

            nub->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
                    (void *) kIOMessageDeviceHasPoweredOn, nub, (void *) 0, (void *) 0);

            if (!nub->getProperty("pci-deferred-registration"))
            {
                nub->registerService();
            }
        }
    }
    else
        ok = false;

    return (ok);
}

UInt8 IOPCIBridge::firstBusNum( void )
{
    return (0);
}

UInt8 IOPCIBridge::lastBusNum( void )
{
    return (255);
}

IOReturn IOPCIBridge::kernelRequestProbe(IOPCIDevice * device, uint32_t options)
{
	IOReturn    ret = kIOReturnUnsupported;

	DLOG("%s::kernelRequestProbe(%x)\n", device->getName(), options);

	if ((kIOPCIProbeOptionEject & options) && device->getProperty(kIOPCIEjectableKey))
	{
		configOpParams cp = {.device = device, .op = kConfigOpEject, .result = nullptr};
		ret = configOp(&cp);
		device = OSDynamicCast(IOPCIDevice, getProvider());
		if (!device)
			return (ret);
		options |= kIOPCIProbeOptionNeedsScan;
		options &= ~kIOPCIProbeOptionEject;
	}

	if (kIOPCIProbeOptionNeedsScan & options)
	{
		bool bootDefer = (0 != device->getProperty(kIOPCITunnelBootDeferKey));
		if (bootDefer)
		{
			IOPCI2PCIBridge * p2pBridge;
			if ((p2pBridge = OSDynamicCast(IOPCI2PCIBridge, this)))
				p2pBridge->startBootDefer(device);

			return (kIOReturnSuccess);
		}

		configOpParams cp = {.device = device, .op = kConfigOpNeedsScan, .result = nullptr};
		ret = configOp(&cp);
	}

	if (kIOPCIProbeOptionDone & options)
	{
		configOpParams cp = {.device = device, .op = kConfigOpScan, .result = nullptr};
		ret = configOp(&cp);
	}

	return (ret);
}

IOReturn IOPCIBridge::protectDevice(IOPCIDevice * device, uint32_t space, uint32_t prot)
{
	IOReturn ret;

	prot &= (VM_PROT_READ|VM_PROT_WRITE);
	prot <<= kPCIDeviceStateConfigProtectShift;

	DLOG("%s::protectDevice(%x, %x)\n", device->getName(), space, prot);

	configOpParams cp = {.device = device, .op = kConfigOpProtect, .result = &prot};
	ret = configOp(&cp);

	return (ret);
}

bool IOPCIBridge::childPrefersMSIX( IOPCIDevice *device )
{
    bool   prefersMSIX = false;
    UInt32 regVal = 0;

    static const uint32_t denyList[] = {
        0x168614e4, // ref: rdar://80923685
    };

    regVal = device->reserved->configEntry->classCode;
    if (0x020000 == regVal)  // Ethernet controller (class / sub-class / prog-if)
    {
        prefersMSIX = true;
    }

    // Even if the child prefers MSIX support, check that it doesn't
    // fall in a deny list which are known to have issues
    regVal = device->reserved->configEntry->vendorProduct;
    for (uint32_t i = 0; i < sizeof(denyList)/sizeof(*denyList); i++)
    {
        if (regVal == denyList[i])
        {
            prefersMSIX = false;
            break;
        }
    }

    return prefersMSIX;
}

// Search for the device's ASPM default bootarg. Only bits [1:0] and [8] are
// used, which correspond to the link control register's ASPM Control and
// Enable Clock Power Management bits, respectively.
IOReturn parseDevASPMDefaultBootArg(uint32_t vidDid, uint16_t *expressASPMDefault)
{
	char str[256];

	if (!PE_i_can_has_debugger(nullptr))
		return kIOReturnError;

	if (PE_parse_boot_argn("pci-dev-aspm-default", &str, sizeof(str)))
	{
		const char *usageStr = "pci-dev-aspm-default=<vid:did>,<value>[;<vid:did>,<value>]";
        char *token, *nextToken;
		unsigned long id, val;

        str[sizeof(str) - 1] = '\0';

		token = str;

		while (true)
		{
			bool found = true;

			id = strtoul(token, &nextToken, 0);
			if (id != (vidDid & 0xFFFF))
			{
				found = false;
			}

			if (nextToken[0] != ':' || (nextToken == token))
			{
				DLOG("Malformed boot-arg, expected: %s\n", usageStr);
				break;
			}

			token = &nextToken[1];

			id = strtoul(token, &nextToken, 0);
			if (id != ((vidDid >> 16) & 0xFFFF))
			{
				found = false;
			}

			if (nextToken[0] != ',' || (nextToken == token))
			{
				DLOG("Malformed boot-arg, expected: %s\n", usageStr);
				break;
			}

			token = &nextToken[1];

			val = strtoul(token, &nextToken, 0);
			if (token == nextToken)
			{
				DLOG("Malformed boot-arg, expected: %s\n", usageStr);
				break;
			}

			if (found)
			{
				*expressASPMDefault = (uint16_t)val;
				return kIOReturnSuccess;
			}

			if (nextToken[0] != ';')
			{
				break;
			}

			token = &nextToken[1];
		}
	}

	return kIOReturnError;
}

void IOPCIBridge::probeBusGated( probeBusParams *params )
{
    IORegistryEntry *  found;
    OSDictionary *     propTable;
    IOPCIDevice *      nub = 0;
    OSIterator *       kidsIter;
    IOService *        provider = params->provider;
    uint8_t            busNum = params->busNum;
    UInt32             index = 0;
    UInt32             idx = 0;
    bool               hotplugBus;

//    kprintf("probe: %s\n", provider->getName());

    hotplugBus = (0 != getProperty(kIOPCIHotPlugKey));
    if (hotplugBus && !provider->getProperty(kIOPCIOnlineKey))
    {
        DLOG("offline\n");    
        return;
    }

    IODTSetResolving(provider, &compareAddressCell, NULL);

    kidsIter = provider->getChildIterator( gIODTPlane );

    // find and copy over any devices from the device tree
    OSArray * nubs = OSArray::withCapacity(0x10);
    assert(nubs);

    if (kidsIter) {
        kidsIter->reset();
        while ((found = (IORegistryEntry *) kidsIter->getNextObject()))
        {
//            kprintf("probe: %s, %s\n", provider->getName(), found->getName());
            if (!found->getProperty("vendor-id")) continue;
            if (found->inPlane(gIOServicePlane))  continue;
            nub = OSDynamicCast(IOPCIDevice, found);
            if (!nub) continue;
            if (nub->reserved->configEntry == NULL) continue;
            propTable = found->getPropertyTable();
			nub->retain();
			initializeNub(nub, propTable);

            {
                IOByteCount capa, msiCapa;
				OSData *    data;

                nubs->setObject(index++, nub);

			    nub->reserved->headerType = (0x7F & nub->configRead8(kIOPCIConfigHeaderType));
                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIPowerManagementCapability, &capa))
                    nub->reserved->powerCapability = capa;

                msiCapa = 0;
                nub->extendedFindPCICapability(kIOPCIMSICapability, &msiCapa);
                capa = 0;
                if ((!msiCapa
                     || (childPrefersMSIX(nub))
                ) && nub->extendedFindPCICapability(kIOPCIMSIXCapability, &capa))
				{
                     nub->reserved->msiCapability = capa;
                     nub->reserved->msiMode      |= kMSIX;
                }
				else nub->reserved->msiCapability = msiCapa;

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIExpressLatencyTolerenceReportingCapability, &capa))
                    nub->reserved->latencyToleranceCapability = capa;

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIExpressAccessControlServicesCapability, &capa)) {
                    nub->reserved->acsCapability = capa;
                    if ((kIOPCIConfiguratorNoACS & gIOPCIFlags) == 0) {
                        nub->enableACS(nub, true);
                    }
                }

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIExpressL1PMSubstatesCapability, &capa))
                {
                    nub->reserved->l1pmCapability = capa;
                    nub->reserved->l1pmCaps = (0xFFFFFFF0 | nub->configRead32(capa + 0x04));
                }

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIPCIExpressCapability, &capa))
                {
                    nub->reserved->expressCapability = capa;
                    nub->reserved->expressCapabilities = nub->configRead16(capa + 0x02);
                    uint32_t linkCaps = nub->configRead32(capa + 0xc);
                    nub->reserved->aspmCaps = ((kIOPCIExpressASPML0s|kIOPCIExpressASPML1)
                                            & (linkCaps >> 10));
                    if ((1 << 18) & linkCaps) nub->reserved->aspmCaps |= kIOPCIExpressClkReq;
#if ACPI_SUPPORT
                    // current aspm mode
                    nub->reserved->expressASPMDefault = ((kIOPCIExpressClkReq|kIOPCIExpressASPML0s|kIOPCIExpressASPML1)
                                                        & (nub->configRead16(capa + 0x10)));
#else
                    nub->reserved->expressASPMDefault = nub->reserved->aspmCaps & ~kIOPCIExpressClkReq;
#endif
                }
				if (nub->reserved->expressCapability && nub->reserved->latencyToleranceCapability
				 && (data = OSDynamicCast(OSData, nub->getProperty(kIOPCIExpressMaxLatencyKey, gIODTPlane))))
				{
					nub->extendedConfigWrite32(nub->reserved->latencyToleranceCapability + 0x04, 
                                                 *((uint32_t *) data->getBytesNoCopy()));
					enableLTR(nub, true);
				}

                if (nub->reserved->expressCapability)
                {
					// ASPM default setting precedence:
					// 1. Per-device boot-arg ("pci-dev-aspm-default")
					// 2. Per-device EDT property
					// 3. Global boot-arg ("pci-aspm-default")
					// 4. Device's hardware capabilities (see code above)

					uint32_t vidDid = nub->configRead32(kIOPCIConfigVendorID);
					uint16_t expressASPMDefault;
					if (parseDevASPMDefaultBootArg(vidDid, &expressASPMDefault) == kIOReturnSuccess)
					{
						nub->reserved->expressASPMDefault = expressASPMDefault;
						setDeviceASPMState(nub, this, nub->reserved->expressASPMDefault);
						nub->setProperty(kIOPCIExpressASPMDefaultKey, nub->reserved->expressASPMDefault, 32);
					}
					else if ((data = OSDynamicCast(OSData, nub->getProperty(kIOPCIExpressASPMDefaultKey))))
					{
						nub->reserved->expressASPMDefault = *((uint32_t *) data->getBytesNoCopy());
						setDeviceASPMState(nub, this, nub->reserved->expressASPMDefault);
					}
					else			
					{
						if (reserved->hostBridgeData && reserved->hostBridgeData->_aspmDefault != 0xFFFF)
						{
							nub->reserved->expressASPMDefault = reserved->hostBridgeData->_aspmDefault;
							setDeviceASPMState(nub, this, nub->reserved->expressASPMDefault);
						}
						nub->setProperty(kIOPCIExpressASPMDefaultKey, nub->reserved->expressASPMDefault, 32);
					}

					// Enable extended tag fields, if supported
					uint32_t deviceCaps            = nub->reserved->configEntry->expressDeviceCaps1;
					uint32_t deviceCaps2           = nub->reserved->configEntry->expressDeviceCaps2;
					bool     useDefaultETFSettings = gIOPCIFlags & kIOPCIConfiguratorDefaultETF;

					if (!(useDefaultETFSettings) && (deviceCaps & 0x20))
					{
						// Need to set ETFE for either 8b or 10b tags
						uint16_t deviceControl = nub->configRead16(nub->reserved->expressCapability + 0x08);
						deviceControl |= 0x100;
						nub->configWrite16(nub->reserved->expressCapability + 0x08, deviceControl);

						// Enable 10b Tag Fields if the device can request them and its root port can complete them
						struct IOPCIConfigEntry *rootPortEntry = nub->reserved->configEntry->rootPortEntry;

						if ((deviceCaps2 & 0x20000) && (rootPortEntry->expressDeviceCaps2 & 0x10000))
						{
							uint16_t deviceControl2 = nub->configRead16(nub->reserved->expressCapability + 0x28);
							deviceControl2 |= 0x1000;
							nub->configWrite16(nub->reserved->expressCapability + 0x28, deviceControl2);
						}
					}
                }

				if (kPCIHeaderType1 == nub->reserved->headerType)
				{
					nub->reserved->rootPort = ((0xF0 & nub->reserved->expressCapabilities) == 0x40);
					uint16_t bridgeControl = nub->configRead16(kPCI2PCIBridgeControl);
					bridgeControl |= 0x0002;	// SERR forward
					nub->configWrite16(kPCI2PCIBridgeControl, bridgeControl);
				}
                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIExpressErrorReportingCapability, &capa))
                {
                    nub->reserved->aerCapability = capa;

					uint32_t dcEnables = 0;
					uint32_t sdata = 0;

					enum { kDeviceControlAllErrors	= ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)) };
					if (kIOPCIConfiguratorAER & gIOPCIFlags) dcEnables |= kDeviceControlAllErrors;

					if ((data = OSDynamicCast(OSData, nub->getProperty(kIOPCIExpressErrorControlKey, gIODTPlane))))
					{
						nub->configWrite32(nub->reserved->aerCapability + 0x18, 
													*((uint32_t *) data->getBytesNoCopy()));
					}
					if ((data = OSDynamicCast(OSData, nub->getProperty(kIOPCIExpressErrorUncorrectableSeverityKey, gIODTPlane))))
					{
						nub->configWrite32(nub->reserved->aerCapability + 0x0C, 
													*((uint32_t *) data->getBytesNoCopy()));
					}
					if ((data = OSDynamicCast(OSData, nub->getProperty(kIOPCIExpressErrorUncorrectableMaskKey, gIODTPlane))))
					{
						sdata = ((uint32_t *) data->getBytesNoCopy())[0];
						nub->configWrite32(nub->reserved->aerCapability + 0x04, sdata);
						nub->configWrite32(nub->reserved->aerCapability + 0x08, sdata);
					}
					if ((data = OSDynamicCast(OSData, nub->getProperty(kIOPCIExpressErrorCorrectableMaskKey, gIODTPlane))))
					{
						sdata = ((uint32_t *) data->getBytesNoCopy())[0];
						nub->configWrite32(nub->reserved->aerCapability + 0x10, sdata);
						nub->configWrite32(nub->reserved->aerCapability + 0x14, sdata);
					}

					if (dcEnables)
					{
						uint32_t deviceControl = nub->configRead32(nub->reserved->expressCapability + 0x08);
						deviceControl |= dcEnables;
						nub->configWrite32(nub->reserved->expressCapability + 0x08, deviceControl);
#if 0
						uint16_t cmd = nub->configRead32(kIOPCIConfigCommand);
						cmd |= kIOPCICommandSERR;
						nub->configWrite32(kIOPCIConfigCommand, cmd);
#endif
					}
				}

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIFPBCapability, &capa))
                {
                    nub->reserved->fpbCapability = capa;
                }

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIExpressCapabilityIDPrecisionTimeManagement, &capa))
                {
                    nub->reserved->ptmCapability = capa;
                }

                nub->release();
            }
        }
    }

    idx = 0;
    while ((nub = (IOPCIDevice *)nubs->getObject(idx++)))
    {
        if (hotplugBus || provider->getProperty(kIOPCIEjectableKey))
        {
			nub->setProperty(kIOPCIEjectableKey, kOSBooleanTrue);
        }

        publishNub(nub , idx);
    }

    nubs->release();
    if (kidsIter)
        kidsIter->release();
}

void IOPCIBridge::probeBus( IOService * provider, UInt8 busNum )
{
    IOPCIHostBridgeData *vars = reserved->hostBridgeData;
    probeBusParams params;

    params.provider = provider;
    params.busNum = busNum;

    vars->_configWorkLoop->runAction(
        OSMemberFunctionCast(IOCommandGate::Action, this, &IOPCIBridge::probeBusGated),
        this, &params);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIBridge::addBridgeIORange( IOByteCount start, IOByteCount length )
{
    bool ok;

#if ACPI_SUPPORT
    // fix - ACPIPCI makes this up for hosts with zero space
    if ((0x0 == start) && (0x10000 == length))
        return (false);
#endif

    ok = IOPCIRangeListAddRange(&reserved->rangeLists[kIOPCIResourceTypeIO],
                                kIOPCIResourceTypeIO,
                                start, length);
    return (ok);
}

bool IOPCIBridge::addBridgeMemoryRange( IOPhysicalAddress start,
                                        IOPhysicalLength length, bool host )
{
    bool ok;

    // fix - ACPIPCI makes this up for hosts with zero space
    if ((0x80000000 == start) && (0x7f000000 == length))
        return (false);

    ok = IOPCIRangeListAddRange(&reserved->rangeLists[kIOPCIResourceTypeMemory],
                                kIOPCIResourceTypeMemory, 
                                start, length);
    return (ok);

}

bool IOPCIBridge::addBridgePrefetchableMemoryRange( addr64_t start,
                                                    addr64_t length )
{
    bool ok;
    ok = IOPCIRangeListAddRange(&reserved->rangeLists[kIOPCIResourceTypePrefetchMemory], 
                                kIOPCIResourceTypePrefetchMemory, 
                                start, length);
    return (ok);
}

bool IOPCIBridge::addBridgePrefetchableMemoryRange( IOPhysicalAddress start,
                                                    IOPhysicalLength length,
                                                    bool host )
{
    return (addBridgePrefetchableMemoryRange(start, length));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIBridge::constructRange( IOPCIAddressSpace * flags,
                                  IOPhysicalAddress64 phys,
                                  IOPhysicalLength64 len,
                                  OSArray * array )
{
    IOMemoryDescriptor *    md;
    IOMemoryDescriptor *    ioMemory;
	IOSubMemoryDescriptor * subMem;
	IOAddressRange 		    range;
    bool                    ok;
    unsigned int            idx;

    for (idx = 0;
    	 (md = (IODeviceMemory *) array->getObject(idx))
          	&& (flags->s.registerNum != (md->getTag() & 0xff));
         idx++) {}

	if (md)
	{
		md->retain();
		md->redirect(TASK_NULL, true);

		DLOG("reloc at (%u:%u:%u:0x%x) 0x%qx, 0x%qx -> 0x%qx, 0x%qx\n",
			 flags->s.busNum, flags->s.deviceNum, flags->s.functionNum, flags->s.registerNum,
			 md->getPhysicalSegment(0, 0, kIOMemoryMapperNone), (uint64_t) md->getLength(),
			 phys, len);

		if ((subMem = OSDynamicCast(IOSubMemoryDescriptor, md)))
		{
			ok = subMem->initSubRange(ioDeviceMemory(), phys, len, (IODirection) kIOMemoryThreadSafe);
		}
		else
		{
			range.address = phys;
			range.length  = len;
			ok = md->initWithOptions(&range, 1, 0, TASK_NULL, 
									 kIOMemoryTypePhysical64
									 // | kIOMemoryRedirected
									 | kIODirectionNone
									 | kIOMemoryMapperNone,
									 NULL);
		}
		if (!ok) panic("IOMD::initWithOptions");
		md->redirect(TASK_NULL, false);
	}
	else
	{
		if (kIOPCIIOSpace == flags->s.space)
		{
			if (!(ioMemory = ioDeviceMemory()))
				md = 0;
			else
			{
				phys &= 0x00ffffff; // seems bogus
				md = IOSubMemoryDescriptor::withSubRange(ioMemory, phys, len, kIOMemoryThreadSafe);
				if (md == 0)
				{
					/* didn't fit */
					md = IOMemoryDescriptor::withAddressRange(
								phys + ioMemory->getPhysicalSegment(0, 0, kIOMemoryMapperNone),
								len, kIODirectionNone | kIOMemoryHostOnly, NULL );
				}
			}
		}
		else
		{
			md = IOMemoryDescriptor::withAddressRange(
								phys, len, kIODirectionNone | kIOMemoryMapperNone, NULL);
		}
        ok = array->setObject(md);
	}

    if (md)
	{
        md->setTag( flags->bits );
        md->release();
    }
    else
        ok = false;

    return (ok);
}

IOReturn IOPCIBridge::getDTNubAddressing( IOPCIDevice * regEntry )
{
    OSArray *           array;
    IORegistryEntry *   parentEntry;
    OSData *            addressProperty;
#if defined(__i386__) || defined(__x86_64__)
    IOPhysicalAddress64 phys;
    IOPhysicalLength64  len;
#else
    IOPhysicalAddress phys;
    IOPhysicalLength  len;
#endif
    UInt32              cells = 5;
    int                 i, num;
    UInt32 *            reg;

    addressProperty = (OSData *) regEntry->getProperty( "assigned-addresses" );
    if (0 == addressProperty)
        return (kIOReturnSuccess);

    parentEntry = regEntry->getParentEntry( gIODTPlane );
    if (0 == parentEntry)
        return (kIOReturnBadArgument);

    array = (OSArray *) regEntry->copyProperty(gIODeviceMemoryKey);
	if (array)
	{
		OSArray * newArray;
		newArray = OSArray::withArray(array);
		array->release();
		array = newArray;
	}

	if (!array) array = OSArray::withCapacity(4);
    if (!array) return (kIOReturnNoMemory);

    reg = (UInt32 *) addressProperty->getBytesNoCopy();
    num = addressProperty->getLength() / (sizeof(UInt32) * cells);

    for (i = 0; i < num; i++)
    {
#if defined(__i386__) || defined(__x86_64__)
		phys = ((IOPhysicalAddress64) reg[1] << 32) | reg[2];
		len = ((IOPhysicalLength64) reg[3] << 32) | reg[4];
		constructRange( (IOPCIAddressSpace *) reg, phys, len, array );
#else
		if (IODTResolveAddressCell(parentEntry, reg, &phys, &len))
			constructRange( (IOPCIAddressSpace *) reg, phys, len, array );
#endif
        reg += cells;
    }

    if (array->getCount())
        regEntry->setProperty( gIODeviceMemoryKey, array);

    array->release();

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::getNubAddressing( IOPCIDevice * nub )
{
    return (kIOReturnError);
}

bool IOPCIBridge::isDTNub( IOPCIDevice * nub )
{
    return (true);
}

IOReturn IOPCIBridge::getNubResources( IOService * service )
{
    IOPCIDevice *       nub = (IOPCIDevice *) service;
    IOReturn            err;

    if (service->getProperty(kIOPCIResourcedKey)) {
#if ACPI_SUPPORT
        // Update the mapper in case a driver has matched
        AppleVTD::adjustDevice(nub);
#endif
        return (kIOReturnSuccess);
    }
    service->setProperty(kIOPCIResourcedKey, kOSBooleanTrue);

    err = getDTNubAddressing( nub );

    if (service->IOService::getProperty(gIOInterruptSpecifiersKey))
    {
        // Effectively this performs non-lazy initialization for ports (bridges)
        // of embedded PCIe Root Complexes.
        // Interrupt vectors of any other device get initialized lazily.
        (void)service->getProperty(gIOInterruptSpecifiersKey);
    }

    nub->callPlatformFunction(gIOPCIDeviceChangedKey,
                              /* waitForFunction */ false,
                              /* nub             */ nub,
                              kOSBooleanTrue, NULL, NULL);

    if(err == kIOReturnSuccess)
    {
        OSArray* deviceMemoryArray = nub->getDeviceMemory();
        if(deviceMemoryArray != NULL)
        {
            for(int i = 0; i < deviceMemoryArray->getCount(); i++)
            {
                IODeviceMemory* currentDeviceMemory = reinterpret_cast<IODeviceMemory*>(deviceMemoryArray->getObject(i));
                if(currentDeviceMemory != NULL)
                {
                    IOPCIAddressSpace addressSpace;
                    addressSpace.bits = static_cast<uint32_t>(currentDeviceMemory->getTag());

                    currentDeviceMemory->retain();
                    nub->reserved->deviceMemory[i] = currentDeviceMemory;
                    if(      (addressSpace.s.space == kIOPCI32BitMemorySpace)
                       || (addressSpace.s.space == kIOPCI64BitMemorySpace))
                    {
                        IOMemoryMap* currentDeviceMemoryMap = currentDeviceMemory->map();
                        if(currentDeviceMemoryMap != NULL)
                        {
                            currentDeviceMemoryMap->retain();
                            nub->reserved->deviceMemoryMap[i] = currentDeviceMemoryMap;
                        }
                    }
                }
            }
            deviceMemoryArray = NULL;
        }
    }

#if ACPI_SUPPORT
	AppleVTD::adjustDevice(nub);
#elif TARGET_CPU_ARM64 || TARGET_CPU_ARM
    IOMapper* dartMapper = IOMapper::copyMapperForDevice(nub);
    if(   (dartMapper != NULL)
       && (dartMapper->metaCast("IODARTMapper") != NULL))
    {
        uint64_t base = 0;
        uint64_t size = 0;
        // only IODARTMappers support this platform function
        const OSSymbol *retrieveVMLimits = OSSymbol::withCString(kIODARTFunctionRetrieveVMLimits);

        err = dartMapper->callPlatformFunction(retrieveVMLimits,
                                               false,
                                               &base,
                                               &size,
                                               NULL,
                                               NULL);

        if(err == kIOReturnSuccess)
        {
            nub->setProperty(kIOPCIDeviceMemoryMapBase, base, sizeof(base) * 8);
            nub->setProperty(kIOPCIDeviceMemoryMapSize, size, sizeof(size) * 8);
        }
        else
        {
            DLOG("%s: failed to get nub %s VM limits\n", __PRETTY_FUNCTION__, nub->getName());
        }

        nub->setProperty(kIOPCIDeviceMapperPageSize, dartMapper->getPageSize(), sizeof(uint64_t) * 8);

        OSSafeReleaseNULL(retrieveVMLimits);
    }
    OSSafeReleaseNULL(dartMapper);
#endif

    return (err);
}

IOReturn IOPCIBridge::relocate(IOPCIDevice * device, uint32_t options)
{
	IOReturn ret = kIOReturnSuccess;

	if (!options)
	{
		spaceFromProperties(device, &device->space);
		ret = getDTNubAddressing(device);
	}
#if ACPI_SUPPORT
	AppleVTD::relocateDevice(device, (0 != options));
#else
	device->callPlatformFunction(gIOPlatformDeviceRelocatedKey,
			/* waitForFunction */ false,
			/* bridge          */ this,
			/* device          */ device,
			/* options         */ (void *) (long)options,
			/*                 */ nullptr);
#endif
    return (ret);
}

bool IOPCIBridge::matchKeys( IOPCIDevice * nub, const char * keys,
                             UInt32 defaultMask, UInt8 regNum )
{
    const char *        next;
    uint64_t            mask, value;
    uint32_t reg;
    bool                found = false;

    do
    {
        value = strtoul( keys, (char **) &next, 16);
        if (next == keys)
            break;

        while ((*next) == ' ')
            next++;

        if ((*next) == '&')
            mask = strtoul( next + 1, (char **) &next, 16);
        else
            mask = defaultMask;

        reg = nub->savedConfig[ regNum >> 2 ];
        found = ((value & mask) == (reg & mask));
        keys = next;
    }
    while (!found);

    return (found);
}


bool IOPCIBridge::pciMatchNub( IOPCIDevice * nub,
                               OSDictionary * table,
                               SInt32 * score )
{
    OSString *          prop;
    const char *        keys;
    bool                match = true;
    UInt8               regNum;
    int                 i;
    SInt32              localScore;

    struct IOPCIMatchingKeys
    {
        const char *    propName;
        UInt8           regs[ 4 ];
        UInt32          defaultMask;
    };
    const IOPCIMatchingKeys *              look;
    static const IOPCIMatchingKeys matching[] = {
                                              { kIOPCIMatchKey,
                                                { kIOPCIConfigVendorID | 1, kIOPCIConfigSubSystemVendorID },
                                                0xffffffff },
                                              { kIOPCIPrimaryMatchKey,
                                                { kIOPCIConfigVendorID },0xffffffff },
                                              { kIOPCISecondaryMatchKey,
                                                { kIOPCIConfigSubSystemVendorID },
                                                0xffffffff },
                                              { kIOPCIClassMatchKey,
                                                { kIOPCIConfigRevisionID },
                                                0xffffff00 }};

    for (look = matching, localScore = 0;
            (match && (look < &matching[4]));
            look++)
    {
        prop = (OSString *) table->getObject( look->propName );
        if (prop)
        {
            keys = prop->getCStringNoCopy();
            match = false;
            for (i = 0;
                    ((false == match) && (i < 4));
                    i++)
            {
                regNum = look->regs[ i ];
                match = matchKeys( nub, keys,
                                   look->defaultMask, regNum & 0xfc );
                if (0 == (1 & regNum)) break;
                if (match && (kIOPCIConfigRevisionID != regNum)) localScore = 1000;
            }
        }
    }

#if !ACPI_SUPPORT
    if (match && score) *score += localScore;
#endif

    return (match);
}

bool IOPCIBridge::matchNubWithPropertyTable( IOService * nub,
        OSDictionary * table,
        SInt32 * score )
{
    bool        matches;

    matches = pciMatchNub( (IOPCIDevice *) nub, table, score);

	if (matches)
	{
		OSString  * classProp;
		classProp = OSDynamicCast(OSString, table->getObject(kIOClassKey));
//		classProp = OSDynamicCast(OSString, table->getObject(kCFBundleIdentifierKey));
		if (classProp)
		{
  			if (nub->getProperty(gIOPCITunnelledKey))
        	{
        		if (!classProp->isEqualTo("IOPCI2PCIBridge"))
				{
					if (!table->getObject(kIOPCITunnelCompatibleKey))
					{
						IOLog("Driver \"%s\" needs \"%s\" key in plist\n", 
								classProp->getCStringNoCopy(), kIOPCITunnelCompatibleKey);
					}
					if ((kIOPCIConfiguratorNoTunnelDrv & gIOPCIFlags)
					  || (kOSBooleanFalse == table->getObject(kIOPCITunnelCompatibleKey))
					  || ((kOSBooleanTrue != table->getObject(kIOPCITunnelCompatibleKey))
							&& (kIOPCIConfiguratorCheckTunnel & gIOPCIFlags))
					 )
					{
						matches = false;
					}
        		}
        	}
		}
	}

//	if (matches && (!strncmp("pci1033", nub->getName(), strlen("pci1033")))) matches = false;

    return (matches);
}

bool IOPCIBridge::compareNubName( const IOService * nub,
                                  OSString * name, OSString ** matched ) const
{
    return (IODTCompareNubName(nub, name, matched));
}

UInt32 IOPCIBridge::findPCICapability( IOPCIAddressSpace space,
                                       UInt8 capabilityID, UInt8 * found )
{
    UInt32      data = 0;
    UInt8       offset;

    if (found)
        *found = 0;

    if (0 == ((kIOPCIStatusCapabilities << 16)
              & (configRead32(space, kIOPCIConfigCommand))))
        return (0);

    offset = (0xff & configRead32(space, kIOPCIConfigCapabilitiesPtr));
    if (offset & 3)
        offset = 0;
    while (offset)
    {
        data = configRead32( space, offset );
        if (capabilityID == (data & 0xff))
        {
            if (found)
                *found = offset;
            break;
        }
        offset = (data >> 8) & 0xff;
        if (offset & 3)
            offset = 0;
    }

    return (offset ? data : 0);
}

UInt32 IOPCIBridge::extendedFindPCICapability( IOPCIAddressSpace space,
                                                UInt32 capabilityID, IOByteCount * found )
{
	uint32_t result;
	uint32_t firstOffset = 0;

	if (found)
		firstOffset = static_cast<uint32_t>(*found);
	result = reserved->hostBridgeData->_configurator->findPCICapability(this, space, capabilityID, &firstOffset);
	if (found)
		*found = firstOffset;

	return ((UInt32) result);
}

UInt32 IOPCIBridge::extendedFindPCICapability(struct IOPCIConfigEntry * entry,
                                              UInt32 capabilityID,
                                              IOByteCount * offset)
{
    uint32_t result;
    uint32_t firstOffset = 0;

    if (offset)
        firstOffset = static_cast<uint32_t>(*offset);
    result = reserved->hostBridgeData->_configurator->findPCICapability(entry, capabilityID, &firstOffset);
    if (offset)
        *offset = firstOffset;

    return ((UInt32) result);
}

void IOPCIBridge::updateLinkStatusProperty(uint16_t linkStatus)
{
	OSIterator *childIter = getChildIterator( gIOServicePlane );
	OSObject *child = NULL;

	while (   (childIter != NULL)
		   && ((child = childIter->getNextObject()) != NULL))
	{
		IOPCIDevice *nub = OSDynamicCast(IOPCIDevice, child);
		if (nub)
		{
			nub->setProperty(kIOPCIExpressLinkStatusKey, linkStatus, 32);
		}
	}

	OSSafeReleaseNULL(childIter);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIBridge::createAGPSpace( IOAGPDevice * lead,
                                      IOOptionBits options,
                                      IOPhysicalAddress * address,
                                      IOPhysicalLength * length )
{
    return (kIOReturnUnsupported);
}

IOReturn IOPCIBridge::destroyAGPSpace( IOAGPDevice * lead )
{
    return (kIOReturnUnsupported);
}

IORangeAllocator * IOPCIBridge::getAGPRangeAllocator( IOAGPDevice * lead )
{
    return (0);
}

IOOptionBits IOPCIBridge::getAGPStatus( IOAGPDevice * lead,
                                        IOOptionBits options )
{
    return (0);
}

IOReturn IOPCIBridge::commitAGPMemory( IOAGPDevice * lead,
                                       IOMemoryDescriptor * memory,
                                       IOByteCount agpOffset,
                                       IOOptionBits options )
{
    return (kIOReturnUnsupported);
}

IOReturn IOPCIBridge::releaseAGPMemory( IOAGPDevice * lead,
                                        IOMemoryDescriptor * memory,
                                        IOByteCount agpOffset,
                                        IOOptionBits options )
{
    return (kIOReturnUnsupported);
}

IOReturn IOPCIBridge::resetAGPDevice( IOAGPDevice * lead,
                                      IOOptionBits options )
{
    return (kIOReturnUnsupported);
}

IOReturn IOPCIBridge::getAGPSpace( IOAGPDevice * lead,
                                   IOPhysicalAddress * address,
                                   IOPhysicalLength * length )
{
    return (kIOReturnUnsupported);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOPCIBridge

OSDefineMetaClassAndStructors(IOPCI2PCIBridge, IOPCIBridge)
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  0);
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  1);
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  2);
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  3);
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  4);
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  5);
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  6);
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  7);
OSMetaClassDefineReservedUnused(IOPCI2PCIBridge,  8);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService * IOPCI2PCIBridge::probe( IOService *         provider,
                                    SInt32 *            score )
{
    if (0 == (fBridgeDevice = OSDynamicCast(IOPCIDevice, provider)))
        return (0);

    *score              -= 100;

    return (this);
}

bool IOPCI2PCIBridge::serializeProperties( OSSerialize * serialize ) const
{
    return (super::serializeProperties(serialize));
}

IOReturn IOPCIBridge::checkLink(uint32_t options)
{
    return (kIOReturnSuccess);
}

IOReturn IOPCI2PCIBridge::checkLink(uint32_t options)
{
	IOReturn ret;
	bool     present;
	uint16_t linkStatus;
	uint32_t linkCaps;

	AbsoluteTime startTime, endTime;
	uint64_t 	 nsec, nsec2;

    if (fBridgeDevice->reserved->dead) return (kIOReturnNoDevice);

    if (kCheckLinkForPower & options)
    {
		if (!fBridgeDevice->reserved->expressCapability)               return (kIOReturnSuccess);
		linkCaps = fBridgeDevice->configRead32(fBridgeDevice->reserved->expressCapability + 0x0C);
		if (!(kLinkCapDataLinkLayerActiveReportingCapable & linkCaps)) return (kIOReturnSuccess);
		linkStatus = fBridgeDevice->configRead16(fBridgeDevice->reserved->expressCapability + 0x12);
		updateLinkStatusProperty(linkStatus);
		if (kLinkStatusDataLinkLayerLinkActive & linkStatus)           return (kIOReturnSuccess);
		return (kIOReturnNoDevice);
    }

	ret = fBridgeDevice->checkLink(options & ~kCheckLinkParents);
	if (kIOReturnSuccess != ret) return (kIOReturnNoDevice);

	if ((kCheckLinkParents & options) || !fHotPlugInts || !fBridgeInterruptSource) return (ret);

	if (fBridgeDevice->isInactive())	return (kIOReturnNoDevice);

	clock_get_uptime(&startTime);
	linkStatus = fBridgeDevice->configRead16(fBridgeDevice->reserved->expressCapability + 0x12);

#if 1
	clock_get_uptime(&endTime);
	absolutetime_to_nanoseconds(startTime, &nsec2);
	SUB_ABSOLUTETIME(&endTime, &startTime);
	absolutetime_to_nanoseconds(endTime, &nsec);

	if (nsec > 1000*1000)
	{
		DLOG("%s: @%lld link %x took %lld us link %x\n", 
			fLogName, nsec2 / 1000,
			linkStatus, nsec / 1000,
			fBridgeDevice->checkLink(options));
	}
#endif

	if (0xffff == linkStatus)
		return (kIOReturnNoDevice);

	present = (0 != ((1 << 13) & linkStatus));
	if (fPresenceInt != present)
	{
		fPresenceInt = present;
		if (!present)
		{
			// disable mmio, bus leading, and I/O space before making changes to the memory ranges
			uint16_t commandRegister = fBridgeDevice->configRead16(kIOPCIConfigurationOffsetCommand);
			fBridgeDevice->configWrite16(kIOPCIConfigurationOffsetCommand, commandRegister & ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace | kIOPCICommandBusLead));
			fBridgeDevice->configWrite32(kPCI2PCIMemoryRange,         0);
			fBridgeDevice->configWrite32(kPCI2PCIPrefetchMemoryRange, 0);
			fBridgeDevice->configWrite32(kPCI2PCIPrefetchUpperBase,   0);
			fBridgeDevice->configWrite32(kPCI2PCIPrefetchUpperLimit,  0);

			// <rdar://problem/65244597> REG JazzG:J230/J214: P112 USB3/2 hub are missing when attach P112 during sleep -> panic when unplug display
			// for Intel Icelake systems this needs to be re-enabled otherwise hot-plug interrupts aren't generated
			fBridgeDevice->configWrite16(kIOPCIConfigurationOffsetCommand, commandRegister);

		}
		DLOG("%s: @%lld -> present %d\n", 
			fLogName, nsec / 1000, present);
	}

	return (present ? kIOReturnSuccess : kIOReturnOffline);
}

enum { 
	kIntsHP  = 0x00000001,
	kIntsAER = 0x00000002,
};



void IOPCI2PCIBridge::handleInterrupt(IOInterruptEventSource * source __unused, int count __unused)
{
    IOReturn ret;

    uint8_t intsPending = 0;

    intsPending = fIntsPending;
    OSBitAndAtomic8(~intsPending, &fIntsPending);

    if (kIOPCIDeviceOffState == fPowerState)
    {
        DLOG("%s device power state is off\n", __PRETTY_FUNCTION__);
        return;
    }

    if (fNoDevice)
    {
        DLOG("%s no device\n", __PRETTY_FUNCTION__);
        return;
    }
    ret = checkLink(kCheckLinkParents);

    if (kIOReturnNoDevice == ret)
    {
        DLOG("%s checked link no device\n", __PRETTY_FUNCTION__);
        return;
    }

    uint16_t linkStatus = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x12 );
    uint16_t linkBandwidthMask = ((1 << 14) | (1 << 15));

    if (linkStatus & linkBandwidthMask)
    {
        DLOG("%s: link bandwidth notification, linkStatus: 0x%x\n", fBridgeDevice->getName(), linkStatus);

		updateLinkStatusProperty(linkStatus);

        fBridgeDevice->configWrite16(fBridgeDevice->reserved->expressCapability + 0x12, linkStatus & linkBandwidthMask);
    }

	if (fHotPlugInts)
	{
        enum { kNeedMask = ((1 << 8) | (1 << 3) | (1 << 0)) };

        uint16_t slotStatus = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x1a );

        // hot plug interrupt occurred
        if ((0 != (kNeedMask & slotStatus)) || (0 != (intsPending & kIntsHP)))
        {
            fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x1a, kNeedMask & slotStatus );

            bool present;
            UInt32 probeTimeMS = 1;

            fHotplugCount++;

            // Re-read link status so its value is up-to-date with respect to slot status
            uint16_t linkStatus = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x12 );
            uint16_t linkControl = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x10 );

            DLOG("%s: hotpInt (%d), fNeedProbe %d, slotStatus %x, linkStatus %x, linkControl %x\n",
                    fLogName,
                    fHotplugCount, fNeedProbe, slotStatus, linkStatus, linkControl);

            present = (0 != ((1 << 6) & slotStatus));

            // PCIe Base spec v5, sec 6.7.3.3:
			// "The Data Link Layer State Changed event must occur within 1
			// second of the event that initiates the hot-insertion. If a power
			// controller is supported, the time out interval is measured from
			// when software initiated a write to the Slot Control register to
			// turn on the power. If a power controller is not supported, the
			// time out interval is measured from presence detect slot event.
			// Software is allowed to time out on a hot add operation if the
			// Data Link Layer State Changed event does not occur within 1
			// second. The action taken by software after such a timeout is
			// implementation specific."

            // DLLSC is 1
            if (slotStatus & (1 << 8))
            {
				DLOG("%s: Cancelling the DLLSC timeout\n", fLogName);
                fDLLSCEventTimer->cancelTimeout();
            }
            // DLLSC is 0, presence-detect state changed from 0->1, power-controller unsupported
            else if (slotStatus & ((1 << 3) | (1 << 6)) && (fAdapterState == kIOPCIAdapterUnused))
            {
                fDLLSCEventTimer->setTimeoutMS(1000);
            }

            if (fLinkControlWithPM)
            {
                uint16_t pmBits = fBridgeDevice->configRead16(fBridgeDevice->reserved->powerCapability + 4);
                if (present && (kPCIPMCSPowerStateD0 != (kPCIPMCSPowerStateMask & pmBits)))
                {
                    DLOG("%s: pwr on\n", fLogName);
                    fBridgeDevice->configWrite16(fBridgeDevice->reserved->powerCapability + 4, kPCIPMCSPMEStatus | kPCIPMCSPowerStateD0);
                    IOSleep(10);
                }
            }

            if (present && ((1 << 4) & linkControl))
            {
                DLOG("%s: enabling link\n", fLogName);
                linkControl &= ~((1 << 4) | (1 << 6));
                fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x10, linkControl );
                fWaitingLinkEnable = true;
                present = false;
            }
            else if (!present)
            {
                if (fLinkControlWithPM)
                {
                    DLOG("%s: pwr off\n", fLogName);
                    fBridgeDevice->configWrite16(fBridgeDevice->reserved->powerCapability + 4, (kPCIPMCSPMEStatus | kPCIPMCSPMEEnable | kPCIPMCSPowerStateD3));
                }
                else if (!((1 << 4) & linkControl))
                {
                    if (fWaitingLinkEnable)
                        fWaitingLinkEnable = false;
                    else
                    {
                        DLOG("%s: disabling link\n", fLogName);
                        linkControl &= ~(1 << 6);
                        linkControl |= (1 << 4);
                        fBridgeDevice->configWrite16(fBridgeDevice->reserved->expressCapability + 0x10, linkControl);
                    }
                }
            }
            if (fLinkChangeOnly)
                return;

			present &= (0 != ((1 << 13) & linkStatus));

			if ((fAdapterState == kIOPCIAdapterHotAddPending)
				&& (slotStatus & (1 << 8)))
			{
				DLOG("%s: AdapterHotAddPending -> AdapterPresent\n", __func__);
				fAdapterState = kIOPCIAdapterPresent;
			}
			else if ((slotStatus & (1 << 0))) //Attention button pressed
            {
				switch (fAdapterState)
				{
				case kIOPCIAdapterPresent:
					// Blink the power indicator and start 5s "abort interval" timer.
					DLOG("%s: AdapterPresent -> AdapterHotRemovePending\n", __func__);
					fAdapterState = kIOPCIAdapterHotRemovePending;
					slotControlWrite(fBridgeDevice, 2 << 8, 3 << 8);
					fAttnButtonTimer->setTimeoutMS(5000);
					break;
				case kIOPCIAdapterNotPresent:
					if (present)
					{
						DLOG("%s: AdapterNotPresent -> AdapterPresent\n", __func__);
						fAdapterState = kIOPCIAdapterPresent;
						break;
					}
					// Blink the power indicator and start 5s "abort interval" timer.
					DLOG("%s: AdapterNotPresent -> AdapterHotAddPending\n", __func__);
					fAdapterState = kIOPCIAdapterHotAddPending;
					slotControlWrite(fBridgeDevice, 2 << 8, 3 << 8);
					fAttnButtonTimer->setTimeoutMS(5000);
					break;
				case kIOPCIAdapterHotRemovePending:
					DLOG("%s: AdapterHotRemovePending -> AdapterPresent\n", __func__);
					fAdapterState = kIOPCIAdapterPresent;
					fAttnButtonTimer->cancelTimeout();
					break;
				case kIOPCIAdapterHotAddPending:
					DLOG("%s: AdapterHotAddPending -> AdapterNotPresent\n", __func__);
					fAdapterState = kIOPCIAdapterNotPresent;
					fAttnButtonTimer->cancelTimeout();
					break;
				default:
					DLOG("%s: Attn button pressed with state %u\n", __func__, fAdapterState);
					break;
				}
            }

            if (fPresence != present)
            {
                DLOG("%s: now present %d\n", fLogName, present);

                fBridgeDevice->removeProperty(kIOPCIConfiguredKey);
                fNeedProbe = true;
                fPresence = present;
                if (!present)
                {
                    // not present
                    fBridgeDevice->removeProperty(kIOPCIOnlineKey);
                }
                else
                {
                    // present
                    fBridgeDevice->setProperty(kIOPCIOnlineKey, true);
                    probeTimeMS = 2000;
                }
            }

            if (fNeedProbe)
            {
                if (kIOPMUndefinedDriverAssertionID == fPMAssertion)
                {
                    fPMAssertion = getPMRootDomain()->createPMAssertion(
                                        kIOPMDriverAssertionCPUBit, kIOPMDriverAssertionLevelOn,
                                        this, "com.apple.iokit.iopcifamily");
                }
                fTimerProbeES->setTimeoutMS(probeTimeMS);
            }
        }
	}


    IOPCIAERRoot * root;
    if ((root = fAERRoot))
    {
        enum { kNeedMask = ((1 << 2) | (1 << 0)) };

        uint32_t status = fBridgeDevice->configRead32(fBridgeDevice->reserved->aerCapability + 0x30);
        if (0 != (kNeedMask & status))
        {
            IOInterruptState ints;
            uint8_t          nextIdx;

            ints    = IOSimpleLockLockDisableInterrupt(fISRLock);
            nextIdx = root->fAERWriteIndex + 1;
            if (nextIdx == kAERISRNum) nextIdx = 0;
            if (nextIdx != root->fAERReadIndex)
            {
                root->fISRErrors[root->fAERWriteIndex].status = status;
                root->fISRErrors[root->fAERWriteIndex].source = fBridgeDevice->configRead32(fBridgeDevice->reserved->aerCapability + 0x34);
                root->fAERWriteIndex = nextIdx;
            }
            IOSimpleLockUnlockEnableInterrupt(fISRLock, ints);
            intsPending |= kIntsAER;
        }
        fBridgeDevice->configWrite16(fBridgeDevice->reserved->aerCapability + 0x30, status);

        if (kIntsAER & intsPending)
        {
            IOPCIAERRoot *     root = fAERRoot;
            IOPCIAddressSpace  space;
            IOInterruptState   ints;
            IOService *        result;
            IOPCIDevice *      device;
            IOPCIEventSource * src;
            IOReturn           ret;
            uint8_t            nextIdx;
            uint32_t           correctable, source, rstatus, status, mask, severity;
            IOPCIEvent		   newEvent;

            ints = IOSimpleLockLockDisableInterrupt(fISRLock);
            while (root->fAERReadIndex != root->fAERWriteIndex)
            {
                rstatus = root->fISRErrors[root->fAERReadIndex].status;
                source  = root->fISRErrors[root->fAERReadIndex].source;
                nextIdx = root->fAERReadIndex + 1;
                if (nextIdx == kAERISRNum) nextIdx = 0;
                root->fAERReadIndex = nextIdx;
                IOSimpleLockUnlockEnableInterrupt(fISRLock, ints);

                DLOG("%s: AER root status %x\n", fLogName, rstatus);
                for (correctable = 0; correctable < 2; correctable++, source <<= 16)
                {
                    if (!((correctable ? 1 : 4) & rstatus)) continue;
                    space.s.busNum      = (source >> 24);
                    space.s.deviceNum   = (31 & (source >> 19));
                    space.s.functionNum = (7  & (source >> 16));

                    configOpParams cp = {.device = this, .op = kConfigOpFindEntry, .result = &result, .arg = &space};
                    ret = configOp(&cp);
                    DLOG("AER source %d %d %d: find %x %p %s 0x%qx\n",
                            space.s.busNum, space.s.deviceNum, space.s.functionNum, ret, result,
                            result ? result->getName() : "",
                            result ? result->getRegistryEntryID() : 0);
                    if (kIOReturnSuccess != ret) continue;

                    if ((device = OSDynamicCast(IOPCIDevice, result))
                         && device->reserved->aerCapability)
                    {
                        status   = device->configRead32(device->reserved->aerCapability + (correctable ? 0x10 : 0x04));
                        device->configWrite32(device->reserved->aerCapability + (correctable ? 0x10 : 0x04), status);
                        mask     = device->configRead32(device->reserved->aerCapability + (correctable ? 0x14 : 0x08));
                        severity = (correctable ? 0 : device->configRead32(device->reserved->aerCapability + 0x0c));
                        newEvent.data[0] = status;
                        newEvent.data[1] = device->configRead32(device->reserved->aerCapability + 0x1c);
                        newEvent.data[2] = device->configRead32(device->reserved->aerCapability + 0x20);
                        newEvent.data[3] = device->configRead32(device->reserved->aerCapability + 0x24);
                        newEvent.data[4] = device->configRead32(device->reserved->aerCapability + 0x28);

                        DLOG("AER %scorrectable status 0x%08x sev 0x%08x TLP 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
                                correctable ? "" : "un", status, severity,
                                newEvent.data[1], newEvent.data[2], newEvent.data[3], newEvent.data[4]);

                        if (status & ~mask)
                        {
                            IOPCIHostBridgeData *vars = ((IOPCIBridge*)this)->reserved->hostBridgeData;
                            IOSimpleLockLock(vars->_eventSourceLock);

                            queue_iterate(&vars->_eventSourceQueue, src, IOPCIEventSource *, fQ)
                            {
                                if (src->fRoot && (this != src->fRoot)) continue;
                                nextIdx = src->fWriteIndex + 1;
                                if (nextIdx == kIOPCIEventNum) nextIdx = 0;
                                if (nextIdx != src->fReadIndex)
                                {
                                    src->fEvents[src->fWriteIndex].event =
                                        correctable ? kIOPCIEventCorrectableError
                                                    : ((status & severity) ? kIOPCIEventFatalError : kIOPCIEventNonFatalError);
                                    device->retain();
                                    src->fEvents[src->fWriteIndex].reporter = device;
                                    memcpy(&src->fEvents[src->fWriteIndex].data[0],
                                            &newEvent.data[0],
                                            sizeof(src->fEvents[src->fWriteIndex].data));
                                    src->fWriteIndex = nextIdx;
                                }
                                if (src->isEnabled()) src->signalWorkAvailable();
                            }
                            IOSimpleLockUnlock(vars->_eventSourceLock);
                        }
                    }
                    result->release();
                }
                ints = IOSimpleLockLockDisableInterrupt(fISRLock);
            }
            IOSimpleLockUnlockEnableInterrupt(fISRLock, ints);
        }
    }
}

void IOPCI2PCIBridge::timerProbe(IOTimerEventSource * es)
{
    if (fNeedProbe && (kIOPCIDeviceOnState == fPowerState))
    {
        fNeedProbe = false;
        DLOG("%s: probe\n", fLogName);
        fBridgeDevice->kernelRequestProbe(kIOPCIProbeOptionDone | kIOPCIProbeOptionLinkInt | kIOPCIProbeOptionNeedsScan);
    }
	if (kIOPMUndefinedDriverAssertionID != fPMAssertion)
	{
		getPMRootDomain()->releasePMAssertion(fPMAssertion);
		fPMAssertion = kIOPMUndefinedDriverAssertionID;
	}
}

void IOPCI2PCIBridge::attnButtonTimer(IOTimerEventSource * es)
{
	if (fAdapterState == kIOPCIAdapterHotAddPending)
	{
		// Clear Slot Control bit 10 to enable power and trigger a
		// data-link-layer-state-changed event, turn on the power
		// indicator, and start a 1s timer for the DLLSC event (PCIe
		// Base spec v5, sec 6.7.3.3)
		slotControlWrite(fBridgeDevice, 0, 1 << 10);
		slotControlWrite(fBridgeDevice, 1 << 8, 3 << 8);
		fDLLSCEventTimer->setTimeoutMS(1000);
	}
	else if (fAdapterState == kIOPCIAdapterHotRemovePending)
	{
		OSIterator *childIter = fBridgeDevice->getChildIterator( gIODTPlane );

		DLOG("%s: AdapterHotRemovePending -> AdapterNotPresentPending\n", __func__);
		// Tear down the device nub, turn off the power controller, and set a
		// 1s timer before going to kIOPCIAdapterNotPresent.
		fAdapterState = kIOPCIAdapterNotPresentPending;

		// Terminate the child IOPCIDevice nodes
		DLOG("[%s()] Terminating children\n", __func__);
		if (childIter) {
			childIter->reset();

			IORegistryEntry *found;
			while ((found = (IORegistryEntry *) childIter->getNextObject()))
			{
				IOPCIDevice *child = OSDynamicCast(IOPCIDevice, found);
				if (!child) continue;

				child->terminate();
			}

			childIter->release();
		}

		thread_call_t threadCall = thread_call_allocate(OSMemberFunctionCast(thread_call_func_t,
					this,
					&IOPCI2PCIBridge::attnButtonHandlerFinish),
				this);

		// Run waitQuiet() in a threadcall so terminating nubs can make forward progress
		if(threadCall != NULL)
		{
			retain();
			if(thread_call_enter1(threadCall, threadCall /* so the call cleans itself up */) == TRUE)
			{
				thread_call_free(threadCall);
				release();
			}
		}
	}
	else if (fAdapterState == kIOPCIAdapterNotPresentPending)
	{
		DLOG("%s: AdapterNotPresentPending -> AdapterNotPresent\n", __func__);
		fAdapterState = kIOPCIAdapterNotPresent;
		// Turn off the power indicator.
		slotControlWrite(fBridgeDevice, 3 << 8, 3 << 8);
	}
}

void IOPCI2PCIBridge::dllscEventTimer(IOTimerEventSource * es)
{
	DLOG("%s: DLLSC timer fired\n", fLogName);
	handleInterrupt(NULL, 0);
}

IOReturn IOPCI2PCIBridge::attnButtonHandlerFinish(thread_call_t threadCall)
{
    IOReturn ret = kIOReturnError;

    // If the bridge becomes inactive while waiting, abort
    while (!isInactive() && ret != kIOReturnSuccess)
    {
        ret = waitQuiet(10 * 1000 * 1000);
    }

    if (!isInactive())
    {
        // Set Slot Control bit 10 to remove power
        DLOG("[%s()] Removing slot power\n", __func__);
        slotControlWrite(fBridgeDevice, 1 << 10, 1 << 10);

        fAttnButtonTimer->setTimeoutMS(1000);
    }
    else
    {
        IOLog("[%s()] bridge inactive while waiting for nubs to terminate", __func__);
    }

    // clean up threadcall
    thread_call_free(threadCall);
    release();

    return ret;
}

bool IOPCI2PCIBridge::start( IOService * provider )
{
    bool ok;

	fPMAssertion = kIOPMUndefinedDriverAssertionID;

    setName(kIOPCI2PCIBridgeName);
    
	snprintf(fLogName, sizeof(fLogName), "%s(%u:%u:%u)(%u-%u)", 
			 fBridgeDevice->getName(), PCI_ADDRESS_TUPLE(fBridgeDevice), firstBusNum(), lastBusNum());

    ok = super::start(provider);

    if (ok && fBridgeInterruptSource && fHotPlugInts)
    {
        // only keep on if the port is capable of receiving hot-plug interrupts. Otherwise let client drivers
        // control the power state
        DLOG("%s: hotplug capable, keeping power state on to receive hotplug events\n", fBridgeDevice->getName());
        changePowerStateTo(kIOPCIDeviceOnState);
    }

    // Hotplug-capable bridges are re-probed when their IOPM state changes to kIOPCIDeviceOnState,
    // unless this property is overridden.
    setProperty(kIOPCIPowerOnProbeKey, kOSBooleanTrue);

    return (ok);
}

bool IOPCI2PCIBridge::configure( IOService * provider )
{
#if ACPI_SUPPORT
	if (gBridgeData)
	{
		((IOPCIBridge*)this)->reserved->hostBridgeData = gBridgeData;
		gBridgeData->retain();
	}
	else
	{
		panic("Shared host bridge data is not initialized.");
	}

#else
	if (fBridgeDevice->reserved && fBridgeDevice->reserved->hostBridge)
	{
		IOPCIHostBridgeData *vars = fBridgeDevice->reserved->hostBridge->reserved->hostBridgeData;
		if (vars)
		{
			((IOPCIBridge*)this)->reserved->hostBridgeData = vars;
			vars->retain();
		}
		else
		{
			panic("Host bridge data is not initialized for the host bridge.");
		}
	}
	else
	{
		panic("Host bridge config entry not found.");
	}
#endif
	fPowerState = kIOPCIDeviceOnState;
    if (fBridgeDevice->reserved->powerCapability)
	{
		fLinkControlWithPM = fBridgeDevice->savedConfig
							&& (0x3B488086 == fBridgeDevice->savedConfig[kIOPCIConfigVendorID >> 2]);
	}

    if (fBridgeDevice->reserved->expressCapability)
    do
    {
        if (fBridgeDevice->getProperty(kIOPCIHotPlugKey))
        {
        	fHotPlugInts = true;
            setProperty(kIOPCIHotPlugKey, kOSBooleanTrue);
        }
        else if (fBridgeDevice->getProperty(kIOPCILinkChangeKey))
        {
            setProperty(kIOPCILinkChangeKey, kOSBooleanTrue);
        	fHotPlugInts = true;
            fLinkChangeOnly = true;
        }
        else if (fBridgeDevice->getProperty(kIOPCITunnelLinkChangeKey))
		{
        	fHotPlugInts = true;
		}
		fIsAERRoot = ((kIOPCIConfiguratorAER & gIOPCIFlags)
			        && (kPCIStatic == configShadow(fBridgeDevice)->hpType)
		            && fBridgeDevice->reserved->rootPort
		            && fBridgeDevice->reserved->aerCapability);
		if (fHotPlugInts || fIsAERRoot)
		{
			allocateBridgeInterrupts(provider);
		}

		fAdapterState = kIOPCIAdapterUnused;

        if (fBridgeDevice->getProperty(kIOPCISlotPowerController))
		{
			setProperty(kIOPCISlotPowerController, kOSBooleanTrue);

			// Step 2: If the device is NOT present, write 1 to slotCtrl here.
			uint16_t slotStatus = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x1a );
			if (slotStatus & (1 << 6))
			{
				fAdapterState = kIOPCIAdapterPresent;
				setProperty(kIOPCISlotDevicePresent, kOSBooleanTrue);
				// Turn on power and the power indicator.
				slotControlWrite(fBridgeDevice, 0, 1 << 10);
				slotControlWrite(fBridgeDevice, 1 << 8, 3 << 8);
			}
			else
			{
				fAdapterState = kIOPCIAdapterNotPresent;
				setProperty(kIOPCISlotDevicePresent, kOSBooleanFalse);
				// Turn off power and the power indicator.
				slotControlWrite(fBridgeDevice, 1 << 10, 1 << 10);
				slotControlWrite(fBridgeDevice, 3 << 8, 3 << 8);
			}
		}

		if (fBridgeInterruptSource && !fBridgeDevice->getProperty(kIOPCITunnelBootDeferKey)) startBridgeInterrupts(provider);
    }
    while(false);

    saveBridgeState();
    if (fBridgeDevice->savedConfig)
    {
        configShadow(fBridgeDevice)->bridge = this;
        configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowBridge;
        if (fHotPlugInts)
			configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowBridgeInterrupts;
        if (OSTypeIDInst(this) != OSTypeID(IOPCI2PCIBridge))
            configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowBridgeDriver;
    }

    return (super::configure(provider));
}


void IOPCI2PCIBridge::allocateBridgeInterrupts(IOService * provider)
{
	IOReturn ret = kIOReturnSuccess;
    do
    {
		int interruptType;
		int intIdx = 1;
		for (intIdx = 1; intIdx >= 0; intIdx--)
		{
			ret = fBridgeDevice->getInterruptType(intIdx, &interruptType);
			if (kIOReturnSuccess == ret)
			{
				fBridgeMSI = (0 != (kIOInterruptTypePCIMessaged & interruptType));
				break;
			}
		}
        if (kIOReturnSuccess != ret) break;

        fBridgeInterruptSource = IOInterruptEventSource::interruptEventSource(this,
                                                                              OSMemberFunctionCast(IOInterruptEventSource::Action,
                                                                                                   this,
                                                                                                   &IOPCI2PCIBridge::handleInterrupt),
                                                                              provider,
                                                                              intIdx);
    }
    while(false);
}


void IOPCI2PCIBridge::startBridgeInterrupts(IOService * provider)
{
	IOReturn ret = kIOReturnSuccess;
    do
    {
        if (!fBridgeInterruptSource) break;

        fWorkLoop = getConfiguratorWorkLoop();
		fTimerProbeES = IOTimerEventSource::timerEventSource(this, 
										OSMemberFunctionCast(IOTimerEventSource::Action,
															this, &IOPCI2PCIBridge::timerProbe));
        if (!fTimerProbeES) break;
        ret = fWorkLoop->addEventSource(fTimerProbeES);
		if (kIOReturnSuccess != ret) break;
		fAttnButtonTimer = IOTimerEventSource::timerEventSource(this, 
										OSMemberFunctionCast(IOTimerEventSource::Action,
															this, &IOPCI2PCIBridge::attnButtonTimer));
        if (!fAttnButtonTimer) break;
        ret = fWorkLoop->addEventSource(fAttnButtonTimer);
		fDLLSCEventTimer = IOTimerEventSource::timerEventSource(this,
										OSMemberFunctionCast(IOTimerEventSource::Action,
															this, &IOPCI2PCIBridge::dllscEventTimer));
        if (!fDLLSCEventTimer) break;
        ret = fWorkLoop->addEventSource(fDLLSCEventTimer);

		if (kIOReturnSuccess != ret) break;
        ret = fWorkLoop->addEventSource(fBridgeInterruptSource);
		if (kIOReturnSuccess != ret) break;

		fISRLock = IOSimpleLockAlloc();
		if (!fISRLock) break;
		if (fIsAERRoot)
		{
			fAERRoot = IOMallocType(IOPCIAERRoot);
			if (!fAERRoot) break;
			fAERRoot->fISRErrors = IONewData(IOPCIAERISREntry, kAERISRNum);
			if (!fAERRoot->fISRErrors) break;
		}

		if (fHotPlugInts)
		{
			fPresence = (0 != fBridgeDevice->getProperty(kIOPCIOnlineKey));
			fPresenceInt = fPresence;
		}

        fBridgeInterruptEnablePending = true;
		enableBridgeInterrupts();
	}
    while(false);
}

void IOPCI2PCIBridge::enableBridgeInterrupts(void)
{
	if (fHotPlugInts)
	{
		fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x1a, 1 << 3 );
		slotControlWrite(fBridgeDevice, kSlotControlEnables, kSlotControlEnables);
	}
	if (fIsAERRoot)
	{
		 DLOG("%s: start AER\n", fBridgeDevice->getName());
		 uint16_t command = fBridgeDevice->configRead32(fBridgeDevice->reserved->aerCapability + 0x2c);
		 fBridgeDevice->configWrite32(fBridgeDevice->reserved->aerCapability + 0x30, 0xff);
		 command |= (1 << 0) | (1 << 1) | (1 << 2);
		 fBridgeDevice->configWrite32(fBridgeDevice->reserved->aerCapability + 0x2c, command);
	}
	uint32_t linkCap = fBridgeDevice->configRead32(fBridgeDevice->reserved->expressCapability + 0xc);
	if (linkCap & (1 << 21))
	{
		DLOG("%s: enable link bandwidth notifications\n", fBridgeDevice->getName());
		uint16_t linkControl = fBridgeDevice->configRead16(fBridgeDevice->reserved->expressCapability + 0x10);
		linkControl |= (1 << 10) | (1 << 11);
		fBridgeDevice->configWrite16(fBridgeDevice->reserved->expressCapability + 0x10, linkControl);
	}
}

void IOPCI2PCIBridge::disableBridgeInterrupts(void)
{
	if (fHotPlugInts)
	{
		uint16_t slotControl = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x18 );
		DLOG("%s: slotControl 0x%x->0x%x\n", fBridgeDevice->getName(), slotControl, slotControl & ~kSlotControlEnables);
		slotControlWrite(fBridgeDevice, 0, kSlotControlEnables);
	}
	if (fIsAERRoot)
	{
		fBridgeDevice->configWrite32(fBridgeDevice->reserved->aerCapability + 0x2c, 0);
	}
}


void IOPCI2PCIBridge::startBootDefer(IOService * provider)
{
	DLOG("%s: start boot deferred\n", provider->getName());
	provider->removeProperty(kIOPCITunnelBootDeferKey);
	startBridgeInterrupts(provider);
    if (fBridgeInterruptEnablePending)
    {
        // enable int source
        fBridgeInterruptSource->enable();
        fBridgeInterruptSource->interruptOccurred(NULL, NULL, 0);
        fBridgeInterruptEnablePending = false;
	}
}

void IOPCI2PCIBridge::probeBus( IOService * provider, UInt8 busNum )
{
	bool bootDefer = (0 != provider->getProperty(kIOPCITunnelBootDeferKey));
    if (!bootDefer)
	{
		snprintf(fLogName, sizeof(fLogName), "%s(%u:%u:%u)(%u-%u)", 
				 fBridgeDevice->getName(), PCI_ADDRESS_TUPLE(fBridgeDevice), firstBusNum(), lastBusNum());
		super::probeBus(provider, busNum);
		if (fBridgeInterruptEnablePending)
		{
			// enable hotp ints
			fBridgeInterruptSource->enable();
            fBridgeInterruptSource->interruptOccurred(NULL, NULL, 0);
			fBridgeInterruptEnablePending = false;
		}
		return;
	}

	DLOG("%s: boot probe deferred\n", provider->getName());
#if 0
	startBootDefer(provider);
#endif
}

IOReturn IOPCI2PCIBridge::requestProbe( IOOptionBits options )
{
    return (super::requestProbe(options));
}

IOReturn IOPCI2PCIBridge::setPowerState( unsigned long powerState,
                                            IOService * whatDevice )
{
	IOReturn ret;

    if ((powerState != fPowerState) 
    	&& fBridgeInterruptSource 
    	&& !fBridgeInterruptEnablePending)
	do
	{
		unsigned long fromPowerState;

		fromPowerState = fPowerState;
		fPowerState = powerState;

		if (kIOPCIDeviceOffState == powerState)
		{
			if (fHotPlugInts && fNeedProbe) DLOG("%s: sleeping with fNeedProbe\n", fLogName);
			disableBridgeInterrupts();
			if (getProperty(kIOPMResetPowerStateOnWakeKey) == kOSBooleanTrue)
			{
				changePowerStateTo(kIOPCIDeviceOffState);
				changePowerStateToPriv(kIOPCIDeviceOffState);
			}
			break;
		}		
		if (kIOPCIDeviceOffState == fromPowerState)
		{
			if (fHotPlugInts)
			{
				if (fNoDevice) break;
				ret = checkLink();
				if (kIOReturnNoDevice == ret)
				{
					fNoDevice = true;
					break;
				}
				fNeedProbe |= fPresence && (getProperty(kIOPCIPowerOnProbeKey) == kOSBooleanTrue);
				OSBitOrAtomic8(kIntsHP, &fIntsPending);
			}
			enableBridgeInterrupts();
		}
		if (kIOPCIDeviceOnState == powerState)
		{
			if (fBridgeDevice->reserved->needsProbe) deferredProbe(fBridgeDevice);
            fBridgeInterruptSource->interruptOccurred(NULL, NULL, 0);
		}
	}
	while (false);

    return (super::setPowerState(powerState, whatDevice));
}

void IOPCI2PCIBridge::adjustPowerState(unsigned long state)
{
	DLOG("%s: adjustPowerState(%ld)\n", fBridgeDevice->getName(), state);
	if (state < kIOPCIDeviceOnState)
	{
		fBridgeDevice->powerOverrideOnPriv();
	}
	else 
	{
		state = kIOPCIDeviceOnState;
		fBridgeDevice->powerOverrideOffPriv();
	}

	fBridgeDevice->changePowerStateToPriv(state);
}

IOReturn IOPCI2PCIBridge::saveDeviceState(IOPCIDevice * device,
                                          IOOptionBits options)
{
	// bridge flags
    if (device->getProperty(kIOPMPCISleepLinkDisableKey))
		configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowSleepLinkDisable;
	else
		configShadow(fBridgeDevice)->flags &= ~kIOPCIConfigShadowSleepLinkDisable;
    if (device->getProperty(kIOPMPCISleepResetKey))
		configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowSleepReset;
	else
		configShadow(fBridgeDevice)->flags &= ~kIOPCIConfigShadowSleepReset;
    if (configShadow(device)->tunnelled || device->getProperty(kIOPCIEjectableKey))
		configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowHotplug;
	else
		configShadow(fBridgeDevice)->flags &= ~kIOPCIConfigShadowHotplug;

	return super::saveDeviceState(device, options);
}

void IOPCI2PCIBridge::stop( IOService * provider )
{
    super::stop( provider);

	IOWorkLoop * tempWL;
	if (fBridgeInterruptSource)
	{
		fBridgeInterruptSource->disable();
		if ((tempWL = fBridgeInterruptSource->getWorkLoop()))
		   tempWL->removeEventSource(fBridgeInterruptSource);
		fBridgeInterruptSource->release();
		fBridgeInterruptSource = 0;
	}
	if (fTimerProbeES)
	{
		fTimerProbeES->cancelTimeout();
		if ((tempWL = fTimerProbeES->getWorkLoop()))
			tempWL->removeEventSource(fTimerProbeES);
		fTimerProbeES->release();
		fTimerProbeES = 0;
	}
	if (fAttnButtonTimer)
	{
		fAttnButtonTimer->cancelTimeout();
		if ((tempWL = fAttnButtonTimer->getWorkLoop()))
			tempWL->removeEventSource(fAttnButtonTimer);
		fAttnButtonTimer->release();
		fAttnButtonTimer = 0;
	}
	if (fDLLSCEventTimer)
	{
		fDLLSCEventTimer->cancelTimeout();
		if ((tempWL = fDLLSCEventTimer->getWorkLoop()))
			tempWL->removeEventSource(fDLLSCEventTimer);
		fDLLSCEventTimer->release();
		fDLLSCEventTimer = 0;
	}
	if (kIOPMUndefinedDriverAssertionID != fPMAssertion)
	{
		getPMRootDomain()->releasePMAssertion(fPMAssertion);
		fPMAssertion = kIOPMUndefinedDriverAssertionID;
	}
	if (fISRLock)
	{
		IOSimpleLockFree(fISRLock);
		fISRLock = 0;
	}
	if (fAERRoot)
	{
		if (fAERRoot->fISRErrors) IODeleteData(fAERRoot->fISRErrors, IOPCIAERISREntry, kAERISRNum);
		IOFreeType(fAERRoot, IOPCIAERRoot);
	}
}

void IOPCI2PCIBridge::free()
{
    super::free();
}

void IOPCI2PCIBridge::saveBridgeState( void )
{
}

void IOPCI2PCIBridge::restoreBridgeState( void )
{
}

UInt8 IOPCI2PCIBridge::firstBusNum( void )
{
    return fBridgeDevice->configRead8( kPCI2PCISecondaryBus );
}

UInt8 IOPCI2PCIBridge::lastBusNum( void )
{
    return fBridgeDevice->configRead8( kPCI2PCISubordinateBus );
}

IOPCIAddressSpace IOPCI2PCIBridge::getBridgeSpace( void )
{
    return (fBridgeDevice->space);
}

UInt32 IOPCI2PCIBridge::configRead32( IOPCIAddressSpace space,
                                      UInt8 offset )
{
    return (fBridgeDevice->configRead32(space, offset));
}

void IOPCI2PCIBridge::configWrite32( IOPCIAddressSpace space,
                                     UInt8 offset, UInt32 data )
{
    fBridgeDevice->configWrite32( space, offset, data );
}

UInt16 IOPCI2PCIBridge::configRead16( IOPCIAddressSpace space,
                                      UInt8 offset )
{
    return (fBridgeDevice->configRead16(space, offset));
}

void IOPCI2PCIBridge::configWrite16( IOPCIAddressSpace space,
                                     UInt8 offset, UInt16 data )
{
    fBridgeDevice->configWrite16( space, offset, data );
}

UInt8 IOPCI2PCIBridge::configRead8( IOPCIAddressSpace space,
                                    UInt8 offset )
{
    return (fBridgeDevice->configRead8(space, offset));
}

void IOPCI2PCIBridge::configWrite8( IOPCIAddressSpace space,
                                    UInt8 offset, UInt8 data )
{
    fBridgeDevice->configWrite8( space, offset, data );
}

IODeviceMemory * IOPCI2PCIBridge::ioDeviceMemory( void )
{
    return (fBridgeDevice->ioDeviceMemory());
}

bool IOPCI2PCIBridge::publishNub( IOPCIDevice * nub, UInt32 index )
{
    if (nub)
        nub->setProperty( "IOChildIndex" , index, 32 );

    return (super::publishNub(nub, index));
}

IOReturn IOPCIBridge::resolveInterrupts(IOPCIDevice * nub )
{
    IOReturn ret = kIOReturnUnsupported;
    bool
    msiDefault = (false
#if 0
                    || (0 == strcmp("display", nub->getName()))
                    || (0 == strcmp("GFX0", nub->getName()))
                    || (0 == strcmp("PXS1", nub->getName()))        // yukon
                    || (0 == strcmp("HDEF", nub->getName()))
                    || (0 == strcmp("SATA", nub->getName()))
                    || (0 == strcmp("LAN0", nub->getName()))
                    || (0 == strcmp("LAN1", nub->getName()))
                    || (0 == strcmp("PXS2", nub->getName()))        // airport
                    || (0 == strcmp("PXS3", nub->getName()))        // express
#endif
    );

    IOService * provider = getProvider();
    if (msiDefault)
        ret = resolveMSIInterrupts( provider, nub );
    IOReturn lret = resolveLegacyInterrupts( provider, nub );
    if (!msiDefault)
        ret = resolveMSIInterrupts( provider, nub );

    return ret == kIOReturnSuccess ? ret : lret;
}

IOReturn IOPCIBridge::resolveMSIInterrupts( IOService * provider, IOPCIDevice * nub, UInt32 numRequired, UInt32 numRequested )
{
    IOReturn ret = kIOReturnUnsupported;

	if (!(kIOPCIConfiguratorTBMSIEnable & gIOPCIFlags)
  		&& nub->getProperty(gIOPCITunnelledKey))
  	{
		return (ret);
	}

    if (reserved && !reserved->messagedInterruptController)
    {
        callPlatformFunction(gIOPlatformGetMessagedInterruptControllerKey, false,
                             (void *)provider,
                             (void *)&reserved->messagedInterruptController,
                             (void *)0, (void *)0);
    }

#if USE_MSI

    uint16_t msiCapability = nub->reserved->msiCapability;
    if (msiCapability && reserved && reserved->messagedInterruptController)
    {
        ret = reserved->messagedInterruptController->allocateDeviceInterrupts(
                nub, 0, msiCapability, 0, 0, numRequired, numRequested);
    }

#endif /* USE_MSI */

    return (ret);
}

IOReturn IOPCIBridge::resolveLegacyInterrupts( IOService * provider, IOPCIDevice * nub )
{
#if USE_LEGACYINTS

    uint32_t pin;
    uint32_t irq = 0;

    pin = nub->configRead8( kIOPCIConfigInterruptPin );
    if ( pin == 0 || pin > 4 )
        return (kIOReturnUnsupported);  // assume no interrupt usage

    pin--;  // make pin zero based, INTA=0, INTB=1, INTC=2, INTD=3

    // Ask the platform driver to resolve the PCI interrupt route,
    // and return its corresponding system interrupt vector.

    if ( kIOReturnSuccess == provider->callPlatformFunction(gIOPlatformResolvePCIInterruptKey,
                   /* waitForFunction */ false,
                   /* provider nub    */ provider,
                   /* device number   */ (void *)(uintptr_t) nub->space.s.deviceNum,
                   /* interrupt pin   */ (void *)(uintptr_t) pin,
                   /* resolved IRQ    */ &irq ))
    {
        DLOG("%s: Resolved interrupt %d (%d) for %s\n",
                  provider->getName(),
                  irq, pin,
                  nub->getName());

        nub->configWrite8( kIOPCIConfigInterruptLine, irq & 0xff );
    }
    else
    {
        irq = nub->configRead8( kIOPCIConfigInterruptLine );
        if ( 0 == irq || 0xff == irq ) return (kIOReturnUnsupported);
        irq &= 0xf;  // what about IO-APIC and irq > 15?
    }

    provider->callPlatformFunction(gIOPlatformSetDeviceInterruptsKey,
              /* waitForFunction */ false,
              /* nub             */ nub, 
              /* vectors         */ (void *) &irq,
              /* vectorCount     */ (void *) 1,
              /* exclusive       */ (void *) false );

#endif /* USE_LEGACYINTS */

    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOPCIEventSource * IOPCIBridge::createEventSource(
			OSObject * owner, IOPCIEventSource::Action action, uint32_t options)
{
	IOPCIEventSource * src;
	bool               ok = false;

	src = OSTypeAlloc(IOPCIEventSource);
	if (!src) return (0);
	do
	{
		if (!src->init(owner, (IOEventSource::Action) action)) break;
		src->fEvents = IONew(IOPCIEvent, kIOPCIEventNum);
		if (!src->fEvents) break;
		src->fRoot   = 0;
		src->fDevice = 0;
		ok = true;
	}
	while (false);

	if (!ok)
	{
		src->release();
		src = 0;
	}
	return (src);
}

IOPCIEventSource * IOPCIBridge::createEventSource(IOPCIDevice * device,
			OSObject * owner, IOPCIEventSource::Action action, uint32_t options)
{
	return (0);
}

IOPCIEventSource * IOPCI2PCIBridge::createEventSource(IOPCIDevice * device,
			OSObject * owner, IOPCIEventSource::Action action, uint32_t options)
{
	IOPCIEventSource * src;

	if (!fIsAERRoot) return (fBridgeDevice->parent->createEventSource(device, owner, action, options));

	src = IOPCIBridge::createEventSource(owner, action, options);
	if (src) 
	{
		src->fRoot = this;
		src->fDevice = device;
		if (device) device->retain();
	}

	return (src);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if defined(__arm64__)
IOReturn IOPCIBridge::deviceMemoryRead(IOMemoryDescriptor* sourceBase,
									   IOByteCount         sourceOffset,
									   IOMemoryDescriptor* destinationBase,
									   IOByteCount         destinationOffset,
									   IOByteCount         size)
{
    return kIOReturnUnsupported;
}

IOReturn IOPCIBridge::deviceMemoryRead(IOMemoryDescriptor* sourceBase,
                                       IOByteCount         sourceOffset,
                                       void*               destination,
                                       IOByteCount         size)
{                                      
    return kIOReturnUnsupported;
}
#endif

void IOPCIBridge::setInReset(bool inReset)
{
	reserved->childrenInReset = inReset;
}

#undef super
#define super IOEventSource

OSDefineMetaClassAndStructors(IOPCIEventSource, IOEventSource)

IOPCIHostBridgeData *IOPCIEventSource::getHostBridgeData(void)
{
	IOPCIHostBridgeData *vars = NULL;
	IOPCIBridge *bridge = OSDynamicCast(IOPCIBridge, fRoot ? fRoot : owner);
	if (bridge == NULL)
	{
		panic("Orphant event source.");
	}

	if (bridge->reserved)
	{
		vars = bridge->reserved->hostBridgeData;
	}

	// Not found? Try assuming this is host-bridge owned event source.
	if (vars == NULL)
	{
		IOPCIHostBridge *hostBridge = OSDynamicCast(IOPCIHostBridge, bridge);
		if (hostBridge)
		{
			vars = hostBridge->bridgeData;
		}
	}
	if (vars == NULL)
	{
		panic("IOPCIEventSource cannot find bridge data.");
	}

	return vars;
}

void IOPCIEventSource::free(void)
{
    IOPCIEvent       event;
    uint32_t         nextIdx;

	if (fEvents) 
	{
		IOPCIHostBridgeData *vars = getHostBridgeData();
		IOSimpleLockLock(vars->_eventSourceLock);
		while (fReadIndex != fWriteIndex)
		{
			event = fEvents[fReadIndex];
			nextIdx = fReadIndex + 1;
			if (nextIdx == kIOPCIEventNum) nextIdx = 0;
			fReadIndex = nextIdx;
			if (event.reporter)
			{
				IOSimpleLockUnlock(vars->_eventSourceLock);
				event.reporter->release();
				IOSimpleLockLock(vars->_eventSourceLock);
			}
		}
		IOSimpleLockUnlock(vars->_eventSourceLock);
		IODelete(fEvents, IOPCIEvent, kIOPCIEventNum);
	}

	if (fDevice) fDevice->release();
    super::free();
}

void IOPCIEventSource::enable()
{
	IOPCIHostBridgeData *vars = getHostBridgeData();
	super::enable();
	IOSimpleLockLock(vars->_eventSourceLock);
	if (!fQ.next) queue_enter(&vars->_eventSourceQueue, this, IOPCIEventSource *, fQ);
	IOSimpleLockUnlock(vars->_eventSourceLock);
}

void IOPCIEventSource::disable()
{
	IOPCIHostBridgeData *vars = getHostBridgeData();
	super::disable();
	IOSimpleLockLock(vars->_eventSourceLock);
	if (fQ.next)
	{
		queue_remove(&vars->_eventSourceQueue, this, IOPCIEventSource *, fQ);
		fQ.next = 0;
	}
	IOSimpleLockUnlock(vars->_eventSourceLock);
}

bool IOPCIEventSource::checkForWork(void)
{
	IOPCIEventAction pciAction = (IOPCIEventAction) action;
	IOPCIEvent       event;
	uint32_t         nextIdx;
	IOPCIHostBridgeData *vars = getHostBridgeData();

	IOSimpleLockLock(vars->_eventSourceLock);
	while (enabled && (fReadIndex != fWriteIndex))
	{
		event = fEvents[fReadIndex];
		nextIdx = fReadIndex + 1;
		if (nextIdx == kIOPCIEventNum) nextIdx = 0;
		fReadIndex = nextIdx;
		IOSimpleLockUnlock(vars->_eventSourceLock);
		(*pciAction)(owner, this, &event);
		if (event.reporter) event.reporter->release();
		IOSimpleLockLock(vars->_eventSourceLock);
	}
	IOSimpleLockUnlock(vars->_eventSourceLock);

	return (false);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

static void
PCIEventTest(OSObject * owner, IOPCIEventSource * es, const IOPCIEvent * event )
{
	kprintf("PCIEventTest %s, 0x%08qx : 0x%08x 0x%08x, 0x%08x 0x%08x 0x%08x 0x%08x\n", 
						event->reporter->getName(), event->reporter->getRegistryEntryID(),
						event->event, event->data[0],
						event->data[1], event->data[2], event->data[3], event->data[4]);
}

IOReturn
IOPCIBridge::setProperties(OSObject * properties)
{
    IOReturn       ret = kIOReturnUnsupported;
    OSDictionary * dict;
    OSDictionary * matching;
    OSArray *      array;
    OSString *     str;
	IOService *    victimS = 0;
    IOPCIDevice *  victim = 0;
    const char *   cmdstr;
    uint64_t       arg = 0;

    dict = OSDynamicCast(OSDictionary, properties);
    if (dict 
     && (array = OSDynamicCast(OSArray, dict->getObject(kIODebugArgumentsKey)))
	 && (str = OSDynamicCast(OSString, array->getObject(0))))
    {
	   	ret = IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator);
		if (kIOReturnSuccess != ret) return (ret);

	    cmdstr = str->getCStringNoCopy();
		kprintf("pcicmd: %s\n", cmdstr);

		str = OSDynamicCast(OSString, array->getObject(1));

		if (!strncmp("find", cmdstr, strlen("find")))
		{
			IOPCIAddressSpace space;
			IOService *       result;			

			space.s.busNum = strtoq(str ? str->getCStringNoCopy() : 0, NULL, 0) & 0xFF;
			str = OSDynamicCast(OSString, array->getObject(2));
			space.s.deviceNum = strtoq(str ? str->getCStringNoCopy() : 0, NULL, 0) & 0x1F;
			str = OSDynamicCast(OSString, array->getObject(3));
			space.s.functionNum = strtoq(str ? str->getCStringNoCopy() : 0, NULL, 0) & 0x7;

			configOpParams cp = {.device = this, .op = kConfigOpFindEntry, .result = &result, .arg = &space};
			IOReturn ret = configOp(&cp);
			kprintf("%d %d %d: find %x %p %s 0x%qx\n", 
					space.s.busNum, space.s.deviceNum, space.s.functionNum, ret, result, 
					result ? result->getName() : "", 
					result ? result->getRegistryEntryID() : 0);
			
		}
		else if (!strcmp("esg", cmdstr))
		{
			IOPCIEventSource * src = createEventSource(this, &PCIEventTest, 0);
			getWorkLoop()->addEventSource(src);
			src->enable();
			IOSleep(10*1000);
			getWorkLoop()->removeEventSource(src);
			src->release();
		}
		else if (str)
		{
			arg = strtoq(str->getCStringNoCopy(), NULL, 0);
			if (arg) matching = registryEntryIDMatching(arg);
			else     matching = nameMatching(str->getCStringNoCopy());
			victimS = copyMatchingService(matching);
		    matching->release();
			victim = OSDynamicCast(IOPCIDevice, victimS);
		}
		if (victim)
		{
			if (!strncmp("pause", cmdstr, strlen("pause")))
			{
				victim->reserved->pauseFlags = true;
				if ('d' == cmdstr[strlen("pause")]) victim->reserved->pauseFlags++;
				configOpParams cp = {.device = victim, .op = kConfigOpTestPause, .result = nullptr};
				configOp(&cp);
				ret = kIOReturnSuccess;
			}
			else if (!strcmp("unpause", cmdstr))
			{
				victim->changePowerStateToPriv(kIOPCIDeviceOnState);
				victim->powerOverrideOffPriv();
				ret = kIOReturnSuccess;
			}
			else if (!strcmp("reset", cmdstr))
			{
				uint16_t control;
				control = victim->configRead16(kPCI2PCIBridgeControl);
				control |= (1 << 6);
				victim->configWrite16(kPCI2PCIBridgeControl, control);
				IOSleep(10);
				control &= ~(1 << 6);
				victim->configWrite16(kPCI2PCIBridgeControl, control);
				ret = kIOReturnSuccess;
			}
			else if (!strncmp("gen", cmdstr, strlen("gen")))
			{
				uint16_t control;
				control = (('1' == cmdstr[strlen("gen")]) ? 1 : 2);
				control |= 0x30;
	            victim->configWrite16(victim->reserved->expressCapability + 0x30, control);
			    control = victim->configRead16(victim->reserved->expressCapability + 0x10);
				control |= (1 << 5);
				victim->configWrite16(victim->reserved->expressCapability + 0x10, control);
				IOSleep(100);
				kprintf("link speed %d\n", (15 & victim->configRead16(victim->reserved->expressCapability + 0x12)));
				ret = kIOReturnSuccess;
			}
			else if (!strcmp("ltr", cmdstr))
			{
				IOOptionBits type = 0;
				uint64_t nsecs = 0;
				str = OSDynamicCast(OSString, array->getObject(2));
				if (str) type = static_cast<IOOptionBits>(strtoq(str->getCStringNoCopy(), NULL, 0));
				str = OSDynamicCast(OSString, array->getObject(3));
				if (str) nsecs = strtoq(str->getCStringNoCopy(), NULL, 0);
				ret = victim->setLatencyTolerance(type, nsecs);
				kprintf("setLatencyTolerance 0x%x\n", ret);
				ret = kIOReturnSuccess;
			}
			else if (!strcmp("cycle", cmdstr))
			{
				uint32_t      idx, did;
				IOPCIBridge * bridge;
				IOPCIBridge * pbridge;
				IOPCIDevice * parent;
				bridge = OSDynamicCast(IOPCIBridge, victim->getProvider());
				parent = OSDynamicCast(IOPCIDevice, bridge->getProvider());
				pbridge = OSDynamicCast(IOPCIBridge, parent->getProvider());

				for (idx = 0; idx < 100; idx++)
				{
					bridge->setDevicePowerState(victim,  0, kIOPCIDeviceOnState,  kIOPCIDeviceOffState);
					pbridge->setDevicePowerState(parent, 0, kIOPCIDeviceOnState,  kIOPCIDeviceOffState);
					pbridge->setDevicePowerState(parent, 0, kIOPCIDeviceOffState, kIOPCIDeviceOnState);
					bridge->setDevicePowerState(victim,  0, kIOPCIDeviceOffState, kIOPCIDeviceOnState);
					did = victim->configRead32(0);
					if (0xffffffff == did) panic("did");
				}
				ret = kIOReturnSuccess;
			}
			else if (!strcmp("es", cmdstr))
			{
				IOPCIEventSource * src = victim->createEventSource(this, &PCIEventTest, 0);
				victim->getWorkLoop()->addEventSource(src);
				src->enable();
				IOSleep(10*1000);
				victim->getWorkLoop()->removeEventSource(src);
				src->release();
			}
			else if (!strcmp("l1ena", cmdstr))
			{
				ret = victim->setTunnelL1Enable(victim, true);
			}
			else if (!strcmp("l1dis", cmdstr))
			{
				ret = victim->setTunnelL1Enable(victim, false);
			}
#if ACPI_SUPPORT
			else if (!strncmp("map", cmdstr, strlen("map")))
			{

				IOMapper * mapper;
				IOBufferMemoryDescriptor * bmd;
				IODMAMapSpecification mapSpec;
				uint32_t mapOptions;
				uint64_t mapAddress1, mapAddress2;
				uint64_t mapLength1, mapLength2;
				size_t   bufLen;

				bzero(&mapSpec, sizeof(mapSpec));

				mapSpec.numAddressBits = 32;
				mapSpec.alignment      = 4096;
				bufLen = 65536;

				mapper = IOMapper::copyMapperForDevice(victim);
				if (mapper)
				{
					bmd = IOBufferMemoryDescriptor::inTaskWithOptions(
								kernel_task, kIODirectionOutIn, bufLen);

					mapOptions = 0;
					mapOptions |= kIODMAMapReadAccess;
					mapOptions |= kIODMAMapWriteAccess;
					mapOptions |= kIODMAMapFixedAddress;

					mapAddress1 = 0x12345000;
					mapLength1 = bufLen;

					ret = mapper->iovmMapMemory(bmd, 0, bufLen, mapOptions,
								&mapSpec, NULL, NULL, &mapAddress1, &mapLength1);
					kprintf("iovmMapMemory 0x%x, 0x%qx\n", ret, mapAddress1);

					mapAddress2 = 0x76543000;
					mapLength2 = bufLen;

					ret = mapper->iovmMapMemory(bmd, 0, bufLen, mapOptions,
								&mapSpec, NULL, NULL, &mapAddress2, &mapLength2);
					kprintf("iovmMapMemory 0x%x, 0x%qx\n", ret, mapAddress2);

					ret = mapper->iovmUnmapMemory(bmd, 0, mapAddress1, mapLength1);
					kprintf("iovmUnmapMemory 0x%x, 0x%qx\n", ret, mapAddress1);

					ret = mapper->iovmUnmapMemory(bmd, 0, mapAddress2, mapLength2);
					kprintf("iovmUnmapMemory 0x%x, 0x%qx\n", ret, mapAddress2);


					bmd->release();
					mapper->release();
				}
				ret = kIOReturnSuccess;
			}
#endif /* ACPI_SUPPORT */
		}
		if (victimS)
		{
			if (!strcmp("psreset", cmdstr))
			{
				victimS->setProperty(kIOPMResetPowerStateOnWakeKey, kOSBooleanTrue);
				ret = kIOReturnSuccess;
			}
			victimS->release();
		}
    }
	else ret = super::setProperties(properties);

	return (ret);
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOPCIBridge::newUserClient(task_t owningTask, void * securityID,
                           UInt32 type,  OSDictionary * properties,
                           IOUserClient ** handler)
{
#if !DEVELOPMENT && !defined(__x86_64__)
    return (super::newUserClient(owningTask, securityID, type, properties, handler));
#else /* !DEVELOPMENT && !defined(__x86_64__) */

    IOPCIDiagnosticsClient * uc;
    bool                     ok;

    if (type != kIOPCIDiagnosticsClientType)
        return (super::newUserClient(owningTask, securityID, type, properties, handler));

	ok = false;
	uc = NULL;
    do
    {
		uc = OSTypeAlloc(IOPCIDiagnosticsClient);
        if (!uc) break;
        ok = uc->initWithTask(owningTask, securityID, type, properties);
		uc->owner = this;
        if (!ok) break;
        ok = uc->attach(this);
        if (!ok) break;
        ok = uc->start(this);
    }
    while (false);

    if (ok)
    {
        *handler = uc;
        return (kIOReturnSuccess);
    }
    else
    {
        if (uc && uc->getRegistryEntryID())
            uc->detach(this);
        if (uc) uc->release();
        *handler = NULL;
        return (kIOReturnUnsupported);
    }
#endif /* !DEVELOPMENT && !defined(__x86_64__) */
}

IOReturn
IOPCIBridge::terminateChild(IOPCIDevice *child)
{
    IOPCIHostBridgeData *vars = reserved->hostBridgeData;

    // Terminate the child and all its functions in the host bridge workloop.
    // In the event of a multi-function device client crash, only one function
    // will trigger the reset.
    return (vars->_configWorkLoop->runAction(
                OSMemberFunctionCast(IOCommandGate::Action, this, &IOPCIBridge::terminateChildGated),
                this, child));
}

void
IOPCIBridge::hotReset(IOPCIDevice *bridgeDevice)
{
    // Toggle secondary bus reset bit
    UInt16 bridgeControl = bridgeDevice->extendedConfigRead16(kPCI2PCIBridgeControl);
    bridgeDevice->extendedConfigWrite16(kPCI2PCIBridgeControl, bridgeControl | (1 << 6));
    IOSleep(10);
    bridgeDevice->extendedConfigWrite16(kPCI2PCIBridgeControl, bridgeControl);
}

void
IOPCIBridge::warmResetDisable(void)
{
	panic("Warm reset not implemented in IOPCIBridge");
}

void
IOPCIBridge::warmResetEnable(void)
{
	panic("Warm reset not implemented in IOPCIBridge");
}

IOReturn
IOPCIBridge::waitForLinkUp(IOPCIDevice *bridgeDevice)
{
	IOReturn ret = kIOReturnSuccess;

    // Poll for link-up for up to 1s.
    AbsoluteTime deadline, now = 0;
    clock_interval_to_deadline(1, kSecondScale, &deadline);
    do
    {
        if (checkLink(kCheckLinkForPower) == kIOReturnSuccess) break;
		IOSleep(1);
        clock_get_uptime(&now);
    }
    while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));

    if (AbsoluteTime_to_scalar(&now) >= AbsoluteTime_to_scalar(&deadline))
    {
        IOLog("%s: link-up poll timed out", bridgeDevice->getName());
		ret = kIOReturnTimeout;
    }

    // Wait 100 ms after link up before making any configuration requests (PCI Express Base 4.0 - 6.6.1)
    IOSleep(100);

	return ret;
}

IOReturn
IOPCIBridge::waitForDeviceReady(IOPCIDevice *child)
{
	IOReturn ret = kIOReturnSuccess;
    AbsoluteTime deadline, now = 0;

    // Following conventional reset, wait up to 1s for the device to properly respond to
    // configuration requests (sec 6.6.1), checking for VID/DID 0x0001/0xFFFF in case the RC
    // implements CRS (sec 2.3.2). If device fails to restore within this window, the
    // subsequent probe will not find it.
    clock_interval_to_deadline(1, kSecondScale, &deadline);
    do
    {
        uint32_t vendorProduct = child->extendedConfigRead32(kIOPCIConfigVendorID);

        if(   vendorProduct != 0 
           && vendorProduct != 0xFFFFFFFFUL
           && vendorProduct != 0xFFFF0001UL)
        {
            DLOG("[%s()]::%s success\n", __PRETTY_FUNCTION__, child->getName());
            break;
        }
		IOSleep(1);
        clock_get_uptime(&now);
    }
    while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));

    if (AbsoluteTime_to_scalar(&now) >= AbsoluteTime_to_scalar(&deadline))
    {
		DLOG("[%s()]::%s timeout\n", __PRETTY_FUNCTION__, child->getName());
		ret = kIOReturnNoDevice;
    }

	return ret;
}

bool
IOPCIBridge::supportsWarmReset(void)
{
	return false;
}

void
IOPCIBridge::warmReset(void)
{
	warmResetDisable();

	warmResetEnable();
}

IOReturn
IOPCIBridge::terminateChildGated(IOPCIDevice *child)
{
    IOPCIDevice* bridgeDevice = OSDynamicCast(IOPCIDevice, getParentEntry(gIOServicePlane));
    if (bridgeDevice == NULL)
	{
		return kIOReturnError;
	}

    if (child->isInactive())
    {
        return kIOReturnNoDevice;
    }

    IOPCIResetType resetType = kIOPCIResetHot;

    if (child->getProperty("built-in") && child->getProperty("crash-terminate-self-only"))
    {
        resetType = kIOPCIResetNone;
    }

    if (child->reserved->crashNotification)
    {
        DLOG("[%s()] Invoking %s's crash notification\n", __func__, child->getName());
        resetType = child->reserved->crashNotification(child->reserved->crashNotificationRef, child);
        DLOG("[%s()] %s's crash notification returned %u\n", __func__, child->getName(), resetType);
    }

    if (resetType > kIOPCIResetHot)
    {
        DLOG("[%s()] Invalid reset type %u, using hot reset\n", __func__, resetType);
        resetType = kIOPCIResetHot;
    }

    // Since the bridge is likely not hot-plug capable its desired power state is
    // off. Temporarily keep it on so we don't save config registers for when the link/device aren't present.
    // This is an asynchronous request, but will be ordered before the child's PMstop() that
    // occurs during termination.
    temporaryPowerClampOn();

    OSIterator* peerIterator = getChildIterator(gIOServicePlane);
    OSObject*   peer         = NULL;
    while (   (peerIterator != NULL)
           && ((peer = peerIterator->getNextObject()) != NULL))
    {
        IOPCIDevice* pciPeer = OSDynamicCast(IOPCIDevice, peer);
        if (   (pciPeer != NULL)
            && (pciPeer->isInactive() == false)
            && (pciPeer == child || resetType == kIOPCIResetHot))
        {
            uint16_t command = pciPeer->extendedConfigRead16(kIOPCIConfigurationOffsetCommand);
            if ((command & (kIOPCICommandBusLead | kIOPCICommandMemorySpace)) != 0)
            {
                DLOG("%s disabling memory and bus leading for device %u:%u:%u\n", __PRETTY_FUNCTION__, PCI_ADDRESS_TUPLE(pciPeer));
                pciPeer->extendedConfigWrite16(kIOPCIConfigurationOffsetCommand, command & ~(kIOPCICommandBusLead | kIOPCICommandMemorySpace));
            }
        }
    }
    OSSafeReleaseNULL(peerIterator);

    switch (resetType)
    {
    case kIOPCIResetHot:
        // Issue a hot reset (AKA secondary bus reset) for the bridge's children.
        hotReset(bridgeDevice);
		waitForLinkUp(bridgeDevice);
		waitForDeviceReady(child);
        break;
    case kIOPCIResetNone:
        break;
    }

    // Wait up to 1s for each terminating device's non-posted requests to complete.
    // They must complete before the DART mappings are destroyed during terminate.
    peerIterator = getChildIterator(gIOServicePlane);
    while (   (peerIterator != NULL)
           && ((peer = peerIterator->getNextObject()) != NULL))
    {
        IOPCIDevice* pciPeer = OSDynamicCast(IOPCIDevice, peer);
        if (   (pciPeer != NULL)
            && (pciPeer->isInactive() == false)
            && (pciPeer == child || resetType == kIOPCIResetHot))
        {
            uint32_t retryTimeoutMS = 1000;
            uint32_t retryCount = 0;

            while (pciPeer->extendedConfigRead8(kIOPCIConfigStatus) & (1 << 5))
            {
                IOSleep(1);
                if (++retryCount == retryTimeoutMS)
                    break;
            }
            // Nothing to do if this times out. The fabric is likely hung.
            if (retryCount == retryTimeoutMS)
                IOLog("%s Unable to flush device %u:%u:%u\n", __PRETTY_FUNCTION__, PCI_ADDRESS_TUPLE(pciPeer));
        }
    }
    OSSafeReleaseNULL(peerIterator);

    // Wait an additional 100ms (arbitrary) for any posted requests to complete.
    // "Writes to system memory using TC0 will be flushed by host reads of the
    // device" (PCIe spec Section 6.4, "Device Synchronization") -- the
    // previous status register read(s) achieved that -- but without client driver
    // involvement there's no guaranteed way to flush non-TC0 posted requests.
    IOSleep(100);

    // Terminate the child IOPCIDevice(s)
    peerIterator = getChildIterator(gIOServicePlane);
    while (   (peerIterator != NULL)
           && ((peer = peerIterator->getNextObject()) != NULL))
    {
        IOPCIDevice* pciPeer = OSDynamicCast(IOPCIDevice, peer);
        if (   (pciPeer != NULL)
            && (pciPeer->isInactive() == false)
            && (pciPeer == child || resetType == kIOPCIResetHot))
        {
            DLOG("%s Terminating device %u:%u:%u\n", __PRETTY_FUNCTION__, PCI_ADDRESS_TUPLE(pciPeer));
            pciPeer->terminate(kIOServiceTerminateNeedWillTerminate);
        }
    }
    OSSafeReleaseNULL(peerIterator);

    return kIOReturnSuccess;
}

bool IOPCIBridge::isSupportedLinkSpeed(IOPCIDevice *device, tIOPCILinkSpeed linkSpeed)
{
	uint32_t linkCaps2 = 0, supportedLinkSpeeds = 0;

	linkCaps2 = device->configRead32(device->reserved->expressCapability + 0x2C);
	supportedLinkSpeeds = (linkCaps2 >> 1) & 0x7F;

	// PCIe base spec implementation note "Software Management of Link Speeds With
	// Earlier Hardware":
	// For software to determine the supported Link speeds for components where
	// the Link Capabilities 2 Register is either not implemented, or the value of
	// its Supported Link Speeds Vector is 0000000b, software can read bits 3:0 of
	// the Link Capabilities Register (now defined to be the Max Link Speed field),
	// and interpret the value as follows:
	// 0001b
	//   2.5 GT/s Link speed supported
	// 0010b
	//   5.0 GT/s and 2.5 GT/s Link speeds supported
	if (supportedLinkSpeeds == 0)
	{
		uint32_t linkCaps = device->configRead32(device->reserved->expressCapability + 0xC);
		uint8_t maxLinkSpeed = linkCaps & 0xF;

		supportedLinkSpeeds = (maxLinkSpeed == 2) ? 0x3 : 0x1;
	}

	return supportedLinkSpeeds & (1 << (linkSpeed - 1));
}

void IOPCIBridge::setTargetLinkSpeed(IOPCIDevice *device, tIOPCILinkSpeed linkSpeed)
{
	uint8_t expressCapability = device->reserved->expressCapability;
	uint8_t speed = static_cast<uint8_t>(linkSpeed);
	uint16_t linkControl2 = device->configRead32(expressCapability + 0x30);
	linkControl2 = (linkControl2 & ~0xF) | speed;
	device->configWrite32(expressCapability + 0x30, linkControl2);
}

IOReturn IOPCI2PCIBridge::setLinkSpeed(tIOPCILinkSpeed linkSpeed, bool retrain)
{
	uint8_t expressCapability = fBridgeDevice->reserved->expressCapability;
	uint16_t linkStatus = 0;

	if (!isSupportedLinkSpeed(fBridgeDevice, linkSpeed))
	{
		return kIOReturnUnsupported;
	}

	setTargetLinkSpeed(fBridgeDevice, linkSpeed);

	if (!retrain)
	{
		return kIOReturnSuccess;
	}

	// Per the PCIe base spec's implementation note "Avoiding Race Conditions
	// When Using the Retrain Link Bit", wait for the Link Training bit to go
	// to 0 before initiating a retrain.
	AbsoluteTime deadline, now;
	clock_get_uptime(&now);
	clock_interval_to_deadline(1, kSecondScale, &deadline);
	do
	{
		linkStatus = fBridgeDevice->configRead16(expressCapability + 0x12);
		if ((linkStatus & (1 << 11)) == 0)
		{
			break;
		}
		IOSleep(1);
		clock_get_uptime(&now);
	}
	while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));

	if (AbsoluteTime_to_scalar(&now) >= AbsoluteTime_to_scalar(&deadline))
	{
		IOLog("Link Training bit didn't reach 0 within 1s.");
		return kIOReturnError;
	}

	// Set the Link Retrain bit
	uint16_t linkControl = fBridgeDevice->configRead16(expressCapability + 0x10);
	linkControl |= (1 << 5);
	fBridgeDevice->configWrite16(expressCapability + 0x10, linkControl);

	// Poll for link training completion for up to 1s
	clock_get_uptime(&now);
	clock_interval_to_deadline(1, kSecondScale, &deadline);
	do
	{
		linkStatus = fBridgeDevice->configRead16(expressCapability + 0x12);
		if ((linkStatus & (1 << 11)) == 0)
		{
			break;
		}
		IOSleep(1);
		clock_get_uptime(&now);
	}
	while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));

	if (AbsoluteTime_to_scalar(&now) >= AbsoluteTime_to_scalar(&deadline))
	{
		IOLog("Link Training didn't complete within 1s.\n");
		return kIOReturnError;
	}

	updateLinkStatusProperty(linkStatus);

	DLOG("%s: link training complete, linkStatus: 0x%x\n", fBridgeDevice->getName(), linkStatus);

	return kIOReturnSuccess;
}

IOReturn IOPCI2PCIBridge::getLinkSpeed(tIOPCILinkSpeed *linkSpeed)
{
	uint16_t linkStatus;

	if (linkSpeed == nullptr)
	{
		return kIOReturnBadArgument;
	}

	linkStatus = fBridgeDevice->configRead16(fBridgeDevice->reserved->expressCapability + 0x12);

	*linkSpeed = static_cast<tIOPCILinkSpeed>(linkStatus & 0xF);

	return kIOReturnSuccess;
}

#define tResetValidTypes   0x0000000F
#define tResetValidOptions 0x00000003

IOReturn IOPCIBridge::resetDeviceGated(tIOPCIDeviceResetTypes *_type, tIOPCIDeviceResetOptions *_options)
{
	tIOPCIDeviceResetOptions options = *_options;
	tIOPCIDeviceResetTypes type = *_type;
	IOReturn ret = kIOReturnError;
	bool needsProbe;

    IOPCIDevice* bridgeDevice = OSDynamicCast(IOPCIDevice, getParentEntry(gIOServicePlane));
    if (bridgeDevice == NULL)
	{
		return kIOReturnError;
	}

	// -- Error checking --
	// Check for multiple reset types and undefined bits
	if ((__builtin_popcount(type) != 1) ||
		(type & ~tResetValidTypes) ||
		(options & ~tResetValidOptions))
	{
		return kIOReturnBadArgument;
	}

	// Reset type-specific error checking
	switch (type)
	{
	case kIOPCIDeviceResetTypeHotReset:
		break;

	case kIOPCIDeviceResetTypeWarmReset:
		if (!supportsWarmReset())
		{
			DLOG("[%s()] Bridge %u:%u:%u does not support warm reset\n", __func__, PCI_ADDRESS_TUPLE(bridgeDevice));
			return kIOReturnBadArgument;
		}
		break;

	case kIOPCIDeviceResetTypeWarmResetDisable:
		if (!supportsWarmReset())
		{
			DLOG("[%s()] Bridge %u:%u:%u does not support warm reset\n", __func__, PCI_ADDRESS_TUPLE(bridgeDevice));
			return kIOReturnBadArgument;
		}
		if (reserved->childrenInReset)
		{
			DLOG("[%s()] Bridge %u:%u:%u's children already disabled\n", __func__, PCI_ADDRESS_TUPLE(bridgeDevice));
			return kIOReturnBadArgument;
		}
		break;

	case kIOPCIDeviceResetTypeWarmResetEnable:
		if (!supportsWarmReset())
		{
			DLOG("[%s()] Bridge %u:%u:%u does not support warm reset\n", __func__, PCI_ADDRESS_TUPLE(bridgeDevice));
			return kIOReturnBadArgument;
		}
		if (!reserved->childrenInReset)
		{
			DLOG("[%s()] Bridge %u:%u:%u's children already enabled\n", __func__, PCI_ADDRESS_TUPLE(bridgeDevice));
			return kIOReturnBadArgument;
		}

		break;
	}

	// -- Block further device usage --
	if (type != kIOPCIDeviceResetTypeWarmResetEnable)
	{
		OSIterator* childIterator = getChildIterator(gIOServicePlane);
		OSObject*   childObj      = NULL;
		while (   (childIterator != NULL)
				&& ((childObj = childIterator->getNextObject()) != NULL))
		{
			IOPCIDevice *child = OSDynamicCast(IOPCIDevice, childObj);

			if (child == NULL)
			{
				continue;
			}

			DLOG("[%s()] Powering off child %s\n", __func__, child->getName());

			if (!(options & kIOPCIDeviceResetOptionTerminate))
			{
				saveDeviceState(child, kIOPCIConfigShadowVolatile);
			}

			child->setPCIPowerState(kIOPCIDeviceOffState, 0);
		}
		OSSafeReleaseNULL(childIterator);

		DLOG("[%s()] Marking children in-reset\n", __func__);
		setInReset(true);

		// Block downstream I/O and memory-space accesses
		uint16_t command = bridgeDevice->configRead16(kIOPCIConfigCommand);
		bridgeDevice->configWrite16(kIOPCIConfigCommand,
									(command & ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace)));
	}


	// -- Perform reset --
	DLOG("[%s()] Resetting children\n", __func__);
	switch (type)
	{
	case kIOPCIDeviceResetTypeHotReset:
		hotReset(bridgeDevice);
		break;

	case kIOPCIDeviceResetTypeWarmReset:
		warmReset();
		break;

	case kIOPCIDeviceResetTypeWarmResetDisable:
		warmResetDisable();
		break;

	case kIOPCIDeviceResetTypeWarmResetEnable:
		warmResetEnable();
		break;
	}

	// -- Wait for reset to complete and unblock device usage --
	if (type != kIOPCIDeviceResetTypeWarmResetDisable)
	{
		// Unblock downstream I/O and memory-space accesses
		uint16_t command = bridgeDevice->configRead16(kIOPCIConfigCommand);
		bridgeDevice->configWrite16(kIOPCIConfigCommand,
									command | kIOPCICommandIOSpace | kIOPCICommandMemorySpace);

		ret = waitForLinkUp(bridgeDevice);

		if (ret != kIOReturnSuccess)
		{
			// Link-up failed, terminate children
			options = static_cast<tIOPCIDeviceResetOptions>(options | kIOPCIDeviceResetOptionTerminate);
		}

		DLOG("[%s()] Clearing children in-reset\n", __func__);
		setInReset(false);

		OSIterator* childIterator = getChildIterator(gIOServicePlane);
		OSObject*   childObj      = NULL;
		while (   (childIterator != NULL)
				&& ((childObj = childIterator->getNextObject()) != NULL))
		{
			IOPCIDevice *child = OSDynamicCast(IOPCIDevice, childObj);

			// manual-enable-s2r ports deassert PERST in response to system wake events, before
			// its IOPower tree has performed setPowerState(). Skip this step in that case.
			if (child == NULL || (bridgeDevice->getProperty("manual-enable-s2r") && getPowerState() == kIOPCIDeviceOffState))
			{
				continue;
			}

			DLOG("[%s()] Powering on child %s\n", __func__, child->getName());

			// Put device in D0 (h/w will already be in D0, this call is needed to update software state)
			child->setPCIPowerState(kIOPCIDeviceOnState, 0);

			// Restore config state, unless we're terminating the child
			if (!(options & kIOPCIDeviceResetOptionTerminate))
			{
				restoreDeviceState(child, 0);
			}
		}
		OSSafeReleaseNULL(childIterator);

		// Request a probe if we're enabling the downstream device for the first time
		needsProbe = (getChildEntry(gIOServicePlane) == NULL);
	}
	else
	{
		ret = kIOReturnSuccess;
	}

	if (options & kIOPCIDeviceResetOptionTerminate)
	{
		OSIterator* childIterator = getChildIterator(gIOServicePlane);
		OSObject*   childObj      = NULL;
		while (   (childIterator != NULL)
				&& ((childObj = childIterator->getNextObject()) != NULL))
		{
			IOPCIDevice *child = OSDynamicCast(IOPCIDevice, childObj);
			if (child == NULL)
			{
				continue;
			}

			child->terminate();
		}
		OSSafeReleaseNULL(childIterator);
	}

	if ((options & kIOPCIDeviceResetOptionTerminate) || needsProbe)
	{
		// Launch a thread to re-probe the bridge once any terminations complete
		thread_call_t threadCall = thread_call_allocate(OSMemberFunctionCast(thread_call_func_t,
		                                                                     this,
		                                                                     &IOPCIBridge::waitForTerminateThreadCall),
		                                                this);

		if(threadCall != NULL)
		{
			retain();
			if(thread_call_enter1(threadCall, threadCall /* so the call cleans itself up */) == TRUE)
			{
				thread_call_free(threadCall);
				release();
			}
		}
	}

	return ret;
}

IOReturn IOPCIBridge::waitForTerminateThreadCall(thread_call_t threadCall)
{
	IOPCIDevice* bridgeDevice = OSDynamicCast(IOPCIDevice, getParentEntry(gIOServicePlane));
	if (bridgeDevice == NULL)
	{
		return kIOReturnError;
	}

	DLOG("%s waiting for downstream devices to finish terminating\n", __PRETTY_FUNCTION__);
	// wait for the drivers and termination to settle
	waitQuiet();

	DLOG("%s reprobing bus\n", __PRETTY_FUNCTION__);
	// re-scan the bridge for this device and its functions
	kernelRequestProbe(bridgeDevice, kIOPCIProbeOptionNeedsScan | kIOPCIProbeOptionDone);

	// clean up threadcall
	release();
	thread_call_free(threadCall);

	return kIOReturnSuccess;
}

IOReturn IOPCIBridge::resetDevice(tIOPCIDeviceResetTypes type, tIOPCIDeviceResetOptions options)
{
    IOPCIHostBridgeData *vars = reserved->hostBridgeData;

    DLOG("%s[%p]::%s(0x%x, 0x%x)\n", getName(), this, __func__, type, options);

    return (vars->_configWorkLoop->runAction(
                OSMemberFunctionCast(IOCommandGate::Action, this, &IOPCIBridge::resetDeviceGated),
                this, &type, &options));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if !DEVELOPMENT && !defined(__x86_64__)
#else

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOPCIDiagnosticsClient, IOUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIDiagnosticsClient::initWithTask(task_t owningTask,
										  void * securityID,
										  UInt32 type,
										  OSDictionary * properties)
{
    uint32_t bootArg;

	if (kIOReturnSuccess != clientHasPrivilege(
		securityID, kIOClientPrivilegeAdministrator))                      return (false);
	if (!PE_i_can_has_debugger(&bootArg) || !bootArg)                      return (false);

    return (super::initWithTask(owningTask, securityID, type, properties));
}

IOReturn IOPCIDiagnosticsClient::clientClose(void)
{
	terminate();
    return (kIOReturnSuccess);
}

IOService * IOPCIDiagnosticsClient::getService(void)
{
    return (owner);
}

IOReturn IOPCIDiagnosticsClient::setProperties(OSObject * properties)
{
    IOReturn            kr = kIOReturnUnsupported;
    return (kr);
}

IOReturn IOPCIDiagnosticsClient::externalMethod(uint32_t selector, IOExternalMethodArguments * args,
												IOExternalMethodDispatch * dispatch, OSObject * target, void * reference)
{
    IOReturn                     ret = kIOReturnBadArgument;
    IOPCIDiagnosticsParameters * params;
	IOMemoryDescriptor         * md;
	IOMemoryMap                * map;
	void                       * vmaddr;

    switch (selector)
    {
        case kIOPCIDiagnosticsMethodWrite:
            if (args->structureInputSize != sizeof(IOPCIDiagnosticsParameters)) return (kIOReturnBadArgument);

			params = (typeof(params)) args->structureInput;
            break;

        case kIOPCIDiagnosticsMethodRead:
            if (args->structureInputSize  != sizeof(IOPCIDiagnosticsParameters)) return (kIOReturnBadArgument);
            if (args->structureOutputSize != sizeof(IOPCIDiagnosticsParameters)) return (kIOReturnBadArgument);

			bcopy(args->structureInput, args->structureOutput, sizeof(IOPCIDiagnosticsParameters));
			params = (typeof(params)) args->structureOutput;
            break;

        default:
        	return (kIOReturnBadArgument);
            break;
	}

	map = 0;
	vmaddr = 0;
	if (kIOPCI64BitMemorySpace == params->spaceType)
	{
		md = IOMemoryDescriptor::withAddressRange(params->address.addr64, 
				(params->bitWidth >> 3), kIODirectionOutIn | kIOMemoryMapperNone, NULL);
		if (md)
		{
			map = md->map();
			md->release();
		}
		if (!map) return (kIOReturnVMError);
		vmaddr = (void *)(uintptr_t) map->getAddress();
	}

    switch (selector)
    {
        case kIOPCIDiagnosticsMethodWrite:

			if (kIOPCI64BitMemorySpace == params->spaceType)
			{
				switch (params->bitWidth)
				{
					case 8:
						*((uint8_t *) vmaddr) = params->value;
						ret = kIOReturnSuccess;
						break;
					case 16:
						*((uint16_t *) vmaddr) = params->value;
						ret = kIOReturnSuccess;
						break;
					case 32:
						*((uint32_t *) vmaddr) = static_cast<uint32_t>(params->value);
						ret = kIOReturnSuccess;
						break;
					case 64:
						*((uint64_t *) vmaddr) = params->value;
						ret = kIOReturnSuccess;
						break;
					default:
						break;
				}
			}
			else if (kIOPCIConfigSpace == params->spaceType)
			{
				IOPCIAddressSpace space;
				space.bits                   = 0;
				space.es.busNum              = params->address.pci.bus;
				space.es.deviceNum           = params->address.pci.device;
				space.es.functionNum         = params->address.pci.function;
				space.es.registerNumExtended = (0xF & (params->address.pci.offset >> 8));
				switch (params->bitWidth)
				{
					case 8:
						owner->configWrite8(space, params->address.pci.offset, params->value);
						ret = kIOReturnSuccess;
						break;
					case 16:
						owner->configWrite16(space, params->address.pci.offset, params->value);
						ret = kIOReturnSuccess;
						break;
					case 32:
						owner->configWrite32(space, params->address.pci.offset, static_cast<uint32_t>(params->value));
						ret = kIOReturnSuccess;
						break;
					default:
						break;
				}
			}
			break;

        case kIOPCIDiagnosticsMethodRead:

			if (kIOPCI64BitMemorySpace == params->spaceType)
			{
				switch (params->bitWidth)
				{
					case 8:
						params->value = *((uint8_t *) vmaddr);
						ret = kIOReturnSuccess;
						break;
					case 16:
						params->value = *((uint16_t *) vmaddr);
						ret = kIOReturnSuccess;
						break;
					case 32:
						params->value = *((uint32_t *) vmaddr);
						ret = kIOReturnSuccess;
						break;
					case 64:
						params->value = *((uint64_t *) vmaddr);
						ret = kIOReturnSuccess;
						break;
					default:
						break;
				}
			}
			else if (kIOPCIConfigSpace == params->spaceType)
			{
				IOPCIAddressSpace space;
				space.bits                   = 0;
				space.es.busNum              = params->address.pci.bus;
				space.es.deviceNum           = params->address.pci.device;
				space.es.functionNum         = params->address.pci.function;
				space.es.registerNumExtended = (0xF & (params->address.pci.offset >> 8));
				switch (params->bitWidth)
				{
					case 8:
						params->value = owner->configRead8(space, params->address.pci.offset);
						ret = kIOReturnSuccess;
						break;
					case 16:
						params->value = owner->configRead16(space, params->address.pci.offset);
						ret = kIOReturnSuccess;
						break;
					case 32:
						params->value = owner->configRead32(space, params->address.pci.offset);
						ret = kIOReturnSuccess;
						break;
					default:
						break;
				}
			}
			break;

        default:
            break;
    }

    if (map) map->release();

    return (ret);
}

#endif /* !DEVELOPMENT && !defined(__x86_64__) */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

