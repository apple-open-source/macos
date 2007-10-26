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

#include <IOKit/sbp2/IOFireWireSBP2ORB.h>
#include <IOKit/sbp2/IOFireWireSBP2Login.h>
#include <IOKit/sbp2/IOFireWireSBP2LUN.h>
#include "FWDebugging.h"

#define FIREWIREPRIVATE
#include <IOKit/firewire/IOFireWireController.h>
#undef FIREWIREPRIVATE

#include <IOKit/firewire/IOFWSimpleContiguousPhysicalAddressSpace.h>

OSDefineMetaClassAndStructors( IOFireWireSBP2ORB, IOCommand );

OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 0);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 1);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 2);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 3);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 4);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 5);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 6);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 7);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ORB, 8);

enum
{
	kFWSBP2CommandMaxPacketSizeOverride			= (1 << 12)
};
	
// initWithLogin
//
// initializer

bool IOFireWireSBP2ORB::initWithLogin( IOFireWireSBP2Login * login )
{
    bool res 			= true;
    fLogin 				= login;
    fLUN				= getFireWireLUN();
    fUnit 				= getFireWireUnit();
    fControl			= fUnit->getController();
 
    // these should already be zeroed
    
    fTimeoutTimerSet				= false;
    fRefCon							= NULL;
    fBufferAddressSpaceAllocated 	= false;
    fBufferDescriptor 				= NULL;
    fMaxPayloadSize					= 0;
    fCommandFlags					= 0;
    fTimeoutDuration				= 0;
	fDMACommand						= NULL;
	fConstraintOptions				= 0;
    // init super
   if( !IOCommand::init() )
        return false;
    
    IOReturn status = allocateResources();
    if( status != kIOReturnSuccess )
        res = false;
    
    return res;
}

// allocateResources
//
// create orb and pageTable

IOReturn IOFireWireSBP2ORB::allocateResources( void )
{
     IOReturn status = kIOReturnSuccess;

	status = setBufferConstraints( kFWSBP2MaxPageClusterSize, 1, 0 );
	
//	IOLog("IOFireWireSBP2ORB::allocateResources - setBufferConstraints, status = 0x%08lx\n", status );
    //
    // create ORB
    //
	
	if( status == kIOReturnSuccess )
    {	
		// calculate orb size
		UInt32 orbSize = sizeof(FWSBP2ORB) - 4 + fLogin->getMaxCommandBlockSize();
		status = allocateORB( orbSize );
	}

//	IOLog("IOFireWireSBP2ORB::allocateResources - allocateORB, status = 0x%08lx\n", status );

	//
	// create page table
	//
	
	if( status == kIOReturnSuccess )
    {
        status = allocatePageTable( PAGE_SIZE / sizeof(FWSBP2PTE) );  // default size
	}

//	IOLog("IOFireWireSBP2ORB::allocateResources - allocatePageTable, status = 0x%08lx\n", status );

	//
	// create timer
	//
	
	if( status == kIOReturnSuccess )
	{
        status = allocateTimer();
    }
    
    //
    // clean up on error
    //
    
    if( status != kIOReturnSuccess )
    {
        deallocateTimer();
        deallocateORB();
        deallocatePageTable();
    }

//	IOLog("IOFireWireSBP2ORB::allocateResources - status = 0x%08lx\n", status );
     
    return status;
}

void IOFireWireSBP2ORB::release() const
{
	if( getRetainCount() >= 2 ) 
		IOCommand::release(2);
}

void IOFireWireSBP2ORB::free( void )
{
    FWKLOG(( "IOFireWireSBP2ORB<0x%08lx> : free\n", (UInt32)this ));

    removeORB( this );

    deallocateTimer();
    deallocatePageTable();
    deallocateBufferAddressSpace();
	deallocateORB();
		
    IOCommand::free();
}

//////////////////////////////////////////////////////////////////////
// ORB
//

// allocateORB
//
//

IOReturn IOFireWireSBP2ORB::allocateORB( UInt32 orbSize )
{
	IOReturn	status = kIOReturnSuccess;

	IOFWSimpleContiguousPhysicalAddressSpace * physical_space;
	if( status == kIOReturnSuccess )
    {
        physical_space = fUnit->createSimpleContiguousPhysicalAddressSpace( orbSize, kIODirectionOut );
     	if( physical_space == NULL )
        	status = kIOReturnNoMemory;
    }

//	IOLog( "IOFireWireSBP2ORB::allocateORB - 1 status = 0x%08lx\n", status );

	if( status == kIOReturnSuccess )
	{
		fORBPhysicalAddressSpace = (IOFWAddressSpace*)physical_space;
		fORBDescriptor = physical_space->getMemoryDescriptor();
		fORBPhysicalAddress = physical_space->getFWAddress();
		fORBBuffer = (FWSBP2ORB *)physical_space->getVirtualAddress();
	}

//	IOLog( "IOFireWireSBP2ORB::allocateORB - 2 status = 0x%08lx\n", status );
	
	if( status == kIOReturnSuccess )
    {
        status = fORBPhysicalAddressSpace->activate();
    }

//	IOLog( "IOFireWireSBP2ORB::allocateORB - 3 status = 0x%08lx\n", status );

    if( status == kIOReturnSuccess )
    {
		//zzz shouldn't be able to write to ORBs
		//zzz of course when running via the physical unit there's nothing to stop writes anyway
		
        fORBPseudoAddressSpace = IOFWPseudoAddressSpace::simpleRW( fControl, &fORBPseudoAddress, fORBDescriptor );
       	if ( fORBPseudoAddressSpace == NULL )
        	status = kIOReturnNoMemory;
    }

//	IOLog( "IOFireWireSBP2ORB::allocateORB - 4 status = 0x%08lx\n", status );

    if( status == kIOReturnSuccess )
    {
        status = fORBPseudoAddressSpace->activate();
		
		FWKLOG( ( "IOFireWireSBP2ORB<0x%08lx> : created orb at phys: 0x%04lx.%08lx psuedo: 0x%04lx.%08lx of size %d\n", 
				(UInt32)this, fORBPhysicalAddress.addressHi, fORBPhysicalAddress.addressLo, fORBPseudoAddress.addressHi, fORBPseudoAddress.addressLo, orbSize ) );
    }

//	IOLog( "IOFireWireSBP2ORB::allocateORB - 5 status = 0x%08lx\n", status );
	
	if( status != kIOReturnSuccess )
	{
		deallocateORB();
	}
	
	return status;
}

