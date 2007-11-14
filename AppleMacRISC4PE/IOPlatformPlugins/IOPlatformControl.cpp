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


#include <IOKit/IOLib.h>
#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformCtrlLoop.h"
#include "IOPlatformControl.h"

#define super OSObject
OSDefineMetaClassAndStructors(IOPlatformControl, OSObject)

bool IOPlatformControl::init( void )
{
	if (!super::init()) return(false);

	// allocate the info dictionary
	if ((infoDict = OSDictionary::withCapacity(7)) == NULL) return(false);

	controlDriver = NULL;

	ctrlLoops = NULL;

	return( initSymbols() );
}

bool IOPlatformControl::initSymbols( void )
{
	return( true );
}

void IOPlatformControl::free( void )
{
	if (infoDict) { infoDict->release(); infoDict = NULL; }
	if (controlDriver) { controlDriver->release(); controlDriver = NULL; }
	if (ctrlLoops) { ctrlLoops->release(); ctrlLoops = NULL; }

	super::free();
}

ControlValue IOPlatformControl::applyCurrentValueTransform( ControlValue hwReading )
{
	return hwReading;
}

ControlValue IOPlatformControl::applyTargetValueTransform( ControlValue hwReading )
{
	return hwReading;
}

ControlValue IOPlatformControl::applyTargetValueInverseTransform( ControlValue pluginReading )
{
	return pluginReading;
}

IOReturn IOPlatformControl::initPlatformControl( const OSDictionary *dict )
{
	const OSNumber *number;
	const OSData *data;
	const OSString *string;

	if ( !dict || !init() ) return(kIOReturnError);


	// id
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginControlIDKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginControlIDKey, number );
	}
	else
	{
		IOLog("IOPlatformControl::initPlatformControl Invalid Thermal Profile omits Control ID\n");
		return(kIOReturnBadArgument);
	}

	// description - if not included in thermal profile, will be set in registerDriver()
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginThermalLocalizedDescKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
	}

	// flags - if not included in thermal profile, will be set in registerDriver()
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginControlFlagsKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginControlFlagsKey, number);
	}

	// type
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginTypeKey))) != NULL)
	{
		infoDict->setObject( gIOPPluginTypeKey, string );
	}

	// version - if not included in thermal profile, will be set in registerDriver()
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginVersionKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginVersionKey, number);
	}

	// location - if not included in thermal profile, will be set in registerDriver()
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginLocationKey))) != NULL)
	{
		infoDict->setObject( gIOPPluginLocationKey, string);
	}

	// zone - if not included in thermal profile, will be set in registerDriver()
	if ((data = OSDynamicCast(OSData, dict->getObject(gIOPPluginZoneKey))) != NULL)
	{
		infoDict->setObject( gIOPPluginZoneKey, data);
	}

	// create the "registered" key and set it to false
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanFalse);

	// create the "current-value" key and set it to zero
	setCurrentValue( 0x0 );

	// create the "target-value" key and set it to zero
	setTargetValue( 0x0 );

	// if there's an initial target value listed in the thermal profile, record it
	if ((number = OSDynamicCast(OSNumber, dict->getObject(kIOPPluginInitialTargetKey))) != NULL)
	{
		infoDict->setObject(kIOPPluginInitialTargetKey, number);
	}

	return(kIOReturnSuccess);
}

IOReturn IOPlatformControl::initPlatformControl( IOService * unknownControl, const OSDictionary * dict )
{
	const OSNumber *number;

	if ( !unknownControl || !dict || !init() ) return(kIOReturnError);

	// id
	if ((number = OSDynamicCast(OSNumber, unknownControl->getProperty(gIOPPluginControlIDKey))) != NULL ||
	    (number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginControlIDKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginControlIDKey, number );
	}
	else
	{
		IOLog("IOPlatformControl::initPlatformControl Unlisted Registrant %s omits Control ID\n", unknownControl->getName());
		return(kIOReturnBadArgument);
	}

	// create the "registered" key and set it to false
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanFalse);

	// create the "current-value" key and set it to zero
	setCurrentValue( 0x0 );

	// create the "target-value" key and set it to zero
	setTargetValue( 0x0 );

	return(kIOReturnSuccess);
}

