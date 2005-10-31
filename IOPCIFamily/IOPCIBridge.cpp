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
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>
#include <IOKit/assert.h>
#include <IOKit/IOCatalogue.h>

#include <libkern/c++/OSContainers.h>

extern "C"
{
#include <machine/machine_routines.h>
};

#define DEBUG_PCI_PWR_MGMT 0

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService
OSDefineMetaClassAndAbstractStructorsWithInit( IOPCIBridge, IOService, IOPCIBridge::initialize() )

OSMetaClassDefineReservedUsed(IOPCIBridge, 0);
OSMetaClassDefineReservedUsed(IOPCIBridge, 1);
OSMetaClassDefineReservedUnused(IOPCIBridge,  2);
OSMetaClassDefineReservedUnused(IOPCIBridge,  3);
OSMetaClassDefineReservedUnused(IOPCIBridge,  4);
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

// 1 log, 2 disable DT, 4 bridge numbering 
int gIOPCIDebug = 0;

#ifdef __i386__
__BEGIN_DECLS
#include <i386/cpu_number.h>
extern void mp_rendezvous_no_intrs(
               void (*action_func)(void *),
               void *arg);
__END_DECLS

static void resolvePCIInterrupt( IOService * provider, IOPCIDevice * nub );
#endif

/* Definitions of PCI2PCI Config Registers */
enum {
    kPCI2PCIPrimaryBus		= 0x18,		// 8 bit
    kPCI2PCISecondaryBus	= 0x19,		// 8 bit
    kPCI2PCISubordinateBus	= 0x1a,		// 8 bit
    kPCI2PCIIORange		= 0x1c,
    kPCI2PCIMemoryRange		= 0x20,
    kPCI2PCIPrefetchMemoryRange	= 0x24,
    kPCI2PCIUpperIORange	= 0x30
};

enum { kIOPCIMaxPCI2PCIBridges = 32 };

static IOPCI2PCIBridge * gIOAllPCI2PCIBridges[kIOPCIMaxPCI2PCIBridges];
IOSimpleLock *  	 gIOAllPCI2PCIBridgesLock;
UInt32			 gIOAllPCI2PCIBridgeState;
const OSSymbol *	 gIOPlatformDeviceMessageKey;

/* Expansion data fields */
#define cardBusMemoryRanges	  reserved->cardBusMemoryRanges

#ifndef kIOPlatformDeviceMessageKey
#define kIOPlatformDeviceMessageKey     "IOPlatformDeviceMessage"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// stub driver has two power states, off and on

enum { kIOPCIBridgePowerStateCount = 3 };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOPCIBridge::initialize(void)
{
    if (!gIOAllPCI2PCIBridgesLock)
	gIOAllPCI2PCIBridgesLock = IOSimpleLockAlloc();

    gIOPlatformDeviceMessageKey = OSSymbol::withCStringNoCopy(kIOPlatformDeviceMessageKey);
}

