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
 *  File: $Id: SMU_Neo2_SlewCtrlLoop.cpp,v 1.15 2004/12/03 23:19:46 raddog Exp $
 *
 */


#include <IOKit/IOLib.h>
#include <machine/machine_routines.h>

#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"

#include "SMU_Neo2_PlatformPlugin.h"
#include "SMU_Neo2_SlewCtrlLoop.h"


OSDefineMetaClassAndStructors( SMU_Neo2_SlewCtrlLoop, IOPlatformCtrlLoop )


static const OSSymbol*				gSMU_Neo2_SetVoltageSlewKey = NULL;

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
bool SMU_Neo2_SlewCtrlLoop::init( void )
{
	if ( !IOPlatformCtrlLoop::init() )
	{
		return( false );
	}

	_slewControl = NULL;
	_cpu0TempSensor = NULL;

	_appleSMU = NULL;

	_canStepProcessor = false;
	_canSlewProcessor = false;

	_fatalErrorOccurred = false;

	_numSlewPoints = 0;
	_slewPoints = NULL;


	AbsoluteTime_to_scalar( &_criticalDeadline ) = 0;

	gSMU_Neo2_SetVoltageSlewKey = OSSymbol::withCString( "setVoltageSlew" );

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
void SMU_Neo2_SlewCtrlLoop::free( void )
	{
	if ( gSMU_Neo2_SetVoltageSlewKey != NULL )
		{
		gSMU_Neo2_SetVoltageSlewKey->release();
		gSMU_Neo2_SetVoltageSlewKey = NULL;
		}

	if ( _slewPoints ) IOFree( _slewPoints, _numSlewPoints * sizeof(fvt_operating_point_t) );

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
IOReturn SMU_Neo2_SlewCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
	IOReturn status;
	OSArray * array;
	OSNumber * number;


	/*
	 * Invoke the base class implementation
	 */

	if ((status = IOPlatformCtrlLoop::initPlatformCtrlLoop( dict )) != kIOReturnSuccess)
		return(status);


	/*
	 *
	 * Grab a few properties out of the thermal profile:
	 *	control-id for the slew control
	 *	sensor-id for the cpu temperature sensor
	 *	step-index-limit, which tells us the max allowed PowerTune index (defaults to 1 => /2)
	 *
	 */

	// index 0 in the ControlIDArray is the control-id of the slew control
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL ||
		(_slewControl = platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(0)) )) == NULL)
	{
		CTRLLOOP_DLOG("SMU_Neo2_SlewCtrlLoop::initPlatformCtrlLoop no control ID!!\n");
		//return(kIOReturnBadArgument);
	}
	else
		addControl( _slewControl );

	// index 0 in the SensorIDArray is the sensor-id of the cpu temperature sensor
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalSensorIDsKey))) == NULL ||
		(_cpu0TempSensor = OSDynamicCast(IOPlatformSensor, platformPlugin->lookupSensorByID( OSDynamicCast(OSNumber, array->getObject(0)) ))) == NULL)
	{
		CTRLLOOP_DLOG("SMU_Neo2_SlewCtrlLoop::initPlatformCtrlLoop no sensor ID!!\n");
		//return(kIOReturnBadArgument);
	}
	else
		addSensor( _cpu0TempSensor );

	/*
	 *
	 *  Calculate the deadline interval. This is used to ensure that the slew control loop always
	 *	responds to changing CPU temperature, even if no other control loop is polling the temperature
	 *	sensor.
	 *
	 */

	clock_interval_to_absolutetime_interval( 2, NSEC_PER_SEC, &_deadlineInterval );
	_deadlineActive = false;

	/*
	 *	Look for the step-index-limit key -- this tells us the limit on the processor stepping index.
	 */

	if ((number = OSDynamicCast(OSNumber, dict->getObject("step-index-limit"))) != NULL)
	{
		_stepIndexLimit = number->unsigned32BitValue();
		CTRLLOOP_DLOG("SMU_Neo2_SlewCtrlLoop::initPlatformCtrlLoop step-index-limit is %d\n", _stepIndexLimit);
	}
	else
		_stepIndexLimit = 1;		// default to only allow /1 and /2

	/*
	 *
	 *	Attempt to fetch the F/V/T table from the SDB.  Get Tmax from the highest listed SYSCLK
	 *	frequency, and publish it for use by the cpu fan control loop.
	 *
	 */

	if ( fetchFVTTable() )
	{
		_tmax.sensValue = _slewPoints[_numSlewPoints - 1].Tmax.sensValue;

		OSNumber * tmax = OSNumber::withNumber( _tmax.sensValue, 32 );
		platformPlugin->setEnv( "cpu-tmax", tmax );
		tmax->release();
	}

	/*
	 *
	 *	Check conditions that processor stepping depends on. We need the power-mode-data
	 *	property present. We need the ability to switch voltage; if Open Firmware has left
	 *	the machine at low voltage, we can't step to high frequency without switching
	 *	voltage.
	 *
	 */

	IORegistryEntry*							cpusRegEntry;
    cpusRegEntry = platformPlugin->fromPath("/cpus/@0", gIODTPlane);

	if ( _numSlewPoints != 0 &&
	     ( _appleSMU = gPlatformPlugin->getAppleSMU() ) != NULL &&
		 cpusRegEntry->getProperty( "power-mode-data" ) != NULL )
	{
		_canStepProcessor = true;
	}
	else
	{
		IOLog( "SMU_Neo2_SlewCtrlLoop::initPlatformCtrlLoop CPU stepping disabled\n" );
	}

	cpusRegEntry->release();

	/*
	 *
	 *	Set the initial operating point and return.
	 *
	 */

	if ( _canStepProcessor )
	{
		if ( !setInitialState() )
		{
			IOLog( "SMU_Neo2_SlewCtrlLoop::initPlatformCtrlLoop FAILED to set initial operating point\n" );
		}
	}

	// stick around
	return( kIOReturnSuccess );
}

