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
#include <IOKit/firewire/IOFireWireFamilyCommon.h>

#include "IOFWUserCommand.h"

OSDefineMetaClassAndAbstractStructors(IOFWUserCommand, OSObject)

OSDefineMetaClassAndStructors(IOFWUserReadCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserWriteCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserCompareSwapCommand, IOFWUserCommand)

#pragma mark === IOFWUserCommand ===
// ============================================================
// IOFWUserCommand
// ============================================================

void
IOFWUserCommand::free()
{
	if ( fCommand )
	{
		IOReturn	cmdStatus = fCommand->getStatus() ;
		if ( cmdStatus == kIOReturnBusy || cmdStatus == kIOFireWirePending )
		{
			IOFireWireUserClientLog_(("cancelling cmd %p\n", fCommand)) ;
			fCommand->cancel( kIOReturnAborted ) ;
		}
		
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
//			result = new IOFWUserReadCommand ;
//			break ;
		case kFireWireCommandType_ReadQuadlet:
//			result = new IOFWUserReadQuadletCommand ;
//			break ;
			result = new IOFWUserReadCommand ;
			break ;
		
		case kFireWireCommandType_Write:
//			result = new IOFWUserWriteCommand ;
//			break ;
		
		case kFireWireCommandType_WriteQuadlet:
//			result = new IOFWUserWriteQuadletCommand ;
//			break ;
			result = new IOFWUserWriteCommand ;
			break ;
			
		case kFireWireCommandType_CompareSwap:
			result = new IOFWUserCompareSwapCommand ;
			break ;
		
		default:
			IOFireWireUserClientLog_(("IOFWUserCommand::withSubmitParams: bad command type!\n")) ;
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
	// not initing member vars as they should be 0'd 
	// for us by the kernel
	
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
//	if (refcon && ((IOFWUserReadQuadletCommand*)refcon)->fAsyncRef[0] )
//	{		
//		IOByteCount		bytesTransferred = ((IOFWUserCommand*)refcon)->fCommand->getBytesTransferred() ;
//		IOReturn result = IOFireWireUserClient::sendAsyncResult( ((IOFWUserCommand*)refcon)->fAsyncRef, status, (void**) & bytesTransferred, 1) ;
//		if (kIOReturnSuccess != result)
//			IOFireWireUserClientLog_(("IOFWUserCommand::asyncReadWriteCommandCompletion: sendAsyncResult returned error 0x%08lX\n", (UInt32) result)) ;
//	}
	IOFWUserCommand*	cmd = (IOFWUserCommand*) refcon ;

	if ( refcon && cmd->fAsyncRef[0] ) 
	{
		IOByteCount		bytesTransferred = cmd->fCommand->getBytesTransferred() ;
		IOReturn		result = IOFireWireUserClient::sendAsyncResult( cmd->fAsyncRef, status, (void**) & bytesTransferred, 1 ) ;
		
		IOFireWireUserClientLogIfErr_( result, ("IOFWUserCommand::asyncReadWriteCommandCompletion: sendAsyncResult returned error 0x%08lX\n", (UInt32) result) ) ;
	}
}

void
IOFWUserCommand::asyncReadQuadletCommandCompletion(
	void *					refcon, 
	IOReturn 				status, 
	IOFireWireNub *			device, 
	IOFWCommand *			fwCmd)
{
	IOFWUserCommand*	cmd = (IOFWUserCommand*)refcon ;

	if (refcon && cmd->fAsyncRef[0] )
	{
		IOReturn result = IOFireWireUserClient::sendAsyncResult( cmd->fAsyncRef, 
																 status, 
																 (void**)cmd->fQuads, 
																 ( cmd->fCommand->getBytesTransferred() >> 2) + 2 ) ;
		IOFireWireUserClientLogIfErr_( result, ("IOFireWireUserClient::asyncReadQuadletCommandCompletion: sendAsyncResult returned error 0x%08lX\n", result)) ;
	}
}

#pragma mark -
#pragma mark === ¥ IOFWUserReadCommand ===
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
	Boolean		syncFlag 	= inParams->flags & kFWCommandInterfaceSyncExecute ;
	Boolean		copyFlag	= inParams->flags & kFireWireCommandUseCopy ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )	// do we need reevaluate our buffers?
	{
		if ( fMem )	// whatever happens, we're going to need a new memory descriptor
			fMem->release() ;

		if ( copyFlag )	// is this command using in-line data?
		{
			if (fQuads && (fNumQuads != inParams->newBufferSize) || syncFlag)	// if we're executing synchronously,
																				// don't need quadlet buffer
			{
				IOFree(fQuads, fNumQuads << 2) ;
				fQuads = NULL ;
			}
			
			if (!syncFlag)
			{
				fNumQuads = inParams->newBufferSize ;
				fQuads = (UInt32*)IOMalloc(inParams->newBufferSize << 2) ;
			}
				
			fMem = NULL ;
		}
		else
		{
			if (fQuads)
			{
				IOFree(fQuads, fNumQuads << 2) ;
				fQuads = NULL ;
				fNumQuads = 0 ;
			}
			
			if (NULL == (fMem = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->newBuffer, 
													inParams->newBufferSize, 
													kIODirectionOut, 
													fUserClient->getOwningTask())) )
			{
				result = kIOReturnNoMemory ;
			}
		}
	}

	if ( kIOReturnSuccess == result)
	{
		if (fCopyFlag != copyFlag)
			if (fCommand)
			{
				fCommand->release() ;	// we had a normal command and need a quadlet command or vice-versa
				fCommand = NULL ;
			}

		if (fCommand)
		{
			if ( copyFlag )
				if (syncFlag)
				{
					result = ((IOFWReadQuadCommand*)fCommand)->reinit( inParams->newTarget,
																	   (UInt32*) inParams+1,
																	   inParams->newBufferSize,
																	   NULL,
																	   this,
																	   inParams->newFailOnReset) ;
				}
				else
				{
					result = ((IOFWReadQuadCommand*)fCommand)->reinit( inParams->newTarget,
																	   fQuads,
																	   fNumQuads,
																	   & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	   this,
																	   inParams->newFailOnReset) ;
				}
			else
				result = ((IOFWReadCommand*)fCommand)->reinit( inParams->newTarget,
															fMem, 
															syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
															this,
															inParams->newFailOnReset ) ;
			fCommand->setGeneration(inParams->newGeneration) ;
			
			IOFireWireUserClientLogIfErr_(result, ("IOFWUserReadCommand::submit: fCommand->reinit result=%08lX\n", result)) ;
		}
		else// if (inParams->staleFlags )
		{
			if ( copyFlag )
			{
				if (syncFlag)
				{
					fCommand = fUserClient->getOwner()->createReadQuadCommand( inParams->newTarget,
																			   (UInt32*) inParams+1,
																			   inParams->newBufferSize,
																			   NULL,
																			   this,
																			   inParams->newFailOnReset ) ;
				}
				else															   
				{
					// create a quadlet command and copy in-line quads into it.
					fCommand = fUserClient->getOwner()->createReadQuadCommand( inParams->newTarget,
																			   fQuads,
																			   fNumQuads,
																			   & IOFWUserCommand::asyncReadQuadletCommandCompletion,
																			   this,
																			   inParams->newFailOnReset) ;
				}
			}
			else
			{
				// create a read command -- memory descriptor based
				fCommand = fUserClient->getOwner()->createReadCommand( inParams->newTarget,
																	   fMem,
																	   syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	   this,
																	   inParams->newFailOnReset ) ;
			}
			
			if (!fCommand)
				result = kIOReturnNoMemory ;
			else
			{
				fCommand->setGeneration( inParams->newGeneration ) ;
				if (inParams->newMaxPacket)
					fCommand->setMaxPacket(inParams->newMaxPacket) ;// zzz is there any reason for us to pay
																	// attention to the result of this function?
																	// if the command is errored/busy, will we get
																	// this far?
			}
		}
	}
	
	if ( kIOReturnSuccess == result)
	{
		setRefCon(inParams->refCon) ;

		result = fCommand->submit() ;
		IOFireWireUserClientLogIfErr_(result, ("IOFWUserReadCommand::submit: fCommand->submit result=%08lX\n", result)) ;
	}
						
	if (syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
}

