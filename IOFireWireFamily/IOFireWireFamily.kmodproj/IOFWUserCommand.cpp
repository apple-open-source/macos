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
#import <IOKit/firewire/IOFireWireController.h>

// private
#import "IOFWUserCommand.h"
#import "IOFireWireLib.h"
#import "IOFWUserVectorCommand.h"

OSDefineMetaClassAndAbstractStructors(IOFWUserCommand, OSObject)
OSDefineMetaClassAndStructors(IOFWUserReadCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserWriteCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserPHYCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserCompareSwapCommand, IOFWUserCommand)
OSDefineMetaClassAndStructors(IOFWUserAsyncStreamCommand, IOFWUserCommand)

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
			DebugLog("cancelling cmd %p\n", fCommand) ;
			fCommand->cancel( kIOReturnAborted ) ;
		}
		
		fCommand->release() ;
		fCommand = NULL;
	}

	if( fOutputArgs )
	{
		IOFree( fOutputArgs, fOutputArgsSize );
		fOutputArgs = NULL;
		fQuads = NULL ;
		fNumQuads = 0 ;
	}
		
	if (fMem)
	{
		fMem->complete() ;
		fMem->release() ;
		fMem = NULL;
	}
	
	OSObject::free() ;
}

void
IOFWUserCommand::setAsyncReference64(
	OSAsyncReference64	inAsyncRef)
{
	bcopy(inAsyncRef, fAsyncRef, sizeof(OSAsyncReference64)) ;
}

IOFWUserCommand*
IOFWUserCommand::withSubmitParams(
	const CommandSubmitParams*	params,
	const IOFireWireUserClient*			inUserClient)
{
	IOFWUserCommand*	result	= NULL ;
	
	switch ( params->type )
	{
		case kFireWireCommandType_Read:
			// fallthru
		case kFireWireCommandType_ReadQuadlet:
			result = OSTypeAlloc( IOFWUserReadCommand );
			break ;
		
		case kFireWireCommandType_Write:
			// fallthru
		case kFireWireCommandType_WriteQuadlet:
			result = OSTypeAlloc( IOFWUserWriteCommand );
			break ;
			
		case kFireWireCommandType_CompareSwap:
			result = OSTypeAlloc( IOFWUserCompareSwapCommand );
			break ;

		case kFireWireCommandType_PHY:
			result = OSTypeAlloc( IOFWUserPHYCommand );
			break ;
			
		case kFireWireCommandType_AsyncStream:
			result = OSTypeAlloc( IOFWUserAsyncStreamCommand );
			break;
		
		default:
			DebugLog( "bad command type!\n" ) ;
			break ;
	}
	
	if (result && !result->initWithSubmitParams( params, inUserClient ))
	{
		result->release() ;
		result = NULL ;
	}

	return result ;
}

bool
IOFWUserCommand::initWithSubmitParams(
	const CommandSubmitParams*	params,
	const IOFireWireUserClient*			inUserClient)
{
	fFlush = true;
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

	// tell the vector
	if( cmd->fVectorCommand )
	{
		cmd->fVectorCommand->asyncCompletion( refcon, status, device, fwCmd );
	}
	else if ( refcon && cmd->fAsyncRef[0] ) 
	{
		io_user_reference_t args[3];
		args[0] = cmd->fCommand->getBytesTransferred();
		args[1] = cmd->fCommand->getAckCode();
		args[2] = cmd->fCommand->getResponseCode();
#if IOFIREWIREDEBUG > 0
		IOReturn		error = 
#endif
		IOFireWireUserClient::sendAsyncResult64( cmd->fAsyncRef, status, args, 3 );
		
		DebugLogCond ( error, "IOFWUserCommand::asyncReadWriteCommandCompletion: sendAsyncResult64 returned error %x\n", error ) ;
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
		cmd->fOutputArgs[0] = cmd->fCommand->getAckCode();
		cmd->fOutputArgs[1] = cmd->fCommand->getResponseCode();
		// quad data is already in fOutputArgs[3] and later
		
#if IOFIREWIREDEBUG > 0
		IOReturn result =
#endif
		IOFireWireUserClient::sendAsyncResult64( cmd->fAsyncRef, status, (io_user_reference_t *)cmd->fOutputArgs, ( cmd->fCommand->getBytesTransferred() >> 2) + 2 ) ;
		DebugLogCond ( result, "IOFireWireUserClient::asyncReadQuadletCommandCompletion: sendAsyncResult64 returned error 0x%08x\n", result) ;
	}
}

