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
#import <System/libkern/OSCrossEndian.h>

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
	
	NuDCL::NuDCL( NuDCLPool & pool, UInt32 numRanges, IOVirtualRange ranges[], NuDCLSharedData::Type type )
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
		unsigned count = MIN( maxRanges, fData.rangeCount ) ;
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
				result.address = MIN( result.address, fData.ranges[index].address ) ;
				end = MAX( end, fData.ranges[index].address + fData.ranges[index].length ) ;
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
			fprintf( file, "\t\t\tranges:\n" ) ;
			for( unsigned index=0; index < fData.rangeCount; ++index )
			{
#ifdef __LP64__
				fprintf( file, "\t\t\t\t%u: < %llx, %u >\n", index, fData.ranges[index].address, fData.ranges[index].length ) ;
#else
				fprintf( file, "\t\t\t\t%u: < %x, %lu >\n", index, fData.ranges[index].address, fData.ranges[index].length ) ;
#endif
			}
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
	NuDCL::Export ( 
		IOVirtualAddress * 		where, 
		IOVirtualRange			bufferRanges[],
		unsigned				bufferRangeCount ) const
	{

		if (where)
		{
			unsigned updateSetCount = 0 ;
			NuDCLExportData * exportedData = reinterpret_cast<NuDCLExportData *>( *where ) ;
			*where += sizeof( NuDCLExportData );
			
			exportedData->type = (UInt32) fData.type;
			exportedData->refcon = (mach_vm_address_t) fData.refcon;
			exportedData->flags = fData.flags;
			exportedData->callback = (mach_vm_address_t) fData.callback;
			
			// export buffer ranges
			exportedData->rangeCount = fData.rangeCount;
			for( unsigned index=0; index < exportedData->rangeCount; ++index )
			{
				exportedData->ranges[ index ].address = findOffsetInRanges( fData.ranges[ index ].address, bufferRanges, bufferRangeCount ) ;
				exportedData->ranges[ index ].length = fData.ranges[ index ].length;
			}
			
			// export update list
			uint64_t * exportList = NULL ;
			if( fData.update.set )
			{
				
				updateSetCount = ::CFSetGetCount( fData.update.set ) ;
				exportList = reinterpret_cast<uint64_t *>( *where ) ;
				*where += sizeof( uint64_t) * updateSetCount ;
				
				
				// We need to read the values out of the CFSet and into a buffer.
				// For 64-bit mode, we just read directly into the export buffer.
				// For 32-bit mode, we offset half way into the list portion of the export buffer as a temporary storage space 
#ifdef __LP64__		
				uint64_t *pDCLUpdateSetPointers = exportList;
#else
				UInt32 *pDCLUpdateSetPointers = (UInt32 *) &exportList[updateSetCount/2];
#endif
				
				// copy contents of update list to export data as array of NuDCL*
				::CFSetGetValues( fData.update.set, reinterpret_cast<const void **>( pDCLUpdateSetPointers ) ) ;
				
				// for each NuDCL* in update list in export data, change NuDCL* to corresponding NuDCL's fData->exportIndex 
				// field value
				for( unsigned index=0; index < updateSetCount; ++index )
				{
					exportList[ index ] = ((NuDCL*)pDCLUpdateSetPointers[ index ])->GetExportIndex() ;
				}
				
				// stuff update list field in exported data with number of DCLs in update list
				exportedData->updateCount = updateSetCount ;
			}
			
			// export user timestamp
			if ( fData.timeStamp.ptr )
				exportedData->timeStampOffset = findOffsetInRanges( (IOVirtualAddress)fData.timeStamp.ptr, bufferRanges, bufferRangeCount ) + 1 ;
			else
				exportedData->timeStampOffset = 0; 
			
			// export user status field
			if ( fData.status.ptr )
				exportedData->statusOffset = findOffsetInRanges( (IOVirtualAddress)fData.status.ptr, bufferRanges, bufferRangeCount ) + 1 ;
			else
				exportedData->statusOffset = 0; 
			
			// export branch
			exportedData->branchIndex = fData.branch.dcl ? fData.branch.dcl->GetExportIndex() : 0 ;

#ifndef __LP64__		
			ROSETTA_ONLY(
				{
						 for( unsigned index=0; index < exportedData->rangeCount; ++index )
						 {
							exportedData->ranges[ index ].address = CFSwapInt64( exportedData->ranges[ index ].address ) ;
							exportedData->ranges[ index ].length = CFSwapInt64( exportedData->ranges[ index ].length ) ;
						 }
						 
						 for( unsigned index=0; index < updateSetCount; ++index )
						 {
							exportList[ index ] = CFSwapInt64( exportList[ index ] ) ;
						 }
						 exportedData->updateCount = CFSwapInt64( exportedData->updateCount ) ;
						 exportedData->timeStampOffset = CFSwapInt64( exportedData->timeStampOffset ) ;
						 exportedData->statusOffset = CFSwapInt64( exportedData->statusOffset ) ;
						 exportedData->type = (NuDCLSharedData::Type)CFSwapInt32( exportedData->type ) ;
						 exportedData->callback = (mach_vm_address_t)CFSwapInt64(exportedData->callback ) ;
						 exportedData->refcon = (mach_vm_address_t)CFSwapInt64(exportedData->refcon) ;
						 exportedData->flags = CFSwapInt32( exportedData->flags | BIT(19) ) ;
						 exportedData->rangeCount = CFSwapInt32( exportedData->rangeCount ) ;
						 exportedData->branchIndex = CFSwapInt64( exportedData->branchIndex ) ;
				}
			) ;
#endif			
		}
		
		return  sizeof( NuDCLExportData ) + ( fData.update.set ? ::CFSetGetCount( fData.update.set ) * sizeof( uint64_t ) : 0 ) ;
	}

