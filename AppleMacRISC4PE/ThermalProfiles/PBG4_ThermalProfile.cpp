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
 *  File: $Id: PBG4_ThermalProfile.cpp,v 1.3 2005/09/06 18:22:30 raddog Exp $
 */


#include "IOPlatformPluginSymbols.h"
#include "PBG4_PlatformPlugin.h"
#include "PBG4_StepCtrlLoop.h"

#include "PBG4_ThermalProfile.h"

OSDefineMetaClassAndStructors( PBG4_ThermalProfile, IOPlatformPluginThermalProfile )


UInt8 PBG4_ThermalProfile::getThermalConfig( void ) 
{
	OSDictionary		*dict, *configDict;
	OSArray				*configArray;
	IORegistryEntry     *cpu0RegEntry;
	OSData				*clockFrequencyData;
	OSNumber			*configFreqNum;
	UInt32				count, clockFreq, configFreq;
	SInt32				configDiff, thisDiff;
	UInt8				config;
	
	DLOG ("PBG4_ThermalProfile::getThermalConfig entered\n");
	
	config = 0;

	dict = OSDynamicCast (OSDictionary, getProperty( kIOPPluginThermalProfileKey));
	if (dict) {
		configArray = OSDynamicCast (OSArray, dict->getObject (kIOPPluginThermalConfigsKey));
		// See if we have more than one config to worry about
		if (configArray && ((count = configArray->getCount()) > 1)) {
			int				pstep;		// xxx temp
		
			pstep = 0;
			if (PE_parse_boot_arg("pstep", &pstep) && ((pstep & kStepTableOverride) != 0)) {
				IOLog ("Stepper table override - using 'better' table\n");
				config = 1;				// Force "better" table
			} else {
				// Determine which table to use based on frequency
				cpu0RegEntry = fromPath("/cpus/@0", gIODTPlane);
				if (cpu0RegEntry && (clockFrequencyData = OSDynamicCast (OSData, cpu0RegEntry->getProperty ("clock-frequency")))) {
					clockFreq = *(UInt32 *)clockFrequencyData->getBytesNoCopy();
					
					// Find the config with the associated clock-frequency closest to the clock-frequency reported by the bootROM
					configDiff = INT32_MAX;
					for (UInt32 i = 0; i < count; i++) {
						configDict = OSDynamicCast (OSDictionary, configArray->getObject (i));
						if (configDict &&
							(configFreqNum = OSDynamicCast (OSNumber, configDict->getObject ("clock-frequency")))) {
							configFreq = configFreqNum->unsigned32BitValue();
							// Figure out the absolute difference
							thisDiff = (configFreq > clockFreq) ? (configFreq - clockFreq) : (clockFreq - configFreq);
							if (thisDiff < configDiff) {
								configDiff = thisDiff;
								config = i;
							}
						} else {
							IOLog ("PBG4_ThermalProfile::getThermalConfig - no clock-frequency data for config %ld.  Defaulting to config 0\n", i);
							DLOG ("PBG4_ThermalProfile::getThermalConfig - no clock-frequency data for config %d.  Defaulting to config 0\n", i);
							config = 0;
							break;
						}
					}
				}

				if (cpu0RegEntry) cpu0RegEntry->release();
			}
		}
	}
	
	return( config );
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
