/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOPLATFORMMONITOR_H
#define _IOPLATFORMMONITOR_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSIterator.h>

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
//#define IOPFMDEBUG 1

#ifdef IOPFMDEBUG
 #define DLOG(fmt, args...)  kprintf(fmt, ## args)
 #define debug_msg( msg ) IOLog(msg)
#else
 #define DLOG(fmt, args...)
 #define debug_msg( msg )
#endif

#define kIOPMonTypeKey 				"sensor-type"
#define kIOPMonControlTypeKey		"control-type"
#define kIOPMonTypePowerSens		"power-sensor"
#define kIOPMonTypeThermalSens		"temp-sensor"
#define kIOPMonTypeClamshellSens	"clamshell-sensor"
#define kIOPMonTypeCPUCon			"cpu-controller"
#define kIOPMonTypeGPUCon			"gpu-controller"
#define kIOPMonTypeSlewCon			"slew"
#define kIOPMonTypeFanCon			"fan-controller"
#define kIOPMonIDKey				"sensor-id"
#define kIOPMonCPUIDKey				"cpu-index"
#define kIOPMonLowThresholdKey		"low-threshold"
#define kIOPMonHighThresholdKey		"high-threshold"
#define kIOPMonThresholdValueKey	"threshold-value"
#define kIOPMonCurrentValueKey		"current-value"

enum {
	kIOPMonMessageRegister			= 1,
	kIOPMonMessageUnregister		= 2,
	kIOPMonMessageLowThresholdHit	= 3,
	kIOPMonMessageHighThresholdHit	= 4,
	kIOPMonMessageCurrentValue		= 5,
	kIOPMonMessageStateChanged		= 6,
	kIOPMonMessagePowerMonitor		= 7,
	kIOPMonMessageError				= 8, 
};

enum
{
    kPowerBookModel			= 0x00010000,
    kPowerMacModel			= 0x00020000,
    kRackMacModel			= 0x00040000,
	kUsesIOPlatformPlugin	= 0x00080000,	// Flag to indicate machine uses IOPlatformPlugin architecture

    kPB51MachineModel		= kPowerBookModel + 0x00000501,

    kPB61MachineModel		= kPowerBookModel + 0x00000601,

    kPB52MachineModel		= kPowerBookModel + 0x00000502,
    kPB53MachineModel		= kPowerBookModel + 0x00000503,
    kPB62MachineModel		= kPowerBookModel + 0x00000602,
    kPB63MachineModel		= kPowerBookModel + 0x00000603,

    kPB64MachineModel		= kPowerBookModel + 0x00000604,
    kPB65MachineModel		= kPowerBookModel + 0x00000605,
    kPB54MachineModel		= kPowerBookModel + 0x00000504,
    kPB55MachineModel		= kPowerBookModel + 0x00000505,
    
    kPB66MachineModel		= kPowerBookModel + 0x00000606,

    kPB67MachineModel		= kPowerBookModel + 0x00000607,
    kPB68MachineModel		= kPowerBookModel + 0x00000608,

    kPB56MachineModel		= kPowerBookModel + 0x00000506 + kUsesIOPlatformPlugin,
    kPB57MachineModel		= kPowerBookModel + 0x00000507 + kUsesIOPlatformPlugin
};

// Thermal sensor values and thresholds are 16.16 fixed point format
typedef UInt32 ThermalValue;

// Macro to convert integer to sensor temperature format (16.16)
#define TEMP_SENSOR_FMT(x) ((x) << 16)

typedef struct OldThresholdInfo {
	ThermalValue	thresholdLow;
	UInt32			nextStateLow;
	ThermalValue	thresholdHigh;
	UInt32			nextStateHigh;
};

typedef struct SmallerThresholdInfo {
	ThermalValue	thresholdLow;
	ThermalValue	thresholdHigh;
};

typedef struct ThresholdInfo {
	ThermalValue	thresholdLow;
	ThermalValue	thresholdHigh;
	ThermalValue	thresholdAction;
};

enum {
	kThrottleCPU			= 0x00000001,
	kThrottleGPU			= 0x00000002
};

enum {
	kNoSpecialState			= 0x00000000,
	kClamshellClosedState	= 0x00000001
};

typedef struct LimitsInfo {
	SInt32		sensorID;
	UInt16		sensorThreshold;
    UInt16		sensorHysteresis;
    UInt32		sensorEffect;
    UInt16		machineState;
};

typedef struct ConSensorInfo {
	UInt32			conSensorType;
	UInt32			conSensorParent;
	IOService		*conSensor;
	OSDictionary 	*dict;
	OSDictionary 	*threshDict;
	UInt32			responseRate;
	UInt32			value;
	UInt32			state;
	UInt32			numStates;
	bool			sensorValid;
	bool			registered;
};

typedef bool (*IOPlatformMonitorAction)( );

