/*
 *  IOFWDCL.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Feb 21 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$ Log:IOFWDCL.cpp,v $
 */

#import "IOFWDCL.h"
#import "FWDebugging.h"
#import "IOFWUserIsochPort.h"

#import "IOFireWireLibNuDCL.h"

#import <libkern/c++/OSCollectionIterator.h>

#if FIRELOG
#import "IOFireLog.h"
#define FIRELOG_MSG(x) FireLog x
#else
#define FIRELOG_MSG(x) do {} while (0)
#endif

using namespace IOFireWireLib ;

OSDefineMetaClassAndAbstractStructors( IOFWDCL, OSObject )

bool
IOFWDCL :: initWithRanges ( 
	OSSet * 				updateSet, 
	unsigned 				rangesCount, 
	IOVirtualRange 			ranges [] )
{
	if ( ! OSObject::init() )
		return false ;
		
	if ( updateSet )
	{
		updateSet->setObject( this ) ;
	}
	
	if ( kIOReturnSuccess != setRanges( rangesCount, ranges ) )
	{
		return false ;
	}
		
	return true ;
}

void
IOFWDCL :: setBranch( IOFWDCL* branch )
{ 
	fBranch = branch ;
}

IOFWDCL *
IOFWDCL :: getBranch() const
{
	return fBranch ;
}

void
IOFWDCL :: setTimeStampPtr ( UInt32* timeStampPtr )
{
	fTimeStampPtr = timeStampPtr ;
}

UInt32 *
IOFWDCL :: getTimeStampPtr () const
{
	return fTimeStampPtr ;
}

void
IOFWDCL :: setCallback( Callback callback )
{
	fCallback = callback ; 
}

IOFWDCL::Callback
IOFWDCL :: getCallback() const
{
	return fCallback ; 
}

void
IOFWDCL :: setStatusPtr( UInt32* statusPtr )
{
	fUserStatusPtr = statusPtr ; 
}

UInt32*
IOFWDCL :: getStatusPtr() const
{
	return fUserStatusPtr ; 
}

void
IOFWDCL :: setRefcon( void * refcon )
{
	fRefcon = refcon ; 
}

void *
IOFWDCL :: getRefcon() const
{
	return fRefcon ; 
}

const OSSet *
IOFWDCL :: getUpdateList() const
{
	return fUpdateList ;
}

IOReturn
IOFWDCL :: addRange ( IOVirtualRange & range )
{	
	IOVirtualRange * newRanges = new IOVirtualRange[ fRangeCount + 1 ] ;
	if ( !newRanges )
	{
		return kIOReturnNoMemory ;
	}
	
	bcopy( fRanges, newRanges, sizeof( IOVirtualRange ) * fRangeCount ) ;
	delete[] fRanges ;
	
	fRanges = newRanges ;
	fRanges[ fRangeCount ] = range ;	
	++fRangeCount ;

	return kIOReturnSuccess ;
}

IOReturn
IOFWDCL :: setRanges ( UInt32 numRanges, IOVirtualRange ranges[] )
{
	delete[] fRanges ;

	fRanges = ( numRanges && ranges ) ? new IOVirtualRange[ numRanges ] : NULL ;
	
	if ( !fRanges )
	{
		return kIOReturnNoMemory ;
	}
	
	bcopy( ranges, fRanges, numRanges * sizeof( IOVirtualRange ) ) ;
	fRangeCount = numRanges ;
	
	return kIOReturnSuccess ;
}

UInt32
IOFWDCL :: getRanges( UInt32 maxRanges, IOVirtualRange ranges[] ) const
{
	unsigned count = maxRanges <? fRangeCount ;
	for( unsigned index=0; index < count; ++index )
	{
		ranges[ index ] = fRanges[ index ] ;
	}
	
	return count ;
}

UInt32
IOFWDCL :: countRanges()
{
	return fRangeCount ; 
}

IOReturn
IOFWDCL :: getSpan( IOVirtualRange& result ) const
{
	if ( fRangeCount == 0 )
	{
		result.address = 0 ;
		result.length = 0 ;
		return kIOReturnSuccess ;
	}

	IOVirtualAddress lowAddress = fRanges[0].address ;
	IOVirtualAddress highAddress = lowAddress + fRanges[0].length ;

	for( unsigned index=1; index < fRangeCount; ++index )
	{
		lowAddress = (fRanges[index].address) <? lowAddress ;
		highAddress = (lowAddress + fRanges[ index ].length) >? highAddress ;
	}
	
	result.address = lowAddress ;
	result.length = highAddress - lowAddress ;
	
	return kIOReturnSuccess ;
}

