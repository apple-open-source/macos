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
*  IOFWUserLocalIsochPortProxy.cpp
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

// protected
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "IOFireWireUserClient.h"
#import "IOFWUserIsochPort.h"

// system
#include <IOKit/IOMemoryDescriptor.h>

/****************************************************************/
OSDefineMetaClassAndStructors(IOFWUserIsochPort, IOFWIsochPort)
/****************************************************************/

// if we wanted super intergration with user space, we could have these functions
// callout to user space syncronously with a failure on timeout.. but this
// type of port is not currently called from inside the kernel.

bool
IOFWUserIsochPort::init()
{
	return IOFWIsochPort::init() ;
}

IOReturn
IOFWUserIsochPort::getSupported(
	IOFWSpeed&		maxSpeed, 
	UInt64&			chanSupported)
{
	IOFireWireUserClientLog_(("+ IOFWUserIsochPort::getSupported\n")) ;
	
	return kIOReturnUnsupported ;
}

IOReturn
IOFWUserIsochPort::allocatePort(IOFWSpeed speed, UInt32 chan)
{
	IOFireWireUserClientLog_(("+ IOFWUserIsochPort::allocatePort\n")) ;
	return kIOReturnSuccess ;
}

IOReturn
IOFWUserIsochPort::releasePort()
{
	IOFireWireUserClientLog_(("+ IOFWUserIsochPort::releasePort\n")) ;
	return kIOReturnSuccess ;
}

IOReturn
IOFWUserIsochPort::start()
{
	IOFireWireUserClientLog_(("+ IOFWUserIsochPort::start\n")) ;
	return kIOReturnSuccess ;
}

IOReturn
IOFWUserIsochPort::stop()
{
	IOFireWireUserClientLog_(("+ IOFWUserIsochPort::stop\n")) ;
	return kIOReturnSuccess ;
}

#pragma mark -
#pragma mark --- IOFWUserIsochPortProxy ----------

/****************************************************************/
OSDefineMetaClassAndStructors(IOFWUserIsochPortProxy, OSObject)
/****************************************************************/
Boolean
IOFWUserIsochPortProxy::init(
	IOFireWireUserClient*	inUserClient)
{
	fLock					= IORecursiveLockAlloc() ;
	if (!fLock)
		return false ;

	fUserClient 			= inUserClient ;	
	fPortStarted			= false ;
	fPortAllocated			= false ;
	fPort 					= NULL ;
	
	IOFireWireUserClientLog_("IOFWUserIsochPortProxy::init: this=%p, fPort %p, fUserClient %p\n", this, fPort, fUserClient) ;
	
	return true ;
}

IOReturn
IOFWUserIsochPortProxy::getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported)
{
	IOReturn	result = kIOReturnSuccess ;

	lock() ;
	
	if (!fPort)
		result = createPort() ;

	if ( result == kIOReturnSuccess )
		result = fPort->getSupported(maxSpeed, chanSupported) ;

	unlock() ;
		
	return result ;
}

IOReturn
IOFWUserIsochPortProxy::allocatePort(IOFWSpeed speed, UInt32 chan)
{
	IOReturn	result		= kIOReturnSuccess ;

	lock() ;
	
	if (!fPortAllocated)
	{
		if (!fPort)
			result = createPort() ;
	
		if ( result == kIOReturnSuccess )
			result = fPort->allocatePort(speed, chan) ;
			
		fPortAllocated = (kIOReturnSuccess == result) ;
	}
	
	unlock() ;
	
	return result ;
}

IOReturn
IOFWUserIsochPortProxy::releasePort()
{
	IOReturn	result		= kIOReturnSuccess ;
	
	lock() ;
	
	if ( fPortStarted )
	{
		IOFireWireUserClientLog_( ("IOFWUserIsochPortProxy::releasePort: calling releasePort() before stop()!\n" ) ) ;
		if (!fPort)
			IOLog("%s %u: strange: fPort == nil when emergency releasing port.\n", __FILE__, __LINE__) ;
			
		if ( kIOReturnSuccess != fPort->stop() )
			IOLog("%s %u: fPort->stop() returned error\n", __FILE__, __LINE__) ;
	}

	if (fPortAllocated)
	{
		if (!fPort)
		{
			IOLog("%s %u: strange: fPort == nil when calling releasePort.\n", __FILE__, __LINE__) ;
			result = kIOReturnError ;
		}
	
		if ( result == kIOReturnSuccess )
			result = fPort->releasePort() ;

		fPortAllocated = false ;
	}
	
	unlock() ;
	
	return result ;
}

IOReturn 
IOFWUserIsochPortProxy::start()
{
	IOReturn	result		= kIOReturnSuccess ;

	lock() ;

	if (fPortAllocated && !fPortStarted)
	{
		if (!fPort)
			result = createPort() ;
		
		if ( result == kIOReturnSuccess )
			result = fPort->start() ;
		else
			IOLog("%s %u: IOFWUserIsochPortProxt::Start(): error %x trying to allocate isoch port\n", __FILE__, __LINE__, result ) ;

		IOFireWireUserClientLog_("starting port %p\n", this) ;

		fPortStarted = (kIOReturnSuccess == result) ;
	}

	unlock() ;
	
	return result ;
}

IOReturn
IOFWUserIsochPortProxy::stop()
{
	IOReturn	result		= kIOReturnSuccess ;

	lock() ;

	if (fPortStarted)
	{
		IOFireWireUserClientLog_("IOFWUserIsochPortProxy::stop: stopping port %p\n", fPort) ;
		
		if (!fPort)
		{
			IOLog("%s %u: IOFWUserIsochPortProxy::stop(): fPort == nil!\n", __FILE__, __LINE__) ;
			result = kIOReturnError ;
		}
		
		if ( result == kIOReturnSuccess )
			result = fPort->stop() ;
			
		fPortStarted = false ;
	}
	
	unlock() ;
	
	return result ;
}

