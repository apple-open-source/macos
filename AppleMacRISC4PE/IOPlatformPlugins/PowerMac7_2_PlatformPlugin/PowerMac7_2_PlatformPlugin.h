/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: PowerMac7_2_PlatformPlugin.h,v $
//		Revision 1.6  2003/07/20 23:41:11  eem
//		[3273577] Q37: Systems need to run at Full Speed during test
//		
//		Revision 1.5  2003/07/16 02:02:10  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.4  2003/06/25 02:16:25  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.3.8.1  2003/06/20 01:40:01  eem
//		Although commented out in this submision, there is support here to nap
//		the processors if the fans are at min, with the intent of keeping the
//		heat sinks up to temperature.
//		
//		Revision 1.3  2003/05/13 02:13:52  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.2.2.1  2003/05/12 11:21:12  eem
//		Support for slewing.
//		
//		Revision 1.2  2003/05/10 06:50:36  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.2.3  2003/05/10 06:32:35  eem
//		Sensor changes, should be ready to merge to trunk as 1.0.1d12.
//		
//		Revision 1.1.2.2  2003/05/03 01:11:40  eem
//		*** empty log message ***
//		
//		Revision 1.1.2.1  2003/05/01 09:28:47  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		

#ifndef _POWEMAC7_2_PLATFORMPLUGIN_H
#define _POWEMAC7_2_PLATFORMPLUGIN_H

#include "IOPlatformPlugin.h"

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

#endif