/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
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


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/system.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOLib.h>

#define kMSIFreeCountKey    "MSIFree"

#ifndef kBaseVectorNumberKey
#define kBaseVectorNumberKey          "Base Vector Number"
#endif

#ifndef kInterruptControllerNameKey
#define kVectorCountKey               "Vector Count"
#endif

#ifndef kInterruptControllerNameKey
#define kInterruptControllerNameKey   "InterruptControllerName"
#endif

extern uint32_t gIOPCIFlags;

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
    if (!vectors) return (false);
    bzero( vectors, sizeof(IOInterruptVector) * _vectorCount );
    _flags = IONew(uint8_t, _vectorCount);
    if (!_flags) return (false);
    bzero(_flags, sizeof(uint8_t) * _vectorCount);

    // Allocate locks for the vectors.

    for (uint32_t i = 0; i < _vectorCount; i++)
    {
        vectors[i].interruptLock = IOLockAlloc();
        if (!vectors[i].interruptLock) return (false);
    }

    attach(getPlatform());
    sym = copyName();
    setProperty(kInterruptControllerNameKey, (OSObject *) sym);
    getPlatform()->registerInterruptController( (OSSymbol *) sym, this );
    sym->release();

    num = OSDynamicCast(OSNumber, getProperty(kBaseVectorNumberKey));
    if (num) _vectorBase = num->unsigned32BitValue();

    _messagedInterruptsAllocator = IORangeAllocator::withRange(0, 0, 4, IORangeAllocator::kLocking);
    _messagedInterruptsAllocator->deallocate(0, _vectorCount);
    setProperty(kMSIFreeCountKey, _messagedInterruptsAllocator->getFreeCount(), 32);

    registerService();

    return (true);
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
                                IOService * entry, uint32_t numVectors, uint32_t msiConfig,
                                uint64_t * msiAddress, uint32_t * msiData)
{
    IOReturn      ret;
    IOPCIDevice * device;
    uint32_t      vector, firstVector = _vectorBase;
    IORangeScalar rangeStart;
    uint32_t      message[3];
    uint16_t      control;
    bool          allocated;

    device = OSDynamicCast(IOPCIDevice, entry);
    if (!device) msiConfig = 0;
    if (msiConfig)
    {
        uint32_t    vendorProd;
        uint32_t    revIDClass;

        msiConfig  = device->reserved->msiConfig;
        control    = device->configRead16(msiConfig + 2);
        if (kMSIX & device->reserved->msiMode)
            numVectors = 1 + (0x7ff & control);
        else
            numVectors = 1 << (0x7 & (control >> 1));

        vendorProd = device->savedConfig[kIOPCIConfigVendorID >> 2];
        revIDClass = device->savedConfig[kIOPCIConfigRevisionID >> 2];

        // pci2pci bridges get none or one for hotplug
        if (0x0604 == (revIDClass >> 16))
        {
            bool tunnelLink = (0 != device->getProperty(kIOPCITunnelLinkChangeKey));
            if (tunnelLink 
                || device->getProperty(kIOPCIHotPlugKey)
                || device->getProperty(kIOPCILinkChangeKey))
            {
                // hot plug bridge, but use legacy if avail
                uint8_t line = device->configRead8(kIOPCIConfigInterruptLine);
                if (tunnelLink)
                {
                    tunnelLink = (0x15138086 != vendorProd)
                              && (0x151a8086 != vendorProd)
                              && (0x151b8086 != vendorProd)
                              && (0x15498086 != vendorProd)
                              && ((0x15478086 != vendorProd) || ((revIDClass & 0xff) > 1));
                }
                if (tunnelLink || (line == 0) || (line == 0xFF))
                {
                    // no legacy ints, need one MSI
                    numVectors = 1;
                }
                else
                    numVectors = 0;
            }
            else
            {
                // no hot plug
                numVectors = 0;
            }
        }
#if !defined(SUPPORT_MULTIPLE_MSI)
        else if (numVectors)
        {
            // max per function is one
            numVectors = 1;
        }
#endif
    }

    allocated = false;
    while (!allocated && numVectors > 0)
    {
        allocated = allocateInterruptVectors(entry, numVectors, &rangeStart);
        if (!allocated)
            numVectors >>= 1;
    }
    if (!allocated)
        return (kIOReturnNoSpace);
    else
        firstVector = rangeStart;

    ret = entry->callPlatformFunction(gIOPlatformGetMessagedInterruptAddressKey,
            /* waitForFunction */ false,
            /* nub             */ entry, 
            /* options         */ (void *) 0,
            /* vector          */ (void *) (uintptr_t) (firstVector + _vectorBase),
            /* message         */ (void *) &message[0]);

    if (kIOReturnSuccess == ret)
    {
        for (vector = firstVector; vector < (firstVector + numVectors); vector++)
        {
            addDeviceInterruptProperties(entry, 
                    vector,
                    kIOInterruptTypeEdge | kIOInterruptTypePCIMessaged, NULL);
        }
        if (msiAddress) *msiAddress = message[0] | (((uint64_t)message[1]) << 32);
        if (msiData)    *msiData    = message[2];
        if (msiConfig)
        {
            IOByteCount msiBlockSize;
            if (kMSIX & device->reserved->msiMode)
            {
                IOByteCount msiTable;
                UInt8 bar;
                IOMemoryDescriptor * memory;
                uint64_t phys;

                control &= ~(1 << 15);          // disabled

                msiBlockSize = 1;   // words

                msiTable = device->configRead32(msiConfig + 4);
                bar = kIOPCIConfigBaseAddress0 + ((msiTable & 7) << 2);
                msiTable &= ~7;

                memory = device->getDeviceMemoryWithRegister(bar);
                if (memory && (phys = memory->getPhysicalSegment(0, 0, kIOMemoryMapperNone)))
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
                control &= ~1;                                      // disabled
                if (numVectors) 
                    numVectors = (31 - __builtin_clz(numVectors));  // log2
                control |= (numVectors << 4);

                msiBlockSize = 3;   // words
                if (0x0080 & control)
                {
                    // 64b
                    device->configWrite32(msiConfig + 4,  message[0]);
                    device->configWrite32(msiConfig + 8,  message[1]);
                    device->configWrite16(msiConfig + 12, message[2]);
                    device->configWrite16(msiConfig + 2,  control);
                    msiBlockSize += 1;
                }
                else
                {
                    device->configWrite32(msiConfig + 4,  message[0]);
                    device->configWrite16(msiConfig + 8,  message[2]);
                    device->configWrite16(msiConfig + 2,  control);
                }
                if (0x0100 & control)
                    msiBlockSize += 2;
            }
            device->reserved->msiBlockSize = msiBlockSize;
        }
    }

    return (ret);
}

