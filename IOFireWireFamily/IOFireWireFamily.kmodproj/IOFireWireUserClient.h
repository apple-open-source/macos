/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * $Log: IOFireWireUserClient.h,v $
 * Revision 1.45  2004/01/22 01:49:59  niels
 * fix user space physical address space getPhysicalSegments
 *
 * Revision 1.44  2003/12/19 22:07:46  niels
 * send force stop when channel dies/system sleeps
 *
 * Revision 1.43  2003/11/07 21:24:28  niels
 * *** empty log message ***
 *
 * Revision 1.42  2003/11/07 21:01:18  niels
 * *** empty log message ***
 *
 * Revision 1.41  2003/11/03 19:11:35  niels
 * fix local config rom reading; fix 3401223
 *
 * Revision 1.40  2003/08/20 18:48:43  niels
 * *** empty log message ***
 *
 * Revision 1.39  2003/08/08 21:03:27  gecko1
 * Merge max-rec clipping code into TOT
 *
 * Revision 1.38  2003/07/29 22:49:22  niels
 * *** empty log message ***
 *
 * Revision 1.37  2003/07/22 10:49:47  niels
 * *** empty log message ***
 *
 * Revision 1.36  2003/07/21 06:52:59  niels
 * merge isoch to TOT
 *
 * Revision 1.35.14.3  2003/07/21 06:44:45  niels
 * *** empty log message ***
 *
 * Revision 1.35.14.2  2003/07/18 00:17:42  niels
 * *** empty log message ***
 *
 * Revision 1.35.14.1  2003/07/01 20:54:07  niels
 * isoch merge
 *
 *
 */

#import "FWDebugging.h"
#import "IOFireWireLibPriv.h"
#import <IOKit/IOMemoryCursor.h>
#import <IOKit/IOUserClient.h>

using namespace IOFireWireLib ;

//struct AsyncRefHolder
//{
//	OSAsyncReference	asyncRef ;
//	void*				userRefCon ;
//	void*				obj ;
//} ;

class IOFireWireDevice;
class IOFWIsochChannel ;
class IOFWUserObjectExporter ;
class IOFireWireUserClient ;
class IOFWBufferFillIsochPort ;
#if IOFIREWIREUSERCLIENTDEBUG > 0
class IOFWUserDebugInfo : public OSObject
{
	OSDeclareDefaultStructors( IOFWUserDebugInfo ) ;

	private :
	
		IOFireWireUserClient *		fUserClient ;
//		OSNumber *					fIsochCallbacks ;
		
	public:
	
		virtual bool		init( IOFireWireUserClient & userClient ) ;
		virtual bool		serialize( OSSerialize * s ) const ;
		virtual void		free () ;
		
//		inline OSNumber *   getIsochCallbackCounter()			{ return fIsochCallbacks ; }
} ;
#endif

#pragma mark -

class IOFireWireUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFireWireUserClient)

	friend class IOFWUserDebugInfo ;

	private:

		struct ExternalMethod 
		{
			UInt32			objectTableLookupIndex ;
			IOMethod		func;
			IOOptionBits	flags;
			IOByteCount		count0;
			IOByteCount		count1;
		};
		
		struct ExternalAsyncMethod 
		{
			UInt32			objectTableLookupIndex ;
			IOAsyncMethod	func;
			IOOptionBits	flags;
			IOByteCount		count0;
			IOByteCount		count1;
		};

		static const ExternalMethod			sMethods[ kNumMethods ] ;
		static const ExternalAsyncMethod	sAsyncMethods[ kNumAsyncMethods ] ;

		IOService *							fObjectTable[3] ;
		task_t								fTask;
		const IOExternalMethod *			fMethods ;
		const IOExternalAsyncMethod *		fAsyncMethods ;
		IOFWUserObjectExporter *			fExporter ;
		mach_port_t							fNotificationPort ;
		UInt32								fNotificationRefCon ;
		OSAsyncReference					fBusResetAsyncNotificationRef ;
		OSAsyncReference					fBusResetDoneAsyncNotificationRef ;
		IONotifier*							fNotifier ;
		IOService*							fOpenClient ;
		bool								fUnsafeResets ;
		IOFireWireNub *						fOwner ;

		bool								fClippedMaxRec;
	
		unsigned							fSelfOpenCount ;

