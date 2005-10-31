/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PowerMac12_1_ThermalProfile.h,v 1.1 2005/05/19 01:36:59 craiger Exp $
 */
 

#ifndef _POWERMAC12_1_THERMALPROFILE_H
#define _POWERMAC12_1_THERMALPROFILE_H

#include "IOPlatformPlugin.h"


class PowerMac12_1_ThermalProfile : public IOPlatformPluginThermalProfile
	{
	OSDeclareDefaultStructors( PowerMac12_1_ThermalProfile )

protected:

	enum
		{
		kCPUPowerSensorID			= 0x03
		};

public:

	virtual			UInt8			getThermalConfig( void );
	virtual			void			adjustThermalProfile( void );
	};


#endif // _POWERMAC12_1_THERMALPROFILE_H
