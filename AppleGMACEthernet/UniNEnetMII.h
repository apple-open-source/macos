/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-1999 by Apple Computer, Inc., All rights reserved.
 *
 * MII protocol and PHY register definitions.
 *
 * HISTORY
 *
 */

	/* MII command frame (32-bits) as documented in IEEE 802.3u	*/

#define MII_OP_READ	0x02
#define MII_OP_WRITE	0x01                            

#define MII_MAX_PHY		32

	/* MII Registers:	*/

#define MII_CONTROL			0
#define MII_STATUS			1
#define MII_ID0				2
#define MII_ID1				3
#define MII_ADVERTISEMENT	4
#define MII_LINKPARTNER		5
#define MII_EXPANSION		6
#define MII_NEXTPAGE		7

#define MII_1000BASETCONTROL	0x09


	/* MII Control register bits:	*/

#define MII_CONTROL_RESET					0x8000
#define MII_CONTROL_LOOPBACK				0x4000
#define MII_CONTROL_SPEED_SELECTION			0x2000
#define MII_CONTROL_AUTONEGOTIATION			0x1000
#define MII_CONTROL_POWERDOWN				0x800
#define MII_CONTROL_ISOLATE					0x400
#define MII_CONTROL_RESTART_NEGOTIATION		0x200
#define MII_CONTROL_FULLDUPLEX				0x100
#define MII_CONTROL_COLLISION_TEST			0x80
#define MII_CONTROL_SPEED_SELECTION_2		0x40

	/* 1 - MII Status register bits:	*/

#define MII_STATUS_100BASET4				0x8000
#define MII_STATUS_100BASETX_FD				0x4000
#define MII_STATUS_100BASETX				0x2000
#define MII_STATUS_10BASET_FD				0x1000
#define MII_STATUS_10BASET					0x800
#define MII_STATUS_NEGOTIATION_COMPLETE		0x20
#define MII_STATUS_REMOTE_FAULT				0x10
#define MII_STATUS_NEGOTIATION_ABILITY		0x8
#define MII_STATUS_LINK_STATUS				0x4
#define MII_STATUS_JABBER_DETECT			0x2
#define MII_STATUS_EXTENDED_CAPABILITY		0x1

	/* 4 - MII ANAR (Auto-Negotiation Advertisement Register) register bits:	*/

#define MII_ANAR_ASYM_PAUSE			0x800 
#define MII_ANAR_PAUSE				0x400
#define MII_ANAR_100BASET4			0x200
#define MII_ANAR_100BASETX_FD		0x100
#define MII_ANAR_100BASETX			0x80
#define MII_ANAR_10BASET_FD			0x40
#define MII_ANAR_10BASET			0x20

	/* 5 - MII ANLPAR register bits:	*/

#define MII_LPAR_NEXT_PAGE		0x8000
#define MII_LPAR_ACKNOWLEDGE	0x4000
#define MII_LPAR_REMOTE_FAULT	0x2000
#define MII_LPAR_ASYM_PAUSE		0x0800
#define MII_LPAR_PAUSE			0x0400
#define MII_LPAR_100BASET4		0x200
#define MII_LPAR_100BASETX_FD	0x100
#define MII_LPAR_100BASETX		0x80
#define MII_LPAR_10BASET_FD		0x40
#define MII_LPAR_10BASET		0x20

	/* 9 - MII 1000-BASET Control register bits:	*/

#define MII_1000BASETCONTROL_FULLDUPLEXCAP	0x0200
#define MII_1000BASETCONTROL_HALFDUPLEXCAP	0x0100


	/*** MII BCM5201 Specific:	***/

	/* MII BCM5201 ID:	*/

#define MII_BCM5201_OUI		0x001018
#define MII_BCM5201_MODEL	0x21
#define MII_BCM5201_REV		0x01
#define MII_BCM5201_ID		((MII_BCM5201_OUI << 10) | (MII_BCM5201_MODEL << 4))
#define MII_BCM5201_MASK	0xFFFFFFF0


	/* MII BCM5201 Regs :	*/

