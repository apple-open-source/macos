/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: RackMac3_1_PWMFanControl.h,v 1.5 2004/03/18 02:18:52 eem Exp $
 */


#ifndef _RACKMAC3_1_PWMFANCONTROL_H
#define _RACKMAC3_1_PWMFANCONTROL_H

#include "IOPlatformPWMFanControl.h"

#define kRM31HwmondThresholds			"hwmondThresholds"
#define kRM31GroupTag					"group"
#define kRM31SortNumber					"sort-key"

class RackMac3_1_PWMFanControl : public IOPlatformPWMFanControl
{

    OSDeclareDefaultStructors(RackMac3_1_PWMFanControl)

protected:

    virtual IOReturn		initPlatformControl( const OSDictionary *dict );

};
#endif // _RACKMAC3_1_PWMFANCONTROL_H
