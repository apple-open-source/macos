/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <IOKit/IOTypes.h>

typedef	char *Ptr;

#define bit0			0x00000001
#define bit1			0x00000002
#define bit2			0x00000004
#define bit3			0x00000008
#define bit4			0x00000010
#define bit5			0x00000020
#define bit6			0x00000040
#define bit7			0x00000080
#define bit8			0x00000100
#define bit9			0x00000200
#define bit10			0x00000400
#define bit11			0x00000800
#define bit12			0x00001000
#define bit13			0x00002000
#define bit14			0x00004000
#define bit15			0x00008000
#define bit16			0x00010000
#define bit17			0x00020000
#define bit18			0x00040000
#define bit19			0x00080000
#define bit20			0x00100000
#define bit21			0x00200000
#define bit22			0x00400000
#define bit23			0x00800000
#define bit24			0x01000000
#define bit25			0x02000000
#define bit26			0x04000000
#define bit27			0x08000000
#define bit28			0x10000000
#define bit29			0x20000000
#define bit30			0x40000000
#define bit31			0x80000000


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Configuration Registers
 *
 */
enum {
        kConfigStart		= 0x00,
        cwVendorID		= 0x00,	/* 0x1000 */
        cwDeviceID		= 0x02,	/* 0x0003 */
        cwCommand		= 0x04,
        cwStatus		= 0x06,
        clClassCodeAndRevID	= 0x08,
        clHeaderAndLatency	= 0x0C,
        clBaseAddressZero	= 0x10,	/* I/O Base address */
        clBaseAddressOne	= 0x14,	/* Memory Base address */
        clExpansionRomAddr	= 0x30,
        clLatGntIntPinLine	= 0x3C,	/* Max_Lat, Max_Gnt, Int. Pin, Int. Line */
        kConfigEnd		= 0x40
};

/*
 * 0x04 cwCommand	Command Register (read/write)
 */
enum {
	cwCommandSERREnable		= bit8,
	cwCommandEnableParityError	= bit6,
	cwCommandEnableBusMaster	= bit2,	/* Set this on initialization	*/
	cwCommandEnableMemorySpace	= bit1,	/* Respond at Base Address One if set	*/
	cwCommandEnableIOSpace		= bit0	/* Respond at Base Address Zero if set	*/
};
/*
 * 0x06 cwStatus	Status Register (read/write)
 */
enum {
	cwStatusDetectedParityError	= bit15, /* Detected from slave			*/
	cwStatusSignaledSystemError	= bit14, /* Device asserts SERR/ signal		*/
	cwStatusMasterAbort		= bit13, /* Master sets when transaction aborts	*/
	cwStatusReceivedTargetAbort	= bit12, /* Master sets when target-abort	*/
	cwStatusDEVSELTimingMask	= (bit10 | bit9), /* DEVSEL timing encoding R/O	*/
	cwStatusDEVSELFastTiming	= 0,
	cwStatusDEVSELMediumTiming	= bit9,
	cwStatusDEVSELSlowTiming	= bit10,
	cwStatusDataParityReported	= bit8
};


////////////////////////////////////////////////////////////////////////////////
//
// OHCI type defs.
//
typedef UInt32		PhysicalPtr;

typedef struct OHCIRegistersStruct
                    OHCIRegisters,
                    *OHCIRegistersPtr;

typedef struct OHCIIntHeadStruct
                    OHCIIntHead,
                    *OHCIIntHeadPtr;

typedef struct OHCIEndpointDescriptorStruct
                    OHCIEndpointDescriptor,
                    *OHCIEndpointDescriptorPtr;

typedef struct OHCIGeneralTransferDescriptorStruct
                    OHCIGeneralTransferDescriptor,
                    *OHCIGeneralTransferDescriptorPtr;

typedef struct OHCIIsochTransferDescriptorStruct
                    OHCIIsochTransferDescriptor,
                    *OHCIIsochTransferDescriptorPtr;

typedef struct OHCIPhysicalLogicalStruct
                    OHCIPhysicalLogical,
                    *OHCIPhysicalLogicalPtr;


////////////////////////////////////////////////////////////////////////////////
//
// OHCI register file.
//

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


// OHCI register file.

