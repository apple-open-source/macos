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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 * 23 Nov 98 sdouglas created from objc version.
 * 05 Nov 99 sdouglas added UniNorth AGP based on UniNorthAGPDriver.c
 *					by Fernando Urbina, Kent Miller.
 *
 */
#define _BIG_ENDIAN 1  // for 2370035

#include <IOKit/system.h>
#include <ppc/proc_reg.h>

#include <libkern/c++/OSContainers.h>
#include <libkern/OSByteOrder.h>

#include <IOKit/IODeviceMemory.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include "AppleMacRiscPCI.h"

#define ALLOC_AGP_RANGE	0

#ifndef kIOAGPCommandValueKey
#define kIOAGPCommandValueKey	"IOAGPCommandValue"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOPCIBridge

OSDefineMetaClassAndStructors(AppleMacRiscPCI, IOPCIBridge)

OSDefineMetaClassAndStructors(AppleMacRiscVCI, AppleMacRiscPCI)

OSDefineMetaClassAndStructors(AppleMacRiscAGP, AppleMacRiscPCI)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleMacRiscPCI::start( IOService * provider )
{
    IOPCIPhysicalAddress 	ioAddrCell;
    IOPhysicalAddress		ioPhys;
    IOPhysicalAddress		ioPhysLen;
    OSArray *			array;
    IODeviceMemory::InitElement	rangeList[ 3 ];
    IORegistryEntry *		bridge;
    OSData *			busProp;

    IORegistryEntry		*parent, *uniNRegEntry;
    OSData               	*tmpData;
    
    if( !IODTMatchNubWithKeys(provider, "('pci', 'vci')"))
	return( false);

    if( IODTMatchNubWithKeys(provider, "'uni-north'"))
	configDataOffsetMask = 0x7;
    else
	configDataOffsetMask = 0x3;      

    if( 0 == (lock = IOSimpleLockAlloc()))
	return( false );
    
    ioAddrCell.physHi.bits 	= 0;
    ioAddrCell.physHi.s.space 	= kIOPCIIOSpace;
    ioAddrCell.physMid 		= 0;
    ioAddrCell.physLo 		= 0;
    ioAddrCell.lengthHi 	= 0;
    ioAddrCell.lengthLo 	= 0x10000;

    //
    // Disable AGP for P58/P69 U2 (v2.0) due to stablity issues
    //
    parent = provider->getParentEntry(gIODTPlane);
    
    // Get the Uni-N Version.
    uniNRegEntry = parent->fromPath("/uni-n", gIODTPlane);
        
    if (uniNRegEntry != NULL)
    {
        tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("device-rev"));
            
        if (tmpData != NULL)
            uniNVersion = *(long *)tmpData->getBytesNoCopy();
    }
            
    bridge = provider;

    if( ! IODTResolveAddressCell( bridge, (UInt32 *) &ioAddrCell,
		&ioPhys, &ioPhysLen) ) {

	IOLog("%s: couldn't find my base\n", getName());
	return( false);
    }

    /* define more explicit ranges */

    rangeList[0].start	= ioPhys;
    rangeList[0].length = ioPhysLen;
    rangeList[1].start	= ioPhys + 0x00800000;
    rangeList[1].length = 4;
    rangeList[2].start	= ioPhys + 0x00c00000;
    rangeList[2].length	= 4;

    IORangeAllocator * platformRanges;
    platformRanges = IOService::getPlatform()->getPhysicalRangeAllocator();
    assert( platformRanges );
    platformRanges->allocateRange( ioPhys, 0x01000000 );

    array = IODeviceMemory::arrayFromList( rangeList, 3 );
    if( !array)
	return( false);

    provider->setDeviceMemory( array );
    array->release();
    ioMemory = (IODeviceMemory *) array->getObject( 0 );

    /* map registers */

    if( (configAddrMap = provider->mapDeviceMemoryWithIndex( 1 )))
        configAddr = (volatile UInt32 *) configAddrMap->getVirtualAddress();
    if( (configDataMap = provider->mapDeviceMemoryWithIndex( 2 )))
        configData = (volatile UInt8 *) configDataMap->getVirtualAddress();

    if( !configAddr || !configData)
	return( false);

    busProp = (OSData *) bridge->getProperty("bus-range");
    if( busProp)
	primaryBus = *((UInt32 *) busProp->getBytesNoCopy());
    
    return( super::start( provider));
}

