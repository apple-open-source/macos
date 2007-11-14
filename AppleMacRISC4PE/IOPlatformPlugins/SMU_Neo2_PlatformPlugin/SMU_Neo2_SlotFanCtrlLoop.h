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
 *  File: $Id: SMU_Neo2_SlotFanCtrlLoop.h,v 1.2 2004/07/19 21:10:19 eem Exp $
 *
 */


#ifndef _SMU_NEO2_SLOTFANCTRLLOOP_H
#define _SMU_NEO2_SLOTFANCTRLLOOP_H

#include "IOPlatformPIDCtrlLoop.h"
#include "SMU_Neo2_PlatformPlugin.h"

class SMU_Neo2_SlotFanCtrlLoop : public IOPlatformPIDCtrlLoop
	{
	OSDeclareDefaultStructors( SMU_Neo2_SlotFanCtrlLoop )

private:

	/*!
		@function populateMetaStates
		@abstract Helper method for extracting fan min/max/safe values from Control registration
		dictionary.
	*/
						void				populateMetaStates( IOPlatformControl* newControl,
																OSDictionary* normalMetaStateDict,
																OSDictionary* failsafeMetaStateDict,
																OSDictionary** safeMetaStateDict );

protected:
	/*!
		@var activeAGPControl If true, then a valid AGP Sensor and AGP Control have registered. In this
		case it is assumed that agpSensor and agpControl are non-NULL and the input-target, output-max
		and output-min have been populated in the AGP Card Control meta state (at index 2). If false,
		standard PCI power measurements will drive the control algorithm.
	*/
						bool				activeAGPControl;

						IOPlatformSensor*	agpSensor;
						IOPlatformControl*	agpControl;

	/*!
		@var pciFanMinWithAGP The PCI chamber fan's minimum speed when in AGP Card Control mode. This is
		different than the minimum speed used for traditional PCI power based fan control. The value is
		fetched from the pci-fan-min entry in the thermal profile CtrlLoopArray entry for this control loop.
	*/
						ControlValue		pciFanMinWithAGP;

	/*!
		@var pciFanMaxWithAGP The PCI chamber fan's maximum speed when in AGP Card Control mode. This is
		different than the maximum speed used for traditional PCI power based fan control. The value is
		fetched from the pci-fan-max entry in the thermal profile CtrlLoopArray entry for this control loop.
	*/
						ControlValue		pciFanMaxWithAGP;
	/*!
		@var pciFanSafe The PCI chamber fan's "safe", or door open, speed.
	*/
						ControlValue		pciFanSafe;

						unsigned int		safeMetaStateIndex;
						unsigned int		agpSafeMetaStateIndex;

		virtual			bool				init( void );
		virtual			bool				cacheMetaState( const OSDictionary* metaState );
		virtual			ControlValue		calculateNewTarget( void ) const;
		virtual			SensorValue			getAggregateSensorValue( void );

public:
		virtual			bool				updateMetaState( void );
		virtual			void				sendNewTarget( ControlValue newTarget );
		virtual			void				sensorRegistered( IOPlatformSensor* aSensor );
		virtual			void				controlRegistered( IOPlatformControl* aControl );
	};


#endif	// _SMU_NEO2_PIDCTRLLOOP_H
