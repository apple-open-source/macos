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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 */


#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "RackMac3_1_PlatformPlugin.h"
#include "IOPlatformSensor.h"
#include "RackMac3_1_CPUBUniFanCtrlLoop.h"

#define super IOPlatformPIDCtrlLoop
OSDefineMetaClassAndStructors(RackMac3_1_CPUBUniFanCtrlLoop, IOPlatformPIDCtrlLoop)

extern const OSSymbol * gRM31DIMMFanCtrlLoopTarget;

bool RackMac3_1_CPUBUniFanCtrlLoop::init( void )
{
    if (!super::init()) return(false);

    secondOutputControl = NULL;
    thirdOutputControl = NULL;

    return(true);
}

void RackMac3_1_CPUBUniFanCtrlLoop::free( void )
{
    if (secondOutputControl) { secondOutputControl->release(); secondOutputControl = NULL; }

    if (thirdOutputControl) { thirdOutputControl->release(); thirdOutputControl = NULL; }

    super::free();
}

IOReturn RackMac3_1_CPUBUniFanCtrlLoop::initPlatformCtrlLoop( const OSDictionary *dict)
{
    IOReturn status;
    const OSArray * array;

    //CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::initPlatformCtrlLoop ENTERED\n");

    status = super::initPlatformCtrlLoop(dict);

    // the second control is the second CPU fan (index 1)
    if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
        (secondOutputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(1)) )) == NULL)
    {
        CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::initPlatformCtrlLoop no second control ID!!\n");
        return(kIOReturnError);
    }

    secondOutputControl->retain();
    addControl( secondOutputControl );

    // the third control is the third CPU fan (index 2)
    if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
        (thirdOutputControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(2)) )) == NULL)
    {
        CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::initPlatformCtrlLoop no third control ID!!\n");
        return(kIOReturnError);
    }

    thirdOutputControl->retain();
    addControl( thirdOutputControl );

    //CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::initPlatformCtrlLoop FINISHED\n");

    return(status);
}

bool RackMac3_1_CPUBUniFanCtrlLoop::updateMetaState( void )
{
    const OSArray * metaStateArray;
    const OSDictionary * metaStateDict;
    const OSNumber * newMetaState;

    // else if there is an overtemp condition, use meta-state 1
    // else if there is a forced meta state, use it
    // else, use meta-state 0

    if ((metaStateArray = OSDynamicCast(OSArray, infoDict->getObject(gIOPPluginThermalMetaStatesKey))) == NULL)
    {
        CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::updateMetaState no meta state array\n");
        return(false);
    }

    // Check for overtemp condition
	if (platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp))
	{
        CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::updateMetaState Entering Overtemp Mode\n");

        if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(1))) != NULL &&
            (cacheMetaState( metaStateDict ) == true))
        {
            // successfully entered overtemp mode
            setMetaState( gIOPPluginOne );
            return(true);
        }
        else
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::updateMetaState Overtemp Mode Failed!\n");
        }
    }

    // Look for forced meta state
    if ((metaStateDict = OSDynamicCast(OSDictionary, infoDict->getObject(gIOPPluginForceCtrlLoopMetaStateKey))) != NULL)
    {
        if (cacheMetaState( metaStateDict ) == true)
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::updateMetaState using forced meta state\n");
            newMetaState = OSNumber::withNumber( 0xFFFFFFFF, 32 );
            setMetaState( newMetaState );
            newMetaState->release();
            return(true);
        }
        else
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::updateMetaState forced meta state is invalid, removing...\n");
            infoDict->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
        }
    }

    // Use default "Normal" meta state
    if ((metaStateDict = OSDynamicCast(OSDictionary, metaStateArray->getObject(0))) != NULL && (cacheMetaState( metaStateDict ) == true))
    {
        //CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::updateMetaState use meta state zero\n");
        setMetaState( gIOPPluginZero );
        return(true);
    }
    else
    {
        // can't find a valid meta state, nothing we can really do except log an error
        CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::updateMetaState no valid meta states!\n");
        return(false);
    }
}

bool RackMac3_1_CPUBUniFanCtrlLoop::cacheMetaState( const OSDictionary * metaState )
{
    const OSNumber * numInterval, * numOverride;
    const OSNumber * numOutputMin, * numOutputMax;
    UInt32 tempInterval;

    // cache the interval.  it is listed in seconds.
    if ((numInterval = OSDynamicCast(OSNumber, metaState->getObject("interval"))) != NULL)
    {
        tempInterval = numInterval->unsigned32BitValue();

        if ((tempInterval == 0) || (tempInterval > 300))
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::cacheMetaState meta state interval is out of bounds\n");
            goto failNoInterval;
        }
    }
    else
    {
        CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::cacheMetaState meta state interval is absent\n");
        goto failNoInterval;
    }

    // if there is an output-override key, flag it.  Otherwise, look for the full
    // set of coefficients, setpoints and output bounds
    if ((numOverride = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputOverrideKey))) != NULL)
    {
        overrideActive = true;
        outputOverride = numOverride;
        outputOverride->retain();

        //CTRLLOOP_DLOG("*** PID CACHE *** Override: 0x%08lX\n", outputOverride->unsigned32BitValue());
    }
    else
    {
        if ((numOutputMin = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputMinKey))) == NULL)
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::cacheMetaState meta state has no output-min\n");
            goto failFullSet;
        }

        if ((numOutputMax = OSDynamicCast(OSNumber, metaState->getObject(kIOPPIDCtrlLoopOutputMaxKey))) == NULL)
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::cacheMetaState meta state has no output-max\n");
            goto failFullSet;
        }

        overrideActive = false;
        if (outputOverride) { outputOverride->release(); outputOverride = NULL; }

        outputMin = numOutputMin->unsigned32BitValue();
        outputMax = numOutputMax->unsigned32BitValue();

    }

    // set the interval
    intervalSec = tempInterval;
    clock_interval_to_absolutetime_interval(intervalSec, NSEC_PER_SEC, &interval);
