/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */


#include "IOPlatformPlugin.h"

#define super IOService
OSDefineMetaClassAndStructors(IOPlatformPlugin, IOService)

#pragma mark
#pragma mark *** IOPlatformPluginFamily Global Symbols ***

/*
 * Symbols are declared here, allocated in initSymbols(), and extern'd
 * in IOPlatformPluginTypes.h for easy access by all helper classes.
 */

const OSSymbol * gIOPPluginForceUpdateKey;
const OSSymbol * gIOPPluginForceUpdateAllKey;
const OSSymbol * gIOPPluginForceSensorCurValKey;
const OSSymbol * gIOPPluginReleaseForcedSensorKey;
const OSSymbol * gIOPPluginForceControlTargetValKey;
const OSSymbol * gIOPPluginReleaseForcedControlKey;
const OSSymbol * gIOPPluginForceCtrlLoopMetaStateKey;
const OSSymbol * gIOPPluginReleaseForcedCtrlLoopKey;
const OSSymbol * gIOPPluginVersionKey;
const OSSymbol * gIOPPluginTypeKey;
const OSSymbol * gIOPPluginLocationKey;
const OSSymbol * gIOPPluginZoneKey;
const OSSymbol * gIOPPluginCurrentValueKey;
const OSSymbol * gIOPPluginPollingPeriodKey;
const OSSymbol * gIOPPluginRegisteredKey;
const OSSymbol * gIOPPluginSensorDataKey;
const OSSymbol * gIOPPluginControlDataKey;
const OSSymbol * gIOPPluginCtrlLoopDataKey;
const OSSymbol * gIOPPluginSensorIDKey;
const OSSymbol * gIOPPluginSensorFlagsKey;
const OSSymbol * gIOPPluginCurrentStateKey;
const OSSymbol * gIOPPluginLowThresholdKey;
const OSSymbol * gIOPPluginHighThresholdKey;
const OSSymbol * gIOPPluginTypeTempSensor;
const OSSymbol * gIOPPluginTypePowerSensor;
const OSSymbol * gIOPPluginTypeVoltageSensor;
const OSSymbol * gIOPPluginTypeCurrentSensor;
const OSSymbol * gIOPPluginTypeADCSensor;
const OSSymbol * gIOPPluginControlIDKey;
const OSSymbol * gIOPPluginControlFlagsKey;
const OSSymbol * gIOPPluginTargetValueKey;
const OSSymbol * gIOPPluginControlMinValueKey;
const OSSymbol * gIOPPluginControlMaxValueKey;
const OSSymbol * gIOPPluginTypeSlewControl;
const OSSymbol * gIOPPluginTypeFanRPMControl;
const OSSymbol * gIOPPluginTypeFanPWMControl;
const OSSymbol * gIOPPluginControlClass;
const OSSymbol * gIOPPluginSensorClass;
const OSSymbol * gIOPPluginEnvInternalOvertemp;
const OSSymbol * gIOPPluginEnvExternalOvertemp;
const OSSymbol * gIOPPluginEnvDynamicPowerStep;
const OSSymbol * gIOPPluginEnvControlFailed;
const OSSymbol * gIOPPluginCtrlLoopIDKey;
const OSSymbol * gIOPPluginCtrlLoopMetaState;
const OSSymbol * gIOPPluginThermalLocalizedDescKey;
const OSSymbol * gIOPPluginThermalValidConfigsKey;
const OSSymbol * gIOPPluginThermalMetaStatesKey;
const OSSymbol * gIOPPluginPlatformID;

const OSNumber * gIOPPluginZero;
const OSNumber * gIOPPluginOne;

IOPlatformPlugin * platformPlugin;

#pragma mark
#pragma mark *** IOService method overrides ***

/*
 * IOService overrides
 */

bool IOPlatformPlugin::start(IOService *nub)
{
	mach_timespec_t waitTimeout;
	AbsoluteTime now;
	const OSArray * tempArray;
	IONotifier * restartNotifier;

	//DLOG("IOPlatformPlugin::start - entered\n");

	if (!super::start (nub)) goto failOnly;

	// store a pointer to ME!!
	platformPlugin = this;

	// set up symbols
	initSymbols();

	// globals used for one, zero, true, false
	gIOPPluginZero = OSNumber::withNumber( (unsigned long long) 0, 1);
	gIOPPluginOne = OSNumber::withNumber( (unsigned long long) 1, 1);

	// allocate the thread call used for timer callbacks
	timerCallout = thread_call_allocate( (thread_call_func_t) IOPlatformPlugin::timerEventOccured,
			(thread_call_param_t) this );

	if (!timerCallout)
	{
		IOLog("IOPlatformPlugin::start failed to allocate thread callout\n");
		goto failReleaseSymbols;
	}

	// allocate the environmental info dict
	envInfo = OSDictionary::withCapacity(3);

	// expose the environment dict to the registry
	setProperty(kIOPPluginEnvInfoKey, envInfo);

	// populate the environment info dict
	setEnv(gIOPPluginEnvInternalOvertemp, (tempArray = OSArray::withCapacity(0)));
	tempArray->release();
	setEnv(gIOPPluginEnvExternalOvertemp, (tempArray = OSArray::withCapacity(0)));
	tempArray->release();
	setEnv(gIOPPluginEnvControlFailed, (tempArray = OSArray::withCapacity(0)));
	tempArray->release();
	setEnv(gIOPPluginEnvDynamicPowerStep, gIOPPluginZero);	// assume fast boot

	// clear the envChanged flag
	envChanged = false;

	// determine what the machine configuration is and cache the result
	machineConfig = probeConfig();
	DLOG("IOPlatformPlugin MACHINE CONFIG %u\n", machineConfig);

	// parse in all the info for this machine
	initThermalProfile(nub);

	// set the timer starting.  Do this before we register with PM, I/O Kit, and
	// IOResources cause we need to guarantee that no one has already grabbed
	// the command gate (and caused setTimeout() to be invoked on another thread).
	clock_get_uptime(&now);
	setTimeout(now);

    // Before we go to sleep we wish to disable the napping mode so that the PMU
    // will not shutdown the system while going to sleep:
	waitTimeout.tv_sec = 30;
	waitTimeout.tv_nsec = 0;

    pmRootDomain = OSDynamicCast(IOPMrootDomain,
			waitForService(serviceMatching("IOPMrootDomain"), &waitTimeout));
    if (pmRootDomain != 0)
    {
        //DLOG("IOPlatformPlugin::start to acknowledge power changes\n");
        pmRootDomain->registerInterestedDriver(this);
        
        // Join the Power Management Tree to receive setAggressiveness calls.
        PMinit();
        nub->joinPMtree(this);
    }

	// Install power change handler (for restart notification)
	restartNotifier = registerPrioritySleepWakeInterest(&sysPowerDownHandler, this, 0);

	// HELLO!!
	registerService();

	// Let the world know we're open for business
	publishResource ("IOPlatformMonitor", this);	// For backward compatibility
	publishResource ("IOPlatformPlugin", this);
	
	return(true);

failReleaseSymbols:
	// I am not releasing all the symbols created in initSymbols() because, well,
	// this should never fail, so I don't think it's worth the code bloat.
	gIOPPluginZero->release();
	gIOPPluginZero = NULL;
	gIOPPluginOne->release();
	gIOPPluginOne = NULL;

failOnly:	
	return(false);
}

