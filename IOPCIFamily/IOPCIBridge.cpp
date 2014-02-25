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

#include <IOKit/system.h>

#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/pci/IOAGPDevice.h>
#include <IOKit/pci/IOPCIConfigurator.h>
#if ACPI_SUPPORT
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#endif

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOSubMemoryDescriptor.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>
#include <IOKit/assert.h>
#include <IOKit/IOCatalogue.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOPolledInterface.h>
#include <IOKit/IOUserClient.h>

#include <libkern/c++/OSContainers.h>
#include <libkern/OSKextLib.h>
#include <libkern/version.h>

extern "C"
{
#include <machine/machine_routines.h>
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef VERSION_MAJOR
#error VERSION_MAJOR
#endif

#if         VERSION_MAJOR < 10
#define     ROM_KEXTS       1
#endif

#ifndef kIOPolledInterfaceActiveKey
#define kIOPolledInterfaceActiveKey  "IOPolledInterfaceActive"
#endif

// #define DEADTEST		"UPS0"
// #define DEFERTEST	1

enum
{
    kIOPCIClassBridge           = 0x06,
    kIOPCIClassNetwork          = 0x02,
    kIOPCIClassGraphics         = 0x03,
    kIOPCIClassMultimedia       = 0x04,

    kIOPCISubClassBridgeHost    = 0x00,
    kIOPCISubClassBridgeISA     = 0x01,
    kIOPCISubClassBridgeEISA    = 0x02,
    kIOPCISubClassBridgeMCA     = 0x03,
    kIOPCISubClassBridgePCI     = 0x04,
    kIOPCISubClassBridgePCMCIA  = 0x05,
    kIOPCISubClassBridgeNuBus   = 0x06,
    kIOPCISubClassBridgeCardBus = 0x07,
    kIOPCISubClassBridgeRaceWay = 0x08,
    kIOPCISubClassBridgeOther   = 0x80,
};

enum
{
	kCheckLinkParents = 0x00000001,
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

const IORegistryPlane * gIOPCIACPIPlane;

static class IOPCIMessagedInterruptController  * gIOPCIMessagedInterruptController;

static IOSimpleLock *      gIOAllPCI2PCIBridgesLock;
UInt32                     gIOAllPCI2PCIBridgeState;

static IOLock *      	   gIOPCIWakeReasonLock;

const OSSymbol *		   gIOPCITunnelIDKey;
const OSSymbol *		   gIOPCITunnelledKey;
const OSSymbol *		   gIOPCIThunderboltKey;

const OSSymbol *           gIOPlatformDeviceMessageKey;
const OSSymbol *           gIOPlatformDeviceASPMEnableKey;
const OSSymbol *           gIOPlatformSetDeviceInterruptsKey;
const OSSymbol *           gIOPlatformResolvePCIInterruptKey;
const OSSymbol *           gIOPlatformFreeDeviceResourcesKey;
const OSSymbol *           gIOPlatformGetMessagedInterruptControllerKey;
const OSSymbol *           gIOPlatformGetMessagedInterruptAddressKey;
const OSSymbol *           gIOPolledInterfaceActiveKey;

#if ACPI_SUPPORT
const OSSymbol *           gIOPCIPSMethods[kIOPCIDevicePowerStateCount];
#endif

static queue_head_t        gIOAllPCIDeviceRestoreQ;
static uint32_t            gIOPCITunnelSleep;
static bool				   gIOPCITunnelWait;

static IOWorkLoop *        gIOPCIConfigWorkLoop;
static IOPCIConfigurator * gIOPCIConfigurator;

uint32_t gIOPCIFlags = 0
             | kIOPCIConfiguratorAllocate
             | kIOPCIConfiguratorPFM64
             | kIOPCIConfiguratorCheckTunnel
             | kIOPCIConfiguratorTBMSIEnable
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum
{
	// data link change, hot plug, presence detect change
	kSlotControlEnables = ((1 << 12) | (1 << 5) | (1 << 3))
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#include "vtd.c"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService
OSDefineMetaClassAndAbstractStructorsWithInit( IOPCIBridge, IOService, IOPCIBridge::initialize() )

OSMetaClassDefineReservedUsed(IOPCIBridge, 0);
OSMetaClassDefineReservedUsed(IOPCIBridge, 1);
OSMetaClassDefineReservedUsed(IOPCIBridge, 2);
OSMetaClassDefineReservedUsed(IOPCIBridge, 3);
OSMetaClassDefineReservedUsed(IOPCIBridge, 4);
OSMetaClassDefineReservedUnused(IOPCIBridge,  5);
OSMetaClassDefineReservedUnused(IOPCIBridge,  6);
OSMetaClassDefineReservedUnused(IOPCIBridge,  7);
OSMetaClassDefineReservedUnused(IOPCIBridge,  8);
OSMetaClassDefineReservedUnused(IOPCIBridge,  9);
OSMetaClassDefineReservedUnused(IOPCIBridge, 10);
OSMetaClassDefineReservedUnused(IOPCIBridge, 11);
OSMetaClassDefineReservedUnused(IOPCIBridge, 12);
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOPCIBridge::initialize(void)
{
    if (!gIOAllPCI2PCIBridgesLock)
    {
        gIOAllPCI2PCIBridgesLock = IOSimpleLockAlloc();
        queue_init(&gIOAllPCIDeviceRestoreQ);
        gIOPCIWakeReasonLock = IOLockAlloc();

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
        gIOPlatformGetMessagedInterruptAddressKey
        	= OSSymbol::withCStringNoCopy(kIOPlatformGetMessagedInterruptAddressKey);
        gIOPlatformGetMessagedInterruptControllerKey
        	= OSSymbol::withCStringNoCopy(kIOPlatformGetMessagedInterruptControllerKey);
#if ACPI_SUPPORT
		gIOPCIPSMethods[kIOPCIDeviceOffState]  = OSSymbol::withCStringNoCopy("_PS3");
		gIOPCIPSMethods[kIOPCIDeviceDozeState] = OSSymbol::withCStringNoCopy("RPS3");
		gIOPCIPSMethods[kIOPCIDeviceOnState]   = OSSymbol::withCStringNoCopy("_PS0");
#endif
        gIOPCIConfigWorkLoop = IOWorkLoop::workLoop();
    }
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

			space.s.busNum = strtoq(str ? str->getCStringNoCopy() : 0, NULL, 0);
			str = OSDynamicCast(OSString, array->getObject(2));
			space.s.deviceNum = strtoq(str ? str->getCStringNoCopy() : 0, NULL, 0);;
			str = OSDynamicCast(OSString, array->getObject(3));
			space.s.functionNum = strtoq(str ? str->getCStringNoCopy() : 0, NULL, 0);;
			IOReturn ret = configOp(NULL, kConfigOpFindEntry, &result, &space);
			kprintf("%d %d %d: find %x %p %s 0x%qx\n", 
					space.s.busNum, space.s.deviceNum, space.s.functionNum, ret, result, 
					result ? result->getName() : "", 
					result ? result->getRegistryEntryID() : 0);
			
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
				configOp(victim, kConfigOpTestPause, NULL);
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
				if (str) type = strtoq(str->getCStringNoCopy(), NULL, 0);
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
		}
		if (victimS) victimS->release();
    }
	else ret = super::setProperties(properties);

	return (ret);
}

IOWorkLoop * getConfiguratorWorkLoop(void)
{
    return (gIOPCIConfigWorkLoop);
}

IOReturn IOPCIBridge::configOp(IOService * device, uintptr_t op, void * result, void * arg)
{
	static OSSet * gIOPCIWaitingPauseSet;
	static OSSet * gIOPCIPausedSet;
	static OSSet * gIOPCIProbeSet;

    IOReturn       ret = kIOReturnSuccess;
    OSSet *        changed;
	IOPCIDevice *  next;
	uint32_t       state;

    if (!gIOPCIConfigWorkLoop->inGate())
        return (gIOPCIConfigWorkLoop->runAction((IOWorkLoop::Action) &IOPCIBridge::configOp,
                                                device, (void *) op, result, arg)); 
    if (!gIOPCIConfigurator)
    {
        uint32_t debug;

		if (getPMRootDomain()->getProperty(kIOPMDeepIdleSupportedKey))
		{
			if (PE_parse_boot_argn("acpi", &debug, sizeof(debug)) && (0x10000 & debug)) {}
			else
			gIOPCIFlags |= kIOPCIConfiguratorDeepIdle;
		}
#if VERSION_MAJOR >= 13
		gIOPCIFlags |= kIOPCIConfiguratorUsePause;
#endif
        if (PE_parse_boot_argn("pci", &debug, sizeof(debug)))
            gIOPCIFlags |= debug;
        if (PE_parse_boot_argn("npci", &debug, sizeof(debug)))
            gIOPCIFlags &= ~debug;

        gIOPCIACPIPlane             = IORegistryEntry::getPlane("IOACPIPlane");
        gIOPCITunnelIDKey           = OSSymbol::withCStringNoCopy(kIOPCITunnelIDKey);
        gIOPCITunnelledKey          = OSSymbol::withCStringNoCopy(kIOPCITunnelledKey);
        gIOPCIThunderboltKey        = OSSymbol::withCStringNoCopy("PCI-Thunderbolt");
        gIOPolledInterfaceActiveKey = OSSymbol::withCStringNoCopy(kIOPolledInterfaceActiveKey);

		gIOPCIWaitingPauseSet = OSSet::withCapacity(4);
		gIOPCIPausedSet       = OSSet::withCapacity(4);
		gIOPCIProbeSet        = OSSet::withCapacity(4);

        gIOPCIConfigurator = OSTypeAlloc(IOPCIConfigurator);
        if (!gIOPCIConfigurator || !gIOPCIConfigurator->init(gIOPCIConfigWorkLoop, gIOPCIFlags))
            panic("!IOPCIConfigurator");

#if defined(__i386__) || defined(__x86_64__)
        if (!gIOPCIMessagedInterruptController)
        {
            enum {
                // LAPIC_DEFAULT_INTERRUPT_BASE (mp.h)
                kBaseMessagedInterruptVectors = 0x70,
                kNumMessagedInterruptVectors = 0xFF - kBaseMessagedInterruptVectors
            };
            bool ok = true;
            IOPCIMessagedInterruptController *
            ic = new IOPCIMessagedInterruptController;
            if (ic && !ic->init(kNumMessagedInterruptVectors, kBaseMessagedInterruptVectors))
            {
                ic->release();
                ic = 0;
            }
			if (ic)
			{
				ok  = ic->reserveVectors(0x7F - kBaseMessagedInterruptVectors, 4);
				ok &= ic->reserveVectors(0xD0 - kBaseMessagedInterruptVectors, 16);
			}
			if (!ic || !ok) panic("IOPCIMessagedInterruptController");
            gIOPCIMessagedInterruptController = ic;
        }
#endif

#if ACPI_SUPPORT
		IOACPIPlatformDevice * acpiDevice;
		if (!(acpiDevice = (typeof(acpiDevice)) device->getProvider()->metaCast("IOACPIPlatformDevice")))
            panic("host!IOACPIPlatformDevice");
		AppleVTD::install(gIOPCIConfigWorkLoop, gIOPCIFlags, acpiDevice, acpiDevice->getACPITableData("DMAR", 0));
#endif
    }

	if (kConfigOpScan != op)
	{
		ret = gIOPCIConfigurator->configOp(device, op, result, arg);
		if (kIOReturnSuccess != ret) return (ret);

		next = (IOPCIDevice *) device;
		if (kConfigOpTerminated == op)
		{
			gIOPCIWaitingPauseSet->removeObject(next);
			gIOPCIPausedSet->removeObject(next);
			gIOPCIProbeSet->removeObject(next);
		}
		else if (kConfigOpTestPause == op)
		{
			if (gIOPCIWaitingPauseSet->setObject(next))
			{
				next->changePowerStateToPriv(kIOPCIDevicePausedState);
				next->powerOverrideOnPriv();
			}
		}

		if (op != kConfigOpPaused) op = 0;
		else
		{
			op = 0;

			DLOG("configOp:->pause: %s(0x%qx)\n", device->getName(), device->getRegistryEntryID());
			if (gIOPCIWaitingPauseSet->containsObject(device))
			{
				gIOPCIPausedSet->setObject(device);
				gIOPCIWaitingPauseSet->removeObject(device);
				if (!gIOPCIWaitingPauseSet->getCount()) op = kConfigOpRealloc;
			}
		}
	}

	while (op)
	{
		ret = gIOPCIConfigurator->configOp(device, op, &changed);
		op = 0;
		if (kIOReturnSuccess != ret) break;
		if (!changed) break;

		while ((next = (IOPCIDevice *) changed->getAnyObject()))
		{
			ret = gIOPCIConfigurator->configOp(next, kConfigOpGetState, &state);
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
					if (gIOPCIWaitingPauseSet->setObject(next))
					{
						next->changePowerStateToPriv(kIOPCIDevicePausedState);
						next->powerOverrideOnPriv();
					}
				}
				else
				{
					DLOG("configOp:->probe: %s(0x%qx), 0x%x\n", next->getName(), next->getRegistryEntryID(), state);
					gIOPCIProbeSet->setObject(next);
				}
			}
			changed->removeObject(next);
		}
		changed->release();
	}

	if (!gIOPCIWaitingPauseSet->getCount())
	{
		while ((next = (IOPCIDevice *) gIOPCIPausedSet->getAnyObject()))
		{
			DLOG("configOp:<-unpause: %s(0x%qx)\n", next->getName(), next->getRegistryEntryID());
			if (2 != next->reserved->pauseFlags)
			{
				next->changePowerStateToPriv(kIOPCIDeviceOnState);
				next->powerOverrideOffPriv();
			}
			next->reserved->pauseFlags = 0;
			gIOPCIPausedSet->removeObject(next);
		}
		while ((next = (IOPCIDevice *) gIOPCIProbeSet->getAnyObject()))
     	{
			DLOG("configOp:<-probe: %s(0x%qx), pm %d\n", next->getName(), next->getRegistryEntryID(), next->reserved->pciPMState);
			if (kIOPCIDeviceOnState == next->reserved->pciPMState) deferredProbe(next);
			else                                     next->reserved->needsProbe = true;
			gIOPCIProbeSet->removeObject(next);
		}
	}

    return (ret);
}

