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

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>
#include <IOKit/assert.h>
#include <IOKit/IOCatalogue.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include <libkern/c++/OSContainers.h>
#include <libkern/version.h>

extern "C"
{
#include <machine/machine_routines.h>
};

#if 0

#define LOG(fmt, args...)  	\
    do {     kprintf(fmt, ## args); IOLog(fmt, ## args);	} while (false);

#else
#define LOG(fmt, args...)
#endif

#ifndef VERSION_MAJOR
#error VERSION_MAJOR
#endif

#if	    VERSION_MAJOR < 10
#define	    ROM_KEXTS	    1
#endif

#define kMSIFreeCountKey    "MSIFree"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

const IORegistryPlane * gIOPCIACPIPlane;

static class IOPCIMessagedInterruptController  * gIOPCIMessagedInterruptController;

enum
{
    kMSIX       = 0x01
};

enum
{
    kIOPCIClassBridge = 0x06
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef kBaseVectorNumberKey
#define kBaseVectorNumberKey          "Base Vector Number"
#endif

#ifndef kInterruptControllerNameKey
#define kVectorCountKey		      "Vector Count"
#endif

#ifndef kInterruptControllerNameKey
#define kInterruptControllerNameKey   "InterruptControllerName"
#endif

class IOPCIMessagedInterruptController : public IOInterruptController
{
    OSDeclareDefaultStructors( IOPCIMessagedInterruptController )

protected:

    // The base global system interrupt number.

    SInt32                  _vectorBase;
    UInt32                  _vectorCount;

    IORangeAllocator *      _messagedInterruptsAllocator;

public:
    bool init( UInt32 numVectors );

    virtual IOReturn registerInterrupt( IOService *        nub,
                                        int                source,
                                        void *             target,
                                        IOInterruptHandler handler,
                                        void *             refCon );

    virtual IOReturn unregisterInterrupt( IOService *	   nub,
					int                source);

    virtual void     initVector( IOInterruptVectorNumber vectorNumber,
                                 IOInterruptVector * vector );

    virtual int      getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);

    virtual bool     vectorCanBeShared( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual void     enableVector( IOInterruptVectorNumber vectorNumber,
                                   IOInterruptVector * vector );

    virtual void     disableVectorHard( IOInterruptVectorNumber vectorNumber,
                                        IOInterruptVector * vector );

    virtual IOReturn handleInterrupt( void * savedState,
                                      IOService * nub,
                                      int source );

//
    bool addDeviceInterruptProperties(
				    IORegistryEntry * device,
				    UInt32            controllerIndex,
				    UInt32            interruptFlags,
				    SInt32 *          deviceIndex);

    IOReturn allocateDeviceInterrupts( IOPCIBridge * bridge,
		    IOPCIDevice * device, UInt32 numVectors);
    IOReturn deallocateDeviceInterrupts(
		    IOPCIBridge * bridge, IOPCIDevice * device);

};

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors( IOPCIMessagedInterruptController, 
                               IOInterruptController )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOPCIMessagedInterruptController::init( UInt32 numVectors )
{
    OSNumber * num;
    const OSSymbol * sym = 0;

    if (!super::init())
	return (false);

    _vectorCount = numVectors;
    setProperty(kVectorCountKey, _vectorCount, 32);

    // Allocate the memory for the vectors shared with the superclass.

    vectors = IONew( IOInterruptVector, _vectorCount );
    if ( 0 == vectors )
        goto fail;

    bzero( vectors, sizeof(IOInterruptVector) * _vectorCount );

    // Allocate locks for the vectors.

    for (uint32_t i = 0; i < _vectorCount; i++)
    {
        vectors[i].interruptLock = IOLockAlloc();
        if ( vectors[i].interruptLock == 0 )
            goto fail;
    }

    attach(getPlatform());
    sym = copyName();
    setProperty(kInterruptControllerNameKey, (OSObject *) sym);
    getPlatform()->registerInterruptController( (OSSymbol *) sym, this );
    sym->release();

    num = OSDynamicCast( OSNumber,
                         getProperty( kBaseVectorNumberKey ) );
    if ( num ) _vectorBase = num->unsigned32BitValue();

    _messagedInterruptsAllocator = IORangeAllocator::withRange(0, 0, 4, IORangeAllocator::kLocking);
    _messagedInterruptsAllocator->deallocate(_vectorBase, _vectorCount);
    setProperty(kMSIFreeCountKey, _messagedInterruptsAllocator->getFreeCount(), 32);

    registerService();

    LOG("%s: start success\n", getName());
    return (true);

fail:
    return false;
}

bool IOPCIMessagedInterruptController::addDeviceInterruptProperties(
				IORegistryEntry * device,
                                UInt32            controllerIndex,
                                UInt32            interruptFlags,
                                SInt32 *          deviceIndex)
{
    OSArray *        controllers;
    OSArray *        specifiers;
    OSArray *        liveCtrls;
    OSArray *        liveSpecs;
    const OSSymbol * symName;
    OSData *         specData;
    bool             success = false;

    if (!device)
        return false;

    liveCtrls = OSDynamicCast(OSArray,
        device->getProperty(gIOInterruptControllersKey));

    liveSpecs = OSDynamicCast(OSArray,
        device->getProperty(gIOInterruptSpecifiersKey));

    if (liveCtrls && liveSpecs)
    {
        // reserve space for new interrupt vector
        controllers = OSArray::withArray(liveCtrls, liveCtrls->getCount() + 1);
        specifiers  = OSArray::withArray(liveSpecs, liveSpecs->getCount() + 1);
    }
    else
    {
        controllers = OSArray::withCapacity(1);
        specifiers  = OSArray::withCapacity(1);
    }

    specData = OSData::withCapacity(2 * sizeof(UInt32));
    symName = copyName();

    if (!controllers || !specifiers || !specData || !symName)
	return (false);

    // Specifier data will be 64-bits long, containing:
    //    data[0] = interrupt number
    //    data[1] = interrupt flags
    // This must agree with interrupt controller drivers.
    //
    // << Warning >>
    // IOInterruptController::registerInterrupt() assumes that
    // the vectorNumber is the first long in the specifier.

    specData->appendBytes(&controllerIndex, sizeof(controllerIndex));
    specData->appendBytes(&interruptFlags,  sizeof(interruptFlags));

    if (deviceIndex)
	*deviceIndex = specifiers->getCount() - 1;

    success = specifiers->setObject(specData)
		&& controllers->setObject(symName);

    if (success)
    {
	device->setProperty(gIOInterruptControllersKey, controllers);
	device->setProperty(gIOInterruptSpecifiersKey, specifiers);
    }

    specifiers->release();
    controllers->release();
    symName->release();
    specData->release();

    return success;
}

IOReturn IOPCIMessagedInterruptController::allocateDeviceInterrupts(
		IOPCIBridge * bridge, IOPCIDevice * device, UInt32 numVectors)
{
    IOReturn      ret;
    IOByteCount   msi = device->reserved->msiConfig;
    IOByteCount   msiBlockSize;
    uint32_t      vector, firstVector = _vectorBase;
    uint16_t      control = device->configRead16(msi + 2);
    IORangeScalar rangeStart;
    uint32_t      message[3];

    // pci2pci bridges get none
    if ((0x0604 == device->configRead16(kIOPCIConfigClassCode + 1))
	&& !device->getProperty(kIOPCIHotPlugKey)
	&& !device->getProperty(kIOPCILinkChangeKey))
    {
	numVectors = 0;
    }
    if (numVectors)
    {
	if (!_messagedInterruptsAllocator->allocate(numVectors, &rangeStart, numVectors))
	    return (kIOReturnNoSpace);
	setProperty(kMSIFreeCountKey, _messagedInterruptsAllocator->getFreeCount(), 32);
	firstVector = rangeStart;
    }

    ret = bridge->callPlatformFunction( "GetMessagedInterruptAddress",
		  /* waitForFunction */ false,
		  /* nub             */ device, 
		  /* options         */ (void *) 0,
		  /* vector          */ (void *) firstVector,
		  /* message         */ (void *) &message[0]);

    if (kIOReturnSuccess == ret)
    {
	for (vector = firstVector; vector < (firstVector + numVectors); vector++)
	{
	    addDeviceInterruptProperties(device, 
			vector - _vectorBase, 
			kIOInterruptTypeEdge | kIOInterruptTypePCIMessaged, NULL);
	}

	if (kMSIX & device->reserved->msiMode)
	{
	    IOByteCount msiTable;
	    UInt8 bar;
	    IODeviceMemory * memory;
	    IOPhysicalAddress phys;

	    control &= ~(1 << 15);	    // disabled

	    msiBlockSize = 1;   // words

	    msiTable = device->configRead32(msi + 4);
	    bar = kIOPCIConfigBaseAddress0 + ((msiTable & 7) << 2);
	    msiTable &= ~7;

	    memory = device->getDeviceMemoryWithRegister(bar);
	    if (memory && (phys = memory->getPhysicalAddress()))
	    {
		control = device->configRead16(kIOPCIConfigCommand);
		device->configWrite16(kIOPCIConfigCommand, control | 4);

		for (vector = 0; vector < numVectors; vector++)
		{
		    IOMappedWrite32(phys + msiTable + vector * 16 + 0, message[0]);
		    IOMappedWrite32(phys + msiTable + vector * 16 + 4, message[1]);
		    IOMappedWrite32(phys + msiTable + vector * 16 + 8, message[2]);
		    IOMappedWrite32(phys + msiTable + vector * 16 + 0, 0);
		}
		device->configWrite16(kIOPCIConfigCommand, control);
	    }
	}
	else
	{
	    control &= ~1;					// disabled
	    if (numVectors) 
		numVectors = (31 - __builtin_clz(numVectors));	// log2
	    control |= (numVectors << 4);

	    msiBlockSize = 3;   // words
	    if (0x0080 & control)
	    {
		// 64b
		device->configWrite32(msi + 4,  message[0]);
		device->configWrite32(msi + 8,  message[1]);
		device->configWrite16(msi + 12, message[2]);
		device->configWrite16(msi + 2,  control);
		msiBlockSize += 1;
	    }
	    else
	    {
		device->configWrite32(msi + 4,  message[0]);
		device->configWrite16(msi + 8,  message[2]);
		device->configWrite16(msi + 2,  control);
	    }
	    if (0x0100 & control)
		msiBlockSize += 2;
	}

	device->reserved->msiBlockSize = msiBlockSize;
    }

    return (ret);
}

IOReturn IOPCIMessagedInterruptController::deallocateDeviceInterrupts(
		IOPCIBridge * bridge, IOPCIDevice * device)
{
    const OSSymbol * myName;
    OSArray *        controllers;
    OSObject *       controller;
    OSArray *        specs;
    OSData *         spec;
    uint32_t         index = 0;

    myName = copyName();

    controllers = OSDynamicCast(OSArray,
        device->getProperty(gIOInterruptControllersKey));

    specs = OSDynamicCast(OSArray,
        device->getProperty(gIOInterruptSpecifiersKey));

    if (!myName || !controllers || !specs)
	return (kIOReturnBadArgument);

    while( (spec = OSDynamicCast(OSData, specs->getObject(index)))
	&& (controller = controllers->getObject(index)))
    {
	if (controller->isEqualTo(myName))
	{
	    IORangeScalar rangeStart = _vectorBase + *((uint32_t *) spec->getBytesNoCopy());
	    _messagedInterruptsAllocator->deallocate(rangeStart, 1);
	    setProperty(kMSIFreeCountKey, _messagedInterruptsAllocator->getFreeCount(), 32);
	}
	index++;
    }
    myName->release();

    return (kIOReturnSuccess);
}

IOReturn IOPCIMessagedInterruptController::registerInterrupt( 
					IOService *	   nub,
					int                source,
					void *             target,
					IOInterruptHandler handler,
					void *             refCon )
{
    IOReturn      ret;
    IOPCIDevice * device = OSDynamicCast(IOPCIDevice, nub);

    ret = super::registerInterrupt(nub, source, target, handler, refCon);

    if ((kIOReturnSuccess == ret) && device)
    {
	if (!device->reserved->msiEnable)
	{
	    IOByteCount msi = device->reserved->msiConfig;
	    uint16_t control;

	    control = device->configRead16(msi + 2);

	    if (kMSIX & device->reserved->msiMode)
	    {
		control |= (1 << 15);
	    }
	    else
	    {
		control |= 1;
	    }

	    device->configWrite16(msi + 2, control);
	    control = device->configRead16(kIOPCIConfigCommand);
	    control |= (1 << 10) | (1 << 2);
	    device->configWrite16(kIOPCIConfigCommand, control);
	    device->setProperty("IOPCIMSIMode", kOSBooleanTrue);
	}
	device->reserved->msiEnable++;
    }

    return (ret);
}

IOReturn IOPCIMessagedInterruptController::unregisterInterrupt( 
					IOService *	   nub,
					int                source)
{
    IOReturn      ret;
    IOPCIDevice * device = OSDynamicCast(IOPCIDevice, nub);

    ret = super::unregisterInterrupt(nub, source);

    if (device && device->reserved->msiEnable && !(--device->reserved->msiEnable))
    {
	IOByteCount msi = device->reserved->msiConfig;
	uint16_t control;

	control = device->configRead16(msi + 2);
	control &= ~((1 << 15) | (7 << 4) | 1);
	device->configWrite16(msi + 2, control);
	device->removeProperty("IOPCIMSIMode");
    }

    return (ret);
}

IOReturn
IOPCIMessagedInterruptController::handleInterrupt( void *      state,
                                                   IOService * nub,
                                                   int         source)
{
    IOInterruptVector * vector;

    source -= _vectorBase;
    if ((source < 0) || (source > (int) _vectorCount))
	return kIOReturnSuccess;
    
    vector = &vectors[source];

    if (!vector->interruptRegistered)
	return kIOReturnInvalid;

    vector->handler(vector->target, vector->refCon,
		    vector->nub, vector->source);

    return kIOReturnSuccess;
}

bool IOPCIMessagedInterruptController::vectorCanBeShared(IOInterruptVectorNumber vectorNumber,
				       IOInterruptVector * vector)
{
    return (false);
}

void IOPCIMessagedInterruptController::initVector(IOInterruptVectorNumber vectorNumber,
				       IOInterruptVector * vector)
{
}

int IOPCIMessagedInterruptController::getVectorType(IOInterruptVectorNumber vectorNumber,
				       IOInterruptVector * vector)
{
    return (kIOInterruptTypeEdge | kIOInterruptTypePCIMessaged);
}

void IOPCIMessagedInterruptController::disableVectorHard(IOInterruptVectorNumber vectorNumber,
				       IOInterruptVector * vector)
{
}

void IOPCIMessagedInterruptController::enableVector(IOInterruptVectorNumber vectorNumber,
				       IOInterruptVector * vector)
{
}

#undef super

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService
OSDefineMetaClassAndAbstractStructorsWithInit( IOPCIBridge, IOService, IOPCIBridge::initialize() )

OSMetaClassDefineReservedUsed(IOPCIBridge, 0);
OSMetaClassDefineReservedUsed(IOPCIBridge, 1);
OSMetaClassDefineReservedUsed(IOPCIBridge, 2);
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

#ifndef kIOPlatformDeviceMessageKey
#define kIOPlatformDeviceMessageKey     "IOPlatformDeviceMessage"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOSimpleLock *  	 gIOAllPCI2PCIBridgesLock;
UInt32			 gIOAllPCI2PCIBridgeState;

static queue_head_t	 gIOAllPCIDeviceRestoreQ;

static IOLock *		 gIOPCIMessagedInterruptControllerLock;
const OSSymbol *	 gIOPlatformDeviceMessageKey;
const OSSymbol *	 gIOPlatformDeviceASPMEnableKey;
static IOWorkLoop *	 gCommonWorkLoop;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Expansion data fields */

#define fXpressCapability		reserved->xpressCapability
#define fBridgeInterruptSource		reserved->bridgeInterruptSource
#define fWorkLoop			reserved->workLoop
#define fHotplugCount			reserved->hotplugCount
#define fPresence			reserved->presence
#define fWaitingLinkEnable		reserved->waitingLinkEnable
//#define fHotplugFirstBus		reserved->hotplugFirstBus
//#define fHotplugLastBus		reserved->hotplugLastBus
#define fBridgeInterruptEnablePending	reserved->interruptEnablePending
#define fLinkChangeOnly			reserved->linkChangeOnly

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// stub driver has two power states, off and on

enum { kIOPCIBridgePowerStateCount = 3 };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOPCIBridge::initialize(void)
{
    if (!gIOAllPCI2PCIBridgesLock)
    {
	gIOAllPCI2PCIBridgesLock = IOSimpleLockAlloc();
	gIOPCIMessagedInterruptControllerLock = IOLockAlloc();
	queue_init(&gIOAllPCIDeviceRestoreQ);
	gIOPlatformDeviceMessageKey = OSSymbol::withCStringNoCopy(kIOPlatformDeviceMessageKey);
	gIOPlatformDeviceASPMEnableKey = OSSymbol::withCStringNoCopy(kIOPlatformDeviceASPMEnableKey);
    }
}

bool IOPCIBridge::start( IOService * provider )
{
    static const IOPMPowerState powerStates[ kIOPCIBridgePowerStateCount ] = {
 	// version,
	// capabilityFlags, outputPowerCharacter, inputPowerRequirement,
               { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 1, 0, kIOPMSoftSleep, kIOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
            };


    if (!gIOPCIACPIPlane)
	gIOPCIACPIPlane = IORegistryEntry::getPlane("IOACPIPlane");

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
	IOPCIRange * range;
	IOPCIRange * nextRange;
	uint32_t     resourceType;
	for (resourceType = 0; resourceType < kIOPCIResourceTypeCount; resourceType++)
	{
	    range = reserved->rangeLists[resourceType];
	    while (range)
	    {
		nextRange = range->next;
		IODelete(range, IOPCIRange, 1);
		range = nextRange;
	    }
	}

        IODelete(reserved, ExpansionData, 1);
    }

    super::free();
}

IOReturn IOPCIBridge::setDeviceASPMBits(IOPCIDevice * device, IOOptionBits state)
{
    UInt16 control;

    if (!device->reserved->expressConfig)
	return (kIOReturnUnsupported);

    control = device->configRead16(device->reserved->expressConfig + 0x10);
    control &= ~3;
    if (state)
	control |= device->reserved->expressASPMDefault;

    device->configWrite16(device->reserved->expressConfig + 0x10, control);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::setDeviceASPMState(IOPCIDevice * device,
					    IOService * client, IOOptionBits state)
{
    IOReturn ret;

    ret = setDeviceASPMBits(device, state);

    return (ret);
}

IOReturn IOPCI2PCIBridge::setDeviceASPMState(IOPCIDevice * device,
					    IOService * client, IOOptionBits state)
{
    IOReturn ret;

    ret = IOPCIBridge::setDeviceASPMState(device, client, state);
    if (kIOReturnSuccess == ret)
	setDeviceASPMBits(bridgeDevice, state);

    return (ret);
}

IOReturn IOPCIBridge::setDevicePowerState( IOPCIDevice * device,
        unsigned long whatToDo )
{
    if ((kSaveDeviceState == whatToDo) || (kRestoreDeviceState == whatToDo))
    {
	IOReturn ret = kIOReturnSuccess;

	if ((device->savedConfig && configShadow(device)->bridge)
	|| (kOSBooleanFalse != device->getProperty(kIOPMPCIConfigSpaceVolatileKey)))
	{
	    if (kRestoreDeviceState == whatToDo)
		ret = restoreDeviceState(device);
	    else
		ret = saveDeviceState(device);
	}

	return (ret);
    }

#if 0
    if (16 == whatToDo)
    {
	OSIterator * iter = getClientIterator();
	if (iter)
	{
	    OSObject * child;
	    while ((child = iter->getNextObject()))
	    {
		IOPCIDevice * pciDevice;
		if ((pciDevice = OSDynamicCast(IOPCIDevice, child)))
		    pciDevice->setBusMasterEnable(false);
	    }
	    iter->release();
	}
    }
#endif

    // Special for pci/pci-bridge devices - 
    // kSaveBridgeState(2) to save immediately, kRestoreBridgeState(3) to restore immediately

    if (kRestoreBridgeState == whatToDo)
	restoreMachineState(0);
    gIOAllPCI2PCIBridgeState = whatToDo;

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::saveDeviceState( IOPCIDevice * device,
                                       IOOptionBits options )
{
    UInt32 flags;
    void * p3 = (void *) 3;
    int i;

    if (!device->savedConfig)
        return (kIOReturnNotReady);

    flags = configShadow(device)->flags;

    if (kIOPCIConfigShadowValid & flags)
        return (kIOReturnSuccess);

    flags |= kIOPCIConfigShadowValid;
    configShadow(device)->flags = flags;

    device->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
	(void *) kIOMessageDeviceWillPowerOff, device, p3, (void *) 0);

    if (kIOPCIConfigShadowBridge & flags)
    {
	if (configShadow(device)->bridge)
	    configShadow(device)->bridge->saveBridgeState();
    }
    else
    {
	for (i = 0; i < kIOPCIConfigShadowRegs; i++)
	{
	    if (kIOPCIVolatileRegsMask & (1 << i))
		device->savedConfig[i] = device->configRead32( i * 4 );
	}
    }

    if (device->reserved->expressConfig)
    {
	device->savedConfig[kIOPCIConfigShadowXPress + 0]   // device control
	    = device->configRead16( device->reserved->expressConfig + 0x08 );
	device->savedConfig[kIOPCIConfigShadowXPress + 1]   // link control
	    = device->configRead16( device->reserved->expressConfig + 0x10 );
	if (0x100 & device->reserved->expressCapabilities)
	{						    // slot control
	    device->savedConfig[kIOPCIConfigShadowXPress + 2]
		= device->configRead16( device->reserved->expressConfig + 0x18 );
	}
    }
    for (i = 0; i < device->reserved->msiBlockSize; i++)
	device->savedConfig[kIOPCIConfigShadowMSI + i] 
	    = device->configRead32( device->reserved->msiConfig + i * 4 );

    device->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
	(void *) kIOMessageDeviceHasPoweredOn, device, p3, (void *) 0);

    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

    queue_enter_first( &gIOAllPCIDeviceRestoreQ,
		 configShadow(device),
		 IOPCIConfigShadow *,
		 link );

    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::_restoreDeviceState( IOPCIDevice * device,
        IOOptionBits options )
{
    UInt32 flags;
    void * p3 = (void *) 0;
    int i;

    flags = configShadow(device)->flags;

    if (!(kIOPCIConfigShadowValid & flags))
	return (kIOReturnNotReady);
    flags &= ~kIOPCIConfigShadowValid;
    configShadow(device)->flags = flags;

//    device->configWrite32(kIOPCIConfigCommand, 0);

    device->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
	(void *) kIOMessageDeviceWillPowerOff, device, p3, (void *) 0);

    AbsoluteTime deadline, now;
    uint32_t     retries = 0;
    uint32_t     data;
    bool         ok;

    clock_interval_to_deadline(20, kMillisecondScale, &deadline);
    do
    {
	data = device->configRead32(kIOPCIConfigVendorID);
	ok = (data && (data != 0xFFFFFFFF));
	if (ok)
	    break;
	retries++;
	clock_get_uptime(&now);
    }
    while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));
    if (retries)
    {
	IOLog("pci restore waited for %s (%d) %s\n", 
		device->getName(), retries, ok ? "ok" : "fail");
    }

    if (kIOPCIConfigShadowBridge & flags)
    {
	if (configShadow(device)->bridge)
	    configShadow(device)->bridge->restoreBridgeState();
    }
    else
    {
	for (i = (kIOPCIConfigRevisionID >> 2); i < kIOPCIConfigShadowRegs; i++)
	{
	    if (kIOPCIVolatileRegsMask & (1 << i))
		device->configWrite32( i * 4, device->savedConfig[ i ]);
	}
	device->configWrite32(kIOPCIConfigCommand, device->savedConfig[1]);
    }

    if (device->reserved->expressConfig)
    {
	device->configWrite16( device->reserved->expressConfig + 0x08,   // device control
				device->savedConfig[kIOPCIConfigShadowXPress + 0]);
	device->configWrite16( device->reserved->expressConfig + 0x10,    // link control
				device->savedConfig[kIOPCIConfigShadowXPress + 1] );
	if (0x100 & device->reserved->expressCapabilities)
	{								  // slot control
	    device->configWrite16( device->reserved->expressConfig + 0x18, 
				    device->savedConfig[kIOPCIConfigShadowXPress + 2] );
	}
    }

    for (i = 0; i < device->reserved->msiBlockSize; i++)
	device->configWrite32( device->reserved->msiConfig + i * 4,  
				device->savedConfig[kIOPCIConfigShadowMSI + i]);

    device->callPlatformFunction(gIOPlatformDeviceMessageKey, false,
	(void *) kIOMessageDeviceHasPoweredOn, device, p3, (void *) 0);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::restoreMachineState( IOOptionBits options )
{
    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

    if (kSaveBridgeState != gIOAllPCI2PCIBridgeState)
    {
	IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);
	return (kIOReturnNotFound);
    }

    gIOAllPCI2PCIBridgeState = kRestoreBridgeState;

    IOPCIConfigShadow * shadow;
    IOPCIConfigShadow * next;
    UInt32              bridgesOnly = true;

    do
    {
	next = (IOPCIConfigShadow *) queue_first(&gIOAllPCIDeviceRestoreQ);
	while (!queue_end(&gIOAllPCIDeviceRestoreQ, (queue_entry_t) next))
	{
	    shadow = next;
	    next   = (IOPCIConfigShadow *) queue_next(&shadow->link);

	    if (bridgesOnly && !(kIOPCIConfigShadowBridge & shadow->flags))
		continue;

	    queue_remove( &gIOAllPCIDeviceRestoreQ,
			  shadow,
			  IOPCIConfigShadow *,
			  link );
	    shadow->link.next = shadow->link.prev = NULL;

	    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);
	    _restoreDeviceState(shadow->device, false);
	    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);
	}
    }
