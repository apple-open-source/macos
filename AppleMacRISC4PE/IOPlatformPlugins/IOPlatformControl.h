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


#ifndef _IOPLATFORMCONTROL_H
#define _IOPLATFORMCONTROL_H

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include "IOPlatformPluginDefs.h"

#ifdef CONTROL_DLOG
#undef CONTROL_DLOG
#endif

// Uncomment for debug info
// #define CONTROL_DEBUG 1

#ifdef CONTROL_DEBUG
#define CONTROL_DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define CONTROL_DLOG(fmt, args...)
#endif

enum {
	kIOPControlTypeUnknown,
	kIOPControlTypeSlew,
	kIOPControlTypeFanRPM,
	kIOPControlTypeFanPWM
};

enum {

	/* some controls are not managed by the plugin, but we still need to
	   know about them so we can register for their failure events.  Setting
	   this flag tells the plugin not to attempt to send commands to this
	   control */
	IOPControlFlagExternallyManaged		= 0x1
};

class IOPlatformCtrlLoop;

/*!
	@class IOPlatformControl
	@abstract An internal abstraction of a device controller entity
	@discussion The platform plugin uses this class (and derived classes) internally to represent a device controller entity.  Basic functionality includes registering for messages from the controller object (used for failure notification) and sending commands to the controller. */

class IOPlatformControl : public OSObject
{

	OSDeclareDefaultStructors(IOPlatformControl)

protected:
	/*! @var infoDict A dictionary containing up-to-date info for publishing in the registry.  At the least this should contain control-id, version, location, type, zone, current-value and target-value keys */
	OSDictionary *	infoDict;

	/*! @var ctrlDriver A pointer to the device driver that implements this controller */
	IOService *		controlDriver;

	/*! @var ctrlLoops An array of IOPlatformCtrlLoops that this control acts as an output for.  */
	OSArray *		ctrlLoops;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual bool initSymbols( void );
	virtual void free( void );

	/* translate between plugin and HW value representations.  Note that current-value and target-value might not represent the same thing.  For example, FCU PWM channels accept/report their target value as a PWM tick value in the range [0-255] but report their current value as an integer RPM.  Here we have a method to convert current-value from HW->plugin, and to convert target from HW->plugin and from plugin->HW. */
	virtual ControlValue	 		applyCurrentValueTransform( ControlValue hwReading );
	virtual ControlValue			applyTargetValueTransform( ControlValue hwReading );
	virtual ControlValue			applyTargetValueInverseTransform( ControlValue pluginReading );

	// notify the control loops that we've got a registered driver
	virtual void notifyCtrlLoops( void );

public:

	/*
	 * Notes on initPlatformControl( const OSDictionary * ), initPlatformControl( IOService * ),
	 * and registerDriver( IOSerivce *, const OSDictionary *, bool )
	 *
	 * These routines are cumulatively responsible for initializing an IOPlatformControl object
	 * and making sure that all the correct data is supplied, parsed, and prioritized.  There
	 * are a number of properties inside a control.  Some can be declared by the thermal
	 * profile, some can be supplied by the registering driver, and some, if left unspecified,
	 * will be set to some default value.
	 *
	 * initPlatformControl( const OSDictionary * ) is invoked when a control is being initialized
	 * from a thermal profile entry.  The OSDictionary parameter is the thermal profile entry.
	 *
	 * initPlatformControl( IOService * ) is invoked when a control driver tries to register and
	 * its control-id is not found in the thermal profile.  The IOService parameter is the driver
	 * that is sending the registration.
	 *
	 * registerDriver( IOService *, const OSDictionary *, bool ) is invoked after one of the
	 * initPlatformControl() routines has been invoked (and some of the parameters have already
	 * been set.
	 *
	 * The following properties are REQUIRED for successful completion of initPlatformControl:
	 *	kIOPPluginControlIDKey
	 *
	 * The following properties may be supplied by the thermal profile, but will be given
	 * default values if not specified (the control driver does not know about these
	 * properties):
	 *
	 *	kIOPPluginThermalLocalizedDescKey		"UNKNOWN_CONTROL"	<OSString>
	 *	kIOPPluginControlFlagsKey				0x0					<OSNumber>
	 *	kIOPPluginInitialTargetKey				<none>
	 *
	 * The following properties are always supplied by the control driver (IOHWControl) but, if
	 * specified, values in the thermal profile will take precedence and be used instead.
	 *
	 *	kIOPPluginTypeKey
	 *	kIOPPluginVersionKey
	 *	kIOPPluginLocationKey
	 *	kIOPPluginZoneKey
	 *
	 * The control driver is responsible for supplying its current-value and target-value in
	 * the registration dictionary.  When an IOPlatformControl is initialized (via either method),
	 * the current-value and target-value are set to zero.  When the control driver registers,
	 * these values are updated to reflect the values being reported by the driver.
	 *
	 * The kIOPPluginInitialTargetKey can be specified in the thermal profile.  If present, the
	 * indicated target will be sent to the control driver upon initialization.
	 */

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

	/*!	@function memberOfCtrlLoops
		@abstract Accessor for ctrlLoops array, so that the plugin can get a list of control loops affected by a change in this control's value/state. */
	virtual OSArray *				memberOfCtrlLoops( void );

	/*!	@function sendMessage
		@abstract Sends a dictionary to the control.  The caller is responsible for setting up the dictionary to represent the desired command.  This call can block, as the control may use the calling thread to perform I/O. */
	virtual IOReturn				sendMessage( OSDictionary * msg );

	// get a reference to the info dictionary
	virtual OSDictionary *			getInfoDict( void );

	// accessors - this just grabs attributes from the info dict
	// version
	virtual OSNumber *				getVersion( void );

	// sensor-id
	virtual OSNumber *				getControlID( void );

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

};

