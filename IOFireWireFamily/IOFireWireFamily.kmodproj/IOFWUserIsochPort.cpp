/*
* Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
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
/*
*  IOFWUserLocalIsochPort.cpp
*  IOFireWireFamily
*
*  Created by NWG on Tue Mar 20 2001.
*  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
*
*/

// public
#import <IOKit/firewire/IOFireWireDevice.h>
#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOFWLocalIsochPort.h>
#import <IOKit/firewire/IOFWDCLTranslator.h>
#import <IOKit/firewire/IOFWDCLPool.h>
#import <IOKit/firewire/IOFWDCL.h>
#import <IOKit/IOBufferMemoryDescriptor.h>

// protected
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "IOFireWireUserClient.h"
#import "IOFWUserIsochPort.h"

#if 0
// DEBUG
class DebugThing
{
	public :
		natural_t * asyncRef ;
		IOFWUserLocalIsochPort * port; 
} ;
#endif

// ============================================================
// utility functions
// ============================================================

// static in IOFWUtils.cpp, shouldn't be included in IOFWUtils.h
extern bool findOffsetInRanges ( IOVirtualAddress address, unsigned rangeCount, IOVirtualRange ranges[], IOByteCount & outOffset ) ;

static bool
getDCLDataBuffer(
	const DCLCommand *			dcl,
	IOVirtualAddress &			outDataBuffer,
	IOByteCount &				outDataLength )
{
	Boolean	result = false ;

	switch ( dcl->opcode & ~kFWDCLOpFlagMask )
	{
		case kDCLSendPacketStartOp:
		case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			outDataBuffer		= (IOVirtualAddress)( (DCLTransferPacket*)dcl )->buffer ;
			outDataLength		= ( (DCLTransferPacket *)dcl )->size ;
			result = true ;
			break ;
			
		case kDCLPtrTimeStampOp:
			outDataBuffer		= (IOVirtualAddress)( (DCLPtrTimeStamp *) dcl )->timeStampPtr ; 
			outDataLength		= sizeof ( *( ( (DCLPtrTimeStamp *)dcl )->timeStampPtr) ) ;
			result = true ;
			break ;

		default:
			break ;
	}
	
	return result ;
}

static void
setDCLDataBuffer (
		DCLCommand*					inDCL,
		IOVirtualAddress			inDataBuffer,
		IOByteCount					inDataLength )
{
	switch(inDCL->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			((DCLTransferPacket*)inDCL)->buffer		= (void*) inDataBuffer ;
			((DCLTransferPacket*)inDCL)->size 		= inDataLength ;
			break ;
			
		case kDCLSendBufferOp:
		case kDCLReceiveBufferOp:
			//zzz what should I do here?
			break ;

		case kDCLPtrTimeStampOp:
			((DCLPtrTimeStamp*)inDCL)->timeStampPtr = (UInt32*)inDataBuffer ;
			break ;

		default:
			break ;
	}
	
}

static IOByteCount
getDCLSize ( DCLCommand* dcl )
{
	IOByteCount result = 0 ;

	switch(dcl->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			result = sizeof(DCLTransferPacket) ;
			break ;
			
		case kDCLSendBufferOp:
		case kDCLReceiveBufferOp:
			result = sizeof(DCLTransferBuffer) ;
			break ;

		case kDCLCallProcOp:
			result = sizeof(DCLCallProc) ;
			break ;
			
		case kDCLLabelOp:
			result = sizeof(DCLLabel) ;
			break ;
			
		case kDCLJumpOp:
			result = sizeof(DCLJump) ;
			break ;
			
		case kDCLSetTagSyncBitsOp:
			result = sizeof(DCLSetTagSyncBits) ;
			break ;
			
		case kDCLUpdateDCLListOp:
			result = sizeof(DCLUpdateDCLList) ;
			break ;

		case kDCLPtrTimeStampOp:
			result = sizeof(DCLPtrTimeStamp) ;
		
		case kDCLSkipCycleOp:
			result = sizeof(DCLCommand) ;
	}
	
	return result ;
}

