/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.68  2008/05/07 03:28:00  collin
 * 64 bit session ref support
 *
 * Revision 1.67  2007/10/16 16:50:21  ayanowit
 * Removed existing "work-in-progress" support for buffer-fill isoch.
 *
 * Revision 1.66  2007/04/28 02:54:23  collin
 * *** empty log message ***
 *
 * Revision 1.65  2007/04/24 02:50:08  collin
 * *** empty log message ***
 *
 * Revision 1.64  2007/03/12 22:15:28  arulchan
 * mach_vm_address_t & io_user_reference_t changes
 *
 * Revision 1.63  2007/03/10 07:48:25  collin
 * *** empty log message ***
 *
 * Revision 1.62  2007/03/10 04:15:25  collin
 * *** empty log message ***
 *
 * Revision 1.61  2007/03/10 02:58:03  collin
 * *** empty log message ***
 *
 * Revision 1.60  2007/03/09 23:57:53  collin
 * *** empty log message ***
 *
 * Revision 1.59  2007/03/08 18:13:56  ayanowit
 * Fix for 5047793. A problem where user-space CompareSwap() was not checking lock results.
 *
 * Revision 1.58  2007/03/08 02:37:09  collin
 * *** empty log message ***
 *
 * Revision 1.57  2007/02/16 00:28:25  ayanowit
 * More work on IRMAllocation APIs
 *
 * Revision 1.56  2007/02/09 20:36:46  ayanowit
 * More Leopard IRMAllocation changes.
 *
 * Revision 1.55  2007/02/06 01:08:41  ayanowit
 * More work on Leopard features such as new User-space IRM allocation APIs.
 *
 * Revision 1.54  2007/01/26 20:52:31  ayanowit
 * changes to user-space isoch stuff to support 64-bit apps.
 *
 * Revision 1.53  2007/01/24 04:10:14  collin
 * *** empty log message ***
 *
 * Revision 1.52  2006/12/21 21:17:44  ayanowit
 * More changes necessary to eventually get support for 64-bit apps working (4222965).
 *
 * Revision 1.51  2006/12/06 00:01:08  arulchan
 * Isoch Channel 31 Generic Receiver
 *
 * Revision 1.50  2006/11/29 18:42:53  ayanowit
 * Modified the IOFireWireUserClient to use the Leopard externalMethod method of dispatch.
 *
 * Revision 1.49  2006/02/09 00:21:51  niels
 * merge chardonnay branch to tot
 *
 * Revision 1.48  2005/09/24 00:55:28  niels
 * *** empty log message ***
 *
 * Revision 1.47  2005/03/31 02:31:44  niels
 * more object exporter fixes
 *
 * Revision 1.46  2005/03/30 22:14:55  niels
 * Fixed compile errors see on Tiger w/ GCC 4.0
 * Moved address-of-member-function calls to use OSMemberFunctionCast
 * Added owner field to IOFWUserObjectExporter
 * User client now cleans up published unit directories when client dies
 *
 * Revision 1.45.18.3  2006/01/31 04:49:51  collin
 * *** empty log message ***
 *
 * Revision 1.45.18.1  2005/08/17 03:33:57  collin
 * *** empty log message ***
 *
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
#import <IOKit/firewire/IOFWCommand.h>

using namespace IOFireWireLib ;

class IOFireWireDevice;
class IOFWIsochChannel ;
class IOFWUserObjectExporter ;
class IOFireWireUserClient ;
class IOFireWireNub ;
//class IOFWCommand ;
//class IOFWReadCommand ;
//class IOFWReadQuadCommand ;
//class IOFWWriteCommand ;
//class IOFWWriteQuadCommand ;
//class IOFWCompareAndSwapCommand ;

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

		IOService *							fObjectTable[3] ;
		task_t								fTask;
		const IOExternalMethod *			fMethods ;
		const IOExternalAsyncMethod *		fAsyncMethods ;
		IOFWUserObjectExporter *			fExporter ;
		mach_port_t							fNotificationPort ;
		UInt32								fNotificationRefCon ;
		OSAsyncReference64					fBusResetAsyncNotificationRef ;
		OSAsyncReference64					fBusResetDoneAsyncNotificationRef ;
		IONotifier*							fNotifier ;
		IOService*							fOpenClient ;
		bool								fUnsafeResets ;
		IOFireWireNub *						fOwner ;

		bool								fClippedMaxRec;
	
		unsigned							fSelfOpenCount ;

#if IOFIREWIREUSERCLIENTDEBUG > 0
		IOFWUserDebugInfo *					fDebugInfo ;
#endif

		IOFireWireLib::UserObjectHandle		fSessionRef;
	
	public:
	
		// OSObject
