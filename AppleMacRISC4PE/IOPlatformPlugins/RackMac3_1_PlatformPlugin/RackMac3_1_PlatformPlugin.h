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
 *  File: $Id: RackMac3_1_PlatformPlugin.h,v 1.6 2004/03/18 02:18:52 eem Exp $
 */


#ifndef _RACKMAC3_1_PLATFORMPLUGIN_H
#define _RACKMAC3_1_PLATFORMPLUGIN_H

#include "IOPlatformPlugin.h"

// RackMac3,1-specific environmental keys
#define kRM31DIMMFanCtrlLoopTarget "DIMMFanCtrlLoopTarget"
#define kRM31EnableSlewing "EnableSlewing"
#define kRM31EnvSystemUncalibrated	"system-uncalibrated"

class RackMac3_1_PlatformPlugin : public IOPlatformPlugin
{
	OSDeclareDefaultStructors(RackMac3_1_PlatformPlugin)

private:
	IOReturn 			togglePowerLED(bool state);

protected:

	// override get config for this platform
	// returns 0 for uni proc config, 1 for dual proc config
	virtual UInt8 		probeConfig(void);

	virtual bool 		start( IOService * provider );
	//virtual void 		stop( IOService * provider );
	virtual bool 		init( OSDictionary * dict );
	virtual void 		free( void );

public:

	// reads the processor IIC ROMs
	bool				readProcROM( UInt32 procID, UInt16 offset, UInt16 size, UInt8 * buf );
	
	virtual	IOReturn 	wakeHandler(void);
};

#endif // _RACKMAC3_1_PLATFORMPLUGIN_H
