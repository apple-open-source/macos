/*
 *  IOFWNuDCLPool.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Mar 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$ Log:IOFWNuDCLPool.cpp,v $
 */

#import "FWDebugging.h"
#import "IOFWDCLPool.h"
#import "IOFWDCL.h"
#import "IOFireWireLibNuDCL.h"
#import "IOFWUserIsochPort.h"

#import <IOKit/IOLib.h>
#import <IOKit/IOMemoryDescriptor.h>

using namespace IOFireWireLib ;

#undef super
#define super OSObject

OSDefineMetaClassAndAbstractStructors( IOFWDCLPool, super ) ;

bool
IOFWDCLPool::initWithLink ( IOFireWireLink& link, UInt32 capacity )
{
	DebugLog("IOFWDCLPool<%p>::initWithLink - link = %p\n", this, &link );
	
	fLink = & link ;
	fCurrentTag = 0 ;
	fCurrentSync = 0 ;
	
	fProgram = OSArray::withCapacity( capacity ) ;
	if ( ! fProgram )
	{
		return false ;
	}
	
	return true ;
}

void
IOFWDCLPool::setCurrentTagAndSync ( UInt8 tag, UInt8 sync )
{
	fCurrentTag = tag ;
	fCurrentSync = sync ;
}

void
IOFWDCLPool::free ()
{
	if ( fProgram )
	{
		fProgram->release() ;
		fProgram = NULL ;
	}
	
	super::free() ;
}

IOFWReceiveDCL *
IOFWDCLPool::appendReceiveDCL (
							   OSSet * 				updateSet,
							   UInt8 					headerBytes,
							   UInt32					rangesCount,
							   IOVirtualRange			ranges[] )
{
	IOFWReceiveDCL * dcl = allocReceiveDCL() ;
	if ( dcl && !dcl->initWithParams( updateSet, headerBytes, rangesCount, ranges ) )
	{
		dcl->release() ;
		dcl = NULL ;
	}
	
	if ( dcl )
	{
		appendDCL( dcl ) ;
		dcl->release() ;
	}
	
	return dcl ;
}

IOFWSendDCL *
IOFWDCLPool::appendSendDCL (
							OSSet * 				updateSet,
							UInt32					rangesCount,
							IOVirtualRange			ranges[] )
{
	IOFWSendDCL * dcl = allocSendDCL() ;
	if ( dcl && !dcl->initWithParams( updateSet, rangesCount, ranges, fCurrentSync, fCurrentTag ) )
	{
		dcl->release() ;
		dcl = NULL ;
	}
	
	if ( dcl )
	{
		appendDCL( dcl ) ;
		dcl->release() ;
	}
	
	return dcl ;
}

IOFWSkipCycleDCL *
IOFWDCLPool::appendSkipCycleDCL ()
{
	IOFWSkipCycleDCL * dcl = allocSkipCycleDCL() ;
	
	if ( dcl && !dcl->init() )
	{
		dcl->release() ;
		dcl = NULL ;
	}
	
	if ( dcl )
	{
		appendDCL( dcl ) ;
		dcl->release() ;
	}
	
	return dcl ;
}

const OSArray *
IOFWDCLPool::getProgramRef () const
{
	fProgram->retain () ;
	return fProgram ;
}

void
IOFWDCLPool::appendDCL( IOFWDCL * dcl )
{
	if ( dcl )
	{
		fProgram->setObject( dcl ) ;
	}
}

