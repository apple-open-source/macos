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
 *  File: $Id: SMU_Neo2_CPUTempSensor.cpp,v 1.6 2004/06/28 23:50:18 dirty Exp $
 */


#include "IOPlatformPluginSymbols.h"
#include "SMU_Neo2_PlatformPlugin.h"

#include "SMU_Neo2_CPUTempSensor.h"


OSDefineMetaClassAndStructors( SMU_Neo2_CPUTempSensor, IOPlatformSensor )


IOReturn SMU_Neo2_CPUTempSensor::initPlatformSensor( const OSDictionary* dict )
	{
	UInt8						bytes[ 2 ];
	OSData*						data;
	IOReturn					status;

	status = IOPlatformSensor::initPlatformSensor( dict );

	if ( !gPlatformPlugin->getSDBPartitionData( kDiodeCalibrationPartID, 4, 2, bytes ) )
		goto SMU_Neo2_CPUTempSensor_initPlatformSensor_MissingDiodePartition;

	scalingFactor = ( UInt32 ) *( ( UInt16 * ) bytes );

	if ( scalingFactor == 0xFFFF )
		return kIOReturnError;

	if ( ( data = OSData::withBytes( bytes, 2 ) ) == NULL )
		return( kIOReturnError );

	// For debugging purposes.
	infoDict->setObject( "m-diode", data );
	data->release();

	if ( !gPlatformPlugin->getSDBPartitionData( kDiodeCalibrationPartID, 6, 2, bytes ) )
		goto SMU_Neo2_CPUTempSensor_initPlatformSensor_MissingDiodePartition;

	constant = ( SInt32 ) *( ( SInt16 * ) bytes );

	if ( constant == -1 )
		return kIOReturnError;

	if ( ( data = OSData::withBytes( bytes, 2 ) ) == NULL )
		return( kIOReturnError );

	// For debugging purposes.
	infoDict->setObject( "b-diode", data );
	data->release();

	return( status );


SMU_Neo2_CPUTempSensor_initPlatformSensor_MissingDiodePartition:

	bool							willPanic = true;


	if ( willPanic )
		panic( "Missing CPU diode calibration information.\n" );

	return( kIOReturnError );
	}


SensorValue SMU_Neo2_CPUTempSensor::applyCurrentValueTransform( SensorValue inputValue ) const
	{
	SensorValue					reading;

	// inputValue is in whole integers.
	// M is 1.15 (unsigned)
	// B is 10.6 (signed)

	long long					mx;
	long long					adjustedConstant;

	adjustedConstant = ( ( long long ) constant << 9 );

	mx = ( ( inputValue.sensValue * scalingFactor ) / 8 )  + adjustedConstant;
	reading.sensValue = ( mx << 1 );		// Get back to 16.16.

	return( reading );
	}
