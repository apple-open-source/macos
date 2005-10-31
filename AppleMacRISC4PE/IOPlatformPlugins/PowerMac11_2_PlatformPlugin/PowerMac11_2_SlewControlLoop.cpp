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


#include <IOKit/IOLib.h>
#include <machine/machine_routines.h>

#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"

#include "SMU_Neo2_PlatformPlugin.h"
#include "PowerMac11_2_SlewControlLoop.h"

//	temporary debugging - comment in the following line to enable debugging output
// #undef CTRLLOOP_DLOG
#ifndef CTRLLOOP_DLOG
#ifndef CTRLLOOP_IOLOG_OUTPUT
//	change the following 1 -> 0 if you want debug output to be kprintf() instead of IOLog()
#define CTRLLOOP_IOLOG_OUTPUT 1
#endif
#if CTRLLOOP_IOLOG_OUTPUT
#define CTRLLOOP_DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define CTRLLOOP_DLOG(fmt, args...)  kprintf(fmt, ## args)
#endif	// CTRLLOOP_IOLOG_OUTPUT
#endif	//CTRLLOOP_DLOG


#define kINITIAL_MILLISECONDS_PER_LOOP_TO_WAIT		8	// initially wait 8ms
#define kSUBSEQUENT_MILLISECONDS_PER_LOOP_TO_WAIT	1	// then wait 1ms on subsequent passes
#define kWAIT_LOOP_ITERATIONS						42
#define kSLEW_COMPLETE_TIME_TO_WAIT		( kINITIAL_MILLISECONDS_PER_LOOP_TO_WAIT + (kSUBSEQUENT_MILLISECONDS_PER_LOOP_TO_WAIT * kWAIT_LOOP_ITERATIONS) )


#define super IOPlatformCtrlLoop
OSDefineMetaClassAndStructors( PowerMac11_2_SlewCtrlLoop, IOPlatformCtrlLoop )


#pragma mark -
#pragma mark ***    Initialization   ***
#pragma mark -

/*******************************************************************************
 * Method:
 *	init
 *
 * Purpose:
 *	Set initial state of instance variables
 ******************************************************************************/
// virtual
bool PowerMac11_2_SlewCtrlLoop::init( void )
{
	if ( ! super::init() )
	{
		return( false );
	}

	ctrlloopState                = kIOPCtrlLoopNotReady;

	_canStepProcessors           = false;
	_canSlewProcessors           = false;

	_currentProcessorStepSetting = kPROCESSOR_FULL_SPEED;
	_currentVoltageSlewSetting   = kVOLTAGE_NOT_SLEWING;

	return( true );
}




/*******************************************************************************
 * Method:
 *	free
 *
 * Purpose:
 *	Free resources associated with instance variables
 ******************************************************************************/
// virtual
void PowerMac11_2_SlewCtrlLoop::free( void )
{

	// if the interrupt for slewing is enabled and we're registered for it,
	// we should disable it and remove our registration for it -here-



	// release symbols, if allocated
	if ( _CpuVoltageSlewControlSym )
	{
		_CpuVoltageSlewControlSym->release();
		_CpuVoltageSlewControlSym = NULL;
	}
	if ( _CpuVoltageSlewStateSym )
	{
		_CpuVoltageSlewStateSym->release();
		_CpuVoltageSlewStateSym = NULL;
	}
	if ( _CpuVoltageSlewingCompleteSym )
	{
		_CpuVoltageSlewingCompleteSym->release();
		_CpuVoltageSlewingCompleteSym = NULL;
	}
}




/*******************************************************************************
 * Method:
 *	initPlatformCtrlLoop
 *
 * Purpose:
 *	Fetch control loop parameters (from thermal profile, SDB, etc.) and setup
 *	control loop initial state.
 ******************************************************************************/