void
IOFWUserIsochPortProxy::free()
{
	IOFireWireUserClientLog_("IOFWUserIsochPortProxy::free: this=%p, releasing fPort @ %p\n", this, fPort) ;

	lock() ;

	if (fPort)
	{
		fPort->release() ;
		fPort = NULL ;
	}
	
	unlock() ;
	
	if ( fLock )
		IORecursiveLockFree( fLock ) ;
	
	OSObject::free() ;
}

IOReturn
IOFWUserIsochPortProxy::createPort()
{
	IOReturn	result = kIOReturnSuccess ;

	IOFWUserIsochPort* newPort = new IOFWUserIsochPort ;
	if (newPort && !newPort->init())
	{
		newPort->release() ;
		newPort = NULL ;
		result = kIOReturnNoMemory ;
		IOLog("%s %u: couldn't make newPort!\n", __FILE__, __LINE__) ;
	}
	
	// no need to take lock... this funcion is always called with lock held
	fPort = newPort ;
	
	return result ;
}

#pragma mark -
#pragma mark --- IOFWUserLocalIsochPortProxy ----------

/****************************************************************/
OSDefineMetaClassAndStructors(IOFWUserLocalIsochPortProxy, IOFWUserIsochPortProxy)
/****************************************************************/

Boolean
IOFWUserLocalIsochPortProxy::initWithUserDCLProgram( LocalIsochPortAllocateParams* inParams, IOFireWireUserClient* inUserClient)
{
	if (!IOFWUserIsochPortProxy::init(inUserClient))
	{
		IOLog("%s %u:IOFWUserIsochPortProxy::init failed\n", __FILE__, __LINE__) ;
		return false ;
	}

	fTalking				= inParams->talking ;
	fStartEvent				= inParams->startEvent ;
	fStartState				= inParams->startState ;
	fStartMask				= inParams->startMask ;
	fUserObj				= inParams->userObj ;
	fTaskInfo.fTask 		= fUserClient->getOwningTask();
		
	//
	// Note: The ranges passed in must be sorted. (Should be if user space code doesn't change)
	//zzz is this still true?
	//
	IOReturn		result					= kIOReturnSuccess ;

	//
	// Get user space range list into kernel and copy user space DCL program to a kernel buffer:
	//

	// allocate list for user space virtual ranges for user space DCL's
	IOVirtualRange*	programRanges	= NULL ;
	IOVirtualRange*	bufferRanges	= NULL ;
	
	// create a memory descriptor around ranges list in user space
	programRanges = new IOVirtualRange[ inParams->userDCLProgramRangeCount ] ;
	IOMemoryDescriptor*	programRangesMem = 
								IOMemoryDescriptor::withAddress((IOVirtualAddress)(inParams->userDCLProgramRanges), 
																sizeof(IOVirtualRange) * inParams->userDCLProgramRangeCount, 
																kIODirectionIn, fUserClient->getOwningTask() ) ;
	if (!programRangesMem)
	{
		IOLog("%s %u: Couldn't create programRangesMem\n", __FILE__, __LINE__) ;
		result = kIOReturnNoMemory ;
	}
		
	// copy user space range list to in kernel list
	IOByteCount		programStartOffset 	= 0 ;
	if ( kIOReturnSuccess == result )
		result = programRangesMem->prepare() ;

	if ( kIOReturnSuccess == result )
	{
		programRangesMem->readBytes(0 /*offset*/, 
									programRanges, 
									sizeof(IOVirtualRange) * inParams->userDCLProgramRangeCount) ;	// gotcha!
	
		// we've copied the user's ranges list, so we're done with this memory descriptor.
		programRangesMem->complete() ;
		programRangesMem->release() ;
		
		if (!findOffsetInRanges( (IOVirtualAddress)(inParams->userDCLProgram),
								programRanges,
								inParams->userDCLProgramRangeCount, 
								& programStartOffset))
		{
			IOFireWireUserClientLog_("IOFWUserLocalIsochPortProxy::initWithUserDCLProgram: failed to find program starting point in ranges provided") ;
		}
		
		// use list to create a memory descriptor covering the memory containing the program in user space
		fUserDCLProgramMem = IOMemoryDescriptor::withRanges(programRanges, inParams->userDCLProgramRangeCount, 
															kIODirectionOutIn, fUserClient->getOwningTask()) ;

		if (!fUserDCLProgramMem)
		{
			IOLog("%s %u: Couldn't get fUserDCLProgramMem\n", __FILE__, __LINE__) ;
			result = kIOReturnNoMemory ;
		}
	}
	
	if ( kIOReturnSuccess == result )
	{
		result = fUserDCLProgramMem->prepare() ;
		fUserDCLProgramMemPrepared = ( result == kIOReturnSuccess ) ;
	}
	
	// allocate an in-kernel buffer to hold a copy of the DCL program from user space
	if (kIOReturnSuccess == result)
	{
		fDCLProgramBytes = fUserDCLProgramMem->getLength() ;
	
		if ( NULL == ( fKernDCLProgramBuffer = new UInt8[ fDCLProgramBytes ] ) )
		{
			result = kIOReturnNoMemory ;
			IOLog("%s %u: couldn't get fKernDCLProgramBuffer\n", __FILE__, __LINE__) ;
		}
		else
			fKernDCLProgramStart = (DCLCommand*)(fKernDCLProgramBuffer + programStartOffset) ;
	}
	
	//
	// === Map user data buffers into kernel
	//
	
	// create an array to hold list of user space ranges
	if ( inParams->userDCLBufferRangeCount > 0 )
	{
		if ( kIOReturnSuccess == result )
		{
			if ( NULL == ( bufferRanges = new IOVirtualRange[ inParams->userDCLBufferRangeCount ] ) )
			{
				result = kIOReturnNoMemory ;
				IOLog("%s %u: couldn't get bufferRanges\n", __FILE__, __LINE__) ;
			}
		}
		
		// create memory descriptor to cover buffer ranges array in user space
		IOMemoryDescriptor*	bufferRangesMem = 0 ;
		if (kIOReturnSuccess == result)
		{
			bufferRangesMem = IOMemoryDescriptor::withAddress((IOVirtualAddress)inParams->userDCLBufferRanges,
															inParams->userDCLBufferRangeCount * sizeof(IOVirtualRange),
															kIODirectionOutIn, fUserClient->getOwningTask() ) ;
			if (!bufferRangesMem)
			{
				IOLog("%s %u: couldn't get bufferRangesMem\n", __FILE__, __LINE__) ;
				result = kIOReturnNoMemory ;
			}
				
		}
		
		// copy user space range list to in-kernel list
		if ( kIOReturnSuccess == result )
			result = bufferRangesMem->prepare() ;
		
		if (kIOReturnSuccess == result)
		{
			bufferRangesMem->readBytes(0, bufferRanges, bufferRangesMem->getLength()) ;
			
			// we're done with this
			bufferRangesMem->complete() ;
		}
	
		if ( bufferRangesMem )
		{
			bufferRangesMem->release() ;
		}


		// create a memory descriptor to cover user buffers using list of virtual ranges
		// copied from user space
		if ( kIOReturnSuccess == result )
		{
			fUserBufferMem = IOMemoryDescriptor::withRanges( bufferRanges, inParams->userDCLBufferRangeCount, 
															kIODirectionOutIn, fUserClient->getOwningTask() ) ;
	
			if ( !fUserBufferMem )
			{
				IOLog("%s %u: couldn't get fUserBufferMem\n", __FILE__, __LINE__) ;			
				result = kIOReturnNoMemory ;
			}
		}
		
		if ( kIOReturnSuccess == result )
		{
			result = fUserBufferMem->prepare( kIODirectionOutIn ) ;
			fUserBufferMemPrepared = ( kIOReturnSuccess == result ) ;
			if (!fUserBufferMemPrepared)
				IOLog("%s %u:couldn't prepare mem\n", __FILE__, __LINE__) ;
		}
	
	}
	
	// allocate a buffer to hold the user space --> kernel dcl mapping table
	if (kIOReturnSuccess == result)
	{
		fUserToKernelDCLLookupTableLength = inParams->userDCLProgramDCLCount ;
		fUserToKernelDCLLookupTable = new DCLCommand*[ fUserToKernelDCLLookupTableLength ] ;
				
		if ( !fUserToKernelDCLLookupTable )
		{
			IOLog("%s %u: couldn't get fUserToKernelDCLLookupTable\n", __FILE__, __LINE__) ;
			result = kIOReturnNoMemory ;
		}
	}		

	DCLCommand**	userDCLTable = NULL ;	// array
	if (kIOReturnSuccess == result)
	{
		userDCLTable = new (DCLCommand*)[ fUserToKernelDCLLookupTableLength ]  ;

		if (!userDCLTable)
		{
			IOLog("%s %u: couldn't get userDCLTable\n", __FILE__, __LINE__) ;
			result = kIOReturnNoMemory ;
		}
	}
	
	//
	//	B. convert program pointers
	//
	if ( kIOReturnSuccess == result )
	{
		// copy user DCL program to contiguous kernel DCL program buffer
		fUserDCLProgramMem->readBytes(0, fKernDCLProgramBuffer, fDCLProgramBytes) ;
		
		// walk the DCL program and
		// 		a. convert user next ptr's to kernel next ptr's
		//		b. convert user data buffers to kernel data buffers
		//		c. fill in table entry mapping user ref ID to kernel DCL		

		if ( fUserBufferMem )
		{
			fUserBufferMemMap = fUserBufferMem->map() ;
			if ( !fUserBufferMemMap )
			{
				fKernDCLProgramStart = NULL ;
				result = kIOReturnVMError ;
				IOLog("%s %u: couldn't map!\n", __FILE__, __LINE__) ;
			}
			else
			{
				fTaskInfo.fDataBaseAddr = fUserBufferMemMap->getVirtualAddress();
				fTaskInfo.fDataSize = fUserBufferMemMap->getLength();
				// Horrible hack!!
				fTaskInfo.fCallRefCon = fUserBufferMem;
						
			}
		}
		
		if (fKernDCLProgramStart)
		{
			DCLCommand*	pCurrentKernDCL 	= fKernDCLProgramStart ;
			UInt32				lookupTableLength	= 0 ;
			DCLCommand*	lastNextUserDCL		= inParams->userDCLProgram ;
			IOByteCount			lastDistance		= programStartOffset ;

			while ( pCurrentKernDCL && (kIOReturnSuccess == result) )
			{
				IOByteCount			distance			= 0 ;
				IOVirtualRange		tempBufferRange	;

				// add this DCL to our lookup table
				userDCLTable[lookupTableLength] = lastNextUserDCL ;
				lastNextUserDCL = pCurrentKernDCL->pNextDCLCommand ;
				fUserToKernelDCLLookupTable[lookupTableLength] = pCurrentKernDCL ;

				// clear out any remnant garbage in compiler data field:
				pCurrentKernDCL->compilerData = 0 ;
	
				// if DCL has a data buffer, 
				// convert user space data buffer pointer to in-kernel data pointer
				if (getDCLDataBuffer(pCurrentKernDCL, & tempBufferRange.address, & tempBufferRange.length))
				{
					if ( tempBufferRange.address != NULL && tempBufferRange.length > 0 )
					{
						if (!findOffsetInRanges( tempBufferRange.address,
												bufferRanges,
												inParams->userDCLBufferRangeCount,
												& distance))
						{
							IOFireWireUserClientLog_("IOFWUserLocalIsochPortProxy::initWithUserDCLProgram: couldn't find DCL data buffer in buffer ranges") ;
						}
						
						// set DCL's data pointer to point to same memory in kernel address space
						IOVirtualAddress	baseAddr = fUserBufferMemMap->getVirtualAddress() ;
	
						setDCLDataBuffer( pCurrentKernDCL, baseAddr + distance, tempBufferRange.length ) ;
					}
				}
	
				// check for other DCL command types that might need conversion
				switch( pCurrentKernDCL->opcode & ~kFWDCLOpFlagMask )
				{
					case kDCLCallProcOp:
						result = convertToKernelDCL( (DCLCallProcStruct*) pCurrentKernDCL,
													userDCLTable[lookupTableLength] ) ;
						break ;
						
					default:
						break ;
				}

				//
				// convert user space next pointer to in-kernel next pointer
				//
				// since all the DCL's have been copied into the kernel sequentially, we can
				// walk the user program and keep track of how many bytes of DCL program we
				// pass through, then use that as an offset from the beginning of our
				// kernel DCL program buffer. We then set the current DCL's next pointer to point to
				// the proper in-kernel DCL
				++lookupTableLength ;
				
				if ( pCurrentKernDCL->pNextDCLCommand && kIOReturnSuccess == result )
				{
					if ( !findOffsetInRanges( (IOVirtualAddress)( pCurrentKernDCL->pNextDCLCommand ), programRanges, inParams->userDCLProgramRangeCount, & distance ) )
					{
						IOFireWireUserClientLog_("IOFWUserLocalIsochPortProxy::initWithUserDCLProgram: couldn't find DCL next ptr in program ranges") ;
						pCurrentKernDCL->pNextDCLCommand = NULL ;
					}
					else
						pCurrentKernDCL->pNextDCLCommand = (DCLCommand*)(fKernDCLProgramBuffer + distance) ;
				}
	
				// fill in compiler data in user space program to point to the proper lookup table entry
				fUserDCLProgramMem->writeBytes(lastDistance + ( (IOVirtualAddress)&pCurrentKernDCL->compilerData - (IOVirtualAddress)&pCurrentKernDCL->pNextDCLCommand ),
											& lookupTableLength /*dclIndex*/, sizeof(pCurrentKernDCL->compilerData)) ;
				lastDistance = distance ;
			
				// move to next DCL (next ptr has been converted to kernel next ptr)
				pCurrentKernDCL = pCurrentKernDCL->pNextDCLCommand ;
			}
			
			// second pass of translation. this converts dcls that need to refer to other, already translated DCLs in the program.
			// this includes, for example, jump and update list DCLs
			if ( kIOReturnSuccess == result )
			{
				// reset parameters
				UInt32		hint 			= 0 ;
							pCurrentKernDCL = fKernDCLProgramStart ;

				while ( pCurrentKernDCL && (kIOReturnSuccess == result) )
				{
					// check for other DCL command types that might need conversion
					switch( pCurrentKernDCL->opcode & ~kFWDCLOpFlagMask )
					{
						case kDCLJumpOp:
							result = convertToKernelDCL( (DCLJumpStruct*) pCurrentKernDCL, userDCLTable, fUserToKernelDCLLookupTable, lookupTableLength, hint ) ;
							break ;
		
						case kDCLUpdateDCLListOp:
							result = convertToKernelDCL( (DCLUpdateDCLListStruct*)pCurrentKernDCL, userDCLTable, fUserToKernelDCLLookupTable, lookupTableLength, hint) ;
							break ;
		
						default:
							break ;
					}
					
					// move to next DCL (next ptr has been converted to kernel next ptr)
					pCurrentKernDCL = pCurrentKernDCL->pNextDCLCommand ;
				}
			}
			
			// warning: lots of output
//			printDCLProgram( fKernDCLProgramStart, inParams->userDCLProgramDCLCount ) ;
		}
		
	}

	// memory descriptor makes a copy, so we can delete these
	delete[] programRanges ;	
	delete[] bufferRanges ;

	// delete temporary storage
	delete[] userDCLTable ;

	return (kIOReturnSuccess == result) ;
}

