/*
 *  IOFWUserCommand.cpp
 *  IOFireWireFamily
 *
 *  Created by noggin on Tue May 08 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

// public
#import <IOKit/firewire/IOFireWireDevice.h>
#import <IOKit/firewire/IOFireWireFamilyCommon.h>

// private
#import "IOFWUserCommand.h"
#import "IOFireWireLib.h"

OSDefineMetaClassAndAbstractStructors(IOFWUserCommand, OSObject)
OSDefineMetaClassAndStructors(IOFWUserReadCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserWriteCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserCompareSwapCommand, IOFWUserCommand)

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
			IOFireWireUserClientLog_("cancelling cmd %p\n", fCommand) ;
			fCommand->cancel( kIOReturnAborted ) ;
		}
		
		fCommand->release() ;
	}
	
	if (fMem)
	{
		fMem->complete() ;
		fMem->release() ;
	}
}

void
IOFWUserCommand::setAsyncReference(
	OSAsyncReference	inAsyncRef)
{
	bcopy(inAsyncRef, fAsyncRef, sizeof(OSAsyncReference)) ;
}

IOFWUserCommand*
IOFWUserCommand::withSubmitParams(
	const CommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	IOFWUserCommand*	result	= NULL ;
	
	switch ( inParams->type )
	{
		case kFireWireCommandType_Read:
			// fallthru
		case kFireWireCommandType_ReadQuadlet:
			result = new IOFWUserReadCommand ;
			break ;
		
		case kFireWireCommandType_Write:
			// fallthru
		case kFireWireCommandType_WriteQuadlet:
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
	const CommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	fUserClient	= inUserClient ;
	return true ;
}


void
IOFWUserCommand::asyncReadWriteCommandCompletion(
	void *					refcon, 
	IOReturn 				status, 
	IOFireWireNub *			device, 
	IOFWCommand *			fwCmd)
{	
	IOFWUserCommand*	cmd = (IOFWUserCommand*) refcon ;

	if ( refcon && cmd->fAsyncRef[0] ) 
	{
		IOByteCount		bytesTransferred = cmd->fCommand->getBytesTransferred() ;
#if IOFIREWIREDEBUG > 0
		IOReturn		result = 
#endif
		IOFireWireUserClient::sendAsyncResult( cmd->fAsyncRef, status, (void**) & bytesTransferred, 1 ) ;
		
		IOFireWireUserClientLogIfErr_( result, "IOFWUserCommand::asyncReadWriteCommandCompletion: sendAsyncResult returned error 0x%08lX\n", (UInt32) result ) ;
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
		#if IOFIREWIREDEBUG > 0
		IOReturn result =
		#endif
		IOFireWireUserClient::sendAsyncResult( cmd->fAsyncRef, status, (void**)cmd->fQuads, ( cmd->fCommand->getBytesTransferred() >> 2) + 2 ) ;
		IOFireWireUserClientLogIfErr_( result, "IOFireWireUserClient::asyncReadQuadletCommandCompletion: sendAsyncResult returned error 0x%08x\n", result) ;
	}
}

#pragma mark -
// ============================================================
// IOFWUserReadCommand
// ============================================================

bool
IOFWUserReadCommand::initWithSubmitParams(
	const CommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(inParams, inUserClient)) ;
	
	if (result)
	{
		fMem = IOMemoryDescriptor::withAddress( (vm_address_t) inParams->newBuffer, inParams->newBufferSize, kIODirectionIn, fUserClient->getOwningTask() ) ;
		result = (NULL != fMem) ;
	}
	
	if (result)
	{
		IOReturn error = fMem->prepare() ;
		result = ( kIOReturnSuccess == error ) ;
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
	CommandSubmitParams*	inParams,
	CommandSubmitResult*	outResult)
{
	IOReturn	error		= kIOReturnSuccess ;
	Boolean		syncFlag 	= ( inParams->flags & kFWCommandInterfaceSyncExecute ) != 0 ;
	Boolean		copyFlag	= ( inParams->flags & kFireWireCommandUseCopy ) != 0;
	Boolean		absFlag		= ( inParams->flags & kFireWireCommandAbsolute ) != 0 ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )	// do we need reevaluate our buffers?
	{
		if ( fMem )	// whatever happens, we're going to need a new memory descriptor
		{
			fMem->complete() ;
			fMem->release() ;
		}
		
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
													kIODirectionIn, 
													fUserClient->getOwningTask())) )
			{
				error = kIOReturnNoMemory ;
			}
			else
			{
				error = fMem->prepare() ;
			
				if ( error )
				{
					fMem->release() ;
					fMem = NULL ;
				}
			}	
		}
	}

	if ( not error )
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
					if ( absFlag )
					{
						error = ((IOFWReadQuadCommand*)fCommand)->reinit( inParams->newGeneration, inParams->newTarget, (UInt32*) inParams+1, inParams->newBufferSize, NULL, this ) ;
					}
					else
						error = ((IOFWReadQuadCommand*)fCommand)->reinit( inParams->newTarget, (UInt32*) inParams+1, inParams->newBufferSize, NULL, this, inParams->newFailOnReset) ;
				}
				else
				{
					error = ((IOFWReadQuadCommand*)fCommand)->reinit( inParams->newTarget,
																	   fQuads,
																	   fNumQuads,
																	   & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	   this,
																	   inParams->newFailOnReset) ;
				}
			else
				error = ((IOFWReadCommand*)fCommand)->reinit( inParams->newTarget,
															fMem, 
															syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
															this,
															inParams->newFailOnReset ) ;

			fCommand->setGeneration(inParams->newGeneration) ;			
			IOFireWireUserClientLogIfErr_(error, "IOFWUserReadCommand::submit: fCommand->reinit error=%08x\n", error) ;
		}
		else// if (inParams->staleFlags )
		{
			if ( copyFlag )
			{
				if (syncFlag)
				{
					if ( absFlag )
					{
						fCommand = fUserClient->createReadQuadCommand( inParams->newGeneration, inParams->newTarget, (UInt32*) inParams+1, inParams->newBufferSize, NULL, this ) ;
					}
					else
						fCommand = fUserClient->getOwner()->createReadQuadCommand( inParams->newTarget, (UInt32*) inParams+1, inParams->newBufferSize, NULL, this, inParams->newFailOnReset ) ;
				}
				else															   
				{
					// create a quadlet command and copy in-line quads into it.
					if ( absFlag )
					{
						fCommand = fUserClient->createReadQuadCommand( inParams->newGeneration, inParams->newTarget, fQuads, fNumQuads, & IOFWUserCommand::asyncReadQuadletCommandCompletion, this ) ;
					}
					else
					{
						fCommand = fUserClient->getOwner()->createReadQuadCommand( inParams->newTarget,
																					fQuads,
																					fNumQuads,
																					& IOFWUserCommand::asyncReadQuadletCommandCompletion,
																					this,
																					inParams->newFailOnReset) ;
						if ( fCommand )
							fCommand->setGeneration( inParams->newGeneration ) ;
					}
				}
			}
			else
			{
				// create a read command -- memory descriptor based
				if ( absFlag )
				{
					fCommand = fUserClient->createReadCommand( inParams->newGeneration, inParams->newTarget, fMem, syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion, this ) ;
				}
				else
				{
					fCommand = fUserClient->getOwner()->createReadCommand( inParams->newTarget,
																			fMem,
																			syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																			this,
																			inParams->newFailOnReset ) ;
					if ( fCommand )
						fCommand->setGeneration( inParams->newGeneration ) ;
				}
			}
			
			if (!fCommand)
				error = kIOReturnNoMemory ;
		}
	}
	
	if ( not error )
	{
		if (inParams->staleFlags & kFireWireCommandStale_MaxPacket)
		{
			fCommand->setMaxPacket(inParams->newMaxPacket) ;
		}
		error = fCommand->submit() ;
		IOFireWireUserClientLogIfErr_(error, "IOFWUserReadCommand::submit: fCommand->submit error=%08x\n", error) ;
	}
						
	if (syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return error ;
}

#pragma mark -
// ============================================================
// IOFWUserWriteCommand
// ============================================================
bool
IOFWUserWriteCommand::initWithSubmitParams(
	const CommandSubmitParams*	inParams,
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
		IOReturn error = fMem->prepare() ;
		result = (error == kIOReturnSuccess) ;
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
	CommandSubmitParams*	inParams,
	CommandSubmitResult*	outResult)
{
	IOReturn	result		= kIOReturnSuccess ;
	Boolean		syncFlag 	= ( inParams->flags & kFWCommandInterfaceSyncExecute ) != 0 ;
	Boolean		copyFlag	= ( inParams->flags & kFireWireCommandUseCopy ) != 0 ;
	Boolean		absFlag		= ( inParams->flags & kFireWireCommandAbsolute ) != 0 ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )	// do we need reevaluate our buffers?
	{
		if ( fMem )	// whatever happens, we're going to need a new memory descriptor
		{
			fMem->complete() ;
			fMem->release() ;
		}
		
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
			else
			{
				result = fMem->prepare() ;
				
				if ( kIOReturnSuccess != result )
				{
					fMem->release() ;
					fMem = NULL ;
				}
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
			
			IOFireWireUserClientLogIfErr_( result, "IOFWUserWriteCommand::submit: fCommand->reinit result=%08x\n", result) ;
		}
		else
		{
			if ( copyFlag )
			{
				if ( absFlag )
					fCommand = fUserClient->createWriteQuadCommand( inParams->newGeneration, inParams->newTarget, (UInt32*) inParams+1, inParams->newBufferSize, syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion, this ) ;
				else
				{
					fCommand = fUserClient->getOwner()->createWriteQuadCommand( inParams->newTarget,
																				(UInt32*) inParams+1,
																				inParams->newBufferSize,
																				syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																				this,
																				inParams->newFailOnReset ) ;
					if ( fCommand )
						fCommand->setGeneration( inParams->newGeneration ) ;
				}
			}
			else
			{
				// create a read command -- memory descriptor based
				if ( absFlag )
					fCommand = fUserClient->createWriteCommand( inParams->newGeneration,
																inParams->newTarget,
																fMem,
																syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																this ) ;
				else
				{
					fCommand = fUserClient->getOwner()->createWriteCommand( inParams->newTarget,
																			fMem,
																			syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																			this,
																			inParams->newFailOnReset ) ;
					if ( fCommand )
						fCommand->setGeneration( inParams->newGeneration ) ;
				}
			}
			
			if (!fCommand)
				result = kIOReturnNoMemory ;
		}
	}
	
	if ( kIOReturnSuccess == result)
	{
		if (inParams->staleFlags & kFireWireCommandStale_MaxPacket)
			fCommand->setMaxPacket(inParams->newMaxPacket) ;// zzz is there any reason for us to pay
		result = fCommand->submit() ;
		IOFireWireUserClientLogIfErr_( result, "IOFWUserReadCommand::submit: fCommand->submit result=%08x\n", result) ;
	}
						
	if (syncFlag)
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
	}	

	return result ;
}

#pragma mark -
// ============================================================
// IOFWUserCompareSwapCommand
// ============================================================
bool
IOFWUserCompareSwapCommand::initWithSubmitParams(
	const CommandSubmitParams*	inParams,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(inParams, inUserClient)) ;
	
	return result ;
}

IOReturn
IOFWUserCompareSwapCommand::submit(
	CommandSubmitParams*	inParams,
	CommandSubmitResult*	outResult)
{
	// cast to the right type:
	// for compare swap commands we are really dealing with a 'CompareSwapSubmitResult'
	// but we have to override submit() with a prototype matching our superclass submit()
	CompareSwapSubmitResult* result = (CompareSwapSubmitResult*)outResult ;

	IOReturn	error		= kIOReturnSuccess ;

	if ( inParams->staleFlags & kFireWireCommandStale_Buffer )
		fSize = inParams->newBufferSize ;

	Boolean			syncFlag 	= ( inParams->flags & kFWCommandInterfaceSyncExecute ) != 0 ;
	Boolean			absFlag		= ( inParams->flags & kFireWireCommandAbsolute ) != 0 ;

	if ( inParams->staleFlags & kFireWireCommandStale )
	{
		if ( fCommand )
		{
			fCommand->release() ;
			fCommand = NULL ;
		}
		
		if ( absFlag )
			fCommand = fUserClient->createCompareAndSwapCommand(	inParams->newGeneration, 
																	inParams->newTarget, 
																	(UInt32*)(inParams+1),// cmpVal past end of param struct
																	(UInt32*)(inParams+1) + 2,// newVal past end of cmpVal
																	fSize >> 2,
																	syncFlag ? NULL : & IOFWUserCompareSwapCommand::asyncCompletion,
																	this ) ;
		else
		{
			fCommand = fUserClient->getOwner()->createCompareAndSwapCommand( inParams->newTarget, 
																			(UInt32*)(inParams+1),// cmpVal past end of param struct
																			(UInt32*)(inParams+1) + 2,// newVal past end of cmpVal
																			fSize >> 2,
																			syncFlag ? NULL : & IOFWUserCompareSwapCommand::asyncCompletion,
																			this,
																			inParams->newFailOnReset ) ;
			if ( fCommand )
				fCommand->setGeneration( inParams->newGeneration ) ;
		}
		
		if ( !fCommand )
			error = kIOReturnNoMemory ;

	}
	else //if ( inParams->staleFlags )
	{
		if ( absFlag )
			error =((IOFWCompareAndSwapCommand*)fCommand)->reinit( inParams->newGeneration, 
					inParams->newTarget, (UInt32*)(inParams+1),// cmpVal past end of param struct
					(UInt32*)(inParams+1) + 2,// newVal past end of cmpVal
					fSize >> 2, syncFlag ? NULL : & IOFWUserCompareSwapCommand::asyncCompletion, this ) ;
		else
		{
			error = ((IOFWCompareAndSwapCommand*)fCommand)->reinit( inParams->newTarget, 
					(UInt32*)(inParams+1),// cmpVal past end of param struct
					(UInt32*)(inParams+1) + 2,// newVal past end of cmpVal
					fSize >> 2, syncFlag ? NULL : & IOFWUserCompareSwapCommand::asyncCompletion, this,
					inParams->newFailOnReset ) ;
			fCommand->setGeneration( inParams->newGeneration ) ;
		}
	}

	if ( !error )
	{
		error = fCommand->submit() ;
							
		if (syncFlag)
		{
			result->result 					= fCommand->getStatus() ;
			result->bytesTransferred		= fCommand->getBytesTransferred() ;
			result->lockInfo.didLock		= ((IOFWCompareAndSwapCommand*)fCommand)->locked( (UInt32*) & result->lockInfo.value ) ;
		}	
	}
	
	return error ;
}

void
IOFWUserCompareSwapCommand::asyncCompletion(
	void *					refcon, 
	IOReturn 				status, 
	IOFireWireNub *			device, 
	IOFWCommand *			fwCmd)
{
	IOFWUserCompareSwapCommand*	cmd = (IOFWUserCompareSwapCommand*)refcon ;

	if (refcon && cmd->fAsyncRef[0] )
	{
		CompareSwapSubmitResult sendResult ;
		
		sendResult.result = status ;
		sendResult.bytesTransferred = cmd->fCommand->getBytesTransferred() ;
		sendResult.lockInfo.didLock	= (Boolean)((IOFWCompareAndSwapCommand*)cmd->fCommand)->locked( (UInt32*) & sendResult.lockInfo.value ) ;

#if IOFIREWIREDEBUG > 0
		IOReturn result =
#endif		
		IOFireWireUserClient::sendAsyncResult( cmd->fAsyncRef, status, (void**)& sendResult, sizeof(sendResult)/sizeof(UInt32) ) ;	// +1 to round up
		IOFireWireUserClientLogIfErr_( result, "IOFireWireUserClient::asyncCompareSwapCommandCompletion: sendAsyncResult returned error 0x%08x\n", result) ;
	}
}