bool AppleMacRiscPCI::configure( IOService * provider )
{
    UInt32	modeSelects, addressSelects;
    UInt32	index;
    bool	ok;

    if( getProvider()->getProperty( "DisableRDG") != 0 ) {
      modeSelects = configRead32( getBridgeSpace(), kMacRISCModeSelect );
      modeSelects |= kMacRISCModeSelectRDGBit;
      configWrite32( getBridgeSpace(), kMacRISCModeSelect, modeSelects );
    }

    addressSelects = configRead32( getBridgeSpace(), kMacRISCAddressSelect );

    coarseAddressMask	= addressSelects >> 16;
    fineAddressMask	= addressSelects & 0xffff;

    for( index = 0; index < 15; index++ ) {
	if( coarseAddressMask & (1 << index)) {
	    ok = addBridgeMemoryRange( index << 28, 0x10000000, true );
	}
    }

//    if( coarseAddressMask & (1 << 15))	// F segment
	for( index = 0; index < 15; index++ ) {
	    if( fineAddressMask & (1 << index)) {
		ok = addBridgeMemoryRange( (0xf0 | index) << 24,
						0x01000000, true );
	    }
	}

    ok = addBridgeIORange( 0, 0x10000 );

    return( super::configure( provider));
}

void AppleMacRiscPCI::free()
{
    if( configAddrMap)
	configAddrMap->release();
    if( configDataMap)
	configDataMap->release();
    if( lock)
	IOSimpleLockFree( lock);

    super::free();
}

IODeviceMemory * AppleMacRiscPCI::ioDeviceMemory( void )
{
    return( ioMemory);
}

IODeviceMemory * AppleMacRiscVCI::ioDeviceMemory( void )
{
    return( 0 );
}

bool AppleMacRiscVCI::configure( IOService * provider )
{
    addBridgeMemoryRange( 0x90000000, 0x10000000, true );

    return( AppleMacRiscPCI::configure( provider));
}

UInt8 AppleMacRiscPCI::firstBusNum( void )
{
    return( primaryBus );
}

UInt8 AppleMacRiscPCI::lastBusNum( void )
{
    return( firstBusNum() );
}

IOPCIAddressSpace AppleMacRiscPCI::getBridgeSpace( void )
{
    IOPCIAddressSpace	space;

    space.bits = 0;
    space.s.busNum = primaryBus;
    space.s.deviceNum = kBridgeSelfDevice;

    return( space );
}

inline bool AppleMacRiscPCI::setConfigSpace( IOPCIAddressSpace space,
					UInt8 offset )
{
    UInt32	addrCycle;

    offset &= 0xfc;
    if( space.s.busNum == primaryBus) {

	if( space.s.deviceNum < kBridgeSelfDevice)
	    return( false);

        // primary config cycle
        addrCycle = (	  (1 << space.s.deviceNum)
			| (space.s.functionNum << 8)
			| offset );

    } else {
        // pass thru config cycle
        addrCycle = (	  (space.bits)
			| offset
			| 1 );
    }

    do {
        OSWriteSwapInt32( configAddr, 0, addrCycle);
        eieio();
    } while( addrCycle != OSReadSwapInt32( configAddr, 0 ));
    eieio();

    return( true );
}


UInt32 AppleMacRiscPCI::configRead32( IOPCIAddressSpace space,
					UInt8 offset )
{
    UInt32		data;
    IOInterruptState	ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        offset = offset & configDataOffsetMask & 4;

        data = OSReadSwapInt32( configData, offset );
        eieio();

    } else
	data = 0xffffffff;

    IOSimpleLockUnlockEnableInterrupt( lock, ints );

    return( data );
}

void AppleMacRiscPCI::configWrite32( IOPCIAddressSpace space, 
					UInt8 offset, UInt32 data )
{
    IOInterruptState ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        offset = offset & configDataOffsetMask & 4;

        OSWriteSwapInt32( configData, offset, data );
        eieio();
	/* read to sync */
        (void) OSReadSwapInt32( configData, offset );
        eieio();
	sync();
	isync();
    }

    IOSimpleLockUnlockEnableInterrupt( lock, ints );
}

UInt16 AppleMacRiscPCI::configRead16( IOPCIAddressSpace space,
					UInt8 offset )
{
    UInt16		data;
    IOInterruptState	ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        offset = offset & configDataOffsetMask & 6;

        data = OSReadSwapInt16( configData, offset );
        eieio();

    } else
	data = 0xffff;

    IOSimpleLockUnlockEnableInterrupt( lock, ints );

    return( data );
}

