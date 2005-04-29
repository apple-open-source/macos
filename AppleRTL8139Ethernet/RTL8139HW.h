/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2001 Realtek Semiconductor Corp.  All rights reserved. 
 *
 * rtl8139HW.h
 *
 * HISTORY
 * Jul 09, 2001 Owen Wei at Realtek Semiconductor Corp. created for Realtek
 *      RTL8139 family NICs.
 *  
 */

#ifndef _RTL8139HW_H
#define _RTL8139HW_H

#define SIZE32_BIT0        0x00000001
#define SIZE32_BIT1        0x00000002
#define SIZE32_BIT2        0x00000004
#define SIZE32_BIT3        0x00000008
#define SIZE32_BIT4        0x00000010
#define SIZE32_BIT5        0x00000020
#define SIZE32_BIT6        0x00000040
#define SIZE32_BIT7        0x00000080
#define SIZE32_BIT8        0x00000100
#define SIZE32_BIT9        0x00000200
#define SIZE32_BIT10       0x00000400
#define SIZE32_BIT11       0x00000800
#define SIZE32_BIT12       0x00001000
#define SIZE32_BIT13       0x00002000
#define SIZE32_BIT14       0x00004000
#define SIZE32_BIT15       0x00008000
#define SIZE32_BIT16       0x00010000
#define SIZE32_BIT17       0x00020000
#define SIZE32_BIT18       0x00040000
#define SIZE32_BIT19       0x00080000
#define SIZE32_BIT20       0x00100000
#define SIZE32_BIT21       0x00200000
#define SIZE32_BIT22       0x00400000
#define SIZE32_BIT23       0x00800000
#define SIZE32_BIT24       0x01000000
#define SIZE32_BIT25       0x02000000
#define SIZE32_BIT26       0x04000000
#define SIZE32_BIT27       0x08000000
#define SIZE32_BIT28       0x10000000
#define SIZE32_BIT29       0x20000000
#define SIZE32_BIT30       0x40000000
#define SIZE32_BIT31       0x80000000

#define SIZE16_BIT0        0x0001
#define SIZE16_BIT1        0x0002
#define SIZE16_BIT2        0x0004
#define SIZE16_BIT3        0x0008
#define SIZE16_BIT4        0x0010
#define SIZE16_BIT5        0x0020
#define SIZE16_BIT6        0x0040
#define SIZE16_BIT7        0x0080
#define SIZE16_BIT8        0x0100
#define SIZE16_BIT9        0x0200
#define SIZE16_BIT10       0x0400
#define SIZE16_BIT11       0x0800
#define SIZE16_BIT12       0x1000
#define SIZE16_BIT13       0x2000
#define SIZE16_BIT14       0x4000
#define SIZE16_BIT15       0x8000

#define SIZE8_BIT0         0x01
#define SIZE8_BIT1         0x02
#define SIZE8_BIT2         0x04
#define SIZE8_BIT3         0x08
#define SIZE8_BIT4         0x10
#define SIZE8_BIT5         0x20
#define SIZE8_BIT6         0x40
#define SIZE8_BIT7         0x80

//---------------------------------------------------------------------------
// Memory or I/O mapped registers
//---------------------------------------------------------------------------

