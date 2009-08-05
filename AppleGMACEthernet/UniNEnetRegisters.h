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
 * Copyright (c) 1998-1999 Apple Computer
 *
 * Interface definition for the Sun GEM (UniN) Ethernet controller.
 *
 *
 */


#define RX_RING_LENGTH_FACTOR	1	// valid from 0 to 8. Overridden by IORegistry value
#define RX_RING_LENGTH			(32 * (1 << RX_RING_LENGTH_FACTOR))	// 64 pkt descs		/* Packet descriptors	*/

#define TX_RING_LENGTH_FACTOR	3	// valid from 0 to 8. Overridden by IORegistry value
#define TX_RING_LENGTH			(32 * (1 << TX_RING_LENGTH_FACTOR))	// 256 pkt descs

#define TX_DESC_PER_INT         32

#define MAX_SEGS_PER_TX_MBUF	32		// maximum number of segments per Transmit mbuf
#define NETWORK_BUFSIZE			((kIOEthernetMaxPacketSize + 4 + 7) & ~7)	// add 4 for VLAN

#define TRANSMIT_QUEUE_SIZE     256		// Overridden by IORegistry value ???

#define WATCHDOG_TIMER_MS       300
#define TX_KDB_TIMEOUT          1000

#define kRxHwCksumStartOffset	34 	/* have hardware start Rx checksumming at byte 34	*/


	struct GMAC_Registers
	{
			/* Global Resources:	*/								// 0x0000

		VU32	SEB_State;		//	3 bits for diagnostics
		VU32	Configuration;	//
		VU32	filler1;
		VU32	Status;			// auto-clear register - use StatusAlias to peek 

		VU32	InterruptMask;										// 0x0010
		VU32	InterruptAck;
		VU32	filler2;
		VU32	StatusAlias;

		UInt8	filler3[ 0x1000 - 0x20 ];

		VU32	PCIErrorStatus;										// 0x1000
		VU32	PCIErrorMask;
		VU32	BIFConfiguration;
		VU32	BIFDiagnostic;

		VU32	SoftwareReset;										// 0x1010

		UInt8	filler4[ 0x2000 - 0x1014 ];

			/* Transmit DMA registers:	*/

		VU32	TxKick;												// 0x2000
		VU32	TxConfiguration;
		VU32	TxDescriptorBaseLow;
		VU32	TxDescriptorBaseHigh;

		VU32	filler5;											// 0x2010
		VU32	TxFIFOWritePointer;
		VU32	TxFIFOShadowWritePointer;
		VU32	TxFIFOReadPointer;

		VU32	TxFIFOShadowReadPointer;							// 0x2020
		VU32	TxFIFOPacketCounter;
		VU32	TxStateMachine;
		VU32	filler6;

		VU32	TxDataPointerLow;									// 0x2030
		VU32	TxDataPointerHigh;

		UInt8	filler7[ 0x2100 - 0x2038 ];

		VU32	TxCompletion;										// 0x2100
		VU32	TxFIFOAddress;
		VU32	TxFIFOTag;
		VU32	TxFIFODataLow;

		VU32	TxFIFODataHighT1;									// 0x2110
		VU32	TxFIFODataHighT0;
		VU32	TxFIFOSize;

		UInt8	filler8[ 0x3000 - 0x211C ];

			/* WOL - WakeOnLan Registers:	*/

		VU32	WOLMagicMatch[ 3 ];			// 6 address bytes		// 0x3000
		VU32	WOLPatternMatchCount;

		VU32	WOLWakeupCSR;										// 0x3010

		UInt8	filler8plus[ 0x4000 - 0x3014 ];

			/* Receive DMA registers: */

		VU32	RxConfiguration;									// 0x4000
		VU32	RxDescriptorBaseLow;
		VU32	RxDescriptorBaseHigh;
		VU32	RxFIFOWritePointer;

		VU32	RxFIFOShadowWritePointer;							// 0x4010
		VU32	RxFIFOReadPointer;
		VU32	RxFIFOPacketCounter;
		VU32	RxStateMachine;

		VU32	PauseThresholds;									// 0x4020
		VU32	RxDataPointerLow;
		VU32	RxDataPointerHigh;

		UInt8	filler9[ 0x4100 - 0x402C ];

		VU32	RxKick;												// 0x4100
		VU32	RxCompletion;
		VU32	RxBlanking;
		VU32	RxFIFOAddress;

		VU32	RxFIFOTag;											// 0x4110
		VU32	RxFIFODataLow;
		VU32	RxFIFODataHighT0;
		VU32	RxFIFODataHighT1;

		VU32	RxFIFOSize;											// 0x4120

		UInt8	filler10[ 0x6000 - 0x4124 ];

			/* MAC registers: */

		VU32	TxMACSoftwareResetCommand;							// 0x6000
		VU32	RxMACSoftwareResetCommand;
		VU32	SendPauseCommand;
		VU32	filler11;

		VU32	TxMACStatus;			// auto-clear				// 0x6010
		VU32	RxMACStatus;			// auto-clear
		VU32	MACControlStatus;		// auto-clear - Pause state here
		VU32	filler12;

		VU32	TxMACMask;											// 0x6020
		VU32	RxMACMask;
		VU32	MACControlMask;
		VU32	filler13;

		VU32	TxMACConfiguration;									// 0x6030
		VU32	RxMACConfiguration;
		VU32	MACControlConfiguration;
		VU32	XIFConfiguration;

		VU32	InterPacketGap0;									// 0x6040
		VU32	InterPacketGap1;
		VU32	InterPacketGap2;
		VU32	SlotTime;

		VU32	MinFrameSize;										// 0x6050
		VU32	MaxFrameSize;
		VU32	PASize;
		VU32	JamSize;

		VU32	AttemptLimit;										// 0x6060
		VU32	MACControlType;
		UInt8	filler14[ 0x6080 - 0x6068 ];

		VU32	MACAddress[ 9 ];									// 0x6080

		VU32	AddressFilter[ 3 ];									// 0x60A4

		VU32	AddressFilter2_1Mask;								// 0x60B0
		VU32	AddressFilter0Mask;
		VU32	filler15[ 2 ];

		VU32	HashTable[ 16 ];									// 0x60C0

			/* Statistics registers:	*/

		VU32	NormalCollisionCounter;								// 0x6100
		VU32	FirstAttemptSuccessfulCollisionCounter;
		VU32	ExcessiveCollisionCounter;
		VU32	LateCollisionCounter;

		VU32	DeferTimer;											// 0x6110
		VU32	PeakAttempts;
		VU32	ReceiveFrameCounter;
		VU32	LengthErrorCounter;

		VU32	AlignmentErrorCounter;								// 0x6120
		VU32	FCSErrorCounter;
		VU32	RxCodeViolationErrorCounter;
		VU32	filler16;

			/* Miscellaneous registers:	*/

		VU32	RandomNumberSeed;									// 0x6130
		VU32	StateMachine;

		UInt8	filler17[ 0x6200 - 0x6138 ];

			/* MIF registers: */

		VU32	MIFBitBangClock;									// 0x6200
		VU32	MIFBitBangData;
		VU32	MIFBitBangOutputEnable;
		VU32	MIFBitBangFrame_Output;

		VU32	MIFConfiguration;									// 0x6210
		VU32	MIFMask;
		VU32	MIFStatus;
		VU32	MIFStateMachine;

		UInt8	filler18[ 0x9000 - 0x6220 ];

			/* PCS/Serialink registers:	*/

		VU32	PCSMIIControl;										// 0x9000
		VU32	PCSMIIStatus;
		VU32	Advertisement;
		VU32	PCSMIILinkPartnerAbility;

		VU32	PCSConfiguration;									// 0x9010
		VU32	PCSStateMachine;
		VU32	PCSInterruptStatus;

		UInt8	filler19[ 0x9050 - 0x901C ];

		VU32	DatapathMode;										// 0x9050
		VU32	SerialinkControl;
		VU32	SharedOutputSelect;
		VU32	SerialinkState;
	};	/* end GMAC_Registers	*/