#ifndef __ppc__
    while (bridgesOnly--);
#else
    while (false);
#endif

    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);

    return (kIOReturnSuccess);
}

IOReturn IOPCIBridge::restoreDeviceState( IOPCIDevice * device, IOOptionBits options )
{
    IOReturn ret;

    if (!device->savedConfig)
        return (kIOReturnNotReady);

#ifndef __ppc__
    ret = restoreMachineState(0);

    if (kIOReturnSuccess != ret)
#endif
    {
	if (configShadow(device)->link.next)
	{
	    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);
	    queue_remove( &gIOAllPCIDeviceRestoreQ,
			  configShadow(device),
			  IOPCIConfigShadow *,
			  link );
	    configShadow(device)->link.next = configShadow(device)->link.prev = NULL;
	    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);
	}
	ret = _restoreDeviceState(device, true);
    }
    // callers expect success
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
    uint32_t	vendor, product, classCode, revID;
    uint32_t	subVendor = 0, subProduct = 0;
    IOByteCount offset;
    OSData *	data;
    OSData *	nameData;
    char	compatBuf[128];
    char *	out;

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

    if (!(data = OSDynamicCast(OSData, entry->getProperty("compatible")))
            || !(nameData = OSDynamicCast(OSData, entry->getProperty("name")))
            || data->isEqualTo(nameData))
    {
	// compatible change needed
	out = compatBuf;
	if ((subVendor || subProduct)
		&& ((subVendor != vendor) || (subProduct != product)))
	    out += snprintf(out, sizeof("pcivvvv,pppp"), "pci%x,%x", subVendor, subProduct) + 1;
	out += snprintf(out, sizeof("pcivvvv,pppp"), "pci%x,%x", vendor, product) + 1;
	out += snprintf(out, sizeof("pciclass,cccccc"), "pciclass,%06x", classCode) + 1;
    
	entry->setProperty("compatible", compatBuf, out - compatBuf);
    }

    offset = 0;
    if (entry->extendedFindPCICapability(kIOPCIPCIExpressCapability, &offset))
    {
	UInt32
	value = entry->configRead16(offset + 0x12);
	entry->setProperty(kIOPCIExpressLinkStatusKey, value, 32);
	value = entry->configRead32(offset + 0x0c);
	entry->setProperty(kIOPCIExpressLinkCapabilitiesKey, value, 32);
    }

    return (true);
}

