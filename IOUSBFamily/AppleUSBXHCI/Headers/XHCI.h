/*
 *  XHCI.h
 *
 *  Copyright © 2011 Apple Inc. All Rights Reserved. 
 *
 */

#ifndef _XHCI_H_
#define _XHCI_H_

#include <IOKit/assert.h>


// These belong in the IOUSBFamily, and should be transplanted sometime.

enum 
{
    kMaxUSB3HubDepth =5,
};
#define kXHCIInvalidRegisterValue	0xFFFFFFFF				// if a register returns this value, the controller is likely gone


typedef u_int64_t USBPhysicalAddress64;
#define HostToUSBI64(x)  (x)
#define USBToHostI64(x)  (x)

struct XHCICapRegistersStruct
{
	//	volatile UInt32				CapVer;
	// This is documented as several smaller regs, but that doesn't work.
	volatile UInt8				CapLength;
	volatile UInt8				Reserved;
	volatile UInt16				HCIVersion;
	
	volatile UInt32				HCSParams1;
	volatile UInt32				HCSParams2;
	volatile UInt32				HCSParams3;
	volatile UInt32				HCCParams;
	volatile UInt32				DBOff;
	volatile UInt32				RTSOff;
	volatile UInt32				reserved;
};

OSCompileAssert ( sizeof ( struct XHCICapRegistersStruct ) == 32 );


struct XHCPortRegStruct
{
	volatile UInt32				PortSC;
	volatile UInt32				PortPMSC;
	volatile UInt32				PortLI;
	volatile UInt32				reserved;
};

typedef struct XHCPortRegStruct
XHCPortReg,
*XHCPortRegStructPtr;

OSCompileAssert ( sizeof ( XHCPortReg ) == 16 );


struct XHCIRegistersStruct
{
	volatile UInt32				USBCMD;
	volatile UInt32				USBSTS;
	volatile UInt32				PageSize;
	volatile UInt32				reserved1;
	
	volatile UInt32				reserved2;
	volatile UInt32				DNCtrl;
	volatile UInt64				CRCR;
	
	volatile UInt32				reserved4;
	volatile UInt32				reserved5;
	volatile UInt32				reserved6;
	volatile UInt32				reserved7;
	
	volatile UInt64				DCBAAP;
	volatile UInt32				Config;
	volatile UInt32				Reserved9[(0x400-0x3c)/sizeof(UInt32)];
	
	volatile XHCPortReg			PortReg[1];
};


typedef struct XHCICapRegistersStruct
XHCICapRegisters,
*XHCICapRegistersPtr;

typedef struct XHCIRegistersStruct
XHCIRegisters,
*XHCIRegistersPtr;

struct XHCIXECPHeaderStruct
{
	volatile UInt8				CapabilityID;
	volatile UInt8				NextPtr;
};

typedef struct XHCIXECPHeaderStruct
XHCIXECPHeader,
*XHCIXECPHeaderPtr;

OSCompileAssert ( sizeof ( XHCIXECPHeader ) == 2 );


struct XHCIXECPStruct
{
	XHCIXECPHeader				Header;
	volatile UInt16				Attrib;
};

typedef struct XHCIXECPStruct
XHCIXECPRegisters,
*XHCIXECPRegistersPtr;

OSCompileAssert ( sizeof ( XHCIXECPRegisters ) == 4 );


struct XHCIXECPProtocolStruct
{
	XHCIXECPHeader	Header;
	volatile UInt8	MinRevision;
	volatile UInt8	MajRevision;
	volatile UInt32	NameString;

	volatile UInt8	compatiblePortOffset;
	volatile UInt8	compatiblePortCount;
	volatile UInt16	protocolDefs;
};

typedef struct XHCIXECPProtocolStruct
XHCIXECPProtoRegisters,
*XHCIXECPProtoRegistersPtr;

OSCompileAssert ( sizeof ( XHCIXECPProtoRegisters ) == 12 );


struct StreamContextStruct
{
	volatile UInt32 offs0;
	volatile UInt32 offs4;
	volatile UInt32 offs8;
	volatile UInt32 offsC;
};

typedef struct StreamContextStruct
StreamContext,
*StreamContextPtr;

