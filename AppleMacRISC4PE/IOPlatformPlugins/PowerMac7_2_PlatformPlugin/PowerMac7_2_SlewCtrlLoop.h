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
//		$Log: PowerMac7_2_SlewCtrlLoop.h,v $
//		Revision 1.4  2003/06/25 02:16:25  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.3.4.1  2003/06/20 01:40:01  eem
//		Although commented out in this submision, there is support here to nap
//		the processors if the fans are at min, with the intent of keeping the
//		heat sinks up to temperature.
//		
//		Revision 1.3  2003/06/07 01:30:58  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.2.4.4  2003/05/31 08:11:38  eem
//		Initial pass at integrating deadline-based timer callbacks for PID loops.
//		
//		Revision 1.2.4.3  2003/05/29 03:51:36  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.2.4.2  2003/05/26 10:07:17  eem
//		Fixed most of the bugs after the last cleanup/reorg.
//		
//		Revision 1.2.4.1  2003/05/23 06:36:59  eem
//		More registration notification stuff.
//		
//		Revision 1.2  2003/05/13 02:13:52  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.1.2.1  2003/05/12 11:21:12  eem
//		Support for slewing.
//		
//
//

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