void IOPlatformPlugin::stop(IOService *nub)
{
	thread_call_cancel(timerCallout);
	thread_call_free(timerCallout);
}

bool IOPlatformPlugin::init( OSDictionary * dict )
{
	if (!super::init(dict)) return(false);

	gate = IORecursiveLockAlloc();
	if (!gate) return(false);

	controls = NULL;
	controlInfoDicts = NULL;
	
	sensors = NULL;
	sensorInfoDicts = NULL;

	ctrlLoops = NULL;
	ctrlLoopInfoDicts = NULL;

	envInfo = NULL;

	pluginPowerState = kIOPPluginRunning;

	return(true);
}

void IOPlatformPlugin::free( void )
{
	if (controls) { controls->release(); controls = NULL; }
	if (controlInfoDicts)
	{
		removeProperty(gIOPPluginControlDataKey);
		controlInfoDicts->release();
		controlInfoDicts = NULL;
	}

	if (sensors) { sensors->release(); sensors = NULL; }
	if (sensorInfoDicts)
	{
		removeProperty(gIOPPluginSensorDataKey);
		sensorInfoDicts->release();
		sensorInfoDicts = NULL;
	}

	if (ctrlLoops) { ctrlLoops->release(); ctrlLoops = NULL; }
	if (ctrlLoopInfoDicts)
	{
		removeProperty(gIOPPluginCtrlLoopDataKey);
		ctrlLoopInfoDicts->release();
		ctrlLoopInfoDicts = NULL;
	}

	if (envInfo) { envInfo->release(); envInfo = NULL; }

	IORecursiveLockFree(gate);

	super::free();
}

#pragma mark
#pragma mark *** Initialization Routines ***

void IOPlatformPlugin::initSymbols( void )
{
	gIOPPluginForceUpdateKey			= OSSymbol::withCString( kIOPPluginForceUpdateKey );
	gIOPPluginForceUpdateAllKey			= OSSymbol::withCString( kIOPPluginForceUpdateAllKey );
	gIOPPluginForceSensorCurValKey		= OSSymbol::withCString( kIOPPluginForceSensorCurValKey );
	gIOPPluginReleaseForcedSensorKey	= OSSymbol::withCString( kIOPPluginReleaseForcedSensorKey );
	gIOPPluginForceControlTargetValKey	= OSSymbol::withCString( kIOPPluginForceCtrlTargetValKey );
	gIOPPluginReleaseForcedControlKey	= OSSymbol::withCString( kIOPPluginReleaseForcedControlKey );
	gIOPPluginForceCtrlLoopMetaStateKey	= OSSymbol::withCString( kIOPPluginForceCtrlLoopMetaStateKey );
	gIOPPluginReleaseForcedCtrlLoopKey	= OSSymbol::withCString( kIOPPluginReleaseForcedCtrlLoopKey );
	gIOPPluginVersionKey				= OSSymbol::withCString( kIOPPluginVersionKey );
	gIOPPluginTypeKey					= OSSymbol::withCString( kIOPPluginTypeKey );
	gIOPPluginLocationKey				= OSSymbol::withCString( kIOPPluginLocationKey );
	gIOPPluginZoneKey					= OSSymbol::withCString( kIOPPluginZoneKey );
	gIOPPluginCurrentValueKey			= OSSymbol::withCString( kIOPPluginCurrentValueKey );
	gIOPPluginPollingPeriodKey			= OSSymbol::withCString( kIOPPluginPollingPeriodKey );
	gIOPPluginRegisteredKey				= OSSymbol::withCString( kIOPPluginRegisteredKey );
	gIOPPluginSensorDataKey				= OSSymbol::withCString( kIOPPluginSensorDataKey );
	gIOPPluginControlDataKey			= OSSymbol::withCString( kIOPPluginControlDataKey );
	gIOPPluginCtrlLoopDataKey			= OSSymbol::withCString( kIOPPluginCtrlLoopDataKey );
	gIOPPluginSensorIDKey				= OSSymbol::withCString( kIOPPluginSensorIDKey );
	gIOPPluginSensorFlagsKey			= OSSymbol::withCString( kIOPPluginSensorFlagsKey );
	gIOPPluginCurrentStateKey			= OSSymbol::withCString( kIOPPluginCurrentStateKey );
	gIOPPluginLowThresholdKey			= OSSymbol::withCString( kIOPPluginLowThresholdKey );
	gIOPPluginHighThresholdKey			= OSSymbol::withCString( kIOPPluginHighThresholdKey );
	gIOPPluginTypeTempSensor			= OSSymbol::withCString( kIOPPluginTypeTempSensor );
	gIOPPluginTypePowerSensor			= OSSymbol::withCString( kIOPPluginTypePowerSensor );
	gIOPPluginTypeVoltageSensor			= OSSymbol::withCString( kIOPPluginTypeVoltageSensor );
	gIOPPluginTypeCurrentSensor			= OSSymbol::withCString( kIOPPluginTypeCurrentSensor );
	gIOPPluginTypeADCSensor				= OSSymbol::withCString( kIOPPluginTypeADCSensor );
	gIOPPluginControlIDKey				= OSSymbol::withCString( kIOPPluginControlIDKey );
	gIOPPluginControlFlagsKey			= OSSymbol::withCString( kIOPPluginControlFlagsKey );
	gIOPPluginTargetValueKey			= OSSymbol::withCString( kIOPPluginTargetValueKey );
	gIOPPluginControlMinValueKey		= OSSymbol::withCString( kIOPPluginControlMinValueKey );
	gIOPPluginControlMaxValueKey		= OSSymbol::withCString( kIOPPluginControlMaxValueKey );
	gIOPPluginTypeSlewControl			= OSSymbol::withCString( kIOPPluginTypeSlewControl );
	gIOPPluginTypeFanRPMControl			= OSSymbol::withCString( kIOPPluginTypeFanRPMControl );
	gIOPPluginTypeFanPWMControl			= OSSymbol::withCString( kIOPPluginTypeFanPWMControl );
	gIOPPluginControlClass				= OSSymbol::withCString( kIOPPluginControlClass );
	gIOPPluginSensorClass				= OSSymbol::withCString( kIOPPluginSensorClass );
	gIOPPluginEnvInternalOvertemp		= OSSymbol::withCString( kIOPPluginEnvInternalOvertemp );
	gIOPPluginEnvExternalOvertemp		= OSSymbol::withCString( kIOPPluginEnvExternalOvertemp );
	gIOPPluginEnvDynamicPowerStep		= OSSymbol::withCString( kIOPPluginEnvDynamicPowerStep );
	gIOPPluginEnvControlFailed			= OSSymbol::withCString( kIOPPluginEnvControlFailed );
	gIOPPluginCtrlLoopIDKey				= OSSymbol::withCString( kIOPPluginCtrlLoopIDKey );
	gIOPPluginCtrlLoopMetaState			= OSSymbol::withCString( kIOPPluginCtrlLoopMetaState );
	gIOPPluginThermalLocalizedDescKey	= OSSymbol::withCString( kIOPPluginThermalLocalizedDescKey );
	gIOPPluginThermalValidConfigsKey	= OSSymbol::withCString( kIOPPluginThermalValidConfigsKey );
	gIOPPluginThermalMetaStatesKey		= OSSymbol::withCString( kIOPPluginThermalMetaStatesKey );
	gIOPPluginPlatformID				= OSSymbol::withCString( kIOPPluginPlatformIDValue );
}