// virtual
IOReturn PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
IOReturn		status;
OSData			* pHandle;
UInt32			pHandleValue;
IOService		* resources;
char			functionName[64];
const OSSymbol	* registerForIntNotificationsSym;
bool			SATsVersionOKForSlewing;


	ctrlloopState = kIOPCtrlLoopNotReady;		// we're not ready till we make it through the initialization



	/*
	 * Invoke the base class implementation
	 */

	status = super::initPlatformCtrlLoop( dict );


	/*
	 *	Check PowerTune-related info:
	 *      Check conditions that processor stepping depends on. We need the 'power-mode-data'
	 *      property present in /cpus/@0 (this is true regardless of how many processors we have).
	 *
	 *      FYI - we can't JUST check for the existence of the 'power-mode-data' property to
	 *      determine whether or not we can do processor stepping.  There must be more than
	 *      one 32-bit value in the property - otherwise if there are zero or one entries,
	 *      we cannot PowerTune.
	 */

	IORegistryEntry         * cpusRegEntry;

	cpusRegEntry = gPlatformPlugin->fromPath("/cpus/@0", gIODTPlane);

	if ( cpusRegEntry != NULL )
	{
	OSData  * powerModeData;

		// do we have a 'power-mode-data' property?
		// It must exist, as OS SW elsewhere may be atttempting to tell us not to do processor stepping.
		if ( cpusRegEntry->getProperty( "power-mode-data" ) != NULL )
		{
			// not only must the property exist, but it must contain more than 1 32-bit entry.
			// if it doesn't, then the machine can only run at whatever speed it's currently running at,
			// as we do not have enough (valid) information to be able for other "steps".
			if ( (powerModeData = OSDynamicCast( OSData, cpusRegEntry->getProperty( "power-mode-data" ))) != NULL )
			{
			unsigned int	pmd_len;

				// you must have at least 2 entries to believe you can actually attempt to do processor stepping
				pmd_len = powerModeData->getLength();
				if ( pmd_len >= (2 * sizeof( uint32_t )) )
				{
					//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - sizeof('power-mode-data') == %d. Stepping ALLOWED\n", pmd_len );
					_canStepProcessors = true;	// yes, we think we can do processor stepping
				}
			}
		}

		cpusRegEntry->release();
	}


	if ( ! _canStepProcessors )	// if we can't do processor stepping, we can't do voltage slewing either - bail
	{
		IOLog( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - CPU stepping disabled.  Exiting.\n" );
		return( kIOReturnError );
	}


	/*
	 * Voltage Slewing Initialization:
	 *
	 *	First we need to be able to find out who we need to talk to be able to do
	 *	the actual voltage slewing change.  This is a GPIO, so we need to find the
	 *	service object that can provide this service.
	 */

	SATsVersionOKForSlewing = true;		// pre-set to we think it's okay


	if ( SATsVersionOKForSlewing )
	{
		// Find pHandle for the Device Tree root provided by platform expert.
		// This is where the BootROM is going to store the "platform-*" commands.
		pHandle      = OSDynamicCast( OSData, gPlatformPlugin->getProvider()->getProperty( kAAPLPHandle ) );
		pHandleValue = ( pHandle ) ?  *(UInt32 *)pHandle->getBytesNoCopy()  :  0;

		// Find service that handles getting and setting cpu voltage slewing
		if ( gPlatformPlugin->getProvider()->getProperty( kVoltageSlewControl ) )
		{
		mach_timespec_t	waitTimeout;

			sprintf( functionName, "%s-%08lx", kVoltageSlewControl, pHandleValue);
			_CpuVoltageSlewControlSym = OSSymbol::withCString( functionName );
			sprintf( functionName, "%s-%08lx", kVoltageSlewState, pHandleValue);
			_CpuVoltageSlewStateSym   = OSSymbol::withCString( functionName );
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - waiting for function '%s'\n", functionName );

			waitTimeout.tv_sec  = 5;
			waitTimeout.tv_nsec = 0;

			resources = gPlatformPlugin->waitForService( gPlatformPlugin->resourceMatching( _CpuVoltageSlewStateSym   ), &waitTimeout );
			if ( resources )
			{
				resources = gPlatformPlugin->waitForService( gPlatformPlugin->resourceMatching( _CpuVoltageSlewControlSym ), &waitTimeout );
				
				// it is assumed here that since the 'kVoltageSlewControl' and 'kVoltageSlewState' look at the same GPIO
				// and hence the same object, so the same object can be used to used to get and set the current VDNAP0
				// (voltage slewing) state value.
				if ( resources )
				{
					_CpuVoltageSlewControl = OSDynamicCast( IOService, resources->getProperty( _CpuVoltageSlewControlSym ) );

					if ( _CpuVoltageSlewControl )
					{
						// now that we have the reference to the service that will allow us to do the toggle the GPIO,
						// we need to find out if we can register with the service that will provide the interrupt that
						// tells us that we're done with the voltage slewing request(s) we make.

						sprintf( functionName, "%s-%08lx", kVoltageSlewComplete, pHandleValue );
						_CpuVoltageSlewingCompleteSym  = OSSymbol::withCString( functionName );
						resources = gPlatformPlugin->waitForService( gPlatformPlugin->resourceMatching( _CpuVoltageSlewingCompleteSym ), &waitTimeout );
						if ( resources )	// we were able to find the 'platform-slewing-done' resource
						{
							registerForIntNotificationsSym = OSSymbol::withCString( kIOPFInterruptRegister );
							status = gPlatformPlugin->callPlatformFunction(_CpuVoltageSlewingCompleteSym,	// symbol of the function you want to register for
										true,																// waitForService
										(void *) slewIsCompleteCallback,									// param1
										(void *) this,														// param2
										(void *) 0,															// param3
										(void *) registerForIntNotificationsSym );							// param4

							if ( status == kIOReturnSuccess )
								_canSlewProcessors = true;
							else
							{
								IOLog( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - unable to register for '%s' interrupt - voltage slewing disabled\n",  kVoltageSlewComplete );
								//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - unable to register for '%s' interrupt\n",  kVoltageSlewComplete );
								// this is initialized in ::init
								//_canSlewProcessors = false;	// set this so that we don't attempt any further slewing setup actions
							}
						}
						else
							CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - no %s resource available - voltage slewing disabled\n", kVoltageSlewComplete );
					}
					else
						CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - no %s control available - voltage slewing disabled\n", kVoltageSlewComplete );
				}
				else
					IOLog ("PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - missing '%s' resource - voltage slewing disabled\n",  kVoltageSlewControl );
			}
			else		// no GPIO state resource - dont allow voltage slewing if we cant detect current state
				IOLog ("PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - missing '%s' resource - voltage slewing disabled\n",  kVoltageSlewState );
		}
		else
		{
			IOLog ("PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - missing '%s' property - voltage slewing disabled\n",  kVoltageSlewControl );
			//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - missing '%s' property\n",  kVoltageSlewControl );
			// even though we won't be able to voltage slew, we might be able to PowerTune.  Proceed on.
			// return( kIOReturnError );
		}
	}




	// if you got here, we -can- do stepping and you -may- be able to do slewing.

	if ( ! _canSlewProcessors )
	{
		IOLog( "PowerMac11_2_SlewCtrlLoop::initPlatformCtrlLoop - CPU voltage slewing disabled\n" );
	}

	// "slewing" in the general sense - put the processor in the state expected by the system.
	// that consists of cranking everything up to full, then looking at the Energy Saver Pref
	// pane setting and doing as much of that as we can (steppings and, if available, voltage slewing).

	setupSlewing();

	// stick around
	return( kIOReturnSuccess );
}




/*******************************************************************************
 * Method:
 *	setupSlewing
 *
 * Purpose:
 *	Since this SlewCtrlLoop does not actively monitor the temperatures of the
 *	CPU cores (that is done in CPUsCtrlLoop), we are only waiting to be told
 *	whether we should perform any action.  We kick off an initial setting of
 *	the slew "control" if necessary.
 ******************************************************************************/
void PowerMac11_2_SlewCtrlLoop::setupSlewing( void )
{
	CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setupSlewing - entered\n" );

	// if there were any other post-initialization processing or
	// pre-flighting that needed to be done, they would get done here.

	setInitialState();

	ctrlloopState = kIOPCtrlLoopFirstAdjustment;

	adjustControls();

	ctrlloopState = kIOPCtrlLoopAllRegistered;
}

#pragma mark -
#pragma mark ***     Slew/Step/Voltage Primitives     ***
#pragma mark -


/*******************************************************************************
 * Method:
 *	setProcessorStepIndex
 *
 * Purpose:
 *	Set the processor stepping to the desired stepping index.
 *	The instance variable _currentProcessorStepSetting will be either
 *
 *		0		full speed
 *		1		full speed / 2		PowerTune
 *
 ******************************************************************************/
bool PowerMac11_2_SlewCtrlLoop::setProcessorStepIndex( unsigned int stepIndex )
{
//	if ( stepIndex != _currentProcessorStepSetting )
	{
#if defined( __ppc__ )
		ml_set_processor_speed( stepIndex );
#endif
		_currentProcessorStepSetting = stepIndex;
	}

	return( true );
}




/*******************************************************************************
 * Method:
 *	getCurrentVoltageSlewIndex
 *
 * Purpose:
 *	Query the voltage slewing state GPIO to determine the current voltage
 *	slewing state as maintained by the voltage slewing controller (SAT, SMU).
 *
 * Arguments:
 *	param1				a pointer to this object
 *	param2, param3		not used - should be zero
 *	param4				the data passed back by the GPIO
 *	
 ******************************************************************************/
bool
PowerMac11_2_SlewCtrlLoop::getCurrentVoltageSlewIndex( unsigned int * returnValue )
{
IOReturn		result;
unsigned int	value;

	result        = kIOReturnError;
	* returnValue = kVOLTAGE_NOT_SLEWING;	// default if unable to read the GPIO

	if ( _CpuVoltageSlewControl )
	{
		result = _CpuVoltageSlewControl->callPlatformFunction( _CpuVoltageSlewStateSym,		// function to call
																false,					// waitForService, which shouldn't matter
																&value,					// initialize our current state
																0, 0, 0 );				// param2, param3, param4
		if ( result == kIOReturnSuccess )
		{
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::getCurrentVoltageSlewIndex - raw value = %d\n", value );
			* returnValue = ( (value & 0x01) == kGPIO_VOLTAGE_SLEW_ON ) ?  kVOLTAGE_SLEWING_ON  :  kVOLTAGE_NOT_SLEWING;
		}
		else
		{
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::getCurrentVoltageSlewIndex - unable to retrieve current value (status = %x)\n", result );
		}
	}

	return( result == kIOReturnSuccess );
}




/*******************************************************************************
 * Method:
 *	slewIsCompleteCallback
 *
 * Purpose:
 *	Provide a routine to the GPIO driver to call us back and let us know when
 *	the slew-change-in-progress is complete.  Technically, the fact that this
 *	routine gets called at all should be enough to tell us that the slew
 *	operation is complete.  But for now we'll be anal about checking the value
 *	of the GPIO and whether we thought we were supposed to be called to be sure.
 *  The extra code can be removed when I'm convinced.
 *
 * Arguments:
 *	param1				a pointer to this object
 *	param2, param3		not used - should be zero
 *	param4				the data passed back by the GPIO
 *	
 ******************************************************************************/
void PowerMac11_2_SlewCtrlLoop::slewIsCompleteCallback( void * param1, void * param2, void * param3, void * param4 )
{
long						GPIOvalue;
PowerMac11_2_SlewCtrlLoop	* me;

	CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::slewIsCompleteCallback - entered\n" );

	me = OSDynamicCast( PowerMac11_2_SlewCtrlLoop, (OSMetaClassBase *) param1 );
	if ( me )	// if callback is really calling ME
	{
		GPIOvalue = (long) param4;	// value of the returned GPIO
		CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::slewIsCompleteCallback - GPIO data returned = %ld\n", GPIOvalue );
		if ( me->_waitingForSlewCompletion )
		{
			me->_waitingForSlewCompletion = false;
			me->_slewOperationComplete    = true;
		}
		else
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::slewIsCompleteCallback - unexpected callback!\n" );
	}
}





/*******************************************************************************
 * Method:
 *	setVoltageSlewIndex
 *
 * Purpose:
 *	Set the processor voltage to the desired slew index.
 ******************************************************************************/
bool PowerMac11_2_SlewCtrlLoop::setVoltageSlewIndex( unsigned int slewTarget )
{
IOReturn		result;
unsigned int	newSlewValue;
int				millisecondsWaited, millisecondsToWait;

	if ( _waitingForSlewCompletion )	// this SHOULD NOT HAPPEN!
	{
		IOLog( "PowerMac11_2_SlewCtrlLoop::setVoltageSlewIndex - entered.  _waitingForSlewCompletion already set!\n" );
		return( false );
	}

	CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setVoltageSlewIndex - setting slew state to %s\n",
									( slewTarget == kVOLTAGE_SLEWING_ON )? "SLEW" : "FULL" );

	newSlewValue =  (slewTarget == kVOLTAGE_SLEWING_ON )? kGPIO_VOLTAGE_SLEW_ON : kGPIO_VOLTAGE_SLEW_OFF;

	_slewOperationComplete    = false;
	_waitingForSlewCompletion = true;

	result = _CpuVoltageSlewControl->callPlatformFunction( _CpuVoltageSlewControlSym, false, (void *)newSlewValue, 0, 0, 0 );

	if ( result == kIOReturnSuccess )
	{
		for ( millisecondsWaited = 0, millisecondsToWait =  kINITIAL_MILLISECONDS_PER_LOOP_TO_WAIT;
				! _slewOperationComplete && ( millisecondsWaited < kSLEW_COMPLETE_TIME_TO_WAIT );  )
		{
			IOSleep( millisecondsToWait );						// sleep this thread for kMILLISECONDS_PER_LOOP_TO_WAIT milliseconds
			millisecondsWaited += millisecondsToWait;			// book-keeping
			millisecondsToWait  = kSUBSEQUENT_MILLISECONDS_PER_LOOP_TO_WAIT;	// after initial pass, we wait a smaller amount of time
			// we'll either exit the loop when the callback routine sets _slewOperationComplete or when our cumulative timeout is reached
		}

		if ( _slewOperationComplete )
		{
			_currentVoltageSlewSetting = slewTarget;
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setVoltageSlewIndex - slew request successful\n" );
		}
		else	//else	the setting remains what it was
			IOLog( "PowerMac11_2_SlewCtrlLoop::setVoltageSlewIndex - slew request timed out\n" );
	}

	_waitingForSlewCompletion = false;	// one way or the other, we are finished attempting this particular slew operation

	return( true );
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
 *	whether or not we can step and switch voltage.
 *
 *	The assumed initial state of Processor Stepping is that we are NOT stepping.
 *	We don't assume what initial voltage slewing state we're in.  We can read
 *	that directly since we've already established the connection to the
 *	_CpuVoltageSlewControl (/State) GPIO in ::initPlatformControlLoop, so we
 *	can find out directly.
 *
 ******************************************************************************/
bool PowerMac11_2_SlewCtrlLoop::setInitialState( void )
{
bool			success = false;
unsigned int	value;

	do
	{	// use a do { ... } while (false); to facilitate bailing out on errors

		if ( _canSlewProcessors )			// if we can't slew processors, then it's going to stay in whatever state it's in
		{
			success = getCurrentVoltageSlewIndex( &value );
			if ( success )
			{
				CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setInitialState - initial voltage slewing state = %d\n", value );
				_currentVoltageSlewSetting = value;
			}

			// initial conditions assume that processor stepping is FULL_SPEED, so we shouldn't be in this condition
			// if we're not, then since we WANT to be in that condition, we must bring up the voltage to full before
			// we can crank the processor up to full speed.
			if ( _currentVoltageSlewSetting != kVOLTAGE_NOT_SLEWING )
			{
				success = setVoltageSlewIndex( kVOLTAGE_NOT_SLEWING );
				if ( ! success )
					CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setInitialState - unable to establish initial slewing condition\n" );
			}

		}
		else
		{
			_currentVoltageSlewSetting = kVOLTAGE_NOT_SLEWING;	// because we either believe it can't or we don't have the facility to do it
		}

		// I know I said we're assumimg Processor Stepping is OFF.  But just to be sure,
		// since we've now made sure the voltage slewing is off, we can tell the kernel
		// to force it to full speed.
		//
		// The -if- is somewhat superfluous, since if the machine can't Step, then you
		// shouldn't have gotten this far into the loop -- you should have exited.
		if ( _canStepProcessors )
		{
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setInitialState Set Stepping OFF\n" );
			success = setProcessorStepIndex( kPROCESSOR_FULL_SPEED );
			if ( ! success )
				CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setInitialState - unable to set processor stepping to initial state\n" );
		}


		// NOW that we're in a known initial state --
		//
		// fetch the current Dynamic Power Step setting (this comes in from Energy Saver Preference Pane)
		// and see if the user wants us to run at Automatic, Highest or Reduced.
		// the value retrieved from this object is either 0 (full-speed) or 1 (NOT full speed (implies stepping AND slewing))
		//
		const OSNumber * curStepNum = OSDynamicCast( OSNumber, gPlatformPlugin->getEnv( gIOPPluginEnvDynamicPowerStep ) );
		unsigned int curStep = curStepNum ? curStepNum->unsigned32BitValue() : kENV_HIGH_SPEED_REQUESTED;

		// now we're running fast. if DPS says slow, verify slow is supported and then slow down.
		// one would guess that you shouldn't be able to get here if you're not allowed to slew or step
		if ( ( curStep != kENV_HIGH_SPEED_REQUESTED ) && ( _canStepProcessors || _canSlewProcessors ) )
		{
			// to go to a low-power state, you must PowerTune first, then initiate slewing
			// again, the -if- is probably superluous since you HAVE to have Stepping or else this loop
			// should have exited during the initialization process.
			if ( _canStepProcessors )
			{
				CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setInitialState - Set Stepping ON\n" );
				setProcessorStepIndex( kPROCESSOR_STEPPING );
			}
			if ( _canSlewProcessors )
			{
				CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::setInitialState - Set Voltage Slewing ON\n" );
				setVoltageSlewIndex( kVOLTAGE_SLEWING_ON );
			}
		}

		// if we get here, the method succeeded
		success = true;

	} while ( false );

	return ( success );
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
void PowerMac11_2_SlewCtrlLoop::adjustControls( void )
{
bool				success = false;
const OSNumber*		tmpNumber;
unsigned int		stepTarget;		// PowerTune's requested speed:
									//		0 = fast, non-zero = slow
unsigned int		slewActionToTake = kVOLTAGE_NOT_SLEWING,
					stepActionToTake = kPROCESSOR_FULL_SPEED;


	// if we've already prepared for sleep, do nothing
	if ( ctrlloopState == kIOPCtrlLoopWillSleep )
	{
		CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustControls - going to sleep, leave settings alone\n" );
		return;
	}
	else if ( 	ctrlloopState == kIOPCtrlLoopNotReady )
	{
		CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustControls - control loop not initialed!  Exiting.\n" );
		return;
	}

	// fetch the current PowerTune/Slew expectation from the system Energy Saver Preference
	// 0 == full speed, 1 = step+slew (or just step if slewing isn't possible)
	tmpNumber = OSDynamicCast( OSNumber, gPlatformPlugin->getEnv( gIOPPluginEnvDynamicPowerStep ) );
	stepTarget = tmpNumber ? tmpNumber->unsigned32BitValue() : kENV_HIGH_SPEED_REQUESTED;	// default to full speed/voltage if not present or unable to read it

	if ( _canStepProcessors )
		stepActionToTake = ( stepTarget == kENV_LOW_SPEED_REQUESTED )?  kPROCESSOR_STEPPING  :  kPROCESSOR_FULL_SPEED;
	if ( _canSlewProcessors )
		slewActionToTake = ( stepTarget == kENV_LOW_SPEED_REQUESTED )?  kVOLTAGE_SLEWING_ON  :  kVOLTAGE_NOT_SLEWING;


	// Call the appropriate helper method to perform necessary state changes
	if ( _canSlewProcessors && _canStepProcessors )
	{
			success = adjustSlewAndStepControls( slewActionToTake, stepActionToTake );
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustControls - adjustSlewAndStepControls(slew=%s,step=%s) returned %s\n",
				(slewActionToTake == kVOLTAGE_SLEWING_ON)? "SLEW" : "FULL", (stepActionToTake == kPROCESSOR_STEPPING)? "STEP" : "FULL", success? "SUCCESS" : "FAILED" );
	}
	else if ( _canStepProcessors )
	{
		success = adjustStepControls( stepActionToTake );
		CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustControls - adjustStepControls(%s) returned %s\n",
												(stepActionToTake == kPROCESSOR_STEPPING)? "STEP" : "FULL" , success? "SUCCESS" : "FAILED"  );
	}
	else if ( _canSlewProcessors )
	{
		// you are not supposed to slew without Stepping (down) first.  if you didn't go through
		// ( _canSlewProcessors && _canStepProcessors ) or _canStepProcessors, then we don't allow
		// voltage slewing all by itself.  report we were unable to comply to the request.
		success = false;
		CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustControls - attempted to adjust slewing without being able to Step.\n" );
	}
	else
	{
		success = true;
	}

	if ( ! success )
	{
		//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustControls failed to change operating point\n" );
		IOLog( "PowerMac11_2_SlewCtrlLoop::adjustControls failed to change operating point\n" );
	}
}




