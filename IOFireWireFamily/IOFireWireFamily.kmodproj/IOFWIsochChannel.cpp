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
    OSIterator *listenIterator;
    IOFWIsochPort *listen;

    //
	// start all listeners
	//
	
	listenIterator = OSCollectionIterator::withCollection( fListeners );
    if( listenIterator ) 
	{
        listenIterator->reset();
        while( (listen = (IOFWIsochPort *)listenIterator->getNextObject()) ) 
		{
            listen->start();
        }
        listenIterator->release();
    }
    
	//
	// start the talker
	//
	
	if( fTalker )
	{
		fTalker->start();
	}
	
    return kIOReturnSuccess;
}

// stop
//
//

IOReturn IOFWIsochChannel::stop()
{
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
	IOReturn		status = kIOReturnSuccess;

	UInt32 			channel = 0;

	FWKLOG(( "IOFWIsochChannel::allocateChannelBegin - entered, fDoIRM = %d, inAllowedChans = 0x%016llx\n", fDoIRM, inAllowedChans ));

	IOLockLock( fLock );
	
	fSpeed = inSpeed;

	if( fDoIRM )
	{
		UInt32 			generation;
		UInt32 			newVal;
		FWAddress 		addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable);
		UInt32 			oldIRM[3];
				
		do
		{
			// reserve bandwidth:
			// bandwidth is in units of quads at 1600 Mbs
			UInt32	bandwidth = (fPacketSize/4 + 3) * 16 / (1 << inSpeed);
	
			// get IRM nodeID into addr.nodeID
			fControl->getIRMNodeID( generation, addr.nodeID );

			FWKLOG(( "IOFWIsochChannel::allocateChannelBegin - generation %d\n", generation ));
			
			// read IRM into oldIRM[3] (up to 5 retries) ;
			fReadCmd->reinit( generation, addr, oldIRM, 3 );
			fReadCmd->setMaxPacket( 4 );		// many cameras don't like block reads to IRM registers, eg. Canon GL-1
			status = fReadCmd->submit();
	
			// Claim bandwidth from IRM
			if( (status == kIOReturnSuccess) && (oldIRM[0] < bandwidth) )
			{
				status = kIOReturnNoSpace;
			}
			
			if( status == kIOReturnSuccess )
			{
				newVal = oldIRM[0] - bandwidth;
				fLockCmd->reinit( generation, addr, &oldIRM[0], &newVal, 1 );
	
				status = fLockCmd->submit();
				if ( (status == kIOReturnSuccess) && !fLockCmd->locked( &oldIRM[0] ) )
				{
					status = kIOReturnCannotLock;
				}
				
				if( status == kIOReturnSuccess )
				{
					FWKLOG(( "IOFWIsochChannel::allocateChannelBegin - allocated bandwidth = %d\n", bandwidth ));
					
					fBandwidth = bandwidth;
				}
			}
		
			if( status == kIOReturnSuccess )
			{
				// mask inAllowedChans by channels IRM has available
				inAllowedChans &= (UInt64)(oldIRM[2]) | ((UInt64)oldIRM[1] << 32);
			}
		
			// if we have an error here, the bandwidth wasn't allocated
			if( status == kIOReturnSuccess )
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
					status = kIOReturnNoResources;
				}
			}
		
			if( status == kIOReturnSuccess )
			{
				UInt32*		oldPtr;
		
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
					newVal = *oldPtr & ~((UInt64)1 << (63 - channel));
				}
				
				fLockCmd->reinit( generation, addr, oldPtr, &newVal, 1 );
				status = fLockCmd->submit();
				if( !status && !fLockCmd->locked(oldPtr) )
				{
					status = kIOReturnCannotLock;
				}
			}
		
			if( status == kIOReturnSuccess )
			{
				FWKLOG(( "IOFWIsochChannel::allocateChannelBegin - allocated channel = %d\n", channel ));
				
				fChannel = channel;
				if( outChannel != NULL )
				{
					*outChannel = channel;
				}
			}
			
			if( status == kIOReturnSuccess )
			{
				fGeneration = generation;
				
				// tell controller we would like to hear about bus resets
				fControl->addAllocatedChannel(this);
			}
		}
		while( (status == kIOFireWireBusReset) || (status == kIOReturnCannotLock) );
	}
	else
	{
		//
		// return a channel even if we aren't doing the IRM management
		//
		
		if( status == kIOReturnSuccess )
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
				status = kIOReturnNoResources;
			}
		}
	
		if( status == kIOReturnSuccess )
		{
			FWKLOG(( "IOFWIsochChannel::allocateChannelBegin - allocated channel = %d\n", channel ));
			
			fChannel = channel;
			if( outChannel != NULL )
			{
				*outChannel = channel;
			}
		}
	}
		
	IOLockUnlock( fLock );

	FWKLOG(( "IOFWIsochChannel::allocateChannelBegin - exited with status = 0x%08lx\n", status ));
	
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

	FWKLOG(( "IOFWIsochChannel::reallocBandwidth - bandwidth %d, channel = %d\n", fBandwidth, fChannel ));
	
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

		FWKLOG(( "IOFWIsochChannel::reallocBandwidth - realloc generation %d, current generation = %d\n", generation, current_generation ));
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
			FWKLOG(( "IOFWIsochChannel::reallocBandwidth - failed to reallocate bandwidth = %d, channel = %d\n", fBandwidth, fChannel ));
			
			// Couldn't reallocate bandwidth!
			fBandwidth = 0;
			fChannel = 64;	// this will keep us from reallocating the channel
		}
		else
		{
			FWKLOG(( "IOFWIsochChannel::reallocBandwidth - reallocated bandwidth = %d\n", fBandwidth ));
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
			FWKLOG(( "IOFWIsochChannel::reallocBandwidth - failed to reallocate channel = %d\n", fChannel ));

			// Couldn't reallocate the channel
			fChannel = 64;
			// fBandwidth will be set to 0 once it is released in the call to releaseChannel below
		}
		else
		{
			FWKLOG(( "IOFWIsochChannel::reallocBandwidth - reallocated channel = %d\n", fChannel ));
		}
	}

	fGeneration = generation;

	FWKLOG(( "IOFWIsochChannel::reallocBandwidth - exited with result = 0x%08lx\n", result ));

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
	return kIOReturnUnsupported;
}

