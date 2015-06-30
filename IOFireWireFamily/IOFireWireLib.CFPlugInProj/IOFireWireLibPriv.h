/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLibPriv.h
 *  IOFireWireLib
 *
 *  Created on Fri Apr 28 2000.
 *  Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: not supported by cvs2svn $
	Revision 1.66  2009/01/15 01:40:02  collin
	<rdar://problem/6400981> BRB-VERIFY: 10A222: Trying to record a movie through QT X, getting error message.
	
	Revision 1.65  2008/12/12 04:43:57  collin
	user space compare swap command fixes
	
	Revision 1.64  2008/11/26 23:55:21  collin
	fix user physical address spaces on K64
	
	Revision 1.63  2007/10/16 16:50:21  ayanowit
	Removed existing "work-in-progress" support for buffer-fill isoch.
	
	Revision 1.62  2007/08/24 01:14:31  collin
	fix resp code on sync command execution
	
	Revision 1.61  2007/05/12 01:10:45  arulchan
	Asyncstream transmit command interface
	
	Revision 1.60  2007/04/28 01:42:35  collin
	*** empty log message ***
	
	Revision 1.59  2007/04/24 02:50:09  collin
	*** empty log message ***
	
	Revision 1.58  2007/03/22 00:30:01  collin
	*** empty log message ***
	
	Revision 1.57  2007/03/10 07:48:25  collin
	*** empty log message ***
	
	Revision 1.56  2007/03/10 05:11:36  collin
	*** empty log message ***
	
	Revision 1.55  2007/03/10 02:58:03  collin
	*** empty log message ***
	
	Revision 1.54  2007/03/09 23:57:53  collin
	*** empty log message ***
	
	Revision 1.53  2007/03/08 02:37:13  collin
	*** empty log message ***
	
	Revision 1.52  2007/02/16 19:04:21  arulchan
	*** empty log message ***
	
	Revision 1.51  2007/02/16 00:28:26  ayanowit
	More work on IRMAllocation APIs
	
	Revision 1.50  2007/02/09 20:36:46  ayanowit
	More Leopard IRMAllocation changes.
	
	Revision 1.49  2007/02/07 01:35:36  collin
	*** empty log message ***
	
	Revision 1.48  2007/02/06 01:08:41  ayanowit
	More work on Leopard features such as new User-space IRM allocation APIs.
	
	Revision 1.47  2007/01/26 20:52:32  ayanowit
	changes to user-space isoch stuff to support 64-bit apps.
	
	Revision 1.46  2007/01/24 04:10:14  collin
	*** empty log message ***
	
	Revision 1.45  2007/01/16 02:41:25  collin
	*** empty log message ***
	
	Revision 1.44  2007/01/11 08:48:24  collin
	*** empty log message ***
	
	Revision 1.43  2007/01/06 06:20:44  collin
	*** empty log message ***
	
	Revision 1.42  2007/01/05 04:56:31  collin
	*** empty log message ***
	
	Revision 1.41  2007/01/04 21:59:38  ayanowit
	more changes for 64-bit apps.
	
	Revision 1.40  2007/01/04 04:07:25  collin
	*** empty log message ***
	
	Revision 1.39  2006/12/06 00:01:10  arulchan
	Isoch Channel 31 Generic Receiver
	
	Revision 1.38  2006/11/29 18:42:53  ayanowit
	Modified the IOFireWireUserClient to use the Leopard externalMethod method of dispatch.
	
	Revision 1.37  2006/02/09 00:21:55  niels
	merge chardonnay branch to tot
	
	Revision 1.36  2005/09/24 00:55:28  niels
	*** empty log message ***
	
	Revision 1.35  2005/04/02 02:43:46  niels
	exporter works outside IOFireWireFamily
	
	Revision 1.34.6.3  2006/01/31 04:49:57  collin
	*** empty log message ***
	
	Revision 1.34.6.1  2006/01/17 00:35:00  niels
	<rdar://problem/4399365> FireWire NuDCL APIs need Rosetta support
	
	Revision 1.34  2004/05/04 22:52:20  niels
	*** empty log message ***
	
	Revision 1.33  2004/01/22 01:50:01  niels
	fix user space physical address space getPhysicalSegments
	
	Revision 1.32  2003/12/19 22:07:46  niels
	send force stop when channel dies/system sleeps
	
	Revision 1.31  2003/11/07 21:01:19  niels
	*** empty log message ***
	
	Revision 1.30  2003/11/03 19:11:37  niels
	fix local config rom reading; fix 3401223
	
	Revision 1.29  2003/08/26 05:11:22  niels
	*** empty log message ***
	
	Revision 1.28  2003/08/25 08:39:17  niels
	*** empty log message ***
	
	Revision 1.27  2003/08/14 17:47:33  niels
	*** empty log message ***
	
	Revision 1.26  2003/08/08 21:03:47  gecko1
	Merge max-rec clipping code into TOT
	
	Revision 1.25  2003/07/24 20:49:50  collin
	*** empty log message ***
	
	Revision 1.24  2003/07/21 06:53:11  niels
	merge isoch to TOT
	
	Revision 1.23.14.3  2003/07/18 00:17:48  niels
	*** empty log message ***
	
	Revision 1.23.14.2  2003/07/09 21:24:08  niels
	*** empty log message ***
	
	Revision 1.23.14.1  2003/07/01 20:54:24  niels
	isoch merge
	
	Revision 1.23  2003/01/09 22:58:12  niels
	radar 3061582: change kCFRunLoopDefaultMode to kCFRunLoopCommonModes
	
	Revision 1.22  2002/12/12 22:44:04  niels
	fixed radar 3126316: panic with Hamamatsu driver
	
	Revision 1.21  2002/09/25 00:27:35  niels
	flip your world upside-down
	
	Revision 1.20  2002/09/12 22:41:56  niels
	add GetIRMNodeID() to user client
