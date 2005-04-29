/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOMessage.h>
#include <mach/clock_types.h>
#include "AppleFan.h"

/*
 * Default Parameters
 *
 * First look for defaults in the personality, otherwise we fall back to these
 * hard coded ones.
 *
 * Speed Table: Linear Ramp, Minimum 57C, Maximum 62C
 *
 * Note: These temperatures are expressed in 8.8 fixed point values.
 */
static fan_speed_table_t gDefaultSpeedTable =
	{ 0x3900, 0x3A4A, 0x3Ad3, 0x3B3C,
	  0x3B94, 0x3BE3, 0x3C29, 0x3C6A,
	  0x3CA6, 0x3CD7, 0x3D15, 0x3D48,
	  0x3D78, 0x3DA7, 0x3DD4, 0x3E00 };

/*
 * Hysteresis Temperature 55 C
 */
static SInt16 gDefaultHysteresisTemp = 0x3700;

/*
 * Polling Period (seconds)
 */
static UInt64 gDefaultPollingPeriod = 8;

/*
 * Speedup Delay (seconds)
 */
static UInt64 gDefaultSpeedupDelay = 8;

/*
 * Slowdown Delay (seconds)
 */
static UInt64 gDefaultSlowdownDelay = 48;


#define super IOService

OSDefineMetaClassAndStructors(AppleFan, IOService)

bool AppleFan::init(OSDictionary *dict)
{
	if (!super::init(dict)) return(false);

	I2C_iface = 0;
	cpu_thermo = 0;

	fI2CAddr = 0;

	timerCallout = NULL;

	fCurrentPowerState = kPowerOn;	// good guess, i suppose

	pollingPeriodKey = OSSymbol::withCString(kFanPollingPeriodKey);
	speedTableKey = OSSymbol::withCString(kFanSpeedTableKey);
	speedupDelayKey = OSSymbol::withCString(kSpeedupDelayKey);
	slowdownDelayKey = OSSymbol::withCString(kSlowdownDelayKey);
	hysteresisTempKey = OSSymbol::withCString(kHysteresisTempKey);
	getTempSymbol = OSSymbol::withCString(kGetTempSymbol);

#ifdef APPLEFAN_DEBUG
	currentSpeedKey = OSSymbol::withCString(kFanCurrentSpeedKey);
	currentCPUTempKey = OSSymbol::withCString(kCPUCurrentTempKey);
	forceUpdateKey = OSSymbol::withCString(kForceUpdateSymbol);
#endif

	AbsoluteTime_to_scalar(&fLastTransition) = 0;
	fLastFanSpeed = 0;
	fLastRmtTemp = 0;
	AbsoluteTime_to_scalar(&fWakeTime) = 0;

	return(true);
}

void AppleFan::free(void)
{
	if (pollingPeriodKey) pollingPeriodKey->release();
	if (speedTableKey) speedTableKey->release();
	if (speedupDelayKey) speedupDelayKey->release();
	if (slowdownDelayKey) slowdownDelayKey->release();
	if (hysteresisTempKey) hysteresisTempKey->release();
	if (getTempSymbol) getTempSymbol->release();

#ifdef APPLEFAN_DEBUG
	if (currentSpeedKey) currentSpeedKey->release();
	if (currentCPUTempKey) currentCPUTempKey->release();
	if (forceUpdateKey) forceUpdateKey->release();
#endif

	super::free();
}

IOService *AppleFan::probe(IOService *provider, SInt32 *score)
{
	OSData *tmp_osdata, *thermo;
	const char *compat;

	// Check compatible property and presence of platform-getTemp
	tmp_osdata = OSDynamicCast(OSData, provider->getProperty("compatible"));
	thermo = OSDynamicCast(OSData, provider->getProperty(kGetTempSymbol));
	if (tmp_osdata && thermo)
	{
		compat = (const char *)tmp_osdata->getBytesNoCopy();
		if (strcmp(compat, kADM1030Compatible) == 0)
		{
			*score = 10000;
			return(this);
		}
	}

	// I can't drive this fan controller
	*score = 0;
	return(0);
}

