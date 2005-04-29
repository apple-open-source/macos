/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
	File:		Z85C30.h

	Contains:	This file contains symbolics specific to the CMOS Z85C30 SCC.
			It is based on the work of Craig Prouse.

	Copyright:	© 1996 by Apple Computer, Inc., all rights reserved.

	Version:	1.0

	File Ownership:

		DRI:			Naga Pappireddi

		Other Contact:		Shashin Vora

		Technology:		Maxwell CPU SW

	Writers:

		(np)	Naga Pappireddi
		(SMD)	Simon Douglas
		(ÖMB)	Öner M. Biçakçi

	Change History (most recent first):

		 <5>	 8/14/96	np		options management implementation
		 <4>	  7/2/96	SMD		(OMB) Enabled AMIC DMA for PDM.
		 <4>	 6/28/96	ÖMB		Added constant definitions.
		 <3>	 6/19/96	ÖMB		Added constant.
		 <2>	 3/15/96	ÖMB		Added AMIC & DBDMA. Updated to d11e2.
		 <1>	 1/29/96	ÖMB		First checked in.
*/

#ifndef __SERIALZ85C30__
#define __SERIALZ85C30__


/***********************************************************************/
//
//	Generic enums
//
/***********************************************************************/

enum
{
    // constants
    kNumSCCWR		= 15,		// there are 15 write registers

    // RR0
    kRxCharAvailable 	= 0x01,
    kZeroCount		= 0x02,
    kTxBufferEmpty	= 0x04,
    kDCDAsserted 	= 0x08,
    kSyncHunt		= 0x10,
    kCTSAsserted 	= 0x20,
    kTXUnderRun		= 0x40,
    kBreakReceived	= 0x80,

    // RR1
    kAllSent		= 0x01,
    kParityErr		= 0x10,
    kRxOverrun      = 0x20,
    kCRCFramingErr	= 0x40,
    kRxErrorsMask	= (kParityErr | kRxOverrun | kCRCFramingErr),

    // RR3
    kChannelARxIP	= 0x20,
    kChannelATxIP	= 0x10,
    kChannelAESIP	= 0x08,
    kChannelBRxIP	= 0x04,
    kChannelBTxIP	= 0x02,
    kChannelBESIP	= 0x01,
    kChannelRxIP	= 0x04,
    kChannelTxIP	= 0x02,
    kChannelESIP	= 0x01,
    
    // WR0
    kResetExtStsInt	= 0x10,
    kResetRxInt		= 0x20,
    kResetTxIP		= 0x28,
    kErrorReset		= 0x30,
    kTxUnderrunReset = 0xC0,
    kHighIUSReset	= 0x38,

    // WR1
    kWReqDisable	= 0x00,
    kWReqEnable		= 0x80,
    kDMAReqSelect	= 0x40,
    kDMAReqOnTx		= 0x00,
    kDMAReqOnRx		= 0x20,
    kRxInt1stOrSC	= 0x08,
    kRxIntAllOrSC	= 0x10,
    kRxIntOnlySC	= 0x18,
    kRxIntMask		= 0x18,
    kParityIsSpCond	= 0x04,
    kTxIntEnable	= 0x02,
    kExtIntEnable	= 0x01,

    // WR3						# Rx Bits, Rx Enable
    kRx5Bits		= 0x00,
    kRx6Bits		= 0x80,
    kRx7Bits		= 0x40,
    kRx8Bits		= 0xC0,
    kRxBitsMask		= 0xC0,
    kRxEnable		= 0x01,

    // WR4						# Clock Mode, Stop Bits, Parity Control
    kX1ClockMode	= 0x00,
    kX16ClockMode	= 0x40,
    kX32ClockMode	= 0x80,
    kX64ClockMode	= 0xC0,
    kClockModeMask	= 0xC0,
    kSyncModeMask	= 0x30,
    kExtSyncMode	= 0x30,
    kSDLCSyncMode	= 0x20,
    k16BitSyncMode	= 0x10,
    k8BitSyncMode	= 0x00,
    k1StopBit		= 0x04,
    k1pt5StopBits	= 0x08,
    k2StopBits		= 0x0C,
    kStopBitsMask	= 0x0C,
    kParityEven		= 0x02,
    kParityEnable	= 0x01,
    kParityMask		= 0x03,

    // WR5			# DTR, Tx Bits, Send Break, Tx Enable, RTS
    kDTR		= 0x80,
    kTx5Bits		= 0x00,
    kTx6Bits		= 0x40,
    kTx7Bits		= 0x20,
    kTx8Bits		= 0x60,
    kTxBitsMask		= 0x60,
    kSendBreak		= 0x10,
    kTxEnable		= 0x08,
    kRTS		= 0x02,

    // WR7'
    kEReq		= 0x10,

    // WR9
    kChannelResetA	= 0x80,
    kChannelResetB	= 0x40,
    kINTACK		= 0x20,
    kStatusHigh		= 0x10,
    kMIE		= 0x08,
    kDLC		= 0x04,
    kNV			= 0x02,
    kVIS		= 0x01,

    // WR10
    kNRZ		= 0x00,
    kNRZI		= 0x20,
    kFM1		= 0x40,
    kFM0		= 0x60,

    // WR11
    kRxClockRTxC	= 0x00,
    kRxClockTRxC	= 0x20,
    kRxClockBRG		= 0x40,
    kRxClockMask	= 0x60,
    kTxClockRTxC	= 0x00,
    kTxClockTRxC	= 0x08,
    kTxClockBRG		= 0x10,
    kTxClockMask	= 0x18,

    // WR14
    kLocalLoopback	= 0x10,
    kBRGFromRTxC	= 0x00,
    kBRGFromPCLK	= 0x02,
    kBRGDisable		= 0x00,
    kBRGEnable		= 0x01,
    
    // WR15
    kBreakAbortIE	= 0x80,
    kCTSIE		= 0x20,
    kDCDIE		= 0x08,
    kERegEnable		= 0x01
};


#endif