#pragma mark -
// ============================================================
// IOFWUserReadCommand
// ============================================================

bool
IOFWUserReadCommand::initWithSubmitParams(
	const CommandSubmitParams*	params,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(params, inUserClient)) ;
	
	if (result)
	{
		fMem = IOMemoryDescriptor::withAddressRange( params->newBuffer, params->newBufferSize, kIODirectionIn, fUserClient->getOwningTask() ) ;
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
		{
			fMem->release() ;
			fMem = NULL;
		}
	}
	
	return result ;
}

IOReturn
IOFWUserReadCommand::submit(
	CommandSubmitParams*	params,
	CommandSubmitResult*	outResult)
{
	IOReturn	error		= kIOReturnSuccess ;
	Boolean		syncFlag 	= ( params->flags & kFWCommandInterfaceSyncExecute ) != 0 ;
	Boolean		copyFlag	= ( params->flags & kFireWireCommandUseCopy ) != 0;
	Boolean		absFlag		= ( params->flags & kFireWireCommandAbsolute ) != 0 ;
	bool		forceBlockFlag	= (params->flags & kFWCommandInterfaceForceBlockRequest) != 0;

	FWAddress target_address;
	target_address.addressLo = (UInt32)(params->newTarget & 0xffffffff);
	target_address.addressHi = (UInt16)((params->newTarget >> 32) & 0x0000ffff);
	target_address.nodeID = (UInt16)(params->newTarget >> 48);

	if ( params->staleFlags & kFireWireCommandStale_Buffer )	// do we need reevaluate our buffers?
	{
		if ( fMem )	// whatever happens, we're going to need a new memory descriptor
		{
			fMem->complete() ;
			fMem->release() ;
			fMem = NULL;
		}
		
		if ( copyFlag )	// is this command using in-line data?
		{
			if( fQuads && (fNumQuads != params->newBufferSize) || syncFlag)	// if we're executing synchronously,
																			// don't need quadlet buffer
			{
				IOFree( fOutputArgs, fOutputArgsSize );
				fOutputArgs = NULL;
				fQuads = NULL ;
				fNumQuads = 0 ;
			}
			
			if (!syncFlag)
			{
				fNumQuads = params->newBufferSize ;
				fOutputArgsSize = (params->newBufferSize + 2) * sizeof(UInt32);
				fOutputArgs = (UInt32*)IOMalloc( fOutputArgsSize );
				fQuads = fOutputArgs + 2;
			}
				
			fMem = NULL ;
		}
		else
		{
			if (fQuads)
			{
				IOFree( fOutputArgs, fOutputArgsSize );
				fOutputArgs = NULL;
				fQuads = NULL ;
				fNumQuads = 0 ;
			}
		
			if (NULL == (fMem = IOMemoryDescriptor::withAddressRange( params->newBuffer, 
													params->newBufferSize, 
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
						error = ((IOFWReadQuadCommand*)fCommand)->reinit( params->newGeneration, target_address, (UInt32*) params+1, params->newBufferSize, NULL, this ) ;
					}
					else
						error = ((IOFWReadQuadCommand*)fCommand)->reinit( target_address, (UInt32*) params+1, params->newBufferSize, NULL, this, params->newFailOnReset) ;
				}
				else
				{
					error = ((IOFWReadQuadCommand*)fCommand)->reinit( target_address,
																	   fQuads,
																	   fNumQuads,
																	   & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	   this,
																	   params->newFailOnReset) ;
				}
			else
				error = ((IOFWReadCommand*)fCommand)->reinit( target_address,
															fMem, 
															syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
															this,
															params->newFailOnReset ) ;

			fCommand->setGeneration(params->newGeneration) ;			
			DebugLogCond ( error, "IOFWUserReadCommand::submit: fCommand->reinit error=%08x\n", error) ;
		}
		else// if (params->staleFlags )
		{
			if ( copyFlag )
			{
				if (syncFlag)
				{
					if ( absFlag )
					{
						fCommand = fUserClient->createReadQuadCommand( params->newGeneration, target_address, (UInt32*) params+1, params->newBufferSize, NULL, this ) ;
					}
					else
						fCommand = fUserClient->getOwner()->createReadQuadCommand( target_address, (UInt32*) params+1, params->newBufferSize, NULL, this, params->newFailOnReset ) ;
				}
				else															   
				{
					// create a quadlet command and copy in-line quads into it.
					if ( absFlag )
					{
						fCommand = fUserClient->createReadQuadCommand( params->newGeneration, target_address, fQuads, fNumQuads, & IOFWUserCommand::asyncReadQuadletCommandCompletion, this ) ;
					}
					else
					{
						fCommand = fUserClient->getOwner()->createReadQuadCommand( target_address,
																					fQuads,
																					fNumQuads,
																					& IOFWUserCommand::asyncReadQuadletCommandCompletion,
																					this,
																					params->newFailOnReset) ;
						if ( fCommand )
							fCommand->setGeneration( params->newGeneration ) ;
					}
				}
			}
			else
			{
				// create a read command -- memory descriptor based
				if ( absFlag )
				{
					fCommand = fUserClient->createReadCommand( params->newGeneration, target_address, fMem, syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion, this ) ;
				}
				else
				{
					fCommand = fUserClient->getOwner()->createReadCommand( target_address,
																			fMem,
																			syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																			this,
																			params->newFailOnReset ) ;
					if ( fCommand )
						fCommand->setGeneration( params->newGeneration ) ;
				}
			}
			
			if (!fCommand)
				error = kIOReturnNoMemory ;
		}
	}
	
	if ( not error )
	{
		if (params->staleFlags & kFireWireCommandStale_MaxPacket)
		{
			fCommand->setMaxPacket(params->newMaxPacket) ;
		}

		if( params->staleFlags & kFireWireCommandStale_Timeout )
		{
			fCommand->setTimeout( params->timeoutDuration );
		}

		if( params->staleFlags & kFireWireCommandStale_Retries )
		{
			fCommand->setRetries( params->retryCount );
		}

		if( params->staleFlags & kFireWireCommandStale_Speed )
		{
			fCommand->setMaxSpeed( params->maxPacketSpeed );
		}

		// block or not
		fCommand->setForceBlockRequests( forceBlockFlag );

		// turn off flushing if requested
		if( !fFlush )
		{
			fCommand->setFlush( fFlush );
		}
		
		error = fCommand->submit() ;
		
		if( !fFlush )
		{
			fCommand->setFlush( true );
		}
		
		DebugLogCond ( error, "IOFWUserReadCommand::submit: fCommand->submit error=%08x\n", error ) ;
	}
						
	if( syncFlag && (outResult != NULL) )
	{
		outResult->result 			= fCommand->getStatus() ;
		outResult->bytesTransferred	= fCommand->getBytesTransferred() ;
		outResult->ackCode			= fCommand->getAckCode();
		outResult->responseCode		= fCommand->getResponseCode();
		
		// mach won't copy any of our result info out on an error, pretend everything is fine
		error = kIOReturnSuccess;
	}	

	return error ;
}

#pragma mark -
// ============================================================
// IOFWUserWriteCommand
// ============================================================
bool
IOFWUserWriteCommand::initWithSubmitParams(
	const CommandSubmitParams*	params,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(params, inUserClient)) ;

	if (result)
	{
		fMem = IOMemoryDescriptor::withAddressRange( params->newBuffer, params->newBufferSize, kIODirectionOut, fUserClient->getOwningTask() ) ;
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
		{
			fMem->release() ;
			fMem = NULL;
		}
	}
	
	return result ;
}