struct OHCIRegistersStruct
{
        // Control and status group.
        volatile UInt32		hcRevision;
        volatile UInt32		hcControl;
        volatile UInt32		hcCommandStatus;
        volatile UInt32		hcInterruptStatus;
        volatile UInt32		hcInterruptEnable;
        volatile UInt32		hcInterruptDisable;

        // Memory pointer group.
        volatile UInt32		hcHCCA;
        volatile UInt32		hcPeriodCurrentED;
        volatile UInt32		hcControlHeadED;
        volatile UInt32		hcControlCurrentED;
        volatile UInt32		hcBulkHeadED;
        volatile UInt32		hcBulkCurrentED;
        volatile UInt32		hcDoneHead;

        // Frame counter group.
        volatile UInt32		hcFmInterval;
        volatile UInt32		hcFmRemaining;
        volatile UInt32		hcFmNumber;
        volatile UInt32		hcPeriodicStart;
        volatile UInt32		hcLSThreshold;

        // Root hub group.
        volatile UInt32		hcRhDescriptorA;
        volatile UInt32		hcRhDescriptorB;
        volatile UInt32		hcRhStatus;
        volatile UInt32		hcRhPortStatus[1];
};

typedef struct OHCIRegistersStruct *OHCIRegistersPtr;

// hcControl register defs.
enum
{
        kOHCIHcControl_CBSR		= OHCIBitRange (0, 1),
        kOHCIHcControl_CBSRPhase	= OHCIBitRangePhase (0, 1),
        kOHCIHcControl_PLE		= kOHCIBit2,
        kOHCIHcControl_IE		= kOHCIBit3,
        kOHCIHcControl_CLE		= kOHCIBit4,
        kOHCIHcControl_BLE		= kOHCIBit5,
        kOHCIHcControl_HCFS		= OHCIBitRange (6, 7),
        kOHCIHcControl_HCFSPhase	= OHCIBitRangePhase (6, 7),
        kOHCIHcControl_IR		= kOHCIBit8,
        kOHCIHcControl_RWC		= kOHCIBit9,
        kOHCIHcControl_RWE		= kOHCIBit10,

        kOHCIHcControl_Reserved		= OHCIBitRange (11, 31),

        kOHCIFunctionalState_Reset		= 0,
        kOHCIFunctionalState_Resume		= 1,
        kOHCIFunctionalState_Operational	= 2,
        kOHCIFunctionalState_Suspend		= 3
};

// hcCommandStatus register defs.
enum
{
        kOHCIHcCommandStatus_HCR	= kOHCIBit0,
        kOHCIHcCommandStatus_CLF	= kOHCIBit1,
        kOHCIHcCommandStatus_BLF	= kOHCIBit2,
        kOHCIHcCommandStatus_OCR	= kOHCIBit3,
        kOHCIHcCommandStatus_SOC	= OHCIBitRange (16, 17),
        kOHCIHcCommandStatus_SOCPhase	= OHCIBitRangePhase (16, 17),

        kOHCIHcCommandStatus_Reserved	= OHCIBitRange (4, 15) | OHCIBitRange (18, 31)
};

// hcInterrupt register defs.
enum
{
        kOHCIHcInterrupt_SO		= kOHCIBit0,
        kOHCIHcInterrupt_WDH		= kOHCIBit1,
        kOHCIHcInterrupt_SF		= kOHCIBit2,
        kOHCIHcInterrupt_RD		= kOHCIBit3,
        kOHCIHcInterrupt_UE		= kOHCIBit4,
        kOHCIHcInterrupt_FNO		= kOHCIBit5,
        kOHCIHcInterrupt_RHSC		= kOHCIBit6,
        kOHCIHcInterrupt_OC		= kOHCIBit30,
        kOHCIHcInterrupt_MIE		= kOHCIBit31
};

// this is what I would like it to be
//#define kOHCIDefaultInterrupts		(kOHCIHcInterrupt_SO | kOHCIHcInterrupt_WDH | kOHCIHcInterrupt_UE | kOHCIHcInterrupt_FNO | kOHCIHcInterrupt_RHSC)
//this is in fc2 right now
//#define kOHCIDefaultInterrupts		(kOHCIHcInterrupt_WDH | kOHCIHcInterrupt_UE | kOHCIHcInterrupt_FNO)

#define kOHCIDefaultInterrupts		(kOHCIHcInterrupt_WDH | kOHCIHcInterrupt_RD | kOHCIHcInterrupt_UE | kOHCIHcInterrupt_FNO | kOHCIHcInterrupt_SO)


