/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/system.h>

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOCommandPool.h>

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBLog.h>

#define super IOUSBBus
#define self this

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
IOReturn IOUSBController::OpenPipe(USBDeviceAddress address, UInt8 speed,
						Endpoint *endpoint)
{
    return _commandGate->runAction(DoCreateEP, (void *)(UInt32) address,
			(void *)(UInt32) speed, endpoint);
}

IOReturn IOUSBController::ClosePipe(USBDeviceAddress address,
                                    		Endpoint * endpoint)
{
    return _commandGate->runAction(DoDeleteEP, (void *)(UInt32) address,
			(void *)(UInt32) endpoint->number, (void *)(UInt32) endpoint->direction);
}

IOReturn IOUSBController::AbortPipe(USBDeviceAddress address,
                                    Endpoint * endpoint)
{
    return _commandGate->runAction(DoAbortEP, (void *)(UInt32) address,
			(void *)(UInt32) endpoint->number, (void *)(UInt32) endpoint->direction);
}

IOReturn IOUSBController::ResetPipe(USBDeviceAddress address,
                                    Endpoint * endpoint)
{
    return _commandGate->runAction(DoClearEPStall, (void *)(UInt32) address,
			(void *)(UInt32) endpoint->number, (void *)(UInt32) endpoint->direction);
}

IOReturn IOUSBController::ClearPipeStall(USBDeviceAddress address,
                                         Endpoint * endpoint)
{
    return _commandGate->runAction(DoClearEPStall, (void *)(UInt32) address,
			(void *)(UInt32) endpoint->number, (void *)(UInt32) endpoint->direction);
}


static void 
DisjointCompletion(void *target, void *parameter, IOReturn status, UInt32 bufferSizeRemaining)
{
    IOUSBCommand 	*command = (IOUSBCommand *)target;
    char		*buf = (char*)parameter;

    if (!command || !buf)
    {
	USBError(1, "IOUSBController: DisjointCompletion sanity check failed");
	return;
    }
    if (command->GetDirection() == kUSBIn)
    {
	USBLog(7, "IOUSBController: DisjointCompletion, copying 0x%lx out of 0x%lx bytes to desc %p from buffer %p", (command->GetDblBufLength()-bufferSizeRemaining), command->GetDblBufLength(), command->GetOrigBuffer(), buf);
	command->GetOrigBuffer()->writeBytes(0, buf, (command->GetDblBufLength()-bufferSizeRemaining));
    }
	
    command->GetBuffer()->complete();
    command->GetBuffer()->release();			// done with this buffer
    IOFree(buf, command->GetDblBufLength());
    // now call through to the original completion routine
    USBLog(7, "IOUSBController: DisjointCompletion calling through!");
    IOUSBCompletion completion = command->GetDisjointCompletion();
    if (completion.action)  
	(*completion.action)(completion.target, completion.parameter, status, bufferSizeRemaining);
}


