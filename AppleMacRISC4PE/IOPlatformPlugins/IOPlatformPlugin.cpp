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
 *  DRI: Dave Radcliffe
 *
 */
//		$Log: IOPlatformPlugin.cpp,v $
//		Revision 1.13  2003/08/01 00:42:30  wgulland
//		Merging in branch PR-3338565
//		
//		Revision 1.12.2.1  2003/07/31 17:53:08  eem
//		3338565 - q37 intake fan speed is 97% of exhaust fan speed, but still gets
//		clipped at the same min/max.  This prevents the intake fan speed from falling
//		below 300 RPM and being turned off by the FCU.  Version bumped to 1.0.1b1.
//		
//		Revision 1.12  2003/07/24 21:47:16  eem
//		[3338565] Q37 Final Fan data
//		
//		Revision 1.11  2003/07/18 00:22:22  eem
//		[3329244] PCI fan conrol algorithm should use integral of power consumed
//		[3254911] Q37 Platform Plugin must disable debugging accessors before GM
//		
//		Revision 1.10  2003/07/17 06:57:36  eem
//		3329222 and other sleep stability issues fixed.
//		
//		Revision 1.9  2003/07/16 02:02:09  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.8  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.7  2003/06/25 02:16:24  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.6.4.1  2003/06/20 01:39:58  eem
//		Although commented out in this submision, there is support here to nap
//		the processors if the fans are at min, with the intent of keeping the
//		heat sinks up to temperature.
//		
//		Revision 1.6  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.5.2.10  2003/06/06 08:17:56  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.5.2.9  2003/06/04 10:21:10  eem
//		Supports forced PID meta states.
//		
//		Revision 1.5.2.8  2003/06/04 00:00:51  eem
//		More PID stuff, working towards support for forced meta states.
//		
//		Revision 1.5.2.7  2003/06/01 14:52:51  eem
//		Most of the PID algorithm is implemented.
//		
//		Revision 1.5.2.6  2003/05/31 09:02:06  eem
//		Fix deadline check.
//		
//		Revision 1.5.2.5  2003/05/31 08:11:34  eem
//		Initial pass at integrating deadline-based timer callbacks for PID loops.
//		
//		Revision 1.5.2.4  2003/05/29 03:51:34  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.5.2.3  2003/05/26 10:07:14  eem
//		Fixed most of the bugs after the last cleanup/reorg.
//		
//		Revision 1.5.2.2  2003/05/23 05:44:40  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.5.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.5  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.4.2.3  2003/05/17 11:08:22  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
//		
//		Revision 1.4.2.2  2003/05/16 07:08:45  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.4.2.1  2003/05/14 22:07:48  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.4  2003/05/13 02:13:51  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.3.2.1  2003/05/12 11:21:10  eem
//		Support for slewing.
//		
//		Revision 1.3  2003/05/10 06:50:33  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.2.2.4  2003/05/10 06:32:34  eem
//		Sensor changes, should be ready to merge to trunk as 1.0.1d12.
//		
//		Revision 1.2.2.3  2003/05/05 21:29:37  eem
//		Checkin 1.1.0d11 for PD distro and submission.  Debugging turned off.
//		
//		Revision 1.2.2.2  2003/05/03 01:11:38  eem
//		*** empty log message ***
//		
//		Revision 1.2.2.1  2003/05/01 09:28:40  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		Revision 1.2  2003/02/18 00:02:05  eem
//		3146943: timebase enable for MP, bump version to 1.0.1d3.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#include "IOPlatformPlugin.h"

#define super IOService
OSDefineMetaClassAndStructors(IOPlatformPlugin, IOService)

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

/*
 * start
 */