#if IOFIREWIREUSERCLIENTDEBUG > 0
		IOFWUserDebugInfo *					fDebugInfo ;
#endif

	public:
	
		// OSObject
//		virtual void 					retain() const	;
		virtual void					free () ;
		
		// IORegistryEntry
		virtual IOReturn				setProperties ( OSObject * properties ) ;

		// IOService
		virtual bool 					start ( IOService * provider );
		virtual void 					stop ( IOService * provider );
		virtual IOReturn 				message(
												UInt32 					type,
												IOService* 				provider,
												void* 					argument );

		// IOUserClient
		virtual IOReturn 				clientClose ( void );
		virtual IOReturn 				clientDied ( void );	
		inline static IOReturn 			sendAsyncResult (
												OSAsyncReference 		reference,
												IOReturn 				result, 
												void*					args[], 
												UInt32 					numArgs)
												{ return IOUserClient::sendAsyncResult(reference, result, args, numArgs) ; }									
		inline static void 				setAsyncReference (
												OSAsyncReference 		asyncRef,
												mach_port_t 			wakePort,
												void*					callback, 
												void*					refcon)
												{ IOUserClient::setAsyncReference(asyncRef, wakePort, callback, refcon) ; }
		IOExternalMethod *				getTargetAndMethodForIndex(
												IOService **			target, 
												UInt32 					index) ;
		IOExternalAsyncMethod *			getAsyncTargetAndMethodForIndex (
												IOService **			target, 
												UInt32 					index) ;
		IOReturn						registerNotificationPort ( 
												mach_port_t 			port, 
												UInt32 					type, 
												UInt32 					refCon ) ;

		// me
		
		static IOFireWireUserClient*	withTask( task_t owningTask ) ;
//		void							deallocateSets () ; 
		const task_t					getOwningTask () const {return fTask;}
		IOFireWireNub *					getOwner ()	const				{ return (IOFireWireNub*) getProvider() ; }

#pragma mark -
		// --- startup ----------
		void							initMethodTable() ;
		void							initAsyncMethodTable() ;
	
#pragma mark -
		// --- open/close ----------
		IOReturn						userOpen() ;
		IOReturn						userOpenWithSessionRef(IOService*	session) ;
		IOReturn						seize(IOOptionBits inFlags ) ;
		IOReturn						userClose() ;
		
#pragma mark -
		// --- utils ----------
		IOReturn						copyToUserBuffer (
												IOVirtualAddress 		kernelBuffer,
												IOVirtualAddress		userBuffer,
												IOByteCount 			bytes,
												IOByteCount & 			bytesCopied ) ;
		IOReturn						copyUserData (
												IOVirtualAddress 		userBuffer, 
												IOVirtualAddress		kernBuffer, 
												IOByteCount 			bytes ) const ;
	
#pragma mark -
		// --- read/write/lock ----------------
		IOReturn						readQuad ( const ReadQuadParams* inParams, UInt32* outVal ) ;
		IOReturn		 				read ( const ReadParams* inParams, IOByteCount* outBytesTransferred ) ;
		IOReturn		 				writeQuad ( const WriteQuadParams* inParams ) ;
		IOReturn		 				write ( const WriteParams* inParams, IOByteCount* outBytesTransferred ) ;
		IOReturn		 				compareSwap ( const CompareSwapParams* inParams, UInt64* oldVal) ;
	
#pragma mark -
		// --- other -----------------
		IOReturn		 				busReset ();
		IOReturn						getGenerationAndNodeID(
												UInt32*					outGeneration,
												UInt32*					outNodeID) const ;
		IOReturn						getLocalNodeID(
												UInt32*					outLocalNodeID) const ;
		IOReturn						getResetTime(
												AbsoluteTime*			outResetTime) const ;
		IOReturn						releaseUserObject (
														UserObjectHandle		obj ) ;
#pragma mark -
		// --- my conversion helpers -------
		IOReturn						getOSStringData(
												UserObjectHandle			inStringRef,
												UInt32					inStringLen,
												char*					inStringBuffer,
												UInt32*					outStringLen) ;
		IOReturn						getOSDataData(
												UserObjectHandle			inDataRef,
												IOByteCount				inDataLen,
												char*					inDataBuffer,
												IOByteCount*			outDataLen) ;
		
