/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/firewire/IOFWUtils.h>

#include "IOFWAsyncStreamReceiver.h"
#include "IOFWAsyncStreamReceivePort.h"
#include "IOFWAsyncStreamListener.h"
#include "FWDebugging.h"


OSDefineMetaClassAndStructors(IOFWAsyncStreamReceiver, OSObject);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamReceiver, 0);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamReceiver, 1);

/*!
    @function initAll
	Initializes the AsyncStream Recieve command object
	@result true if successfull.
*/
bool IOFWAsyncStreamReceiver::initAll( IOFireWireController *control, UInt32 channel )
{    
	IOReturn status	= kIOReturnSuccess;
	
	fControl	= control;
	fFWIM		= fControl->getLink();

	fControl->closeGate();
	
	// Create a DCL program
	fdclProgram = CreateAsyncStreamRxDCLProgram(receiveAsyncStream, this);
	if(fdclProgram == NULL){
		DebugLog("IOFWAsyncStreamReceiver::initAll -> CreateAsyncStreamRxDCLProgram failed %x\n", status);
		return false;
	}

	fChannel = channel;
	fSpeed	 = kFWSpeedMaximum;

	fActive = false;
    fInitialized = true;

	fAsyncStreamClients = OSSet::withCapacity(2);
	if( fAsyncStreamClients )
		fAsyncStreamClientIterator = OSCollectionIterator::withCollection( fAsyncStreamClients );

	status = activate(control->getBroadcastSpeed());

	if ( status == kIOReturnSuccess )
	{
		fIODclProgram->setForceStopProc( forceStopNotification, this, fAsyncStreamChan );
	}
	
	fControl->openGate();
	
	return ( status == kIOReturnSuccess );
}

void IOFWAsyncStreamReceiver::free()
{
	fControl->closeGate();

	deactivate();
	
	removeAllListeners();
	
	if ( fAsyncStreamClients )
	{
		fAsyncStreamClients->release();
		fAsyncStreamClients = NULL;
	}
	
	if( fAsyncStreamClientIterator )
	{
		fAsyncStreamClientIterator->release();
		fAsyncStreamClientIterator = NULL;
	}

	// free dcl program
	if( fdclProgram )
	{
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		fdclProgram = NULL;
	}
	
	// free the buffer 
	if( fBufDesc ) 
	{
		fBufDesc->release();
		fBufDesc = NULL;
	}

	fControl->openGate();

	OSObject::free();
}

void IOFWAsyncStreamReceiver::FreeAsyncStreamRxDCLProgram(DCLCommandStruct *dclProgram) 
{
    UInt32				seg = 0;
	DCLLabel			*startLabel;
	DCLTransferPacket	*receivePacket;
	DCLUpdateDCLList	*update;
	DCLCommand			*dclCommand;
	DCLCallProc			*callProc;
	FWAsyncStreamReceiveRefCon		*rxProcData;
    DCLJump				*jump;
    
	if ( fReceiveSegmentInfoPtr )
	{
		for (seg=0;	seg<kMaxAsyncStreamReceiveBuffers-1;	seg++)
		{
			// free label
			startLabel = fReceiveSegmentInfoPtr[seg].pSegmentLabelDCL;
			if(startLabel == NULL)
				break;
			
			receivePacket = (DCLTransferPacket*)startLabel->pNextDCLCommand;
			if(receivePacket == NULL)
				break;
			
			update = (DCLUpdateDCLList*)receivePacket->pNextDCLCommand;
			if(update == NULL)
				break;
			
			dclCommand = (DCLCommand*)update->dclCommandList[0];
			if(dclCommand == NULL)
				break;
			
			callProc = (DCLCallProc*)update->pNextDCLCommand;
			if(callProc == NULL)
				break;
			
			rxProcData = (FWAsyncStreamReceiveRefCon*)callProc->procData;
			if(rxProcData == NULL)
				break;

			jump = (DCLJump *)callProc->pNextDCLCommand;
			if(jump == NULL)
				break;

			delete rxProcData;
			delete dclCommand;
			delete update->dclCommandList;
			delete update;
			delete startLabel;
			delete jump;
			delete callProc;
		}
		delete fReceiveSegmentInfoPtr;
		fReceiveSegmentInfoPtr = NULL;
	}
	
	if ( fDCLOverrunLabelPtr )
	{
		receivePacket = (DCLTransferPacket*)fDCLOverrunLabelPtr->pNextDCLCommand;
		if(receivePacket != NULL)
		{
			update = (DCLUpdateDCLList*)receivePacket->pNextDCLCommand;
			if(update != NULL)
			{
				callProc = (DCLCallProc*)update->pNextDCLCommand;
				if(callProc != NULL)
					delete callProc;

				delete update->dclCommandList;
				delete update;
			}
			delete receivePacket;
		}
		delete fDCLOverrunLabelPtr;
		fDCLOverrunLabelPtr = NULL;
	}
}

