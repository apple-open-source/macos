//
//  PCIDriverKitPEX8733VBridge.cpp
//  PCIDriverKitPEX8733VBridge
//
//  Created by Kevin Strasberg on 10/29/19.
//
#include <stdio.h>
#include <os/log.h>
#include <DriverKit/DriverKit.h>
#include <PCIDriverKit/PCIDriverKit.h>

#include "PCIDriverKitPEX8733VBridge.h"
#include "PEX8733Definitions.h"
#define debugLog(fmt, args...)  os_log(OS_LOG_DEFAULT, "PCIDriverKitPEX8733VBridge::%s:  " fmt,  __FUNCTION__,##args)

struct PCIDriverKitPEX8733VBridge_IVars
{
    IOPCIDevice* pciDevice;
    uint64_t     vendorCapabilityID;
};

bool
PCIDriverKitPEX8733VBridge::init()
{
    if(super::init() != true)
    {
        return false;
    }

    ivars = IONewZero(PCIDriverKitPEX8733VBridge_IVars, 1);

    if(ivars == NULL)
    {
        return false;
    }

    return true;
}

void
PCIDriverKitPEX8733VBridge::free()
{
    IOSafeDeleteNULL(ivars, PCIDriverKitPEX8733VBridge_IVars, 1);
    super::free();
}

kern_return_t
IMPL(PCIDriverKitPEX8733VBridge, Start)
{
    kern_return_t result = Start(provider, SUPERDISPATCH);

    if(result != kIOReturnSuccess)
    {
        return result;
    }

    ivars->pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if(ivars->pciDevice == NULL)
    {
        Stop(provider);
        return kIOReturnNoDevice;
    }
    ivars->pciDevice->retain();

    if(ivars->pciDevice->Open(this, 0) != kIOReturnSuccess)
    {
        Stop(provider);
        return kIOReturnNoDevice;
    }

    debugLog("enabling bus lead and memory space");
    uint16_t command;
    ivars->pciDevice->ConfigurationRead16(kIOPCIConfigurationOffsetCommand, &command);
    ivars->pciDevice->ConfigurationWrite16(kIOPCIConfigurationOffsetCommand,
                                           command | kIOPCICommandBusLead | kIOPCICommandMemorySpace);

    return result;
}

kern_return_t
IMPL(PCIDriverKitPEX8733VBridge, Stop)
{
    debugLog("disabling bus lead and memory space");
    if(ivars->pciDevice != NULL)
    {
        uint16_t command;
        ivars->pciDevice->ConfigurationRead16(kIOPCIConfigurationOffsetCommand, &command);
        ivars->pciDevice->ConfigurationWrite16(kIOPCIConfigurationOffsetCommand,
                                               command & ~(kIOPCICommandBusLead | kIOPCICommandMemorySpace));

    }
    OSSafeReleaseNULL(ivars->pciDevice);
    return Stop(provider, SUPERDISPATCH);
}
