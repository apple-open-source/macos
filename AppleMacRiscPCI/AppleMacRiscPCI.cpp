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
#include <IOKit/IOMapper.h>
#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include "AppleMacRiscPCI.h"

#define NO_FASTWRITE		1
#define NO_NVIDIA_FASTWRITE	1

#define ALLOC_AGP_RANGE		0

#ifndef kIOAGPCommandValueKey
#define kIOAGPCommandValueKey	"IOAGPCommandValue"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOPCIBridge

OSDefineMetaClassAndStructors(AppleMacRiscPCI, IOPCIBridge)

OSDefineMetaClassAndStructors(AppleMacRiscVCI, AppleMacRiscPCI)

OSDefineMetaClassAndStructors(AppleMacRiscAGP, AppleMacRiscPCI)

OSDefineMetaClassAndStructors(AppleMacRiscHT, IOPCIBridge)

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

    IORegistryEntry		*uniNRegEntry;
    OSData               	*tmpData;
    
    // Match the name of the provider to weed out the memory controller
    if( !IODTMatchNubWithKeys(provider, "('pci', 'vci')"))
	return( false);

    // Match the compatible property to find modern MacRISC PCI bridges
    if( IODTMatchNubWithKeys(provider, "('uni-north', 'u3-agp')"))
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
    // Get the Uni-N Version.
    uniNRegEntry = IORegistryEntry::fromPath("/uni-n", gIODTPlane);
    if (!uniNRegEntry)
	uniNRegEntry = IORegistryEntry::fromPath("/u3", gIODTPlane);
    if (uniNRegEntry)
    {
        tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("device-rev"));
        if (tmpData)
		{
            uniNVersion = *(long *)tmpData->getBytesNoCopy();
	uniNRegEntry->release();
			//uniNVersion &= 0x3f;
		}
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
    if( busProp) {
	secondaryBus = *((UInt32 *) busProp->getBytesNoCopy());
	subordinateBus = *((UInt32 *) busProp->getBytesNoCopy() + 1);
    }
    
    return( super::start( provider));
}

bool AppleMacRiscPCI::configure( IOService * provider )
{
    UInt32	modeSelects, addressSelects;
    UInt32	index;
    bool	ok;

    if( getProvider()->getProperty( "DisableRDG") != 0 ) {
      modeSelects = configRead32( getBridgeSpace(), kMacRISCPCIModeSelect );
      modeSelects |= kMacRISCPCIModeSelectRDGBit;
      configWrite32( getBridgeSpace(), kMacRISCPCIModeSelect, modeSelects );
    }

    addressSelects = configRead32( getBridgeSpace(), kMacRISCPCIAddressSelect );

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
    return( secondaryBus );
}

UInt8 AppleMacRiscPCI::lastBusNum( void )
{
    return( subordinateBus );
}

IOPCIAddressSpace AppleMacRiscPCI::getBridgeSpace( void )
{
    IOPCIAddressSpace	space;

    space.bits = 0;
    space.s.busNum = secondaryBus;
    space.s.deviceNum = kPCIBridgeSelfDevice;

    return( space );
}