#pragma mark -
		// local config directory
		IOReturn						localConfigDirectory_Create ( 
														UserObjectHandle *			dir ) ;
		IOReturn						localConfigDirectory_addEntry_Buffer ( 
														UserObjectHandle		dirHandle, 
														int 					key, 
														char * 					buffer, 
														UInt32 					kr_size,
														const char *			descCString,
														UInt32					descLen ) const ;
		IOReturn						localConfigDirectory_addEntry_UInt32 ( 
														UserObjectHandle 		dirHandle, 
														int 					key, 
														UInt32 					value,
														const char *			descCString,
														UInt32					descLen ) const ;
		IOReturn						localConfigDirectory_addEntry_FWAddr ( 
														UserObjectHandle	dirHandle, 
														int					key, 
														const char *		descCString,
														UInt32				descLen,
														FWAddress *			value ) const ;
		IOReturn						localConfigDirectory_addEntry_UnitDir ( 
														UserObjectHandle	dirHandle, 
														int 				key, 
														UserObjectHandle 	valueHandle,
														const char *		descCString,
														UInt32				descLen ) const ;
		IOReturn						localConfigDirectory_Publish (
														UserObjectHandle			dir ) const ;
		IOReturn						localConfigDirectory_Unpublish (
														UserObjectHandle			dir ) const ;

#pragma mark -
		// Address Spaces
		IOReturn						addressSpace_Create ( 
												AddressSpaceCreateParams *	params, 
												UserObjectHandle *			outAddressSpaceHandle ) ;
		IOReturn						addressSpace_GetInfo ( 
												UserObjectHandle 		addrSpace, 
												AddressSpaceInfo *		outInfo ) ;
		IOReturn						addressSpace_ClientCommandIsComplete (
												UserObjectHandle		inAddrSpaceRef,
												FWClientCommandID		inCommandID,
												IOReturn				inResult ) ;	
		IOReturn						setAsyncRef_Packet( OSAsyncReference asyncRef, UserObjectHandle addrSpace,
												void* callback, void* userRefCon, void*, void*, void* ) ;
		IOReturn						setAsyncRef_SkippedPacket(
												OSAsyncReference		asyncRef,
												UserObjectHandle		inAddrSpaceRef,
												void*					inCallback,
												void*					inUserRefCon,
												void*,
												void*,
												void*) ;
		IOReturn						setAsyncRef_Read(
												OSAsyncReference		asyncRef,
												UserObjectHandle		inAddrSpaceRef,
												void*					inCallback,
												void*					inUserRefCon,
												void*,
												void*,
												void*) ;
		IOReturn						setAsyncRef_BusReset(
												OSAsyncReference		asyncRef,
												void*					inCallback,
												void*					inUserRefCon,
												void*,
												void*,
												void*,
												void*) ;
		IOReturn						setAsyncRef_BusResetDone(
												OSAsyncReference		asyncRef,
												void*					inCallback,
												void*					inUserRefCon,
												void*,
												void*,
												void*,
												void*) ;

#pragma mark -
		// physical address space stuff
		IOReturn						physicalAddressSpace_Create ( 
												UInt32						size,
												void *						backingStore,
												UInt32						flags,
												UserObjectHandle * 			outKernAddrSpaceRef ) ;
		IOReturn						physicalAddressSpace_GetSegments (
												UserObjectHandle			addressSpaceHandle,
												UInt32								inSegmentCount,
												IOMemoryCursor::IOPhysicalSegment *	outSegments,
												UInt32*								outSegmentCount) ;

#pragma mark -
		// async commands
		IOReturn						userAsyncCommand_Submit(
												OSAsyncReference					asyncRef,
												CommandSubmitParams *				inParams,
												CommandSubmitResult *				outResult,
												IOByteCount							inParamsSize,
												IOByteCount*						outResultSize) ;
		static void						userAsyncCommand_ReadWriteCompletion (
												void *								refcon, 
												IOReturn 							status, 
												IOFireWireNub *						device, 
												IOFWCommand *						fwCmd ) ;
		
