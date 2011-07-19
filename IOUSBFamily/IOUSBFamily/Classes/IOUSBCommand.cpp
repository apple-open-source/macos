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


#include <libkern/version.h>

#include <libkern/OSDebug.h>
#include <IOKit/IOLib.h>
#include <IOKit/usb/IOUSBCommand.h>
#include <IOKit/usb/IOUSBLog.h>

OSDefineMetaClassAndStructors(IOUSBCommand, IOCommand)

OSDefineMetaClassAndStructors(IOUSBIsocCommand, IOCommand)

#define super	IOCommand	// same for both

IOUSBCommand*
IOUSBCommand::NewCommand()
{
    IOUSBCommand *me = new IOUSBCommand;
    
    if (me && !me->init())
    {
		me->release();
		me = NULL;
    }
    return me;
}



bool 
IOUSBCommand::init()
{
    if (!super::init())
        return false;
    // allocate our expansion data
    if (!_expansionData)
    {
		_expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
		if (!_expansionData)
			return false;
		bzero(_expansionData, sizeof(ExpansionData));
    }
    return true;
}


void 
IOUSBCommand::free()
{
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
    {
		IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }
	
    super::free();
}

// accessor methods
void 
IOUSBCommand::SetSelector(usbCommand sel) 
{
    _selector = sel;
}

void 
IOUSBCommand::SetRequest(IOUSBDeviceRequestPtr req) 
{
    _request = req;
}

void 
IOUSBCommand::SetAddress(USBDeviceAddress addr) 
{
    _address = addr;
}

void 
IOUSBCommand::SetEndpoint(UInt8 ep) 
{
    _endpoint = ep;
}

void 
IOUSBCommand::SetDirection(UInt8 dir) 
{
    _direction = dir;
}

void 
IOUSBCommand::SetType(UInt8 type) 
{
    _type = type;
}

void 
IOUSBCommand::SetBufferRounding(bool br) 
{ 
    _bufferRounding = br;
}

void 
IOUSBCommand::SetBuffer(IOMemoryDescriptor *buf) 
{	
    _buffer = buf;
}

void 
IOUSBCommand::SetUSLCompletion(IOUSBCompletion completion) 
{
    _uslCompletion = completion;
}

void 
IOUSBCommand::SetClientCompletion(IOUSBCompletion completion) 
{
    _clientCompletion = completion;
}

void 
IOUSBCommand::SetDataRemaining(UInt32 dr) 
{
	if (_expansionData->_masterUSBCommand)
		_expansionData->_masterUSBCommand->_dataRemaining = dr;
	else
		_dataRemaining = dr;
}

void 
IOUSBCommand::SetStage(UInt8 stage) 
{
	if (_expansionData->_masterUSBCommand)
		_expansionData->_masterUSBCommand->_stage = stage;
	else
		_stage = stage;
}

void 
IOUSBCommand::SetStatus(IOReturn stat) 
{
	if (_expansionData->_masterUSBCommand)
		_expansionData->_masterUSBCommand->_status = stat;
	else
		_status = stat;
}

void 
IOUSBCommand::SetOrigBuffer(IOMemoryDescriptor *buf) 
{
    _origBuffer = buf;
}

void 
IOUSBCommand::SetDisjointCompletion(IOUSBCompletion completion) 
{
    _disjointCompletion = completion;
}

void 
IOUSBCommand::SetDblBufLength(IOByteCount len) 
{
    _dblBufLength = len;
}

void 
IOUSBCommand::SetNoDataTimeout(UInt32 to) 
{
    _noDataTimeout = to;
}

void 
IOUSBCommand::SetCompletionTimeout(UInt32 to) 
{
    _completionTimeout = to;
}

void 
IOUSBCommand::SetUIMScratch(UInt32 index, UInt32 value) 
{ 
    if (index < 10)
		if (_expansionData->_masterUSBCommand)
			_expansionData->_masterUSBCommand->_UIMScratch[index] = value;
		else
			_UIMScratch[index] = value;
}

void 
IOUSBCommand::SetReqCount(IOByteCount reqCount) 
{
    _expansionData->_reqCount = reqCount;
}

void 
IOUSBCommand::SetRequestMemoryDescriptor(IOMemoryDescriptor *requestMemoryDescriptor) 
{
    _expansionData->_requestMemoryDescriptor = requestMemoryDescriptor;
}

void 
IOUSBCommand::SetBufferMemoryDescriptor(IOMemoryDescriptor *bufferMemoryDescriptor) 
{
    _expansionData->_bufferMemoryDescriptor = bufferMemoryDescriptor;
}