bool IOPlatformControl::isManaged( void )
{
	return(!(getControlFlags()->unsigned32BitValue() & IOPControlFlagExternallyManaged));
}

OSBoolean *IOPlatformControl::isRegistered( void )
{
	return(OSDynamicCast(OSBoolean, infoDict->getObject(gIOPPluginRegisteredKey)));
}

IOReturn IOPlatformControl::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
	const OSData * data;
	const OSString * string;
	const OSNumber * number;
	ControlValue controlValue;

	if (isRegistered() == kOSBooleanTrue || driver == NULL)
	{
		return(kIOReturnError);
	}

	// save a pointer to the driver, and set the registered flag
	controlDriver = driver;
	controlDriver->retain();

	// If there's no localized description key, add a default
	if (getControlDescKey() == NULL)
	{
		string = OSSymbol::withCString("UNKNOWN_CONTROL");
		infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
		string->release();
	}

	// If there's no flags, set a default of zero (no bits set)
	if (getControlFlags() == NULL)
	{
		infoDict->setObject(kIOPPluginControlFlagsKey, gIOPPluginZero);
	}

	// check for some properties.  If the thermal profile hasn't overridden these values and the
	// control driver is non-comformant (doesn't supply the properties), print an error to system log
	// so someone gets nagged about it.
	// type
	if (getControlType() == NULL)
	{
		if ((string = OSDynamicCast(OSString, controlDriver->getProperty(gIOPPluginTypeKey))) != NULL ||
		    (string = OSDynamicCast(OSString, dict->getObject(gIOPPluginTypeKey))) != NULL)
		{
			infoDict->setObject( gIOPPluginTypeKey, string );
		}
		else
		{
			// the type has to be there.  If we can't find it anywhere, this control is no good to us.
			IOLog("IOPlatformControl::registerDriver can't register Control ID %08X (%s) with unknown Control Type!\n",
					getControlID()->unsigned32BitValue(), controlDriver->getName());
			return(kIOReturnBadArgument);
		}
	}

	if (getVersion() == NULL)
	{
		if ((number = OSDynamicCast(OSNumber, controlDriver->getProperty(gIOPPluginVersionKey))) != NULL ||
		    (number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginVersionKey))) != NULL)
		{
			infoDict->setObject(gIOPPluginVersionKey, number);
		}
		else
		{
			infoDict->setObject(gIOPPluginVersionKey, gIOPPluginOne);
			IOLog("IOPlatformControl::registerDriver Control Driver %s did not supply version property, using default\n",
					controlDriver->getName());
		}
	}

	// Are there min-value, max-value or safe-value keys?

	if ( infoDict->getObject( gIOPPluginControlMinValueKey ) == NULL )
		{
		if ( ( ( number = OSDynamicCast( OSNumber, controlDriver->getProperty( gIOPPluginControlMinValueKey ) ) ) != NULL ) ||
		    ( ( number = OSDynamicCast( OSNumber, dict->getObject( gIOPPluginControlMinValueKey ) ) ) != NULL ) )
			{
			infoDict->setObject( gIOPPluginControlMinValueKey, number );
			}
		}

	if ( infoDict->getObject( gIOPPluginControlMaxValueKey ) == NULL )
		{
		if ( ( ( number = OSDynamicCast( OSNumber, controlDriver->getProperty( gIOPPluginControlMaxValueKey ) ) ) != NULL ) ||
		    ( ( number = OSDynamicCast( OSNumber, dict->getObject( gIOPPluginControlMaxValueKey ) ) ) != NULL ) )
			{
			infoDict->setObject( gIOPPluginControlMaxValueKey, number );
			}
		}

	if ( infoDict->getObject( gIOPPluginControlSafeValueKey ) == NULL )
		{
		if ( ( ( number = OSDynamicCast( OSNumber, controlDriver->getProperty( gIOPPluginControlSafeValueKey ) ) ) != NULL ) ||
		    ( ( number = OSDynamicCast( OSNumber, dict->getObject( gIOPPluginControlSafeValueKey ) ) ) != NULL ) )
			{
			infoDict->setObject( gIOPPluginControlSafeValueKey, number );
			}
		}

	if (getControlLocation() == NULL)
	{
		if ((string = OSDynamicCast(OSString, controlDriver->getProperty(gIOPPluginLocationKey))) != NULL ||
		    (string = OSDynamicCast(OSString, dict->getObject(gIOPPluginLocationKey))) != NULL)
		{
			infoDict->setObject(gIOPPluginLocationKey, string);
		}
		else
		{
			string = OSSymbol::withCString("Unknown Control");
			infoDict->setObject(gIOPPluginLocationKey, string);
			string->release();
			IOLog("IOPlatformControl::registerDriver Control Driver %s did not supply location property, using default\n",
					controlDriver->getName());
		}
	}

	if (getControlZone() == NULL)
	{
		if ((data = OSDynamicCast(OSData, controlDriver->getProperty(gIOPPluginZoneKey))) != NULL ||
		    (data = OSDynamicCast(OSData, dict->getObject(gIOPPluginZoneKey))) != NULL)
		{
			infoDict->setObject(gIOPPluginZoneKey, data);
		}
		else
		{
			UInt32 zero = 0;
			data = OSData::withBytes( &zero, sizeof(UInt32) );
			infoDict->setObject(gIOPPluginZoneKey, data);
			data->release();
			IOLog("IOPlatformControl::registerDriver Control Driver %s did not supply zone property, using default\n",
					controlDriver->getName());
		}
	}

	// store the control's initial reported current-value
	number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginCurrentValueKey));
	if (number)
	{
		controlValue = number->unsigned32BitValue();
		controlValue = applyCurrentValueTransform( controlValue );
		setCurrentValue( controlValue );
	}
	else
	{
		setCurrentValue( 0x0 );
		IOLog("IOPlatformControl::registerDriver Control Driver %s did not supply current-value, using default\n",
				controlDriver->getName());
	}

	// flag the successful registration
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanTrue );

	// handle the target value: if there was an initial-target set in the thermal profile, then send it
	// down to the driver.  Otherwise, read the target-value out of the control's registration dictionary.
	// Handle the case of a non-comformant control that doesn't publish its target value as expected
	if ((number = OSDynamicCast(OSNumber, infoDict->getObject(kIOPPluginInitialTargetKey))) != NULL)
	{
		controlValue = number->unsigned32BitValue();

		if (sendTargetValue(controlValue))
		{
			setTargetValue(controlValue);
		}
		else
		{
			IOLog("IOPlatformControl::registerDriver failed to send target-value to Control Driver %s\n",
					controlDriver->getName());
		}
	}
	else
	{
		number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginTargetValueKey));
		if (number)
		{
			controlValue = number->unsigned32BitValue();

			// we need to apply the transformation (because the value came from the control driver),
			// and only need to setTargetValue().   sendTargetValue() is not necessary -- the control
			// driver is alreay using this target value.
			controlValue = applyTargetValueTransform( controlValue );
			setTargetValue( controlValue );
		}
		else
		{
			setTargetValue( 0x0 );
			IOLog("IOPlatformControl::registerDriver Control Driver %s did not supply target-value, using default\n",
					controlDriver->getName());
		}
	}

	if (notify) notifyCtrlLoops();

	return(kIOReturnSuccess);
}