/*******************************************************************************
 * Method:
 *	adjustStepControls
 *
 * Purpose:
 *	Adjust controls for case when we can only do stepping.
 *
 * Arguments:
 *		stepTarget			0	kPROCESSOR_FULL_SPEED or
 *							1	kPROCESSOR_STEPPING
 *
 ******************************************************************************/
bool PowerMac11_2_SlewCtrlLoop::adjustStepControls( unsigned int stepTarget )
{
bool			success = false;
unsigned int	targetStepIndex;

	// Choose the target step index. Force slow if another control loop flagged overtemp (we're in meta state 1)
	// else set Stepping to the state requested in -stepTarget-

	if ( getMetaState()->isEqualTo( gIOPPluginOne ) )
	{
		targetStepIndex = kPROCESSOR_STEPPING;
	}
	// else
	{
		targetStepIndex = stepTarget;
	}

	// If a change is required, do it
	if ( targetStepIndex == _currentProcessorStepSetting )
	{
		//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustStepVoltControls No Action Required (%d)\n", _currentProcessorStepSetting );
		success = true;
	}
	else
	{
		success = setProcessorStepIndex( targetStepIndex );
	}

	// An error is considered fatal/unrecoverable until we go through a sleep cycle
	if ( !success )
	{
		CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustStepVoltControls - unable to change Step (fatal error)\n" );
		IOLog( "PowerMac11_2_SlewCtrlLoop::adjustStepVoltControls - unable to change Step (fatal error)\n" );
	}

	return( success );
}




