/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformControl.cpp,v $
//		Revision 1.5  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.4.2.6  2003/06/06 08:17:56  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.4.2.5  2003/06/04 10:21:10  eem
//		Supports forced PID meta states.
//		
//		Revision 1.4.2.4  2003/05/26 10:19:00  eem
//		Fixed OSNumber leaks.
//		
//		Revision 1.4.2.3  2003/05/26 10:07:14  eem
//		Fixed most of the bugs after the last cleanup/reorg.
//		
//		Revision 1.4.2.2  2003/05/23 05:44:40  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.4.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.4  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.3.2.3  2003/05/17 11:08:22  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.3.2.2  2003/05/16 07:08:45  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.3.2.1  2003/05/14 22:07:48  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.3  2003/05/13 02:13:51  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.2.2.1  2003/05/12 11:21:09  eem
//		Support for slewing.
//		
//		Revision 1.2  2003/05/10 06:50:33  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.2.1  2003/05/01 09:28:40  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		

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

const OSNumber *IOPlatformControl::applyCurrentValueTransform( const OSNumber * hwReading )
{
	hwReading->retain();
	return hwReading;
}

const OSNumber *IOPlatformControl::applyTargetValueTransform( const OSNumber * hwReading )
{
	hwReading->retain();
	return hwReading;
}

const OSNumber *IOPlatformControl::applyTargetHWTransform( const OSNumber * value )
{
	value->retain();
	return value;
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
		infoDict->setObject(gIOPPluginControlIDKey, number);
	}
	else
	{
		CONTROL_DLOG("IOPlatformControl::initPlatformControl no Control ID\n");
		return(kIOReturnBadArgument);
	}

	// description
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginThermalLocalizedDescKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
	}
	else
	{
		string = OSSymbol::withCString("UNKNOWN_CONTROL");
		infoDict->setObject(gIOPPluginThermalLocalizedDescKey, string);
		string->release();
	}

	// location
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginLocationKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginLocationKey, string);
	}
	else
	{
		string = OSSymbol::withCString("Unknown Control");
		infoDict->setObject(gIOPPluginLocationKey, string);
		string->release();
	}

	// flags
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginControlFlagsKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginControlFlagsKey, number);
	}
	else
	{
		infoDict->setObject(gIOPPluginControlFlagsKey, gIOPPluginZero);
	}

	// version
	if ((number = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginVersionKey))) != NULL)
	{
		infoDict->setObject(gIOPPluginVersionKey, number);
	}
	else
	{
		// if no version was found, assume version 1
		infoDict->setObject(gIOPPluginVersionKey, gIOPPluginOne);
	}

	// zone
	if ((data = OSDynamicCast(OSData, dict->getObject(gIOPPluginZoneKey))) != NULL)
	{
		UInt32 zone;

		if (data->getLength() < sizeof(UInt32))
		{
			CONTROL_DLOG("IOPlatformControl::initPlatformControl invalid zone in thermal profile!!\n");
		}
		else
		{
			zone = *((UInt32 *) data->getBytesNoCopy());
			//CONTROL_DLOG("IOPlatformControl::initPlatformControl got zone %lu\n", zone);
			infoDict->setObject(gIOPPluginZoneKey, data);
		}
	}

	// type - optional override
	if ((string = OSDynamicCast(OSString, dict->getObject(gIOPPluginTypeKey))) != NULL)
	{
		//CONTROL_DLOG("IOPlatformControl::initPlatformControl got type override %s\n", string->getCStringNoCopy());
		infoDict->setObject(gIOPPluginTypeKey, string);
	}

	// create the "registered" key and set it to false
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanFalse);

	// create the "current-value" key and set it to zero
	setCurrentValue(gIOPPluginZero);

	// if there's an initial target value listed, use it, otherwise set to zero
	if ((number = OSDynamicCast(OSNumber, dict->getObject("initial-target"))) != NULL)
	{
		setTargetValue(number);
		//CONTROL_DLOG("IOPlatformControl::initPlatformControl initial target value %u\n", number->unsigned32BitValue());
	}
	else
	{
		setTargetValue(gIOPPluginZero);
	}

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
	const OSNumber * curValue;

	if (isRegistered() == kOSBooleanTrue || driver == NULL)
	{
		return(kIOReturnError);
	}

	// save a pointer to the driver, and set the registered flag
	controlDriver = driver;

	// flag the successful registration
	infoDict->setObject(gIOPPluginRegisteredKey, kOSBooleanTrue );

	// store the control's initial reported current-value
	curValue = applyCurrentValueTransform( OSDynamicCast(OSNumber, dict->getObject(gIOPPluginCurrentValueKey)) );
	setCurrentValue( curValue );
	curValue->release();

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

UInt32 IOPlatformControl::getControlMinValueUInt32( void )
{
	OSNumber * min;

	if ((min = (OSNumber *) getControlMinValue()) == NULL)
	{
		CONTROL_DLOG("IOPlatformControl::getControlMinValue (UInt32) no min value!\n");
		return(0x0);
	}
	else
	{
		return(min->unsigned32BitValue());
	}
}

OSNumber *IOPlatformControl::getControlMinValue( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginControlMinValueKey));
}

UInt32 IOPlatformControl::getControlMaxValueUInt32( void )
{
	OSNumber * max;

	if ((max = getControlMaxValue()) == NULL)
	{
		CONTROL_DLOG("IOPlatformControl::getControlMaxValue (UInt32) no max value!\n");
		return(0xFFFFFFFF);
	}
	else
	{
		return(max->unsigned32BitValue());
	}
}

