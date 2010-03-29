/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _USBEHCI_H_
#define _USBEHCI_H_

#ifndef	__IOKIT_IOTYPES_H
#include <IOKit/IOTypes.h>
#endif

#ifndef _IOKIT_IOUSBCOMMAND_H
#include <IOKit/usb/IOUSBCommand.h>
#endif

// OHCI needed for error codes which we translate to
#ifndef _USBOHCI_H_
#include "USBOHCI.h"
#endif

#define		APPLE_USB_EHCI_64	1


typedef	char *Ptr;

////////////////////////////////////////////////////////////////////////////////
//
// EHCI register file.
//

enum{a=1};

enum
{
        kEHCIBit0					= (1 << 0),
        kEHCIBit1					= (1 << 1),
        kEHCIBit2					= (1 << 2),
        kEHCIBit3					= (1 << 3),
        kEHCIBit4					= (1 << 4),
        kEHCIBit5					= (1 << 5),
        kEHCIBit6					= (1 << 6),
        kEHCIBit7					= (1 << 7),
        kEHCIBit8					= (1 << 8),
        kEHCIBit9					= (1 << 9),
        kEHCIBit10					= (1 << 10),
        kEHCIBit11					= (1 << 11),
        kEHCIBit12					= (1 << 12),
        kEHCIBit13					= (1 << 13),
        kEHCIBit14					= (1 << 14),
        kEHCIBit15					= (1 << 15),
        kEHCIBit16					= (1 << 16),
        kEHCIBit17					= (1 << 17),
        kEHCIBit18					= (1 << 18),
        kEHCIBit19					= (1 << 19),
        kEHCIBit20					= (1 << 20),
        kEHCIBit21					= (1 << 21),
        kEHCIBit22					= (1 << 22),
        kEHCIBit23					= (1 << 23),
        kEHCIBit24					= (1 << 24),
        kEHCIBit25					= (1 << 25),
        kEHCIBit26					= (1 << 26),
        kEHCIBit27					= (1 << 27),
        kEHCIBit28					= (1 << 28),
        kEHCIBit29					= (1 << 29),
        kEHCIBit30					= (1 << 30),
        kEHCIBit31					= (1 << 31)
};

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
    kEHCIPageSize			= 4096,
    kEHCIPagesPerTD			= 5
};
#define kEHCIPageOffsetMask	( kEHCIPageSize - 1 )		// mask off just the offset bits (low 12)
#define kEHCIPageMask 		(~(kEHCIPageOffsetMask))	// mask off just the page number (high 20)

#define	kEHCIStructureAllocationPhysicalMask	0x00000000FFFFF000ULL			// for use with inTaskWithPhysicalMask (below 4GB and 4K aligned)

enum{
	kEHCIMaxPollingInterval		= 32,					// the maximum polling rate (in ms)
	kEHCIuFramesPerFrame		= 8,					// 8 uFrames per frame
	};

enum{
	kEHCIPeriodicListEntries 	= 1024,
	kEHCIPeriodicFrameListsize 	= 4096
	};

// bandwidth numbers (for various packets exclusive of the data content)
enum {
	kEHCIHSTokenSameDirectionOverhead = 19,			// 4 byte sync, 3 byte token, 1 byte EOP, 11 byte ipg
	kEHCIHSTokenChangeDirectionOverhead = 9,		// 4 byte sync, 3 byte token, 1 byte EOP, 1 byte bus turn
	kEHCIHSDataSameDirectionOverhead = 19,			// 4 byte sync, 1 byte PID, 2 byte CRC, 1 byte EOP, 11 byte ipg
	kEHCIHSDataChangeDirectionOverhead = 9,			// 4 byte sync, 1 byte PID, 2 byte CRC, 1 byte EOP, 1 byte bus turn
	kEHCIHSHandshakeOverhead = 7,					// 4 byte sync, 1 byte PID, 1 byte EOP, 1 byte bus turn
	kEHCIHSSplitSameDirectionOverhead = 39,			// 4 byte sync, 4 byte split token, 1 byte EOP, 11 byte ipg, 4 byte sync, 3 byte FS token, 1 byte EOP, 11 byte ipg
	kEHCIHSSplitChangeDirectionOverhead = 29,		// 4 byte sync, 4 byte split token, 1 byte EOP, 11 byte ipg, 4 byte sync, 3 byte FS token, 1 byte EOP, 1 byte ipg
	kEHCIHSMaxPeriodicBytesPeruFrame = 6000,		// 80% of 7500 bytes in a microframe
	