/*******************************************************************************
 * Method:
 *	adjustSlewStepVoltControls
 *
 * Purpose:
 *	Adjust controls when we have full stepping and slewing capability.
 *
 * Arguments
 *	stepTarget			0	kPROCESSOR_FULL_SPEED or
 *						1	kPROCESSOR_STEPPING
 *
 *	slewTarget			0	kVOLTAGE_NOT_SLEWING or
 *						1	kVOLTAGE_SLEWING_ON
 *
 ******************************************************************************/
bool PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls( unsigned int stepTarget, unsigned int slewTarget )
{
bool				success = false;
unsigned int		previousSlewIndex, targetSlewIndex;
unsigned int		previousStepIndex, targetStepIndex;
bool				needToStep, needToSlew;

	previousStepIndex	= _currentProcessorStepSetting;
	previousSlewIndex	= _currentVoltageSlewSetting;

	// find the slewpoint that corresponds to the current temperature, or use the slowest
	// slewpoint if another control loop flagged overtemp.
	if ( getMetaState()->isEqualTo( gIOPPluginOne ) )	// someone else (CPUsCtrlLoop) has told us we're in overtemp
	{
		targetSlewIndex = kVOLTAGE_SLEWING_ON;
		targetStepIndex = kPROCESSOR_STEPPING;
	}
	else
	{
		targetSlewIndex = slewTarget;					// attempt to do what the caller has requested
		targetStepIndex = stepTarget;
	}


	needToStep = ( previousStepIndex != targetStepIndex );
	needToSlew = ( previousSlewIndex != targetSlewIndex );


	// if we're already at the desired slewing/stepping, don't do anything
	if ( ! needToSlew && ! needToStep )
	{
		//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls No Action Required (%d,%d)\n",
		//		previousSlewIndex, previousStepIndex );
		return(true);
	}


	do	// dummy do-while loop
	{
		/*
		 * We need to make sure that we don't cause a transition to go outside the rated frequency/voltage
		 * constraints. Since we've already raised the voltage (if necessary), we can almost always just
		 * step first, then slew. The one case we can't is if we're slewing down and stepping up -- then we
		 * have to do the slew first.  It seems unlikely that we would be doing that, but ...
		 *
		 * The state machine here is:	needToStep && needToSlew means that both stepping and slewing need to change
		 *								targetSlewIndex == kVOLTAGE_NOT_SLEWING means we want to boost the voltage
		 *								targetStepIndex == KPROCESSOR_FULL_SPEED means we want to boost the frequency
		 * When you need to boost both voltage and stepping, the voltage boost must be done first.
		 * If both ( needToStep && needToSlew ) are true and the targets are what they are, then the implication is
		 * that we currently ARE voltage slewing and are currently stepping.
		 */

		if ( ( needToStep && needToSlew ) && /* currentValues == SLEWING_ON, STEPPING_ON */
		     ( targetSlewIndex == kVOLTAGE_NOT_SLEWING ) && ( targetStepIndex == kPROCESSOR_FULL_SPEED ) )
		{
			// slew UP first
			//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls - going to full voltage (%d,%d) => (%d,%d)\n",
			//		previousSlewIndex, previousStepIndex, targetSlewIndex, targetStepIndex );
			if( ! setVoltageSlewIndex( targetSlewIndex ) )
			{
				CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls (step+slew) - failed to change slewing target behavior\n" );
				break;
			}

			// then step UP
			// CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls - Set Stepping OFF (%d,%d) => (%d,%d)\n",
			//		previousSlewIndex, previousStepIndex, targetSlewIndex, targetStepIndex );
			if( ! setProcessorStepIndex( targetStepIndex ) )
			{
				CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls (step+slew) - failed to change stepping target behavior\n" );
				break;
			}
		}
		else	// all other cases, it's OK to just step first
				// this encompasses the step up/down (if no voltage slewing is involved) or
				// the step down, where the processor is stepped down first, then the voltage is slewed.
				// you shouldn't ever be able to be in the voltage is currently slewed and the processor
				// wants to step UP, since that's the case the -if- above catches.  if one cannot slew,
				// -needToSlew- should never be true.
		{
			if ( needToStep )
			{
				//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls Set stepping state (%d) => (%d)\n",
				//			previousStepIndex, targetStepIndex );
				if( ! setProcessorStepIndex( targetStepIndex ) )
				{
					CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls (step) - failed to change stepping target behavior\n" );
					break;
				}
			}

			if ( needToSlew )
			{
				//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls Set voltage slew state (%d) => (%d)\n",
				//		previousSlewIndex, targetSlewIndex );
				if( ! setVoltageSlewIndex( targetSlewIndex ) )
				{
					CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls (slew) = failed to change slewing target behavior\n" );
					break;
				}
			}
		}

		// done!
		success = true;

	} while ( false );

	// An error is considered fatal/unrecoverable until we go through a sleep cycle
	// *** do we really believe showing this output will make a difference?
	if ( !success )
	{
		//CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls FATAL ERROR OCCURRED !! \n" );
		CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::adjustSlewAndStepControls (step=%d,slew=%d) - failed\n", targetStepIndex,targetSlewIndex );
	}

	return( success );
}