OSCompileAssert ( sizeof ( StreamContext ) == 16 );


struct TRBStruct
{
	volatile UInt32 offs0;
	volatile UInt32 offs4;
	volatile UInt32 offs8;
	volatile UInt32 offsC;
};

typedef struct TRBStruct
TRB,
*TRBPtr;

OSCompileAssert ( sizeof ( TRB ) == 16 );


// Refer section 6.2 in XHCI specification
//
//      6.2.2 Slot Context Data Structure
//
//          offs00 - Context Entries[31..27] Hub[26] MTT[25] RsvdZ[24] Speed[23..20] Route String[19..0] 
//          offs04 - Number of Ports[31..24] Root Hub Port Number[23..16] Max Exit Latency[15..0]
//          offs08 - Interrupter Target[31..22] RsvdZ[21..18] TTT[17..16] TT Port Number[15..8] TT Hub Slot ID[7..0]
//          offs0C - Slot State[31..27] RsvdZ[26..8] USB Device Address[7..0]
//          offs10 - RsvdO[31..0]
//          offs14 - RsvdO[31..0]
//          offs18 - RsvdO[31..0]
//          offs1C - RsvdO[31..0]
//          
//      6.2.3 End point Context Data Structure
//
//          offs00 - RsvdZ[31..24] Interval[23..16] LSA[15] MaxPStreams[14..10] Mult[9..8] RsvdZ[7..3] EP State[2..0]
//          offs04 - Max Packet Size[31..16] Max Burst Size[15..8] HID[7] RsvdZ[6] EP Type[5..3] CErr[2..1] RsvdZ[0]
//          offs08 - TR Dequeue Pointer Lo[31..4] RsvdZ[3..1] DCS[0]
//          offs0C - TR Dequeue Pointer Hi[31..0]
//          offs10 - Max ESIT Payload[31..16] Average TRB Length[15..0]
//          offs14 - RsvdO[31..0]
//          offs18 - RsvdO[31..0]
//          offs1C - RsvdO[31..0]
//          
//
//      6.2.4 Stream Context 
//
//          offs00 - TR Dequeue Pointer Lo[31..4] SCT[3..1] DCS[0]
//          offs04 - TR Dequeue Pointer Hi[31..0]
//          offs08 - RsvdO[31..0]
//          offs0C - RsvdO[31..0]
//
//          
//      6.2.5 Input Context
//
//          offs00 - [D31..D2]  - Drop context flags
//          offs04 - [A31..A0]  - Add context flags
//          offs08 - RsvdO[31..0]
//          offs0C - RsvdO[31..0]
//          offs10 - RsvdO[31..0]
//          offs14 - RsvdO[31..0]
//          offs18 - RsvdO[31..0]
//          offs1C - RsvdO[31..0]
//
//      6.2.6 Port Bandwidth Context
//
//          offs00 - Port3[31..24] Port2[23..16] Port1[15..8] Rsvd[7..0]  
//
//                          PortX - Percentage of Total Available Bandwidth
//
//          offs04 - Port7[31..24] Port6[23..16] Port5[15..8] Port4[7..0]  
//
struct ContextStruct
{
	volatile UInt32 offs00;
	volatile UInt32 offs04;
	volatile UInt32 offs08;
	volatile UInt32 offs0C;
	volatile UInt32 offs10;
	volatile UInt32 offs14;
	volatile UInt32 offs18;
	volatile UInt32 offs1C;
};

typedef struct ContextStruct
Context,
*ContextPtr;

OSCompileAssert ( sizeof ( Context ) == 32 );

// When the Context Size (CSZ) bit is set to 1 in HCCPARAMS each context
// data structure is of size 64 bytes. 
struct ContextStruct64
{
	volatile UInt32 offs00;
	volatile UInt32 offs04;
	volatile UInt32 offs08;
	volatile UInt32 offs0C;
	volatile UInt32 offs10;
	volatile UInt32 offs14;
	volatile UInt32 offs18;
	volatile UInt32 offs1C;
	volatile UInt32 offs20;
	volatile UInt32 offs24;
	volatile UInt32 offs28;
	volatile UInt32 offs2C;
	volatile UInt32 offs30;
	volatile UInt32 offs34;
	volatile UInt32 offs38;
	volatile UInt32 offs3C;
};