bool IOPCIMessagedInterruptController::allocateInterruptVectors( IOService *device,
                                                                 uint32_t numVectors,
                                                                 IORangeScalar *rangeStartOut)
{
    bool result;
   
    result = _messagedInterruptsAllocator->allocate(numVectors, rangeStartOut, numVectors);
    if (result)
	    setProperty(kMSIFreeCountKey, _messagedInterruptsAllocator->getFreeCount(), 32);

    return result;
}

IOReturn IOPCIMessagedInterruptController::deallocateDeviceInterrupts(IOService * device)
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
            UInt32 vector = *((uint32_t *) spec->getBytesNoCopy());
            deallocateInterrupt(vector);
        }
        index++;
    }
    myName->release();

    return (kIOReturnSuccess);
}

void IOPCIMessagedInterruptController::deallocateInterrupt(UInt32 vector)
{
    IORangeScalar rangeStart = vector;
    _messagedInterruptsAllocator->deallocate(rangeStart, 1);
    setProperty(kMSIFreeCountKey, _messagedInterruptsAllocator->getFreeCount(), 32);
}

IOReturn IOPCIMessagedInterruptController::registerInterrupt( 
                                        IOService *        nub,
                                        int                source,
                                        void *             target,
                                        IOInterruptHandler handler,
                                        void *             refCon )
{
    IOReturn      ret;
    IOPCIDevice * device = OSDynamicCast(IOPCIDevice, nub);

    ret = super::registerInterrupt(nub, source, target, handler, refCon);

#if INT_TEST
    if ((0x000 & gIOPCIFlags) && !_parentInterruptController)
    {
        IOInterruptSource *interruptSources;
        IOInterruptVectorNumber vectorNumber;
        IOInterruptVector *vector;
        OSData            *vectorData;
      
        interruptSources = nub->_interruptSources;
        vectorData = interruptSources[source].vectorData;
        vectorNumber = *(IOInterruptVectorNumber *)vectorData->getBytesNoCopy();
        vector = &vectors[vectorNumber];
    
        kprintf("ping %d %p\n", vector->interruptRegistered, vector->handler);
        handleInterrupt(NULL, nub, vectorNumber + _vectorBase);
    }
#endif

    if (kIOReturnSuccess == ret)
        enableDeviceMSI(device);

    return (ret);
}