#pragma mark -
#pragma mark === ¥ IOFWUserWriteCommand ===
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
	Boolean		syncFlag 	= inParams->flags & kFWCommandInterfaceSyncExecute ;
	Boolean		copyFlag	= inParams->flags & kFireWireCommandUseCopy ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )	// do we need reevaluate our buffers?
	{
		if ( fMem )	// whatever happens, we're going to need a new memory descriptor
			fMem->release() ;

		if ( copyFlag )	// is this command using in-line data?
			fMem = NULL ;
		else
		{
			if (NULL == (fMem = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->newBuffer, 
													inParams->newBufferSize, 
													kIODirectionOut, 
													fUserClient->getOwningTask())) )
			{
				result = kIOReturnNoMemory ;
			}
		}
	}

	if ( kIOReturnSuccess == result)
	{
		if (fCopyFlag != copyFlag)
			if (fCommand)
			{
				fCommand->release() ;	// we had a normal command and need a quadlet command or vice-versa
				fCommand = NULL ;
			}

		if (fCommand)
		{
			if ( copyFlag )
			{
				result = ((IOFWWriteQuadCommand*)fCommand)->reinit( inParams->newTarget,
																	(UInt32*) inParams+1,
																	inParams->newBufferSize,
																	syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	this,
																	inParams->newFailOnReset) ;
			}
			else
			{
				result = ((IOFWWriteCommand*)fCommand)->reinit( inParams->newTarget,
															    fMem, 
															    syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
															    this,
															    inParams->newFailOnReset ) ;
			}
			
			IOFireWireUserClientLogIfErr_( result, ("IOFWUserWriteCommand::submit: fCommand->reinit result=%08lX\n", result)) ;
		}
		else
		{
			if ( copyFlag )
			{
				fCommand = fUserClient->getOwner()->createWriteQuadCommand( inParams->newTarget,
																		   (UInt32*) inParams+1,
																		   inParams->newBufferSize,
																		   syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																		   this,
																		   inParams->newFailOnReset ) ;
			}
			else
			{
				// create a read command -- memory descriptor based
				fCommand = fUserClient->getOwner()->createWriteCommand( inParams->newTarget,
																	   fMem,
																	   syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	   this,
																	   inParams->newFailOnReset ) ;
			}
			
			if (!fCommand)
				result = kIOReturnNoMemory ;
			else
			{
				fCommand->setGeneration( inParams->newGeneration ) ;
				if (inParams->staleFlags & kFireWireCommandStale_MaxPacket)
					fCommand->setMaxPacket(inParams->newMaxPacket) ;// zzz is there any reason for us to pay
																	// attention to the result of this function?
																	// if the command is errored/busy, will we get
																	// this far?
			}
		}
	}
	
	if ( kIOReturnSuccess == result)
	{
		setRefCon(inParams->refCon) ;

		result = fCommand->submit() ;
		IOFireWireUserClientLogIfErr_( result, ("IOFWUserReadCommand::submit: fCommand->submit result=%08lX\n", result)) ;
	}
						
	if (syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
}