bool IOPlatformPlugin::initThermalProfile(IOService *nub)
{
	const OSDictionary *thermalProfile;

#ifdef PLUGIN_DEBUG
	const OSDictionary *dict;
	const OSString *string;
	const OSArray *array;
#endif

	//DLOG("IOPlatformPlugin::initThermalProfile - entered\n");

	// get the top of the thermal profile dictionary
	thermalProfile = OSDynamicCast(OSDictionary, getProperty(kIOPPluginThermalProfileKey));
	if (thermalProfile == NULL)
	{
		DLOG("IOPlatformPlugin::initThermalProfile failed to find thermal profile!!\n");
		return(false);
	}

#ifdef PLUGIN_DEBUG
	else
	{
		if ((string = OSDynamicCast(OSString, thermalProfile->getObject(kIOPPluginThermalCreationDateKey))) != NULL)
		{
#endif
			DLOG("IOPlatformPlugin::initThermalProfile found profile created %s\n", string->getCStringNoCopy());
#ifdef PLUGIN_DEBUG
		}
	}

	// Determine what config we've got
	if ((array = OSDynamicCast(OSArray, thermalProfile->getObject(kIOPPluginThermalConfigsKey))) != NULL &&
	    (dict = OSDynamicCast(OSDictionary, array->getObject(machineConfig))) != NULL &&
		(string = OSDynamicCast(OSString, dict->getObject(kIOPPluginThermalGenericDescKey))) != NULL)
	{
#endif
		DLOG("IOPlatformPlugin::initThermalProfile using config %s\n", string->getCStringNoCopy());
#ifdef PLUGIN_DEBUG
	}
	else
	{
#endif
		DLOG("IOPlatformPlugin::initThermalProfile profile does not contain Config %u information!\n", machineConfig);
#ifdef PLUGIN_DEBUG
	}
#endif

	// parse the control array
	if (!initControls( OSDynamicCast(OSArray, thermalProfile->getObject(kIOPPluginThermalControlsKey)) ))
	{
		DLOG("IOPlatformPlugin::initThermalProfile failure while parsing control array\n");
		return(false);
	}

	// parse the sensor array
	if (!initSensors( OSDynamicCast(OSArray, thermalProfile->getObject(kIOPPluginThermalSensorsKey)) ))
	{
		DLOG("IOPlatformPlugin::initThermalProfile failure while parsing sensor array\n");
		return(false);
	}

	// parse the ctrlloop array
	if (!initCtrlLoops( OSDynamicCast(OSArray, thermalProfile->getObject(kIOPPluginThermalCtrlLoopsKey)) ))
	{
		DLOG("IOPlatformPlugin::initThermalProfile failure while parsing ctrlloop array\n");
		return(false);
	}

	// done
	//DLOG("IOPlatformPlugin::initThermalProfile - done\n");
	return(true);
}

bool IOPlatformPlugin::initControls( const OSArray * controlDicts )
{
	const OSString * string;
	const OSDictionary * dict;
	const OSArray * array;
	IOPlatformControl * control;
	IOReturn result;
	int i, count;

	// Allocate the control lists and add the control info array to the registry
	controls = OSArray::withCapacity(0);
	controlInfoDicts = OSArray::withCapacity(0);
	setProperty(gIOPPluginControlDataKey, controlInfoDicts);

	if (controlDicts == NULL)
	{
		DLOG("IOPlatformPlugin::initControls no control array\n");

		// this is not a fatal error
		return(true);
	}

	count = controlDicts->getCount();
	for (i = 0; i < count; i++)
	{
		// grab the controlDicts element
		if ((dict = OSDynamicCast(OSDictionary, controlDicts->getObject(i))) == NULL)
		{
			DLOG("IOPlatformPlugin::initControls error grabbing controlDicts element %d, skipping\n", i);
			continue;
		}

		// Verify that this control is valid on this machine's config
		if ((array = OSDynamicCast(OSArray, dict->getObject(gIOPPluginThermalValidConfigsKey))) != NULL &&
		    !validOnConfig(array))
		{
			// found a ValidConfigs array, and the current config is not listed, so skip this control
			continue;
		}

		// Determine the class to use for this control object - defaults to IOPlatformControl
		if ((string = OSDynamicCast(OSString, dict->getObject(kIOClassKey))) == NULL)
		{
			//DLOG("IOPlatformPlugin::initControls using IOPlatformControl default class\n");
			string = gIOPPluginControlClass;
		}
		else
		{
			//DLOG("IOPlatformPlugin: control class %s\n", string->getCStringNoCopy());
		}

		// create the control object
		if ((control = OSDynamicCast(IOPlatformControl,
				OSMetaClass::allocClassWithName(string))) == NULL)
		{
			DLOG("IOPlatformPlugin::initControls failed to allocate %s\n", string->getCStringNoCopy());
			continue;
		}

		// initialize the control object
		if ((result = control->initPlatformControl(dict)) != kIOReturnSuccess)
		{
			DLOG("IOPlatformPlugin::initControls failed to init control object\n");
			control->release();
			continue;
		}

		// Add the object class to it's dictionary
		control->getInfoDict()->setObject(kIOClassKey, string);

		// Add this control to the control list
		controls->setObject(control);
	}

	return(true);
}

