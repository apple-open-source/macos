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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PBG4_ThermalProfile.cpp,v 1.1 2004/11/23 01:55:14 raddog Exp $
 */


#include "IOPlatformPluginSymbols.h"
#include "PBG4_PlatformPlugin.h"

#include "PBG4_ThermalProfile.h"

OSDefineMetaClassAndStructors( PBG4_ThermalProfile, IOPlatformPluginThermalProfile )


UInt8 PBG4_ThermalProfile::getThermalConfig( void ) 
{
	DLOG ("PBG4_ThermalProfile::getThermalConfig entered\n");
	/*
	 * XXXX - this is a placeholder for now.  getThermalConfig needs to determine
	 * which config is correct for the platform.  This value is used to determine
	 * if a particular control is used on the config (based on values in the ValidConfigs)
	 * field of the control or control loop.  
	 *
	 * For now there is only one config (0), so return that.
	 */
	return( 0 );
}


void PBG4_ThermalProfile::adjustThermalProfile( void ) 
{
	DLOG ("PBG4_ThermalProfile::adjustThermalProfile entered\n");
	/*
	 * XXXX - this is a placeholder for now.  adjustThermalProfile() can be used to 
	 * tweak the thermal profile before it is used by the plugin
	 */

	return;
}
