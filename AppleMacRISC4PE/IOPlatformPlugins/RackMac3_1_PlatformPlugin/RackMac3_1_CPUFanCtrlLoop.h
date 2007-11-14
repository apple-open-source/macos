/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: RackMac3_1_CPUFanCtrlLoop.h,v 1.6 2004/03/18 02:18:52 eem Exp $
 */


#ifndef _RACKMAC3_1_CPUFANCTRLLOOP_H
#define _RACKMAC3_1_CPUFANCTRLLOOP_H

#include "IOPlatformPIDCtrlLoop.h"

#define kRM31CPUPIDDatasetsKey		"RM31-CPU-PID-datasets"
#define kRM31ProcessorBinKey		"processor-bin"
#define kRM31MaxPowerKey			"power-max"
#define kRM31MaxPowerAdjustmentKey	"power-max-adjustment"
#define kRM31PIDDatasetVersionKey	"pid-dataset-version"
#define kRM31PIDDatasetSourceKey	"pid-dataset-source"
#define kRM31PIDDatasetSourceIICROM	"MPU EEPROM"
#define kRM31PIDDatasetSourcePList  	"Thermal Profile PList"

// after 30 seconds at max cooling if we're still above T_max, sleep the machine
#define kRM31CPUMaxCoolingLimit		30

// if at any time the temperature exceeds T_max + 8, sleep the machine immediately
// this is in 16.16 fixed point format
#define kRM31CPUTempCriticalOffset	(8 << 16)

/*!	@class RackMac3_1_CPUFanCtrlLoop
	@abstract This class implements a PID-based fan control loop for use on RackMac3,1 machines.  This control loop is designed to work for both uni- and dual-processor machines.  There are actually two (mostly) independent control loops on dual-processor machines, but control is implemented in one control loop object to ease implementation of tach-lock. */

class RackMac3_1_CPUFanCtrlLoop : public IOPlatformPIDCtrlLoop
{

    OSDeclareDefaultStructors(RackMac3_1_CPUFanCtrlLoop)

protected:

    IOPlatformControl * secondOutputControl;
    IOPlatformControl * thirdOutputControl;
    IOPlatformSensor * currentSensor, * voltageSensor, * powerSensor;

    // for debugging purposes
#ifdef CTRLLOOP_DEBUG
    IOPlatformControl * slewControl;
#endif

    // the index for this cpu
    UInt32 procID;

    // dedicated temperature sample buffer -- sampleHistory will be used for power readings
    SensorValue tempHistory[2];
    int tempIndex;

    // Max CPU temperature at diode
    SensorValue inputMax;

    // PowerMaxAdj = PowerMaxROM - AdjStaticFactor
    SensorValue powerMaxAdj;

    // If we've been at max cooling for 30 seconds and are still making no progress,
    // we have to put the machine to sleep.  This counter is used to determine how
    // many seconds we've been at max cooling.
    unsigned int secondsAtMaxCooling;

    // overrides from OSObject superclass
    virtual bool init( void );
    virtual void free( void );

    //virtual const OSNumber *calculateNewTarget( void ) const;

    virtual bool cacheMetaState( const OSDictionary * metaState );
    bool choosePIDDataset( const OSDictionary * ctrlLoopDict );
    OSDictionary * fetchPIDDatasetFromROM( void ) const;
    int comparePIDDatasetVersions( const OSData * v1, const OSData * v2 ) const;

public:

    // By setting a deadline and handling the deadlinePassed() callback, we get a periodic timer
    // callback that is funnelled through the platform plugin's command gate.
    //virtual void deadlinePassed( void );

    virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);
    virtual bool updateMetaState( void );
    virtual void deadlinePassed( void );
    bool acquireSample( void );		// gets a sample (using clock_get_uptime() and getAggregateSensorValue()) and stores it at 
    virtual SensorValue calculateDerivativeTerm( void ) const;
    virtual ControlValue calculateNewTarget( void ) const;
    virtual void sendNewTarget( ControlValue newTarget );
    //virtual void deadlinePassed( void );
    virtual void sensorRegistered( IOPlatformSensor * aSensor );
    virtual void controlRegistered( IOPlatformControl * aControl );
};

#endif	// _RACKMAC3_1_CPUFANCTRLLOOP_H
