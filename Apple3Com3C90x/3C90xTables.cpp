/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include "3C90x.h"

/*
 * Adapter information.
 */
const AdapterInfo adapterInfoTable[] =
{
    { 0x9000,  kAdapterType3C90x,   "3C900-TPO"    },
    { 0x9001,  kAdapterType3C90x,   "3C900-Combo"  },
    { 0x9050,  kAdapterType3C90x,   "3C905-TX"     },
    { 0x9051,  kAdapterType3C90x,   "3C905-T4"     },
    { 0x9055,  kAdapterType3C90xB,  "3C905B-TX"    },
    { 0x9056,  kAdapterType3C90xB,  "3C905B-T4"    },
    { 0x9005,  kAdapterType3C90xB,  "3C905B-Combo" },
    { 0x9004,  kAdapterType3C90xB,  "3C905B-TPO"   },
    { 0x9006,  kAdapterType3C90xB,  "3C905B-TPC"   },
    { 0x900A,  kAdapterType3C90xB,  "3C905B-FL"    },
    { 0x9056,  kAdapterType3C90xB,  "3C905B-FX"    },
    { 0x9200,  kAdapterType3C90xC,  "3C905C"       },
    { 0xFFFF,  kAdapterType3C90xC,  "3C905x"       }
};

const UInt32 adapterInfoTableCount = ( sizeof(adapterInfoTable) /
                                       sizeof(adapterInfoTable[0]) );

/*
 * Media port programming information.
 */
const MediaPortInfo mediaPortTable[] =
{
/*     Name         Speed  MediaCode        selectable       IOKit type        */
    { "10BaseT",     10,   kMediaCode10TP,  true,   kIOMediumEthernet10BaseT   },
    { "AUI",         10,   kMediaCodeSQE,   true,   kIOMediumEthernet10Base5   },
    { "Auto",         0,   kMediaCodeDef,   true,   kIOMediumEthernetAuto      },
    { "BNC",         10,   kMediaCodeDef,   true,   kIOMediumEthernet10Base2   },
    { "100BaseTX",  100,   kMediaCodeLink,  true,   kIOMediumEthernet100BaseTX },
    { "100BaseFX",  100,   kMediaCodeLink,  true,   kIOMediumEthernet100BaseFX },
    { "MII",          0,   kMediaCodeDef,   false,  kIOMediumEthernetNone      },
    { "100BaseT4",  100,   kMediaCodeDef,   true,   kIOMediumEthernet100BaseT4 },
    { "AutoNeg",      0,   kMediaCodeLink,  false,  kIOMediumEthernetNone      },
    { "None",         0,   kMediaCodeDef,   false,  kIOMediumEthernetNone      }
};