void IOFireWireSBP2ORB::deallocateORB( void )
{
    IOReturn status = kIOReturnSuccess;
    
	// deallocate physical address space
    if( fORBPhysicalAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fORBPhysicalAddressSpace->deactivate();
        fORBPhysicalAddressSpace->release();
		fORBPhysicalAddressSpace = NULL;
    }

	// deallocate pseudo address space
    if( fORBPseudoAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fORBPseudoAddressSpace->deactivate();
        fORBPseudoAddressSpace->release();
		fORBPseudoAddressSpace = NULL;
	}
}

/////////////////////////////////////////////////////////////////////
// command execution
//

// prepareORBForExecution
//
//

void IOFireWireSBP2ORB::prepareORBForExecution( void )
{
    
    // make sure orb points nowhere
    fORBBuffer->nextORBAddressHi = OSSwapHostToBigInt32(0x80000000);
    fORBBuffer->nextORBAddressLo = OSSwapHostToBigInt32(0x00000000);

    // update data descriptor with local node ID
    UInt32 generation;	// Hmm, shouldn't this be checked?
    UInt16 unitNode;
    UInt16 localNode;
    fUnit->getNodeIDGeneration( generation, unitNode, localNode );
    fORBBuffer->dataDescriptorHi &= OSSwapHostToBigInt32(0x0000ffff);
    fORBBuffer->dataDescriptorHi |= OSSwapHostToBigInt32(((UInt32)localNode) << 16);

    // clear options
    fORBBuffer->options &= OSSwapHostToBigInt16(0x000f);

    // mark for notify if requested
    if( fCommandFlags & kFWSBP2CommandCompleteNotify )
        fORBBuffer->options |= OSSwapHostToBigInt16(0x8000);

    // mark ORB rq_fmt. if kFWSBP2CommandNormalORB is set, the zero is already in place.
    // the driver is not supposed to set more than one of these flags
    if( fCommandFlags & kFWSBP2CommandReservedORB )
        fORBBuffer->options |= OSSwapHostToBigInt16(0x2000);

    if( fCommandFlags & kFWSBP2CommandVendorORB )
        fORBBuffer->options |= OSSwapHostToBigInt16(0x4000);

    if( fCommandFlags & kFWSBP2CommandDummyORB )
        fORBBuffer->options |= OSSwapHostToBigInt16(0x6000);

    // mark as "read" if requested
    if( fCommandFlags & kFWSBP2CommandTransferDataFromTarget )
        fORBBuffer->options |= OSSwapHostToBigInt16(0x0800);

    //
    // set speed
    //
    
    IOFWSpeed speed = fUnit->FWSpeed();
    switch( speed )
    {
		case kFWSpeed800MBit:
			fORBBuffer->options |= OSSwapHostToBigInt16(0x0300);
			break;

        case kFWSpeed400MBit:
            fORBBuffer->options |= OSSwapHostToBigInt16(0x0200);
             break;

        case kFWSpeed200MBit:
            fORBBuffer->options |= OSSwapHostToBigInt16(0x0100);
            break;

        default:
            // default options is |= 0x0000
            break;
     }

    //
    // calculate transfer size
    //

    UInt32 transferSizeBytes = 4096; // start at max packet size
	
	bool size_override = (fCommandFlags & kFWSBP2CommandMaxPacketSizeOverride);
	
	if( !size_override )
	{
		// clip by ARMDMAMax for performance
		UInt32 ARDMAMax = fLogin->getARDMMax();
		if( (fCommandFlags & kFWSBP2CommandTransferDataFromTarget) &&	// if this is a read
			(ARDMAMax != 0) )											// and we've got an ARDMA clip
		{
			transferSizeBytes = ARDMAMax;
		}
	}
	
    // trim by max payload sizes
    UInt32 loginMaxPayloadSize = fLogin->getMaxPayloadSize();
    if( loginMaxPayloadSize != 0 && loginMaxPayloadSize < transferSizeBytes )
        transferSizeBytes = loginMaxPayloadSize;

    if( fMaxPayloadSize != 0 && fMaxPayloadSize < transferSizeBytes )
        transferSizeBytes = fMaxPayloadSize;

    // find the largest power of two less than or equal to transferSizeBytes/4
    UInt32 transferSizeLog = 0;
    while( (transferSizeBytes >= 8) && (transferSizeLog < 15) )
    {
        transferSizeBytes >>= 1;
        transferSizeLog++;
    }

	if( !size_override )
	{
		// trim by maxPackLog
		UInt32 maxPackLog = fUnit->maxPackLog(!(fCommandFlags & kFWSBP2CommandTransferDataFromTarget));
		maxPackLog -= 2; // convert to quads
		if( maxPackLog < transferSizeLog )
			transferSizeLog = maxPackLog;
	}
	else
	{
		UInt32 maxPackLog = 7;
		
		IOFWSpeed speed = fUnit->FWSpeed();
		switch( speed )
		{
			case kFWSpeed800MBit:
				maxPackLog = 10;
				break;

			case kFWSpeed400MBit:
				maxPackLog = 9;
				 break;

			case kFWSpeed200MBit:
				maxPackLog = 8;
				break;

			default:
				break;
		 }
		 
		if( maxPackLog < transferSizeLog )
			transferSizeLog = maxPackLog;		 
	}
	
    // set transfer size, actual max is 2 ^ (size + 2) bytes (or 2 ^ size quads)
    fORBBuffer->options |= OSSwapHostToBigInt16(transferSizeLog << 4);
}

// prepareFastStartPacket
//
//