#define MII_BCM5201_AUXSTATUS	0x18

	/* MII BCM5201 AUXSTATUS register bits:	*/

#define MII_BCM5201_AUXSTATUS_DUPLEX	0x0001
#define MII_BCM5201_AUXSTATUS_SPEED		0x0002

	/* MII BCM5201 MULTIPHY interrupt register.		*/
	/* Added 4/20/2000 by A.W. for power management	*/

#define MII_BCM5201_INTERRUPT 				0x1A
#define MII_BCM5201_INTERRUPT_INTREnable	0x4000
#define MII_BCM5201_INTERRUPT_FDXChange		0x0008
#define MII_BCM5201_INTERRUPT_SPDChange		0x0004
#define MII_BCM5201_INTERRUPT_LINKChange	0x0002

#define MII_BCM5201_AUXMODE2 				0x1B
#define MII_BCM5201_AUXMODE2_LOWPOWER		0x0008

#define MII_BCM5201_MULTIPHY				0x1E

	/* MII BCM5201 MULTIPHY register bits:	*/

#define MII_BCM5201_MULTIPHY_SERIALMODE		0x0002
#define MII_BCM5201_MULTIPHY_SUPERISOLATE	0x0008

	/*** MII BCM5221 Specific:	***/

	/* MII BCM5221 ID:			*/

#define MII_BCM5221_OUI		0x001018
#define MII_BCM5221_MODEL	0x1E
#define MII_BCM5221_REV		0x00
#define MII_BCM5221_ID		((MII_BCM5221_OUI << 10) | (MII_BCM5221_MODEL << 4))
#define MII_BCM5221_MASK	0xFFFFFFF0

	/* 5221 Shadow registers:	*/

#define MII_BCM5221_AuxiliaryMode4		0x1A
#define MII_BCM5221_SetIDDQMode				0x0001
#define MII_BCM5221_EnableClkDuringLowPwr	0x0004

#define MII_BCM5221_AuxiliaryStatus2	0x1B
#define MII_BCM5221_APD_EnableBit 			0x0020	// Auto Power Detect
#define MII_BCM5221_5SecSleepTimer			0x0010
#define MII_BCM5221_APDWakeUPTimer			0x000F

#define MII_BCM5221_AuxiliaryStatus3	0x1C
#define MII_BC_5221EnableAutoMDIX			0x0800

#define MII_BCM5221_TestRegister		0x1F
#define MII_BCM5221_ShadowRegEnableBit 		0x0080


	/*** MII LXT971 (Level One) Specific:	***/

	/* MII LXT971 ID:	*/

#define MII_LXT971_OUI		0x0004DE
#define MII_LXT971_MODEL	0x0E
#define MII_LXT971_REV		0x01
#define MII_LXT971_ID		((MII_LXT971_OUI << 10) | (MII_LXT971_MODEL << 4))
#define MII_LXT971_MASK		0xFFFFFFF0

	/* MII LXT971 Regs:	*/

#define MII_LXT971_STATUS_2		0x11

	/* MII LXT971 Status #2 register bits:	*/

#define MII_LXT971_STATUS_2_DUPLEX	0x0200
#define MII_LXT971_STATUS_2_SPEED	0x4000


	/*** MII BCM5400 Specific:	***/

	/* MII BCM5400 ID:			*/

#define MII_BCM5400_OUI		0x000818
#define MII_BCM5400_MODEL	0x04
#define MII_BCM5401_MODEL	0x05
#define MII_BCM5411_MODEL	0x07		// 0x6071
#define MII_BCM5421_MODEL	0x0E		// 0x60E0
#define MII_BCM54K2_MODEL	0x2E		// 0x62E0 for K2 ASIC
#define MII_BCM5462_MODEL	0x0D		// 0x60D0 for vesta
#define MII_BCM5400_REV		0x01
#define MII_BCM5400_ID		((MII_BCM5400_OUI << 10) | (MII_BCM5400_MODEL << 4))
#define MII_BCM5401_ID		((MII_BCM5400_OUI << 10) | (MII_BCM5401_MODEL << 4))
#define MII_BCM5411_ID		((MII_BCM5400_OUI << 10) | (MII_BCM5411_MODEL << 4))
#define MII_BCM5421_ID		((MII_BCM5400_OUI << 10) | (MII_BCM5421_MODEL << 4))
#define MII_BCM54K2_ID		((MII_BCM5400_OUI << 10) | (MII_BCM54K2_MODEL << 4))
#define MII_BCM5462_ID		((MII_BCM5400_OUI << 10) | (MII_BCM5462_MODEL << 4))
#define MII_BCM5400_MASK	0xFFFFFFF0


	/* MII BCM5400 Regs:	*/

