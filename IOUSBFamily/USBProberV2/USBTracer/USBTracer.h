/*
 * Copyright © 2009-2012 Apple Inc.  All rights reserved.
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

//—————————————————————————————————————————————————————————————————————————————
//	Includes
//—————————————————————————————————————————————————————————————————————————————

#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <spawn.h>

#include <mach/clock_types.h>
#include <mach/mach_time.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/file.h>

#include <libutil.h>		// for reexec_to_match_kernel()
#include <mach/mach_host.h> // for host_info()

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/usb/USB.h>
#include "USBTracepoints.h"
	
#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif // KERNEL_PRIVATE

#include <AvailabilityMacros.h>


#include "USBTracepoints.h"
#include "IOUSBFamilyInfoPlist.pch"

#define DEBUG 			0

#ifndef USBTRACE_VERSION
	#define	USBTRACE_VERSION "100.4.0"
#endif

#define log(a,b,c,d,x,...)			if ( PrintHeader(a,b,c,d) ) { if (x){fprintf(stdout,x, ##__VA_ARGS__);} fprintf(stdout, "\n"); }
#define logs(a,b,c,x...)			log(a,b,c,0,x...)
#define	vlog(x...)					if ( gVerbose ) { fprintf(stdout,x); }
#define	elog(x...)					fprintf(stderr, x)

//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define kTraceBufferSampleSize			65500
#define kMicrosecondsPerSecond			1000000
#define kMicrosecondsPerMillisecond		1000
#define kPrintMaskAllTracepoints		0x80000000
#define kTimeStringSize					44
#define kTimeStampKernel				0x1
#define kTimeStampLocalTime				0x2
#define kPrintStartToken				"->"
#define kPrintEndToken					"<-"
#define kPrintMedialToken				"  "
#define	kFilePathMaxSize				256
#define	kMicrosecondsPerCollectionDelay	20
#define gTimeStampDivisorString			"TimeStampDivisor="
#define kInvalid						0xdeadbeef
#define kDivisorEntry					0xfeedface
#define kKernelTraceCodes				"/usr/local/share/misc/trace.codes"

//—————————————————————————————————————————————————————————————————————————————
//	Types
//—————————————————————————————————————————————————————————————————————————————

typedef struct {
	uint64_t	timestamp;
	uintptr_t	thread;       /* will hold current thread */
	uint32_t	debugid;
	uint32_t	cpuid;
} trace_info;


//—————————————————————————————————————————————————————————————————————————————
//	OHCI Defines
//—————————————————————————————————————————————————————————————————————————————

enum
{
	kOHCIBit0					= (1 << 0),
	kOHCIBit1					= (1 << 1),
	kOHCIBit2					= (1 << 2),
	kOHCIBit3					= (1 << 3),
	kOHCIBit4					= (1 << 4),
	kOHCIBit5					= (1 << 5),
	kOHCIBit6					= (1 << 6),
	kOHCIBit7					= (1 << 7),
	kOHCIBit8					= (1 << 8),
	kOHCIBit9					= (1 << 9),
	kOHCIBit10					= (1 << 10),
	kOHCIBit11					= (1 << 11),
	kOHCIBit12					= (1 << 12),
	kOHCIBit13					= (1 << 13),
	kOHCIBit14					= (1 << 14),
	kOHCIBit15					= (1 << 15),
	kOHCIBit16					= (1 << 16),
	kOHCIBit17					= (1 << 17),
	kOHCIBit18					= (1 << 18),
	kOHCIBit19					= (1 << 19),
	kOHCIBit20					= (1 << 20),
	kOHCIBit21					= (1 << 21),
	kOHCIBit22					= (1 << 22),
	kOHCIBit23					= (1 << 23),
	kOHCIBit24					= (1 << 24),
	kOHCIBit25					= (1 << 25),
	kOHCIBit26					= (1 << 26),
	kOHCIBit27					= (1 << 27),
	kOHCIBit28					= (1 << 28),
	kOHCIBit29					= (1 << 29),
	kOHCIBit30					= (1 << 30),
	kOHCIBit31					= (1 << 31)
};

#define OHCIBitRange(start, end)				\
(								\
((((UInt32) 0xFFFFFFFF) << (31 - (end))) >>		\
((31 - (end)) + (start))) <<				\
(start)							\
)