#pragma mark -
		// config directory functions
		IOReturn		 				configDirectory_Create (
														UserObjectHandle *			outDirRef ) ;
		IOReturn		 				configDirectory_GetKeyType ( 
														UserObjectHandle			dirRef, 
														int 						key, 
														IOConfigKeyType * 			type ) const ;
		IOReturn						configDirectory_GetKeyValue_UInt32 (
														UserObjectHandle	 		dirHandle, 
														int 						key,
														UInt32 						wantText, 
														UInt32 * 					outValue, 
														UserObjectHandle *			outStringHandle, 
														UInt32 * 					outStringLen ) const ;
		IOReturn		 				configDirectory_GetKeyValue_Data ( 
														UserObjectHandle 			inDirRef, 
														int 						key, 
														UInt32 						wantText,
														GetKeyValueDataResults * 	results ) const ;
		IOReturn		 				configDirectory_GetKeyValue_ConfigDirectory ( 
														UserObjectHandle 			dirRef,
														int 						key, 
														UInt32 						wantText, 
														UserObjectHandle *			outValue, 
														UserObjectHandle * 			outString, 
														UInt32 * 					outStringLen ) const ;
		IOReturn		 				configDirectory_GetKeyOffset_FWAddress( 
														UserObjectHandle 			inDirRef, 
														int							key, 
														UInt32 						wantText, 
														GetKeyOffsetResults * 		results ) const ;
		IOReturn		 				configDirectory_GetIndexType( 
														UserObjectHandle			inDirRef, 
														int							index, 
														IOConfigKeyType * 			outType ) const ;
		IOReturn		 				configDirectory_GetIndexKey( 
														UserObjectHandle 			dirRef, 
														int							index, 
														int * 						outKey ) const ;
		IOReturn		 				configDirectory_GetIndexValue_UInt32( 
														UserObjectHandle 			dirRef, 
														int							index, 
														UInt32 * 					outKey) const ;
		IOReturn		 				configDirectory_GetIndexValue_Data( 
														UserObjectHandle			dirRef, 
														int							index, 
														UserObjectHandle *			outDataRef, 
														IOByteCount *				outDataLen ) const ;
		IOReturn		 				configDirectory_GetIndexValue_String( 
														UserObjectHandle			dirRef, 
														int							index, 
														UserObjectHandle *			outString, 
														UInt32 *					outStringLen ) const;
		IOReturn		 				configDirectory_GetIndexValue_ConfigDirectory( 
												UserObjectHandle					dirRef,
												int									index, 
												UserObjectHandle *					outDirRef ) const ;
		IOReturn		 				configDirectory_GetIndexOffset_FWAddress ( 
														UserObjectHandle			inDirRef,
														int							index, 
														FWAddress *					address ) const ;
		IOReturn		 				configDirectory_GetIndexOffset_UInt32 ( 
														UserObjectHandle			inDirRef,
														int							index, 
														UInt32 *					outValue ) const ;
		IOReturn		 				configDirectory_GetIndexEntry ( 
														UserObjectHandle			dirRef, 
														int							index,
														UInt32 *					outValue ) const ;
		IOReturn		 				configDirectory_GetSubdirectories (
														UserObjectHandle			dirHandle,
														UserObjectHandle *			outIteratorHandle ) const ;
		IOReturn		 				configDirectory_GetKeySubdirectories (
														UserObjectHandle			dirHandle ,
														int							key,
														UserObjectHandle*			outIteratorHandle ) const ;
		IOReturn		 				configDirectory_GetType ( 
														UserObjectHandle			dirRef, 
														int *						outType ) const ;
		IOReturn		 				configDirectory_GetNumEntries ( 
														UserObjectHandle			dirRef, 
														int *						outNumEntries ) const ;

#pragma mark -
		// local isoch port
		IOReturn						localIsochPort_GetSupported (
														UserObjectHandle			inPortRef,
														IOFWSpeed *					outMaxSpeed,
														UInt32 *					outChanSupportedHi,
														UInt32 *					outChanSupportedLo ) const ;
		IOReturn						localIsochPort_Create ( 
														LocalIsochPortAllocateParams * params, 
														UserObjectHandle *			portRef ) ;
		IOReturn						localIsochPort_SetChannel (
														UserObjectHandle			portHandle,
														UserObjectHandle			channelHandle ) ;
		IOReturn						setAsyncRef_DCLCallProc ( 
														OSAsyncReference			asyncRef, 
														UserObjectHandle			portRef ) ;
		