void IOPCIBridge::deferredProbe(IOPCIDevice * device)
{
	IOService *   client;
	IOPCIBridge * bridge;

	client = device->copyClientWithCategory(gIODefaultMatchCategoryKey);
	if ((bridge = OSDynamicCast(IOPCIBridge, client)))
	{
		DLOG("configOp:<-probe: %s(0x%qx)\n", device->getName(), device->getRegistryEntryID());
		bridge->probeBus(device, bridge->firstBusNum());
	}
	if (client) client->release();

	device->reserved->needsProbe = false;
}

//*********************************************************************************

static const IOPMPowerState gIOPCIPowerStates[kIOPCIDevicePowerStateCount] = {
    // version, capabilityFlags, outputPowerCharacter, inputPowerRequirement, staticPower, stateOrder
	{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 2, 0, kIOPMSoftSleep, kIOPMSoftSleep, 0, 1, 0, 0, 0, 0, 0, 0 },
	{ 2, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 3, 0, 0, 0, 0, 0, 0 },
	{ 2, kIOPMConfigRetained, kIOPMConfigRetained, kIOPMConfigRetained, 0, 2, 0, 0, 0, 0, 0, 0 }
};

static const IOPMPowerState gIOPCIHostPowerStates[kIOPCIDevicePowerStateCount] = {
    // version, capabilityFlags, outputPowerCharacter, inputPowerRequirement, staticPower, stateOrder
	{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 2, 0, kIOPMSoftSleep, kIOPMSoftSleep, 0, 1, 0, 0, 0, 0, 0, 0 },
	{ 2, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 3, 0, 0, 0, 0, 0, 0 },
	{ 2, kIOPMConfigRetained, kIOPMConfigRetained, kIOPMPowerOn, 0, 2, 0, 0, 0, 0, 0, 0 }
};


