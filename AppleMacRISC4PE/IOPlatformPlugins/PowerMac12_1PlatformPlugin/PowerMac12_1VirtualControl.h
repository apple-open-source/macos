/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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


#ifndef _PM12_1VIRTUALCONTROL_H
#define _PM12_1VIRTUALCONTROL_H

#include "IOPlatformControl.h"

#define	kPowerMac12_1TargetControlID	"TargetControlID"

class PowerMac12_1_CPUFanCtrlLoop;

/*!
	@class IOPlatformControl
	@abstract An internal abstraction of a device controller entity
	@discussion The platform plugin uses this class (and derived classes) internally to represent a device controller entity.  Basic functionality includes registering for messages from the controller object (used for failure notification) and sending commands to the controller. */

class PowerMac12_1VirtualControl : public IOPlatformControl
{

	OSDeclareDefaultStructors(PowerMac12_1VirtualControl)

protected:
#define	kMaxSetOfControlLoops 4

	typedef struct RegistredControlLoop
	{
		IOPlatformCtrlLoop	*ctrlLoop;
		bool				hasActiveValue;
		ControlValue		currentActiveValue;
		UInt32				referenceParameter;
	} RegistredControlLoop;

	RegistredControlLoop mRegistredControlLoopsArray[kMaxSetOfControlLoops];
	int			mActualNumberOfRegistredControlLoops;
	
	/*! @var chosenTarget The currently chosen target value.  */
	ControlValue mChosenTarget;
	
	/*! @var targetOwner The control loop that was chosen for the target value.  */
	IOPlatformCtrlLoop *mChosenTargetOwner;

	/*! @var mNumberOfConnectedControlLoops counter for the number of registred values  */
	int mNumberOfConnectedControlLoops;
	
	/*! @var pmmutex this object is very simple, but changes in it need to be atomic. So this lock ensures the atomicity.  */
	IORecursiveLock *mVirtualControlLock;

	/*! @var hwControl this is the real control.  */
	IOPlatformControl *mHWControl;
	
	/*! @var rubberBandControl this is the control (if defined) that corrects out output.  */
	IOPlatformControl *	mRubberBandControl;
	SInt32				mControlOffset;		// Fixed 16.16
	SInt32				mControlSlope;		// Fixed 16.16

	// From Bob Ridenour:
	// The following limits the min output of the system using a linear calculation
	// from a given integral term of a control loop:
	PowerMac12_1_CPUFanCtrlLoop * mReferenceControlLoop;
	SInt32				mControlLoopOffset;		// Fixed 16.16
	SInt32				mControlLoopSlope;		// Fixed 16.16

	// overrides from OSObject superclass
	virtual void free( void );
	virtual bool init( void );
	
	/*! @function privateSetControlTargetValue
		@abstract stes its target value and the target value of the target control. */
	virtual IOReturn privateSetControlTargetValue( void );
	
public:

	/*!	@function initPlatformControl 
		@abstract Initialize an IOPlatformControl object from its ControlArray dict as included in the IOPlatformThermalProfile.
		@param dict The thermal profile dictionary for this control
	*/
	
	virtual IOReturn				initPlatformControl( const OSDictionary *dict );

	/*!	@function initPlatformControl 
		@abstract Initialize an IOPlatformControl object from a control driver that is attempting to register
		@param unknownControl An IOService reference to the driver attempting to register
		@param dict The OSDictionary that the control sent with the registration message
	*/
	virtual IOReturn				initPlatformControl( IOService * unknownControl, const OSDictionary * dict );

	/*! @function isManaged
		@abstract Boolean to tell whether the platform plugin is managing this control.  Some controls are only represented so that they can report errors/failures. */
	virtual bool					isManaged( void );

	/*! @function isRegistered
		@abstract Tells whether there is an IOService * associated with this control. */
	virtual OSBoolean *				isRegistered( void );

	/*!	@function registerDriver
		@abstract Used to associated an IOService with this control.
		@param driver the driver that is attempting to register
		@param dict the registration dictionary passed up by the driver
	*/
	virtual IOReturn				registerDriver( IOService * driver, const OSDictionary * dict, bool notify = true );

	/*! @function joinedCtrlLoop
		@abstract Called by IOPlatformCtrlLoop when a control is added.
		@param aCtrlLoop The IOPlatformCtrlLoop* referencing the control loop. */
	virtual bool					joinedCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop );

	/*! @function leftCtrlLoop
		@abstract Called by IOPlatformCtrlLoop when a control is removed from a loop.
		@param aCtrlLoop The control loop that this control is leaving. */
	virtual bool					leftCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop );
	
	// accessors - this just grabs attributes from the info dict
	// version
	virtual OSNumber *				getVersion( void );

	// type
	virtual OSString *				getControlType( void );
	virtual UInt32					getControlTypeID( void );

	// zone
	virtual OSData *				getControlZone( void );

	// location
	virtual OSString *				getControlLocation( void );

	// Desc-Key
	virtual OSString *				getControlDescKey( void );

	// control-flags
	virtual OSNumber *				getControlFlags( void );

	// Generic value accessor routine
	virtual ControlValue			getValue( const OSSymbol * key, ControlValue defaultValue );
	virtual void					setValue( const OSSymbol * key, ControlValue newValue );

	// min-value, max-value -- these may not be used for all controls
	virtual ControlValue			getControlMinValue( void );
	virtual ControlValue			getControlMaxValue( void );
	virtual ControlValue			getControlSafeValue( void );
	virtual	void					setControlMinValue( ControlValue );
	virtual	void					setControlMaxValue( ControlValue );
	virtual	void					setControlSafeValue( ControlValue );

	// current-value
	virtual ControlValue			getCurrentValue( void ); // reads current-value from infoDict
	virtual void					setCurrentValue( ControlValue newValue ); // sets current-value in infoDict
	virtual ControlValue			fetchCurrentValue( void );

	/*!	@function sendForceUpdateMessage
		@abstract Formulates and sends a force-update message to the control.  This causes the IOHWControl driver to poll the hardware and update it's current-value. */
	virtual IOReturn				sendForceUpdate( void );

	/*! @function forceAndFetchCurrentValue
		@abstract Same as fetchCurrentValue(), except calls sendForceUpdate() before fetching, transforming and storing the current-value.
	*/
	virtual ControlValue			forceAndFetchCurrentValue( void );	// blocks for I/O

	// target-value
	virtual ControlValue			getTargetValue(void);
	virtual void					setTargetValue( ControlValue target );
	
	// this send the target-value stored in infoDict to the control
	// if forced == true, this is a forced value from setProperties
	virtual bool					sendTargetValue( ControlValue target, bool forced = false ); // this can block !!

	// accessor for controlDriver member variable
	virtual IOService *				getControlDriver(void);
	
	// replacement for the setTargetValue:
	virtual void					setTargetValue( ControlValue target, IOPlatformCtrlLoop *settingCtrlLoop, UInt32 referenceParameter);
	
	// returns a reference to the controlled control (!?)
	virtual IOPlatformControl*		targetControl();
	
	// Using the given control loop average adjust the low bound of the target control
	virtual IOReturn				initRubberControlConnection();
	virtual IOReturn				initControlLoopConnection( IOPlatformCtrlLoop * aCtrlLoop );
	virtual ControlValue			adjustLowTargetControl(ControlValue newTarget);
	
	// Control Rubberbanding:
	virtual	ControlValue			rubberBanding(ControlValue newTarget);
};
#endif /* _PM12_1VIRTUALCONTROL_H */