typedef struct ContextStruct64
Context64,
*ContextPtr64;

OSCompileAssert ( sizeof ( Context64 ) == 64 );


struct InterrupterStruct
{
	volatile UInt32 IMAN;       // Interrupt Management - 5.5.2.1
	volatile UInt32 IMOD;       // Interrupt Moderation - 5.5.2.2
	volatile UInt32 ERSTSZ;     // Event Ring Segment Table Size - 5.5.2.3.1
	volatile UInt32 reserved;   
	volatile UInt64 ERSTBA;     // Event Ring Segment Table Base Address - 5.5.2.3.2
	volatile UInt64 ERDP;       // Event Ring Dequeue Pointer - 5.5.2.3.3
};

typedef struct InterrupterStruct
Interrupter,
*InterrupterPtr;

OSCompileAssert ( sizeof ( Interrupter ) == 32 );

struct XHCIRunTimeRegStruct
{
	volatile UInt32 MFINDEX;
	volatile UInt32 Reserved[(0x20-0x4)/sizeof(UInt32)];
	Interrupter IR[];
};

typedef struct XHCIRunTimeRegStruct
XHCIRunTimeReg,
*XHCIRunTimeRegPtr;

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

// HCIVERSION

enum
{
	kXHCIVersion0100 = 0x0100,
};

// HCSPARAMS1

enum 
{
	kXHCINumDevsMask = XHCIBitRange(0,7),
	kXHCINumPortsMask = XHCIBitRange(24,31),
	kXHCINumPortsShift = XHCIBitRangePhase(24, 31),
	kXHCIMaxInterruptersMask = XHCIBitRange(8,16),
	kXHCIMaxInterruptersShift = XHCIBitRangePhase(8,16),
};

// HCSPARAMS2

enum 
{
	kXHCIMaxScratchpadBufsLo_Mask = XHCIBitRange(27, 31),
	kXHCIMaxScratchpadBufsLo_Shift = XHCIBitRangePhase(27, 31),
	kXHCIMaxScratchpadBufsHi_Mask = XHCIBitRange(21, 25),
	kXHCIMaxScratchpadBufsHi_Shift = XHCIBitRangePhase(21, 25),
	kXHCIMaxScratchpadBufsLo_Width = 5,

	kXHCIIST_Mask = XHCIBitRange(0,3),
	kXHCIIST_Phase = XHCIBitRangePhase(0,3),
	
	kXHCIERSTMax_Mask = XHCIBitRange(4, 7),
	kXHCIERSTMax_Phase = XHCIBitRangePhase(4, 7)
	
};
	
// HCCPARAMS
enum
{
	kXHCIAC64Bit = kXHCIBit0,
	kXHCICSZBit = kXHCIBit2,
	kXHCIPPCBit = kXHCIBit3,
	kXHCIMaxPSA_Mask = XHCIBitRange(12, 15),
	kXHCIMaxPSA_Phase = XHCIBitRangePhase(12, 15),
	
	kXHCIMaxPSASize_Mask =  XHCIBitRange(12, 15),
	kXHCIMaxPSASize_Shift = XHCIBitRangePhase(12, 15),

	kXHCIxECP_Mask =  XHCIBitRange(16, 31),
	kXHCIxECP_Shift = XHCIBitRangePhase(16, 31)
};


// USBCMD
enum 
{
	kXHCICMDRunStop = kXHCIBit0,
	kXHCICMDHCReset = kXHCIBit1,
	kXHCICMDINTE = kXHCIBit2,
	kXHCICMDCSS = kXHCIBit8,
	kXHCICMDCRS = kXHCIBit9,
	kXHCICMDEWE = kXHCIBit10,
};


// USBSTS
enum 
{
	kXHCIHCHaltedBit = kXHCIBit0,
    kXHCIHSEBit = kXHCIBit2,
	kXHCIEINT = kXHCIBit3,
    kXHCIPCD = kXHCIBit4,
	kXHCISSS = kXHCIBit8,
	kXHCIRSS = kXHCIBit9,
	kXHCISRE = kXHCIBit10,
};