#define OHCIBitRangePhase(start, end)				\
(start)

#define EHCIBitRange(start, end)				\
(								\
((((UInt32) 0xFFFFFFFF) << (31 - (end))) >>		\
((31 - (end)) + (start))) <<				\
(start)							\
)

#define EHCIBitRangePhase(start, end)				\
(start)

enum
{
    kOHCIEDControl_FA			= OHCIBitRange (0,  6),
    kOHCIEDControl_FAPhase		= OHCIBitRangePhase (0, 6),
    kOHCIEDControl_EN			= OHCIBitRange (7, 10),
    kOHCIEDControl_ENPhase		= OHCIBitRangePhase (7, 10),
    kOHCIEDControl_D			= OHCIBitRange (11, 12),
    kOHCIEDControl_DPhase		= OHCIBitRangePhase (11, 12),
    kOHCIEDControl_S			= OHCIBitRange (13, 13),
    kOHCIEDControl_SPhase		= OHCIBitRangePhase (13, 13),
    kOHCIEDControl_K			= kOHCIBit14,
    kOHCIEDControl_F			= OHCIBitRange (15, 15),
    kOHCIEDControl_FPhase		= OHCIBitRangePhase (15, 15),
    kOHCIEDControl_MPS			= OHCIBitRange (16, 26),
    kOHCIEDControl_MPSPhase		= OHCIBitRangePhase (16, 26),
	
    kOHCITailPointer_tailP		= OHCIBitRange (4, 31),
    kOHCITailPointer_tailPPhase		= OHCIBitRangePhase (4, 31),
	
    kOHCIHeadPointer_H			= kOHCIBit0,
    kOHCIHeadPointer_C			= kOHCIBit1,
    kOHCIHeadPointer_headP		= OHCIBitRange (4, 31),
    kOHCIHeadPointer_headPPhase		= OHCIBitRangePhase (4, 31),
	
    kOHCINextEndpointDescriptor_nextED		= OHCIBitRange (4, 31),
    kOHCINextEndpointDescriptor_nextEDPhase	= OHCIBitRangePhase (4, 31),
	
    kOHCIEDDirectionTD			= 0,
    kOHCIEDDirectionOut			= 1,
    kOHCIEDDirectionIn			= 2,
	
    kOHCIEDSpeedFull			= 0,
    kOHCIEDSpeedLow			= 1,
	
    kOHCIEDFormatGeneralTD		= 0,
    kOHCIEDFormatIsochronousTD		= 1
};

typedef  UInt8 OHCIEDFormat;	// really only need 1 bit

// General Transfer Descriptor
enum
{
    kOHCIGTDControl_R			= kOHCIBit18,
    kOHCIGTDControl_DP			= OHCIBitRange (19, 20),
    kOHCIGTDControl_DPPhase		= OHCIBitRangePhase (19, 20),
    kOHCIGTDControl_DI			= OHCIBitRange (21, 23),
    kOHCIGTDControl_DIPhase		= OHCIBitRangePhase (21, 23),
    kOHCIGTDControl_T			= OHCIBitRange (24, 25),
    kOHCIGTDControl_TPhase		= OHCIBitRangePhase (24, 25),
    kOHCIGTDControl_EC			= OHCIBitRange (26, 27),
    kOHCIGTDControl_ECPhase		= OHCIBitRangePhase (26, 27),
    kOHCIGTDControl_CC			= OHCIBitRange (28, 31),
    kOHCIGTDControl_CCPhase		= OHCIBitRangePhase (28, 31),
	
    kOHCIGTDPIDSetup			= 0,
    kOHCIGTDPIDOut			= 1,
    kOHCIGTDPIDIn			= 2,
	
    kOHCIGTDNoInterrupt			= 7,
	
    kOHCIGTDDataToggleCarry		= 0,
    kOHCIGTDDataToggle0			= 2,
    kOHCIGTDDataToggle1			= 3,
	
