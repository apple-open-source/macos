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
 */


#ifndef _IOKIT_IOFIREWIREUSERCLIENT_H
#define _IOKIT_IOFIREWIREUSERCLIENT_H

// public
#import <IOKit/firewire/IOFWCommand.h>

// private
#import "IOFireWireLibPriv.h"

// system
#import <IOKit/IOUserClient.h>

#if IOFIREWIREUSERCLIENTDEBUG > 0
	#define IOFireWireUserClientLog_(x...) IOLog(x)
	#define IOFireWireUserClientLogIfNil_(x, y...) \
	{ if ((void*)(x) == NULL) { IOFireWireUserClientLog_(y); } }
	#define IOFireWireUserClientLogIfErr_(x, y...) \
	{ if ((x) != 0) { IOFireWireUserClientLog_(y); } }
	#define IOFireWireUserClientLogIfFalse_(x, y...) \
	{ if (!(x)) { IOFireWireUserClientLog_(y); } }
	#define IOFireWireUserClientLogIfTrue_(x, y...) \
	{ if ((x)) { IOFireWireUserClientLog_(y) ; } }
#else
	#define IOFireWireUserClientLog_(x...)
	#define IOFireWireUserClientLogIfNil_(x, y...)
	#define IOFireWireUserClientLogIfErr_(x, y...)
	#define IOFireWireUserClientLogIfFalse_(x, y...)
	#define IOFireWireUserClientLogIfTrue_(x, y...)
#endif

using namespace IOFireWireLib ;

typedef struct AsyncRefHolder_t
{
	OSAsyncReference	asyncRef ;
	void*				userRefCon ;
	void*				obj ;
} AsyncRefHolder ;

class IOFireWireUserClientStatistics
{
 public:
	OSDictionary*		dict ;

	OSNumber*			isochCallbacks ;
	OSSet*				pseudoAddressSpaces ;
} ;


class IOFireWireDevice;
class IOFWIsochChannel ;

class IOFireWireUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFireWireUserClient)

private:
    IOFireWireNub *			fOwner;
    task_t					fTask;
	IOExternalMethod		fMethods[ kNumMethods ];
	IOExternalAsyncMethod 	fAsyncMethods[ kNumAsyncMethods ];

	IOLock*					fSetLock ;

	OSSet*					fUserPseudoAddrSpaces ;	// all user allocated pseudo address spaces
	OSSet*					fUserPhysicalAddrSpaces ;
	OSSet*					fUserUnitDirectories ;
	OSSet*					fUserRemoteConfigDirectories ;
	OSSet*					fUserIsochChannels ;
	OSSet*					fUserIsochPorts ;
	OSSet*					fUserCommandObjects ;
	
	mach_port_t				fNotificationPort ;
	UInt32					fNotificationRefCon ;
	OSAsyncReference		fBusResetAsyncNotificationRef ;
	OSAsyncReference		fBusResetDoneAsyncNotificationRef ;
	IONotifier*				fNotifier ;
	IOService*				fOpenClient ;
	bool					fUnsafeResets ;