OSDictionary * IOPCIBridge::constructProperties( IOPCIAddressSpace space )
{
    OSDictionary *	propTable;
    uint32_t		value;
    uint32_t		vendor, product, classCode, revID;
    uint32_t		subVendor = 0, subProduct = 0;
    OSData *		prop;
    const char *	name;
    const OSSymbol *	nameProp;
    char		compatBuf[128];
    char *		out;

    struct IOPCIGenericNames
    {
        const char *	name;
        uint32_t	mask;
        uint32_t	classCode;
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
            out += snprintf(out, sizeof("pcivvvv,pppp"), "pci%x,%x", subVendor, subProduct) + 1;

        if (0 == name)
            name = out;

        out += snprintf(out, sizeof("pcivvvv,pppp"), "pci%x,%x", vendor, product) + 1;
        out += snprintf(out, sizeof("pciclass,cccccc"), "pciclass,%06x", classCode) + 1;

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

void IOPCIBridge::removeDevice( IOPCIDevice * device, IOOptionBits options )
{
    IOReturn ret = kIOReturnSuccess;

    if (device->reserved->msiConfig && gIOPCIMessagedInterruptController)
	ret = gIOPCIMessagedInterruptController->deallocateDeviceInterrupts(this, device);

    IOSimpleLockLock(gIOAllPCI2PCIBridgesLock);

    if (configShadow(device)->link.next)
    {
	queue_remove( &gIOAllPCIDeviceRestoreQ,
		      configShadow(device),
		      IOPCIConfigShadow *,
		      link );
	configShadow(device)->link.next = configShadow(device)->link.prev = NULL;
    }

    IOSimpleLockUnlock(gIOAllPCI2PCIBridgesLock);
}

bool IOPCIBridge::publishNub( IOPCIDevice * nub, UInt32 /* index */ )
{
    char			location[ 24 ];
    bool			ok;
#if ROM_KEXTS
    OSData *			data;
    OSData *			driverData;
    UInt32			*regData, expRomReg;
    IOMemoryMap *		memoryMap;
    IOVirtualAddress		virtAddr;
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
	    shadow->device = nub;
	    nub->savedConfig = &shadow->savedConfig[0];
	    for (int i = 0; i < kIOPCIConfigShadowRegs; i++)
		if (!(kIOPCIVolatileRegsMask & (1 << i)))
		    nub->savedConfig[i] = nub->configRead32( i << 2 );
	}

	checkProperties( nub );

	if (shadow && (kIOPCIClassBridge == nub->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24))
	    shadow->flags |= kIOPCIConfigShadowBridge;

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

void IOPCIBridge::probeBus( IOService * provider, UInt8 busNum )
{
    IORegistryEntry *  found;
    OSDictionary *     propTable;
    IOPCIDevice *      nub = 0;
    OSIterator *       kidsIter;
    UInt32             index = 0;
    UInt32             idx = 0;
    bool	       hotplugBus;

    hotplugBus = (0 != getProperty(kIOPCIHotPlugKey));
    if (hotplugBus && !provider->getProperty(kIOPCIOnlineKey))
    {
	LOG("offline\n");    
	return;
    }

#if USE_IOPCICONFIGURATOR
    bool configured;
    configured = (0 != provider->getProperty(kIOPCIConfiguredKey));
    if (!configured)
    {
//	IOSleep(5000);	LOG("hotp IOPCIConfigurator\n");

	IOPCIConfigurator * pciConfig = new IOPCIConfigurator;
	if (pciConfig && pciConfig->init())
	{
	    pciConfig->attach(this);
	    pciConfig->start(this);
	    pciConfig->detach(this);
	    pciConfig->release();
	}
    }
#endif

    IODTSetResolving(provider, &compareAddressCell, &nvLocation);

    if (2 & gIOPCIDebug)
        kidsIter = 0;
    else
        kidsIter = provider->getChildIterator( gIODTPlane );

    // find and copy over any devices from the device tree
    OSArray * nubs = OSArray::withCapacity(0x10);
    assert(nubs);

    if (kidsIter) {
	kidsIter->reset();
	while ((found = (IORegistryEntry *) kidsIter->getNextObject()))
	{
	    if (!found->getProperty("vendor-id"))
		continue;
	    if (found->inPlane(gIOServicePlane))
		continue;

	    propTable = found->getPropertyTable();
	    nub = createNub( propTable );
	    if ( nub 
		&& initializeNub(nub, propTable)
		&& nub->init(found, gIODTPlane) )
	    {
		IOByteCount capa;

		nubs->setObject(index++, nub);

		capa = 0;
		if (nub->extendedFindPCICapability(kIOPCIPCIExpressCapability, &capa))
		{
		    nub->reserved->expressConfig       = capa;
		    nub->reserved->expressCapabilities = nub->configRead16(capa + 0x02);
		    nub->reserved->expressASPMDefault  = (3 & (nub->configRead16(capa + 0x10)));
		    nub->setProperty("IOPCIExpressASPMDefault", nub->reserved->expressASPMDefault, 32);
		}
		capa = 0;
#if 0
		if (nub->extendedFindPCICapability(kIOPCIMSIXCapability, &capa))
		{
		    nub->reserved->msiConfig = capa;
		    nub->reserved->msiMode   |= kMSIX;
		}
		else 
#endif
		if (nub->extendedFindPCICapability(kIOPCIMSICapability, &capa))
		    nub->reserved->msiConfig = capa;

	    }
	    if (nub)
		nub->release();
	}
    }

    idx = 0;
    while (nub = (IOPCIDevice *)nubs->getObject(idx++))
    {
	if (hotplugBus || provider->getProperty(kIOPCIEjectableKey))
	    nub->setProperty(kIOPCIEjectableKey, kOSBooleanTrue);

        publishNub(nub , idx);

        if (1 & gIOPCIDebug)
            IOLog("%08x = 0:%08x 4:%08x  ", (uint32_t) nub->space.bits,
                (uint32_t) nub->configRead32(kIOPCIConfigVendorID),
                (uint32_t) nub->configRead32(kIOPCIConfigCommand) );
    }

    nubs->release();
    if (kidsIter)
	kidsIter->release();
}

bool IOPCIBridge::addBridgeMemoryRange( IOPhysicalAddress start,
                                        IOPhysicalLength length, bool host )
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
            kprintf("%s: didn't get host range (%08x:%08x)\n", getName(),
                    (uint32_t) start, (uint32_t) length);
    }

    // and into the bridge
    bridgeMemoryRanges->deallocate( start, length );

    IOPCIRange * newRange = IONew(IOPCIRange, 1);
    if (newRange)
    {
	bzero(newRange, sizeof(IOPCIRange));
	newRange->start = start;
	newRange->size = length;
	newRange->alignment = 0;
	newRange->type = kIOPCIResourceTypeMemory;
	newRange->flags = 0;

	newRange->next = reserved->rangeLists[kIOPCIResourceTypeMemory];
	reserved->rangeLists[kIOPCIResourceTypeMemory] = newRange;
    }

    return (ok);

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
            kprintf("%s: didn't get host range (%08llx:%08llx)\n", getName(),
                    (uint64_t) start, (uint64_t) length);
    }

    // and into the bridge
    bridgeMemoryRanges->deallocate( start, length );

    IOPCIRange * newRange = IONew(IOPCIRange, 1);
    if (newRange)
    {
	bzero(newRange, sizeof(IOPCIRange));
	newRange->start     = start;
	newRange->size      = length;
	newRange->alignment = 0;
	newRange->type      = kIOPCIResourceTypePrefetchMemory;
	newRange->flags     = 0;

	newRange->next = reserved->rangeLists[kIOPCIResourceTypePrefetchMemory];
	reserved->rangeLists[kIOPCIResourceTypePrefetchMemory] = newRange;
    }

    return (ok);
}