/*******************************************************************************
 * Method:
 *	fetchFVTTable
 *
 * Purpose:
 *	Fetch the Frequenct/Voltage/Temperature operating points partition from the
 *	SDB (partition 0x12). Parse the table and store it for use in slewing/stepping
 *	algorithms.
 *
 * Notes:
 *	This code will only parse up to 5 F/V/T entries. If the table is larger than
 *	5 entries, it is considered invalid.
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::fetchFVTTable( void )
{
	UInt8					*partitionBuf;
	sdb_partition_header_t	partHeader;
	sdb_cpu_fvt_entry_t		*fvtEntry;
	short unsigned int 		nBytes;

#define SDB_BUF_SIZE (sizeof(sdb_partition_header_t) + ( 5 * sizeof(sdb_cpu_fvt_entry_t) ))

	// allocate a temporary buffer to read in the F/V/T table
	if ((partitionBuf = (UInt8 *) IOMalloc( SDB_BUF_SIZE )) == NULL) return(false);

	// get the partition header
	if ( !gPlatformPlugin->getSDBPartitionData( kCPU_FVT_OperatingPointsPartID, 0, sizeof(sdb_partition_header_t), (UInt8 *) &partHeader ) )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::fetchFVTTable failed to fetch SDB partition header\n" );
		goto Error_Exit;
	}

	// calculate the size of the partition in bytes
	nBytes = partHeader.pLEN * sizeof(UInt32);

	// make sure we don't request more bytes than we allocated in our buffer -- which is enough for the max 5 F/V/T entries
	if (nBytes > SDB_BUF_SIZE)
	{
		IOLog( "SMU_Neo2_SlewCtrlLoop::fetchFVTTable FAILED: too many F/V/T operating points\n" );
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::fetchFVTTable FAILED: too many F/V/T operating points\n" );
		goto Error_Exit;
	}

	// fetch the partition
	if ( !gPlatformPlugin->getSDBPartitionData( kCPU_FVT_OperatingPointsPartID, 0, nBytes, partitionBuf ) )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::fetchFVTTable failed to fetch SDB partition\n" );
		goto Error_Exit;
	}

	// start calculating the number of operating points listed in the SDB
	nBytes  = ((sdb_partition_header_t *)partitionBuf)->pLEN;	// pLEN is a count of UInt32s
	nBytes *= sizeof(UInt32);									// convert to number of bytes
	nBytes -= sizeof(sdb_partition_header_t);					// subtract the size of the partition header

	// make sure the remaining number of bytes is a multiple of the fvt entry length
	if ( ( nBytes % sizeof( sdb_cpu_fvt_entry_t) ) != 0 )
	{
		IOLog( "SMU_Neo2_SlewCtrlLoop::fetchFVTTable FAILED: F/V/T partition size invalid\n" );
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::fetchFVTTable FAILED: F/V/T partition size invalid\n" );
		goto Error_Exit;
	}

	// finally, compute the actual number of slew points
	_numSlewPoints = nBytes / sizeof(sdb_cpu_fvt_entry_t);

	// make sure there's something to look at in the table
	if ( _numSlewPoints == 0 )
	{
		IOLog( "SMU_Neo2_SlewCtrlLoop::fetchFVTTable FAILED: no operating points specified in F/V/T partition\n" );
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::fetchFVTTable FAILED: no operating points specified in F/V/T partition\n" );
		goto Error_Exit;
	}
 
	// build our own operating point table
	if ((_slewPoints = (fvt_operating_point_t *) IOMalloc( _numSlewPoints * sizeof(fvt_operating_point_t) )) == NULL)
		goto Error_Exit;

	fvtEntry = (sdb_cpu_fvt_entry_t *) (partitionBuf + sizeof(sdb_partition_header_t));	// point to first entry

	for ( unsigned int i = 0; i < _numSlewPoints; i++ )		// walk through entries and tranform/copy data
	{
		_slewPoints[i].freq = SwapUInt32SMU( fvtEntry[i].freq );

		if (fvtEntry[i].Tmax != 0xFF)
			_slewPoints[i].Tmax.sensValue = ((SInt32) fvtEntry[i].Tmax ) << 16;	// convert to signed 16.16
		else
			_slewPoints[i].Tmax.sensValue = 60 << 16;	// if Tmax is absent, use something safe

		_slewPoints[i].Vcore[0] = SwapUInt16SMU( fvtEntry[i].Vcore[0] );
		_slewPoints[i].Vcore[1] = SwapUInt16SMU( fvtEntry[i].Vcore[1] );
		_slewPoints[i].Vcore[2] = SwapUInt16SMU( fvtEntry[i].Vcore[2] );

		CTRLLOOP_DLOG( "          %02d: %ld Hz %02dC /1 0x%04X /2 0x%04X /4 0x%04X\n",
				i,
				_slewPoints[i].freq,
				_slewPoints[i].Tmax.thermValue.intValue,
				_slewPoints[i].Vcore[0],
				_slewPoints[i].Vcore[1],
				_slewPoints[i].Vcore[2]);
	}

	// free the temporary memory we used to hold the sdb partition
	IOFree( partitionBuf, SDB_BUF_SIZE );

	return(true);

Error_Exit:
	if (partitionBuf) IOFree( partitionBuf, SDB_BUF_SIZE );
	return(false);

}

/*******************************************************************************
 * Method:
 *	sensorRegistered
 *
 * Purpose:
 *	Complete control loop setup after a sensor registers w/ the plugin.
 ******************************************************************************/