bool IOPlatformPlugin::initSensors( const OSArray * sensorDicts )
{
	const OSString * string;
	const OSDictionary * dict;
	const OSArray * array;
	IOPlatformSensor * sensor;
	IOReturn result;
	int i, count;

	// Allocate the sensor info array and put it in the registry
	sensors = OSArray::withCapacity(0);
	sensorInfoDicts = OSArray::withCapacity(0);
	setProperty(gIOPPluginSensorDataKey, sensorInfoDicts);

	if (sensorDicts == NULL)
	{
		DLOG("IOPlatformPlugin::initSensors no sensor array\n");

		// this is not a fatal error
		return(true);
	}

	count = sensorDicts->getCount();
	for (i = 0; i < count; i++)
	{
		// grab the array element
		if ((dict = OSDynamicCast(OSDictionary, sensorDicts->getObject(i))) == NULL)
		{
			DLOG("IOPlatformPlugin::initSensors error grabbing sensorDicts element %d, skipping\n", i);
			continue;
		}

		// Verify that this sensor is valid on this machine's config
		if ((array = OSDynamicCast(OSArray, dict->getObject(gIOPPluginThermalValidConfigsKey))) != NULL &&
		    !validOnConfig(array))
		{
			// found a ValidConfigs array, and the current config is not listed, so skip this sensor
			continue;
		}
	
		// Determine the class to use for this sensor object - defaults to IOPlatformSensor
		if ((string = OSDynamicCast(OSString, dict->getObject(kIOClassKey))) == NULL)
		{
			//DLOG("IOPlatformPlugin::initSensors using IOPlatformSensor default class\n");
			string = gIOPPluginSensorClass;
		}
		else
		{
			//DLOG("IOPlatformPlugin::initSensors sensor class %s\n", string->getCStringNoCopy());
		}

		// create the sensor object
		if ((sensor = OSDynamicCast(IOPlatformSensor,
				OSMetaClass::allocClassWithName(string))) == NULL)
		{
			DLOG("IOPlatformPlugin::initSensors failed to allocate %s\n", string->getCStringNoCopy());
			continue;
		}

		// initialize the sensor object
		if ((result = sensor->initPlatformSensor(dict)) != kIOReturnSuccess)
		{
			DLOG("IOPlatformPlugin::initSensors failed to init sensor object\n");
			sensor->release();
			continue;
		}

		// Add the object class to it's dictionary
		sensor->getInfoDict()->setObject(kIOClassKey, string);

		// Add this sensor to the sensor array
		sensors->setObject( sensor );
	}

	return(true);
}

bool IOPlatformPlugin::initCtrlLoops( const OSArray * ctrlLoopDicts )
{
	const OSString * string;
	const OSDictionary * dict;
	const OSArray * array;
	IOPlatformCtrlLoop * ctrlLoop;
	IOReturn result;
	int i, count;

	// allocate the ctrlloop lists and add the ctrlloop info array to the registry
	ctrlLoops = OSArray::withCapacity(0);
	ctrlLoopInfoDicts = OSArray::withCapacity(0);
	setProperty( gIOPPluginCtrlLoopDataKey, ctrlLoopInfoDicts );

	if (ctrlLoopDicts == NULL)
	{
		DLOG("IOPlatformPlugin::initCtrlLoops no ctrlloop array\n");

		// this is not a fatal error
		return(true);
	}

	count = ctrlLoopDicts->getCount();
	for (i = 0; i < count; i++)
	{
		// grab the array element
		if ((dict = OSDynamicCast(OSDictionary, ctrlLoopDicts->getObject(i))) == NULL)
		{
			DLOG("IOPlatformPlugin::initCtrlLoops error parsing ctrlLoopDicts element %d, skipping\n", i);
			continue;
		}

		// Verify that this ctrlloop is valid on this machine's config
		if ((array = OSDynamicCast(OSArray, dict->getObject(gIOPPluginThermalValidConfigsKey))) != NULL &&
		    !validOnConfig(array))
		{
			// found a ValidConfigs array, and the current config is not listed, so skip this ctrlloop
			continue;
		}

		// Determine the class to use for this CtrlLoop object - IOPlatformCtrlLoop must be subclassed!!
		if ((string = OSDynamicCast(OSString, dict->getObject(kIOClassKey))) == NULL)
		{
			DLOG("IOPlatformPlugin::initCtrlLoops no IOPlatformCtrlLoop subclass specified, skipping...\n");
			continue;
		}

		// create the CtrlLoop object
		if ((ctrlLoop = OSDynamicCast(IOPlatformCtrlLoop,
				OSMetaClass::allocClassWithName(string))) == NULL)
		{
			DLOG("IOPlatformPlugin::initCtrlLoops failed to allocate %s\n", string->getCStringNoCopy());
			continue;
		}

		// initialize the CtrlLoop object
		if ((result = ctrlLoop->initPlatformCtrlLoop(dict)) != kIOReturnSuccess)
		{
			DLOG("IOPlatformPlugin::initCtrlLoops failed to init CtrlLoop object\n");
			ctrlLoop->release();
			continue;
		}

		// Add the object class to it's dictionary
		ctrlLoop->getInfoDict()->setObject(kIOClassKey, string);

		// Add this ctrlloop to the ctrlloop array
		ctrlLoops->setObject(ctrlLoop);

		// control loops don't register, so just add this to the registry now
		ctrlLoopInfoDicts->setObject( ctrlLoop->getInfoDict() );
	}

	return(true);
}

#pragma mark
#pragma mark *** Machine Config Routines ***

/* Called in ::start(), subclasses should override this routine to properly determine the machine config */
UInt8 IOPlatformPlugin::probeConfig(void)
{
	return 0;	// generic class doesn't know anything about a machine's configs...
}

/* simple accessor for the (previously probed) machine config */
UInt8 IOPlatformPlugin::getConfig( void )
{
	return machineConfig;
}

/* a ValidConfigs array from the thermal profile can be passed into this routine, and a flag will be returned which tells whether the current config is listed in the array */
bool IOPlatformPlugin::validOnConfig( const OSArray * validConfigs )
{
	const OSNumber * number;
	int j, nconfigs;
	bool valid = false;

	// Verify that this control is valid on this machine's config
	if (validConfigs == NULL) return(false);

	nconfigs = validConfigs->getCount();
	for (j = 0; j < nconfigs; j++)
	{
		if ((number = OSDynamicCast(OSNumber, validConfigs->getObject(j))) != NULL &&
		     machineConfig == number->unsigned8BitValue())
		{
			valid = true;
			break;
		}
	}
	
	return( valid );
}

#pragma mark
#pragma mark *** Sensor / Control / CtrlLoop Lookup Routines ***

IOPlatformSensor *IOPlatformPlugin::lookupSensorByID( const OSNumber * sensorID ) const
{
	IOPlatformSensor *tmpSensor, *result = NULL;

	if (sensorID && sensors)
	{
		unsigned int i, count;

		count = sensors->getCount();
		for (i=0; i<count; i++)
		{
			tmpSensor = OSDynamicCast(IOPlatformSensor, sensors->getObject(i));

			if (tmpSensor && sensorID->isEqualTo( tmpSensor->getSensorID() ))
			{
				result = tmpSensor;
				break;
			}
		}
	}

	return(result);
}

IOPlatformControl *IOPlatformPlugin::lookupControlByID( const OSNumber * controlID ) const
{
	IOPlatformControl *tmpControl, *result = NULL;

	if (controlID && controls)
	{
		unsigned int i, count;

		count = controls->getCount();
		for (i=0; i<count; i++)
		{
			tmpControl = OSDynamicCast(IOPlatformControl, controls->getObject(i));

			if (controlID &&controlID->isEqualTo( tmpControl->getControlID() ))
			{
				result = tmpControl;
				break;
			}
		}
	}

	return(result);
}

IOPlatformCtrlLoop *IOPlatformPlugin::lookupCtrlLoopByID( const OSNumber * ctrlLoopID ) const
{
	IOPlatformCtrlLoop *tmpCtrlLoop, *result = NULL;

	if (ctrlLoopID && controls)
	{
		unsigned int i, count;

		count = ctrlLoops->getCount();
		for (i=0; i<count; i++)
		{
			tmpCtrlLoop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i));

			if (ctrlLoopID && ctrlLoopID->isEqualTo( tmpCtrlLoop->getCtrlLoopID() ))
			{
				result = tmpCtrlLoop;
				break;
			}
		}
	}

	return(result);
}

