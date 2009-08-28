/*
 *  IOFireWireLibNuDCLPool.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Tue Feb 11 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$ Log:IOFireWireLibNuDCLPool.cpp,v $
 */

#import "IOFireWireLibNuDCLPool.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibNuDCL.h"
#import "IOFireWireLibCoalesceTree.h"

#import <typeinfo>
#import <mach/mach.h>

#if IOFIREWIRELIBDEBUG
#define CAST_DCL( _type, _pointer ) reinterpret_cast<_type>(_pointer)
#define CHECK_DCL( _type, _pointer ) if (!CAST_DCL(_type, _pointer)) { DebugLog("could not cast DCL %p to type ##_type!\n", _pointer); return ; }
#define CHECK_DCL_IORETURN( _type, _pointer ) if (!CAST_DCL(_type, _pointer)) { return kIOReturnUnsupported ; }
#define CHECK_DCL_NULL( _type, _pointer ) if (!CAST_DCL( _type, _pointer )) { return NULL ; }
#define CHECK_DCL_ZERO( _type, _pointer ) if (!CAST_DCL( _type, _pointer )) { return 0 ; }
#else
#define CAST_DCL( _type, _pointer ) ((_type)_pointer)
#define CHECK_DCL( _type, _pointer )
#define CHECK_DCL_IORETURN( _type, _pointer )
#define CHECK_DCL_NULL( _type, _pointer )
#define CHECK_DCL_ZERO( _type, _pointer )
#endif

#undef super
#define super IOFireWireIUnknown

#define OUTPUT_FILE stdout
namespace IOFireWireLib {

	static void cfArrayReleaseNuDCLObject(CFAllocatorRef allocator,const void *ptr)
	{
		const NuDCL *	dcl = reinterpret_cast< const NuDCL* >( ptr ) ;
		delete dcl;
	}

	NuDCLPool::NuDCLPool( const IUnknownVTbl & vTable, Device& device, UInt32 capacity )
	: super( vTable )
	,fDevice( device )
	,fCurrentTag( 0 )
	,fCurrentSync( 0 )
	{
		CFArrayCallBacks arrayCallbacks;
	
		// Initialize callbacks
		arrayCallbacks.version = 0;
		arrayCallbacks.retain = NULL;
		arrayCallbacks.copyDescription = NULL;
		arrayCallbacks.equal = NULL;
		arrayCallbacks.release = cfArrayReleaseNuDCLObject;
	
		// Create fProgram array
		fProgram = ::CFArrayCreateMutable( kCFAllocatorDefault, capacity, &arrayCallbacks );
	}

	NuDCLPool::~NuDCLPool()
	{
		// Release the fProgram array. The array's release callback will delete all the elements!
		if (fProgram)
			CFRelease(fProgram);
	}

	DCLCommand*
	NuDCLPool::GetProgram()
	{
		if ( ::CFArrayGetCount( fProgram ) == 0 )
			return nil ;

		fLeader.pNextDCLCommand = nil ;
		fLeader.opcode = kDCLNuDCLLeaderOp ;
		fLeader.program = this ;

		return reinterpret_cast< DCLCommand * > ( & fLeader ) ;
	}

	CFArrayRef
	NuDCLPool::GetDCLs()
	{
		if ( fProgram )
		{
			::CFRetain( fProgram ) ;
		}
		
		return fProgram ;
	}

	void
	NuDCLPool::SetCurrentTagAndSync ( UInt8 tag, UInt8 sync )
	{
		fCurrentTag = tag ;
		fCurrentSync = sync ;
	}
	
	NuDCL *
	NuDCLPool::AppendDCL( CFMutableSetRef saveBag, NuDCL * dcl )
	{
		if ( dcl )
		{
			if ( saveBag )
			{
				::CFSetSetValue( saveBag, dcl ) ;
			}
				
			::CFArrayAppendValue( fProgram, dcl ) ;
			dcl->SetExportIndex( ::CFArrayGetCount( fProgram ) ) ;
		}
		
		return dcl ;
	}
	