*/

#pragma mark USER SPACE

#import "IOFWUserObjectExporter.h"

#ifndef KERNEL

#import "IOFireWireLib.h"

// IOFireWireLib factory ID
// 	uuid string: A1478010-F197-11D4-A28B-000502072F80
#define	kIOFireWireLibFactoryID			CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault, 0xA1, 0x47, 0x80, 0x10,0xF1, 0x97, 0x11, 0xD4, 0xA2, 0x8B, 0x00, 0x05,0x02, 0x07, 0x2F, 0x80)

#if (IOFIREWIRELIBDEBUG)
#	include <syslog.h>

#   define  DebugLog( x... )						{ printf( "%s %u:", __FILE__, __LINE__ ); printf( x ) ; }
#   define  DebugLogCond( x, y... )					{ if (x) DebugLog( y ) ; }
# else
#	define DebugLog(x...)
#   define DebugLogCond( x, y... )
# endif

#define InfoLog(x...) {}

/* Macros for min/max. */
#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif	/* MAX */

extern "C"
{
	void * IOFireWireLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID) ;
}

#endif // ifndef KERNEL

#pragma mark -
#pragma mark SHARED

#define kIOFireWireLibConnection 11

namespace IOFireWireLib {

	//
	// command objects
	//
	
	enum CommandType {
		kCommandType_Read,
		kCommandType_ReadQuadlet,
		kCommandType_Write,
		kCommandType_WriteQuadlet,
		kCommandType_CompareSwap
	} ;

	enum {
		kFireWireCommandStale				= (1 << 0),
		kFireWireCommandStale_Buffer		= (1 << 1),
		kFireWireCommandStale_MaxPacket		= (1 << 2),
		kFireWireCommandStale_Timeout		= (1 << 3),
		kFireWireCommandStale_Retries		= (1 << 4),
		kFireWireCommandStale_Speed			= (1 << 5)
	} ;

	typedef enum IOFireWireCommandType_t {
		kFireWireCommandType_Read,
		kFireWireCommandType_ReadQuadlet,
		kFireWireCommandType_Write,
		kFireWireCommandType_WriteQuadlet,
		kFireWireCommandType_CompareSwap,
		kFireWireCommandType_PHY,
		kFireWireCommandType_AsyncStream
	} IOFireWireCommandType ;
	
	struct WriteQuadParams
	{
		FWAddress					addr ;
		UInt32  			 		val ;
		Boolean						failOnReset ;
		UInt32						generation ;
		Boolean						isAbs ;
	}  ;
	
	typedef struct
	{	
		FWAddress					addr ;
		mach_vm_address_t			buf ;
		UInt32						size ;
		Boolean						failOnReset ;
		UInt32						generation ;
		Boolean						isAbs ;
	} ReadParams, WriteParams, ReadQuadParams ;
	
	struct CompareSwapParams
	{
		FWAddress					addr ;
		UInt64						cmpVal ;
		UInt64						swapVal ;
		UInt32						size ;
		Boolean						failOnReset ;
		UInt32						generation ;
		Boolean						isAbs ;
	}  ;
	
	struct CommandSubmitParams
	{
		UserObjectHandle			kernCommandRef ;
		IOFireWireCommandType		type ;
		mach_vm_address_t			callback ;
		mach_vm_address_t			refCon ;
		UInt32						flags ;
		