#pragma mark
#pragma mark *** Environment Helpers ***

// Put an arbitrary object into the environment dictionary with the supplied key
bool IOPlatformPlugin::setEnv( const OSString *aKey, const OSMetaClassBase *anObject )
{
	if (envInfo->setObject( aKey, anObject ))
	{
		envChanged = true;
		return(true);
	}
	else
	{
		return(false);
	}
}

// Put an arbitrary object into the environment dictionary with the supplied key
bool IOPlatformPlugin::setEnv( const char *aKey, const OSMetaClassBase *anObject )
{
	if (envInfo->setObject( aKey, anObject ))
	{
		envChanged = true;
		return(true);
	}
	else
	{
		return(false);
	}
}

// Put an arbitrary object into the environment dictionary with the supplied key
bool IOPlatformPlugin::setEnv( const OSSymbol *aKey, const OSMetaClassBase *anObject )
{
	if (envInfo->setObject( aKey, anObject ))
	{
		envChanged = true;
		return(true);
	}
	else
	{
		return(false);
	}
}

/*
 * Some environment conditions are set per-ctrlloop, so we can track which ctrl loops are causing
 * a condition to exists.  If the array is empty, none of the loops are exhibiting the condition.
 *
 * This routine looks for the condition key, and then sets the boolean flag for the indicated
 * caller.  So if a control loop wants to set the overtemp flag for itself, it can call:
 *
 * platformPlugin->setEnvArray( gOverTempSymbol, this, true );
 *
 * and the flag can be cleared by calling
 *
 * platformPlugin->setEnvArray( gOverTempSymbol, this, false );
 *
 */

bool IOPlatformPlugin::setEnvArray( const OSSymbol * aKey, const OSObject * setter, bool setting )
{
	OSArray * array;
	int i, count;

	if ((array = OSDynamicCast(OSArray, getEnv( aKey ))) != NULL)
	{
		// see if the ctrl loop is already listed in the array
		count = array->getCount();
		for (i=0; i<count; i++)
		{
			if (array->getObject(i) == setter) break;
		}

		// we found the ctrl loop and we want to remove it
		if (i<count && !setting)
		{
			array->removeObject(i);
			if (array->getCount() == 0)
			{
				envChanged = true;
			}
			return(true);
		}

		// we didn't find the ctrl loop and we want to add it
		if (i>=count && setting)
		{
			array->setObject(setter);
			envChanged = true;
			return(true);
		}
	}

	// something went wrong: the key doesn't exist in the env dict, or the ctrl loop
	// is already listed in the array and can't be added again, or the ctrl loop is
	// not already listed and cannot be removed
	return(false);
}

// return true if any ctrl loop has the indicated condition set to true
bool IOPlatformPlugin::envArrayCondIsTrue( const OSSymbol *cond ) const
{
	OSArray * array;

	if ((array = OSDynamicCast(OSArray, getEnv( cond ))) != NULL &&
		 array->getCount() > 0)
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// return true if the indicated ctrl loop has the indicated condition set to true
bool IOPlatformPlugin::envArrayCondIsTrueForObject( const OSObject * obj, const OSSymbol *cond ) const
{
	OSArray * array;
	int i, count;

	if ((array = OSDynamicCast(OSArray, getEnv( cond ))) != NULL)
	{
		count = array->getCount();
		for (i=0; i<count; i++)
		{
			if (array->getObject(i) == obj)
			{
				return(true);
			}
		}
	}

	return(false);
}

OSObject *IOPlatformPlugin::getEnv( const char *aKey ) const
{
	return envInfo->getObject( aKey );
}

OSObject *IOPlatformPlugin::getEnv( const OSString *aKey ) const
{
	return envInfo->getObject( aKey );
}

OSObject *IOPlatformPlugin::getEnv( const OSSymbol *aKey ) const
{
	return envInfo->getObject( aKey );
}

void IOPlatformPlugin::environmentChanged( void )
{
	IOPlatformCtrlLoop *loop;
	int i, count;

	// let the control loops update their meta state and, if necessary, adjust their controls
	if (ctrlLoops)
	{
		count = ctrlLoops->getCount();
		for (i=0; i<count; i++)
		{
			if ((loop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i))) != NULL)
			{
				//DLOG("IOPlatformPlugin::enviromentChanged notifying ctrlLoop %d\n", i);
				loop->updateMetaState();
				loop->adjustControls();
			}
		}
	}
}

#pragma mark
#pragma mark *** Miscellaneous Helpers ***

void IOPlatformPlugin::sleepSystem( void )
{
	DLOG("IOPlatformPlugin::sleepSystem issuing sleep demand\n");

	pmRootDomain->sleepSystem();
}

// Iterate the control loop list and find the closest deadline, if any.  Set the timer callback
// to fire at the deadline.
void IOPlatformPlugin::setTimeout( const AbsoluteTime now )
{
	AbsoluteTime loopDeadline, soonest;
	IOPlatformCtrlLoop *loop;
	int i, count, cmp;

	//DLOG("IOPlatformPlugin::setTimeout - entered\n");

	// find the soonest non-zero deadline
	if (ctrlLoops)
	{
		AbsoluteTime_to_scalar(&soonest) = 0;
		count = ctrlLoops->getCount();
		for (i=0; i<count; i++)
		{
			if ((loop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i))) != NULL)
			{
				loopDeadline = loop->getDeadline();

				if (AbsoluteTime_to_scalar(&loopDeadline) != 0)
				{
					if (CMP_ABSOLUTETIME(&loopDeadline, &now) == -1)
					{
						// There is a non-zero deadline that already passed.  This probably means that
						// something went wrong and this control loop didn't get it's timer callback.
						// If we get here, just schedule a thread callout for the soonest possible
						// dispatch.
						DLOG("IOPlatformPlugin::setTimeout ctrlloop %d's deadline already passed\n", i);
						thread_call_enter( timerCallout );
						return;
					}
					else
					{
						cmp = CMP_ABSOLUTETIME(&loopDeadline, &soonest);
						if ( AbsoluteTime_to_scalar(&soonest) == 0 || cmp == -1 )
						{
							AbsoluteTime_to_scalar(&soonest) = AbsoluteTime_to_scalar(&loopDeadline);
						}
					}
				}
			}
		}

		if (AbsoluteTime_to_scalar(&soonest) != 0)
		{
			thread_call_enter_delayed( timerCallout, soonest );
		}
	}
}

#pragma mark
#pragma mark *** Event Handling Helpers ***