#if 0
static bool
findOffsetIndexInRanges ( 
		IOVirtualAddress 		address, 
		unsigned 				rangeCount, 
		IOVirtualRange 			ranges[], 
		unsigned & 				outIndex, 
		unsigned & 				hint )
{
	if ( hint > 0 )
		--hint ;
	
	unsigned index = hint ;

	do
	{
		if ( ( ranges[ index ].address <= address ) && ( ( ranges[ index ].address + ranges[ index ].length ) > address ) )
		{
			outIndex = index ;
			hint = index ;
			
			return true ;
		}

		index = ( index + 1 ) % rangeCount ;
		
	} while ( index != hint ) ;

	return false ;
}
#endif

#pragma mark -

#undef super
#define super IOFWLocalIsochPort

OSDefineMetaClassAndStructors ( IOFWUserLocalIsochPort, super )
#if 0
{}
#endif

#if IOFIREWIREDEBUG > 0
bool
IOFWUserLocalIsochPort :: serialize( OSSerialize * s ) const
{
	const OSString * keys[ 1 ] =
	{
		OSString::withCString( "program" )
	} ;
	
	const OSObject * objects[ 1 ] =
	{
		fProgram ? (const OSObject*)fProgram : (const OSObject*)OSString::withCString( "(null)" )
	} ;
	
	OSDictionary * dict = OSDictionary::withObjects( objects, keys, sizeof( keys )/sizeof( OSObject* ) ) ;
	
	if ( !dict )
		return false ;
	
	bool result = dict->serialize( s ) ;
	dict->release() ;
	
	return result ;
}
#endif // IOFIREWIREDEBUG > 0

void
IOFWUserLocalIsochPort :: free()
{
	// release DCL pool (if we have one)
	if ( fDCLPool )
	{
		fDCLPool->release() ;
		fDCLPool = NULL ;
	}

	// release fProgramBuffer (if we have one)
	delete [] (UInt8*)fProgramBuffer ;
	fProgramBuffer = NULL ;

	if ( fLock )
	{
		IORecursiveLockFree( fLock ) ;
	}
	
	delete[] fDCLTable ;
	fDCLTable = NULL ;
	
	super :: free() ;
}