void IOPCIMessagedInterruptController::enableDeviceMSI(IOPCIDevice *device)
{
    if (device && device->reserved && !device->isInactive())
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
            control |= kIOPCICommandInterruptDisable | kIOPCICommandBusMaster;
            device->configWrite16(kIOPCIConfigCommand, control);
            device->setProperty("IOPCIMSIMode", kOSBooleanTrue);
        }
        device->reserved->msiEnable++;
    }
}

IOReturn IOPCIMessagedInterruptController::unregisterInterrupt( 
                                        IOService *        nub,
                                        int                source)
{
    IOReturn      ret;
    IOPCIDevice * device = OSDynamicCast(IOPCIDevice, nub);

#if INT_TEST
    if (0x000 & gIOPCIFlags)
    {
        IOInterruptSource *interruptSources;
        IOInterruptVectorNumber vectorNumber;
        IOInterruptVector *vector;
        OSData            *vectorData;

        interruptSources = nub->_interruptSources;
        vectorData = interruptSources[source].vectorData;
        vectorNumber = *(IOInterruptVectorNumber *)vectorData->getBytesNoCopy();
        vector = &vectors[vectorNumber];
    
        kprintf("uping %d %p\n", vector->interruptRegistered, vector->handler);
        handleInterrupt(NULL, nub, vectorNumber + _vectorBase);
    }
#endif

    ret = super::unregisterInterrupt(nub, source);

    disableDeviceMSI(device);

    return (ret);
}

void IOPCIMessagedInterruptController::disableDeviceMSI(IOPCIDevice *device)
{
    if (device && device->reserved 
        && device->reserved->msiEnable 
        && !(--device->reserved->msiEnable)
        && !device->isInactive())
    {
        IOByteCount msi = device->reserved->msiConfig;
        uint16_t control;

        control = device->configRead16(msi + 2);
        control &= ~((1 << 15) | 1);
        device->configWrite16(msi + 2, control);

		control = device->configRead16(kIOPCIConfigCommand);
		control &= ~kIOPCICommandInterruptDisable;
        device->configWrite16(kIOPCIConfigCommand, control);

        device->removeProperty("IOPCIMSIMode");
    }
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

    if (vector->interruptRegistered)
    {
        if (vector->interruptDisabledHard) _flags[source] = true;
        else
        {
            vector->handler(vector->target, vector->refCon, vector->nub, vector->source);
        }
    }

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
    if (_flags[vectorNumber])
    {
        _flags[vectorNumber] = 0;
        vector->handler(vector->target, vector->refCon,
                        vector->nub, vector->source);
    }
}
