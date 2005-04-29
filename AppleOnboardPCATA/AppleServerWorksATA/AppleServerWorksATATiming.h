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

#ifndef _APPLESERVERWORKSATATIMING_H
#define _APPLESERVERWORKSATATIMING_H

#define kPIOModeCount    5   /* PIO   mode 0 - 4 */
#define kDMAModeCount    3   /* DMA   mode 0 - 2 */
#define kUltraModeCount  6   /* Ultra mode 0 - 5 */

typedef struct
{
    UInt16  cycleTimeNS;   /* total cycle time */
    UInt8   timingValue;   /* timing register value */
    UInt8   modeNumber;
} TimingParameter;

/*---------------------------------------------------------------------------
 *
 * PIO Timings
 *
 ---------------------------------------------------------------------------*/

static const TimingParameter PIOTimingTable[ kPIOModeCount ]=
{
   /* Cycle  Value   Mode */
    { 600,   0x5d,   0 },
    { 383,   0x47,   1 },
    { 240,   0x34,   2 },
    { 180,   0x22,   3 },
    { 120,   0x20,   4 }
};

/*---------------------------------------------------------------------------
 *
 * Multi-word DMA Timings
 *
 ---------------------------------------------------------------------------*/

static const TimingParameter DMATimingTable[ kDMAModeCount ]=
{
   /* Cycle  Value   Mode */
    { 480,   0x77,   0 },
    { 150,   0x21,   1 },
    { 120,   0x20,   2 }
};

#endif /* !_APPLESERVERWORKSATATIMING_H */
