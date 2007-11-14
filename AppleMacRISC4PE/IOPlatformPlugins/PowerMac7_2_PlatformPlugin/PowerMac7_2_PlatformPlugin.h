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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _POWEMAC7_2_PLATFORMPLUGIN_H
#define _POWEMAC7_2_PLATFORMPLUGIN_H

#include "IOPlatformPlugin.h"
#include "U3.h"

// Symbols for I2C access
#define kWriteI2Cbus                            "writeI2CBus"
#define kReadI2Cbus                             "readI2CBus"
#define kOpenI2Cbus                             "openI2CBus"
#define kCloseI2Cbus                            "closeI2CBus"
#define kSetDumbMode                            "setDumbMode"
#define kSetStandardMode                        "setStandardMode"
#define kSetStandardSubMode                     "setStandardSubMode"
#define kSetCombinedMode                        "setCombinedMode"
#define kNumRetries			10

// Q37-specific environmental keys
#define kPM72EnvSystemUncalibrated				"system-uncalibrated"
#define kPM72EnvShroudRemoved					"shroud-removed"
#define kPM72EnvAllowNapping					"allow-napping"

class PowerMac7_2_PlatformPlugin : public IOPlatformPlugin
{

        OSDeclareDefaultStructors(PowerMac7_2_PlatformPlugin)

private:
        UInt32	uniNVersion;
        IOService *uniN;

protected:

/*
enum {
	kI2CDumbMode		= 0x01,
	kI2CStandardMode	= 0x02,
	kI2CStandardSubMode	= 0x03,
	kI2CCombinedMode	= 0x04,
	kI2CUnspecifiedMode	= 0x05
};
*/

	// override get config for this platform
	// returns 0 for uni proc config, 1 for dual proc config
	virtual UInt8 probeConfig(void);

	virtual bool start( IOService * provider );
	//virtual void stop( IOService * provider );
	virtual bool init( OSDictionary * dict );
	virtual void free( void );

/*
	// IIC variables
	IOService *			fI2C_iface;
	UInt8				fI2CBus;
	UInt8				fI2CAddress;

	// helper methods for i2c stuff
	bool				openI2C(UInt8 busNo, UInt8 addr);
	void				closeI2C();
	bool				writeI2C( UInt8 subAddr, UInt8 * data, UInt16 size );
	bool				readI2C( UInt8 subAddr, UInt8 * data, UInt16 size );
	bool				setI2CDumbMode( void );
	bool				setI2CStandardMode( void );
	bool				setI2CStandardSubMode( void );
	bool				setI2CCombinedMode( void );
*/

public:

	// reads the processor IIC ROMs
	bool 				readProcROM( UInt32 procID, UInt16 offset, UInt16 size, UInt8 * buf );

};

