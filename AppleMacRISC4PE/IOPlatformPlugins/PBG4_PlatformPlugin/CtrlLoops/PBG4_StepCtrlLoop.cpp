/*
 * Copyright (c) 2004-2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PBG4_StepCtrlLoop.cpp,v 1.11 2005/09/17 00:11:43 raddog Exp $
 *
 */



#include <IOKit/IOLib.h>
#include <machine/machine_routines.h>

#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"

#include "PBG4_PlatformPlugin.h"
#include "PBG4_StepCtrlLoop.h"

#define super IOPlatformCtrlLoop
OSDefineMetaClassAndStructors( PBG4_StepCtrlLoop, IOPlatformCtrlLoop )

extern const OSSymbol		*gIOPluginEnvStepperDataLoadRequest,
							*gIOPluginEnvStepControlState;

#pragma mark -
#pragma mark ***    Stepper Kernel Support   ***
#pragma mark -

// These are globals so the C routines can see them
static bool		gUseStepper;
static UInt32	gStepControl, gDebugLevel;

// These functions are not documented well in pms.h.  Here are more descriptive definitions
//typedef void (*pmsSetFunc_t)(uint32_t, uint32_t, uint32_t);	/* Function used to set hardware power state */
//typedef void (*pmsSetFunc_t)(uint32_t pmsHWSelector, uint32_t, cpu, uint32_t, pmsPlatformData);	/* Function used to set hardware power state */
// uint32_t (*pmsQueryFunc_t)(uint32_t cpu, uint32_t pmsPlatformData);	/* Function used to query hardware power state */
// cpu is the cpuNumber for which the function applies
// pmsPlatformData is passed in by the plugin in pmsBuild and returned to us in the call.  Basically a refCon.

static UInt32 gPMSHWstate = 0xFFFFFFFF,
				gPMSHWSel,
				gStepperState;

static UInt32 gPMSHWFunction (UInt32 pmsHWSelector, UInt32 cpu, UInt32 pmsPlatformData);
static uint32_t gPMSQueryFunction (uint32_t cpu, uint32_t pmsPlatformData);

static				thread_call_t	    fSetAggressivenessGPUCallOut;

static void haltStepperProgram ( void )
{
	if (gStepperState != pmsPrepCng) {
		pmsRun (pmsPrepCng);					// Park the stepper
		gStepperState = pmsPrepCng;
	}
	
	return;
}

static void loadStepperProgram (pmsDef *data, UInt32 length, pmsSetFunc_t *pmsFuncTabP, UInt32 pmsPlatformData)
{
	kern_return_t kret;

	if (gUseStepper) {
		CTRLLOOP_DLOG ("loadStepperProgram - halting stepper\n");
		haltStepperProgram ();														// Park the stepper
		CTRLLOOP_DLOG ("loadStepperProgram - calling pmsBuild\n");
		kret = pmsBuild (data, length, pmsFuncTabP, pmsPlatformData, gPMSQueryFunction);	// Load new data
		CTRLLOOP_DLOG ("loadStepperProgram - calling pmsStart\n");
		pmsStart ();
		CTRLLOOP_DLOG ("loadStepperProgram - pmsStart () done\n");
		gStepperState = pmsNormHigh;
		CTRLLOOP_DLOG ("loadStepperProgram - done\n");
	}

	return;
}

#if STEPENGINE_DEBUG
extern unsigned int mfhid1(void);

extern __inline__ unsigned int mfhid1(void)
{
  unsigned int result;
  __asm__ volatile("mfspr %0, hid1" : "=r" (result));
  return result;
}
#endif

