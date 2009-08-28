/*
 * Copyright (c) 2007-2008 Apple Inc. All rights reserved.
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

#ifndef __IOKIT_IO_FIREWIRE_FAMILY_TRACEPOINTS__
#define __IOKIT_IO_FIREWIRE_FAMILY_TRACEPOINTS__

#include <IOKit/IOTypes.h>

#include <sys/kdebug.h>

#ifdef __cplusplus
extern "C" {
#endif
	
#define FIREWIRE_SYSCTL "debug.FireWire"
#define kFireWireTypeDebug 'FWDB'
#define DEBUG_UNUSED( X ) ( void )( X )

extern UInt32 gFireWireDebugFlags;
	
typedef struct FireWireSysctlArgs
{
	uint32_t		type;
	uint32_t		operation;
	uint32_t		debugFlags;
} FireWireSysctlArgs;
	
enum
{
	kFireWireOperationGetFlags 	= 0,
	kFireWireOperationSetFlags	= 1
};

enum
{
	kFireWireEnableDebugLoggingBit		= 0,
	kFireWireEnableTracePointsBit		= 1,
	
	kFireWireEnableDebugLoggingMask		= (1 << kFireWireEnableDebugLoggingBit),
	kFireWireEnableTracePointsMask		= (1 << kFireWireEnableTracePointsBit),
};

	
/* Kernel Tracepoints
 *
 * Kernel tracepoints are a logging mechanism reduces the size of a log-laden binary.
 * Codes are placed into a buffer, from the kernel, and picked up by a userspace
 * tool that displays a unique log message for each tracepoint. Additionally, each
 * tracepoint may contain up-to four 32-bit (may change with LP64) arguments. 
 *
 * To add a tracepoint, use the code below as an example:
 * FWTrace( kFWTController, kTPControllerStart, (uintptr_t)myArgValue );
 * Next, add the corresponding tracepoint code in the FireWireTracer tool, using
 * the existing examples. Avoid using confidential information in the log strings.
 * Some functions have a argument counter, to signify which of the function's tracepoints
 * are actually being logged. When adding a tracepoint using an existing code, you
 * must verify that you increment this argument counter properly.
 *
 * The trace codes consist of the following:
 *
 * ----------------------------------------------------------------------
 *| Class (8)      | SubClass (8)   | FWGroup(6) | Code (8)       |Func |
 *| DBG_IOKIT      | DBG_IOFIREWIRE |            |                |Qual(2)|
 * ----------------------------------------------------------------------
 *
 * DBG_IOKIT(05h)  DBG_IOFIREWIRE(2Fh)
 *
 * See <sys/kdebug.h> and IOTimeStamp.h for more details.
 */


// FireWire groupings (max of 64)
enum
{
	// Family groupings
	kFWTController				= 0,
	kFWTDevice					= 1,
	kFWTIsoch					= 2,
	kFWTUserClient				= 3,
	// 4-15 reserved
	
	// FWIM groupings
	kFWTFWIM					= 16,
	kFWTPowerManager			= 17,
	// 19-25 reserved
	
	// SBP2 groupings
	// - reserved
	
	// IP groupings
	// - reserved
	
	// AVC groupings
	// - reserved
	
	// Actions
	kFWTResetBusAction			= 58,
	kFWTStateChangeAction		= 59,
	kFWTScheduleResetLinkAction	= 60
	// 61-63 reserved
};

// FireWire Controller Tracepoints		0x05270000 - 0x052703FF
// kFWTController
enum
{
	kTPControllerStart						= 1,
	kTPControllerWillChangePowerState		= 2,
	kTPControllerPoweredStart				= 3,
	kTPControllerSetPowerState				= 4,
	kTPControllerResetBus					= 5,
	kTPControllerDelayedStateChange			= 6,
	kTPControllerProcessSelfIDs				= 7,
	kTPControllerStartBusScan				= 8,
	kTPControllerReadDeviceROM				= 9,
	kTPControllerReadDeviceROMSubmitCmd		= 10,
	kTPControllerUpdateDevice				= 11,
	kTPControllerUpdateDeviceNewDevice		= 12,
	kTPControllerUpdateDeviceCreateDevice	= 13,
	kTPControllerFinishedBusScan			= 14,
	kTPControllerBuildTopology				= 15,
	kTPControllerUpdatePlane				= 16,
	kTPControllerAsyncRead					= 17,
	kTPControllerAsyncWrite					= 18,
	kTPControllerAsyncLock					= 19,
	kTPControllerAsyncStreamWrite			= 20,
	kTPControllerProcessRcvPacket			= 21,
	kTPControllerProcessRcvPacketWR			= 22,
	kTPControllerProcessRcvPacketRQ			= 23,
	kTPControllerProcessRcvPacketRB			= 24,
	kTPControllerProcessRcvPacketRQR		= 25,
	kTPControllerProcessRcvPacketRBR		= 26,
	kTPControllerProcessWriteRequest		= 27,
	kTPControllerProcessLockRequest			= 28,
	kTPControllerAsyncLockResponse			= 29,
	kTPControllerTimeoutQBusReset			= 30,
	kTPControllerTimeoutQProcessTimeout		= 31
};