DCLCommandStruct *IOFWAsyncStreamReceiver::CreateAsyncStreamRxDCLProgram(DCLCallCommandProc* proc, void *callbackObject) 
{
	// Create a DCL program
	UInt64 mask = fControl->getFireWirePhysicalBufferMask();							// get controller mask
	UInt16 size = kMaxAsyncStreamReceiveBufferSize;
	
	mask		&= ~((UInt64)(PAGE_SIZE-1));											// page align	
	fBufDesc	= IOBufferMemoryDescriptor::inTaskWithPhysicalMask(	
															kernel_task,				// kernel task
															0,							// options
															kMaxAsyncStreamReceiveBuffers*size,	// size
															mask );						// mask for physically addressable memory

    if(fBufDesc == NULL)
        return NULL;

    UInt8	*currentBuffer = (UInt8*)fBufDesc->getBytesNoCopy() ;

	fReceiveSegmentInfoPtr = new FWAsyncStreamReceiveSegment[kMaxAsyncStreamReceiveBuffers-1];
 
    // start of new way
   	for (fSegment=0;	fSegment<kMaxAsyncStreamReceiveBuffers-1;	fSegment++)
	{
        DCLLabel *pLastDCL = new DCLLabel ;
        {
            pLastDCL->opcode = kDCLLabelOp ;
        }

        fReceiveSegmentInfoPtr[fSegment].pSegmentLabelDCL = NULL;
        fReceiveSegmentInfoPtr[fSegment].pSegmentJumpDCL = NULL;

		// Allocate the label for this segment, and save pointer in seg info
		fReceiveSegmentInfoPtr[fSegment].pSegmentLabelDCL = (DCLLabelPtr) pLastDCL;

		if (fSegment == 0)
		{
			fdclProgram = (DCLCommand*)pLastDCL;
		}

        DCLTransferPacket	*receivePacket = new DCLTransferPacket ;
        {
            receivePacket->opcode = kDCLReceivePacketStartOp ;
            receivePacket->buffer = currentBuffer ;
            receivePacket->size = size ;
        }

        DCLUpdateDCLList	*update = new DCLUpdateDCLList ;
        {
            update->opcode = kDCLUpdateDCLListOp ;
            update->dclCommandList = new DCLCommand*[1] ;
            update->dclCommandList[0] = (DCLCommand*)receivePacket ;
            update->numDCLCommands = 1;		// Number of DCL commands in list.
        }

		DCLCallProc	*callProc = new DCLCallProc ;
		{
			callProc->opcode = kDCLCallProcOp ;
			callProc->proc = proc ;
			callProc->procData = (UInt32)new FWAsyncStreamReceiveRefCon ;
			
			((FWAsyncStreamReceiveRefCon*)callProc->procData)->obj = callbackObject;
			((FWAsyncStreamReceiveRefCon*)callProc->procData)->thisObj = this;
			((FWAsyncStreamReceiveRefCon*)callProc->procData)->buffer = currentBuffer ;
			((FWAsyncStreamReceiveRefCon*)callProc->procData)->index = fSegment ;
		}

		DCLJump	*jump = new DCLJump ;
		{
			jump->opcode = kDCLJumpOp ;
			jump->pJumpDCLLabel = pLastDCL ;
		}

		pLastDCL->pNextDCLCommand = (DCLCommand*)receivePacket ;
		receivePacket->pNextDCLCommand = (DCLCommand*)update ;
		update->pNextDCLCommand = (DCLCommand*)callProc ;
        callProc->pNextDCLCommand = (DCLCommand*)jump ;

		currentBuffer += receivePacket->size ;

        // Store the jump information.
		fReceiveSegmentInfoPtr[fSegment].pSegmentJumpDCL = jump;
    }
    
	// Allocate Overrun label & callback DCL
	fDCLOverrunLabelPtr = new DCLLabel ;

    fDCLOverrunLabelPtr->opcode = kDCLLabelOp ;

    DCLTransferPacket	*receivePacket = new DCLTransferPacket ;
    {
        receivePacket->opcode = kDCLReceivePacketStartOp ;
        receivePacket->buffer = currentBuffer ;
        receivePacket->size = size ;
    }

    DCLUpdateDCLList	*update = new DCLUpdateDCLList ;
    {
        update->opcode = kDCLUpdateDCLListOp ;
        update->dclCommandList = new DCLCommand*[1] ;
        update->dclCommandList[0] = (DCLCommand*)receivePacket ;
        update->numDCLCommands = 1;		// Number of DCL commands in list.
    }

    DCLCallProc	*callProc = new DCLCallProc ;
    {
        callProc->opcode = kDCLCallProcOp ;
        callProc->proc = overrunNotification;
        callProc->procData = (UInt32)this;
    }

    fDCLOverrunLabelPtr->pNextDCLCommand = (DCLCommand*)receivePacket ;
    receivePacket->pNextDCLCommand = (DCLCommand*)update ;
    update->pNextDCLCommand = (DCLCommand*)callProc ;
 	// Set the next pointer in the last DCL to nil
	callProc->pNextDCLCommand = NULL ;
    
    fixDCLJumps(false);

    
	return (DCLCommand*)fdclProgram;
}