bool
IOFWUserLocalIsochPort :: initWithUserDCLProgram ( 
		AllocateParams * 			params,
		IOFireWireUserClient & 		userclient,
		IOFireWireController &		controller )
{
	
// sanity checking
	
	if ( params->programExportBytes == 0 )
	{
		ErrorLog ( "No program!" ) ;
		return false ;
	}
	
	fLock = IORecursiveLockAlloc () ;
	if ( ! fLock )
	{
		ErrorLog ( "Couldn't allocate recursive lock\n" ) ;
		return false ;
	}

// init easy params

	fUserObj = params->userObj ;
	fUserClient = & userclient ;
	fDCLPool = NULL ;
	fProgramCount = 0;
	fStarted = false ;

	IOReturn error = kIOReturnSuccess ;
	
// get user program ranges:

	IOVirtualRange * bufferRanges = new IOVirtualRange[ params->bufferRangeCount ] ;
	if ( !bufferRanges )
	{
		error = kIOReturnNoMemory ;
	}
	
	if ( !error )
	{
		error = fUserClient->copyUserData(	(IOVirtualAddress)params->bufferRanges, (IOVirtualAddress)bufferRanges, 
											sizeof ( IOVirtualRange ) * params->bufferRangeCount ) ;
	}
	
// create descriptor for program buffers

	IOMemoryDescriptor * bufferDesc = NULL ;
	if ( ! error )
	{
		{
			IOByteCount length = 0 ;
			for ( unsigned index = 0; index < params->bufferRangeCount; ++index )
			{
				length += bufferRanges[ index ].length ;
			}			
		}
	
		bufferDesc = IOMemoryDescriptor :: withRanges (	bufferRanges, params->bufferRangeCount, kIODirectionOutIn, 
															fUserClient->getOwningTask() ) ;
		if ( ! bufferDesc )
			error = kIOReturnNoMemory ;
		else
			error = bufferDesc->prepare() ;
	}
	
// create map for buffers; we will need to get a virtual address for them

	IOMemoryMap * bufferMap = NULL ;
	if ( !error )
	{
		bufferMap = bufferDesc->map() ;
		if ( !bufferMap )
		{
			DebugLog( "Couldn't map program buffers\n" ) ;
			error = kIOReturnVMError ;
		}
		
		bufferDesc->release() ;
	}
	
	IOMemoryDescriptor * userProgramExportDesc = NULL ;
	if ( !error )
	{
		userProgramExportDesc = IOMemoryDescriptor::withAddress( 
														params->programData, 
														params->programExportBytes, 
														kIODirectionOut, 
														fUserClient->getOwningTask() ) ;
	
	}

	// get map of program export data
	if ( userProgramExportDesc )
	{
		error = userProgramExportDesc->prepare() ;
	}
	
	if ( !error )	
	{
		DCLCommand * opcodes = NULL ;
		switch ( params->version )
		{
			case 0 :

				error = importUserProgram( userProgramExportDesc, params->bufferRangeCount, bufferRanges, bufferMap ) ;
				ErrorLogCond( error, "importUserProgram returned %x\n", error ) ;

				if ( ! error )
				{
					opcodes = (DCLCommand*)fProgramBuffer ;
				}
				
				break ;
			
			case 1 :

				fDCLPool = fUserClient->getOwner()->getBus()->createDCLPool() ;
				
				if ( ! fDCLPool )
				{
					error = kIOReturnNoMemory ;
				}

				if ( !error )
				{
					error = fDCLPool->importUserProgram( userProgramExportDesc, params->bufferRangeCount, bufferRanges, bufferMap ) ;
				}
				
				fProgramBuffer = new UInt8[ sizeof( DCLNuDCLLeader ) ] ;
				{
					DCLNuDCLLeader * leader = (DCLNuDCLLeader*)fProgramBuffer ;
					{
						leader->pNextDCLCommand = NULL ;	// unused - always NULL
						leader->opcode = kDCLNuDCLLeaderOp ;
						leader->program = fDCLPool ;
					}
					
					opcodes = (DCLCommand*)leader ;
				}
				
				break ;
			
			default :
			
				ErrorLog ( "unsupported DCL program type\n" ) ;
				error = kIOReturnBadArgument ;
				
				break ;
		}
		
		ErrorLogCond( !opcodes, "Couldn't get opcodes\n" ) ;
		
		IODCLProgram * program = NULL ;
		
		if ( opcodes )
		{
//			IOFWLocalIsochPort::printDCLProgram( opcodes ) ;
		
			IOFireWireBus::DCLTaskInfoAux	infoAux ;
			{
				infoAux.version = 2 ;

				infoAux.u.v2.bufferMemoryMap = bufferMap ;
				infoAux.u.v2.workloop = params->options & kFWIsochPortUseSeparateKernelThread ? createRealtimeThread() : NULL ;
				infoAux.u.v2.options = params->options ;
			}
						
			IOFireWireBus::DCLTaskInfo info = { 0, 0, 0, 0, 0, 0, & infoAux } ;
			
			program = fUserClient->getOwner()->getController()->getLink()->createDCLProgram(	params->talking,
																								opcodes,
																								& info,
																								params->startEvent, 
																								params->startState,
																								params->startMask ) ;

			bufferMap->release() ;		// retained by DCL program
			bufferMap = NULL ;
			
			if (  infoAux.u.v2.workloop )
			{
				// If we created a custom workloop, it will be retained by the program...
				// We can release our reference...
				infoAux.u.v2.workloop->release() ;
			}
			
			DebugLogCond( !program, "createDCLProgram returned nil\n" ) ;
		}

		if ( program )
		{
			if ( ! super::init( program, & controller ) )
			{
				ErrorLog ( "IOFWUserIsochPort :: init failed\n" ) ;
				error = kIOReturnError ;
			}
		
			program->setForceStopProc( IOFireWireUserClient::s_IsochChannel_ForceStopHandler, 0, NULL ) ;
					
//			program->release() ;
//			program = NULL ;
		}
		else
		{
			DebugLog ( "Couldn't create DCL program\n" ) ;
			error = kIOReturnNoMemory ;
		}
		
		userProgramExportDesc->complete() ;
		userProgramExportDesc->release() ;
		userProgramExportDesc = NULL ;
	}
	
	delete [] bufferRanges ;
	
	InfoLog( "-IOFWUserLocalIsochPort :: initWithUserDCLProgram error=%x (build date "__TIME__" "__DATE__")\n", error ) ;

	return ( ! error ) ;
}


