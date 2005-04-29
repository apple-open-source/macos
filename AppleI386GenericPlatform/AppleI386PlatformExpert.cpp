/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 */
 
#include <IOKit/system.h>

#include <pexpert/i386/boot.h>

#include <IOKit/IORegistryEntry.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSUnserialize.h>

extern "C" {
#include <i386/cpuid.h>
}

#include <IOKit/platform/ApplePlatformExpert.h>
#include "AppleI386PlatformExpert.h"

#include <IOKit/assert.h>

__BEGIN_DECLS
extern void kdreboot(void);
__END_DECLS

enum {
    kIRQAvailable   = 0,
    kIRQExclusive   = 1,
    kIRQSharable    = 2,
    kSystemIRQCount = 16
};

static struct {
    UInt16  consumers;
    UInt16  status;
} IRQ[kSystemIRQCount];

static IOLock * ResourceLock;

class AppleI386PlatformExpertGlobals
{
public:
    bool isValid;
    AppleI386PlatformExpertGlobals();
    ~AppleI386PlatformExpertGlobals();
};

static AppleI386PlatformExpertGlobals AppleI386PlatformExpertGlobals;

AppleI386PlatformExpertGlobals::AppleI386PlatformExpertGlobals()
{
    ResourceLock         = IOLockAlloc();
    bzero(IRQ, sizeof(IRQ));
}

AppleI386PlatformExpertGlobals::~AppleI386PlatformExpertGlobals()
{
    if (ResourceLock) IOLockFree(ResourceLock);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOPlatformExpert

OSDefineMetaClassAndStructors(AppleI386PlatformExpert, IOPlatformExpert)

IOService * AppleI386PlatformExpert::probe(IOService * 	/* provider */,
                                           SInt32 *		score )
{
    return (this);
}

bool
AppleI386PlatformExpert::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return false;

    _interruptControllerName = OSSymbol::withString((OSString *) getProperty("InterruptControllerName"));

    return true;
}

bool
AppleI386PlatformExpert::start(IOService * provider)
{
    setBootROMType(kBootROMTypeNewWorld); /* hammer to new world for i386 */

    if (!super::start(provider))
        return false;

    // Install halt/restart handler.
    PE_halt_restart = handlePEHaltRestart;

    registerService();

    return true;
}

bool AppleI386PlatformExpert::configure( IOService * provider )
{
    OSArray *      topLevel;
    OSDictionary * dict;
    IOService *    nub;

    topLevel = OSDynamicCast( OSArray, getProperty("top-level") );

    if (topLevel )
    {
        while ((dict = OSDynamicCast(OSDictionary, topLevel->getObject(0))))
        {
            dict->retain();
            topLevel->removeObject( 0 );
            nub = createNub( dict );
            if ( 0 == nub )
                continue;
            dict->release();
            nub->attach( this );
            nub->registerService();
            // nub->release();
        }
    }

    return true;
}


IOService * AppleI386PlatformExpert::createNub(OSDictionary * from)
{
    IOService *      nub;
    OSData *		 prop;
    KernelBootArgs_t * bootArgs;

    nub = super::createNub(from);

    if (nub)
    {
        const char *name = nub->getName();
        
        if (0 == strcmp( "pci", name))
        {
        	bootArgs = (KernelBootArgs_t *) PE_state.bootArgs;
        	prop = OSData::withBytesNoCopy(&bootArgs->pciInfo,
               	                           sizeof(bootArgs->pciInfo));
        	assert(prop);
        	if (prop)
                from->setObject( "pci-bus-info", prop);
        }
	else if (0 == strcmp("bios", name))
	{
	    setupBIOS(nub);
        }
        else if (0 != strcmp("8259-pic", name))
        {
            setupPIC(nub);
        }
    }

    return (nub);
}