inline bool AppleMacRiscPCI::setConfigSpace( IOPCIAddressSpace space,
					UInt8 offset )
{
    UInt32	addrCycle;

    offset &= 0xfc;
    if( space.s.busNum == secondaryBus) {

	if( space.s.deviceNum < kPCIBridgeSelfDevice)
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

bool AppleMacRiscHT::start( IOService * provider )
{
    IOPhysicalAddress		ioPhysAddrs[2];
    IOPhysicalAddress		ioPhysLengths[2];
    OSArray *			array;
    IODeviceMemory::InitElement	rangeList[ 4 ];
    IODeviceMemory *		md;
    IORegistryEntry *		bridge;
    OSData *			busProp;

    bridge = provider;

    md = provider->getDeviceMemoryWithIndex(0);
    if (md == 0)
	return 0;
    ioPhysAddrs[0] = md->getPhysicalAddress();
    ioPhysLengths[0] = md->getLength();

    md = provider->getDeviceMemoryWithIndex(1);
    if (md == 0)
	return 0;
    ioPhysAddrs[1] = md->getPhysicalAddress();
    ioPhysLengths[1] = md->getLength();

    /* define more explicit ranges */
    
    rangeList[0].start	= ioPhysAddrs[0];
    rangeList[0].length = 0x01000000UL;
    rangeList[1].start	= ioPhysAddrs[0] + 0x01000000;
    rangeList[1].length	= 0x01000000UL;
    rangeList[2].start	= ioPhysAddrs[0] + 0x02000000;
    rangeList[2].length = 0x00400000UL;
    rangeList[3].start	= ioPhysAddrs[1];
    rangeList[3].length = ioPhysLengths[1];

    IORangeAllocator * platformRanges;
    platformRanges = IOService::getPlatform()->getPhysicalRangeAllocator();
    assert( platformRanges );
    platformRanges->allocateRange( ioPhysAddrs[0], ioPhysLengths[0] );

    array = IODeviceMemory::arrayFromList( rangeList, 4 );
    if( !array)
	return( false);

    provider->setDeviceMemory( array );
    ioMemory = (IODeviceMemory *) array->getObject( 2 );
    array->release();

    /* map registers */

    if( (configType0Map = provider->mapDeviceMemoryWithIndex( 0 )))
        configType0 = (volatile UInt8 *) configType0Map->getVirtualAddress();
    if( (configType1Map = provider->mapDeviceMemoryWithIndex( 1 )))
        configType1 = (volatile UInt8 *) configType1Map->getVirtualAddress();
    if( (configSelfMap = provider->mapDeviceMemoryWithIndex( 3 )))
        configSelf = (volatile UInt8 *) configSelfMap->getVirtualAddress();

    if( !configType0 || !configType1 || !configSelf)
	return( false);

    busProp = (OSData *) bridge->getProperty("bus-range");
    if( busProp)
	primaryBus = *((UInt32 *) busProp->getBytesNoCopy());

    return( super::start( provider));
}

bool AppleMacRiscHT::configure( IOService * provider )
{
    UInt32	addressSelects;
    UInt32	index;
    bool	ok;

    addressSelects = configRead32( getBridgeSpace(), kMacRISCHTAddressSelect );

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

void AppleMacRiscHT::free()
{
    if( configType0Map)
	configType0Map->release();
    if( configType1Map)
	configType1Map->release();
    if( configSelfMap)
	configSelfMap->release();

    super::free();
}

IODeviceMemory * AppleMacRiscHT::ioDeviceMemory( void )
{
    return( ioMemory);
}

IOPCIAddressSpace AppleMacRiscHT::getBridgeSpace( void )
{
    IOPCIAddressSpace	space;

    space.bits = 0;
    space.s.busNum = primaryBus;
    space.s.deviceNum = kHTBridgeSelfDevice;

    return( space );
}

volatile UInt8 * AppleMacRiscHT::setConfigSpace( IOPCIAddressSpace space,
                                            UInt8 offset )
{
    volatile UInt8 * configData;

    offset &= 0xfc;
    if( space.s.busNum == primaryBus) {

	if( space.s.deviceNum == kHTBridgeSelfDevice) {
            // primary config cycle on self
            configData = configSelf + (offset << 2);
        } else {
            // primary config cycle
            configData = configType0 + ((space.bits & 0xffffff00) | offset);
        }
    } else {

        // pass thru config cycle
        configData = configType1 + ((space.bits & 0xffffff00) | offset);
    }

    return( configData );
}

UInt32 AppleMacRiscHT::configRead32( IOPCIAddressSpace space,
					UInt8 offset )
{
    volatile UInt8 * configData;
    UInt32	     data;

    configData = setConfigSpace( space, offset );

    offset = offset & 3 & 4;

    data = OSReadSwapInt32( configData, offset );
    eieio();

    return( data );
}

void AppleMacRiscHT::configWrite32( IOPCIAddressSpace space, 
					UInt8 offset, UInt32 data )
{
    volatile UInt8 * configData;

    configData = setConfigSpace( space, offset );

    offset = offset & 3 & 4;

    OSWriteSwapInt32( configData, offset, data );
    eieio();
    /* read to sync */
    (void) OSReadSwapInt32( configData, offset );
    eieio();
    sync();
    isync();
}

UInt16 AppleMacRiscHT::configRead16( IOPCIAddressSpace space,
					UInt8 offset )
{
    volatile UInt8 * configData;
    UInt16	     data;

    configData = setConfigSpace( space, offset );

    offset = offset & 3 & 6;

    data = OSReadSwapInt16( configData, offset );
    eieio();

    return( data );
}

void AppleMacRiscHT::configWrite16( IOPCIAddressSpace space, 
					UInt8 offset, UInt16 data )
{
    volatile UInt8 * configData;

    configData = setConfigSpace( space, offset );

    offset = offset & 3 & 6;

    OSWriteSwapInt16( configData, offset, data );
    eieio();
    /* read to sync */
    (void) OSReadSwapInt16( configData, offset );
    eieio();
    sync();
    isync();
}

UInt8 AppleMacRiscHT::configRead8( IOPCIAddressSpace space,
					UInt8 offset )
{
    volatile UInt8 * configData;
    UInt16	     data;

    configData = setConfigSpace( space, offset );

    offset = offset & 3;

    data = configData[ offset ];
    eieio();

    return( data );
}

void AppleMacRiscHT::configWrite8( IOPCIAddressSpace space, 
					UInt8 offset, UInt8 data )
{
    volatile UInt8 * configData;

    configData = setConfigSpace( space, offset );

    offset = offset & 3;

    configData[ offset ] = data;
    eieio();
    /* read to sync */
    data = configData[ offset ];
    eieio();
    sync();
    isync();
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
	gartCtrl |= kGART_DIS_SBA_DET;
    }

    isU3 = IODTMatchNubWithKeys(provider, "u3-agp");
    if (isU3) {
	
        uniNVersion &= 0x3f;

        gartCtrl |= kGART_PERF_RD;
	
        if(uniNVersion > 0x33)
        {
            /* B2B_GNT seems broken on U3 Twins */
            //gartCtrl |= kGART_B2B_GNT;
            //gartCtrl |= kGART_FAST_DDR;
        }
	
        isU32     = (uniNVersion > 0x30);
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
    if( isAGP 
     && ((0 != getProvider()->getProperty("AAPL,noAGP")) || (uniNVersion == 0x20)))
    {
        isAGP = false;
        IOLog("AGP disabled on this machine\n");
    }
    
    if( isAGP)
    {
	UInt64	   flags;
	OSNumber * num;

	num = OSDynamicCast(OSNumber, getProperty(kIOAGPBusFlagsKey));
	if (num)
	    flags = num->unsigned64BitValue();
	else
	    flags = 0;

	if (isU32)
	    flags &= ~kIOAGPGartInvalidate;
	else
	    flags |= kIOAGPGartInvalidate;
#if 0
	if (isU3 || (uniNVersion < 0x08))
	    flags |= kIOAGPDisablePageSpans;
#else
	if ((uniNVersion < 0x08) || (isU3 && (uniNVersion < 0x33)))
	    flags |= kIOAGPDisablePageSpans;
#endif
	if (isU3 && (uniNVersion < 0x34))
	    flags |= kIOAGPDisableUnaligned;
	if (uniNVersion < 0x20)
	    flags |= kIOAGPDisableAGPWrites;
	if (uniNVersion == 0x30)
	    flags |= kIOAGPDisablePCIReads | kIOAGPDisablePCIWrites;

	num = OSNumber::withNumber(flags, 64);
	nub = new IOAGPDevice;
        if (nub)
            ((IOAGPDevice *)nub)->masterAGPRegisters = masterAGPRegisters;
	if (num)
	{
	    from->setObject(kIOAGPBusFlagsKey, num);
	    num->release();
	}
    }
    else
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
    UInt64		gartPhys;
    OSData *		data;

    enum { agpSpacePerPage = 4 * 1024 * 1024 };
    enum { agpBytesPerGartByte = 1024 };
    enum { alignLen = 4 * 1024 * 1024 - 1 };

    destroyAGPSpace( master );

    if (isU32 && !dummyPage)
	dummyPage = IOBufferMemoryDescriptor::withOptions(0, page_size, page_size);
    if (dummyPage) {
	IOPhysicalLength len;
	dummyPhys = dummyPage->getPhysicalSegment64(0, &len);
        configWrite32( target, kUniNDUMMY_PAGE, dummyPhys >> PAGE_SHIFT );
    }

    agpCommandMask = 0xffffffff;
//  agpCommandMask &= ~kIOAGPSideBandAddresssing;

    if (isU3)
	agpCommandMask &= ~kIOAGPFastWrite;

    {
	// There's an nVidia NV11 ROM (revision 1017) that says that it can do fast writes,
	// but can't, and can often lock the machine up when fast writes are enabled.
	#define kNVIDIANV11EntryName	"NVDA,NVMac"
	#define kNVIDIAEntryName	"NVDA,"
	#define kNVROMRevPropertyName 	"rom-revision"
	#define kNVBadRevision		'1017'

#if NO_NVIDIA_FASTWRITE

	if( 0 == strncmp( kNVIDIAEntryName, master->getName(), strlen(kNVIDIAEntryName)))
        {
	    agpCommandMask &= ~kIOAGPFastWrite;			// NV34 systems (Q26B/Q54) has issues with this
	    if (!isU3)
		agpCommandMask &= ~kIOAGPSideBandAddresssing;	// NV34 systems (Q26B/Q54) has issues with this
        }

#else	/* NO_NVIDIA_FASTWRITE */

	const UInt32    badRev = kNVBadRevision;

	if( (0 == strcmp( kNVIDIANV11EntryName, master->getName()))
	 && (data = OSDynamicCast(OSData, master->getProperty(kNVROMRevPropertyName)))
	 && (data->isEqualTo( &badRev, sizeof(badRev)))	 )
	    agpCommandMask &= ~kIOAGPFastWrite;

#endif	/* !NO_NVIDIA_FASTWRITE */
    }

#if NO_FASTWRITE
    agpCommandMask &= ~kIOAGPFastWrite;
#endif	/* !NO_FASTWRITE */

    if ((data = OSDynamicCast(OSData, getProvider()->getProperty("AAPL,agp-clear"))))
	agpCommandMask &= ~*((UInt32 *)data->getBytesNoCopy());
    if ((data = OSDynamicCast(OSData, getProvider()->getProperty("AAPL,agp-set"))))
	agpCommandSet |= *((UInt32 *)data->getBytesNoCopy());
    if ((data = OSDynamicCast(OSData, master->getProperty("AAPL,agp-clear"))))
	agpCommandMask &= ~*((UInt32 *)data->getBytesNoCopy());
    if ((data = OSDynamicCast(OSData, master->getProperty("AAPL,agp-set"))))
	agpCommandSet |= *((UInt32 *)data->getBytesNoCopy());

    agpLength = *length;
    if( !agpLength)
	agpLength = 32 * 1024 * 1024;

    agpLength = (agpLength + alignLen) & ~alignLen;

    err = kIOReturnVMError;
    do {
        ppnum_t physPage;

	gartLength = agpLength / agpBytesPerGartByte;
        gartHandle = IOMapper::NewARTTable(gartLength, (void **) &gartArray, &physPage);
        if (!gartHandle)
            continue;
        gartPhys = ptoa_64(physPage);

        // IOMapPages( kernel_map, gartArray, gartPhys, gartLength, kIOMapInhibitCache );
	bzero( (void *) gartArray, gartLength);
	flush_dcache( (vm_offset_t) gartArray, gartLength, false);

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

	UInt32 gartBaseExt = 0;
	if (isU3)
	    gartBaseExt = (gartPhys >> 32);

        configWrite32( target, kUniNAGP_BASE, (agpBaseIndex << 28) | gartBaseExt );

        configWrite32( target, kUniNGART_BASE,
     			(gartPhys & ~PAGE_MASK) | (agpLength / agpSpacePerPage));

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

inline void AppleMacRiscAGP::configSetClearMask( IOPCIAddressSpace space, 
						 UInt8 offset, UInt32 data, UInt32 mask )
{
    IOInterruptState ints;

    ints = IOSimpleLockLockDisableInterrupt( lock );

    if( setConfigSpace( space, offset )) {

        offset = offset & configDataOffsetMask & 4;

        OSWriteSwapInt32( configData, offset, data | mask );
        eieio();
	/* read to sync */
        (void) OSReadSwapInt32( configData, offset );
        eieio();
	sync();
	isync();

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

IOReturn AppleMacRiscAGP::commitAGPMemory( IOAGPDevice * master, 
				      IOMemoryDescriptor * memory,
				      IOByteCount agpOffset,
				      IOOptionBits options )
{
    IOPCIAddressSpace 	target = getBridgeSpace();
    IOReturn		err = kIOReturnSuccess;
    UInt32		offset = 0;
    addr64_t		phys64;
    IOByteCount		len;
    IOByteCount         agpFlushStart, flushLength;

//    ok = agpRange->allocate( memory->getLength(), &agpOffset );

    assert( agpOffset < systemLength );
    agpOffset /= (page_size / 4);
    agpFlushStart = agpOffset;

    while( (phys64 = memory->getPhysicalSegment64(offset, &len)))
    {
	offset += len;
	len = (len + PAGE_MASK) & ~PAGE_MASK;
	while( len > 0) {
	    if (isU3)
		OSWriteBigInt32( gartArray, agpOffset,
				    (((UInt32)(phys64 >> PAGE_SHIFT)) | 0x80000000));
	    else
		OSWriteLittleInt32( gartArray, agpOffset,
				    ((phys64 & ~PAGE_MASK) | 1));
	    agpOffset += 4;
	    phys64 += page_size;
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

    if (isU32) {
	configSetClearMask( target, kUniNGART_CTRL, gartCtrl | kGART_EN, kGART_INV );
    
    } else if( kIOAGPGartInvalidate & options) {
	configSetClearMask( target, kUniNGART_CTRL, gartCtrl | kGART_EN, kGART_INV );
	configSetClearMask( target, kUniNGART_CTRL, gartCtrl | kGART_EN, kGART_2xRESET );
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

    length = (length + PAGE_MASK) & ~PAGE_MASK;
    agpOffset /= page_size;
    agpFlushStart = agpOffset;
    while( length > 0) {
	gartArray[agpOffset++] = 0;
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
    length = OSReadLittleInt32( gartArray, agpOffset - 4 );
    sync();
    isync();

    if (isU32) {
	configSetClearMask( target, kUniNGART_CTRL, gartCtrl | kGART_EN, kGART_INV );
    
    } else if( kIOAGPGartInvalidate & options) {
	configSetClearMask( target, kUniNGART_CTRL, gartCtrl | kGART_EN, kGART_INV );
	configSetClearMask( target, kUniNGART_CTRL, gartCtrl | kGART_EN, kGART_2xRESET );
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
                | kIOAGP3Mode
		| kIOAGP4xDataRate | kIOAGP2xDataRate | kIOAGP1xDataRate;
	command &= targetStatus;
	command &= masterStatus;

        if (uniNVersion == 0x21)
        {
            command &= ~(kIOAGP4xDataRate);
            IOLog("AGP 4x mode disabled on this machine\n");
        }

        command &= agpCommandMask;
        command |= agpCommandSet;

	if (isU32)
	    _master->setProperty("AAPL,agp3-mode", (command & kIOAGP3Mode) ? kOSBooleanTrue : kOSBooleanFalse);

	if (isU32 && (command & kIOAGP3Mode))
	{
	    command &= ~kIOAGP4xDataRate;
	    if( command & kIOAGP8xDataRateMode3)
		command &= ~(kIOAGP4xDataRateMode3 | kIOAGPFastWrite);
	    else if( 0 == (command & kIOAGP4xDataRateMode3))
		return( kIOReturnUnsupported );
	}
	else
	{
	    if( command & kIOAGP4xDataRate)
		command &= ~(kIOAGP2xDataRate | kIOAGP1xDataRate);
	    else { 
		if (isU3)
		    return( kIOReturnUnsupported );
    
		if( command & kIOAGP2xDataRate)
		    command &= ~(kIOAGP1xDataRate);
		else if( 0 == (command & kIOAGP1xDataRate))
		    return( kIOReturnUnsupported );
	    }
	}

	command |= kIOAGPEnable;

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
        agpDev->savedConfig[ 16 ] = agpSave[0];
        agpDev->savedConfig[ 17 ] = agpSave[1];
        agpDev->savedConfig[ 18 ] = agpSave[2];
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
        agpSave[0] = agpDev->savedConfig[ 16 ];
        agpSave[1] = agpDev->savedConfig[ 17 ];
        agpSave[2] = agpDev->savedConfig[ 18 ];
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