IOReturn
IOFWUserLocalIsochPort :: importUserProgram (
		IOMemoryDescriptor *		userExportDesc,
		unsigned 					userBufferRangeCount, 
		IOVirtualRange				userBufferRanges[],
		IOMemoryMap *				bufferMap )
{	
	IOReturn error = kIOReturnSuccess ;
	
	if ( ! error )
	{
		fProgramBuffer = new UInt8[ userExportDesc->getLength() ] ;
		if ( !fProgramBuffer )
		{
			error = kIOReturnNoMemory ;
		}
	}
	
	// copy user program to kernel buffer:
	if ( !error )
	{
		unsigned byteCount = userExportDesc->readBytes( 0, (void*)fProgramBuffer, userExportDesc->getLength() ) ;
		if ( byteCount < userExportDesc->getLength() )
		{
			error = kIOReturnVMError ;
		}
	}

	// convert next pointers...
	// we can set the next pointer of each kernel DCL to the DCL immediately succeeding it
	// because we copied the DCLs from user space in the order they're linked together..
	if ( ! error )
	{
		for( DCLCommand * dcl = (DCLCommand*)fProgramBuffer; dcl != NULL && !error; dcl = dcl->pNextDCLCommand )
		{					
			{
				unsigned opcode = dcl->opcode & ~kFWDCLOpFlagMask ;
				if ( opcode > 15 && opcode != 20 )
				{
					ErrorLog("found invalid DCL in export data\n") ;
					error = kIOFireWireBogusDCLProgram ;
					break ;
				}
			}
			
			unsigned size = getDCLSize( dcl ) ;
			
			{
				// Convert the DCL data pointers from user space to kernel space:
				// (new style programs will have this step performed automatically when the program
				// is imported to the kernel...)
				IOVirtualRange tempRange ;
				if ( getDCLDataBuffer ( dcl, tempRange.address, tempRange.length ) )
				{
					if ( tempRange.address != NULL && tempRange.length > 0 )
					{
						IOByteCount offset ;
						if ( ! findOffsetInRanges (	tempRange.address, userBufferRangeCount, userBufferRanges, offset ) )
						{
							DebugLog( "IOFWUserLocalIsochPort::initWithUserDCLProgram: couldn't find DCL data buffer in buffer ranges") ;
						}
						
						// set DCL's data pointer to point to same memory in kernel address space
						setDCLDataBuffer ( dcl, bufferMap->getVirtualAddress() + offset, tempRange.length ) ;					
					}
				}
			}
			
			switch( dcl->opcode & ~kFWDCLOpFlagMask )
			{
				case kDCLUpdateDCLListOp:
					size += sizeof( DCLCommand * ) * ((DCLUpdateDCLList*)dcl)->numDCLCommands ;
					error = convertToKernelDCL( ( DCLUpdateDCLList * ) dcl ) ;
					
//					for( unsigned index=0; index < ((DCLUpdateDCLList*)dcl)->numDCLCommands; ++index )  // nnn debug only
//					{
//						if ( (IOVirtualAddress)dcl < fProgramBuffer
//							|| (IOVirtualAddress)((DCLUpdateDCLList*)dcl)->dclCommandList[ index ] > ( fProgramBuffer + userExportDesc->getLength() ) )
//							panic("oops!\n") ;
//					}
					
					break ;
				case kDCLCallProcOp :
					size += sizeof( OSAsyncReference ) ;
					error = convertToKernelDCL ( (DCLCallProc*) dcl ) ;
					
//					if ( (IOVirtualAddress)dcl < fProgramBuffer
//						|| (IOVirtualAddress)((DCLCallProc*)dcl)->procData > (fProgramBuffer + userExportDesc->getLength() ) ) // nnn debug only
//					{
//						panic("oops3\n") ;
//					}
						
					break ;
				case kDCLJumpOp:
					error = convertToKernelDCL( ( DCLJump * ) dcl ) ;
					
//					if ( ((DCLJump*)dcl)->pJumpDCLLabel )
//					{
//						if ( (IOVirtualAddress)dcl < fProgramBuffer
//							|| (IOVirtualAddress)((DCLJump*)dcl)->pJumpDCLLabel > ( fProgramBuffer + userExportDesc->getLength() ) )  // nnn debug only
//						{
//							panic("oops2\n") ;
//						}
//					}
					
					break ;
			}
			
			if ( dcl->pNextDCLCommand )
				dcl->pNextDCLCommand = (DCLCommand*)( ((UInt8*)dcl) + size ) ;

			++fProgramCount ;			
		}
		
		fDCLTable = new (DCLCommand*)[ fProgramCount ] ;
		
		InfoLog( "made DCL table, %d entries\n", fProgramCount ) ;
		
		if ( !fDCLTable )
			error = kIOReturnNoMemory ;
		
		if ( !error )
		{
			unsigned index = 0 ;
			for( DCLCommand * dcl = (DCLCommand*)fProgramBuffer; dcl != NULL; dcl = dcl->pNextDCLCommand )
			{
				if ( index >= fProgramCount )
					panic("dcl table out of bounds\n") ;
					
				fDCLTable[ index++ ] = dcl ;
			}
		}
	}

	return error ;
}