//		virtual void 					retain() const	;
		virtual void					free () ;
		
		// IORegistryEntry
		virtual IOReturn				setProperties ( OSObject * properties ) ;

		// IOService
		virtual bool					initWithTask( task_t owningTask, void * securityToken, UInt32 type, OSDictionary * properties );
		virtual bool 					start ( IOService * provider );
		virtual IOReturn 				message(
												UInt32 					type,
												IOService* 				provider,
												void* 					argument );

		// IOUserClient
	
		virtual IOReturn				externalMethod( uint32_t selector, 
														IOExternalMethodArguments * arguments, 
														IOExternalMethodDispatch * dispatch, 
														OSObject * target, 
														void * reference);
	
		virtual IOReturn 				clientClose ( void );
		virtual IOReturn 				clientDied ( void );	

		inline static IOReturn 			sendAsyncResult64 (OSAsyncReference64 		reference,
														   IOReturn 				result, 
														   io_user_reference_t      args[], 
														   UInt32 					numArgs)
														{ return IOUserClient::sendAsyncResult64(reference, result, args, numArgs) ; }									
	
		IOReturn						registerNotificationPort ( 
												mach_port_t 			port, 
												UInt32 					type, 
												UInt32 					refCon ) ;

		// me
		
//		void							deallocateSets () ; 
		const task_t					getOwningTask () const {return fTask;}
		IOFWUserObjectExporter *		getExporter()		{ return fExporter; }
		IOFireWireNub *					getOwner ()	const				{ return fOwner ; }

#pragma mark -
		// --- startup ----------
		void							initMethodTable() ;
		void							initAsyncMethodTable() ;
	
#pragma mark -
		// --- open/close ----------
		IOReturn						userOpen() ;
		IOReturn						userOpenWithSessionRef( IOFireWireLib::UserObjectHandle	session) ;
		IOReturn						seize(IOOptionBits inFlags ) ;
		IOReturn						userClose() ;
		
#pragma mark -
		// --- utils ----------
		IOReturn						copyToUserBuffer (
												IOVirtualAddress 		kernelBuffer,
												mach_vm_address_t		userBuffer,
												IOByteCount 			bytes,
												IOByteCount & 			bytesCopied ) ;
		IOReturn						copyUserData (
												mach_vm_address_t 		userBuffer, 
												mach_vm_address_t		kernBuffer, 
												mach_vm_size_t 			bytes ) const ;
	