	// for use by split transactions
	kEHCIFSSplitInterruptOverhead = 13,
	kEHCILSSplitInterruptOverhead = ((14 * 8) + 5),
	kEHCIFSSplitIsochOverhead = 9,
	kEHCISplitTTThinkTime = 1,						// think time of a HS hub TT (may be adjusted)
	kEHCIFSSOFBytesUsed = 6,						// Bytes in a SOF packet on the FS bus
	kEHCIFSHubAdjustBytes = 30,						// Number of bytes to give away for the hub's use at the beginning of the frame (0-60)
	kEHCIFSMinStartTime = (kEHCIFSSOFBytesUsed + kEHCIFSHubAdjustBytes),
	kEHCIFSMaxFrameBytes = 1157,					// (90% of 1500) * 6 / 7 for bit stuffing
	kEHCIFSLargeIsochPacket = 579,					// this is a "large" Isoch packet, which means it will take more than half the total frame. it is treated "special"
	kEHCIFSBytesPeruFrame = 188,					// max FS bytes per microframe
};

// This belongs in USB.h
enum {
    kUSBMaxHSIsocFrameReqCount = (3*1024) // max size (bytes) of any one Isoc frame
};


enum
{
	kEHCIPortSC_Connect				= kEHCIBit0,
	kEHCIPortSC_ConnectChange		= kEHCIBit1,
	kEHCIPortSC_Enabled				= kEHCIBit2,
	kEHCIPortSC_EnableChange		= kEHCIBit3,
	kEHCIPortSC_OverCurrent			= kEHCIBit4,
	kEHCIPortSC_OCChange			= kEHCIBit5,
	kEHCIPortSC_Resume				= kEHCIBit6,
	kEHCIPortSC_Suspend				= kEHCIBit7,
	kEHCIPortSC_Reset				= kEHCIBit8,
	kEHCIPortSC_LineSt				= EHCIBitRange (10, 11),
	kEHCIPortSC_LineStPhase			= EHCIBitRangePhase (10, 11),
	kEHCIPortSC_Power				= kEHCIBit12,
	kEHCIPortSC_Owner				= kEHCIBit13,
	kEHCIPortSC_TestControl			= EHCIBitRange(16, 19),
	kEHCIPortSC_TestControlPhase	= EHCIBitRangePhase(16, 19),
	kEHCIPortSC_WKCNNT_E			= kEHCIBit20,
	kEHCIPortSC_WKDSCNNT_E			= kEHCIBit21,
	kEHCILine_Low					= 1
};


// these are for the HCSPARAMS register
enum
{
	kEHCINumPortsMask	= EHCIBitRange (0, 3),				// Number of ports (4 bits)
	kEHCIPPCMask 		= kEHCIBit4,						// Power Port Control
};


// these are for the HCCPARAMS register
enum
{
	kEHCIISTMask		= EHCIBitRange(4, 7),				// Isochronous Scheduling Threshold mask
	kEHCIISTPhase		= EHCIBitRangePhase(4, 7),			// IST shift amount
	kEHCIEECPMask		= EHCIBitRange(8, 15),				// EECP offset
	kEHCIEECPPhase		= EHCIBitRangePhase(8, 15),			// EECP shift amount
	kEHCI64Bit			= kEHCIBit0							// whether we use 64 bit addressing
};



// This is for various transfer desciptors 
enum{
	kEHCITermFlag = 1
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

enum
{
	kEHCITDFlags_Status		= EHCIBitRange (0, 7),
	kEHCITDFlags_StatusPhase	= EHCIBitRangePhase (0, 7),
	kEHCITDFlags_PID		= EHCIBitRange (8, 9),
	kEHCITDFlags_PIDPhase		= EHCIBitRangePhase (8, 9),
	kEHCITDFlags_Cerr		= EHCIBitRange (10, 11),
	kEHCITDFlags_CerrPhase		= EHCIBitRangePhase (10, 11),
	kEHCITDFlags_C_Page		= EHCIBitRange (12, 14),
	kEHCITDFlags_C_PagePhase	= EHCIBitRangePhase (12, 14),
	kEHCITDFlags_IOC		= EHCIBitRange (15, 15),
	kEHCITDFlags_IOCPhase		= EHCIBitRangePhase (15, 15),
	kEHCITDFlags_Bytes		= EHCIBitRange (16, 30),
	kEHCITDFlags_BytesPhase		= EHCIBitRangePhase (16, 30),
	kEHCITDFlags_DT			= EHCIBitRange (31, 31),
	kEHCITDFlags_DTPhase		= EHCIBitRangePhase (31, 31),
	