// CRCR
enum 
{
	kXHCI_RCS = kXHCIBit0,
	kXHCI_CS  = kXHCIBit1,
	kXHCI_CA  = kXHCIBit2,
	kXHCI_CRR  = kXHCIBit3,
	kXHCICRCRFlags_Mask = XHCIBitRange(0, 5)

};

// PortSC
enum 
{
	kXHCIPortSC_CCS = kXHCIBit0,
	kXHCIPortSC_PED = kXHCIBit1,
	kXHCIPortSC_Rsvd1 = kXHCIBit2,
	kXHCIPortSC_OCA = kXHCIBit3,
	kXHCIPortSC_PR = kXHCIBit4,

	kXHCIPortSC_LinkState_Mask = XHCIBitRange(5, 8),
	kXHCIPortSC_LinkState_Shift = XHCIBitRangePhase(5, 8),

	kXHCIPortSC_PP = kXHCIBit9,
	
	kXHCIPortSC_Speed_Mask = XHCIBitRange(10, 13),
	kXHCIPortSC_Speed_Shift = XHCIBitRangePhase(10, 13),
	
	kXHCISpeed_Full = 1,
	kXHCISpeed_Low = 2,
	kXHCISpeed_High = 3,
	kXHCISpeed_Super = 4,
	

	kXHCIPortSC_LWS = kXHCIBit16,
	kXHCIPortSC_CSC = kXHCIBit17,
	kXHCIPortSC_PEC = kXHCIBit18,
	kXHCIPortSC_WRC = kXHCIBit19,
	kXHCIPortSC_OCC = kXHCIBit20,
	kXHCIPortSC_PRC = kXHCIBit21,
	kXHCIPortSC_PLC = kXHCIBit22,
	kXHCIPortSC_CEC = kXHCIBit23,
	kXHCIPortSC_CAS = kXHCIBit24,
	kXHCIPortSC_WCE = kXHCIBit25,
	kXHCIPortSC_WDE = kXHCIBit26,
	kXHCIPortSC_WOE = kXHCIBit27,

	kXHCIPortSC_DR = kXHCIBit30,
	kXHCIPortSC_WPR = kXHCIBit31,
	
	kXHCIPortSC_Write_Zeros = (	kXHCIPortSC_PED | kXHCIPortSC_PR | kXHCIPortSC_LinkState_Mask | kXHCIPortSC_LWS |
								kXHCIPortSC_CSC | kXHCIPortSC_PEC | kXHCIPortSC_WRC | kXHCIPortSC_OCC | kXHCIPortSC_PRC | 
							   	kXHCIPortSC_PLC | kXHCIPortSC_CEC | kXHCIPortSC_WPR),
	
	// Port Link State
	kXHCIPortSC_PLS_U0 				= 0,
	kXHCIPortSC_PLS_U1 				= 1,
	kXHCIPortSC_PLS_U2 				= 2,
	kXHCIPortSC_PLS_U3 				= 3,
	kXHCIPortSC_PLS_Suspend 		= 3,
	kXHCIPortSC_PLS_Disabled 		= 4,
	kXHCIPortSC_PLS_RxDetect 		= 5,
	kXHCIPortSC_PLS_Inactive 		= 6,
	kXHCIPortSC_PLS_Polling 		= 7,
	kXHCIPortSC_PLS_Recovery 		= 8,
	kXHCIPortSC_PLS_HotReset 		= 9,
	kXHCIPortSC_PLS_ComplianceMode 	= 10,
	kXHCIPortSC_PLS_TestMode 		= 11,
	kXHCIPortSC_PLS_Resume 			= 15,

	//Port Speed
	kXHCIPortSC_FS = 1,
	kXHCIPortSC_LS = 2,
	kXHCIPortSC_HS = 3,
	kXHCIPortSC_SS = 4
	
};

// USB2 PortPMSC
enum 
{
	kXHCIPortMSC_L1S_Mask = XHCIBitRange(0, 2),						// L1 - LPM Status register
	kXHCIPortMSC_L1S_Shift = XHCIBitRangePhase(0, 2),
	
	kXHCIPortMSC_RWE = kXHCIBit3,
	
	kXHCIPortMSC_BESL_Mask = XHCIBitRange(4, 7),					// Best Effort Service Latency
	kXHCIPortMSC_BESL_Shift = XHCIBitRangePhase(4, 7),