#if 0
IOReturn
IOFWUserLocalIsochPort :: releasePort ()
{
//	lock() ;
//	
//	if ( fBufferDescPrepared )
//	{
//		fBufferDesc->complete() ;
//		fBufferDescPrepared = false ;
//	}
//
//	unlock() ;

	return super :: releasePort () ;
}
#endif

IOReturn
IOFWUserLocalIsochPort::start()
{
	// calling fProgram->start() takes the isoch workloop lock, 
	// so we don't need to call lock() here...
//	lock() ;
	
	IOReturn error = super::start() ;
	
	fStarted = (!error) ;
	
//	unlock() ;
	
	return error ;
}

IOReturn
IOFWUserLocalIsochPort :: stop ()
{
	// we are sending a stop token, but take isoch workloop lock to make sure all
	// callbacks coming from FWIM have already been cleared out.

//	IOFireWireLink * link = fUserClient->getOwner()->getController()->getLink() ;	
//	link->closeIsochGate () ;	// nnn need replacement?

//	lock() ;

	IOReturn error ;
	
	// we ignore any errors from above here because we need to call super :: stop() always
	error = super :: stop() ;
	
	if ( fStarted )
	{
		error = IOFireWireUserClient::sendAsyncResult( fStopTokenAsyncRef, kIOFireWireLastDCLToken, NULL, 0 ) ;
		
		fStarted = false ;
	}

//	unlock() ;
	
//	link->openIsochGate () ;

 	return error ;
}

void
IOFWUserLocalIsochPort :: s_dclCallProcHandler( DCLCallProc * dcl )
{
#if 0
#if IOFIREWIREUSERCLIENTDEBUG > 0
	IOFWUserLocalIsochPort * me = (IOFWUserLocalIsochPort *) holder->obj ;

	DebugLog("+IOFWUserLocalIsochPort :: s_dclCallProcHandler, holder=%p, (holder->asyncRef)[0]=0x%x\n", holder, (holder->asyncRef)[0]) ;

	me->fUserClient->getStatistics()->getIsochCallbackCounter()->addValue( 1 ) ;
#endif
#endif
	
	if ( dcl->procData )
	{
#if 0
// DEBUG
		DebugThing * debugThing = (DebugThing*)dcl->procData ;
		IOFireWireUserClient::sendAsyncResult( (natural_t*)debugThing->asyncRef, kIOReturnSuccess, NULL, 0 ) ;
		DebugLog("send callback port=%p\n", debugThing->port ) ;
#else
		IOFireWireUserClient::sendAsyncResult( (natural_t*)dcl->procData, kIOReturnSuccess, NULL, 0 ) ;
#endif
	}	
}

void
IOFWUserLocalIsochPort :: s_nuDCLCallout( void * refcon )
{
	natural_t * asyncRef = (natural_t *)refcon ;
 
	IOFireWireUserClient :: sendAsyncResult( asyncRef, kIOReturnSuccess, NULL, 0 ) ;
}