	kEHCITDioc			= kEHCIBit15,

    /* don't confuse these with Isoc errors, these are EHCI_TD errors */
	kEHCITDStatus_Active		= kEHCIBit7,
	kEHCITDStatus_Halted		= kEHCIBit6,
	kEHCITDStatus_BuffErr		= kEHCIBit5,
	kEHCITDStatus_Babble		= kEHCIBit4,
	kEHCITDStatus_XactErr		= kEHCIBit3,
	kEHCITDStatus_MissedUF		= kEHCIBit2,
	kEHCITDStatus_SplitXState	= kEHCIBit1,
	kEHCITDStatus_PingState		= kEHCIBit0,

    /* Isoc fields in the transaction fields  */
    
	kEHCI_ITDTr_Offset		= EHCIBitRange (0, 11),
	kEHCI_ITDTr_OffsetPhase		= EHCIBitRangePhase (0, 11),
	kEHCI_ITDTr_Page		= EHCIBitRange (12, 14),
	kEHCI_ITDTr_PagePhase		= EHCIBitRangePhase (12, 14),
	kEHCI_ITDTr_IOC			= kEHCIBit15,
	kEHCI_ITDTr_Len			= EHCIBitRange (16, 27),
	kEHCI_ITDTr_LenPhase		= EHCIBitRangePhase (16, 27),
    /* Isoc status bits are different. */
	kEHCI_ITDStatus_Active		= kEHCIBit31,
	kEHCI_ITDStatus_BuffErr		= kEHCIBit30,
	kEHCI_ITDStatus_Babble		= kEHCIBit29,
	kEHCI_ITDStatus_XactErr		= kEHCIBit28,

    /* Isoc in the buffer fields */
	kEHCI_ITDBuf_Ptr		= EHCIBitRange (12, 31),
	kEHCI_ITDBuf_PtrPhase		= EHCIBitRangePhase (12, 31),
	kEHCI_ITDBuf_FnAddr		= EHCIBitRange (0, 6),
	kEHCI_ITDBuf_FnAddrPhase	= EHCIBitRangePhase (0, 6),
	kEHCI_ITDBuf_R			= kEHCIBit8,
	kEHCI_ITDBuf_EP			= EHCIBitRange (8, 11),
	kEHCI_ITDBuf_EPPhase		= EHCIBitRangePhase (8, 11),
	kEHCI_ITDBuf_IO			= kEHCIBit11,
	kEHCI_ITDBuf_MPS		= EHCIBitRange (0, 10),
	kEHCI_ITDBuf_MPSPhase		= EHCIBitRangePhase (0, 10),
	kEHCI_ITDBuf_Mult		= EHCIBitRange (0, 1),
	kEHCI_ITDBuf_MultPhase		= EHCIBitRangePhase (0, 1),

    // split Isoc section (Figure 3-5)
	// route flags
	kEHCIsiTDRouteDirection		= EHCIBitRange(31, 31),
	kEHCIsiTDRouteDirectionPhase	= EHCIBitRangePhase(31, 31),
	kEHCIsiTDRoutePortNumber	= EHCIBitRange(24, 30),
	kEHCIsiTDRoutePortNumberPhase	= EHCIBitRangePhase(24, 30),
	// bit 23 is reserved
	kEHCIsiTDRouteHubAddr		= EHCIBitRange(16, 22),
	kEHCIsiTDRouteHubAddrPhase	= EHCIBitRangePhase(16, 22),
	// bits 12-15 are reserved
	kEHCIsiTDRouteEndpoint		= EHCIBitRange(8, 11),
	kEHCIsiTDRouteEndpointPhase	= EHCIBitRangePhase(8, 11),
	// bit 7 is reserved
	kEHCIsiTDRouteDeviceAddr	= EHCIBitRange(0, 6),
	kEHCIsiTDRouteDeviceAddrPhase	= EHCIBitRangePhase(0, 6),
	