#define kConfiguration_Infinite_Burst	0x00000001	// for Tx only
#define kConfiguration_RonPaulBit		0x00000800	// for pci reads at end of infinite burst, next command is memory read multiple
#define kConfiguration_EnableBug2Fix	0x00001000	// fix Rx hang after overflow
#define kConfiguration_TX_DMA_Limit		(0x1F << 1)
#define kConfiguration_RX_DMA_Limit		(0x1F << 6)

	/* The following bits are used in the								*/
	/* Status, InterruptMask, InterruptAck, and StatusAlias registers:	*/

#define kStatus_TX_INT_ME				0x00000001
#define kStatus_TX_ALL					0x00000002
#define kStatus_TX_DONE					0x00000004
#define kStatus_RX_DONE					0x00000010
#define kStatus_Rx_Buffer_Not_Available	0x00000020
#define kStatus_RX_TAG_ERROR			0x00000040
#define kStatus_PCS_INT					0x00002000
#define kStatus_TX_MAC_INT				0x00004000
#define kStatus_RX_MAC_INT				0x00008000
#define kStatus_MAC_CTRL_INT			0x00010000
#define kStatus_MIF_Interrupt			0x00020000
#define kStatus_PCI_ERROR_INT			0x00040000
#define kStatus_TxCompletion_Shift		19

