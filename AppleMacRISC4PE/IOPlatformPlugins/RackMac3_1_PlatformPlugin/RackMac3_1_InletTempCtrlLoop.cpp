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

#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "IOPlatformSensor.h"
#include "RackMac3_1_InletTempCtrlLoop.h"

#define super IOPlatformPIDCtrlLoop
OSDefineMetaClassAndStructors(RackMac3_1_InletTempCtrlLoop, IOPlatformPIDCtrlLoop)

extern const OSSymbol * gRM31EnableSlewing;

IOReturn RackMac3_1_InletTempCtrlLoop::initPlatformCtrlLoop( const OSDictionary *dict)
{
    IOReturn status;
    const OSArray * array;

    //CTRLLOOP_DLOG("RackMac3_1_InletTempCtrlLoop::initPlatformCtrlLoop ENTERED\n");

    status = super::initPlatformCtrlLoop(dict);

    if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalSensorIDsKey))) == NULL)
    {
        CTRLLOOP_DLOG("RackMac3_1_InletTempCtrlLoop::initPlatformCtrlLoop no SensorIDArray\n");
        return(kIOReturnError);
    }

    // get 2nd sensor at index 2 in the sensor-id array
    if ((inputSensor2 = OSDynamicCast(IOPlatformSensor, platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(1)) ))) == NULL)
    {
        CTRLLOOP_DLOG("RackMac3_1_InletTempCtrlLoop::initPlatformCtrlLoop no inlet temperature CPU B sensor ID!!\n");
        return(kIOReturnError);
    }

    inputSensor2->retain();
    addSensor( inputSensor2 );

    //CTRLLOOP_DLOG("RackMac3_1_InletTempCtrlLoop::initPlatformCtrlLoop FINISHED\n");

    return(status);
}

bool RackMac3_1_InletTempCtrlLoop::acquireSample( void )
{
	inletATemperature = inputSensor->forceAndFetchCurrentValue();
	inputSensor->setCurrentValue( inletATemperature );

    inletBTemperature = inputSensor2->forceAndFetchCurrentValue();
    inputSensor2->setCurrentValue( inletBTemperature );

    return(true);
}

void RackMac3_1_InletTempCtrlLoop::deadlinePassed( void )
{
    bool deadlineAbsolute;
    SensorValue result;
	
    deadlineAbsolute = (ctrlloopState == kIOPCtrlLoopFirstAdjustment);

    timerCallbackActive = true;

    // sample the input
    if (!acquireSample())
    {
        CTRLLOOP_DLOG("RackMac3_1_CPUFanCtrlLoop::deadlinePassed FAILED TO ACQUIRE INPUT SAMPLE!!!\n");
    }

	// Average the temperature across the 2 inlet sensors
	result.sensValue = (inletATemperature.sensValue + inletBTemperature.sensValue)/2;

	if( result.sensValue >= inputTarget.sensValue)
        platformPlugin->setEnvArray(gRM31EnableSlewing, this, true);
	else
        platformPlugin->setEnvArray(gRM31EnableSlewing, this, false);

    // set the deadline
    if (deadlineAbsolute)
    {
        // this is the first time we're setting the deadline.  In order to better stagger
        // timer callbacks, offset the deadline by 100us * ctrlloopID.
        AbsoluteTime adjustedInterval;
        const OSNumber * id = getCtrlLoopID();

        // 100 * ctrlLoopID -> absolute time format
        clock_interval_to_absolutetime_interval(100 * id->unsigned32BitValue(), NSEC_PER_USEC, &adjustedInterval);

        // Add standard interval to produce adjusted interval
        ADD_ABSOLUTETIME( &adjustedInterval, &interval );

        clock_absolutetime_interval_to_deadline(adjustedInterval, &deadline);

		ctrlloopState = kIOPCtrlLoopAllRegistered;
    }
    else
    {
        ADD_ABSOLUTETIME(&deadline, &interval);
    }

    timerCallbackActive = false;
}