	kXHCIPortMSC_L1Device_Mask = XHCIBitRange(8, 15),				// L1 Device Slot
	kXHCIPortMSC_L1Device_Shift = XHCIBitRangePhase(8, 15),
	
	kXHCIPortMSC_HLE = kXHCIBit16,

	kXHCIPortMSC_PortTestControl_Mask = XHCIBitRange(28, 31),		// Port Test Control
	kXHCIPortMSC_PortTestControl_Shift = XHCIBitRangePhase(28, 31)
};

// Interrupter
enum 
{
	kXHCIIRQ_IP = kXHCIBit0,
	kXHCIIRQ_IE = kXHCIBit1,
	kXHCIIRQ_EHB = kXHCIBit3	
};

// DoorBells
enum 
{
    kXHCIDB_Controller = 0,
	kXHCIDB_Stream_Mask = XHCIBitRange(16, 31),
	kXHCIDB_Stream_Shift = XHCIBitRangePhase(16, 31),
};



#if 0
// These need 64 byte alignment, but can't span a page, so make the alignment pagesized.
#define kXHCIDCBAAPhysMask    (0xFFFFFFFFFFFFF000ULL)

// And a 64 byte boundary
#define kXHCICMDRingPhysMask  (0xFFFFFFFFFFFFF000ULL)

// And a 64 byte boundary
#define kXHCIEventRingPhysMask  (0xFFFFFFFFFFFFF000ULL)

#define kXHCIInputContextPhysMask  (0xFFFFFFFFFFFFF000ULL)
#define kXHCIContextPhysMask  (0xFFFFFFFFFFFFF000ULL)
#define kXHCITransferRingPhysMask  (0xFFFFFFFFFFFFF000ULL)

#else
// These need 64 byte alignment, but can't span a page, so make the alignment pagesized.
#define kXHCIDCBAAPhysMask    (0x00000000FFFFF000ULL)

// And a 64 byte boundary
#define kXHCICMDRingPhysMask  (0x00000000FFFFF000ULL)

// And a 64 byte boundary
#define kXHCIEventRingPhysMask  (0x00000000FFFFF000ULL)

#define kXHCIInputContextPhysMask  (0x00000000FFFFF000ULL)
#define kXHCIContextPhysMask  (0x00000000FFFFF000ULL)
#define kXHCITransferRingPhysMask  (0x00000000FFFFF000ULL)
#define kXHCIStreamContextPhysMask  (0x00000000FFFFF000ULL)
#endif

#define kXHCIAC32Mask  (0x00000000FFFFFFFFULL)

enum 
{
	kXHCIDCBAElSize = 8	// 64bit address size
	 
};

// For TRBs


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

	// Separate this one, or it makes all the enums 64 bit and a pain
enum
{
	kXHCITRB_SIA = kXHCIBit31
};


// For Contexts

enum{
    kXHCI_Num_Contexts = 32,
};
// Slot context
enum 
{
	kXHCISlCtx_resZ0Bit = kXHCIBit24,
	kXHCISlCtx_MTTBit = kXHCIBit25,
	kXHCISlCtx_HubBit = kXHCIBit26,
	
	// Route string
	kXHCISlCtx_Route_Mask = XHCIBitRange(0, 19),

	// Device Speed
	kXHCISlCtx_Speed_Mask = XHCIBitRange(20, 23),
	kXHCISlCtx_Speed_Shift = XHCIBitRangePhase(20, 23),
	
	// Context Entries
	kXHCISlCtx_CtxEnt_Mask = XHCIBitRange(27, 31),
	kXHCISlCtx_CtxEnt_Shift = XHCIBitRangePhase(27, 31),
	
	// Root hub port num 
	kXHCISlCtx_RHPNum_Mask = XHCIBitRange(16, 23),
	kXHCISlCtx_RHPNum_Shift = XHCIBitRangePhase(16, 23),
	
	// Number of ports 
	kXHCISlCtx_NumPorts_Mask = XHCIBitRange(24, 31),
	kXHCISlCtx_NumPorts_Shift = XHCIBitRangePhase(24, 31),
	