void IOPlatformControl::notifyCtrlLoops( void )
{
	IOPlatformCtrlLoop * loop;
	int i, count;

	// notify my ctrl loops that the driver registered
	if (ctrlLoops)
	{
		count = ctrlLoops->getCount();
		for ( i=0; i<count; i++ )
		{
			loop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i));
			if (loop) loop->controlRegistered( this );
		}
	}
}

bool IOPlatformControl::joinedCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop )
{
	if (!aCtrlLoop)
	{
		CONTROL_DLOG("IOPlatformControl::joinedCtrlLoop bad argument\n");
		return(false);
	}

	if (!ctrlLoops)
	{
		ctrlLoops = OSArray::withObjects((const OSObject **) &aCtrlLoop, 1);
		return(true);
	}
	else
	{
		// Make sure this control loop isn't already listed
		unsigned index, count;
		count = ctrlLoops->getCount();
		for (index = 0; index < count; index++)
		{
			if (ctrlLoops->getObject(index) == aCtrlLoop)
			{
				CONTROL_DLOG("IOPlatformControl::joingCtrlLoop already a member\n");
				return(false);
			}
		}

		ctrlLoops->setObject(aCtrlLoop);
		return(true);
	}
}

bool IOPlatformControl::leftCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop )
{
	if (!aCtrlLoop)
	{
		CONTROL_DLOG("IOPlatformControl::leftCtrlLoop bad argument\n");
		return(false);
	}

	if (!ctrlLoops)
	{
		CONTROL_DLOG("IOPlatformControl::leftCtrlLoop no control loops\n");
		return(false);
	}

	bool removed = false;
	int index, count;
	count = ctrlLoops->getCount();
	for (index = 0; index < count; index++)
	{
		if (ctrlLoops->getObject(index) == aCtrlLoop)
		{
			ctrlLoops->removeObject(index);
			removed = true;
		}
	}

	if (!removed)
		CONTROL_DLOG("IOPlatformControl::leftCtrlLoop not a member\n");

	return(removed);
}