#pragma mark -
		// isoch channel
		static IOReturn					s_IsochChannel_ForceStopHandler(
												void*					refCon,
												IOFWIsochChannel*		isochChannelID,
												UInt32					stopCondition) ;
		IOReturn						isochChannel_Create (
												bool					inDoIRM,
												UInt32					inPacketSize,
												IOFWSpeed				inPrefSpeed,
												UserObjectHandle*	outIsochChannelRef) ;
		IOReturn						isochChannel_AllocateChannelBegin( 
												UserObjectHandle		channelRef,
												UInt32 					speed, 
												UInt32 					chansHi, 
												UInt32					chansLo,
												UInt32 *				outSpeed,
												UInt32 *				outChannel ) ;
		IOReturn						setAsyncRef_IsochChannelForceStop(
														OSAsyncReference		asyncRef,
														UserObjectHandle		channel ) ;
	
#pragma mark -
		// command objects
//		IOReturn						userAsyncCommand_Cancel( UserObjectHandle command, IOReturn reason ) 	{ return kIOReturnUnsupported; }
	
#pragma mark -
		// statistics
#if IOFIREWIREUSERCLIENTDEBUG > 0
		inline IOFWUserDebugInfo *		getStatistics() const						{ return fDebugInfo ; }
#endif
	
#pragma mark -
		// absolute address firewire commands
		IOFWReadCommand*				createReadCommand( 
												UInt32				generation,
												FWAddress 			devAddress, 
												IOMemoryDescriptor*	hostMem,
												FWDeviceCallback 	completion,
												void *				refcon ) const ;
		IOFWReadQuadCommand*			createReadQuadCommand(
												UInt32				generation,
												FWAddress 			devAddress, 
												UInt32 *			quads, 
												int 				numQuads,
												FWDeviceCallback 	completion,
												void *				refcon ) const ;
		IOFWWriteCommand*				createWriteCommand( 
												UInt32				generation,
												FWAddress 			devAddress, 
												IOMemoryDescriptor*	hostMem,
												FWDeviceCallback 	completion,
												void*				refcon ) const ;
		IOFWWriteQuadCommand*			createWriteQuadCommand( 
												UInt32				generation,
												FWAddress 			devAddress, 
												UInt32*				quads, 
												int 				numQuads,
												FWDeviceCallback 	completion,
												void *				refcon ) const ;
	
			// size is 1 for 32 bit compare, 2 for 64 bit.
		IOFWCompareAndSwapCommand*		createCompareAndSwapCommand( 
												UInt32				generation,
												FWAddress 			devAddress,
												const UInt32 *		cmpVal, 
												const UInt32 *		newVal, 
												int 				size,
												FWDeviceCallback 	completion, 
												void *				refcon ) const ;
	
#pragma mark -
		IOReturn	firelog( const char* string, IOByteCount bufSize ) const ;
		IOReturn 	getBusGeneration( UInt32* outGeneration ) ;
		IOReturn 	getLocalNodeIDWithGeneration( UInt32 generation, UInt32* outLocalNodeID ) ;
		IOReturn 	getRemoteNodeID( UInt32 generation, UInt32* outRemoteNodeID ) ;
		IOReturn 	getSpeedToNode( UInt32 generation, UInt32* outSpeed ) ;
		IOReturn 	getSpeedBetweenNodes( UInt32 generation, UInt32 fromNode, UInt32 toNode, UInt32* outSpeed ) ;
	
#pragma mark -
		//
		// v5
		//
		
		IOReturn						getIRMNodeID( UInt32 generation, UInt32* irmNodeID ) ;

#pragma mark -
		//
		// v6
		//
		
		IOReturn 						clipMaxRec2K( Boolean clipMaxRec ) ;

//		IOReturn						bufferFillIsochPort_Create(
//														BufferFillIsochPortCreateParams * params ) ;
									
		void							s_userBufferFillPacketProc( 
														IOFWBufferFillIsochPort *   port,
														IOVirtualRange				packets[],
														unsigned					packetCount ) ;
		//
		// v7
		//
		IOReturn						getSessionRef( IOFireWireSessionRef * sessionRef ) ;
} ;