void AppleMacRiscPCI::configWrite16( IOPCIAddressSpace space, 
					UInt8 offset, UInt16 data )
{
    IOInterruptState ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        offset = offset & configDataOffsetMask & 6;

        OSWriteSwapInt16( configData, offset, data );
        eieio();
	/* read to sync */
        (void) OSReadSwapInt16( configData, offset );
        eieio();
	sync();
	isync();
    }

    IOSimpleLockUnlockEnableInterrupt( lock, ints );
}

UInt8 AppleMacRiscPCI::configRead8( IOPCIAddressSpace space,
					UInt8 offset )
{
    UInt16		data;
    IOInterruptState	ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        offset = offset & configDataOffsetMask;

        data = configData[ offset ];
        eieio();

    } else
	data = 0xff;

    IOSimpleLockUnlockEnableInterrupt( lock, ints );

    return( data );
}

void AppleMacRiscPCI::configWrite8( IOPCIAddressSpace space, 
					UInt8 offset, UInt8 data )
{
    IOInterruptState ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        offset = offset & configDataOffsetMask;

        configData[ offset ] = data;
        eieio();
	/* read to sync */
        data = configData[ offset ];
        eieio();
	sync();
	isync();
    }

    IOSimpleLockUnlockEnableInterrupt( lock, ints );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super AppleMacRiscPCI

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleMacRiscAGP::configure( IOService * provider )
{
    if( !findPCICapability( getBridgeSpace(), kIOPCIAGPCapability, &targetAGPRegisters ))
	return( false );

    if( false && (uniNVersion >= 0x10)) {
	// causes problems with nv25 SBA
	gartCtrl |= kGART_DISSBADET;
    }

    return( super::configure( provider));
}

