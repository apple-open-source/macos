/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

// #import <IOKit/firewire/IOFireLog.h>

#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/IOSyncer.h>
 
#include "IOFWAsyncStreamRxCommand.h"


OSDefineMetaClassAndStructors(IOFWAsyncStreamRxCommand, OSObject);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamRxCommand, 0);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamRxCommand, 1);

/*!
    @function initAll
	Initializes the AsyncStream Recieve command object
	@result true if successfull.
*/
bool IOFWAsyncStreamRxCommand::initAll(UInt32 channel, DCLCallCommandProc* proc, 
									   IOFireWireController *control,
									   UInt32 size,
									   void	*callbackObject)
{    
	IOReturn status	= kIOReturnSuccess;
	
	fControl = control;
	fFWIM = fControl->getLink();
	
	
	// Create a DCL program
	fdclProgram = CreateAsyncStreamRxDCLProgram(proc, size, callbackObject);
	if(fdclProgram == NULL){
		IOLog("    Error: AddAsyncStreamClient %d %x\n", __LINE__, status);
		return false;
	}

	fChan = channel;
	fSpeed = kFWSpeedMaximum;

	bStarted = false;
    fInitialized = true;
    
	return true;
}

void IOFWAsyncStreamRxCommand::free()
{
	//IOLog("    AddAsyncStreamClient Release called %d\n", __LINE__);
	stop();
	
	// free dcl program
	FreeAsyncStreamRxDCLProgram(fdclProgram);

	// free the buffer 
	fBufDesc->release();
	
	OSObject::free();
}

void IOFWAsyncStreamRxCommand::FreeAsyncStreamRxDCLProgram(DCLCommandStruct *dclProgram) 
{
    UInt32	seg = 0;
	DCLLabel			*startLabel;
	DCLTransferPacket	*receivePacket;
	DCLUpdateDCLList	*update;
	DCLCommand			*dclCommand;
	DCLCallProc			*callProc;
	RXProcData			*rxProcData;
    DCLJump				*jump;
    
    for (seg=0;	seg<MAX_BCAST_BUFFERS-1;	seg++)
	{
        // free label
        startLabel = receiveSegmentInfo[seg].pSegmentLabelDCL;
        if(startLabel == NULL)
            return;
            
        receivePacket = (DCLTransferPacket*)startLabel->pNextDCLCommand;
        if(receivePacket == NULL)
            return;
        
        update = (DCLUpdateDCLList*)receivePacket->pNextDCLCommand;
        if(update == NULL)
            return;
        
        dclCommand = (DCLCommand*)update->dclCommandList[0];
        if(dclCommand == NULL)
            return;
        
        callProc = (DCLCallProc*)update->pNextDCLCommand;
        if(callProc == NULL)
            return;
        
        rxProcData = (RXProcData*)callProc->procData;
        if(rxProcData == NULL)
            return;

        jump = (DCLJump *)callProc->pNextDCLCommand;
        if(jump == NULL)
            return;

        delete rxProcData;
        delete dclCommand;
        delete update;
        delete receivePacket;
        delete startLabel;
        delete jump;
       	delete callProc;
    }
    // free overrun label stuff
    receivePacket = (DCLTransferPacket*)fDCLOverrunLabel->pNextDCLCommand;
    if(receivePacket == NULL)
        return;
    
    update = (DCLUpdateDCLList*)receivePacket->pNextDCLCommand;
    if(update == NULL)
        return;
    
    dclCommand = (DCLCommand*)update->dclCommandList[0];
    if(dclCommand == NULL)
        return;
    
    callProc = (DCLCallProc*)update->pNextDCLCommand;
    if(callProc == NULL)
        return;
    
    rxProcData = (RXProcData*)callProc->procData;
    if(rxProcData == NULL)
        return;

    jump = (DCLJump *)callProc->pNextDCLCommand;
    if(jump == NULL)
        return;

    delete rxProcData;
    delete dclCommand;
    delete update;
    delete receivePacket;
    delete fDCLOverrunLabel;
    delete jump;
    delete callProc;
    
    delete receiveSegmentInfo;
}