IOReturn
IOFWUserWriteCommand::submit(
	CommandSubmitParams*	params,
	CommandSubmitResult*	outResult)
{
	IOReturn	result		= kIOReturnSuccess;
	Boolean		syncFlag 	= (params->flags & kFWCommandInterfaceSyncExecute) != 0;
	Boolean		copyFlag	= (params->flags & kFireWireCommandUseCopy) != 0;
	Boolean		absFlag		= (params->flags & kFireWireCommandAbsolute) != 0;
	bool		forceBlockFlag	= (params->flags & kFWCommandInterfaceForceBlockRequest) != 0;
	
	FWAddress target_address;
	target_address.addressLo = (UInt32)(params->newTarget & 0xffffffff);
	target_address.addressHi = (UInt16)((params->newTarget >> 32) & 0x0000ffff);
	target_address.nodeID = (UInt16)(params->newTarget >> 48);

	if ( params->staleFlags & kFireWireCommandStale_Buffer )	// do we need reevaluate our buffers?
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
			if (NULL == (fMem = IOMemoryDescriptor::withAddressRange( params->newBuffer, 
													params->newBufferSize, 
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
				result = ((IOFWWriteQuadCommand*)fCommand)->reinit( target_address,
																	(UInt32*) params+1,
																	params->newBufferSize,
																	syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																	this,
																	params->newFailOnReset) ;
			}
			else
			{
				result = ((IOFWWriteCommand*)fCommand)->reinit( target_address,
															    fMem, 
															    syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
															    this,
															    params->newFailOnReset ) ;
			}
			
			DebugLogCond ( result, "IOFWUserWriteCommand::submit: fCommand->reinit result=%08x\n", result) ;
		}
		else
		{
			if ( copyFlag )
			{
				if ( absFlag )
					fCommand = fUserClient->createWriteQuadCommand( params->newGeneration, target_address, (UInt32*) params+1, params->newBufferSize, syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion, this ) ;
				else
				{
					fCommand = fUserClient->getOwner()->createWriteQuadCommand( target_address,
																				(UInt32*) params+1,
																				params->newBufferSize,
																				syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																				this,
																				params->newFailOnReset ) ;
					if ( fCommand )
						fCommand->setGeneration( params->newGeneration ) ;
				}
			}
			else
			{
				// create a read command -- memory descriptor based
				if ( absFlag )
					fCommand = fUserClient->createWriteCommand( params->newGeneration,
																target_address,
																fMem,
																syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																this ) ;
				else
				{
					fCommand = fUserClient->getOwner()->createWriteCommand( target_address,
																			fMem,
																			syncFlag ? NULL : & IOFWUserCommand::asyncReadWriteCommandCompletion,
																			this,
																			params->newFailOnReset ) ;
					if ( fCommand )
						fCommand->setGeneration( params->newGeneration ) ;
				}
			}
			
			if (!fCommand)
				result = kIOReturnNoMemory ;
		}
	}
	
	if ( kIOReturnSuccess == result)
	{
		if (params->staleFlags & kFireWireCommandStale_MaxPacket)
		{
			fCommand->setMaxPacket(params->newMaxPacket) ;// zzz is there any reason for us to pay
		}
			
		if( params->staleFlags & kFireWireCommandStale_Timeout )
		{
			fCommand->setTimeout( params->timeoutDuration );
		}

		if( params->staleFlags & kFireWireCommandStale_Retries )
		{
			fCommand->setRetries( params->retryCount );
		}

		if( params->staleFlags & kFireWireCommandStale_Speed )
		{
			fCommand->setMaxSpeed( params->maxPacketSpeed );
		}
	
		// block or not
		fCommand->setForceBlockRequests( forceBlockFlag );
		
		// turn off flushing if requested
		if( !fFlush )
		{
			fCommand->setFlush( fFlush );
		}
		
		result = fCommand->submit() ;
		
		if( !fFlush )
		{
			fCommand->setFlush( true );
		}

		DebugLogCond ( result, "IOFWUserReadCommand::submit: fCommand->submit result=%08x\n", result);
	}
						
	if( syncFlag && (outResult != NULL) )
	{
		outResult->result 			= fCommand->getStatus();
		outResult->bytesTransferred	= fCommand->getBytesTransferred();
		outResult->ackCode			= fCommand->getAckCode();
		outResult->responseCode		= fCommand->getResponseCode();

		// mach won't copy any of our result info out on an error, pretend everything is fine
		result = kIOReturnSuccess;
	}	
	
	return result;
}