	NuDCLSendPacketRef
	NuDCLPool::AllocateSendPacket ( CFMutableSetRef saveSet, UInt32 numBuffers, IOVirtualRange* buffers )
	{
		SendNuDCL *dcl = new SendNuDCL( *this, numBuffers, buffers );
		dcl->SetSync( fCurrentSync );
		dcl->SetTag( fCurrentTag );
		return reinterpret_cast<NuDCLSendPacketRef>( AppendDCL( saveSet, dcl ) ) ;
	}
	
	NuDCLSkipCycleRef
	NuDCLPool::AllocateSkipCycle ( CFMutableSetRef saveSet )
	{
		return reinterpret_cast<NuDCLSkipCycleRef>( AppendDCL( saveSet, new SkipCycleNuDCL( *this ) ) ) ;
	}
	
	
	NuDCLReceivePacketRef
	NuDCLPool::AllocateReceivePacket ( CFMutableSetRef saveBag, UInt8 headerBytes, UInt32 numRanges, IOVirtualRange* ranges )
	{
		return reinterpret_cast<NuDCLReceivePacketRef>( AppendDCL( saveBag, new ReceiveNuDCL( *this, headerBytes, numRanges, ranges ) ) ) ;
	}

	IOByteCount
	NuDCLPool::Export ( 
		IOVirtualAddress * 		outExportData, 
		IOVirtualRange			bufferRanges[],
		unsigned				bufferRangeCount ) const
	{
		unsigned programCount = ::CFArrayGetCount( fProgram ) ;
		IOByteCount exportBytes = 0 ;
		
		for( unsigned index=0; index < programCount; ++index )
		{
			const NuDCL *	dcl = reinterpret_cast< const NuDCL* >(::CFArrayGetValueAtIndex( fProgram, index ) ) ;

			exportBytes += dcl->Export( NULL, NULL, 0 ) ;		// find export data size needed
		}
		
		vm_allocate( mach_task_self(), (vm_address_t*)outExportData, exportBytes, true /*anywhere*/ ) ;
		
		{
			IOVirtualAddress exportCursor = *outExportData ;
			
			for ( unsigned index = 0 ; index < programCount ; ++index )
			{
				const NuDCL *	dcl = reinterpret_cast< const NuDCL* >(::CFArrayGetValueAtIndex( fProgram, index ) ) ;
	
				dcl->Export( & exportCursor, bufferRanges, bufferRangeCount ) ;			// make export data.. we don't care about the returned size
			}
		}
		
		// export program ranges... (There is only 1; it points to a block containing a serialized
		// version our program)
		
		return exportBytes ;		// 1 range contains serialized program data
	}
	
	void
	NuDCLPool::CoalesceBuffers ( CoalesceTree & toTree ) const
	{
		CFIndex count = ::CFArrayGetCount( fProgram ) ;
		for ( CFIndex index = 0 ; index < count ; ++index )
		{
			reinterpret_cast< const NuDCL* >( ::CFArrayGetValueAtIndex( fProgram, index ) )->CoalesceBuffers( toTree ) ;
		}
	}

#pragma mark -
#undef Class
#define Class NuDCLPoolCOM

#undef super
#define super NuDCLPool

	const IOFireWireNuDCLPoolInterface NuDCLPoolCOM::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 1, 		// version/revision
	