void IOFireWireSBP2ORB::prepareFastStartPacket( IOBufferMemoryDescriptor * descriptor )
{
	UInt32 offset = 16;
	UInt32 length = descriptor->getLength();
	
	//
	// write orb
	//
	
	UInt32 orbLength = fORBDescriptor->getLength();
	if( length < (offset + orbLength) )
	{
		IOLog( "IOFireWireSBP2ORB<0x%08lx>::prepareFastStartPacket - fast start packet length (%d) < orblength (%d) + 16\n", this, length, orbLength );
	}
	descriptor->writeBytes( offset, fORBBuffer, orbLength );

	offset += orbLength;
	
	//
	// write page table
	//
		
	FWSBP2PTE pte;
	UInt32 pageTableOffset = 0;
	UInt32 pageTableSize = fPTECount * sizeof(FWSBP2PTE);

	while( offset < length && pageTableOffset < pageTableSize )
	{
		if( (length - offset) < sizeof(FWSBP2PTE) )
		{
			IOLog( "IOFireWireSBP2ORB<0x%08lx>::prepareFastStartPacket - fast start packet not full, yet pte doesn't fit\n", this );
			break;
		}
						
		fPageTableDescriptor->readBytes( pageTableOffset, &pte, sizeof(FWSBP2PTE) );
		descriptor->writeBytes( offset, &pte, sizeof(FWSBP2PTE) );

		
		offset += sizeof(FWSBP2PTE);
		pageTableOffset += sizeof(FWSBP2PTE);
	}
	
	if( offset > length )
	{
		IOLog( "IOFireWireSBP2ORB<0x%08lx>::prepareFastStartPacket - offset > length\n", this );
	}
	
	//
	// trim descriptor size if necessary
	//
	
	descriptor->setLength( offset );
}