/*!
    @class IOPlatformMonitor
    @abstract A class for monitor system functions such as power and thermal */
class IOPlatformMonitor : public IOService
{
    OSDeclareDefaultStructors(IOPlatformMonitor)	

protected:

typedef struct IOPMonEventData {
	IOService		*conSensor;
	UInt32			event;
	OSDictionary	*eventDict;
};

// Forward declaration
struct IOPMonCommandThreadSet;

typedef void (*IOPMonCommandFunctionType)(IOPMonCommandThreadSet *threadSet);

typedef struct IOPMonCommandThreadSet {
	IOPlatformMonitor			*me;
	UInt32						command;
	thread_call_t				workThread;
	IOPMonCommandFunctionType	commandFunction;
	IOPMonEventData				eventData;
};

private:
	bool retrieveValueByKey (const OSSymbol *key, OSDictionary *dict, UInt32 *value);
	static void executeCommandThread (IOPMonCommandThreadSet *threadSet);
	virtual bool threadCommon (IOPMonCommandThreadSet *threadSet);	

protected:

// IOPMon controller/sensor types
enum {
	kIOPMonUnknownSensor		= 0,
	kIOPMonPowerSensor			= 1,
	kIOPMonThermalSensor		= 2,
	kIOPMonClamshellSensor		= 3,
	kIOPMonCPUController		= 4,
	kIOPMonGPUController		= 5,
	kIOPMonSlewController		= 6,
	kIOPMonFanController		= 7
};

enum {
	kIOPMonCommandHandleEvent	= 1,
	kIOPMonCommandSaveState		= 2,
	kIOPMonCommandRestoreState	= 3
};

	//	Common symbols
	const OSSymbol 				*gIOPMonTypeKey;
    const OSSymbol 				*gIOPMonConTypeKey;
	const OSSymbol 				*gIOPMonTypePowerSens;
	const OSSymbol 				*gIOPMonTypeThermalSens;
	const OSSymbol				*gIOPMonTypeClamshellSens;
	const OSSymbol 				*gIOPMonTypeCPUCon;
	const OSSymbol 				*gIOPMonTypeGPUCon;
	const OSSymbol 				*gIOPMonTypeSlewCon;
	const OSSymbol 				*gIOPMonTypeFanCon;
	const OSSymbol 				*gIOPMonIDKey;
	const OSSymbol 				*gIOPMonCPUIDKey;
	const OSSymbol 				*gIOPMonLowThresholdKey;
	const OSSymbol 				*gIOPMonHighThresholdKey;
	const OSSymbol 				*gIOPMonThresholdValueKey;
	const OSSymbol 				*gIOPMonCurrentValueKey;

	UInt32						currentPowerState;
    UInt32						currentClamshellState;
    UInt32						currentThermalState;		// Not needed for iBooks/miniPBs
    
	UInt32						lastPowerState;
    UInt32						lastClamshellState;
    UInt32						lastThermalState;			// Not needed for iBooks/miniPBs
    
    IOPlatformMonitorAction		lastAction;					// Not needed for iBooks/miniPBs	

    IOPMrootDomain				*pmRootDomain;

    IOWorkLoop					*workLoop;         // The workloop:
    IOCommandGate				*commandGate;      // The command gate

	IOCommandGate::Action		commandGateCaller;	// handler for commandGate runCommand

	virtual bool initSymbols ();

	virtual IOReturn monitorPower (OSDictionary *dict, IOService *provider);
	
	// Dictionary access utility functions
	bool retrieveSensorIndex (OSDictionary *dict, UInt32 *value);
	bool retrieveThreshold (OSDictionary *dict, ThermalValue *value);
	bool retrieveCurrentValue (OSDictionary *dict, UInt32 *value);

	// Unneeded by cleaned up PlatformMonitor, delete when possible

	virtual bool initPowerState ();
	virtual void savePowerState ();
	virtual bool restorePowerState ();
	
	virtual bool initThermalState ();
	virtual void saveThermalState ();
	virtual bool restoreThermalState ();

	virtual bool initClamshellState ();
	virtual void saveClamshellState ();
	virtual bool restoreClamshellState ();
public:

    virtual bool start(IOService *nub);
    virtual void free();
    virtual IOReturn message( UInt32 type, IOService * provider, void * argument = 0 );
    virtual IOReturn powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);
    virtual IOReturn setAggressiveness(unsigned long selector, unsigned long newLevel);

	virtual bool initPlatformState ();
	virtual void savePlatformState ();
	virtual void restorePlatformState ();
	virtual bool adjustPlatformState ();

	virtual IOReturn registerConSensor (OSDictionary *dict, IOService *conSensor);
	virtual bool unregisterSensor (UInt32 sensorID);
	virtual IOReturn handleEvent (IOPMonEventData *event);
	
	
};
#endif
