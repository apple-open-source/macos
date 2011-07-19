/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include "MacTypes.r"
#include "IOFWDVComponents.h"

#define forCarbon			1
#define UseExtendedThingResource	1
#include "Components.r"

resource 'thng' (kIsocCodecThing, "DV_IHandler")
{
    'ihlr',
    'dv  ',
    'appl',
    0, 					// 0x80000000 cmpWantsRegisterMessage,
    kAnyComponentFlagsMask,
    'dlle', kIsocCodecBaseID,
    'STR ', kIsocCodecNameID,
    'STR ', kIsocCodecNameID,
    '    ', 0,
    0x30000,
    // component registration flags
    componentHasMultiplePlatforms, // | componentDoAutoVersion | componentWantsUnregister,
    0, 					// resource id of icon family
    {
//        cmpWantsRegisterMessage  |
            0,		// component flags
        'dlle', kIsocCodecBaseID, platformPowerPCNativeEntryPoint,
            0,		// component flags
        'dlle', kIsocCodecBaseID, platformIA32NativeEntryPoint,
    }
};

resource 'dlle' (kIsocCodecBaseID)
{
	"FWDVICodecComponentDispatch"
};

resource 'thng' (kControlCodecThing, "DV_DCHandler")
{
    'devc',
    'fwdv',
    'appl',
    0, 					// 0x80000000 cmpWantsRegisterMessage,
    kAnyComponentFlagsMask,
    'dlle', kControlCodecBaseID,
    'STR ', kControlCodecNameID,
    'STR ', kControlCodecNameID,
    '    ', 0,
    0x30000,
    // component registration flags
    componentHasMultiplePlatforms, // | componentDoAutoVersion | componentWantsUnregister,
    0, 					// resource id of icon family
    {
//        cmpWantsRegisterMessage  |
            0,		// component flags
        'dlle', kControlCodecBaseID, platformPowerPCNativeEntryPoint,
            0,		// component flags
        'dlle', kControlCodecBaseID, platformIA32NativeEntryPoint,
    }
};

resource 'dlle' (kControlCodecBaseID)
{
	"FWDVCCodecComponentDispatch"
};