void
IOUSBCommand::SetMultiTransferTransaction(bool multiTDTransaction)
{
    _expansionData->_multiTransferTransaction = multiTDTransaction;
}


void
IOUSBCommand::SetFinalTransferInTransaction(bool finalTDinTransaction)
{
    _expansionData->_finalTransferInTransaction = finalTDinTransaction;
}


void
IOUSBCommand::SetUseTimeStamp(bool useTimeStamp)
{
    _expansionData->_useTimeStamp = useTimeStamp;
}


void
IOUSBCommand::SetTimeStamp(AbsoluteTime timeStamp)
{
    _expansionData->_timeStamp = timeStamp;
}

void
IOUSBCommand::SetIsSyncTransfer(bool isSync)
{
    _expansionData->_isSyncTransfer = isSync;
}


void					
IOUSBCommand::SetBufferUSBCommand(IOUSBCommand *bufferUSBCommand)
{
	if (!bufferUSBCommand && (_expansionData->_bufferUSBCommand))
		_expansionData->_bufferUSBCommand->_expansionData->_masterUSBCommand = NULL;
	
	_expansionData->_bufferUSBCommand = bufferUSBCommand;
	
	if (bufferUSBCommand)
		bufferUSBCommand->_expansionData->_masterUSBCommand = this; 
}


usbCommand 
IOUSBCommand::GetSelector(void) 
{
	// This one can be different for the two command (master and buffer)
	return _selector;
}

IOUSBDeviceRequestPtr 
IOUSBCommand::GetRequest(void) 
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_request : _request;
}

USBDeviceAddress 
IOUSBCommand::GetAddress(void) 
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_address : _address;
}

UInt8 
IOUSBCommand::GetEndpoint(void) 
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_endpoint : _endpoint;
}

UInt8 
IOUSBCommand::GetDirection(void) 
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_direction : _direction;
}

UInt8 
IOUSBCommand::GetType(void) 
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_type : _type;
}

bool 
IOUSBCommand::GetBufferRounding(void) 
{
    return _bufferRounding;
}

IOMemoryDescriptor* 
IOUSBCommand::GetBuffer(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_buffer : _buffer;
}

IOUSBCompletion 
IOUSBCommand::GetUSLCompletion(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_uslCompletion : _uslCompletion;
}

IOUSBCompletion 
IOUSBCommand::GetClientCompletion(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_clientCompletion : _clientCompletion;
}

UInt32 
IOUSBCommand::GetDataRemaining(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_dataRemaining : _dataRemaining;
}

UInt8 
IOUSBCommand::GetStage(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_stage : _stage;
}

IOReturn 
IOUSBCommand::GetStatus(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_status : _status;
}

IOMemoryDescriptor * 
IOUSBCommand::GetOrigBuffer(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_origBuffer : _origBuffer;
}

IOUSBCompletion 
IOUSBCommand::GetDisjointCompletion(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_disjointCompletion : _disjointCompletion;
}

IOByteCount 
IOUSBCommand::GetDblBufLength(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_dblBufLength : _dblBufLength;
}

UInt32 
IOUSBCommand::GetNoDataTimeout(void) 
{ 
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_noDataTimeout : _noDataTimeout;
}

UInt32 
IOUSBCommand::GetCompletionTimeout(void) 
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_completionTimeout : _completionTimeout;
}

UInt32 IOUSBCommand::GetUIMScratch(UInt32 index) 
{ 
	if (index < 10)
		return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_UIMScratch[index] : _UIMScratch[index];
	else
		return 0;
}

IOByteCount 
IOUSBCommand::GetReqCount(void) 
{ 
    return _expansionData->_reqCount;
}

IOMemoryDescriptor*
IOUSBCommand::GetRequestMemoryDescriptor(void)
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_expansionData->_requestMemoryDescriptor : _expansionData->_requestMemoryDescriptor;
}

// this one is different in that the buffer command (the child) will use this for its own memory descriptor
IOMemoryDescriptor*
IOUSBCommand::GetBufferMemoryDescriptor(void)
{
    return _expansionData->_bufferMemoryDescriptor;
}


bool 
IOUSBCommand::GetMultiTransferTransaction(void)
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_expansionData->_multiTransferTransaction : _expansionData->_multiTransferTransaction;
}


bool 
IOUSBCommand::GetFinalTransferInTransaction(void)
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_expansionData->_finalTransferInTransaction : _expansionData->_finalTransferInTransaction;
}

bool 
IOUSBCommand::GetUseTimeStamp(void)
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_expansionData->_useTimeStamp : _expansionData->_useTimeStamp;
}

AbsoluteTime 
IOUSBCommand::GetTimeStamp(void)
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_expansionData->_timeStamp : _expansionData->_timeStamp;
}

