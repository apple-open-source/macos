/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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

#ifndef _APPLENVIDIANFORCETIMING_H
#define _APPLENVIDIANFORCETIMING_H

#define kPIOModeCount	5	/* PIO mode 0 to 4 */
#define kDMAModeCount	3	/* DMA mode 0 to 2 */
#define kUltraModeCount	7	/* Ultra mode 0 to 6 */

/*
 * nForce PCI config space registers.
 */
#define PCI_IDE_ENABLE      0x50
#define PCI_IDE_CONFIG      0x51
#define PCI_CABLE_DETECT	0x52
#define PCI_FIFO_CONFIG     0x53
#define PCI_DATA_TIMING     0x58
#define PCI_CMD_TIMING      0x5e
#define PCI_ADDRESS_SETUP   0x5c
#define PCI_ULTRA_TIMING    0x60

typedef struct
{
    UInt16  cycleTime;		/* t0  min total cycle time */
    UInt16  setupTime;		/* t1  min address setup time */
    UInt16  activeTime;     /* t2  min command active time */
    UInt16  recoveryTime;   /* t2i min command recovery time */
} TimingParameter;

/*
 * Virtual timing registers
 */
enum TimingReg {
    kTimingRegCommandActive = 0,
    kTimingRegCommandRecovery,
    kTimingRegDataActive,
    kTimingRegDataRecovery,
    kTimingRegAddressSetup,
    kTimingRegUltra,
    kTimingRegCount
};

/*
 * This table returns the PCI config space offset of any virtual
 * timing register indexed by ATA channel number, and drive number.
 */
static const UInt8
TimingRegOffset[kTimingRegCount][kMaxChannelCount][kMaxDriveCount] = 
{
    {   /* kTimingRegCommandActive */
        { PCI_CMD_TIMING + 1, PCI_CMD_TIMING + 1 },     /* 0, 1 */
        { PCI_CMD_TIMING + 0, PCI_CMD_TIMING + 0 }      /* 2, 3 */
    },
    {   /* kTimingRegCommandRecovery */
        { PCI_CMD_TIMING + 1, PCI_CMD_TIMING + 1 },     /* 0, 1 */
        { PCI_CMD_TIMING + 0, PCI_CMD_TIMING + 0 }      /* 2, 3 */
    },
    {   /* kTimingRegDataActive */
        { PCI_DATA_TIMING + 3, PCI_DATA_TIMING + 2 },   /* 0, 1 */
        { PCI_DATA_TIMING + 1, PCI_DATA_TIMING + 0 }    /* 2, 3 */
    },
    {   /* kTimingRegDataRecovery */
        { PCI_DATA_TIMING + 3, PCI_DATA_TIMING + 2 },   /* 0, 1 */
        { PCI_DATA_TIMING + 1, PCI_DATA_TIMING + 0 }    /* 2, 3 */
    },
    {   /* kTimingRegAddressSetup */
        { PCI_ADDRESS_SETUP, PCI_ADDRESS_SETUP },       /* 0, 1 */
        { PCI_ADDRESS_SETUP, PCI_ADDRESS_SETUP }        /* 2, 3 */
    },
    {   /* kTimingRegUltra */
        { PCI_ULTRA_TIMING + 3, PCI_ULTRA_TIMING + 2 }, /* 0, 1 */
        { PCI_ULTRA_TIMING + 1, PCI_ULTRA_TIMING + 0 }  /* 2, 3 */
    }
};

/*
 * Properties for each virtual timing register.
 */
static const struct {
    UInt8  bitmask;
    UInt8  bitnum;
    UInt8  minValue;
    UInt8  maxValue;
} TimingRegInfo[ kTimingRegCount ] =
{
    { 0x0F, 4, 1, 16 },  /* kTimingRegCommandActive */
    { 0x0F, 0, 1, 16 },  /* kTimingRegCommandRecovery */
    { 0x0F, 4, 1, 16 },  /* kTimingRegDataActive */
    { 0x0F, 0, 1, 16 },  /* kTimingRegDataRecovery */
    { 0x03, 6, 1,  4 },  /* kTimingRegAddressSetup */
    { 0xFF, 0, 0,  0 }   /* kTimingRegUltra */
};

/*---------------------------------------------------------------------------
 *
 * PIO
 *
 ---------------------------------------------------------------------------*/

static const TimingParameter PIOTimingTable[ kPIOModeCount ] =
{
   /* Cycle  Setup  Act    Rec */
    { 600,    70,   290,   310 },
    { 383,    50,   290,    93 },
    { 240,    30,   290,    50 },
    { 180,    30,    90,    90 }, 
    { 120,    25,    75,    45 }
};

/*---------------------------------------------------------------------------
 *
 * Multi-word DMA
 *
 ---------------------------------------------------------------------------*/

static const TimingParameter DMATimingTable[ kDMAModeCount ]=
{
   /* Cycle  Setup   tD     tK */
    { 480,    70,   240,   240 },
    { 150,    50,    90,    60 },
    { 120,    30,    75,    45 }
};

/*---------------------------------------------------------------------------
 *
 * Ultra DMA
 *
 ---------------------------------------------------------------------------*/

static const UInt8 UltraTimingTable[kUltraModeCount] =
{
/* UDMA  0     1     2     3     4     5     6 */
      0xc2, 0xc1, 0xc0, 0xc4, 0xc5, 0xc6, 0xc7
};

#endif /* !_APPLENVIDIANFORCETIMING_H */