void
IOFWUserLocalIsochPortProxy::free()
{
	// should walk DCL program here and free all allocated associated data structures...

	if (fUserBufferMem)
	{
		if (fUserBufferMemPrepared)
			fUserBufferMem->complete() ;
		if ( fUserBufferMemMap )
			fUserBufferMemMap->release() ;
		
		IOFireWireUserClientLog_("releasing fUserBufferMem, retain=%u\n", fUserBufferMem->getRetainCount()) ;
		fUserBufferMem->release() ;
	}

	if ( fUserDCLProgramMem )
	{
		if ( fUserDCLProgramMemPrepared )
			fUserDCLProgramMem->complete() ;
		
		fUserDCLProgramMem->release() ;
		fUserDCLProgramMem = NULL ;
	}
	
	delete[] fUserToKernelDCLLookupTable ;

	if (fKernDCLProgramBuffer)
	{
		DCLCommand*		currentDCL = fKernDCLProgramStart ;
		
		while (currentDCL)
		{
			switch( currentDCL->opcode )
			{
				case kDCLCallProcOp:
					// the procData field of this DCL can hold a pointer to an AsyncRefHolder structure,
					// dynamically allocated when the program was sent in from user space
					if (((DCLCallProcStruct*)currentDCL)->procData)
						delete (AsyncRefHolder*)(((DCLCallProcStruct*)currentDCL)->procData) ;

					break ;

				case kDCLUpdateDCLListOp:
					// we allocated an array to hold pointers to the DCLs that need updating.
					// we free the array here...
					delete[] ((DCLUpdateDCLListStruct*)currentDCL)->dclCommandList ;

					break ;

				default:
					// it don't mean a thing if it ain't got that swing...
					break ;
			}
			
			currentDCL = currentDCL->pNextDCLCommand ;
		}

		delete[] fKernDCLProgramBuffer ;
	}	
	
	IOFWUserIsochPortProxy::free() ;
}