// virtual
void SMU_Neo2_SlewCtrlLoop::sensorRegistered( IOPlatformSensor * aSensor )
{
	if ( aSensor == _cpu0TempSensor )
	{
		// now that we've got a cpu temperature sensor, set the 2 sec deadline to
		// ensure the slew control loop always reacts to temperature changes
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::sensorRegistered setting 2 sec deadline\n" );
		clock_absolutetime_interval_to_deadline( _deadlineInterval, &deadline );
	}

	// attempt to set up slewing
	setupSlewing();
}

/*******************************************************************************
 * Method:
 *	controlRegistered
 *
 * Purpose:
 *	Complete control loop setup after a control registers w/ the plugin.
 ******************************************************************************/
// virtual
void SMU_Neo2_SlewCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
	// attempt to set up slewing
	setupSlewing();
}

/*******************************************************************************
 * Method:
 *	setupSlewing
 *
 * Purpose:
 *	Complete initialization/setup of frequency slewing, including sending
 *	slewpoints to the pulsar driver so it can initialize its slewpoint
 *	registers. All pre-conditions (CPU temperature sensor is registered,
 *	successfully parsed F/V/T table, slew controller is registered) are met
 *	before this method is called.
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::setupSlewing( void )
{
	IOReturn status;

	if ( !_cpu0TempSensor )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setupSlewing FAILED: _cpu0TempSensor is NULL\n" );
		return( false );
	}

	if ( !_slewControl )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setupSlewing FAILED: _slewControl is NULL\n" );
		return( false );
	}

	if ( !_slewPoints )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setupSlewing FAILED: no F/V/T tables\n" );
		return( false );
	}

	if ( _cpu0TempSensor->isRegistered() != kOSBooleanTrue )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setupSlewing FAILED: CPU temperature sensor has not yet registered\n" );
		return( false );
	}
		
	if ( _slewControl->isRegistered() != kOSBooleanTrue )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setupSlewing FAILED: slew controller has not yet registered\n" );
		return( false );
	}

	// send the slewpoint data to the pulsar driver
	UInt32 * freqs;
	if ((freqs = (UInt32 *) IOMalloc( 5 * sizeof(UInt32) )) == NULL)
		return(false);

	for (unsigned int i = 0; i<5; i++)
	{
		if (i < _numSlewPoints)
			freqs[i] = _slewPoints[i].freq;
		else
			freqs[i] = _slewPoints[_numSlewPoints - 1].freq;
	}

	status = _slewControl->getControlDriver()->callPlatformFunction( "setPulsarSlewPoints", false, (void *) freqs, NULL, NULL, NULL );

	IOFree( freqs, 5 * sizeof(UInt32) );

	if (status != kIOReturnSuccess)
	{
		CTRLLOOP_DLOG("SMU_Neo2_SlewCtrlLoop::setupSlewing failed to set pulsar slew points, error: 0x%08lX\n", status);
		return(false);
	}

	// success!! now that slewing is in the picture call adjustControls()
	_canSlewProcessor = true;

	CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setupSlewing completed successfully!\n" );

	adjustControls();

	return(true);
}

#pragma mark -
#pragma mark ***     Slew/Step/Voltage Primitives     ***
#pragma mark -

/*******************************************************************************
 * Method:
 *	setProcessorSlewIndex
 *
 * Purpose:
 *	this routine will instruct the slew control driver to slew to slewIndex. It
 *	does NOT check to see if slewIndex is different than the current state,
 *	callers must make that check themselves.
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::setProcessorSlewIndex( unsigned int slewIndex )
{
	bool status;

	CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setProcessorSlewIndex slewing to point %d\n", slewIndex );

	status = _slewControl->sendTargetValue( slewIndex );


	if ( status )
	{
		_slewControl->setTargetValue( slewIndex );
		_slewControl->setCurrentValue( _slewControl->forceAndFetchCurrentValue() );

		int pt = ( slewIndex == 0 ) ? _numSlewPoints - 1 : slewIndex - 1;

		_tmax.sensValue = _slewPoints[pt].Tmax.sensValue;

		OSNumber * tmax = OSDynamicCast( OSNumber, platformPlugin->getEnv( "cpu-tmax" ) );
		if (tmax) tmax->setValue( (UInt32) _tmax.sensValue );

		return(true);
	}
	else
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setProcessorSlewIndex failed slew to point %d\n", slewIndex );
		return(false);
	}
}

/*******************************************************************************
 * Method:
 *	setProcessorStepIndex
 *
 * Purpose:
 *	Set the processor stepping to the desired stepping index.
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::setProcessorStepIndex( unsigned int stepIndex )
	{

	// check to make sure stepIndex is within bounds. currently, only /1 and /2 are supported, so the
	// limit is set accordingly.
	if ( stepIndex > _stepIndexLimit )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setProcessorStepIndex: stepIndex %d is out of range, using %d.\n", stepIndex, _stepIndexLimit );

		stepIndex = _stepIndexLimit;
		}

#if defined( __ppc__ )
	ml_set_processor_speed( stepIndex );
#endif
	_stepIndex = stepIndex;

	return( true );
	}

/*******************************************************************************
 * Method:
 *	setProcessorVoltageIndex
 *
 * Purpose:
 *	Set the processor voltage to the desired slew/step index.
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::setProcessorVoltageIndex( unsigned int slewIndex, unsigned int stepIndex )
	{
	IOReturn										result;

	CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setProcessorVoltageIndex setting voltage to %d,%d\n", slewIndex, stepIndex );

	// If we haven't gotten the SMU reference, try and get it.
	if ( _appleSMU == NULL )
		{
		if ( ( _appleSMU = gPlatformPlugin->getAppleSMU() ) == NULL )
			return( true );
		}

	// Sanity check the slew point -- slew point zero corresponds to 0 Volts Vcore. We never want
	// to turn off the voltage to the processor.

	if ( slewIndex == 0 )
	{
		IOLog( "SMU_Neo2_SlewCtrlLoop - Attempted to set VID/Slewpoint to 0!! \n" );
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setProcessorVoltageIndex - Attempted to set VID/Slewpoint to 0!! \n" );
		return ( kIOReturnError );
	}

	// Handle the mechanics of reducing the processor voltage.  If we are PowerTuning, we only can go down
	// to /2.  Currently 0 is full speed and 1 is /2.

	if ( stepIndex > _stepIndexLimit )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setProcessorVoltageIndex: stepIndex %d is out of range, using %d.\n", stepIndex, _stepIndexLimit );

		stepIndex = _stepIndexLimit;
		}

	// param1 -> processorIndex (0xFF for all processors).
	// param2 -> slewPoint.
	// param3 -> stepPoint.  0 => /1, 1 => /2, 2 => /4, 3 => /64.  Since we only support /2, this will either be 0 or 1.

	if ( ( result = _appleSMU->callPlatformFunction( gSMU_Neo2_SetVoltageSlewKey, true, ( void * ) 0xFF, ( void * ) slewIndex, ( void * ) stepIndex, 0 ) ) != kIOReturnSuccess )
		{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setProcessorVoltageIndex: Error from setVoltageSlew (0x%08X).\n", result );

		return( false );
		}

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
 *	whether or not we can step and switch voltage. At this early point we'll
 *	never have a temp sensor or a slew controller available yet.
 *
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::setInitialState( void )
{
	bool success = false;

	do {	// use a do { ... } while (false); to facilitate bailing out on errors

	// perform a safe transision to a known state - high voltage and /1 stepping
	CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setInitialState Set Voltage %d,0 \n", _slewPoints ? _numSlewPoints : 1 );
	if ( !setProcessorVoltageIndex( _slewPoints ? _numSlewPoints : 1, 0 ) ) break;

	CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setInitialState Set Stepping 0\n" );
	if ( !setProcessorStepIndex( 0 ) ) break;
	
	// fetch the current Dynamic Power Step setting
	const OSNumber * curDPSNum = OSDynamicCast( OSNumber, gPlatformPlugin->getEnv( gIOPPluginEnvDynamicPowerStep ) );
	unsigned int curDPS = curDPSNum ? curDPSNum->unsigned32BitValue() : 0;

	// now we're running fast. if DPS says slow, verify slow is supported and then slow down.
	if ( curDPS != 0 && _slewPoints && _slewPoints[ _numSlewPoints ].Vcore[ _stepIndexLimit ] != 0 )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setInitialState DPS => SLOW: Set Stepping %d\n", _stepIndexLimit );
		if ( !setProcessorStepIndex( _stepIndexLimit ) ) break;

		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setInitialState DPS => SLOW: Set Voltage %d,%d \n", _numSlewPoints, _stepIndexLimit );
		if ( !setProcessorVoltageIndex( _numSlewPoints, _stepIndexLimit ) ) break;
	}

	// if we get here, the method succeeded
	success = true;

	} while ( false );

	// An error is considered fatal/unrecoverable until we go through a sleep cycle
	if ( !success )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::setInitialState FATAL ERROR OCCURRED !! \n" );
		IOLog( "SMU_Neo2_SlewCtrlLoop::setInitialState FATAL ERROR OCCURRED !! \n" );
		_fatalErrorOccurred = true;
	}

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
void SMU_Neo2_SlewCtrlLoop::adjustControls( void )
{
	bool				success = false;
	const OSNumber*		tmpNumber;

	unsigned int		dpsTarget;		// Dynamic Power Step's requested speed:
										//		0 = fast, non-zero = slow

	SensorValue			currentTemp;
	bool				currentTempValid = false;
										// If the temperature sensor is available,
										// currentTempValid will be set to true
										// and currentTemp will be set to the
										// current temperature reading. Otherwise,
										// currentTempValid will be false.

	// if there's been a fatal error, the machine is in an indeterminate state. do nothing.
	if ( _fatalErrorOccurred ) return;

	// if we've already prepared for sleep, do nothing
	if ( ctrlloopState == kIOPCtrlLoopWillSleep )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustControls going down to sleep, so doing nothing...\n" );
		return;
	}

	// fetch the current Dynamic Power Step setting
	tmpNumber = OSDynamicCast( OSNumber, gPlatformPlugin->getEnv( gIOPPluginEnvDynamicPowerStep ) );
	dpsTarget = tmpNumber ? tmpNumber->unsigned32BitValue() : 0;

	// fetch the current temperature
	if ( _cpu0TempSensor && ( _cpu0TempSensor->isRegistered() == kOSBooleanTrue ) )
	{
		currentTemp = _cpu0TempSensor->getCurrentValue();
		currentTempValid = true;
	}

	// Call the appropriate helper method to perform necessary state changes
	if ( _canSlewProcessor && _canStepProcessor )
	{
		success = adjustSlewStepVoltControls( dpsTarget, currentTempValid, currentTemp );
	}
	else if ( _canSlewProcessor )
	{
		success = adjustSlewVoltControls( dpsTarget, currentTempValid, currentTemp );
	}
	else if ( _canStepProcessor )
	{
		success = adjustStepVoltControls( dpsTarget, currentTempValid, currentTemp );
	}
	else
	{
		success = true;
	}

	if ( !success )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustControls failed to change operating point! \n" );
		IOLog( "SMU_Neo2_SlewCtrlLoop::adjustControls failed to change operating point! \n" );
	}

	// Call the routine that implements failsafe mechanisms
	checkThermalFailsafes( currentTempValid, currentTemp );
}

/*******************************************************************************
 * Method:
 *	adjustStepVoltControls
 *
 * Purpose:
 *	Adjust controls for case when we can only do stepping.
 *
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls( unsigned int dpsTarget, bool currentTempValid, SensorValue currentTemp )
{
	bool				success = false;
	unsigned int		previousStepIndex, targetStepIndex;

	previousStepIndex = _stepIndex;

	// Choose the target step index. Force slow if the current temp exceeds Tmax, or
	// if another control loop flagged overtemp (we're in meta state 1). Otherwise
	// use the speed provided by DPS.
	if ( getMetaState()->isEqualTo( gIOPPluginOne ) ||
	     ( currentTempValid && _slewPoints && ( currentTemp.sensValue >= _tmax.sensValue ) ) )
	{
		targetStepIndex = _stepIndexLimit;
	}
	else
	{
		targetStepIndex = ( dpsTarget == 0 ? 0 : _stepIndexLimit );
	}

	// verify that the target state is valid
	if ( _slewPoints && _slewPoints[ _numSlewPoints - 1 ].Vcore[ targetStepIndex ] == 0 )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls Invalid Target State (%d,%d)\n",
				_numSlewPoints - 1, targetStepIndex );
		return(false);
	}

	// If a change is required, do it
	if ( targetStepIndex == previousStepIndex )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls No Action Required (?,%d)\n", previousStepIndex );
		success = true;
	}
	else
	{
		do {
	
		if ( targetStepIndex == 0 )	// LOW => HIGH
		{
			// raise voltage first
			CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls Pre-Set Voltage (?,%d) => (%d,%d)\n",
					previousStepIndex, _slewPoints ? _numSlewPoints : 1, targetStepIndex );
			if ( !setProcessorVoltageIndex( _slewPoints ? _numSlewPoints : 1, targetStepIndex ) ) break;
		
			// then raise freq
			CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls Set Stepping (?,%d) => (%d,%d)\n",
					previousStepIndex, _slewPoints ? _numSlewPoints : 1, targetStepIndex );
			if ( !setProcessorStepIndex( targetStepIndex ) ) break;
		}
		else	// HIGH => LOW
		{
			// lower freq first
			CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls Pre-Set Stepping (?,%d) => (%d,%d)\n",
					previousStepIndex, _slewPoints ? _numSlewPoints : 1, targetStepIndex );
			if ( !setProcessorStepIndex( _stepIndexLimit ) ) break;
		
			// then lower voltage
			CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls Set Voltage (?,%d) => (%d,%d)\n",
					previousStepIndex, _slewPoints ? _numSlewPoints : 1, targetStepIndex );
			if ( !setProcessorVoltageIndex( _slewPoints ? _numSlewPoints : 1, targetStepIndex ) ) break;
		}

		success = true;

		} while ( false );
	}

	// An error is considered fatal/unrecoverable until we go through a sleep cycle
	if ( !success )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls FATAL ERROR OCCURRED !! \n" );
		IOLog( "SMU_Neo2_SlewCtrlLoop::adjustStepVoltControls FATAL ERROR OCCURRED !! \n" );
		_fatalErrorOccurred = true;
	}

	return( success );
}

/*******************************************************************************
 * Method:
 *	adjustSlewVoltControls
 *
 * Purpose:
 *	Adjust controls for case when we can only slew, no stepping capability.
 *
 *	Some assumptions are made by this method based on the fact that it can only
 *	be called when _canSlewProcessor has been set to true:
 *
 *		_slewControl is non-NULL and _slewControl->isRegistered() is true
 *		_cpu0TempSensor is non-NULL and _cpu0TempSensor->isRegistered() is true
 *		by virtue of above, currentTempValid will always be true
 *
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls( unsigned int dpsTarget, bool currentTempValid, SensorValue currentTemp )
{
	bool				success = false;
	unsigned int		previousSlewIndex, targetSlewIndex;

	previousSlewIndex = _slewControl->getCurrentValue();

	// find the slewpoint that corresponds to the current temperature, or use the slowest
	// slewpoint if another control loop flagged overtemp.
	if ( getMetaState()->isEqualTo( gIOPPluginOne ) )
	{
		targetSlewIndex = 1;
	}
	else
	{
		targetSlewIndex = _numSlewPoints;	// start at the highest sysclk and work backwards

		while ( targetSlewIndex > 1 )
		{
			if ( currentTemp.sensValue < _slewPoints[ targetSlewIndex - 1 ].Tmax.sensValue )
				break;
	
			targetSlewIndex -= 1;	// lower the sysclk to give thermal margin
		}
	}

	// verify that the target state is valid
	if ( _slewPoints[ targetSlewIndex - 1 ].Vcore[ 0 ] == 0 )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls Invalid Target State (%d,%d)\n",
				targetSlewIndex, 0 );
		return(false);
	}

	// If a change is required, do it
	do {	// dummy do-while loop -- so I can bail out if an error occurs
	
	if ( targetSlewIndex > previousSlewIndex )
	{
		// raise voltage first
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls Pre-Set Voltage (%d,%d) => (%d,%d)\n",
				previousSlewIndex, 0, targetSlewIndex, 0 );
		if ( !setProcessorVoltageIndex( targetSlewIndex, 0 ) ) break;
	
		// then raise freq
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls Set Slew Point (%d,%d) => (%d,%d)\n",
				previousSlewIndex, 0, targetSlewIndex, 0 );
		if ( !setProcessorSlewIndex( targetSlewIndex ) ) break;

		success = true;
	}
	else if ( targetSlewIndex < previousSlewIndex )
	{
		// lower freq first
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls Pre-Set Slew Point (%d,%d) => (%d,%d)\n",
				previousSlewIndex, 0, targetSlewIndex, 0 );
		if ( !setProcessorSlewIndex( targetSlewIndex ) ) break;
	
		// then lower voltage
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls Set Voltage (%d,%d) => (%d,%d)\n",
				previousSlewIndex, 0, targetSlewIndex, 0 );
		if ( !setProcessorVoltageIndex( targetSlewIndex, 0 ) ) break;

		success = true;
	}
	else // if ( targetSlewIndex == previousSlewIndex )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls No Action Required (%d,%d)\n", previousSlewIndex, 0 );		
		success = true;
	}

	} while ( false );	// end of dummy do-while loop

	// An error is considered fatal/unrecoverable until we go through a sleep cycle
	if ( !success )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls FATAL ERROR OCCURRED !! \n" );
		IOLog( "SMU_Neo2_SlewCtrlLoop::adjustSlewVoltControls FATAL ERROR OCCURRED !! \n" );
		_fatalErrorOccurred = true;
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
 *	Some assumptions are made by this method based on the fact that it can only
 *	be called when _canSlewProcessor has been set to true:
 *
 *		_slewControl is non-NULL and _slewControl->isRegistered() is true
 *		_cpu0TempSensor is non-NULL and _cpu0TempSensor->isRegistered() is true
 *		by virtue of above, currentTempValid will always be true
 *
 ******************************************************************************/