bool IOPlatformPlugin::start(IOService *nub)
{
	mach_timespec_t waitTimeout;
	AbsoluteTime now;
	const OSArray * tempArray;

	//DLOG("IOPlatformPlugin::start - entered\n");

	if (!super::start (nub)) goto failOnly;

	// store a pointer to ME!!
	platformPlugin = this;

	// set up symbols
	initSymbols();

	// globals used for one, zero, true, false
	gIOPPluginZero = OSNumber::withNumber( (unsigned long long) 0, 1);
	gIOPPluginOne = OSNumber::withNumber( (unsigned long long) 1, 1);

/*
    // Creates the Workloop and attaches all the event handlers to it:
    // ---------------------------------------------------------------
    workLoop = getWorkLoop();
    if (workLoop == NULL) {
		IOLog("IOPlatformPlugin::start failed to get a workloop\n");
        goto failReleaseSymbols;
    }

    // Creates the command gate for the events that need to be in the queue
    commandGate = IOCommandGate::commandGate(this, (IOCommandGate::Action)commandGateCaller);

    // and adds it to the workloop:
    if ((commandGate == NULL) ||
        (workLoop->addEventSource(commandGate) != kIOReturnSuccess))
    {
		IOLog("IOPlatformPlugin::start failed to initialize command gate\n");
        goto failReleaseCmdGate;
    }
*/
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

	// HELLO!!
	registerService();

	// Let the world know we're open for business
	publishResource ("IOPlatformMonitor", this);	// For backward compatibility
	publishResource ("IOPlatformPlugin", this);
	
	return(true);

/*
failReleaseCmdGate:
	if (commandGate)
	{
		commandGate->release();
		commandGate = NULL;
	}
*/

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
/*
	if (workLoop)
	{
		if (commandGate)
		{
			workLoop->removeEventSource(commandGate);
			commandGate->release();
			commandGate = NULL;
		}

		// workLoop->release();
		workLoop = NULL;
	}
*/
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

	if (controlDicts == NULL)
	{
		DLOG("IOPlatformPlugin::initControls no control array\n");

		// this is not a fatal error
		return(true);
	}

	// Allocate the control lists and add the control info array to the registry
	controls = OSArray::withCapacity(0);
	controlInfoDicts = OSArray::withCapacity(0);
	setProperty(gIOPPluginControlDataKey, controlInfoDicts);

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

	if (sensorDicts == NULL)
	{
		DLOG("IOPlatformPlugin::initSensors no sensor array\n");

		// this is not a fatal error
		return(true);
	}

	// Allocate the sensor info array and put it in the registry
	sensors = OSArray::withCapacity(0);
	sensorInfoDicts = OSArray::withCapacity(0);
	setProperty(gIOPPluginSensorDataKey, sensorInfoDicts);

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

	if (ctrlLoopDicts == NULL)
	{
		DLOG("IOPlatformPlugin::initCtrlLoops no ctrlloop array\n");

		// this is not a fatal error
		return(true);
	}

	// allocate the ctrlloop lists and add the ctrlloop info array to the registry
	ctrlLoops = OSArray::withCapacity(0);
	ctrlLoopInfoDicts = OSArray::withCapacity(0);
	setProperty( gIOPPluginCtrlLoopDataKey, ctrlLoopInfoDicts );

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

UInt8 IOPlatformPlugin::probeConfig(void)
{
	return 0;	// generic class doesn't know anything about a machine's configs...
}

UInt8 IOPlatformPlugin::getConfig( void )
{
	return machineConfig;
}

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

IOReturn IOPlatformPlugin::setPropertiesHandler( OSObject * properties )
{
	IOReturn status = kIOReturnUnsupported;

#ifdef PLUGIN_DEBUG
	OSDictionary * commandDict, * forceDict;
	const OSNumber * id, * value;
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
			if ((sensor = lookupSensorByID(id)) != NULL &&
			    (value = sensor->fetchCurrentValue()) != NULL)
			{
				//DLOG("IOPlatformPlugin::setProperties force-update sensor-id %u\n", id->unsigned16BitValue());

				// update current-value
				sensor->setCurrentValue( value );
				value->release();
				status = kIOReturnSuccess;
			}
		}

		else if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginControlIDKey))) != NULL)
		{
			if ((control = lookupControlByID( id )) != NULL &&
				(value = control->fetchCurrentValue()) != NULL)
			{
				//DLOG("IOPlatformPlugin::setProperties force-update control-id %u\n", id->unsigned16BitValue());

				// update current-value
				control->setCurrentValue( value );
				value->release();
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

				if ((sensor = OSDynamicCast(IOPlatformSensor, sensors->getObject(i))) != NULL &&
					(value = sensor->fetchCurrentValue()) != NULL)
				{
					sensor->setCurrentValue( value );
					value->release();
				}
			}
		}

		if (controls)
		{
			count = controls->getCount();
			for (i=0; i<count; i++)
			{
				DLOG("IOPlatformPlugin::setProperties control %d\n", i);

				if ((control = OSDynamicCast(IOPlatformControl, controls->getObject(i))) != NULL &&
				    (value = control->fetchCurrentValue()) != NULL)
				{
					DLOG("IOPlatformPlugin::setProperties control %d value 0x%08lX\n", i, value->unsigned32BitValue());
					
					control->setCurrentValue( value );
					value->release();
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
	else if ((value = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginForceControlTargetValKey))) != NULL)
	{
		if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginControlIDKey))) != NULL &&
			(control = lookupControlByID(id)) != NULL)
		{
			if (control->sendTargetValue( value, true ))
			{
				control->getInfoDict()->setObject(gIOPPluginForceControlTargetValKey, value);

				DLOG("IOPlatformPlugin Control ID 0x%08lX Forced to 0x%08lX\n", id->unsigned32BitValue(),
						value->unsigned32BitValue());
			}
		}
	}

	// release a forced control value
	else if ((value = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginReleaseForcedControlKey))) != NULL)
	{
		if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginControlIDKey))) != NULL &&
		    (control = lookupControlByID(id)) != NULL &&
			(control->getInfoDict()->getObject(gIOPPluginForceControlTargetValKey)) != NULL)
		{
			if (control->sendTargetValue( control->getTargetValue(), true ))
			{
				DLOG("IOPlatformPlugin ControlID 0x%08lX Release Forced Value\n", id->unsigned32BitValue());
				control->getInfoDict()->removeObject(gIOPPluginForceControlTargetValKey);
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
	else if ((value = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginReleaseForcedCtrlLoopKey))) != NULL)
	{
		OSSerialize *s;

		if ((s = OSSerialize::withCapacity(2048)) != NULL &&
			commandDict->serialize(s))
		{
			DLOG("IOPlatformPlugin::setPropertiesHandler release-forced-ctrlloop %s\n", s->text());
		}

		if (s) s->release();

		if ((id = OSDynamicCast(OSNumber, commandDict->getObject(gIOPPluginCtrlLoopIDKey))) != NULL &&
		    (ctrlLoop = lookupCtrlLoopByID(id)) != NULL &&
			(ctrlLoop->getInfoDict()->getObject(gIOPPluginForceCtrlLoopMetaStateKey)) != NULL)
		{
			DLOG("IOPlatformPlugin CtrlLoopID 0x%08lX Release Forced Ctrl Loop\n", id->unsigned32BitValue());
			ctrlLoop->getInfoDict()->removeObject(gIOPPluginForceCtrlLoopMetaStateKey);
			ctrlLoop->updateMetaState();
		}
	}

	// DLOG("IOPlatformPlugin::setPropertiesHandler - done\n");

