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
 */

#include "Portable_PlatformMonitor.h"

static const OSSymbol 				*gIOPMonPowerStateKey;
static const OSSymbol 				*gIOPMonClamshellStateKey;
static const OSSymbol 				*gIOPMonCPUActionKey;
static const OSSymbol 				*gIOPMonGPUActionKey;
	
static const OSSymbol				*gIOPMonState0;
static const OSSymbol				*gIOPMonState1;
static const OSSymbol				*gIOPMonState2;
static const OSSymbol				*gIOPMonState3;
static const OSSymbol				*gIOPMonFull;
static const OSSymbol				*gIOPMonReduced;
static const OSSymbol				*gIOPMonSlow;
static const OSSymbol				*gIOClamshellStateOpen;
static const OSSymbol				*gIOClamshellStateClosed;

static	Portable_PlatformMonitor	*gIOPMon;

static 	IOService					*provider;

/*
 * conSensorArray, like platformActionArray is platform-dependent.  One element for each primary
 * controller/sensor and each must register itself with us before we can use it.
 *
 * conSensorArray is initialized in the start routine.
 */
static ConSensorInfo conSensorArray[kMaxConSensors];

/*
 * The subSensorArray contains information about secondary sensors and maps those sensors into the primary 
 * sensor.  There are kMaxSensorIndex subSensors and each must map to a primary sensor.  On this platform, 
 * all individual sensors are thermal so all map to kThermalSensor, although that may not always be the case.
 * If more than one thermal zone is to be managed then a primary thermal sensor would be created for each 
 * zone and the subSensorArray can be used to map each sensor into a particular zone.
 *
 * subSensorArray is initialized in the start routine.
 */
static ConSensorInfo subSensorArray[kMaxSensorIndex];


ThresholdInfo	thermalThresholdInfoArray[kMaxSensorIndex][kNumClamshellStates][kMaxThermalStates];

LimitsInfo			*thermalLimits = NULL;

IORecursiveLock		*setSpeedLock;

/*
 *	Current Threshold Arrays are WAY too big with very little useful data in them.  The LimitsInfo structure
 *	will pretty much hold all the data that 
 */