DCLCommandStruct *IOFWAsyncStreamRxCommand::CreateAsyncStreamRxDCLProgram(DCLCallCommandProc* proc, UInt32 size, void *callbackObject) 
{
        
	// Create a DCL program
	fBufDesc = new IOBufferMemoryDescriptor;
    if(fBufDesc == NULL)
        return NULL;

	if(!fBufDesc->initWithOptions( 0, MAX_BCAST_BUFFERS* size, PAGE_SIZE))	// get buffer large enough for 2 packets
	{
        return NULL;
	}

    UInt8	*currentBuffer = (UInt8*)fBufDesc->getBytesNoCopy() ;

	receiveSegmentInfo = new IPRxSegment[MAX_BCAST_BUFFERS-1];
 
 
    // start of new way
   	for (fSeg=0;	fSeg<MAX_BCAST_BUFFERS-1;	fSeg++)
	{
        DCLLabel *pLastDCL = new DCLLabel ;
        {
            pLastDCL->opcode = kDCLLabelOp ;
        }

        receiveSegmentInfo[fSeg].pSegmentLabelDCL = NULL;
        receiveSegmentInfo[fSeg].pSegmentJumpDCL = NULL;

		// Allocate the label for this segment, and save pointer in seg info
		receiveSegmentInfo[fSeg].pSegmentLabelDCL = (DCLLabelPtr) pLastDCL;

		if (fSeg == 0)
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
			callProc->procData = (UInt32)new RXProcData ;
			
			((RXProcData*)callProc->procData)->obj = callbackObject;
			((RXProcData*)callProc->procData)->thisObj = this;
			((RXProcData*)callProc->procData)->buffer = currentBuffer ;
			((RXProcData*)callProc->procData)->index = fSeg ;
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
		receiveSegmentInfo[fSeg].pSegmentJumpDCL = jump;

//      IOLog("%s:%d %ld Label:%p Jump:%p\n", __FILE__, __LINE__, fSeg, pLastDCL, jump);        
    }
    
	// Allocate Overrun label & callback DCL
	fDCLOverrunLabel = new DCLLabel ;

    fDCLOverrunLabel->opcode = kDCLLabelOp ;

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
        callProc->proc = restart;
        callProc->procData = (UInt32)this;
    }
	
    fDCLOverrunLabel->pNextDCLCommand = (DCLCommand*)receivePacket ;
    receivePacket->pNextDCLCommand = (DCLCommand*)update ;
    update->pNextDCLCommand = (DCLCommand*)callProc ;
 	// Set the next pointer in the last DCL to nil
	callProc->pNextDCLCommand = NULL ;
    
    fixDCLJumps(false);

    
	return (DCLCommand*)fdclProgram;
}

IOIPPort *IOFWAsyncStreamRxCommand::CreateAsyncStreamPort(bool talking, DCLCommandStruct *opcodes, void *info,
													UInt32 startEvent, UInt32 startState, UInt32 startMask,
													UInt32 channel )
{
    IOIPPort *port;

    if(fFWIM == NULL)
    {
		IOLog("    %s:%d fFWIM not initialized\n", __FILE__, __LINE__);
		return NULL;
    }

    fIODclProgram = fFWIM->createDCLProgram(talking, opcodes, NULL, startEvent, startState, startMask);
    if(!fIODclProgram) 
	{
		IOLog("    %s:%d IODclProgram returned NULL from FWIM\n", __FILE__, __LINE__);
		return NULL;
	}

    port = new IOIPPort;
    if(!port) {
		fIODclProgram->release();
		return NULL;
    }

    if(!port->init(fIODclProgram, fControl, channel)) {
		IOLog("    %s:%d IOIPPort init failed \n", __FILE__, __LINE__);
		port->release();
		port = NULL;
    }

    return port;
}

IOReturn IOFWAsyncStreamRxCommand::start(IOFWSpeed fBroadCastSpeed) {

	IOReturn status	= kIOReturnSuccess;

    if(fInitialized == false)
        return status;

	if(bStarted) 
		return status;

//	IOLog("    starting channel ... %d\n", __LINE__);

	fSpeed = fBroadCastSpeed;

//	IOLog("    channel %ld \n", fChan);

	// Create IOIPPort with the required channel and DCL program
	fAsynStreamPort = CreateAsyncStreamPort(false, fdclProgram, NULL, 0, 0, 0, fChan);
	if(fAsynStreamPort == NULL){
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		IOLog("    Error: AddAsyncStreamClient %d %x\n", __LINE__, status);
		return kIOReturnError;
	}

	// Create a IOFWIsocChannel with the created port
	fAsyncStreamChan = fControl->createIsochChannel(false, 0, fSpeed, NULL, NULL);

	if(fAsyncStreamChan == NULL) {
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		fAsynStreamPort->release() ;
		IOLog("    Error: AddAsyncStreamClient %d %x\n", __LINE__, status);
		return kIOReturnError;
	}
	
	status = fAsyncStreamChan->addListener(fAsynStreamPort);
	if(status != kIOReturnSuccess){
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		fAsyncStreamChan->releaseChannel();
		fAsynStreamPort->release() ;
		IOLog("    Error: AddAsyncStreamClient %d %x\n", __LINE__, status);
		return kIOReturnError;
	}
	
	// Allocate channel
	status = fAsyncStreamChan->allocateChannel();
#ifdef FIREWIRETODO // Ignore the error right now
	if(status != kIOReturnSuccess){
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		fAsyncStreamChan->releaseChannel();
		fAsynStreamPort->release() ;
		IOLog("    Error: AddAsyncStreamClient %d %x\n", __LINE__, status);
		return kIOReturnError;
	}
#endif

    fixDCLJumps(true);
	
	// Start channel
	status = fAsyncStreamChan->start();
	if(status != kIOReturnSuccess){
		FreeAsyncStreamRxDCLProgram(fdclProgram);
		fAsyncStreamChan->releaseChannel();
		fAsynStreamPort->release() ;
		IOLog("    Error: AddAsyncStreamClient %d %x\n", __LINE__, status);
		return kIOReturnError;
	}
	
	bStarted = true;

//	fAsynStreamPort->printDCLProgram (fdclProgram,
//										0,
//										IOLog, 10);

		
	return status;
}

