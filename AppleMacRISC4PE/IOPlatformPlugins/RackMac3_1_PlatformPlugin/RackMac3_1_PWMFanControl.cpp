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
 *  File: $Id: RackMac3_1_PWMFanControl.cpp,v 1.5 2004/03/18 02:18:52 eem Exp $
 */


#include "RackMac3_1_PWMFanControl.h"

#define super IOPlatformPWMFanControl
OSDefineMetaClassAndStructors(RackMac3_1_PWMFanControl, IOPlatformPWMFanControl)

IOReturn RackMac3_1_PWMFanControl::initPlatformControl( const OSDictionary *dict )
{
    OSDictionary 	*tmp_osdict;
    OSString		*tmp_string;
    OSNumber		*tmp_number;

    IOReturn status = super::initPlatformControl( dict );

    tmp_osdict = OSDynamicCast(OSDictionary, dict->getObject(kRM31HwmondThresholds));
    if (tmp_osdict)
		infoDict->setObject(kRM31HwmondThresholds, tmp_osdict);
#if CONTROL_DEBUG
    else
        CONTROL_DLOG("RackMac3_1_PWMFanControl::initPlatformControl no hwmondThresholds dictionary for control id = 0x%x!!\n", getControlID()->unsigned16BitValue());
#endif

    tmp_string = OSDynamicCast(OSString, dict->getObject(kRM31GroupTag));
    if (tmp_string)
		infoDict->setObject(kRM31GroupTag, tmp_string);
#if CONTROL_DEBUG
    else
        CONTROL_DLOG("RackMac3_1_PWMFanControl::initPlatformControl no group tag for control id = 0x%x!!\n", getControlID()->unsigned16BitValue());
#endif

    tmp_number = OSDynamicCast(OSNumber, dict->getObject(kRM31SortNumber));
    if (tmp_number)
		infoDict->setObject(kRM31SortNumber, tmp_number);
#if CONTROL_DEBUG
    else
        CONTROL_DLOG("RackMac3_1_PWMFanControl::initPlatformControl no sort number for control id = 0x%x!!\n", getControlID()->unsigned16BitValue());
#endif

    return(status);
}
