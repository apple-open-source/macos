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
 * Copyright (c) 2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PowerMac12_1_CPUFanCtrlLoop.h,v 1.5 2005/09/29 21:34:23 mpontil Exp $
 *
 */


#ifndef _POWERMAC12_1_CPUFANCTRLLOOP_H
#define _POWERMAC12_1_CPUFANCTRLLOOP_H


#include "IOPlatformPIDCtrlLoop.h"
#include "SMU_Neo2_PIDCtrlLoop.h"
#include "SMU_Neo2_PlatformPlugin.h"
#include "SMU_Neo2_CPUFanCtrlLoop.h"

// Keeps track of the weighted average paramter:
#define kWeightedAverageKey		"Weighted-Average-Parameter"
#define kWeightedAverageDefault	100

class PowerMac12_1_CPUFanCtrlLoop : public SMU_Neo2_CPUFanCtrlLoop
	{
	OSDeclareDefaultStructors( PowerMac12_1_CPUFanCtrlLoop )

private:
	// Keeps track of the average parameter and the last iteration:
	UInt16			mWeightedAverage;
	SensorValue		mPreviousWeightedAverage;

protected:

	virtual			ControlValue	calculateNewTarget( void ) const;

public:
	virtual			IOReturn		initPlatformCtrlLoop(const OSDictionary *dict);
	virtual			bool			cacheMetaState( const OSDictionary* metaState );
	virtual			void			sendNewTarget( ControlValue newTarget );
	virtual			void			controlRegistered( IOPlatformControl* aControl );
	virtual			SensorValue		averagePower();
};