IOPCIDevice * AppleMacRiscAGP::createNub( OSDictionary * from )
{
    IOPCIDevice *	nub;
    IOPCIAddressSpace	space;
    bool		isAGP;
    UInt8		masterAGPRegisters;
    
    spaceFromProperties( from, &space);

    isAGP = (  (space.s.deviceNum != getBridgeSpace().s.deviceNum)
	    && findPCICapability( space, kIOPCIAGPCapability, &masterAGPRegisters ));

    // more of P58/P69 AGP disable code from start
    if( isAGP && (uniNVersion == 0x20))
    {
        isAGP = false;
        IOLog("AGP mode disabled on this machine\n");
    }
    
    if( isAGP) {
	nub = new IOAGPDevice;
        if( nub)
            ((IOAGPDevice *)nub)->masterAGPRegisters = masterAGPRegisters;
	from->setObject( kIOAGPBusFlagsKey, getProperty(kIOAGPBusFlagsKey));
    } else
        nub = super::createNub( from );

    return( nub );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn AppleMacRiscAGP::createAGPSpace( IOAGPDevice * master, 
				      IOOptionBits options,
				      IOPhysicalAddress * address, 
				      IOPhysicalLength * length )
{
    IOReturn		err;
    IOPCIAddressSpace 	target = getBridgeSpace();
    IOPhysicalLength	agpLength;
    IOPhysicalAddress	gartPhys;

    enum { agpSpacePerPage = 4 * 1024 * 1024 };
    enum { agpBytesPerGartByte = 1024 };
    enum { alignLen = 4 * 1024 * 1024 - 1 };

    destroyAGPSpace( master );

    agpCommandMask = 0xffffffff;
    agpCommandMask &= ~kIOAGPFastWrite;
//  agpCommandMask &= ~kIOAGPSideBandAddresssing;

    {
	// There's an nVidia NV11 ROM (revision 1017) that says that it can do fast writes,
	// but can't, and can often lock the machine up when fast writes are enabled.
	
	#define kNVIDIANV11EntryName	"NVDA,NVMac"
	#define kNVROMRevPropertyName 	"rom-revision"
	#define kNVBadRevision			'1017'

	const UInt32    badRev = kNVBadRevision;
	OSData *	data;

	if( (0 == strcmp( kNVIDIANV11EntryName, master->getName()))
	 && (data = OSDynamicCast(OSData, master->getProperty(kNVROMRevPropertyName)))
	 && (data->isEqualTo( &badRev, sizeof(badRev) )))

	    agpCommandMask &= ~kIOAGPFastWrite;
    }

    agpLength = *length;
    if( !agpLength)
	agpLength = 32 * 1024 * 1024;

    agpLength = (agpLength + alignLen) & ~alignLen;

    err = kIOReturnVMError;
    do {

	gartLength = agpLength / agpBytesPerGartByte;
	gartArray = (volatile UInt32 *) IOMallocContiguous( 
				gartLength, 4096, &gartPhys );
	if( !gartArray)
	    continue;
        // IOMapPages( kernel_map, gartArray, gartPhys, gartLength, kIOMapInhibitCache );
        bzero( (void *) gartArray, gartLength);

#if ALLOC_AGP_RANGE
        IORangeAllocator * platformRanges
            = getPlatform()->getPhysicalRangeAllocator();
	for( agpBaseIndex = 0xf; agpBaseIndex > 0; agpBaseIndex--) {
	    systemBase = agpBaseIndex * 0x10000000;
	    if( platformRanges->allocateRange( systemBase, agpLength )) {
		systemLength = agpLength;
		break;
	    }
	}
#else
        agpBaseIndex	= 0;
        systemBase	= 0;
        systemLength	= agpLength;
#endif
	if( !systemLength)
	    continue;

	agpRange = IORangeAllocator::withRange( agpLength, 4096 );
	if( !agpRange)
	    continue;

        *address = systemBase;
        *length = systemLength;
#if 0
        coarseAddressMask |= (1 << agpBaseIndex);
        configWrite32( target, kMacRISCAddressSelect,
				(coarseAddressMask << 16) | fineAddressMask );
#endif
        configWrite32( target, kUniNAGP_BASE, agpBaseIndex << 28 );

        assert( 0 == (gartPhys & 0xfff));
        configWrite32( target, kUniNGART_BASE,
     			gartPhys | (agpLength / agpSpacePerPage));

        err = kIOReturnSuccess;

    } while( false );

    if( kIOReturnSuccess == err)
        setAGPEnable( master, true, 0 );
    else
	destroyAGPSpace( master );

    return( err );
}

IOReturn AppleMacRiscAGP::getAGPSpace( IOAGPDevice * master,
                                  IOPhysicalAddress * address, 
				  IOPhysicalLength * length )
{
    if( systemLength) {

        if( address)
            *address = systemBase;
        if( length)
            *length  = systemLength;
        return( kIOReturnSuccess );

    } else
        return( kIOReturnNotReady );
}

IOReturn AppleMacRiscAGP::destroyAGPSpace( IOAGPDevice * master )
{

    setAGPEnable( master, false, 0 );

    if( gartArray) {
	IOFreeContiguous( (void *) gartArray, gartLength);
	gartArray = 0;
    }
    if( agpRange) {
	agpRange->release();
	agpRange = 0;
    }
    if( systemLength) {
#if ALLOC_AGP_RANGE
        IORangeAllocator * platformRanges
                        = getPlatform()->getPhysicalRangeAllocator();
	platformRanges->deallocate( systemBase, systemLength);
#endif
	systemLength = 0;
    }

    return( kIOReturnSuccess );
}

IORangeAllocator * AppleMacRiscAGP::getAGPRangeAllocator(
					IOAGPDevice * master )
{
//    if( agpRange)	agpRange->retain();
    return( agpRange );
}

IOOptionBits AppleMacRiscAGP::getAGPStatus( IOAGPDevice * master,
					    IOOptionBits options )
{
    IOPCIAddressSpace 	target = getBridgeSpace();

    return( configRead32( target, kUniNINTERNAL_STATUS ) );
}

IOReturn AppleMacRiscAGP::commitAGPMemory( IOAGPDevice * master, 
				      IOMemoryDescriptor * memory,
				      IOByteCount agpOffset,
				      IOOptionBits options )
{
    IOPCIAddressSpace 	target = getBridgeSpace();
    IOReturn		err = kIOReturnSuccess;
    UInt32		offset = 0;
    IOPhysicalAddress	physAddr;
    IOByteCount		len;
    IOByteCount         agpFlushStart, flushLength;

//    ok = agpRange->allocate( memory->getLength(), &agpOffset );

    assert( agpOffset < systemLength );
    agpOffset /= (page_size / 4);
    agpFlushStart = agpOffset;
    while( (physAddr = memory->getPhysicalSegment( offset, &len ))) {

	offset += len;
	len = (len + 0xfff) & ~0xfff;
	while( len > 0) {
	    OSWriteLittleInt32( gartArray, agpOffset,
				((physAddr & ~0xfff) | 1));
	    agpOffset += 4;
	    physAddr += page_size;
	    len -= page_size;
	}
    }

    // Just to be paranoid, I want to feed nice happy values to flush_dcache.
    // I want the start address to be cache line aligned, and I want the size
    // to be a multiple of cache line size.  For now I'm going to assume that
    // any CPU hooked to an AGP chipset using this driver has a 32-byte cache
    // line size.  Ugh.
    agpFlushStart &= ~31; // Round down to cache line boundary
    flushLength = (agpOffset - agpFlushStart);
    // Round up to 32 bytes, which hopefully is safe since we pushed the start
    // down to a 32-byte boundary first.
    flushLength = (flushLength + 31) & ~31;
    sync();
    isync();
    flush_dcache( ((vm_offset_t) gartArray)+agpFlushStart, flushLength, false);
    sync();
    isync();
    len = OSReadLittleInt32( gartArray, agpOffset - 4 );
    sync();
    isync();
    
    if( kIOAGPGartInvalidate & options) {
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN | kGART_INV );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN | kGART_2xRESET);
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN );
    }

    return( err );
}

