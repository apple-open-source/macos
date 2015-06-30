/*
 *  IOFireWireLibNuDCL.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Thu Feb 27 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: not supported by cvs2svn $
 *	Revision 1.8  2007/01/26 20:52:32  ayanowit
 *	changes to user-space isoch stuff to support 64-bit apps.
 *	
 *	Revision 1.7  2006/02/09 00:21:55  niels
 *	merge chardonnay branch to tot
 *	
 *	Revision 1.6.20.1  2006/01/17 00:35:00  niels
 *	<rdar://problem/4399365> FireWire NuDCL APIs need Rosetta support
 *	
 *	Revision 1.6  2003/08/25 08:39:17  niels
 *	*** empty log message ***
 *	
 *	Revision 1.5  2003/08/22 18:15:17  niels
 *	*** empty log message ***
 *	
 *	Revision 1.4  2003/08/20 18:48:45  niels
 *	*** empty log message ***
 *	
 *	Revision 1.3  2003/08/14 17:47:33  niels
 *	*** empty log message ***
 *	
 *	Revision 1.2  2003/07/21 06:53:11  niels
 *	merge isoch to TOT
 *	
 *	Revision 1.1.2.4  2003/07/18 00:17:48  niels
 *	*** empty log message ***
 *	
 *	Revision 1.1.2.3  2003/07/11 18:15:36  niels
 *	*** empty log message ***
 *	
 *	Revision 1.1.2.2  2003/07/09 21:24:07  niels
 *	*** empty log message ***
 *	
 *	Revision 1.1.2.1  2003/07/01 20:54:24  niels
 *	isoch merge
 *	
 */

#import "IOFireWireFamilyCommon.h"
//#import "IOFireWireLibDevice.h"

#ifndef KERNEL
#import <CoreFoundation/CoreFoundation.h>
#endif

#ifdef KERNEL
// this file is both user and kernel, and in user space we use CFMutableSet...
// In the kernel, we just make this the same as a void *

typedef void * CFMutableSetRef ;
#endif

namespace IOFireWireLib {

	enum NuDCLType {
		kReserved = 0,
		kSendNuDCLType,
		kReceiveNuDCLType,
		kSkipCycleNuDCLType
	} ;
	
	class NuDCL ;
	class NuDCLSharedData
	{
		public :
			
			enum Type
			{
				kSendType = 'send',
				kReceiveType = ' rcv',
				kSkipCycleType = 'skip'
			} ;
		
			Type				type ;
			union
			{
				NuDCL *				dcl ;
				unsigned			index ;
			} branch ;
			
			NuDCLCallback		callback ;
			union
			{
				UInt32 *				ptr ;
				IOByteCount				offset ;
			} timeStamp ;
			
			UInt32				rangeCount ;
			IOVirtualRange		ranges[6] ;
			
			union 
			{
				CFMutableSetRef		set ;		// In user space contains update set
				UInt32				count ;		// In kernel, contains number of DCLs in update list following this DCL 
												// in export data
			} update ;
			
			union
			{
				UInt32 *			ptr ;
				IOByteCount			offset ;
			} status ;
			
			void*				refcon ;
			UInt32				flags ;
			
			inline NuDCLSharedData( Type type ) ;
	} ;

	typedef struct NuDCLExportDataStruct
	{
		UInt32 type;
		uint64_t branchIndex;
		mach_vm_address_t callback;
		uint64_t timeStampOffset;
		UInt32 rangeCount;
		IOAddressRange ranges[6];
		uint64_t updateCount;
		uint64_t statusOffset;
		mach_vm_address_t refcon;
		UInt32 flags;
	} __attribute__ ((packed)) NuDCLExportData;
	
	class ReceiveNuDCLSharedData
	{
		public :
		
			UInt8		headerBytes ;
			UInt8		wait ;
			
			inline ReceiveNuDCLSharedData() : headerBytes( 0 ), wait( false ) {}
	} ;

	typedef struct ReceiveNuDCLExportDataStruct
	{
		UInt8		headerBytes ;
		UInt8		wait ;
	} __attribute__ ((packed)) ReceiveNuDCLExportData;
	
	class SendNuDCLSharedData
	{
		public :
		
