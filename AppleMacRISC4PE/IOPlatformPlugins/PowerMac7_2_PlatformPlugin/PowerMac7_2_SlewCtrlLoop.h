/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _POWERMAC7_2_SLEWCTRLLOOP_H
#define _POWERMAC7_2_SLEWCTRLLOOP_H

#include "IOPlatformSlewClockControl.h"
#include "IOPlatformCtrlLoop.h"

/*!	@class PowerMac7_2_CPUFanCtrlLoop
	@abstract This class implements a PID-based fan control loop for use on PowerMac7,2 machines.  This control loop is designed to work for both uni- and dual-processor machines.  There are actually two (mostly) independent control loops on dual-processor machines, but control is implemented in one control loop object to ease implementation of tach-lock. */

class PowerMac7_2_SlewCtrlLoop : public IOPlatformCtrlLoop
{

	OSDeclareDefaultStructors(PowerMac7_2_SlewCtrlLoop)

protected:

	IOPlatformSlewClockControl * slewControl;

	// We have to control napping of the processor(s).  This variable tells us what was the last
	// thing we programmed, so we don't have to calls this too often.
	bool lastAllowedNapping;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );

public:

	virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);
	virtual bool updateMetaState( void );
	virtual void adjustControls( void );

	virtual void controlRegistered( IOPlatformControl * aControl );
};

#endif	// _POWERMAC7_2_SLEWCTRLLOOP_H
