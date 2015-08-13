/*
* Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IODMACommand.h>

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
		io_user_reference_t * asyncRef ;
		IOFWUserLocalIsochPort * port; 
} ;
#endif

// ============================================================
// utility functions
// ============================================================

// static in IOFWUtils.cpp, shouldn't be included in IOFWUtils.h
extern bool findOffsetInRanges ( mach_vm_address_t address, unsigned rangeCount, IOAddressRange ranges[], IOByteCount & outOffset ) ;

static bool
getDCLDataBuffer(
	const UserExportDCLCommand *dcl,
	mach_vm_address_t &			outDataBuffer,
	mach_vm_size_t &			outDataLength )
{
	Boolean	result = false ;

	switch ( dcl->opcode & ~kFWDCLOpFlagMask )
	{
		case kDCLSendPacketStartOp:
		//case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			outDataBuffer		= ( (UserExportDCLTransferPacket*)dcl )->buffer ;
			outDataLength		= ( (UserExportDCLTransferPacket *)dcl )->size ;
			result = true ;
			break ;
			
		case kDCLPtrTimeStampOp:
			outDataBuffer		= ( (UserExportDCLPtrTimeStamp *) dcl )->timeStampPtr ; 
			outDataLength		= sizeof ( mach_vm_address_t ) ;
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
		mach_vm_address_t			inDataBuffer,
		mach_vm_size_t				inDataLength )
{
	switch(inDCL->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		//case kDCLSendPacketWithHeaderStartOp:
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
getDCLSize ( UserExportDCLCommand* dcl )
{
	IOByteCount result = 0 ;

	switch(dcl->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		//case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			result = sizeof(UserExportDCLTransferPacket) ;
			break ;
			
		case kDCLSendBufferOp:
		case kDCLReceiveBufferOp:
			result = sizeof(UserExportDCLTransferBuffer) ;
			break ;

		case kDCLCallProcOp:
			result = sizeof(UserExportDCLCallProc) ;
			break ;
			
		case kDCLLabelOp:
			result = sizeof(UserExportDCLLabel) ;
			break ;
			
		case kDCLJumpOp:
			result = sizeof(UserExportDCLJump) ;
			break ;
			
		case kDCLSetTagSyncBitsOp:
			result = sizeof(UserExportDCLSetTagSyncBits) ;
			break ;
			
		case kDCLUpdateDCLListOp:
			result = sizeof(UserExportDCLUpdateDCLList) ;
			break ;

		case kDCLPtrTimeStampOp:
			result = sizeof(UserExportDCLPtrTimeStamp) ;
			break;
		
		case kDCLSkipCycleOp:
			result = sizeof(UserExportDCLCommand) ;
			break;
	}
	
	return result ;
}


#pragma mark -

#undef super
#define super IOFWLocalIsochPort

OSDefineMetaClassAndStructors ( IOFWUserLocalIsochPort, super )
#if 0
{}
#endif

#if IOFIREWIREDEBUG > 0
bool
IOFWUserLocalIsochPort::serialize( OSSerialize * s ) const
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
IOFWUserLocalIsochPort::free()
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
	
	super::free() ;
}

#if 0

IOReturn checkMemoryInRange( IOMemoryDescriptor * memory, UInt64 mask )
{
	IOReturn status = kIOReturnSuccess;

	if( memory == NULL )
	{
		status = kIOReturnBadArgument;
	}
	
	//
	// setup
	//
	
	bool memory_prepared = false;
	if( status == kIOReturnSuccess )
	{
		status = memory->prepare( kIODirectionInOut );
	}
	
	if( status == kIOReturnSuccess )
	{
		memory_prepared = true;
	}
	
	UInt64 length = 0;
	IODMACommand * dma_command = NULL;
	if( status == kIOReturnSuccess )
	{
		length = memory->getLength();
		dma_command = IODMACommand::withSpecification( 
												kIODMACommandOutputHost64,		// segment function
												64,								// max address bits
												length,							// max segment size
												IODMACommand::kMapped | IODMACommand::kIterateOnly,		// IO mapped & don't bounce buffer
												length,							// max transfer size
												0,								// page alignment
												NULL,							// mapper
												NULL );							// refcon
		if( dma_command == NULL )
			status = kIOReturnError;
		
	}
	
	if( status == kIOReturnSuccess )
	{
		// set memory descriptor and don't prepare it
		status = dma_command->setMemoryDescriptor( memory, false ); 
	}	

	bool dma_command_prepared = false;
	if( status == kIOReturnSuccess )
	{
		status = dma_command->prepare( 0, length, true );
	}

	if( status == kIOReturnSuccess )
	{
		dma_command_prepared = true;
	}
	
	//
	// check ranges
	//

	if( status == kIOReturnSuccess )
	{
		UInt64 offset = 0;
		while( (offset < length) && (status == kIOReturnSuccess) )
		{
			IODMACommand::Segment64 segments[10];
			UInt32 num_segments = 10;
			status = dma_command->gen64IOVMSegments( &offset, segments, &num_segments );
			if( status == kIOReturnSuccess )
			{
				for( UInt32 i = 0; i < num_segments; i++ )
				{
				//	IOLog( "checkSegments - segments[%d].fIOVMAddr = 0x%016llx, fLength = %d\n", i, segments[i].fIOVMAddr, segments[i].fLength  );
						
					if( (segments[i].fIOVMAddr & (~mask)) )
					{
						IOLog( "checkSegmentsFailed - 0x%016llx & 0x%016llx\n", segments[i].fIOVMAddr, mask );
						status = kIOReturnNotPermitted;
						break;
					}
				}
			}
		}
	}
	
	//
	// clean up
	//
	
	if( dma_command_prepared )
	{
		dma_command->complete();
		dma_command_prepared = false;
	}
		
	if( dma_command )
	{
		dma_command->clearMemoryDescriptor(); 
		dma_command->release();
		dma_command = NULL;
	}
	
	if( memory_prepared )
	{
		memory->complete();
		memory_prepared = false;
	}
	
	return status;
}

#endif

bool
IOFWUserLocalIsochPort::initWithUserDCLProgram ( 
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

	IOAddressRange * bufferRanges = new IOAddressRange[ params->bufferRangeCount ] ;
	if ( !bufferRanges )
	{
		error = kIOReturnNoMemory ;
	}
	
	if ( !error )
	{
		error = fUserClient->copyUserData(params->bufferRanges,(mach_vm_address_t)bufferRanges, sizeof ( IOAddressRange ) * params->bufferRangeCount ) ;
	}

// create descriptor for program buffers

	IOMemoryDescriptor * bufferDesc = NULL ;
	if ( ! error )
	{
		IOByteCount length = 0 ;
		for ( unsigned index = 0; index < params->bufferRangeCount; ++index )
		{
			length += bufferRanges[ index ].length ;
		}			
	
		bufferDesc = IOMemoryDescriptor::withAddressRanges (	bufferRanges, params->bufferRangeCount, kIODirectionOutIn, 
															fUserClient->getOwningTask() ) ;
		if ( ! bufferDesc )
		{
			error = kIOReturnNoMemory ;
		}
		else
		{
		
			// IOLog( "IOFWUserLocalIsochPort::initWithUserDCLProgram - checkMemoryInRange status 0x%08lx\n", checkMemoryInRange( bufferDesc, 0x000000001FFFFFFF ) );
		
			error = bufferDesc->prepare( kIODirectionPrepareToPhys32 ) ;
			
			FWTrace( kFWTIsoch, kTPIsochPortUserInitWithUserDCLProgram, (uintptr_t)(fUserClient->getOwner()->getController()->getLink()), error, length, 0 );
			
			// IOLog( "IOFWUserLocalIsochPort::initWithUserDCLProgram - prep 32 checkMemoryInRange status 0x%08lx\n", checkMemoryInRange( bufferDesc, 0x000000001FFFFFFF ) );
			
		}
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
		userProgramExportDesc = IOMemoryDescriptor::withAddressRange( 
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
			case kDCLExportDataLegacyVersion :

				error = importUserProgram( userProgramExportDesc, params->bufferRangeCount, bufferRanges, bufferMap ) ;
				ErrorLogCond( error, "importUserProgram returned %x\n", error ) ;

				if ( ! error )
				{
					opcodes = (DCLCommand*)fProgramBuffer ;
				}
				
				break ;

			case kDCLExportDataNuDCLRosettaVersion :

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
				infoAux.u.v2.options = (IOFWIsochPortOptions)params->options ;
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
				ErrorLog ( "IOFWUserIsochPort::init failed\n" ) ;
				error = kIOReturnError ;
			}
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
	
	InfoLog( "-IOFWUserLocalIsochPort::initWithUserDCLProgram error=%x (build date "__TIME__" "__DATE__")\n", error ) ;

	return ( ! error ) ;
}


IOReturn
IOFWUserLocalIsochPort::importUserProgram (
		IOMemoryDescriptor *		userExportDesc,
		unsigned 					userBufferRangeCount, 
		IOAddressRange				userBufferRanges[],
		IOMemoryMap *				bufferMap )
{	
	IOReturn error = kIOReturnSuccess ;
	UInt8 *pUserImportProgramBuffer;
	
	// Allocate a temporary buffer to hold user-space exported program.
	if ( ! error )
	{
		pUserImportProgramBuffer = new UInt8[ userExportDesc->getLength() ] ;
		if ( !pUserImportProgramBuffer )
		{
			error = kIOReturnNoMemory ;
		}
	}
	
	// copy user program to kernel buffer:
	if ( !error )
	{
		unsigned byteCount = userExportDesc->readBytes( 0, (void*)pUserImportProgramBuffer, userExportDesc->getLength() ) ;
		if ( byteCount < userExportDesc->getLength() )
		{
			error = kIOReturnVMError ;
		}
	}
	
	// Allocate the buffer for the "real" kernel DCL program.
	if ( ! error )
	{
		fProgramBuffer = new UInt8[ userExportDesc->getLength() ] ;
		if ( !fProgramBuffer )
		{
			error = kIOReturnNoMemory ;
		}
	}
	
	DCLCommand *pCurrentDCL;
	UserExportDCLCommand *pExportDCL;
	UInt32 nextUserExportDCLOffset = 0;
	DCLCommand *pLastDCL = NULL;
	unsigned size; 
	do
	{
		pExportDCL = (UserExportDCLCommand*)(pUserImportProgramBuffer + nextUserExportDCLOffset);
		pCurrentDCL = (DCLCommand*)(fProgramBuffer + nextUserExportDCLOffset);
		
		UInt32 opcode = pExportDCL->opcode & ~kFWDCLOpFlagMask;
		
		// Sanity check
		if ( opcode > 15 && opcode != 20 )
		{
			DebugLog("found invalid DCL in export data\n") ;
			error = kIOFireWireBogusDCLProgram ;
			break ;
		}
		
		size = getDCLSize( pExportDCL ) ;

		// Set the "next" pointer in the previous DCL
		if (pLastDCL != NULL)
			pLastDCL->pNextDCLCommand = pCurrentDCL;
		pLastDCL = pCurrentDCL;
		
		switch ( opcode )
		{
			
			case kDCLSendPacketStartOp:
			//case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
				{
					DCLTransferPacket *pDCLTransferPacket = (DCLTransferPacket*) pCurrentDCL; 
					pDCLTransferPacket->opcode = pExportDCL->opcode;
					pDCLTransferPacket->compilerData = 0;
					//pDCLTransferPacket->buffer - handled by calls to getDCLDataBuffer/setDCLDataBuffer, below!
					//pDCLTransferPacket->size - handled by calls to getDCLDataBuffer/setDCLDataBuffer, below!
				}
				break ;
			
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				{
					DCLTransferBuffer *pDCLTransferBuffer = (DCLTransferBuffer*) pCurrentDCL; 
					pDCLTransferBuffer->opcode = pExportDCL->opcode;
					pDCLTransferBuffer->compilerData = 0;
					//pDCLTransferBuffer->buffer - handled by calls to getDCLDataBuffer/setDCLDataBuffer, below!
					//pDCLTransferBuffer->size - handled by calls to getDCLDataBuffer/setDCLDataBuffer, below!
					pDCLTransferBuffer->packetSize = ((UserExportDCLTransferBuffer*)pExportDCL)->packetSize;
					pDCLTransferBuffer->reserved = ((UserExportDCLTransferBuffer*)pExportDCL)->reserved;
					pDCLTransferBuffer->bufferOffset = ((UserExportDCLTransferBuffer*)pExportDCL)->bufferOffset;
				}
				break ;
			
			case kDCLCallProcOp:
				{
					DCLCallProc *pDCLCallProc = (DCLCallProc*) pCurrentDCL; 
					pDCLCallProc->opcode = pExportDCL->opcode;
					pDCLCallProc->compilerData = 0;
					//pDCLCallProc->proc - handled by call to convertToKernelDCL, below
					//pDCLCallProc->procData - handled by call to convertToKernelDCL, below
					size += sizeof( uint64_t[kOSAsyncRef64Count] ) ;
					error = convertToKernelDCL( ((UserExportDCLCallProc*)pExportDCL), pDCLCallProc ) ;
				}
				break ;
			
			case kDCLLabelOp:
				{
					DCLLabel *pDCLLabel = (DCLLabel*) pCurrentDCL; 
					pDCLLabel->opcode = pExportDCL->opcode;
					pDCLLabel->compilerData = 0;
				}
				break ;
			
			case kDCLJumpOp:
				{
					DCLJump *pDCLJump = (DCLJump*) pCurrentDCL; 
					pDCLJump->opcode = pExportDCL->opcode;
					pDCLJump->compilerData = 0;
					//pDCLJump->pJumpDCLLabel - handled by call to convertToKernelDCL, below
					error = convertToKernelDCL( ((UserExportDCLJump*)pExportDCL), pDCLJump ) ;
				}
				break ;
			
			case kDCLSetTagSyncBitsOp:
				{
					DCLSetTagSyncBits *pDCLSetTagSyncBits = (DCLSetTagSyncBits*) pCurrentDCL; 
					pDCLSetTagSyncBits->opcode = pExportDCL->opcode;
					pDCLSetTagSyncBits->compilerData = 0;
					pDCLSetTagSyncBits->tagBits = ((UserExportDCLSetTagSyncBits*)pExportDCL)->tagBits;
					pDCLSetTagSyncBits->syncBits = ((UserExportDCLSetTagSyncBits*)pExportDCL)->syncBits;
				}
				break ;
			
			case kDCLUpdateDCLListOp:
				{
					DCLUpdateDCLList *pDCLUpdateDCLList = (DCLUpdateDCLList*) pCurrentDCL; 
					pDCLUpdateDCLList->opcode = pExportDCL->opcode;
					pDCLUpdateDCLList->compilerData = 0;
					//pDCLUpdateDCLList->dclCommandList - handled by call to convertToKernelDCL, below
					pDCLUpdateDCLList->numDCLCommands = ((UserExportDCLUpdateDCLList*)pExportDCL)->numDCLCommands;
					size += sizeof( mach_vm_address_t ) * ((UserExportDCLUpdateDCLList*)pExportDCL)->numDCLCommands ;
					error = convertToKernelDCL( ((UserExportDCLUpdateDCLList*)pExportDCL), pDCLUpdateDCLList ) ;
				}
				break ;
			
			case kDCLPtrTimeStampOp:
				{
					DCLPtrTimeStamp *pDCLPtrTimeStamp = (DCLPtrTimeStamp*) pCurrentDCL; 
					pDCLPtrTimeStamp->opcode = pExportDCL->opcode;
					pDCLPtrTimeStamp->compilerData = 0;
					//pDCLPtrTimeStamp->timeStampPtr - handled by calls to getDCLDataBuffer/setDCLDataBuffer, below!
				}
				break ;
			
			case kDCLSkipCycleOp:
				{
					DCLCommand *pDCLCommand = (DCLCommand*) pCurrentDCL; 
					pDCLCommand->opcode = pExportDCL->opcode;
					pDCLCommand->compilerData = 0;
					pDCLCommand->operands[0] = ((UserExportDCLCommand*)pExportDCL)->operands[0];
				}
				break ;
		}
		
		// Break out of the loop if we got an error!
		if (error)
			break;
		
		// Convert the DCL data pointers from user space to kernel space:
		// (new style programs will have this step performed automatically when the program
		// is imported to the kernel...)
		IOAddressRange tempRange ;
		tempRange.address = 0;		// supress warning
		tempRange.length = 0;		// supress warning
		if ( getDCLDataBuffer ( pExportDCL, tempRange.address, tempRange.length ) )
		{
			if ( tempRange.address != NULL && tempRange.length > 0 )
			{
				IOByteCount offset ;
				if ( ! findOffsetInRanges (	tempRange.address, userBufferRangeCount, userBufferRanges, offset ) )
				{
					DebugLog( "IOFWUserLocalIsochPort::initWithUserDCLProgram: couldn't find DCL data buffer in buffer ranges") ;
					error = kIOReturnError;
					break;
				}
				
				// set DCL's data pointer to point to same memory in kernel address space
				setDCLDataBuffer ( pCurrentDCL, bufferMap->getVirtualAddress() + offset, tempRange.length ) ;					
			}
		}
		
		// increment the count of DCLs
		++fProgramCount ;			
		
		// Break out of this loop if we found the end.
		if (pExportDCL->pNextDCLCommand == NULL)
			break;
		else
			nextUserExportDCLOffset += size;
		
		// Sanity Check
		if (nextUserExportDCLOffset >= userExportDesc->getLength())
		{
			error = kIOReturnError;
			break;
		}
	}while(1);
	
	if ( ! error )
	{
		// Set the "next" pointer in the last DCL to NULL
		pLastDCL->pNextDCLCommand = NULL;
		
		fDCLTable = new DCLCommand*[ fProgramCount ] ;
		
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

	// Need to delete the pUserImportProgramBuffer!
	if (pUserImportProgramBuffer)
		delete [] pUserImportProgramBuffer;
	
	return error ;
}

#if 0
IOReturn
IOFWUserLocalIsochPort::releasePort ()
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

	return super::releasePort () ;
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
IOFWUserLocalIsochPort::stop()
{
	// we are sending a stop token, but take isoch workloop lock to make sure all
	// callbacks coming from FWIM have already been cleared out.

//	IOFireWireLink * link = fUserClient->getOwner()->getController()->getLink() ;	
//	link->closeIsochGate () ;	// nnn need replacement?

//	lock() ;

	IOReturn error ;
	
	// we ignore any errors from above here because we need to call super::stop() always
	error = super::stop() ;
	
	if ( fStarted )
	{
		error = IOFireWireUserClient::sendAsyncResult64( fStopTokenAsyncRef, kIOFireWireLastDCLToken, NULL, 0 ) ;
		
		fStarted = false ;
	}

//	unlock() ;
	
//	link->openIsochGate () ;

 	return error ;
}

void
IOFWUserLocalIsochPort::s_dclCallProcHandler( DCLCallProc * dcl )
{
#if 0
#if IOFIREWIREUSERCLIENTDEBUG > 0
	IOFWUserLocalIsochPort * me = (IOFWUserLocalIsochPort *) holder->obj ;

	DebugLog("+IOFWUserLocalIsochPort::s_dclCallProcHandler, holder=%p, (holder->asyncRef)[0]=0x%x\n", holder, (holder->asyncRef)[0]) ;

	me->fUserClient->getStatistics()->getIsochCallbackCounter()->addValue( 1 ) ;
#endif
#endif
	
	if ( dcl->procData )
	{
#if 0
// DEBUG
		DebugThing * debugThing = (DebugThing*)dcl->procData ;
		IOFireWireUserClient::sendAsyncResult64( (io_user_reference_t*)debugThing->asyncRef, kIOReturnSuccess, NULL, 0 ) ;
		DebugLog("send callback port=%p\n", debugThing->port ) ;
#else
		IOFireWireUserClient::sendAsyncResult64( (io_user_reference_t *)dcl->procData, kIOReturnSuccess, NULL, 0 ) ;
#endif
	}	
}

void
IOFWUserLocalIsochPort::s_nuDCLCallout( void * refcon )
{
	io_user_reference_t * asyncRef = (io_user_reference_t *)refcon ;
 
	IOFireWireUserClient::sendAsyncResult64( asyncRef, kIOReturnSuccess, NULL, 0 ) ;
}

IOReturn
IOFWUserLocalIsochPort::setAsyncRef_DCLCallProc( OSAsyncReference64 asyncRef )
{
	// set up stop token async ref
	bcopy( asyncRef, fStopTokenAsyncRef, sizeof( OSAsyncReference64 ) ) ;

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
				io_user_reference_t * dclAsyncRef = (io_user_reference_t*)dcl->getRefcon() ;
				if ( asyncRef )
				{
					bcopy( asyncRef, dclAsyncRef, sizeof( io_user_reference_t ) * kIOAsyncReservedCount ) ;
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
					((io_user_reference_t*)((DCLCallProc*)dcl)->procData)[ 0 ] = asyncRef[ 0 ] ;
				}
#endif
			}
		
			dcl = dcl->pNextDCLCommand ;
		}
	}
	
	return kIOReturnSuccess ;
}

IOReturn
IOFWUserLocalIsochPort::modifyJumpDCL ( UInt32 inJumpDCLCompilerData, UInt32 inLabelDCLCompilerData)
{
	if ( !fProgram )
	{
		ErrorLog("no program!\n") ;
		return kIOReturnError ;
	}

	--inJumpDCLCompilerData ;
	--inLabelDCLCompilerData ;

	// be sure opcodes exist
	if ( (inJumpDCLCompilerData >= fProgramCount) || (inLabelDCLCompilerData >= fProgramCount) )
	{
		DebugLog( "IOFWUserLocalIsochPort::modifyJumpDCL: DCL index (inJumpDCLCompilerData=%u, inLabelDCLCompilerData=%u) past end of lookup table (length=%u)\n", 
				(uint32_t)inJumpDCLCompilerData,
				(uint32_t)inLabelDCLCompilerData,
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
IOFWUserLocalIsochPort::modifyDCLSize ( UInt32 dclCompilerData, IOByteCount newSize )
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
IOFWUserLocalIsochPort::convertToKernelDCL (UserExportDCLUpdateDCLList *pUserExportDCL, DCLUpdateDCLList *dcl)
{
	UInt8 *pListAddress = (UInt8 *)pUserExportDCL;
	pListAddress += sizeof(UserExportDCLUpdateDCLList);
	mach_vm_address_t *pExportListItem = (mach_vm_address_t*)pListAddress;
	
	// when the program was imported to the kernel, the update list was placed in
	// the DCL program export buffer immediately after the DCL
	dcl->dclCommandList = (DCLCommand**)(dcl + 1) ;
	
	for( unsigned index = 0 ; index < dcl->numDCLCommands; ++index )
	{
		dcl->dclCommandList[ index ] = (DCLCommand*)( fProgramBuffer + pExportListItem[index]) ;
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
IOFWUserLocalIsochPort::convertToKernelDCL (UserExportDCLJump *pUserExportDCL, DCLJump *dcl )
{
	// the label field contains a an offset from the beginning of fProgramBuffer
	// where the label can be found
	dcl->pJumpDCLLabel = (DCLLabel*)( fProgramBuffer + (UInt32)pUserExportDCL->pJumpDCLLabel ) ;

	if ( ( dcl->pJumpDCLLabel->opcode & ~kFWDCLOpFlagMask ) != kDCLLabelOp )
	{
		dcl->pJumpDCLLabel = NULL ;
		DebugLog( "Jump %p pointing to non-label %p\n", dcl, dcl->pJumpDCLLabel ) ;
//		return kIOFireWireBogusDCLProgram ;
	}

	return kIOReturnSuccess ;
}

IOReturn
IOFWUserLocalIsochPort::convertToKernelDCL (UserExportDCLCallProc *pUserExportDCL, DCLCallProc * dcl)
{
	//if ( !dcl->proc )
	//	return NULL ;
	
	io_user_reference_t * asyncRef = (io_user_reference_t *)(dcl + 1 ) ;
	
	asyncRef[0] = 0 ;
	asyncRef[ kIOAsyncCalloutFuncIndex ] = (mach_vm_address_t)pUserExportDCL->proc ;
	asyncRef[ kIOAsyncCalloutRefconIndex ] = (io_user_reference_t)pUserExportDCL->procData ;
	
	dcl->proc				= (DCLCallCommandProc*) & s_dclCallProcHandler ;
	dcl->procData			= (DCLCallProcDataType) asyncRef ;


#if 0
// DEBUG
	DebugThing * debugThing = new DebugThing ;
	dcl->procData			= debugThing ;
	debugThing->asyncRef = asyncRef ;
	debugThing->port = this ;
#else
	dcl->procData			= (DCLCallProcDataType) asyncRef ;
#endif
		
	return kIOReturnSuccess ;
}

void
IOFWUserLocalIsochPort::exporterCleanup( const OSObject * self )
{
	IOFWUserLocalIsochPort * me = (IOFWUserLocalIsochPort*)self;
	me->stop() ;
	me->releasePort() ;
}

IOReturn
IOFWUserLocalIsochPort::userNotify (
		UInt32			notificationType,
		UInt32			numDCLs,
		void *			data,
		IOByteCount		dataSize )
{
	InfoLog("+IOFWUserLocalIsochPort::userNotify, numDCLs=%ld\n", numDCLs ) ;
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
            void * iter_data = data;
            
			IOMemoryMap * bufferMap = fProgram->getBufferMap() ;
			for( unsigned index=0; index < numDCLs; ++index )
			{
                // don't run of the end of the data buffer
                if( iter_data >= ((UInt8*)data + dataSize) )
                {
                    error = kIOReturnBadArgument;
                    break;
                }

				unsigned dclIndex = *(unsigned*)iter_data - 1 ;
				if ( dclIndex >= programLength )
				{
					DebugLog("out of range DCL dclIndex=%d, programLength=%d\n", dclIndex, programLength ) ;
					error = kIOReturnBadArgument ;
				}
				else
				{
					dcls[ index ] = (IOFWDCL*)program->getObject( dclIndex ) ;
					
					iter_data = (UInt8*)iter_data + sizeof( unsigned ) ;
					IOByteCount import_data_size = 0;
					
					error = dcls[ index ]->importUserDCL( (UInt8*)iter_data, import_data_size, bufferMap, program ) ;

					// if there is no branch set, make sure the DCL "branches" to the 
					// dcl that comes next in the program if there is one...
					if ( dclIndex + 1 < programLength && !dcls[ index ]->getBranch() )
					{
						dcls[ index ]->setBranch( (IOFWDCL*)program->getObject( dclIndex + 1 ) ) ;
					}
					
					iter_data = (UInt8*)iter_data + import_data_size;
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
            unsigned dcl_index_count = dataSize / sizeof(unsigned);
            
			// subtract 1 from each index in our list.
			// when the notification type is kFWNuDCLModifyJumpNotification, the dcl list
			// actually contains pairs of DCL indices. The first is the dcl having its branch modified,
			// the second is the index of the DCL to branch to.
			{
				unsigned index = 0 ;
				unsigned pairIndex = 0 ;

				while( pairIndex < numDCLs )
				{
                    // don't run of the end of the data buffer
                    if( index >= dcl_index_count )
                    {
                        error = kIOReturnBadArgument;
                        break;
                    }
                    
					--dclIndexTable[ index ] ;
					if ( dclIndexTable[ index ] >= programLength )
					{
						DebugLog("out of range DCL index=%d, dclIndices[ index ]=%d, programLength=%d\n", index, dclIndexTable[ index ], programLength ) ;
						error = kIOReturnBadArgument ;
						break ;
					}

					dcls[ pairIndex ] = (IOFWDCL*)program->getObject( dclIndexTable[ index ] ) ;
					
					++index ;
					
					if (dclIndexTable[ index ])
					{
						--dclIndexTable[ index ] ;
						
						if ( dclIndexTable[ index ] >= programLength )
						{
							DebugLog("out of range DCL index=%d, dclIndices[ index ]=%d, programLength=%d\n", index, dclIndexTable[ index ], programLength ) ;
							error = kIOReturnBadArgument ;
							break ;						
						}
						
						dcls[ pairIndex ]->setBranch( (IOFWDCL*)program->getObject( dclIndexTable[ index ] ) ) ;
					}
					else
					{
						dcls[ pairIndex ]->setBranch( 0 ) ;
					}

					++index ;
					++pairIndex ;
				}
			}

			break ;
		}
		
		case kFWNuDCLUpdateNotification :
		{
            unsigned dcl_indices_count = dataSize / sizeof(unsigned);

            unsigned index = 0 ;
			while ( index < numDCLs )
			{
				unsigned * dclIndices = (unsigned*)data ;

                // don't run of the end of the data buffer
                if( index >= dcl_indices_count )
                {
                    error = kIOReturnBadArgument;
                    break;
                }
                
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
			DebugLog("unsupported notification type 0x%08x\n", (uint32_t)notificationType) ;
			break ;
		}
	}
	
	program->release() ;
	
	return error ? error : notify( (IOFWDCLNotificationType)notificationType, (DCLCommand**)dcls, numDCLs ) ;
}

IOWorkLoop *
IOFWUserLocalIsochPort::createRealtimeThread()
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