// since this is a new method, I am not making it a member function, so that I don't
// have to change the class definition
OSMetaClassDefineReservedUsed(IOUSBController,  17);
IOReturn
IOUSBController::CheckForDisjointDescriptor(IOUSBCommand *command, UInt16 maxPacketSize)
{
    IOMemoryDescriptor		*buf = command->GetBuffer();
    IOMemoryDescriptor		*newBuf = NULL;
    IOByteCount			length = command->GetReqCount();
    IOByteCount			segLength = 0;
    IOByteCount			offset = 0;
    char			*segPtr;
    IOReturn			err;
    
    // Zero length buffers are valid, but they are surely not disjoint, so just return success.  
    //
    if ( length == 0 )
        return kIOReturnSuccess;
        
    while (length)
    {
        segPtr = (char*)buf->getPhysicalSegment(offset, &segLength);
        if (!segPtr)
        {
            USBError(1, "IOUSBController: CheckForDisjointDescriptor - no segPtr at offset %d, length = %d, segLength = %d, total length = %d, buf = %p", (int)offset, (int)length, (int)segLength, (int)command->GetReqCount(), buf);
            return kIOReturnBadArgument;
        }
	
	// 3036056 since length might be less than the length of the descriptor, we are OK if the physical
	// segment is longer than we need
        if (segLength >= length)
            return kIOReturnSuccess;		// this is the last segment, so we are OK
            
        // so the segment is less than the rest of the length - we need to check against maxPacketSize
        if (segLength % maxPacketSize)
        {
            // this is the error case. I need to copy the descriptor to a new descriptor and remember that I did it
            USBLog(7, "IOUSBController: CheckForDisjointDescriptor - found a disjoint segment of length %d!", (int)segLength);
	    length = command->GetReqCount();		// we will not return to the while loop, so don't worry about changing the value of length
	    // allocate a new descriptor which is the same total length as the old one
	    segPtr = (char*)IOMalloc(length);
	    if (!segPtr)
	    {
           	USBError(1, "IOUSBController: CheckForDisjointDescriptor - could not allocate new buffer");
		return kIOReturnNoMemory;
	    }
	    USBLog(7, "IOUSBController: CheckForDisjointDescriptor, obtained buffer %p of length %d", segPtr, (int)length);
	    // copy the bytes to the buffer if necessary
	    if (command->GetDirection() == kUSBOut)
	    {
		USBLog(7, "IOUSBController: CheckForDisjointDescriptor, copying %d bytes from desc %p from buffer %p", (int)length, buf, segPtr);
		if (buf->readBytes(0, segPtr, length) != length)
		{
		    USBError(1, "IOUSBController: CheckForDisjointDescriptor - bad copy on a write");
		    IOFree(segPtr, length);
		    return kIOReturnNoMemory;
		}
	    }
	    newBuf = IOMemoryDescriptor::withAddress(segPtr, length, (command->GetDirection() == kUSBIn) ? kIODirectionIn : kIODirectionOut);
	    if (!newBuf)
	    {
           	USBError(1, "IOUSBController: CheckForDisjointDescriptor - could not create new IOMemoryDescriptor");
		IOFree(segPtr, length);
		return kIOReturnNoMemory;
	    }
	    err = newBuf->prepare();
	    if (err)
	    {
           	USBError(1, "IOUSBController: CheckForDisjointDescriptor - err 0x%x in prepare", err);
		newBuf->release();
		IOFree(segPtr, length);
		return err;
	    }
	    command->SetOrigBuffer(command->GetBuffer());
	    command->SetDisjointCompletion(command->GetClientCompletion());
	    command->SetBuffer(newBuf);
	    
	    IOUSBCompletion completion;
	    completion.target = command;
	    completion.action = DisjointCompletion;
	    completion.parameter = segPtr;
	    command->SetClientCompletion(completion);
	    
	    command->SetDblBufLength(length);			// for the IOFree - the other buffer may change size
            return kIOReturnSuccess;
        }
        length -= segLength;		// adjust our master length pointer
	offset += segLength;
    }
    USBLog(5, "IOUSBController: CheckForDisjointDescriptor - returning kIOReturnBadArgument(0x%x)", kIOReturnBadArgument);
    return kIOReturnBadArgument;
}



// Transferring Data
IOReturn 
IOUSBController::Read(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion)
{
    USBLog(7, "%s[%p]::Read #1", getName(), this);
    return  Read(buffer, address, endpoint, completion, 0, 0);
}



OSMetaClassDefineReservedUsed(IOUSBController,  7);
IOReturn 
IOUSBController::Read(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout)
{    
    USBLog(7, "%s[%p]::Read #2", getName(), this);
    // Validate that there is a buffer so that we can call getLength on it
    if (!buffer)
    {
        USBLog(5, "%s[%p]::Read #2 - No Buffer, returning kIOReturnBadArgument(0x%x)", getName(), this, kIOReturnBadArgument);
	return kIOReturnBadArgument;
    }
    
    return Read(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, buffer->getLength());
}