// hcFmInterval register defs.
enum
{
        kOHCIHcFmInterval_FI		= OHCIBitRange (0, 13),
        kOHCIHcFmInterval_FIPhase	= OHCIBitRangePhase (0, 13),
        kOHCIHcFmInterval_FSMPS		= OHCIBitRange (16, 30),
        kOHCIHcFmInterval_FSMPSPhase	= OHCIBitRangePhase (16, 30),
        kOHCIHcFmInterval_FIT		= kOHCIBit31,

        kOHCIHcFmInterval_Reserved	= OHCIBitRange (14, 15)
};

enum
{
        kOHCIMax_OverHead		= 210	// maximum bit time overhead for a xaction
};


// hcRhDescriptorA register defs.
enum
{
    kOHCIHcRhDescriptorA_NDP		= OHCIBitRange (0, 7),
    kOHCIHcRhDescriptorA_NDPPhase	= OHCIBitRangePhase (0, 7),
    kOHCIHcRhDescriptorA_PSM		= kOHCIBit8,
    kOHCIHcRhDescriptorA_NPS		= kOHCIBit9,
    kOHCIHcRhDescriptorA_DT		= kOHCIBit10,
    kOHCIHcRhDescriptorA_OCPM		= kOHCIBit11,
    kOHCIHcRhDescriptorA_NOCP		= kOHCIBit12,
    kOHCIHcRhDescriptorA_POTPGT		= OHCIBitRange (24, 31),
    kOHCIHcRhDescriptorA_POTPGTPhase	= OHCIBitRangePhase (24, 31),

    kOHCIHcRhDescriptorA_Reserved	= OHCIBitRange (13,23)
};


// hcRhDescriptorB register defs.
enum
{
    kOHCIHcRhDescriptorB_DR		= OHCIBitRange (0, 15),
    kOHCIHcRhDescriptorB_DRPhase	= OHCIBitRangePhase (0, 15),
    kOHCIHcRhDescriptorB_PPCM		= OHCIBitRange (16, 31),
    kOHCIHcRhDescriptorB_PPCMPhase	= OHCIBitRangePhase (16, 31)
};


// Config space defs.
enum
{
    kOHCIConfigRegBaseAddressRegisterNumber	= 0x10
};


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



// misc definitions -- most of these need to be cleaned up/replaced with better terms defined previously

enum
{
    // Barry, note - Root hub defines moved to OHCIRootHub.h

    kOHCIEndpointNumberOffset		= 7,
    kOHCIEndpointDirectionOffset	= 11,
    kOHCIMaxPacketSizeOffset		= 16,
    kOHCISpeedOffset			= 13,
    kOHCIBufferRoundingOffset		= 18,
    kOHCIDirectionOffset		= 19,
    kENOffset				= 7,

    kUniqueNumMask			= OHCIBitRange (0, 12),
    kUniqueNumNoDirMask			= OHCIBitRange (0, 10),
    kOHCIHeadPMask			= OHCIBitRange (4, 31),
    kOHCIInterruptSOFMask		= kOHCIHcInterrupt_SF,
    kOHCISkipped			= kOHCIEDControl_K,
    kOHCIDelayIntOffset			= 21,
    kOHCIPageSize			= 4096,
    kOHCIEndpointDirectionMask		= OHCIBitRange (11, 12),
    kOHCIEDToggleBitMask 		= OHCIBitRange (1, 1),
    kOHCIGTDClearErrorMask		= OHCIBitRange (0, 25),
    kHCCAalignment			= 0x100,	// required alignment for HCCA
    kHCCAsize				= 256		// size of HCCA
};


enum {
    kOHCIBulkTransferOutType		= 1,
    kOHCIBulkTransferInType		= 2,
    kOHCIControlSetupType		= 3,
    kOHCIControlDataType		= 4,
    kOHCIControlStatusType 		= 5,
    kOHCIInterruptInType		= 6,
    kOHCIInterruptOutType		= 7,
    kOHCIOptiLSBug			= 8,
    kOHCIIsochronousInType		= 9,
    kOHCIIsochronousOutType		= 10
};

enum {
    kOHCIFrameOffset			= 16,
    kOHCIFmNumberMask			= OHCIBitRange (0, 15),
    kOHCIFrameOverflowBit		= kOHCIBit16,
    kOHCIMaxRetrys			= 20

};