bool AppleFan::start(IOService *provider)
{
	OSData			*tmp_osdata;
	UInt32			*tmp_uint32;
	IOService		*tmp_svc;
	const OSSymbol	*uninI2C;
    mach_timespec_t WaitTimeOut;

	DLOG("+AppleFan::start\n");

	// We have two power states - off and on
	static const IOPMPowerState powerStates[2] = {
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

	// Use a 30 second timeout when calling waitForService()
	WaitTimeOut.tv_sec = 30;
	WaitTimeOut.tv_nsec = 0;

	if (!super::start(provider)) return(false);

	// get my I2C address from the device tree
	tmp_osdata = OSDynamicCast(OSData, provider->getProperty("reg"));
	if (!tmp_osdata)
	{
		IOLog("AppleFan::start failed to fetch provider's reg property!\n");
		DLOG("-AppleFan::start\n");
		return(false);
	}

	tmp_uint32 = (UInt32 *)tmp_osdata->getBytesNoCopy();
	fI2CBus = (UInt8)(*tmp_uint32 >> 8);
	fI2CAddr = (UInt8)*tmp_uint32;
	fI2CAddr >>= 1;		// right shift by one to make a 7-bit address

	DLOG("@AppleFan::start fI2CBus=%02x fI2CAddr=%02x\n",
			fI2CBus, fI2CAddr);

	// find the UniN I2C driver
	uninI2C = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-uni-n");
	tmp_svc = waitForService(resourceMatching(uninI2C), &WaitTimeOut);
	if (tmp_svc)
	{
		I2C_iface = (PPCI2CInterface *)tmp_svc->getProperty(uninI2C);
	}

	if (uninI2C) uninI2C->release();

	// I2C_iface is initialized to 0 so if it is not set here, we didn't find I2C
	if (!I2C_iface)
	{
		IOLog("AppleFan::start failed to find UniN I2C interface!\n");
		DLOG("-AppleFan::start\n");
		return(false);
	}

	DLOG("@AppleFan::start I2C_iface=%08x\n", (unsigned int)I2C_iface);

	// find the AppleCPUThermo driver
	cpu_thermo = waitForService( serviceMatching( "AppleCPUThermo" ), &WaitTimeOut );
	
	if ( cpu_thermo == NULL )
	{
		IOLog("AppleFan::start failed to find CPU thermistor driver!\n");
		DLOG("-AppleFan::start\n");
		return(false);
	}

	DLOG("@AppleFan::start cpu_thermo=%08x\n", (unsigned int)cpu_thermo);

	// the platform-getTemp property in our provider's property table holds
	// the phandle of the temp-monitor node.  Check to see if this matches
	// cpu_thermo's provider's phandle.
	tmp_osdata = OSDynamicCast(OSData, provider->getProperty(kGetTempSymbol));
	if (tmp_osdata)
	{
		tmp_uint32 = (UInt32 *)tmp_osdata->getBytesNoCopy();
		fThermoPHandle = *tmp_uint32;

		tmp_svc = cpu_thermo->getProvider();
		tmp_osdata = OSDynamicCast(OSData, tmp_svc->getProperty("AAPL,phandle"));
		tmp_uint32 = (UInt32 *)tmp_osdata->getBytesNoCopy();

		if (fThermoPHandle != *tmp_uint32)
		{
			IOLog("AppleFan::start AppleCPUThermo is attached to wrong thermo!!\n");
			DLOG("-AppleFan::start\n");
			return(false);
		}
	}

	// Set up operating parameters, checking for overrides in device tree
	if (!initParms(provider))
	{
		IOLog("AppleFan::start failed to initialize operating parameters!\n");
		return(false);
	}

	// Set up the timer
	timerCallout = thread_call_allocate( (thread_call_func_t) AppleFan::timerEventOccured,
			(thread_call_param_t) this );

	if (!timerCallout)
	{
		IOLog("IOPlatformPlugin::start failed to allocate thread callout\n");
		return(false);
	}


	// Program the chip's registers as the last operation that can fail
	if (!initHW(provider))
	{
		IOLog("AppleFan::start failed to initialize ADM1030!\n");
		// don't need to thread_call_cancel because we haven't scheduled the callout
		thread_call_free( timerCallout );
		return(false);
	}

	// register interest in power state changes
	DLOG("@AppleFan setting up PM notifications\n");

	PMinit();
	provider->joinPMtree(this);
	registerPowerDriver(this, (IOPMPowerState *)powerStates, 2);

	// Get restart and shutdown events too
    registerPrioritySleepWakeInterest(sPMNotify, this, NULL);

	// Register with I/O Kit matching engine
	registerService();

	// do the initial update.  this sets a timeout as its last step and
	// begins the fan control in motion.
	doUpdate(true);

	DLOG("-AppleFan::start\n");
	return(true);
}

void AppleFan::stop(IOService *provider)
{
	// driver is going away, so put the chip back in the state OF left it in
	restoreADM1030State(&fSavedRegs);

	if (timerCallout)
	{
		thread_call_cancel( timerCallout );
		thread_call_free( timerCallout );
		timerCallout = NULL;
	}

	super::stop(provider);
}

/*************************************************************************************
	Put the ADM1030 in filtered automatic control mode to prepare it for operation.
	
*************************************************************************************/
bool AppleFan::initHW(IOService *provider)
{
	UInt8 myByte;

	// Save initial state of the biznatch
	if (!saveADM1030State(&fSavedRegs))
	{
		IOLog("AppleFan::initHW unable to save ADM1030State!!\n");
		return(false);
	}

	// Open the I2C bus
	if (!doI2COpen())
	{
		IOLog("AppleFan::initHW failed to open I2C bus\n");
		DLOG("-AppleFan::initHW\n");
		return(false);
	}

	// Fan Filter Register - 0x23
	myByte = kFilterEnable							|	// Enable auto-mode filtering
			 (kSampleRateMask & kSampleRate1_4KHz)	|	// 1.4 KHz ADC sample rate
			 (kRampRateMask & kRampRate1)			|	// Ramp Rate factor 1
			 kSpinUpDisable;							// disable fan spin-up

	if (!doI2CWrite(kFanFilterReg, &myByte, 1))
	{
		doI2CClose();
		DLOG("-AppleFan::initHw failed to write fan filter reg\n");
		return(false);
	}

	// Configuration Register 2 - 0x01
	//DLOG("@AppleFan::initHW setting config register 2\n");
	myByte = kPWMOutputEnable;		// Drive the PWM output

	if (!doI2CWrite(kConfigReg2, &myByte, 1))
	{
		doI2CClose();
		DLOG("-AppleFan::initHW failed to write config reg 2\n");
		return(false);
	}

	// Configuration Register 1 - 0x00
	//DLOG("@AppleFan::initHW setting config register 1\n");
	myByte = kMonitorEnable							|	// Enable temp monitoring
	         kTACHModeSelect						|	// Select analog TACH
	         kFanFaultEnable						|	// Enable FAN_FAULT output
	         (kPWMModeSelectMask & kPWMModeRemote)	|	// Use remote temp
	         kAutoEnable;								// Use automatic control mode

	if (!doI2CWrite(kConfigReg1, &myByte, 1))
	{
		doI2CClose();
		DLOG("-AppleFan::initHW failed to write config reg 1\n");
		return(false);
	}

	doI2CClose();

	return(true);
}

/*************************************************************************************
	initParms() and parseDict() are responsible for setting the initial values for
		polling period
		hysteresis temperature
		speed lookup table
		speedup delay time
		slowdown delay time

	There should be defaults for these provided in the I/O Kit personality.  If those
	are not found, then we revert to the hardcoded defaults declared statically in
	this code.
	
	parseDict is also called from setProperties when we're in debug mode.
*************************************************************************************/
bool AppleFan::initParms(IOService *provider)
{
	int		i;
	OSObject *value;
	OSDictionary *personality;
	OSDictionary *defaults = OSDictionary::withCapacity(5);

	for (i=0; i<kNumFanSpeeds; i++)
		fSpeedTable[i] = gDefaultSpeedTable[i];
	fPollingPeriod = gDefaultPollingPeriod * NSEC_PER_SEC;
	fSpeedupDelay = gDefaultSpeedupDelay * NSEC_PER_SEC;
	fSlowdownDelay = gDefaultSlowdownDelay * NSEC_PER_SEC;
	fHysteresisTemp = gDefaultHysteresisTemp;

	personality = OSDynamicCast(OSDictionary, getProperty(kDefaultParamsKey));

	if (personality == NULL || defaults == NULL) return true;

	// Look for speed table
	if (value = personality->getObject(speedTableKey))
	{
		DLOG("@AppleFan::initParams using personality's speed table\n");
		defaults->setObject(speedTableKey, value);
	}

	// Set the polling period
	if (value = personality->getObject(pollingPeriodKey))
	{
		DLOG("@AppleFan::initParams using personality's polling period\n");
		defaults->setObject(pollingPeriodKey, value);
	}

	// Set up delays
	if (value = personality->getObject(speedupDelayKey))
	{
		DLOG("@AppleFan::initParams using personality's speedup delay\n");
		defaults->setObject(speedupDelayKey, value);
	}

	if (value = personality->getObject(slowdownDelayKey))
	{
		DLOG("@AppleFan::initParams using personality's slowdown delay\n");
		defaults->setObject(slowdownDelayKey, value);
	}

	if (value = personality->getObject(hysteresisTempKey))
	{
		DLOG("@AppleFan::initParams using personality's hysteresis temp\n");
		defaults->setObject(hysteresisTempKey, value);
	}

	parseDict(defaults);

	return(true);
}

void AppleFan::parseDict(OSDictionary *props)
{
	unsigned int count, index;
	SInt16 temperature;
	OSNumber *number;
	OSArray *speeds;

	if ((number = OSDynamicCast(OSNumber, props->getObject(pollingPeriodKey))) != 0)
	{
		fPollingPeriod = number->unsigned64BitValue();

		// Convert to nanoseconds
		fPollingPeriod *= NSEC_PER_SEC;
	}

	if ((number = OSDynamicCast(OSNumber, props->getObject(speedupDelayKey))) != 0)
	{
		fSpeedupDelay = number->unsigned64BitValue();

		// Convert to nanoseconds
		fSpeedupDelay *= NSEC_PER_SEC;
	}

	if ((number = OSDynamicCast(OSNumber, props->getObject(slowdownDelayKey))) != 0)
	{
		fSlowdownDelay = number->unsigned64BitValue();

		// Convert to nanoseconds
		fSlowdownDelay *= NSEC_PER_SEC;
	}

	if ((number = OSDynamicCast(OSNumber, props->getObject(hysteresisTempKey))) != 0)
	{
		fHysteresisTemp = (SInt16)number->unsigned16BitValue();
	}

	if ((speeds = OSDynamicCast(OSArray, props->getObject(speedTableKey))) != 0)
	{
		count = speeds->getCount();
		if (count == kNumFanSpeeds)
		{
			for (index=0; index<count; index++)
			{
				number = OSDynamicCast(OSNumber, speeds->getObject(index));
				if (number == NULL)
				{
					IOLog("AppleFan::setProperties SOMETHING IS VERY WRONG!!!");
					break;
				}
			
				temperature = (SInt16)number->unsigned16BitValue();
				fSpeedTable[index] = temperature;
			}
		}
	}
}

/*************************************************************************************
	doUpdate() is where we read the CPU temp and look it up in the speed table to
	choose a fan speed.  Also here is the timer callback routine which repeatedly
	calls doUpdate().
*************************************************************************************/

/* static */
void AppleFan::timerEventOccured( void *self )
{
	AppleFan * me = OSDynamicCast(AppleFan, (OSMetaClassBase *) self);

	if (me)	me->doUpdate(false);
}

void AppleFan::doUpdate(bool first)
{
	AbsoluteTime interval;
	UInt8 newSpeed;
	SInt16 cpu_temp;

	DLOG("+AppleFan::doUpdate\n");

	// Get temperature data
	if (!getCPUTemp(&cpu_temp))
	{
		IOLog("AppleFan::doUpdate ERROR FETCHING CPU TEMP!!!\n");
		restoreADM1030State(&fSavedRegs);
		terminate();
		return;
	}

	// look up the fan speed
	newSpeed = 0;
	while ((cpu_temp >= fSpeedTable[newSpeed]) && (newSpeed < (kNumFanSpeeds - 1)))
		newSpeed++;

	// set fan speed cfg register
	setFanSpeed(newSpeed, cpu_temp, first);

	// implement a periodic timer
	if (first) clock_get_uptime(&fWakeTime);

	nanoseconds_to_absolutetime(fPollingPeriod, &interval);
	ADD_ABSOLUTETIME(&fWakeTime, &interval);
	
	thread_call_enter_delayed( timerCallout, fWakeTime );

	DLOG("-AppleFan::doUpdate\n");
}

/*************************************************************************************
	Routines which take a fan speed as input and program the ADM1030 to run at the
	desired speed.  Some nasty tricks are in this code, but it is pretty well
	encapsulated and explained in comments...
	
	The routines are setFanSpeed and setADM1030SpeedMagically (lots of comments in
	the latter for obvious reasons...)
*************************************************************************************/
void AppleFan::setFanSpeed(UInt8 speed, SInt16 cpu_temp, bool first)
{
	UInt8 desiredSpeed;
	SInt16 rmt_temp;
	AbsoluteTime ticksPassed;
	UInt64 nsecPassed;

	if (!getRemoteTemp(&rmt_temp))
	{
		IOLog("AppleFan::setFanSpeed FATAL ERROR FETCHING REMOTE CHANNEL TEMP!!!\n");
		restoreADM1030State(&fSavedRegs);
		terminate();
		return;
	}

	if (first)
	{
		// If this is the first run, don't apply any of the hysteresis mechanisms,
		// just program the chip with the speed that was produced from the table
		// lookup
		DLOG("@AppleFan::setFanSpeed initial speed is %u\n", speed);
		setADM1030SpeedMagically(speed, rmt_temp);
		clock_get_uptime(&fLastTransition);
	}
	else
	{
		if (speed == fLastFanSpeed)
		{
			if (rmt_temp != fLastRmtTemp)
			{
				// need to update the remote temp limit register
				DLOG("@AppleFan::setFanSpeed environmental update\n");
				setADM1030SpeedMagically(speed, rmt_temp);
				return;
			}

			DLOG("@AppleFan::setFanSpeed no update needed\n");
		}
		else 
		{
			// calculate nanoseconds since last speed change
			clock_get_uptime(&ticksPassed);
			SUB_ABSOLUTETIME(&ticksPassed, &fLastTransition);
			absolutetime_to_nanoseconds(ticksPassed, &nsecPassed);
			
			if (speed < fLastFanSpeed)
			{
				// Hysteresis mechanism - don't turn off the fan unless we've reached
				// the hysteresis temp
				if (speed == kDutyCycleOff && fLastFanSpeed == kDutyCycle07)
				{
					DLOG("@AppleFan::setFanSpeed hysteresis check cpu_temp 0x%04x fHysteresisTemp %04x\n",
							cpu_temp, fHysteresisTemp);

					if (cpu_temp > fHysteresisTemp)
					{
						DLOG("@AppleFan::setFanSpeed hysteresis active\n");

						// do an environmental update if needed
						if (rmt_temp != fLastRmtTemp)
							setADM1030SpeedMagically(fLastFanSpeed, rmt_temp);

						return;
					}
				}

				DLOG("@AppleFan::setFanSpeed downward check nsecPassed 0x%llX fSlowdownDelay 0x%llX\n",
						nsecPassed, fSlowdownDelay);

				// apply downward delay
				if (nsecPassed > fSlowdownDelay)
				{
					desiredSpeed = fLastFanSpeed - 1;
					DLOG("@AppleFan::setFanSpeed slowdown to %u\n", desiredSpeed);
					setADM1030SpeedMagically(desiredSpeed, rmt_temp);
					clock_get_uptime(&fLastTransition);
				}
				else
				{
					DLOG("@AppleFan::setFanSpeed slowdown delay active\n");

					// do an environmental update if needed
					if (rmt_temp != fLastRmtTemp)
						setADM1030SpeedMagically(fLastFanSpeed, rmt_temp);
				}
			}
			else if (speed > fLastFanSpeed)
			{
				DLOG("@AppleFan::setFanSpeed upward check nsecPassed 0x%llX fSpeedupDelay 0x%llX\n",
						nsecPassed, fSpeedupDelay);

				// apply upward hysteresis
				if (nsecPassed > fSpeedupDelay)
				{
					desiredSpeed = fLastFanSpeed + 1;
					DLOG("@AppleFan::setFanSpeed speedup to %u\n", desiredSpeed);
					setADM1030SpeedMagically(desiredSpeed, rmt_temp);
					clock_get_uptime(&fLastTransition);
				}
				else
				{
					DLOG("@AppleFan::setFanSpeed speedup delay active\n");

					// do an environmental update if needed
					if (rmt_temp != fLastRmtTemp)
						setADM1030SpeedMagically(fLastFanSpeed, rmt_temp);
				}
			}
			else { /* not reached */ }
		}
	}
}

void AppleFan::setADM1030SpeedMagically(UInt8 desiredSpeed, SInt16 rmt_temp)
{
	UInt8 TminTrange, speed;

	// shift rmt_temp(14:10) into TminTrange(7:3)
	TminTrange = (UInt8)(rmt_temp >> 7);
	TminTrange &= ~kTrangeMask;	// clear out the 3 LSBs
	TminTrange |= 0x7;			// T_range = highest possible

	/*
	 * setFanSpeed() calculates a speed between 0x0 and 0xF and passes it
	 * into this routine (in the variable named "speed").  setFanSpeed
	 * is responsible for setting the remote T_min/T_range and the speed
	 * config register to make the PWM match the requested speed.
	 *
	 * If we want the PWM to be completely inactive, we have to set
	 * Tmin ABOVE the current remote temp.  We set the speed config
	 * register to zero in this case.
	 *
	 * For other PWM values, we program Tmin to a value just below the
	 * current remote temp.  This will instruct the ADM1030 to operate
	 * the PWM at a speed just above whatever speed is programmed into
	 * the speed config register (which, in automatic control mode,
	 * sets the minimum speed at which the fan runs when the current
	 * remote temp exceeds Tmin).  Then, we program the speed config
	 * register with speed - 1 ; that is, one less than the value that
	 * was passed in by doUpdate().  It may seem less than intuitive
	 * to program the speed config register with 0 when we want speed
	 * 1, just remember that the difference is made by Tmin -- is the
	 * fan operating below the linear range (PWM completely inactive),
	 * or just inside the linear range (PWM active)?
	 */

	// The first "if" clause handles two cases:
	//
	// 1.  The fan is already set below the linear range.  This is
	//     when speed=0 and fLastFanSpeed=0.  We program Tmin 8 degrees
	//     above rmt_temp, and preserve the speed config reg at 0.
	//
	// 2.  The fan is currently in the linear range, but we are about
	//     to shift below the linear range and shut off PWM entirely.
	//     This is denoted by speed=0 and fLastFanSpeed=1.
	if (desiredSpeed == 0)
	{
		// Transition from 1 to 0 takes fan out of linear range
		TminTrange += 0x10;		// raise Tmin above current rmt_temp
		speed = desiredSpeed;
		fLastFanSpeed = desiredSpeed;
	}
	// Put the hw control loop into the linear range
	else
	{
		TminTrange -= 0x08;
		speed = desiredSpeed - 1;	
		fLastFanSpeed = desiredSpeed;
	}

	// Recored the rmt_temp at the time of this update
	fLastRmtTemp = rmt_temp;

#ifdef APPLEFAN_DEBUG
	char debug[16];
	temp2str(rmt_temp, debug);
#endif

	DLOG("@AppleFan::setADM1030SpeedMagically speed=%u rmt_temp=%x (%sC) TminTrange=%x\n",
			speed, rmt_temp, debug, TminTrange);

	if (!doI2COpen())
	{
		IOLog("AppleFan failed to open bus for setting fan speed\n");
		return;
	}

	if (!doI2CWrite(kRmtTminTrange, &TminTrange, 1))
	{
		doI2CClose();
		IOLog("AppleFan failed to write to T_min/T_range register!\n");
		return;
	}		

	if (!doI2CWrite(kSpeedCfgReg, &speed, 1))
	{
		doI2CClose();
		IOLog("AppleFan failed to write to fan speed register\n");
		return;
	}

	doI2CClose();
}

/*************************************************************************************
	Read temperature registers -- cpu temp (via AppleCPUThermo) and remote channel
	
*************************************************************************************/

bool AppleFan::getRemoteTemp(SInt16 *rmt_temp)
{
	SInt16	scratch;
	UInt8	ext_res_reg, remote_reg;
	bool	failed;

	// sanity check
	if (rmt_temp == NULL)
	{
		IOLog("AppleFan::getRemoteTemp bad arguments\n");
		return(false);
	}

	// get local and remote temperatures from ADM1030
	if (!doI2COpen())
	{
		IOLog("AppleFan::getRemoteTemp cannot open I2C!!\n");
		return(false);
	}

	failed = false;
	if (!doI2CRead(kExtTempReg, &ext_res_reg, 1) ||
	    !doI2CRead(kRemoteTempReg, &remote_reg, 1))
	{
		IOLog("AppleFan::getRemoteTemp failed to read temperature reg!\n");
		failed = true;
	}

	doI2CClose();

	if (failed) return(false);

	scratch = (SInt16)(ext_res_reg & kRemoteExtMask);
	scratch <<= kRemoteExtShift;
	*rmt_temp = (SInt16)(remote_reg << 8);
	*rmt_temp |= scratch;

	return(true);
}

bool AppleFan::getCPUTemp(SInt16 *cpu_temp)
{
	if (!cpu_temp || !cpu_thermo)
	{
		IOLog("AppleFan::getCPUTemp bad params!!\n");
		return(false);
	}

	// Get CPU temp
	if (cpu_thermo->callPlatformFunction(getTempSymbol, false,
			(void *) cpu_temp, 0, 0, 0) != kIOReturnSuccess)
	{
		IOLog("AppleFan::getCPUTemp failed to retreive CPU temp!!\n");
		return(false);
	}

	return(true);
}

/*************************************************************************************
// 3056480:
// When the system is restarted, we have to put the ADM1030 in S/W
// control mode with PWM output set to 0% duty cycle.  This prevents
// any nasty spin-up noises when HWInit/OF re-program the chip into
// (filtered) automatic control mode.  This is a workaround for an
// apparent problem with the ADM1030.
*************************************************************************************/

void AppleFan::setRestartMode(void)
{
	UInt8	regval;

#ifdef APPLEFAN_DEBUG
	bool	success	= false;
#endif

	DLOG("+AppleFan::setRestartMode\n");

	if (!doI2COpen())
	{
		IOLog("AppleFan::setRestartMode failed to open bus\n");
		return;
	}

	do {
		// Restore sane values to TminTrange registers
		// Tmin = 32
		// Trange = 10
		regval = 0x41;
		if (!doI2CWrite(kLocTminTrange, &regval, 1)) break;
		if (!doI2CWrite(kRmtTminTrange, &regval, 1)) break;

		// Fan Characteristics gets power on defaults
		regval = kSpinUp2000MS		|
		         kPWMFreq31Hz		|
		         kSpeedRange1324;
		if (!doI2CWrite(kFanCharReg, &regval, 1)) break;

		// Set the PWM output to 7% duty cycle
		regval = kDutyCycleOff;
		if (!doI2CWrite(kSpeedCfgReg, &regval, 1)) break;

		// Disable fan spin-up, disable filter mode
		regval = kSampleRate1_4KHz	|
		         kRampRate1			|
		         kSpinUpDisable;
		if (!doI2CWrite(kFanFilterReg, &regval, 1)) break;

		// config reg 2 gets power-on defaults, except disable TACH input
		regval = kPWMOutputEnable	|
		         kLocTempINTEnable	|
		         kRmtTempINTEnable;
		if (!doI2CWrite(kConfigReg2, &regval, 1)) break;

		// config reg 1 - use s/w control mode, fan fault.  nothing else.
		regval = kFanFaultEnable;
		if (!doI2CWrite(kConfigReg1, &regval, 1)) break;

#ifdef APPLEFAN_DEBUG
		success = true;
#endif
	} while (false);

	doI2CClose();

	DLOG("-AppleFan::setRestartMode success = %s\n", success ? "TRUE" : "FALSE");
}

/*************************************************************************************
	Power management-related functions

*************************************************************************************/

// Program the ADM1030 in preparation for a power state change
IOReturn AppleFan::powerStateWillChangeTo(IOPMPowerFlags flags,
				unsigned long stateNumber, IOService *whatDevice)
{
	//DLOG("@AppleFan powerStateWillChangeTo %lu\n", stateNumber);
	return(IOPMAckImplied);
}

IOReturn AppleFan::setPowerState(unsigned long powerStateOrdinal,
				IOService *whatDevice)
{
	DLOG("@AppleFan setPowerState %lu\n", powerStateOrdinal);

	if (fCurrentPowerState == kPowerOff && powerStateOrdinal == kPowerOn)
	{
		doWake();
	}
	else if (fCurrentPowerState == kPowerOn && powerStateOrdinal == kPowerOff)
	{
		doSleep();
	}

	fCurrentPowerState = powerStateOrdinal;

	return(IOPMAckImplied);
}

IOReturn AppleFan::powerStateDidChangeTo(IOPMPowerFlags flags,
				unsigned long stateNumber, IOService *whatDevice)
{
	//DLOG("@AppleFan powerStateDidChangeTo %lu\n", stateNumber);
	return(IOPMAckImplied);
}

IOReturn AppleFan::sPMNotify(void *target, void *refCon,
		long unsigned int messageType, IOService *provider,
		void *messageArg, vm_size_t argSize)
{	
	AppleFan	*self;
	IOService	*svc_target;

	DLOG("+AppleFan::sPMNotify\n");

	svc_target = (IOService *)target;

    if (OSDynamicCast(AppleFan, svc_target) != 0)
    {
        self = (AppleFan *)target;
    }
	else
    {
		IOLog("AppleFan::sPMNotify invalid target\n");
		return(kIOReturnBadArgument);
	}

	switch (messageType)
	{
		case kIOMessageSystemWillRestart:
			DLOG("@AppleFan::sPMNotify kIOMessageSystemWillRestart\n");
			self->doRestart();
			break;

		case kIOMessageSystemWillPowerOff:
			DLOG("@AppleFan::sPMNotify kIOMessageSystemWillPowerOff\n");
		default:
			break;
	}

	return(kIOReturnSuccess);
}

// Transition from kPowerOn to kPowerOff
void AppleFan::doSleep(void)
{
	DLOG("+AppleFan::doSleep\n");

	// Cancel any outstanding timer events
	thread_call_cancel( timerCallout );

	// Set the fan to speed zero so it doesn't spin up unnecessarily coming out
	// of sleep.  Should be good enough to use the last remote temp rather than
	// doing extra I2C cycles here.
	if (fLastFanSpeed != kDutyCycleOff)
		setADM1030SpeedMagically( kDutyCycleOff, fLastRmtTemp );

	DLOG("-AppleFan::doSleep\n");
}

// Transition from kPowerOff to kPowerOn
void AppleFan::doWake(void)
{
	DLOG("+AppleFan::doWake\n");

	// Force an update, this will sync up Tmin with the current
	// remote temp reading and restart the timer
	doUpdate(true);

	DLOG("-AppleFan::doWake\n");
}

// Handle restart
void AppleFan::doRestart(void)
{
	DLOG("+AppleFan::doRestart\n");

	// Disable updates
	thread_call_cancel( timerCallout );

	setRestartMode();

	DLOG("-AppleFan::doRestart\n");
}

/*************************************************************************************
	Save and Restore ADM1030 register state
		saveADM1030State
		restoreADM1030State
		
	These functions save and restore only the registers that are modified by this
	driver, NOT THE ENTIRE REGISTER SET.

	restoreADM1030State's final write is to config reg 1 as per the ADM1030 data
	sheet.
*************************************************************************************/

bool AppleFan::saveADM1030State(adm1030_regs_t *regs)
{
	bool success = false;

	if (!doI2COpen())
	{
		IOLog("AppleFan::saveADM1030State failed to open bus!!\n");
		return false;
	}

	do
	{
		if (!doI2CRead(kConfigReg1, &regs->config1, 1)) break;
		if (!doI2CRead(kConfigReg2, &regs->config2, 1)) break;
		if (!doI2CRead(kFanCharReg, &regs->fan_char, 1)) break;
		if (!doI2CRead(kSpeedCfgReg, &regs->speed_cfg, 1)) break;
		if (!doI2CRead(kFanFilterReg, &regs->fan_filter, 1)) break;
		if (!doI2CRead(kLocTminTrange, &regs->loc_tmin_trange, 1)) break;
		if (!doI2CRead(kRmtTminTrange, &regs->rmt_tmin_trange, 1)) break;

		success = true;
	} while (false);

	doI2CClose();

	return success;
}

void AppleFan::restoreADM1030State(adm1030_regs_t *regs)
{
	bool success = false;

	if (!doI2COpen())
	{
		IOLog("AppleFan::restoreADM1030State failed to open bus!!\n");
		return;
	}

	do
	{
		if (!doI2CWrite(kRmtTminTrange, &regs->rmt_tmin_trange, 1)) break;
		if (!doI2CWrite(kLocTminTrange, &regs->loc_tmin_trange, 1)) break;
		if (!doI2CWrite(kFanFilterReg, &regs->fan_filter, 1)) break;
		if (!doI2CWrite(kSpeedCfgReg, &regs->speed_cfg, 1)) break;
		if (!doI2CWrite(kFanCharReg, &regs->fan_char, 1)) break;
		if (!doI2CWrite(kConfigReg2, &regs->config2, 1)) break;
		if (!doI2CWrite(kConfigReg1, &regs->config1, 1)) break;

		success = true;
	} while (false);

	doI2CClose();
}

/*************************************************************************************
	I2C Wrappers -- these functions use fI2CBus, fI2CAddr instance vars, so they
	cannot be called until after AppleFan::start initializes these vars.
*************************************************************************************/

bool AppleFan::doI2COpen(void)
{
	DLOG("@AppleFan::doI2COpen bus=%02x\n", fI2CBus);
	return(I2C_iface->openI2CBus(fI2CBus));
}

void AppleFan::doI2CClose(void)
{
	DLOG("@AppleFan::doI2CClose\n");
	I2C_iface->closeI2CBus();
}

bool AppleFan::doI2CRead(UInt8 sub, UInt8 *bytes, UInt16 len)
{
	UInt8 retries;

#ifdef APPLEFAN_DEBUG
	char	debugStr[128];
	sprintf(debugStr, "@AppleFan::doI2CRead addr=%02x sub=%02x bytes=%08x len=%04x",
			fI2CAddr, sub, (unsigned int)bytes, len);
#endif

	I2C_iface->setCombinedMode();

	retries = kNumRetries;

	while (!I2C_iface->readI2CBus(fI2CAddr, sub, bytes, len))
	{
		if (retries > 0)
		{
			IOLog("AppleFan::doI2CRead read failed, retrying...\n");
			retries--;
		}
		else
		{
			IOLog("AppleFan::doI2CRead cannot read from I2C!!\n");
			return(false);
		}
	}

	DLOG("%s (first byte %02x)\n", debugStr, bytes[0]);

	return(true);
}

bool AppleFan::doI2CWrite(UInt8 sub, UInt8 *bytes, UInt16 len)
{
	UInt8 retries;

	DLOG("@AppleFan::doI2CWrite addr=%02x sub=%02x bytes=%08x len=%04x (first byte %02x)\n",
			fI2CAddr, sub, (unsigned int)bytes, len, bytes[0]);

	I2C_iface->setStandardSubMode();

	retries = kNumRetries;

	while (!I2C_iface->writeI2CBus(fI2CAddr, sub, bytes, len))
	{
		if (retries > 0)
		{
			IOLog("AppleFan::doI2CWrite write failed, retrying...\n");
			retries--;
		}
		else
		{
			IOLog("AppleFan::doI2CWrite cannot write to I2C!!\n");
			return(false);
		}
	}

	return(true);
}

//###################################################################################
// Routines to publish internal variables to I/O Registry
// Only enabled for APPLEFAN_DEBUG builds
//

#ifdef APPLEFAN_DEBUG

// User-land clients can call into this to force an update of the I/O Registry or
// or set parameters at run-time.
IOReturn AppleFan::setProperties(OSObject *properties)
{
	OSDictionary *props = OSDynamicCast(OSDictionary, properties);
	if (props == NULL) return kIOReturnBadArgument;

	if (props->getObject(forceUpdateKey) != NULL)
	{
		// refresh the I/O registry
		publishSpeedTable();
		publishPollingPeriod();
		publishDelays();
		publishHysteresisTemp();
		publishCurrentSpeed();
		publishCurrentCPUTemp();
		return kIOReturnSuccess;
	}

	// The ground is about to move beneath us, so disable the timer while
	// we are changing things up
	thread_call_cancel( timerCallout );

	parseDict(props);

	// start up the timer, and pass first==true to force a write using the new
	// parameters
	doUpdate(true);

	return kIOReturnSuccess;
}

void AppleFan::publishSpeedTable(void)
{
	int i;
	SInt64 mylonglong;
	OSNumber *entries[kNumFanSpeeds];
	OSArray *entryArray;

	// encapsulate each array element into an OSData
	for (i=0; i<kNumFanSpeeds; i++)
	{
		mylonglong = (SInt64)fSpeedTable[i];
		entries[i] = OSNumber::withNumber(mylonglong, sizeof(SInt64) * 8);
	}

	// stash the OSData's into an OSArray
	entryArray = OSArray::withObjects((const OSObject **)entries, kNumFanSpeeds, 0);

	// release my reference on the OSData's
	for (i=0; i<kNumFanSpeeds; i++)
		entries[i]->release();

	// set the OSData as the property value
	setProperty(speedTableKey, OSDynamicCast(OSObject, entryArray));

	// release my reference to the array
	entryArray->release();
}

void AppleFan::publishPollingPeriod(void)
{
	OSNumber *period = OSNumber::withNumber(fPollingPeriod / NSEC_PER_SEC,
			sizeof(fPollingPeriod) * 8);

	if (period)
	{
		setProperty(pollingPeriodKey, period);
		period->release();
	}
}

void AppleFan::publishDelays(void)
{
	OSNumber *speedupDelay = OSNumber::withNumber(fSpeedupDelay / NSEC_PER_SEC,
			sizeof(fSpeedupDelay) * 8);
	OSNumber *slowdownDelay = OSNumber::withNumber(fSlowdownDelay / NSEC_PER_SEC,
			sizeof(fSlowdownDelay) * 8);

	if (speedupDelay)
	{
		setProperty(speedupDelayKey, speedupDelay);
		speedupDelay->release();
	}

	if (slowdownDelay)
	{
		setProperty(slowdownDelayKey, slowdownDelay);
		slowdownDelay->release();
	}
}

void AppleFan::publishHysteresisTemp(void)
{
	OSNumber *hysteresisTemp = OSNumber::withNumber(fHysteresisTemp, sizeof(fHysteresisTemp) * 8);

	if (hysteresisTemp)
	{
		setProperty(hysteresisTempKey, hysteresisTemp);
		hysteresisTemp->release();
	}
}

void AppleFan::publishCurrentSpeed(void)
{
	UInt64 mylonglong = (UInt64)fLastFanSpeed;
	OSNumber *curSpeed = OSNumber::withNumber(mylonglong, sizeof(UInt64) * 8);

	if (curSpeed)
	{
		setProperty(currentSpeedKey, curSpeed);
		curSpeed->release();
	}
}

void AppleFan::publishCurrentCPUTemp(void)
{
	OSNumber *cpuTempNum;
	SInt64 mylonglong;
	SInt16 cpu_temp;

	if (getCPUTemp(&cpu_temp))
	{
		mylonglong = (SInt64)cpu_temp;
		cpuTempNum = OSNumber::withNumber(mylonglong, sizeof(SInt64) * 8);

		if (cpuTempNum)
		{
			setProperty(currentCPUTempKey, cpuTempNum);
			cpuTempNum->release();
		}
	}
}
#endif
//
//###################################################################################