    kOHCIGTDConditionNoError		= 0,
    kOHCIGTDConditionCRC		= 1,
    kOHCIGTDConditionBitStuffing	= 2,
    kOHCIGTDConditionDataToggleMismatch	= 3,
    kOHCIGTDConditionStall		= 4,
    kOHCIGTDConditionDeviceNotResponding	= 5,
    kOHCIGTDConditionPIDCheckFailure	= 6,
    kOHCIGTDConditionUnexpectedPID	= 7,
    kOHCIGTDConditionDataOverrun	= 8,
    kOHCIGTDConditionDataUnderrun	= 9,
    kOHCIGTDConditionBufferOverrun	= 12,
    kOHCIGTDConditionBufferUnderrun	= 13,
    kOHCIGTDConditionNotAccessed	= 15
};

// Isochronous Transfer Descriptor
enum
{
    kOHCIITDControl_SF			= OHCIBitRange (0,15),
    kOHCIITDControl_SFPhase		= OHCIBitRangePhase(0,15),				
    kOHCIITDControl_DI			= OHCIBitRange (21,23),
    kOHCIITDControl_DIPhase		= OHCIBitRangePhase (21,23),
    kOHCIITDControl_FC			= OHCIBitRange (24,26),
    kOHCIITDControl_FCPhase		= OHCIBitRangePhase (24,26),
    kOHCIITDControl_CC			= OHCIBitRange (28,31),
    kOHCIITDControl_CCPhase		= OHCIBitRangePhase (28,31),
	
    // The Offset/PSW words have two slightly different formats depending on whether they have been accessed
    // by the host controller or not.   They are initialized in Offset format, with 3-bit of condition code (=NOTACCESSED)
    // if the OHCI controller accesses this frame it fills in the 4-bit condition code, and the PSW size field contains
    // the number of bytes transferred IN, or 0 for an OUT transaction.
	
    // PSW format bit field definitions
    kOHCIITDPSW_Size			= OHCIBitRange(0,10),
    kOHCIITDPSW_SizePhase		= OHCIBitRangePhase(0,10),
    kOHCIITDPSW_CC			= OHCIBitRange(12,15),
    kOHCIITDPSW_CCPhase 		= OHCIBitRangePhase(12,15),
	
    // offset format bit field definitions
    kOHCIITDOffset_Size			= OHCIBitRange(0,11),
    kOHCIITDOffset_SizePhase		= OHCIBitRangePhase(0,11),
    kOHCIITDOffset_PC			= OHCIBitRange(12,12),
    kOHCIITDOffset_PCPhase		= OHCIBitRangePhase(12,12),
    kOHCIITDOffset_CC			= OHCIBitRange(13,15),
    kOHCIITDOffset_CCPhase 		= OHCIBitRangePhase(13,15),
    kOHCIITDConditionNoError		= 0,
    kOHCIITDConditionCRC		= 1,
    kOHCIITDConditionBitStuffing	= 2,
    kOHCIITDConditionDataToggleMismatch	= 3,
    kOHCIITDConditionStall		= 4,
    kOHCIITDConditionDeviceNotResponding = 5,
    kOHCIITDConditionPIDCheckFailure	= 6,
    kOHCIITDConditionUnExpectedPID	= 7,
    kOHCIITDConditionDataOverrun	= 8,
    kOHCIITDConditionDataUnderrun	= 9,
    kOHCIITDConditionBufferOverrun	= 12,
    kOHCIITDConditionBufferUnderrun	= 13,
    kOHCIITDOffsetConditionNotAccessed	= 7,	// note that this is the "Offset" variant (3-bit) of this condition code
    kOHCIITDConditionNotAccessedReturn	= 15,
    kOHCIITDConditionNotCrossPage	= 0,
    kOHCIITDConditionCrossPage		= 1
};

enum
{
	// Endpoint Characteristics - see EHCI spec table 3-19
	kEHCIEDFlags_FA			= EHCIBitRange (0,  6),
	kEHCIEDFlags_FAPhase		= EHCIBitRangePhase (0, 6),
	kEHCIEDFlags_EN			= EHCIBitRange (8, 11),
	kEHCIEDFlags_ENPhase		= EHCIBitRangePhase (8, 11),
	kEHCIEDFlags_S			= EHCIBitRange (12, 13),
	kEHCIEDFlags_SPhase		= EHCIBitRangePhase (12, 13),
	kEHCIEDFlags_MPS		= EHCIBitRange (16, 26),
	kEHCIEDFlags_MPSPhase		= EHCIBitRangePhase (16, 26),
	
