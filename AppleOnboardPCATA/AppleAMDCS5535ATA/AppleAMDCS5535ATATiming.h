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

#ifndef _APPLEAMDCS5535ATATIMING_H
#define _APPLEAMDCS5535ATATIMING_H

#define kPIOModeCount    5   /* PIO   mode 0 - 4 */
#define kDMAModeCount    3   /* DMA   mode 0 - 2 */
#define kUltraModeCount  5   /* Ultra mode 0 - 4 */

typedef struct
{
    UInt16  cycleTimeNS;   /* total cycle time */
    UInt32  timingValue;   /* timing register program */
} TimingParameter;

/*---------------------------------------------------------------------------
 *
 * PIO Timings
 *
 ---------------------------------------------------------------------------*/

static const TimingParameter PIOTimingTable[ kPIOModeCount ]=
{
   /* Cycle  Program */
    { 600,   0xF7F4F7F4 },  /* Mode 0 */
    { 383,   0x53F3F173 },  /* Mode 1 */
    { 240,   0x13F18141 },  /* Mode 2 */
    { 180,   0x51315131 },  /* Mode 3 */
    { 120,   0x11311131 }   /* Mode 4 */ 
};

/*---------------------------------------------------------------------------
 *
 * Multi-Word DMA Timings
 *
 ---------------------------------------------------------------------------*/

static const TimingParameter DMATimingTable[ kDMAModeCount ]=
{
   /* Cycle  Program */
    { 480,   0x7F0FFFF3 },  /* Mode 0 */
    { 150,   0x7F035352 },  /* Mode 1 */
    { 120,   0x7F024241 }   /* Mode 2 */
};

/*---------------------------------------------------------------------------
 *
 * Ultra DMA Timings
 *
 ---------------------------------------------------------------------------*/

static const TimingParameter UltraTimingTable[ kUltraModeCount ]=
{
   /* Cycle  Program */
    { 0,     0x7F7436A1 },  /* Mode 0 */
    { 0,     0x7F733481 },  /* Mode 1 */
    { 0,     0x7F723261 },  /* Mode 2 */
    { 0,     0x7F713161 },  /* Mode 3 */
    { 0,     0x7F703061 }   /* Mode 4 (UDMA/66) */
};

#endif /* !_APPLEAMDCS5535ATATIMING_H */