static UInt32 gPMSHWFunction (UInt32 pmsHWSelector, UInt32 cpu, UInt32 pmsPlatformData)
{
	//UInt32 newFreq;
	UInt32				newVoltage, newState, newClock;
	PBG4_StepCtrlLoop	*self;
#if STEPENGINE_DEBUG
	volatile UInt32		dfsVal;
	
	dfsVal = (mfhid1 () >> 21 ) & 6;
	
	STEPENGINE_DLOG ("HW/%d %x\n", dfsVal, pmsHWSelector);
#endif

	newState = pmsHWSelector & pmsPowerID;									/* Isolate the new state ID */
	
	if(newState != gPMSHWstate) {											/* Has the state changed? */
		//newFreq = ((pmsHWSelector & pmsCPU) >> 16);						/* Isolate the new frequency (we don't handle frequency) */
		
		// Set Delay_AACK if necessary
		if (pmsHWSelector & pmsCngXClk) {									/* Should we change the 'clock'? */
			newClock = ((pmsHWSelector & pmsXClk) >> 24);					/* Isolate the new 'external' clock */
			self = (PBG4_StepCtrlLoop *) pmsPlatformData;
			self->setDelayAACK (newClock & 3);
		}

		
		// Handle voltage changes
		if(pmsHWSelector & pmsCngVolt) {									/* Should we move the voltage? */
			newVoltage = ((pmsHWSelector & pmsVoltage) >> 8);				/* Isolate the new voltage */
			self = (PBG4_StepCtrlLoop *) pmsPlatformData;
			self->setProcessorVoltage (newVoltage);							/* Call the control loop to do it */
		}
			
		gPMSHWstate = newState;												/* Remember the state */
		gPMSHWSel = pmsHWSelector;
	}
	
	STEPENGINE_DLOG ("HW %x done\n", pmsHWSelector);
	
	return 1;
}