#pragma mark -
// ============================================================
// IOFWUserPHYCommand
// ============================================================
bool
IOFWUserPHYCommand::initWithSubmitParams(
	const CommandSubmitParams*	params,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(params, inUserClient));
	
	return result;
}

IOReturn
IOFWUserPHYCommand::submit(
	CommandSubmitParams*	params,
	CommandSubmitResult*	outResult)
{
	IOReturn	result		= kIOReturnSuccess;
	Boolean		syncFlag 	= (params->flags & kFWCommandInterfaceSyncExecute) != 0;
	
	if( kIOReturnSuccess == result)
	{	
		if( fPHYCommand )
		{
			result = fPHYCommand->reinit(	params->newGeneration,
											params->data1,
											params->data2, 
											syncFlag ? NULL : & IOFWUserPHYCommand::asyncPHYCommandCompletion,
											this,
											params->newFailOnReset );
	
			DebugLogCond ( result, "IOFWUserPHYCommand::submit: fCommand->reinit result=%08x\n", result) ;
		}
		else
		{
			IOFireWireController * control = fUserClient->getOwner()->getController();
			
			fPHYCommand = control->createAsyncPHYCommand(	params->newGeneration,
															params->data1,
															params->data2, 
															syncFlag ? NULL : &IOFWUserPHYCommand::asyncPHYCommandCompletion,
															this,
															params->newFailOnReset );
			
			if( !fPHYCommand )
			{
				result = kIOReturnNoMemory;
			}
		}
	}
	
	if( kIOReturnSuccess == result )
	{
		if( params->staleFlags & kFireWireCommandStale_Timeout )
		{
			fPHYCommand->setTimeout( params->timeoutDuration );
		}

		if( params->staleFlags & kFireWireCommandStale_Retries )
		{
			fPHYCommand->setRetries( params->retryCount );
		}
	
		// turn off flushing if requested
		if( !fFlush )
		{
			fPHYCommand->setFlush( fFlush );
		}
		
		result = fPHYCommand->submit() ;
		
		if( !fFlush )
		{
			fPHYCommand->setFlush( true );
		}

		DebugLogCond ( result, "IOFWUserPHYCommand::submit: fCommand->submit result=%08x\n", result);
	}
						
	if( syncFlag && (outResult != NULL) )
	{
		outResult->result 			= fPHYCommand->getStatus();
		outResult->bytesTransferred	= 8;
		outResult->ackCode			= fPHYCommand->getAckCode();
		outResult->responseCode		= fPHYCommand->getResponseCode();

		// mach won't copy any of our result info out on an error, pretend everything is fine
		result = kIOReturnSuccess;

	}	
	
	return result;
}

