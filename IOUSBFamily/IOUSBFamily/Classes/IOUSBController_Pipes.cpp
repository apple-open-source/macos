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
//    return UIMAbortEndpoint(address,
//                endpoint->number, endpoint->direction);
    return _commandGate->runAction(DoAbortEP, (void *)(UInt32) address,
			(void *)(UInt32) endpoint->number, (void *)(UInt32) endpoint->direction);
}

IOReturn IOUSBController::ResetPipe(USBDeviceAddress address,
                                    Endpoint * endpoint)
{
//    return UIMClearEndpointStall(address,
//                endpoint->number, endpoint->direction);
    return _commandGate->runAction(DoClearEPStall, (void *)(UInt32) address,
			(void *)(UInt32) endpoint->number, (void *)(UInt32) endpoint->direction);
}

IOReturn IOUSBController::ClearPipeStall(USBDeviceAddress address,
                                         Endpoint * endpoint)
{
//    return UIMClearEndpointStall(address,
//                endpoint->number, endpoint->direction);
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
	USBLog(7, "IOUSBController: DisjointCompletion, copying %x out of %x bytes to desc %p from buffer %p", (command->GetDblBufLength()-bufferSizeRemaining), command->GetDblBufLength(), command->GetOrigBuffer(), buf);
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
static IOReturn
CheckForDisjointDescriptor(IOUSBCommand *command, UInt16 maxPacketSize)
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
	return kIOReturnBadArgument;
	
    return Read(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, buffer->getLength());
}