IOByteCount
IOFWDCL :: getSize() const
{
	IOByteCount size = 0 ;
	for( unsigned index=0; index < fRangeCount; ++index )
		size += fRanges[ index ].length ;
	
	return size ;
}

IOReturn
IOFWDCL :: setUpdateList( OSSet* updateList )
{
	if ( updateList )
	{
		updateList->retain() ;
	}
	
	if ( fUpdateList )
	{
		fUpdateList->release() ;
		fUpdateList = NULL ;
	}

	if ( fUpdateIterator )
	{
		fUpdateIterator->release() ;
		fUpdateIterator = NULL ;
	}
	
	if ( updateList )
	{
		fUpdateList = updateList ;
		fUpdateIterator = OSCollectionIterator::withCollection( fUpdateList ) ;
	}
	
	return kIOReturnSuccess ;
}

void
IOFWDCL :: setFlags( UInt32 flags )
{
	fFlags = flags ; 
}

UInt32
IOFWDCL :: getFlags() const
{
	return fFlags ; 
}

void
IOFWDCL :: debug()
{
	if ( fBranch )
	{
		FIRELOG_MSG(( "    branch --> %p\n", fBranch )) ;
	}
	
	if ( fCallback )
	{
		FIRELOG_MSG(( "    callback %p\n", fCallback )) ;
	}
	
	if ( fTimeStampPtr )
	{
		FIRELOG_MSG(( "    time stamp @ %p\n", fTimeStampPtr )) ;
	}
	
	if ( fRangeCount )
	{
		FIRELOG_MSG(( "    ranges\n" )) ;
		for( unsigned index=0; index < fRangeCount; ++index )
		{
			FIRELOG_MSG(( "        %d: %p ( +%dd, +0x%x )\n", index, fRanges[index].address, fRanges[index].length, fRanges[index].length )) ;
		}
	}
	
	if ( fUpdateList )
	{
		FIRELOG_MSG(( "    update" )) ;
		OSIterator * iterator = OSCollectionIterator::withCollection( fUpdateList ) ;

		if ( !iterator )
		{
			FIRELOG_MSG(("couldn't get iterator!\n")) ;
		}
		else
		{
#if FIRELOG	
			// bracket this to hide unused variable warning
			unsigned count = 0 ;
			while( OSObject * obj = iterator->getNextObject() )
			{
				if ( ( count++ & 0x7 ) == 0 )
				{
					FIRELOG_MSG(("\n" )) ;
					FIRELOG_MSG(("        ")) ;
				}
				FIRELOG_MSG(( "%p ", obj )) ;
			}
			FIRELOG_MSG(("\n")) ;
#endif			
			iterator->release() ;
		}
	}

	if ( fUserStatusPtr )
	{
		FIRELOG_MSG(( "    status @ %p\n", fUserStatusPtr )) ;
	}
	
	if ( fRefcon )
	{
		FIRELOG_MSG(( "    refcon 0x%x\n", fRefcon )) ;
	}
}

void
IOFWDCL :: free ()
{
	if ( fUpdateList )
	{
		fUpdateList->release() ;
	}
	if ( fUpdateIterator )
	{
		fUpdateIterator->release() ;
	}
	
	if ( fCallback == IOFWUserLocalIsochPort::s_nuDCLCallout )
	{
		delete [] (natural_t*)fRefcon ;
	}
	
	delete[] fRanges ;
	
	delete fLoLevel ;
	
	OSObject::free() ;	
}

void
IOFWDCL :: finalize ( IODCLProgram & )
{
	if ( fUpdateList )
	{
		fUpdateList->release() ;
		fUpdateList = NULL ;
	}
	
	if ( fUpdateIterator )
	{
		fUpdateIterator->release() ;
		fUpdateIterator = NULL ;
	}
}