// version without kIOPCIDevicePausedState
static const IOPMPowerState gIOPCIPowerStatesV1[kIOPCIDevicePowerStateCount - 1] = {
    // version, capabilityFlags, outputPowerCharacter, inputPowerRequirement,
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 0, kIOPMSoftSleep, kIOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
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
	if (domainState & kIOPMPowerOn)        return (kIOPCIDeviceOnState);
	if (domainState & kIOPMSoftSleep)      return (kIOPCIDeviceDozeState);
	if (domainState & kIOPMConfigRetained) return (kIOPCIDevicePausedState);
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

    reserved = IONew(ExpansionData, 1);
    if (reserved == 0) return (false);
    bzero(reserved, sizeof(ExpansionData));

    if (!configure(provider)) return (false);
	pciDevice = OSDynamicCast(IOPCIDevice, provider);

    // initialize superclass variables
    PMinit();
    // clamp power on
//    temporaryPowerClampOn();
    // register as controlling driver
    IOPCIRegisterPowerDriver(this, !pciDevice);

    // join the tree
    provider->joinPMtree(this);

	pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDevice)
    {
        IOReturn
        ret = configOp(this, kConfigOpAddHostBridge, 0); 
        if (kIOReturnSuccess != ret)
            return (false);
    }

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
    
    registerService();

    return (true);
}

void IOPCIBridge::stop( IOService * provider )
{
    PMstop();
    super::stop( provider);
}

void IOPCIBridge::free( void )
{
    if (reserved) IODelete(reserved, ExpansionData, 1);

    super::free();
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
		aspmBits = (device->reserved->aspmCaps & device->reserved->expressASPMDefault);
		l1pmBits = (device->reserved->l1pmCaps);
	}
	else
	{
        aspmBits = l1pmBits = 0;
	}

	setDeviceL1PMBits(device, l1pmBits);
    setDeviceASPMBits(device, aspmBits);

    return (kIOReturnSuccess);
}