IOReturn
IOFWUserLocalIsochPortProxy::getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported)
{
	lock() ;
	
	if (!fPort)
		createPort() ;

	unlock() ;
	
	return IOFWUserIsochPortProxy::getSupported( maxSpeed, chanSupported ) ;
}

IOReturn
IOFWUserLocalIsochPortProxy::allocatePort(IOFWSpeed speed, UInt32 chan)
{
	IOReturn	result = kIOReturnSuccess ;

	lock() ;
	
	if (!fPort)
		result = createPort() ;
			
	if ( result == kIOReturnSuccess && fUserBufferMem && !fUserBufferMemPrepared )
	{
		result = fUserBufferMem->prepare() ;
		fUserBufferMemPrepared = ( result == kIOReturnSuccess )  ;
	}

	if ( result == kIOReturnSuccess && !fUserDCLProgramMemPrepared )
	{
		result = fUserDCLProgramMem->prepare() ;
		fUserDCLProgramMemPrepared = ( result == kIOReturnSuccess ) ;
	}

	if (kIOReturnSuccess == result)
		result = IOFWUserIsochPortProxy::allocatePort(speed, chan) ;
	else
	{
		IOFireWireUserClientLog_("%s %u IOFWUserLocalIsochPortProxy::allocatePort: failed with error %x\n", __FILE__, __LINE__, result ) ;
		fPort->release() ;
		fPort = NULL ;
	}

	unlock() ;
			
	return result ;
}