IOReturn
IOFWDCL :: importUserDCL (
	UInt8 *					data,
	IOByteCount &			dataSize,
	IOMemoryMap *			bufferMap,
	const OSArray *			dcls )
{
	IOVirtualAddress		kernBaseAddress = bufferMap->getVirtualAddress() ;
	NuDCLSharedData * 		sharedData = ( NuDCLSharedData * )data ;
	dataSize = sizeof( *sharedData ) ;
	data += dataSize ;

	IOReturn error = kIOReturnSuccess ;

	{
		IOVirtualRange kernRanges[ sharedData->rangeCount ] ;
		for( unsigned index=0; index < sharedData->rangeCount; ++index )
		{
			kernRanges[ index ].address = kernBaseAddress + (IOByteCount)sharedData->ranges[ index ].address ;
			kernRanges[ index ].length = sharedData->ranges[ index ].length ;
		}

		error = setRanges( sharedData->rangeCount, kernRanges ) ;
	}

	if ( sharedData->update.count )
	{
		// In the shared data struct for this DCL, the updateList field has
		// been filled in with the updateList length, if any, replacing
		// the user space CFMutableSetRef.
		// The update list data follows the dcl shared data struct
		// in the export data block.

		UInt32 * userUpdateList = ( UInt32 * )data ;
		dataSize += sharedData->update.count * sizeof( UInt32* ) ;

		OSSet * updateSet = OSSet::withCapacity( (unsigned)sharedData->update.count ) ;

		if ( __builtin_expect( !updateSet, false ) )
		{
			error = kIOReturnNoMemory ;
		}
		else
		{
			for( unsigned index=0; index < (unsigned)sharedData->update.count; ++index )
			{
				updateSet->setObject( dcls->getObject( userUpdateList[ index ] - 1 ) ) ;
			}

			setUpdateList( updateSet ) ;
			updateSet->release() ;
		}

		setBranch( sharedData->branch.index ? (IOFWDCL*)dcls->getObject( (unsigned)sharedData->branch.index - 1 ) : NULL ) ;
	}

	if ( !error )
	{
		setFlags( IOFWDCL::kUser | sharedData->flags ) ;
	}

	if ( !error )
	{
		if ( sharedData->callback )
		{
			setCallback( sharedData->callback ? IOFWUserLocalIsochPort::s_nuDCLCallout : NULL ) ;

			natural_t * asyncRef = (natural_t *) getRefcon() ;

			if ( !asyncRef )
			{
				asyncRef = new natural_t[ kOSAsyncRefCount ] ;
			}

			if ( !asyncRef )
			{
				error = kIOReturnNoMemory ;
			}
			else
			{
				asyncRef[ kIOAsyncCalloutFuncIndex ] = (natural_t)sharedData->callback ;
				asyncRef[ kIOAsyncCalloutRefconIndex ] = (natural_t)sharedData->refcon ;

				setRefcon( asyncRef ) ;
			}
		}
		else
		{
			if ( getCallback() == IOFWUserLocalIsochPort::s_nuDCLCallout )
			{
				delete [] (natural_t*)getRefcon() ;
			}

			setCallback( NULL ) ;
			setRefcon( 0 ) ;
		}
	}

	if ( !error )
	{
		setTimeStampPtr( sharedData->timeStamp.offset ? (UInt32*)( kernBaseAddress + sharedData->timeStamp.offset - 1 ) : NULL ) ;
		setStatusPtr( sharedData->status.offset ? (UInt32*)( kernBaseAddress + sharedData->status.offset - 1 ) : NULL ) ;
	}

	if ( !error )
	{
		if ( sharedData->branch.index )
		{
			unsigned branchIndex = sharedData->branch.index - 1 ;
			if ( branchIndex >= dcls->getCount() )
			{
				DebugLog("branch index out of range\n") ;
				error = kIOReturnBadArgument ;
			}
			else
			{
				setBranch( (IOFWDCL*)dcls->getObject( branchIndex ) ) ;
			}
		}
		else
		{
			setBranch( NULL ) ;
		}
	}

	return error ;
}

OSMetaClassDefineReservedUnused ( IOFWDCL, 0 ) ;
OSMetaClassDefineReservedUnused ( IOFWDCL, 1 ) ;
OSMetaClassDefineReservedUnused ( IOFWDCL, 2 ) ;
OSMetaClassDefineReservedUnused ( IOFWDCL, 3 ) ;

#pragma mark -

OSDefineMetaClassAndAbstractStructors( IOFWReceiveDCL, IOFWDCL )

bool
IOFWReceiveDCL:: initWithParams( 
	OSSet * 			updateSet, 
	UInt8 				headerBytes, 
	unsigned 			rangesCount, 
	IOVirtualRange 		ranges[] )
{
	// can only get 0, 1, or 2 header quads...
	if (!( headerBytes == 0 || headerBytes == 4 || headerBytes == 8 ) )
	{	
		DebugLog("receive DCL header bytes must be 0, 4, or 8\n") ;
		return false ;
	}
	
	fHeaderBytes = headerBytes ;

	return IOFWDCL::initWithRanges( updateSet, rangesCount, ranges ) ;		
}


