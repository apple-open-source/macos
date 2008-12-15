/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#ifndef __LP64__
#pragma options align=mac68k
#endif

struct VDConfigurationFeatureListRec
{
    OSType *	csConfigFeatureList;
    ItemCount	csNumConfigFeatures;
    UInt32	csReserved1;
    UInt32	csReserved2;
};

enum { cscGetFeatureList = 39 };

enum
{
    kDVIPowerSwitchFeature        = (1 << 0),	/* Used for csConfigFeature*/
    kDVIPowerSwitchSupportMask    = (1 << 0),	/* Read-only*/
    kDVIPowerSwitchActiveMask     = (1 << 0),	/* Read/write for csConfigValue*/
    kDVIPowerSwitchPowerOffDelay  = 200		/* ms before power off */
};

enum
{
    kIODVIPowerEnableFlag  = 0x00010000,
    kIOI2CPowerEnableFlag  = 0x00020000,
    kIONoncoherentTMDSFlag = 0x00040000
};

#define kIONDRVDisplayConnectFlagsKey	"display-connect-flags"

enum { kIONDRVAVJackProbeDelayMS = 1000 };

enum {
    cscSleepWake = 0x86,
    sleepWakeSig = 'slwk',
    vdSleepState = 0,
    vdWakeState  = 1
};

struct VDSleepWakeInfo
{
    UInt8	csMode;
    UInt8	fill;
    UInt32	csData;
};
typedef struct VDSleepWakeInfo VDSleepWakeInfo;


#ifndef __LP64__
#pragma options align=reset
#endif