#endif  // PLUGIN_DEBUG

	return(status);
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
			tempID = OSDynamicCast(OSNumber, sender->getProperty("sensor-id"));
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
			if ((status = sensor->initPlatformSensor(sender)) != kIOReturnSuccess)
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
		else
		{
			DLOG("IOPlatformPlugin::registrationHandler unrecognized control ID %08lX\n", id->unsigned32BitValue());
			return status;
		}
	}
	else
	{
		IOLog("IOPlatformPlugin got registration from unknown entity %s\n", sender->getName());
		return(kIOReturnUnsupported);
	}
}

IOReturn IOPlatformPlugin::sleepHandler(void)
{

/*
	// Sleep sequence:
	kprintf("IOPlatformPlugin::powerStateWillChangeTo (currently unsupported!!) to acknowledge power changes (DOWN) we set napping false\n");
	IOLog("IOPlatformPlugin::powerStateWillChangeTo (currently unsupported!!) to acknowledge power changes (DOWN) we set napping false\n");

	// xxx- this is placeholder code.  We need to make this happen for each cpu
	// rememberNap = ml_enable_nap(getCPUNumber(), false);        // Disable napping (the function returns the previous state)
*/

	DLOG("IOPlatformPlugin::sleepHandler - entered\n");

	pluginPowerState = kIOPPluginSleeping;

	return(IOPMAckImplied);
}

IOReturn IOPlatformPlugin::wakeHandler(void)
{
/*
	// Wake sequence:
	kprintf("IOPlatformPlugin::powerStateWillChangeTo (currently unsupported!!) to acknowledge power changes (UP) we set napping %s\n", 
		rememberNap ? "true" : "false");
	IOLog("IOPlatformPlugin::powerStateWillChangeTo (currently unsupported!!) to acknowledge power changes (UP) we set napping %s\n", 
		rememberNap ? "true" : "false");

	// xxx- this is placeholder code.  We need to make this happen for each cpu
	// ml_enable_nap(getCPUNumber(), rememberNap); 		   // Re-set the nap as it was before.
*/

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


/* static */
/*
IOReturn IOPlatformPlugin::commandGateCaller(OSObject *object, void *arg0, void *arg1, void *arg2, void *arg3)
{
	IOPlatformPlugin *me;
	IOPPluginEventData *event;

	if ((me = OSDynamicCast(IOPlatformPlugin, object)) == NULL ||
	    (event = (IOPPluginEventData *) arg0) == NULL)
	{
		DLOG("IOPlatformPlugin::commandGateCaller invalid args\n");
		return(kIOReturnBadArgument);
	}

	return me->handleEvent(event);
}
*/

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

void IOPlatformPlugin::sleepSystem( void )
{
	DLOG("IOPlatformPlugin::sleepSystem issuing sleep demand\n");

	pmRootDomain->sleepSystem();
}