void
AppleI386PlatformExpert::setupPIC(IOService *nub)
{
    int            i;
    OSDictionary * propTable;
    OSArray *      controller;
    OSArray *      specifier;
    OSData *       tmpData;
    long           tmpLong;

    propTable = nub->getPropertyTable();

    //
    // For the moment.. assume a classic 8259 interrupt controller
    // with 16 interrupts.
    //
    // Later, this will be changed to detect a APIC and/or MP-Table
    // and then will set the nubs appropriately.

    // Create the interrupt specifer array.
    specifier = OSArray::withCapacity(kSystemIRQCount);
    assert(specifier);
    for (i = 0; i < kSystemIRQCount; i++) {
        tmpLong = i;
        tmpData = OSData::withBytes(&tmpLong, sizeof(tmpLong));
        specifier->setObject(tmpData);
    }

    // Create the interrupt controller array.
    controller = OSArray::withCapacity(kSystemIRQCount);
    assert(controller);
    for (i = 0; i < kSystemIRQCount; i++)
        controller->setObject(_interruptControllerName);

    // Put the two arrays into the property table.
    propTable->setObject(gIOInterruptControllersKey, controller);
    propTable->setObject(gIOInterruptSpecifiersKey, specifier);

    // Release the arrays after being added to the property table.
    specifier->release();
    controller->release();
}

void
AppleI386PlatformExpert::setupBIOS(IOService *nub)
{
    KernelBootArgs_t *bootArgs = (KernelBootArgs_t *) PE_state.bootArgs;
    IOPlatformDevice *bios_dev;
    int i;
    OSNumber *value;

    for (i=0; i<bootArgs->numDrives; i++) {
      boot_drive_info_t *device_p = &bootArgs->driveInfo[i];
      char namebuf[16];
      bios_dev = new IOPlatformDevice;
      bios_dev->init();
      bios_dev->start(this);
      sprintf(namebuf, "drive%d", i);
      bios_dev->setProperty("IOName", namebuf);
      bios_dev->setName(namebuf);
      value = OSNumber::withNumber(i, sizeof(i)*8);
      if (value) {
        bios_dev->setProperty("Drive", value);
	value->release();
      }
      value = OSNumber::withNumber(device_p->params.phys_cyls, sizeof(device_p->params.phys_cyls)*8);
      if (value) {
        bios_dev->setProperty("PhysicalCylinders", value);
	value->release();
      }
      value = OSNumber::withNumber(device_p->params.phys_heads, sizeof(device_p->params.phys_heads)*8);
      if (value) {
        bios_dev->setProperty("PhysicalHeads", value);
	value->release();
      }
      value = OSNumber::withNumber(device_p->params.phys_spt, sizeof(device_p->params.phys_spt)*8);
      if (value) {
        bios_dev->setProperty("PhysicalSectorsPerTrack", value);
	value->release();
      }
      value = OSNumber::withNumber(device_p->params.phys_nbps, sizeof(device_p->params.phys_nbps)*8);
      if (value) {
        bios_dev->setProperty("PhysicalBytesPerSector", value);
	value->release();
      }
      value = OSNumber::withNumber(device_p->params.phys_sectors, sizeof(device_p->params.phys_sectors)*8);
      if (value) {
        bios_dev->setProperty("PhysicalSectors", value);
	value->release();
      }
      value = OSNumber::withNumber(device_p->params.info_flags, sizeof(device_p->params.info_flags)*8);
      if (value) {
        bios_dev->setProperty("InfoFlags", value);
	value->release();
      }

      if (device_p->params.dpte_offset != 0xFFFF && device_p->params.dpte_segment != 0xFFFF) {
	value = OSNumber::withNumber(device_p->dpte.io_port_base, sizeof(device_p->dpte.io_port_base)*8);
        if (value) {
	  bios_dev->setProperty("IOPortBase", value);
	  value->release();
        }
	value = OSNumber::withNumber(device_p->dpte.control_port_base, sizeof(device_p->dpte.control_port_base)*8);
        if (value) {
	  bios_dev->setProperty("ControlPortBase", value);
	  value->release();
        }
	value = OSNumber::withNumber((unsigned char)device_p->dpte.irq, 8);
        if (value) {
	  bios_dev->setProperty("IRQ", value);
	  value->release();
        }
	value = OSNumber::withNumber(device_p->dpte.block_count, sizeof(device_p->dpte.block_count)*8);
        if (value) {
	  bios_dev->setProperty("BlockCount", value);
	  value->release();
        }
	value = OSNumber::withNumber((unsigned char)device_p->dpte.dma_channel, 8);
        if (value) {
	  bios_dev->setProperty("DMAChannel", value);
	  value->release();
        }
	value = OSNumber::withNumber((unsigned char)device_p->dpte.dma_type, 8);
        if (value) {
	  bios_dev->setProperty("DMAType", value);
	  value->release();
        }
	value = OSNumber::withNumber((unsigned char)device_p->dpte.pio_type, 8);
        if (value) {
	  bios_dev->setProperty("PIOType", value);
	  value->release();
        }
	value = OSNumber::withNumber(device_p->dpte.option_flags, sizeof(device_p->dpte.option_flags)*8);
        if (value) {
	  bios_dev->setProperty("OptionFlags", value);
	  value->release();
        }
	value = OSNumber::withNumber(device_p->dpte.head_flags, sizeof(device_p->dpte.head_flags)*8);
        if (value) {
	  bios_dev->setProperty("HeadFlags", value);
	  value->release();
        }
	value = OSNumber::withNumber(device_p->dpte.vendor_info, sizeof(device_p->dpte.vendor_info)*8);
        if (value) {
	  bios_dev->setProperty("VendorInfo", value);
	  value->release();
        }
      }

      nub->attachToChild(bios_dev, gIOServicePlane);
    }
}