	kEHCIEDFlags_C			= EHCIBitRange (27, 27),
	kEHCIEDFlags_CPhase		= EHCIBitRangePhase (27, 27),
	kEHCIEDFlags_H			= EHCIBitRange (15, 15),
	kEHCIEDFlags_HPhase		= EHCIBitRangePhase (15, 15),
	kEHCIEDFlags_DTC		= EHCIBitRange (14, 14),
	kEHCIEDFlags_DTCPhase		= EHCIBitRangePhase (14, 14),
	
	// Endpoint capabilities - see EHCI spec table 3-20 
	kEHCIEDSplitFlags_Mult		= EHCIBitRange (30, 31),
	kEHCIEDSplitFlags_MultPhase	= EHCIBitRangePhase (30, 31),
	kEHCIEDSplitFlags_Port		= EHCIBitRange (23, 29),
	kEHCIEDSplitFlags_PortPhase	= EHCIBitRangePhase (23, 29),
	kEHCIEDSplitFlags_HubAddr	= EHCIBitRange (16, 22),
	kEHCIEDSplitFlags_HubAddrPhase	= EHCIBitRangePhase (16, 22),
	kEHCIEDSplitFlags_CMask		= EHCIBitRange (8, 15),
	kEHCIEDSplitFlags_CMaskPhase	= EHCIBitRangePhase (8, 15),
	kEHCIEDSplitFlags_SMask		= EHCIBitRange (0, 7),
	kEHCIEDSplitFlags_SMaskPhase	= EHCIBitRangePhase (0, 7),
	
	kEHCIUniqueNumNoDirMask				= kEHCIEDFlags_EN | kEHCIEDFlags_FA,
	
	
	kEHCIEDDirectionTD			= 2,
	//	kEHCIEDDirectionOut			= 1,
	//	kEHCIEDDirectionIn			= 2,
	
	//	kEHCIGTDPIDOut				= 0,
	//	kEHCIGTDPIDIn				= 1,
	//	kEHCIGTDPIDSetup			= 2,
	
	kEHCIEDFormatGeneralTD		= 0,
	kEHCIEDFormatIsochronousTD	= 1
	
};

enum{
	kEHCITyp_iTD 				= 0,
	kEHCITyp_QH 				= 1,
	kEHCITyp_siTD 				= 2,
	kEHCIEDNextED_Typ			= EHCIBitRange (1,  2),
	kEHCIEDNextED_TypPhase		= EHCIBitRangePhase (1,  2),
	kEHCIEDTDPtrMask			= EHCIBitRange (5,  31)
	
};

enum
{
	kXHCIBit0					= (1 << 0),
	kXHCIBit1					= (1 << 1),
	kXHCIBit2					= (1 << 2),
	kXHCIBit3					= (1 << 3),
	kXHCIBit4					= (1 << 4),
	kXHCIBit5					= (1 << 5),
	kXHCIBit6					= (1 << 6),
	kXHCIBit7					= (1 << 7),
	kXHCIBit8					= (1 << 8),
	kXHCIBit9					= (1 << 9),
	kXHCIBit10					= (1 << 10),
	kXHCIBit11					= (1 << 11),
	kXHCIBit12					= (1 << 12),
	kXHCIBit13					= (1 << 13),
	kXHCIBit14					= (1 << 14),
	kXHCIBit15					= (1 << 15),
	kXHCIBit16					= (1 << 16),
	kXHCIBit17					= (1 << 17),
	kXHCIBit18					= (1 << 18),
	kXHCIBit19					= (1 << 19),
	kXHCIBit20					= (1 << 20),
	kXHCIBit21					= (1 << 21),
	kXHCIBit22					= (1 << 22),
	kXHCIBit23					= (1 << 23),
	kXHCIBit24					= (1 << 24),
	kXHCIBit25					= (1 << 25),
	kXHCIBit26					= (1 << 26),
	kXHCIBit27					= (1 << 27),
	kXHCIBit28					= (1 << 28),
	kXHCIBit29					= (1 << 29),
	kXHCIBit30					= (1 << 30),
	kXHCIBit31					= (1 << 31)
};