// FireWire Device Tracepoints			
// kFWTDevice
enum
{
	kTPDeviceSetNodeROM						= 1,
	kTPDeviceProcessROM						= 2,
	kTPDeviceProcessUnitDirectories			= 3
};

// FireWire Isoch Tracepoints			
// kFWTIsoch
enum
{
	// IRM
	kTPIsochIRMAllocateIsochResources		= 1,
	kTPIsochIRMThreadFunc					= 2,
	// 3-10 reserved
	
	// channel
	kTPIsochAllocateChannelBegin			= 11,
	kTPIsochReallocBandwidth				= 12,
	// 12-15 reserved
	
	// port (+ user)
	// 16-19 reserved
	kTPIsochPortUserInitWithUserDCLProgram	= 20
};

// FireWire UserClient Tracepoints			
// kFWTUserClient
enum
{
	kTPUserClientStart						= 1,
	kTPUserClientFree						= 2,
	kTPUserClientClientClose				= 3,
	kTPUserClientClientDied					= 4,
	kTPUserClientBusReset					= 5
};

// FireWire FWIM Tracepoints			
// kFWTFWIM
enum
{
	kTPFWIMStart							= 1,
	kTPFWIMPoweredStart						= 2,
	kTPFWIMFinishStart						= 3,
	kTPFWIMResetLink						= 4,
	kTPFWIMInitLink							= 5,
	kTPFWIMHandleBusResetInt				= 6,
	kTPFWIMHandleSelfIDInt					= 7,
	kTPFWIMEnterLoggingMode					= 8,
	kTPFWIMKPF								= 9
};

// FireWire PowerManager Tracepoints	
// kFWTPowerManager
enum
{
	kTPPMSetupPowerManagement				= 1,
	kTPPMHandleLinkOnInterrupt				= 2,
	kTPPMStartPowerSavingsTimer				= 3,
	kTPPMHandlePowerSavingsTimer			= 4,
	kTPPMExitPowerSavings					= 5,
	kTPPMCancelPowerSavingsTimer			= 6,
	kTPPMSleep								= 7,
	kTPPMDoze								= 8
};
	
// FireWire ResetBus Action Tracepoints			
// kFWTResetBusAction
// FWTrace(kFWTResetBusAction, kTPResetDelayedStateChangeWaitingBusReset, (uintptr_t)fFWIM, me->fBusState);
enum
{
	kTPResetSetPowerState						= 1,
	kTPResetResetStateChangeResetScheduled		= 2,
	kTPResetDelayedStateChangeWaitingBusReset	= 3,
	kTPResetProcessSelfIDs						= 4,
	kTPResetAssignCycleMaster					= 5,
	kTPResetFinishedBusScan						= 6,
	kTPResetUpdatePlane							= 7,
	kTPResetAddUnitDirectory					= 8,
	kTPResetRemoveUnitDirectory					= 9,
	kTPResetMakeRoot							= 10,
	kTPResetFWIMHandleSelfIDInt					= 11,
	kTPResetFWIMHAABB							= 12,
	kTPResetFWIMHandleSystemShutDown			= 13
};
	
// FireWire StateChange Action Tracepoints		
// kFWTStateChangeAction
// FWTrace(kFWTStateChangeAction, kTPStateChangeResetBus, (uintptr_t)fFWIM );
enum
{
	kTPStateChangeResetBus					= 1,
	kTPStateChangeResetStateChange			= 2,
	kTPStateChangeProcessSelfIDs			= 3,
	kTPStateChangeFinishedBusScan			= 4,
	kTPStateChangeDoBusReset				= 5,
	kTPStateChangeProcessBusReset			= 6
};
	
// FireWire ScheduleReset Action Tracepoints		
// kFWTScheduleResetLinkAction
// FWTrace(kFWTScheduleResetLinkAction, kTPScheduleResetNotifyInvalidSelfIDs, (uintptr_t)this );
enum
{
	kTPScheduleReset						= 1,
	kTPScheduleResetNotifyInvalidSelfIDs	= 2,
	kTPScheduleResetHandleInterrupts		= 3,
	kTPScheduleResetHandleSelfIDInt			= 4,
	kTPScheduleResetWritePhyRegister		= 5,
	kTPScheduleResetReadPhyRegister			= 6,
	kTPScheduleResetSetLinkMode				= 7,
	kTPScheduleResetDCLContextDied			= 8,
	kTPScheduleResetARxResProcessReceivedPacket	= 9,
	kTPScheduleResetARxReqCheckForReceivedPackets = 10,
	kTPScheduleResetARxReqFilterPhysicalReadRequests = 11,
	kTPScheduleResetARxWaitForDMA			= 12,
	kTPScheduleResetATxHandleCompletedCommand = 13,
	kTPScheduleResetATxWaitForDMA			= 14,
	kTPScheduleResetATxAllocateCommandElement = 15
};
	