/*IOReturn
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

	Boolean	syncFlag = inParams->flags & kFWCommandInterfaceSyncExecute ;
	if ( kIOReturnSuccess == result )
	{
		if (!fCommand)
		{
			if (NULL == (fCommand = fUserClient->getOwner()->createWriteCommand( inParams->newTarget,
																				 fMem,
																				 syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																				 this,
																				 inParams->newFailOnReset ) ))
				result = kIOReturnNoMemory ;
			else
				fCommand->setGeneration( inParams->newGeneration ) ;
		}
		else //if ( inParams->staleFlags )
		{
			result = ((IOFWWriteCommand*)fCommand)->reinit( inParams->newTarget,
															fMem,
															syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
															this,
															inParams->newFailOnReset ) ;
			fCommand->setGeneration( inParams->newGeneration ) ;
		}
	}
	
	if ( kIOReturnSuccess == result )
		result = fCommand->submit() ;
						
	if (syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
} */

#pragma mark -
#pragma mark === ¥ IOFWUserCompareSwapCommand ===
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

	Boolean	syncFlag = inParams->flags & kFWCommandInterfaceSyncExecute ;
	if ( kIOReturnSuccess == result )
	{
		if (result)
		{
			if (NULL == (fCommand = fUserClient->getOwner()->createCompareAndSwapCommand( inParams->newTarget, 
																			(UInt32*)(inParams+1),// cmpVal past end of param struct
																			((UInt32*)(inParams+1)) + (fSize << 2),// newVal past end of cmpVal
																			fSize,
																			syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																			this,
																			inParams->newFailOnReset ) ))
				result = kIOReturnNoMemory ;
			else
				fCommand->setGeneration( inParams->newGeneration ) ;

		}
		else //if ( inParams->staleFlags )
		{
			result = ((IOFWCompareAndSwapCommand*)fCommand)->reinit( inParams->newTarget, 
																	 (UInt32*)(inParams+1),// cmpVal past end of param struct
																	 ((UInt32*)(inParams+1)) + (fSize << 2),// newVal past end of cmpVal
																	 fSize,
																	 syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	 this,
																	 inParams->newFailOnReset ) ;
			fCommand->setGeneration( inParams->newGeneration ) ;
		}
	}
	if ( kIOReturnSuccess == result)
		result = fCommand->submit() ;
						
	if (syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
}