OSArray *IOPlatformControl::memberOfCtrlLoops( void )
{
	return ctrlLoops;
}

OSDictionary *IOPlatformControl::getInfoDict( void )
{
	return infoDict;
}

OSNumber *IOPlatformControl::getVersion( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginVersionKey));
}

OSNumber *IOPlatformControl::getControlID( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginControlIDKey));
}

OSString *IOPlatformControl::getControlType( void )
{
	return OSDynamicCast(OSString, infoDict->getObject(gIOPPluginTypeKey));
}

UInt32 IOPlatformControl::getControlTypeID( void )
{
	OSString * typeString;

	typeString = getControlType();

	if (!typeString)
		return kIOPControlTypeUnknown;
	else if (typeString->isEqualTo(gIOPPluginTypeSlewControl))
		return kIOPControlTypeSlew;
	else if (typeString->isEqualTo(gIOPPluginTypeFanRPMControl))
		return kIOPControlTypeFanRPM;
	else if (typeString->isEqualTo(gIOPPluginTypeFanPWMControl))
		return kIOPControlTypeFanPWM;
	else
		return kIOPControlTypeUnknown;
}

OSData *IOPlatformControl::getControlZone( void )
{
	return OSDynamicCast(OSData, infoDict->getObject(gIOPPluginZoneKey));
}

OSString *IOPlatformControl::getControlLocation( void )
{
	return OSDynamicCast(OSString, infoDict->getObject(gIOPPluginLocationKey));
}

OSString *IOPlatformControl::getControlDescKey( void )
{
	return OSDynamicCast(OSString, infoDict->getObject(gIOPPluginThermalLocalizedDescKey));
}

OSNumber *IOPlatformControl::getControlFlags( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginControlFlagsKey));
}


ControlValue IOPlatformControl::getValue( const OSSymbol * key, ControlValue defaultValue )
{
	ControlValue value;
	OSNumber * num;

	num = OSDynamicCast(OSNumber, infoDict->getObject(key));
	value = num ? num->unsigned32BitValue() : defaultValue;

	return value;
}

