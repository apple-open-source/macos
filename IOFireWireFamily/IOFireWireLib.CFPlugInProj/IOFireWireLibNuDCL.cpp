/*
 *  IOFireWireLibNuDCL.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Thu Feb 27 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$ Log:IOFireWireLibNuDCL.cpp,v $
 */

#import "IOFireWireLibPriv.h"
#import "IOFireWireLibNuDCL.h"
#import "IOFireWireLibNuDCLPool.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibCoalesceTree.h"

namespace IOFireWireLib {

	static IOByteCount
	findOffsetInRanges ( IOVirtualAddress address, IOVirtualRange ranges[], unsigned rangeCount )
	{
		UInt32			index			= 0 ;
		IOByteCount		distanceInRange ;
		IOByteCount		offset = 0 ;
		
		{
			bool found = false ;
			while ( !found && index < rangeCount )
			{
				distanceInRange = address - ranges[index].address ;
				if ( found = ( distanceInRange < ranges[ index ].length ) )
					offset += distanceInRange ;
				else
					offset += ranges[ index ].length ;
				
				++index ;
			}
		}
		
		return offset ;
	}

	#undef Class
	#define Class NuDCL
	
	NuDCL::NuDCL( NuDCLPool & pool, UInt32 numRanges, IOVirtualRange ranges[], NuDCLSharedData :: Type type )
	: fData( type )
	, fPool( pool )
	{
		if ( numRanges > 6 )
			throw kIOReturnBadArgument ;
			
		// copy passed in ranges to our ranges array
		bcopy( ranges, fData.ranges, numRanges * sizeof( IOVirtualRange ) ) ;
		fData.rangeCount = numRanges ;
	}

	NuDCL::~NuDCL()
	{
	}

	IOReturn
	NuDCL::AppendRanges ( UInt32 numRanges, IOVirtualRange ranges[] )
	{
		if ( fData.rangeCount + numRanges > 6 )
			return kIOReturnOverrun ;
		
		bcopy( ranges, & fData.ranges[ fData.rangeCount ], numRanges * sizeof(IOVirtualRange) ) ;
		fData.rangeCount += numRanges ;
		
		return kIOReturnSuccess ;
	}
	
	IOReturn
	NuDCL::SetRanges ( UInt32 numRanges, IOVirtualRange ranges[] )
	{
		fData.rangeCount = numRanges ;
		bcopy( ranges, fData.ranges, numRanges * sizeof( IOVirtualRange ) ) ;
		
		return kIOReturnSuccess ;
	}
	
	UInt32
	NuDCL::GetRanges( UInt32 maxRanges, IOVirtualRange ranges[] ) const
	{
		unsigned count = maxRanges <? fData.rangeCount ;
		bcopy( fData.ranges, ranges, count * sizeof( IOVirtualRange ) ) ;
		
		return count ;
	}
	
	IOReturn
	NuDCL::GetSpan( IOVirtualRange& result ) const
	{		
		if ( fData.rangeCount )
		{
			result.address = fData.ranges[0].address ;
			IOVirtualAddress end = result.address + fData.ranges[0].length ;
			
			for( unsigned index=2; index < fData.rangeCount; ++index )
			{
				result.address = result.address <? fData.ranges[index].address ;
				end = end >? ( fData.ranges[index].address + fData.ranges[index].length ) ;
			}
			
			result.length = end - result.address ;
		}
		else
		{
			result = IOVirtualRangeMake( 0, 0 ) ;
		}
		
		return kIOReturnSuccess ;
	}
	
	IOByteCount
	NuDCL::GetSize() const
	{
		IOByteCount result = 0 ;
		
		for( unsigned index=0; index < fData.rangeCount; ++index )
			result += fData.ranges[index].length ;
		
		return result ;
	}

	IOReturn
	NuDCL::AppendUpdateList( NuDCL* updateDCL )
	{
		if ( !fData.update.set )
			fData.update.set = ::CFSetCreateMutable( kCFAllocatorDefault, 1, nil ) ;

		if ( !fData.update.set )
			return kIOReturnNoMemory ;

		::CFSetSetValue( fData.update.set, updateDCL ) ;
		
		return kIOReturnSuccess ;
	}
	
	IOReturn
	NuDCL::SetUpdateList( CFSetRef updateList )
	{
		if ( fData.update.set )
			::CFRelease( fData.update.set ) ;
		
		if ( updateList )
		{
			fData.update.set = ::CFSetCreateMutableCopy( kCFAllocatorDefault, ::CFSetGetCount( updateList ), updateList ) ;
			
			if ( !fData.update.set )
				return kIOReturnNoMemory ;
		}
		
		return kIOReturnSuccess ;
	}

