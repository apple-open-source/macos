/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: SMU_Neo2_CPUVoltageSensor.h,v 1.2 2004/03/30 01:16:35 dirty Exp $
 */


#ifndef _SMU_NEO2_CPUVOLTAGESENSOR_H
#define _SMU_NEO2_CPUVOLTAGESENSOR_H

#include "IOPlatformSensor.h"


class SMU_Neo2_CPUVoltageSensor : public IOPlatformSensor
	{
	OSDeclareDefaultStructors( SMU_Neo2_CPUVoltageSensor )

protected:

	UInt32							scalingFactor;
	SInt32							offsetFactor;

	virtual			IOReturn		initPlatformSensor( const OSDictionary* dict );
	virtual			SensorValue		applyCurrentValueTransform( SensorValue ) const;
	};

#endif // _SMU_NEO2_CPUVOLTAGESENSOR_H