IOReturn AppleMacRiscAGP::releaseAGPMemory( IOAGPDevice * master,
                                            IOMemoryDescriptor * memory,
                                            IOByteCount agpOffset,
                                            IOOptionBits options )
{
    IOPCIAddressSpace 	target = getBridgeSpace();
    IOReturn		err = kIOReturnSuccess;
    IOByteCount		length;
    IOByteCount         agpFlushStart, flushLength;

    if( !memory)
	return( kIOReturnBadArgument );

    length = memory->getLength();

    if( (agpOffset + length) > systemLength)
	return( kIOReturnBadArgument );

//    agpRange->deallocate( agpOffset, length );

    length = (length + 0xfff) & ~0xfff;
    agpOffset /= page_size;
    agpFlushStart = agpOffset;
    while( length > 0) {
	gartArray[ agpOffset++ ] = 0;
	length -= page_size;
    }
    
    agpFlushStart *= 4;
    agpOffset *= 4;
    agpFlushStart &= ~31;
    flushLength = (agpOffset - agpFlushStart);  // Flush full cache lines
    // Round up to 32 bytes, which hopefully is safe since we pushed the start
    // down to a 32-byte boundary first.
    flushLength = (flushLength + 31) & ~31;
    sync();
    isync();
    flush_dcache( ((vm_offset_t) gartArray)+agpFlushStart, flushLength, false);
    sync();
    isync();
    length = OSReadLittleInt32( gartArray, 4 * (agpOffset - 1) );
    sync();
    isync();

    if( kIOAGPGartInvalidate & options) {
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN | kGART_INV );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN | kGART_2xRESET);
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN );
    }

    return( err );
}

IOReturn AppleMacRiscAGP::setAGPEnable( IOAGPDevice * _master,
					bool enable, IOOptionBits options )
{
    IOReturn		err = kIOReturnSuccess;
    IOPCIAddressSpace 	target = getBridgeSpace();
    IOPCIAddressSpace 	master = _master->space;
    UInt32		command;
    UInt32		targetStatus, masterStatus;
    UInt8		masterAGPRegisters = _master->masterAGPRegisters;

    if( enable) {

        configWrite32( target, kUniNGART_CTRL, gartCtrl | 0);

	targetStatus = configRead32( target,
                                     targetAGPRegisters + kIOPCIConfigAGPStatusOffset );
	masterStatus = configRead32( master,
                                     masterAGPRegisters + kIOPCIConfigAGPStatusOffset );

	command = kIOAGPSideBandAddresssing
                | kIOAGPFastWrite
		| kIOAGP4xDataRate | kIOAGP2xDataRate | kIOAGP1xDataRate;
	command &= targetStatus;
	command &= masterStatus;

        if (uniNVersion == 0x21)
        {
            command &= ~(kIOAGP4xDataRate);
            IOLog("AGP 4x mode disabled on this machine\n");
        }

	if( command & kIOAGP4xDataRate)
	    command &= ~(kIOAGP2xDataRate | kIOAGP1xDataRate);
	else if( command & kIOAGP2xDataRate)
	    command &= ~(kIOAGP1xDataRate);
	else if( 0 == (command & kIOAGP1xDataRate))
            return( kIOReturnUnsupported );

	command |= kIOAGPEnable;
        command &= agpCommandMask;

	if( targetStatus > masterStatus)
	    targetStatus = masterStatus;
	command |= (targetStatus & kIOAGPRequestQueueMask);

        _master->setProperty(kIOAGPCommandValueKey, &command, sizeof(command));

        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN | kGART_INV );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN );

	do {
	    configWrite32( target, targetAGPRegisters + kIOPCIConfigAGPCommandOffset, command );
	} while( (command & kIOAGPEnable) != 
	(kIOAGPEnable & configRead32( target, targetAGPRegisters + kIOPCIConfigAGPCommandOffset)));

	do {
	    configWrite32( master,
                            masterAGPRegisters + kIOPCIConfigAGPCommandOffset, command );
	} while( (command & kIOAGPEnable) != 
                    (kIOAGPEnable & configRead32( master,
                            masterAGPRegisters + kIOPCIConfigAGPCommandOffset)));