IOReturn
IOFWUserLocalIsochPort::setAsyncRef_DCLCallProc( OSAsyncReference asyncRef )
{
	// set up stop token async ref
	bcopy( asyncRef, fStopTokenAsyncRef, sizeof( OSAsyncReference ) ) ;

	// walk through DCL program and set mach port
	// for all callproc DCLs
	if ( fDCLPool )
	{
		const OSArray * program = fDCLPool->getProgramRef() ;
		for( unsigned index = 0, count = program->getCount(); index < count; ++index )
		{
			IOFWDCL * dcl = reinterpret_cast< IOFWDCL * >( program->getObject( index ) ) ;
			if ( dcl->getCallback() )
			{
				natural_t * dclAsyncRef = (natural_t*)dcl->getRefcon() ;
				if ( asyncRef )
				{
					bcopy( asyncRef, dclAsyncRef, sizeof( natural_t ) * kIOAsyncReservedCount ) ;
				}
			}
			
		}
		
		program->release() ;
	}
	else
	{
		for( unsigned index=0; index < fProgramCount; ++index )
		{
			DCLCommand * dcl = fDCLTable[ index ] ;
			if ( ( dcl->opcode & ~kFWDCLOpFlagMask ) == kDCLCallProcOp )
			{
#if 0
// DEBUG
				if ( ((DCLCallProc*)dcl)->proc && ((DCLCallProc*)dcl)->procData )
				{
					((DebugThing*)((DCLCallProc*)dcl)->procData)->asyncRef[0] = asyncRef[0] ;
				}
#else
				{
					((natural_t*)((DCLCallProc*)dcl)->procData)[ 0 ] = asyncRef[ 0 ] ;
				}
#endif
			}
		
			dcl = dcl->pNextDCLCommand ;
		}
	}
	
	return kIOReturnSuccess ;
}

IOReturn
IOFWUserLocalIsochPort :: modifyJumpDCL ( UInt32 inJumpDCLCompilerData, UInt32 inLabelDCLCompilerData)
{
	if ( !fProgram )
	{
		ErrorLog("no program!\n") ;
		return kIOReturnError ;
	}

	--inJumpDCLCompilerData ;
	--inLabelDCLCompilerData ;

	// be sure opcodes exist
	if ( inJumpDCLCompilerData > fProgramCount || inLabelDCLCompilerData > fProgramCount )
	{
		DebugLog( "IOFWUserLocalIsochPort::modifyJumpDCL: DCL index (inJumpDCLCompilerData=%lu, inLabelDCLCompilerData=%lu) past end of lookup table (length=%u)\n", 
				inJumpDCLCompilerData,
				inLabelDCLCompilerData,
				fProgramCount ) ;
		return kIOReturnBadArgument ;
	}
		
	DCLJump * jumpDCL = (DCLJump *) fDCLTable[ inJumpDCLCompilerData ] ;
	DCLLabel * labelDCL = (DCLLabel *) fDCLTable[ inLabelDCLCompilerData ] ;

	// make sure we're modifying a jump and that it's pointing to a label
	if ( ( jumpDCL->opcode & ~kFWDCLOpFlagMask ) != kDCLJumpOp || ( labelDCL->opcode & ~kFWDCLOpFlagMask ) != kDCLLabelOp )
	{
		DebugLog("IOFWUserLocalIsochPort::modifyJumpDCL: modifying non-jump (%p, %d) or pointing jump to non-label (%p, %d)\n", jumpDCL, (int)inJumpDCLCompilerData, jumpDCL->pJumpDCLLabel, (int)inLabelDCLCompilerData ) ;
		return kIOReturnBadArgument ;
	}

	// point jump to label
	jumpDCL->pJumpDCLLabel = labelDCL ;

//	lock() ;
	fProgram->closeGate() ;

	IOReturn error = notify ( kFWDCLModifyNotification, (DCLCommand**) & jumpDCL, 1 ) ;

//	unlock() ;
	fProgram->openGate() ;
	
	return error ;
}

IOReturn
IOFWUserLocalIsochPort :: modifyDCLSize ( UInt32 dclCompilerData, IOByteCount newSize )
{
	return kIOReturnUnsupported ;

// to fix?
#if 0
	--dclCompilerData ;

	// be sure opcodes exist
	if ( dclCompilerData > fUserToKernelDCLLookupTableLength )
	{
		DebugLog("IOFWUserLocalIsochPort::modifyJumpDCLSize: DCL index (dclCompilerData=%lu) past end of lookup table (length=%lu)\n", 
				dclCompilerData, fUserToKernelDCLLookupTableLength ) ;
		return kIOReturnBadArgument ;
	}
		
	DCLTransferPacket*	dcl = (DCLTransferPacket*)( fUserToKernelDCLLookupTable[ dclCompilerData ] ) ;
	IOReturn			result = kIOReturnSuccess ;
	lock() ;
	
	if (fPort)
		result = ((IOFWLocalIsochPort*)fPort)->notify(kFWDCLModifyNotification, (DCLCommand**)&dcl, 1) ;

	unlock() ;
	return result ;
#endif	
}