IOReturn
IOFWDCLPool::importUserProgram (
								IOMemoryDescriptor *	userExportDesc,
								unsigned				bufferRangeCount,
								IOAddressRange			bufferRanges[],
								IOMemoryMap *			bufferMap )
{
	IOByteCount exportLength = userExportDesc->getLength() ;
	DebugLog("IOFWDCLPool<%p>::importUserProgram - exportLength = 0x%llu\n", this, exportLength ) ;
	
	if ( exportLength == 0 )
	{
		return kIOReturnError ;
	}
	
	UInt8 * exportData = new UInt8[ exportLength ] ;
	if ( !exportData )
	{
		return kIOReturnNoMemory ;
	}
	
	IOReturn error = kIOReturnSuccess ;
	
	
	// copy user export data block to kernel
	{
		unsigned byteCount = (unsigned int)(userExportDesc->readBytes( 0, exportData, exportLength )) ;
		if ( byteCount < exportLength )
		{
			error = kIOReturnVMError ;
		}
	}
	
	{
		// import first pass
		
		UInt8 * exportCursor = exportData ;
		
		DebugLog("IOFWDCLPool<%p>::importUserProgram - import DCLs: pass 1\n", this ) ;
		
		while( !error && exportCursor < exportData + exportLength )
		{
			NuDCLExportData * data = (NuDCLExportData*)exportCursor ;
			exportCursor += sizeof(NuDCLExportData );
			
			IOVirtualRange		kernRanges[ data->rangeCount ] ;
			IOVirtualAddress	kernBaseAddress = bufferMap->getVirtualAddress() ;
			
			for( unsigned index=0; index < data->rangeCount; ++index )
			{
				kernRanges[ index ].address = kernBaseAddress + data->ranges[ index ].address ;
				kernRanges[ index ].length = data->ranges[ index ].length ;
			}
			
			//
			// skip over update list info..
			// We can't make our update list since it might refer to
			// DCLs that have not been imported yet. We'll do it in the second pass...
			//
			
			if ( !error )
			{
				// (IOFireWireFamily) IOFWDCLPool<0xffffff8056313360>::importUserProgram - kernBaseAddress = 0xffffff9241e65000, rangeCount = 0x00000001, type = 0x20726376
				//	DebugLog("IOFWDCLPool<%p>::importUserProgram - kernBaseAddress = %p, rangeCount = 0x%08x, type = 0x%08x\n", this, (void *)kernBaseAddress, data->rangeCount, data->type ) ;
				
				if ( data->updateCount )
				{
					// In the shared data struct for this DCL, the updateList field has
					// been filled in with the updateList length, if any, replacing
					// the user space CFMutableSetRef.
					// The update list data follows the dcl shared data struct
					// in the export data block.
					
					exportCursor += sizeof( uint64_t ) * data->updateCount ;
				}
				
				// what type of DCL?
				switch( data->type )
				{
					case NuDCLSharedData::kSendType :
					{
						//SendNuDCLExportData * sendData = ( SendNuDCLExportData * )exportCursor ;
						exportCursor += sizeof(SendNuDCLExportData) ;
						
						if ( !appendSendDCL( NULL, data->rangeCount, kernRanges ) )
						{
							error = kIOReturnNoMemory ;
						}
						
						break ;
					}
						
					case NuDCLSharedData::kReceiveType :
					{
						ReceiveNuDCLExportData * rcvData = ( ReceiveNuDCLExportData * )exportCursor ;
						exportCursor += sizeof( ReceiveNuDCLExportData ) ;
						
						if ( !appendReceiveDCL( NULL, rcvData->headerBytes, data->rangeCount, kernRanges ) )
						{
							error = kIOReturnNoMemory ;
						}
						
						break ;
					}
						
					case NuDCLSharedData::kSkipCycleType :
					{
						if ( !appendSkipCycleDCL() )
						{
							error = kIOReturnNoMemory ;
						}
						
						break ;
					}
						
					default :
						ErrorLog("IOFWDCLPool<%p>::importUserProgram - invalid export data\n", this) ;
						error = kIOReturnInternalError ;
						break ;
				}
			}
		}
		
		if( error != kIOReturnSuccess )
		{
			DebugLog("IOFWDCLPool<%p>::importUserProgram - ERROR: pass 1 failed with 0x%08x\n", this, error ) ;
		}
	}
	
	if ( !error )
	{
		DebugLog("IOFWDCLPool<%p>::importUserProgram - import DCLs: pass 2\n", this ) ;
		
		// import pass 2.
		// All DCLs are now created; fix up any "features"
		
		UInt8 * exportCursor = exportData ;
		unsigned dclIndex = 0 ;
		
		while( !error && exportCursor < ( exportData + exportLength ) )
		{
			IOFWDCL * 		theDCL = (IOFWDCL*)fProgram->getObject( dclIndex ) ;
			IOByteCount 	dataSize ;
			
			// (IOFireWireFamily) IOFWDCLPool<0xffffff805999d800>::importUserProgram - theDCL = 0xffffff805975ee60
			//	DebugLog("IOFWDCLPool<%p>::importUserProgram - theDCL = %p\n", this, theDCL ) ;
			
			error = theDCL->importUserDCL( exportCursor, dataSize, bufferMap, fProgram ) ;
			exportCursor += dataSize ;
			++dclIndex ;
		}
		
		if( error != kIOReturnSuccess )
		{
			DebugLog("IOFWDCLPool<%p>::importUserProgram - ERROR: pass 2 failed with 0x%08x\n", this, error ) ;
		}
	}
	
	DebugLog("IOFWDCLPool<%p>::importUserProgram - import DCLs: DONE (error = 0x%08x)\n", this, error ) ;
	
	delete[] exportData;
	
	return error ;
}

DCLCommand *
IOFWDCLPool::getProgram()
{
	fLeader.opcode = kDCLNuDCLLeaderOp ;
	fLeader.pNextDCLCommand = NULL ;
	fLeader.program = this ;
	
	return (DCLCommand*) & fLeader ;
}

OSMetaClassDefineReservedUnused ( IOFWDCLPool, 0);
OSMetaClassDefineReservedUnused ( IOFWDCLPool, 1);
OSMetaClassDefineReservedUnused ( IOFWDCLPool, 2);
OSMetaClassDefineReservedUnused ( IOFWDCLPool, 3);
OSMetaClassDefineReservedUnused ( IOFWDCLPool, 4);
OSMetaClassDefineReservedUnused ( IOFWDCLPool, 5);
OSMetaClassDefineReservedUnused ( IOFWDCLPool, 6);
OSMetaClassDefineReservedUnused ( IOFWDCLPool, 7);