#define kInterruptMask_None				0xFFFFFFFF

#define kBIFConfiguration_SLOWCLK	0x1
#define kBIFConfiguration_B64D_DIS	0x2
#define kBIFConfiguration_M66EN		0x8

#define kSoftwareReset_TX		0x1
#define kSoftwareReset_RX		0x2
#define kSoftwareReset_RSTOUT	0x4

		// register TxConfiguration 2004:
#define kTxConfiguration_Tx_DMA_Enable				0x00000001
#define kTxConfiguration_Tx_Desc_Ring_Size_Shift	1			// bits 1:4
#define kTxConfiguration_TxFIFO_Threshold			0x001FFC00	// obsolete
#define kTxConfiguration_Paced_Mode					0x00200000	///

		// register WOLPatternMatchCount 300C:
#define kWOLPatternMatchCount_N		0x0010				// match count
#define kWOLPatternMatchCount_M		(0 << 8)			// frame count

		// register WOLWakeupCSR 3010:
#define kWOLWakeupCSR_Magic_Wakeup_Enable			0x0001
#define kWOLWakeupCSR_Mode_MII						0x0002
#define kWOLWakeupCSR_Magic_Event_Seen				0x0004

#define kWOLWakeupCSR_Rx_Filter_Unicast_Enable		0x0008
#define kWOLWakeupCSR_Rx_Filter_Multicast_Enable	0x0010
#define kWOLWakeupCSR_Rx_Filter_Broadcast_Enable	0x0020
#define kWOLWakeupCSR_Rx_Filter_Event_Seen			0x0040

		// register RxConfiguration 4000:
#define kRxConfiguration_Rx_DMA_Enable				0x00000001
#define kRxConfiguration_Rx_Desc_Ring_Size_Shift	1			// bits 1:4
#define kRxConfiguration_Batch_Disable				0x00000020
#define kRxConfiguration_First_Byte_Offset_Mask		(7 << 10)
#define kRxConfiguration_Checksum_Start_Offset		(kRxHwCksumStartOffset << 13)	// start checksumming
#define kRxConfiguration_RX_DMA_Threshold			0x01000000	// 128 bytes

		// 0x4020 - PauseThresholds register:
#define kPauseThresholds_Factor					64
#define kPauseThresholds_OFF_Threshold_Shift	0	// 9 bit fields
#define kPauseThresholds_ON_Threshold_Shift		12

	/* 4108 - RxBlanking values:											*/
	/* The RX_INTR_PACKETS is set to 10.									*/
	/* The RX_INTR_TIME value is based on the PCI bus speed.				*/
	/* RX_INTR_TIME = FACTOR33 * 2048 / Bus Speed 	 						*/
	/* If FACTOR33 is 2, then 2 * 2048 / 33333333 yields 122.8 microseconds	*/
#define FACTOR33	2			/* for a 33 MHz PCI bus.	*/
#define FACTOR66	4			/* for a 66 MHz PCI bus.	*/

#define kRxBlanking_default_33	(FACTOR33<<12 | 10)	// 10 pkts since last RX_DONE int
#define kRxBlanking_default_66	(FACTOR66<<12 | 10)

#define kTxMACSoftwareResetCommand_Reset	1	// 1 bit register
#define kRxMACSoftwareResetCommand_Reset	1