#if 0
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN | kGART_INV );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN | kGART_2xRESET);
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN );
#endif

        _master->masterState |= kIOAGPStateEnabled;

    } else {

	while( 0 == (kIOAGPIdle & configRead32( getBridgeSpace(),
					kUniNINTERNAL_STATUS )))
		{}

        configWrite32( master, masterAGPRegisters + kIOPCIConfigAGPCommandOffset, 0 );
        configWrite32( target, targetAGPRegisters + kIOPCIConfigAGPCommandOffset, 0 );
#if 0
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_EN | kGART_INV );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | 0 );
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_2xRESET);
        configWrite32( target, kUniNGART_CTRL, gartCtrl | 0 );
#endif
        configWrite32( target, kUniNGART_CTRL, gartCtrl | kGART_2xRESET );

        _master->masterState &= ~kIOAGPStateEnabled;
    }

    return( err );
}

IOReturn AppleMacRiscAGP::resetAGPDevice( IOAGPDevice * master,
                                          IOOptionBits options )
{
    IOReturn ret;

    if( master->masterState & kIOAGPStateEnablePending) {
        ret = setAGPEnable( master, true, 0 );
        master->masterState &= ~kIOAGPStateEnablePending;

    } else {
#if 1
        ret = setAGPEnable( master, false, 0 );
        ret = setAGPEnable( master, true, 0 );
#endif
        ret = kIOReturnSuccess;
    }
    return( ret );
}

IOReturn AppleMacRiscAGP::saveDeviceState( IOPCIDevice * device,
                                           IOOptionBits options )
{
    IOReturn		ret;
    IOAGPDevice *	agpDev;
    UInt32		agpSave[3];
    IOPCIAddressSpace 	target = getBridgeSpace();

    if( (agpDev = OSDynamicCast( IOAGPDevice, device))) {
        if( agpDev->masterState & kIOAGPStateEnabled)
            setAGPEnable( agpDev, false, 0 );
        agpSave[0] = configRead32( target, kUniNAGP_BASE );
        agpSave[1] = configRead32( target, kUniNGART_BASE );
        agpSave[2] = configRead32( target, targetAGPRegisters + kIOPCIConfigAGPCommandOffset );
    }

    ret = super::saveDeviceState( device, options);

    if( agpDev && (ret == kIOReturnSuccess)) {
        agpDev->savedConfig[ kUniNAGP_BASE / 4 ] = agpSave[0];
        agpDev->savedConfig[ kUniNGART_BASE / 4 ] = agpSave[1];
        agpDev->savedConfig[ (targetAGPRegisters + kIOPCIConfigAGPCommandOffset) / 4 ] = agpSave[2];
    }

    return( ret );
}

IOReturn AppleMacRiscAGP::restoreDeviceState( IOPCIDevice * device,
                                              IOOptionBits options )
{
    IOReturn		ret;
    IOAGPDevice *	agpDev;
    UInt32		agpSave[3];
    IOPCIAddressSpace 	target = getBridgeSpace();

    agpDev = OSDynamicCast( IOAGPDevice, device);
    if( agpDev && device->savedConfig) {
        agpSave[0] = agpDev->savedConfig[ kUniNAGP_BASE / 4 ];
        agpSave[1] = agpDev->savedConfig[ kUniNGART_BASE / 4 ];
        agpSave[2] = agpDev->savedConfig[ (targetAGPRegisters + kIOPCIConfigAGPCommandOffset) / 4 ];
    }
    
    ret = super::restoreDeviceState( device, options);

    if( agpDev && (kIOReturnSuccess == ret)) {
        configWrite32( target, kUniNAGP_BASE, agpSave[0] );
        configWrite32( target, kUniNGART_BASE, agpSave[1] );
        // soon, grasshopper
        if( kIOAGPEnable & agpSave[2])
            agpDev->masterState |= kIOAGPStateEnablePending;
        else
            agpDev->masterState &= ~kIOAGPStateEnablePending;
    }
    
    return( ret );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