// PowerBook6,7 values ( Q72B / Q73B )
static LimitsInfo limits_Q72B[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			63,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - Pwr/Memory Bottomside
	{		1,			83,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Bottomside
	{		2,			92,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook6,8 values	( Q54B )
static LimitsInfo limits_Q54B[6] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			54,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - HDD Bottomside
	{		0,			59,				2, 			kThrottleCPU,	kClamshellClosedState,	},		// Sensor 0 - HDD Bottomside
	{		1,			79,				4, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Topside
	{		2,			103,			4, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			42,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook6,5 values ( Q72 / Q73 )
static LimitsInfo limits_Q72[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			63,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - Pwr/Memory Bottomside
	{		1,			83,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Bottomside
	{		2,			92,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook6,4 values	( Q54A )
static LimitsInfo limits_Q54A[6] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			54,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - HDD Bottomside
	{		0,			59,				2, 			kThrottleCPU,	kClamshellClosedState,	},		// Sensor 0 - HDD Bottomside
	{		1,			79,				4, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Topside
	{		2,			103,			4, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			42,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook5,4 values ( Q16A )
static LimitsInfo limits_Q16A[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			76,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - HDD Bottomside
	{		1,			75,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Topside
	{		2,			82,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			42,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook5,5 values ( Q41A )
static LimitsInfo limits_Q41A[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			68,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - Pwr/Memory Bottomside
	{		1,			68,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Bottomside
	{		2,			68,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			50,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook6,6 values ( Q79 )
static LimitsInfo limits_Q79[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			63,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - Pwr/Memory Bottomside
	{		1,			83,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Bottomside
	{		2,			92,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook6,1 values ( P99 )
static LimitsInfo limits_P99[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - GPU TOPSIDE
	{		1,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU BOTTOMSIDE
	{		2,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - CPU TOPSIDE
	{		4,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 4 - DIMM BOTTOMSIDE
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook6,2 values ( Q54 )
static LimitsInfo limits_Q54[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			53,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - HDD Bottomside
	{		1,			79,				4, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Topside
	{		2,			103,			4, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook6,3 values ( P72D )
static LimitsInfo limits_P72D[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			63,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - Pwr/Memory Bottomside
	{		1,			83,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Bottomside
	{		2,			92,				1, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook5,2 values ( Q16 )
static LimitsInfo limits_Q16[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			76,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - HDD Bottomside
	{		1,			75,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Topside
	{		2,			82,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			50,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook5,3 values ( Q41 )
static LimitsInfo limits_Q41[5] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			68,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - Pwr/Memory Bottomside
	{		1,			68,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - CPU Bottomside
	{		2,			68,				2, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - GPU on Die
	{		3,			50,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 3 - Battery
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

// PowerBook5,1 values ( P84 )
static LimitsInfo limits_P84[4] = 
{		//Sensor	//templimit		//hysteresis	//effect		// special state 
	{		0,			100,			5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 0 - GPU Bottomside
	{		1,			100,			5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 1 - Pwr Suppy Bottomside
	{		2,			73,				5, 			kThrottleCPU,	kNoSpecialState,		},		// Sensor 2 - CPU Bottomside
    {	   -1, 			 0,				0,			0,				0,						}		// No more sensors
};

#ifndef sub_iokit_graphics
#	define sub_iokit_graphics           err_sub(5)
#endif
#ifndef kIOFBLowPowerAggressiveness
#	define kIOFBLowPowerAggressiveness iokit_family_err(sub_iokit_graphics,1)
#endif

#define super IOPlatformMonitor
OSDefineMetaClassAndStructors(Portable_PlatformMonitor, IOPlatformMonitor)

// **********************************************************************************
// start
//
// **********************************************************************************
bool Portable_PlatformMonitor::start ( IOService * nub )
{
	UInt32 				i;
	IORegistryEntry		*cpuEntry, *powerPCEntry;
	OSIterator			*childIterator;
	OSData				*cpuSpeedData;
	UInt32 				newCPUSpeed, newNum;
    bool				needs2003Fixes;
        
	if (!initSymbols())
        return false;
		
	provider = nub;
    
    debug_msg("Portable_PlatformMonitor::start - starting\n");
    
    macRISC2PE = OSDynamicCast(MacRISC2PE, getPlatform());
    
    // Set up flag that tells us when the computer is about to go to sleep to false
    // ...this flag is used to tell when to ignore all setaggressiveness calls...
    goingToSleep = false;
    
    machineModel = macRISC2PE->pMonPlatformNumber;
    
    switch (machineModel)
    {
        case kPB51MachineModel:		thermalLimits = limits_P84;		break;

        case kPB61MachineModel:		thermalLimits = limits_P99;		break;

        case kPB52MachineModel:		thermalLimits = limits_Q16;		break;
        case kPB53MachineModel:		thermalLimits = limits_Q41;		break;
        case kPB62MachineModel:		thermalLimits = limits_Q54;		break;
        case kPB63MachineModel:		thermalLimits = limits_P72D;	break;

        case kPB64MachineModel:		thermalLimits = limits_Q54A;	break;
        case kPB65MachineModel:		thermalLimits = limits_Q72;		break;		// Also covers Q72A
        case kPB54MachineModel:		thermalLimits = limits_Q16A;	break;
        case kPB55MachineModel:		thermalLimits = limits_Q41A;	break;
        case kPB66MachineModel:		thermalLimits = limits_Q79;		break;
        
        case kPB67MachineModel:		thermalLimits = limits_Q72B;	break;
        case kPB68MachineModel:		thermalLimits = limits_Q54B;	break;

        case kPB56MachineModel:		thermalLimits = limits_Q16A;	break;		// For now, same values as Q16A
        case kPB57MachineModel:		thermalLimits = limits_Q41A;	break;		// For now, same values as Q41A

        default:	thermalLimits = NULL;
    }

    createThermalThresholdArray(thermalLimits);

    // This code will also be in the fix up code.  Which machines can get the needed power out
    // of a 65W or airline adapter, and which machines need to reduce speed when on battery?
    
    // Current machines supported by this Platform Montitor have the following requirements...
    
    //       Machine				 Needs to Reduce Without Battery			Can Utilize More then 45W			
    //       -------				 -------------------------------			-------------------------
    //	Powerbook5,6 ( Q16A )					YES										   YES
    //  Powerbook5,7 ( Q41A )					YES										   YES

    //	Powerbook5,4 ( Q16A )					YES										   YES
    //  Powerbook5,5 ( Q41A )					YES										   YES
    //  Powerbook6,4 ( Q54A )					YES										    NO
    //  Powerbook6,5 ( Q72/A )					 NO										    NO

    //  Powerbook6,8 ( Q54B )					 NO										    NO
    //  Powerbook6,7 ( Q72B )					 NO										    NO

    //	Powerbook5,2 ( Q16  )					YES										   YES
    //  Powerbook5,3 ( Q41  )					YES										   YES
    //  Powerbook6,2 ( Q54  ) 					YES										    NO
    //  Powerbook6,3 ( P72D )					 NO										    NO

    //  Powerbook5,1 ( P84  )					YES										   YES

    //  Powerbook6,1 ( P99 )					YES										    NO
    
    machineUtilizes65W = (( machineModel == kPB52MachineModel ) || ( machineModel == kPB53MachineModel ) || 			// Q16, Q41
                          ( machineModel == kPB54MachineModel ) || ( machineModel == kPB55MachineModel ) ||				// Q41A, Q16A
                          ( machineModel == kPB56MachineModel ) || ( machineModel == kPB57MachineModel ));				// Q41A, Q16A
    
	machineReducesOnNoBattery = (( machineModel == kPB51MachineModel ) || ( machineModel == kPB52MachineModel ) || 		// P84, Q16
                                 ( machineModel == kPB53MachineModel ) || ( machineModel == kPB54MachineModel ) ||		// Q41, Q16A
                                 ( machineModel == kPB55MachineModel ) || ( machineModel == kPB56MachineModel ) ||		// Q41, Q16B
                                 ( machineModel == kPB57MachineModel ) || ( machineModel == kPB61MachineModel ) ||		// Q41B, P99
                                 ( machineModel == kPB62MachineModel ) || ( machineModel == kPB64MachineModel ));		// Q54, Q54A

    needs2003Fixes = (( machineModel == kPB52MachineModel ) || ( machineModel == kPB53MachineModel ) || 				// Q16, Q41
                      ( machineModel == kPB62MachineModel ) || ( machineModel == kPB63MachineModel ));					// Q54, P72D

    // The initial slewing portables lack the 'has-bus-slewing' property in the
    // device tree, so it was hard coded into initial Portable2003 implementation.
    if (needs2003Fixes)
        macRISC2PE->processorSpeedChangeFlags |= kBusSlewBasedSpeedChange;
    
    useBusSlewing = ((macRISC2PE->processorSpeedChangeFlags & kBusSlewBasedSpeedChange) != 0);
    
    // platform expert assumes that dynamic power stepping means that it's a processor-based
    // speed change, but here it's not, so fix flags
    if (useBusSlewing)
        macRISC2PE->processorSpeedChangeFlags &= ~kProcessorBasedSpeedChange;
        
    if (machineModel == kPB51MachineModel) {
        initialPowerState = kPowerState0;		// Processor boots at fast speed
        initialCPUPowerState = kCPUPowerState0;	
    }
    else {
        initialPowerState = kPowerState1;		// Processor boots at slow speed
        initialCPUPowerState = kCPUPowerState1;	
    }
      
    if (machineModel == kPB61MachineModel)
        wakingPowerState = kPowerState1;	// Only one machine, the processor is slow comming out of sleep
    else
        wakingPowerState = kPowerState0;	// Processor is running fast comming out of sleep

    whichCPUSpeedController = ( useBusSlewing ? kSlewController : kCPUController );

	initPlatformState ();					// Must be called after we know all about our platform!
                                            // ( or perhaps move all approprate code there )

    commandGateCaller = &iopmonCommandGateCaller;	// Inform superclass about our caller

    // use the "max-clock-frequency" to determine what the CPU speed should be if its greater 
    // than what OF reported to us in the "clock-frequency" property
    cpuEntry = fromPath("/cpus", gIODTPlane);
    if (cpuEntry != 0) {
        if ((childIterator = cpuEntry->getChildIterator (gIODTPlane)) != NULL) {
            while ((powerPCEntry = (IORegistryEntry *)(childIterator->getNextObject ())) != NULL) {
                if (!strncmp ("PowerPC", powerPCEntry->getName(gIODTPlane), strlen ("PowerPC"))) {
                    cpuSpeedData = OSDynamicCast( OSData, powerPCEntry->getProperty( "max-clock-frequency" ));
                    if (cpuSpeedData) {
                        newCPUSpeed = *(UInt32 *) cpuSpeedData->getBytesNoCopy();
                        if (newCPUSpeed != gPEClockFrequencyInfo.cpu_clock_rate_hz)
                        {
                            //  IOLog("Portable_PlatformMonitor::start - use max-clock-frequency to set new CPU speed\n");
                            newNum = newCPUSpeed / (gPEClockFrequencyInfo.cpu_clock_rate_hz /
                                                    gPEClockFrequencyInfo.bus_to_cpu_rate_num);
                            gPEClockFrequencyInfo.bus_to_cpu_rate_num = newNum;			// Set new numerator
                            gPEClockFrequencyInfo.cpu_clock_rate_hz = newCPUSpeed;		// Set new speed
                        }
                    }
                    break;
                }
            }
        }
    }

	// Initialize our controller/sensor types (platform-dependent)
	// Primary sensors
	conSensorArray[kPowerSensor].conSensorType = kIOPMonPowerSensor;			// platform power monitor
	conSensorArray[kPowerSensor].conSensor = this;								// platform power monitor
	conSensorArray[kPowerSensor].numStates = kMaxPowerStates;
	conSensorArray[kPowerSensor].sensorValid = true;
	conSensorArray[kPowerSensor].registered = true;								// built-in
	
	conSensorArray[kThermalSensor].conSensorType = kIOPMonThermalSensor;		// primary thermal sensor
	conSensorArray[kThermalSensor].conSensor = this;
	conSensorArray[kThermalSensor].numStates = kMaxThermalStates;
	conSensorArray[kThermalSensor].sensorValid = true;
	conSensorArray[kThermalSensor].registered = true;							// built-in aggregate sensor
	
	conSensorArray[kClamshellSensor].conSensorType = kIOPMonClamshellSensor;	// pmu clamshell sensor
	conSensorArray[kClamshellSensor].numStates = kNumClamshellStates;
	conSensorArray[kClamshellSensor].sensorValid = true;
	conSensorArray[kClamshellSensor].registered = false;

	// Controllers
	conSensorArray[kCPUController].conSensorType = kIOPMonCPUController;		// cpu controller
	conSensorArray[kCPUController].registered = false;

	conSensorArray[kGPUController].conSensorType = kIOPMonGPUController;		// gpu controller
	conSensorArray[kGPUController].state = kGPUPowerState0;
	conSensorArray[kGPUController].registered = true;							// built-in

	conSensorArray[kSlewController].conSensorType = kIOPMonSlewController;		// slew controller
	conSensorArray[kSlewController].registered = false;
        
	// Subsensors (all are thermal sensors)
	for (i = 0; i < kMaxSensorIndex; i++) {
		subSensorArray[i].conSensorType = kIOPMonThermalSensor;
		subSensorArray[i].conSensorParent = kThermalSensor;						// Index into primary array of our parent
		subSensorArray[i].numStates = kMaxThermalStates;
		// If a sensor is not to be used, set sensorValid false;
		subSensorArray[i].sensorValid = true;
		subSensorArray[i].registered = false;
	}
	
	if (!(dictPowerLow = OSDictionary::withCapacity (3)))			return false;
	if (!(dictPowerHigh = OSDictionary::withCapacity (3)))			return false;
	if (!(dictClamshellOpen = OSDictionary::withCapacity (2)))		return false;
	if (!(dictClamshellClosed = OSDictionary::withCapacity (2)))	return false;
	
	// On platforms with more than one CPU these dictionaries must accurately reflect which cpu is involved
	dictPowerLow->setObject (gIOPMonCPUIDKey, OSNumber::withNumber ((unsigned long long)0, 32));
	dictPowerLow->setObject (gIOPMonCurrentValueKey, OSNumber::withNumber ((unsigned long long)kPowerState1, 32));
	dictPowerLow->setObject (gIOPMonTypeKey, gIOPMonTypePowerSens);
	
	dictPowerHigh->setObject (gIOPMonCPUIDKey, OSNumber::withNumber ((unsigned long long)0, 32));
	dictPowerHigh->setObject (gIOPMonCurrentValueKey, OSNumber::withNumber ((unsigned long long)kPowerState0, 32));
	dictPowerHigh->setObject (gIOPMonTypeKey, gIOPMonTypePowerSens);
		
	dictClamshellOpen->setObject (gIOPMonCurrentValueKey, OSNumber::withNumber ((unsigned long long)kClamshellStateOpen, 32));
	dictClamshellOpen->setObject (gIOPMonTypeKey, gIOPMonTypeClamshellSens);
		
	dictClamshellClosed->setObject (gIOPMonCurrentValueKey, OSNumber::withNumber ((unsigned long long)kClamshellStateClosed, 32));
	dictClamshellClosed->setObject (gIOPMonTypeKey, gIOPMonTypeClamshellSens);
	
	if (!gIOPMonPowerStateKey)		gIOPMonPowerStateKey = OSSymbol::withCString (kIOPMonPowerStateKey);
	if (!gIOPMonClamshellStateKey)	gIOPMonClamshellStateKey = OSSymbol::withCString (kIOPMonClamshellStateKey);

	if (!gIOPMonCPUActionKey)		gIOPMonCPUActionKey = OSSymbol::withCString (kIOPMonCPUActionKey);
	if (!gIOPMonGPUActionKey)		gIOPMonGPUActionKey = OSSymbol::withCString (kIOPMonGPUActionKey);

	if (!gIOPMonState0)				gIOPMonState0 = OSSymbol::withCString (kIOPMonState0);
	if (!gIOPMonState1)				gIOPMonState1 = OSSymbol::withCString (kIOPMonState1);
	if (!gIOPMonState2)				gIOPMonState2 = OSSymbol::withCString (kIOPMonState2);
	if (!gIOPMonState3)				gIOPMonState3 = OSSymbol::withCString (kIOPMonState3);

	if (!gIOClamshellStateOpen)		gIOClamshellStateOpen = OSSymbol::withCString (kIOClamshellStateOpen);
	if (!gIOClamshellStateClosed)	gIOClamshellStateClosed = OSSymbol::withCString (kIOClamshellStateClosed);

	if (!gIOPMonFull)				gIOPMonFull	= OSSymbol::withCString (kIOPMonFull);
	if (!gIOPMonReduced)			gIOPMonReduced = OSSymbol::withCString (kIOPMonReduced);
	if (!gIOPMonSlow)				gIOPMonSlow = OSSymbol::withCString (kIOPMonSlow);
    
	lastPowerState = kMaxPowerStates;
	lastClamshellState = kNumClamshellStates;
	
    setSpeedLock = IORecursiveLockAlloc();
	
	// Put initial state and action info into the IORegistry
	updateIOPMonStateInfo(kIOPMonPowerSensor, currentPowerState);
	updateIOPMonStateInfo(kIOPMonClamshellSensor, currentClamshellState);
	provider->setProperty (gIOPMonCPUActionKey, (OSObject *)gIOPMonReduced);
	provider->setProperty (gIOPMonGPUActionKey, (OSObject *)gIOPMonFull);
        
	// Let the world know we're open for business
	publishResource ("IOPlatformMonitor", this);

	if (super::start (nub))
    {
		// pmRootDomain (found by our parent) is the recipient of GPU controller messages
		conSensorArray[kGPUController].conSensor = pmRootDomain;
                gIOPMon = this;
		return true;
	}
    else
		return false;
}

// **********************************************************************************
// createThermalThresholdArray
//
// **********************************************************************************
void Portable_PlatformMonitor::createThermalThresholdArray(LimitsInfo	*curThermalLimits)
{
    LimitsInfo			*workLimits;
    UInt32				whichClamshell;
    ThresholdInfo		*workThreshold;
	    
    workLimits = curThermalLimits;
        
    while ( workLimits->sensorID >= 0 )
    {
        whichClamshell = ((( workLimits->machineState && kClamshellClosedState ) != 0 ) ? 1 : 0 );
    
        workThreshold = &thermalThresholdInfoArray[workLimits->sensorID][whichClamshell][0];
        
        workThreshold->thresholdLow = TEMP_SENSOR_FMT(0);
        workThreshold->thresholdHigh = TEMP_SENSOR_FMT(workLimits->sensorThreshold);
        workThreshold->thresholdAction = 0;
        
        workThreshold++;
        workThreshold->thresholdLow = TEMP_SENSOR_FMT(workLimits->sensorThreshold - workLimits->sensorHysteresis);
        workThreshold->thresholdHigh = TEMP_SENSOR_FMT(127);
        workThreshold->thresholdAction = workLimits->sensorEffect;
       
        // While I so dislike this, populate the closed clamshell state as well for now
        // ( any clamshell specific numbers better be second in table )
        
        if ( whichClamshell == 0 )
        {
            workThreshold = &thermalThresholdInfoArray[workLimits->sensorID][1][0];
            
            workThreshold->thresholdLow = TEMP_SENSOR_FMT(0);
            workThreshold->thresholdHigh = TEMP_SENSOR_FMT(workLimits->sensorThreshold);
            workThreshold->thresholdAction = 0;
            
            workThreshold++;
            workThreshold->thresholdLow = TEMP_SENSOR_FMT(workLimits->sensorThreshold - workLimits->sensorHysteresis);
            workThreshold->thresholdHigh = TEMP_SENSOR_FMT(127);
            workThreshold->thresholdAction = workLimits->sensorEffect;
        } 
            
        workLimits++;
    }
}

// **********************************************************************************
// powerStateWillChangeTo
//
// **********************************************************************************
IOReturn Portable_PlatformMonitor::powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*)
{	
    if ( ! (theFlags & IOPMPowerOn) )			// Machine is going to sleep
    {
        IORecursiveLockLock(setSpeedLock);
                
        setCPUSpeed(true);						// Before going to sleep, 2003 and on portables must step/slew high
        goingToSleep = true;			
        
        IORecursiveLockUnlock(setSpeedLock);
    }
    
    return IOPMAckImplied;
}

// **********************************************************************************
// powerStateDidChangeTo
//
// **********************************************************************************
IOReturn Portable_PlatformMonitor::powerStateDidChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*)
{	
    if (theFlags & IOPMPowerOn)			// Machine is waking from sleep
    {
        goingToSleep = false;
		debug_msg ("Portable_PlatformMonitor::powerStateDidChangeTo - wake\n");
        
        conSensorArray[whichCPUSpeedController].state = wakingPowerState;
        
        // we don't want to remember any prior actions and start "fresh" after a wake from sleep
		restorePlatformState();
    }
     
    return IOPMAckImplied;
}

// **********************************************************************************
// setAggressiveness
//
// setAggressiveness is called by the Power Manager to change our power state
// It is most commonly called when the user changes the Energy Saver preferences
// to change the power state but it can be called for other reasons.
//
// One of these reasons is a forced-reduced-speed condition but we now detect and
// act on this condition immediately (through a clamshell event) so by the time the
// PM calls us here we have already altered the condition and there is nothing to do.
//
// **********************************************************************************
IOReturn Portable_PlatformMonitor::setAggressiveness(unsigned long selector, unsigned long newLevel)
{
	IOPMonEventData 	event;
	IOReturn			result;
        	    
    if (goingToSleep) 
        return kIOReturnError;

    result = super::setAggressiveness(selector, newLevel);

    newLevel &= 0x7FFFFFFF;		// mask off high bit... upcoming kernel change will use the high bit to indicate whether setAggressiveness call
                                // was user induced (Energy Saver) or not.  Currently not using this info so mask it off.
	if (selector == kPMSetProcessorSpeed)
    {
        if ((newLevel != currentPowerState) && (newLevel < 2))			// This only works if we have two power states
        {
            event.event = kIOPMonMessageStateChanged;					// create and transmit internal event
            event.conSensor = this;
            event.eventDict = (newLevel == 0) ? dictPowerHigh : dictPowerLow;
            
            handleEvent (&event);											// send it
        }
	}
	
	return result;
}

// **********************************************************************************
// 	 setCPUSpeed
// **********************************************************************************
bool Portable_PlatformMonitor::setCPUSpeed( bool setPowerHigh )
{
	IOService 			*serv;
    UInt32				newSpeed;
    UInt32				goSlowOffset = 0;
    OSObject			*newProperty;
    
    IORecursiveLockLock(setSpeedLock);
    
    if (goingToSleep)
    {
        IORecursiveLockUnlock(setSpeedLock);		// Leaving early, be sure to free the key
        return false;								// CPU speed can not change while a sleep is occuring
    }
        
    if ( setPowerHigh )
    {
        newSpeed = kCPUPowerState0;
        newProperty = (OSObject *)gIOPMonFull;
    }
    else
    {
        newSpeed = kCPUPowerState1;
        goSlowOffset = 1;
        newProperty = (OSObject *)gIOPMonReduced;
    }
    
    if (!conSensorArray[whichCPUSpeedController].registered)
    {
        IORecursiveLockUnlock(setSpeedLock);		// Leaving early, be sure to free the key
    	return false;
	}
    
    if ((conSensorArray[whichCPUSpeedController].state != newSpeed) && (serv = conSensorArray[whichCPUSpeedController].conSensor))
    {	
        if ( whichCPUSpeedController == kSlewController )
            slewBusSpeed ((UInt32) 0 + goSlowOffset);							// 0 = fast, 1 = slow
        else
            serv->setAggressiveness (kPMSetProcessorSpeed, 2 + goSlowOffset);	// 2 = fast, 3 = slow

        conSensorArray[whichCPUSpeedController].state = newSpeed;
        
        provider->setProperty (gIOPMonCPUActionKey, newProperty);
    }

    IORecursiveLockUnlock(setSpeedLock);

	return true;
}

// **********************************************************************************
// slewBusSpeed
//
// **********************************************************************************
void Portable_PlatformMonitor::slewBusSpeed (UInt32 newLevel)
{
    OSDictionary 		*dict;
    const OSObject		*target_value[1];
    const OSSymbol		*key[1];
    
    key[0] = OSSymbol::withCString(kIOPMonSlewActionKey);
    
    // we should have a slewcontroller, but let's check to make sure.
    if (conSensorArray[kSlewController].registered == true)
    {
        if (newLevel == 0)
        { 	// set bus speed high
            target_value[0] = OSNumber::withNumber((unsigned long long) 0, 32);
            dict = OSDictionary::withObjects(target_value, key, (unsigned int) 1, (unsigned int) 0);
        }
        else
        {
            target_value[0] = OSNumber::withNumber((unsigned long long) 1, 32);
            dict = OSDictionary::withObjects(target_value, key, 1, 0);
        }
        conSensorArray[kSlewController].conSensor->setProperties(dict);
    }
    
    key[0]->release();
    target_value[0]->release();
    dict->release();
}

// **********************************************************************************
// setGPUSpeed
//
// **********************************************************************************
bool Portable_PlatformMonitor::setGPUSpeed( UInt32 newGPUSpeed )
{
	IOService 			*serv;
    OSObject			*newProperty;
    UInt32				newAggressiveness;
    
    return true;		// Nothing uses this yet
    
    switch (newGPUSpeed)
    {
        case kGPUPowerState0:	newProperty = (OSObject *)gIOPMonFull;		newAggressiveness = 0;		break;
        case kGPUPowerState1:	newProperty = (OSObject *)gIOPMonReduced;	newAggressiveness = 1;		break;
        case kGPUPowerState2:	newProperty = (OSObject *)gIOPMonSlow;		newAggressiveness = 2;		break;
        
        default:	return false;	// It has to be one of these three states
    }

    if (!conSensorArray[kGPUController].registered) return false;
    
	// GPU at new power if not already there
    if ((conSensorArray[kGPUController].state != newGPUSpeed) && (serv = conSensorArray[kGPUController].conSensor))
    {
		conSensorArray[kGPUController].state = newGPUSpeed;
		serv->setAggressiveness (kIOFBLowPowerAggressiveness, newAggressiveness);
		provider->setProperty (gIOPMonGPUActionKey, (OSObject *)newProperty);
	}
    
    return true;
}

// **********************************************************************************
// applyPowerSettings
//
// **********************************************************************************
void Portable_PlatformMonitor::applyPowerSettings(UInt32  actionToTake)
{
    bool		cpuFullSpeed;
    bool		gpuFullSpeed;
    
    if (currentPowerState == kPowerState0)
        cpuFullSpeed = ((actionToTake && kThrottleCPU) == 0);
    else
        cpuFullSpeed = false;
    setCPUSpeed(cpuFullSpeed);
    
    gpuFullSpeed = ((actionToTake && kThrottleCPU) == 0);
    setGPUSpeed(gpuFullSpeed);
}

// **********************************************************************************
// setSensorThermalThresholds
//
// **********************************************************************************
void Portable_PlatformMonitor::setSensorThermalThresholds(UInt32 sensorID, ThermalValue lowTemp, ThermalValue highTemp)
{
	OSNumber	*threshLow, *threshHigh;

    // Set low thresholds - this will cause sensor to update info
    threshLow = (OSNumber *)subSensorArray[sensorID].threshDict->getObject (gIOPMonLowThresholdKey);
    threshLow->setValue((long long)lowTemp);
    threshHigh = (OSNumber *)subSensorArray[sensorID].threshDict->getObject (gIOPMonHighThresholdKey);
    threshHigh->setValue((long long)highTemp);
    // Send thresholds to sensor
    subSensorArray[sensorID].conSensor->setProperties (subSensorArray[sensorID].threshDict);
}

// **********************************************************************************
// resetSensorThermalThresholds
//
// **********************************************************************************
void Portable_PlatformMonitor::resetThermalSensorThresholds(void)
{
	UInt32				subsi;
    OSNumber			*num;
    ConSensorInfo		*csInfo;
    UInt32				value;
    UInt32				myThermalState;

    for (subsi = 0; subsi < kMaxSensorIndex; subsi++)
    {
        csInfo = &subSensorArray[subsi];
        
        if (csInfo->registered)
        {
            if (num = OSDynamicCast (OSNumber, csInfo->dict->getObject(gIOPMonCurrentValueKey)))
            {
                value = num->unsigned32BitValue();
                csInfo->state = kThermalState0;											// Favor low state
                
                myThermalState = lookupThermalStateFromValue (subsi, value);
                subSensorArray[subsi].state = myThermalState;

                setSensorThermalThresholds(subsi, thermalThresholdInfoArray[subsi][currentClamshellState][myThermalState].thresholdLow, 
                                                thermalThresholdInfoArray[subsi][currentClamshellState][myThermalState].thresholdHigh);
            }
        }
    }
}

// **********************************************************************************
// monitorPower
//
// **********************************************************************************
IOReturn Portable_PlatformMonitor::monitorPower (OSDictionary *dict, IOService *provider)
{
	UInt32 				type, index, subIndex, value;
	OSNumber			*num;
	IOPMonEventData 	event;
    bool				clamshellChanged;
    UInt32				adapterWatts;
    bool				mayNeedToRunSlow = false;
	    
	if (lookupConSensorInfo (dict, provider, &type, &index, &subIndex))
    {
		// See if any low power conditions exist.
		if (num = OSDynamicCast (OSNumber, dict->getObject(gIOPMonCurrentValueKey)))
        {
			value = num->unsigned32BitValue();
			value &= ~kIOPMForceLowSpeed;  		// Clear low speed bit
                      
            // For Q16A and Q41A with a >= 65W adapter plugged in OR airline adapter,
            // don't enforce force-reduced-speed conditions.
            
            if ( machineReducesOnNoBattery )
            {
               if ( machineUtilizes65W )
                {
                    adapterWatts = (value & 0xFF000000) >> 24;				// High byte may contain wattage of the adapter
                
                    if ( adapterWatts == 0 )
                        // Is a charger connected which has no charge capability?
                        mayNeedToRunSlow = !((value & (kIOPMACInstalled | kIOPMACnoChargeCapability)) == (kIOPMACInstalled | kIOPMACnoChargeCapability));
                    else
                        // Is the adapter less then 65 Watts
                        if ( adapterWatts < 0x41 ) mayNeedToRunSlow = true;		
                }
                else
                    mayNeedToRunSlow = true;
                
                if (mayNeedToRunSlow)
                {
                    if ((value & (kIOPMACInstalled | kIOPMACnoChargeCapability)) == (kIOPMACInstalled | kIOPMACnoChargeCapability))
                            value |= kIOPMForceLowSpeed;
                    else if ((value & (kIOPMRawLowBattery | kIOPMBatteryDepleted)) != 0)
                            value |= kIOPMForceLowSpeed;
                    else if ((value & kIOPMBatteryInstalled) == 0)
                            value |= kIOPMForceLowSpeed;
                }
            }

            num->setValue((long long)value);		// Send updated value back

            // Its AppleMacRISC2PE's job to monitor the clamshell state and send off an event if it changes.

            if ((value & kIOPMClosedClamshell) != 0)
                clamshellChanged = (currentClamshellState == kClamshellStateOpen);
            else
                clamshellChanged = (currentClamshellState != kClamshellStateOpen);
            
            if (clamshellChanged)
            {
                event.event = kIOPMonMessageStateChanged;
                event.conSensor = this;
                event.eventDict = (currentClamshellState == kClamshellStateOpen) ? dictClamshellClosed : dictClamshellOpen;

                // send it
                handleEvent (&event);

                return kIOReturnSuccess;
            }
		}
	}
    
	return kIOReturnBadArgument;
}

// **********************************************************************************
// updateIOPMonStateInfo
//
// **********************************************************************************
void Portable_PlatformMonitor::updateIOPMonStateInfo (UInt32 type, UInt32 state)
{
	const OSSymbol		*stateKey, *stateValue;
	
	stateKey = stateValue = NULL;
	
	if (type == kIOPMonPowerSensor)
    {
		stateKey = gIOPMonPowerStateKey;
    
        switch (state)
        {
            case kThermalState0:	stateValue = gIOPMonState0;		break;
            case kThermalState1:	stateValue = gIOPMonState1;		break;
//          case kThermalState2:	stateValue = gIOPMonState2;		break;
//          case kThermalState3:	stateValue = gIOPMonState3;		break;
        }
    }
	else if (type == kIOPMonClamshellSensor)
    {
		stateKey = gIOPMonClamshellStateKey;
        
        if (state == kClamshellStateOpen)
            stateValue = gIOClamshellStateOpen;
        else if (state == kClamshellStateClosed)
            stateValue = gIOClamshellStateClosed;
     }
        
	if (stateKey && stateValue)
		provider->setProperty (stateKey, (OSObject *)stateValue);
	
	return;
}

// **********************************************************************************
// initPlatformState
//
// **********************************************************************************
bool Portable_PlatformMonitor::initPlatformState ()
{
	currentPowerState = initialPowerState;				// We will have booted slow
    currentClamshellState = kClamshellStateOpen;		// Will get an event if lid is closed
    
    return true;
}

// **********************************************************************************
// savePlatformState
//
//		-- protected by CommandGate - call via IOPlatformMonitor::savePlatformState
//
// **********************************************************************************
void Portable_PlatformMonitor::savePlatformState ()
{
	return;
}

// **********************************************************************************
// restorePlatformState
//
//		-- protected by CommandGate - call via IOPlatformMonitor::restorePlatformState
//
// **********************************************************************************
void Portable_PlatformMonitor::restorePlatformState ()
{
	// Last states are indeterminate
    
	lastPowerState = kMaxPowerStates;
	lastClamshellState = kNumClamshellStates;

    resetThermalSensorThresholds();
    
	adjustPlatformState();
}

// **********************************************************************************
// adjustPlatformState
//
// This gets called whenever there is a possible state change.  A state change doesn't
// imply that you always have to take an action - in fact, most of the time you don't -
// so only if the action we should take is different from the last action we took do
// we do something.
//
// **********************************************************************************
bool Portable_PlatformMonitor::adjustPlatformState ()
{
	bool		result = true;
    bool		reevaluateThermalLevels = false;
    UInt32		actionToTake = 0;
    UInt32		subSensor;
	
    if (lastPowerState != currentPowerState)
    {
		lastPowerState = currentPowerState;
		updateIOPMonStateInfo(kIOPMonPowerSensor, currentPowerState);
	}
    
	if (lastClamshellState != currentClamshellState)
    {
		lastClamshellState = currentClamshellState;
		updateIOPMonStateInfo(kIOPMonClamshellSensor, currentClamshellState);
		reevaluateThermalLevels = true;
	}
	
    if ( reevaluateThermalLevels )
        resetThermalSensorThresholds();
    
    // All the sensors should be programmed approprately, set the things we have control over to the proper
    // power level for the current thermals.
    
    for (subSensor = 0; subSensor < kMaxSensorIndex; subSensor++)
        if (subSensorArray[subSensor].registered)
            actionToTake |= thermalThresholdInfoArray[subSensor][currentClamshellState][subSensorArray[subSensor].state].thresholdAction;
    
    applyPowerSettings(actionToTake);
    
	return result;
}

// **********************************************************************************
// registerConSensor
//
// **********************************************************************************
IOReturn Portable_PlatformMonitor::registerConSensor (OSDictionary *dict, IOService *conSensor)
{
	UInt32 			csi, subsi, type, initialState, initialValue;
	ConSensorInfo	*csInfo;
	OSObject		*obj;
        		
	if (!lookupConSensorInfo (dict, conSensor, &type, &csi, &subsi))
		return kIOReturnBadArgument;
		
	if (subsi < kMaxSensorIndex)	// Is subsensor index valid? If so use subSensorArray
		csInfo = &subSensorArray[subsi];
	else
		csInfo = &conSensorArray[csi];
	csInfo->conSensor = conSensor;
	csInfo->dict = dict;
	dict->retain ();
	
	initialState = initialValue = 0;
	// type dependent initialization
	switch (csInfo->conSensorType)
    {
		case kIOPMonThermalSensor:
			/*
			 * Thermal sensors get aggregated through the subSensorArray.  The main thermal sensor
			 * entry in conSensorArray only tracks the overall state
			 */
			if (!csInfo->sensorValid)
				return kIOReturnUnsupported;		// Don't need this sensor - tell it to go away
			
			//if (!retrieveCurrentValue (dict, &initialThermalValue))
			//	return kIOReturnBadArgument;
			
			// Figure out our initial state
            // Just initialize the initial state/initial value to 0 for now and let the normal HandleThermalEvent get called if it needs to later...
			//initialState = lookupThermalStateFromValue (subsi, initialThermalValue);
			
			if (!(csInfo->threshDict = OSDictionary::withCapacity(3)))
				return kIOReturnNoMemory;
				
			csInfo->threshDict->setObject (gIOPMonIDKey, OSNumber::withNumber (subsi, 32));
			csInfo->threshDict->setObject (gIOPMonLowThresholdKey, 
				OSNumber::withNumber (thermalThresholdInfoArray[subsi][currentClamshellState][initialState].thresholdLow, 32));
			csInfo->threshDict->setObject (gIOPMonHighThresholdKey, 
				OSNumber::withNumber (thermalThresholdInfoArray[subsi][currentClamshellState][initialState].thresholdHigh, 32));
            
			// Send thresholds to sensor
			conSensor->setProperties (csInfo->threshDict);
			csInfo->registered = true;
			break;
		
		case kIOPMonPowerSensor:
			initialState = initialPowerState;		// Whether a machine booted slow or fast is set up at Start time 
			break;
		
		case kIOPMonClamshellSensor:
			if (!retrieveCurrentValue (dict, &initialValue))
				return kIOReturnBadArgument;

			csInfo->registered = true;
			break;
		
		case kIOPMonCPUController:
			initialState = initialCPUPowerState;		// Whether a machine booted slow or fast is set up at Start time
			csInfo->registered = true;
			break;
		
		case kIOPMonGPUController:
			initialState = kGPUPowerState0;			// GPU starts out fast
			break;
                        
        case kIOPMonSlewController:
                obj = csInfo->dict->getObject ("current-value");
                initialValue = (OSDynamicCast (OSNumber, obj))->unsigned32BitValue();
                initialState = initialValue;
                csInfo->registered = true;
			break;
		
		default:
			break;
	}
	
	csInfo->value = initialValue;
	csInfo->state = initialState;

	return kIOReturnSuccess;
}

// **********************************************************************************
// unregisterSensor
//
// **********************************************************************************
bool Portable_PlatformMonitor::unregisterSensor (UInt32 sensorID)
{
	if (sensorID >= kMaxSensors)
		return false;
	
	conSensorArray[sensorID].conSensor = NULL;
	return true;
}

// **********************************************************************************
// lookupConSensorInfo
//
// **********************************************************************************
bool Portable_PlatformMonitor::lookupConSensorInfo (OSDictionary *dict, IOService *conSensor, 
	UInt32 *type, UInt32 *index, UInt32 *subIndex)
{
	OSSymbol	*typeString;
	UInt32		tempType;
	
	OSObject				*obj, *obj2;
	
	*index = kMaxConSensors;					// assume not found
	*subIndex = kMaxSensorIndex;				// assume not found
    
	// See if we have a type
	*type = tempType = kIOPMonUnknownSensor;	// assume not
	obj = dict->getObject (gIOPMonTypeKey);
	obj2 = dict->getObject (gIOPMonConTypeKey);
    
	if ((obj && (typeString = OSDynamicCast (OSSymbol, obj))) || (obj2 && (typeString = OSDynamicCast (OSSymbol, obj2))))
    {
		if (typeString->isEqualTo (gIOPMonTypePowerSens))
			tempType = kIOPMonPowerSensor;
		else if (typeString->isEqualTo (gIOPMonTypeThermalSens))
			tempType = kIOPMonThermalSensor;
		else if (typeString->isEqualTo (gIOPMonTypeClamshellSens))
			tempType = kIOPMonClamshellSensor;
		else if (typeString->isEqualTo (gIOPMonTypeCPUCon))
			tempType = kIOPMonCPUController;
		else if (typeString->isEqualTo (gIOPMonTypeGPUCon))
			tempType = kIOPMonGPUController;
		else if (typeString->isEqualTo (gIOPMonTypeSlewCon))
			tempType = kIOPMonSlewController;
		else if (typeString->isEqualTo (gIOPMonTypeFanCon))
			tempType = kIOPMonFanController;
	} 
		
	if (retrieveSensorIndex (dict, subIndex))
    {
		if (*subIndex >= kMaxSensorIndex)
			return false;
			
		// We're dealing with a subSensor, so set the type and validate it in the subSensor array by index
		*type = (tempType == kIOPMonUnknownSensor) ? subSensorArray[*subIndex].conSensorType : tempType;
			
		return true;
	}
	
	/*
	 * Treat anything else as a primary sensor.
	 * For this platform there is only one of each primary controller/sensor so if we find the type
	 * we have the index.
	 */
	for (*index = 0; *index < kMaxConSensors; (*index)++)
    {
		if (conSensorArray[*index].conSensorType == tempType)
        {
			*type = tempType;
			break;
		}
	}
	
	return (*index < kMaxConSensors);
}

// **********************************************************************************
// lookupThermalStateFromValue
//
// **********************************************************************************
UInt32 Portable_PlatformMonitor::lookupThermalStateFromValue (UInt32 sensorIndex, ThermalValue value)
{
	UInt32	i;
	UInt32	curSensorState;
        
	// First, lets see if we are at our current know sensor state...
        
    curSensorState = subSensorArray[sensorIndex].state;
    if (curSensorState != kMaxThermalStates)
    {
        if (value > thermalThresholdInfoArray[sensorIndex][currentClamshellState][curSensorState].thresholdLow &&
                                value < thermalThresholdInfoArray[sensorIndex][currentClamshellState][curSensorState].thresholdHigh)
            return curSensorState;
    }
    
    // If not, lets see what state we are at...
        
	if (value > thermalThresholdInfoArray[sensorIndex][currentClamshellState][0].thresholdLow)
    {
		for (i = 0 ; i < subSensorArray[sensorIndex].numStates; i++)
			if (value > thermalThresholdInfoArray[sensorIndex][currentClamshellState][i].thresholdLow &&
				value < thermalThresholdInfoArray[sensorIndex][currentClamshellState][i].thresholdHigh)
					return i;

		// Sensor's already over the limit - need to figure right response
		return kMaxThermalStates;
	}
	
	// Safely below the lowest threshold
	return kThermalState0;
}

// **********************************************************************************
// handlePowerEvent
//
//		-- protected by CommandGate - call through handleEvent
//
// **********************************************************************************
bool Portable_PlatformMonitor::handlePowerEvent (IOPMonEventData *eventData)
{
	UInt32		nextPowerState;
	bool		result;
        
	debug_msg ("Portable_PlatformMonitor::handlePowerEvent - START\n");
    
	if (!(retrieveCurrentValue (eventData->eventDict, &nextPowerState)))
		return false;
		
	result = true;
	if (currentPowerState != nextPowerState)
    {
		currentPowerState = nextPowerState;
		result = adjustPlatformState ();
	}
	
	return result;
}

// **********************************************************************************
// handleThermalEvent
//
//		-- protected by CommandGate - call through handleEvent
//
// **********************************************************************************
bool Portable_PlatformMonitor::handleThermalEvent (IOPMonEventData *eventData)
{
	UInt32					type, csi, subsi, myThermalState;
	ThermalValue			value;
	bool					result = true;
        
	debug_msg ("Portable_PlatformMonitor::handleThermalEvent - START\n");
    
	if (!(retrieveCurrentValue (eventData->eventDict, &value)))
		return false;
	
	switch (eventData->event)
    {		
		case kIOPMonMessageLowThresholdHit:		
		case kIOPMonMessageHighThresholdHit:
			if (lookupConSensorInfo (eventData->eventDict, eventData->conSensor, &type, &csi, &subsi) && subSensorArray[subsi].registered)
            {
				myThermalState = lookupThermalStateFromValue (subsi, value);
				if (myThermalState >= kMaxThermalStates)
					result = false;
			}
            else
				result = false;
			break;
				
		default:
			result = false;
			break;
	}
	
	if (result)
    {
		if (myThermalState != subSensorArray[subsi].state)
        {
			subSensorArray[subsi].state = myThermalState;
			if (!subSensorArray[subsi].registered) 
				return false;					// Not registered

            setSensorThermalThresholds(subsi, thermalThresholdInfoArray[subsi][currentClamshellState][myThermalState].thresholdLow, 
                                              thermalThresholdInfoArray[subsi][currentClamshellState][myThermalState].thresholdHigh);
		}
        
        result = adjustPlatformState ();
	}
    
	return result;
}

// **********************************************************************************
// handleClamshellEvent
//
//		-- protected by CommandGate - call through handleEvent
//
// **********************************************************************************
bool Portable_PlatformMonitor::handleClamshellEvent (IOPMonEventData *eventData)
{
	UInt32		nextClamshellState;
	bool		result = true;
        
	if (!(retrieveCurrentValue (eventData->eventDict, &nextClamshellState)))  return false;
    
	if (currentClamshellState != nextClamshellState)
    {
		currentClamshellState = nextClamshellState;
		result = adjustPlatformState ();
	}
	
	return result;
}

// **********************************************************************************
// iopmonCommandGateCaller - invoked by commandGate->runCommand
//
//		This is single threaded!!
//
// **********************************************************************************
//static
IOReturn Portable_PlatformMonitor::iopmonCommandGateCaller(OSObject *object, void *arg0, void *arg1, void *arg2, void *arg3)
{
	Portable_PlatformMonitor	*me;
	IOPMonCommandThreadSet			*commandSet;
	UInt32							type, conSensorID, subSensorID;
	bool							result;
	
	// Pull our event data and, since we're a static function, a reference to our object out of the parameters
    
	if (((commandSet = (IOPMonCommandThreadSet *)arg0) == NULL) ||
		((me = OSDynamicCast(Portable_PlatformMonitor, commandSet->me)) == NULL))
		return kIOReturnError;
	
	result = true; 

	switch (commandSet->command) {
		case kIOPMonCommandHandleEvent:
			if (!(commandSet->eventData.eventDict))
            {
				IOLog ("Portable_PlatformMonitor::iopmonCommandGateCaller - bad dictionary\n");
				return kIOReturnBadArgument;
			}
			if (!(commandSet->eventData.conSensor))
            {
				IOLog ("Portable_PlatformMonitor::iopmonCommandGateCaller - bad conSensor\n");
				return kIOReturnBadArgument;
			}
			if (!me->lookupConSensorInfo (commandSet->eventData.eventDict, commandSet->eventData.conSensor, &type, &conSensorID, &subSensorID))
            {
				IOLog ("Portable_PlatformMonitor::iopmonCommandGateCaller - bad sensor info lookup\n");
				return kIOReturnBadArgument;
			}
			
			switch (type) {
				case kIOPMonPowerSensor:			// platform power monitor
					result = me->handlePowerEvent (&commandSet->eventData);
					break;
				case kIOPMonThermalSensor:			// thermal sensors
					result = me->handleThermalEvent (&commandSet->eventData);
					break;
				case kIOPMonClamshellSensor:		// pmu clamshell sensor
					result = me->handleClamshellEvent (&commandSet->eventData);
					break;
				
				default:
					IOLog ("Portable_PlatformMonitor::iopmonCommandGateCaller -  bad sensorType(%ld), sensorID(%ld), subsi(%ld)\n", 
						type, conSensorID, subSensorID);
					result = false;
					break;
			}
			break;
		
		case kIOPMonCommandSaveState:
			me->savePlatformState ();
			break;
			
		case kIOPMonCommandRestoreState:
			me->restorePlatformState ();
			break;
		
		default:
			IOLog ("Portable_PlatformMonitor::iopmonCommandGateCaller - bad command %ld\n", commandSet->command);
			result = false;
			break;
	}			
	
	return result ? kIOReturnSuccess : kIOReturnError;
}