IOReturn IOPCI2PCIBridge::setDeviceASPMState(IOPCIDevice * device,
                                            IOService * client, IOOptionBits state)
{
    uint32_t aspmBits, l1pmBits;

    // Need to enable upstream first then downstream, reverse for disable
    if (state)
    {
		l1pmBits = (fBridgeDevice->reserved->l1pmCaps & device->reserved->l1pmCaps);
        setDeviceL1PMBits(fBridgeDevice, l1pmBits);
        setDeviceL1PMBits(device,       l1pmBits);

		// L1 and L0s need to be supported on both ends to enable
		aspmBits = (fBridgeDevice->reserved->expressASPMDefault 
				   & device->reserved->expressASPMDefault
				   & fBridgeDevice->reserved->aspmCaps
				   & device->reserved->aspmCaps
				   & (kIOPCIExpressASPML0s | kIOPCIExpressASPML1));

        setDeviceASPMBits(fBridgeDevice, aspmBits);
        setDeviceASPMBits(device,       aspmBits);
    }
    else 
    {
        aspmBits = l1pmBits = 0;
        setDeviceL1PMBits(device,       l1pmBits);
        setDeviceL1PMBits(fBridgeDevice, l1pmBits);
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
	if (status != kIOReturnSuccess)
		return status;
	return (device->enableLTR(device, enable));
}

IOReturn IOPCIBridge::setDevicePowerState(IOPCIDevice * device, IOOptionBits options,
                                          unsigned long prevState, unsigned long newState)
{
    bool noSave;

	noSave = ((kIOPCIConfigShadowVolatile & options) 
		    && (kOSBooleanFalse == device->getProperty(kIOPMPCIConfigSpaceVolatileKey)));

    DLOG("%s[%p]::setDevicePowerState(%ld, %ld, %d)\n", device->getName(), device, prevState, newState, noSave);
    
	if (newState == prevState) return (kIOReturnSuccess);

    switch (newState)
    {
        case kIOPCIDeviceOffState:
			if (noSave) break;
			saveDeviceState(device, options);
		    if (kOSBooleanTrue == device->getProperty(kIOPolledInterfaceActiveKey))
		    {
		    	newState = kIOPCIDeviceOnState;
			}
			else if (kIOPCIConfiguratorDeepIdle & gIOPCIFlags)
			{
				newState = kIOPCIDeviceDozeState;
			}
			device->setPCIPowerState(newState, 0);
            break;
            
        case kIOPCIDeviceDozeState:
			if (noSave) break;
			saveDeviceState(device, options);
			device->setPCIPowerState(newState, 0);
            break;

        case kIOPCIDeviceOnState:
			configOp(device, kConfigOpUnpaused, 0);
			if (noSave) break;
			if (kIOPCIDevicePausedState != prevState)
			{
				if ((kIOPCIDeviceOffState == prevState) 
				 && ((kIOPCIClassGraphics == (device->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24))
				  || (kIOPCIClassMultimedia == (device->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24))))
				{
					tunnelsWait(device);
				}
			}
			device->setPCIPowerState(newState, 0);
			if (kIOPCIDevicePausedState == prevState) break;
            restoreDeviceState(device, options);
            break;

        case kIOPCIDevicePausedState:
			IOLog("pci pause: %s\n", device->getName());
			configOp(device, kConfigOpPaused, 0);
			if (noSave) break;
			device->setPCIPowerState(newState, 0);
            restoreDeviceState(device, options);
            break;
    }
    
    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::setDevicePowerState( IOPCIDevice * device,
        unsigned long whatToDo )
{
    // Special for pci/pci-bridge devices - 
    // kSaveBridgeState(2) to save immediately, kRestoreBridgeState(3) to restore immediately

    if (kRestoreBridgeState == whatToDo)
    {
		if (kSaveBridgeState == gIOAllPCI2PCIBridgeState)
		{
			restoreMachineState(kMachineRestoreBridges, 0);
			restoreMachineState(kMachineRestoreEarlyDevices, 0);
		}
        gIOAllPCI2PCIBridgeState = kRestoreBridgeState;
		gIOPCITunnelWait = gIOPCITunnelSleep;
    }
    else if (kSaveBridgeState == whatToDo)
        gIOAllPCI2PCIBridgeState = whatToDo;
	else panic("setDevicePowerState");

    return (kIOReturnSuccess);
}

static void IOPCILogDevice(const char * log, IOPCIDevice * device, bool dump)
{
	int      slen, len, pos;
	char *   string;
	uint32_t offset, data = 0;
	
	slen = 2048;
	pos  = 0;
	string = IONew(char, slen);
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
	DLOG(string);
	IODelete(string, char, slen);
}

IOReturn IOPCIBridge::saveDeviceState( IOPCIDevice * device,
                                       IOOptionBits options )
{
    IOPCIConfigShadow * shadow;
	IOService         * root;
	IOReturn ret;
    UInt32   flags;
	uint32_t data;
    int      i;
	bool     ok;
	uint64_t time;

    if (!device->savedConfig) return (kIOReturnNotReady);

	shadow = configShadow(device);
    flags = shadow->flags;

    if (kIOPCIConfigShadowValid & flags) return (kIOReturnSuccess);

	DLOG("%s::saveDeviceState(0x%x)\n", device->getName(), options);

    flags |= kIOPCIConfigShadowValid | options;
    shadow->flags = flags;

	if (device->getProperty(gIOPCITunnelledKey))
	{
		shadow->tunnelID = device->copyProperty(gIOPCITunnelIDKey, gIOServicePlane);
	}
	for (root = device;
		 (!root->getProperty(gIOPCIThunderboltKey)) && (root = root->getProvider());) 
	{}
	shadow->tunnelRoot = OSDynamicCast(IOPCIDevice, root);

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
            if (kIOPCISaveRegsMask & (1 << i)) device->savedConfig[i] = device->configRead32(i * 4);
        }
    }

	if (device->reserved->l1pmCapability)
	{
        device->savedConfig[kIOPCIConfigShadowL1PM + 0]
            = device->configRead32(device->reserved->l1pmCapability + 0x08);
        device->savedConfig[kIOPCIConfigShadowL1PM + 1]
            = device->configRead32(device->reserved->l1pmCapability + 0x0C);
    }

	if (device->reserved->latencyToleranceCapability)
	{
        device->savedConfig[kIOPCIConfigShadowLTR + 0]
            = device->configRead32(device->reserved->latencyToleranceCapability + 0x04);
    }

    if (device->reserved->expressCapability)
    {
        device->savedConfig[kIOPCIConfigShadowXPress + 0]   // device control
            = device->configRead16( device->reserved->expressCapability + 0x08 );
        device->savedConfig[kIOPCIConfigShadowXPress + 1]   // link control
            = device->configRead16( device->reserved->expressCapability + 0x10 );
		if ((kIOPCIConfigShadowBridgeInterrupts & shadow->flags)
		 || (0x100 & device->reserved->expressCapabilities))
        {                                                   // slot control
            device->savedConfig[kIOPCIConfigShadowXPress + 2]
                = device->configRead16( device->reserved->expressCapability + 0x18 );
        }
		if (expressV2(device))
		{
			device->savedConfig[kIOPCIConfigShadowXPress + 3]   // device control2
				= device->configRead16( device->reserved->expressCapability + 0x28 );
			device->savedConfig[kIOPCIConfigShadowXPress + 4]   // link control2
				= device->configRead16( device->reserved->expressCapability + 0x30 );
			device->savedConfig[kIOPCIConfigShadowXPress + 5]   // slot control2
				= device->configRead16( device->reserved->expressCapability + 0x38 );
		}
		if (kIOPCIConfigShadowSleepLinkDisable & shadow->flags)
		{
            device->configWrite16(device->reserved->expressCapability + 0x10,
            						(1 << 4) | device->savedConfig[kIOPCIConfigShadowXPress + 1]);
		}
		if (kIOPCIConfigShadowSleepReset & shadow->flags)
		{
			UInt16 bridgeControl;
			bridgeControl = device->configRead16(kPCI2PCIBridgeControl);
			device->configWrite16(kPCI2PCIBridgeControl, bridgeControl | 0x40);
			IOSleep(10);
			device->configWrite16(kPCI2PCIBridgeControl, bridgeControl);
		}
    }

	if (device->reserved->msiBlockSize > kIOPCIConfigShadowMSICount) panic("msiBlockSize");
    for (i = 0; i < device->reserved->msiBlockSize; i++)
        device->savedConfig[kIOPCIConfigShadowMSI + i] 
            = device->configRead32( device->reserved->msiCapability + i * 4 );

    if (shadow->handler)
    {
		time = mach_absolute_time();
        (*shadow->handler)(shadow->handlerRef, kIOMessageDeviceHasPoweredOff, device, 3);
		time = mach_absolute_time() - time;
		absolutetime_to_nanoseconds(time, &time);
		DLOG("%s::configHandler(kIOMessageDeviceHasPoweredOff) %lld ms\n", device->getName(), time / 1000000ULL);
    }

	if (kIOPCIConfigShadowHotplug & shadow->flags)
	{
		data = device->configRead32(kIOPCIConfigVendorID);
#ifdef DEADTEST
		if (!strcmp(DEADTEST, device->getName())) data = 0xFFFFFFFF;
#endif
		ok = (data && (data != 0xFFFFFFFF));
		if (!ok)
		{
			DLOG("saveDeviceState kill device %s\n", device->getName());
			ret = configOp(device, kConfigOpKill, 0);
			shadow->flags &= ~kIOPCIConfigShadowValid;
		}
	}

	if (kIOPCIConfigShadowValid & shadow->flags)
	{
		configOp(device, kConfigOpShadowed, &shadow->savedConfig[0]);
		restoreQEnter(device);
	}

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::_restoreDeviceState(IOPCIDevice * device, IOOptionBits options)
{
    AbsoluteTime deadline, start, now = 0;
    uint32_t     retries;
    uint32_t     data;
    bool         ok;
    UInt32       flags;
    int          i;
    uint64_t     time;
    IOReturn     ret;

    flags = configShadow(device)->flags;

    if (!(kIOPCIConfigShadowValid & flags)) return (kIOReturnNoResources);

    if (configShadow(device)->handler)
    {
		time = mach_absolute_time();
        ret = (*configShadow(device)->handler)(configShadow(device)->handlerRef, 
                                               kIOMessageDeviceWillPowerOn, device, 3);
		if ((kIOPCIRestoreDeviceStateEarly & options) && (kIOReturnNotReady == ret)) return (ret);
		time = mach_absolute_time() - time;
		absolutetime_to_nanoseconds(time, &time);
		DLOG("%s::configHandler(kIOMessageDeviceWillPowerOn) %lld ms\n", device->getName(), time / 1000000ULL);
    }

    flags &= ~kIOPCIConfigShadowValid;
    configShadow(device)->flags = flags;

	if (!device->reserved->dead)
	{
		if (kIOReturnSuccess != device->parent->checkLink(kCheckLinkParents))
        {
            DLOG("%s: pci restore no link\n", device->getName());
			device->reserved->dead = true;
        }
	}

    if ((!device->reserved->dead) && !(kIOPCIConfigShadowBridgeDriver & flags))
    {
		retries = 0;
		clock_get_uptime(&start);
        clock_interval_to_deadline(200, kMillisecondScale, &deadline);
        do
        {
            if (retries) IOSleep(2);
            data = device->configRead32(kIOPCIConfigVendorID);
            ok = (data && (data != 0xFFFFFFFF));
            if (ok) break;
            retries++;
            clock_get_uptime(&now);
        }
        while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));

        if (retries)
        {
			absolutetime_to_nanoseconds(now - start, &now);
            DLOG("%s: pci restore waited for %qd ms %s\n", 
                    device->getName(), now / 1000000ULL, ok ? "ok" : "fail");
        }
        if (data != device->savedConfig[kIOPCIConfigVendorID >> 2])
        {
            DLOG("%s: pci restore invalid deviceid\n", device->getName());
			device->reserved->dead = true;
        }
    }

	if (!device->reserved->dead)
	{
		if (kIOPCIConfiguratorLogSaveRestore & gIOPCIFlags)
			IOPCILogDevice("before restore", device, true);
		else if ((kIOPCIConfiguratorIOLog | kIOPCIConfiguratorKPrintf) & gIOPCIFlags)
			IOPCILogDevice("restore device", device, false);

		if (kIOPCIConfigShadowHostBridge & flags) {}
		else
		{
			uint32_t regCount = kIOPCIConfigEPShadowRegs;
			if (configShadow(device)->bridge)
			{
				regCount = kIOPCIConfigBridgeShadowRegs;
				configShadow(device)->bridge->restoreBridgeState();
			}
			for (i = (kIOPCIConfigRevisionID >> 2); i < regCount; i++)
			{
				if (kIOPCISaveRegsMask & (1 << i))
				    device->configWrite32( i * 4, device->savedConfig[ i ]);
			}
			device->configWrite32(kIOPCIConfigCommand, device->savedConfig[1]);
		}

		if (device->reserved->l1pmCapability)
		{
			device->configWrite32(device->reserved->l1pmCapability + 0x0C, 
								  device->savedConfig[kIOPCIConfigShadowL1PM + 1]);
			device->configWrite32(device->reserved->l1pmCapability + 0x08,
								  device->savedConfig[kIOPCIConfigShadowL1PM + 0]);
		}

		if (device->reserved->latencyToleranceCapability)
		{
			device->configWrite32(device->reserved->latencyToleranceCapability + 0x04, 
								  device->savedConfig[kIOPCIConfigShadowLTR + 0]);
		}

		if (device->reserved->expressCapability)
		{
			device->configWrite16( device->reserved->expressCapability + 0x08,   // device control
									device->savedConfig[kIOPCIConfigShadowXPress + 0]);
			device->configWrite16( device->reserved->expressCapability + 0x10,    // link control
									device->savedConfig[kIOPCIConfigShadowXPress + 1] );
			if ((kIOPCIConfigShadowBridgeInterrupts & configShadow(device)->flags)
			 || (0x100 & device->reserved->expressCapabilities))
			{                                                                 // slot control
				device->configWrite16( device->reserved->expressCapability + 0x18, 
										device->savedConfig[kIOPCIConfigShadowXPress + 2] );
			}
			if (expressV2(device))
			{
				device->configWrite16(device->reserved->expressCapability + 0x28,   // device control2
									  device->savedConfig[kIOPCIConfigShadowXPress + 3]);
				device->configWrite16(device->reserved->expressCapability + 0x30,    // link control2
									  device->savedConfig[kIOPCIConfigShadowXPress + 4]);
				device->configWrite16(device->reserved->expressCapability + 0x38,    // slot control2
									  device->savedConfig[kIOPCIConfigShadowXPress + 5]);
			}
		}

		for (i = 0; i < device->reserved->msiBlockSize; i++)
			device->configWrite32( device->reserved->msiCapability + i * 4,  
									device->savedConfig[kIOPCIConfigShadowMSI + i]);

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

	configOp(device, kConfigOpShadowed, NULL);

    return (kIOReturnSuccess);
}