IOReturn
IOFWReceiveDCL :: setWaitControl( bool wait )
{
	fWait = wait ; 
	return kIOReturnSuccess ; 
}

void
IOFWReceiveDCL :: debug ()
{
	FIRELOG_MSG(("%p: RECEIVE\n", this )) ;
	FIRELOG_MSG(("    wait: %s, headerBytes: %d\n", fWait ? "YES" : "NO", fHeaderBytes )) ;
	
	IOFWDCL::debug() ;
}

IOReturn
IOFWReceiveDCL :: importUserDCL (
	UInt8 *					data,
	IOByteCount &			dataSize,
	IOMemoryMap *			bufferMap,
	const OSArray *			dcls )
{
	IOReturn error = IOFWDCL::importUserDCL( data, dataSize, bufferMap, dcls ) ;

	if ( !error )
	{
		ReceiveNuDCLSharedData * rcvData = (ReceiveNuDCLSharedData*)( data + dataSize ) ;
		dataSize += sizeof( *rcvData ) ;
	
		error = setWaitControl( rcvData->wait ) ;
	}
	
	return error ;
}

#pragma mark -

OSDefineMetaClassAndAbstractStructors( IOFWSendDCL, IOFWDCL )

void
IOFWSendDCL::free()
{
	if ( fSkipCallback == IOFWUserLocalIsochPort::s_nuDCLCallout )
	{
		delete[] (natural_t*)fSkipRefcon ;
	}

	IOFWDCL::free() ;
}

bool
IOFWSendDCL :: initWithParams (
	OSSet * 				updateSet, 
	unsigned 				rangesCount,
	IOVirtualRange 			ranges[],
	UInt8					sync,
	UInt8					tag )
{
	if ( !IOFWDCL::initWithRanges( updateSet, rangesCount, ranges ) )
		return false ;
	
	fSync = sync ;
	fTag = tag ;
	
	return true ;
}

IOReturn
IOFWSendDCL :: addRange ( IOVirtualRange & range )
{
	if ( fRangeCount >= 5 )
		return kIOReturnError ;

	return IOFWDCL::addRange( range ) ;
}

void
IOFWSendDCL :: setUserHeaderPtr( UInt32* userHeaderPtr, UInt32 * maskPtr )
{ 
	fUserHeaderPtr = userHeaderPtr ; 
	fUserHeaderMaskPtr = maskPtr ; 
}

UInt32 *
IOFWSendDCL :: getUserHeaderPtr()
{
	return fUserHeaderPtr ; 
}

UInt32 *
IOFWSendDCL :: getUserHeaderMask()
{
	return fUserHeaderMaskPtr ; 
}

void
IOFWSendDCL :: setSkipBranch( IOFWDCL * skipBranchDCL )
{
	fSkipBranchDCL = skipBranchDCL ; 
}

IOFWDCL *
IOFWSendDCL :: getSkipBranch() const
{
	return fSkipBranchDCL ; 
}

void
IOFWSendDCL :: setSkipCallback( Callback callback )
{
	fSkipCallback = callback ; 
}

void
IOFWSendDCL :: setSkipRefcon( void * refcon )
{
	fSkipRefcon = 0 ; 
}

IOFWDCL::Callback
IOFWSendDCL :: getSkipCallback() const
{
	return fSkipCallback ;
}

void *
IOFWSendDCL :: getSkipRefcon() const
{
	return fSkipRefcon ; 
}

void
IOFWSendDCL :: setSync( UInt8 sync )
{
	fSync = sync ; 
}

UInt8
IOFWSendDCL :: getSync() const
{
	return fSync ; 
}

void
IOFWSendDCL :: setTag( UInt8 tag )
{
	fTag = tag ; 
}

UInt8
IOFWSendDCL :: getTag() const
{
	return fTag ; 
}

IOReturn
IOFWSendDCL :: setRanges ( UInt32 numRanges, IOVirtualRange ranges[] )
{
	if ( numRanges > 5 )
	{
		DebugLog( "can't build send DCL with more than 5 ranges\n" ) ;
		return kIOReturnBadArgument ;
	}
	
	return IOFWDCL::setRanges( numRanges, ranges ) ;
}