void
IOFWUserPHYCommand::free()
{
	if( fPHYCommand )
	{
		IOReturn	cmdStatus = fPHYCommand->getStatus();
		if ( cmdStatus == kIOReturnBusy || cmdStatus == kIOFireWirePending )
		{
			DebugLog("cancelling cmd %p\n", fCommand);
			fPHYCommand->cancel( kIOReturnAborted );
		}
		
		fPHYCommand->release() ;
		fCommand = NULL;
	}
	
	IOFWUserCommand::free();
}

void
IOFWUserPHYCommand::asyncPHYCommandCompletion(
	void *					refcon, 
	IOReturn 				status, 
	IOFireWireBus *			bus, 
	IOFWAsyncPHYCommand *	fwCmd )
{	
	IOFWUserPHYCommand*	cmd = (IOFWUserPHYCommand*) refcon ;

	// tell the vector
	if( cmd->fVectorCommand )
	{
		cmd->fVectorCommand->asyncPHYCompletion( refcon, status, bus, fwCmd );
	}
	else if ( refcon && cmd->fAsyncRef[0] ) 
	{
		io_user_reference_t args[3];
		args[0] = 8;
		args[1] = cmd->fPHYCommand->getAckCode();
		args[2] = cmd->fPHYCommand->getResponseCode();
#if IOFIREWIREDEBUG > 0
		IOReturn		error = 
#endif
		IOFireWireUserClient::sendAsyncResult64( cmd->fAsyncRef, status, args, 3 );
		
		DebugLogCond ( error, "IOFWUserCommand::asyncReadWriteCommandCompletion: sendAsyncResult64 returned error %x\n", error ) ;
	}
}