#define MII_BCM5400_AUXCONTROL	0x18	// CAUTION - this reg is different on 5401

	/* MII BCM5400 AUXCONTROL register bits:	*/

#define MII_BCM5400_AUXCONTROL_PWR10BASET	0x0004


#define MII_BCM5400_AUXSTATUS	0x19

	/* MII BCM5400 AUXSTATUS register bits:	*/

#define MII_BCM5400_AUXSTATUS_LINKMODE_MASK	0x0700
#define MII_BCM5400_AUXSTATUS_LINKMODE_BIT	0x0100  


	/**** Marvell Registers:	****/

	/*** Marvell 88E1011 Specific:	***/

	/* Marvell ID:	*/

#define MII_MARVELL_ID		0x01410C20
#define MII_MARVELL_ID_1	0x01410C60
#define MII_MARVELL_ID_2	0x01410CC0		// 88E1111 - required patch - never shipped
#define MII_MARVELL_ID_2_1	0x01410CC1		// 88E1111 - rev 1 - no patch required.
#define MII_MARVELL_MASK	0xFFFFFFF0

	/* MII Marvell PHY Specific Control bits:	*/

#define MII_MARVELL_PHY_SPECIFIC_CONTROL	0x10
#define MII_MARVELL_PHY_SPECIFIC_CONTROL_AUTOL_MDIX		0x40	// Sense crossover cable
#define MII_MARVELL_PHY_SPECIFIC_CONTROL_MANUAL_MDIX	0x20	// Crossover cable



	/* MII Marvell PHY Specific Status bits:	*/

#define MII_MARVELL_PHY_SPECIFIC_STATUS		0x11

#define MII_MARVELL_PHY_SPECIFIC_STATUS_1000		0x8000
#define MII_MARVELL_PHY_SPECIFIC_STATUS_100			0x4000
#define MII_MARVELL_PHY_SPECIFIC_STATUS_10			0x0000
#define MII_MARVELL_PHY_SPECIFIC_STATUS_RESOLVED	0x0800

#define MII_MARVELL_PHY_SPECIFIC_STATUS_SPEED_MASK	(	MII_MARVELL_PHY_SPECIFIC_STATUS_1000 \
													  | MII_MARVELL_PHY_SPECIFIC_STATUS_100  \
													  | MII_MARVELL_PHY_SPECIFIC_STATUS_10)

#define MII_MARVELL_PHY_SPECIFIC_STATUS_FULL_DUPLEX		0x2000


#define MII_MARVELL_INT_ENABLE			0x12
#define MII_MARVELL_INT_ENABLE_SPEED	0x4000
#define MII_MARVELL_INT_ENABLE_DUPLEX	0x2000
#define MII_MARVELL_INT_ENABLE_LINK		0x0400

#define MII_MARVELL_INT_STATUS			0x13
#define MII_MARVELL_INT_STATUS_SPEED	0x4000
#define MII_MARVELL_INT_STATUS_DUPLEX	0x2000
#define MII_MARVELL_INT_STATUS_LINK		0x0400


	/* MII timeout:	*/

#define MII_DEFAULT_DELAY	20
#define MII_RESET_TIMEOUT	100
#define MII_RESET_DELAY		10

//#define MII_LINK_TIMEOUT	2500
///#define MII_LINK_TIMEOUT	5000	/// latest Marvell needs more than 2.5 secs
#define MII_LINK_TIMEOUT	10000	/// Broadcom 5421 needs more than 6.4 secs
#define MII_LINK_DELAY		50
