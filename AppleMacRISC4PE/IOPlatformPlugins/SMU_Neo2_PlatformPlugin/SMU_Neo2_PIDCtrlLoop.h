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
 *  File: $Id: SMU_Neo2_PIDCtrlLoop.h,v 1.4.6.2 2004/07/30 00:26:24 dirty Exp $
 *
 */


#ifndef _SMU_NEO2_PIDCTRLLOOP_H
#define _SMU_NEO2_PIDCTRLLOOP_H


#include "IOPlatformPIDCtrlLoop.h"

#include "SMU_Neo2_PlatformPlugin.h"


class SMU_Neo2_PIDCtrlLoop : public IOPlatformPIDCtrlLoop
	{
	OSDeclareDefaultStructors( SMU_Neo2_PIDCtrlLoop )

protected:

		// In this class, if we detect a second control in ControlIDsArray,
		// then we will treat it as the linked control.

		// Even though they are identical, then can have differing min/max values.

	ControlValue			targetValue;

	IOPlatformControl*		linkedControl;
	ControlValue			linkedControlOutputMin;
	ControlValue			linkedControlOutputMax;

	ControlValue			minMinimum;
	ControlValue			maxMaximum;

		virtual			void			free( void );

		virtual			bool			cacheMetaState( const OSDictionary* metaState );

		virtual			ControlValue	calculateNewTarget( void ) const;
		
		// Whether this control is a type that wants a delta from it's current value (RPM fans), or
		// requires a direct target value (PWM fans). This should really be incorporated into IOPlatformPIDCtrlLoop
		// so we can just use it's calculateNewTarget.
		
		bool			isDirectControlType;
		unsigned int	safeMetaStateIndex;

public:
		virtual			IOReturn		initPlatformCtrlLoop(const OSDictionary *dict);
		virtual			bool			updateMetaState( void );

		virtual			void			sendNewTarget( ControlValue newTarget );
		virtual			void			controlRegistered( IOPlatformControl* aControl );
	};


#endif	// _SMU_NEO2_PIDCTRLLOOP_H
