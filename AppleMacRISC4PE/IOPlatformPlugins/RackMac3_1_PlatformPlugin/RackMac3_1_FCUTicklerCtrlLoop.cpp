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

#include "RackMac3_1_FCUTicklerCtrlLoop.h"

#define super IOPlatformCtrlLoop
OSDefineMetaClassAndStructors( RackMac3_1_FCUTicklerCtrlLoop, IOPlatformCtrlLoop )

void RackMac3_1_FCUTicklerCtrlLoop::didWake( void )
{
	super::didWake();

	clock_absolutetime_interval_to_deadline(interval, &deadline);
}

IOReturn RackMac3_1_FCUTicklerCtrlLoop::initPlatformCtrlLoop( const OSDictionary *dict)
{
	IOReturn status;
	const OSArray * array;
	int i;
	
	status = super::initPlatformCtrlLoop(dict);

	// populate list of controls to "tickle"
	array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey));
	if (array == NULL)
	{
		CTRLLOOP_DLOG("RackMac3_1_FCUTicklerCtrlLoop::initPlatformCtrlLoop no control ID array!!\n");
		goto failNoControl;
	}

	controlIDCount = array->getCount();

	for(i = 0; i < controlIDCount; i++)
	{
		tickleChannel[i] = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(i)));
		if(tickleChannel[i])
		{
			tickleChannel[i]->retain();
			addControl( tickleChannel[i] );
		}
	}

	// populate list of sensors to "update"
	array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalSensorIDsKey));
	if (array == NULL)
	{
		CTRLLOOP_DLOG("RackMac3_1_FCUTicklerCtrlLoop::initPlatformCtrlLoop no sensor ID array!!\n");
		goto failNoControl;
	}

	sensorIDCount = array->getCount();

	for(i = 0; i < sensorIDCount; i++)
	{
		updateChannel[i] = platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(i)));

		if(updateChannel[i])
		{
			updateChannel[i]->retain();
			addSensor( updateChannel[i] );
		}
	}

	clock_interval_to_absolutetime_interval(10, NSEC_PER_SEC, &interval);	// 10 seconds

	clock_absolutetime_interval_to_deadline(interval, &deadline);

	return status;

failNoControl:

	return(kIOReturnError);
}

void RackMac3_1_FCUTicklerCtrlLoop::deadlinePassed( void )
{
	int i;
	
	// Get/Set current control value
	for(i = 0; i < controlIDCount; i++)
	{
		if(tickleChannel[i] != NULL)
		{
			tickleChannel[i]->setCurrentValue( tickleChannel[i]->forceAndFetchCurrentValue() );
		}
	}

	// Set current sensor value
	for(i = 0; i < sensorIDCount; i++)
	{
		if(updateChannel[i] != NULL)
		{
			// Here we do a fetchCurrentValue() instead of a forceAndFetchCurrentValue().
			// This is because these sensor channels are all known to have non-zero
			// polling periods set -- we don't need to send a force-update message because
			// IOHWSensor is already polling.  Just read IOHWSensor's current-value,
			// apply the transform and store the value in the dictionary for consumers to
			// grab.
			updateChannel[i]->setCurrentValue( updateChannel[i]->fetchCurrentValue() );
		}
	}
	
	// Set new deadline
	clock_absolutetime_interval_to_deadline(interval, &deadline);
}