#define XHCIBitRange(start, end)				\
(								\
((((UInt32) 0xFFFFFFFF) << (31 - (end))) >>		\
((31 - (end)) + (start))) <<				\
(start)							\
)

#define XHCIBitRangePhase(start, end)				\
(start)


enum 
{
	kXHCITRB_Normal = 1,
	kXHCITRB_Setup,
	kXHCITRB_Data,
	kXHCITRB_Status,
	kXHCITRB_Isoc,
	kXHCITRB_Link,
	kXHCITRB_EventData,
	kXHCITRB_TrNoOp,
	
	kXHCITRB_EnableSlot = 9,
	kXHCITRB_DisableSlot = 10,
	kXHCITRB_AddressDevice = 11,
	kXHCITRB_ConfigureEndpoint = 12,
	kXHCITRB_EvaluateContext = 13,
	kXHCITRB_ResetEndpoint = 14,
	kXHCITRB_StopEndpoint = 15,
	kXHCITRB_SetTRDqPtr = 16,
	kXHCITRB_ResetDevice = 17,
	
    kXHCITRB_GetPortBandwidth = 21,
	kXHCITRB_CMDNoOp = 23,
    
	kXHCITRB_CMDNEC = 49,   // NEC vendor specific command to get firmware version
	
	kXHCITRB_TE = 32,
	kXHCITRB_CCE = 33,
	kXHCITRB_PSCE = 34,
	kXHCITRB_DevNot = 38,   // Device notification event.
	kXHCITRB_MFWE = 39,
    kXHCITRB_NECCCE = 48,
	
    // TRT- Transfer type in a Control request TRB.
    kXHCI_TRT_NoData = 0,
    kXHCI_TRT_OutData = 2,
    kXHCI_TRT_InData = 3,
    
    
	kXHCIFrameNumberIncrement = kXHCIBit11,
    
    // Note XHCI spec sec 4.11.2.5 says an Isoc transaction shouldn't be more than 895ms in future.
    kXHCIFutureIsocLimit = 895,
	
	kXHCITRB_C = kXHCIBit0,
	kXHCITRB_DCS = kXHCIBit0,
	kXHCITRB_TC = kXHCIBit1,
	kXHCITRB_ENT = kXHCIBit1,
	kXHCITRB_ISP = kXHCIBit2,
	kXHCITRB_ED = kXHCIBit2,
	kXHCITRB_CH = kXHCIBit4,
	kXHCITRB_IOC = kXHCIBit5,
	kXHCITRB_IDT = kXHCIBit6,
	kXHCITRB_BSR = kXHCIBit9,
	kXHCITRB_BEI = kXHCIBit9,
	kXHCITRB_DIR = kXHCIBit16,
	
	kXHCITRB_Normal_Len_Mask = XHCIBitRange(0, 16),
	kXHCITRB_TDSize_Mask = XHCIBitRange(17, 21),
	kXHCITRB_TDSize_Shift = XHCIBitRangePhase(17, 21),
	kXHCITRB_InterrupterTarget_Mask = XHCIBitRange(22, 31),
	kXHCITRB_InterrupterTarget_Shift = XHCIBitRangePhase(22, 31),
	kXHCITRB_TBC_Mask = XHCIBitRange(7,8),
	kXHCITRB_TBC_Shift = XHCIBitRangePhase(7,8),
	kXHCITRB_TLBPC_Mask = XHCIBitRange(16, 19),
	kXHCITRB_TLBPC_Shift = XHCIBitRangePhase(16, 19),
	kXHCITRB_Type_Mask = XHCIBitRange(10, 15),
	kXHCITRB_Type_Shift = XHCIBitRangePhase(10, 15),
	kXHCITRB_TRT_Mask = XHCIBitRange(16, 17),
	kXHCITRB_TRT_Shift = XHCIBitRangePhase(16, 17),
	kXHCITRB_SlotID_Mask = XHCIBitRange(24, 31),
	kXHCITRB_SlotID_Shift = XHCIBitRangePhase(24, 31),
	kXHCITRB_TR_Len_Mask = XHCIBitRange(0, 23),
	kXHCITRB_CC_Mask = XHCIBitRange(24, 31),
	kXHCITRB_CC_Shift = XHCIBitRangePhase(24, 31),
	kXHCITRB_Ep_Mask = XHCIBitRange(16, 20),
	kXHCITRB_Ep_Shift = XHCIBitRangePhase(16, 20),
	kXHCITRB_Stream_Mask = XHCIBitRange(16, 31),
	kXHCITRB_Stream_Shift = XHCIBitRangePhase(16, 31),
	kXHCITRB_Port_Mask = XHCIBitRange(24, 31),
	kXHCITRB_Port_Shift = XHCIBitRangePhase(24, 31),
	
