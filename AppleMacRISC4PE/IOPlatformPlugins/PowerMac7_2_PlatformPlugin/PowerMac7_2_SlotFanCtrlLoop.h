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
 *
 */


#ifndef _POWERMAC7_2_SLOTFANCTRLLOOP_H
#define _POWERMAC7_2_SLOTFANCTRLLOOP_H

#include "IOPlatformPIDCtrlLoop.h"

class PowerMac7_2_SlotFanCtrlLoop : public IOPlatformPIDCtrlLoop
{

	OSDeclareDefaultStructors(PowerMac7_2_SlotFanCtrlLoop)

protected:
	/*!
		@var activeAGPControl If true, then a valid AGP Sensor and AGP Control have registered. In this
		case it is assumed that agpSensor and agpControl are non-NULL and the input-target, output-max
		and output-min have been populated in the AGP Card Control meta state (at index 2). If false,
		standard PCI power measurements will drive the control algorithm.
	*/
	bool					activeAGPControl;

	IOPlatformSensor*		agpSensor;
	IOPlatformControl*		agpControl;

	ControlValue			pciFanMin;
	ControlValue			pciFanMax;

	virtual ControlValue	calculateNewTarget( void ) const;

	virtual bool			init( void );

	virtual SensorValue		getAggregateSensorValue( void );

public:

	virtual bool updateMetaState( void );
	virtual void sendNewTarget( ControlValue newTarget );

	virtual void sensorRegistered( IOPlatformSensor * aSensor );
	virtual void controlRegistered( IOPlatformControl * aControl );

};

#endif // _POWERMAC7_2_SLOTFANCTRLLOOP_H