			union
			{
				UInt32 *			ptr ;
				IOByteCount			offset ;
			} userHeader ;
			
			union
			{
				UInt32 *			ptr ;
				IOByteCount			offset ;
			} userHeaderMask ;
			
			union
			{
				NuDCL *				dcl ;
				unsigned			index ;
			} skipBranch ;
			
			NuDCLCallback		skipCallback ;
			void *				skipRefcon ;
			UInt8				syncBits ;
			UInt8				tagBits ;
	
			inline SendNuDCLSharedData() 
			: skipCallback( 0 )
			, syncBits( 0 )
			, tagBits( 0 )
			{
				userHeader.ptr = NULL ;
				userHeaderMask.ptr = NULL ;
				skipBranch.dcl = NULL ;
			}
	} ;

	typedef struct SendNuDCLExportDataStruct
	{
		uint64_t userHeaderOffset;
		uint64_t userHeaderMaskOffset;
		uint64_t skipBranchIndex;
		mach_vm_address_t skipCallback;
		mach_vm_address_t skipRefcon ;
		UInt8 syncBits ;
		UInt8 tagBits ;
	} __attribute__ ((packed)) SendNuDCLExportData;
	
#ifndef KERNEL	
	class CoalesceTree ;
	class NuDCLPool ;
	
#pragma mark -
	class NuDCL
	{
		protected:
		
			NuDCLSharedData		fData ;
			unsigned			fExportIndex ;		// index of this DCL in export chunk + 1
			NuDCLPool &			fPool ;
			
		public:
		
			NuDCL( NuDCLPool & pool, UInt32 numRanges, IOVirtualRange ranges[], NuDCLSharedData::Type type ) ;
			virtual ~NuDCL() ;
		
		public:

			void					SetBranch ( NuDCL* branch )						{ fData.branch.dcl = branch ; }
			NuDCL*					GetBranch () const								{ return fData.branch.dcl ; }
			void					SetTimeStampPtr ( UInt32* timeStampPtr )		{ fData.timeStamp.ptr = timeStampPtr ; }
			UInt32*					GetTimeStampPtr () const						{ return fData.timeStamp.ptr ; }
			void					SetCallback ( NuDCLCallback callback )			{ fData.callback = callback ; }
			NuDCLCallback			GetCallback () const							{ return fData.callback ; }
			void					SetStatusPtr ( UInt32* statusPtr )				{ fData.status.ptr = statusPtr ; }
			UInt32*					GetStatusPtr () const							{ return fData.status.ptr ; }
			void					SetRefcon ( void* refcon )						{ fData.refcon = refcon ; }
			void*					GetRefcon ()									{ return fData.refcon ; }
			CFSetRef				GetUpdateList ()								{ return fData.update.set ; }
			
			virtual IOReturn		AppendRanges ( UInt32 numRanges, IOVirtualRange ranges[] ) ;
			virtual IOReturn		SetRanges ( UInt32 numRanges, IOVirtualRange ranges[] ) ;
			UInt32					GetRanges ( UInt32 maxRanges, IOVirtualRange ranges[] ) const ;
			UInt32					CountRanges () const							{ return fData.rangeCount ; }
			virtual IOReturn		GetSpan ( IOVirtualRange& result ) const ;
			virtual IOByteCount		GetSize () const ;
			IOReturn				AppendUpdateList ( NuDCL* updateDCL ) ;
			IOReturn				SetUpdateList ( CFSetRef updateList ) ;
			void					EmptyUpdateList () ;
			void					SetFlags( UInt32 flags )						{ fData.flags = flags ; }
			UInt32					GetFlags() const								{ return fData.flags ; }

			virtual void		 	Print ( FILE* file ) const ;
			void					CoalesceBuffers ( CoalesceTree & tree ) const ;
			virtual IOByteCount		Export ( 
													IOVirtualAddress * 		where,
													IOVirtualRange			bufferRanges[],
													unsigned				bufferRangesCount ) const ;
			unsigned				GetExportIndex() const							{ return fExportIndex ; }
			void					SetExportIndex( unsigned index )				{ fExportIndex = index ; }
			
		protected :
		