		& Class::S_GetProgram
		,& Class::S_GetDCLs
		,& Class::S_PrintProgram
		,& Class::S_PrintDCL
		,& Class::S_SetCurrentTagAndSync
		,& Class::S_AllocateSendPacket
		,& Class::S_AllocateSendPacket_v
		,& Class::S_AllocateSkipCycle
		,& Class::S_AllocateReceivePacket
		,& Class::S_AllocateReceivePacket_v
//		,& Class::S_SetDCLNextDCL
		,& Class::S_FindDCLNextDCL
		,& Class::S_SetDCLBranch
		,& Class::S_GetDCLBranch
		,& Class::S_SetDCLTimeStampPtr
		,& Class::S_GetDCLTimeStampPtr
		,& Class::S_SetDCLStatusPtr
		,& Class::S_GetDCLStatusPtr
		,& Class::S_AddDCLRanges
		,& Class::S_SetDCLRanges
		,& Class::S_SetDCLRanges_v
		,& Class::S_GetDCLRanges
		,& Class::S_CountDCLRanges
		,& Class::S_GetDCLSpan
		,& Class::S_GetDCLSize
		,& Class::S_SetDCLCallback
		,& Class::S_GetDCLCallback
		,& Class::S_SetDCLUserHeaderPtr
		,& Class::S_GetDCLUserHeaderPtr
		,& Class::S_GetDCLUserHeaderMaskPtr
		,& Class::S_SetDCLRefcon
		,& Class::S_GetDCLRefcon
		,& Class::S_AppendDCLUpdateList
		,& Class::S_SetDCLUpdateList
		,& Class::S_GetDCLUpdateList
		,& Class::S_EmptyDCLUpdateList
		,& Class::S_SetDCLWaitControl
		,& NuDCLPoolCOM::S_SetDCLFlags
		, & NuDCLPoolCOM::S_GetDCLFlags
		, & NuDCLPoolCOM::S_SetDCLSkipBranch
		, & NuDCLPoolCOM::S_GetDCLSkipBranch
		, & NuDCLPoolCOM::S_SetDCLSkipCallback
		, & NuDCLPoolCOM::S_GetDCLSkipCallback
		, & NuDCLPoolCOM::S_SetDCLSkipRefcon
		, & NuDCLPoolCOM::S_GetDCLSkipRefcon
		, & NuDCLPoolCOM::S_SetDCLSyncBits
		, & NuDCLPoolCOM::S_GetDCLSyncBits
		, & NuDCLPoolCOM::S_SetDCLTagBits
		, & NuDCLPoolCOM::S_GetDCLTagBits
	} ;

	NuDCLPoolCOM::NuDCLPoolCOM( Device& device, UInt32 numDCLs )
	: super( reinterpret_cast<const IUnknownVTbl &>( sInterface ), device, numDCLs )
	{
	}
	
	NuDCLPoolCOM::~NuDCLPoolCOM()
	{
	}

	const IUnknownVTbl **
	NuDCLPoolCOM::Alloc( Device& device, UInt32 capacity )
	{
		NuDCLPoolCOM *	me = nil;
		try {
			me = new NuDCLPoolCOM( device, capacity ) ;
		} catch(...) {
		}

		return (nil == me) ? nil : reinterpret_cast<const IUnknownVTbl**>( & me->GetInterface() ) ;
	}

	HRESULT
	NuDCLPoolCOM::QueryInterface( REFIID iid, LPVOID* ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( ::CFEqual(interfaceID, IUnknownUUID) ||  ::CFEqual(interfaceID, kIOFireWireNuDCLPoolInterfaceID) )
		{
			*ppv = & GetInterface() ;
			AddRef() ;
		}
		else
		{
			*ppv = nil ;
			result = E_NOINTERFACE ;
		}	
		
		::CFRelease(interfaceID) ;
		return result ;		
	}
	
	DCLCommand*
	NuDCLPoolCOM::S_GetProgram( IOFireWireLibNuDCLPoolRef self  )
	{
		return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->GetProgram( ) ;
	}
	
	CFArrayRef
	NuDCLPoolCOM::S_GetDCLs( IOFireWireLibNuDCLPoolRef self )
	{
		return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->GetDCLs( ) ;
	}
		
	void
	NuDCLPoolCOM::S_PrintProgram( IOFireWireLibNuDCLPoolRef self )
	{
		NuDCLPoolCOM* me = IOFireWireIUnknown::InterfaceMap< NuDCLPoolCOM >::GetThis( self ) ;
		CFIndex count = ::CFArrayGetCount( me->fProgram ) ;
		
		for( CFIndex index=0; index < count; ++index )
		{
			fprintf( OUTPUT_FILE, "%ld:", index ) ;
			reinterpret_cast<const NuDCL*>( ::CFArrayGetValueAtIndex( me->fProgram, index ) )->Print( OUTPUT_FILE ) ; 
		}
	}
	
	void
	NuDCLPoolCOM::S_PrintDCL( NuDCLRef dcl  )
	{
		CHECK_DCL( NuDCL*, dcl ) ;

		CAST_DCL( NuDCL*, dcl )->Print( OUTPUT_FILE ) ;
	}
				
	// Allocating send NuDCLs:
			
	void
	NuDCLPoolCOM::S_SetCurrentTagAndSync( IOFireWireLibNuDCLPoolRef self, UInt8 tag, UInt8 sync  )
	{
		IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->SetCurrentTagAndSync( tag, sync ) ;
	}
			
	NuDCLSendPacketRef
	NuDCLPoolCOM::S_AllocateSendPacket( IOFireWireLibNuDCLPoolRef self, CFMutableSetRef saveBag, UInt32 numBuffers, IOVirtualRange* buffers )
	{
		return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->AllocateSendPacket( saveBag, numBuffers, buffers ) ;
	}

	NuDCLSendPacketRef
	NuDCLPoolCOM::S_AllocateSendPacket_v( IOFireWireLibNuDCLPoolRef self, CFMutableSetRef saveBag, IOVirtualRange* firstRange, ... )
	{
		if ( firstRange )
		{
			unsigned 	count = 1 ;			
			va_list 	args;
			
			// count args
			
			va_start( args, firstRange ) ;
			while ( va_arg( args, IOVirtualRange* ) )
				++count ;
			va_end( args );
	
			IOVirtualRange buffers[ count ] ;
			
			// copy args to buffers array
			
			buffers[0] = *firstRange ;
			va_start( args, firstRange ) ;
			for( unsigned index=1; index < count; ++index )
				buffers[index] = *va_arg( args, IOVirtualRange* ) ;
			va_end( args ) ;
			
			return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->AllocateSendPacket( saveBag, count, buffers ) ;
		}
		
		return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->AllocateSendPacket( saveBag, 0, nil ) ;
	}
	

	NuDCLSkipCycleRef
	NuDCLPoolCOM::S_AllocateSkipCycle( IOFireWireLibNuDCLPoolRef self  )
	{
		return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->AllocateSkipCycle() ;
	}
		
	//
	// Allocating send NuDCLs:
	//
		
	NuDCLReceivePacketRef
	NuDCLPoolCOM::S_AllocateReceivePacket( IOFireWireLibNuDCLPoolRef self, CFMutableSetRef saveBag, UInt8 headerBytes, UInt32 numBuffers, IOVirtualRange* buffers )
	{
		return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->AllocateReceivePacket( saveBag, headerBytes, numBuffers, buffers ) ;
	}

	NuDCLReceivePacketRef
	NuDCLPoolCOM::S_AllocateReceivePacket_v( IOFireWireLibNuDCLPoolRef self, CFMutableSetRef saveBag, UInt8 headerBytes, IOVirtualRange* firstRange, ... )
	{
		if ( firstRange )
		{
			unsigned 	count = 1 ;			
			va_list 	args;
			
			// count args
			
			va_start( args, firstRange ) ;
			while ( va_arg( args, IOVirtualRange* ) )
				++count ;
			va_end( args );
	
			IOVirtualRange buffers[ count ] ;
			
			// copy args to buffers array
			
			buffers[0] = *firstRange ;
			va_start( args, firstRange ) ;
			for( unsigned index=1; index < count; ++index )
				buffers[index] = *va_arg( args, IOVirtualRange* ) ;
			va_end( args ) ;
			
			return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->AllocateReceivePacket( saveBag, headerBytes, count, buffers ) ;
		}
		
		return IOFireWireIUnknown::InterfaceMap< Class >::GetThis( self )->AllocateReceivePacket( saveBag, headerBytes, 0, nil ) ;
	}
	
	//
	// NuDCL configuration
	//
			
	NuDCLRef
	NuDCLPoolCOM::S_FindDCLNextDCL( IOFireWireLibNuDCLPoolRef self, NuDCLRef dcl )
	{
		CHECK_DCL_NULL( NuDCL*, dcl ) ;
		
		NuDCLPoolCOM* me = IOFireWireIUnknown::InterfaceMap<NuDCLPoolCOM>::GetThis( self ) ;
		
		CFIndex count = ::CFArrayGetCount( me->fProgram ) ;
		CFIndex index = ::CFArrayGetFirstIndexOfValue( me->fProgram, ::CFRangeMake( 0, count ),  dcl ) ;
		if ( index == kCFNotFound || index == count )
			return 0 ;

		return (NuDCLRef)::CFArrayGetValueAtIndex( me->fProgram, index+1 ) ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLBranch( NuDCLRef dcl, NuDCLRef branchDCL  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		CAST_DCL( NuDCL*, dcl )->SetBranch( CAST_DCL( NuDCL*, branchDCL ) ) ;
		
		return kIOReturnSuccess ;
	}
	
	NuDCLRef
	NuDCLPoolCOM::S_GetDCLBranch( NuDCLRef dcl  )
	{
		CHECK_DCL_NULL( NuDCL*, dcl ) ;
		
		return reinterpret_cast<NuDCLRef>( CAST_DCL( NuDCL*, dcl )->GetBranch() ) ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLTimeStampPtr( NuDCLRef dcl, UInt32* timeStampPtr  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		CAST_DCL( NuDCL*, dcl )->SetTimeStampPtr( timeStampPtr ) ;

		return kIOReturnSuccess ;
	}
	
	UInt32*
	NuDCLPoolCOM::S_GetDCLTimeStampPtr( NuDCLRef dcl  )
	{
		CHECK_DCL_NULL( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->GetTimeStampPtr() ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_AddDCLRanges( NuDCLRef dcl, UInt32 numRanges, IOVirtualRange* ranges  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->AppendRanges( numRanges, ranges ) ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLRanges( NuDCLRef dcl, UInt32 numRanges, IOVirtualRange* ranges  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->SetRanges( numRanges, ranges ) ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLRanges_v ( NuDCLRef dcl, IOVirtualRange* firstRange, ... )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;

		if ( firstRange )
		{
			unsigned 	count = 1 ;			
			va_list 	args;
			
			// count args
			
			va_start( args, firstRange ) ;
			while ( va_arg( args, IOVirtualRange* ) )
				++count ;
			va_end( args );
	
			IOVirtualRange buffers[ count ] ;
			
			// copy args to buffers array
			
			buffers[0] = *firstRange ;
			va_start( args, firstRange ) ;
			for( unsigned index=1; index < count; ++index )
				buffers[index] = *va_arg( args, IOVirtualRange* ) ;
			va_end( args ) ;
			
			return CAST_DCL( NuDCL*, dcl )->SetRanges( count, buffers ) ;
		}
		
		return kIOReturnSuccess ;
	}
	

	UInt32
	NuDCLPoolCOM::S_GetDCLRanges( NuDCLRef dcl, UInt32 maxRanges, IOVirtualRange* outRanges  )
	{
		CHECK_DCL_ZERO( NuDCL*, dcl ) ;

		return CAST_DCL( NuDCL*, dcl )->GetRanges( maxRanges, outRanges ) ;
	}
	
	UInt32
	NuDCLPoolCOM::S_CountDCLRanges( NuDCLRef dcl )
	{
		CHECK_DCL_ZERO( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->CountRanges() ;
	}

	IOReturn
	NuDCLPoolCOM::S_GetDCLSpan ( NuDCLRef dcl, IOVirtualRange* spanRange )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->GetSpan( *spanRange ) ;
	}
	

	IOByteCount
	NuDCLPoolCOM::S_GetDCLSize( NuDCLRef dcl )
	{
		CHECK_DCL_ZERO( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->GetSize() ;
	}
	

	IOReturn
	NuDCLPoolCOM::S_SetDCLCallback( NuDCLRef dcl, NuDCLCallback callback  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		CAST_DCL( NuDCL*, dcl )->SetCallback( callback ) ;
		
		return kIOReturnSuccess ;
	}
	
	NuDCLCallback
	NuDCLPoolCOM::S_GetDCLCallback( NuDCLRef dcl  )
	{
		CHECK_DCL_NULL( NuDCL*,  dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->GetCallback() ;
	}

	IOReturn
	NuDCLPoolCOM::S_SetDCLUserHeaderPtr( NuDCLRef dcl, UInt32 * headerPtr, UInt32 * mask )
	{
		CHECK_DCL_IORETURN( SendNuDCL*, dcl ) ;
		
		CAST_DCL( SendNuDCL*, dcl )->SetUserHeaderPtr( headerPtr, mask ) ;
		
		return kIOReturnSuccess ;
	}
	
	UInt32*
	NuDCLPoolCOM::S_GetDCLUserHeaderPtr ( NuDCLRef dcl )
	{
		CHECK_DCL_NULL( SendNuDCL*, dcl ) ;
		
		return CAST_DCL( SendNuDCL*, dcl )->GetUserHeaderPtr() ;
	}
	
	UInt32 *
	NuDCLPoolCOM::S_GetDCLUserHeaderMaskPtr ( NuDCLRef dcl )
	{
		CHECK_DCL_NULL( SendNuDCL*, dcl ) ;

		return CAST_DCL( SendNuDCL*, dcl )->GetUserHeaderMask() ;
	}

	IOReturn
	NuDCLPoolCOM::S_SetDCLStatusPtr( NuDCLRef dcl, UInt32* statusPtr  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		CAST_DCL( NuDCL*, dcl )->SetStatusPtr( statusPtr ) ;
		
		return kIOReturnSuccess ;
	}
	
	UInt32*
	NuDCLPoolCOM::S_GetDCLStatusPtr( NuDCLRef dcl )
	{
		CHECK_DCL_NULL( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->GetStatusPtr() ;
	}
	
	void
	NuDCLPoolCOM::S_SetDCLRefcon( NuDCLRef dcl, void* refcon  )
	{
		CHECK_DCL( NuDCL*, dcl ) ;
		
		CAST_DCL( NuDCL*, dcl )->SetRefcon( refcon ) ;
	}
	
	void*
	NuDCLPoolCOM::S_GetDCLRefcon( NuDCLRef dcl  )
	{
		CHECK_DCL_NULL( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->GetRefcon() ;
	}

	IOReturn
	NuDCLPoolCOM::S_SetDCLUpdateList( NuDCLRef dcl, CFSetRef dclList  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		CAST_DCL( NuDCL*, dcl )->SetUpdateList( dclList ) ;
		
		return kIOReturnSuccess ;
	}
	
	CFSetRef
	NuDCLPoolCOM::S_GetDCLUpdateList( NuDCLRef dcl )
	{
		CHECK_DCL_NULL( NuDCL*, dcl ) ;
		
		return CAST_DCL( NuDCL*, dcl )->GetUpdateList() ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_AppendDCLUpdateList( NuDCLRef dcl, NuDCLRef updateDCL  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		CHECK_DCL_IORETURN( NuDCL*, updateDCL ) ;
		
		CAST_DCL( NuDCL*, dcl )->AppendUpdateList( CAST_DCL( NuDCL*, updateDCL ) ) ;
		
		return kIOReturnSuccess ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_EmptyDCLUpdateList( NuDCLRef dcl  )
	{
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		CAST_DCL( NuDCL*, dcl )->EmptyUpdateList() ;
		
		return kIOReturnSuccess ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLWaitControl( NuDCLRef dcl, Boolean wait  )
	{
		CHECK_DCL_IORETURN( ReceiveNuDCL*, dcl ) ;
		
		return CAST_DCL( ReceiveNuDCL*, dcl )->SetWaitControl( (bool)wait ) ;
	}

	void
	NuDCLPoolCOM::S_SetDCLFlags( NuDCLRef dcl, UInt32 flags )
	{
		CHECK_DCL( NuDCL*, dcl ) ;
		
		CAST_DCL( NuDCL*, dcl )->SetFlags( flags ) ;
	}
	
	UInt32
	NuDCLPoolCOM::S_GetDCLFlags( NuDCLRef dcl )
	{
		CHECK_DCL_ZERO( NuDCL*, dcl ) ;
	
		return CAST_DCL( NuDCL*, dcl )->GetFlags() ;
	}

	IOReturn
	NuDCLPoolCOM::S_SetDCLSkipBranch( NuDCLRef dcl, NuDCLRef skipCycleDCL )
	{
		CHECK_DCL_IORETURN( SendNuDCL*, dcl ) ;
		CHECK_DCL_IORETURN( NuDCL*, dcl ) ;
		
		CAST_DCL( SendNuDCL*, dcl )->SetSkipBranch( CAST_DCL( NuDCL*, skipCycleDCL ) ) ;
		return kIOReturnSuccess ;
	}
	
	NuDCLRef
	NuDCLPoolCOM::S_GetDCLSkipBranch( NuDCLRef dcl )
	{
		CHECK_DCL_NULL( SendNuDCL *, dcl ) ;
		
		return reinterpret_cast<NuDCLRef>( CAST_DCL( SendNuDCL *, dcl )->GetSkipBranch() ) ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLSkipCallback( NuDCLRef dcl, NuDCLCallback callback )
	{
		CHECK_DCL_IORETURN( SendNuDCL *, dcl ) ;
		
		CAST_DCL( SendNuDCL *, dcl)->SetSkipCallback( callback ) ;
		return kIOReturnSuccess ;
	}
	
	NuDCLCallback
	NuDCLPoolCOM::S_GetDCLSkipCallback( NuDCLRef dcl )
	{
		CHECK_DCL_NULL( SendNuDCL *, dcl ) ;
		
		return CAST_DCL( SendNuDCL *, dcl )->GetSkipCallback() ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLSkipRefcon( NuDCLRef dcl, void * refcon )
	{
		CHECK_DCL_IORETURN( SendNuDCL *, dcl ) ;
		
		CAST_DCL( SendNuDCL *, dcl )->SetSkipRefcon( refcon ) ;
		return kIOReturnSuccess ;
	}
	
	void *
	NuDCLPoolCOM::S_GetDCLSkipRefcon( NuDCLRef dcl )
	{
		CHECK_DCL_NULL( SendNuDCL *, dcl ) ;
		
		return CAST_DCL( SendNuDCL *, dcl )->GetSkipRefcon() ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLSyncBits( NuDCLRef dcl, UInt8 syncBits )
	{
		CHECK_DCL_IORETURN( SendNuDCL *, dcl ) ;
		
		CAST_DCL( SendNuDCL *, dcl )->SetSync( syncBits ) ;
		return kIOReturnSuccess ;
	}
	
	UInt8
	NuDCLPoolCOM::S_GetDCLSyncBits( NuDCLRef dcl )
	{
		CHECK_DCL_ZERO( SendNuDCL*, dcl ) ;
		
		return CAST_DCL( SendNuDCL *, dcl )->GetSync() ;
	}
	
	IOReturn
	NuDCLPoolCOM::S_SetDCLTagBits( NuDCLRef dcl, UInt8 tagBits )
	{
		CHECK_DCL_IORETURN( SendNuDCL *, dcl ) ;
		
		CAST_DCL( SendNuDCL *, dcl )->SetTag( tagBits ) ;
		return kIOReturnSuccess ;
	}
	
	UInt8
	NuDCLPoolCOM::S_GetDCLTagBits( NuDCLRef dcl )
	{
		CHECK_DCL_ZERO( SendNuDCL *, dcl ) ;

		return CAST_DCL( SendNuDCL *, dcl )->GetTag() ;
	}
	
} // namespace