public:
    virtual void retain() const	;
    static IOFireWireUserClient*	withTask(
											task_t 					owningTask);
 	virtual bool 					start(
											IOService * 			provider );
    virtual void stop( IOService * provider );
	void							deallocateSets() ; 
	virtual void					free() ;
    virtual IOReturn 				clientClose( void );
    virtual IOReturn 				clientDied( void );	
	virtual IOReturn				setProperties(
											OSObject*				properties ) ;
	const task_t					getOwningTask() const {return fTask;}
	IOFireWireNub*					getOwner() const { return fOwner; }

	// --- startup ----------
	void							initMethodTable() ;
	void							initIsochMethodTable() ;
	void							initAsyncMethodTable() ;
	void							initIsochAsyncMethodTable() ;

	// --- open/close ----------
	IOReturn						userOpen() ;
	IOReturn						userOpenWithSessionRef(IOService*	session) ;
	IOReturn						seize(IOOptionBits inFlags ) ;
	IOReturn						userClose() ;
	
	// --- utils ----------
	inline static IOReturn 			sendAsyncResult(
											OSAsyncReference 		reference,
											IOReturn 				result, 
											void*					args[], 
											UInt32 					numArgs)
											{ return IOUserClient::sendAsyncResult(reference, result, args, numArgs) ; }									
    inline static void 				setAsyncReference(
											OSAsyncReference 		asyncRef,
											mach_port_t 			wakePort,
											void*					callback, 
											void*					refcon)
											{ IOUserClient::setAsyncReference(asyncRef, wakePort, callback, refcon) ; }
	virtual IOExternalMethod*		getTargetAndMethodForIndex(
											IOService **			target, 
											UInt32 					index) ;
	virtual IOExternalAsyncMethod*	getAsyncTargetAndMethodForIndex(
											IOService **			target, 
											UInt32 					index) ;
	virtual IOReturn				registerNotificationPort(
											mach_port_t 			port,
											UInt32					,//type,
											UInt32					refCon) ;

	// --- allocation management ----------
	virtual IOReturn				addObjectToSet(
											OSObject* 				object, 
											OSSet* 					set) ;
	virtual void					removeObjectFromSet(
											OSObject* 				object, 
											OSSet* 					set) ;

	// --- read/write/lock ----------------
	virtual IOReturn				readQuad( const ReadQuadParams* inParams, UInt32* outVal ) ;
	virtual IOReturn 				read( const ReadParams* inParams, IOByteCount* outBytesTransferred ) ;
    virtual IOReturn 				writeQuad( const WriteQuadParams* inParams ) ;
    virtual IOReturn 				write( const WriteParams* inParams, IOByteCount* outBytesTransferred ) ;
    virtual IOReturn 				compareSwap( const CompareSwapParams* inParams, UInt64* oldVal) ;

	// --- other -----------------
    virtual IOReturn 				busReset();
	virtual IOReturn				getGenerationAndNodeID(
											UInt32*					outGeneration,
											UInt32*					outNodeID) const ;
	virtual IOReturn				getLocalNodeID(
											UInt32*					outLocalNodeID) const ;
	virtual IOReturn				getResetTime(
											AbsoluteTime*			outResetTime) const ;
    virtual IOReturn 				message(
											UInt32 					type,
											IOService* 				provider,
											void* 					argument );
	// --- my conversion helpers -------
	virtual IOReturn				getOSStringData(
											KernOSStringRef			inStringRef,
											UInt32					inStringLen,
											char*					inStringBuffer,
											UInt32*					outStringLen) ;
	virtual IOReturn				getOSDataData(
											KernOSDataRef			inDataRef,
											IOByteCount				inDataLen,
											char*					inDataBuffer,
											IOByteCount*			outDataLen) ;
	
	//
    // --- CSR ROM Methods ----------
	//
	virtual IOReturn				unitDirCreate( KernUnitDirRef* dir ) ;
	virtual IOReturn				unitDirRelease( KernUnitDirRef dir ) ;
	virtual IOReturn				addEntry_Buffer( KernUnitDirRef dir, int key, char* buffer, UInt32 kr_size ) ;
	virtual IOReturn				addEntry_UInt32( KernUnitDirRef dir, int key, UInt32 value ) ;
	virtual IOReturn				addEntry_FWAddr( KernUnitDirRef dir,  int key, FWAddress value ) ;
	virtual IOReturn				addEntry_UnitDir( KernUnitDirRef dir, int key, KernUnitDirRef value ) ;
	virtual IOReturn				publish(
											KernUnitDirRef		inDir ) ;
	virtual IOReturn				unpublish(
											KernUnitDirRef		inDir) ;

	//
    // --- Address Spaces Methods ----------
	//
	virtual IOReturn				allocateAddressSpace( AddressSpaceCreateParams* params, KernAddrSpaceRef* outKernAddrSpaceRef) ;
	virtual IOReturn				releaseAddressSpace( KernAddrSpaceRef addrSpace ) ;
	virtual IOReturn				getPseudoAddressSpaceInfo( KernAddrSpaceRef addrSpace, UInt32* outNodeID,
											UInt32* outAddressHi, UInt32* outAddressLo) ;
	virtual IOReturn				setAsyncRef_Packet( OSAsyncReference asyncRef, KernAddrSpaceRef addrSpace,
											void* callback, void* userRefCon, void*, void*, void* ) ;
	virtual IOReturn				setAsyncRef_SkippedPacket(
											OSAsyncReference		asyncRef,
											KernAddrSpaceRef		inAddrSpaceRef,
											void*					inCallback,
											void*					inUserRefCon,
											void*,
											void*,
											void*) ;
	virtual IOReturn				setAsyncRef_Read(
											OSAsyncReference		asyncRef,
											KernAddrSpaceRef		inAddrSpaceRef,
											void*					inCallback,
											void*					inUserRefCon,
											void*,
											void*,
											void*) ;
	virtual IOReturn				setAsyncRef_BusReset(
											OSAsyncReference		asyncRef,
											void*					inCallback,
											void*					inUserRefCon,
											void*,
											void*,
											void*,
											void*) ;
	virtual IOReturn				setAsyncRef_BusResetDone(
											OSAsyncReference		asyncRef,
											void*					inCallback,
											void*					inUserRefCon,
											void*,
											void*,
											void*,
											void*) ;
	virtual IOReturn				clientCommandIsComplete(
											KernAddrSpaceRef		inAddrSpaceRef,
											FWClientCommandID		inCommandID,
											IOReturn				inResult ) ;

	//
	//	--- physical address space stuff ----------
	//
	virtual IOReturn				allocatePhysicalAddressSpace( PhysicalAddressSpaceCreateParams* params, KernPhysicalAddrSpaceRef* outKernAddrSpaceRef) ;
	virtual IOReturn				releasePhysicalAddressSpace(
											IOFWUserClientPhysicalAddressSpace*	inAddrSpace) ;
	virtual IOReturn				getPhysicalAddressSpaceSegmentCount(
											IOFWUserClientPhysicalAddressSpace*	inAddrSpace,
											UInt32*								outSegmentCount) ;
	virtual IOReturn				getPhysicalAddressSpaceSegments(
											IOFWUserClientPhysicalAddressSpace*	inAddrSpace,
											UInt32								inSegmentCount,
											UInt32*								outSegmentCount,
											IOPhysicalAddress					segments[],
											IOByteCount							segmentLengths[]) ;
	
	//
	//	--- async commands ----------
	//
	virtual IOReturn				lazyAllocateUserCommand( CommandSubmitParams* params, IOFWUserCommand** outCommand) ;
	virtual IOReturn				userAsyncCommand_Submit(
											OSAsyncReference			asyncRef,
											CommandSubmitParams*	inParams,
											CommandSubmitResult*	outResult,
											IOByteCount					inParamsSize,
											IOByteCount*				outResultSize) ;
	static void						asyncReadWriteCommandCompletion(
											void *					refcon, 
											IOReturn 				status, 
											IOFireWireNub *			device, 
											IOFWCommand *			fwCmd);
	
	//
	//	--- config directory functions ----------
	//
	virtual IOReturn 				configDirectoryCreate( KernConfigDirectoryRef*	outDirRef) ;
	virtual IOReturn 				configDirectoryRelease( KernConfigDirectoryRef	dirRef) ;
	virtual IOReturn 				configDirectoryUpdate( KernConfigDirectoryRef 	dirRef, UInt32 offset, const UInt32*& romBase ) ;
    virtual IOReturn 				configDirectoryGetKeyType( KernConfigDirectoryRef dirRef, int key, IOConfigKeyType* type );
    virtual IOReturn 				configDirectoryGetKeyValue_UInt32( KernConfigDirectoryRef dirRef,
											int key, UInt32 wantText, UInt32* outValue, KernOSStringRef* outString,
											UInt32* outStringLen);
    virtual IOReturn 				configDirectoryGetKeyValue_Data( KernConfigDirectoryRef inDirRef, int key, UInt32 wantText,
											GetKeyValueDataResults* results ) ;
    virtual IOReturn 				configDirectoryGetKeyValue_ConfigDirectory( KernConfigDirectoryRef dirRef,
											int key, UInt32 wantText, KernConfigDirectoryRef* outValue, 
											KernOSStringRef* outString, UInt32* outStringLen);
    virtual IOReturn 				configDirectoryGetKeyOffset_FWAddress( KernConfigDirectoryRef inDirRef, int key, 
											UInt32 wantText, GetKeyOffsetResults* results ) ;
    virtual IOReturn 				configDirectoryGetIndexType( KernConfigDirectoryRef inDirRef, int index, IOConfigKeyType* outType);
    virtual IOReturn 				configDirectoryGetIndexKey( KernConfigDirectoryRef dirRef, int index, int* outKey );
    virtual IOReturn 				configDirectoryGetIndexValue_UInt32( KernConfigDirectoryRef dirRef, int index, UInt32* outKey);
    virtual IOReturn 				configDirectoryGetIndexValue_Data( KernConfigDirectoryRef dirRef, int index, KernOSDataRef* outDataRef, IOByteCount* outDataLen );
    virtual IOReturn 				configDirectoryGetIndexValue_String( KernConfigDirectoryRef dirRef, int index, KernOSStringRef* outString, UInt32* outStringLen );
    virtual IOReturn 				configDirectoryGetIndexValue_ConfigDirectory( KernConfigDirectoryRef dirRef,
											int index, KernConfigDirectoryRef* outDirRef );
    virtual IOReturn 				configDirectoryGetIndexOffset_FWAddress( KernConfigDirectoryRef inDirRef,
											int index, UInt32* addressHi, UInt32* addressLo );
    virtual IOReturn 				configDirectoryGetIndexOffset_UInt32( KernConfigDirectoryRef inDirRef,
											int index, UInt32* outValue );
    virtual IOReturn 				configDirectoryGetIndexEntry( KernConfigDirectoryRef dirRef, int index,
											UInt32* outValue );
    virtual IOReturn 				configDirectoryGetSubdirectories( KernConfigDirectoryRef dirRef, OSIterator** outIterator );
	virtual IOReturn 				configDirectoryGetKeySubdirectories( KernConfigDirectoryRef dirRef,
											int key, OSIterator** outIterator);
	virtual IOReturn 				configDirectoryGetType( KernConfigDirectoryRef dirRef, int *outType) ;
	virtual IOReturn 				configDirectoryGetNumEntries( KernConfigDirectoryRef dirRef, int *outNumEntries) ;
							
	//
	// --- isoch port -------------
	//
	virtual IOReturn	isochPortAllocate( IsochPortAllocateParams* params, KernIsochPortRef* outPortRef) ;
	virtual IOReturn	isochPortRelease(
								KernIsochPortRef		inPortRef) ;
	virtual IOReturn	isochPortGetSupported(
								KernIsochPortRef		inPortRef,
								IOFWSpeed*				outMaxSpeed,
								UInt32*					outChanSupportedHi,
								UInt32*					outChanSupportedLo) ;
	virtual IOReturn	isochPortAllocatePort(
								KernIsochPortRef		inPortRef,
								IOFWSpeed				inSpeed,
								UInt32					inChannel) ;
	virtual IOReturn	isochPortReleasePort(
								KernIsochPortRef		inPortRef) ;
	virtual IOReturn	isochPortStart(
								KernIsochPortRef		inPortRef) ;
	virtual IOReturn	isochPortStop(
								KernIsochPortRef		inPortRef) ;

	//
	// local isoch port
	//
	virtual IOReturn				localIsochPortAllocate( LocalIsochPortAllocateParams* params, KernIsochPortRef* portRef ) ;
	virtual IOReturn				localIsochPortModifyJumpDCL(
											KernIsochPortRef		inPortRef,
											UInt32					inJumpDCLCompilerData,
											UInt32					inLabelDCLCompilerData) ;
	virtual IOReturn				localIsochPortModifyJumpDCLSize( KernIsochPortRef inPortRef, UInt32 inDCLCompilerData,
											IOByteCount newSize ) ;
	virtual IOReturn				setAsyncRef_DCLCallProc( OSAsyncReference asyncRef, KernIsochPortRef portRef, 
											DCLCallCommandProc* proc ) ;
	
	//
	// --- isoch channel ----------
	//
	static IOReturn		isochChannelForceStopHandler(
								void*					refCon,
								IOFWIsochChannel*		isochChannelID,
								UInt32					stopCondition) ;
	virtual IOReturn	isochChannelAllocate(
								bool					inDoIRM,
								UInt32					inPacketSize,
								IOFWSpeed				inPrefSpeed,
								KernIsochChannelRef*	outIsochChannelRef) ;
	virtual IOReturn	isochChannelRelease(
								KernIsochChannelRef	inChannelRef) ;
	virtual IOReturn	isochChannelUserAllocateChannelBegin(
								KernIsochChannelRef	inChannelRef,
								IOFWSpeed				inSpeed,
								UInt32					inAllowedChansHi,
								UInt32					inAllowedChansLo,
//								UInt64					inAllowedChans,
								IOFWSpeed*				outSpeed,
								UInt32*					outChannel) ;
	virtual IOReturn	isochChannelUserReleaseChannelComplete(
								KernIsochChannelRef	inChannelRef) ;
	virtual IOReturn	setAsyncRef_IsochChannelForceStop(
								OSAsyncReference		asyncRef,
								void*					inCallback,
								void*					inUserRefCon,
								void*,
								void*,
								void*,
								void*) ;

	//
	// --- firewire command objects ----------------------
	//
	virtual IOReturn				userAsyncCommand_Release( KernCommandRef command ) ;
	virtual IOReturn				userAsyncCommand_Cancel( KernCommandRef command, IOReturn reason ) 	{ return kIOReturnUnsupported; }

	//
	// --- statistics ----------
	//
	const IOFireWireUserClientStatistics*	
									getStatistics()							{ return fStatistics ; }


	//
	// --- absolute address firewire commands ----------
	//
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

	IOReturn	firelog( const char* string, IOByteCount bufSize ) const ;
	IOReturn 	getBusGeneration( UInt32* outGeneration ) ;
	IOReturn 	getLocalNodeIDWithGeneration( UInt32 generation, UInt32* outLocalNodeID ) ;
	IOReturn 	getRemoteNodeID( UInt32 generation, UInt32* outRemoteNodeID ) ;
	IOReturn 	getSpeedToNode( UInt32 generation, UInt32* outSpeed ) ;
	IOReturn 	getSpeedBetweenNodes( UInt32 generation, UInt32 fromNode, UInt32 toNode, UInt32* outSpeed ) ;

	//
	// v5
	//
	
	IOReturn	getIRMNodeID( UInt32 generation, UInt32* irmNodeID ) ;
	
 protected:
	IOFireWireUserClientStatistics*		fStatistics ;
};

#endif /* ! _IOKIT_IOFIREWIREUSERCLIENT_H */