#if 0
IOReturn checkMemoryInRange( IOMemoryDescriptor * memory, UInt64 mask, IODMACommand * dma_command_arg )
{
	IOReturn status = kIOReturnSuccess;

	if( memory == NULL )
	{
		status = kIOReturnBadArgument;
	}
	
	//
	// setup
	//
	
	bool memory_prepared = false;
	if( dma_command_arg == NULL )
	{
		if( status == kIOReturnSuccess )
		{
			status = memory->prepare( kIODirectionInOut );
		}
		
		if( status == kIOReturnSuccess )
		{
			memory_prepared = true;
		}
	}
	
		
	UInt64 length = 0;
	
	if( status == kIOReturnSuccess )
	{
		length = memory->getLength();
	}
	
	IODMACommand * dma_command = dma_command_arg;
	bool dma_command_prepared = false;
	if( dma_command == NULL )
	{
		if( status == kIOReturnSuccess )
		{
			dma_command = IODMACommand::withSpecification( 
													kIODMACommandOutputHost64,		// segment function
													64,								// max address bits
													0,								// max segment size
													(IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly),		// IO mapped & don't bounce buffer
													0,							// max transfer size
													0,								// page alignment
													NULL,							// mapper
													NULL );							// refcon
			if( dma_command == NULL )
				status = kIOReturnError;
			
		}
		
		if( status == kIOReturnSuccess )
		{
			// set memory descriptor and don't prepare it
			status = dma_command->setMemoryDescriptor( memory, false ); 
		}	

		if( status == kIOReturnSuccess )
		{
			status = dma_command->prepare( 0, length, true );
		}

		if( status == kIOReturnSuccess )
		{
			dma_command_prepared = true;
		}
	}
	else
	{
#if 0
	dma_command->setMemoryDescriptor( memory, false ); 
	dma_command->prepare( 0, length, true );
	dma_command_prepared = true;
#endif	
	}
	//
	// check ranges
	//

	if( status == kIOReturnSuccess )
	{
		IOLog( "checkSegments - length = %d\n", length  );
	
		UInt64 offset = 0;
		while( (offset < length) && (status == kIOReturnSuccess) )
		{
			IODMACommand::Segment64 segments[10];
			UInt32 num_segments = 10;
			status = dma_command->gen64IOVMSegments( &offset, segments, &num_segments );
			if( status == kIOReturnSuccess )
			{
				for( UInt32 i = 0; i < num_segments; i++ )
				{
					IOLog( "checkSegments - offset = 0x%016llx segments[%d].fIOVMAddr = 0x%016llx, fLength = %d\n", offset, i, segments[i].fIOVMAddr, segments[i].fLength  );
						
					if( (segments[i].fIOVMAddr & (~mask)) )
					{
						IOLog( "checkSegmentsFailed - 0x%016llx & 0x%016llx\n", segments[i].fIOVMAddr, mask );
						status = kIOReturnNotPermitted;
					//	break;
					}
				}
			}
			else
			{
				IOLog( "checkSegments - offset = %lld 0x%08lx\n", offset, status  );
			}
		}
	}
	
	//
	// clean up
	//
	
	if( dma_command_prepared )
	{
		dma_command->complete();
		dma_command_prepared = false;
	}
		
	if( dma_command && (dma_command_arg == NULL) )
	{
		dma_command->release();
		dma_command = NULL;
	}
	
	if( memory_prepared )
	{
		memory->complete();
		memory_prepared = false;
	}
	
	return status;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////
// timeouts
//

IOReturn IOFireWireSBP2ORB::allocateTimer( void )
{
    IOReturn status = kIOReturnSuccess;
    
    if( status == kIOReturnSuccess )
	{
		fTimeoutCommand = (IOFWDelayCommand*)fControl->createDelayedCmd( fTimeoutDuration * 1000,
																		 IOFireWireSBP2ORB::orbTimeoutStatic, 
																		 this );
		if( fTimeoutCommand == NULL )
			status = kIOReturnError;
	}
	
	if( status != kIOReturnSuccess )
	{
		deallocateTimer();
	}
    
    return status;
}

void IOFireWireSBP2ORB::deallocateTimer( void )
{
    // cancel timer
    if( fTimeoutTimerSet )
        cancelTimer();

	if( fTimeoutCommand )
	{
		fTimeoutCommand->release();
		fTimeoutCommand = NULL;
    }
}

// startTimer
//
//

void IOFireWireSBP2ORB::startTimer( void )
{	
    //
    // set timeout if necessary
    //

    if( fCommandFlags & kFWSBP2CommandCompleteNotify )
	{
		fInProgress = true;
		
		if( fTimeoutDuration != 0 )
		{
			IOReturn ORBtimeoutSubmitStatus;
			
			FWKLOG( ( "IOFireWireSBP2ORB<0x%08lx> : set timeout\n", (UInt32)this ) );
			
			fTimeoutTimerSet = true;
			ORBtimeoutSubmitStatus = fTimeoutCommand->submit();
		
			FWPANICASSERT( ORBtimeoutSubmitStatus == kIOReturnSuccess );
		}
	}
}

// isTimerSet
//
// 

bool IOFireWireSBP2ORB::isTimerSet( void )
{
    return fInProgress;
}

// cancelTimer
//
//

void IOFireWireSBP2ORB::cancelTimer( void )
{
    FWKLOG( ( "IOFireWireSBP2ORB<0x%08lx> : cancel timer\n", (UInt32)this ) );
	
    // cancel timer
    if( fTimeoutTimerSet )
	{
        fTimeoutCommand->cancel( kIOReturnAborted );
	}
	else
	{
		fInProgress = false;
	}
}

// orb timeout handler
//
// static wrapper & virtual method

void IOFireWireSBP2ORB::orbTimeoutStatic( void *refcon, IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    ((IOFireWireSBP2ORB*)refcon)->orbTimeout( status, bus, fwCmd );
}

void IOFireWireSBP2ORB::orbTimeout( IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    fTimeoutTimerSet = false;
    fInProgress = false;
	
    if( status == kIOReturnTimeout )
    {
        FWKLOG( ( "IOFireWireSBP2ORB<0x%08lx> : orb timeout\n", (UInt32)this ) );
		sendTimeoutNotification( this );
    }
    else
    {
        FWKLOG( ( "IOFireWireSBP2ORB<0x%08lx> : orb timeout cancelled\n", (UInt32)this ) );
    }
}

//////////////////////////////////////////////////////////////////////
// page table
//

// allocatePageTable
//
//

IOReturn IOFireWireSBP2ORB::allocatePageTable( UInt32 entryCount )
{
	IOReturn	status = kIOReturnSuccess;
	
	deallocatePageTable();	
	
	//
    // create page table
    //
    
    // allocate mem for page table
    fPageTableSize = sizeof(FWSBP2PTE) * entryCount;
	UInt64 phys_mask = fControl->getFireWirePhysicalAddressMask();
	fPageTableDescriptor = IOBufferMemoryDescriptor::inTaskWithPhysicalMask( kernel_task, kIODirectionOutIn, fPageTableSize, phys_mask );
    if( fPageTableDescriptor == NULL )
        status =  kIOReturnNoMemory;

    if( status == kIOReturnSuccess )
    {
        fPageTableDescriptor->setLength( fPageTableSize );
	}

	bool dma_command_prepared = false;
	IODMACommand * dma_command = NULL;
	if( status == kIOReturnSuccess )
    {
		dma_command = IODMACommand::withSpecification( 
												kIODMACommandOutputHost64,		// segment function
												48,								// max address bits
												fPageTableSize,					// max segment size
												(IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly),		// IO mapped & don't bounce buffer
												fPageTableSize,					// max transfer size
												0,								// no alignment
												NULL,							// mapper
												NULL );							// refcon
		if( dma_command == NULL )
			status = kIOReturnNoMemory;
	}

//	IOLog( "IOFireWireSBP2ORB::allocatePageTable (1) - status = 0x%08lx\n", status );
	
	if( status == kIOReturnSuccess )
	{
		// set memory descriptor and don't prepare it
		status = dma_command->setMemoryDescriptor( fPageTableDescriptor, false ); 
	}	

	if( status == kIOReturnSuccess )
	{
		status = dma_command->prepare( 0, fPageTableSize, true );
	}

	if( status == kIOReturnSuccess )
	{
		dma_command_prepared = true;
	}

//	IOLog( "IOFireWireSBP2ORB::allocatePageTable (2) - status = 0x%08lx\n", status );
	
	IODMACommand::Segment64 segment;
	UInt32 numSegments = 1;
	if( status == kIOReturnSuccess )
	{
		UInt64 offset = 0;
		status = dma_command->gen64IOVMSegments( &offset, &segment, &numSegments );
	}
	
	if( status == kIOReturnSuccess )
	{
		if( numSegments != 1 )
		{
			status = kIOReturnNoMemory;
		}
	}

//	IOLog( "IOFireWireSBP2ORB::allocatePageTable (3) - status = 0x%08lx\n", status );
	
	if( status == kIOReturnSuccess )
	{
		fPageTablePhysicalLength = segment.fLength;
		fPageTablePhysicalAddress.nodeID = 0x0000;		// invalid node id
		fPageTablePhysicalAddress.addressHi = (segment.fIOVMAddr >> 32) & 0x000000000000ffffULL;
		fPageTablePhysicalAddress.addressLo = segment.fIOVMAddr & 0x00000000ffffffffULL;
	}

	if( dma_command_prepared )
	{
		dma_command->complete();
		dma_command_prepared = false;
	}
	
	if( dma_command )
	{
		dma_command->release();
		dma_command = NULL;
	}

//	IOLog( "IOFireWireSBP2ORB::allocatePageTable (1) - status = 0x%08lx segment = 0x%016llx len = %d\n", status, segment.fIOVMAddr, segment.fLength );
	
    // allocate and register a physical address space for the page table

    if( status == kIOReturnSuccess )
    {
        fPageTablePhysicalAddressSpace = fUnit->createPhysicalAddressSpace( fPageTableDescriptor );
       	if ( fPageTablePhysicalAddressSpace == NULL )
        	status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        status = fPageTablePhysicalAddressSpace->activate();
    }

    // allocate and register a pseudo address space for the page table

    if( status == kIOReturnSuccess )
    {
		//zzz shouldn't be able to write to the page table
		//zzz of course when run physically there's nothing to stop writes anyway

        fPageTablePseudoAddressSpace = IOFWPseudoAddressSpace::simpleRW( fControl, &fPageTablePseudoAddress, fPageTableDescriptor );
       	if ( fPageTablePseudoAddressSpace == NULL )
        	status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        status = fPageTablePseudoAddressSpace->activate();
    }
	
	if( status != kIOReturnSuccess )
	{
		deallocatePageTable();
	}
	
	return status;
}

// deallocatePageTable
//
//

void IOFireWireSBP2ORB::deallocatePageTable( void )
{
    IOReturn status = kIOReturnSuccess;
    
	// deallocate physical address space
    if( fPageTablePhysicalAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fPageTablePhysicalAddressSpace->deactivate();
        fPageTablePhysicalAddressSpace->release();
		fPageTablePhysicalAddressSpace = NULL;
    }

	// deallocate pseudo address space
    if( fPageTablePseudoAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fPageTablePseudoAddressSpace->deactivate();
        fPageTablePseudoAddressSpace->release();
		fPageTablePseudoAddressSpace = NULL;
	}

    // free mem
    if( fPageTableDescriptor != NULL )
	{
        fPageTableDescriptor->release();
		fPageTableDescriptor = NULL;
	}
		
	fPageTableSize = 0;
}

// setBufferConstraints
//
//

IOReturn IOFireWireSBP2ORB::setBufferConstraints( UInt64 maxSegmentSize, UInt32 alignment, UInt32 options )
{
	IOReturn status = kIOReturnSuccess;

	if( fBufferAddressSpaceAllocated )
	{
		status = kIOReturnBusy;
	}
	
	if( status == kIOReturnSuccess )
	{
		fConstraintOptions = options;
		
		deallocateBufferAddressSpace();
		
		//
		// create DMACommand
		//
		
		if( status == kIOReturnSuccess )
		{
			UInt32 address_bits = fControl->getFireWirePhysicalAddressBits();
#if 0
		// testing
		//	address_bits = 29;
#endif
			
			UInt64 segment_size = maxSegmentSize;
			if( (segment_size > kFWSBP2MaxPageClusterSize) || (segment_size == 0) )
				segment_size = kFWSBP2MaxPageClusterSize;
					
			fDMACommand = IODMACommand::withSpecification( 
													kIODMACommandOutputHost64,		// segment function
													address_bits,					// max address bits
													segment_size,					// max segment size
													IODMACommand::kMapped,			// IO mapped & allow bounce buffering
													0,								// max transfer size
													alignment,						// alignment
													NULL,							// mapper
													NULL );							// refcon
			if( fDMACommand == NULL )
				status = kIOReturnError;
		}
	}

	// allocate and register an address space for the buffers
	
//	IOLog( "IOFireWireSBP2ORB::setBufferConstraints - (1) status = 0x%08lx\n", status );
	
	if( status == kIOReturnSuccess )
	{
		fBufferAddressSpace = fUnit->createPhysicalAddressSpace( NULL );
		if( fBufferAddressSpace == NULL )
		{
			status = kIOReturnNoMemory;
		}
	}

//	IOLog( "IOFireWireSBP2ORB::setBufferConstraints - (2) status = 0x%08lx\n", status );

	if( status == kIOReturnSuccess )
	{
		((IOFWPhysicalAddressSpace*)fBufferAddressSpace)->setDMACommand( fDMACommand );
	}
	
	return status;
}

// prepareBufferAddressSpace
//
//

IOReturn IOFireWireSBP2ORB::prepareBufferAddressSpace( IOMemoryDescriptor * memoryDescriptor )
{
	if( fBufferAddressSpaceAllocated )
	{
		completeBufferAddressSpace();
	}

	fBufferDescriptor = memoryDescriptor;
	fBufferDescriptor->retain();

	((IOFWPhysicalAddressSpace*)fBufferAddressSpace)->setMemoryDescriptor( fBufferDescriptor );
	fBufferAddressSpace->activate();
	
	if( fConstraintOptions & kFWSBP2ConstraintForceDoubleBuffer )
	{
		((IOFWPhysicalAddressSpace*)fBufferAddressSpace)->synchronize( IODMACommand::kForceDoubleBuffer | kIODirectionOut );
	}
	
	fBufferAddressSpaceAllocated = true;
	
	return kIOReturnSuccess;
}

// completeBufferAddressSpace
//
//

IOReturn IOFireWireSBP2ORB::completeBufferAddressSpace( void )
{
	if( fBufferAddressSpace )
	{
		fBufferAddressSpace->deactivate();
		((IOFWPhysicalAddressSpace*)fBufferAddressSpace)->setMemoryDescriptor( NULL );
	}
	
	if( fBufferDescriptor )
	{
		fBufferDescriptor->release();
		fBufferDescriptor = NULL;
	}
	
	fBufferAddressSpaceAllocated = false;

	if( fORBBuffer )
	{
		fORBBuffer->dataDescriptorHi = 0;
		fORBBuffer->dataDescriptorLo = 0;
		fORBBuffer->options &= OSSwapHostToBigInt16(0xfff0); // no page table
		fORBBuffer->dataSize = 0;
	}
	
	return kIOReturnSuccess;
}

// deallocateBufferAddressSpace
//
//

void IOFireWireSBP2ORB::deallocateBufferAddressSpace( void )
{
    fControl->closeGate();
    
	fPTECount = 0;
	
    if( fBufferAddressSpaceAllocated )
    {
		completeBufferAddressSpace();		
	}
	
	if( fBufferAddressSpace )
	{
		fBufferAddressSpace->release();
		fBufferAddressSpace = NULL;
	}
	
	if( fDMACommand )
	{
		fDMACommand->release();
		fDMACommand = NULL;
	}

	// no page table
	if( fORBBuffer )
	{
		fORBBuffer->dataDescriptorHi	= 0;
		fORBBuffer->dataDescriptorLo	= 0;
		fORBBuffer->options				&= OSSwapHostToBigInt16(0xfff0);
	// no page table
		fORBBuffer->dataSize			= 0;
	}
	
	fPTECount = 0;

    fControl->openGate();
}

// setCommandBuffersAsRanges
//
// allocates and prepares a memory descriptor from given virtual ranges
// and then calls setCommandBuffers with the descriptor

IOReturn IOFireWireSBP2ORB::setCommandBuffersAsRanges(  IOVirtualRange * ranges,
                                                        UInt32           withCount,
                                                        IODirection      withDirection,
                                                        task_t           withTask,
                                                        UInt32	         offset,
                                                        UInt32	         length )
{
    IOReturn			status = kIOReturnSuccess;
    IOMemoryDescriptor *	memory = NULL;

    if( status == kIOReturnSuccess )
    {
        memory = IOMemoryDescriptor::withRanges( ranges, withCount, withDirection, withTask );
        if( !memory )
            status = kIOReturnNoMemory;
    }
    
    if( status == kIOReturnSuccess )
    {
        status = memory->prepare( withDirection );
    }

    if( status == kIOReturnSuccess )
    {
        status = setCommandBuffers( memory, offset, length );
    }

	if( memory )
	{
		memory->release();
	}
	
    return status;
}

// setCommandBuffersAsRanges64
//
//

IOReturn IOFireWireSBP2ORB::setCommandBuffersAsRanges64(	IOAddressRange *	ranges,
															uint64_t			withCount,
															IODirection			withDirection,
															task_t				withTask,
															uint64_t			offset,
															uint64_t			length )
{
    IOReturn			status = kIOReturnSuccess;
    IOMemoryDescriptor *	memory = NULL;

    if( status == kIOReturnSuccess )
    {
        memory = IOMemoryDescriptor::withAddressRanges( ranges, withCount, withDirection, withTask );
        if( !memory )
            status = kIOReturnNoMemory;
    }
    
    if( status == kIOReturnSuccess )
    {
        status = memory->prepare( withDirection );
    }

    if( status == kIOReturnSuccess )
    {
        status = setCommandBuffers( memory, offset, length );
    }

	if( memory )
	{
		memory->release();
	}
	
    return status;

}

// releaseCommandBuffers
//
// tell SBP2 that the IO is done and command buffers can be released

IOReturn IOFireWireSBP2ORB::releaseCommandBuffers( void )
{
    return setCommandBuffers( NULL );
}

// setCommandBuffers
//
// write a page table from the given memoryDecriptor

IOReturn IOFireWireSBP2ORB::setCommandBuffers( IOMemoryDescriptor * memoryDescriptor, UInt32 offset, UInt32 length )
{
    IOReturn status = kIOReturnSuccess;
 
	fControl->closeGate();

    //
    // deallocate previous mapping
    //

    // I obsolete the old address space even if we fail at mapping a new one
    // that seems like it would be the more expected behavior

	completeBufferAddressSpace();

	//
	// bail if no memory descriptor
	//
	
    if( memoryDescriptor == NULL )
    {
		fControl->openGate();

	//	IOLog( "IOFireWireSBP2ORB::setCommandBuffers status = 0x%08lx\n", status );
	
		return kIOReturnSuccess;
	}

#if 0   
   	// memoryDescriptor is valid from here on

	IOReturn check_status = checkMemoryInRange( memoryDescriptor, 0x000000001fffffff, NULL );
#endif

	//
	// map buffers
	//
	
	// allocate and register an address space for the buffers
	
	if( status == kIOReturnSuccess )
	{
		status = prepareBufferAddressSpace( memoryDescriptor );
	}

#if 0
	if( check_status != kIOReturnSuccess )
	{
		IOLog( "After prepare\n" );
		check_status = checkMemoryInRange( memoryDescriptor, 0x000000001fffffff, fDMACommand );
		IOLog( "After prepare = 0x%08lx\n", check_status );
	}
#endif
	
   	//
   	// fix up length if necessary
   	//
   	
    if( status == kIOReturnSuccess )
    {
        // use memory descriptor's length if length = 0

        if( length == 0 )
        {
            length = fDMACommand->getMemoryDescriptor()->getLength();
		}
		
#if FWLOGGING
        // I occasionally get memory descriptors with bogus lengths

        UInt32 tempLength = fDMACommand->getMemoryDescriptor()->getLength();

		if(length != tempLength)
		{
        	FWKLOG( ( "IOFireWireSBP2ORB<0x%08lx> : ### buffer length = %d, memDescriptor length = %d ###\n", (UInt32)this, length, tempLength  ) );
        // length = tempLength;
		}
#endif
        
	}
               
    //
    // write page table
    //

	UInt32 		pte = 0;
    
	if( status == kIOReturnSuccess ) 
	{    	
		UInt32 	ptes_allocated = 0;
		bool 	done = false;
		
		// algorithm is usually 1 pass, but may take 2 
		// passes if we need to grow the page table
		
		// we never shrink the page table
		
		while( !done )
		{
			ptes_allocated = fPageTableSize / sizeof(FWSBP2PTE);
			
			vm_size_t pos = offset;
 			
 			pte = 0;
			
			while( pos < (length + offset) )
			{
	    		UInt64 	phys = 0;
   				UInt64 	lengthOfSegment = 0;

				// get next segment
	
				IODMACommand::Segment64 segment;
				UInt32 numSegments = 1;
				if( status == kIOReturnSuccess )
				{
					UInt64 dma_pos = pos;  // don't mangle pos
					status = fDMACommand->gen64IOVMSegments( &dma_pos, &segment, &numSegments );
				//	IOLog( "IOFireWireSBP2ORB::setCommandBuffers - pos = %d, status = 0x%08lx\n", pos, status );
				//	IOSleep( 10 );
				}
				
				if( status == kIOReturnSuccess )
				{
					if( numSegments != 1 )
					{
						status = kIOReturnNoMemory;
					}
				}

				if( status == kIOReturnSuccess )
				{
					phys = segment.fIOVMAddr;
					lengthOfSegment = segment.fLength;
				}
	
				// nothing more to map
				
				if( phys == 0 )
				{
					status = kIOReturnBadArgument;  // buffer not large enough
					done = true;
					break;
				}
				
				// map until we are done or we run out of segment

				// DMA command constraints should make this code unnecessary
				
				while( (pos < (length + offset)) && (lengthOfSegment != 0) )
				{
					UInt32 		step = 0;
    				UInt32		toMap = 0;

					// 64k max page table entry, so we do it in chunks

					if( lengthOfSegment > kFWSBP2MaxPageClusterSize )
						step = kFWSBP2MaxPageClusterSize;
					else
						step = lengthOfSegment;
	
					lengthOfSegment -= step;
	
					// clip mapping by length if necessary
					
					if( (pos + step) > (length + offset) )
						toMap = length + offset - pos;
					else
						toMap = step;
	
					// map it if we've got anything to map
					
					if( toMap != 0 )
					{	
						pte++;

						if( pte <= ptes_allocated )
						{
							FWSBP2PTE entry;
		
							entry.segmentLength = OSSwapHostToBigInt16( toMap );
							entry.segmentBaseAddressHi = OSSwapHostToBigInt16( ((phys >> 32) & 0x000000000000ffffULL) );
							entry.segmentBaseAddressLo = OSSwapHostToBigInt32( (phys & 0x00000000ffffffffULL) );
		
							fPageTableDescriptor->writeBytes( (pte-1) * sizeof(FWSBP2PTE), &entry, sizeof(FWSBP2PTE) );
					
						//	IOLog( "IOFireWireSBP2ORB<0x%08lx> : PTE = %d, size = %d\n", (UInt32)this, pte-1, toMap  );
							
		 //                  FWKLOG( ( "IOFireWireSBP2ORB<0x%08lx> : PTE = %d, size = %d\n", (UInt32)this, pte-1, toMap  ) );
						}
						
						// move to new page table entry and beginning of unmapped memory
						phys += toMap;
						pos += toMap;
					}
				}
			}

			FWKLOG( ( "IOFireWireSBP2ORB<0x%08lx> : number of required PTE's = %d\n", (UInt32)this, pte ) );
			
			if( pte <= ptes_allocated )
			{
				done = true;
			}
			else
			{
				if( fCommandFlags & kFWSBP2CommandFixedSize )
				{
					status = kIOReturnNoMemory;
					done = true;
				}
				else
				{
					// reallocate
					status = allocatePageTable( pte );
					if( status != kIOReturnSuccess )
					{
						status = kIOReturnNoMemory;
						done = true;
					}
				}
			}			
		}
	}
	    
    //
    // fill in orb
    //

    if( status == kIOReturnSuccess )
    {		
        if( pte == 0 )
        {
			// no page table
            fORBBuffer->dataDescriptorHi	= 0;
            fORBBuffer->dataDescriptorLo	= 0;
            fORBBuffer->options				&= OSSwapHostToBigInt16(0xfff0);	// no page table
            fORBBuffer->dataSize			= 0;
			fPTECount = 0;
        }
        else 
		{
			FWSBP2PTE firstEntry;
			
			fPageTableDescriptor->readBytes( 0, &firstEntry, sizeof(FWSBP2PTE) );

			// very strictly speaking, if we don't use a page table, the buffer
			// must be quadlet-aligned.  SBP-2 is vague about this.  We'll enforce it.

			if( pte == 1 && !(firstEntry.segmentBaseAddressLo & OSSwapHostToBigInt32(0x00000003)) )  // make sure quadlet aligned
			{
				// directly addressed buffer
				fORBBuffer->dataDescriptorHi	= 0;			// tbd later
				fORBBuffer->dataDescriptorLo	= firstEntry.segmentBaseAddressLo;
				fORBBuffer->options				&= OSSwapHostToBigInt16(0xfff0);		// no page table
				fORBBuffer->dataSize			= firstEntry.segmentLength;
				fPTECount = 0;
			}
			else if( pte * sizeof(FWSBP2PTE) <= fPageTablePhysicalLength )
			{
				// use physical unit
				fORBBuffer->dataDescriptorHi	= OSSwapHostToBigInt32(fPageTablePhysicalAddress.addressHi);			// tbd later
				fORBBuffer->dataDescriptorLo	= OSSwapHostToBigInt32(fPageTablePhysicalAddress.addressLo);
				fORBBuffer->options				&= OSSwapHostToBigInt16(0xfff0);
				fORBBuffer->options				|= OSSwapHostToBigInt16(0x0008);		// unrestricted page table
				fORBBuffer->dataSize			= OSSwapHostToBigInt16(pte);
				fPTECount = pte;
			}
			else
			{
				// use software address space
				fORBBuffer->dataDescriptorHi	= OSSwapHostToBigInt32(fPageTablePseudoAddress.addressHi);			// tbd later
				fORBBuffer->dataDescriptorLo	= OSSwapHostToBigInt32(fPageTablePseudoAddress.addressLo);
				fORBBuffer->options				&= OSSwapHostToBigInt16(0xfff0);
				fORBBuffer->options				|= OSSwapHostToBigInt16(0x0008);		// unrestricted page table
				fORBBuffer->dataSize			= OSSwapHostToBigInt16(pte);			
				fPTECount = pte;
			}
		}
    }

	if( status != kIOReturnSuccess )
	{
		completeBufferAddressSpace();
	}
	
    fControl->openGate();

//	IOLog( "IOFireWireSBP2ORB::setCommandBuffers return status = 0x%08lx\n", status );
       
    return status;
}

// getCommandBufferDesceiptor
//
//

IOMemoryDescriptor * IOFireWireSBP2ORB::getCommandBufferDescriptor( void )
{
	return fBufferDescriptor;
}

/////////////////////////////////////////////////////////////////////
// command block
//

// setCommandBlock
//
// set the command block portion of the orb with buffer and length

IOReturn IOFireWireSBP2ORB::setCommandBlock( void * buffer, UInt32 length )
{
    fControl->closeGate();
    
    UInt32 maxCommandBlockSize = fLogin->getMaxCommandBlockSize();
    
    if( length > maxCommandBlockSize )
        return kIOReturnNoMemory;

    bzero( &fORBBuffer->commandBlock[0], maxCommandBlockSize );
    bcopy( buffer, &fORBBuffer->commandBlock[0], length );

    fControl->openGate();

    return kIOReturnSuccess;
}

// setCommandBlock
//
// set the command block portion of the orb with memory descriptor

IOReturn IOFireWireSBP2ORB::setCommandBlock( IOMemoryDescriptor * memory )
{
    fControl->closeGate();
    
    UInt32 maxCommandBlockSize = fLogin->getMaxCommandBlockSize();
    IOByteCount length = memory->getLength();
	
    if( length > maxCommandBlockSize )
        return kIOReturnNoMemory;

    bzero( &fORBBuffer->commandBlock[0], maxCommandBlockSize );
    memory->readBytes(0, &fORBBuffer->commandBlock[0], length );
	
    fControl->openGate();

    return kIOReturnSuccess;
}

/////////////////////////////////////////////////////////////////////
// accessors
//

void IOFireWireSBP2ORB::setCommandFlags( UInt32 flags )
{
    fCommandFlags = flags;
}

UInt32 IOFireWireSBP2ORB::getCommandFlags( void )
{
    return fCommandFlags;
}

// get / set refCon

void IOFireWireSBP2ORB::setRefCon( void * refCon )
{
    fRefCon = (UInt64)refCon;
}

void * IOFireWireSBP2ORB::getRefCon( void )
{
    return (void*)fRefCon;
}

// get / set refCon 64

void IOFireWireSBP2ORB::setRefCon64( UInt64 refCon )
{
    fRefCon = refCon;
}

UInt64 IOFireWireSBP2ORB::getRefCon64( void )
{
    return fRefCon;
}

// get / set max payload size

void IOFireWireSBP2ORB::setMaxPayloadSize( UInt32 maxPayloadSize )
{
    fMaxPayloadSize = maxPayloadSize;
}

UInt32 IOFireWireSBP2ORB::getMaxPayloadSize( void )
{
    return fMaxPayloadSize;
}

// get / set command timeout

void IOFireWireSBP2ORB::setCommandTimeout( UInt32 timeout )
{
    IOReturn ORBTimeoutReinitStatus;
    
    fControl->closeGate();
    
    fTimeoutDuration = timeout;
	ORBTimeoutReinitStatus = fTimeoutCommand->reinit( fTimeoutDuration * 1000, IOFireWireSBP2ORB::orbTimeoutStatic, this );    
    
    //zzz perhaps this should pass error back to user
    FWPANICASSERT( ORBTimeoutReinitStatus == kIOReturnSuccess );
    
    fControl->openGate();
}

UInt32 IOFireWireSBP2ORB::getCommandTimeout( void )
{
    return fTimeoutDuration;
}

// get / set command generation

void IOFireWireSBP2ORB::setCommandGeneration( UInt32 gen )
{
    fGeneration = gen;
}

UInt32 IOFireWireSBP2ORB::getCommandGeneration( void )
{
     return fGeneration;
}

// returns the ORB address

void IOFireWireSBP2ORB::getORBAddress( FWAddress * address )
{
	if( fCommandFlags & kFWSBP2CommandVirtualORBs )
	{
		*address = fORBPseudoAddress;
	}
	else
	{
		*address = fORBPhysicalAddress;
	}
}

// sets the address of the next ORB field
void IOFireWireSBP2ORB::setNextORBAddress( FWAddress address )
{
    fORBBuffer->nextORBAddressHi = OSSwapHostToBigInt32((UInt32)address.addressHi);
    fORBBuffer->nextORBAddressLo = OSSwapHostToBigInt32(address.addressLo);
}

// get login
IOFireWireSBP2Login * IOFireWireSBP2ORB::getLogin( void )
{
    return fLogin;
}

void IOFireWireSBP2ORB::setToDummy( void )
{
    // set rq_fmt to 3 (NOP) in case it wasn't fetched yet
    fORBBuffer->options |= OSSwapHostToBigInt16(0x6000);
}

bool IOFireWireSBP2ORB::isAppended( void )
{
	return fIsAppended;
}

void IOFireWireSBP2ORB::setIsAppended( bool state )
{
	fIsAppended = state;
}

UInt32 IOFireWireSBP2ORB::getFetchAgentWriteRetries( void )
{
    return fFetchAgentWriteRetries;
}

void IOFireWireSBP2ORB::setFetchAgentWriteRetries( UInt32 retries )
{
    fFetchAgentWriteRetries = retries;
}

// getFetchAgentWriteRetryInterval
//
//

UInt32 IOFireWireSBP2ORB::getFetchAgentWriteRetryInterval( void )
{
	return fFetchAgentWriteRetryInterval;
}

// setFetchAgentWriteRetryInterval
//
//

void IOFireWireSBP2ORB::setFetchAgentWriteRetryInterval( UInt32 interval )
{
	fFetchAgentWriteRetryInterval = interval;
}

//////////////////////////////////////////////////////////////////////////////////////////
// friend class wrappers

// login friend class wrappers

IOFireWireUnit * IOFireWireSBP2ORB::getFireWireUnit( void )
{ 
	return fLogin->getFireWireUnit(); 
}

IOFireWireSBP2LUN * IOFireWireSBP2ORB::getFireWireLUN( void )
{ 
	return fLogin->getFireWireLUN(); 
}

IOReturn IOFireWireSBP2ORB::removeORB( IOFireWireSBP2ORB * orb )
{ 
	return fLogin->removeORB( orb ); 
}

void IOFireWireSBP2ORB::sendTimeoutNotification( IOFireWireSBP2ORB * orb )
{ 
	fLogin->sendTimeoutNotification( orb ); 
}
