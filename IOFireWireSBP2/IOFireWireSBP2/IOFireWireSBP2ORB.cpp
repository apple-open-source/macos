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

#include <IOKit/firewire/IOFireWireController.h>

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

    //
    // create ORB
    //
	
	if( status == kIOReturnSuccess )
    {	
		// calculate orb size
		UInt32 orbSize = sizeof(FWSBP2ORB) - 4 + fLogin->getMaxCommandBlockSize();
        status = allocateORB( orbSize );
	}

	//
	// create page table
	//
	
	if( status == kIOReturnSuccess )
    {
        status = allocatePageTable( PAGE_SIZE / sizeof(FWSBP2PTE) );  // default size
	}

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
     
    return status;
}

void IOFireWireSBP2ORB::release() const
{
	if( getRetainCount() >= 2 ) 
		IOCommand::release(2);
}

void IOFireWireSBP2ORB::free( void )
{
    FWKLOG(( "IOFireWireSBP2ORB : free\n" ));

    removeORB( this );

    deallocateTimer();
    deallocateORB();
	deallocatePageTable();
    deallocateBufferAddressSpace();

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
		
	//
    // allocate ORB
    //
    
    // allocate mem for page table
	fORBDescriptor = IOBufferMemoryDescriptor::withOptions( kIODirectionOutIn | kIOMemoryUnshared, orbSize, orbSize );
    if( fORBDescriptor == NULL )
        status =  kIOReturnNoMemory;
		
    if( status == kIOReturnSuccess )
    {
        fORBDescriptor->setLength( orbSize );

        IOPhysicalLength lengthOfSegment = 0;
        IOPhysicalAddress phys = fORBDescriptor->getPhysicalSegment( 0, &lengthOfSegment );
        FWKLOG( ( "IOFireWireSBP2ORB : ORB segment length = %d\n", lengthOfSegment ) );
            
        if( lengthOfSegment != 0 )
        {
            fORBPhysicalAddress = phys;
            FWKLOG( ( "IOFireWireSBP2ORB : ORB - phys = 0x%08lx\n", fORBPhysicalAddress ) );
        }
        else
        {
            status =  kIOReturnNoMemory;
        }
    }
	
	if( status == kIOReturnSuccess )
	{
		// if physically contiguous, must be virtually contiguous
		fORBBuffer = (FWSBP2ORB*)fORBDescriptor->getBytesNoCopy();  

		// clear orb
		bzero( fORBBuffer, orbSize );
	}

    // allocate and register a physical address space for the ORB

    if( status == kIOReturnSuccess )
    {
        fORBPhysicalAddressSpace = fUnit->createPhysicalAddressSpace( fORBDescriptor );
       	if ( fORBPhysicalAddressSpace == NULL )
        	status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        status = fORBPhysicalAddressSpace->activate();
    }

    // allocate and register a pseudo address space for the ORB
	
    if( status == kIOReturnSuccess )
    {
		//zzz shouldn't be able to write to ORBs
		//zzz of course when run physically there's nothing to stop writes anyway
		
        fORBPseudoAddressSpace = IOFWPseudoAddressSpace::simpleRW( fControl, &fORBPseudoAddress, fORBDescriptor );
       	if ( fORBPseudoAddressSpace == NULL )
        	status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
		fORBPseudoAddress.addressHi &= 0xffff;	// Mask off nodeID part.
        status = fORBPseudoAddressSpace->activate();
		
		FWKLOG( ( "IOFireWireSBP2ORB : created orb at phys: 0x%04lx.%08lx psuedo: 0x%04lx.%08lx of size %d\n", 0, fORBPhysicalAddress, fORBPseudoAddress.addressHi, fORBPseudoAddress.addressLo, orbSize ) );
    }
	
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

    // free mem
    if( fORBDescriptor != NULL )
	{
        fORBDescriptor->release();
		fORBDescriptor = NULL;
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
    fORBBuffer->nextORBAddressHi = 0x80000000;
    fORBBuffer->nextORBAddressLo = 0x00000000;

    // update data descriptor with local node ID
    UInt32 generation;	// Hmm, shouldn't this be checked?
    UInt16 unitNode;
    UInt16 localNode;
    fUnit->getNodeIDGeneration( generation, unitNode, localNode );
    fORBBuffer->dataDescriptorHi &= 0x0000ffff;
    fORBBuffer->dataDescriptorHi |= ((UInt32)localNode) << 16;

    // clear options
    fORBBuffer->options &= 0x000f;

    // mark for notify if requested
    if( fCommandFlags & kFWSBP2CommandCompleteNotify )
        fORBBuffer->options |= 0x8000;

    // mark ORB rq_fmt. if kFWSBP2CommandNormalORB is set, the zero is already in place.
    // the driver is not supposed to set more than one of these flags
    if( fCommandFlags & kFWSBP2CommandReservedORB )
        fORBBuffer->options |= 0x2000;

    if( fCommandFlags & kFWSBP2CommandVendorORB )
        fORBBuffer->options |= 0x4000;

    if( fCommandFlags & kFWSBP2CommandDummyORB )
        fORBBuffer->options |= 0x6000;

    // mark as "read" if requested
    if( fCommandFlags & kFWSBP2CommandTransferDataFromTarget )
        fORBBuffer->options |= 0x0800;

    //
    // set speed
    //
    
    IOFWSpeed speed = fUnit->FWSpeed();
    switch( speed )
    {
        case kFWSpeed400MBit:
            fORBBuffer->options |= 0x0200;
             break;

        case kFWSpeed200MBit:
            fORBBuffer->options |= 0x0100;
            break;

        default:
            // default options is |= 0x0000
            break;
     }

    //
    // calculate transfer size
    //

    UInt32 transferSizeBytes = 2048; // start at max packet size

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

    // trim by maxPackLog
    UInt32 maxPackLog = fUnit->maxPackLog(!(fCommandFlags & kFWSBP2CommandTransferDataFromTarget));
    maxPackLog -= 2; // convert to quads
    if( maxPackLog < transferSizeLog )
        transferSizeLog = maxPackLog;

    // set transfer size, actual max is 2 ^ (size + 2) bytes (or 2 ^ size quads)
    fORBBuffer->options |= transferSizeLog << 4;
}

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
			
			FWKLOG( ( "IOFireWireSBP2ORB : set timeout\n" ) );
			
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
    FWKLOG( ( "IOFireWireSBP2ORB : cancel timer\n" ) );
	
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
        FWKLOG( ( "IOFireWireSBP2ORB : orb timeout\n" ) );
		sendTimeoutNotification( this );
    }
    else
    {
        FWKLOG( ( "IOFireWireSBP2ORB : orb timeout cancelled\n" ) );
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
    fPageTableDescriptor = IOBufferMemoryDescriptor::withOptions( kIODirectionOutIn | kIOMemoryUnshared, fPageTableSize, PAGE_SIZE );
    if( fPageTableDescriptor == NULL )
        status =  kIOReturnNoMemory;

    if( status == kIOReturnSuccess )
    {
        fPageTableDescriptor->setLength( fPageTableSize );

        IOPhysicalLength lengthOfSegment = 0;
        IOPhysicalAddress phys = fPageTableDescriptor->getPhysicalSegment( 0, &lengthOfSegment );
    //    FWKLOG( ( "IOFireWireSBP2ORB : pageTable segment length = %d\n", lengthOfSegment ) );
            
        if( lengthOfSegment != 0 )
        {
            fPageTablePhysicalAddress = phys;
 //           FWKLOG( ( "IOFireWireSBP2ORB : pageTable - phys = 0x%08lx\n", fPageTablePhysicalAddress ) );
        }
        else
        {
            status =  kIOReturnNoMemory;
        }
    }

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

// deallocateBufferAddressSpace
//
//

void IOFireWireSBP2ORB::deallocateBufferAddressSpace( void )
{
    fControl->closeGate();
    
    if( fBufferAddressSpaceAllocated )
    {
        fBufferAddressSpace->deactivate();
        fBufferAddressSpace->release();
        fBufferDescriptor->release();
        fBufferAddressSpaceAllocated = false;
    }
    
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
    // count ptes
    //
    
    UInt32 		pte = 0;
    vm_size_t 		pos = offset;
    UInt32 		step = 0;
    UInt32		toMap = 0;
	
    IOPhysicalAddress 	phys;
    IOPhysicalLength 	lengthOfSegment;
  
    if( memoryDescriptor != NULL )
    {
        // use memory descriptor's length if length = 0
        if( length == 0 )
            length = memoryDescriptor->getLength();

#if FWLOGGING
        // I occasionally get memory descriptors with bogus lengths
        UInt32 tempLength = memoryDescriptor->getLength();
	if(length != tempLength)
        FWKLOG( ( "IOFireWireSBP2ORB : ### buffer length = %d, memDescriptor length = %d ###\n", length, tempLength  ) );
        // length = tempLength;
#endif
        
        while( pos < (length + offset) )
        {
            // get next segment
            phys = memoryDescriptor->getPhysicalSegment(pos, &lengthOfSegment);

 //           FWKLOG( ( "IOFireWireSBP2ORB : physical address = 0x%08lx, length = %d\n", phys, lengthOfSegment ) );

            // nothing more to map
            if( phys == 0 )
            {
                status = kIOReturnBadArgument;  // buffer not large enough
                break;
            }

            while( (pos < (length + offset)) && (lengthOfSegment != 0) )
            {
                // kFWSBP2MaxPageClusterSize max page table entry
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

                // count it if we've got anything to map
                if( toMap != 0 )
                    pte++;
                
                pos += toMap;
            }
        }

        FWKLOG( ( "IOFireWireSBP2ORB : number of required PTE's = %d\n", pte ) );

        // make sure we have enough memory
        if( pte > fPageTableSize / sizeof(FWSBP2PTE) )
		{
			if( fCommandFlags & kFWSBP2CommandFixedSize )
			{
            	fControl->openGate();
            	return kIOReturnNoMemory;
			}
            else
			{
                // reallocate
                status = allocatePageTable( pte );
                if( status != kIOReturnSuccess )
                {
                    fControl->openGate();
                    return kIOReturnNoMemory;
                }
            }
		}
	}
    
    //
    // deallocate old physical address space
    //

    // I obsolete the old address space even if we fail at mapping a new one
    // that seems like it would be the more expected behavior
    
    deallocateBufferAddressSpace();
    
    //
    // write page table
    //

    if( ( status == kIOReturnSuccess ) && ( memoryDescriptor != NULL ) )
    {
        pos = offset;
        pte = 0;

        while( pos < (length + offset) )
        {
            // get next segment
            phys = memoryDescriptor->getPhysicalSegment(pos, &lengthOfSegment);

            // nothing more to map
            if( phys == 0 )
            {
                status = kIOReturnBadArgument;  // buffer not large enough
                break;
            }
            
            // map until we are done or we run out of segment
            while( (pos < (length + offset)) && (lengthOfSegment != 0) )
            {
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
					FWSBP2PTE entry;

                    entry.segmentLength = toMap;
                    entry.segmentBaseAddressHi = 0x0000;
                    entry.segmentBaseAddressLo = phys;

					fPageTableDescriptor->writeBytes( pte * sizeof(FWSBP2PTE), &entry, sizeof(FWSBP2PTE) );
					
 //                   FWKLOG( ( "IOFireWireSBP2ORB : PTE = %d, size = %d\n", pte, toMap  ) );

                    // move to new page table entry and beginning of unmapped memory
                    pte++;
                    phys += toMap;
                    pos += toMap;
                }
            }
        }

        // map buffers

        // allocate and register an address space for the buffers
        if( status == kIOReturnSuccess )
        {
            fBufferDescriptor = memoryDescriptor;
            fBufferDescriptor->retain();
            fBufferAddressSpace = fUnit->createPhysicalAddressSpace( memoryDescriptor );
            if( fBufferAddressSpace == NULL )
                status = kIOReturnNoMemory;
        }

        if( status == kIOReturnSuccess )
        {
            status = fBufferAddressSpace->activate();
        }

        if( status == kIOReturnSuccess )
        {
            fBufferAddressSpaceAllocated = true;
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
            fORBBuffer->options				&= 0xfff0;	// no page table
            fORBBuffer->dataSize			= 0;
        }
        else 
		{
			FWSBP2PTE firstEntry;
			
			fPageTableDescriptor->readBytes( 0, &firstEntry, sizeof(FWSBP2PTE) );

			// very strictly speaking, if we don't use a page table, the buffer
			// must be quadlet-aligned.  SBP-2 is vague about this.  We'll enforce it.

			if( pte == 1 && !(firstEntry.segmentBaseAddressLo & 0x00000003) )  // make sure quadlet aligned
			{
				// directly addressed buffer
				fORBBuffer->dataDescriptorHi	= 0;			// tbd later
				fORBBuffer->dataDescriptorLo	= firstEntry.segmentBaseAddressLo;
				fORBBuffer->options				&= 0xfff0;		// no page table
				fORBBuffer->dataSize			= firstEntry.segmentLength;
			}
			else if( pte * sizeof(FWSBP2PTE) < PAGE_SIZE )
			{
				// use physical unit
				fORBBuffer->dataDescriptorHi	= 0;			// tbd later
				fORBBuffer->dataDescriptorLo	= fPageTablePhysicalAddress;
				fORBBuffer->options				&= 0xfff0;
				fORBBuffer->options				|= 0x0008;		// unrestricted page table
				fORBBuffer->dataSize			= pte;
			}
			else
			{
				// use software address space
				fORBBuffer->dataDescriptorHi	= fPageTablePseudoAddress.addressHi;			// tbd later
				fORBBuffer->dataDescriptorLo	= fPageTablePseudoAddress.addressLo;
				fORBBuffer->options				&= 0xfff0;
				fORBBuffer->options				|= 0x0008;		// unrestricted page table
				fORBBuffer->dataSize			= pte;			
			}
		}
    }
 
    fControl->openGate();
       
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
    fRefCon = refCon;
}

void * IOFireWireSBP2ORB::getRefCon( void )
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
		*address = fORBPseudoAddress;
	else
	{
		address->addressHi = 0L;
		address->addressLo = fORBPhysicalAddress;
	}
}

// sets the address of the next ORB field
void IOFireWireSBP2ORB::setNextORBAddress( FWAddress address )
{
    fORBBuffer->nextORBAddressHi = address.addressHi;
    fORBBuffer->nextORBAddressLo = address.addressLo;
}

// get login
IOFireWireSBP2Login * IOFireWireSBP2ORB::getLogin( void )
{
    return fLogin;
}

void IOFireWireSBP2ORB::setToDummy( void )
{
    // set rq_fmt to 3 (NOP) in case it wasn't fetched yet
    fORBBuffer->options |= 0x6000;
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