enum {
    RTL_IDR0            = 0x00,   // ID reg 0
    RTL_IDR4            = 0x04,   // ID reg 4, four byte access only
    RTL_MAR0            = 0x08,   // multicast reg 0, 4 byte access 
	RTL_MAR4            = 0x0c,   // multicast reg 4 
	RTL_TSD0            = 0x10,   // tx status of descriptor 0 
	RTL_TSD1            = 0x14,   // tx status of descriptor 1 
	RTL_TSD2            = 0x18,   // tx status of descriptor 2 
	RTL_TSD3            = 0x1c,   // tx status of descriptor 3 
	RTL_TSAD0           = 0x20,   // tx start address of descriptor 0 
	RTL_TSAD1           = 0x24,   // tx start address of descriptor 1 
	RTL_TSAD2           = 0x28,   // tx start address of descriptor 2 
	RTL_TSAD3           = 0x2c,   // tx start address of descriptor 3 
	RTL_RBSTART         = 0x30,   // rx buffer start address 
	RTL_ERBCR           = 0x34,   // early rx byte count register 
	RTL_ERSR            = 0x36,   // early rx status register 
	RTL_CM              = 0x37,   // command register 
	RTL_CAPR            = 0x38,   // current address of packet read
	RTL_CBA             = 0x3a,   // current buffer address
	RTL_IMR             = 0x3c,   // interrupt mask register 
	RTL_ISR             = 0x3e,   // interrupt status register 
	RTL_TCR             = 0x40,   // tx configuration register
	RTL_8139TYPE        = 0x43,
	RTL_RCR             = 0x44,   // rx configuration register 
	RTL_TCTR            = 0x48,   // timer count register 
	RTL_MPC             = 0x4c,   // missed packet counter 
	RTL_9346CR          = 0x50,
	RTL_CR9346          = 0x50,
	RTL_CONFIG0         = 0x51,
	RTL_CONFIG1         = 0x52,
	RTL_FLASH           = 0x54,   // flash memory read/write register 
	RTL_MEDIA_STATUS    = 0x58,   // general purpose register 
	RTL_GEPCTL          = 0x59,   // general purpose control register 
	RTL_MIIR            = 0x5a,   // MII register 
	RTL_HLTCLK          = 0x5b,   // halt clock register 
	RTL_MULINT          = 0x5c,   // multiple interrupt 
	RTL_8139ID          = 0x5e,   // 8139 ID  = 0x10
	RTL_TSAD            = 0x60,   // tx status of all descriptors 
	RTL_BMC             = 0x62,
	RTL_BMS             = 0x64,
	RTL_70              = 0x70,
	RTL_74              = 0x74,
	RTL_78              = 0x78,
	RTL_7C              = 0x7c
};

#define PARM7C_P1_0     0x0cb39de43
#define PARM7C_P2_0     0x0cb39ce43
#define PARM7C_P3_0     0x0cb39ce83
#define PARM7C_P4_0     0x0cb39ce83

#define PARM7C_P1_1     0x0cb39de43
#define PARM7C_P2_1     0x0cb39ce43
#define PARM7C_P3_1     0x0cb39ce83
#define PARM7C_P4_1     0x0cb39ce83

#define PARM7C_P1_3     0x0cb39de43
#define PARM7C_P2_3     0x0cb39ce43
#define PARM7C_P3_3     0x0cb39ce83
#define PARM7C_P4_3     0x0cb39ce83

#define PARM7C_P1_7     0x0bb39de43
#define PARM7C_P2_7     0x0bb39ce43
#define PARM7C_P3_7     0x0bb39ce83
#define PARM7C_P4_7     0x0bb39ce83

#define DELAY_7C        10000 // 0.01 sec

//---------------------------------------------------------------------------
// TCR bits --- TX CONFIGURATION
//---------------------------------------------------------------------------

enum {
    Burst16         = 0x0000,
    Burst32         = 0x0100,
    Burst64         = 0x0200,
    Burst128        = 0x0300,
    Burst256        = 0x0400,
    Burst512        = 0x0500,
    Burst1024       = 0x0600,
    BurstUnlimited  = 0x0700
};

enum {
    R_TCR_CLRABT    = SIZE32_BIT0,    // clear abort
	R_TCR_SNPAC     = SIZE32_BIT1,    // send next packet
	R_TCR_MXDMA     = Burst1024,      // max DMA burst size
	R_TCR_CRC       = SIZE32_BIT16,   // inhibit CRC
	R_TCR_LBKI      = SIZE32_BIT17,   // internal loopback
	R_TCR_LBKE      = SIZE32_BIT18,   // external loopback
	R_TCR_IFG       = ((UInt32) 0),   // interframe gap, violate IEEE 802.3
};

//---------------------------------------------------------------------------
// TSD bits --- TX status of descriptors 0~3
//---------------------------------------------------------------------------