static uint32_t gPMSQueryFunction (uint32_t cpu, uint32_t pmsPlatformData)
{
	// This should return actual HW state but for now return internal soft state
	return (gPMSHWSel & (pmsXClk | pmsVoltage | pmsPowerID));
}

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
IOReturn PBG4_StepCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
	OSData			*stepperData, *vCoreProp;
	OSArray			*stepperDataArray;
	OSNumber		*watts;
	UInt32			vCoreGPIORange;
	int				pstep;		// xxx temp

	IOReturn		result;
	
	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::initPlatformCtrlLoop entered\n" );
		
	// lookup the cpu temp sensor by key kCPUTempSensorDesc
	if (!(fCpuTempSensor = lookupAndAddSensorByKey (kCPUTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_StepCtrlLoop::initPlatformCtrlLoop no CPU sensor!!\n");

	// lookup the battery temp sensor by key kBatteryTempSensorDesc
	if (!(fBattTempSensor = lookupAndAddSensorByKey (kBatteryTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_StepCtrlLoop::initPlatformCtrlLoop no battery sensor!!\n");

	// lookup the track pad temp sensor by key kTrackPadTempSensorDesc
	if (!(fTPadTempSensor = lookupAndAddSensorByKey (kTrackPadTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_StepCtrlLoop::initPlatformCtrlLoop no track pad sensor!!\n");

	// lookup the Uni-N temp sensor by key kUniNTempSensorDesc
	if (!(fUniNTempSensor = lookupAndAddSensorByKey (kUniNTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_StepCtrlLoop::initPlatformCtrlLoop no Uni-N temp sensor!!\n");

	// lookup the power supply temp sensor by key kPwrSupTempSensorDesc
	if (!(fPwrSupTempSensor = lookupAndAddSensorByKey (kPwrSupTempSensorDesc)))
		CTRLLOOP_DLOG("PBG4_StepCtrlLoop::initPlatformCtrlLoop no power supply sensor!!\n");

	// lookup the GPU temp sensor by key kGPUSensorDesc
	if (!(fGPUIdleSensor = lookupAndAddSensorByKey (kGPUSensorDesc)))
		CTRLLOOP_DLOG("PBG4_DPSCtrlLoop::initPlatformCtrlLoop no GPU sensor!!\n");

	watts = OSDynamicCast (OSNumber, dict->getObject (kCtrlLoopPowerAdapterBaseline));
	if (watts) {
		fBaselineWatts = watts->unsigned32BitValue();
		// CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::initPlatformCtrlLoop got baseline watts %d\n", fBaselineWatts );
	} else {
		fBaselineWatts = 0xFF;
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::initPlatformCtrlLoop no baseline watts - defaulting to worst case\n" );
	}

	result = super::initPlatformCtrlLoop (dict);

	fUsePowerPlay = gPlatformPlugin->fUsePowerPlay;
	
	fSleepState = OSNumber::withNumber( (unsigned long long) kStepSleep, 32);
	
	// pmRootDomain is the recipient of GPU controller messages
	fPMRootDomain = OSDynamicCast(IOPMrootDomain, 
		gPlatformPlugin->waitForService(gPlatformPlugin->serviceMatching("IOPMrootDomain")));
		
	gUseStepper = true;
	gStepControl = kStepNorm;
	gDebugLevel = 0;
	if (PE_parse_boot_arg("pstep", &pstep)) {
		gUseStepper = (pstep & kStepEnableMask) == kStepEnable;
		gStepControl = (pstep & kStepControlMask) >> kStepControlShift;
		gDebugLevel = (pstep & kStepDebugMask) >> kStepDebugShift;
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::initPlatformCtrlLoop - stepper %sabled, control %d, debug %d\n", gUseStepper ? "en" : "dis",
			gStepControl, gDebugLevel);
	}
	
	// Get the vcore voltage control property
	vCoreProp = OSDynamicCast (OSData, gPlatformPlugin->getProvider()->getProperty ("cpu-vcore-control"));
				
	if (!vCoreProp) {
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::initPlatformCtrlLoop no stepper voltage control data, exiting\n" );
		return false;
	}

	fVCoreControlLength = vCoreProp->getLength();
	fVCoreControl = (UInt32 *)IOMalloc (fVCoreControlLength);
	memcpy (fVCoreControl, vCoreProp->getBytesNoCopy(), fVCoreControlLength);

	switch (fVCoreControl[0]) {
		case kVCoreType0:
			vCoreGPIORange = 1;
			break;
		default:
			CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::initPlatformCtrlLoop - type %d voltage control not implemented for M23, exiting\n", fVCoreControl[0] );
			return false;
	}
	
	
	// Map the range starting at physical address specified by fVCoreControl[1]
	fGpioDesc = IOMemoryDescriptor::withPhysicalAddress((IOPhysicalAddress)fVCoreControl[1],
			vCoreGPIORange, kIODirectionIn);
	if (fGpioDesc) {
		fGpioMap = fGpioDesc->map();
		if (fGpioMap) {
			switch (fVCoreControl[0]) {
				case kVCoreType0:
					// Convert the physical address to a virtual address
					fVCoreControl[1] = (UInt32)fGpioMap->getVirtualAddress();
					
					// If fVCoreControl[2] is present then we need to handle Delay_AACK as well
					// fVCoreControl[2] is the physical address of the AACK control register in Intrepid
					if ((fVCoreControlLength / sizeof (UInt32)) > 2) {
						fAACKCntlDesc = IOMemoryDescriptor::withPhysicalAddress((IOPhysicalAddress)fVCoreControl[2],
							1, kIODirectionIn);
						if (fAACKCntlDesc) {
							fAACKCntlMap = fAACKCntlDesc->map();
							if (fAACKCntlMap) {
								// Convert the physical address to a virtual address
								fVCoreControl[2] = (UInt32)fAACKCntlMap->getVirtualAddress();
								fSetDelayAACK = true;
							}
						}
					}
					break;
			}
		}
	}


	fPmsFuncTab[0] = 0;						// zero is reserved by the kernel to mean it handles the function internally
	fPmsFuncTab[1] = (pmsSetFunc_t)gPMSHWFunction;
	
	// See if we have an array - found on systems with multiple configs
	stepperDataArray = OSDynamicCast (OSArray, dict->getObject (kCtrlLoopStepperDataArray));
	if (stepperDataArray) {
		// Get the data for current config
		stepperData = OSDynamicCast (OSData, stepperDataArray->getObject (gPlatformPlugin->getConfig()));
	} else
		// Only one config so look for stepper data in dictionary
		stepperData = OSDynamicCast (OSData, dict->getObject (kCtrlLoopStepperData));
		
	if (stepperData) {
		CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::initPlatformCtrlLoop - got stepper data\n");
		fStepperTableEntry.address = (pmsDef *)stepperData->getBytesNoCopy();
		fStepperTableEntry.length = stepperData->getLength();
		fStepperTableEntry.dataObj = stepperData;	// Keep a copy
		fStepperTableEntry.dataObj->retain();
	} else {
		CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::initPlatformCtrlLoop - no stepper table - stepping disabled\n");
		return kIOReturnError;
	}
			
	gPlatformPlugin->setProperty(kCtrlLoopStepperData, stepperData);
				
	if (result == kIOReturnSuccess) {
		(void) setInitialState ();
		fPMRootDomain->publishFeature("Stepper CPU");
	}
	
	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::initPlatformCtrlLoop done\n" );
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
 *	This is a public entry point for stepper processing within the control loop
 *  Stepper function requests come directly from the kernel and are NOT 
 *	governed by the platform plugin event dispatch sequencer nor can they block
 *	waiting for the control loop to reach a known state.  
 *
 *	As a result, *all* voltage change requests must come as a result of stepper 
 *	function request and the control loop should never call this function 
 *	directly.  For example, if an overcurrent event happens, the action take 
 *	cannot be to immediately lower the voltage.  Instead, the control loop puts
 *	the stepper into a low power run state which enforces the low power
 *	situation.
 *
 *	Set the processor voltage high or low for vStep
 *
 * stepValue == 1 => run fast (voltage high)
 * stepValue == 0 => run slow (voltage low)
 *
 *	NOTE - stepValue usage is opposite of how setProcessorVoltage is called in  
 *		the PBG4_DFSCtrlLoop.  This way works better because 1 means voltage high
 *		to both the kernel and the the code below:
 *
 * gpioData == 1 (true)  => run fast (voltage high)
 * gpioData == 0 (false) => run slow (voltage low)
 ******************************************************************************/
bool PBG4_StepCtrlLoop::setProcessorVoltage( UInt32 stepValue )
{	
	if (fVCoreControl[0] == kVCoreType0) {
#if STEPENGINE_DEBUG
		volatile UInt8 val;
		
		val = *(volatile UInt8 *)fVCoreControl[1];		// Snag current value
#endif
		*(volatile UInt8 *)fVCoreControl[1] = 0x4 + (stepValue & 1);
		// Log hi/lo, old value hi/lo, new value hi/lo
		STEPENGINE_DLOG ("%c(%d) %c %c\n", stepValue ? 'H' : 'L', stepValue, (val & 1) ? 'H' : 'L', 
			((*(volatile UInt8 *)fVCoreControl[1]) & 1) ? 'H' : 'L');
	}


	return true;
}

/*******************************************************************************
 * Method:
 *	setDelayAACK
 *
 * Purpose:
 *	Set the Uni-north delay AACK value
 ******************************************************************************/
void PBG4_StepCtrlLoop::setDelayAACK( UInt32 delayValue )
{
	if (fSetDelayAACK) {
		STEPENGINE_DLOG ("DA %x (%x)\n", delayValue, *(volatile UInt32 *)fVCoreControl[2]);
				
		*(volatile UInt32 *)fVCoreControl[2] = delayValue;
	}
#if CTRLLOOP_DEBUG
	else
		CTRLLOOP_DLOG ( "PBG4_StepCtrlLoop::setDelayAACK - unexpected setDelayAACK request - ignored\n");
#endif
	
	return;
}

/*******************************************************************************
 * Method:
 *	setGPUAgressivenessThread
 *
 * Purpose:
 *	Set the graphics processor speed needs to be in a seperate thread to avoid 
 *  deadlock situations.
 ******************************************************************************/
void PBG4_StepCtrlLoop::setGPUAgressivenessThread( void *self )
{
	PBG4_StepCtrlLoop* me = OSDynamicCast(PBG4_StepCtrlLoop, (OSMetaClassBase*)self);

	if(!me) return;
	
	// Change GPU accordingly
	me->fPMRootDomain->setAggressiveness (kIOFBLowPowerAggressiveness, me->fNewDesiredGPUState ? 3 : 0);
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
bool PBG4_StepCtrlLoop::setGraphicsProcessorStepValue( const OSNumber *newGPUMetaState )
{
	bool		newUserStateSlow, prevUserStateSlow;

	// Determine new and old conditions
	newUserStateSlow = (newGPUMetaState->isEqualTo(gIOPPluginOne) || newGPUMetaState->isEqualTo(gIOPPluginThree));
	prevUserStateSlow = (fPreviousGPUMetaState->isEqualTo(gIOPPluginOne) || fPreviousGPUMetaState->isEqualTo(gIOPPluginThree));

	if (prevUserStateSlow != newUserStateSlow) {
		//CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::setGraphicsProcessorStepValue - sending %d\n",
			//newUserStateSlow ? 3 : 0);
			
		fNewDesiredGPUState = newUserStateSlow;
		thread_call_enter(fSetAggressivenessGPUCallOut);	// Off to the thread
	
		fPreviousGPUMetaState = newGPUMetaState;			// Remember state
	}
	
	return true;
	
}

/*******************************************************************************
 * Method:
 *	batteryIsOvercurrent
 *
 ******************************************************************************/
bool PBG4_StepCtrlLoop::batteryIsOvercurrent( void )
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
			CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::batteryIsOvercurrent - deadline expired, setting overcurrent '%s'\n",
				fBatteryIsOvercurrent ? "true" : "false");
		}
	} else {
		// Not currently in overcurrent
		if (envBatOvercurrent) {
			// Setup a deadline (kOvercurrentInterval seconds) for the overcurrent to expire
			clock_interval_to_deadline( kOvercurrentInterval /* seconds */, NSEC_PER_SEC, 
				&deadline );
				
			fBatteryIsOvercurrent = true;
			CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::batteryIsOvercurrent - setting overcurrent 'true'\n");
		}
	}
	
	return fBatteryIsOvercurrent;	
}

/*******************************************************************************
 * Method:
 *	lowPowerForcedDPS
 *
 ******************************************************************************/
bool PBG4_StepCtrlLoop::lowPowerForcedDPS( void )
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
		
			CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::lowPowerForcedDPS - got powerStatus 0x%x, curWatts %d\n", powerVal, curWatts);
			if (((powerVal & (kIOPMACInstalled | kIOPMACnoChargeCapability)) == 
				(kIOPMACInstalled | kIOPMACnoChargeCapability)) ||
				((powerVal & (kIOPMRawLowBattery | kIOPMBatteryDepleted)) != 0) ||
				((powerVal & kIOPMBatteryInstalled) == 0)) {
					CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::lowPowerForcedDPS - forcing reduced speed\n");
					result = true;							// force low speed
			}
		}
	}
	
	return result;
}

void  PBG4_StepCtrlLoop::logStepTransition ( UInt32 oldState, UInt32 newState )
{
	static const char *stateNames[] = {
		"pmsIdle",
		"pmsNorm",
		"pmsNormHigh",
		"pmsBoost",
		"pmsLow",
		"pmsHigh",
		"pmsPrepCng",
		"pmsPrepSleep",
		"pmsOverTemp",
		"pmsEnterNorm"
	};
	
	// Log to system.log (turned on by kStepDebugLog flag: boot-args pstep=0x9)
	if ((gDebugLevel & kStepDebugLog) && (oldState <= pmsEnterNorm) && (newState <= pmsEnterNorm)) {
		IOLog ("Stepper state change from %s to %s\n", stateNames[oldState], stateNames[newState]);

		// Also log to kprintf
		kprintf ("Stepper state change from %s to %s\n", stateNames[oldState], stateNames[newState]);
	}
	
	return;
}

/*******************************************************************************
 * Method:
 *	setStepperProgram
 *
 * Purpose:
 *	Modulate the stepper program based on the meta state
 *******************************************************************************/
bool  PBG4_StepCtrlLoop::setStepperProgram ( const OSNumber *newMetaState )
{
	if (newMetaState->isEqualTo(fPreviousMetaState)) {
		//CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::setStepperProgram - no change in metaState - exiting\n");
		return true;
	}

	switch (newMetaState->unsigned32BitValue()) {
		case kStepNorm:
			if (gStepperState != pmsNormHigh) {
				logStepTransition (gStepperState, pmsNormHigh);
				// Enter normal operation
				pmsPark ();
				pmsRun(pmsEnterNorm);
				gStepperState = pmsNormHigh;
			}
			break;
		case kStepOverTemp:
			if (gStepperState != kStepOverTemp) {
				logStepTransition (gStepperState, pmsOverTemp);
				// Force speed to low for overtemp/overcurrent, etc.
				pmsPark ();
				pmsRun(pmsOverTemp);
				gStepperState = pmsOverTemp;
			}
			break;
		case kStepLow:
			if (gStepperState != pmsLow) {
				logStepTransition (gStepperState, pmsLow);
				// Force speed to low
				pmsPark ();
				pmsRun(pmsLow);
				gStepperState = pmsLow;
			}
			break;
		case kStepHigh:
			if (gStepperState != pmsHigh) {
				logStepTransition (gStepperState, pmsHigh);
				// Force speed to high
				pmsPark ();
				pmsRun(pmsHigh);
				gStepperState = pmsHigh;
			}
			break;
		case kStepSleep:
			if (gStepperState != pmsPrepSleep) {
				logStepTransition (gStepperState, pmsPrepSleep);
				// Signal stepper for sleep
				pmsRun(pmsPrepSleep);
				gStepperState = pmsPrepSleep;
			}
			break;
		default:
			panic ("PBG4_StepCtrlLoop::setStepperProgram - impossible stepper state!");
			
	}
	
	setMetaState (newMetaState);

	fPreviousMetaState = newMetaState;			// Remember state

	return true;
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
bool PBG4_StepCtrlLoop::setInitialState( void )
{
	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::setInitialState entered\n" );

	gStepperState = pmsParked;					// Initially we're parked
	 
	// In safe boot we behave like overtemp
	if (safeBoot)
		gStepControl = kStepOverTemp;
	
	if (!fSetAggressivenessGPUCallOut)
            fSetAggressivenessGPUCallOut = thread_call_allocate((thread_call_func_t)PBG4_StepCtrlLoop::setGPUAgressivenessThread,
                                (thread_call_param_t)this);
								
	// fStepControlState is a global state we can be in at the request of the user
	// fStepControlState states map the same as the meta states but meta states can change in response to conditions
	switch (gStepControl) {
		case kStepNorm:
			fStepControlState = gIOPPluginZero;
			fPreviousMetaState = gIOPPluginOne;			// Pretend initially in low state
			CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::setInitialState - setting norm\n");
			break;
		case kStepOverTemp:
			fStepControlState = gIOPPluginOne;
			fPreviousMetaState = gIOPPluginZero;		// Pretend initially in high state
			CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::setInitialState - setting OT\n");
			break;
		case kStepLow:
			fStepControlState = gIOPPluginTwo;
			fPreviousMetaState = gIOPPluginZero;		// Pretend initially in high state
			CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::setInitialState - setting low\n");
			break;
		case kStepHigh:
			fStepControlState = gIOPPluginThree;
			fPreviousMetaState = gIOPPluginZero;		// Pretend initially in high state
			break;
			CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::setInitialState - setting high\n");
		default:
			CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::setInitialState - invalid gStepControl state 0x%x, defaulting to normal operation\n", gStepControl );
			fStepControlState = gIOPPluginZero;
			fPreviousMetaState = gIOPPluginOne;			// Pretend initially in low state
			break;
	}
	
	setMetaState( fPreviousMetaState );				// Remember the initial state

	fPreviousGPUMetaState = gIOPPluginZero;			// Graphics starts out high

	CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::setInitialState - loading initial stepper program\n");
	// Load initial program and start normal operation
	loadStepperProgram (fStepperTableEntry.address, fStepperTableEntry.length, fPmsFuncTab, (UInt32) this);

	CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::setInitialState - starting initial stepper program %d\n", fStepControlState->unsigned32BitValue());
	// Set program as requested by the user
	setStepperProgram (fStepControlState);			// Per user request
		
	ctrlloopState = kIOPCtrlLoopFirstAdjustment;
	
	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::setInitialState returning true\n");

	return true;
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
void PBG4_StepCtrlLoop::adjustControls( void )
{
	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::adjustControls entered\n" );

	updateMetaState();		// updateMetaState does the work for us
		
	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::adjustControls done\n" );
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
bool PBG4_StepCtrlLoop::updateMetaState( void )
{
	OSNumber		*tmpNumber;
	const OSNumber	*newMetaState, *newGPUMetaState, *stepControlState;
	UInt32			dpsTarget;
	bool			isIdle, didAct;
	
	if (ctrlloopState == kIOPCtrlLoopNotReady) {
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState entered with control loop not ready\n");
		return false;
	}
	
	if (ctrlloopState == kIOPCtrlLoopWillSleep) {
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState entered with control loop in sleep state - exiting\n" );
		return false;
	}
	
	// In a safe boot we start out in reduced speed mode and stay there no matter what.  So there's nothing to do here.
	if (safeBoot) return true;
	
	//CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState entered\n" );
	
	if (gPlatformPlugin->getEnv(gIOPluginEnvStepperDataLoadRequest) == kOSBooleanTrue) {
		OSData				*stepperData;
		StepTableEntry		tmpStepperTableEntry;
		
		stepperData = OSDynamicCast (OSData, gPlatformPlugin->getProperty(gIOPluginEnvStepperDataLoadRequest));
		if (stepperData) {			
			CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::updateMetaState - got stepper table data\n");
			tmpStepperTableEntry.address = (pmsDef *)stepperData->getBytesNoCopy();
			tmpStepperTableEntry.length = stepperData->getLength();
			tmpStepperTableEntry.dataObj = stepperData;	// Keep a copy
			tmpStepperTableEntry.dataObj->retain();
		}
		
		// OK we now have a new step table entry - let the kernel know about it
		haltStepperProgram ();						// Park the stepper before going further
		fStepperTableEntry.dataObj->release();		// release old data
		bcopy (&tmpStepperTableEntry, &fStepperTableEntry, sizeof (StepTableEntry));
		// Load new program
		loadStepperProgram (fStepperTableEntry.address, fStepperTableEntry.length, fPmsFuncTab, (UInt32) this);
				
		CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::updateMetaState - starting new stepper program %d\n", fStepControlState->unsigned32BitValue());
		// Start program
		setStepperProgram (fStepControlState);			// start it at requested user level
				
		// Clear the stepper load request.  This triggers another environment change which will
		// get us called again where we can re-evaluate the situation.  In the meantime just return
		gPlatformPlugin->setEnv(gIOPluginEnvStepperDataLoadRequest, kOSBooleanFalse);
		
		return true;
	}
	
	if (stepControlState = OSDynamicCast (OSNumber, gPlatformPlugin->getEnv(gIOPluginEnvStepControlState))) {
		// User requests control
		bool newStepLoad = true;
		
		switch (stepControlState->unsigned32BitValue()) {
			case kStepNorm:
				fStepControlState = gIOPPluginZero;
				CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::updateMetaState - setting new state normal\n");
				break;
			case kStepOverTemp:
				fStepControlState = gIOPPluginOne;
				CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::updateMetaState - setting new state OT\n");
				break;
			case kStepLow:
				fStepControlState = gIOPPluginTwo;
				CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::updateMetaState - setting new state low\n");
				break;
			case kStepHigh:
				fStepControlState = gIOPPluginThree;
				break;
				CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::updateMetaState - setting new state high\n");
			default:
				CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState - invalid step state 0x%x ignored\n", stepControlState->unsigned32BitValue() );
				newStepLoad = false;
				break;
		}
		if (newStepLoad) {
			CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::setInitialState - starting new stepper program %d\n", fStepControlState->unsigned32BitValue());
			// Start program
			setStepperProgram (fStepControlState);			// Per user request
			CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::setInitialState - done starting new stepper program %d\n", fStepControlState->unsigned32BitValue());
		}
		
		gPlatformPlugin->delEnv (gIOPluginEnvStepControlState);
		return newStepLoad;
	}
	
	if (!fStepControlState->isEqualTo(gIOPPluginZero)) {
		CTRLLOOP_DLOG ("PBG4_StepCtrlLoop::updateMetaState - staying in stepper state %d\n", fStepControlState->unsigned32BitValue());
		// We're in a forced control situation, do nothing
		return true;
	}
	
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
	
	// Start out in 0 - the highest metaState or 1 if DFS requests it
	newMetaState = dpsTarget ? gIOPPluginOne : gIOPPluginZero;
	
	if (!fIdleTimerActive && batteryIsOvercurrent()) {
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState - battery overcurrent forced metastate 1\n");
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
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState CPU0 non-zero temp state 0x%lx\n", safeGetState(fCpuTempSensor));
	}

	// fetch the current batt temperature state
	if (safeGetState(fBattTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState battery non-zero temp state 0x%lx\n", safeGetState(fBattTempSensor));
	}

	// fetch the current trackpad temperature state
	if (safeGetState(fTPadTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState trackpad non-zero temp state 0x%lx\n", safeGetState(fTPadTempSensor));
	}
	
	// fetch the current Uni-N temperature state
	if (safeGetState(fUniNTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState Uni-N non-zero temp state 0x%lx\n", safeGetState(fUniNTempSensor));
	}
	
	// fetch the current power supply temperature state
	if (safeGetState(fPwrSupTempSensor) >= 1) {
		newMetaState = gIOPPluginOne;
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState power supply non-zero temp state 0x%lx\n", safeGetState(fPwrSupTempSensor));
	}

	// Check GPU state 
	// where state 0 -> 0 - 69 % idle - Set GPU to highest power level
	// where state 1 -> 70 - 100 % idle - Set GPU to lowest power level
	
	// fetch the current GPU state (assume highest)
	newGPUMetaState = gIOPPluginZero;
	if (fUsePowerPlay) {
		if (safeGetState(fGPUIdleSensor) == 0) {
			newGPUMetaState = gIOPPluginZero;
			fGPUState1Count = 0;
			CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState check GPU state 0x%lx\n", safeGetState(fGPUIdleSensor));
		}
		else
		if (safeGetState(fGPUIdleSensor) == 1) {
			if (fGPUState1Count < fGPUStateCount1Max)
			{
				UInt32 tryState;
				const OSNumber *tmpNumber;
				
				CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::DONT'T CHANGE STATE fGPUState1Count %d check GPU state 0x%lx\n", fGPUState1Count, safeGetState(fGPUIdleSensor));
				fGPUState1Count++;
				
				// Set the sensor state back to 0 for now
				tryState = 0;
				tmpNumber = OSNumber::withNumber( tryState, 32 );
				safeSetState(fGPUIdleSensor,tmpNumber);
			}
			else
			{
				CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState fGPUState1Count %d check GPU state 0x%lx\n", fGPUState1Count, safeGetState(fGPUIdleSensor));
				newGPUMetaState = gIOPPluginOne;
			}
		}
	}
	
	// Check for low power conditions
	if (lowPowerForcedDPS ()) {
		newMetaState = gIOPPluginOne;
	}

	//CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState - final metaState %d\n", newMetaState->unsigned32BitValue() );
	didAct = false;
	
	// Signal stepper, if necessary
	if (!newMetaState->isEqualTo(getMetaState())) {
		setMetaState( newMetaState );	// Set the new state
		
		setStepperProgram (newMetaState);
		
		// If we're not in an overcurrent situation, handle idle timers
		if (!fBatteryIsOvercurrent) {
			// If the new state is high, clear any idle timer
			if (newMetaState->isEqualTo(gIOPPluginZero)) {
				// Clear out deadline so we don't get called because of the idle timer
				super::deadlinePassed();		// Use the IOPlatformCtrlLoop implementation so we don't get re-entered
				// CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState - state high - clearing idle timer\n");
				fIdleTimerActive = false;
			} else {
				// The new state is low so, start the idle timer
				// Setup a deadline (kIdleInterval seconds) for the activity monitor to expire
				// CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState - state low - setting idle timer\n");
				clock_interval_to_deadline( kIdleInterval /* seconds */, NSEC_PER_SEC, 
					&deadline );
				fIdleTimerActive = true;
			}
		}
		didAct = true;
	}
	
	// Step graphics processor, if necessary
	if (!newGPUMetaState->isEqualTo(fPreviousGPUMetaState)) {
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::updateMetaState calling setGraphicsProcessorStepValue(0x%lx)\n", 
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
void PBG4_StepCtrlLoop::willSleep( void )
{
	if (ctrlloopState == kIOPCtrlLoopNotReady) {
		CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::willSleep entered with control loop not ready\n");
		return;
	}

 	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::willSleep entered\n");

	super::willSleep();
	
	fSavedMetaState = getMetaState();		// Save state
	
	setStepperProgram( fSleepState );		// Set sleep state
	
 	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::willSleep done\n");
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
void PBG4_StepCtrlLoop::didWake( void )
{
	if (ctrlloopState == kIOPCtrlLoopNotReady) {
		//CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::didWake entered with control loop not ready\n");
		return;
	}
	
	// Ignore didWake() if willSleep() was not previously called
	if (!fSavedMetaState) {
		//CTRLLOOP_DLOG( "PBG4_DPSCtrlLoop::didWake entered with no savedMetaState - exiting\n");
		return;
	}

 	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::didWake restoring previous state\n");
	
	// Return program to previous state
	setStepperProgram (fSavedMetaState);

    super::didWake();
	
	updateMetaState();						// Set new state based on current conditions

 	CTRLLOOP_DLOG( "PBG4_StepCtrlLoop::didWake done\n");
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
void PBG4_StepCtrlLoop::deadlinePassed( void )
{
	super::deadlinePassed();		// clear the deadline
	updateMetaState();				// Re-evaluate the situation
	
	return;
}

