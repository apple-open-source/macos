/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
 *  File: $Id: SMU_Neo2_CPUFanCtrlLoop.h,v 1.7 2004/07/30 00:17:29 dirty Exp $
 *
 */


#ifndef _SMU_NEO2_CPUFANCTRLLOOP_H
#define _SMU_NEO2_CPUFANCTRLLOOP_H


#include "IOPlatformPIDCtrlLoop.h"

#include "SMU_Neo2_PlatformPlugin.h"


// keys for CPU Fan PID parameters

#define kIOPPIDCtrlLoopMaxPowerKey				"power-max"
#define kIOPPIDCtrlLoopMaxPowerAdjustmentKey	"power-max-adjustment"


class SMU_Neo2_CPUFanCtrlLoop : public IOPlatformPIDCtrlLoop
	{
	OSDeclareDefaultStructors( SMU_Neo2_CPUFanCtrlLoop )

protected:

	IOPlatformSensor*		powerSensor;

		// Q78 has two CPU fans which should be driven exactly the same.
		// In this class, if we detect a second control in ControlIDsArray,
		// then we will treat it as the linked control.

		// Even though they are identical, then can have differing min/max values.

	ControlValue			targetValue;

	IOPlatformControl*		linkedControl;
	ControlValue			linkedControlOutputMin;
	ControlValue			linkedControlOutputMax;

	ControlValue			minMinimum;
	ControlValue			maxMaximum;

		// Dedicated temperature sample buffer -- historyArray will be used for power readings.

	SensorValue				tempHistory[ 2 ];
	int						tempIndex;

	SensorValue				powerMaxAdj;

	SensorValue				inputMax;
	SensorValue				inputTargetDelta;

		// If the output control has a "safe" speed, a new meta state will be created and the
		// new meta state's index will be stored in safeMetaStateIndex

	unsigned int			safeMetaStateIndex;

		virtual			bool			init( void );
		virtual			void			free( void );

		virtual			bool			cacheMetaState( const OSDictionary* metaState );

		virtual			OSDictionary*	getPIDDataset( const OSDictionary* dict ) const;

						bool			acquireSample( void );

		virtual			SensorValue		calculateDerivativeTerm( void ) const;
		virtual			ControlValue	calculateNewTarget( void ) const;

public:

		virtual			IOReturn		initPlatformCtrlLoop( const OSDictionary* dict );
		virtual			bool			updateMetaState( void );
		virtual			void			deadlinePassed( void );

		virtual			void			sendNewTarget( ControlValue newTarget );
		virtual			void			sensorRegistered( IOPlatformSensor* aSensor );
		virtual			void			controlRegistered( IOPlatformControl* aControl );
	};


#endif	// _SMU_NEO2_CPUFANCTRLLOOP_H
