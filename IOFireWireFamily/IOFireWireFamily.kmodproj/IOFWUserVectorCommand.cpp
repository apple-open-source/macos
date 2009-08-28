/*
 * Copyright (c) 1998-2007 Apple Computer, Inc. All rights reserved.
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

#include "IOFWUserVectorCommand.h"

#import "IOFWUserCommand.h"

#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOFireWireNub.h>

OSDefineMetaClassAndStructors( IOFWUserVectorCommand, OSObject );

// withUserClient
//
//

IOFWUserVectorCommand *
IOFWUserVectorCommand::withUserClient( IOFireWireUserClient * inUserClient )
{
	IOFWUserVectorCommand*	result	= NULL ;
	
	result = OSTypeAlloc( IOFWUserVectorCommand );
	if( result && !result->initWithUserClient( inUserClient ) )
	{
		result->release() ;
		result = NULL ;
	}

	return result ;
}

// init
//
//

bool 
IOFWUserVectorCommand::initWithUserClient( IOFireWireUserClient * inUserClient )
{
	bool success = true;
	
	if( !OSObject::init() )
		success = false;
	
	if( success )
	{
		fUserClient = inUserClient;
		fControl = fUserClient->getOwner()->getController();
	}
	
	return success;
}

// free
//
//

void 
IOFWUserVectorCommand::free( void )
{
	if( fSubmitDesc )
	{
		fSubmitDesc->complete();
		fSubmitDesc->release();
		fSubmitDesc = NULL;
	}

	if( fResultDesc )
	{
		fResultDesc->complete();
		fResultDesc->release();
		fResultDesc = NULL;
	}
		
	OSObject::free();
}

#pragma mark -
/////////////////////////////////////////////////////////////////////////////////

// submit
//
//

IOReturn 
IOFWUserVectorCommand::setBuffers(	mach_vm_address_t submit_buffer_address, mach_vm_size_t submit_buffer_size,
									mach_vm_address_t result_buffer_address, mach_vm_size_t result_buffer_size )
{
	IOReturn status = kIOReturnSuccess;
	
	if( (submit_buffer_address == 0) || (submit_buffer_size == 0) ||
		(result_buffer_address == 0) || (result_buffer_size == 0) )
	{
		status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		if( fSubmitDesc )
		{
			fSubmitDesc->complete();
			fSubmitDesc->release();
			fSubmitDesc = NULL;
		}

		fSubmitDesc = IOMemoryDescriptor::withAddressRange(	submit_buffer_address, 
															submit_buffer_size, 
															kIODirectionOut, 
															fUserClient->getOwningTask() );
		if( fSubmitDesc == NULL )
		{
			status = kIOReturnNoMemory;
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		status = fSubmitDesc->prepare();
	}

	if( status == kIOReturnSuccess )
	{
		if( fResultDesc )
		{
			fResultDesc->complete();
			fResultDesc->release();
			fResultDesc = NULL;
		}

		fResultDesc = IOMemoryDescriptor::withAddressRange(	result_buffer_address, 
															result_buffer_size, 
															kIODirectionIn, 
															fUserClient->getOwningTask() );
		if( fResultDesc == NULL )
		{
			status = kIOReturnNoMemory;
		}
	}

	if( status == kIOReturnSuccess )
	{
		status = fResultDesc->prepare();
	}
		
	return status;
}

// submit
//
//

IOReturn 
IOFWUserVectorCommand::submit( OSAsyncReference64 async_ref, mach_vm_address_t callback, io_user_reference_t refCon )
{
	IOReturn status = kIOReturnSuccess;
	
	if( fSubmitDesc == NULL )
	{
		status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		IOFireWireUserClient::setAsyncReference64( fAsyncRef, (mach_port_t)async_ref[0], callback, refCon );
	}
	
	if( status == kIOReturnSuccess )
	{
		fControl->closeGate();
		
		fInflightCmds = 0;
		fResultOffset = 0;
		fVectorStatus = kIOReturnSuccess;
		
		mach_vm_size_t offset = 0;
		mach_vm_size_t length = sizeof(CommandSubmitParams);
		mach_vm_size_t size = fSubmitDesc->getLength();
		while( (status == kIOReturnSuccess) && ((offset + length) <= size) )
		{
			CommandSubmitParams params;
			fSubmitDesc->readBytes( offset, &params, length );
			status = submitOneCommand( &params );
			//zzz probably need an output array communicating which commands succeeded and failed
			
			offset += length;
		}
		
		fControl->openGate();
	}
	
	return status;
}

// submitOneCommand
//
//

IOReturn 
IOFWUserVectorCommand::submitOneCommand( CommandSubmitParams * params )
{
	IOReturn status = kIOReturnSuccess;
	
	if( status == kIOReturnSuccess )
	{
		if( params->kernCommandRef == NULL )
		{
			status = kIOReturnBadArgument;
		}
	}
	
	const OSObject * object = NULL;
	if( status == kIOReturnSuccess )
	{
		IOFWUserObjectExporter * exporter = fUserClient->getExporter();
		object = exporter->lookupObject( params->kernCommandRef );
		if( !object )
		{
			status = kIOReturnBadArgument;
		}
	}
	
	IOFWUserCommand * cmd = NULL;
	if( status == kIOReturnSuccess )
	{
		cmd = OSDynamicCast( IOFWUserCommand, object );
		if( !cmd )
		{
			object->release();
			object = NULL;
			status = kIOReturnBadArgument;
		}
	}

	if( status == kIOReturnSuccess )
	{
	//	OSAsyncReference64 * async_ref = cmd->getAsyncReference64();
	//	setAsyncReference64( *async_ref, (mach_port_t)async_ref[0], (mach_vm_address_t)params->callback, (io_user_reference_t)params->refCon);	
		
		// disable packet flushing during vector submit
		cmd->setFlush( false );
		cmd->setVectorCommand( this );	// connect to vector
		
		status = cmd->submit( params, NULL );
		fInflightCmds++;
		// turn flush back on for future submits (possibly not using a vector);
		cmd->setFlush( true );
	}
		
	if( cmd )
	{
		cmd->release();		// we need to release this in all cases
		cmd = NULL;
	}

	return status;
}

// asyncCompletion
//
//

void
IOFWUserVectorCommand::asyncCompletion(
	void *					refcon, 
	IOReturn 				status, 
	IOFireWireNub *			device, 
	IOFWCommand *			fwCmd )
{
	if( fInflightCmds > 0 )
	{
		fInflightCmds--;
		
		IOFWUserCommand * cmd = (IOFWUserCommand*)refcon;
		IOFWAsyncCommand * async_cmd = cmd->getAsyncCommand();
		
		CommandSubmitResult result;
		
		result.kernCommandRef = 0;	// not used on vector path
		result.result = status;
		result.bytesTransferred = async_cmd->getBytesTransferred();
		result.ackCode = async_cmd->getAckCode();
		result.responseCode = async_cmd->getResponseCode();
		result.refCon = cmd->getRefCon();
		
		fResultDesc->writeBytes( fResultOffset, &result, sizeof(CommandSubmitResult) );
		fResultOffset += sizeof(CommandSubmitResult);
		
		if( status != kIOReturnSuccess )
		{
			fVectorStatus = status;
		}
		
		cmd->setVectorCommand( NULL );	// disconnect from vector
		
		if( fInflightCmds == 0 )
		{
			// we're done
			IOFireWireUserClient::sendAsyncResult64( fAsyncRef, fVectorStatus, NULL, 0 );
		}
	}
}

// asyncPHYCompletion
//
//

void
IOFWUserVectorCommand::asyncPHYCompletion(
	void *					refcon, 
	IOReturn 				status, 
	IOFireWireBus *			bus, 
	IOFWAsyncPHYCommand *	fwCmd )
{
	if( fInflightCmds > 0 )
	{
		fInflightCmds--;
		
		IOFWUserPHYCommand * cmd = (IOFWUserPHYCommand*)refcon;
		IOFWAsyncPHYCommand * async_cmd = cmd->getAsyncPHYCommand();
		
		CommandSubmitResult result;
		
		result.kernCommandRef = 0;	// not used on vector path
		result.result = status;
		result.bytesTransferred = 8;
		result.ackCode = async_cmd->getAckCode();
		result.responseCode = async_cmd->getResponseCode();
		result.refCon = cmd->getRefCon();
		
		fResultDesc->writeBytes( fResultOffset, &result, sizeof(CommandSubmitResult) );
		fResultOffset += sizeof(CommandSubmitResult);
		
		if( status != kIOReturnSuccess )
		{
			fVectorStatus = status;
		}
		
		cmd->setVectorCommand( NULL );	// disconnect from vector
		
		if( fInflightCmds == 0 )
		{
			// we're done
			IOFireWireUserClient::sendAsyncResult64( fAsyncRef, fVectorStatus, NULL, 0 );
		}
	}
}