IOFWAsyncStreamReceivePort *IOFWAsyncStreamReceiver::CreateAsyncStreamPort(bool talking, DCLCommandStruct *opcodes, void *info,
													UInt32 startEvent, UInt32 startState, UInt32 startMask,
													UInt32 channel )
{
    IOFWAsyncStreamReceivePort *port;

    if(fFWIM == NULL)
    {
		DebugLog("IOFWAsyncStreamReceiver::CreateAsyncStreamPort failed -> FWIM is NULL\n");
		return NULL;
    }

    fIODclProgram = fFWIM->createDCLProgram(talking, opcodes, NULL, startEvent, startState, startMask);
    if(!fIODclProgram) 
	{
		DebugLog("IOFWAsyncStreamReceiver::CreateAsyncStreamPort failed -> fIODclProgram is NULL\n");
		return NULL;
	}

    port = new IOFWAsyncStreamReceivePort;
    if(!port) 
	{
		DebugLog("IOFWAsyncStreamReceiver::CreateAsyncStreamPort failed -> IOFWAsyncStreamReceivePort is NULL\n");
		fIODclProgram->release();
		return NULL;
    }

    if(!port->init(fIODclProgram, fControl, channel)) 
	{
		DebugLog("IOFWAsyncStreamReceiver::CreateAsyncStreamPort failed -> IOFWAsyncStreamReceivePort::init failed\n");
		port->release();
		port = NULL;
    }

    return port;
}