OSNumber *IOPlatformControl::getControlMaxValue( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginControlMaxValueKey));
}

void IOPlatformControl::setControlMinValue( OSNumber * min )
{
	if (min)
	{
		infoDict->setObject(gIOPPluginControlMinValueKey, min);
	}
}

void IOPlatformControl::setControlMaxValue( OSNumber * max )
{
	if (max)
	{
		infoDict->setObject(gIOPPluginControlMaxValueKey, max);
	}
}

const OSNumber *IOPlatformControl::getCurrentValue( void )
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginCurrentValueKey));
}

void IOPlatformControl::setCurrentValue( const OSNumber * controlValue )
{
	if (!controlValue)
	{
		CONTROL_DLOG("IOPlatformControl::setCurrentValue got null controlValue\n");
		return;
	}

	infoDict->setObject(gIOPPluginCurrentValueKey, controlValue);
}

const OSNumber *IOPlatformControl::fetchCurrentValue( void )
{
	IOReturn status;
	OSDictionary * dict;
	OSArray * controlInfo;
	const OSNumber * currentValue, * raw, * myID, * tmpID;

	//CONTROL_DLOG("IOPlatformControl::fetchCurrentValue - entered\n");

	if (!(isRegistered() == kOSBooleanTrue) || !controlDriver)
	{
		CONTROL_DLOG("IOPlatformControl::fetchCurrentValue not registered\n");
		return(NULL);
	}

	dict = OSDictionary::withCapacity(2);
	if (!dict) return(NULL);
	dict->setObject(gIOPPluginForceUpdateKey, gIOPPluginZero);
	dict->setObject(gIOPPluginControlIDKey, getControlID());

	// force an update
	status = sendMessage( dict );
	dict->release();

	if (status != kIOReturnSuccess )
	{
		CONTROL_DLOG("IOPlatformControl::fetchCurrentValue sendMessage failed!!\n");
		return(NULL);
	}

	// read the updated value
	// This is a little tricky at present.  The AppleSlewClock driver publishes its current-value
	// as a property in the driver's registry entry.  But the AppleFCU driver publishes an
	// array, called "control-info", with data on all the controls it is responsible for.  Here
	// we'll check for a simple current-value property, and if we don't find it we'll look
	// for the control-info array and iterate it for the control we care about.
	if ((raw = OSDynamicCast(OSNumber,	controlDriver->getProperty(gIOPPluginCurrentValueKey))) != NULL)
	{
		currentValue = applyCurrentValueTransform(raw);
		//raw->release();
		return(currentValue);
	}
	else if ((controlInfo = OSDynamicCast(OSArray, controlDriver->getProperty("control-info"))) != NULL)
	{
		int i, count;

		myID = getControlID();

		count = controlInfo->getCount();
		for (i=0; i<count; i++)
		{
			if ((dict = OSDynamicCast(OSDictionary, controlInfo->getObject(i))) != NULL)
			{
				if ((tmpID = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginControlIDKey))) != NULL &&
					myID->isEqualTo(tmpID) &&
					(raw = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginCurrentValueKey))) != NULL)
				{
					currentValue = applyCurrentValueTransform(raw);
					//raw->release();
					return(currentValue);
				}
			}
		}
	}

	// if we get here, the current-value was not found in any known location
	CONTROL_DLOG("IOPlatformControl::fetchCurrentValue can't find current value in controlDriver!!\n");
	return(NULL);
}

const OSNumber *IOPlatformControl::getTargetValue(void)
{
	return OSDynamicCast(OSNumber, infoDict->getObject(gIOPPluginTargetValueKey));
}

void IOPlatformControl::setTargetValue( UInt32 value )
{
	const OSNumber * target = OSNumber::withNumber( value, 32 );
	setTargetValue( target );
	target->release();
}

void IOPlatformControl::setTargetValue( const OSNumber * value )
{
	if (value)
	{
		infoDict->setObject( gIOPPluginTargetValueKey, value );
	}
}

bool IOPlatformControl::sendTargetValue( const OSNumber * target, bool forced /* = false */)
{
	IOReturn status;
	OSDictionary * dict;
	const OSNumber * temp;

	//CONTROL_DLOG("IOPlatformControl::sendTargetValue - entered\n");

	if (!(isRegistered() == kOSBooleanTrue) || !controlDriver)
	{
		CONTROL_DLOG("IOPlatformControl::sendTargetValue not registered\n");
		return(false);
	}

	if (!target)
	{
		CONTROL_DLOG("IOPlatformControl::sendTargetValue value is NULL!\n");
		return(false);
	}

	if (!forced && infoDict->getObject(gIOPPluginForceControlTargetValKey))
	{
		// ignore this request, but make it appear to have succeeded.  The caller
		// did not set the forced flag, but this control is being externally forced
		// to another value.
		return(true);
	}
	
	dict = OSDictionary::withCapacity(2);
	if (!dict) return(NULL);
	temp = applyTargetHWTransform(target);
	dict->setObject(gIOPPluginTargetValueKey, temp );
	temp->release();
	dict->setObject(gIOPPluginControlIDKey, getControlID());

	// send the dict
	status = sendMessage( dict );
	dict->release();

	if (status != kIOReturnSuccess )
	{
		CONTROL_DLOG("IOPlatformControl::sendTargetValue sendMessage failed!!\n");
		return(false);
	}

	return(true);
}