enum {
    // 2000/06/09 Owen
    R_TSD_ERTXTH    = SIZE32_BIT20,   // 16*32 early tx threshold
    R_TSD_CRS       = SIZE32_BIT31,   // carrier sense loss 
	R_TSD_TABT      = SIZE32_BIT30,   // tx abort 
	R_TSD_OWC       = SIZE32_BIT29,   // out of windows collision 
	R_TSD_CDH       = SIZE32_BIT28,   // CD heart beat 
	R_TSD_NCC       = 0x0f000000,     // No. collision count mask 
	R_TSD_TOK       = SIZE32_BIT15,   // tx ok 
	R_TSD_TUN       = SIZE32_BIT14,   // tx fifo underrun 
	R_TSD_OWN       = SIZE32_BIT13    // set when tx buffer move to the fifo 
};

//---------------------------------------------------------------------------
// ISR bits --- interrupt status register
//---------------------------------------------------------------------------

enum {
	R_ISR_ROK       = SIZE16_BIT0,    // rx ok 
	R_ISR_RER       = SIZE16_BIT1,    // rx error 
	R_ISR_TOK       = SIZE16_BIT2,    // tx ok 
	R_ISR_TER       = SIZE16_BIT3,    // tx error 
	R_ISR_RXOVW     = SIZE16_BIT4,    // rx buffer overflow 
	R_ISR_PUN       = SIZE16_BIT5,    // rx packet underrun
	R_ISR_FOVW      = SIZE16_BIT6,    // rx FIFO overflow
	R_ISR_TMOUT     = SIZE16_BIT14,   // PCS time out 
	R_ISR_SERR      = SIZE16_BIT15,   // system error 
	R_ISR_ALL       = 0x007f,
	R_ISR_NONE      = 0x0000,
};

//---------------------------------------------------------------------------
// TSAD bits --- tx status of all descriptor
//---------------------------------------------------------------------------

enum {
	R_TSAD_OWN0     = SIZE16_BIT0,    // own bit of descriptor 0 
	R_TSAD_OWN1     = SIZE16_BIT1,    // own bit of descriptor 1
	R_TSAD_OWN2     = SIZE16_BIT2,    // own bit of descriptor 2 
	R_TSAD_OWN3     = SIZE16_BIT3,    // own bit of descriptor 3 
	R_TSAD_TABT0    = SIZE16_BIT4,    // tx ABT 0 
	R_TSAD_TABT1    = SIZE16_BIT5,    // tx ABT 1 
	R_TSAD_TABT2    = SIZE16_BIT6,    // tx ABT 2 
	R_TSAD_TABT3    = SIZE16_BIT7,    // tx ABT 3 
	R_TSAD_TUN0     = SIZE16_BIT8,    // tx underrun 0 
	R_TSAD_TUN1     = SIZE16_BIT9,    // tx underrun 1 
	R_TSAD_TUN2     = SIZE16_BIT10,   // tx underrun 2 
	R_TSAD_TUN3     = SIZE16_BIT11,   // tx underrun 3 
	R_TSAD_TOK0     = SIZE16_BIT12,   // tx ok 0 
	R_TSAD_TOK1     = SIZE16_BIT13,   // tx ok 1 
	R_TSAD_TOK2     = SIZE16_BIT14,   // tx ok 2
	R_TSAD_TOK3     = SIZE16_BIT15    // tx ok 3 
};

//---------------------------------------------------------------------------
// RCR bits --- rx configuration
//---------------------------------------------------------------------------

enum {
	R_RCR_AAP       = SIZE32_BIT0,    // accept all physical 
	R_RCR_APM       = SIZE32_BIT1,    // accept physical match 
	R_RCR_AM        = SIZE32_BIT2,    // accept multicast 
	R_RCR_AB        = SIZE32_BIT3,    // accept broadcast
	R_RCR_AR        = SIZE32_BIT4,    // accept runt packet 
	R_RCR_AER       = SIZE32_BIT5,    // accept error packet
	R_RCR_WRAP      = SIZE32_BIT7,
	R_RCR_MXDMA     = (SIZE32_BIT10 | SIZE32_BIT9 | SIZE32_BIT8),
	R_RCR_RBLEN_16K = SIZE32_BIT11,   // rx buf 16K 
	R_RCR_RBLEN_32K = SIZE32_BIT12,   // rx buf 32K 
	R_RCR_RXFTH     = (SIZE32_BIT15 | SIZE32_BIT13),
    R_RCR_ERTH      = ((UInt32) 0)
};

//---------------------------------------------------------------------------
// RSR bits --- rx status
//---------------------------------------------------------------------------