bool SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls( unsigned int dpsTarget, bool currentTempValid, SensorValue currentTemp )
{
	bool				success = false;
	bool				didVoltageSwitch = false;
	unsigned int		previousSlewIndex, targetSlewIndex;
	unsigned int		previousStepIndex, targetStepIndex;
	bool				needToStep, needToSlew;

	previousStepIndex	= _stepIndex;
	previousSlewIndex	= _slewControl->getCurrentValue();

	// find the slewpoint that corresponds to the current temperature, or use the slowest
	// slewpoint if another control loop flagged overtemp.
	if ( getMetaState()->isEqualTo( gIOPPluginOne ) )
	{
		targetSlewIndex = 1;
	}
	else
	{
		targetSlewIndex = _numSlewPoints;	// start at the highest sysclk and work backwards

		while ( targetSlewIndex > 1 )
		{
			if ( currentTemp.sensValue < _slewPoints[ targetSlewIndex - 1 ].Tmax.sensValue )
				break;
	
			targetSlewIndex -= 1;	// lower the sysclk to give thermal margin
		}
	}

	// Choose the target step index. Force slow if the current temp exceeds Tmax, or
	// if another control loop flagged overtemp (we're in meta state 1). Otherwise
	// use the speed provided by DPS.
	if ( getMetaState()->isEqualTo( gIOPPluginOne ) ||
	     ( currentTempValid && _slewPoints && ( currentTemp.sensValue >= _slewPoints[ targetSlewIndex - 1 ].Tmax.sensValue ) ) )
	{
		targetStepIndex = _stepIndexLimit;
	}
	else
	{
		targetStepIndex = ( dpsTarget == 0 ? 0 : _stepIndexLimit );
	}

	// verify that the target state is valid
	if (_slewPoints[ targetSlewIndex - 1 ].Vcore[ targetStepIndex ] == 0)
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls Invalid Target State (%d,%d)\n",
				targetSlewIndex, targetStepIndex );
		return(false);
	}

	needToStep = ( previousStepIndex != targetStepIndex );
	needToSlew = ( previousSlewIndex != targetSlewIndex );

	// if we're already at the desired slewing/stepping, don't do anything
	if ( !needToSlew && !needToStep )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls No Action Required (%d,%d)\n",
				previousSlewIndex, previousStepIndex );
		return(true);
	}

	// there's a corner case on the very first state transition after the slew controller registers
	// and also at wake time. previousSlewIndex is 0 -- the machine powers up at slewpoint 0. We
	// don't want to index into _slewPoints[ previousSlewIndex - 1 ] when previousSlewIndex is 0,
	// so here we change previousSlewIndex to be the max index if it is 0. This will behave as expected
	// since the highest F/V/T entry is the same SYSCLK as slewpoint 0.
	if ( previousSlewIndex == 0 )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls Masking previousSlewPoint=0 with %d\n", _numSlewPoints );
		previousSlewIndex = _numSlewPoints;
	}

	do {	// dummy do-while loop

	// if the new voltage point is higher than the old voltage point, switch the voltage
 	if ( _slewPoints[ targetSlewIndex - 1 ].Vcore[ targetStepIndex ] >= _slewPoints[ previousSlewIndex - 1 ].Vcore[ previousStepIndex ] )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls Pre-Set Voltage (%d,%d) => (%d,%d)\n",
				previousSlewIndex, previousStepIndex, targetSlewIndex, targetStepIndex );
		if( !setProcessorVoltageIndex( targetSlewIndex, targetStepIndex ) ) break;
		didVoltageSwitch = true;
	}

	/*
	 * We need to make sure that we don't cause a transition to go outside the rated frequency/voltage
	 * constraints. Since we've already raised the voltage (if necessary), we can almost always just
	 * step first, then slew. The one case we can't is if we're slewing down and stepping up -- then we
	 * have to do the slew first.
	 */

	if ( ( targetSlewIndex < previousSlewIndex ) &&		// slewing to a lower freq
	     ( targetStepIndex < previousStepIndex ) )      // stepping to a higher freq
	{
		// slew first
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls Pre-Set Slew Point (%d,%d) => (%d,%d)\n",
				previousSlewIndex, previousStepIndex, targetSlewIndex, targetStepIndex );
		if( !setProcessorSlewIndex( targetSlewIndex ) ) break;

		// then step
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls Set Stepping (%d,%d) => (%d,%d)\n",
				previousSlewIndex, previousStepIndex, targetSlewIndex, targetStepIndex );
		if( !setProcessorStepIndex( targetStepIndex ) ) break;
	}
	else	// all other cases, it's OK to just step first
	{
		if (needToStep)
		{
			CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls Pre-Set Stepping (%d,%d) => (%d,%d)\n",
					previousSlewIndex, previousStepIndex, targetSlewIndex, targetStepIndex );
			if( !setProcessorStepIndex( targetStepIndex ) ) break;
		}

		if (needToSlew)
		{
			CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls Set Slew Point (%d,%d) => (%d,%d)\n",
					previousSlewIndex, previousStepIndex, targetSlewIndex, targetStepIndex );
			if( !setProcessorSlewIndex( targetSlewIndex ) ) break;
		}
	}

	// if the new voltage point is lower than the old, now we can lower the voltage
	if (!didVoltageSwitch)
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls Set Voltage (%d,%d) => (%d,%d)\n",
				previousSlewIndex, previousStepIndex, targetSlewIndex, targetStepIndex );
		if( !setProcessorVoltageIndex( targetSlewIndex, targetStepIndex ) ) break;
	}

	// done!
	success = true;

	} while ( false );

	// An error is considered fatal/unrecoverable until we go through a sleep cycle
	if ( !success )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls FATAL ERROR OCCURRED !! \n" );
		IOLog( "SMU_Neo2_SlewCtrlLoop::adjustSlewStepVoltControls FATAL ERROR OCCURRED !! \n" );
		_fatalErrorOccurred = true;
	}

	return( success );
}