void IOPCIBridge::restoreQEnter(IOPCIDevice * device)
{
	queue_head_t *      que = NULL;
    IOPCIConfigShadow * shadow;

    shadow = configShadow(device);
	if (shadow->tunnelRoot) 
	{
        DLOG("queued %s on %s\n", device->getName(), shadow->tunnelRoot->getName());
        que = &configShadow(shadow->tunnelRoot)->dependents;
		if (device == shadow->tunnelRoot)
		{
			IOLockLock(gIOPCIWakeReasonLock);
			gIOPCITunnelSleep++;
			IOLockUnlock(gIOPCIWakeReasonLock);
		}
	}
    else
    {
        que = &gIOAllPCIDeviceRestoreQ;
	}

	IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);
	queue_enter_first(que,
					  shadow,
					  IOPCIConfigShadow *,
					  link );
	IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);
}

void IOPCIBridge::restoreQRemove(IOPCIDevice * device)
{
	queue_head_t *      que = NULL;
    IOPCIConfigShadow * shadow;

    shadow = configShadow(device);

	if (!configShadow(device)->link.next) return;

	if (shadow->tunnelRoot)
	{
        que = &configShadow(shadow->tunnelRoot)->dependents;
	}
    else
    {
        que = &gIOAllPCIDeviceRestoreQ;
	}

	IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);
	queue_remove(que,
				 shadow,
				 IOPCIConfigShadow *,
				 link );
	IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);
}

void IOPCIBridge::tunnelsWait(IOPCIDevice * device)
{
	IOLockLock(gIOPCIWakeReasonLock);
	DLOG("%s: tunnel stall(%d, %d)\n", device->getName(), gIOPCITunnelWait, gIOPCITunnelSleep);
	if (gIOPCITunnelWait)
	{
		IOLockSleep(gIOPCIWakeReasonLock, &gIOPCITunnelWait, THREAD_UNINT);
		DLOG("%s: tunnels done\n", device->getName());
	}
	IOLockUnlock(gIOPCIWakeReasonLock);
}