	// time flags
	kEHCIsiTDTimeCMask		= EHCIBitRange(8, 15),
	kEHCIsiTDTimeCMaskPhase		= EHCIBitRangePhase(8, 15),
	kEHCIsiTDTimeSMask		= EHCIBitRange(0, 7),
	kEHCIsiTDTimeSMaskPhase		= EHCIBitRangePhase(0, 7),
	
	// stat flags
	kEHCIsiTDStatIOC				= EHCIBitRange(31, 31),
	kEHCIsiTDStatIOCPhase			= EHCIBitRangePhase(31, 31),
	kEHCIsiTDStatPageSelect			= EHCIBitRange(30, 30),
	kEHCIsiTDStatPageSelectPhase	= EHCIBitRangePhase(30, 30),
	// bits 26-29 are reserved
	kEHCIsiTDStatLength				= EHCIBitRange(16, 25),
	kEHCIsiTDStatLengthPhase		= EHCIBitRangePhase(16, 25),
	kEHCIsiTDStatCSplitProg			= EHCIBitRange(8, 15),
	kEHCIsiTDStatCSplitProgPhase	= EHCIBitRangePhase(8, 15),
	kEHCIsiTDStatStatus				= EHCIBitRange(0, 7),
	kEHCIsiTDStatStatusActive		= EHCIBitRange(7, 7),
	kEHCIsiTDStatStatusERR			= EHCIBitRange(6, 6),	// ERR from transaction translator
	kEHCIsiTDStatStatusDBE			= EHCIBitRange(5, 5),	// data buffer err (over/underrun)
	kEHCIsiTDStatStatusBabble		= EHCIBitRange(4, 4),	// babble detected
	kEHCIsiTDStatStatusXActErr		= EHCIBitRange(3, 3),	// invalid response (IN only)
	kEHCIsiTDStatStatusMMF			= EHCIBitRange(2, 2),	// missed micro-frame
	kEHCUsiTDStatStatusSplitXState	= EHCIBitRange(1,1),	// do-start-split or do-complete-split
	kEHCIsiTDStatStatusPhase		= EHCIBitRangePhase(0, 7),
	
	// buffer pointer 1 extra bits
	kEHCIsiTDBuffPtr1TP		= EHCIBitRange(3, 4),
	kEHCIsiTDBuffPtr1TPPhase	= EHCIBitRangePhase(3, 4)
	
};


typedef  UInt8		EHCIEDFormat;			// really only need 1 bit

struct EHCICapRegistersStruct
{


//	volatile UInt32				CapVer;
	// This is documented as several smaller regs, but that doesn't work.
	volatile UInt8				CapLength;
	volatile UInt8				Reserved;
	volatile UInt16				HCIVersion;

	volatile UInt32				HCSParams;
	volatile UInt32				HCCParams;
	volatile UInt8				HCSPPortroute[15];
};

typedef struct EHCICapRegistersStruct
								EHCICapRegisters,
								*EHCICapRegistersPtr;

typedef struct EHCIRegistersStruct
								EHCIRegisters,
								*EHCIRegistersPtr;
typedef struct EHCIRegistersStruct
                    EHCIRegisters,
                    *EHCIRegistersPtr;

typedef struct EHCIRegistersStruct *EHCIRegistersPtr;

typedef struct EHCIQueueHeadShared
                    EHCIQueueHeadShared,
                    *EHCIQueueHeadSharedPtr;

typedef struct EHCIGeneralTransferDescriptorShared
		    EHCIGeneralTransferDescriptorShared,
		    *EHCIGeneralTransferDescriptorSharedPtr;

typedef struct EHCIIsochTransferDescriptorShared
                    EHCIIsochTransferDescriptorShared,
                    *EHCIIsochTransferDescriptorSharedPtr;
		    
typedef struct EHCISplitIsochTransferDescriptorShared
		    EHCISplitIsochTransferDescriptorShared,
		    *EHCISplitIsochTransferDescriptorSharedPtr;

struct EHCIRegistersStruct
{
	volatile UInt32				USBCMD;
	volatile UInt32				USBSTS;
	volatile UInt32				USBIntr;
	volatile UInt32				FRIndex;
	volatile UInt32				CTRLDSSegment;
	volatile UInt32				PeriodicListBase;
	volatile UInt32				AsyncListAddr;
	volatile UInt32				Reserved[9];
	volatile UInt32				ConfigFlag;
	volatile UInt32				PortSC[1];
};

// This is the "shared" data area between the controller and the UIM. It is 48 bytes long, but since it must be on a 32 byte
// boundary, and since we allocate them back to back, we pad to 64 bytes

struct EHCIQueueHeadShared
{
	volatile USBPhysicalAddress32				nextQH;				// 0x00 Pointer to next ED (physical)
	volatile UInt32						flags;				// 0x04 
	volatile UInt32						splitFlags;			// 0x08 Routing for split transactions
	volatile USBPhysicalAddress32				CurrqTDPtr;			// 0x0c pointer to last TD (physical address)
	volatile USBPhysicalAddress32				NextqTDPtr;			// 0x10 pointer to first TD (physical)
	volatile USBPhysicalAddress32				AltqTDPtr;			// 0x14 pointer to first TD (physical)
	volatile UInt32						qTDFlags;			// 0x18
	volatile USBPhysicalAddress32				BuffPtr[5];			// 0x1C - 2F	
#if !APPLE_USB_EHCI_64
	UInt32							padding[4];			// 0x30-0x3f
};												// 0x40 length of structure
#else
	volatile USBPhysicalAddress32				extBuffPtr[5];			// 0x30-0x43	
	UInt32							padding[7];			// 0x44-0x5F
};												// 0x60 length of structure
#endif											


