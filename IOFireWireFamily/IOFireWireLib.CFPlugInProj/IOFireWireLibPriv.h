/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  IOFireWireLibPriv.h
 *  IOFireWireLib
 *
 *  Created on Fri Apr 28 2000.
 *  Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: IOFireWireLibPriv.h,v $
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


extern "C"
{
	void * IOFireWireLibFactory(CFAllocatorRef allocator, CFUUIDRef typeID) ;
}

#endif // ifndef KERNEL

#pragma mark -
#pragma mark SHARED

#define kIOFireWireLibConnection 11

namespace IOFireWireLib {

	typedef struct AKernelObject* UserObjectHandle ;

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
		kFireWireCommandStale_MaxPacket		= (1 << 2)
	} ;

	typedef enum IOFireWireCommandType_t {
		kFireWireCommandType_Read,
		kFireWireCommandType_ReadQuadlet,
		kFireWireCommandType_Write,
		kFireWireCommandType_WriteQuadlet,
		kFireWireCommandType_CompareSwap
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
		const void*  			 	buf ;
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
		IOByteCount					size ;
		Boolean						failOnReset ;
		UInt32						generation ;
		Boolean						isAbs ;
	}  ;
	
	struct CommandSubmitParams
	{
		UserObjectHandle			kernCommandRef ;
		IOFireWireCommandType		type ;
		void*						callback ;
		void*						refCon ;
		UInt32						flags ;
		
		UInt32						staleFlags ;
		FWAddress					newTarget ;
		void*						newBuffer ;
		IOByteCount					newBufferSize ;	// note: means numQuads for quadlet commands!
		Boolean						newFailOnReset ;
		UInt32						newGeneration ;
		IOByteCount					newMaxPacket ;
	}  ;
	
	struct CommandSubmitResult
	{
		UserObjectHandle				kernCommandRef ;
		IOReturn					result ;
		IOByteCount					bytesTransferred ;
	}  ;
	
	struct FWCompareSwapLockInfo
	{
		Boolean 	didLock ;
		UInt64		value ;
	}  ;
	
	struct CompareSwapSubmitResult
	{
		UserObjectHandle				kernCommandRef ;
		IOReturn					result ;
		IOByteCount					bytesTransferred ;
		FWCompareSwapLockInfo		lockInfo ;
	}  ;
		
	//
	// DCL stuff
	//
	
	struct LocalIsochPortAllocateParams
	{
		unsigned			version ;

		bool				talking ;
		unsigned			startEvent ;
		unsigned			startState ;
		unsigned			startMask ;

		IOByteCount			programExportBytes ;
		IOVirtualAddress	programData ;
		
		unsigned			bufferRangeCount ;
		IOVirtualRange *	bufferRanges ;
		
		void*				userObj ;
	}  ;
	
	//
	// address spaces
	//
	
	struct AddressSpaceInfo
	{
		FWAddress		address ;
	} ;

	struct AddressSpaceCreateParams
	{
		UInt32		size ;
		void*		backingStore ;
		UInt32		queueSize ;
		void*		queueBuffer ;
		UInt32		flags ;
		void*		refCon ;
	
		// for initial units address spaces:
		Boolean		isInitialUnits ;
		UInt32		addressLo ;
	}  ;
	
	struct PhysicalAddressSpaceCreateParams
	{
		UInt32		size ;
		void*		backingStore ;
		UInt32		flags ;
	}  ;
	
	// 
	// config ROM
	//
	
	typedef struct
	{
		UserObjectHandle		data ;
		IOByteCount			dataLength ;
		UserObjectHandle	text ;
		UInt32				textLength ;	
	} GetKeyValueDataResults ;
	
	typedef struct
	{
		FWAddress			address ;
		UserObjectHandle	text ;
		UInt32				length ;
	} GetKeyOffsetResults ;

	//
	// Buffer Fill IsochPort
	//
	
	struct BufferFillIsochPortCreateParams
	{
		UInt32				interruptUSec ;
		IOByteCount			expectedBytesPerSecond ;
	} ;

	// make sure these values don't conflict with any
	// in enum 'NuDCLFlags' as defined in IOFireWireFamilyCommon.h
	
	enum
	{
		kNuDCLUser			= BIT( 18 )		// set back to BIT(18) when we want to support user space update before callback
	} ;
	
	//
	// trap table
	//
	
	enum MethodSelector
	{
		// --- open/close ----------------------------
		kOpen = 0,
		kOpenWithSessionRef,
		kClose,
		
		// --- user client general methods -----------
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
		
		// --- conversion helpers --------------------
		kGetOSStringData,
		kGetOSDataData,
		
		// --- user unit directory methods -----------
		kLocalConfigDirectory_Create,
		kLocalConfigDirectory_AddEntry_Buffer,
		kLocalConfigDirectory_AddEntry_UInt32,
		kLocalConfigDirectory_AddEntry_FWAddr,
		kLocalConfigDirectory_AddEntry_UnitDir,
		kLocalConfigDirectory_Publish,
		kLocalConfigDirectory_Unpublish,
		
		// --- pseudo address space methods ----------
		kPseudoAddrSpace_Allocate,
		kPseudoAddrSpace_GetFWAddrInfo,
		kPseudoAddrSpace_ClientCommandIsComplete,
		
		// --- physical address space methods ----------
		kPhysicalAddrSpace_Allocate,
		kPhysicalAddrSpace_GetSegmentCount_d,
		kPhysicalAddrSpace_GetSegments,
		
		// --- command completion --------------------
//		kClientCommandIsComplete,
		
		// --- config directory ----------------------
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
		
		// --- isoch port methods ----------------------------
//		kIsochPort_Allocate,
//		kIsochPort_Release,
		kIsochPort_GetSupported,
		kIsochPort_AllocatePort_d,
		kIsochPort_ReleasePort_d,
		kIsochPort_Start_d,
		kIsochPort_Stop_d,
//		kIsochPort_SetSupported,
		
		// --- local isoch port methods ----------------------
		kLocalIsochPort_Allocate,
		kLocalIsochPort_ModifyJumpDCL_d,
		kLocalIsochPort_Notify_d,
//		kLocalIsochPort_ModifyTransferPacketDCLSize,
//		kLocalIsochPort_ModifyTransferPacketDCL,
		
		// --- isoch channel methods -------------------------
		kIsochChannel_Allocate,
		kIsochChannel_UserAllocateChannelBegin_d,
		kIsochChannel_UserReleaseChannelComplete_d,
		
		// --- firewire command objects ----------------------
//		kCommand_Release,
		kCommand_Cancel_d,
//		kCommand_DidLock,
//		kCommand_Locked32,
//		kCommand_Locked64,
		
		// --- seize service ----------
		kSeize,
		
		// --- firelog ----------
		kFireLog,
		
		// --- More user client general methods (new in v3)
		kGetBusCycleTime,
		
		//
		// v4
		//
		
		kGetBusGeneration,
		kGetLocalNodeIDWithGeneration,
		kGetRemoteNodeID,
		kGetSpeedToNode,
		kGetSpeedBetweenNodes,
		
		//
		// v5
		//
		
		kGetIRMNodeID,
		
		// v6
		
		kClipMaxRec2K,
		kIsochPort_SetIsochResourceFlags_d,
//		kIsochPort_RunNuDCLUpdateList_d,
//		kBufferFillIsochPort_Create,
		
		// -------------------------------------------
		kNumMethods
		
	} ;


	typedef enum
	{
	
		kSetAsyncRef_BusReset,
		kSetAsyncRef_BusResetDone,
	
		//
		// pseudo address space
		//
		kSetAsyncRef_Packet,
		kSetAsyncRef_SkippedPacket,
		kSetAsyncRef_Read,
	
		//
		// user command objects
		//
		kCommand_Submit,
		
		//
		// isoch channel
		//
		kSetAsyncRef_IsochChannelForceStop,
	
		//
		// isoch port
		//
		kSetAsyncRef_DCLCallProc,
		
		kNumAsyncMethods
	
	} AsyncMethodSelector ;

}	// namespace IOFireWireLib
