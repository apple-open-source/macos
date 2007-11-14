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
 *  File: $Id: SMU_Neo2_PlatformPlugin.h,v 1.9 2005/09/12 21:02:19 larson Exp $
 */


#ifndef _SMU_NEO2_PLATFORMPLUGIN_H
#define _SMU_NEO2_PLATFORMPLUGIN_H

#include "IOPlatformPlugin.h"


// SDB partition header

typedef struct
{
	UInt8					pID;
	UInt8					pLEN;
	UInt8					pVER;
	UInt8					pFlags;
} sdb_partition_header_t;

// Partitions we currently use

enum
{
	kDebugSwitchesPartID					= 0x05,
	kCPU_FVT_OperatingPointsPartID			= 0x12,
	kCPUDependentThermalPIDParamsPartID		= 0x17,
	kDiodeCalibrationPartID					= 0x18,
	kThermalPositioningADCConstantsPartID	= 0x21,
	kSensorTreePartID						= 0x25,
	kSlotsPowerADCConstantsPartID			= 0x78
};

// SDB partition 0x12 - CPU F/V/T operating points

typedef struct
{
	UInt32	freq;			// SYSCLK frequency in Hz
	UInt8	reserved00;
	UInt8	Tmax;			// Max temperature for this operating pt
	UInt16	Vcore[3];		// Core voltage for each stepping
} sdb_cpu_fvt_entry_t;

// SDB partition 0x21 - Processor voltage, current, and power conversion constants

typedef struct
{
	sdb_partition_header_t	header;
	
	UInt16					voltageScale;			// 4.12 unsigned number
	UInt16					voltageOffset;			// 4.12 signed number

	UInt16					currentScale;			// 4.12 unsigned number
	UInt16					currentOffset;			// 4.12 signed number

	// CPU power supply efficiency quadratic equation variables
	
	SInt32					a_Value;				// 4.28 signed number
	SInt32					b_Value;				// 4.28 signed number
	SInt32					c_Value;				// 4.28 signed number

} sdb_thermal_pos_adc_constants_2_part_t;


#define SwapUInt16SMU(w) _OSSwapInt16(w)
#define SwapUInt32SMU(d) ((((d) & 0x00FF00FF) << 8) | (((d) & 0xFF00FF00) >> 8))

// Symbols for callPlatformFunction accesses into AppleSMU
#define kSymGetExtSwitches		"getExtSwitches"
#define kSymRegisterForInts		"smu_registerForInterrupts"
#define kSymDeRegisterForInts	"smu_deRegisterForInterrupts"

// this is the prototype for AppleSMU interrupt callbacks
typedef void (*AppleSMUClient)(IOService * client,UInt8 matchingMask, UInt32 length, UInt8 * buffer); 

// SMU interrupt mask bit definitions
typedef enum {
    kPMUextInt 			= 0x01,   // interrupt type 0 (machine-specific)
    kPMUMD1Int 			= 0x02,   // interrupt type 1 (machine-specific)
    kPMUpcmicia 		= 0x04,   // pcmcia (buttons and timeout-eject)
    kPMUbrightnessInt 	= 0x08,   // brightness button has been pressed, value changed
    kPMUADBint 			= 0x10,   // ADB
    kPMUbattInt         = 0x20,   // battery
    kPMUenvironmentInt 	= 0x40,   // environment
    kPMUoneSecInt       = 0x80    // one second interrupt
};

// Other definitions, such as kClamshellClosedEventMast, are defined in <IOKit/pwr_mgt/IOPM.h>

class SMU_Neo2_PlatformPlugin : public IOPlatformPlugin
    {
	OSDeclareDefaultStructors( SMU_Neo2_PlatformPlugin )

private:
	static		void										chassisSwitchHandler( IOService * client,UInt8 matchingMask, UInt32 length, UInt8 * buffer );
	static		IOReturn									chassisSwitchSyncHandler( void * p1 /* mask */, void * p2 /* length */, void * p3 /* buffer */ );

protected:

				IOPlatformPluginThermalProfile*				thermalProfile;
				IOService*									thermalNub;
				IOService*									appleSMU;

	virtual		UInt8										probeConfig( void );
	virtual		bool										start( IOService* provider );
	virtual		bool										initThermalProfile(IOService *nub);

	virtual		void										registerChassisSwitchNotifier( void );
	virtual		OSBoolean*									pollChassisSwitch( void );

public:

				bool										getSDBPartitionData( UInt8 partitionID, UInt16 offset, UInt16 length, UInt8* buffer );
				IOService*									getAppleSMU( void );
    };


extern SMU_Neo2_PlatformPlugin*			gPlatformPlugin;


#endif	// _SMU_NEO2_PLATFORMPLUGIN_H