//	CTRLLOOP_DLOG("***************** Interval: %u\n", intervalSec);

    return(true);

failFullSet:
failNoInterval:
    return(false);

}

ControlValue RackMac3_1_CPUBUniFanCtrlLoop::calculateNewTarget( void ) const
{
    UInt32 newOutputMin;
	ControlValue newTarget;
    const OSNumber * newDIMMFanCtrlLoopTarget;
	
    // if there is an output override, use it
    if (overrideActive)
    {
        CTRLLOOP_DLOG("*** PID *** Override Active\n");
        newTarget = outputOverride->unsigned32BitValue();
    }
    else
    {
		newTarget = outputMin;
		
		// Determine min fan speed based on DIMM control loop
		newOutputMin = outputMin;
		newDIMMFanCtrlLoopTarget = OSDynamicCast(OSNumber, platformPlugin->getEnv(gRM31DIMMFanCtrlLoopTarget));
		if(newDIMMFanCtrlLoopTarget)
		{
			newOutputMin = newDIMMFanCtrlLoopTarget->unsigned32BitValue();

			if(newOutputMin < outputMin)
				newOutputMin = outputMin;
		}
		
        // apply the hard limits
        if (newTarget < newOutputMin)
            newTarget = newOutputMin;
        else if (newTarget > outputMax)
            newTarget = outputMax;
    }

    return(newTarget);
}

void RackMac3_1_CPUBUniFanCtrlLoop::deadlinePassed( void )
{
    bool deadlineAbsolute;
	ControlValue newTarget;

    deadlineAbsolute = (ctrlloopState == kIOPCtrlLoopFirstAdjustment);

    timerCallbackActive = true;

	if (ctrlloopState == kIOPCtrlLoopNotReady)
		return;

	// Apply the PID algorithm
	newTarget = calculateNewTarget();

	// set the target
	sendNewTarget( newTarget );

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
    }
    else
    {
        ADD_ABSOLUTETIME(&deadline, &interval);
    }

    timerCallbackActive = false;
}

void RackMac3_1_CPUBUniFanCtrlLoop::sendNewTarget( ControlValue newTarget )
{
    // If the new target value is different, send it to the control
    if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
        ctrlloopState == kIOPCtrlLoopDidWake ||
        newTarget != outputControl->getTargetValue() )
    {
        if (outputControl->sendTargetValue( newTarget ))
        {
            outputControl->setTargetValue(newTarget);
            ctrlloopState = kIOPCtrlLoopAllRegistered;
        }
        else
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::sendNewTarget failed to send target value to first control\n");
        }

        if (secondOutputControl->sendTargetValue( newTarget ))
        {
            secondOutputControl->setTargetValue(newTarget);
            ctrlloopState = kIOPCtrlLoopAllRegistered;
        }
        else
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::sendNewTarget failed to send target value to second control\n");
        }

        if (thirdOutputControl->sendTargetValue( newTarget ))
        {
            thirdOutputControl->setTargetValue(newTarget);
            ctrlloopState = kIOPCtrlLoopAllRegistered;
        }
        else
        {
            CTRLLOOP_DLOG("RackMac3_1_CPUBUniFanCtrlLoop::sendNewTarget failed to send target value to third control\n");
        }
    }
}

void RackMac3_1_CPUBUniFanCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
    //CTRLLOOP_DLOG("RackMac3_1_CPUFanCtrlLoop::sensorRegistered - entered\n");
    
    if( (outputControl->isRegistered() == kOSBooleanTrue) &&
		(secondOutputControl->isRegistered() == kOSBooleanTrue) &&
		(thirdOutputControl->isRegistered() == kOSBooleanTrue) )
    {
        //CTRLLOOP_DLOG("RackMac3_1_CPUFanCtrlLoop::sensorRegistered allRegistered!\n");

        ctrlloopState = kIOPCtrlLoopFirstAdjustment;

        // set the deadline
        deadlinePassed();
    }
}

void RackMac3_1_CPUBUniFanCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
    //CTRLLOOP_DLOG("RackMac3_1_CPUFanCtrlLoop::controlRegistered - entered\n");
    
    if( (outputControl->isRegistered() == kOSBooleanTrue) &&
		(secondOutputControl->isRegistered() == kOSBooleanTrue) &&
		(thirdOutputControl->isRegistered() == kOSBooleanTrue) )
    {
        //CTRLLOOP_DLOG("RackMac3_1_CPUFanCtrlLoop::controlRegistered allRegistered!\n");

        ctrlloopState = kIOPCtrlLoopFirstAdjustment;

        // set the deadline
        deadlinePassed();
    }
}