#pragma mark -
// ============================================================
// IOFWUserCompareSwapCommand
// ============================================================
bool
IOFWUserCompareSwapCommand::initWithSubmitParams(
	const CommandSubmitParams*	params,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(params, inUserClient)) ;
	
	return result ;
}

IOReturn
IOFWUserCompareSwapCommand::submit ( CommandSubmitParams *	params, CommandSubmitResult * outResult )
{
	// cast to the right type:
	// for compare swap commands we are really dealing with a 'CompareSwapSubmitResult'
	// but we have to override submit() with a prototype matching our superclass submit()
	CompareSwapSubmitResult* result = (CompareSwapSubmitResult*)outResult ;

	IOReturn	error		= kIOReturnSuccess ;

	if ( params->staleFlags & kFireWireCommandStale_Buffer )
		fSize = params->newBufferSize ;

	Boolean			syncFlag 	= ( params->flags & kFWCommandInterfaceSyncExecute ) != 0 ;
	Boolean			absFlag		= ( params->flags & kFireWireCommandAbsolute ) != 0 ;

	FWAddress target_address;
	target_address.addressLo = (UInt32)(params->newTarget & 0xffffffff);
	target_address.addressHi = (UInt16)((params->newTarget >> 32) & 0x0000ffff);
	target_address.nodeID = (UInt16)(params->newTarget >> 48);
		
	if ( params->staleFlags & kFireWireCommandStale )
	{
		if ( fCommand )
		{
			fCommand->release() ;
			fCommand = NULL ;
		}
		
		if ( absFlag )
			fCommand = fUserClient->createCompareAndSwapCommand(	params->newGeneration, 
																	target_address, 
																	(UInt32*)(params+1),// cmpVal past end of param struct
																	(UInt32*)(params+1) + 2,// newVal past end of cmpVal
																	fSize >> 2,
																	syncFlag ? NULL : & IOFWUserCompareSwapCommand::asyncCompletion,
																	this ) ;
		else
		{
			fCommand = fUserClient->getOwner()->createCompareAndSwapCommand( target_address, 
																			(UInt32*)(params+1),// cmpVal past end of param struct
																			(UInt32*)(params+1) + 2,// newVal past end of cmpVal
																			fSize >> 2,
																			syncFlag ? NULL : & IOFWUserCompareSwapCommand::asyncCompletion,
																			this,
																			params->newFailOnReset ) ;
			if ( fCommand )
				fCommand->setGeneration( params->newGeneration ) ;
		}
		
		if ( !fCommand )
			error = kIOReturnNoMemory ;

	}
	else //if ( params->staleFlags )
	{
		if ( absFlag )
			error =((IOFWCompareAndSwapCommand*)fCommand)->reinit( params->newGeneration, 
					target_address, (UInt32*)(params+1),// cmpVal past end of param struct
					(UInt32*)(params+1) + 2,// newVal past end of cmpVal
					fSize >> 2, syncFlag ? NULL : & IOFWUserCompareSwapCommand::asyncCompletion, this ) ;
		else
		{
			error = ((IOFWCompareAndSwapCommand*)fCommand)->reinit( target_address, 
					(UInt32*)(params+1),// cmpVal past end of param struct
					(UInt32*)(params+1) + 2,// newVal past end of cmpVal
					fSize >> 2, syncFlag ? NULL : & IOFWUserCompareSwapCommand::asyncCompletion, this,
					params->newFailOnReset ) ;
			fCommand->setGeneration( params->newGeneration ) ;
		}
	}

	if ( !error )
	{
		if( params->staleFlags & kFireWireCommandStale_Timeout )
		{
			fCommand->setTimeout( params->timeoutDuration );
		}

		if( params->staleFlags & kFireWireCommandStale_Retries )
		{
			fCommand->setRetries( params->retryCount );
		}

		if( params->staleFlags & kFireWireCommandStale_Speed )
		{
			fCommand->setMaxSpeed( params->maxPacketSpeed );
		}
	
		// turn off flushing if requested
		if( !fFlush )
		{
			fCommand->setFlush( fFlush );
		}
		
		error = fCommand->submit() ;
		
		if( !fFlush )
		{
			fCommand->setFlush( true );  // always restore command to true
		}
							
		if( syncFlag && (result != NULL) )
		{
			result->result 					= fCommand->getStatus() ;
			result->bytesTransferred		= fCommand->getBytesTransferred() ;
			result->lockInfo.didLock		= ((IOFWCompareAndSwapCommand*)fCommand)->locked( (UInt32*) & result->lockInfo.value ) ;
			result->ackCode					= fCommand->getAckCode();
			result->responseCode			= fCommand->getResponseCode();

			// mach won't copy any of our result info out on an error, pretend everything is fine
			error = kIOReturnSuccess;
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
		UInt32 lock_value[2];
		lock_value[0] = 0;
		lock_value[1] = 0;
		bool locked = ((IOFWCompareAndSwapCommand*)cmd->fCommand)->locked( lock_value );
	
		UInt64 args[7];
		args[0] = (UInt64)status;
		args[1] = (UInt64)cmd->fCommand->getBytesTransferred();
		args[2] = (UInt64)cmd->fCommand->getAckCode();
		args[3] = (UInt64)cmd->fCommand->getResponseCode();
		args[4] = (UInt64)locked;
		args[5] = (UInt64)lock_value[0];
		args[6] = (UInt64)lock_value[1];
#if 0
		int i = 0;
		for( i = 0; i < 7; i++ )
		{
#ifdef __LP64__			
			IOLog( "IOFWUserCompareSwapCommand::asyncCompletion - args[%d] - %llx\n", i, args[i] );
#else
			IOLog( "IOFWUserCompareSwapCommand::asyncCompletion- args[%d] - %llx\n", i, args[i] );
#endif
		}
#endif
		
#if IOFIREWIREDEBUG > 0
		IOReturn error =
#endif	
		IOFireWireUserClient::sendAsyncResult64( cmd->fAsyncRef, status, args, 7 );
		DebugLogCond ( error, "IOFireWireUserClient::asyncCompareSwapCommandCompletion: sendAsyncResult64 returned error 0x%08x\n", error ) ;


#if 0
		CompareSwapSubmitResult sendResult ;
		
		sendResult.result = status ;
		sendResult.bytesTransferred = cmd->fCommand->getBytesTransferred() ;
		sendResult.lockInfo.didLock	= (Boolean)((IOFWCompareAndSwapCommand*)cmd->fCommand)->locked( (UInt32*) & sendResult.lockInfo.value ) ;
		sendResult.ackCode = cmd->fCommand->getAckCode();
		sendResult.responseCode = cmd->fCommand->getResponseCode();
	
#if IOFIREWIREDEBUG > 0
		IOReturn error =
#endif	
		IOFireWireUserClient::sendAsyncResult64( cmd->fAsyncRef, status, (io_user_reference_t *)& sendResult, sizeof(sendResult)/sizeof(UInt32) ) ;	// +1 to round up
		DebugLogCond ( error, "IOFireWireUserClient::asyncCompareSwapCommandCompletion: sendAsyncResult64 returned error 0x%08x\n", error ) ;
#endif

	}
}

#pragma mark -
// ============================================================
// IOFWUserAsyncStreamCommand
// ============================================================
bool
IOFWUserAsyncStreamCommand::initWithSubmitParams(
	const CommandSubmitParams*	params,
	const IOFireWireUserClient*			inUserClient)
{
	bool	result = true ;

	result = (NULL != IOFWUserCommand::initWithSubmitParams(params, inUserClient));
	
	if (result)
	{
		fMem = IOMemoryDescriptor::withAddressRange( params->newBuffer, params->newBufferSize, kIODirectionOut, fUserClient->getOwningTask() ) ;
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
		{
			fMem->release() ;
			fMem = NULL;
		}
	}

	return result;
}

IOReturn
IOFWUserAsyncStreamCommand::submit(
	CommandSubmitParams*	params,
	CommandSubmitResult*	outResult)
{
	IOReturn	result		= kIOReturnSuccess;
	Boolean		syncFlag 	= (params->flags & kFWCommandInterfaceSyncExecute) != 0;

	if ( params->staleFlags & kFireWireCommandStale_Buffer )	// IOFWUserAsyncStreamCommand
	{
		if ( fMem )	// whatever happens, we're going to need a new memory descriptor
		{
			fMem->complete() ;
			fMem->release() ;
		}
	
		if (NULL == (fMem = IOMemoryDescriptor::withAddressRange( params->newBuffer, 
												params->newBufferSize, 
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

	if ( kIOReturnSuccess == result)
	{
		if (fAsyncStreamCommand)
		{
			fAsyncStreamCommand->release() ;
			fAsyncStreamCommand = NULL ;
		}

		if (fAsyncStreamCommand)
		{
			result = ((IOFWAsyncStreamCommand*)fAsyncStreamCommand)->reinit( params->newGeneration,
																			 params->data1,				// channel
																			 params->sync,
																			 params->tag,
																			 fMem, 
																			 params->newBufferSize,
																			 params->maxPacketSpeed,
																			 syncFlag ? NULL : & IOFWUserAsyncStreamCommand::asyncStreamCommandCompletion,
																			 this,
																			 params->newFailOnReset) ;
			DebugLogCond ( result, "IOFWUserAsyncStreamCommand::submit: fCommand->reinit result=%08x\n", result) ;
		}
		else
		{
			IOFireWireController * control = fUserClient->getOwner()->getController();

		    fAsyncStreamCommand = control->createAsyncStreamCommand( params->newGeneration,
																	 params->data1,						// channel
																	 params->sync,
																	 params->tag,
																	 fMem, 
																	 params->newBufferSize,
																	 params->maxPacketSpeed,
																	 syncFlag ? NULL : & IOFWUserAsyncStreamCommand::asyncStreamCommandCompletion,
																	 this,
																	 params->newFailOnReset);
			
			if (!fAsyncStreamCommand)
				result = kIOReturnNoMemory ;
		}
	}

	
	if ( kIOReturnSuccess == result)
	{
		if( params->staleFlags & kFireWireCommandStale_Timeout )
		{
			fAsyncStreamCommand->setTimeout( params->timeoutDuration );
		}

		result = fAsyncStreamCommand->submit() ;
		
		DebugLogCond ( result, "IOFWUserAsyncStreamCommand::submit: fCommand->submit result=%08x\n", result);
	}
						
	if( syncFlag && (outResult != NULL) )
	{
		outResult->result 			= fAsyncStreamCommand->getStatus();
		outResult->responseCode		= 0;

		// mach won't copy any of our result info out on an error, pretend everything is fine
		result = kIOReturnSuccess;
	}	
	
	return result;
}

void
IOFWUserAsyncStreamCommand::free()
{
	if( fAsyncStreamCommand )
	{
		IOReturn	cmdStatus = fAsyncStreamCommand->getStatus();
		if ( cmdStatus == kIOReturnBusy || cmdStatus == kIOFireWirePending )
		{
			DebugLog("cancelling cmd %p\n", fAsyncStreamCommand);
			fAsyncStreamCommand->cancel( kIOReturnAborted );
		}
		
		fAsyncStreamCommand->release() ;
		fAsyncStreamCommand = NULL;
	}
	
	IOFWUserCommand::free();
}

void
IOFWUserAsyncStreamCommand::asyncStreamCommandCompletion(void						*refcon, 
														IOReturn					status, 
														IOFireWireBus				*bus,
														IOFWAsyncStreamCommand		*fwCmd )
{	
	IOFWUserAsyncStreamCommand*	cmd = (IOFWUserAsyncStreamCommand*) refcon ;

	if ( refcon && cmd->fAsyncRef[0] ) 
	{
		io_user_reference_t args[3];
		args[0] = 8;
#if IOFIREWIREDEBUG > 0
		IOReturn		error = 
#endif
		IOFireWireUserClient::sendAsyncResult64( cmd->fAsyncRef, status, args, 3 );
		
		DebugLogCond ( error, "IOFWUserCommand::asyncStreamCommandCompletion: sendAsyncResult64 returned error %x\n", error ) ;
	}
}