IOReturn IOPlatformPlugin::handleEvent(IOPPluginEventData *event)
{
	IOReturn status;
	AbsoluteTime now;

	// Cancel any outstanding timer events.  The deadlines will be checked and the timer reset after this
	// event is serviced.
	thread_call_cancel(timerCallout);

	clock_get_uptime(&now);

	switch (event->eventType)
	{
		case IOPPluginEventTimer:
			timerHandler(now);
			status = kIOReturnSuccess;
			break;

		case IOPPluginEventMessage:
			status = messageHandler((UInt32)event->param1, OSDynamicCast(IOService, (OSMetaClassBase *) event->param2),
					OSDynamicCast(OSDictionary, (OSMetaClassBase *) event->param3));
			break;

		case IOPPluginEventSetAggressiveness:
			status = setAggressivenessHandler((unsigned long) event->param1, (unsigned long) event->param2);
			break;

		case IOPPluginEventSystemWillSleep:
			status = sleepHandler();
			break;

		case IOPPluginEventSystemDidWake:
			status = wakeHandler();
			break;

		case IOPPluginEventSystemRestarting:
			status = restartHandler();
			break;

		case IOPPluginEventSetProperties:
			status = setPropertiesHandler( OSDynamicCast(OSObject, (OSMetaClassBase *) event->param1) );
			break;

		case IOPPluginEventPlatformFunction:
		default:
			DLOG("IOPlatformPlugin::handleEvent got unknown event type\n");
			status = kIOReturnUnsupported;
			break;
	}

	// if there were any changes to the environment, notify control loops
	if (envChanged)
	{
		environmentChanged();

		// clear the environment changed flag
		envChanged = false;
	}

	// check deadlines and set the timer if necessary
	if (pluginPowerState != kIOPPluginSleeping) setTimeout(now);

	return(status);
}

IOReturn IOPlatformPlugin::dispatchEvent(IOPPluginEventData *event)
{
	IOReturn status;

	// close the gate
	IORecursiveLockLock(gate);

	// handle the event
	status = handleEvent(event);

	// open the gate
	IORecursiveLockUnlock(gate);

	return(status);
}

#pragma mark
#pragma mark *** Unsynchronized Entry Points ***

IOReturn IOPlatformPlugin::setAggressiveness(unsigned long selector, unsigned long newLevel)
{
	IOReturn result;
	IOPPluginEventData saEvent;

	result = super::setAggressiveness(selector, newLevel);

	saEvent.eventType = IOPPluginEventSetAggressiveness;
	saEvent.param1 = (void *) selector;
	saEvent.param2 = (void *) newLevel;

	dispatchEvent(&saEvent);

	return(result);
}

IOReturn IOPlatformPlugin::message( UInt32 type, IOService * provider, void * argument)
{
	IOPPluginEventData msgEvent;

	msgEvent.eventType = IOPPluginEventMessage;
	msgEvent.param1 = (void *) type;
	msgEvent.param2 = (void *) provider;
	msgEvent.param3 = (void *) argument;

	return dispatchEvent(&msgEvent);
}	

IOReturn IOPlatformPlugin::setProperties( OSObject * properties )
{
	IOPPluginEventData spEvent;

	spEvent.eventType = IOPPluginEventSetProperties;
	spEvent.param1 = (void *) properties;

	return dispatchEvent(&spEvent);
}

/* static */
void IOPlatformPlugin::timerEventOccured( void *self )
{
	IOPPluginEventData tEvent;

	IOPlatformPlugin * me = OSDynamicCast(IOPlatformPlugin, (OSMetaClassBase *) self);

	if (me)
	{
		tEvent.eventType = IOPPluginEventTimer;

		me->dispatchEvent(&tEvent);
	}
}

IOReturn IOPlatformPlugin::powerStateWillChangeTo( IOPMPowerFlags theFlags, unsigned long, IOService*)
{
	IOPPluginEventData powerStateEvent;
	IOReturn status = IOPMAckImplied;

    if ( ! (theFlags & IOPMPowerOn) )
	{
		DLOG("IOPlatformPlugin::powerStateWillChangeTo theFlags = 0x%X\n", theFlags);
		powerStateEvent.eventType = IOPPluginEventSystemWillSleep;
		status = dispatchEvent(&powerStateEvent);
    }

    return(status);
}

IOReturn IOPlatformPlugin::powerStateDidChangeTo( IOPMPowerFlags theFlags, unsigned long, IOService*)
{
	IOPPluginEventData powerStateEvent;
	IOReturn status = IOPMAckImplied;

    if ( theFlags & IOPMPowerOn )
	{
		DLOG("IOPlatformPlugin::powerStateDidChangeTo theFlags = 0x%X\n", theFlags);
		powerStateEvent.eventType = IOPPluginEventSystemDidWake;
		status = dispatchEvent(&powerStateEvent);
    }

    return(status);
}

IOReturn IOPlatformPlugin::sysPowerDownHandler(void *target, void *refCon, UInt32 messageType, IOService *service, void *messageArgument, vm_size_t argSize )
{
	IOPPluginEventData powerStateEvent;
	IOReturn status = IOPMAckImplied;

	IOPlatformPlugin * me = OSDynamicCast(IOPlatformPlugin, (OSMetaClassBase *) target);
	if (me == NULL)
		return(status);

    switch (messageType)
    {
        case kIOMessageSystemWillSleep:
            break;
            
        case kIOMessageSystemWillPowerOff: // iokit_common_msg(0x250)
        case kIOMessageSystemWillRestart: // iokit_common_msg(0x310)
			powerStateEvent.eventType = IOPPluginEventSystemRestarting;
			status = me->dispatchEvent(&powerStateEvent);
            break;

        default:
            break;
    }

    return(status);
}

#pragma mark
#pragma mark *** Synchronized Event Handlers ***

IOReturn IOPlatformPlugin::setAggressivenessHandler(unsigned long selector, unsigned long newLevel)
{
	if (selector == kPMSetProcessorSpeed)
	{
		const OSNumber * speed;

		DLOG("IOPlatformPlugin::setAggressivenessHandler Dynamic Power Step = %lx\n", newLevel);

		speed = OSNumber::withNumber( (unsigned long long) newLevel, 32 );
		setEnv(gIOPPluginEnvDynamicPowerStep, speed);
		speed->release();
	}

	return(IOPMAckImplied);
}


IOReturn IOPlatformPlugin::messageHandler(UInt32 type, IOService *sender, OSDictionary *dict)
{
	IOPlatformSensor * sensorRef;
	IOPlatformStateSensor * stateSensorRef;
	OSSerialize *s;

	switch (type)
	{
		case kIOPPluginMessageRegister:
			return registrationHandler( sender, dict );

		case kIOPPluginMessageLowThresholdHit:
		case kIOPPluginMessageHighThresholdHit:

			sensorRef = lookupSensorByID( OSDynamicCast(OSNumber, dict->getObject(gIOPPluginSensorIDKey)) );

			if ((stateSensorRef = OSDynamicCast(IOPlatformStateSensor, sensorRef)) != NULL)
			{
				//DLOG("IOPlatformPlugin::messageHander got threshold message\n");
				return stateSensorRef->thresholdHit( type == kIOPPluginMessageLowThresholdHit, dict );
			}

			break;

		case kIOPPluginMessageGetPlatformID:

			if ( dict )
			{
				dict->setObject( kIOPPluginPlatformIDKey, gIOPPluginPlatformID );
			}

			return kIOReturnSuccess;

			break;

		default:
			if ((s = OSSerialize::withCapacity(2048)) != NULL &&
			dict->serialize(s))
			{
				DLOG("IOPlatformPlugin::messageHandler - unknown message type %08lx\n"
				     "    sender: %s\n"
				     "    dict: %s\n", type, sender->getName(), s->text());

				s->release();
			}
			else
			{
				DLOG("IOPlatformPlugin::messageHandler unable to serialize\n");
			}
			break;
	}
		
	return(kIOReturnSuccess);
}