/*******************************************************************************
 * Method:
 *	checkThermalFailsafes
 *
 * Purpose:
 *	Check for overtemp conditions and put system to sleep if a critical situation
 *	is detected. Namely, if the current temperature exceeds Tmax and the fans
 *	are at full for 30 seconds, we put the system to sleep.
 *	
 ******************************************************************************/
void SMU_Neo2_SlewCtrlLoop::checkThermalFailsafes( bool currentTempValid, SensorValue currentTemp )
{
	if ( !currentTempValid || ( currentTempValid && currentTemp.sensValue < _tmax.sensValue ) )
	{
		// clear internal overtemp flag, if set
		platformPlugin->setEnvArray( gIOPPluginEnvInternalOvertemp, this, false );
		
		// zero the critical sleep deadline
		AbsoluteTime_to_scalar( &_criticalDeadline ) = 0;

		return;
	}

	// set the internal overtemp flag
	gPlatformPlugin->setEnvArray( gIOPPluginEnvInternalOvertemp, this, true );

	// if the cpu fan control loop has it's fans at max, or there is no cpu control loop and
	// we are operating on the 2 sec deadline timer, start the 30 sec overtemp timer
	if (gPlatformPlugin->envArrayCondIsTrue( gIOPPluginEnvCtrlLoopOutputAtMax ) || _deadlineActive)
	{
		// get the current time
		AbsoluteTime now;
		clock_get_uptime(&now);

		// if we've just entered this condition, we'll have to calculate the sleep deadline
		if (AbsoluteTime_to_scalar( &_criticalDeadline ) == 0)
		{
			CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::checkThermalFailsafes system at max cooling, starting 30 sec timer...\n" );

			// store the value 30 seconds in the deadline
			clock_interval_to_absolutetime_interval( 30 /* seconds */, NSEC_PER_SEC, &_criticalDeadline );

			// add the current time to 30 seconds to get the deadline
			ADD_ABSOLUTETIME( &_criticalDeadline, &now );
		}
		else
		{
			// check if we've passed the deadline
			if ( CMP_ABSOLUTETIME( &now, &_criticalDeadline ) > 0 )
			{
				IOLog( "Thermal Manager: max temperature exceeded for 30 seconds, forcing system sleep\n" );
				CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::checkThermalFailsafes exceeded Tmax @ max cooling for 30s, sleeping system\n" );
	
				platformPlugin->coreDump();

				// send the sleep demand
				platformPlugin->sleepSystem();
			}
			else
			{
				CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::checkThermalFailsafes 30 sec hasn't yet elapsed\n" );
			}
		}
	}
}

