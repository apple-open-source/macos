/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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
	
	DebugLog("fPacketSize = %d\n", packetSize) ;
	
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
		fReadCmd = new IOFWReadQuadCommand;
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
		fLockCmd = new IOFWCompareAndSwapCommand;
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

// setTalker
//
//

IOReturn IOFWIsochChannel::setTalker( IOFWIsochPort *talker )
{
    fTalker = talker;

	IOFWLocalIsochPort * localIsochPort = OSDynamicCast( IOFWLocalIsochPort, talker ) ;
	if ( localIsochPort )
	{
		IODCLProgram * program = localIsochPort->getProgramRef() ;

		program->setForceStopProc( fStopProc, fStopRefCon, this ) ;
		program->release() ;
	}

    return kIOReturnSuccess;
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
			IODCLProgram * program = localIsochPort->getProgramRef() ;
			
			program->setForceStopProc( fStopProc, fStopRefCon, this ) ;
			program->release() ;
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
		DebugLog("channel %p start - started on isoch channel %d\n", this, fChannel ) ;
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
	DebugLog( "IOFWIsochChannel<%p>::allocateChannelBegin() - entered, inSpeed = %d, fDoIRM = %d, inAllowedChans = 0x%016llx, fChannel=%u, fBandwidth=%u\n", this, inSpeed, fDoIRM, inAllowedChans, fChannel, fBandwidth );

	IOReturn		status = kIOReturnSuccess;

	IOLockLock( fLock );
	
	fSpeed = ( inSpeed == kFWSpeedMaximum ) ? fControl->getLink()->getPhySpeed() : inSpeed ;

	if( fDoIRM )
	{
		UInt32 			oldIRM[3];
		
		// reserve bandwidth:
		// bandwidth is in units of quads at 1600 Mbs
		UInt32 bandwidth = (fPacketSize/4 + 3) * 16 / (1 << inSpeed);

		do
		{
			FWAddress			addr ;
			UInt32				generation ;

			status = kIOReturnSuccess ;			

			//
			// get IRM nodeID into addr.nodeID
			//
			
			fControl->getIRMNodeID( generation, addr.nodeID );
			
			DebugLog( "IOFWIsochChannel<%p>::allocateChannelBegin() - generation %d\n", this, generation );
			
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

					clock_get_uptime( & currentUpTime );
					SUB_ABSOLUTETIME( & currentUpTime, fControl->getResetTime() ) ;
					absolutetime_to_nanoseconds( currentUpTime, & nanos ) ;
					if (nanos < 1000000000LL)
					{
						delayInMSec = (UInt32) ((1000000000LL - nanos)/1000000LL);
						
						DebugLog( "IOFWIsochChannel<%p>::allocateChannelBegin() - delaying for %u msec\n", this, delayInMSec);
						
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
				DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - oldIRM set to %08x %08x %08x\n", this, oldIRM[0], oldIRM[1], oldIRM[2] ) ;
			}
			
			//
			// Claim bandwidth from IRM
			//
			
			if ( fBandwidth == 0 )
			{
				if( ( status == kIOReturnSuccess ) && ( oldIRM[0] < bandwidth ) )
				{
					DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - no bandwidth left\n", this) ;
					status = kIOReturnNoSpace;
				}
				
				if( status == kIOReturnSuccess )
				{
					UInt32			newVal = oldIRM[0] - bandwidth;
					
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
						DebugLog("IOFWIsochChannel<%p>::allocateChannelBegin() - allocated bandwidth %u\n", this, bandwidth) ;
						
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
			
				// mask inAllowedChans by channels IRM has available
				inAllowedChans &= (UInt64)(oldIRM[2]) | ((UInt64)oldIRM[1] << 32);
			
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
						newVal = *oldPtr & ~(1<<(31 - channel));
					}
					else
					{
						addr.addressLo = kCSRChannelsAvailable63_32;
						oldPtr = &oldIRM[2];
						newVal = *oldPtr & ~(1 << (63 - channel));
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
		IOCreateThread( threadFunc, threadInfo );
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

	DebugLog( "IOFWIsochChannel<%p>::reallocBandwidth() - bandwidth %d, channel = %d\n", this, fBandwidth, fChannel );
	
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
			
			// make sure there's space
			if( oldVal < fBandwidth ) 
			{
				result = kIOReturnNoSpace;
			}
			
			if( result == kIOReturnSuccess )
			{
				newVal = oldVal - fBandwidth;
		
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
			DebugLog( "IOFWIsochChannel<%p>::reallocBandwidth() - failed to reallocate bandwidth = %d, channel = %d\n", this, fBandwidth, fChannel );
			
			// Couldn't reallocate bandwidth!
			fBandwidth = 0;
			fChannel = 64;	// this will keep us from reallocating the channel
		}
		else
		{
			DebugLog( "IOFWIsochChannel<%p>::reallocBandwidth() - reallocated bandwidth = %d\n", this, fBandwidth );
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
			UInt32 newVal = oldVal & ~mask;
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
			DebugLog( "IOFWIsochChannel<%p>::reallocBandwidth() - failed to reallocate channel = %d\n", this, fChannel );

			// Couldn't reallocate the channel
			fChannel = 64;
			// fBandwidth will be set to 0 once it is released in the call to releaseChannel below
		}
		else
		{
			DebugLog( "IOFWIsochChannel<%p>::reallocBandwidth() - reallocated channel = %d\n", this, fChannel );
		}
	}

	fGeneration = generation;

	DebugLog( "IOFWIsochChannel<%p>::reallocBandwidth() - exited with result = 0x%08lx\n", this, result );

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
				UInt32 newVal = newVal = oldVal + fBandwidth;
		
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
				UInt32 newVal = oldVal | mask;
                
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

