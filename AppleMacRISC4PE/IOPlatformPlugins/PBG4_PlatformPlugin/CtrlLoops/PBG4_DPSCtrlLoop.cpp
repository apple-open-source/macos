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
 *  File: $Id: PBG4_DPSCtrlLoop.cpp,v 1.11 2005/01/06 19:31:50 raddog Exp $
 *
 */


#include <IOKit/IOLib.h>
#include <ppc/machine_routines.h>

#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"

#include "PBG4_PlatformPlugin.h"
#include "PBG4_DPSCtrlLoop.h"

#define super IOPlatformCtrlLoop
OSDefineMetaClassAndStructors( PBG4_DPSCtrlLoop, IOPlatformCtrlLoop )

#pragma mark -
#pragma mark ***    Initialization   ***
#pragma mark -

/*******************************************************************************
 * Method:
 *	initPlatformCtrlLoop
 *
 * Purpose:
 *	Fetch control loop parameters (from thermal profile, SDB, etc.) and setup
 *	control loop initial state.
 ******************************************************************************/
// virtual
IOReturn PBG4_DPSCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
	char			functionName[64];
	OSData			*pHandle;
	OSNumber		*watts;
	UInt32			pHandleValue;
	IOService 		*resources;
	IOReturn		result;
	
	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::initPlatformCtrlLoop entered\n" );
	
	// Find pHandle provided by platform expert
	if (pHandle = OSDynamicCast (OSData, gPlatformPlugin->getProvider()->getProperty (kAAPLPHandle))) 
		pHandleValue = *(UInt32 *)pHandle->getBytesNoCopy();
	else
		pHandleValue = 0;
	
	// Find service that handles cpu voltage
	if (gPlatformPlugin->getProvider()->getProperty (kCpuVCoreSelect)) {
		sprintf(functionName, "%s-%08lx", kCpuVCoreSelect, pHandleValue);
		fCpuVoltageControlSym = OSSymbol::withCString(functionName);
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::initPlatformCtrlLoop waiting for function '%s'\n", functionName );
		resources = gPlatformPlugin->waitForService(gPlatformPlugin->resourceMatching(fCpuVoltageControlSym));
		if (resources) {
			fCpuVoltageControlService = OSDynamicCast (IOService, resources->getProperty(fCpuVoltageControlSym));
		}
	} else {
		IOLog ("PBG4_DPSCtrlLoop::initPlatformCtrlLoop no property '%s' - Dynamic Power Step will NOT be supported\n",  kCpuVCoreSelect);
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::initPlatformCtrlLoop no property '%s'\n",  kCpuVCoreSelect);
		return kIOReturnError;
	}
	
	// lookup the cpu temp sensor by key kCPUTempSensorDesc
	if (!(fCpuTempSensor = lookupAndAddSensorByKey (kCPUTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_DPSCtrlLoop::initPlatformCtrlLoop no CPU sensor!!\n");

	// lookup the battery temp sensor by key kBatteryTempSensorDesc
	if (!(fBattTempSensor = lookupAndAddSensorByKey (kBatteryTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_DPSCtrlLoop::initPlatformCtrlLoop no battery sensor!!\n");

	// lookup the track pad temp sensor by key kTrackPadTempSensorDesc
	if (!(fTPadTempSensor = lookupAndAddSensorByKey (kTrackPadTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_DPSCtrlLoop::initPlatformCtrlLoop no track pad sensor!!\n");

	// lookup the Uni-N temp sensor by key kUniNTempSensorDesc
	if (!(fUniNTempSensor = lookupAndAddSensorByKey (kUniNTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_DPSCtrlLoop::initPlatformCtrlLoop no Uni-N temp sensor!!\n");

	// lookup the power supply temp sensor by key kPwrSupTempSensorDesc
	if (!(fPwrSupTempSensor = lookupAndAddSensorByKey (kPwrSupTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_DPSCtrlLoop::initPlatformCtrlLoop no power supply sensor!!\n");

	watts = OSDynamicCast (OSNumber, dict->getObject (kCtrlLoopPowerAdapterBaseline));
	if (watts) {
		fBaselineWatts = watts->unsigned32BitValue();
		// CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::initPlatformCtrlLoop got baseline watts %d\n", fBaselineWatts );
	} else {
		fBaselineWatts = 0xFF;
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::initPlatformCtrlLoop no baseline watts - defaulting to worst case\n" );
	}

	result = super::initPlatformCtrlLoop (dict);

	fUsePowerPlay = gPlatformPlugin->fUsePowerPlay;
	
	// pmRootDomain is the recipient of GPU controller messages
	fPMRootDomain = OSDynamicCast(IOPMrootDomain, 
		gPlatformPlugin->waitForService(gPlatformPlugin->serviceMatching("IOPMrootDomain")));
				
	if (result == kIOReturnSuccess)
		(void) setInitialState ();
	
	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::initPlatformCtrlLoop done\n" );
	return result;
}

#pragma mark -
#pragma mark ***     Control Loop Primitives     ***
#pragma mark -

/*******************************************************************************
 * Method:
 *	setProcessorVoltage
 *
 * Purpose:
 *	Set the processor voltage high or low for vStep
 * stepValue == 0 => run fast (voltage high)
 * stepValue == 1 => run slow (voltage low)
 *
 *	NOTE - this is opposite the sense of how the GPIO is set:
 * gpioData == 1 (true)  => run fast (voltage high)
 * gpioData == 0 (false) => run slow (voltage low)
 ******************************************************************************/
bool PBG4_DPSCtrlLoop::setProcessorVoltage( UInt32 stepValue )
{
	if (fCpuVoltageControlSym && fCpuVoltageControlService)
		fCpuVoltageControlService->callPlatformFunction (fCpuVoltageControlSym, false, 
			(void *) (stepValue == 0), 0, 0, 0);

	// Wait for power to settle when raising voltage
	if (stepValue == 0)
		IODelay (600);		// 600 µseconds
		
	return true;
}

/*******************************************************************************
 * Method:
 *	setProcessorVStepValue
 *
 * Purpose:
 *	Set the processor stepping to the desired stepping value.
 * metaState == 0 => run fast	(DP)
 * metaState == 1 => run slow	(DP)
 *
 ******************************************************************************/
bool PBG4_DPSCtrlLoop::setProcessorVStepValue( const OSNumber *newMetaState )
{
	bool		newUserStateSlow, prevUserStateSlow;
	
	//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - entered\n");
	if (newMetaState->isEqualTo(fPreviousMetaState)) {
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - no change in metaState - exiting\n");
		return true;
	}

	// Determine new and old conditions
	newUserStateSlow = (newMetaState->isEqualTo(gIOPPluginOne) || newMetaState->isEqualTo(gIOPPluginThree));
	prevUserStateSlow = (fPreviousMetaState->isEqualTo(gIOPPluginOne) || fPreviousMetaState->isEqualTo(gIOPPluginThree));
	// If going fast, raise voltage first
	if (!newUserStateSlow) {
		// Step the processor voltage up
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - stepping voltage up\n");
		setProcessorVoltage (0);
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - step voltage up done\n");
	}
	
	if (prevUserStateSlow != newUserStateSlow) {
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - setting processor speed %d\n",
			//newUserStateSlow ? 1 : 0);
#if defined( __ppc__ )
		ml_set_processor_speed( newUserStateSlow ? 1 : 0 );
#endif
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - done setting processor speed\n");
		IODelay (300);		// 300 µseconds
	} // else CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - no processor speed change required\n");

	// If going slow, lower voltage after setting speed
	if (newUserStateSlow) {
		// Step the processor voltage down
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - stepping voltage down\n");
		setProcessorVoltage (1);
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - step voltage down done\n");
	}

	fPreviousMetaState = newMetaState;			// Remember state
	
	//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setProcessorVStepValue - done\n");
	return( true );
}

/*******************************************************************************
 * Method:
 *	setGraphicsProcessorStepValue
 *
 * Purpose:
 *	Set the graphics processor stepping to the desired stepping value.
 * metaState == 0 => run fast
 * metaState == 1 => run slow
 *
 * This is not based on load but based on user setting "Reduced" in the
 * Energy Saver preferences.  This is transmitted to us in the high bit
 * (kSetAggrUserAuto) of the setAggressiveness call
 ******************************************************************************/
bool PBG4_DPSCtrlLoop::setGraphicsProcessorStepValue( const OSNumber *newGPUMetaState )
{
	bool		newUserStateSlow, prevUserStateSlow;

	// Determine new and old conditions
	newUserStateSlow = (newGPUMetaState->isEqualTo(gIOPPluginOne) || newGPUMetaState->isEqualTo(gIOPPluginThree));
	prevUserStateSlow = (fPreviousGPUMetaState->isEqualTo(gIOPPluginOne) || fPreviousGPUMetaState->isEqualTo(gIOPPluginThree));

	if (prevUserStateSlow != newUserStateSlow) {
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setGraphicsProcessorStepValue - sending %d\n",
			//newUserStateSlow ? 3 : 0);
			
		// Change GPU accordingly
		fPMRootDomain->setAggressiveness (kIOFBLowPowerAggressiveness, newUserStateSlow ? 3 : 0);
	
		fPreviousGPUMetaState = newGPUMetaState;			// Remember state
	}
	
	return true;
	
}

/*******************************************************************************
 * Method:
 *	batteryIsOvercurrent
 *
 ******************************************************************************/
bool PBG4_DPSCtrlLoop::batteryIsOvercurrent( void )
{
	bool	envBatOvercurrent;
	
	if (!gPlatformPlugin->fEnvDataIsValid)
		// No battery data yet, assume we're OK
		return false;
	
	// Get the currently reported global environment state of the battery current
	envBatOvercurrent = (gPlatformPlugin->getEnv(gIOPPluginEnvBatteryOvercurrent) == kOSBooleanTrue);
	
	//If we're in an overcurrent situation re-evaluate it once the deadline expires
	if (fBatteryIsOvercurrent) {
		if (AbsoluteTime_to_scalar(&deadline) == 0) {
			// Deadline has expired, clear it unless the env overcurrent says otherwise
			fBatteryIsOvercurrent = envBatOvercurrent;
			CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::batteryIsOvercurrent - deadline expired, setting overcurrent '%s'\n",
				fBatteryIsOvercurrent ? "true" : "false");
		}
	} else {
		// Not currently in overcurrent
		if (envBatOvercurrent) {
			// Setup a deadline (kOvercurrentInterval seconds) for the overcurrent to expire
			clock_interval_to_deadline( kOvercurrentInterval /* seconds */, NSEC_PER_SEC, 
				&deadline );
				
			fBatteryIsOvercurrent = true;
			CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::batteryIsOvercurrent - setting overcurrent 'true'\n");
		}
	}
	
	return fBatteryIsOvercurrent;	
}

/*******************************************************************************
 * Method:
 *	lowPowerForcedDPS
 *
 ******************************************************************************/
bool PBG4_DPSCtrlLoop::lowPowerForcedDPS( void )
{
	OSNumber	*powerStatus;
	UInt32		powerVal, curWatts;
	bool		result;
	
	result = false;
	
	powerStatus = OSDynamicCast (OSNumber, gPlatformPlugin->getEnv (gIOPPluginEnvPowerStatus));
	if (powerStatus) {
		powerVal = powerStatus->unsigned32BitValue();
		curWatts = (powerVal >> 24) & 0xFF;
		if (!((curWatts > 0)  &&
			(curWatts >= fBaselineWatts)) ||
			((curWatts == 0) && 
			((powerVal & (kIOPMACInstalled | kIOPMACnoChargeCapability)) == 
				(kIOPMACInstalled | kIOPMACnoChargeCapability)))) {
		
			CTRLLOOP_DLOG ("PBG4_DPSCtrlLoop::lowPowerForcedDPS - got powerStatus 0x%x, curWatts %d\n", powerVal, curWatts);
			if (((powerVal & (kIOPMACInstalled | kIOPMACnoChargeCapability)) == 
				(kIOPMACInstalled | kIOPMACnoChargeCapability)) ||
				((powerVal & (kIOPMRawLowBattery | kIOPMBatteryDepleted)) != 0) ||
				((powerVal & kIOPMBatteryInstalled) == 0)) {
					CTRLLOOP_DLOG ("PBG4_DPSCtrlLoop::lowPowerForcedDPS - forcing reduced speed\n");
					result = true;							// force low speed
			}
		}
	}
	
	return result;
}


#pragma mark -
#pragma mark ***     High Level Transition Logic     ***
#pragma mark -

/*******************************************************************************
 * Method:
 *	setInitialState
 *
 * Purpose:
 *	At the end of initialization, we set the stepping and voltage to known
 *	initial settings. Initial state is based on dynamic power step and
 *	whether or not we can step and switch voltage. At this early point we'll
 *	never have a temp sensor or a slew controller available yet.
 *
 ******************************************************************************/
bool PBG4_DPSCtrlLoop::setInitialState( void )
{
	bool			success = false;
	
	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setInitialState entered\n" );

	fPreviousMetaState = gIOPPluginOne;			// Start out in low state
	
	setMetaState( fPreviousMetaState );			// Remember the initial state

	fPreviousGPUMetaState = gIOPPluginZero;		// Graphics starts out high

	ctrlloopState = kIOPCtrlLoopFirstAdjustment;

	success = updateMetaState();
	
	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::setInitialState returning %s\n", success ? "true" : "false" );

	return success;
}

/*******************************************************************************
 * Method:
 *	adjustControls
 *
 * Purpose:
 *	Implements high-level slewing/stepping algorithm and performs system sleep
 *	check(s).
 *
 ******************************************************************************/
void PBG4_DPSCtrlLoop::adjustControls( void )
{
	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::adjustControls entered\n" );

	updateMetaState();		// updateMetaState does the work for us
		
	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::adjustControls done\n" );
	return;
}

/*******************************************************************************
 * Method:
 *	updateMetaState
 *
 * Purpose:
 *	Examine environmental conditions and choose DPSCtrlLoop's meta state.
 *
 ******************************************************************************/
// virtual
bool PBG4_DPSCtrlLoop::updateMetaState( void )
{
	OSNumber		*tmpNumber;
	const OSNumber	*newMetaState, *newGPUMetaState;
	UInt32			dpsTarget;
	bool			isIdle, didAct;
	
	if (ctrlloopState == kIOPCtrlLoopNotReady) {
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState entered with control loop not ready\n");
		return false;
	}
	
	if (ctrlloopState == kIOPCtrlLoopWillSleep) {
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState entered with control loop in sleep state - exiting\n" );
		return false;
	}
	
	//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState entered\n" );

	// fetch the current Dynamic Power Step setting
	tmpNumber = OSDynamicCast( OSNumber, gPlatformPlugin->getEnv( gIOPPluginEnvDynamicPowerStep ) );
	dpsTarget = tmpNumber ? tmpNumber->unsigned32BitValue() : 0;
	
	/*
	 * Check idle state - based on no events happening in kIdleInterval when we're on battery
	 * Only do this if we are DFS low and not in an overcurrent situation, i.e., the idleTimer is active
	 * Once the deadline reaches zero, we consider ourselves in the idle state until fIdleTimerActive is cleared
	 */
	isIdle = false;
	if ((gPlatformPlugin->getEnv(gIOPPluginEnvACPresent) == kOSBooleanFalse) && fIdleTimerActive)
		isIdle = (AbsoluteTime_to_scalar(&deadline) == 0); 

	/*
	 * gIOPPluginEnvUserPowerAuto being true indicates user set "Automatic" in Energy Saver Preferences
	 * If it's false then the user has chosen either "Highest" or "Reduced".  If it's "Reduced", then
	 * our response to that is to request the graphics processor run slower
	 *
	 * If fUsePowerPlay is false we never go slow on graphics
	 */
	if ((gPlatformPlugin->getEnv(gIOPPluginEnvUserPowerAuto) == kOSBooleanTrue) || !fUsePowerPlay) {
		if (!fUsePowerPlay)
			// Stay with highest
			newGPUMetaState = gIOPPluginZero;
		else {
			if (isIdle)
				CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState GPU low based on idle\n");

			// Base it on the idle state
			newGPUMetaState = isIdle ? gIOPPluginOne : gIOPPluginZero;
		}
	} else
		// Use whatever user set
		newGPUMetaState = dpsTarget ? gIOPPluginOne : gIOPPluginZero;
	
	// Start out in 0 - the highest metaState or 1 if DFS requests it
	newMetaState = dpsTarget ? gIOPPluginOne : gIOPPluginZero;
	
	if (!fIdleTimerActive && batteryIsOvercurrent()) {
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState - battery overcurrent forced metastate 1\n");
		newMetaState = gIOPPluginOne;
	}
	
	/*
	 * Get thermal sensor states.  The current model here is that if *any* sensors report a thermal
	 * state >= 1 then we DFS low.  There is more resolution than states 0 & 1 in the state sensors
	 * but for now all the two states are all we care about
	 */
	// fetch the current cpu0 temperature state
	if (safeGetState(fCpuTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState CPU0 non-zero temp state 0x%lx\n", safeGetState(fCpuTempSensor));
	}

	// fetch the current batt temperature state
	if (safeGetState(fBattTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState battery non-zero temp state 0x%lx\n", safeGetState(fBattTempSensor));
	}

	// fetch the current trackpad temperature state
	if (safeGetState(fTPadTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState trackpad non-zero temp state 0x%lx\n", safeGetState(fTPadTempSensor));
	}
	
	// fetch the current Uni-N temperature state
	if (safeGetState(fUniNTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState Uni-N non-zero temp state 0x%lx\n", safeGetState(fUniNTempSensor));
	}
	
	// fetch the current power supply temperature state
	if (safeGetState(fPwrSupTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState power supply non-zero temp state 0x%lx\n", safeGetState(fPwrSupTempSensor));
	}
	
	// Check for low power conditions
	if (lowPowerForcedDPS ()) {
		newMetaState = gIOPPluginOne;
	}

	//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState - final metaState %d\n", newMetaState->unsigned32BitValue() );
	didAct = false;
	
	// Step processor, if necessary
	if (!newMetaState->isEqualTo(getMetaState())) {
		setMetaState( newMetaState );	// Set the new state
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState calling setProcessorVStepValue(0x%lx)\n", 
			newMetaState->unsigned32BitValue());
		setProcessorVStepValue (newMetaState);
		
		// If we're not in an overcurrent situation, handle idle timers
		if (!fBatteryIsOvercurrent) {
			// If the new state is high, clear any idle timer
			if (newMetaState->isEqualTo(gIOPPluginZero)) {
				// Clear out deadline so we don't get called because of the idle timer
				super::deadlinePassed();		// Use the IOPlatformCtrlLoop implementation so we don't get re-entered
				// CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState - state high - clearing idle timer\n");
				fIdleTimerActive = false;
			} else {
				// The new state is low so, start the idle timer
				// Setup a deadline (kIdleInterval seconds) for the activity monitor to expire
				// CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState - state low - setting idle timer\n");
				clock_interval_to_deadline( kIdleInterval /* seconds */, NSEC_PER_SEC, 
					&deadline );
				fIdleTimerActive = true;
			}
		}
		didAct = true;
	}
	
	// Step graphics processor, if necessary
	if (!newGPUMetaState->isEqualTo(fPreviousGPUMetaState)) {
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::updateMetaState calling setGraphicsProcessorStepValue(0x%lx)\n", 
			newGPUMetaState->unsigned32BitValue());
		setGraphicsProcessorStepValue (newGPUMetaState);
		didAct = true;
	}

	return(didAct);					// Signal the change
}

#pragma mark -
#pragma mark ***     Helpers / Entry Points     ***
#pragma mark -

/*******************************************************************************
 * Method:
 *	willSleep
 *
 * Purpose:
 *	This is a virtual method that's called when the system is going down to sleep
 ******************************************************************************/
// virtual
void PBG4_DPSCtrlLoop::willSleep( void )
{
	if (ctrlloopState == kIOPCtrlLoopNotReady) {
		CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::willSleep entered with control loop not ready\n");
		return;
	}

 	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::willSleep entered\n");

	super::willSleep();
	fSavedMetaState = getMetaState();	// Save state
	
	setMetaState( gIOPPluginZero );		// Force highspeed
	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::willSleep calling setProcessorVStepValue(0)\n");
	setProcessorVStepValue (gIOPPluginZero);
	
 	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::willSleep done\n");
	return;
}

/*******************************************************************************
 * Method:
 *	didWake
 *
 * Purpose:
 *	This is a virtual method that's called when the system is waking from sleep
 ******************************************************************************/
// virtual
void PBG4_DPSCtrlLoop::didWake( void )
{
	if (ctrlloopState == kIOPCtrlLoopNotReady) {
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::didWake entered with control loop not ready\n");
		return;
	}
	
	if (!fSavedMetaState) {
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::didWake entered with no savedMetaState - exiting\n");
		return;
	}

 	//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::didWake entered\n");
	setMetaState( fSavedMetaState );		// Restore previous state
	//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::didWake calling setProcessorVStepValue(0x%lx)\n", 
		//fSavedMetaState->unsigned32BitValue());
	setProcessorVStepValue (fSavedMetaState);

    super::didWake();
	
	updateMetaState();						// Set new state based on current conditions

 	CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::didWake done\n");
	return;
}

/*******************************************************************************
 * Method:
 *	deadlinePassed
 *
 * Purpose:
 *	When a battery overcurrent event occurs we set an interval timer to clear
 *	it later.  We get called here when that happens.  The deadline is set in
 *	batteryIsOvercurrent()
 *******************************************************************************/
// virtual
void PBG4_DPSCtrlLoop::deadlinePassed( void )
{
	super::deadlinePassed();		// clear the deadline
	updateMetaState();				// Re-evaluate the situation
	
	return;
}