/*******************************************************************************
 * Method:
 *	updateMetaState
 *
 * Purpose:
 *	Examine environmental conditions and choose SlewCtrlLoop's meta state.
 ******************************************************************************/
// virtual
bool SMU_Neo2_SlewCtrlLoop::updateMetaState( void )
	{
	const OSNumber*									newMetaState;
	bool											intOverTemp;
	bool											extOverTemp;

	// we are in metastate 0 by default.  If any of the following are true, we are in metastate 1:
	//    internal overtemp
	//    external overtemp

	intOverTemp = platformPlugin->envArrayCondIsTrue( gIOPPluginEnvInternalOvertemp );
	extOverTemp = platformPlugin->envArrayCondIsTrue( gIOPPluginEnvExternalOvertemp );

	if ( intOverTemp || extOverTemp )
		{
		newMetaState = gIOPPluginOne;
		}
	else
		{
		newMetaState = gIOPPluginZero;
		}

	if ( !newMetaState->isEqualTo( getMetaState() ) )
		{
		setMetaState( newMetaState );

		return( true );
		}

	return( false );
	}

#pragma mark -
#pragma mark ***     Helpers / Entry Points     ***
#pragma mark -

/*******************************************************************************
 * Method:
 *	sensorCurrentValueWasSet
 *
 * Purpose:
 *	This is a virtual method that's called whenever the fan control loop polls
 *	the temperature sensor (once/sec)
 ******************************************************************************/