#define kSendPauseCommand_default	0x1BF0	// SlotTime units 7152 3.661 ms. Over 256 packets worth.

														// 0x6010:
#define kTX_MAC_Status_Frame_Transmitted		0x001
#define kTX_MAC_Status_Tx_Underrun				0x002
#define kTX_MAC_Status_Max_Pkt_Err				0x004
#define kTX_MAC_Status_Normal_Coll_Cnt_Exp		0x008
#define kTX_MAC_Status_Excess_Coll_Cnt_Exp		0x010
#define kTX_MAC_Status_Late_Coll_Cnt_Exp		0x020
#define kTX_MAC_Status_First_Coll_Cnt_Exp		0x040
#define kTX_MAC_Status_Defer_Timer_Exp			0x080
#define kTX_MAC_Status_Peak_Attempts_Cnt_Exp	0x100
														// 0x6014:
#define kRX_MAC_Status_Frame_Received			0x01
#define kRX_MAC_Status_Rx_Overflow				0x02	// Rx FIFO overflow
#define kRX_MAC_Status_Frame_Cnt_Exp			0x04
#define kRX_MAC_Status_Align_Err_Cnt_Exp		0x08
#define kRX_MAC_Status_CRC_Err_Cnt_Exp			0x10
#define kRX_MAC_Status_Length_Err_Cnt_Exp		0x20
#define kRX_MAC_Status_Viol_Err_Cnt_Exp			0x40


#define kTxMACMask_default			0x1FF		// enable none
#define kRxMACMask_default			0x3F		// enable none
#define kMACControlMask_default		7			// enable no Pause interrupts

														// 6030:
#define kTxMACConfiguration_TxMac_Enable			0x001
#define kTxMACConfiguration_Ignore_Carrier_Sense	0x002
#define kTxMACConfiguration_Ignore_Collisions		0x004
#define kTxMACConfiguration_Enable_IPG0				0x008
#define kTxMACConfiguration_Never_Give_Up			0x010
#define kTxMACConfiguration_Never_Give_Up_Limit		0x020
#define kTxMACConfiguration_No_Backoff				0x040
#define kTxMACConfiguration_Slow_Down				0x080
#define kTxMACConfiguration_No_FCS					0x100
#define kTxMACConfiguration_TX_Carrier_Extension	0x200

														// 6034:
#define kRxMACConfiguration_Rx_Mac_Enable			0x001
#define kRxMACConfiguration_Strip_Pad				0x002
#define kRxMACConfiguration_Strip_FCS				0x004
#define kRxMACConfiguration_Promiscuous				0x008
#define kRxMACConfiguration_Promiscuous_Group		0x010
#define kRxMACConfiguration_Hash_Filter_Enable		0x020
#define kRxMACConfiguration_Address_Filter_Enable	0x040
#define kRxMACConfiguration_Disable_Discard_On_Err	0x080
#define kRxMACConfiguration_Rx_Carrier_Extension	0x100
															// 6038:
#define kMACControlConfiguration_Send_Pause_Enable		0x1
#define kMACControlConfiguration_Receive_Pause_Enable	0x2
#define kMACControlConfiguration_Pass_MAC_Control		0x4
															// 603C:
#define kXIFConfiguration_Tx_MII_OE			0x01	// output enable on the MII bus
#define kXIFConfiguration_MII_Int_Loopback	0x02
#define kXIFConfiguration_Disable_Echo		0x04
#define kXIFConfiguration_GMIIMODE			0x08
#define kXIFConfiguration_MII_Buffer_OE		0x10
#define kXIFConfiguration_LINKLED			0x20
#define kXIFConfiguration_FDPLXLED			0x40

#define kInterPacketGap0_default	0
#define kInterPacketGap1_default	8
#define kInterPacketGap2_default	4

#define kSlotTime_default		0x0040
#define kMinFrameSize_default	0x0040
#define kMaxFrameSize_default	1522	// 1500 data + 12 address + 2 pkt type + 4 FCS + 4 VLAN

#define kPASize_default			0x07
#define kJamSize_default		0x04
#define kAttemptLimit_default	0x10
#define kMACControlType_default	0x8808

#define kMACAddress_default_6	0x0001
#define kMACAddress_default_7	0xC200
#define kMACAddress_default_8	0x0180