IOReturn
IOFWUserLocalIsochPortProxy::releasePort()
{
	IOReturn	result = kIOReturnSuccess ;
	
	lock() ;
	
	if (fPort)
	{
		result = IOFWUserIsochPortProxy::releasePort() ;
		
		if ( fUserBufferMemPrepared )
		{
			fUserBufferMem->complete() ;
			fUserBufferMemPrepared = false ;
		}

		if ( fUserDCLProgramMemPrepared )
		{
			fUserDCLProgramMem->complete() ;
			fUserDCLProgramMemPrepared = false ;
		}

		fPort->release() ;
		fPort = NULL ;
	}
	
	unlock() ;
	
	return result ;
}

IOReturn
IOFWUserLocalIsochPortProxy::stop()
{
	IOFireWireLink*		link = fUserClient->getOwner()->getController()->getLink() ;

	// we are sending a stop token, but take isoch workloop lock to make sure all
	// callbacks coming from FWIM have already been cleared out.

//	workLoop->(IOWorkLoop::Action*)
//	workLoop->closeGate() ;
	link->closeIsochGate() ;

	#if IOFIREWIREDEBUG > 0
	IOReturn result = 
	#endif
	IOFireWireUserClient::sendAsyncResult( fStopTokenAsyncRef, kIOFireWireLastDCLToken, & fUserObj, 1 ) ;
	
	IOFireWireUserClientLogIfErr_(result, "IOFWUserLocalIsochPortProxy::stop: sendAsyncResult returned error 0x%08lX!\n", (UInt32) result) ;

	IOReturn err = IOFWUserIsochPortProxy::stop() ;
	link->openIsochGate() ;
	
	return err ;
}

// ============================================================
// utility functions
// ============================================================

Boolean
IOFWUserLocalIsochPortProxy::getDCLDataBuffer(
	const DCLCommand*		inDCL,
	IOVirtualAddress*			outDataBuffer,
	IOByteCount*				outDataLength)
{
	Boolean	result = false ;

	switch(inDCL->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			*outDataBuffer		= (IOVirtualAddress)((DCLTransferPacketStruct*)inDCL)->buffer ;
			*outDataLength		= ((DCLTransferPacketStruct*)inDCL)->size ;
			result = true ;
			break ;
			
		case kDCLSendBufferOp:
		case kDCLReceiveBufferOp:
			//zzz what should I do here?
			break ;
		case kDCLPtrTimeStampOp:
			*outDataBuffer		= (IOVirtualAddress)((DCLPtrTimeStamp*)inDCL)->timeStampPtr ; 
			*outDataLength		= sizeof( *(((DCLPtrTimeStamp*)inDCL)->timeStampPtr) ) ;
			result = true ;
			break ;

		default:
			break ;
	}
	
	return result ;
}