OSMetaClassDefineReservedUsed(IOUSBController,  12);
IOReturn 
IOUSBController::Read(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
    IOReturn	 	err = kIOReturnSuccess;
    IOUSBCommand 	*command;
    IOUSBCompletion 	nullCompletion;
    int			i;

    USBLog(7, "%s[%p]::Read #3 - reqCount = %ld", getName(), this, reqCount);
    // Validate its a inny pipe and that there is a buffer
    if ((endpoint->direction != kUSBIn) || !buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "%s[%p]::Read #3 - direction is not kUSBIn (%d), No Buffer, or buffer length < reqCount (%ld < %ld). Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->direction,  buffer->getLength(), reqCount, kIOReturnBadArgument);
	return kIOReturnBadArgument;
    }
    
    if ((endpoint->transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "%s[%p]::Read #3 - Pipe is NOT kUSBBulk (%d) AND specified a timeout (%ld, %ld).  Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->transferType, noDataTimeout, completionTimeout, kIOReturnBadArgument);
	return kIOReturnBadArgument; // timeouts only on bulk pipes
    }
    
    // Validate the completion
    if (!completion)
    {
        USBLog(5, "%s[%p]::Read #3 - No Completion routine.  Returning kIOReturnNoCompletion(0x%x)", getName(), this, kIOReturnNoCompletion);
	return kIOReturnNoCompletion;
    }
    
    // Validate the command gate
    if (!_commandGate)
    {
        USBLog(5, "%s[%p]::Read #3 - Could not get _commandGate.  Returning kIOReturnInternalError(0x%x)", getName(), this, kIOReturnInternalError);
	return kIOReturnInternalError;
    }
    
    // allocate the command
    command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
	
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseCommandPool();
        
        command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::Read #3 Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }

	// Set up a flag indicating that we have a synchronous request in this command
	//
    if (  (UInt32) completion->action == (UInt32) &IOUSBSyncCompletion )
		command->SetIsSyncTransfer(true);
	else
		command->SetIsSyncTransfer(false);
	
    command->SetUseTimeStamp(false);
    command->SetSelector(READ);
    command->SetRequest(0);            	// Not a device request
    command->SetAddress(address);
    command->SetEndpoint(endpoint->number);
    command->SetDirection(kUSBIn);
    command->SetType(endpoint->transferType);
    command->SetBuffer(buffer);
    command->SetReqCount(reqCount);
    command->SetClientCompletion(*completion);
    command->SetNoDataTimeout(noDataTimeout);
    command->SetCompletionTimeout(completionTimeout);
    for (i=0; i < 10; i++)
		command->SetUIMScratch(i, 0);
	
    nullCompletion.target = (void *) NULL;
    nullCompletion.action = (IOUSBCompletionAction) NULL;
    nullCompletion.parameter = (void *) NULL;
    command->SetDisjointCompletion(nullCompletion);
    
    err = CheckForDisjointDescriptor(command, endpoint->maxPacketSize);
    if (kIOReturnSuccess == err)
	{
		err = _commandGate->runAction(DoIOTransfer, command);
		
		// If we have a sync request, then we always return the command after the DoIOTransfer.  If it's an async request, we only return it if 
		// we get an immediate error
		//
		if ( command->GetIsSyncTransfer() ||  (!command->GetIsSyncTransfer() && (kIOReturnSuccess != err)) )
		{
			_freeUSBCommandPool->returnCommand(command);
		}
	}
	else
	{
		// CheckFordDisjoint returned an error, so free up the comand
		//
		_freeUSBCommandPool->returnCommand(command);
	}
	
    return err;
}



IOReturn 
IOUSBController::Write(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion)
{
    USBLog(7, "%s[%p]::Write #1", getName(), this);
    return Write(buffer, address, endpoint, completion, 0, 0);
}



OSMetaClassDefineReservedUsed(IOUSBController,  8);
IOReturn 
IOUSBController::Write(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    USBLog(7, "%s[%p]::Write #2", getName(), this);
    // Validate that we have a buffer so that we can call getLength on it
    if(!buffer)
    {
        USBLog(5, "%s[%p]::Write #2 - No buffer!.  Returning kIOReturnBadArgument(0x%x)", getName(), this, kIOReturnBadArgument);
	return kIOReturnBadArgument;
    }

    return Write(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, buffer->getLength());
}



OSMetaClassDefineReservedUsed(IOUSBController,  13);
IOReturn 
IOUSBController::Write(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
    IOReturn		 err = kIOReturnSuccess;
    IOUSBCommand 	*command;
    IOUSBCompletion 	nullCompletion;
    int			i;

    USBLog(7, "%s[%p]::Write #3 - reqCount = %ld", getName(), this, reqCount);
    
    // Validate its a outty pipe and that we have a buffer
    if((endpoint->direction != kUSBOut) || !buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "%s[%p]::Write #3 - direction is not kUSBOut (%d), No Buffer, or buffer length < reqCount (%ld < %ld). Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->direction,  buffer->getLength(), reqCount, kIOReturnBadArgument);
	return kIOReturnBadArgument;
    }

    if ((endpoint->transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "%s[%p]::Write #3 - Pipe is NOT kUSBBulk (%d) AND specified a timeout (%ld, %ld).  Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->transferType, noDataTimeout, completionTimeout, kIOReturnBadArgument);
	return kIOReturnBadArgument;							// timeouts only on bulk pipes
    }
	
    // Validate the command gate
    if (!_commandGate)
    {
        USBLog(5, "%s[%p]::Write #3 - Could not get _commandGate.  Returning kIOReturnInternalError(0x%x)", getName(), this, kIOReturnInternalError);
		return kIOReturnInternalError;
    }

    // allocate the command
    command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
    
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseCommandPool();
        
        command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::Write #3 Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }

	// Set up a flag indicating that we have a synchronous request in this command
	//
    if (  (UInt32) completion->action == (UInt32) &IOUSBSyncCompletion )
		command->SetIsSyncTransfer(true);
	else
		command->SetIsSyncTransfer(false);
	
    command->SetUseTimeStamp(false);
    command->SetSelector(WRITE);
    command->SetRequest(0);            // Not a device request
    command->SetAddress(address);
    command->SetEndpoint(endpoint->number);
    command->SetDirection(kUSBOut);
    command->SetType(endpoint->transferType);
    command->SetBuffer(buffer);
    command->SetReqCount(reqCount);
    command->SetClientCompletion(*completion);
    command->SetNoDataTimeout(noDataTimeout); 
    command->SetCompletionTimeout(completionTimeout);
    for (i=0; i < 10; i++)
	command->SetUIMScratch(i, 0);

    nullCompletion.target = (void *) NULL;
    nullCompletion.action = (IOUSBCompletionAction) NULL;
    nullCompletion.parameter = (void *) NULL;
    command->SetDisjointCompletion(nullCompletion);

    err = CheckForDisjointDescriptor(command, endpoint->maxPacketSize);
    if (kIOReturnSuccess == err)
	{
		err = _commandGate->runAction(DoIOTransfer, command);
		
		// If we have a sync request, then we always return the command after the DoIOTransfer.  If it's an async request, we only return it if 
		// we get an immediate error
		//
		if ( command->GetIsSyncTransfer() ||  (!command->GetIsSyncTransfer() && (kIOReturnSuccess != err)) )
		{
			_freeUSBCommandPool->returnCommand(command);
		}
	}
	else
	{
		// CheckFordDisjoint returned an error, so free up the comand
		//
		_freeUSBCommandPool->returnCommand(command);
	}
	
    return err;
}



IOReturn 
IOUSBController::IsocIO(	IOMemoryDescriptor *	buffer,
							UInt64					frameStart,
							UInt32					numFrames,
							IOUSBIsocFrame *		frameList,
                               USBDeviceAddress address,
                               Endpoint * endpoint,
                               IOUSBIsocCompletion * completion)
{
    IOReturn	 err = kIOReturnSuccess;
    IOUSBIsocCommand *	command;
    bool		crossEndianRequest = false;
	
	// Validate the completion
	//
        if (completion == 0)
        {
            USBLog(5, "%s[%p]::IsocIO - No completion.  Returning kIOReturnNoCompletion(0x%x)", getName(), this, kIOReturnNoCompletion);
		return kIOReturnNoCompletion;
	}
	
	// Validate the commandGate
	//
	if (_commandGate == 0)
	{
		USBLog(5, "%s[%p]::IsocIO - Could not get _commandGate.  Returning kIOReturnInternalError(0x%x)", getName(), this, kIOReturnInternalError);
		return kIOReturnInternalError;
	}
	
		// If the high order bit of the endpoint transfer type is set, then this means it's a request from an Rosetta client
		if ( endpoint->direction & 0x80 )
		{
			endpoint->direction &= ~0x80;
			crossEndianRequest = true;
		}

		// Validate the direction of the endpoint -- it has to be kUSBIn or kUSBOut
	if ( (endpoint->direction != kUSBOut) && ( endpoint->direction != kUSBIn) )
	{		
		USBLog(5, "%s[%p]::IsocIO - Direction is not kUSBOut or kUSBIn (%d).  Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->direction, kIOReturnBadArgument);
		return kIOReturnBadArgument;
	}

	command = (IOUSBIsocCommand *)_freeUSBIsocCommandPool->getCommand(false);
	
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseIsocCommandPool();
        
        command = (IOUSBIsocCommand *)_freeUSBIsocCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::IsocIO Could not get a IOUSBIsocCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }
    
	// If the high order bit of the endpoint transfer type is set, then this means it's a request from an Rosetta client
	if ( crossEndianRequest )
	{
		command->SetRosettaClient(true);
	}
	else
		command->SetRosettaClient(false);
	
	// Set up a flag indicating that we have a synchronous request in this command
	//
    if (  (UInt32) completion->action == (UInt32) &IOUSBSyncIsoCompletion )
		command->SetIsSyncTransfer(true);
	else
		command->SetIsSyncTransfer(false);
	
	// Setup the direction
	if (endpoint->direction == kUSBOut) 
	{
		command->SetSelector(WRITE);
		command->SetDirection(kUSBOut);
	}
	else if (endpoint->direction == kUSBIn) 
	{
		command->SetSelector(READ);
		command->SetDirection(kUSBIn);
        }

        command->SetUseTimeStamp(false);
        command->SetAddress(address);
        command->SetEndpoint(endpoint->number);
        command->SetBuffer(buffer);
        command->SetCompletion(*completion);
        command->SetStartFrame(frameStart);
        command->SetNumFrames(numFrames);
        command->SetFrameList(frameList);
        command->SetStatus(kIOReturnBadArgument);

	err = _commandGate->runAction(DoIsocTransfer, command);

	// If we have a sync request, then we always return the command after the DoIOTransfer.  If it's an async request, we only return it if 
	// we get an immediate error
	//
	if ( command->GetIsSyncTransfer() || (!command->GetIsSyncTransfer() && (kIOReturnSuccess != err)) )
	{
		_freeUSBIsocCommandPool->returnCommand(command);
	}
	
    return err;
}

OSMetaClassDefineReservedUsed(IOUSBController,  15);
IOReturn 
IOUSBController::IsocIO(	IOMemoryDescriptor *			buffer,
							UInt64							frameStart,
							UInt32							numFrames,
							IOUSBLowLatencyIsocFrame *		frameList,
                               USBDeviceAddress address,
                               Endpoint * endpoint,
                               IOUSBLowLatencyIsocCompletion * completion,
                               UInt32 updateFrequency)
{
    IOReturn	 err = kIOReturnSuccess;
    IOUSBIsocCommand *	command;
    bool		crossEndianRequest = false;
    
	// Validate the completion
	//
	if (completion == 0)
	{
		USBLog(5, "%s[%p]::IsocIO(LL) - No completion.  Returning kIOReturnNoCompletion(0x%x)", getName(), this, kIOReturnNoCompletion);
		return kIOReturnNoCompletion;
	}
	
	// Validate the commandGate
	//
	if (_commandGate == 0)
	{
		USBLog(5, "%s[%p]::IsocIO(LL) - Could not get _commandGate.  Returning kIOReturnInternalError(0x%x)", getName(), this, kIOReturnInternalError);
		return kIOReturnInternalError;
	}
	
	// If the high order bit of the endpoint transfer type is set, then this means it's a request from an Rosetta client
	if ( endpoint->direction & 0x80 )
	{
		endpoint->direction &= ~0x80;
		crossEndianRequest = true;
	}
	
	// Validate the direction of the endpoint -- it has to be kUSBIn or kUSBOut
	if ( (endpoint->direction != kUSBOut) && ( endpoint->direction != kUSBIn) )
	{		
		USBLog(5, "%s[%p]::IsocIO(LL) - Direction is not kUSBOut or kUSBIn (%d).  Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->direction, kIOReturnBadArgument);
		return kIOReturnBadArgument;
	}
	
	command = (IOUSBIsocCommand *)_freeUSBIsocCommandPool->getCommand(false);
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseIsocCommandPool();
        
        command = (IOUSBIsocCommand *)_freeUSBIsocCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::IsocIO(LL) Could not get a IOUSBIsocCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }
	
	// If the high order bit of the endpoint transfer type is set, then this means it's a request from an Rosetta client
	if ( crossEndianRequest )
	{
		command->SetRosettaClient(true);
	}
	else
		command->SetRosettaClient(false);
	
	// Set up a flag indicating that we have a synchronous request in this command
	//
    if (  (UInt32) completion->action == (UInt32) &IOUSBSyncIsoCompletion )
		command->SetIsSyncTransfer(true);
	else
		command->SetIsSyncTransfer(false);
	
	// Setup the direction
	if (endpoint->direction == kUSBOut) {
		command->SetSelector(WRITE);
		command->SetDirection(kUSBOut);
	}
	else if (endpoint->direction == kUSBIn) {
		command->SetSelector(READ);
		command->SetDirection(kUSBIn);
        }

        command->SetUseTimeStamp(false);
        command->SetAddress(address);
        command->SetEndpoint(endpoint->number);
        command->SetBuffer(buffer);
        command->SetCompletion( * ((IOUSBIsocCompletion *) completion) );
        command->SetStartFrame(frameStart);
        command->SetNumFrames(numFrames);
        command->SetFrameList( (IOUSBIsocFrame *) frameList);
        command->SetStatus(kIOReturnBadArgument);
        command->SetUpdateFrequency(updateFrequency);

	err = _commandGate->runAction(DoLowLatencyIsocTransfer, command);
	
	// If we have a sync request, then we always return the command after the DoIOTransfer.  If it's an async request, we only return it if 
	// we get an immediate error
	//
	if ( command->GetIsSyncTransfer() || (!command->GetIsSyncTransfer() && (kIOReturnSuccess != err)) )
	{
		_freeUSBIsocCommandPool->returnCommand(command);
	}

    return err;
}