bool
AppleI386PlatformExpert::matchNubWithPropertyTable(IOService *    nub,
					                               OSDictionary * propTable )
{
    OSString * nameProp;
    OSString * match;

    if (0 == (nameProp = (OSString *) nub->getProperty(gIONameKey)))
        return (false);

    if ( 0 == (match = (OSString *) propTable->getObject(gIONameMatchKey)))
        return (false);

    return (match->isEqualTo( nameProp ));
}

bool AppleI386PlatformExpert::getMachineName( char * name, int maxLength )
{
    strncpy( name, "x86", maxLength );

    return (true);
}


bool AppleI386PlatformExpert::getModelName( char * name, int maxLength )
{
    i386_cpu_info_t *cpuid_cpu_info = cpuid_info();

    if (cpuid_cpu_info->cpuid_brand_string[0] != '\0') {
        strncpy(name, cpuid_cpu_info->cpuid_brand_string, maxLength);
    } else {
        strncpy(name, cpuid_cpu_info->cpuid_model_string, maxLength);
    }

    name[maxLength - 1] = '\0';

    return (true);
}


int AppleI386PlatformExpert::handlePEHaltRestart( unsigned int type )
{
    int ret = 1;
	
    switch ( type )
    {
        case kPERestartCPU:
            // Use the pexpert service to reset the system through
            // the keyboard controller.
            kdreboot();
            break;

        case kPEHaltCPU:
        default:
            ret = -1;
            break;
    }

    return ret;
}


bool AppleI386PlatformExpert::setNubInterruptVectors(
                                                     IOService *  nub,
                                                     const UInt32 vectors[],
                                                     UInt32       vectorCount )
{
    OSArray * controller = 0;
    OSArray * specifier  = 0;
    bool      success = false;

    if ( vectorCount == 0 )
    {
        nub->removeProperty( gIOInterruptControllersKey );
        nub->removeProperty( gIOInterruptSpecifiersKey );
        return true;
    }

    // Create the interrupt specifer and controller arrays.

    specifier  = OSArray::withCapacity( vectorCount );
    controller = OSArray::withCapacity( vectorCount );
    if (!specifier || !controller) goto done;

    for ( UInt32 i = 0; i < vectorCount; i++ )
    {
        OSData * data = OSData::withBytes(&vectors[i], sizeof(vectors[i]));
        specifier->setObject( data );
        controller->setObject( _interruptControllerName );
        if (data) data->release();
    }

    nub->setProperty( gIOInterruptControllersKey, controller );
    nub->setProperty( gIOInterruptSpecifiersKey,  specifier  );
    success = true;

done:
        if (specifier)  specifier->release();
    if (controller) controller->release();
    return success;
}