////////////////////////////////////////////////////////////////////////////////
//
// OHCI UIM data records.
//

//typedef short RootHubID;

// Interrupt head struct
struct OHCIIntHeadStruct
{
    OHCIEndpointDescriptorPtr		pHead;
    OHCIEndpointDescriptorPtr		pTail;
    UInt32				pHeadPhysical;
    int					nodeBandwidth;
};

struct OHCIEndpointDescriptorStruct
{
    UInt32			flags;			// 0x00 control
    PhysicalPtr			tdQueueTailPtr;		// 0x04 pointer to last TD (physical address)
    PhysicalPtr			tdQueueHeadPtr;		// 0x08 pointer to first TD (physical)
    PhysicalPtr			nextED;			// 0x0c Pointer to next ED (physical)
    OHCIEndpointDescriptorPtr	pLogicalNext;		// 0x10
    PhysicalPtr			pPhysical;		// 0x14
    void*			pLogicalTailP;		// 0x18
    void*			pLogicalHeadP;		// 0x1c
};	// 0x20 length of structure

struct OHCIGeneralTransferDescriptorStruct
{
    volatile UInt32			ohciFlags;		// 0x00 Data controlling transfer.
    volatile PhysicalPtr		currentBufferPtr;	// 0x04 Current buffer pointer (physical)
    volatile PhysicalPtr		nextTD;			// 0x08 Pointer to next transfer descriptor
    PhysicalPtr				bufferEnd;		// 0x0c Pointer to end of buffer (physical)
    IOUSBCommand	    	  	*command;		// 0x10 the command associated with this TD
    UInt32				lastFrame;		// 0x14 the lower 32 bits the last time we checked this TD
    UInt32				lastRemaining;		// 0x18 the "remaining" count the last time we checked
    OHCIEndpointDescriptorPtr		pEndpoint;		// 0x1c pointer to TD's Endpoint
    UInt16				pType;			// 0x20 Note this must appear at the same offset (32) in GTD & ITD structs
    UInt16				uimFlags;		// 0x22 Note this must appear at the same offset (34) in GTD & ITD structs
    PhysicalPtr				pPhysical;		// 0x24 Note this must appear at the same offset (36) in GTD & ITD structs
    OHCIGeneralTransferDescriptorPtr	pLogicalNext;		// 0x28 Note this must appear at the same offset (40) in GTD & ITD structs
    UInt32				bufferSize;		// 0x2c used only by control transfers to keep track of data buffers size leftover
								// 0x30 total length
};

struct OHCIIsochTransferDescriptorStruct
{
    UInt32				flags;		// 0x00 Condition code/FrameCount/DelayInterrrupt/StartingFrame.
    PhysicalPtr				bufferPage0;	// 0x04 Buffer Page 0 (physical)
    PhysicalPtr				nextTD;		// 0x08 Pointer to next transfer descriptor (physical)
    PhysicalPtr				bufferEnd;	// 0x0c Pointer to end of buffer (physical)
    UInt16				offset[8];	// 0x10
    UInt16				pType;		// 0x20 Note this must appear at the same offset (32) in GTD & ITD structs
    UInt16				uimFlags;	// 0x22 Note this must appear at the same offset (34) in GTD & ITD structs
    UInt32				pPhysical;	// 0x24 Note this must appear at the same offset (36) in GTD & ITD structs
    OHCIIsochTransferDescriptorPtr	pLogicalNext;	// 0x28 Note this must appear at the same offset (40) in GTD & ITD structs
    IOUSBIsocCompletion  		completion;	// 0x2c callback for Isoch transactions
    IOUSBIsocFrame *			pIsocFrame;	// 0x38 ptr to USLs status and length array
    UInt32				frameNum;	// 0x3c	index to pIsocFrame array
							// 0x40 total length
};

struct OHCIPhysicalLogicalStruct
{
    UInt32				LogicalStart;
    UInt32				LogicalEnd;
    UInt32				PhysicalStart;
    UInt32				PhysicalEnd;
    UInt32				type;
    OHCIPhysicalLogicalStruct *		pNext;
};

#define kOHCIPageOffsetMask	( kOHCIPageSize - 1 )		// mask off just the offset bits (low 12)
#define kOHCIPageMask 		(~(kOHCIPageOffsetMask))	// mask off just the page number (high 20)