#pragma mark -

	#undef super
	#define super NuDCL

	ReceiveNuDCL::ReceiveNuDCL( NuDCLPool & pool, UInt8 headerBytes, UInt32 numRanges, IOVirtualRange ranges[] )
	: NuDCL( pool, numRanges, ranges, NuDCLSharedData::kReceiveType ),
	  fReceiveData()
	{
		fReceiveData.headerBytes = headerBytes ;
	}

	IOReturn
	ReceiveNuDCL::SetWaitControl ( bool wait )
	{
		fReceiveData.wait = wait ;

		return kIOReturnSuccess ;
	}

	void
	ReceiveNuDCL::Print( FILE* file ) const
	{
		fprintf( file, "\tRCV %p\thdr bytes=%d, wait=%s", this, fReceiveData.headerBytes, fReceiveData.wait ? "YES" : "NO" ) ;
		if ( fReceiveData.wait )
			fprintf( file, " (wait)" ) ;
		fprintf( file, "\n" ) ;
		
		super::Print( file ) ;
	}

	IOByteCount
	ReceiveNuDCL::Export ( 
		IOVirtualAddress * 		where, 
		IOVirtualRange			bufferRanges[],
		unsigned				bufferRangeCount ) const
	{
		IOByteCount size = NuDCL::Export( where, bufferRanges, bufferRangeCount ) ;
		
		if ( where )
		{
			ReceiveNuDCLExportData * exportedData = reinterpret_cast<ReceiveNuDCLExportData *>( *where ) ;
			*where += sizeof( ReceiveNuDCLExportData );

			exportedData->headerBytes = fReceiveData.headerBytes;
			exportedData->wait = fReceiveData.wait;
		}
		
		return size + sizeof( ReceiveNuDCLExportData );
	}
	

#pragma mark -

	#undef super
	#define super NuDCL

	SendNuDCL::SendNuDCL( NuDCLPool & pool, UInt32 numRanges, IOVirtualRange ranges[] )
	: NuDCL( pool, numRanges, ranges, NuDCLSharedData::kSendType )
	, fSendData()
	{
	}

	void
	SendNuDCL::Print( FILE* file ) const
	{
		fprintf( file, "\tSEND %p\thdr=", this ) ;
		if ( fSendData.userHeader.ptr )
		{
			fprintf( file, "user @ %p, mask @ %p\n", fSendData.userHeader.ptr, fSendData.userHeaderMask.ptr ) ;
		}
		else
		{
			fprintf( file, "auto\n" ) ;
		}
		
		if ( fSendData.skipBranch.dcl )
		{
			fprintf( file, "\t\t\tskip --> %p\n", fSendData.skipBranch.dcl ) ;
		}

		if ( fSendData.skipCallback )
		{
			fprintf( file, "\t\t\tskip callback:%p refcon:%p\n", fSendData.skipCallback, fSendData.skipRefcon ) ;
		}
			
		super::Print( file ) ;
	}

	IOByteCount
	SendNuDCL::Export ( 
		IOVirtualAddress * 		where, 
		IOVirtualRange			bufferRanges[],
		unsigned				bufferRangeCount ) const
	{
		IOByteCount size = NuDCL::Export( where, bufferRanges, bufferRangeCount ) ;

		if ( where )
		{
			SendNuDCLExportData * exportedData = reinterpret_cast<SendNuDCLExportData *>( *where ) ;
			*where += sizeof(SendNuDCLExportData);

			exportedData->tagBits = fSendData.tagBits;
			exportedData->syncBits = fSendData.syncBits;
			exportedData->skipCallback = (mach_vm_address_t)fSendData.skipCallback;
			exportedData->skipRefcon = (mach_vm_address_t)fSendData.skipRefcon;

			if ( fSendData.skipBranch.dcl )
				exportedData->skipBranchIndex = fSendData.skipBranch.dcl->GetExportIndex() ;
			else
				exportedData->skipBranchIndex = 0 ;
			
			if ( fSendData.userHeader.ptr )
				exportedData->userHeaderOffset = findOffsetInRanges( (IOVirtualAddress)fSendData.userHeader.ptr, bufferRanges, bufferRangeCount ) + 1 ;
			else
				exportedData->userHeaderOffset = 0; 
			
			if ( fSendData.userHeaderMask.ptr )
				exportedData->userHeaderMaskOffset = findOffsetInRanges( (IOVirtualAddress)fSendData.userHeaderMask.ptr, bufferRanges, bufferRangeCount ) + 1;
			else
				exportedData->userHeaderMaskOffset = 0; 

#ifndef __LP64__		
			ROSETTA_ONLY(
				{
					exportedData->skipBranchIndex = CFSwapInt64( exportedData->skipBranchIndex ) ;
					exportedData->skipCallback = (mach_vm_address_t)CFSwapInt64(exportedData->skipCallback ) ;
					exportedData->skipRefcon = (mach_vm_address_t)CFSwapInt64( (UInt32)exportedData->skipRefcon ) ;
					exportedData->userHeaderOffset = CFSwapInt64( exportedData->userHeaderOffset ) ;
					exportedData->userHeaderMaskOffset = CFSwapInt64( exportedData->userHeaderMaskOffset ) ;
				}
			) ;
#endif			
			
		}

		return size + sizeof( SendNuDCLExportData );
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