bool AppleI386PlatformExpert::setNubInterruptVector( IOService * nub,
                                                     UInt32      vector )
{
    return setNubInterruptVectors( nub, &vector, 1 );
}


IOReturn AppleI386PlatformExpert::callPlatformFunction(
                                                       const OSSymbol * functionName,
                                                       bool waitForFunction,
                                                       void * param1, void * param2,
                                                       void * param3, void * param4 )
{
    bool ok;

    if ( functionName->isEqualTo( "SetDeviceInterrupts" ) )
    {
        IOService * nub         = (IOService *) param1;
        UInt32 *    vectors     = (UInt32 *)    param2;
        UInt32      vectorCount = (UInt32)      param3;
        bool        exclusive   = (bool)        param4;

        if (vectorCount != 1) return kIOReturnBadArgument;

        ok = reserveSystemInterrupt( nub, vectors[0], exclusive );
        if (ok == false) return kIOReturnNoResources;

        ok = setNubInterruptVector( nub, vectors[0] );
        if (ok == false)
            releaseSystemInterrupt( nub, vectors[0], exclusive );

        return ( ok ? kIOReturnSuccess : kIOReturnNoMemory );

    }
    else if ( functionName->isEqualTo( "SetBusClockRateMHz" ) )
    {
        UInt32     rateInMHz       = (UInt32)      param1;

        gPEClockFrequencyInfo.bus_clock_rate_hz = (rateInMHz * 1000000);

        return kIOReturnSuccess;
    }
    else if ( functionName->isEqualTo( "SetCPUClockRateMHz" ) )
    {
        UInt32     rateInMHz       = (UInt32)      param1;

        gPEClockFrequencyInfo.cpu_clock_rate_hz = (rateInMHz * 1000000);

        return kIOReturnSuccess;
    }
    
    return super::callPlatformFunction( functionName, waitForFunction,
                                        param1, param2, param3, param4 );
}



//---------------------------------------------------------------------------

bool AppleI386PlatformExpert::reserveSystemInterrupt( IOService * client,
                                                      UInt32      vectorNumber,
                                                      bool        exclusive )
{
    bool  ok = false;

    if ( vectorNumber >= kSystemIRQCount ) return ok;

    IOLockLock( ResourceLock );

    if ( exclusive )
    {
        if (IRQ[vectorNumber].status == kIRQAvailable)
        {
            IRQ[vectorNumber].status = kIRQExclusive;
            IRQ[vectorNumber].consumers = 1;
            ok = true;
        }
    }
    else
    {
        if (IRQ[vectorNumber].status == kIRQAvailable ||
            IRQ[vectorNumber].status == kIRQSharable)
        {
            IRQ[vectorNumber].status = kIRQSharable;
            IRQ[vectorNumber].consumers++;
            ok = true;
        }
    }

    IOLockUnlock( ResourceLock );

    return ok;
}

void AppleI386PlatformExpert::releaseSystemInterrupt( IOService * client,
                                                      UInt32      vectorNumber,
                                                      bool        exclusive )
{
    if ( vectorNumber >= kSystemIRQCount ) return;

    IOLockLock( ResourceLock );

    if ( exclusive )
    {
        if (IRQ[vectorNumber].status == kIRQExclusive)
        {
            IRQ[vectorNumber].status = kIRQAvailable;
            IRQ[vectorNumber].consumers = 0;
        }
    }
    else
    {
        if (IRQ[vectorNumber].status == kIRQSharable &&
            --IRQ[vectorNumber].consumers == 0)
        {
            IRQ[vectorNumber].status = kIRQAvailable;
        }
    }

    IOLockUnlock( ResourceLock );
}

