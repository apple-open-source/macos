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
 *  File: $Id: SMU_Neo2_PowerSensor.h,v 1.3 2005/01/27 00:22:53 dirty Exp $
 */


#ifndef _SMU_NEO2_POWERSENSOR_H
#define _SMU_NEO2_POWERSENSOR_H

#include "IOPlatformSensor.h"

// This class corresponds to a virtual sensor.  It depends on the current and voltage values
// from other sensors and generates it's power value from those.

class SMU_Neo2_PowerSensor : public IOPlatformSensor
	{
	OSDeclareDefaultStructors( SMU_Neo2_PowerSensor )

protected:

	IOPlatformSensor*						currentSensor;
	IOPlatformSensor*						voltageSensor;

	bool									fUseQuadraticTransform;
	sdb_thermal_pos_adc_constants_2_part_t	fThermalPosConstantsPart;

	virtual			IOReturn		registerDriver( IOService* driver, const OSDictionary* dict, bool notify = true );

	virtual			IOReturn		initPlatformSensor( const OSDictionary* dict );

	virtual			SensorValue		fetchCurrentValue( void );

	virtual			bool			sendPollingPeriod( OSNumber* period );
	};


#endif	// _SMU_NEO2_POWERSENSOR_H