IOReturn IOFWAsyncStreamReceiver::activate(IOFWSpeed fBroadCastSpeed) 
{
	IOReturn status	= kIOReturnSuccess;

    if(fInitialized == false)
        return status;

	if(fActive) 
		return status;

	fSpeed = fBroadCastSpeed;

	// Create IOFWAsyncStreamReceivePort with the required channel and DCL program
	fAsynStreamPort = CreateAsyncStreamPort(false, fdclProgram, NULL, 0, 0, 0, fChannel);
	if(fAsynStreamPort == NULL)
	{
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		DebugLog("IOFWAsyncStreamReceiver::activate -> CreateAsyncStreamPort failed %x\n", status);
		return kIOReturnError;
	}

	// Create a IOFWIsocChannel with the created port
	fAsyncStreamChan = fControl->createIsochChannel(false, 0, fSpeed, NULL, NULL);
	if(fAsyncStreamChan == NULL) 
	{
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		fAsynStreamPort->release() ;
		fAsynStreamPort = NULL;
		DebugLog("IOFWAsyncStreamReceiver::activate -> createIsochChannel failed %x\n", status);
		return kIOReturnError;
	}
	
	status = fAsyncStreamChan->addListener(fAsynStreamPort);
	if(status != kIOReturnSuccess)
	{
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		fAsyncStreamChan->stop();
		fAsyncStreamChan->releaseChannel();
		fAsyncStreamChan->release();
		fAsyncStreamChan = NULL;
		fAsynStreamPort->release() ;
		fAsynStreamPort = NULL;
		DebugLog("IOFWAsyncStreamReceiver::activate -> addListener failed %x\n", status);
		return kIOReturnError;
	}
	
	// Allocate channel
	status = fAsyncStreamChan->allocateChannel();
	
    fixDCLJumps(true);

	// Start channel
	status = fAsyncStreamChan->start();
	if(status != kIOReturnSuccess)
	{
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		fAsyncStreamChan->stop();
		fAsyncStreamChan->releaseChannel();
		fAsyncStreamChan->release();
		fAsyncStreamChan = NULL;
		fAsynStreamPort->release() ;
		fAsynStreamPort = NULL;
		DebugLog("IOFWAsyncStreamReceiver::activate -> channel start failed %x\n", status);
		return kIOReturnError;
	}
	
	fActive = true;
		
	return status;
}

IOReturn IOFWAsyncStreamReceiver::modifyDCLJumps(DCLCommandStruct *callProc)
{
	DCLCallProc 	*ptr = (DCLCallProc*)callProc;
	FWAsyncStreamReceiveRefCon		*proc = (FWAsyncStreamReceiveRefCon*)ptr->procData;
    IOReturn		status = kIOReturnError;

    if(fInitialized == false)
        return status;

	if(!fActive) 
		return status;
    
    if(proc == NULL)
    {
        DebugLog("IOFWAsyncStreamReceiver::modifyDCLJumps callproc data is NULL\n");
        return status;
    }

    // Make the current segment's jump point to OverRun Label
	UInt16 jumpIndex	= (proc->index - 1 + (kMaxAsyncStreamReceiveBuffers-1)) % (kMaxAsyncStreamReceiveBuffers-1);
	
	fReceiveSegmentInfoPtr[proc->index].pSegmentJumpDCL->pJumpDCLLabel = fDCLOverrunLabelPtr;
	status = fAsynStreamPort->notify(kFWDCLModifyNotification,
									(DCLCommand**) & fReceiveSegmentInfoPtr[proc->index].pSegmentJumpDCL,
									1);
	if(status != kIOReturnSuccess)
		DebugLog("IOFWAsyncStreamReceiver::modifyDCLJumps failed %x\n", status);

	fReceiveSegmentInfoPtr[jumpIndex].pSegmentJumpDCL->pJumpDCLLabel = fReceiveSegmentInfoPtr[proc->index].pSegmentLabelDCL;
	status = fAsynStreamPort->notify(kFWDCLModifyNotification,
									(DCLCommand**) & fReceiveSegmentInfoPtr[jumpIndex].pSegmentJumpDCL,
									1);
	if(status != kIOReturnSuccess)
		DebugLog("IOFWAsyncStreamReceiver::modifyDCLJumps failed %x\n", status);
		
	return status;
}

