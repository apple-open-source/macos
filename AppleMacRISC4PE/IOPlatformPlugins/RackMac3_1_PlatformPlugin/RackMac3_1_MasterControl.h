/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */

#ifndef _RACKMAC3_1_MASTERCONTROL_H
#define _RACKMAC3_1_MASTERCONTROL_H

#include "IOPlatformControl.h"

#define kRM31HwmondThresholds			"hwmondThresholds"
#define kRM31GroupTag					"group"
#define kRM31SortNumber					"sort-key"

// The MasterControl class is used to post theshold percentanges for use by hwmond

class RackMac3_1_MasterControl : public IOPlatformControl
{

    OSDeclareDefaultStructors(RackMac3_1_MasterControl)

protected:

    virtual IOReturn		initPlatformControl( const OSDictionary * dict );

};
#endif // _RACKMAC3_1_MASTERCONTROL_H