// virtual
void SMU_Neo2_SlewCtrlLoop::sensorCurrentValueWasSet( IOPlatformSensor * aSensor, SensorValue newValue )
{
	// make sure the 2 second deadline is reset
	if ( AbsoluteTime_to_scalar( &deadline ) != 0 )
	{
		//CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::sensorCurrentValueWasSet setting 2 sec deadline\n" );
		clock_absolutetime_interval_to_deadline( _deadlineInterval, &deadline );
	}

	// apply the slewing algorithms
	adjustControls();
}

/*******************************************************************************
 * Method:
 *	willSleep
 *
 * Purpose:
 *	This is a virtual method that's called when the system is going down to sleep
 ******************************************************************************/
// virtual
void SMU_Neo2_SlewCtrlLoop::willSleep( void )
	{
	IOPlatformCtrlLoop::willSleep();

	// transition to full freq, /1 stepping before sleep
	do {

	if ( !_fatalErrorOccurred && _canStepProcessor && _stepIndex != 0 )
	{
		// raise voltage first
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::willSleep Pre-Set Voltage (?,%d) => (%d,%d)\n",
				_stepIndex, _slewPoints ? _numSlewPoints : 1, 0 );
		if ( !setProcessorVoltageIndex( _slewPoints ? _numSlewPoints : 1, 0 ) ) break;
	
		// then raise freq
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::willSleep Set Stepping (?,%d) => (%d,%d)\n",
				_stepIndex, _slewPoints ? _numSlewPoints : 1, 0 );
		if ( !setProcessorStepIndex( 0 ) ) break;
	}

	} while ( false );

	// clear the deadline
	AbsoluteTime_to_scalar( &_criticalDeadline ) = 0;
	AbsoluteTime_to_scalar( &deadline ) = 0;

	ctrlloopState = kIOPCtrlLoopWillSleep;
	}