enum {
	R_RSR_ROK       = SIZE16_BIT0,    // rx ok
	R_RSR_FAE       = SIZE16_BIT1,    // frame alignment error
	R_RSR_CRC       = SIZE16_BIT2,    // CRC error
	R_RSR_LONG      = SIZE16_BIT3,    // long packet > 8k bytes
	R_RSR_RUNT      = SIZE16_BIT4,    // runt packet receive < 64 byte
	R_RSR_ISE       = SIZE16_BIT5,    // invalid symbol error 100base-tx only
	R_RSR_BAR       = SIZE16_BIT13,   // broadcast address rx
	R_RSR_PAM       = SIZE16_BIT14,   // physical address match
	R_RSR_MAR       = SIZE16_BIT15    // multicast address rx
};

//---------------------------------------------------------------------------
// CM bits --- command register
//---------------------------------------------------------------------------

enum {
	R_CM_BUFE       = SIZE8_BIT0,     // buf empty 
	R_CM_EMPTY      = SIZE8_BIT0,     // buf empty 
	R_CM_TE         = SIZE8_BIT2,     // tx enable 
	R_CM_RE         = SIZE8_BIT3,     // rx enable 
	R_CM_RST        = SIZE8_BIT4      // reset 
};

//---------------------------------------------------------------------------
// ERSR bits --- early rx status register
//---------------------------------------------------------------------------

enum {
	R_ERSR_ERGOOD   = SIZE8_BIT3,     // early rx good packet 
	R_ERSR_ERBAD    = SIZE8_BIT2,     // early rx bad 
	R_ERSR_EROVW    = SIZE8_BIT1,     // early rx overwrite 
	R_ERSR_EROK     = SIZE8_BIT0      // early rx ok 
};

//---------------------------------------------------------------------------
// CONFIG0 bits
//---------------------------------------------------------------------------

enum {
    R_CONFIG0_SCR   = SIZE8_BIT7,     // scrambler mode 
	R_CONFIG0_PCS   = SIZE8_BIT6,     // PCS mode 
	R_CONFIG0_T10   = SIZE8_BIT5,     // 10 MHz mode 
	R_CONFIG0_PL0   = SIZE8_BIT3,     // select 10 Mbps medium types 
	R_CONFIG0_PL1   = SIZE8_BIT4
};

//---------------------------------------------------------------------------
// RTL_MEDIA_STATUS bits (0x58)
//---------------------------------------------------------------------------

enum {
	R_MEDIA_STATUS_SPEED_10 = SIZE8_BIT3,
	R_MEDIA_STATUS_LINKB    = SIZE8_BIT2
};

//---------------------------------------------------------------------------
// Basic Mode Control bits (0x62)
//---------------------------------------------------------------------------

enum {
    R_BMC_DUPLEXMODE = SIZE16_BIT8
};

//---------------------------------------------------------------------------
// CONFIG1 bits --- early rx status register
//---------------------------------------------------------------------------

enum {
    R_CONFIG1_PCIWAIT   = SIZE8_BIT7,
	R_CONFIG1_FUDUP     = SIZE8_BIT6,
	R_CONFIG1_DVRLOAD   = SIZE8_BIT5,
	R_CONFIG1_LEDSEL    = SIZE8_BIT4,
	R_CONFIG1_MEMMAP    = SIZE8_BIT3,
	R_CONFIG1_IOMAP     = SIZE8_BIT2,
	R_CONFIG1_SLEEP     = SIZE8_BIT1,
	R_CONFIG1_PWRDN     = SIZE8_BIT0
};

//---------------------------------------------------------------------------
// CR9346
//---------------------------------------------------------------------------

enum {
    R_CR9346_EEM1   = SIZE8_BIT7,
    R_CR9346_EEM0   = SIZE8_BIT6
};

//-------------------------------------------------------------------------
// Media types
//-------------------------------------------------------------------------

enum {
    PCI_CFID_REALTEK8139 = 0x813910ec
};

typedef UInt32 MediumIndex;

enum {
    MEDIUM_INDEX_10_HD = 0,
    MEDIUM_INDEX_10_FD = 1,
    MEDIUM_INDEX_TX_HD = 2,
    MEDIUM_INDEX_TX_FD = 3,
    MEDIUM_INDEX_T4    = 4,
    MEDIUM_INDEX_AUTO  = 5,
    MEDIUM_INDEX_NONE  = 6,
    MEDIUM_INDEX_COUNT = 6
};