IOReturn IOPCIBridge::restoreTunnelState(IOPCIDevice * rootDevice, IOOptionBits options)
{
	IOReturn            ret;
    IOPCIConfigShadow * root;
    IOPCIConfigShadow * shadow;
    IOPCIConfigShadow * next;

    DLOG("restoreTunnelState(%s, %d)\n", rootDevice->getName(), options);
	root = configShadow(rootDevice);
    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

	next = (IOPCIConfigShadow *) queue_first(&root->dependents);
	while (!queue_end(&root->dependents, (queue_entry_t) next))
	{
		shadow = next;
		next   = (IOPCIConfigShadow *) queue_next(&shadow->link);

		if (kMachineRestoreBridges & options) 
		{
			if (!(kIOPCIConfigShadowBridge & shadow->flags))    continue;
		}

		if (!(kMachineRestoreTunnels & options))
		{
			if (shadow->tunnelID) continue;
		}

		IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

		if (kIOPCIConfigShadowVolatile & shadow->flags)
		{
			shadow->device->setPCIPowerState(kIOPCIDeviceOnState, options);

			ret = _restoreDeviceState(shadow->device, kIOPCIRestoreDeviceStateEarly);
			if (kIOReturnNotReady == ret)
			{
				ret = _restoreDeviceState(shadow->device, 0);
			}
		}
		if (shadow->tunnelID)
		{
			shadow->tunnelID->release();
			shadow->tunnelID = 0;
		}

		IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

		next   = (IOPCIConfigShadow *) queue_next(&shadow->link);
		queue_remove(&root->dependents,
					 shadow,
					 IOPCIConfigShadow *,
					 link);
		shadow->link.next = shadow->link.prev = NULL;
	}


    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::restoreMachineState(IOOptionBits options, IOPCIDevice * device)
{
	IOReturn            ret;
    IOPCIConfigShadow * shadow;
    IOPCIConfigShadow * next;

    DLOG("restoreMachineState(%d)\n", options);

    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

	next = (IOPCIConfigShadow *) queue_first(&gIOAllPCIDeviceRestoreQ);
	while (!queue_end(&gIOAllPCIDeviceRestoreQ, (queue_entry_t) next))
	{
		shadow = next;
		next   = (IOPCIConfigShadow *) queue_next(&shadow->link);

		if (shadow->tunnelRoot || shadow->tunnelID) panic("tunnel");

		if (shadow->device != device)
		{
			if (kMachineRestoreBridges & options) 
			{
				if (!(kIOPCIConfigShadowBridge & shadow->flags))    continue;
			}

			if (!(kIOPCIConfigShadowVolatile & shadow->flags))      continue;
#if ACPI_SUPPORT
			if (!(kMachineRestoreDehibernate & options)
				// skip any slow PS methods
				&& (shadow->device->reserved->psMethods[0] >= 0)
				// except for nvidia bus zero devices
				&& (shadow->device->space.s.busNum 
					|| (0x10de != (shadow->savedConfig[kIOPCIConfigVendorID >> 2] & 0xffff))))
																	continue;
#endif
			if (kMachineRestoreEarlyDevices & options)
			{
				if (shadow->device->space.s.busNum)                 continue;
				if (shadow->handler)                                continue;
				if (shadow->device->reserved->pmSleepEnabled)       continue;
			}
		}

		IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

		shadow->device->setPCIPowerState(kIOPCIDeviceOnState, options);

		ret = _restoreDeviceState(shadow->device, kIOPCIRestoreDeviceStateEarly);

        IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

		next   = (IOPCIConfigShadow *) queue_next(&shadow->link);
		queue_remove(&gIOAllPCIDeviceRestoreQ,
					 shadow,
					 IOPCIConfigShadow *,
					 link);
		shadow->link.next = shadow->link.prev = NULL;
	}

    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::restoreDeviceState( IOPCIDevice * device, IOOptionBits options )
{
    IOReturn ret = kIOReturnNotFound;

    if (!device->savedConfig)
        return (kIOReturnNotReady);

    if (kSaveBridgeState == gIOAllPCI2PCIBridgeState)
    {
        ret = restoreMachineState(kMachineRestoreDehibernate | kMachineRestoreBridges, device);
    }

    if (kIOReturnSuccess != ret)
    {
		if (!queue_empty(&configShadow(device)->dependents))
		{
			ret = restoreTunnelState(device, kMachineRestoreBridges);
			ret = restoreTunnelState(device, 0);
			ret = restoreTunnelState(device, kMachineRestoreTunnels);
			if (!queue_empty(&configShadow(device)->dependents)) panic("tunnelq");

			IOLockLock(gIOPCIWakeReasonLock);
			if (!--gIOPCITunnelSleep && gIOPCITunnelWait)
			{
				gIOPCITunnelWait = false;
				IOLockWakeup(gIOPCIWakeReasonLock, &gIOPCITunnelWait, false);
			}
			IOLockUnlock(gIOPCIWakeReasonLock);
		}
		else
		{
			restoreQRemove(device);
			ret = _restoreDeviceState(device, 0);
		}
    }
    // callers expect success
    return (kIOReturnSuccess);
}

IOReturn 
IOPCIBridge::callPlatformFunction(const OSSymbol * functionName,
                                          bool waitForFunction,
                                          void * p1, void * p2,
                                          void * p3, void * p4)
{
    IOReturn result;

    result = super::callPlatformFunction(functionName, waitForFunction,
                                         p1, p2, p3, p4);

#if 0
    if ((kIOReturnUnsupported == result) 
     && (gIOPlatformDeviceASPMEnableKey == functionName)
     && getProperty(kIOPCIDeviceASPMSupportedKey))
    {
        result = parent->setDeviceASPMState(this, (IOService *) p1, (IOOptionBits)(uintptr_t) p2);
    }
#endif
    if ((kIOReturnUnsupported == result) 
     && (gIOPlatformGetMessagedInterruptControllerKey == functionName))
    {
        *(IOPCIMessagedInterruptController **)p2 =
                gIOPCIMessagedInterruptController;
    }

    return (result);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIBridge::configure( IOService * provider )
{
    return (true);
}

#if !defined(__LP64__) || defined(__x86_64__)
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

void IOPCIBridge::nvLocation( IORegistryEntry * entry,
                              UInt8 * busNum, UInt8 * deviceNum, UInt8 * functionNum )
{
    IOPCIDevice *       nub;

    nub = OSDynamicCast( IOPCIDevice, entry );
    assert( nub );

    *busNum             = nub->space.s.busNum;
    *deviceNum          = nub->space.s.deviceNum;
    *functionNum        = nub->space.s.functionNum;
}

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

#if VERSION_MAJOR < 13
static char *
strnstr(char *s, const char *find, size_t slen)
{
  char c, sc;
  size_t len;
  
  if ((c = *find++) != '\0') {
    len = strlen(find);
    do {
      do {
        if ((sc = *s++) == '\0' || slen-- < 1)
          return (NULL);
      } while (sc != c);
      if (len > slen)
        return (NULL);
    } while (strncmp(s, find, len) != 0);
    s--;
  }
  return (s);
}
#endif

#ifndef kIOPMRootDomainWakeTypeNetwork
#define kIOPMRootDomainWakeTypeNetwork      "Network"
#endif

void IOPCIBridge::updateWakeReason(IOPCIDevice * device)
{
	OSObject *   obj;
	OSString *   reasonProp;
	const char * reason;
	const char * propCStr;
	unsigned int len;
	char         wakeBuffer[128];

	reason = device->getName();
	IOLockLock(gIOPCIWakeReasonLock);
	do
	{
		obj = getPMRootDomain()->copyProperty(kIOPMRootDomainWakeReasonKey);
		reasonProp = OSDynamicCast(OSString, obj);
		if (reasonProp && (len = reasonProp->getLength()))
		{
			propCStr = reasonProp->getCStringNoCopy();
			if (strnstr((char *) propCStr, reason, len + 1)) break;
			snprintf(wakeBuffer, sizeof(wakeBuffer), "%s %s", propCStr, reason);
			reason = wakeBuffer;
		}
	    getPMRootDomain()->setProperty(kIOPMRootDomainWakeReasonKey, reason);
	    if (obj) obj->release();
	}
	while (false);

	if ((kIOPCIClassNetwork == (device->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24))
      && (!getPMRootDomain()->getProperty(kIOPMRootDomainWakeTypeKey)))
	{
        getPMRootDomain()->setProperty(kIOPMRootDomainWakeTypeKey, kIOPMRootDomainWakeTypeNetwork);
	}
	IOLockUnlock(gIOPCIWakeReasonLock);
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

    if (device->reserved->msiCapability && gIOPCIMessagedInterruptController)
        ret = gIOPCIMessagedInterruptController->deallocateDeviceInterrupts(device);

    getPlatform()->callPlatformFunction(gIOPlatformFreeDeviceResourcesKey,
									  /* waitForFunction */ false,
									  /* nub             */ device, NULL, NULL, NULL);
	restoreQRemove(device);
    configOp(device, kConfigOpTerminated, 0);
}

bool IOPCIBridge::publishNub( IOPCIDevice * nub, UInt32 /* index */ )
{
    char                        location[ 24 ];
    bool                        ok;
#if ROM_KEXTS
    OSData *                    data;
    OSData *                    driverData;
    UInt32                      *regData, expRomReg;
    IOMemoryMap *               memoryMap;
    IOVirtualAddress            virtAddr;
#endif

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

        IOPCIConfigShadow * shadow = IONew(IOPCIConfigShadow, 1);
        if (shadow)
        {
            bzero(shadow, sizeof(IOPCIConfigShadow));
            queue_init(&shadow->dependents);
            shadow->device = nub;
            nub->savedConfig = &shadow->savedConfig[0];
            for (int i = 0; i < kIOPCIConfigEPShadowRegs; i++)
            {
                if (kIOPCISaveRegsMask & (1 << i))
                	nub->savedConfig[i] = nub->configRead32( i << 2 );
			}
        }

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

#if ROM_KEXTS
        // look for a "driver-reg,AAPL,MacOSX,PowerPC" property.

        if ((data = (OSData *)nub->getProperty("driver-reg,AAPL,MacOSX,PowerPC")))
        {
            if (data->getLength() == (2 * sizeof(UInt32)))
            {
                regData = (UInt32 *)data->getBytesNoCopy();

                getNubResources(nub);
                memoryMap = nub->mapDeviceMemoryWithRegister(kIOPCIConfigExpansionROMBase);
                if (memoryMap != 0)
                {
                    virtAddr = memoryMap->getVirtualAddress();
                    virtAddr += regData[0];

                    nub->setMemoryEnable(true);

                    expRomReg = nub->configRead32(kIOPCIConfigExpansionROMBase);
                    nub->configWrite32(kIOPCIConfigExpansionROMBase, expRomReg | 1);

                    driverData = OSData::withBytesNoCopy((void *)virtAddr, regData[1]);
                    if (driverData != 0)
                    {
                        gIOCatalogue->addExtensionsFromArchive(driverData);

                        driverData->release();
                    }

                    nub->configWrite32(kIOPCIConfigExpansionROMBase, expRomReg);

                    nub->setMemoryEnable(false);

                    memoryMap->release();
                }
            }
        }
#endif
        ok = nub->attach( this );

        if (ok)
        {
            nub->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
                    (void *) kIOMessageDeviceWillPowerOff, nub, (void *) 0, (void *) 0);

            nub->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
                    (void *) kIOMessageDeviceHasPoweredOn, nub, (void *) 0, (void *) 0);

            nub->registerService();
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
		ret = configOp(device, kConfigOpEject, 0);
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

		ret = configOp(device, kConfigOpNeedsScan, 0);
	}

    if (kIOPCIProbeOptionDone & options) ret = configOp(device, kConfigOpScan, NULL);

    return (ret);
}

IOReturn IOPCIBridge::protectDevice(IOPCIDevice * device, uint32_t space, uint32_t prot)
{
    IOReturn ret;

	prot &= (VM_PROT_READ|VM_PROT_WRITE);
	prot <<= kPCIDeviceStateConfigProtectShift;

    DLOG("%s::protectDevice(%x, %x)\n", device->getName(), space, prot);

    ret = configOp(device, kConfigOpProtect, &prot);

	return (ret);
}

void IOPCIBridge::probeBus( IOService * provider, UInt8 busNum )
{
    IORegistryEntry *  found;
    OSDictionary *     propTable;
    IOPCIDevice *      nub = 0;
    OSIterator *       kidsIter;
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

    IODTSetResolving(provider, &compareAddressCell, &nvLocation);

    kidsIter = provider->getChildIterator( gIODTPlane );

    // find and copy over any devices from the device tree
    OSArray * nubs = OSArray::withCapacity(0x10);
    assert(nubs);

    if (kidsIter) {
        kidsIter->reset();
        while ((found = (IORegistryEntry *) kidsIter->getNextObject()))
        {
//            kprintf("probe: %s, %s\n", provider->getName(), found->getName());
            if (!found->getProperty("vendor-id"))
                continue;
            if (found->inPlane(gIOServicePlane))
                continue;

            propTable = found->getPropertyTable();
            nub = OSDynamicCast(IOPCIDevice, found);
            if (nub)
            {
                nub->retain();
                initializeNub(nub, propTable);
            }
            else
            {
                nub = createNub( propTable );
                if (nub &&
                    (!initializeNub(nub, propTable) || !nub->init(found, gIODTPlane)))
                {
                    nub->release();
                    nub = 0;
                }
            }

            if (nub)
            {
                IOByteCount capa;
				OSData *    data;

                nubs->setObject(index++, nub);

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIPowerManagementCapability, &capa))
                    nub->reserved->powerCapability = capa;
                capa = 0;
#if 0
                if (nub->extendedFindPCICapability(kIOPCIMSIXCapability, &capa))
                    nub->reserved->msiCapability = capa;
                    nub->reserved->msiMode   |= kMSIX;
                }
                else 
#endif
                if (nub->extendedFindPCICapability(kIOPCIMSICapability, &capa))
                    nub->reserved->msiCapability = capa;
                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIExpressLatencyTolerenceReportingCapability, &capa))
                    nub->reserved->latencyToleranceCapability = capa;

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIExpressL1PMSubstatesCapability, &capa))
                {
                    nub->reserved->l1pmCapability = capa;
                    nub->reserved->l1pmCaps = (0xFFFFFFF0 | nub->configRead32(capa + 0x04));
                }

                capa = 0;
                if (nub->extendedFindPCICapability(kIOPCIPCIExpressCapability, &capa)) {
                    nub->reserved->expressCapability = capa;
                    nub->reserved->expressCapabilities = nub->configRead16(capa + 0x02);
                    nub->reserved->aspmCaps = (3 & (nub->configRead32(capa + 0xc) >> 10));
#if ACPI_SUPPORT
                    // current aspm mode
                    nub->reserved->expressASPMDefault = (3 & (nub->configRead16(capa + 0x10)));
#else
                    nub->reserved->expressASPMDefault = nub->reserved->aspmCaps;
#endif
		}

				if (nub->reserved->expressCapability && nub->reserved->latencyToleranceCapability
				 && (data = OSDynamicCast(OSData, nub->getProperty(kIOPCIExpressMaxLatencyKey, gIOServicePlane))))
				{
					nub->extendedConfigWrite32(nub->reserved->latencyToleranceCapability + 0x04, 
												*((uint32_t *) data->getBytesNoCopy()));
					enableLTR(nub, true);
				}

                if (nub->reserved->expressCapability)
                {
					if ((data = OSDynamicCast(OSData, nub->getProperty(kIOPCIExpressASPMDefaultKey))))
					{
						nub->reserved->expressASPMDefault = *((uint32_t *) data->getBytesNoCopy());
						setDeviceASPMState(nub, this, true);
					}
					else			
					{
						nub->setProperty(kIOPCIExpressASPMDefaultKey, nub->reserved->expressASPMDefault, 32);
					}
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIBridge::addBridgeIORange( IOByteCount start, IOByteCount length )
{
    bool ok;

    // fix - ACPIPCI makes this up for hosts with zero space
    if ((0x0 == start) && (0x10000 == length))
        return (false);

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

    if (service->getProperty(kIOPCIResourcedKey))
        return (kIOReturnSuccess);
    service->setProperty(kIOPCIResourcedKey, kOSBooleanTrue);

    err = getDTNubAddressing( nub );

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
        resolveMSIInterrupts( provider, nub );
    resolveLegacyInterrupts( provider, nub );
    if (!msiDefault)
        resolveMSIInterrupts( provider, nub );

    return (err);
}

IOReturn IOPCIBridge::relocate(IOPCIDevice * device, uint32_t options)
{
    spaceFromProperties(device, &device->space);
	return (getDTNubAddressing(device));
}

bool IOPCIBridge::matchKeys( IOPCIDevice * nub, const char * keys,
                             UInt32 defaultMask, UInt8 regNum )
{
    const char *        next;
    UInt32              mask, value, reg;
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

    struct IOPCIMatchingKeys
    {
        const char *    propName;
        UInt8           regs[ 4 ];
        UInt32          defaultMask;
    };
    const IOPCIMatchingKeys *              look;
    static const IOPCIMatchingKeys matching[] = {
                                              { kIOPCIMatchKey,
                                                { 0x00 + 1, 0x2c }, 0xffffffff },
                                              { kIOPCIPrimaryMatchKey,
                                                { 0x00 }, 0xffffffff },
                                              { kIOPCISecondaryMatchKey,
                                                { 0x2c }, 0xffffffff },
                                              { kIOPCIClassMatchKey,
                                                { 0x08 }, 0xffffff00 }};

    for (look = matching;
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
                if (0 == (1 & regNum))
                    break;
            }
        }
    }

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
		firstOffset = *found;
	result = gIOPCIConfigurator->findPCICapability(space, capabilityID, &firstOffset);
	if (found)
		*found = firstOffset;

	return ((UInt32) result);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOPCIBridge::createAGPSpace( IOAGPDevice * master,
                                      IOOptionBits options,
                                      IOPhysicalAddress * address,
                                      IOPhysicalLength * length )
{
    return (kIOReturnUnsupported);
}