/*******************************************************************************
 * Method:
 *	didWake
 *
 * Purpose:
 *	This is a virtual method that's called when the system is waking from sleep
 ******************************************************************************/
// virtual
void SMU_Neo2_SlewCtrlLoop::didWake( void )
	{
	IOPlatformCtrlLoop::didWake();

	// The processor wakes at /1 stepping
	_stepIndex = 0;

	// The system always wakes at SP#0, regardless of what slew point we were at
	// when the system went to sleep. Make sure everything stays in sync.
	if ( _canSlewProcessor )
	{
		// refresh the slew control's current value
		ControlValue slewPoint = _slewControl->forceAndFetchCurrentValue();
		if ( slewPoint != 0 )
		{
			CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::didWake Pulsar SlewPoint is not Zero: %d ?!?\n", slewPoint );
			slewPoint = 0;
		}
		_slewControl->setCurrentValue( slewPoint );

		// set _tmax to the
		_tmax.sensValue = _slewPoints[ _numSlewPoints ].Tmax.sensValue;

		OSNumber * tmax = OSDynamicCast( OSNumber, platformPlugin->getEnv( "cpu-tmax" ) );
		if (tmax) tmax->setValue( (UInt32) _tmax.sensValue );
	}

	// Since we come out of sleep in a determinate state, we can clear the fatal error flag
	// and attempt to adjust controls again.
	_fatalErrorOccurred = false;

	// if we have a temperature sensor, set the deadline
	if ( _cpu0TempSensor && ( _cpu0TempSensor->isRegistered() == kOSBooleanTrue ) )
	{
		CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::didWake setting 2 sec deadline\n" );
		clock_absolutetime_interval_to_deadline( _deadlineInterval, &deadline );
	}

	ctrlloopState = kIOPCtrlLoopDidWake;
	}

/*******************************************************************************
 * Method:
 *	deadlinePassed
 *
 * Purpose:
 *	During normal operation, this slew control loop piggybacks on the CPU fan
 *	control loop's temperature polling by using the sensorCurrentValueWasSet()
 *	mechanism. However, this won't work if the CPU fan control loop is not
 *	present, or is in output-override mode (no polling). To ensure that we will
 *	properly respond to overtemp conditions by slewing slow and/or sleeping,
 *	this slew control loop maintains a 2 sec deadline at all times.
 ******************************************************************************/
// virtual
void SMU_Neo2_SlewCtrlLoop::deadlinePassed( void )
{
	CTRLLOOP_DLOG( "SMU_Neo2_SlewCtrlLoop::deadlinePassed FORCING CPU TEMPERATURE READ!! \n" );

	// poll the temperature sensor. this will cause the sensorCurrentValueWasSet() method
	// to be called.
	if ( _cpu0TempSensor && ( _cpu0TempSensor->isRegistered() == kOSBooleanTrue ) )
	{
		_deadlineActive = true;
		_cpu0TempSensor->setCurrentValue( _cpu0TempSensor->forceAndFetchCurrentValue() );
		_deadlineActive = false;
	}
}

