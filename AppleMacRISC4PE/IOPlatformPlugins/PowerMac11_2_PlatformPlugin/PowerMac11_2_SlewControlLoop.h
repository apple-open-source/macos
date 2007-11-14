/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _POWERMAC11_2_SLEWCONTROLLOOP_H
#define _POWERMAC11_2_SLEWCONTROLLOOP_H


#include "IOPlatformSlewClockControl.h"
#include "IOPlatformCtrlLoop.h"

#define kVoltageSlewControl			"platform-set-vdnap0"
#define kVoltageSlewState			"platform-get-vdnap0"
#define kVoltageSlewComplete		"platform-slewing-done"
#define kAAPLPHandle				"AAPL,phandle"
#define kIOPFInterruptRegister      "IOPFInterruptRegister"

#define kVOLTAGE_NOT_SLEWING		0
#define kVOLTAGE_SLEWING_ON			1
#define kPROCESSOR_STEPPING			1
#define kPROCESSOR_FULL_SPEED		0

#define kGPIO_VOLTAGE_SLEW_OFF		1
#define kGPIO_VOLTAGE_SLEW_ON		0

#define kENV_LOW_SPEED_REQUESTED	1
#define kENV_HIGH_SPEED_REQUESTED	0

class PowerMac11_2_SlewCtrlLoop : public IOPlatformCtrlLoop
{
	OSDeclareDefaultStructors( PowerMac11_2_SlewCtrlLoop )

protected:

	// flags to tell us if all the prerequisites have been met in order to perform
	// each type of operation

	bool					_canStepProcessors;
	bool					_canSlewProcessors;

	unsigned int			_currentProcessorStepSetting;
	unsigned int			_currentVoltageSlewSetting;

	volatile bool			_waitingForSlewCompletion;
	volatile bool			_slewOperationComplete;


	// platform-function symbols for voltage control
	const OSSymbol			* _CpuVoltageSlewControlSym;
	const OSSymbol			* _CpuVoltageSlewStateSym;
	const OSSymbol			* _CpuVoltageSlewingCompleteSym;


	// platform-function services for voltage control
	IOService				* _CpuVoltageSlewControl;		// used to call get/set VDNAP0
	IOService				* _CpuVoltageSlewComplete;		// used to contact GPIO to tell when slewing is complete


	virtual		bool			init( void );
	virtual		void			free( void );

				bool			setProcessorStepIndex( unsigned int stepIndex );
				bool			setVoltageSlewIndex( unsigned int slewTarget );

				void			setupSlewing( void );
				bool			setInitialState( void );

				bool			getCurrentVoltageSlewIndex( unsigned int * );

				bool			adjustStepControls( unsigned int stepTarget );
				bool			adjustSlewVoltControls( unsigned int stepTarget );
				bool			adjustSlewStepVoltControls( unsigned int stepTarget );
				bool			adjustSlewAndStepControls( unsigned int stepTarget, unsigned int slewTarget );

public:

	virtual		IOReturn		initPlatformCtrlLoop(const OSDictionary *dict);

	virtual		bool			updateMetaState( void );
	virtual		void			adjustControls( void );

	// static GPIO interrupt callback routine
	static		void			slewIsCompleteCallback( void * param1, void * param2, void * param3, void * param4 );

	virtual		void			willSleep( void );
	virtual		void			didWake( void );
	};


#endif	// _POWERMAC11_2_SLEWCONTROLLOOP_H