IOReturn
IOFWUserLocalIsochPort :: convertToKernelDCL ( 
		DCLUpdateDCLList * 		dcl )
{
	// when the program was imported to the kernel, the update list was placed in
	// the DCL program export buffer immediately after the DCL
	dcl->dclCommandList = (DCLCommand**)(dcl + 1) ;
	
	for( unsigned index = 0 ; index < dcl->numDCLCommands; ++index )
	{
		dcl->dclCommandList[ index ] = (DCLCommand*)( fProgramBuffer + (UInt32)dcl->dclCommandList[ index ] ) ;
		{
			unsigned opcode = dcl->dclCommandList[ index ]->opcode & ~kFWDCLOpFlagMask ;
			if ( opcode > 15 && opcode != 20 )
			{
				panic("invalid opcode\n") ;
			}
		}
	}

	return kIOReturnSuccess ;
}

IOReturn
IOFWUserLocalIsochPort :: convertToKernelDCL ( 
		DCLJump * 				dcl )
{
	// the label field contains a an offset from the beginning of fProgramBuffer
	// where the label can be found
	dcl->pJumpDCLLabel = (DCLLabel*)( fProgramBuffer + (UInt32)dcl->pJumpDCLLabel ) ;

	if ( ( dcl->pJumpDCLLabel->opcode & ~kFWDCLOpFlagMask ) != kDCLLabelOp )
	{
		dcl->pJumpDCLLabel = NULL ;
		DebugLog( "Jump %p pointing to non-label %p\n", dcl, dcl->pJumpDCLLabel ) ;
//		return kIOFireWireBogusDCLProgram ;
	}

	return kIOReturnSuccess ;
}

IOReturn
IOFWUserLocalIsochPort :: convertToKernelDCL ( 
	DCLCallProc * dcl)
{
	if ( !dcl->proc )
		return NULL ;
	
	natural_t * asyncRef = (natural_t *)(dcl + 1 ) ;
	
	asyncRef[0] = 0 ;
	asyncRef[ kIOAsyncCalloutFuncIndex ] = (natural_t)dcl->proc ;
	asyncRef[ kIOAsyncCalloutRefconIndex ] = (natural_t)dcl->procData ;
	
	dcl->proc				= (DCLCallCommandProc*) & s_dclCallProcHandler ;

#if 0
// DEBUG
	DebugThing * debugThing = new DebugThing ;
	dcl->procData			= debugThing ;
	debugThing->asyncRef = asyncRef ;
	debugThing->port = this ;
#else
	dcl->procData			= (UInt32) asyncRef ;
#endif
		
	return kIOReturnSuccess ;
}

void
IOFWUserLocalIsochPort :: exporterCleanup ()
{
	stop() ;
	releasePort() ;
}

