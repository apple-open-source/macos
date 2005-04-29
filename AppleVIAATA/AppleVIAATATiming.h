/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLEVIAATATIMING_H
#define _APPLEVIAATATIMING_H

#include "AppleVIAATAHardware.h"

#define kPIOModeCount    5   /* PIO mode 0 to 4 */
#define kDMAModeCount    3   /* DMA mode 0 to 2 */
#define kUDMAModeCount   7   /* Ultra mode 0 to 6 */

typedef struct
{
    UInt16  cycle;      /* t0  min total cycle time */
    UInt16  setup;      /* t1  min address setup time */
    UInt16  active;     /* t2  min command active time */
    UInt16  recovery;   /* t2i min command recovery time */
} VIATimingParameter;

/*
 * Enumeration of virtual timing registers.
 */
enum VIATimingReg {
    kVIATimingRegCommandActive = 0,
    kVIATimingRegCommandRecovery,
    kVIATimingRegDataActive,
    kVIATimingRegDataRecovery,
    kVIATimingRegAddressSetup,
    kVIATimingRegUltra,
    kVIATimingRegCount
};

/*
 * This table returns the PCI config space offset of any virtual
 * timing register indexed by ATA channel number, and drive number.
 */
static const UInt8
VIATimingRegOffset[kVIATimingRegCount][kMaxChannelCount][kMaxDriveCount] = 
{
    {   /* kVIATimingRegCommandActive */
        { VIA_CMD_TIMING + 1, VIA_CMD_TIMING + 1 },     /* 0, 1 */
        { VIA_CMD_TIMING + 0, VIA_CMD_TIMING + 0 }      /* 2, 3 */
    },
    {   /* kVIATimingRegCommandRecovery */
        { VIA_CMD_TIMING + 1, VIA_CMD_TIMING + 1 },     /* 0, 1 */
        { VIA_CMD_TIMING + 0, VIA_CMD_TIMING + 0 }      /* 2, 3 */
    },
    {   /* kVIATimingRegDataActive */
        { VIA_DATA_TIMING + 3, VIA_DATA_TIMING + 2 },   /* 0, 1 */
        { VIA_DATA_TIMING + 1, VIA_DATA_TIMING + 0 }    /* 2, 3 */
    },
    {   /* kVIATimingRegDataRecovery */
        { VIA_DATA_TIMING + 3, VIA_DATA_TIMING + 2 },   /* 0, 1 */
        { VIA_DATA_TIMING + 1, VIA_DATA_TIMING + 0 }    /* 2, 3 */
    },
    {   /* kVIATimingRegAddressSetup */
        { VIA_ADDRESS_SETUP, VIA_ADDRESS_SETUP },       /* 0, 1 */
        { VIA_ADDRESS_SETUP, VIA_ADDRESS_SETUP }        /* 2, 3 */
    },
    {   /* kVIATimingRegUltra */
        { VIA_ULTRA_TIMING + 3, VIA_ULTRA_TIMING + 2 }, /* 0, 1 */
        { VIA_ULTRA_TIMING + 1, VIA_ULTRA_TIMING + 0 }  /* 2, 3 */
    }
};

/*
 * Properties about each virtual timing register.
 */
static const struct {
    UInt8  mask;
    UInt8  shift;
    UInt8  minValue;
    UInt8  maxValue;
} VIATimingRegInfo[ kVIATimingRegCount ] =
{
    { 0xF0, 4, 1, 16 },  /* kVIATimingRegCommandActive */
    { 0x0F, 0, 1, 16 },  /* kVIATimingRegCommandRecovery */
    { 0xF0, 4, 1, 16 },  /* kVIATimingRegDataActive */
    { 0x0F, 0, 1, 16 },  /* kVIATimingRegDataRecovery */
    { 0xC0, 6, 1,  4 },  /* kVIATimingRegAddressSetup */
    { 0xFF, 0, 0,  0 }   /* kVIATimingRegUltra */
};

/*---------------------------------------------------------------------------
 *
 * PIO
 *
 ---------------------------------------------------------------------------*/

/*
 * Minimum cycle time for each PIO mode number.
 */
static const UInt16 PIOMinCycleTime[ kPIOModeCount ] =
{
    600,   /* Mode 0 */
    383,   /* Mode 1 */
    240,   /* Mode 2 */
    180,   /* Mode 3 */
    120    /* Mode 4 */
};

/*
 * PIO timing parameters.
 * The setup and recovery times are equal to or larger than the
 * minimal values defined by the ATA spec.
 */
#define kPIOTimingCount 5

static const VIATimingParameter PIOTimingTable[ kPIOTimingCount ]=
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

/*
 * Minimum cycle time for each MW-DMA mode number.
 */
static const UInt16 DMAMinCycleTime[ kDMAModeCount ] =
{
    480,   /* Mode 0 */
    150,   /* Mode 1 */
    120    /* Mode 2 */
};

/*
 * DMA timing parameters.
 * The tD and tK parameters are equal to or larger than the
 * minimal values defined by the ATA spec.
 */
#define kDMATimingCount 3

static const VIATimingParameter DMATimingTable[ kDMATimingCount ]=
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

static const UInt8 UltraTimingTable[VIA_HW_COUNT][kUDMAModeCount] =
{
/* UDMA  0     1     2     3     4     5     6 */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* No UDMA    */
    { 0xc2, 0xc1, 0xc0, 0x00, 0x00, 0x00, 0x00 },  /* VIA ATA33  */
    { 0xee, 0xec, 0xea, 0xe9, 0xe8, 0x00, 0x00 },  /* VIA ATA66  */
    { 0xf7, 0xf6, 0xf4, 0xf2, 0xf1, 0xf0, 0x00 },  /* VIA ATA100 */
    { 0xf7, 0xf7, 0xf6, 0xf4, 0xf2, 0xf1, 0xf0 },  /* VIA ATA133 */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* VIA SATA   */
};

#endif /* !_APPLEVIAATATIMING_H */