void IOFWAsyncStreamReceiver::fixDCLJumps(bool	restart)
{
	UInt32 	 	i;
    IOReturn	error;

	for (i=0; i<kMaxAsyncStreamReceiveBuffers-1; i++)
	{
		if (i != (kMaxAsyncStreamReceiveBuffers-2))
        {
            fReceiveSegmentInfoPtr[i].pSegmentJumpDCL->pJumpDCLLabel = fReceiveSegmentInfoPtr[i+1].pSegmentLabelDCL;
            fReceiveSegmentInfoPtr[i].pSegmentJumpDCL->pNextDCLCommand = (DCLCommand*)fReceiveSegmentInfoPtr[i+1].pSegmentLabelDCL;
        }
		else
        {
            fReceiveSegmentInfoPtr[i].pSegmentJumpDCL->pJumpDCLLabel = fDCLOverrunLabelPtr;
			fReceiveSegmentInfoPtr[i].pSegmentJumpDCL->pNextDCLCommand = (DCLCommand*)fDCLOverrunLabelPtr;
        }

		// Only if fAsynStreamPort becomes valid, do it !!
		if(restart == true && fAsynStreamPort != NULL)
		{
			error = fAsynStreamPort->notify(kFWDCLModifyNotification,
											(DCLCommand**) & fReceiveSegmentInfoPtr[i].pSegmentJumpDCL,
											1);
			if(error != kIOReturnSuccess)
				DebugLog("IOFWAsyncStreamReceiver::fixDCLJumps failed %x\n", error);
		}
	}
}

void IOFWAsyncStreamReceiver::overrunNotification(DCLCommandStruct *callProc)
{
	DCLCallProc 				*ptr = (DCLCallProc*)callProc;
	
	if( ptr )
	{
	    IOFWAsyncStreamReceiver	*fwRxAsyncStream = OSDynamicCast( IOFWAsyncStreamReceiver, (OSObject*)ptr->procData );

		if( fwRxAsyncStream )
		{
			fwRxAsyncStream->fIsoRxOverrun++;
			fwRxAsyncStream->restart();
		}
	}
}

IOReturn IOFWAsyncStreamReceiver::forceStopNotification( void* refCon, IOFWIsochChannel* channel, UInt32 stopCondition )
{
    IOFWAsyncStreamReceiver	*fwRxAsyncStream = OSDynamicCast(IOFWAsyncStreamReceiver, (OSObject*)refCon);

	if( fwRxAsyncStream )
		fwRxAsyncStream->restart();
	
	return kIOReturnSuccess;
}

void IOFWAsyncStreamReceiver::restart()
{
	// Stop the channel
    fAsyncStreamChan->stop();

    fixDCLJumps(true);

	// Start the channel
    fAsyncStreamChan->start();
}
	
IOReturn IOFWAsyncStreamReceiver::deactivate() 
{
	IOReturn status	= kIOReturnSuccess;

    if(fInitialized == false)
        return status;

	if(!fActive) 
		return status;

	// Stop the channel
	if( fAsyncStreamChan )
		fAsyncStreamChan->stop();

	// Lets release the channel
	if( fAsyncStreamChan )
		fAsyncStreamChan->releaseChannel();

	// free the channel	
	if( fAsyncStreamChan )
	{
		fAsyncStreamChan->release();
		fAsyncStreamChan = NULL;
	}
	
	// free the port
	if( fAsynStreamPort )
	{
		fAsynStreamPort->release();
		fAsynStreamPort = NULL;
	}
	
	fActive = false;
	
	return status;
}