			static void				S_NuDCLKernelCallout( NuDCL * dcl, UserObjectHandle kernProgramRef ) ;
	} ;
	
#pragma mark -
	class ReceiveNuDCL : public NuDCL
	{
		private:
		
			ReceiveNuDCLSharedData	fReceiveData ;
			
		public:
		
			ReceiveNuDCL( NuDCLPool & pool, UInt8 headerBytes, UInt32 numRanges, IOVirtualRange ranges[] ) ;
		
		public:
		
			IOReturn				SetWaitControl ( bool wait ) ;

			virtual void		 	Print ( FILE* file ) const ;
//			virtual IOByteCount		GetExportSize ( void ) const								{ return NuDCL::GetExportSize() + sizeof ( fReceiveData ) ; }
			virtual IOByteCount		Export ( 
													IOVirtualAddress * 		where,
													IOVirtualRange			bufferRanges[],
													unsigned				bufferRangesCount ) const ;
	} ;
	
#pragma mark -
	class SendNuDCL : public NuDCL
	{
		private:
		
			SendNuDCLSharedData		fSendData ;
			
		public:
		
			SendNuDCL( NuDCLPool & pool, UInt32 numRanges, IOVirtualRange ranges[] ) ;

		public:
		
			void					SetUserHeaderPtr ( UInt32 * userHeaderPtr, UInt32 * mask )	{ fSendData.userHeader.ptr = userHeaderPtr ; fSendData.userHeaderMask.ptr = mask ;  }
			UInt32 *				GetUserHeaderPtr ()	const									{ return fSendData.userHeader.ptr ; }
			UInt32 *				GetUserHeaderMask () const									{ return fSendData.userHeaderMask.ptr ; }

			void					SetSkipBranch( NuDCL * skipBranchDCL )						{ fSendData.skipBranch.dcl = skipBranchDCL ; }
			NuDCL *					GetSkipBranch() const										{ return fSendData.skipBranch.dcl ; }
			void					SetSkipCallback( NuDCLCallback callback )					{ fSendData.skipCallback = callback ; }
			NuDCLCallback			GetSkipCallback() const										{ return fSendData.skipCallback ; }
			void					SetSkipRefcon( void * refcon )								{ fSendData.skipRefcon = refcon ; }
			void *					GetSkipRefcon() const										{ return fSendData.skipRefcon ; }
			void					SetSync( UInt8 syncBits )									{ fSendData.syncBits = syncBits ; }
			UInt8					GetSync() const												{ return fSendData.syncBits ; }
			void					SetTag( UInt8 tagBits )										{ fSendData.tagBits = tagBits ; }
			UInt8					GetTag() const												{ return fSendData.tagBits ; }

			virtual void		 	Print ( FILE* file ) const ;
			virtual IOByteCount		Export ( 
													IOVirtualAddress * 		where,
													IOVirtualRange			bufferRanges[],
													unsigned				bufferRangesCount ) const ;
	} ;
	
#pragma mark -

	class SkipCycleNuDCL : public NuDCL
	{
		public:
		
			SkipCycleNuDCL( NuDCLPool & pool ): NuDCL( pool, 0, nil, NuDCLSharedData::kSkipCycleType ) {}
	
		public:
		
			virtual IOReturn		AddRange ( IOVirtualRange& range )							{ return kIOReturnUnsupported ; }
			virtual IOReturn		SetRanges ( UInt32 numRanges, IOVirtualRange ranges[] )		{ return kIOReturnUnsupported ; }
			UInt32					GetRanges ( UInt32 maxRanges, IOVirtualRange ranges[] ) const		{ return 0 ; }
			virtual IOReturn		GetSpan ( IOVirtualRange& result ) const					{ return kIOReturnUnsupported ; }
			virtual IOByteCount		GetSize () const											{ return 0 ; }		

			virtual void		 	Print ( FILE* file ) const ;
	} ;
#endif

	NuDCLSharedData::NuDCLSharedData( Type type )
	: type( type )
	, callback( NULL )
	, rangeCount( 0 )
	, refcon( 0 )
	{
		timeStamp.ptr = NULL ;
		branch.dcl = NULL ;
		update.set = 0 ;
		status.ptr = NULL ;
	}
	
} // namespace