bool 
IOUSBCommand::GetIsSyncTransfer(void)
{
	return _expansionData->_masterUSBCommand ? _expansionData->_masterUSBCommand->_expansionData->_isSyncTransfer : _expansionData->_isSyncTransfer;
}


IOUSBIsocCommand*
IOUSBIsocCommand::NewCommand()
{
    IOUSBIsocCommand *me = new IOUSBIsocCommand;
    
    if (me && !me->init())
    {
		me->release();
		me = NULL;
    }
    return me;
}

bool 
IOUSBIsocCommand::init()
{
    if (!super::init())
        return false;
    // allocate our expansion data
    if (!_expansionData)
    {
		_expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
		if (!_expansionData)
			return false;
		bzero(_expansionData, sizeof(ExpansionData));
    }
    return true;
}


void 
IOUSBIsocCommand::free()
{
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
    {
		IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }
    super::free();
}

//================================================================================================
//
//   IOUSBCommandPool
//
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBCommandPool, IOCommandPool);

IOCommandPool *
IOUSBCommandPool::withWorkLoop(IOWorkLoop * inWorkLoop)
{
	IOCommandPool * me = new IOUSBCommandPool;
    
	if (me && !me->initWithWorkLoop(inWorkLoop)) {
		me->release();
		return 0;
	}
	
	return me;
}

IOReturn
IOUSBCommandPool::gatedGetCommand(IOCommand ** command, bool blockForCommand)
{
	IOReturn ret;
	
	ret = IOCommandPool::gatedGetCommand(command, blockForCommand);

	return ret;
}

IOReturn
IOUSBCommandPool::gatedReturnCommand(IOCommand * command)
{
	IOUSBCommand		*usbCommand		= OSDynamicCast(IOUSBCommand, command);					// only one of these should be non-null
	IOUSBIsocCommand	*isocCommand	= OSDynamicCast(IOUSBIsocCommand, command);

	USBLog(7,"IOUSBCommandPool[%p]::gatedReturnCommand %p", this, command);
	if (!command)
	{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
		panic("IOUSBCommandPool::gatedReturnCommand( NULL )");
#endif
		return kIOReturnBadArgument;
	}
	
	if (command->fCommandChain.next &&
	    (&command->fCommandChain != command->fCommandChain.next || 
		 &command->fCommandChain != command->fCommandChain.prev))
	{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
		kprintf("WARNING: gatedReturnCommand(%p) already on queue [next=%p prev=%p]\n", command, command->fCommandChain.next, command->fCommandChain.prev);
		panic("IOUSBCommandPool::gatedReturnCommand already on queue");
#endif
		char*		bt[8];
		
		OSBacktrace((void**)bt, 8);
		
		USBError(1,"IOUSBCommandPool[%p]::gatedReturnCommand  command already in queue, not putting it back into the queue, bt: [%p][%p][%p][%p][%p][%p][%p][%p]", this, bt[0], bt[1], bt[2], bt[3], bt[4], bt[5], bt[6], bt[7]);
		return kIOReturnBadArgument;
	}
	
	if (usbCommand)
	{
		IODMACommand *dmaCommand = usbCommand->GetDMACommand();
		if (dmaCommand)
		{
			if (dmaCommand->getMemoryDescriptor())
			{
				USBError(1, "IOUSBCommandPool::gatedReturnCommand - command (%p) still has dmaCommand(%p) with an active memory descriptor(%p)", usbCommand, dmaCommand, dmaCommand->getMemoryDescriptor());
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
				panic("IOUSBCommandPool::gatedReturnCommand -dmaCommand still has active IOMD");
#endif
			}
		}
		else
		{
			USBError(1,"IOUSBCommandPool[%p]::gatedReturnCommand - missing dmaCommand in IOUSBCommand", this);
		}
	}
	
	if (isocCommand)
	{
		IODMACommand *dmaCommand = isocCommand->GetDMACommand();
		if (dmaCommand)
		{
			if (dmaCommand->getMemoryDescriptor())
			{
				USBError(1, "IOUSBCommandPool::gatedReturnCommand - isocCommand (%p) still has dmaCommand(%p) with an active memory descriptor(%p)", isocCommand, dmaCommand, dmaCommand->getMemoryDescriptor());
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
				panic("IOUSBCommandPool::gatedReturnCommand - dmaCommand still has active IOMD (isoc)");
#endif
			}
		}
		else
		{
			USBError(1,"IOUSBCommandPool[%p]::gatedReturnCommand - missing dmaCommand in IOUSBIsocCommand", this);
		}
	}
	return IOCommandPool::gatedReturnCommand(command);
}



