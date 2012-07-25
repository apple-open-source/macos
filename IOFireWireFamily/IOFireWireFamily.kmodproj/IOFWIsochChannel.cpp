/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */

#include <IOKit/assert.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWIsochChannel.h>
#include <IOKit/firewire/IOFWLocalIsochPort.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#import <IOKit/firewire/IOFWDCLProgram.h>
#import <IOKit/firewire/IOFireWireLink.h>

#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>
#include "FWDebugging.h"

#include <IOKit/firewire/IOFWUtils.h>

OSDefineMetaClassAndStructors(IOFWIsochChannel, OSObject)
OSMetaClassDefineReservedUnused(IOFWIsochChannel, 0);
OSMetaClassDefineReservedUnused(IOFWIsochChannel, 1);
OSMetaClassDefineReservedUnused(IOFWIsochChannel, 2);
OSMetaClassDefineReservedUnused(IOFWIsochChannel, 3);

#pragma mark -
//////////////////////////////////////////////////////////////////////////////////////////

// init
//
//

bool IOFWIsochChannel::init(	IOFireWireController *		control, 
								bool 						doIRM,
								UInt32 						packetSize, 
								IOFWSpeed 					prefSpeed, 
								ForceStopNotificationProc *	stopProc, 
								void *						stopRefCon )
{
	bool success = true;

	FWKLOG(( "IOFWIsochChannel::init - doIRM = %d\n", doIRM ));
	
	DebugLog("fPacketSize = %d\n", (uint32_t)packetSize) ;
	
	success = OSObject::init();
    
	if( success )
	{
		fControl = control;
		fDoIRM = doIRM;
		fPacketSize = packetSize;
		fPrefSpeed = prefSpeed;
		fStopProc = stopProc;
		fStopRefCon = stopRefCon;
		fTalker = NULL;

		fChannel = 64;		// no channel allocated, 64 is an illegal channel
		fBandwidth = 0;		// no bandwidth allocated
		
		// Bandwidth allocation is not done on the main FireWire workloop so
		// we need to allocate a lock for synchronizing access to the resources
		// used in bandwidth allocation
		
		fLock = IOLockAlloc();
		if( fLock == NULL )
		{
			success = false;
		}
	}
	
	if( success )
	{
		fListeners = OSSet::withCapacity( 1 );
		if( fListeners == NULL  )
		{
			success = false;
		}
	}	
    
	//
	// create command for reading bandwidth available and channel available register
	//
	
	if( success )
	{
		fReadCmd = OSTypeAlloc( IOFWReadQuadCommand );
		if( fReadCmd == NULL )
		{
			success = false;
		}
	}
	
	if( success )
	{
	    fReadCmd->initAll( fControl, 0, FWAddress(), NULL, 0, NULL, NULL );
	}
	
	//
	// create lock cmd for updating the bandwidth available and channel available registers
	//
	
	if( success )
	{
		fLockCmd = OSTypeAlloc( IOFWCompareAndSwapCommand );
		if( fLockCmd == NULL )
		{
			success = false;
		}
	}
	
    if( success )
	{
	    fLockCmd->initAll( fControl, 0, FWAddress(), NULL, NULL, 0, NULL, NULL );
	}
	
    return success;
}

// free
//
//

void IOFWIsochChannel::free()
{
    if( fListeners )
    {
	    fListeners->release();
		fListeners = NULL;
	}
	
	if( fReadCmd )
    {
	    fReadCmd->release();
		fReadCmd = NULL;
    }
	
	if( fLockCmd )
    {
	    fLockCmd->release();
		fLockCmd = NULL;
	}
	
	if( fLock )
	{
		IOLockFree( fLock );
		fLock = NULL;
	}
	
	OSObject::free();
}

#pragma mark -
//////////////////////////////////////////////////////////////////////////////////////////

// checkMemoryInRange
//
//