void
IOFWSendDCL :: debug ()
{
	FIRELOG_MSG(("%p: SEND\n", this )) ;
	if ( fSkipBranchDCL )
	{
		FIRELOG_MSG(("    skip --> %p\n", fSkipBranchDCL )) ;
		if ( fSkipCallback )
		{
			FIRELOG_MSG(("        skip callback: %p\n", fSkipCallback )) ;
			FIRELOG_MSG(("        skip refcon: 0x%lx\n", fSkipRefcon )) ;
		}
	}
	
	IOFWDCL::debug() ;
}

IOReturn
IOFWSendDCL :: importUserDCL (
	UInt8 *					data,
	IOByteCount &			dataSize,
	IOMemoryMap *			bufferMap,
	const OSArray *			dcls )
{
	IOReturn error = IOFWDCL::importUserDCL( data, dataSize, bufferMap, dcls ) ;
	if ( error )
	{
		return error ;
	}

	SendNuDCLSharedData * sendData = (SendNuDCLSharedData*) ( data + dataSize ) ;
	dataSize += sizeof( *sendData ) ;

	{
		setSync( sendData->syncBits ) ;
		setTag( sendData->tagBits ) ;
	}

	{
		UInt32 * userHeaderPtr = NULL ;
		UInt32 * userHeaderMaskPtr = NULL ;

		if ( sendData->userHeader.offset && sendData->userHeaderMask.offset )
		{
			IOVirtualAddress		kernBaseAddress = bufferMap->getVirtualAddress() ;

			userHeaderPtr = (UInt32*)( kernBaseAddress + sendData->userHeader.offset - 1 ) ;
			userHeaderMaskPtr = (UInt32*)( kernBaseAddress + sendData->userHeaderMask.offset - 1 ) ;
		}

		setUserHeaderPtr( userHeaderPtr, userHeaderMaskPtr ) ;
	}

	if ( sendData->skipBranch.index )
	{
		unsigned branchIndex = sendData->skipBranch.index - 1 ;
		if ( branchIndex >= dcls->getCount() )
		{
			DebugLog("skip branch index out of range\n") ;
			error = kIOReturnBadArgument ;
		}
		else
		{
			setSkipBranch( (IOFWDCL*)dcls->getObject( branchIndex ) ) ;
		}
	}
	else
	{
		setSkipBranch( NULL ) ;
	}

	{
		if ( sendData->skipCallback )
		{
			setSkipCallback( sendData->skipCallback ? IOFWUserLocalIsochPort::s_nuDCLCallout : NULL ) ;

			natural_t * asyncRef = (natural_t *) getSkipRefcon() ;

			if ( !asyncRef )
			{
				asyncRef = new natural_t[ kOSAsyncRefCount ] ;
			}

			if ( !asyncRef )
			{
				error = kIOReturnNoMemory ;
			}
			else
			{
				asyncRef[ kIOAsyncCalloutFuncIndex ] = (natural_t)sendData->skipCallback ;
				asyncRef[ kIOAsyncCalloutRefconIndex ] = (natural_t)sendData->skipRefcon ;

				setSkipRefcon( asyncRef ) ;
			}
		}
		else
		{
			if ( getSkipCallback() == IOFWUserLocalIsochPort::s_nuDCLCallout )
			{
				delete [] (natural_t*)getSkipRefcon() ;
			}

			setSkipCallback( NULL ) ;
			setSkipRefcon( 0 ) ;
		}
	}


	return kIOReturnSuccess ;
}

#pragma mark -

OSDefineMetaClassAndAbstractStructors( IOFWSkipCycleDCL, IOFWDCL )

bool
IOFWSkipCycleDCL :: init ()
{
	bool result = IOFWDCL::initWithRanges( NULL, 0, NULL ) ;
	
	return result ;
}

IOReturn
IOFWSkipCycleDCL::addRange ( IOVirtualRange& range )
{
	return kIOReturnUnsupported ;
}

IOReturn
IOFWSkipCycleDCL :: setRanges ( UInt32 numRanges, IOVirtualRange ranges[] )		
{
	if ( numRanges == 0 )
	{
		// init always calls setRanges.. for this DCL we will be called with 0 ranges, which
		// is okay, so return success ;
		
		return kIOReturnSuccess ;
	}
	return kIOReturnUnsupported ; 
}

IOReturn
IOFWSkipCycleDCL :: getSpan ( IOVirtualRange& result )
{
	return kIOReturnUnsupported ;
}

void
IOFWSkipCycleDCL :: debug ()
{
	FIRELOG_MSG(("%p: SKIP CYCLE\n", this )) ;
	IOFWDCL::debug() ;
}