	void
	NuDCL::EmptyUpdateList()
	{
		if ( fData.update.set )
			::CFSetRemoveAllValues( fData.update.set ) ;
	}

	void
	NuDCL::Print( FILE* file ) const
	{
		if ( fData.rangeCount > 0 )
		{
			for( unsigned index=0; index < fData.rangeCount; ++index )
				fprintf( file, "\t\t\t%u: < %x, %lu >\n", index, fData.ranges[index].address, fData.ranges[index].length ) ;
		}
		if ( fData.branch.dcl )
			fprintf( file, "\t\t\tbranch --> %p\n", fData.branch.dcl ) ;
		if ( fData.callback )
			fprintf( file, "\t\t\tcallback @%p\n", fData.callback ) ;
		if ( fData.timeStamp.ptr )
			fprintf( file, "\t\t\ttime stamp %p\n", fData.timeStamp.ptr ) ;
		if ( fData.update.set )
		{
			CFIndex count = ::CFSetGetCount( fData.update.set ) ;
			if ( count > 0 )
			{				
				fprintf( file, "\t\t\tupdate {" ) ;

				const void* values[ count ] ;
				::CFSetGetValues( fData.update.set, values ) ;
				
				for( CFIndex index=0; index < count; ++index )
					fprintf( file, " %p", values[ index ] ) ;

				fprintf( file, " }\n") ;
			}
		}
		if ( fData.status.ptr )
			fprintf( file, "\t\t\tstatus ptr %p\n", fData.status.ptr ) ;
		fprintf( file, "\t\t\trefcon %p\n", fData.refcon ) ;
	}

	void
	NuDCL::CoalesceBuffers( CoalesceTree & tree ) const
	{
		for( unsigned index=0; index < fData.rangeCount; ++index )
		{
			tree.CoalesceRange( fData.ranges[ index ] ) ;
		}
		
		if ( fData.timeStamp.ptr )
		{
			tree.CoalesceRange( IOVirtualRangeMake( (IOVirtualAddress) fData.timeStamp.ptr, sizeof( *fData.timeStamp.ptr ) ) ) ;
		}

		if ( fData.status.ptr )
		{
			tree.CoalesceRange( IOVirtualRangeMake( (IOVirtualAddress) fData.status.ptr, sizeof( *fData.status.ptr ) ) ) ;
		}
	}

	IOByteCount
	NuDCL :: Export ( 
		IOVirtualAddress * 		where, 
		IOVirtualRange			bufferRanges[],
		unsigned				bufferRangeCount ) const
	{
		if ( where )
		{
			NuDCLSharedData * exportedData = reinterpret_cast<NuDCLSharedData *>( *where ) ;
			*where = (IOVirtualAddress)( exportedData + 1 ) ;
			*exportedData = fData ;
			
			for( unsigned index=0; index < exportedData->rangeCount; ++index )
			{
				exportedData->ranges[ index ].address = findOffsetInRanges( exportedData->ranges[ index ].address, bufferRanges, bufferRangeCount ) ;
			}
			
			if( exportedData->update.set )
			{
				unsigned count = ::CFSetGetCount( exportedData->update.set ) ;

				UInt32 * exportList = reinterpret_cast<UInt32 *>( *where ) ;
				*where += sizeof( NuDCL*) * count ;
				
				// copy contents of update list to export data as array of NuDCL*
				::CFSetGetValues( exportedData->update.set, reinterpret_cast<const void **>( exportList ) ) ;
				
				// for each NuDCL* in update list in export data, change NuDCL* to corresponding NuDCL's fData->exportIndex 
				// field value
				for( unsigned index=0; index < count; ++index )
				{
					exportList[ index ] = ((NuDCL*)exportList[ index ])->GetExportIndex() ;
				}

				// stuff update list field in exported data with number of DCLs in update list
				exportedData->update.count = count ;
			}
			
			if ( exportedData->timeStamp.ptr )
			{
				exportedData->timeStamp.offset = findOffsetInRanges( (IOVirtualAddress)exportedData->timeStamp.ptr, bufferRanges, bufferRangeCount ) + 1 ;
			}
			
			if ( exportedData->status.ptr )
			{
				exportedData->status.offset = findOffsetInRanges( (IOVirtualAddress)exportedData->status.ptr, bufferRanges, bufferRangeCount ) + 1 ;
			}
			
			exportedData->branch.index = exportedData->branch.dcl ? exportedData->branch.dcl->GetExportIndex() : 0 ;
		}
		
		return sizeof( NuDCLSharedData ) + ( fData.update.set ? ::CFSetGetCount( fData.update.set ) * sizeof( UInt32 ) : 0 ) ;
	}

#pragma mark -