IOReturn IOPCIBridge::destroyAGPSpace( IOAGPDevice * master )
{
    return (kIOReturnUnsupported);
}

IORangeAllocator * IOPCIBridge::getAGPRangeAllocator( IOAGPDevice * master )
{
    return (0);
}

IOOptionBits IOPCIBridge::getAGPStatus( IOAGPDevice * master,
                                        IOOptionBits options )
{
    return (0);
}

IOReturn IOPCIBridge::commitAGPMemory( IOAGPDevice * master,
                                       IOMemoryDescriptor * memory,
                                       IOByteCount agpOffset,
                                       IOOptionBits options )
{
    return (kIOReturnUnsupported);
}

IOReturn IOPCIBridge::releaseAGPMemory( IOAGPDevice * master,
                                        IOMemoryDescriptor * memory,
                                        IOByteCount agpOffset,
                                        IOOptionBits options )
{
    return (kIOReturnUnsupported);
}

IOReturn IOPCIBridge::resetAGPDevice( IOAGPDevice * master,
                                      IOOptionBits options )
{
    return (kIOReturnUnsupported);
}

IOReturn IOPCIBridge::getAGPSpace( IOAGPDevice * master,
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

	AbsoluteTime startTime, endTime;
	uint64_t 	 nsec, nsec2;

	ret = fBridgeDevice->checkLink(options & ~kCheckLinkParents);
	if (kIOReturnSuccess != ret) return (kIOReturnNoDevice);

	if ((kCheckLinkParents & options) || !fBridgeInterruptSource) return (ret);

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
			fBridgeDevice->configWrite32(kPCI2PCIMemoryRange,         0);
			fBridgeDevice->configWrite32(kPCI2PCIPrefetchMemoryRange, 0);
			fBridgeDevice->configWrite32(kPCI2PCIPrefetchUpperBase,   0);
			fBridgeDevice->configWrite32(kPCI2PCIPrefetchUpperLimit,  0);
		}
		DLOG("%s: @%lld -> present %d\n", 
			fLogName, nsec / 1000, present);
	}

	return (present ? kIOReturnSuccess : kIOReturnOffline);
}

