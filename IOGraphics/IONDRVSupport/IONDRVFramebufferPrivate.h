/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#pragma options align=mac68k

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
    kIODVIPowerEnableFlag = 0x10000,
    kIOI2CPowerEnableFlag = 0x20000
};

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


#pragma options align=reset