struct EHCIGeneralTransferDescriptorShared
{
	volatile USBPhysicalAddress32			nextTD;				// 0x00 Pointer to next transfer descriptor (physical)
	volatile USBPhysicalAddress32			altTD;				// 0x04 Pointer to next transfer descriptor on short packet(physical)
	volatile UInt32					flags;				// 0x08 Data controlling transfer.
	volatile USBPhysicalAddress32			BuffPtr[5];			// 0x0c-0x1f  buffer pointer (physical)
#if !APPLE_USB_EHCI_64
};											// 0x20 length of structure
#else
	volatile USBPhysicalAddress32			extBuffPtr[5];			// 0x20-0x33  buffer pointer (physical)
	UInt32						padding[3];			// 0x34-0x3F padding for alignment
};											// 0x40 length of data structure
#endif 


struct EHCIIsochTransferDescriptorShared
{
	volatile USBPhysicalAddress32				nextiTD;			// 0x00 Pointer to next transfer descriptor (physical)
	UInt32									Transaction0;		// 0x04 status, len, offset etc.
	UInt32									Transaction1;		// 0x08 status, len, offset etc.
	UInt32									Transaction2;		// 0x0c status, len, offset etc.
	UInt32									Transaction3;		// 0x10 status, len, offset etc.
	UInt32									Transaction4;		// 0x14 status, len, offset etc.
	UInt32									Transaction5;		// 0x18 status, len, offset etc.
	UInt32									Transaction6;		// 0x1c status, len, offset etc.
	UInt32									Transaction7;		// 0x20 status, len, offset etc.
	USBPhysicalAddress32						bufferPage0;		// 0x24 Buffer Page 0 (physical)
	USBPhysicalAddress32						bufferPage1;		// 0x28 Buffer Page 1 (physical)
	USBPhysicalAddress32						bufferPage2;		// 0x2c Buffer Page 2 (physical)
	USBPhysicalAddress32						bufferPage3;		// 0x30 Buffer Page 3 (physical)
	USBPhysicalAddress32						bufferPage4;		// 0x34 Buffer Page 4 (physical)
	USBPhysicalAddress32						bufferPage5;		// 0x38 Buffer Page 5 (physical)
	USBPhysicalAddress32						bufferPage6;		// 0x3c Buffer Page 6 (physical)
#if !APPLE_USB_EHCI_64
};																// 0x40 length of structure
#else
	USBPhysicalAddress32						extBufferPage0;		// 0x40 extended Buffer Page 0 (physical)
	USBPhysicalAddress32						extBufferPage1;		// 0x44 extended Buffer Page 1 (physical)
	USBPhysicalAddress32						extBufferPage2;		// 0x48 extended Buffer Page 2 (physical)
	USBPhysicalAddress32						extBufferPage3;		// 0x4c extended Buffer Page 3 (physical)
	USBPhysicalAddress32						extBufferPage4;		// 0x50 extended Buffer Page 4 (physical)
	USBPhysicalAddress32						extBufferPage5;		// 0x54 extended Buffer Page 5 (physical)
	USBPhysicalAddress32						extBufferPage6;		// 0x58 extended Buffer Page 6 (physical)
	UInt32									padding;			// 0x5C to align on 32 bit boundary
};																// 0x60 length of structure
#endif