IOReturn IOPlatformPlugin::registrationHandler( IOService *sender, OSDictionary *dict )
{
	const OSNumber *id;
	IOPlatformSensor *sensor;
	IOPlatformControl *control;
	IOReturn status = kIOReturnError;

	//DLOG("IOPlatformPlugin::registrationHandler - entered\n");

	// sanity check args
	if (!sender || !dict)
	{
		DLOG("IOPlatformPlugin::registrationHandler got bad args!\n");
		return(kIOReturnBadArgument);
	}

	// Look for a sensor-id
	if ((id = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginSensorIDKey))) != NULL)
	{
		// Look for the sensor in the thermal profile info
		sensor = lookupSensorByID( id );

		if (sensor)	// sensor is already listed
		{
			// duplicate registration?
			if (sensor->isRegistered() == kOSBooleanTrue)
			{
				DLOG("IOPlatformPlugin::registrationHandler sensor with id 0x%lx already registered!!\n", id->unsigned32BitValue());
				return(kIOReturnPortExists);
			}

			DLOG("IOPlatformPlugin::registrationHandler got known entity, sensor id 0x%08lX\n",
					sensor->getSensorID()->unsigned32BitValue());

			status = sensor->registerDriver( sender, dict );

			if (status == kIOReturnSuccess)
				sensorInfoDicts->setObject( sensor->getInfoDict() );

			return(status);
		}
		else	// the registration is from an unlisted sensor
		{
#ifdef PLUGIN_DEBUG
			const OSNumber * tempID;
			tempID = OSDynamicCast(OSNumber, sender->getProperty(kIOPPluginSensorIDKey));
#endif
			DLOG("IOPlatformPlugin::registrationHandler got unknown entity, sensor id 0x%08lX\n",
				tempID != NULL ? tempID->unsigned32BitValue() : 0xDEADBEEF);

			// create the sensor object
			if ((sensor = OSDynamicCast(IOPlatformSensor,
					OSMetaClass::allocClassWithName(gIOPPluginSensorClass))) == NULL)
			{
				DLOG("IOPlatformPlugin::registrationHandler failed to allocate IOPlatformSensor\n");
				return(kIOReturnNoResources);
			}

			// initialize the sensor object
			if ((status = sensor->initPlatformSensor( sender, dict )) != kIOReturnSuccess)
			{
				DLOG("IOPlatformPlugin::registrationHandler failed to init sensor from unlisted registrant\n");
				sensor->release();
				return(status);
			}

			// add this new sensor to the list
			sensors->setObject( sensor );

			status = sensor->registerDriver( sender, dict );

			if (status == kIOReturnSuccess)
				sensorInfoDicts->setObject( sensor->getInfoDict() );

			return(status);
		}
	}

	// Look for a control-id
	else if ((id = OSDynamicCast(OSNumber, dict->getObject(gIOPPluginControlIDKey))) != NULL)
	{
		// Look for the control in the thermal profile info
		control = lookupControlByID( id );

		if (control) // control is already listed
		{
			// duplicate or conflict?
			if (control->isRegistered() == kOSBooleanTrue)
			{
				DLOG("IOPlatformPlugin::registrationHandler control with id 0x%lx already registered!!\n", id->unsigned32BitValue());
				return(kIOReturnPortExists);
			}

			DLOG("IOPlatformPlugin::registrationHandler got known entity, control id 0x%08lX\n",
					control->getControlID()->unsigned32BitValue());

			status = control->registerDriver( sender, dict );

#ifdef PLUGIN_DEBUG
			const OSString *name1, *name2;

			// flag a warning if the thermal profile and reported types don't match
			name1 = control->getControlType();
			name2 = OSDynamicCast(OSString, dict->getObject( kIOPPluginControlTypeKey ));

			if (name1 && name2 && !name1->isEqualTo(name2))
			{
#endif
				DLOG("IOPlatformPlugin::registrationHandler thermal profile overrode control type\n");
#ifdef PLUGIN_DEBUG
			}
#endif

			if (status == kIOReturnSuccess)
				controlInfoDicts->setObject( control->getInfoDict() );

			return status;
		}
		else	// the registration is from an unlisted control
		{
#ifdef PLUGIN_DEBUG
			const OSNumber * tempID;
			tempID = OSDynamicCast(OSNumber, sender->getProperty(kIOPPluginControlIDKey));
#endif
			DLOG("IOPlatformPlugin::registrationHandler got unknown entity, control id 0x%08lX\n",
				tempID != NULL ? tempID->unsigned32BitValue() : 0xDEADBEEF);

			// create the control object
			if ((control = OSDynamicCast(IOPlatformControl,
					OSMetaClass::allocClassWithName(gIOPPluginControlClass))) == NULL)
			{
				DLOG("IOPlatformPlugin::registrationHandler failed to allocate IOPlatformControl\n");
				return(kIOReturnNoResources);
			}

			if ((status = control->initPlatformControl( sender, dict )) != kIOReturnSuccess)
			{
				DLOG("IOPlatformPlugin::registrationHandler failed to init control from unlisted registrant\n");
				control->release();
				return(status);
			}

			// add this new control to the list
			controls->setObject( control );

			status = control->registerDriver( sender, dict );

			if (status == kIOReturnSuccess)
				controlInfoDicts->setObject( control->getInfoDict() );

			return(status);
		}
	}
	else
	{
		IOLog("IOPlatformPlugin got registration from unknown entity %s\n", sender->getName());
		return(kIOReturnUnsupported);
	}
}