//---------------------------------------------------------------------------
// Basic Mode Control Register
//---------------------------------------------------------------------------

enum {
    MII_CONTROL_RESET               = SIZE16_BIT15,
    MII_CONTROL_LOOPBACK            = SIZE16_BIT14,
    MII_CONTROL_100                 = SIZE16_BIT13,
    MII_CONTROL_AUTONEG_ENABLE      = SIZE16_BIT12,
    MII_CONTROL_POWER_DOWN          = SIZE16_BIT11,
    MII_CONTROL_ISOLATE             = SIZE16_BIT10,
    MII_CONTROL_RESTART_AUTONEG     = SIZE16_BIT9,
    MII_CONTROL_FULL_DUPLEX         = SIZE16_BIT8,
    MII_CONTROL_CDT_ENABLE          = SIZE16_BIT7
};

//---------------------------------------------------------------------------
// Basic Mode Status Register
//---------------------------------------------------------------------------

enum {
    MII_STATUS_T4                   = SIZE16_BIT15,
    MII_STATUS_TX_FD                = SIZE16_BIT14,
    MII_STATUS_TX_HD                = SIZE16_BIT13,
    MII_STATUS_10_FD                = SIZE16_BIT12,
    MII_STATUS_10_HD                = SIZE16_BIT11,
    MII_STATUS_AUTONEG_COMPLETE     = SIZE16_BIT5,
    MII_STATUS_REMOTE_FAULT_DETECT  = SIZE16_BIT4,
    MII_STATUS_AUTONEG_CAPABLE      = SIZE16_BIT3,
    MII_STATUS_LINK_STATUS          = SIZE16_BIT2,
    MII_STATUS_JABBER_DETECTED      = SIZE16_BIT1,
    MII_STATUS_EXTENDED_CAPABILITY  = SIZE16_BIT0
};

//---------------------------------------------------------------------------
// Auto-negotiation Advertisement Register
//---------------------------------------------------------------------------

enum {
    MII_ANAR_NEXT_PAGE              = SIZE16_BIT15,
    MII_ANAR_ACKNOWLEDGE            = SIZE16_BIT14,
    MII_ANAR_REMOTE_FAULT           = SIZE16_BIT13,
    MII_ANAR_T4                     = SIZE16_BIT9,
    MII_ANAR_TX_FD                  = SIZE16_BIT8,
    MII_ANAR_TX_HD                  = SIZE16_BIT7,
    MII_ANAR_10_FD                  = SIZE16_BIT6,
    MII_ANAR_10_HD                  = SIZE16_BIT5,
    MII_ANAR_SELECTOR_CSMACD        = 0x1
};

//---------------------------------------------------------------------------
// Auto-negotiation Link Partner Ability Register
//---------------------------------------------------------------------------

enum {
    MII_ANLP_NEXT_PAGE              = SIZE16_BIT15,
    MII_ANLP_ACKNOWLEDGE            = SIZE16_BIT14,
    MII_ANLP_REMOTE_FAULT           = SIZE16_BIT13,
    MII_ANLP_T4                     = SIZE16_BIT9,
    MII_ANLP_TX_FD                  = SIZE16_BIT8,
    MII_ANLP_TX_HD                  = SIZE16_BIT7,
    MII_ANLP_10_FD                  = SIZE16_BIT6,
    MII_ANLP_10_HD                  = SIZE16_BIT5
};

//---------------------------------------------------------------------------
// Auto-negotiation Expansion Register
//---------------------------------------------------------------------------

enum {
    MII_ANEX_PARALLEL_DETECT_FAULT  = SIZE16_BIT4,
    MII_ANEX_LP_NEXT_PAGEABLE       = SIZE16_BIT3,
    MII_ANEX_NEXT_PAGEABLE          = SIZE16_BIT2,
    MII_ANEX_PAGE_RECEIVED          = SIZE16_BIT1,
    MII_ANEX_LP_AUTONEGOTIABLE      = SIZE16_BIT0
};

#endif /* !_RTL8139HW_H */