struct EHCISplitIsochTransferDescriptorShared
{
    volatile USBPhysicalAddress32				nextSITD;		// 0x00 Physical Ptr to next descriptor (QH, sitd, itd, or fstn)
    volatile UInt32							routeFlags;		// 0x04 How to route the packet
    volatile UInt32							timeFlags;		// 0x08 which microframe to go on
    volatile UInt32							statFlags;		// 0x0c status information
    volatile UInt32							buffPtr0;		// 0x10 page 0 pointer and offset
    volatile UInt32							buffPtr1;		// 0x14 page 1 pointer and other flags
    volatile USBPhysicalAddress32				backPtr;		// 0x18 back pointer
#if !APPLE_USB_EHCI_64
    UInt32									padding;		// 0x1c make sure we are 32 byte aligned
};															// 0x20 length of structure
#else
    volatile USBPhysicalAddress32				extBuffPtr0;	// 0x1c extended page 0 pointer
    volatile USBPhysicalAddress32				extBuffPtr1;	// 0x20 extended page 1 pointer
    UInt32									padding[7];		// 0x24 to make sure of 32 bit alignment
};															// 0x40 length of data structure
#endif



enum
{
	kEHCIPortRoutingBit = kEHCIBit0
};

enum
{ 	// STS bits, not int status
    kEHCISTSAsyncScheduleStatus = kEHCIBit15,
    kEHCISTSPeriodicScheduleStatus = kEHCIBit14
};



enum
{    // Interrupt enables and status bits in USBSTS/USBIntr

    kEHCIHCHaltedBit = kEHCIBit12,
    kEHCIAAEIntBit = kEHCIBit5,
    kEHCIHostErrorIntBit = kEHCIBit4,
    kEHCIFrListRolloverIntBit = kEHCIBit3,
    kEHCIPortChangeIntBit = kEHCIBit2,
    kEHCIErrorIntBit = kEHCIBit1,
    kEHCICompleteIntBit = kEHCIBit0,
    kEHCIIntBits = (kEHCIAAEIntBit | kEHCIHostErrorIntBit | kEHCIFrListRolloverIntBit | 
				    kEHCIPortChangeIntBit | kEHCIErrorIntBit | kEHCICompleteIntBit)
};

enum
{   // CMD reg fields
    kEHCICMDRunStop = kEHCIBit0,
    kEHCICMDHCReset = kEHCIBit1,

    kEHCICMDFrameListSizeMask = EHCIBitRange(2,3),
    kEHCICMDFrameListSizeOffset = (1<<2),
    
    kEHCICMDPeriodicEnable = kEHCIBit4,
    kEHCICMDAsyncEnable = kEHCIBit5,
    kEHCICMDAsyncDoorbell = kEHCIBit6,

    kEHCICMDAsyncParkModeCountMask = EHCIBitRange(8,9),
    kEHCICMDAsyncParkModeCountMaskPhase = EHCIBitRangePhase(8,9),
    kEHCICMDAsyncParkModeEnable = kEHCIBit11,

    kEHCICMDIntThresholdMask = EHCIBitRange(16,23),
    kEHCICMDIntThresholdOffset = EHCIBitRangePhase(16,23)

};

enum 
{
    kEHCITypeBulk		= 1,
    kEHCITypeControl	= 2,
    kEHCITypeInterrupt	= 3,
    kEHCITypeIsoch		= 4
};

// bits in the FRIndex register
enum
{
    kEHCIFRIndexMillisecondMask		= EHCIBitRange (3, 13),
    kEHCIFrameNumberIncrement		= kEHCIBit11,
    // please note: if the Frame List Size in the USBCMD register changes, this needs to change as well
    kEHCIFRIndexRolloverBit			= kEHCIBit13,
    kEHCIMicroFrameNumberIncrement	= kEHCIBit14		// ok, not really in the register, but related to the above
};

// bits in USBLEGSUP extended EHCI/PCI capability
enum {
    kEHCI_USBLEGSUP_ID        = 0x01,
    kEHCI_USBLEGSUP_BIOSOwned = kEHCIBit16,
    kEHCI_USBLEGSUP_OSOwned   = kEHCIBit24
};

#pragma mark ---end----
#endif