bool IOPCI2PCIBridge::filterInterrupt( IOFilterInterruptEventSource * source)
{
	IOReturn ret;

//	DLOG("%s: filterInterrupt\n", 
//		fLogName);

	if (kIOPCIDeviceOffState == fPowerState)
		return (false);
	if (fNoDevice)
		return (false);
	ret = checkLink();
	if (kIOReturnNoDevice == ret)
	{
		fNoDevice = true;
		return (false);
	}

    enum { kNeedMask = ((1 << 8) | (1 << 3)) };

    uint16_t slotStatus = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x1a );
    if (kNeedMask & slotStatus)
        fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x1a, slotStatus );

    return (0 != (kNeedMask & slotStatus));
}

void IOPCI2PCIBridge::handleInterrupt(IOInterruptEventSource * source, int count)
{
    bool present;
	UInt32 probeTimeMS = 1;

    fHotplugCount++;

    uint16_t slotStatus  = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x1a );
    uint16_t linkStatus  = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x12 );
    uint16_t linkControl = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x10 );

    DLOG("%s: hotpInt (%d), fNeedProbe %d, slotStatus %x, linkStatus %x, linkControl %x\n",
            fLogName, 
            fHotplugCount, fNeedProbe, slotStatus, linkStatus, linkControl);

    present = (0 != ((1 << 6) & slotStatus));

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

bool IOPCI2PCIBridge::start( IOService * provider )
{
    bool ok;

	fPMAssertion = kIOPMUndefinedDriverAssertionID;

    setName(kIOPCI2PCIBridgeName);
    
	snprintf(fLogName, sizeof(fLogName), "%s(%u:%u:%u)(%u-%u)", 
			 fBridgeDevice->getName(), PCI_ADDRESS_TUPLE(fBridgeDevice), firstBusNum(), lastBusNum());

    ok = super::start(provider);

    if (ok && fBridgeInterruptSource)
        changePowerStateTo(kIOPCIDeviceOnState);

    return (ok);
}

bool IOPCI2PCIBridge::configure( IOService * provider )
{
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
            setProperty(kIOPCIHotPlugKey, kOSBooleanTrue);
        else if (fBridgeDevice->getProperty(kIOPCILinkChangeKey))
        {
            setProperty(kIOPCILinkChangeKey, kOSBooleanTrue);
            fLinkChangeOnly = true;
        }
        else if (fBridgeDevice->getProperty(kIOPCITunnelLinkChangeKey))
		{
		}
		else
            break;

		if (!fBridgeDevice->getProperty(kIOPCITunnelBootDeferKey))
			startHotPlug(provider);
    }
    while(false);

    saveBridgeState();
    if (fBridgeDevice->savedConfig)
    {
        configShadow(fBridgeDevice)->bridge = this;
        configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowBridge;
        if (fBridgeInterruptSource)
			configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowBridgeInterrupts;
        if (OSTypeIDInst(this) != OSTypeID(IOPCI2PCIBridge))
            configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowBridgeDriver;
    }

    return (super::configure(provider));
}

void IOPCI2PCIBridge::startHotPlug(IOService * provider)
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

        fBridgeInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(
                      this,
                      OSMemberFunctionCast(IOInterruptEventSource::Action,
                                            this, &IOPCI2PCIBridge::handleInterrupt),
                      OSMemberFunctionCast(IOFilterInterruptEventSource::Filter,
                                            this, &IOPCI2PCIBridge::filterInterrupt),
                      provider, intIdx);
        if (!fBridgeInterruptSource) break;

        fWorkLoop = gIOPCIConfigurator->getWorkLoop();
		fTimerProbeES = IOTimerEventSource::timerEventSource(this, 
										OSMemberFunctionCast(IOTimerEventSource::Action,
															this, &IOPCI2PCIBridge::timerProbe));
        if (!fTimerProbeES) break;

        ret = fWorkLoop->addEventSource(fTimerProbeES);
		if (kIOReturnSuccess != ret) break;
        ret = fWorkLoop->addEventSource(fBridgeInterruptSource);
		if (kIOReturnSuccess != ret) break;

        fBridgeInterruptEnablePending = true;

		fPresence = (0 != fBridgeDevice->getProperty(kIOPCIOnlineKey));
		fPresenceInt = fPresence;

		uint16_t slotControl = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x18 );
        fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x1a, 1 << 3 );
        slotControl |= kSlotControlEnables;
        fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x18, slotControl );
	}
    while(false);
}

void IOPCI2PCIBridge::startBootDefer(IOService * provider)
{
	DLOG("%s: start boot deferred\n", provider->getName());
	provider->removeProperty(kIOPCITunnelBootDeferKey);
	startHotPlug(provider);
    if (fBridgeInterruptEnablePending)
    {
        // enable hotp ints
        fBridgeInterruptSource->enable();
		fBridgeInterruptSource->signalInterrupt();
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
			fBridgeInterruptSource->signalInterrupt();
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
		uint16_t slotControl;
		unsigned long fromPowerState;

		fromPowerState = fPowerState;
		fPowerState = powerState;

		if (kIOPCIDeviceOffState == powerState)
		{
			if (fNeedProbe) DLOG("%s: sleeping with fNeedProbe\n", fLogName);
			slotControl = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x18 );
			slotControl &= ~kSlotControlEnables;
			fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x18, slotControl );
			break;
		}		
		if (kIOPCIDeviceOffState == fromPowerState)
		{
			if (fNoDevice) break;
			ret = checkLink();
			if (kIOReturnNoDevice == ret)
			{
				fNoDevice = true;
				break;
			}

			fNeedProbe |= fPresence;

			slotControl = fBridgeDevice->configRead16( fBridgeDevice->reserved->expressCapability + 0x18 );
			fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x1a, 1 << 3 );
			slotControl |= kSlotControlEnables;
			fBridgeDevice->configWrite16( fBridgeDevice->reserved->expressCapability + 0x18, slotControl );

		}
		if (kIOPCIDeviceOnState == powerState)
		{
			if (fBridgeDevice->reserved->needsProbe) deferredProbe(fBridgeDevice);
			fBridgeInterruptSource->signalInterrupt();
		}
	}
	while (false);

    return (super::setPowerState(powerState, whatDevice));
}

void IOPCI2PCIBridge::adjustPowerState(unsigned long state)
{
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
    if (device->getProperty(kIOPMPCISleepLinkDisableKey))
		configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowSleepLinkDisable;
	else
		configShadow(fBridgeDevice)->flags &= ~kIOPCIConfigShadowSleepLinkDisable;
    if (device->getProperty(kIOPMPCISleepResetKey))
		configShadow(fBridgeDevice)->flags |= kIOPCIConfigShadowSleepReset;
	else
		configShadow(fBridgeDevice)->flags &= ~kIOPCIConfigShadowSleepReset;

    if (device->getProperty(gIOPCITunnelledKey) || device->getProperty(kIOPCIEjectableKey))
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
	if (kIOPMUndefinedDriverAssertionID != fPMAssertion)
	{
		getPMRootDomain()->releasePMAssertion(fPMAssertion);
		fPMAssertion = kIOPMUndefinedDriverAssertionID;
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


IOReturn IOPCIBridge::resolveMSIInterrupts( IOService * provider, IOPCIDevice * nub )
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

    IOByteCount msiCapability = nub->reserved->msiCapability;
    if (msiCapability && reserved && reserved->messagedInterruptController)
    {
        ret = reserved->messagedInterruptController->allocateDeviceInterrupts(
       	        nub, 0, msiCapability);
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


