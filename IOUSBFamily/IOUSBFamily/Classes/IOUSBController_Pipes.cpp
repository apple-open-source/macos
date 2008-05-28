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

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOCommandPool.h>

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBLog.h>

#define super IOUSBBus
#define self this

#define _freeUSBCommandPool				_expansionData->freeUSBCommandPool
#define _freeUSBIsocCommandPool			_expansionData->freeUSBIsocCommandPool

#define CONTROLLER_PIPES_USE_KPRINTF 0

#if CONTROLLER_PIPES_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= 3) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

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
DisjointCompletion(IOUSBController *me, IOUSBCommand *command, IOReturn status, UInt32 bufferSizeRemaining)
{
    IOBufferMemoryDescriptor	*buf = NULL;
	IODMACommand				*dmaCommand = NULL;

    if (!me || !command)
    {
		USBError(1, "DisjointCompletion sanity check failed - me(%p) command (%p)", me, command);
		return;
    }
	
	buf = OSDynamicCast(IOBufferMemoryDescriptor, command->GetBuffer());
	dmaCommand = command->GetDMACommand();
	
	if (!dmaCommand || !buf)
	{
		USBLog(1, "%s[%p]::DisjointCompletion - no dmaCommand", me->getName(), me);
		return;
	}
	
	if (dmaCommand->getMemoryDescriptor())
	{
		if (dmaCommand->getMemoryDescriptor() != buf)
		{
			USBLog(1, "%s[%p]::DisjointCompletion - buf(%p) doesn't match getMemoryDescriptor(%p)", me->getName(), me, buf, dmaCommand->getMemoryDescriptor());
		}
		
		// need to complete the dma command
		USBLog(6, "%s[%p]::DisjointCompletion - clearing memory descriptor (%p) from dmaCommand (%p)", me->getName(), me, dmaCommand->getMemoryDescriptor(), dmaCommand);
		dmaCommand->clearMemoryDescriptor();
	}
	
    if (command->GetDirection() == kUSBIn)
    {
		USBLog(5, "%s[%p]::DisjointCompletion, copying %d out of %d bytes to desc %p from buffer %p", me->getName(), me, (int)(command->GetDblBufLength()-bufferSizeRemaining), (int)command->GetDblBufLength(), command->GetOrigBuffer(), buf);
		command->GetOrigBuffer()->writeBytes(0, buf->getBytesNoCopy(), (command->GetDblBufLength()-bufferSizeRemaining));
    }
	
    buf->complete();
	buf->release();								// done with this buffer
	command->SetBuffer(NULL);
	
    // now call through to the original completion routine
    IOUSBCompletion completion = command->GetDisjointCompletion();
    USBLog(5, "%s[%p]::DisjointCompletion calling through to %p - status (%p)!", me->getName(), me, completion.action, (void*)status);
    if (completion.action)  
		(*completion.action)(completion.target, completion.parameter, status, bufferSizeRemaining);
}