    // Section 6.4.5 TRB Completion Codes
	kXHCITRB_CC_Invalid = 0,
	kXHCITRB_CC_Success = 1,
	kXHCITRB_CC_Data_Buffer = 2,
	kXHCITRB_CC_Babble_Detected = 3,
	kXHCITRB_CC_XActErr = 4,
	kXHCITRB_CC_TRBErr = 5,
	kXHCITRB_CC_STALL = 6,
	kXHCITRB_CC_ResourceErr = 7,
	kXHCITRB_CC_Bandwidth = 8,
	kXHCITRB_CC_NoSlots = 9,
	kXHCITRB_CC_Invalid_Stream_Type = 10,
	kXHCITRB_CC_Slot_Not_Enabled = 11,
	kXHCITRB_CC_Endpoint_Not_Enabled = 12,
	kXHCITRB_CC_ShortPacket = 13,
	kXHCITRB_CC_RingUnderrun = 14,
	kXHCITRB_CC_RingOverrun = 15,
	kXHCITRB_CC_VF_Event_Ring_Full = 16,
	kXHCITRB_CC_CtxParamErr = 17,
	kXHCITRB_CC_Bandwidth_Overrun = 18,
	kXHCITRB_CC_CtxStateErr = 19,
	kXHCITRB_CC_No_Ping_Response = 20,
	kXHCITRB_CC_Event_Ring_Full = 21,
	kXHCITRB_CC_Incompatible_Device = 22,
	kXHCITRB_CC_Missed_Service = 23,
	kXHCITRB_CC_CMDRingStopped = 24,
	kXHCITRB_CC_Command_Aborted = 25,
	kXHCITRB_CC_Stopped = 26,
	kXHCITRB_CC_Stopped_Length_Invalid = 27,
	kXHCITRB_CC_Max_Exit_Latency_Too_Large = 29,
	kXHCITRB_CC_Isoch_Buffer_Overrun = 31,
	kXHCITRB_CC_Event_Lost = 32,
	kXHCITRB_CC_Undefined = 33,
	kXHCITRB_CC_Invalid_Stream_ID = 34,
	kXHCITRB_CC_Secondary_Bandwidth = 35,
	kXHCITRB_CC_Split_Transaction = 36,
    
    // Intel specifc errors
	kXHCITRB_CC_CNTX_ENTRIES_GTR_MAXEP = 192,
	kXHCITRB_CC_FORCE_HDR_USB2_NO_SUPPORT = 193,
	kXHCITRB_CC_UNDEFINED_BEHAVIOR = 194,
	kXHCITRB_CC_CMPL_VEN_DEF_ERR_195 = 195,
	kXHCITRB_CC_NOSTOP = 196,
	kXHCITRB_CC_HALT_STOP = 197,
	kXHCITRB_CC_DL_ERR = 198,
	kXHCITRB_CC_CMPL_WITH_EMPTY_CONTEXT = 199,
	kXHCITRB_CC_VENDOR_CMD_FAILED = 200,
	
    kXHCITRB_CC_NULLRing = 256, // Fake error to return if you find ring is NULL
    
	kXHCITRB_FrameID_Mask = XHCIBitRange(20, 30),
	kXHCITRB_FrameID_Shift = XHCIBitRangePhase(20, 30),
	kXHCIFrameMask = XHCIBitRange(0,10)	
};



//—————————————————————————————————————————————————————————————————————————————
//	Codes
//—————————————————————————————————————————————————————————————————————————————

#define kTPAllUSB					USB_TRACE ( 0, 0, 0 )

#pragma mark Prototypes
//———————————————————————————————————————————————————————————————————————————
//	Prototypes
//———————————————————————————————————————————————————————————————————————————

