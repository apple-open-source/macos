/*
 *  IOFWUserCommand.cpp
 *  IOFireWireFamily
 *
 *  Created by noggin on Tue May 08 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <libkern/c++/OSObject.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFWUserCommand.h>
#include <IOKit/firewire/IOFireWireFamilyCommon.h>

OSDefineMetaClassAndAbstractStructors(IOFWUserCommand, OSObject)

OSDefineMetaClassAndStructors(IOFWUserReadCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserReadQuadletCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserWriteCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserWriteQuadletCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserCompareSwapCommand, IOFWUserCommand)

void
IOFWUserCommand::free()
{
//	IOLog("IOFWUserCommand::free: this=%08lX, fCommand=%08lX, fUserClient=%08lX, fMem=%08lX\n", this, fCommand, fUserClient, fMem) ;

	if ( fCommand )
	{
//		IOReturn	cancelStatus	= fCommand->( kIOReturnAborted ) ;
//		IOLog("IOFWUserCommand::free: fCommand->cancel failed w/ error 0x%08lX\n", cancelStatus) ;
		
		fCommand->release() ;
	}
	
	if (fUserClient)
		fUserClient->release() ;
	
	if (fMem)
		fMem->release() ;
}

void
IOFWUserCommand::setAsyncReference(
	OSAsyncReference	inAsyncRef)
{
	bcopy(inAsyncRef, fAsyncRef, sizeof(OSAsyncReference)) ;
}

IOFWUserCommand*
IOFWUserCommand::withSubmitParams(
	const FWUserCommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	IOFWUserCommand*	result	= NULL ;
	
	switch ( inParams->type )
	{
		case kFireWireCommandType_Read:
			result = new IOFWUserReadCommand ;
			break ;
		case kFireWireCommandType_ReadQuadlet:
			result = new IOFWUserReadQuadletCommand ;
			break ;
		
		case kFireWireCommandType_Write:
			result = new IOFWUserWriteCommand ;
			break ;
		
		case kFireWireCommandType_WriteQuadlet:
			result = new IOFWUserWriteQuadletCommand ;
			break ;
		
		case kFireWireCommandType_CompareSwap:
			result = new IOFWUserCompareSwapCommand ;
			break ;
		
		default:
			IOLog("IOFWUserCommand::withSubmitParams: bad command type!\n") ;
			break ;
	}
	
	if (result && !result->initWithSubmitParams( inParams, inUserClient ))
	{
		result->release() ;
		result = NULL ;
	}
	
	return result ;
}

bool
IOFWUserCommand::initWithSubmitParams(
	const FWUserCommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	fAsyncRef[0] = 0 ;

	fUserClient	= inUserClient ;
	fUserClient->retain() ;
	
	return true ;
}

void
IOFWUserCommand::asyncReadWriteCommandCompletion(
	void *					refcon, 
	IOReturn 				status, 
	IOFireWireNub *			device, 
	IOFWCommand *			fwCmd)
{	
	if (refcon && ((IOFWUserReadQuadletCommand*)refcon)->fAsyncRef[0] )
	{		
		IOByteCount		bytesTransferred = ((IOFWUserCommand*)refcon)->fCommand->getBytesTransferred() ;
		IOReturn result = IOFireWireUserClient::sendAsyncResult( ((IOFWUserCommand*)refcon)->fAsyncRef, status, (void**) & bytesTransferred, 1) ;
		if (kIOReturnSuccess != result)
			IOLog("IOFWUserCommand::asyncReadWriteCommandCompletion: sendAsyncResult returned error 0x%08lX\n", (UInt32) result) ;
	}
}

void
IOFWUserReadQuadletCommand::asyncReadQuadletCommandCompletion(
	void *					refcon, 
	IOReturn 				status, 
	IOFireWireNub *			device, 
	IOFWCommand *			fwCmd)
{
//	IOLog( "+ IOFireWireUserClient::asyncReadQuadletCommandCompletion: refCon=0x%08lX, status=%08lX, device=%08lX, fwCmd=%08lX\n",
//		   refcon, status, device, fwCmd) ;
//	IOLog("IOFireWireUserClient::asyncReadQuadletCommandCompletion: command->getBytesTransferred=%ud\n", ((IOFWUserReadQuadletCommand*)refcon)->fCommand->getBytesTransferred()) ;
//	IOLog("IOFWUserCommand::asyncReadQuadletCommandCompletion: callback=%08lX\n", ((IOFWUserCommand*)refcon)->fAsyncRef[kIOAsyncCalloutFuncIndex]) ;

	if (refcon && ((IOFWUserReadQuadletCommand*)refcon)->fAsyncRef[0] )
	{
		IOReturn result = IOFireWireUserClient::sendAsyncResult( ((IOFWUserReadQuadletCommand*)refcon)->fAsyncRef, 
																 status, 
																 (void**)((IOFWUserReadQuadletCommand*)refcon)->fQuads, 
																 (((IOFWUserReadQuadletCommand*)refcon)->fCommand->getBytesTransferred() >> 2) + 2 ) ;
		if (kIOReturnSuccess != result)
			IOLog("IOFireWireUserClient::asyncReadQuadletCommandCompletion: sendAsyncResult returned error 0x%08lX\n", result) ;
	}
}

// ============================================================
// IOFWUserReadQuadletCommand
// ============================================================

void
IOFWUserReadQuadletCommand::free()
{
	if (fQuads)
	{
		delete[] fQuads ;
		fQuads = NULL ;
		fNumQuads = 0 ;
	}

	IOFWUserCommand::free() ;
}

bool
IOFWUserReadQuadletCommand::initWithSubmitParams(
	const FWUserCommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(inParams, inUserClient)) ;
	
	if (result)
	{
		fMem = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->newBuffer, inParams->newBufferSize, kIODirectionOut, fUserClient->getOwningTask() ) ;
		result = (NULL != fMem) ;
	}
	
	if (result)
	{
		fNumQuads = inParams->newBufferSize >> 2 ;
		fQuads = new UInt32[fNumQuads+2] ;

		result = (NULL != fQuads) ;
	}
	
	if (!result)
	{
		if (fQuads)
			delete[] fQuads ;
		if (fMem)
			fMem->release() ;
	}
	
	return result ;
}

IOReturn
IOFWUserReadQuadletCommand::submit(
	FWUserCommandSubmitParams*	inParams,
	FWUserCommandSubmitResult*	outResult)
{
	// read quads uses quads by reference, so we have to keep fQuads around
	IOReturn	result		= kIOReturnSuccess ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer)
	{
		if (fNumQuads != inParams->newBufferSize >> 2 )
		{
			if ( !inParams->syncFlag )
			{
				if (fQuads)
					delete[] fQuads ;
					
				fQuads = new UInt32[ fNumQuads + 2 ] ;	// padding to ensure at least 3 quads
														// to make sendAsyncResult work as expected
				if (fQuads)
					fNumQuads = inParams->newBufferSize >> 2 ;
				else
				{
					fNumQuads = 0 ;
					fMem->release() ;
					fMem = NULL ;
				}
			}
			else
				fNumQuads = inParams->newBufferSize >> 2 ;
		}
	}
	
	if ( kIOReturnSuccess == result )
	{
		if ( !fCommand )
		{
			if (NULL == (fCommand = fUserClient->getOwner()->createReadQuadCommand( inParams->newTarget,
																	inParams->syncFlag ? (UInt32*)(outResult + 1) : fQuads,
																	fNumQuads,
																	inParams->syncFlag ? NULL : & IOFWUserReadQuadletCommand::asyncReadQuadletCommandCompletion,
																	this,
																	inParams->newFailOnReset ) ))
				result = kIOReturnNoMemory ;
			else
				fCommand->setGeneration( inParams->newGeneration ) ;
		}
		else// if ( inParams->staleFlags )
		{
			result = ((IOFWReadQuadCommand*)fCommand)->reinit( inParams->newTarget,
															   inParams->syncFlag ? (UInt32*)(outResult + 1) : fQuads,
															   fNumQuads,
															   inParams->syncFlag ? NULL : & IOFWUserReadQuadletCommand::asyncReadQuadletCommandCompletion,
															   this,
															   inParams->newFailOnReset ) ;
			fCommand->setGeneration(inParams->newGeneration) ;
		}
	}
	
//	FWAddress	address = fCommand->getAddress() ;
//	IOLog( "IOFWUserReadQuadletCommand: submitting command 0x%08lX with addr=%04lX:%04lX:%08lX, size=%ud\n",
//		   fCommand, address.nodeID, address.addressHi, address.addressLo, fCommand->fSize) ;
	if (kIOReturnSuccess == result)
		result = fCommand->submit() ;

	if (inParams->syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
}

// ============================================================
// IOFWUserReadCommand
// ============================================================

bool
IOFWUserReadCommand::initWithSubmitParams(
	const FWUserCommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(inParams, inUserClient)) ;
	
	if (result)
	{
		fMem = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->newBuffer, inParams->newBufferSize, kIODirectionOut, fUserClient->getOwningTask() ) ;
		result = (NULL != fMem) ;
	}
	
	if (!result)
	{
		if (fMem)
			fMem->release() ;
	}
	
	return result ;
}

IOReturn
IOFWUserReadCommand::submit(
	FWUserCommandSubmitParams*	inParams,
	FWUserCommandSubmitResult*	outResult)
{
	IOReturn	result		= kIOReturnSuccess ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )
	{
		if ( fMem )
			fMem->release() ;

		if (NULL == (fMem = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->newBuffer, 
												inParams->newBufferSize, 
												kIODirectionOut, 
												fUserClient->getOwningTask())) )
		{
			result = kIOReturnNoMemory ;
		}
	}

	if ( kIOReturnSuccess == result)
	{
		if (!fCommand)
		{
			if (NULL == (fCommand = fUserClient->getOwner()->createReadCommand( inParams->newTarget,
																fMem,
																inParams->syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																this,
																inParams->newFailOnReset ) ))
				result = kIOReturnNoMemory ;
			else
				fCommand->setGeneration( inParams->newGeneration ) ;
		}
		else// if (inParams->staleFlags )
		{
//			IOLog( "IOFWUserReadCommand::submit: fCommand=%08lX, newGeneration=%08lX, newTarget=%04lX%04lX:%08lX, fMem=%08lX\n", 
//				   fCommand, 
//				   inParams->newGeneration, 
//				   inParams->newTarget.nodeID, inParams->newTarget.addressHi, inParams->newTarget.addressLo,
//				   fMem ) ;
		
			setRefCon(inParams->refCon) ;
			result = ((IOFWReadCommand*)fCommand)->reinit( inParams->newTarget,
														   fMem, 
														   inParams->syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
														   this,
														   inParams->newFailOnReset ) ;
			fCommand->setGeneration(inParams->newGeneration) ;
			
			if (kIOReturnSuccess != result)
				IOLog("IOFWUserReadCommand::submit: fCommand->reinit result=%08lX\n", result) ;
		}
	}
	
	if ( kIOReturnSuccess == result)
	{
		result = fCommand->submit() ;
		if (kIOReturnSuccess != result)
			IOLog("IOFWUserReadCommand::submit: fCommand->submit result=%08lX\n", result) ;
	}
						
	if (inParams->syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
}

// ============================================================
// IOFWUserWriteQuadletCommand
// ============================================================

bool
IOFWUserWriteQuadletCommand::initWithSubmitParams(
	const FWUserCommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(inParams, inUserClient)) ;
	
	if (result)
		fNumQuads = inParams->newBufferSize >> 2 ;
	
	return result ;
}

IOReturn
IOFWUserWriteQuadletCommand::submit(
	FWUserCommandSubmitParams*	inParams,
	FWUserCommandSubmitResult*	outResult)
{
	IOReturn	result = kIOReturnSuccess ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )
		fNumQuads = inParams->newBufferSize >> 2 ;

	if (!fCommand)
	{
		if (NULL == (fCommand = fUserClient->getOwner()->createWriteQuadCommand( inParams->newTarget,
																	(UInt32*)(inParams+1),
																	fNumQuads,
																inParams->syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	this,
																	inParams->newFailOnReset ) ))
			result = kIOReturnNoMemory ;
		else
			fCommand->setGeneration( inParams->newGeneration ) ;
	}
	else //if ( inParams->staleFlags )
	{
		result = ((IOFWWriteQuadCommand*)fCommand)->reinit( inParams->newTarget, 
															(UInt32*)(inParams + 1),
															fNumQuads,
																inParams->syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
															this,
															inParams->newFailOnReset ) ;
		fCommand->setGeneration(inParams->newGeneration) ;
	}
	
	if (kIOReturnSuccess == result)
		result = fCommand->submit() ;
						
	if (inParams->syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
}

// ============================================================
// IOFWUserWriteCommand
// ============================================================
bool
IOFWUserWriteCommand::initWithSubmitParams(
	const FWUserCommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(inParams, inUserClient)) ;
	
	if (result)
	{
		fMem = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->newBuffer, inParams->newBufferSize, kIODirectionIn, fUserClient->getOwningTask() ) ;
		result = (NULL != fMem) ;
	}
	
	if (!result)
	{
		if (fMem)
			fMem->release() ;
	}
	
	return result ;
}

IOReturn
IOFWUserWriteCommand::submit(
	FWUserCommandSubmitParams*	inParams,
	FWUserCommandSubmitResult*	outResult)
{
	IOReturn	result		= kIOReturnSuccess ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )
	{
		if ( fMem )
			fMem->release() ;

		if (NULL == (fMem = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->newBuffer, 
												inParams->newBufferSize, 
												kIODirectionOut, 
												fUserClient->getOwningTask())) )
		{
			result = kIOReturnNoMemory ;
		}
	}

	if ( kIOReturnSuccess == result )
	{
		if (!fCommand)
		{
			if (NULL == (fCommand = fUserClient->getOwner()->createWriteCommand( inParams->newTarget,
																				 fMem,
																inParams->syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																				 this,
																				 inParams->newFailOnReset ) ))
				result = kIOReturnNoMemory ;
			else
				fCommand->setGeneration( inParams->newGeneration ) ;

//			IOLog("IOFWUserWriteCommand::submit: new command created @ 0x%08lX\n", fCommand) ;
		}
		else //if ( inParams->staleFlags )
		{
			result = ((IOFWWriteCommand*)fCommand)->reinit( inParams->newTarget,
															fMem,
																inParams->syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
															this,
															inParams->newFailOnReset ) ;
			fCommand->setGeneration( inParams->newGeneration ) ;

//			IOByteCount	tempSize = 4 ;
//			IOLog("IOFWUserWriteCommand::submit: command reinited addr=%04lX:%08lX, value=%08lX\n", inParams->newTarget.addressHi, inParams->newTarget.addressLo, *(UInt32*)fMem->getVirtualSegment(0, &tempSize)) ;
		}
	}
	
//	FWAddress	address = fCommand->getAddress() ;
//	IOLog( "IOFWUserReadQuadletCommand: submitting command 0x%08lX with addr=%04lX:%04lX:%08lX, size=%ud\n",
//		   fCommand, address.nodeID, address.addressHi, address.addressLo, fCommand->fSize) ;

	if ( kIOReturnSuccess == result )
		result = fCommand->submit() ;
						
	if (inParams->syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
//		IOLog("IOFWUserWriteCommand::submit: synchronous sizeof(result)=0x%08lX, result=0x%08lX, bytes transferred=0x%08lX\n", sizeof(*outResult), outResult->result, outResult->bytesTransferred) ;
	}	

	return result ;
}

// ============================================================
// IOFWUserCompareSwapCommand
// ============================================================
bool
IOFWUserCompareSwapCommand::initWithSubmitParams(
	const FWUserCommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(inParams, inUserClient)) ;
	
	return result ;
}

IOReturn
IOFWUserCompareSwapCommand::submit(
	FWUserCommandSubmitParams*	inParams,
	FWUserCommandSubmitResult*	outResult)
{
	IOReturn	result		= kIOReturnSuccess ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )
		fSize = inParams->newBufferSize >> 2 ;

	if ( kIOReturnSuccess == result )
	{
		if (result)
		{
			if (NULL == (fCommand = fUserClient->getOwner()->createCompareAndSwapCommand( inParams->newTarget, 
																			(UInt32*)(inParams+1),// cmpVal past end of param struct
																			((UInt32*)(inParams+1)) + (fSize << 2),// newVal past end of cmpVal
																			fSize,
																			inParams->syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																			this,
																			inParams->newFailOnReset ) ))
				result = kIOReturnNoMemory ;
			else
				fCommand->setGeneration( inParams->newGeneration ) ;

//			IOLog("IOFWUserWriteCommand::submit: new command created @ 0x%08lX\n", fCommand) ;				
		}
		else //if ( inParams->staleFlags )
		{
			result = ((IOFWCompareAndSwapCommand*)fCommand)->reinit( inParams->newTarget, 
																	 (UInt32*)(inParams+1),// cmpVal past end of param struct
																	 ((UInt32*)(inParams+1)) + (fSize << 2),// newVal past end of cmpVal
																	 fSize,
																	 inParams->syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	 this,
																	 inParams->newFailOnReset ) ;
			fCommand->setGeneration( inParams->newGeneration ) ;
		}
	}
	if ( kIOReturnSuccess == result)
		result = fCommand->submit() ;
						
	if (inParams->syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
}