void IOFWAsyncStreamRxCommand::modifyDCLJumps(DCLCommandStruct *callProc)
{
	DCLCallProc 	*ptr = (DCLCallProc*)callProc;
	RXProcData		*proc = (RXProcData*)ptr->procData;
    IOReturn	error;
    
    if(proc == NULL)
    {
        IOLog("%s:%d NULL callproc data\n", __FILE__, __LINE__);
        return;
    }

    //
    // Make the current segment's jump point to OverRun Label
    //
	//   IOLog("seg:%d\n", proc->index);
    
    receiveSegmentInfo[proc->index].pSegmentJumpDCL->pJumpDCLLabel = fDCLOverrunLabel;
                            
    error = fAsynStreamPort->notify(kFWDCLModifyNotification,
                                    (DCLCommand**) & receiveSegmentInfo[proc->index].pSegmentJumpDCL,
                                    1);
    if(error != kIOReturnSuccess)
        IOLog("%s:%d %d\n", __FILE__, __LINE__, error);
    
    // loop previous dcl to current
    if(proc->index == 0)
    {
        // during first callback, make the last segment point to the first
        receiveSegmentInfo[MAX_BCAST_BUFFERS-2].pSegmentJumpDCL->pJumpDCLLabel = receiveSegmentInfo[0].pSegmentLabelDCL;
        error = fAsynStreamPort->notify(kFWDCLModifyNotification,
                                        (DCLCommand**) & receiveSegmentInfo[MAX_BCAST_BUFFERS-2].pSegmentJumpDCL,
                                        1);
        if(error != kIOReturnSuccess)
            IOLog("%s:%d %d\n", __FILE__, __LINE__, error);
    }
    else
    {
        // everything else here !, 
        receiveSegmentInfo[proc->index-1].pSegmentJumpDCL->pJumpDCLLabel = receiveSegmentInfo[proc->index].pSegmentLabelDCL;
        error = fAsynStreamPort->notify(kFWDCLModifyNotification,
                                        (DCLCommand**) & receiveSegmentInfo[proc->index-1].pSegmentJumpDCL,
                                        1);
        if(error != kIOReturnSuccess)
            IOLog("%s:%d %d\n", __FILE__, __LINE__, error);
    }
}

void IOFWAsyncStreamRxCommand::fixDCLJumps(bool	bRestart)
{
	UInt32 	 	i;
    IOReturn	error;

	for (i=0; i<MAX_BCAST_BUFFERS-1; i++)
	{
		if (i != (MAX_BCAST_BUFFERS-2))
        {
            receiveSegmentInfo[i].pSegmentJumpDCL->pJumpDCLLabel = receiveSegmentInfo[i+1].pSegmentLabelDCL;
            receiveSegmentInfo[i].pSegmentJumpDCL->pNextDCLCommand = (DCLCommand*)receiveSegmentInfo[i+1].pSegmentLabelDCL;
        }
		else
        {
            receiveSegmentInfo[i].pSegmentJumpDCL->pJumpDCLLabel = fDCLOverrunLabel;
			receiveSegmentInfo[i].pSegmentJumpDCL->pNextDCLCommand = (DCLCommand*)fDCLOverrunLabel;
        }

		//
		// Only if fAsynStreamPort becomes valid, do it !!
		//
		if(bRestart == true && fAsynStreamPort != NULL)
		{
			error = fAsynStreamPort->notify(kFWDCLModifyNotification,
											(DCLCommand**) & receiveSegmentInfo[i].pSegmentJumpDCL,
											1);
			if(error != kIOReturnSuccess)
				IOLog("%s:%d %d\n", __FILE__, __LINE__, error);
		}
	}
}

void IOFWAsyncStreamRxCommand::restart(DCLCommandStruct *callProc)
{
	DCLCallProc 				*ptr = (DCLCallProc*)callProc;
    IOFWAsyncStreamRxCommand	*fwRxAsyncStream = (IOFWAsyncStreamRxCommand*)ptr->procData;

    //
    // Overrun so restart everything !!
    //
	// IOLog("%s:%d  OR\n", __FILE__, __LINE__);
	fwRxAsyncStream->fIsoRxOverrun++;

	// Stop the channel
    fwRxAsyncStream->fAsyncStreamChan->stop();

	// Start the channel
    fwRxAsyncStream->fAsyncStreamChan->start();
}
	
IOReturn IOFWAsyncStreamRxCommand::stop() 
{
	IOReturn status	= kIOReturnSuccess;

    if(fInitialized == false)
        return status;

	if(!bStarted) 
		return status;

	// IOLog("    stopping channel ... %d\n", __LINE__);
	
	// Stop the channel
	fAsyncStreamChan->stop();

	// Lets release the channel
	fAsyncStreamChan->releaseChannel();
	
	// free the channel	
	fAsyncStreamChan->release();

	// free the port
	fAsynStreamPort->release();
	
	// Release the program, don't the port does it for you
	// fIODclProgram->release();

	bStarted = false;
	
	return status;
}


                                                                                                    