static void EnableUSBTracing ( void );
static void EnableTraceBuffer ( int val );
static void SignalHandler ( int signal );
static void GetDivisor ( void );
static void RegisterSignalHandlers ( void );
static void AllocateTraceBuffer ( void );
static void RemoveTraceBuffer ( void );
static void SetTraceBufferSize ( int nbufs );
static void GetTraceBufferInfo ( kbufinfo_t * val );
static void Quit ( const char * s );
static void ResetDebugFlags ( void );
static void InitializeTraceBuffer ( void );
static void Reinitialize ( void );
static void SetInterest ( unsigned int type );
static void ParseArguments ( int argc, const char * argv[] );
static void PrintUsage ( void );

static void CollectTrace ( void );
static void CollectWithAlloc( void );
static void ProcessTracepoint( kd_buf tracepoint );

static void CollectTraceController( kd_buf tracepoint );		
static void CollectTraceControllerUserClient( kd_buf tracepoint );
static void CollectTraceDevice ( kd_buf tracepoint ); //2,
static void CollectTraceDeviceUserClient ( kd_buf tracepoint ); //3,
static void CollectTraceHub ( kd_buf tracepoint ); //4,
static void CollectTraceHubPort ( kd_buf tracepoint ); //5,
static void CollectTraceHSHubUserClient ( kd_buf tracepoint ); //6,
static void CollectTraceHID	( kd_buf tracepoint ); //7,
static void CollectTracePipe ( kd_buf tracepoint ); //8,
static void CollectTraceInterfaceUserClient	( kd_buf tracepoint ); //9,

static void CollectTraceEnumeration( kd_buf tracepoint ); //10
// UIM groupings
static void CollectTraceUHCI ( kd_buf tracepoint ); //11,
static void CollectTraceUHCIUIM	( kd_buf tracepoint ); 
static void CollectTraceUHCIInterrupts ( kd_buf tracepoint ); //13,
static void CollectTraceOHCI ( kd_buf tracepoint ); //14,
static void CollectTraceOHCIInterrupts ( kd_buf tracepoint ); //15,
static void CollectTraceOHCIDumpQs ( kd_buf tracepoint ); //16,
static void CollectTraceEHCI ( kd_buf tracepoint ); //20,
static void CollectTraceEHCIUIM	( kd_buf tracepoint ); //21,
static void CollectTraceEHCIHubInfo	( kd_buf tracepoint ); //22,
static void CollectTraceEHCIInterrupts	( kd_buf tracepoint ); //23,
static void CollectTraceEHCIDumpQs ( kd_buf tracepoint ); //24,
static void CollectTraceXHCI ( kd_buf tracepoint );				//20,
static void CollectTraceXHCIInterrupts	( kd_buf tracepoint ); //23,
static void CollectTraceXHCIRootHubs	( kd_buf tracepoint ); //24,
static void CollectTraceXHCIPrintTRB	( kd_buf tracepoint ); //25,

static void CollectTraceHubPolicyMaker	( kd_buf tracepoint ); //35,
static void CollectTraceCompositeDriver ( kd_buf tracepoint ); //36,
// Actions
static void CollectTraceOutstandingIO ( kd_buf tracepoint ); //42,

// Other drivers
static void CollectTraceAudioDriver ( kd_buf tracepoint ); //50,

static void CollectTraceUnknown ( kd_buf tracepoint );

static void CollectCodeFile ( void );
static void ReadRawFile( const char * filepath );
static void CollectToRawFile ( FILE * file );
static void PrependDivisorEntry ( FILE * file );
static void CollectTraceUnknown ( kd_buf tracepoint );
static char * DecodeID ( uint32_t id, char * string, const int max );
static void CollectTraceBasic ( kd_buf tracepoint );

static char * ConvertTimestamp ( uint64_t timestamp, char * timestring );
static bool PrintHeader ( trace_info info, const char * group, const char * method, uintptr_t theThis );
static void TabIndent ( int numOfTabs );
static void Indent ( int numOfTabs );
static void IndentIn ( int numOfTabs );
static void IndentOut ( int numOfTabs );

const char * DecodeUSBTransferType( uint32_t type );