IOReturn IOFWIsochChannel::checkMemoryInRange( IOMemoryDescriptor * memory )
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
	if( status == kIOReturnSuccess )
	{
		status = memory->prepare( kIODirectionInOut );
	}
	
	if( status == kIOReturnSuccess )
	{
		memory_prepared = true;
	}
	
	UInt64 length = 0;
	if( status == kIOReturnSuccess )
	{
		length = memory->getLength();
		if( length == 0 )
		{
			status = kIOReturnError;
		}
	}
	
	//
	// create IODMACommand
	//
	
	IODMACommand * dma_command = NULL;
	if( status == kIOReturnSuccess )
	{
		dma_command = IODMACommand::withSpecification( 
												kIODMACommandOutputHost64,		// segment function
												64,								// max address bits
												length,							// max segment size
												(IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly),		// IO mapped & don't bounce buffer
												length,							// max transfer size
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

	bool dma_command_prepared = false;
	if( status == kIOReturnSuccess )
	{
		status = dma_command->prepare( 0, length, true );
	}

	if( status == kIOReturnSuccess )
	{
		dma_command_prepared = true;
	}
	
	//
	// check ranges
	//

	if( status == kIOReturnSuccess )
	{
		UInt64 offset = 0;
		UInt64 mask = fControl->getFireWirePhysicalAddressMask();
		while( (offset < length) && (status == kIOReturnSuccess) )
		{
			IODMACommand::Segment64 segments[10];
			UInt32 num_segments = 10;
			status = dma_command->gen64IOVMSegments( &offset, segments, &num_segments );
			if( status == kIOReturnSuccess )
			{
				for( UInt32 i = 0; i < num_segments; i++ )
				{
				//	IOLog( "checkSegments - segments[%d].fIOVMAddr = 0x%016llx, fLength = %d\n", i, segments[i].fIOVMAddr, segments[i].fLength  );
						
					if( (segments[i].fIOVMAddr & (~mask)) )
					{
				//		IOLog( "checkSegmentsFailed - 0x%016llx & 0x%016llx\n", segments[i].fIOVMAddr, mask );
						status = kIOReturnNotPermitted;
						break;
					}
				}
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
		
	if( dma_command )
	{
		dma_command->clearMemoryDescriptor(); 
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

// setTalker
//
//

IOReturn IOFWIsochChannel::setTalker( IOFWIsochPort *talker )
{
	IOReturn error = kIOReturnSuccess;
    fTalker = talker;

	IOFWLocalIsochPort * localIsochPort = OSDynamicCast( IOFWLocalIsochPort, talker );
	if ( localIsochPort )
	{
		IODCLProgram * program = localIsochPort->getProgramRef();
		IOMemoryMap * map = program->getBufferMap();
		if( map == NULL )
		{
			error = kIOReturnNoMemory;
		}
		
		IOMemoryDescriptor * memory = NULL;
		if( error == kIOReturnSuccess )
		{
			memory = map->getMemoryDescriptor();
			if( memory == NULL )
			{
				error = kIOReturnNoMemory;
			}
		}

#if 0
		if( error == kIOReturnSuccess )
		{
			error = checkMemoryInRange( memory );
		}
#endif
		
		if( error == kIOReturnSuccess )
		{
	
			program->setForceStopProc( fStopProc, fStopRefCon, this );
			program->release();
		}
	}

    return error;
}

// addListener
//
//

IOReturn IOFWIsochChannel::addListener( IOFWIsochPort *listener )
{	
	IOReturn error = kIOReturnSuccess;
	
    if( !fListeners->setObject( listener ) )
    {
		error = kIOReturnNoMemory;
	}

	if ( !error )
	{
		IOFWLocalIsochPort * localIsochPort = OSDynamicCast( IOFWLocalIsochPort, listener ) ;
		if ( localIsochPort )
		{
			IODCLProgram * program = localIsochPort->getProgramRef();
			IOMemoryMap * map = program->getBufferMap();
			if( map == NULL )
			{
				error = kIOReturnNoMemory;
			}
			
			IOMemoryDescriptor * memory = NULL;
			if( error == kIOReturnSuccess )
			{
				memory = map->getMemoryDescriptor();
				if( memory == NULL )
				{
					error = kIOReturnNoMemory;
				}
			}
				
			if( error == kIOReturnSuccess )
			{
				error = checkMemoryInRange( memory );
			}

			if( error == kIOReturnSuccess )
			{			
				program->setForceStopProc( fStopProc, fStopRefCon, this ) ;
				program->release() ;
			}
		}
	}
	
	return error ;
}

// start
//
//

IOReturn IOFWIsochChannel::start()
{
	DebugLog("channel %p start\n", this ) ;

    OSIterator *listenIterator;
    IOFWIsochPort *listen;
	IOReturn error = kIOReturnSuccess ;

	// <rdar://problem/4033119>
	// the user requested doIRM, but we don't have an allocation..
	// they should have called allocateChannel()...
	if ( fDoIRM && fChannel == 64 )
	{
		return kIOReturnNotReady ;
	}

    //
	// start all listeners
	//
	
	listenIterator = OSCollectionIterator::withCollection( fListeners );
    if( listenIterator ) 
	{
        listenIterator->reset();
        while( !error && (listen = (IOFWIsochPort *)listenIterator->getNextObject()) ) 
		{
            error = listen->start();
        }
        listenIterator->release();
    }
    
	//
	// start the talker
	//
	
	if( !error && fTalker )
	{
		error = fTalker->start();
	}
	
	if (error)
	{
		DebugLog("channel %p start - error=%x\n", this, error ) ;
		
	}
	else
	{
		DebugLog("channel %p start - started on isoch channel %d\n", this, (uint32_t)fChannel ) ;
	}
	
    return error ;
}

// stop
//
//

IOReturn IOFWIsochChannel::stop()
{
	DebugLog("channel %p stop\n", this ) ;
	
    OSIterator *listenIterator;
    IOFWIsochPort *listen;

    //
	// stop all listeners
	//
	
	listenIterator = OSCollectionIterator::withCollection( fListeners );
    if( listenIterator ) 
	{
        listenIterator->reset();
		while( (listen = (IOFWIsochPort *)listenIterator->getNextObject()) )
		{
            listen->stop();
        }
		listenIterator->release();
    }
    
	//
	// stop talker
	//
	
	if( fTalker )
	{
		fTalker->stop();
	}
	
    return kIOReturnSuccess;
}

#pragma mark -
//////////////////////////////////////////////////////////////////////////////////////////

// allocateChannel
//
// cannot be called on the workloop

IOReturn IOFWIsochChannel::allocateChannel()
{
	DebugLog("channel %p allocateChannel\n", this ) ;
	
	IOReturn 			status = kIOReturnSuccess;

	IOFWIsochPort *		listen = NULL;
	OSIterator *		listenIterator 		= NULL;

	//
	// get best speed and find supported channels
	//
			
	if( status == kIOReturnSuccess )
	{
		UInt64 				portChans;
		IOFWSpeed 			portSpeed;
		UInt64 				allowedChans;
		IOFWSpeed			speed;
		
		// Find minimum of requested speed and paths from talker to each listener
		// Reduce speed to minimum of what all ports can do, find supported channels

		speed = fPrefSpeed;

		//
		// get supported channels and max speed from talker
		//
		
        allowedChans = ~(UInt64)0;
        if( fTalker ) 
		{
            fTalker->getSupported( portSpeed, portChans );
            if( portSpeed < speed )
			{
                speed = portSpeed;
            }
			allowedChans &= portChans;
        }
        
		//
		// get supported channels and max speed from all listeners
		//
		
		listenIterator = OSCollectionIterator::withCollection( fListeners );
        if( listenIterator ) 
		{
            while( (listen = (IOFWIsochPort *)listenIterator->getNextObject()) ) 
			{
                listen->getSupported( portSpeed, portChans );
                if( portSpeed < speed )
				{
                    speed = portSpeed;
                }
				allowedChans &= portChans;
            }
        }

		//
        // do bandwidth and channel allocation
		//
		
		// allocateChannelBegin sets up fSpeed, fGeneration, fBandwidth, and fChannel
		
		status = allocateChannelBegin( speed, allowedChans );
	}
	
	//
	// allocate hardware resources for all listeners
	//
	
	if( status == kIOReturnSuccess )
	{
        if( listenIterator ) 
		{
            listenIterator->reset();
            while( (listen = (IOFWIsochPort *)listenIterator->getNextObject()) && 
				   (status == kIOReturnSuccess) ) 
			{
                status = listen->allocatePort( fSpeed, fChannel );
            }
		}		
	}

	//
	// allocate hardware resources for talker
	//
	
	if( status == kIOReturnSuccess )
	{
        if( fTalker )
		{
            status = fTalker->allocatePort( fSpeed, fChannel );
		}
	}

	//
	// clean up
	//
	
    if( listenIterator )
    {
	    listenIterator->release();
	}
	
    if( status != kIOReturnSuccess ) 
	{
		// on error release anything we may have allocated
	
        releaseChannel();
    }
	    
	return status;
}

// allocateChannelBegin
//
// cannot be called on the workloop

IOReturn IOFWIsochChannel::allocateChannelBegin( 	IOFWSpeed		inSpeed,
													UInt64			inAllowedChans,
													UInt32 *		outChannel )
{
	DebugLog( "IOFWIsochChannel<%p>::allocateChannelBegin() - entered, inSpeed = %d, fDoIRM = %d, inAllowedChans = 0x%016llx, fChannel=%d, fBandwidth=%d\n", this, inSpeed, fDoIRM, inAllowedChans, (uint32_t)fChannel, (uint32_t)fBandwidth );

	IOReturn		status = kIOReturnSuccess;

	IOLockLock( fLock );
	
	fSpeed = ( inSpeed == kFWSpeedMaximum ) ? fControl->getLink()->getPhySpeed() : inSpeed ;

	if( fDoIRM )
	{
		UInt32 			oldIRM[3];
		
		// reserve bandwidth:
		// bandwidth is in units of quads at 1600 Mbs
		UInt32 bandwidth = 0;
		if( fPacketSize != 0 )
		{
			bandwidth = (fPacketSize/4 + 3) * 16 / (1 << inSpeed);
		}
		
		do
		{
			FWAddress			addr ;
			UInt32				generation ;

			status = kIOReturnSuccess ;			

			//
			// get IRM nodeID into addr.nodeID
			//
			
			fControl->getIRMNodeID( generation, addr.nodeID );
			
			DebugLog( "IOFWIsochChannel<%p>::allocateChannelBegin() - generation %d\n", this, (uint32_t)generation );
			
			//
			// Make sure we're at least one second from the last bus-reset
			//
			
			{
				unsigned delayRetries = 15;
				while (delayRetries > 0)
				{
					UInt32			currentGeneration;
					AbsoluteTime	currentUpTime;
					UInt64			nanos;
					UInt32			delayInMSec;

					IOFWGetAbsoluteTime( & currentUpTime );
					SUB_ABSOLUTETIME( & currentUpTime, fControl->getResetTime() ) ;
					absolutetime_to_nanoseconds( currentUpTime, & nanos ) ;
					if (nanos < 1000000000LL)
					{
						delayInMSec = (UInt32) ((1000000000LL - nanos)/1000000LL);
						
						DebugLog( "IOFWIsochChannel<%p>::allocateChannelBegin() - delaying for %d msec\n", this, (uint32_t)delayInMSec);
						
						// Delay until it's been one second from last bus-reset
						IOSleep(delayInMSec);
						
						// get generation
						fControl->getIRMNodeID( currentGeneration, addr.nodeID );
						if (currentGeneration == generation)
						{
							break;
						}
						else
						{
							generation = currentGeneration; // Another bus-reset occurred, do the delay check again
						}
					}
					else
					{
						break;
					}
					
					delayRetries--;
				}
			
				// Make sure we didn't use all the delay retries
				if (delayRetries == 0)
				{
					DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - timed out waiting for bus resets to end\n", this) ;
					status = kIOReturnTimeout;
				}
			}
			
			if( status == kIOReturnSuccess )
			{
				// read IRM into oldIRM[3] (up to 5 retries) ;
				
				addr.addressHi = kCSRRegisterSpaceBaseAddressHi ;
				addr.addressLo = kCSRBandwidthAvailable ;
			
				fReadCmd->reinit( generation, addr, oldIRM, 3 );
				fReadCmd->setMaxPacket( 4 );		// many cameras don't like block reads to IRM registers, eg. Canon GL-1
				
				status = fReadCmd->submit();
				
				DebugLogCond( status, "IOFWIsochChannel<%p>::allocateChannelBegin() line %u - read command got error 0x%x\n", this, __LINE__, status ) ;
				DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - oldIRM set to %08x %08x %08x\n", this, 
							OSSwapBigToHostInt32(oldIRM[0]), OSSwapBigToHostInt32(oldIRM[1]), OSSwapBigToHostInt32(oldIRM[2]) ) ;
			}
			
			//
			// Claim bandwidth from IRM
			//
			
			if ( fBandwidth == 0 )
			{
				UInt32 old_bandwidth = OSSwapBigToHostInt32( oldIRM[0] );
				if( ( status == kIOReturnSuccess ) && ( old_bandwidth < bandwidth ) )
				{
					DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - no bandwidth left\n", this) ;
					status = kIOReturnNoSpace;
				}
				
				if( status == kIOReturnSuccess )
				{
					UInt32			newVal = OSSwapHostToBigInt32(old_bandwidth - bandwidth);
					
					fLockCmd->reinit( generation, addr, &oldIRM[0], &newVal, 1 );
		
					status = fLockCmd->submit();
					
					if ( status )
					{
						DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() line %u - lock command got error 0x%x\n", this, __LINE__, status ) ;
					}
					else
					{
						if ( !fLockCmd->locked( &oldIRM[0] ) )
						{
							DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - lock for bandwidth failed\n", this) ;
							status = kIOReturnCannotLock;
						}
					}
					
					if( status == kIOReturnSuccess )
					{
						DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - allocated bandwidth %u\n", this, (uint32_t)bandwidth) ;
						
						fBandwidth = bandwidth;
					}
				}
			}
			
			//
			// claim channel from IRM
			//
			
			// (if we have an error here, the bandwidth wasn't allocated)
			if ( status == kIOReturnSuccess && fChannel == 64 )
			{
				unsigned channel ;
			
				UInt32 old_channel_hi = OSSwapBigToHostInt32( oldIRM[1] );
				UInt32 old_channel_lo = OSSwapBigToHostInt32( oldIRM[2] );
				
				// mask inAllowedChans by channels IRM has available
				inAllowedChans &= (UInt64)(old_channel_lo) | ((UInt64)old_channel_hi << 32);
			
				for( channel = 0; channel < 64; ++channel )
				{
					if( inAllowedChans & ((UInt64)1 << (63 - channel)) )
					{
						break;
					}
				}
		
				if( channel == 64 )
				{
					DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - no acceptable free channels\n", this) ;
					status = kIOReturnNoResources;
				}
			
				if( status == kIOReturnSuccess )
				{
					UInt32 *		oldPtr;
					UInt32			newVal ;
			
					// Claim channel
					if( channel < 32 )
					{
						addr.addressLo = kCSRChannelsAvailable31_0;
						oldPtr = &oldIRM[1];
						newVal = OSSwapHostToBigInt32( old_channel_hi & ~(1<<(31 - channel)) );
					}
					else
					{
						addr.addressLo = kCSRChannelsAvailable63_32;
						oldPtr = &oldIRM[2];
						newVal = OSSwapHostToBigInt32( old_channel_lo & ~(1 << (63 - channel)) );
					}
					
					fLockCmd->reinit( generation, addr, oldPtr, &newVal, 1 );
					status = fLockCmd->submit();

					if ( status )
					{
						DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - lock command got error 0x%x\n", this, status ) ;
					}
					else
					{
						if( !fLockCmd->locked(oldPtr) )
						{
							DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - couldn't claim channel %u\n", this, channel) ;
							status = kIOReturnCannotLock;
						}
					}
				}
			
				if( status == kIOReturnSuccess )
				{
					DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - allocated channel %u\n", this, channel) ;
					
					fChannel = channel;
					if( outChannel != NULL )
					{
						*outChannel = channel;
					}
				}
				else if ( fBandwidth != 0 )
				{
					// Release the bandwidth we allocated, since we did not get the channel
					fControl->releaseIRMBandwidthInGeneration(fBandwidth, generation);
				}
			}
			
			if( status == kIOReturnSuccess )
			{
				fGeneration = generation;
				
				// tell controller we would like to hear about bus resets
				fControl->addAllocatedChannel(this);
			}
			
			if( status == kIOFireWireBusReset )
			{
				// we lost any allocations if there was a bus reset
				fBandwidth = 0;
				fChannel = 64; 
			}
			
		} while( (status == kIOFireWireBusReset) || (status == kIOReturnCannotLock) );
	}
	else
	{
		//
		// return a channel even if we aren't doing the IRM management
		//
		
		unsigned channel ;

		{
			for( channel = 0; channel < 64; channel++ )
			{
				if( inAllowedChans & ((UInt64)1 << (63 - channel)) )
				{
					break;
				}
			}
	
			if( channel == 64 )
			{
				DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - couldn't find acceptable channel\n", this) ;
				status = kIOReturnNoResources;
			}
		}
	
		if( status == kIOReturnSuccess )
		{
			DebugLog( "IOFWIsochChannel<%p>::allocateChannelBegin() - allocated channel = %d\n", this, channel );
			
			fChannel = channel;
			if( outChannel != NULL )
			{
				*outChannel = channel;
			}
		}
	}
		
	IOLockUnlock( fLock );
	
	FWTrace( kFWTIsoch, kTPIsochAllocateChannelBegin, (uintptr_t)(fControl->getLink()), fChannel, fBandwidth, status );
	DebugLogCond( status, "IOFWIsochChannel<%p>::allocateChannelBegin() - exited with status = 0x%x\n", this, status );
	
    return status;
}

// ChannelThreadInfo
//
// A little struct for keeping track of our this pointer and generation
// when transitioning to a second thread during bandwidth reallocation.

struct ChannelThreadInfo
{
    IOFWIsochChannel *		fChannel;
    UInt32 					fGeneration;
};

// handleBusReset
//
//

void IOFWIsochChannel::handleBusReset()
{
	//
	// setup thread info and spawn a thread
	//
	
	ChannelThreadInfo * threadInfo = (ChannelThreadInfo *)IOMalloc( sizeof(ChannelThreadInfo) );
	if( threadInfo ) 
	{
		threadInfo->fGeneration = fControl->getGeneration();
		threadInfo->fChannel = this;
		retain();	// retain ourself for the thread to use
		
		thread_t		thread;
		if( kernel_thread_start((thread_continue_t)threadFunc, threadInfo, &thread ) == KERN_SUCCESS )
		{
			thread_deallocate(thread);
		}
	}
}

// threadFunc
//
//

void IOFWIsochChannel::threadFunc( void * arg )
{
	//
	// extract info from thread info
	//
	
    ChannelThreadInfo * threadInfo = (ChannelThreadInfo *)arg;
    IOFWIsochChannel * 	channel = threadInfo->fChannel;
	UInt32 				generation = threadInfo->fGeneration;
	
	//
	// realloc bandwidth
	//
	
	channel->reallocBandwidth( generation );	
	
	//
	// clean up thread info
	//
	
	IOFree( threadInfo, sizeof(threadInfo) );
    channel->release();		// retain occurred in handleBusReset
}

// reallocBandwidth
//
// cannot be called on the workloop

void IOFWIsochChannel::reallocBandwidth( UInt32 generation )
{
	IOReturn result = kIOReturnSuccess;

    FWAddress addr( kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable );
		
	IOLockLock( fLock );

	InfoLog( "IOFWIsochChannel<%p>::reallocBandwidth() - bandwidth %ld, channel = %ld\n", this, fBandwidth, fChannel );
	
	if( result == kIOReturnSuccess )
	{
		UInt32 current_generation;
		UInt16 irm;
		fControl->getIRMNodeID( current_generation, irm );
		addr.nodeID = irm;
		if( current_generation != generation )
		{
			result = kIOFireWireBusReset;
		}

//		FWKLOG(( "IOFWIsochChannel::reallocBandwidth - realloc generation %d, current generation = %d\n", generation, current_generation ));
	}
	
	if( result == kIOReturnSuccess )
	{
		// check to make sure we don't allocate twice on a generation
		
		if( fGeneration == generation )
		{
			result = kIOReturnError;
		}
	}
	
	//
	// reallocate bandwidth
	//
	
	if( fBandwidth != 0 ) 
	{
		UInt32 oldVal = 0;
		
		//
		// read the bandwidth available register
		//
		
		if( result == kIOReturnSuccess ) 
		{
            fReadCmd->reinit( generation, addr, &oldVal, 1 );
            result = fReadCmd->submit();
		}

		//
		// compare swap loop
		//
		
		bool done = false;
		while( (result == kIOReturnSuccess) && !done )
		{
			UInt32 newVal = 0;
			UInt32 old_bandwidth = OSSwapBigToHostInt32( oldVal );
			
			// make sure there's space
			if( old_bandwidth < fBandwidth ) 
			{
				result = kIOReturnNoSpace;
			}
			
			if( result == kIOReturnSuccess )
			{
				newVal = OSSwapHostToBigInt32(old_bandwidth - fBandwidth);
		
				fLockCmd->reinit( generation, addr, &oldVal, &newVal, 1 );
				result = fLockCmd->submit();
			}
			
			if( result == kIOReturnSuccess )
			{
				done = fLockCmd->locked(&oldVal);
			}
		} 
		
		if( result == kIOReturnNoSpace ) 
		{
			DebugLog( "IOFWIsochChannel<%p>::reallocBandwidth() - failed to reallocate bandwidth = %d, channel = %d\n", this, (uint32_t)fBandwidth, (uint32_t)fChannel );
			
			// Couldn't reallocate bandwidth!
			fBandwidth = 0;
			fChannel = 64;	// this will keep us from reallocating the channel
		}
		else
		{
			InfoLog( "IOFWIsochChannel<%p>::reallocBandwidth() - reallocated bandwidth = %ld\n", this, fBandwidth );
		}
	}

	//
	// reallocate channel
	//
	
	if( fChannel != 64 ) 
	{
		UInt32 	mask = 0;
		UInt32	oldVal = 0;
        
		//
		// read the channels available register
		//
		
		if( result == kIOReturnSuccess )
		{
			if( fChannel <= 31 ) 
			{
                addr.addressLo = kCSRChannelsAvailable31_0;
                mask = 1 << (31-fChannel);
            }
            else 
			{
                addr.addressLo = kCSRChannelsAvailable63_32;
                mask = 1 << (63-fChannel);
            }
            
			fReadCmd->reinit( generation, addr, &oldVal, 1 );
            result = fReadCmd->submit();
		}
		
		//
		// compare swap loop
		//
		
		bool done = false;
		while( (result == kIOReturnSuccess) && !done )
		{
			UInt32 old_channels_avail = OSSwapBigToHostInt32( oldVal );
			UInt32 newVal = OSSwapHostToBigInt32( old_channels_avail & ~mask );
			if( newVal == oldVal ) 
			{
				// Channel already allocated!
				result = kIOFireWireChannelNotAvailable ;
			}
		
			if( result == kIOReturnSuccess )
			{
				fLockCmd->reinit( generation, addr, &oldVal, &newVal, 1 );
                result = fLockCmd->submit();
			}
			
			if( result == kIOReturnSuccess )
			{
				done = fLockCmd->locked( &oldVal );
			}
		}
		
		if( result == kIOFireWireChannelNotAvailable )
		{
			DebugLog( "IOFWIsochChannel<%p>::reallocBandwidth() - failed to reallocate channel = %d\n", this, (uint32_t)fChannel );

			// Couldn't reallocate the channel
			fChannel = 64;
			// fBandwidth will be set to 0 once it is released in the call to releaseChannel below
		}
		else
		{
			InfoLog( "IOFWIsochChannel<%p>::reallocBandwidth() - reallocated channel = %ld\n", this, fChannel );
		}
	}

	fGeneration = generation;

	FWTrace( kFWTIsoch, kTPIsochReallocBandwidth, (uintptr_t)(fControl->getLink()), fChannel, fBandwidth, result );
	DebugLogCond( result, "IOFWIsochChannel<%p>::reallocBandwidth() - exited with result = 0x%x\n", this, result );

	IOLockUnlock( fLock );
    
	if( result == kIOReturnNoSpace || result == kIOFireWireChannelNotAvailable ) 
	{
        // Couldn't reallocate bandwidth or channel
		
		stop();
        
		// fChannel and fBandwidth have been left in such a way that releaseChannel() 
		// will know to release both, one, or none
		
		releaseChannel();
		
		if ( fStopProc )
		{
			(*fStopProc)( fStopRefCon, this, kIOFireWireChannelNotAvailable );
		}
    }
	
}

// releaseChannel
//
//

IOReturn IOFWIsochChannel::releaseChannel()
{
	DebugLog("IOFWIsochChannel<%p>::releaseChannel()\n", this ) ;

    OSIterator *listenIterator;
    IOFWIsochPort *listen;

    if( fTalker ) 
	{
        fTalker->releasePort();
    }
    
	listenIterator = OSCollectionIterator::withCollection(fListeners);
    if( listenIterator ) 
	{
        while( (listen = (IOFWIsochPort *)listenIterator->getNextObject()) ) 
		{
            listen->releasePort();
        }
        listenIterator->release();
    }

    return releaseChannelComplete();
}

// releaseChannelComplete
//
//

IOReturn IOFWIsochChannel::releaseChannelComplete()
{
	IOReturn result = kIOReturnSuccess;
	
    // release bandwidth and channel

	FWKLOG(( "IOFWIsochChannel::releaseChannelComplete - entered, fDoIRM = %d\n", fDoIRM ));

	if( fDoIRM ) 
	{
		FWAddress 	addr( kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable );
		UInt32		generation = 0;
		
		IOLockLock( fLock );

		//
		// Tell the controller that we don't need to know about
		// bus resets before doing anything else, since a bus reset
		// sets us into the state we want (no allocated bandwidth).
		//
		
		if( result == kIOReturnSuccess )
		{
			fControl->removeAllocatedChannel( this );
		}

		if( result == kIOReturnSuccess )
		{
			UInt16 irm;
			fControl->getIRMNodeID( generation, irm );
			addr.nodeID = irm;
			
			FWKLOG(( "IOFWIsochChannel::releaseChannelComplete - generation = %d allocated generation = %d\n", generation, fGeneration ));

			if( generation != fGeneration )
			{
				result = kIOFireWireBusReset;
				fBandwidth = 0;
				fChannel = 64;
			}
		}

		//
		// release bandwidth
		//
		
		if( fBandwidth != 0 ) 
		{
			UInt32 oldVal = 0;
			
			//
			// read the bandwidth available register
			//
			
			if( result == kIOReturnSuccess ) 
			{
				fReadCmd->reinit( generation, addr, &oldVal, 1 );
				result = fReadCmd->submit();
			}
	
			//
			// compare swap loop
			//
			
			bool done = false;
			while( (result == kIOReturnSuccess) && !done )
			{
				UInt32 old_bandwidth = OSSwapBigToHostInt32( oldVal );
				UInt32 newVal = OSSwapHostToBigInt32( old_bandwidth + fBandwidth );
		
				fLockCmd->reinit( generation, addr, &oldVal, &newVal, 1 );
				result = fLockCmd->submit();
			
				if( result == kIOReturnSuccess )
				{
					done = fLockCmd->locked(&oldVal);
				}
			} 
			
			// error or not, we've released our bandwidth
			fBandwidth = 0;
			
			FWKLOG(( "IOFWIsochChannel::releaseChannelComplete - released bandwidth\n" ));
		
			if( result != kIOFireWireBusReset )
			{
				// as long as we didn't get a bus reset error, let's pretend 
				// this was successful so we can give channel deallocation a try
				
				result = kIOReturnSuccess;
			}
		}

		//
		// release channel
		//
		
		if( fChannel != 64 ) 
		{
			UInt32 	mask = 0;
			UInt32	oldVal = 0;
			
			//
			// read the channels available register
			//
			
			if( result == kIOReturnSuccess )
			{
				if( fChannel <= 31 ) 
				{
					addr.addressLo = kCSRChannelsAvailable31_0;
					mask = 1 << (31-fChannel);
				}
				else 
				{
					addr.addressLo = kCSRChannelsAvailable63_32;
					mask = 1 << (63-fChannel);
				}
				
				fReadCmd->reinit( generation, addr, &oldVal, 1 );
				result = fReadCmd->submit();
			}
			
			//
			// compare swap loop
			//
			
			bool done = false;
			while( (result == kIOReturnSuccess) && !done )
			{
				UInt32 old_channels_avail = OSSwapBigToHostInt32( oldVal );
				UInt32 newVal = OSSwapHostToBigInt32( old_channels_avail | mask );
                
				fLockCmd->reinit( generation, addr, &oldVal, &newVal, 1 );
				result = fLockCmd->submit();
				
				if( result == kIOReturnSuccess )
				{
					done = fLockCmd->locked( &oldVal );
				}
			}

			FWKLOG(( "IOFWIsochChannel::releaseChannelComplete - released channel\n" ));
			
			// error or not, we've released the channel
			fChannel = 64;
		}

		// error or not we've released our channel and bandwidth
		
		fBandwidth = 0;
		fChannel = 64;
		
		fGeneration = generation;
    
		IOLockUnlock( fLock );
	}
		
    return kIOReturnSuccess;
}

// updateBandwidth
//
// deprecated

IOReturn IOFWIsochChannel::updateBandwidth( bool /* claim */ )
{
	DebugLog("driver calling deprecated IOFWIsochChannel::updateBandwidth on channel %p\n", this ) ;
	
	return kIOReturnUnsupported;
}