/*******************************************************************************
 * Method:
 *	updateMetaState
 *
 * Purpose:
 *	Examine environmental conditions and choose SlewCtrlLoop's meta state.
 ******************************************************************************/
// virtual
bool PowerMac11_2_SlewCtrlLoop::updateMetaState( void )
	{
	const OSNumber	* newMetaState;
	bool			intOverTemp;
	bool			extOverTemp;

	// we are in metastate 0 by default.  If any of the following are true, we are in metastate 1:
	//    internal overtemp
	//    external overtemp

	intOverTemp = platformPlugin->envArrayCondIsTrue( gIOPPluginEnvInternalOvertemp );
	extOverTemp = platformPlugin->envArrayCondIsTrue( gIOPPluginEnvExternalOvertemp );

	if ( intOverTemp || extOverTemp )	newMetaState = gIOPPluginOne;
	else								newMetaState = gIOPPluginZero;

	if ( ! newMetaState->isEqualTo( getMetaState() ) )
	{
		setMetaState( newMetaState );

		return( true );
	}

	return( false );
}

#pragma mark -
#pragma mark ***     Sleep/Wake  Entry Points     ***
#pragma mark -

/*******************************************************************************
 * Method:
 *	willSleep
 *
 * Purpose:
 *	This is a virtual method that's called when the system is going down to sleep
 ******************************************************************************/