		UInt32						staleFlags ;
		UInt64						newTarget ;
		mach_vm_address_t			newBuffer ;
		UInt32						newBufferSize ;	// note: means numQuads for quadlet commands!
		Boolean						newFailOnReset ;
		UInt32						newGeneration ;
		UInt32						newMaxPacket ;
		
		UInt32						timeoutDuration;
		UInt32						retryCount;
		UInt32						maxPacketSpeed;
		UInt32						data1;
		UInt32						data2;
		UInt32						tag;
		UInt32						sync;
	} __attribute__ ((packed));
	
	struct CommandSubmitResult
	{
		UserObjectHandle			kernCommandRef ;
		IOReturn					result ;
		UInt32						bytesTransferred ;
		UInt32						ackCode;
		UInt32						responseCode;
		mach_vm_address_t			refCon;
	} __attribute__ ((packed));
	
	struct FWCompareSwapLockInfo
	{
		Boolean 	didLock ;
		UInt32		value[2];
	}   __attribute__ ((packed));
	
	struct CompareSwapSubmitResult
	{
		UserObjectHandle			kernCommandRef ;
		IOReturn					result ;
		UInt32						bytesTransferred ;
		UInt32						ackCode;
		UInt32						responseCode;
		FWCompareSwapLockInfo		lockInfo ;
	}  __attribute__ ((packed));
		
	//
	// DCL stuff
	//
	
	typedef struct LocalIsochPortAllocateParamsStruct
	{
		UInt32				version ;

		UInt32				startEvent ;
		UInt32				startState ;
		UInt32				startMask ;

		mach_vm_address_t	programData ;
		UInt32				programExportBytes ;
		
		UInt32				bufferRangeCount ;
		mach_vm_address_t	bufferRanges ;

		mach_vm_address_t	userObj ;

		UInt32				talking ;

		UInt32				options ;

	}  __attribute__ ((packed)) LocalIsochPortAllocateParams;
	
	//
	// address spaces
	//
	
	struct AddressSpaceInfo
	{
		FWAddress		address ;
	} ;

	struct AddressSpaceCreateParams
	{
		mach_vm_size_t			size ;
		mach_vm_address_t		backingStore ;
		mach_vm_size_t			queueSize ;
		mach_vm_address_t		queueBuffer ;
		UInt32					flags ;
		mach_vm_address_t		refCon ;
	
		// for initial units address spaces:
		Boolean		isInitialUnits ;
		UInt32		addressLo ;
	}  __attribute__ ((packed));

	struct FWUserAsyncStreamListenerCreateParams
	{
		UInt32					channel;
		mach_vm_size_t			queueSize ;
		mach_vm_address_t		queueBuffer ;
		UInt32					flags ;
		mach_vm_address_t		callback ;
		mach_vm_address_t		refCon ;
	}  __attribute__ ((packed));
	
//	struct PhysicalAddressSpaceCreateParams
//	{
//		UInt32		size ;
//		void*		backingStore ;
//		UInt32		flags ;
//	}  ;
	
	// 
	// config ROM
	//
	
	typedef struct
	{
		UserObjectHandle		data ;
		UInt32				dataLength ;
		UserObjectHandle	text ;
		UInt32				textLength ;	
	} GetKeyValueDataResults ;
	
	typedef struct
	{
		FWAddress			address ;
		UserObjectHandle	text ;
		UInt32				length ;
	} GetKeyOffsetResults ;

	typedef struct 
	{
		IOPhysicalAddress32 location;
		IOPhysicalLength32  length;
	} FWPhysicalSegment32;

	typedef struct 
	{
		mach_vm_address_t	address;
		mach_vm_size_t	length;
	} FWVirtualAddressRange;

	// make sure these values don't conflict with any
	// in enum 'NuDCLFlags' as defined in IOFireWireFamilyCommon.h
	
	enum
	{
		kNuDCLUser			= BIT( 18 )		// set back to BIT(18) when we want to support user space update before callback
	} ;
	
	enum
	{
		kDCLExportDataLegacyVersion = 0
		, kDCLExportDataNuDCLRosettaVersion = 8
	} ;
	