IOReturn IOPlatformPlugin::setPropertiesHandler( OSObject * properties )
{
	IOReturn status = kIOReturnUnsupported;

#if IMPLEMENT_SETPROPERTIES
	OSDictionary * commandDict, * forceDict;
	const OSNumber * id, * num;
	ControlValue controlValue;
	IOPlatformSensor * sensor;
	IOPlatformControl * control;
	IOPlatformCtrlLoop * ctrlLoop;
	//OSSerialize *s;

	//DLOG("IOPlatformPlugin::setPropertiesHandler - entered\n");

	if ((commandDict = OSDynamicCast(OSDictionary, properties)) == NULL)
		return kIOReturnBadArgument;

	// look for a force-update request
	if (commandDict->getObject(gIOPPluginForceUpdateKey) != NULL)
	{
		//DLOG("IOPlatformPlugin::setProperties force-update\n");

		// force-update is accompanied by either a sensor-id or a ctrl-id
		if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginSensorIDKey))) != NULL)
		{
			if ((sensor = lookupSensorByID(id)) != NULL)
			{
				sensor->setCurrentValue( sensor->forceAndFetchCurrentValue() );
				status = kIOReturnSuccess;
			}
		}

		else if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginControlIDKey))) != NULL)
		{
			if ((control = lookupControlByID( id )) != NULL)
			{
				control->setCurrentValue( control->forceAndFetchCurrentValue() );
				status = kIOReturnSuccess;
			}
		}
	}

	// look for force-update-all request
	else if ((commandDict->getObject(gIOPPluginForceUpdateAllKey)) != NULL)
	{
		//DLOG("IOPlatformPlugin::setProperties force-update-all\n");

		int i, count;

		if (sensors)
		{
			count = sensors->getCount();
			for (i=0; i<count; i++)
			{
				//DLOG("IOPlatformPlugin::setProperties sensor %d\n", i);

				if ((sensor = OSDynamicCast(IOPlatformSensor, sensors->getObject(i))) != NULL)
				{
					sensor->setCurrentValue( sensor->forceAndFetchCurrentValue() );
				}
			}
		}

		if (controls)
		{
			count = controls->getCount();
			for (i=0; i<count; i++)
			{
				//DLOG("IOPlatformPlugin::setProperties control %d\n", i);

				if ((control = OSDynamicCast(IOPlatformControl, controls->getObject(i))) != NULL)
				{
					control->setCurrentValue( control->forceAndFetchCurrentValue() );
				}
			}
		}

		// everything is updated
		status = kIOReturnSuccess;
	}

/*
	else if ((commandDict->getObject(gIOPPluginForceSensorCurValKey)) != NULL)
	{
	}
*/

	// force a control value
	else if ((num = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginForceControlTargetValKey))) != NULL)
	{
		if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginControlIDKey))) != NULL &&
			(control = lookupControlByID(id)) != NULL)
		{
			controlValue = num->unsigned32BitValue();
			if (control->sendTargetValue( controlValue, true ))
			{
				control->getInfoDict()->setObject(gIOPPluginForceControlTargetValKey, num);
				//DLOG("IOPlatformPlugin Control ID 0x%08lX Forced to 0x%08lX\n", id->unsigned32BitValue(), controlValue);
				status = kIOReturnSuccess;
			}
		}
	}

	// release a forced control value
	else if ((num = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginReleaseForcedControlKey))) != NULL)
	{
		if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginControlIDKey))) != NULL &&
		    (control = lookupControlByID(id)) != NULL &&
			(control->getInfoDict()->getObject(gIOPPluginForceControlTargetValKey)) != NULL)
		{
			if (control->sendTargetValue( control->getTargetValue(), true ))
			{
				//DLOG("IOPlatformPlugin ControlID 0x%08lX Release Forced Value\n", id->unsigned32BitValue());
				control->getInfoDict()->removeObject(gIOPPluginForceControlTargetValKey);
				status = kIOReturnSuccess;
			}
		}
	}

	// force a control loop meta state
	else if ((forceDict = OSDynamicCast(OSDictionary, commandDict->getObject(gIOPPluginForceCtrlLoopMetaStateKey))) != NULL)
	{
		if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginCtrlLoopIDKey))) != NULL &&
			(ctrlLoop = lookupCtrlLoopByID(id)) != NULL)
		{
			ctrlLoop->getInfoDict()->setObject(gIOPPluginForceCtrlLoopMetaStateKey, forceDict);
			ctrlLoop->updateMetaState();
			status = kIOReturnSuccess;
		}
/*
		if ((s = OSSerialize::withCapacity(2048)) != NULL &&
			commandDict->serialize(s))
		{
			DLOG("IOPlatformPlugin::setPropertiesHandler force-ctrlloop-meta-state %s\n", s->text());
		}

		if (s) s->release();
*/
	}

	// release a forced control loop meta state
	else if (OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginReleaseForcedCtrlLoopKey)) != NULL)
	{
/*
		OSSerialize *s;

		if ((s = OSSerialize::withCapacity(2048)) != NULL &&
			commandDict->serialize(s))
		{
			DLOG("IOPlatformPlugin::setPropertiesHandler release-forced-ctrlloop %s\n", s->text());
		}

		if (s) s->release();
*/

		if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginCtrlLoopIDKey))) != NULL &&
		    (ctrlLoop = lookupCtrlLoopByID(id)) != NULL &&
			(ctrlLoop->getInfoDict()->getObject(gIOPPluginForceCtrlLoopMetaStateKey)) != NULL)
		{
			//DLOG("IOPlatformPlugin CtrlLoopID 0x%08lX Release Forced Ctrl Loop\n", id->unsigned32BitValue());
			ctrlLoop->getInfoDict()->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
			ctrlLoop->updateMetaState();
			status = kIOReturnSuccess;
		}
	}

	// DLOG("IOPlatformPlugin::setPropertiesHandler - done\n");

#endif  // IMPLEMENT_SETPROPERTIES

	return(status);
}

void IOPlatformPlugin::timerHandler( const AbsoluteTime now )
{
	AbsoluteTime loopDeadline;
	IOPlatformCtrlLoop *loop;
	int i, count;

	//DLOG("IOPlatformPlugin::timerHandler - entered\n");

	// check deadlines and call updateControls() on any control loop whose deadline has
	// passed ( and is non-zero ).
	if (ctrlLoops)
	{
		count = ctrlLoops->getCount();
		for (i=0; i<count; i++)
		{
			if ((loop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i))) != NULL)
			{
				loopDeadline = loop->getDeadline();

				if (AbsoluteTime_to_scalar(&loopDeadline) != 0 &&
				    CMP_ABSOLUTETIME(&loopDeadline, &now) <= 0)
				{
					//DLOG("IOPlatformPlugin::timerHandler ctrlLoop %d deadline passed\n", i);
					loop->deadlinePassed();
				}
			}
		}
	}
}

IOReturn IOPlatformPlugin::sleepHandler(void)
{

	DLOG("IOPlatformPlugin::sleepHandler - entered\n");

	pluginPowerState = kIOPPluginSleeping;

	return(IOPMAckImplied);
}

IOReturn IOPlatformPlugin::wakeHandler(void)
{

	IOPlatformCtrlLoop *loop;
	int i, count;

	DLOG("IOPlatformPlugin::wakeHandler - entered\n");

	// tell all the control loop we just woke up
	if (ctrlLoops)
	{
		count = ctrlLoops->getCount();
		for (i=0; i<count; i++)
		{
			if ((loop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i))) != NULL)
			{
				loop->didWake();
			}
		}
	}

	// force an update cycle for all ctrlloops by setting the environment changed flag
	envChanged = true;
	pluginPowerState = kIOPPluginRunning;

	return(IOPMAckImplied);
}

IOReturn IOPlatformPlugin::restartHandler(void)
{
	DLOG("IOPlatformPlugin::restartHandler - entered\n");

	pluginPowerState = kIOPPluginSleeping; 	// same as sleep

	return(IOPMAckImplied);
}