void
IOFWUserLocalIsochPortProxy::setDCLDataBuffer(
	DCLCommand*			inDCL,
	IOVirtualAddress			inDataBuffer,
	IOByteCount					inDataLength)
{
	switch(inDCL->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			((DCLTransferPacketStruct*)inDCL)->buffer		= (void*) inDataBuffer ;
			((DCLTransferPacketStruct*)inDCL)->size 		= inDataLength ;
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

IOByteCount
IOFWUserLocalIsochPortProxy::getDCLSize(
	DCLCommand*	inDCL)
{
	IOByteCount result = 0 ;

	switch(inDCL->opcode & ~kFWDCLOpFlagMask)
	{
		case kDCLSendPacketStartOp:
		case kDCLSendPacketWithHeaderStartOp:
		case kDCLSendPacketOp:
		case kDCLReceivePacketStartOp:
		case kDCLReceivePacketOp:
			result = sizeof(DCLTransferPacketStruct) ;
			break ;
			
		case kDCLSendBufferOp:
		case kDCLReceiveBufferOp:
			result = sizeof(DCLTransferBufferStruct) ;
			break ;

		case kDCLCallProcOp:
			result = sizeof(DCLCallProcStruct) ;
			break ;
			
		case kDCLLabelOp:
			result = sizeof(DCLLabelStruct) ;
			break ;
			
		case kDCLJumpOp:
			result = sizeof(DCLJumpStruct) ;
			break ;
			
		case kDCLSetTagSyncBitsOp:
			result = sizeof(DCLSetTagSyncBitsStruct) ;
			break ;
			
		case kDCLUpdateDCLListOp:
			result = sizeof(DCLUpdateDCLListStruct) ;
			break ;

		case kDCLPtrTimeStampOp:
			result = sizeof(DCLPtrTimeStampStruct) ;
	}
	
	return result ;
}

void
IOFWUserLocalIsochPortProxy::printDCLProgram(
	const DCLCommand*		inDCL,
	UInt32						inDCLCount) 
{
	const DCLCommand*		currentDCL	= inDCL ;
	UInt32						index		= 0 ;
	
	IOLog("IOFWUserLocalIsochPortProxy::printDCLProgram: inDCL=0x%08lX, inDCLCount=%08lX\n", (UInt32)inDCL, (UInt32)inDCLCount) ;
	
	while ( (index < inDCLCount) && currentDCL )
	{
		IOLog("\n#0x%04lX  @0x%08lX   next=0x%08lX, cmplrData=0x%08lX, op=%u ", 
			index, 
			(UInt32)currentDCL,
			(UInt32)currentDCL->pNextDCLCommand,
			(UInt32)currentDCL->compilerData,
			(int) currentDCL->opcode) ;

		switch(currentDCL->opcode & ~kFWDCLOpFlagMask)
		{
			case kDCLSendPacketStartOp:
			case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
				IOLog("(DCLTransferPacketStruct) buffer=%08lX, size=%u",
					(UInt32)((DCLTransferPacketStruct*)currentDCL)->buffer,
					(int)((DCLTransferPacketStruct*)currentDCL)->size) ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				IOLog("(DCLTransferBufferStruct) buffer=%08lX, size=%lu, packetSize=%08X, bufferOffset=%08lX",
					(UInt32)((DCLTransferBufferStruct*)currentDCL)->buffer,
					((DCLTransferBufferStruct*)currentDCL)->size,
					((DCLTransferBufferStruct*)currentDCL)->packetSize,
					(UInt32)((DCLTransferBufferStruct*)currentDCL)->bufferOffset) ;
				break ;
	
			case kDCLCallProcOp:
				IOLog("(DCLCallProcStruct) proc=%08lX, procData=%08lX (OSAsyncRef: ref @ 0x%08lX, userRefCon=%08lX)",
					(UInt32)((DCLCallProcStruct*)currentDCL)->proc,
					(UInt32)((DCLCallProcStruct*)currentDCL)->procData,
					(UInt32)((AsyncRefHolder*)((DCLCallProcStruct*)currentDCL)->procData)->asyncRef,
					(UInt32)((AsyncRefHolder*)((DCLCallProcStruct*)currentDCL)->procData)->userRefCon) ;
				break ;
				
			case kDCLLabelOp:
				IOLog("(DCLLabelStruct)") ;
				break ;
				
			case kDCLJumpOp:
				IOLog("(DCLJumpStruct) pJumpDCLLabel=%08lX",
					(UInt32)((DCLJumpStruct*)currentDCL)->pJumpDCLLabel) ;
				break ;
				
			case kDCLSetTagSyncBitsOp:
				IOLog("(DCLSetTagSyncBitsStruct) tagBits=%04lX, syncBits=%04lX",
					(UInt32)((DCLSetTagSyncBitsStruct*)currentDCL)->tagBits,
					(UInt32)((DCLSetTagSyncBitsStruct*)currentDCL)->syncBits) ;
				break ;
				
			case kDCLUpdateDCLListOp:
				IOLog("(DCLUpdateDCLListStruct) dclCommandList=%08lX, numDCLCommands=%lud\n",
					(UInt32)((DCLUpdateDCLListStruct*)currentDCL)->dclCommandList,
					((DCLUpdateDCLListStruct*)currentDCL)->numDCLCommands) ;
				
				for(UInt32 listIndex=0; listIndex < ((DCLUpdateDCLListStruct*)currentDCL)->numDCLCommands; ++listIndex)
				{
					IOLog("%08lX ", (UInt32)(((DCLUpdateDCLListStruct*)currentDCL)->dclCommandList)[listIndex]) ;
					IOSleep(8) ;
				}
				
				IOLog("\n") ;
				break ;
	
			case kDCLPtrTimeStampOp:
				IOLog("(DCLPtrTimeStampStruct) timeStampPtr=0x%08lX",
					(UInt32)((DCLPtrTimeStampStruct*)currentDCL)->timeStampPtr) ;
		}
		
		currentDCL = currentDCL->pNextDCLCommand ;
		++index ;
		IOSleep(40) ;
	}
	
	IOLog("\n") ;

	if (index != inDCLCount)
		IOLog("unexpected end of program\n") ;
	
	if (currentDCL != NULL)
		IOLog("program too long for count\n") ;
}

IOReturn
IOFWUserLocalIsochPortProxy::convertToKernelDCL(
	DCLJumpStruct*			inDCLCommand,
	DCLCommand*		inUserDCLTable[],
	DCLCommand*		inUserToKernelDCLLookupTable[],
	UInt32					inTableLength,
	UInt32&					inOutHint )
{
	IOReturn	result	= kIOReturnSuccess ;
	if ( !userToKernLookup( (DCLCommand*) inDCLCommand->pJumpDCLLabel, 
							inUserDCLTable, 
							inUserToKernelDCLLookupTable, 
							inTableLength,
							inOutHint,
							(DCLCommand**) &inDCLCommand->pJumpDCLLabel))
		result = kIOReturnError ;

	IOFireWireUserClientLogIfErr_( result, "couldn't convert jump DCL (inDCLCommand=0x%08lX, inDCLCommand->pJumpDCLLabel=0x%08lX)\n", (UInt32)inDCLCommand, (UInt32)inDCLCommand->pJumpDCLLabel) ;
		
	return result ;
}

IOReturn
IOFWUserLocalIsochPortProxy::convertToKernelDCL(
	DCLUpdateDCLListStruct*	inDCLCommand,
	DCLCommand*		inUserDCLTable[],
	DCLCommand*		inUserToKernelDCLLookupTable[],
	UInt32					inTableLength,
	UInt32&					inOutHint )
{
	IOReturn			result 		= kIOReturnSuccess ;
	IOByteCount			listSize	= inDCLCommand->numDCLCommands * sizeof(DCLCommand*) ;
	IOMemoryDescriptor*	dclListMem	= IOMemoryDescriptor::withAddress(
																(IOVirtualAddress) inDCLCommand->dclCommandList,
																listSize,
																kIODirectionIn,
																fUserClient->getOwningTask() ) ;
	if (!dclListMem)
	{
		IOLog("%s %u: couldn't get dclListMem\n", __FILE__, __LINE__) ;
		result = kIOReturnNoMemory ;
	}
	if ( kIOReturnSuccess == result )
	{
		inDCLCommand->dclCommandList = new (DCLCommand*)[inDCLCommand->numDCLCommands] ;
		if (!inDCLCommand->dclCommandList)
		{
			IOFireWireUserClientLog_("%s %u: couldn't allocate inDCLCommand->dclCommandList\n", __FILE__, __LINE__) ;
			result = kIOReturnNoMemory ;
		}
	}
	
	if ( kIOReturnSuccess == result )
	{
		if ( dclListMem->readBytes(0, inDCLCommand->dclCommandList, listSize) < listSize )
		{
			IOFireWireUserClientLog_("%s %u: dclListMem->readBytes failed\n", __FILE__, __LINE__) ;
			result = kIOReturnVMError ;
		}
	}
	
	if ( kIOReturnSuccess == result )
	{
		for(UInt32	index = 0; index < inDCLCommand->numDCLCommands; ++index)
		{
			if ( !userToKernLookup( inDCLCommand->dclCommandList[index], 
									inUserDCLTable, 
									inUserToKernelDCLLookupTable, 
									inTableLength,
									inOutHint, 
									& (inDCLCommand->dclCommandList)[index]))
			{
				IOFireWireUserClientLog_("%s %u: couldn't find dcl referenced in dcl list in program\n", __FILE__, __LINE__) ;
				result = kIOReturnError ;
			}
		}
	}
	
	if (dclListMem)
	{
		dclListMem->release() ;
	}

	return result ;
}

IOReturn
IOFWUserLocalIsochPortProxy::convertToKernelDCL(
	DCLCallProcStruct*		inDCLCommand,
	DCLCommand*		inUserDCL )
{
	IOReturn		result = kIOReturnSuccess ;

	AsyncRefHolder*		holder	= new AsyncRefHolder ;
	
	if (!holder)
	{
		IOLog("%s %u: couldn't get holder\n", __FILE__, __LINE__) ;
		result = kIOReturnNoMemory ;
	}
	else
	{
		(holder->asyncRef)[0] 	= 0 ;
		holder->userRefCon 		= inUserDCL ;
		holder->obj				= this ;
		
		inDCLCommand->procData	= (UInt32) holder ;
		inDCLCommand->proc		= & (IOFWUserLocalIsochPortProxy::dclCallProcHandler) ;
	}
	
	IOFireWireUserClientLogIfErr_(result, "couldn't convert call proc DCL\n") ;
		
	return result ;
}


Boolean
IOFWUserLocalIsochPortProxy::findOffsetInRanges(
	IOVirtualAddress	inAddress,
	IOVirtualRange		inRanges[],
	UInt32				inRangeCount,
	IOByteCount*		outOffset)
{
	UInt32			index			= 0 ;
	IOByteCount		distanceInRange ;
	Boolean			found			= false ;

	*outOffset = 0 ;
	while (!found && (index < inRangeCount))
	{
		distanceInRange = inAddress - inRanges[index].address ;
		if (found = ((distanceInRange >= 0) && (distanceInRange < inRanges[index].length)))
			*outOffset += distanceInRange ;
		else
			*outOffset += inRanges[index].length ;
		
		++index ;
	}
	
	return found ;
}

Boolean
IOFWUserLocalIsochPortProxy::userToKernLookup(
	DCLCommand*	inDCLCommand,
	DCLCommand*	inUserDCLList[],
	DCLCommand*	inKernDCLList[],
	UInt32				inTableLength,
	UInt32&				inOutHint,
	DCLCommand**	outDCLCommand)
{
	inOutHint = (inOutHint - 1) % inTableLength ;
	
	UInt32		tableIndex	= inOutHint ;

	do
	{
		if ( inUserDCLList[tableIndex] != inDCLCommand )
		{
			tableIndex = (tableIndex + 1) % inTableLength ;
		}
		else
		{
			*outDCLCommand = inKernDCLList[tableIndex] ;
			inOutHint = tableIndex ;
			if ( inDCLCommand == NULL )
			{
				IOFireWireUserClientLog_("IOFWUserLocalIsochPortProxy::userToKernLookup: ") ;
			}
			
			return true ;
		}
		
	} while ( tableIndex != inOutHint ) ;

	IOFireWireUserClientLog_("IOFWUserLocalIsochPortProxy::userToKernLookup: couldn't find 0x%08lX\n", (UInt32)inDCLCommand) ;

	return false ;
}

void
IOFWUserLocalIsochPortProxy::dclCallProcHandler(
	DCLCommand*	pDCLCommand)
{
	AsyncRefHolder*	holder = (AsyncRefHolder*) (((DCLCallProcStruct*)pDCLCommand)->procData) ;

	IOFWUserLocalIsochPortProxy*	me	 = (IOFWUserLocalIsochPortProxy*) holder->obj ;

#if IOFIREWIREUSERCLIENTDEBUG > 0
	if (!me->fUserClient->getStatistics())
		panic("%s %u", __FILE__, __LINE__) ;
		
	me->fUserClient->getStatistics()->isochCallbacks->addValue( 1 ) ;
#endif
	
	if ((holder->asyncRef)[0])
	{	
#if IOFIREWIREUSERCLIENTDEBUG > 0
		IOReturn result =
#endif
		IOFireWireUserClient::sendAsyncResult( holder->asyncRef,
																kIOReturnSuccess, 
																& me->fUserObj, 1) ;

		IOFireWireUserClientLogIfErr_(result, "IOFWUserLocalIsochPortProxy::dclCallProcHandler: sendAsyncResult returned error 0x%08lX!\n", (UInt32) result) ;
	}
}

IOReturn
IOFWUserLocalIsochPortProxy::setAsyncRef_DCLCallProc( OSAsyncReference asyncRef, DCLCallCommandProc* inProc )
{
	DCLCommand*	pCurrentDCL = fKernDCLProgramStart ;
	AsyncRefHolder*		holder ;
	
	// set up stop token async ref
	IOFireWireUserClient::setAsyncReference( asyncRef, (mach_port_t) asyncRef[0], (void*)inProc, (void*)0xFFFFFFFF ) ;
	bcopy( asyncRef, fStopTokenAsyncRef, sizeof( OSAsyncReference ) ) ;
	
	// we walk the DCL program looking for callproc DCLs. 
	// when we find them, we update their asyncRef's as appropriate
	while (pCurrentDCL)	// should be careful, this has potential to loop forever if passed an endless program
	{
		if ((pCurrentDCL->opcode & ~kFWDCLOpFlagMask) == kDCLCallProcOp)
		{
			holder = (AsyncRefHolder*)((DCLCallProcStruct*)pCurrentDCL)->procData ;
			IOFireWireUserClient::setAsyncReference( asyncRef, (mach_port_t) asyncRef[0], 
													(void*) inProc, 
													holder->userRefCon );
			bcopy( asyncRef, 
				& holder->asyncRef, 
				sizeof(OSAsyncReference)) ;
		}
		
		pCurrentDCL = pCurrentDCL->pNextDCLCommand ;
	}

	return kIOReturnSuccess ;
}

IOReturn
IOFWUserLocalIsochPortProxy::modifyJumpDCL( UInt32 inJumpDCLCompilerData, UInt32 inLabelDCLCompilerData)
{
	--inJumpDCLCompilerData ;
	--inLabelDCLCompilerData ;

	// be sure opcodes exist
	if ( inJumpDCLCompilerData > fUserToKernelDCLLookupTableLength || inLabelDCLCompilerData > fUserToKernelDCLLookupTableLength )
	{
		IOFireWireUserClientLog_("IOFWUserLocalIsochPort::modifyJumpDCL: DCL index (inJumpDCLCompilerData=%lu, inLabelDCLCompilerData=%lu) past end of lookup table (length=%lu)\n", 
				inJumpDCLCompilerData,
				inLabelDCLCompilerData,
				fUserToKernelDCLLookupTableLength) ;
		return kIOReturnBadArgument ;
	}
		
	DCLJumpStruct*	pJumpDCL	= (DCLJumpStruct*)(fUserToKernelDCLLookupTable[inJumpDCLCompilerData]) ;
	pJumpDCL->pJumpDCLLabel = (DCLLabelStruct*)(fUserToKernelDCLLookupTable[inLabelDCLCompilerData]) ;
	
	// make sure we're modifying a jump and that it's pointing to a label
	if ((pJumpDCL->pJumpDCLLabel->opcode & ~kFWDCLOpFlagMask) != kDCLLabelOp)
	{
		IOFireWireUserClientLog_("IOFWUserLocalIsochPortProxy::modifyJumpDCL: modifying non-jump or pointing jump to non-label\n") ;
		return kIOReturnBadArgument ;
	}

	if (!pJumpDCL->compilerData)
		return kIOReturnSuccess ;

	IOReturn result = kIOReturnSuccess ;
	
	lock() ;
	
	if (fPort)
		result = ((IOFWLocalIsochPort*)fPort)->notify(kFWDCLModifyNotification, (DCLCommand**) &pJumpDCL, 1) ;

	unlock() ;
	
	return result ;
}

IOReturn
IOFWUserLocalIsochPortProxy::modifyJumpDCLSize( UInt32 dclCompilerData, IOByteCount newSize )
{
	--dclCompilerData ;

	// be sure opcodes exist
	if ( dclCompilerData > fUserToKernelDCLLookupTableLength )
	{
		IOFireWireUserClientLog_("IOFWUserLocalIsochPort::modifyJumpDCLSize: DCL index (dclCompilerData=%lu) past end of lookup table (length=%lu)\n", 
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
}

// should be called with lock held...
IOReturn
IOFWUserLocalIsochPortProxy::createPort()
{
	IOReturn	result = kIOReturnSuccess ;

	if ( fUserBufferMem && !fUserBufferMemPrepared )
	{
		result = fUserBufferMem->prepare(kIODirectionOutIn) ;
		fUserBufferMemPrepared = (kIOReturnSuccess == result) ;
	}
	
	if ( result == kIOReturnSuccess )
	{   		
		fPort = fUserClient->getOwner()->getController()->createLocalIsochPort(
																fTalking,
																fKernDCLProgramStart,
																&fTaskInfo, 
																fStartEvent,
																fStartState,
																fStartMask) ;

		if (!fPort)
		{
			IOLog("%s %u: no fPort", __FILE__, __LINE__) ;
			result = kIOReturnNoMemory ;
		}
	}
	
	return result ;
}