void IOPlatformControl::setValue( const OSSymbol * key, ControlValue newValue )
{
	OSNumber * num;

	num = OSDynamicCast(OSNumber, infoDict->getObject(key));

	if (num)
	{
		num->setValue( newValue );
	}
	else
	{
		num = OSNumber::withNumber( newValue, 32 );
		infoDict->setObject( key, num );
		num->release();
	}
}

ControlValue IOPlatformControl::getControlMinValue( void )
{
	return getValue( gIOPPluginControlMinValueKey, 0x0 );
}

ControlValue IOPlatformControl::getControlMaxValue( void )
{
	return getValue( gIOPPluginControlMaxValueKey, 0xFFFFFFFF );
}

ControlValue IOPlatformControl::getControlSafeValue( void )
{
	// default value is 0xFFFF because this corresponds to an
	// "unpopulated" value in the SMU data block
	return getValue( gIOPPluginControlSafeValueKey, 0xFFFF );
}

void IOPlatformControl::setControlMinValue( ControlValue min )
{
	setValue( gIOPPluginControlMinValueKey, min );
}

void IOPlatformControl::setControlMaxValue( ControlValue max )
{
	setValue( gIOPPluginControlMaxValueKey, max );
}

void IOPlatformControl::setControlSafeValue( ControlValue safe )
{
	setValue( gIOPPluginControlSafeValueKey, safe );
}

ControlValue IOPlatformControl::getCurrentValue( void )
{
	return getValue( gIOPPluginCurrentValueKey, 0x0 );
}

void IOPlatformControl::setCurrentValue( ControlValue newValue )
{
	setValue( gIOPPluginCurrentValueKey, newValue );

	// notify control loops that the value changed
	if (ctrlLoops)
	{
		IOPlatformCtrlLoop * aCtrlLoop;
		int index, count;
		count = ctrlLoops->getCount();
		for (index = 0; index < count; index++)
		{
			if ((aCtrlLoop = OSDynamicCast( IOPlatformCtrlLoop, ctrlLoops->getObject(index) )) != NULL)
				aCtrlLoop->controlCurrentValueWasSet( this, newValue );
		}
	}
}

ControlValue IOPlatformControl::forceAndFetchCurrentValue( void )
{
#if CONTROL_DEBUG
	if (kIOReturnSuccess != sendForceUpdate())
	{
		CONTROL_DLOG("IOPlatformControl::forceAndFetchCurrentValue(0x%08lX) failed to force-update\n", getControlID()->unsigned32BitValue());
	}
#else
	sendForceUpdate();
#endif
	
	return(fetchCurrentValue());
}


ControlValue IOPlatformControl::fetchCurrentValue( void )
{
	OSDictionary * dict;
	OSArray * controlInfo;
	ControlValue pluginValue;
	const OSNumber * hwReading, * myID, * tmpID;

	//CONTROL_DLOG("IOPlatformControl::fetchCurrentValue - entered\n");

	if (!(isRegistered() == kOSBooleanTrue) || !controlDriver)
	{
		CONTROL_DLOG("IOPlatformControl::fetchCurrentValue not registered\n");
		return 0x0;
	}

	// read the control value
	// This is a little tricky at present.  The AppleSlewClock driver publishes its current-value
	// as a property in the driver's registry entry.  But the AppleFCU driver publishes an
	// array, called "control-info", with data on all the controls it is responsible for.  Here
	// we'll check for a simple current-value property, and if we don't find it we'll look
	// for the control-info array and iterate it for the control we care about.
	if ((hwReading = OSDynamicCast(OSNumber, controlDriver->getProperty(gIOPPluginCurrentValueKey))) == NULL)
	{
		if ((controlInfo = OSDynamicCast(OSArray, controlDriver->getProperty("control-info"))) == NULL)
		{
			// if we get here, the current-value was not found in any known location
			CONTROL_DLOG("IOPlatformControl::fetchCurrentValue can't find current value in controlDriver!!\n");
			return 0x0;
		}

		int i, count;

		myID = getControlID();

		count = controlInfo->getCount();
		for (i=0; i<count; i++)
		{
			if ((dict = OSDynamicCast(OSDictionary, controlInfo->getObject(i))) == NULL ||
			    (tmpID = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginControlIDKey))) == NULL ||
			    !myID->isEqualTo(tmpID) ||
			    (hwReading = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginCurrentValueKey))) == NULL)
			{
				continue;
			}
		}

		if (!hwReading)
		{
			// if we get here, the current-value was not found in the control-info array
			CONTROL_DLOG("IOPlatformControl::fetchCurrentValue can't find current value in control-info array!!\n");
			return 0x0;
		}
	}

	pluginValue = hwReading->unsigned32BitValue();
	pluginValue = applyCurrentValueTransform( pluginValue );

	return(pluginValue);
}