bool IOPCIBridge::start( IOService * provider )
{
    static const IOPMPowerState powerStates[ kIOPCIBridgePowerStateCount ] = {
                { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 1, 0, IOPMSoftSleep, IOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 1, IOPMPowerOn, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
            };

    if (!super::start(provider))
        return (false);

    reserved = IONew(ExpansionData, 1);
    if (reserved == 0) return (false);

    bzero(reserved, sizeof(ExpansionData));

    // empty ranges to start
    bridgeMemoryRanges = IORangeAllocator::withRange( 0, 1, 8,
                         IORangeAllocator::kLocking );
    assert( bridgeMemoryRanges );
    setProperty( "Bridge Memory Ranges", bridgeMemoryRanges );

    bridgeIORanges = IORangeAllocator::withRange( 0, 1, 8,
                     IORangeAllocator::kLocking );
    assert( bridgeIORanges );
    setProperty( "Bridge IO Ranges", bridgeIORanges );

#ifdef __i386__
    cardBusMemoryRanges = IORangeAllocator::withRange( 0, 1, 8,
                          IORangeAllocator::kLocking );
#endif

    if (!configure(provider))
        return (false);

    // initialize superclass variables
    PMinit();
    // register as controlling driver
    registerPowerDriver( this, (IOPMPowerState *) powerStates,
                         kIOPCIBridgePowerStateCount);
    // join the tree
    provider->joinPMtree( this);
    // clamp power on
    temporaryPowerClampOn();

    probeBus( provider, firstBusNum() );

    return (true);
}

void IOPCIBridge::free( void )
{
    if (bridgeMemoryRanges)
    {
        bridgeMemoryRanges->release();
        bridgeMemoryRanges = 0;
    }

    if (bridgeIORanges)
    {
        bridgeIORanges->release();
        bridgeIORanges = 0;
    }

    if (reserved)
    {
        if (cardBusMemoryRanges)
        {
            cardBusMemoryRanges->release();
            cardBusMemoryRanges = 0;
        }
        IODelete(reserved, ExpansionData, 1);
    }

    super::free();
}

IOReturn IOPCIBridge::setDevicePowerState( IOPCIDevice * device,
        unsigned long whatToDo )
{
    if ((kSaveDeviceState == whatToDo) || (kRestoreDeviceState == whatToDo))
    {
	IOReturn ret = kIOReturnSuccess;
	void * p3    = (kSaveDeviceState == whatToDo) ? (void *) 0 : (void *) 3;

	device->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
	    (void *) kIOMessageDeviceWillPowerOff, device, p3, (void *) 0);

        if (kOSBooleanFalse != device->getProperty(kIOPMPCIConfigSpaceVolatileKey))
	{
	    if (kRestoreDeviceState == whatToDo)
		ret = restoreDeviceState(device);
	    else
		ret = saveDeviceState(device);
	}

	device->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
	    (void *) kIOMessageDeviceHasPoweredOn, device, p3, (void *) 0);

	return (ret);
    }

    // Special for pci/pci-bridge devices - 2 to save immediately, 3 to restore immediately

    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

    if (gIOAllPCI2PCIBridgeState != whatToDo)
    {
	for (UInt32 i = 0; i < kIOPCIMaxPCI2PCIBridges; i++)
	{
	    if (gIOAllPCI2PCIBridges[i])
	    {
		if (kSaveBridgeState == whatToDo)
		    gIOAllPCI2PCIBridges[i]->saveBridgeState();
		else
		    gIOAllPCI2PCIBridges[i]->restoreBridgeState();
	    }
	}
	gIOAllPCI2PCIBridgeState = whatToDo;
    }

    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::saveDeviceState( IOPCIDevice * device,
                                       IOOptionBits options )
{
    UInt32 flags;
    int i;

    if (!device->savedConfig)
        return (kIOReturnNotReady);

    flags = device->savedConfig[kIOPCIConfigShadowFlags];
    if ((kIOPCIConfigShadowValid | kIOPCIConfigShadowBridge) & flags)
        return (kIOReturnSuccess);
    flags |= kIOPCIConfigShadowValid;
    device->savedConfig[kIOPCIConfigShadowFlags] = flags;

    for (i = 0; i < kIOPCIConfigShadowRegs; i++)
        if (kIOPCIVolatileRegsMask & (1 << i))
            device->savedConfig[i] = device->configRead32( i * 4 );

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::restoreDeviceState( IOPCIDevice * device,
        IOOptionBits options )
{
    UInt32 flags;
    int i;

    if (!device->savedConfig)
        return (kIOReturnNotReady);

    flags = device->savedConfig[kIOPCIConfigShadowFlags];
    if (kIOPCIConfigShadowBridge & flags)
        return (kIOReturnSuccess);
    if (!(kIOPCIConfigShadowValid & flags))
        return (kIOReturnNotReady);
    flags &= ~kIOPCIConfigShadowValid;
    device->savedConfig[kIOPCIConfigShadowFlags] = flags;

    for (i = (kIOPCIConfigRevisionID >> 2); i < kIOPCIConfigShadowRegs; i++)
        if (kIOPCIVolatileRegsMask & (1 << i))
            device->configWrite32( i * 4, device->savedConfig[ i ]);

    device->configWrite32( kIOPCIConfigCommand, device->savedConfig[1]);

    return (kIOReturnSuccess);
}


bool IOPCIBridge::configure( IOService * provider )
{
    return (true);
}

SInt32 IOPCIBridge::compareAddressCell( UInt32 /* cellCount */, UInt32 cleft[], UInt32 cright[] )
{
    IOPCIPhysicalAddress *  left 	= (IOPCIPhysicalAddress *) cleft;
    IOPCIPhysicalAddress *  right 	= (IOPCIPhysicalAddress *) cright;
    static const UInt8      spacesEq[] 	= { 0, 1, 2, 2 };

    if (spacesEq[ left->physHi.s.space ] != spacesEq[ right->physHi.s.space ])
        return (-1);

    return (left->physLo - right->physLo);
}

void IOPCIBridge::nvLocation( IORegistryEntry * entry,
                              UInt8 * busNum, UInt8 * deviceNum, UInt8 * functionNum )
{
    IOPCIDevice *	nub;

    nub = OSDynamicCast( IOPCIDevice, entry );
    assert( nub );

    *busNum		= nub->space.s.busNum;
    *deviceNum		= nub->space.s.deviceNum;
    *functionNum	= nub->space.s.functionNum;
}

void IOPCIBridge::spaceFromProperties( OSDictionary * propTable,
                                       IOPCIAddressSpace * space )
{
    OSData *			regProp;
    IOPCIAddressSpace * 	inSpace;

    space->bits = 0;

    if ((regProp = (OSData *) propTable->getObject("reg")))
    {
        inSpace = (IOPCIAddressSpace *) regProp->getBytesNoCopy();
        space->s.busNum = inSpace->s.busNum;
        space->s.deviceNum = inSpace->s.deviceNum;
        space->s.functionNum = inSpace->s.functionNum;
    }
    else if ((regProp = (OSData *) propTable->getObject("acpi-reg")))
    {
        inSpace = (IOPCIAddressSpace *) regProp->getBytesNoCopy();
        space->s.busNum = firstBusNum();
        space->s.deviceNum = inSpace->s.deviceNum;
        space->s.functionNum = inSpace->s.functionNum;
    }
}

IORegistryEntry * IOPCIBridge::findMatching( OSIterator * kids,
        IOPCIAddressSpace space )
{
    IORegistryEntry *		found = 0;
    IOPCIAddressSpace		regSpace;

    if (kids)
    {
        kids->reset();
        while ((0 == found)
                && (found = (IORegistryEntry *) kids->getNextObject()))
        {
            spaceFromProperties( found->getPropertyTable(), &regSpace);
            if (space.bits != regSpace.bits)
                found = 0;
        }
    }
    return (found);
}

bool IOPCIBridge::checkProperties( IOPCIDevice * entry )
{
    UInt32	vendor, product, classCode, revID;
    UInt32	subVendor = 0, subProduct = 0;
    IOByteCount offset;
    OSData *	data;
    OSData *	nameData;
    char	compatBuf[128];
    char *	out;

    if ((data = OSDynamicCast(OSData, entry->getProperty("vendor-id"))))
        vendor = *((UInt32 *) data->getBytesNoCopy());
    else
        return (false);
    if ((data = OSDynamicCast(OSData, entry->getProperty("device-id"))))
        product = *((UInt32 *) data->getBytesNoCopy());
    else
        return (false);
    if ((data = OSDynamicCast(OSData, entry->getProperty("class-code"))))
        classCode = *((UInt32 *) data->getBytesNoCopy());
    else
        return (false);
    if ((data = OSDynamicCast(OSData, entry->getProperty("revision-id"))))
        revID = *((UInt32 *) data->getBytesNoCopy());
    else
        return (false);
    if ((data = OSDynamicCast(OSData, entry->getProperty("subsystem-vendor-id"))))
        subVendor = *((UInt32 *) data->getBytesNoCopy());
    if ((data = OSDynamicCast(OSData, entry->getProperty("subsystem-id"))))
        subProduct = *((UInt32 *) data->getBytesNoCopy());

    if (entry->savedConfig)
    {
	// update matching config space regs from properties
	entry->savedConfig[kIOPCIConfigVendorID >> 2] = (product << 16) | vendor;
	entry->savedConfig[kIOPCIConfigRevisionID >> 2] = (classCode << 8) | revID;
	if (subVendor && subProduct)
	    entry->savedConfig[kIOPCIConfigSubSystemVendorID >> 2] = (subProduct << 16) | subVendor;
    }

    if (!(data = OSDynamicCast(OSData, entry->getProperty("compatible")))
            || !(nameData = OSDynamicCast(OSData, entry->getProperty("name")))
            || data->isEqualTo(nameData))
    {
	// compatible change needed
	out = compatBuf;
	if ((subVendor || subProduct)
		&& ((subVendor != vendor) || (subProduct != product)))
	    out += sprintf(out, "pci%lx,%lx", subVendor, subProduct) + 1;
	out += sprintf(out, "pci%lx,%lx", vendor, product) + 1;
	out += sprintf(out, "pciclass,%06lx", classCode) + 1;
    
	entry->setProperty("compatible", compatBuf, out - compatBuf);
    }

    offset = 0;
    if (entry->extendedFindPCICapability(kIOPCIPCIExpressCapability, &offset))
    {
	UInt16 linkStatus = entry->configRead16(offset + 0x12);

	entry->setProperty(kIOPCIExpressLinkStatusKey, linkStatus, 32);
    }

    return (true);
}

OSDictionary * IOPCIBridge::constructProperties( IOPCIAddressSpace space )
{
    OSDictionary *	propTable;
    UInt32		value;
    UInt32		vendor, product, classCode, revID;
    UInt32		subVendor = 0, subProduct = 0;
    OSData *		prop;
    const char *	name;
    const OSSymbol *	nameProp;
    char		compatBuf[128];
    char *		out;

    struct IOPCIGenericNames
    {
        const char *	name;
        UInt32		mask;
        UInt32		classCode;
    };
    static const IOPCIGenericNames genericNames[] = {
                { "display", 	0xffffff, 0x000100 },
                { "scsi", 	0xffff00, 0x010000 },
                { "ethernet", 	0xffff00, 0x020000 },
                { "display", 	0xff0000, 0x030000 },
                { "pci-bridge", 0xffff00, 0x060400 },
                { 0, 0, 0 }
            };
    const IOPCIGenericNames *	nextName;


    propTable = OSDictionary::withCapacity( 8 );

    if (propTable)
    {
        prop = OSData::withBytes( &space, sizeof( space) );
        if (prop)
        {
            propTable->setObject("reg", prop );
            prop->release();
        }

        value = configRead32( space, kIOPCIConfigVendorID );
        vendor = value & 0xffff;
        product = value >> 16;

        prop = OSData::withBytes( &vendor, sizeof(vendor) );
        if (prop)
        {
            propTable->setObject("vendor-id", prop );
            prop->release();
        }

        prop = OSData::withBytes( &product, sizeof(product) );
        if (prop)
        {
            propTable->setObject("device-id", prop );
            prop->release();
        }

        value = configRead32( space, kIOPCIConfigRevisionID );
        revID = value & 0xff;
        prop = OSData::withBytes( &revID, sizeof(revID) );
        if (prop)
        {
            propTable->setObject("revision-id", prop );
            prop->release();
        }

        classCode = value >> 8;
        prop = OSData::withBytes( &classCode, sizeof(classCode) );
        if (prop)
        {
            propTable->setObject("class-code", prop );
            prop->release();
        }

        // make generic name

        name = 0;
        for (nextName = genericNames;
                (0 == name) && nextName->name;
                nextName++)
        {
            if ((classCode & nextName->mask) == nextName->classCode)
                name = nextName->name;
        }

        // or name from IDs

        value = configRead32( space, kIOPCIConfigSubSystemVendorID );
        if (value)
        {
            subVendor = value & 0xffff;
            subProduct = value >> 16;

            prop = OSData::withBytes( &subVendor, sizeof(subVendor) );
            if (prop)
            {
                propTable->setObject("subsystem-vendor-id", prop );
                prop->release();
            }
            prop = OSData::withBytes( &subProduct, sizeof(subProduct) );
            if (prop)
            {
                propTable->setObject("subsystem-id", prop );
                prop->release();
            }
        }

        out = compatBuf;
        if ((subVendor || subProduct)
                && ((subVendor != vendor) || (subProduct != product)))
            out += sprintf(out, "pci%lx,%lx", subVendor, subProduct) + 1;
        if (0 == name)
            name = out;
        out += sprintf(out, "pci%lx,%lx", vendor, product) + 1;
        out += sprintf(out, "pciclass,%06lx", classCode) + 1;

        prop = OSData::withBytes( compatBuf, out - compatBuf );
        if (prop)
        {
            propTable->setObject("compatible", prop );
            prop->release();
        }

        nameProp = OSSymbol::withCString( name );
        if (nameProp)
        {
            propTable->setObject( "name", (OSSymbol *) nameProp);
            propTable->setObject( gIONameKey, (OSSymbol *) nameProp);
            nameProp->release();
        }
    }

    return (propTable);
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

bool IOPCIBridge::publishNub( IOPCIDevice * nub, UInt32 /* index */ )
{
    char			location[ 24 ];
    bool			ok;
    OSData *			data;
    OSData *			driverData;
    UInt32			*regData, expRomReg;
    IOMemoryMap *		memoryMap;
    IOVirtualAddress		virtAddr;

    if (nub)
    {
        if (nub->space.s.functionNum)
            sprintf( location, "%X,%X", nub->space.s.deviceNum,
                     nub->space.s.functionNum );
        else
            sprintf( location, "%X", nub->space.s.deviceNum );
        nub->setLocation( location );
        IODTFindSlotName( nub, nub->space.s.deviceNum );

	// set up config space shadow

	nub->savedConfig = IONew( UInt32, kIOPCIConfigShadowSize );
	if (nub->savedConfig)
	{
	    nub->savedConfig[kIOPCIConfigShadowFlags] = 0;
	    for (int i = 0; i < kIOPCIConfigShadowRegs; i++)
		if (!(kIOPCIVolatileRegsMask & (1 << i)))
		    nub->savedConfig[i] = nub->configRead32( i << 2 );
	}

	checkProperties( nub );

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

        ok = nub->attach( this );
        if (ok)
            nub->registerService();
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

bool IOPCIBridge::checkCardBusNumbering(OSArray * children)
{
    IOPCIDevice * child;
    int childIndex;
    UInt8 parentSecBus, parentSubBus;
    UInt8 priBus, secBus, subBus;
    UInt8 theBusIDs[256];

    OSArray * yentas = OSArray::withCapacity(0x10);
    OSArray * pciBridges = OSArray::withCapacity(0x10);
    if (!yentas || !pciBridges) goto error;

    // clear out the bus id we will be checking
    parentSecBus = firstBusNum();
    parentSubBus = lastBusNum();
    for (int i = parentSecBus; i <= parentSubBus; theBusIDs[i] = 0, i++);

    childIndex = 0;
    while (child = OSDynamicCast(IOPCIDevice, children->getObject(childIndex++))) {

    	UInt8 headerType = child->configRead8(kIOPCIConfigHeaderType);

	// we only care about bridge chips
	if ( ((headerType & 0x7) != 0x1) && ((headerType & 0x7) != 0x2) ) {
	    continue;
	}

	// is there room for an subordinate pci bridge? we only need to
	// check for this if we find a child bridge
	if (parentSecBus == parentSubBus) {
	    IOLog("%s: bad bridge bus numbering, no room to fix, bailing out!\n", getName());
	    goto error;
	}
		
	priBus = child->configRead8(kPCI2PCIPrimaryBus);
	secBus = child->configRead8(kPCI2PCISecondaryBus);
	subBus = child->configRead8(kPCI2PCISubordinateBus);

	UInt8 device = child->getDeviceNumber();
	UInt8 function = child->getFunctionNumber();
	if (4 & gIOPCIDebug) {
	    IOLog("DEBUG name = %s, hdr = 0x%x, parent [%d - %d], device(%d, %d) pri %d [%d - %d]\n", 
		  child->getName(), headerType, parentSecBus, parentSubBus,
		  device, function, priBus, secBus, subBus);
	}

	if ((headerType & 0x7) == 0x2) {	// we only tweek the yenta bridges

	    // keep track the yenta bridges for later
	    yentas->setObject(child);

	    // check cardbus settings,  parentSec = pri < sec <= sub <= parentSub
		    
	    if (priBus != parentSecBus) {
		if (4 & gIOPCIDebug)
		    IOLog("DEBUG fixing bad primary bus setting on %s device(%d, %d) - was %d now %d\n",
			  child->getName(), device, function, priBus, parentSecBus);
		priBus = parentSecBus;
		child->configWrite8(kPCI2PCIPrimaryBus, priBus);
	    }
	    if (subBus > parentSubBus) {
		if (4 & gIOPCIDebug)
		    IOLog("DEBUG bad subordinate bus setting (subBus > parentSubBus) - was %d now %d\n",
			  subBus, parentSubBus);

		// try to squeeze it in
		subBus = parentSubBus;
		child->configWrite8(kPCI2PCISubordinateBus, subBus);
	    }
	    if (subBus <= parentSecBus) {
		if (4 & gIOPCIDebug)
		    IOLog("DEBUG bad subordinate bus setting (subBus <= parentSecBus) - was %d now %d\n",
			  subBus, parentSecBus + 1);

		// try to squeeze it in
		subBus = parentSecBus + 1;
		child->configWrite8(kPCI2PCISubordinateBus, subBus);
	    }

	    // at this point we should have reasonable pri and sub settings
	    // although range may be too small to contain what is attached
		    
	    if (secBus <= priBus) {
		if (4 & gIOPCIDebug)
		    IOLog("DEBUG bad secondary bus setting (secBus <= priBus) - was %d now %d\n",
			  secBus, priBus + 1);

		// try to bump it up one
		secBus = priBus +1;
		child->configWrite8(kPCI2PCISecondaryBus, secBus);
	    }
	    if (secBus > subBus) {
		if (4 & gIOPCIDebug)
		    IOLog("DEBUG bad secondary bus setting (secBus > subBus) - was %d now %d\n",
			  secBus, subBus);

		// try to set its sub
		secBus = subBus;
		child->configWrite8(kPCI2PCISecondaryBus, secBus);
	    }

	} else {	// not a yenta bridge
        
        pciBridges->setObject(child);

	    for (int i = secBus; i <= subBus; i++) {

		// check for overlaps with other bridge ranges on the same bus
		// on the first pass skipping yenta bridges since they may also be
		// claiming to be using bus ids that also belong to other bridges.

		if (theBusIDs[i]) {

		    IOLog("%s: bogus pci bridge config, bus id %d is being used twice, hdr type = 0x%x\n",
			  getName(), i, headerType);
		    goto error;

		} else {
		    
		    theBusIDs[i] = 1;
		}
	    }
	}
    }

#ifdef __i386__
    // Fill the void between PCI-PCI bridges by expanding their 
    // subordinate bus number. This attempts to propagate a larger
    // bus number range to the cardbus controllers.

    if ( yentas->getCount() == 0 && pciBridges->getCount() )
    {
        childIndex = 0;
        while (child = (IOPCIDevice *)pciBridges->getObject(childIndex++))
        {
            UInt8 newSubBus = subBus = child->configRead8( kPCI2PCISubordinateBus );

            for (int j = subBus + 1; j <= parentSubBus && theBusIDs[j] == 0; j++)
            {
                newSubBus = j;
                theBusIDs[j] = 1;
            }

            if ( newSubBus != subBus )
                child->configWrite8( kPCI2PCISubordinateBus, newSubBus );
        }
    }
#endif

    // on the second pass, we know what what has been claimed by the
    // the other busses, if what is set fits then just use it, else
    // try to give a range of one bus to use.

    childIndex = 0;
    while (child = (IOPCIDevice *)yentas->getObject(childIndex++)) {
	
	secBus = child->configRead8(kPCI2PCISecondaryBus);
	subBus = child->configRead8(kPCI2PCISubordinateBus);
    
	for (int j = secBus; j <= subBus; j++) {

	    if (theBusIDs[j]) {
		if (4 & gIOPCIDebug)
		    IOLog("DEBUG yenta bus id %d is being used twice\n", j);

		// back out bad bus ids, except this one
		for (int k = secBus; k < j; k++) theBusIDs[k] = 0;

		// find a free bus id
		int free = 0;
		for (int k = parentSecBus + 1; k <= parentSubBus; k++) {
		    if (theBusIDs[k] == 0) {
			free = k;
			break;
		    }
		}
			    
		if (free) {
		    if (4 & gIOPCIDebug)
			IOLog("DEBUG found free bus range at %d\n", free);

		    // set it up
		    child->configWrite8(kPCI2PCISecondaryBus, free);
		    child->configWrite8(kPCI2PCISubordinateBus, free);
				
		    // take it out of the array
		    theBusIDs[free] = 1;
		} else {
		    // it looks like we have already given too much
		    // another yenta bridge?

		    // we could zero out all yentas, and recurse on ourselves?
		    
		    // bail out for now on this bridge (for now)
		    child->configWrite8(kPCI2PCISecondaryBus, 0);
		    child->configWrite8(kPCI2PCISubordinateBus, 0);
		}

		break;

	    } else {

		theBusIDs[j] = 1;
	    }
	}
    }

    if (yentas) yentas->release();
    if (pciBridges) pciBridges->release();
    return false;
    
 error:
    if (yentas) yentas->release();
    if (pciBridges) pciBridges->release();
    return true;
}

void IOPCIBridge::checkCardBusResources( const OSArray * nubs,
                                         UInt32 * yentaIndices, UInt32 yentaCount )
{
    IOPCIDevice * yenta;
    UInt32        index;
    UInt32        socketBase;

    if (!yentaCount || !cardBusMemoryRanges) return;

    // Allocate 4K bytes for socket registers on each cardbus bridge
    if (cardBusMemoryRanges->allocate(4096 * yentaCount, &socketBase, 4096) == false)
    {
        IOLog("%s: cardbus memory allocation failed\n", getName());
        return;
    }

    // Create "ranges" property
    OSData * ranges = OSData::withCapacity( 5 * sizeof(UInt32) * 2 );
    if (ranges)
    {
        IOPCIAddressSpace space;
        IOPhysicalAddress start;

        for ( IOByteCount size = 8192 * 1024; size >= 4096; size >>= 1 )
        {
            if (cardBusMemoryRanges->allocate( size, &start, 4096 ))
            {
                space.s.space = kIOPCI32BitMemorySpace;
                ranges->appendBytes( &space, sizeof(space) );
                ranges->appendBytes( &start, sizeof(start) );
                ranges->appendBytes( &space, sizeof(space) );
                ranges->appendBytes( &start, sizeof(start) );
                ranges->appendBytes( &size,  sizeof(size)  );
                if (1 & gIOPCIDebug)
                    IOLog("%s: cardbus memory range %ld bytes @ 0x%08lx\n",
                          getName(), size, start);
                break;
            }
        }

        for ( IOByteCount size = 8 * 1024; size >= 32; size >>= 1 )
        {
            if (bridgeIORanges->allocate( size, &start, 0 ))
            {
                space.s.space = kIOPCIIOSpace;
                ranges->appendBytes( &space, sizeof(space) );
                ranges->appendBytes( &start, sizeof(start) );
                ranges->appendBytes( &space, sizeof(space) );
                ranges->appendBytes( &start, sizeof(start) );
                ranges->appendBytes( &size,  sizeof(size)  );
                if (1 & gIOPCIDebug)
                    IOLog("%s: cardbus I/O range %ld bytes at 0x%08lx\n",
                          getName(), size, start);
                break;
            }
        }
    }

    for ( UInt32 i = 0; i < yentaCount; i++ )
    {
        index = yentaIndices[i];  // 1-based index
        yenta = (IOPCIDevice *) nubs->getObject(index - 1);
        assert(yenta);

        yenta->setMemoryEnable( false );
        yenta->configWrite32( kIOPCIConfigBaseAddress0, socketBase );
        socketBase += 4096;

        if (ranges && 0==i) yenta->setProperty( "ranges", ranges );
        publishNub( yenta, index );
    }
}

void IOPCIBridge::probeBus( IOService * provider, UInt8 busNum )
{
    IORegistryEntry *	found;
    OSDictionary *	propTable;
    IOPCIDevice *	nub = 0;
    IOPCIAddressSpace	space;
    UInt32		vendor;
    UInt8		scanDevice, scanFunction, lastFunction;
    OSIterator *	kidsIter;
    UInt32		index = 0;
    UInt32		yentaIndices[8];
    UInt32		yentaCount = 0;
    UInt32		i = 0;

    IODTSetResolving(provider, &compareAddressCell, &nvLocation);

    if (2 & gIOPCIDebug)
        kidsIter = 0;
    else
        kidsIter = provider->getChildIterator( gIODTPlane );

    // find and copy over any devices from the OF device tree
    OSArray * nubs = OSArray::withCapacity(0x10);
    assert(nubs);

    if (kidsIter) {
	kidsIter->reset();
	while ((found = (IORegistryEntry *) kidsIter->getNextObject()))
	{
	    propTable = found->getPropertyTable();
	    nub = createNub( propTable );
	    if ( nub && initializeNub(nub, propTable) &&
             nub->init(found, gIODTPlane) )
        {
#ifdef __i386__
            spaceFromProperties( propTable, &space );
            if ((propTable = constructProperties(space)))
            {
                OSCollectionIterator * propIter =
                    OSCollectionIterator::withCollection(propTable);
                if (propIter)
                {
                    const OSSymbol * propKey;
                    while ((propKey = (const OSSymbol *)propIter->getNextObject()))
                    {
                        if ( 0 == nub->getProperty(propKey))
                            nub->setProperty(propKey, propTable->getObject(propKey));
                    }
                    propIter->release();
                }
            	propTable->release();
            }
#endif

            nubs->setObject(index++, nub);
        }
	}
    }

    // scan the pci bus for "additional" devices
    space.bits = 0;
    space.s.busNum = busNum;

    for (scanDevice = 0; scanDevice <= 31; scanDevice++)
    {
        lastFunction = 0;
        for (scanFunction = 0; scanFunction <= lastFunction; scanFunction++)
        {
            space.s.deviceNum = scanDevice;
            space.s.functionNum = scanFunction;
            
            if (findMatching(kidsIter, space) == 0)
            {
                /* probe - should guard exceptions */
#ifdef __ppc__
                // DEC bridge really needs safe probe
                continue;
#endif
                vendor = configRead32( space, kIOPCIConfigVendorID );
                vendor &= 0x0000ffff;
                if ((0 == vendor) || (0xffff == vendor))
                    continue;

                propTable = constructProperties( space );

                if (propTable)
                {
                    if ((nub = createNub(propTable))
                    && (initializeNub(nub, propTable))
                    && (nub->init(propTable)))
                    {
                        nubs->setObject(index++, nub);
                    }
                    propTable->release();
                }
            }

            // look in function 0 for multi function flag
            if ((0 == scanFunction)
                    && (0x00800000 & configRead32(space,
                                                  kIOPCIConfigCacheLineSize)))
                lastFunction = 7;
        }
    }

    checkCardBusNumbering(nubs);

#ifdef __i386__
    // probe all device BAR's before publishing nubs
    i = 0;
    while (nub = (IOPCIDevice *)nubs->getObject(i++)) {
        resolvePCIInterrupt( provider, nub );
        if ((nub->configRead8(kIOPCIConfigHeaderType) & 0x7) == 0x2)
        {
            if (yentaCount < 8) yentaIndices[yentaCount++] = i;
                continue;  // skip cardbus bridges
        }
        getNubResources( nub );
    }
#endif

    i = 0;
    while (nub = (IOPCIDevice *)nubs->getObject(i++)) {
        publishNub(nub , i);
        if (1 & gIOPCIDebug)
            IOLog("%08lx = 0:%08lx 4:%08lx  ", nub->space.bits,
                nub->configRead32(kIOPCIConfigVendorID),
                nub->configRead32(kIOPCIConfigCommand) );
    }

    checkCardBusResources( nubs, yentaIndices, yentaCount );

    nubs->release();
    if (kidsIter) kidsIter->release();
}

bool IOPCIBridge::addBridgeMemoryRange( IOPhysicalAddress start,
                                        IOPhysicalLength length, bool host )
{
    if (cardBusMemoryRanges)
        cardBusMemoryRanges->deallocate( start, length );

    return addBridgePrefetchableMemoryRange( start, length, host );
}

bool IOPCIBridge::addBridgePrefetchableMemoryRange( IOPhysicalAddress start,
                                                    IOPhysicalLength length,
                                                    bool host )
{
    IORangeAllocator *	platformRanges;
    bool		ok = true;

    if (host)
    {
        platformRanges = getPlatform()->getPhysicalRangeAllocator();
        assert( platformRanges );

        // out of the platform
        ok = platformRanges->allocateRange( start, length );
        if (!ok)
            kprintf("%s: didn't get host range (%08lx:%08lx)\n", getName(),
                    start, length);
    }

    // and into the bridge
    bridgeMemoryRanges->deallocate( start, length );

    return (ok);
}

bool IOPCIBridge::addBridgeIORange( IOByteCount start, IOByteCount length )
{
    bool	ok = true;

    // into the bridge
    bridgeIORanges->deallocate( start, length );

    return (ok);
}

bool IOPCIBridge::constructRange( IOPCIAddressSpace * flags,
                                  IOPhysicalAddress phys,
                                  IOPhysicalLength len,
                                  OSArray * array )
{
    IODeviceMemory *	range;
    IODeviceMemory *	ioMemory;
    IORangeAllocator *	bridgeRanges;
    bool		ok;

    if (!array)
        return (false);

    if (kIOPCIIOSpace == flags->s.space)
    {
        bridgeRanges = bridgeIORanges;
        if ((ioMemory = ioDeviceMemory()))
        {
            phys &= 0x00ffffff;	// seems bogus
            range = IODeviceMemory::withSubRange( ioMemory, phys, len );
            if (range == 0)
                /* didn't fit */
                range = IODeviceMemory::withRange(
                            phys + ioMemory->getPhysicalAddress(), len );
        }
        else
            range = 0;
    }
    else
    {
        bridgeRanges = bridgeMemoryRanges;
        range = IODeviceMemory::withRange( phys, len );
    }

    if (range)
    {
        ok = bridgeRanges->allocateRange( phys, len );
#ifdef __ppc__
        if (!ok)
            IOLog("%s: bad range %d(%08lx:%08lx)\n", getName(), flags->s.space,
                  phys, len);
#endif

        if (cardBusMemoryRanges && (bridgeRanges == bridgeMemoryRanges))
            cardBusMemoryRanges->allocateRange( phys, len );

        range->setTag( flags->bits );
        ok = array->setObject( range );
        range->release();
    }
    else
        ok = false;

    return (ok);
}


IOReturn IOPCIBridge::getDTNubAddressing( IOPCIDevice * regEntry )
{
    OSArray *		array;
    IORegistryEntry *	parentEntry;
    OSData *		addressProperty;
    IOPhysicalAddress	phys;
    IOPhysicalLength	len;
    UInt32		cells = 5;
    int			i, num;
    UInt32 *		reg;

    addressProperty = (OSData *) regEntry->getProperty( "assigned-addresses" );
    if (0 == addressProperty)
        return (kIOReturnSuccess);

    parentEntry = regEntry->getParentEntry( gIODTPlane );
    if (0 == parentEntry)
        return (kIOReturnBadArgument);

    array = OSArray::withCapacity( 1 );
    if (0 == array)
        return (kIOReturnNoMemory);

    reg = (UInt32 *) addressProperty->getBytesNoCopy();
    num = addressProperty->getLength() / (4 * cells);

    for (i = 0; i < num; i++)
    {
        if (IODTResolveAddressCell(parentEntry, reg, &phys, &len))

            constructRange( (IOPCIAddressSpace *) reg, phys, len, array );

        reg += cells;
    }

    if (array->getCount())
        regEntry->setProperty( gIODeviceMemoryKey, array);

    array->release();

    return (kIOReturnSuccess);
}

struct SafeProbeParam {
    IOPCIDevice *   nub;
    UInt8           regNum;
    UInt32          value;
    UInt32          save;
};

static void probeBAR( void * refcon )
{
    SafeProbeParam *    param = (SafeProbeParam *)refcon;
    IOPCIDevice *       nub;

    nub = param->nub;
    param->save = nub->configRead32( param->regNum );
    nub->configWrite32( param->regNum, 0xffffffff );
    param->value = nub->configRead32( param->regNum );
    nub->configWrite32( param->regNum, param->save );
}

#ifdef __i386__
static void safeProbeBAR( void * refcon )
{
    assert(refcon);
    if (cpu_number() == 0)
        probeBAR(refcon);
}
#else /* !__i386__ */
static void safeProbeBAR( void * refcon )
{
    SafeProbeParam *    param = (SafeProbeParam *)refcon;
    IOPCIDevice *       nub;
    bool                memEna, ioEna;
    boolean_t           s;

    nub = param->nub;
    s = ml_set_interrupts_enabled(FALSE);
    memEna = nub->setMemoryEnable( false );
    ioEna = nub->setIOEnable( false );

    probeBAR(refcon);

    nub->setMemoryEnable( memEna );
    nub->setIOEnable( ioEna );
    ml_set_interrupts_enabled( s );
}
#endif /* !__i386__ */

IOReturn IOPCIBridge::getNubAddressing( IOPCIDevice * nub )
{
    OSArray *		array;
    OSData *		assignedProp;
    IOPhysicalAddress	phys;
    IOPhysicalLength	len;
    UInt32		save, value;
    IOPCIAddressSpace	reg;
    UInt8		regNum;
    UInt8       headerType;
    SafeProbeParam	probeParam;

    value = nub->configRead32( kIOPCIConfigRevisionID );
    if ((value >> 8) == 0x060000)	// skip host bridge aliases
        return (kIOReturnSuccess);

    value = nub->configRead32( kIOPCIConfigVendorID );
    if (0x0003106b == value)		// control doesn't play well
        return (kIOReturnSuccess);

    // headers type 0 and 2
    headerType = nub->configRead8( kIOPCIConfigHeaderType ) & 0x7f;
    if (headerType != 0 && headerType != 2)
        return (kIOReturnSuccess);

    array = OSArray::withCapacity( 1 );
    if (0 == array)
        return (kIOReturnNoMemory);
    assignedProp = OSData::withCapacity( 3 * sizeof(IOPCIPhysicalAddress) );
    if (0 == assignedProp)
        return (kIOReturnNoMemory);

    for (regNum = 0x10; regNum < 0x28; regNum += 4)
    {
        // Only look at CardBus socket BAR
        if ( (2 == headerType) && (regNum > 0x10) )
            break;

		// begin scary
        probeParam.nub = nub;
        probeParam.regNum = regNum;
#ifdef __i386__
        mp_rendezvous_no_intrs(&safeProbeBAR, &probeParam);
#else
        safeProbeBAR(&probeParam);
#endif
        value = probeParam.value;
        save = probeParam.save;
        // end scary

        if (0 == value)
            continue;

        reg = nub->space;
        reg.s.registerNum = regNum;

        if (value & 1)
        {
            reg.s.space = kIOPCIIOSpace;

            // If the upper 16 bits for I/O space
            // are all 0, then we should ignore them.
            if ((value & 0xFFFF0000) == 0)
            {
                value = value | 0xFFFF0000;
            }
        }
        else
        {
            reg.s.prefetch = (0 != (value & 8));

            switch (value & 6)
            {
                case 2: /* below 1Mb */
                    reg.s.t = 1;
                    /* fall thru */
                case 0: /* 32-bit mem */
                case 6:	/* reserved */
                    reg.s.space = kIOPCI32BitMemorySpace;
                    break;

                case 4: /* 64-bit mem */
                    reg.s.space = kIOPCI64BitMemorySpace;
                    regNum += 4;
                    break;
            }
        }

        value &= 0xfffffff0;
        phys = IOPhysical32( 0, save & value );
        len = IOPhysical32( 0, -value );

        if (assignedProp)
        {
            IOPCIPhysicalAddress assigned;
            assigned.physHi = reg;
            assigned.physMid = 0;
            assigned.physLo = phys;
            assigned.lengthHi = 0;
            assigned.lengthLo = len;

            assignedProp->appendBytes( &assigned, sizeof(assigned) );
        }

        if (1 & gIOPCIDebug)
            IOLog("Space %08lx : %08lx, %08lx\n", reg.bits, phys, len);

        constructRange( &reg, phys, len, array );
    }

    if (array->getCount())
        nub->setProperty( gIODeviceMemoryKey, array);
    array->release();

    if (assignedProp->getLength())
        nub->setProperty( "assigned-addresses", assignedProp );
    assignedProp->release();

    return (kIOReturnSuccess);
}

bool IOPCIBridge::isDTNub( IOPCIDevice * nub )
{
    return (0 != nub->getParentEntry(gIODTPlane));
}

IOReturn IOPCIBridge::getNubResources( IOService * service )
{
    IOPCIDevice *	nub = (IOPCIDevice *) service;
    IOReturn		err;

    if (service->getDeviceMemory())
        return (kIOReturnSuccess);

#ifdef __ppc__
    if (isDTNub(nub))
        err = getDTNubAddressing( nub );
    else
#endif
        err = getNubAddressing( nub );

    return (err);
}

bool IOPCIBridge::matchKeys( IOPCIDevice * nub, const char * keys,
                             UInt32 defaultMask, UInt8 regNum )
{
    const char *	next;
    UInt32		mask, value, reg;
    bool		found = false;

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
    OSString *		prop;
    const char *	keys;
    bool		match = true;
    UInt8		regNum;
    int			i;

    struct IOPCIMatchingKeys
    {
        const char *	propName;
        UInt8		regs[ 4 ];
        UInt32		defaultMask;
    };
    const IOPCIMatchingKeys *		   look;
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
    bool	matches;

    matches = pciMatchNub( (IOPCIDevice *) nub, table, score);

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
    UInt32	data = 0;
    UInt8	offset;

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
    UInt32	data = 0;
    IOByteCount	offset, firstOffset = 0;

    if (found)
    {
	firstOffset = *found;
        *found = 0;
    }

    if (0 == ((kIOPCIStatusCapabilities << 16)
              & (configRead32(space, kIOPCIConfigCommand))))
        return (0);

    if (capabilityID >= 0x100)
    {
	capabilityID =- capabilityID;
	offset = 0x100;
	while (offset)
	{
	    space.es.registerNumExtended = (offset >> 8);
	    data = configRead32( space, offset );
	    if ((offset > firstOffset) && (capabilityID == (data & 0xffff)))
	    {
		if (found)
		    *found = offset;
		break;
	    }
	    offset = (data >> 20) & 0xfff;
	    if ((offset < 0x100) || (offset & 3))
		offset = 0;
	}
    }
    else
    {
	offset = (0xff & configRead32(space, kIOPCIConfigCapabilitiesPtr));
	if (offset & 3)
	    offset = 0;
	while (offset)
	{
	    data = configRead32( space, offset );
	    if ((offset > firstOffset) && (capabilityID == (data & 0xff)))
	    {
		if (found)
		    *found = offset;
		break;
	    }
	    offset = (data >> 8) & 0xff;
	    if (offset & 3)
		offset = 0;
	}
    }

    return (offset ? data : 0);
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

IOService * IOPCI2PCIBridge::probe( IOService * 	provider,
                                    SInt32 *		score )
{
    if (0 == (bridgeDevice = OSDynamicCast(IOPCIDevice, provider)))
        return (0);

    *score 		-= 100;

    return (this);
}

bool IOPCI2PCIBridge::configure( IOService * provider )
{
    UInt32	end;
    UInt32	start;
    bool 	ok;

    end = bridgeDevice->configRead32( kPCI2PCIMemoryRange );
    if (end)
    {
        start = (end & 0xfff0) << 16;
        end |= 0x000fffff;
        ok = addBridgeMemoryRange( start, end - start + 1, false );
    }

    end = bridgeDevice->configRead32( kPCI2PCIPrefetchMemoryRange );
    if (end)
    {
        start = (end & 0xfff0) << 16;
        end |= 0x000fffff;
        ok = addBridgePrefetchableMemoryRange( start, end - start + 1, false );
    }

    end = bridgeDevice->configRead32( kPCI2PCIIORange );
    if (end)
    {
        start = (end & 0xf0) << 8;
        end = (end & 0xffff) | 0xfff;
        ok = addBridgeIORange( start, end - start + 1 );
    }

    saveBridgeState();
    if (bridgeDevice->savedConfig)
        bridgeDevice->savedConfig[kIOPCIConfigShadowFlags] |= kIOPCIConfigShadowBridge;

    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

    UInt32 i;
    for (i = 0;
	 gIOAllPCI2PCIBridges[i] && (i < kIOPCIMaxPCI2PCIBridges);
	 i++)	{}

    if (i < kIOPCIMaxPCI2PCIBridges)
	gIOAllPCI2PCIBridges[i] = this;

    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

    return (super::configure(provider));
}

void IOPCI2PCIBridge::free()
{
    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

    UInt32 i;
    for (i = 0;
	 (this != gIOAllPCI2PCIBridges[i]) && (i < kIOPCIMaxPCI2PCIBridges);
	 i++)	{}

    if (i < kIOPCIMaxPCI2PCIBridges)
	gIOAllPCI2PCIBridges[i] = 0;

    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

    super::free();
}

void IOPCI2PCIBridge::saveBridgeState( void )
{
    long cnt;

    for (cnt = 0; cnt < kIOPCIBridgeRegs; cnt++)
    {
        bridgeState[cnt] = bridgeDevice->configRead32(cnt * 4);
    }
}

void IOPCI2PCIBridge::restoreBridgeState( void )
{
  long cnt;
  
    // start at config space location 8 -- bytes 0-3 are
    // defined by the PCI Spec. as ReadOnly, and we don't
    // want to write anything to the Command or Status
    // registers until the rest of config space is set up.

    for (cnt = (kIOPCIConfigCommand >> 2) + 1; cnt < kIOPCIBridgeRegs; cnt++)
    {
        bridgeDevice->configWrite32(cnt * 4, bridgeState[cnt]);
    }

    // once the rest of the config space is restored,
    // turn on all the enables (,etc.) in the Command register.
    // NOTE - we also reset any status bits in the Status register
    // that may have been previously indicated by writing a '1'
    // to the bits indicating whatever they were indicating.

    bridgeDevice->configWrite32(kIOPCIConfigCommand,
				bridgeState[kIOPCIConfigCommand >> 2]);
}

UInt8 IOPCI2PCIBridge::firstBusNum( void )
{
    return bridgeDevice->configRead8( kPCI2PCISecondaryBus );
}

UInt8 IOPCI2PCIBridge::lastBusNum( void )
{
    return bridgeDevice->configRead8( kPCI2PCISubordinateBus );
}

IOPCIAddressSpace IOPCI2PCIBridge::getBridgeSpace( void )
{
    return (bridgeDevice->space);
}

UInt32 IOPCI2PCIBridge::configRead32( IOPCIAddressSpace space,
                                      UInt8 offset )
{
    return (bridgeDevice->configRead32(space, offset));
}

void IOPCI2PCIBridge::configWrite32( IOPCIAddressSpace space,
                                     UInt8 offset, UInt32 data )
{
    bridgeDevice->configWrite32( space, offset, data );
}

UInt16 IOPCI2PCIBridge::configRead16( IOPCIAddressSpace space,
                                      UInt8 offset )
{
    return (bridgeDevice->configRead16(space, offset));
}

void IOPCI2PCIBridge::configWrite16( IOPCIAddressSpace space,
                                     UInt8 offset, UInt16 data )
{
    bridgeDevice->configWrite16( space, offset, data );
}

UInt8 IOPCI2PCIBridge::configRead8( IOPCIAddressSpace space,
                                    UInt8 offset )
{
    return (bridgeDevice->configRead8(space, offset));
}

void IOPCI2PCIBridge::configWrite8( IOPCIAddressSpace space,
                                    UInt8 offset, UInt8 data )
{
    bridgeDevice->configWrite8( space, offset, data );
}

IODeviceMemory * IOPCI2PCIBridge::ioDeviceMemory( void )
{
    return (bridgeDevice->ioDeviceMemory());
}

bool IOPCI2PCIBridge::publishNub( IOPCIDevice * nub, UInt32 index )
{
    if (nub)
        nub->setProperty( "IOChildIndex" , index, 32 );

    return (super::publishNub(nub, index));
}

#ifdef __i386__

static void resolvePCIInterrupt( IOService * provider, IOPCIDevice * nub )
{
    UInt32 pin;
    UInt32 irq = 0;

    pin = (UInt8)(nub->configRead32( kIOPCIConfigInterruptLine ) >> 8);
    if ( pin == 0 || pin > 4 )
        return;  // assume no interrupt usage

    pin--;  // make pin zero based, INTA=0, INTB=1, INTC=2, INTD=3

    // Ask the platform driver to resolve the PCI interrupt route,
    // and return its corresponding system interrupt vector.

    if ( provider->callPlatformFunction( "ResolvePCIInterrupt",
                   /* waitForFunction */ false,
                   /* provider nub    */ provider,
                   /* device number   */ (void *) nub->space.s.deviceNum,
                   /* interrupt pin   */ (void *) pin,
                   /* resolved IRQ    */ &irq ) == kIOReturnSuccess )
    {
        if (1 & gIOPCIDebug)
            IOLog("%s: Resolved interrupt %ld (%d) for %s\n",
                  provider->getName(),
                  irq, nub->configRead8(kIOPCIConfigInterruptLine),
                  nub->getName());
        
        nub->configWrite8( kIOPCIConfigInterruptLine, irq & 0xff );
    }
    else
    {
        irq = nub->configRead8( kIOPCIConfigInterruptLine );
        if ( 0 == irq || 0xff == irq ) return;
        irq &= 0xf;  // what about IO-APIC and irq > 15?
    }

    provider->callPlatformFunction( "SetDeviceInterrupts",
              /* waitForFunction */ false,
              /* nub             */ nub, 
              /* vectors         */ (void *) &irq,
              /* vectorCount     */ (void *) 1,
              /* exclusive       */ (void *) false );
}

#endif /* __i386__ */