OSMetaClassDefineReservedUsed(IOUSBController,  12);
IOReturn 
IOUSBController::Read(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
    IOReturn	 	err = kIOReturnSuccess;
    IOUSBCommand 	*command;
    int			i;

    USBLog(7, "%s[%p]::Read #3 - reqCount = %d", getName(), this, reqCount);
    // Validate its a inny pipe and that there is a buffer
    if ((endpoint->direction != kUSBIn) || !buffer || (buffer->getLength() < reqCount))
	return kIOReturnBadArgument;

    if ((endpoint->transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
	return kIOReturnBadArgument;							// timeouts only on bulk pipes
	
    // Validate the completion
    if (!completion)
	return kIOReturnNoCompletion;

    // Validate the command gate
    if (!_commandGate)
	return kIOReturnInternalError;

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
            USBLog(3,"%s[%p]::DeviceRequest Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }

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


    err = CheckForDisjointDescriptor(command, endpoint->maxPacketSize);
    if (!err)
	err = _commandGate->runAction(DoIOTransfer, command);
	
    if (err)
        _freeUSBCommandPool->returnCommand(command);

    return (err);
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
	return kIOReturnBadArgument;

    return Write(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, buffer->getLength());
}



OSMetaClassDefineReservedUsed(IOUSBController,  13);
IOReturn 
IOUSBController::Write(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletion *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
    IOReturn		 err = kIOReturnSuccess;
    IOUSBCommand 	*command;
    int			i;

    USBLog(7, "%s[%p]::Write #3 - reqCount = %d", getName(), this, reqCount);
    
    // Validate its a outty pipe and that we have a buffer
    if((endpoint->direction != kUSBOut) || !buffer || (buffer->getLength() < reqCount))
	return kIOReturnBadArgument;

    if ((endpoint->transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
	return kIOReturnBadArgument;							// timeouts only on bulk pipes
	
    // Validate the command gate
    if (!_commandGate)
	return kIOReturnInternalError;

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
            USBLog(3,"%s[%p]::DeviceRequest Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }

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

    err = CheckForDisjointDescriptor(command, endpoint->maxPacketSize);
    if (!err)
        err = _commandGate->runAction(DoIOTransfer, command);

    if (err)
        _freeUSBCommandPool->returnCommand(command);

    return (err);
}



IOReturn 
IOUSBController::IsocIO(IOMemoryDescriptor * buffer,
                               UInt64 frameStart,
                               UInt32 numFrames,
                               IOUSBIsocFrame *frameList,
                               USBDeviceAddress address,
                               Endpoint * endpoint,
                               IOUSBIsocCompletion * completion)
{
    IOReturn	 err = kIOReturnSuccess;
    IOUSBIsocCommand *command = (IOUSBIsocCommand *)_freeUSBIsocCommandPool->getCommand(false);
    
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseIsocCommandPool();
        
        command = (IOUSBIsocCommand *)_freeUSBIsocCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::DeviceRequest Could not get a IOUSBIsocCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }
    
    do
    {
        /* Validate the completion */
        if (completion == 0)
        {
            err = kIOReturnNoCompletion;
            break;
        }

        /* Set up direction */
        if (endpoint->direction == kUSBOut) {
            command->SetSelector(WRITE);
            command->SetDirection(kUSBOut);
	}
        else if (endpoint->direction == kUSBIn) {
            command->SetSelector(READ);
            command->SetDirection(kUSBIn);
	}
	else {
            err = kIOReturnBadArgument;
            break;
        }

        command->SetAddress(address);
        command->SetEndpoint(endpoint->number);
        command->SetBuffer(buffer);
        command->SetCompletion(*completion);
        command->SetStartFrame(frameStart);
        command->SetNumFrames(numFrames);
        command->SetFrameList(frameList);
        command->SetStatus(kIOReturnBadArgument);

        if (_commandGate == 0)
        {
            err = kIOReturnInternalError;
            break;
        }

        if ((err = _commandGate->runAction(DoIsocTransfer, command)))
            break;

        return(err);

    } while (0);

    // Free/give back the command 
    _freeUSBIsocCommandPool->returnCommand(command);

    return (err);
}

OSMetaClassDefineReservedUsed(IOUSBController,  15);
IOReturn 
IOUSBController::IsocIO(IOMemoryDescriptor * buffer,
                               UInt64 frameStart,
                               UInt32 numFrames,
                               IOUSBLowLatencyIsocFrame *frameList,
                               USBDeviceAddress address,
                               Endpoint * endpoint,
                               IOUSBLowLatencyIsocCompletion * completion,
                               UInt32 updateFrequency)
{
    IOReturn	 err = kIOReturnSuccess;
    IOUSBIsocCommand *command = (IOUSBIsocCommand *)_freeUSBIsocCommandPool->getCommand(false);
    
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseIsocCommandPool();
        
        command = (IOUSBIsocCommand *)_freeUSBIsocCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::DeviceRequest Could not get a IOUSBIsocCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }
    
    do
    {
        /* Validate the completion */
        if (completion == 0)
        {
            err = kIOReturnNoCompletion;
            break;
        }

        /* Set up direction */
        if (endpoint->direction == kUSBOut) {
            command->SetSelector(WRITE);
            command->SetDirection(kUSBOut);
	}
        else if (endpoint->direction == kUSBIn) {
            command->SetSelector(READ);
            command->SetDirection(kUSBIn);
	}
	else {
            err = kIOReturnBadArgument;
            break;
        }

        command->SetAddress(address);
        command->SetEndpoint(endpoint->number);
        command->SetBuffer(buffer);
        command->SetCompletion( * ((IOUSBIsocCompletion *) completion) );
        command->SetStartFrame(frameStart);
        command->SetNumFrames(numFrames);
        command->SetFrameList( (IOUSBIsocFrame *) frameList);
        command->SetStatus(kIOReturnBadArgument);
        command->SetUpdateFrequency(updateFrequency);

        if (_commandGate == 0)
        {
            err = kIOReturnInternalError;
            break;
        }

        if ((err = _commandGate->runAction(DoLowLatencyIsocTransfer, command)))
            break;

        return err;

    } while (0);

    // Free/give back the command 
    _freeUSBIsocCommandPool->returnCommand(command);

    return err;
}