	// TT SlotID 
	kXHCISlCtx_TTSlot_Mask = XHCIBitRange(0, 7),
	
	// TT port number 
	kXHCISlCtx_TTPort_Mask = XHCIBitRange(8, 15),
	kXHCISlCtx_TTPort_Shift = XHCIBitRangePhase(8, 15),
	
	// TT Think Time 
	kXHCISlCtx_TTT_Mask = XHCIBitRange(16, 17),
	kXHCISlCtx_TTT_Shift = XHCIBitRangePhase(16, 17),
    
    // Interrupter Target
    kXHCISlCtx_Interrupter_Mask = XHCIBitRange(22, 31),
    kXHCISlCtx_Interrupter_Shift = XHCIBitRangePhase(22, 31),

	// USB Device Address
	kXHCISlCtx_USBDeviceAddress_Mask = XHCIBitRange(0, 7),

	// Slot State
	kXHCISlCtx_SlotState_Mask = XHCIBitRange(27, 31),
	kXHCISlCtx_SlotState_Shift = XHCIBitRangePhase(27, 31),
};

// Endpoint context
enum 
{
	kXHCIEpCtx_EPType_IsocOut = 1,
	kXHCIEpCtx_EPType_BulkOut = 2,
	kXHCIEpCtx_EPType_IntOut = 3,
	kXHCIEpCtx_EPType_Control = 4,
	kXHCIEpCtx_EPType_IsocIn = 5,
	kXHCIEpCtx_EPType_BulkIN = 6,
	kXHCIEpCtx_EPType_IntIn = 7,
	
	kXHCIEpCtx_State_Disabled = 0,
	kXHCIEpCtx_State_Running = 1,
	kXHCIEpCtx_State_Halted = 2,
	kXHCIEpCtx_State_Stopped = 3,
	kXHCIEpCtx_State_Error = 4,
	
	kXHCI_SCT_PrimaryTRB = 1,
	
	kXHCIEpCtx_State_Mask = XHCIBitRange(0,2),
	
	kXHCIEpCtx_EPType_Mask = XHCIBitRange(3,5),
	kXHCIEpCtx_EPType_Shift = XHCIBitRangePhase(3,5),

	kXHCIEpCtx_MaxPStreams_Mask = XHCIBitRange(10,14),
	kXHCIEpCtx_MaxPStreams_Shift = XHCIBitRangePhase(10,14),

	kXHCIEpCtx_Mult_Mask = XHCIBitRange(8,9),
	kXHCIEpCtx_Mult_Shift = XHCIBitRangePhase(8,9),
    
	kXHCIEpCtx_MaxBurst_Mask = XHCIBitRange(8,15),
	kXHCIEpCtx_MaxBurst_Shift = XHCIBitRangePhase(8,15),

	kXHCIEpCtx_Interval_Mask = XHCIBitRange(16,23),
	kXHCIEpCtx_Interval_Shift = XHCIBitRangePhase(16,23),

	kXHCIEpCtx_MPS_Mask = XHCIBitRange(16,31),
	kXHCIEpCtx_MPS_Shift = XHCIBitRangePhase(16,31),
	
	kXHCIEpCtx_TRDQpLo_Mask = XHCIBitRange(4,31),
	
	kXHCIEpCtx_DCS = kXHCIBit0,
	kXHCIEpCtx_LSA = kXHCIBit15,

	kXHCIEpCtx_CErr_Mask = XHCIBitRange(1,2),
	kXHCIEpCtx_CErr_Shift = XHCIBitRangePhase(1,2),
	kXHCIEpCtx_MaxESITPayload_Mask = XHCIBitRange(16,31),
	kXHCIEpCtx_MaxESITPayload_Shift = XHCIBitRangePhase(16,31),
	
	kXHCIStrCtx_SCT_Mask = XHCIBitRange(1,3),
	kXHCIStrCtx_SCT_Shift = XHCIBitRangePhase(1,3)
};


// Extended Capability - xHCI - Section 7 - Table 143
enum
{
	kXHCIReserved			= 0,
	kXHCILegacySupport		= 1,
	kXHCISupportedProtocol	= 2,
};

#define kUSBNameString		0x20425355
#define kUSBHSMajversion	0x02
#define kUSBSSMajversion	0x03


#endif