// Tracepoint macros.
#define FIREWIRE_TRACE(FWClass, code, funcQualifier)	( ( ( DBG_IOKIT & 0xFF ) << 24) | ( ( DBG_IOFIREWIRE & 0xFF ) << 16 ) | ( ( FWClass & 0x3F ) << 10 ) | ( ( code & 0xFF ) << 2 ) | ( funcQualifier & 0x3 ) )

// Family
#define FW_CONTROLLER_TRACE(code)	FIREWIRE_TRACE ( kFWTController, code, DBG_FUNC_NONE )
#define FW_DEVICE_TRACE(code)		FIREWIRE_TRACE ( kFWTDevice, code, DBG_FUNC_NONE )
#define FW_ISOCH_TRACE(code)		FIREWIRE_TRACE ( kFWTIsoch, code, DBG_FUNC_NONE )
#define FW_USERCLIENT_TRACE(code)	FIREWIRE_TRACE ( kFWTUserClient, code, DBG_FUNC_NONE )

// FWIM
#define FW_FWIM_TRACE(code)			FIREWIRE_TRACE ( kFWTFWIM, code, DBG_FUNC_NONE )
#define FW_PM_TRACE(code)			FIREWIRE_TRACE ( kFWTPowerManager, code, DBG_FUNC_NONE )

// Actions
#define FW_RESETBUS_TRACE(code)		FIREWIRE_TRACE ( kFWTResetBusAction, code, DBG_FUNC_NONE )
#define FW_STATECHANGE_TRACE(code)	FIREWIRE_TRACE ( kFWTStateChangeAction, code, DBG_FUNC_NONE )
#define FW_RESETLINK_TRACE(code)	FIREWIRE_TRACE ( kFWTScheduleResetLinkAction, code, DBG_FUNC_NONE )
	
#ifdef KERNEL
	
#include <IOKit/IOTimeStamp.h>

#define FWTrace(FWClass, code, a, b, c, d) {	 	\
	if (gFireWireDebugFlags & kFireWireEnableTracePointsMask) { \
		IOTimeStampConstant( FIREWIRE_TRACE(FWClass, code, DBG_FUNC_NONE), a, b, c, d ); \
	}	 \
}
	
#define FWTrace_Start(FWClass, code, a, b, c, d) {	 	\
	if (gFireWireDebugFlags & kFireWireEnableTracePointsMask) { \
		IOTimeStampConstant( FIREWIRE_TRACE(FWClass, code, DBG_FUNC_START), a, b, c, d ); \
	}	 \
}
	
#define FWTrace_End(FWClass, code, a, b, c, d) {	 	\
	if (gFireWireDebugFlags & kFireWireEnableTracePointsMask) { \
		IOTimeStampConstant( FIREWIRE_TRACE(FWClass, code, DBG_FUNC_END), a, b, c, d ); \
	}	 \
}
/*
static inline void FWTracePoint ( unsigned int FWClass, unsigned int code, unsigned int funcQualifier, uintptr_t a=0, uintptr_t b=0, uintptr_t c=0, uintptr_t d=0 )
{
	if ( gFireWireDebugFlags & kFireWireEnableTracePointsMask )
	{
		IOTimeStampConstant( FIREWIRE_TRACE(FWClass, code, funcQualifier), a, b, c, d );
	}
}
	
static inline void FWTrace ( unsigned int FWClass, unsigned int code, uintptr_t a=0, uintptr_t b=0, uintptr_t c=0, uintptr_t d=0 ) {
	FWTracePoint(FWClass, code, DBG_FUNC_NONE, a, b, c, d );
}
	
static inline void FWTrace_Start ( unsigned int FWClass, unsigned int code, uintptr_t a=0, uintptr_t b=0, uintptr_t c=0, uintptr_t d=0 ) {
	FWTracePoint(FWClass, code, DBG_FUNC_START, a, b, c, d );
}

static inline void FWTrace_End ( unsigned int FWClass, unsigned int code, uintptr_t a=0, uintptr_t b=0, uintptr_t c=0, uintptr_t d=0 ) {
	FWTracePoint(FWClass, code, DBG_FUNC_END, a, b, c, d );
}
*/
#endif



#ifdef __cplusplus
}
#endif


#endif	/* __IOKIT_IO_FIREWIRE_FAMILY_TRACEPOINTS__ */