	#undef super
	#define super NuDCL

	ReceiveNuDCL::ReceiveNuDCL( NuDCLPool & pool, UInt8 headerBytes, UInt32 numRanges, IOVirtualRange ranges[] )
	: NuDCL( pool, numRanges, ranges, NuDCLSharedData :: kReceiveType ),
	  fReceiveData()
	{
		fReceiveData.headerBytes = headerBytes ;
	}

	IOReturn
	ReceiveNuDCL :: SetWaitControl ( bool wait )
	{
		fReceiveData.wait = wait ;

		return kIOReturnSuccess ;
	}

	void
	ReceiveNuDCL :: Print( FILE* file ) const
	{
		fprintf( file, "\tRCV %p\thdr bytes=%d, wait=%s", this, fReceiveData.headerBytes, fReceiveData.wait ? "YES" : "NO" ) ;
		if ( fReceiveData.wait )
			fprintf( file, " (wait)" ) ;
		fprintf( file, "\n" ) ;
		
		super::Print( file ) ;
	}

	IOByteCount
	ReceiveNuDCL :: Export ( 
		IOVirtualAddress * 		where, 
		IOVirtualRange			bufferRanges[],
		unsigned				bufferRangeCount ) const
	{
		IOByteCount size = NuDCL::Export( where, bufferRanges, bufferRangeCount ) ;
		
		if ( where )
		{
			*reinterpret_cast<ReceiveNuDCLSharedData *>( *where ) = fReceiveData ; 
			*where += sizeof( fReceiveData ) ;
		}
			
		return size + sizeof( fReceiveData ) ;
	}
	

#pragma mark -

	#undef super
	#define super NuDCL

	SendNuDCL :: SendNuDCL( NuDCLPool & pool, UInt32 numRanges, IOVirtualRange ranges[] )
	: NuDCL( pool, numRanges, ranges, NuDCLSharedData :: kSendType )
	, fSendData()
	{
	}

	void
	SendNuDCL :: Print( FILE* file ) const
	{
		fprintf( file, "\tSEND %p\thdr=", this ) ;
		if ( fSendData.userHeader.ptr )
		{
			fprintf( file, "user @ %08lx %08lx, mask=%08lx %08lx\n", fSendData.userHeader.ptr[0], fSendData.userHeader.ptr[1], 
					fSendData.userHeaderMask.ptr[0], fSendData.userHeaderMask.ptr[1] ) ;
		}
		else
		{
			fprintf( file, "auto\n" ) ;
		}
		
		super::Print( file ) ;
	}

	IOByteCount
	SendNuDCL :: Export ( 
		IOVirtualAddress * 		where, 
		IOVirtualRange			bufferRanges[],
		unsigned				bufferRangeCount ) const
	{
		IOByteCount size = NuDCL::Export( where, bufferRanges, bufferRangeCount ) ;
		
		if ( where )
		{
			SendNuDCLSharedData * exportedData = reinterpret_cast<SendNuDCLSharedData *>( *where ) ;
			*where = (IOVirtualAddress)( exportedData + 1 ) ;
			
			*exportedData = fSendData ; 
			if ( exportedData->skipBranch.dcl )
			{
				exportedData->skipBranch.index = exportedData->skipBranch.dcl->GetExportIndex() ;
			}

			if ( exportedData->userHeader.ptr )
			{
				exportedData->userHeader.offset = findOffsetInRanges( (IOVirtualAddress)exportedData->userHeader.ptr, bufferRanges, bufferRangeCount ) + 1 ;
			}

			if ( exportedData->userHeaderMask.ptr )
			{
				exportedData->userHeaderMask.offset = findOffsetInRanges( (IOVirtualAddress)exportedData->userHeaderMask.ptr, bufferRanges, bufferRangeCount ) + 1;
			}
		}
			
		return size + sizeof( fSendData ) ;
	}

#pragma mark -

	#undef super
	#define super NuDCL

	void
	SkipCycleNuDCL::Print( FILE* file ) const
	{
		fprintf( file, "\tSKIP %p\n", this ) ;

		super::Print( file ) ;
	}
	
} // namespace