#define kMIFBitBangFrame_Output_ST_default	0x40000000	// 2 bits: ST of frame
#define kMIFBitBangFrame_Output_OP_read		0x20000000	// OP code - 2 bits:
#define kMIFBitBangFrame_Output_OP_write	0x10000000	// Read=10; Write=01
#define kMIFBitBangFrame_Output_PHYAD_shift	23			// 5 bit PHY ADdress
#define kMIFBitBangFrame_Output_REGAD_shift	18			// 5 bit REGister ADdress
#define kMIFBitBangFrame_Output_TA_MSB		0x00020000	// Turn Around MSB
#define kMIFBitBangFrame_Output_TA_LSB		0x00010000	// Turn Around LSB

#define kMIFConfiguration_PHY_Select	0x0001			// register 0x6210
#define kMIFConfiguration_Poll_Enable	0x0002
#define kMIFConfiguration_BB_Mode		0x0004
#define kMIFConfiguration_MDI_0			0x0010
#define kMIFConfiguration_MDI_1			0x0020
#define kMIFConfiguration_Poll_Reg_Adr_Shift	3
#define kMIFConfiguration_Poll_Phy_Adr_Shift	10

#define kPCSMIIControl_1000_Mbs_Speed_Select	0x0040
#define kPCSMIIControl_Collision_Test			0x0080
#define kPCSMIIControl_Duplex_Mode				0x0100
#define kPCSMIIControl_Restart_Auto_Negotiation	0x0200
#define kPCSMIIControl_Isolate					0x0400
#define kPCSMIIControl_Power_Down				0x0800
#define kPCSMIIControl_Auto_Negotiation_Enable	0x1000
#define kPCSMIIControl_Wrapback					0x4000
#define kPCSMIIControl_Reset					0x8000

#define kAdvertisement_Full_Duplex	0x0020
#define kAdvertisement_Half_Duplex	0x0040
#define kAdvertisement_PAUSE		0x0080	// symmetrical to link partner
#define kAdvertisement_ASM_DIR		0x0100	// pause asymmetrical to link partner
#define kAdvertisement_Ack			0x4000

#define kPCSConfiguration_Enable					0x01
#define kPCSConfiguration_Signal_Detect_Override	0x02
#define kPCSConfiguration_Signal_Detect_Active_Low	0x04
#define kPCSConfiguration_Jitter_Study				// 2 bit field			
#define kPCSConfiguration_10ms_Timer_Override		0x20

#define kDatapathMode_XMode				0x01
#define kDatapathMode_ExtSERDESMode		0x02
#define kDatapathMode_GMIIMode			0x04	/// MII/GMII
#define kDatapathMode_GMIIOutputEnable	0x08

#define kSerialinkControl_DisableLoopback	0x01
#define kSerialinkControl_EnableSyncDet		0x02
#define kSerialinkControl_LockRefClk		0x04


		/* Rx descriptor:	*/

	struct RxDescriptor
	{
		VU16		tcpPseudoChecksum;
		VU16		frameDataSize;			// Has ownership bit
		VU32		flags;
		VU64		bufferAddr;
	};

	/* Note: Own is in the high bit of frameDataSize field	*/

#define kGEMRxDescFrameSize_Mask	0x7FFF
#define kGEMRxDescFrameSize_Own		0x8000

	/* Rx flags field	*/

#define kGEMRxDescFlags_HashValueBit            0x00001000
#define kGEMRxDescFlags_HashValueMask           0x0FFFF000
#define kGEMRxDescFlags_HashPass                0x10000000
#define kGEMRxDescFlags_AlternateAddr           0x20000000
#define kGEMRxDescFlags_BadCRC                  0x40000000


	struct TxDescriptor
	{
		VU32		flags0;			// start/end of frame...buffer size
		VU32		flags1;			// Int me
		VU64		bufferAddr;
	};


#define kGEMTxDescFlags0_ChecksumStart_Shift	15
#define kGEMTxDescFlags0_ChecksumStuff_Shift	21
#define kGEMTxDescFlags0_ChecksumEnable		0x20000000
#define kGEMTxDescFlags0_EndOfFrame			0x40000000
#define kGEMTxDescFlags0_StartOfFrame		0x80000000

#define kGEMTxDescFlags1_Int				0x00000001
#define kGEMTxDescFlags1_NoCRC				0x00000002
