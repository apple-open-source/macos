/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _PowerMac7_2_U3TwinsPIDCtrlLoop_H
#define _PowerMac7_2_U3TwinsPIDCtrlLoop_H

#include "PowerMac7_2_PIDCtrlLoop.h"

class PowerMac7_2_U3TwinsPIDCtrlLoop : public PowerMac7_2_PIDCtrlLoop
{

	OSDeclareDefaultStructors(PowerMac7_2_U3TwinsPIDCtrlLoop)

protected:

public:


#define kPM72PID_DEFAULT_tDiodeSamples	30		// Default number of samples.

	// U3 thermal diode history data...
	SInt32		*tDiodeHistory;	// Sample history array.
	SInt32		tDiodeIndex;	// Current sample index.
	SInt32		tDiodeSamples;	// Number of samples in history array.
	SInt32		tDiodeMinimum;	// Current sample minimum. (min(tDiodeHistory[0..tDiodeSamples-1]))
	SInt64		outputTarget;	// 28.36 fixed-point output target value.

	virtual void adjustControls( void );
	virtual ControlValue calculateNewTarget( void ); // non-const overload for saving outputTarget

	virtual void deadlinePassed( void );
	bool acquireSample( void );		// gets a sample (using clock_get_uptime() and getAggregateSensorValue()) and stores it at 

	virtual IOReturn initPlatformCtrlLoop( const OSDictionary *dict);
};

#endif	// _PowerMac7_2_U3TwinsPIDCtrlLoop_H
