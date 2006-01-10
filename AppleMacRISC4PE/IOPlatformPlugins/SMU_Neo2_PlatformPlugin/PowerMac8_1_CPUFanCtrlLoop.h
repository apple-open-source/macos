/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PowerMac8_1_CPUFanCtrlLoop.h,v 1.2 2004/08/10 01:27:58 dirty Exp $
 *
 */


#ifndef _POWERMAC8_1_CPUFANCTRLLOOP_H
#define _POWERMAC8_1_CPUFANCTRLLOOP_H


#include "IOPlatformPIDCtrlLoop.h"

#include "SMU_Neo2_PlatformPlugin.h"
#include "PowerMac8_1_SystemFansCtrlLoop.h"
#include "SMU_Neo2_CPUFanCtrlLoop.h"


class PowerMac8_1_SystemFansCtrlLoop;

class PowerMac8_1_CPUFanCtrlLoop : public SMU_Neo2_CPUFanCtrlLoop
	{
	OSDeclareDefaultStructors( PowerMac8_1_CPUFanCtrlLoop )

protected:

	PowerMac8_1_SystemFansCtrlLoop*			systemFansCtrlLoop;

	SInt32									scalingFactorC;
	SInt32									constantD;

public:

		virtual			IOReturn		initPlatformCtrlLoop( const OSDictionary* dict );

		virtual			void			sendNewTarget( ControlValue newTarget );

		virtual			ControlValue	getCtrlLoopTarget( void ) const;
	};


#endif	// _POWERMAC8_1_CPUFANCTRLLOOP_H