#pragma mark -
		// --- read/write/lock ----------------
		IOReturn						readQuad ( const ReadQuadParams* inParams, UInt32* outVal ) ;
		IOReturn		 				read ( const ReadParams* inParams, IOByteCount* outBytesTransferred ) ;
		IOReturn		 				writeQuad ( const WriteQuadParams* inParams ) ;
		IOReturn		 				write ( const WriteParams* inParams, IOByteCount* outBytesTransferred ) ;
		IOReturn		 				compareSwap ( const CompareSwapParams* inParams, UInt32* oldVal) ;
	
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
												UserObjectHandle		inStringRef,
												UInt32					inStringLen,
												mach_vm_address_t		inStringBuffer,
												UInt32*					outStringLen) ;
		IOReturn						getOSDataData(
												UserObjectHandle		inDataRef,
												IOByteCount				inDataLen,
												mach_vm_address_t		inDataBuffer,
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

		IOReturn						setAsyncStreamRef_Packet (
												OSAsyncReference64		asyncRef,
												UserObjectHandle		asyncStreamListenerHandle,
												mach_vm_address_t		inCallback,
												io_user_reference_t		inUserRefCon,
												void*,
												void*,
												void* ) ;
												
		IOReturn						setAsyncStreamRef_SkippedPacket (
												OSAsyncReference64		asyncRef,
												UserObjectHandle		inAsyncStreamListenerRef,
												mach_vm_address_t		inCallback,
												io_user_reference_t		inUserRefCon,
												void*,
												void*,
												void*) ;
												
		IOReturn						setAsyncRef_Packet( 
												OSAsyncReference64		asyncRef, 
												UserObjectHandle		addrSpace,
												mach_vm_address_t		inCallback,
												io_user_reference_t		inUserRefCon,
												void*, 
												void*, 
												void* ) ;
												
		IOReturn						setAsyncRef_SkippedPacket(
												OSAsyncReference64		asyncRef,
												UserObjectHandle		inAddrSpaceRef,
												mach_vm_address_t		inCallback,
												io_user_reference_t		inUserRefCon,
												void*,
												void*,
												void*) ;
		IOReturn						setAsyncRef_Read(
												OSAsyncReference64		asyncRef,
												UserObjectHandle		inAddrSpaceRef,
												mach_vm_address_t		inCallback,
												io_user_reference_t		inUserRefCon,
												void*,
												void*,
												void*) ;
		IOReturn						setAsyncRef_BusReset(
												OSAsyncReference64		asyncRef,
												mach_vm_address_t		inCallback,
												io_user_reference_t		inUserRefCon,
												void*,
												void*,
												void*,
												void*) ;
		IOReturn						setAsyncRef_BusResetDone(
												OSAsyncReference64		asyncRef,
												mach_vm_address_t		inCallback,
												io_user_reference_t		inUserRefCon,
												void*,
												void*,
												void*,
												void*) ;

#pragma mark -
		// physical address space stuff
		IOReturn						physicalAddressSpace_Create ( 
												mach_vm_size_t				size,
												mach_vm_address_t			backingStore,
												UInt32						flags,
												UserObjectHandle * 			outKernAddrSpaceRef ) ;
		IOReturn						physicalAddressSpace_GetSegments (
												UserObjectHandle			addressSpaceHandle,
												UInt32						inSegmentCount,
												mach_vm_address_t			outSegments,
												UInt32*						outSegmentCount) ;

#pragma mark -
		// async commands
		IOReturn						userAsyncCommand_Submit(
												OSAsyncReference64					asyncRef,
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
														OSAsyncReference64			asyncRef, 
														UserObjectHandle			portRef ) ;
		
#pragma mark -
		// isoch channel
//		static IOReturn					s_IsochChannel_ForceStopHandler(
//												void*					refCon,
//												IOFWIsochChannel*		isochChannelID,
//												UInt32					stopCondition) ;
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
														OSAsyncReference64		asyncRef,
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

		//
		// v7
		//
		IOReturn						getSessionRef( IOFireWireSessionRef * sessionRef ) ;

		//
		// v8
		//
		IOReturn						asyncStreamListener_Create ( 
														FWUserAsyncStreamListenerCreateParams*	params, 
														UserObjectHandle*						outAsyncStreamListenerHandle );
		
		IOReturn						asyncStreamListener_ClientCommandIsComplete (
														UserObjectHandle		asyncStreamListenerHandle,
														FWClientCommandID		inCommandID );

		IOReturn						asyncStreamListener_GetOverrunCounter (
														UserObjectHandle		asyncStreamListenerHandle,
														UInt32					*overrunCounter );

		IOReturn						asyncStreamListener_SetFlags (
														UserObjectHandle		asyncStreamListenerHandle,
														UInt32					flags );

		IOReturn						asyncStreamListener_GetFlags (
														UserObjectHandle		asyncStreamListenerHandle,
														UInt32					*flags );
														
		IOReturn						asyncStreamListener_TurnOnNotification (
														UserObjectHandle		asyncStreamListenerHandle );

		IOReturn						asyncStreamListener_TurnOffNotification (
														UserObjectHandle		asyncStreamListenerHandle );
	
		IOReturn						allocateIRMBandwidthInGeneration(UInt32 bandwidthUnits, UInt32 generation) ;
	
		IOReturn						releaseIRMBandwidthInGeneration(UInt32 bandwidthUnits, UInt32 generation) ;
	
		IOReturn						allocateIRMChannelInGeneration(UInt8 isochChannel, UInt32 generation) ;
	
		IOReturn						releaseIRMChannelInGeneration(UInt8 isochChannel, UInt32 generation) ;
	
		IOReturn						irmAllocation_Create(Boolean releaseIRMResourcesOnFree, UserObjectHandle* outIRMAllocationHandle);	

		IOReturn						irmAllocation_AllocateResources(UserObjectHandle irmAllocationHandle, UInt8 isochChannel, UInt32 bandwidthUnits);

		IOReturn						irmAllocation_DeallocateResources(UserObjectHandle irmAllocationHandle);

		Boolean							irmAllocation_areResourcesAllocated(UserObjectHandle irmAllocationHandle, UInt8 *pIsochChannel, UInt32 *pBandwidthUnits);

		void							irmAllocation_setDeallocateOnRelease(UserObjectHandle irmAllocationHandle, Boolean doDeallocationOnRelease);
	
		IOReturn						irmAllocation_setRef(OSAsyncReference64 asyncRef,
															 UserObjectHandle		irmAllocationHandle,
															 io_user_reference_t	inCallback,
															 io_user_reference_t	inUserRefCon);
		
		IOReturn						createAsyncCommand(	OSAsyncReference64 asyncRef,
															CommandSubmitParams * params,
															UserObjectHandle * kernel_ref );

		IOReturn						createVectorCommand( UserObjectHandle * kernel_ref );
	
	public:
		static void						setAsyncReference64(OSAsyncReference64 asyncRef,
											mach_port_t wakePort,
											mach_vm_address_t callback, io_user_reference_t refcon)
										{
											// why is this protected?
											// hack to make it public
											IOUserClient::setAsyncReference64( asyncRef, wakePort, callback, refcon );
										}

		IOReturn						createPHYPacketListener( UInt32 queue_count, UserObjectHandle * kernel_ref );

} ;