ControlValue IOPlatformControl::getTargetValue( void )
{
	return getValue( gIOPPluginTargetValueKey, 0x0 );
}

void IOPlatformControl::setTargetValue( ControlValue target )
{
	setValue( gIOPPluginTargetValueKey, target );

	// notify control loops that the value changed
	if (ctrlLoops)
	{
		IOPlatformCtrlLoop * aCtrlLoop;
		int index, count;
		count = ctrlLoops->getCount();
		for (index = 0; index < count; index++)
		{
			if ((aCtrlLoop = OSDynamicCast( IOPlatformCtrlLoop, ctrlLoops->getObject(index) )) != NULL)
				aCtrlLoop->controlTargetValueWasSet( this, target );
		}
	}
}

IOReturn IOPlatformControl::sendMessage( OSDictionary * msg )
{
	//CONTROL_DLOG("IOPlatformControl::sendMessage - entered\n");

	if (!(isRegistered() == kOSBooleanTrue) || !controlDriver)
	{
		CONTROL_DLOG("IOPlatformControl::sendMessage not registered\n");
		return(kIOReturnOffline);
	}

	if ( !msg )
	{
		CONTROL_DLOG("IOPlatformControl::sendMessage no message\n");
		return(kIOReturnBadArgument);
	}

	return(controlDriver->setProperties( msg ));
}

bool IOPlatformControl::sendTargetValue( ControlValue target, bool forced /* = false */)
{
	IOReturn status;
	OSDictionary * dict;
	const OSNumber * num;

	if (!forced && infoDict->getObject(gIOPPluginForceControlTargetValKey))
	{
		// ignore this request, but make it appear to have succeeded.  The caller
		// did not set the forced flag, but this control is being externally forced
		// to another value.
		return(true);
	}
	
	dict = OSDictionary::withCapacity(2);
	if (!dict) return(false);

	// apply the inverse target value transform
	target = applyTargetValueInverseTransform(target);

	// add the target value to the command dict
	num = OSNumber::withNumber( target, 32 );
	dict->setObject(gIOPPluginTargetValueKey, num );
	num->release();

	// add the control ID to the command dict
	dict->setObject(gIOPPluginControlIDKey, getControlID());

	// send the dict
	status = sendMessage( dict );
	dict->release();

	return ( status == kIOReturnSuccess );
}

IOReturn IOPlatformControl::sendForceUpdate( void )
{
	IOReturn status;
	OSDictionary *forceUpdateDict;

	forceUpdateDict = OSDictionary::withCapacity(2);
	if (!forceUpdateDict) return(kIOReturnNoMemory);
	forceUpdateDict->setObject(gIOPPluginForceUpdateKey, gIOPPluginZero);
	forceUpdateDict->setObject(gIOPPluginControlIDKey, getControlID());

	// force an update
	status = sendMessage( forceUpdateDict );
	forceUpdateDict->release();

	return(status);
}

IOService *IOPlatformControl::getControlDriver( void )
{
	return controlDriver;
}