// since this is a new method, I am not making it a member function, so that I don't
// have to change the class definition
OSMetaClassDefineReservedUsed(IOUSBController,  17);
IOReturn
IOUSBController::CheckForDisjointDescriptor(IOUSBCommand *command, UInt16 maxPacketSize)
{
    IOMemoryDescriptor			*buf = command->GetBuffer();
    IOBufferMemoryDescriptor	*newBuf = NULL;
    IOByteCount					length = command->GetReqCount();
	IODMACommand				*dmaCommand = command->GetDMACommand();
    IOByteCount					segLength = 0;
    IOByteCount					offset = 0;
    IOReturn					err;
	UInt64						offset64;
	IODMACommand::Segment64		segment64;
	UInt32						numSegments;
	
    // Zero length buffers are valid, but they are surely not disjoint, so just return success.  
    //
    if ( length == 0 )
        return kIOReturnSuccess;
	
	if (!dmaCommand)
	{
		USBLog(1, "%s[%p]::CheckForDisjointDescriptor - no dmaCommand", getName(), this);
		return kIOReturnBadArgument;
	}
	
	if (dmaCommand->getMemoryDescriptor() != buf)
	{
		USBLog(1, "%s[%p]::CheckForDisjointDescriptor - mismatched memory descriptor (%p) and dmaCommand memory descriptor (%p)", getName(), this, buf, dmaCommand->getMemoryDescriptor());
		return kIOReturnBadArgument;
	}
	
    while (length)
    {
		offset64 = offset;
		numSegments = 1;
		
		err = dmaCommand->gen64IOVMSegments(&offset64, &segment64, &numSegments);
        if (err || (numSegments != 1))
        {
            USBLog(1, "%s[%p]::CheckForDisjointDescriptor - err (%p) trying to generate segments at offset (%Ld), length (%d), segLength (%d), total length (%d), buf (%p), numSegments (%d)", getName(), this, (void*)err, offset64, (int)length, (int)segLength, (int)command->GetReqCount(), buf, (int)numSegments);
            return kIOReturnBadArgument;
        }

		
		// 3036056 since length might be less than the length of the descriptor, we are OK if the physical
		// segment is longer than we need
        if (segment64.fLength >= length)
            return kIOReturnSuccess;		// this is the last segment, so we are OK
		
		// since length is a 32 bit quantity, then we know from the above statement that if we are here we are 32 bit only
		segLength = (IOByteCount)segment64.fLength;

        // so the segment is less than the rest of the length - we need to check against maxPacketSize
        if (segLength % maxPacketSize)
        {
            // this is the error case. I need to copy the descriptor to a new descriptor and remember that I did it
            USBLog(5, "%s[%p]::CheckForDisjointDescriptor - found a disjoint segment of length (%d) MPS (%d)", getName(), this, (int)segLength, maxPacketSize);
			length = command->GetReqCount();		// we will not return to the while loop, so don't worry about changing the value of length
													// allocate a new descriptor which is the same total length as the old one
			newBuf = IOBufferMemoryDescriptor::withOptions((command->GetDirection() == kUSBIn) ? kIODirectionIn : kIODirectionOut, length);
			if (!newBuf)
			{
				USBLog(1, "%s[%p]::CheckForDisjointDescriptor - could not allocate new buffer", getName(), this);
				return kIOReturnNoMemory;
			}
			USBLog(5, "%s[%p]::CheckForDisjointDescriptor, obtained buffer %p of length %d", getName(), this, newBuf, (int)length);
			
			// first close out (and complete) the original dma command descriptor
			USBLog(6, "%s[%p]::CheckForDisjointDescriptor, clearing memDec (%p) from dmaCommand (%p)", getName(), this, dmaCommand->getMemoryDescriptor(), dmaCommand);
			dmaCommand->clearMemoryDescriptor();
			
			// copy the bytes to the buffer if necessary
			if (command->GetDirection() == kUSBOut)
			{
				USBLog(7, "%s[%p]::CheckForDisjointDescriptor, copying %d bytes from desc %p to buffer %p", getName(), this, (int)length, buf, newBuf->getBytesNoCopy());
				if (buf->readBytes(0, newBuf->getBytesNoCopy(), length) != length)
				{
					USBLog(1, "%s[%p]::CheckForDisjointDescriptor - bad copy on a write", getName(), this);
					newBuf->release();
					return kIOReturnNoMemory;
				}
			}
			err = newBuf->prepare();
			if (err)
			{
				USBLog(1, "%s[%p]::CheckForDisjointDescriptor - err 0x%x in prepare", getName(), this, err);
				newBuf->release();
				return err;
			}
			command->SetOrigBuffer(command->GetBuffer());
			command->SetDisjointCompletion(command->GetClientCompletion());
			USBLog(5, "%s[%p]::CheckForDisjointDescriptor - changing buffer from (%p) to (%p) and putting new buffer in dmaCommand (%p)", getName(), this, command->GetBuffer(), newBuf, dmaCommand);
			command->SetBuffer(newBuf);
			dmaCommand->setMemoryDescriptor(newBuf);
			
			IOUSBCompletion completion;
			completion.target = this;
			completion.action = (IOUSBCompletionAction)DisjointCompletion;
			completion.parameter = command;
			command->SetClientCompletion(completion);
			
			command->SetDblBufLength(length);			// for the IOFree - the other buffer may change size
            return kIOReturnSuccess;
		}
        length -= segLength;		// adjust our master length pointer
		offset += segLength;
	}
    USBLog(5, "%s[%p]::CheckForDisjointDescriptor - returning kIOReturnBadArgument(0x%x)", getName(), this, kIOReturnBadArgument);
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
    IOReturn			err = kIOReturnSuccess;
    IOUSBCommand *		command = NULL;
	IODMACommand *		dmaCommand = NULL;
    IOUSBCompletion 	nullCompletion;
    int					i;

    USBLog(7, "%s[%p]::Read - reqCount = %ld", getName(), this, reqCount);
    // Validate its a inny pipe and that there is a buffer
    if ((endpoint->direction != kUSBIn) || !buffer || (buffer->getLength() < reqCount))
    {
        USBLog(2, "%s[%p]::Read - direction is not kUSBIn (%d), No Buffer, or buffer length < reqCount (%ld < %ld). Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->direction,  buffer->getLength(), reqCount, kIOReturnBadArgument);
		return kIOReturnBadArgument;
    }
    
    if ((endpoint->transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(2, "%s[%p]::Read - Pipe is NOT kUSBBulk (%d) AND specified a timeout (%ld, %ld).  Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->transferType, noDataTimeout, completionTimeout, kIOReturnBadArgument);
		return kIOReturnBadArgument; // timeouts only on bulk pipes
    }
    
    // Validate the completion
    if (!completion)
    {
        USBLog(2, "%s[%p]::Read - No Completion routine.  Returning kIOReturnNoCompletion(0x%x)", getName(), this, kIOReturnNoCompletion);
		return kIOReturnNoCompletion;
    }
    
    // Validate the command gate
    if (!_commandGate)
    {
        USBLog(1, "%s[%p]::Read - Could not get _commandGate.  Returning kIOReturnInternalError(0x%x)", getName(), this, kIOReturnInternalError);
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
            USBLog(1,"%s[%p]::Read Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }
	
	if (reqCount)
	{
		IOMemoryDescriptor		*memDesc;

		dmaCommand = command->GetDMACommand();
		
		if (!dmaCommand)
		{
			USBLog(1, "%s[%p]::Read - no DMA COMMAND", getName(), this);
            return kIOReturnNoResources;
		}
		memDesc = (IOMemoryDescriptor*)dmaCommand->getMemoryDescriptor();
		if (memDesc)
		{
			USBLog(1, "%s[%p]::Read - dmaCommand (%p) already contains memory descriptor (%p) - clearing", getName(), this, dmaCommand, memDesc);
			dmaCommand->clearMemoryDescriptor();
			// memDesc->complete();		// should i do this?
		}
		// buffer->retain();			// should i do this?
		// err = buffer->prepare();		// should i do this?
		if (0) // if (err)
		{
			USBLog(1, "%s[%p]::Read - err (%p) preparing memory descriptor (%p)", getName(), this, (void*)err, buffer);
            return err;
		}
		USBLog(7, "%s[%p]::Read - setting memory descriptor (%p) into dmaCommand (%p)", getName(), this, buffer, dmaCommand);
		err = dmaCommand->setMemoryDescriptor(buffer);
		if (err)
		{
			USBLog(1, "%s[%p]::Read - err(%p) attempting to set the memory descriptor to the dmaCommand", getName(), this, (void*)err);
			// buffer->complete();		// should i do this?
			return err;
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
		bool	isSyncTransfer = command->GetIsSyncTransfer();

		err = _commandGate->runAction(DoIOTransfer, command);
		
		// If we have a sync request, then we always return the command after the DoIOTransfer.  If it's an async request, we only return it if 
		// we get an immediate error
		//
		if ( isSyncTransfer || (kIOReturnSuccess != err) )
		{
			IODMACommand		*dmaCommand = command->GetDMACommand();
			IOMemoryDescriptor	*memDesc = dmaCommand ? (IOMemoryDescriptor	*)dmaCommand->getMemoryDescriptor() : NULL;
			
			nullCompletion = command->GetDisjointCompletion();
			if (nullCompletion.action)
			{
				USBLog(2, "%s[%p]::Read - SYNC xfer or immediate error with Disjoint Completion", getName(), this);
			}
			if (memDesc)
			{
				USBLog(7, "%s[%p]::Read - sync xfer or err return - clearing memory descriptor (%p) from dmaCommand (%p)", getName(), this, memDesc, dmaCommand);
				dmaCommand->clearMemoryDescriptor();
				// memDesc->complete();					should i do this?
				// memDesc->release();					should i do this?
			}
			_freeUSBCommandPool->returnCommand(command);
		}
	}
	else
	{
		IODMACommand		*dmaCommand = command->GetDMACommand();
		IOMemoryDescriptor	*memDesc = dmaCommand ? (IOMemoryDescriptor	*)dmaCommand->getMemoryDescriptor() : NULL;
		// CheckForDisjoint returned an error, so free up the comand
		//
		if (memDesc)
		{
			USBLog(7, "%s[%p]::Read - CheckForDisjoint error (%p) - clearing memory descriptor (%p) from dmaCommand (%p)", getName(), this, (void*)err, memDesc, dmaCommand);
			dmaCommand->clearMemoryDescriptor();
			// memDesc->complete();					should i do this?
			// memDesc->release();					should i do this?
		}
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
    IOReturn				err = kIOReturnSuccess;
    IOUSBCommand *			command = NULL;
	IODMACommand *			dmaCommand = NULL;
    IOUSBCompletion			nullCompletion;
    int						i;

    USBLog(7, "%s[%p]::Write - reqCount = %ld", getName(), this, reqCount);
    
    // Validate its a outty pipe and that we have a buffer
    if((endpoint->direction != kUSBOut) || !buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "%s[%p]::Write - direction is not kUSBOut (%d), No Buffer, or buffer length < reqCount (%ld < %ld). Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->direction,  buffer->getLength(), reqCount, kIOReturnBadArgument);
		return kIOReturnBadArgument;
    }

    if ((endpoint->transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "%s[%p]::Write - Pipe is NOT kUSBBulk (%d) AND specified a timeout (%ld, %ld).  Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->transferType, noDataTimeout, completionTimeout, kIOReturnBadArgument);
		return kIOReturnBadArgument;							// timeouts only on bulk pipes
    }
	
    // Validate the command gate
    if (!_commandGate)
    {
        USBLog(5, "%s[%p]::Write - Could not get _commandGate.  Returning kIOReturnInternalError(0x%x)", getName(), this, kIOReturnInternalError);
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
            USBLog(3,"%s[%p]::Write Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }

	if (reqCount)
	{
		IOMemoryDescriptor	*memDesc;
		
		dmaCommand = command->GetDMACommand();
		
		if (!dmaCommand)
		{
			USBLog(1, "%s[%p]::Write - no DMA COMMAND", getName(), this);
            return kIOReturnNoResources;
		}
		memDesc = (IOMemoryDescriptor *)dmaCommand->getMemoryDescriptor();
		if (memDesc)
		{
			USBLog(1, "%s[%p]::Write - dmaCommand (%p) already contains memory descriptor (%p) - clearing", getName(), this, dmaCommand, memDesc);
			dmaCommand->clearMemoryDescriptor();
			// memDesc->complete();					should i do this?
			// memDesc->release();					should i do this?
		}
		// buffer->retain();			// should i do this?
		// err = buffer->prepare();		// should i do this?
		if (0) // if (err)
		{
			USBLog(1, "%s[%p]::Write - err (%p) preparing buffer (%p)", getName(), this, (void*)err, buffer);
            return err;
		}
		USBLog(7, "%s[%p]::Write - setting memory descriptor (%p) into dmaCommand (%p)", getName(), this, buffer, dmaCommand);
		err = dmaCommand->setMemoryDescriptor(buffer);
		if (err)
		{
			USBLog(1, "%s[%p]::Write - err(%p) attempting to set the memory descriptor to the dmaCommand", getName(), this, (void*)err);
			// buffer->complete();		// should i do this?
			return err;
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
		bool	isSyncTransfer = command->GetIsSyncTransfer();
		
		err = _commandGate->runAction(DoIOTransfer, command);
		
		// If we have a sync request, then we always return the command after the DoIOTransfer.  If it's an async request, we only return it if 
		// we get an immediate error
		//
		if ( isSyncTransfer || (kIOReturnSuccess != err) )
		{
			IODMACommand		*dmaCommand = command->GetDMACommand();
			IOMemoryDescriptor	*memDesc = dmaCommand ? (IOMemoryDescriptor	*)dmaCommand->getMemoryDescriptor() : NULL;

			nullCompletion = command->GetDisjointCompletion();
			if (nullCompletion.action)
			{
				USBLog(2, "%s[%p]::Write - SYNC xfer or immediate error with Disjoint Completion", getName(), this);
			}
			if (memDesc)
			{
				USBLog(7, "%s[%p]::Write - sync xfer or err return - clearing memory descriptor (%p) from dmaCommand (%p)", getName(), this, memDesc, dmaCommand);
				dmaCommand->clearMemoryDescriptor();
				// memDesc->complete();					should i do this?
				// memDesc->release();					should i do this?
			}
			_freeUSBCommandPool->returnCommand(command);
		}
	}
	else
	{
		IODMACommand		*dmaCommand = command->GetDMACommand();
		IOMemoryDescriptor	*memDesc = dmaCommand ? (IOMemoryDescriptor	*)dmaCommand->getMemoryDescriptor() : NULL;
		// CheckForDisjoint returned an error, so free up the comand
		//
		if (memDesc)
		{
			USBLog(7, "%s[%p]::Write - CheckForDisjoint error (%p) - clearing memory descriptor (%p) from dmaCommand (%p)", getName(), this, (void*)err, memDesc, dmaCommand);
			dmaCommand->clearMemoryDescriptor();
			// memDesc->complete();					should i do this?
			// memDesc->release();					should i do this?
		}
		_freeUSBCommandPool->returnCommand(command);
	}
	
    return err;
}



IOReturn 
IOUSBController::IsocIO(IOMemoryDescriptor *				buffer,
						UInt64								frameStart,
						UInt32								numFrames,
						IOUSBIsocFrame *					frameList,
						USBDeviceAddress					address,
						Endpoint *							endpoint,
						IOUSBIsocCompletion *				completion)
{
    IOReturn				err = kIOReturnSuccess;
    IOUSBIsocCommand *		command;
    bool					crossEndianRequest = false;
	IODMACommand *			dmaCommand = NULL;
	bool					syncTransfer = false;
	
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
    
	dmaCommand = command->GetDMACommand();
	if (!dmaCommand)
	{
		USBLog(1, "%s[%p]::IsocIO no IODMACommand in the IOUSBCommand", getName(), this);
		return kIOReturnNoResources;
	}

	//USBLog(1, "%s[%p]::IsocIO preparing memory descriptor (%p)", getName(), this, buffer);
	// buffer->retain();					// should i do this?
	// err = buffer->prepare();				// should i do this?
	if (0) // if (err)
	{
		USBLog(3,"%s[%p]::IsocIO err (%p) trying to prepare buffer (%p)", getName(), this, (void*)err, buffer);
		_freeUSBIsocCommandPool->returnCommand(command);
		buffer->release();
		return err;
	}
	USBLog(7, "%s[%p]::IsocIO - putting buffer (%p) into dmaCommand (%p) which has getMemoryDescriptor (%p)", getName(), this, buffer, command->GetDMACommand(), command->GetDMACommand()->getMemoryDescriptor());
	err = dmaCommand->setMemoryDescriptor(buffer);								// this automatically calls prepare()
	if (err)
	{
		USBLog(1, "%s[%p]::IsocIO - dmaCommand[%p]->setMemoryDescriptor(%p) failed with status (%p)", getName(), this, command->GetDMACommand(), buffer, (void*)err);
		// buffer->complete();				// should i do this?
		//buffer->release();				// should i do this?
		_freeUSBIsocCommandPool->returnCommand(command);
		return err;
	}
	
	// If the high order bit of the endpoint transfer type is set, then this means it's a request from an Rosetta client
	command->SetRosettaClient(crossEndianRequest);
	
	// Set up a flag indicating that we have a synchronous request in this command
	//
    if ( (UInt32)completion->action == (UInt32) &IOUSBSyncIsoCompletion )
		syncTransfer = true;
		
	command->SetIsSyncTransfer(syncTransfer);
	
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
	command->SetLowLatency(false);

	err = _commandGate->runAction(DoIsocTransfer, command);

    return err;
}



OSMetaClassDefineReservedUsed(IOUSBController,  15);
IOReturn 
IOUSBController::IsocIO(IOMemoryDescriptor *			buffer,
						UInt64							frameStart,
						UInt32							numFrames,
						IOUSBLowLatencyIsocFrame *		frameList,
						USBDeviceAddress				address,
						Endpoint *						endpoint,
						IOUSBLowLatencyIsocCompletion * completion,
						UInt32							updateFrequency)
{
    IOReturn				err = kIOReturnSuccess;
    IOUSBIsocCommand *		command = NULL;
    bool					crossEndianRequest = false;
	IODMACommand *			dmaCommand = NULL;
	bool					syncTransfer = false;
    
	// Validate the completion
	//
	USBLog(7, "%s[%p]::IsocIO(LL)", getName(), this);
	if (completion == 0)
	{
		USBLog(1, "%s[%p]::IsocIO(LL) - No completion.  Returning kIOReturnNoCompletion(0x%x)", getName(), this, kIOReturnNoCompletion);
		return kIOReturnNoCompletion;
	}
	
	// Validate the commandGate
	//
	if (_commandGate == 0)
	{
		USBLog(1, "%s[%p]::IsocIO(LL) - Could not get _commandGate.  Returning kIOReturnInternalError(0x%x)", getName(), this, kIOReturnInternalError);
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
		USBLog(1, "%s[%p]::IsocIO(LL) - Direction is not kUSBOut or kUSBIn (%d).  Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->direction, kIOReturnBadArgument);
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
            USBLog(1, "%s[%p]::IsocIO(LL) Could not get a IOUSBIsocCommand", getName(), this);
            return kIOReturnNoResources;
        }
    }

	dmaCommand = command->GetDMACommand();
	if (!dmaCommand)
	{
		USBLog(1, "%s[%p]::IsocIO(LL) no IODMACommand in the IOUSBCommand", getName(), this);
		return kIOReturnNoResources;
	}
	
	//USBLog(1, "%s[%p]::IsocIO(LL) preparing memory descriptor (%p)", getName(), this, buffer);
	// buffer->retain();						// should i do this?
	// err = buffer->prepare();					// should i do this?
	if (0) // if (err)
	{
		USBLog(3,"%s[%p]::IsocIO(LL) err (%p) trying to prepare buffer (%p)", getName(), this, (void*)err, buffer);
		_freeUSBIsocCommandPool->returnCommand(command);
		buffer->release();
		return err;
	}
	USBLog(7, "%s[%p]::IsocIO(LL) - putting buffer %p into dmaCommand %p which has getMemoryDescriptor %p", getName(), this, buffer, command->GetDMACommand(), command->GetDMACommand()->getMemoryDescriptor());
	err = dmaCommand->setMemoryDescriptor(buffer);								// this automatically calls prepare()
	if (err)
	{
		USBLog(1, "%s[%p]::IsocIO(LL) - dmaCommand[%p]->setMemoryDescriptor(%p) failed with status (%p)", getName(), this, command->GetDMACommand(), buffer, (void*)err);
		// buffer->complete();					// should i do this?
		// buffer->release();					// should i do this?
		_freeUSBIsocCommandPool->returnCommand(command);
		return err;
	}
	
	// If the high order bit of the endpoint transfer type is set, then this means it's a request from an Rosetta client
	command->SetRosettaClient(crossEndianRequest);
	
	// Set up a flag indicating that we have a synchronous request in this command
	//
    if ( (UInt32)completion->action == (UInt32) &IOUSBSyncIsoCompletion )
		syncTransfer = true;
		
	command->SetIsSyncTransfer(syncTransfer);
	
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
	command->SetCompletion( * ((IOUSBIsocCompletion *) completion) );
	command->SetStartFrame(frameStart);
	command->SetNumFrames(numFrames);
	command->SetFrameList( (IOUSBIsocFrame *) frameList);
	command->SetStatus(kIOReturnBadArgument);
	command->SetUpdateFrequency(updateFrequency);
	command->SetLowLatency(true);

	err = _commandGate->runAction(DoIsocTransfer, command);
	
    return err;
}