// virtual
void PowerMac11_2_SlewCtrlLoop::willSleep( void )
{
	CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::willSleep - entered\n" );

	// transition to full freq, /1 stepping before sleep

	// if you're not at full speed and _can_ step, step the processor up to full speed
	do	// seemingly unnecessary -loop- construct
	{
		if ( _canStepProcessors && ( _currentProcessorStepSetting != kPROCESSOR_FULL_SPEED ) )
		{
			// ... but you can't raise the frequency unless you're running at highest voltage
			if ( _canSlewProcessors && ( _currentVoltageSlewSetting != kVOLTAGE_NOT_SLEWING ) )
			{
				// raise voltage first
				CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::willSleep - must raise voltage first\n" );
				if ( ! setVoltageSlewIndex( kVOLTAGE_NOT_SLEWING ) )	// raise voltage to highest
				{
					CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::willSleep - slew operation failed!\n" );
					break;
				}
			}
			// then raise freq
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::willSleep - now raising speed to full\n" );
			setProcessorStepIndex( kPROCESSOR_FULL_SPEED );		// full-speed freq
		}
		else
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::willSleep - already at desired step/slew target.\n" );
	} while ( false );		// bogus loop so that we can use -break- to escape code path if voltage slewing fails

	CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::willSleep - ready to sleep\n" );

	super::willSleep();			// invoke super-class;  sets ctrlloopState 
}



/*******************************************************************************
 * Method:
 *	didWake
 *
 * Purpose:
 *	This is a virtual method that's called when the system is waking from sleep
 ******************************************************************************/
// virtual
void PowerMac11_2_SlewCtrlLoop::didWake( void )
{
unsigned int	value;


	if ( ctrlloopState == kIOPCtrlLoopWillSleep )		// if not actually waking, but starting up
	{
		// The processor wakes at /1 stepping
		_currentProcessorStepSetting = kPROCESSOR_FULL_SPEED;
		if ( getCurrentVoltageSlewIndex( &value ) )
		{
			_currentVoltageSlewSetting = value;
		}

		// The system always wakes at slew point 0 (full speed, voltage),
		// regardless of what slew point we were at when the system went to sleep.
		// Make sure everything stays in sync.

		if ( _canSlewProcessors || _canStepProcessors )
		{
			CTRLLOOP_DLOG( "PowerMac11_2_SlewCtrlLoop::didWake - adjusting step/slew controls" );
			adjustControls();
		}
	}

	super::didWake();			// invoke super-class;  sets ctrlloopState 
}