bool IOPCIBridge::addBridgeIORange( IOByteCount start, IOByteCount length )
{
    bool	ok = true;

    // into the bridge
    bridgeIORanges->deallocate( start, length );

    IOPCIRange * newRange = IONew(IOPCIRange, 1);
    if (newRange)
    {
	bzero(newRange, sizeof(IOPCIRange));
	newRange->start     = start;
	newRange->size      = length;
	newRange->alignment = 0;
	newRange->type      = kIOPCIResourceTypeIO;
	newRange->flags     = 0;

	newRange->next = reserved->rangeLists[kIOPCIResourceTypeIO];
	reserved->rangeLists[kIOPCIResourceTypeIO] = newRange;
    }

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
        safeProbeBAR(&probeParam);
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
            IOLog("Space %08x : %08llx, %08llx\n", (uint32_t) reg.bits, (uint64_t) phys, (uint64_t) len);

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

    if (service->getProperty(kIOPCIResourcedKey))
        return (kIOReturnSuccess);
    service->setProperty(kIOPCIResourcedKey, kOSBooleanTrue);

#if USE_IOPCICONFIGURATOR
    err = getDTNubAddressing( nub );
#else
    if (isDTNub(nub))
        err = getDTNubAddressing( nub );
    else
        err = getNubAddressing( nub );
#endif


    bool 
    msiDefault = (false
#if 0
		    || (0 == strcmp("display", nub->getName()))
		    || (0 == strcmp("GFX0", nub->getName()))
		    || (0 == strcmp("PXS1", nub->getName()))	    // yukon
		    || (0 == strcmp("HDEF", nub->getName()))
		    || (0 == strcmp("SATA", nub->getName()))
		    || (0 == strcmp("LAN0", nub->getName()))
		    || (0 == strcmp("LAN1", nub->getName()))
		    || (0 == strcmp("PXS2", nub->getName()))	    // airport
		    || (0 == strcmp("PXS3", nub->getName()))	    // express
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

bool IOPCI2PCIBridge::serializeProperties( OSSerialize * serialize ) const
{
    return (super::serializeProperties(serialize));
}

bool IOPCI2PCIBridge::filterInterrupt( IOFilterInterruptEventSource * source)
{
    enum { kNeedMask = ((1 << 8) | (1 << 3)) };

    uint16_t slotStatus = bridgeDevice->configRead16( fXpressCapability + 0x1a );
    if (kNeedMask & slotStatus)
	bridgeDevice->configWrite16( fXpressCapability + 0x1a, slotStatus );

    return (0 != (kNeedMask & slotStatus));
}

void IOPCIBridge::checkTerminateChildren(IOService * bridgeDevice, bool eject)
{
    OSIterator * iter;
    IOService *  child;

    iter = bridgeDevice->getChildIterator(gIODTPlane);
    if (!iter) return;

    while ((child = (IOService *) iter->getNextObject()))
    {
	if (eject)
	{
	    if (kOSBooleanFalse != child->getProperty(kIOPCIOnlineKey))
		continue;
	    LOG("eject-terminate %p\n", child);    
	}
	else
	    LOG("hotp-terminate %p\n", child);
	child->terminate();
	LOG("did terminate %p\n", child);    
    }
    iter->release();
}

void IOPCI2PCIBridge::handleInterrupt( IOInterruptEventSource * source,
				     int                      count )
{
    bool present;

    fHotplugCount++;

    uint16_t slotStatus  = bridgeDevice->configRead16( fXpressCapability + 0x1a );
    uint16_t linkStatus  = bridgeDevice->configRead16( fXpressCapability + 0x12 );
    uint16_t linkControl = bridgeDevice->configRead16( fXpressCapability + 0x10 );

    LOG("hotpInt (%d), slotStatus %x, linkStatus %x, linkControl %x\n",
	    fHotplugCount, slotStatus, linkStatus, linkControl);

    present = (0 != ((1 << 6) & slotStatus));
    if (present && ((1 << 4) & linkControl))
    {
	LOG("enabling link\n");
	linkControl &= ~(1 << 4);
	bridgeDevice->configWrite16( fXpressCapability + 0x10, linkControl );
	fWaitingLinkEnable = true;
	present = false;
    }
    else if (!present && !((1 << 4) & linkControl))
    {
	if (fWaitingLinkEnable)
	    fWaitingLinkEnable = false;
	else
	{
	    LOG("disabling link\n");
	    bridgeDevice->configWrite16( fXpressCapability + 0x10, linkControl | (1 << 4) );
	}
    }

    if (fLinkChangeOnly)
	return;

    present &= (0 != ((1 << 13) & linkStatus));

    if (fPresence == present)
    {
	if (present) checkTerminateChildren(bridgeDevice, true);
	return;
    }

    bridgeDevice->removeProperty(kIOPCIConfiguredKey);

    fPresence = present;
    if (!present)
    {
	// not present
	bridgeDevice->removeProperty(kIOPCIOnlineKey);
	checkTerminateChildren(bridgeDevice, false);
    }
    else
    {
	// present
	bridgeDevice->setProperty(kIOPCIOnlineKey, true);

#if 0
	// /bus pad
	if (fHotplugLastBus <= fHotplugFirstBus)
	{
	    fHotplugFirstBus = 36;
	    fHotplugLastBus  = 40;

	    uint32_t reg32 = bridgeDevice->configRead32(kPCI2PCIPrimaryBus);
	    reg32 &= ~0x00ffff00;
	    bridgeDevice->configWrite32(kPCI2PCIPrimaryBus, reg32 | (0xff << 8) | (0x00 << 16));
	    reg32 |= (fHotplugFirstBus << 8) | (fHotplugLastBus << 16);
	    bridgeDevice->configWrite32(kPCI2PCIPrimaryBus, reg32);

	    fHotplugFirstBus = bridgeDevice->configRead8( kPCI2PCISecondaryBus );
	    fHotplugLastBus  = bridgeDevice->configRead8( kPCI2PCISubordinateBus );
	    LOG("making first bus %d, last bus %d\n", fHotplugFirstBus, fHotplugLastBus);    
	}
	// bus pad/
#endif
	IOSleep(1* 1000);
	LOG("probeBus()\n");
    	setProperty(kIOPCIResetKey, kOSBooleanTrue);
	probeBus( bridgeDevice, firstBusNum() );
    }
}


bool IOPCI2PCIBridge::start( IOService * provider )
{
    bool ok;

    reserved = IONew(ExpansionData, 1);
    if (reserved == 0) return (false);

    bzero(reserved, sizeof(ExpansionData));

    ok = super::start(provider);

    if (ok && fBridgeInterruptSource)
	changePowerStateTo(2);

    return (ok);
}

bool IOPCI2PCIBridge::configure( IOService * provider )
{
    UInt32	end;
    UInt32	start;
    bool 	ok;

    IOByteCount offset = 0;

    if (bridgeDevice->extendedFindPCICapability(kIOPCIPCIExpressCapability, &offset))
	fXpressCapability = offset;

    if (fXpressCapability)
    do
    {
	IOReturn ret;

	if (bridgeDevice->getProperty(kIOPCIHotPlugKey))
	    setProperty(kIOPCIHotPlugKey, kOSBooleanTrue);
	else if (bridgeDevice->getProperty(kIOPCILinkChangeKey))
	{
	    setProperty(kIOPCILinkChangeKey, kOSBooleanTrue);
	    fLinkChangeOnly = true;
	}
	else
	    break;

	if (!gCommonWorkLoop)
	    gCommonWorkLoop = IOWorkLoop::workLoop();
	fWorkLoop = gCommonWorkLoop;
	    
	fBridgeInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(
                      this,
		      OSMemberFunctionCast(IOInterruptEventSource::Action,
					    this, &IOPCI2PCIBridge::handleInterrupt),
		      OSMemberFunctionCast(IOFilterInterruptEventSource::Filter,
					    this, &IOPCI2PCIBridge::filterInterrupt),
                      provider, 0);
	if (!fBridgeInterruptSource)
	    break;

        ret = fWorkLoop->addEventSource( fBridgeInterruptSource );
	if (kIOReturnSuccess != ret)
	    break;

	uint16_t slotStatus  = bridgeDevice->configRead16( fXpressCapability + 0x1a );
	uint16_t linkStatus  = bridgeDevice->configRead16( fXpressCapability + 0x12 );
	uint16_t linkControl = bridgeDevice->configRead16( fXpressCapability + 0x10 );

	LOG("hotp configure slotStatus %x, linkStatus %x, linkControl %x\n",
	    slotStatus, linkStatus, linkControl);

	fPresence = (0 != ((1 << 13) & linkStatus));
	fPresence &= (0 != ((1 << 6) & slotStatus));
	if (fPresence)
	{
	    if (!fLinkChangeOnly)
		bridgeDevice->setProperty(kIOPCIOnlineKey, kOSBooleanTrue);
	}
	else if (!((1 << 4) & linkControl))
	{
	    LOG("disable link\n");
	    linkControl |= (1 << 4);
	    bridgeDevice->configWrite16( fXpressCapability + 0x10, linkControl );
	}

//	fHotplugFirstBus = bridgeDevice->configRead8( kPCI2PCISecondaryBus );
//	fHotplugLastBus  = bridgeDevice->configRead8( kPCI2PCISubordinateBus );
//	LOG("first bus %d, last bus %d\n", fHotplugFirstBus, fHotplugLastBus);    

	fBridgeInterruptEnablePending = true;
    }
    while(false);

    uint8_t secBus = bridgeDevice->configRead8( kPCI2PCISecondaryBus );
    if (!secBus)
    {
	LOG("Bridge has secBus 0\n");
	return (false);
    }

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
    {
        configShadow(bridgeDevice)->flags |= kIOPCIConfigShadowBridge;
        configShadow(bridgeDevice)->bridge = this;
    }

    return (super::configure(provider));
}

void IOPCI2PCIBridge::probeBus( IOService * provider, UInt8 busNum )
{
    super::probeBus(provider, busNum);
    
    if (fBridgeInterruptEnablePending)
    {
	// enable hotp ints
	uint16_t slotControl = bridgeDevice->configRead16( fXpressCapability + 0x18 );
	bridgeDevice->configWrite16( fXpressCapability + 0x1a, 1 << 3 );
	slotControl |= (1 << 12) | (1 << 5) | (1 << 3);
	bridgeDevice->configWrite16( fXpressCapability + 0x18, slotControl );
	fBridgeInterruptSource->enable();
	fBridgeInterruptEnablePending = false;
    }
}

IOReturn IOPCI2PCIBridge::requestProbe( IOOptionBits options )
{
    if (kIOPCIProbeOptionEject != options)
	return (super::requestProbe(options));

    if (!fBridgeInterruptSource || fBridgeInterruptEnablePending)
	return (kIOReturnNotReady);

    fBridgeInterruptSource->signalInterrupt();

    return (kIOReturnSuccess);
}

IOReturn IOPCI2PCIBridge::setPowerState( unsigned long powerState,
					    IOService * whatDevice )
{

    if (powerState && reserved && fBridgeInterruptSource && !fBridgeInterruptEnablePending)
    {
	fBridgeInterruptSource->signalInterrupt();
    }

    return (super::setPowerState(powerState, whatDevice));
}

void IOPCI2PCIBridge::stop( IOService * provider )
{
    if (reserved && fBridgeInterruptSource)
    {
       fBridgeInterruptSource->disable();

       IOWorkLoop * tempWL = fBridgeInterruptSource->getWorkLoop();
       if (tempWL)
	   tempWL->removeEventSource(fBridgeInterruptSource);

       fBridgeInterruptSource->release();
       fBridgeInterruptSource = 0;
    }

    super::stop( provider);
}

void IOPCI2PCIBridge::free()
{
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


IOReturn IOPCIBridge::resolveMSIInterrupts( IOService * provider, IOPCIDevice * nub )
{
    IOReturn ret = kIOReturnUnsupported;

#if USE_MSI

    IOByteCount msi = nub->reserved->msiConfig;

    if (msi) do
    {
	uint16_t control = nub->configRead16(msi + 2);
	uint32_t numMessages;

	if (kMSIX & nub->reserved->msiMode)
	    numMessages = 1 + (0x7ff & control);
	else
	    numMessages = 1 << (0x7 & (control >> 1));

	IOLockLock(gIOPCIMessagedInterruptControllerLock);

	if (!gIOPCIMessagedInterruptController)
	{
	    enum {
		// LAPIC_DEFAULT_INTERRUPT_BASE (mp.h)
		kNumMessagedInterruptVectors = 0xD0 - 0x90
	    };

	    IOPCIMessagedInterruptController *
	    ic = new IOPCIMessagedInterruptController;
	    if (ic 
		&& !ic->init(kNumMessagedInterruptVectors))
	    {
		ic->release();
		ic = 0;
	    }
	    gIOPCIMessagedInterruptController = ic;
	}

	IOLockUnlock(gIOPCIMessagedInterruptControllerLock);

	if (!gIOPCIMessagedInterruptController)
	    continue;

	ret = gIOPCIMessagedInterruptController->allocateDeviceInterrupts(this, nub, numMessages);
    }
    while (false);

#endif /* USE_MSI */

    return (ret);
}

IOReturn IOPCIBridge::resolveLegacyInterrupts( IOService * provider, IOPCIDevice * nub )
{
#if USE_LEGACYINTS

    uint32_t pin;
    uint32_t irq = 0;

    pin = (UInt8)(nub->configRead32( kIOPCIConfigInterruptLine ) >> 8);
    if ( pin == 0 || pin > 4 )
        return (kIOReturnUnsupported);  // assume no interrupt usage

    pin--;  // make pin zero based, INTA=0, INTB=1, INTC=2, INTD=3

    // Ask the platform driver to resolve the PCI interrupt route,
    // and return its corresponding system interrupt vector.

    if ( kIOReturnSuccess == provider->callPlatformFunction( "ResolvePCIInterrupt",
                   /* waitForFunction */ false,
                   /* provider nub    */ provider,
                   /* device number   */ (void *) nub->space.s.deviceNum,
                   /* interrupt pin   */ (void *) pin,
                   /* resolved IRQ    */ &irq ))
    {
        if (1 & gIOPCIDebug)
            IOLog("%s: Resolved interrupt %d (%d) for %s\n",
                  provider->getName(),
                  irq, nub->configRead8(kIOPCIConfigInterruptLine),
                  nub->getName());
        
        nub->configWrite8( kIOPCIConfigInterruptLine, irq & 0xff );
    }
    else
    {
        irq = nub->configRead8( kIOPCIConfigInterruptLine );
        if ( 0 == irq || 0xff == irq ) return (kIOReturnUnsupported);
        irq &= 0xf;  // what about IO-APIC and irq > 15?
    }

    provider->callPlatformFunction( "SetDeviceInterrupts",
              /* waitForFunction */ false,
              /* nub             */ nub, 
              /* vectors         */ (void *) &irq,
              /* vectorCount     */ (void *) 1,
              /* exclusive       */ (void *) false );

#endif /* USE_LEGACYINTS */

    return (kIOReturnSuccess);
}