	/////////////////////////////////////////
	//
	// Method Selectors For User Client
	//
	/////////////////////////////////////////
	enum MethodSelector
	{
		kOpen = 0,
		kOpenWithSessionRef,
		kClose,
		kReadQuad,
		kRead,
		kWriteQuad,
		kWrite,
		kCompareSwap,
		kBusReset,
		kCycleTime,
		kGetGenerationAndNodeID,
		kGetLocalNodeID,
		kGetResetTime,
		kReleaseUserObject,
		kGetOSStringData,
		kGetOSDataData,
		kLocalConfigDirectory_Create,
		kLocalConfigDirectory_AddEntry_Buffer,
		kLocalConfigDirectory_AddEntry_UInt32,
		kLocalConfigDirectory_AddEntry_FWAddr,
		kLocalConfigDirectory_AddEntry_UnitDir,
		kLocalConfigDirectory_Publish,
		kLocalConfigDirectory_Unpublish,
		kPseudoAddrSpace_Allocate,
		kPseudoAddrSpace_GetFWAddrInfo,
		kPseudoAddrSpace_ClientCommandIsComplete,
		kPhysicalAddrSpace_Allocate,
		kPhysicalAddrSpace_GetSegmentCount_d,
		kPhysicalAddrSpace_GetSegments,
		kConfigDirectory_Create,
		kConfigDirectory_GetKeyType,
		kConfigDirectory_GetKeyValue_UInt32,
		kConfigDirectory_GetKeyValue_Data,
		kConfigDirectory_GetKeyValue_ConfigDirectory,
		kConfigDirectory_GetKeyOffset_FWAddress,	
		kConfigDirectory_GetIndexType,
		kConfigDirectory_GetIndexKey,
		kConfigDirectory_GetIndexValue_UInt32,
		kConfigDirectory_GetIndexValue_Data,
		kConfigDirectory_GetIndexValue_String,
		kConfigDirectory_GetIndexValue_ConfigDirectory,
		kConfigDirectory_GetIndexOffset_FWAddress,
		kConfigDirectory_GetIndexOffset_UInt32,
		kConfigDirectory_GetIndexEntry,
		kConfigDirectory_GetSubdirectories,
		kConfigDirectory_GetKeySubdirectories,
		kConfigDirectory_GetType,
		kConfigDirectory_GetNumEntries,
		kIsochPort_GetSupported,
		kIsochPort_AllocatePort_d,
		kIsochPort_ReleasePort_d,
		kIsochPort_Start_d,
		kIsochPort_Stop_d,
		kLocalIsochPort_Allocate,
		kLocalIsochPort_ModifyJumpDCL_d,
		kLocalIsochPort_Notify_d,
		kLocalIsochPort_SetChannel,
		kIsochChannel_Allocate,
		kIsochChannel_UserAllocateChannelBegin,
		kIsochChannel_UserReleaseChannelComplete_d,
		kCommand_Cancel_d,
		kSeize,
		kFireLog,
		kGetBusCycleTime,
		kGetBusGeneration,
		kGetLocalNodeIDWithGeneration,
		kGetRemoteNodeID,
		kGetSpeedToNode,
		kGetSpeedBetweenNodes,
		kGetIRMNodeID,
		kClipMaxRec2K,
		kIsochPort_SetIsochResourceFlags_d,
		kGetSessionRef,
		kAsyncStreamListener_Allocate,
		kAsyncStreamListener_ClientCommandIsComplete,
		kAsyncStreamListener_GetOverrunCounter,
		kAsyncStreamListener_SetFlags,
		kAsyncStreamListener_GetFlags,
		kAsyncStreamListener_TurnOnNotification,
		kAsyncStreamListener_TurnOffNotification,
		kAllocateIRMBandwidth,
		kReleaseIRMBandwidth,
		kAllocateIRMChannel,
		kReleaseIRMChannel,
		kSetAsyncRef_BusReset,
		kSetAsyncRef_BusResetDone,
		kSetAsyncRef_Packet,
		kSetAsyncRef_SkippedPacket,
		kSetAsyncRef_Read,
		kCommand_Submit,
		kSetAsyncRef_IsochChannelForceStop,
		kSetAsyncRef_DCLCallProc,
		kSetAsyncStreamRef_Packet,
		kSetAsyncStreamRef_SkippedPacket,
		kIRMAllocation_Allocate,
		kIRMAllocation_SetRef,
		kIRMAllocation_AllocateResources,
		kIRMAllocation_DeallocateResources,
		kIRMAllocation_areResourcesAllocated,
		kIRMAllocation_setDeallocateOnRelease,
		kCommandCreateAsync,
		kVectorCommandSetBuffers,
		kVectorCommandSubmit,
		kVectorCommandCreate,
		kPHYPacketListenerCreate,
		kPHYPacketListenerSetPacketCallback,
		kPHYPacketListenerSetSkippedCallback,
		kPHYPacketListenerActivate,
		kPHYPacketListenerDeactivate,
		kPHYPacketListenerClientCommandIsComplete,
		kNumMethods
	} ;

}	// namespace IOFireWireLib