IOReturn
IOFWUserLocalIsochPort :: userNotify (
		UInt32			notificationType,
		UInt32			numDCLs,
		void *			data,
		IOByteCount		dataSize )
{
	InfoLog("+IOFWUserLocalIsochPort :: userNotify, numDCLs=%ld\n", numDCLs ) ;
	if ( __builtin_expect( numDCLs > 64, false ) )
	{
		return kIOReturnBadArgument ;
	}

	IOFWDCL * 			dcls[ numDCLs ] ;
	const OSArray *		program 		= fDCLPool->getProgramRef() ;
	unsigned 			programLength	= program->getCount() ;
	IOReturn			error			= kIOReturnSuccess ;
	
	switch( (IOFWDCLNotificationType)notificationType )
	{
		case kFWNuDCLModifyNotification :
		{
			IOMemoryMap * bufferMap = fProgram->getBufferMap() ;
			for( unsigned index=0; index < numDCLs; ++index )
			{
				unsigned dclIndex = *(unsigned*)data - 1 ;
				if ( dclIndex >= programLength )
				{
					DebugLog("out of range DCL dclIndex=%d, programLength=%d\n", dclIndex, programLength ) ;
					error = kIOReturnBadArgument ;
				}
				else
				{
					dcls[ index ] = (IOFWDCL*)program->getObject( dclIndex ) ;
					
					data = (UInt8*)data + sizeof( unsigned ) ;
					IOByteCount dataSize ;
					
					error = dcls[ index ]->importUserDCL( (UInt8*)data, dataSize, bufferMap, program ) ;

					// if there is no branch set, make sure the DCL "branches" to the 
					// dcl that comes next in the program if there is one...
					if ( dclIndex + 1 < programLength && !dcls[ index ]->getBranch() )
					{
						dcls[ index ]->setBranch( (IOFWDCL*)program->getObject( dclIndex + 1 ) ) ;
					}
					
					data = (UInt8*)data + dataSize ;
				}

				if ( error )
				{
					break ;
				}
			}
			
			break ;
		}
		
		case kFWNuDCLModifyJumpNotification :
		{
			unsigned * dclIndexTable = (unsigned*)data ;
			
			// subtract 1 from each index in our list.
			// when the notification type is kFWNuDCLModifyJumpNotification, the dcl list
			// actually contains pairs of DCL indices. The first is the dcl having its branch modified,
			// the second is the index of the DCL to branch to.
			{
				unsigned index = 0 ;
				unsigned pairIndex = 0 ;

				while( pairIndex < numDCLs )
				{
					--dclIndexTable[ index ] ;
					if ( dclIndexTable[ index ] >= programLength )
					{
						DebugLog("out of range DCL index=%d, dclIndices[ index ]=%d, programLength=%d\n", index, dclIndexTable[ index ], programLength ) ;
						error = kIOReturnBadArgument ;
						break ;
					}

					dcls[ pairIndex ] = (IOFWDCL*)program->getObject( dclIndexTable[ index ] ) ;
					
					++index ;
					--dclIndexTable[ index ] ;
					
					if ( dclIndexTable[ index ] >= programLength )
					{
						DebugLog("out of range DCL index=%d, dclIndices[ index ]=%d, programLength=%d\n", index, dclIndexTable[ index ], programLength ) ;
						error = kIOReturnBadArgument ;
						break ;						
					}

					dcls[ pairIndex ]->setBranch( (IOFWDCL*)program->getObject( dclIndexTable[ index ] ) ) ;

					++index ;
					++pairIndex ;
				}
			}

			break ;
		}
		
		case kFWNuDCLUpdateNotification :
		{
			unsigned index = 0 ;
			while ( index < numDCLs )
			{
				unsigned * dclIndices = (unsigned*)data ;
				
				--dclIndices[ index ] ;
				if ( __builtin_expect( dclIndices[ index ] >= programLength, false ) )
				{
					DebugLog("out of range DCL index=%d, dclIndices[ index ]=%d, programLength=%d\n", index, dclIndices[ index ], programLength ) ;
					error = kIOReturnBadArgument ;
					break ;
				}
				
				dcls[ index ] = (IOFWDCL*)program->getObject( dclIndices[ index ] ) ;
				
				++index ;
			}

			break ;
		}
		
		default:
		{
			error = kIOReturnBadArgument ;
			DebugLog("unsupported notification type 0x%08lx\n", notificationType) ;
			break ;
		}
	}
	
	program->release() ;
	
	return error ? error : notify( (IOFWDCLNotificationType)notificationType, (DCLCommand**)dcls, numDCLs ) ;
}

IOWorkLoop *
IOFWUserLocalIsochPort :: createRealtimeThread()
{
	IOWorkLoop * workloop = IOWorkLoop::workLoop() ;
	if ( workloop )
	{
		// Boost isoc workloop into realtime range
		thread_time_constraint_policy_data_t	constraints;
		AbsoluteTime							time;
		
		nanoseconds_to_absolutetime(625000, &time);
		constraints.period = AbsoluteTime_to_scalar(&time);
		nanoseconds_to_absolutetime(60000, &time);
		constraints.computation = AbsoluteTime_to_scalar(&time);
		nanoseconds_to_absolutetime(1250000, &time);
		constraints.constraint = AbsoluteTime_to_scalar(&time);

		constraints.preemptible = TRUE;

		{
			IOThread thread;
			thread = workloop->getThread();
			thread_policy_set( thread, THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t) & constraints, THREAD_TIME_CONSTRAINT_POLICY_COUNT );			
		}
	}
	
	return workloop ;
}
