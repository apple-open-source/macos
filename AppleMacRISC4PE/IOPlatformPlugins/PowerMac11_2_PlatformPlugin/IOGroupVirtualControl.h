/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _IOGROUPVIRTUALCONTROL_H
#define _IOGROUPVIRTUALCONTROL_H

#include "IOPlatformControl.h"

/*!
	@class IOGroupVirtualControl
	@abstract An internal abstraction of a device controller entity
	@discussion The platform plugin uses this class (and derived classes) internally to represent a device controller entity.  Basic functionality includes registering for messages from the controller object (used for failure notification) and sending commands to the controller. */

// Note: do not be tempted to inherit from IOPlatformControl. Doing so would imply things that are not true and induce behaviors
// and bugs that would be hard to debug. However keep the names of the API similar to the ones of the controls (not identical
// however) so that the developer is immedialety familiar with their behavior.

#define kMaxNegativeFanSpeed	300

class IOGroupVirtualControl : public OSObject
{

	OSDeclareDefaultStructors(IOGroupVirtualControl)

protected:

#define kMaxSetOfControls	2

	IOPlatformControl* intakeFans[kMaxSetOfControls];
	IOPlatformControl* exaustFans[kMaxSetOfControls];
	IOPlatformControl* pumps[kMaxSetOfControls];
	
	int mActualNumberOfIntakeFans;
	int mActualNumberOfExaustFans;
	int mActualNumberOfPumps;

#define	kMaxSetOfControlLoops 4

	typedef struct RegistredControlLoop
	{
		IOPlatformCtrlLoop	*ctrlLoop;
		bool				hasActiveValue;
		ControlValue		currentActiveValue;
		SInt32				referenceParameter;
	} RegistredControlLoop;

	RegistredControlLoop mRegistredControlLoopsArray[kMaxSetOfControlLoops];
	int			mActualNumberOfRegistredControlLoops;
	
	// The folloring value limits the delta of fan speed when the fans tend to go slower.
	UInt32	mLimitingLowerDelta;
	
	// When this trad call is created the controls are updated at regular intevals of kRegularIntervalInMilliseconds
#define	kRegularIntervalInMilliseconds 1000
	thread_call_t	mTimedThreadCallToUpdateControls;

	/*! @var chosenTarget The currently chosen target value.  */
	ControlValue mChosenTarget;
	
	/*! @var targetOwner The control loop that was chosen for the target value.  */
	IOPlatformCtrlLoop *mChosenTargetOwner;

	/*! @var mNumberOfConnectedControlLoops counter for the number of registred values  */
	int mNumberOfConnectedControlLoops;
	
	/*! @var nextCandidateOwner The control loop that could chosen for the target value if the current owner is not valid.  */
	int mNumberOfControlLoopsThatSetAValue;

	/*! @var pmmutex this object is very simple, but changes in it need to be atomic. So this lock ensures the atomicity.  */
	IORecursiveLock *mVirtualControlLock;

public:
	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );
	
	// Returns how many REAL pumps there are in the system
	UInt16 numberOfPumpsInThisSystem();

	// Scaling / Transformations
	virtual ControlValue			setIntakeFanValueFromControlValue(ControlValue newValue, ControlValue maxValue, IOPlatformControl *intakeFan);
	virtual ControlValue			setExaustFanValueFromControlValue(ControlValue newValue, ControlValue maxValue, IOPlatformControl *exaustFan);
	virtual ControlValue			setPumpValueFromControlValue(ControlValue newValue, ControlValue maxValue, IOPlatformControl *pump);
	
	// Program the new target in this control set:
	virtual void					programSetWithValue(ControlValue newTargetValue);

	/*! @function registerControlLoop
		@abstract registers a control Loop with this virtual control. */
	virtual IOReturn				registerControlLoop( IOPlatformCtrlLoop *newControlLoop);

	/*! @function addControlSet
		@abstract Adds a set of controls (intake/exaust/pump) to the existing set. */
	virtual IOReturn				addIntakeFan( IOPlatformControl *intakeFan );
	virtual IOReturn				addExaustFan( IOPlatformControl *exaustFan );
	virtual IOReturn				addPump( IOPlatformControl *pump );

	/*! @function setDecresingLimitingFactor
		@abstract sets a new value for the limiting factor of how the fans can decrease */
	virtual IOReturn				setDecresingLimitingFactor( UInt32 newValue );

	/*! @function isGroupManaged
		@abstract Boolean to tell whether the platform plugin is managing these controls.  Some controls are only represented so that they can report errors/failures. */
	virtual bool					isGroupManaged( void );

	/*! @function isRegistered
		@abstract Tells whether there is an IOService * associated with these controls. */
	virtual OSBoolean *				isGroupRegistered( void );

	// target-value
	virtual ControlValue			getTargetValue( IOPlatformCtrlLoop **setter = NULL );
	virtual void					setTargetValue( ControlValue target, IOPlatformCtrlLoop *settingCtrlLoop, SInt32 referenceParameter);

	/*! @function UpdateControlsWithCurrentTargetValue
		@abstract reads the current candidate value and makes it the current target value. */	
	virtual void					UpdateControlsWithCurrentTargetValue();
};

#endif //_IOGROUPVIRTUALCONTROL_H