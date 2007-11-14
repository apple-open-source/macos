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
 *  File: $Id: RackMac3_1_CPUTempSensor.h,v 1.5 2004/03/18 02:18:52 eem Exp $
 *
 */


#ifndef _RACKMAC3_1_CPUTEMPSENSOR_H
#define _RACKMAC3_1_CPUTEMPSENSOR_H

#include "IOPlatformSensor.h"

#define kRM31GroupTag				"group"
#define kRM31SortNumber				"sort-key"

class RackMac3_1_CPUTempSensor : public IOPlatformSensor
{

    OSDeclareDefaultStructors(RackMac3_1_CPUTempSensor)

protected:

    UInt32 scalingFactor;
    SInt32 constant;

    // initialize a sensor from it's SensorArray dict in the IOPlatformThermalProfile
    virtual IOReturn		initPlatformSensor( const OSDictionary * dict );

    // apply scaling factor
	virtual SensorValue applyCurrentValueTransform( SensorValue ) const;
	//virtual SensorValue applyCurrentValueInverseTransform( SensorValue ) const;
};

#endif // _RACKMAC3_1_CPUTEMPSENSOR_H