void IOFWAsyncStreamReceiver::receiveAsyncStream(DCLCommandStruct *callProc)
{
	DCLCallProc	*ptr = (DCLCallProc*)callProc;

	if( not ptr ) return;
		
	FWAsyncStreamReceiveRefCon	*proc = (FWAsyncStreamReceiveRefCon*)ptr->procData;

	if( not proc ) return;
	
	IOFWAsyncStreamReceiver *receiver = (IOFWAsyncStreamReceiver*)proc->thisObj;
	
	if( not receiver) return;

	UInt8 *buffer = proc->buffer;

	receiver->indicateListeners( buffer );

    if( receiver->modifyDCLJumps( callProc ) == kIOReturnError ) return;
}

bool IOFWAsyncStreamReceiver::addListener ( IOFWAsyncStreamListener *listener )
{
	fControl->closeGate();

	bool ret = true;

    if( listener )
	{
		if(not fAsyncStreamClients->setObject( listener ))
			ret = false;
	}
	
	fControl->openGate();

	return ret;
}

void IOFWAsyncStreamReceiver::removeAllListeners()
{
	IOFWAsyncStreamListener * found;

	fControl->closeGate();

	fAsyncStreamClientIterator->reset();
	
    while( (found = (IOFWAsyncStreamListener *) fAsyncStreamClientIterator->getNextObject())) 
		fAsyncStreamClients->removeObject(found);

	fControl->openGate();
        
	return;
}

void IOFWAsyncStreamReceiver::removeListener ( IOFWAsyncStreamListener *listener )
{
	fControl->closeGate();
	
	if( listener )
		fAsyncStreamClients->removeObject(listener);

	fControl->openGate();
	
	return;
}

void IOFWAsyncStreamReceiver::indicateListeners ( UInt8 *buffer )
{
	IOFWAsyncStreamListener * found;

	fAsyncStreamClientIterator->reset();
	
    while( (found = (IOFWAsyncStreamListener *) fAsyncStreamClientIterator->getNextObject() ) ) 
		found->invokeClients(buffer);
	
}

UInt32	IOFWAsyncStreamReceiver::getClientsCount()
{
	return fAsyncStreamClients->getCount();
}


OSDefineMetaClassAndStructors(IOFWAsyncStreamListener, OSObject);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamListener, 0);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamListener, 1);

bool IOFWAsyncStreamListener::initAll(IOFireWireController *control, UInt32 channel, FWAsyncStreamReceiveCallback proc, void *obj)
{
	fControl  = control;

	fControl->closeGate();

    fReceiver = control->getAsyncStreamReceiver( channel );
	
	bool ret = false;
	
	if( fReceiver == NULL ) 
		fReceiver = control->allocAsyncStreamReceiver( channel, proc, obj );

	if( fReceiver and fReceiver->addListener( this ) )
	{
		fReceiver->retain();
		fClientProc	= proc;
		fRefCon		= obj;
		ret			= true;
		TurnOffNotification();
	}

	fControl->openGate();
		
	return ret;
}

const FWAsyncStreamReceiveCallback IOFWAsyncStreamListener::setListenerHandler( FWAsyncStreamReceiveCallback inReceiver )
{ 
	FWAsyncStreamReceiveCallback previousCallback = fClientProc;
	fClientProc = inReceiver; 
	
	return previousCallback; 
}

void IOFWAsyncStreamListener::free()
{
	fControl->closeGate();

	if( fReceiver )
	{
		if( fReceiver->getClientsCount() == 0 )
			fControl->removeAsyncStreamReceiver(fReceiver);

		fReceiver->release();

		fReceiver  = NULL;
	}

	fControl->openGate();
	
	OSObject::free();
}

void IOFWAsyncStreamListener::invokeClients(UInt8 *buffer)
{
	if( fNotify ) 
	{
		fClientProc(fRefCon, buffer);
	}
}																				

UInt32 IOFWAsyncStreamListener::getOverrunCounter()
{ 
	return fReceiver->getOverrunCounter(); 
}

void IOFWAsyncStreamListener::setFlags( UInt32 flags )
{
	fFlags = flags; 
}

UInt32 IOFWAsyncStreamListener::getFlags()
{
	return fFlags; 
}


