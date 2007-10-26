/*
 * Copyright (c) 1998-2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */
#include <sys/cdefs.h>

__BEGIN_DECLS

#include <ppc/proc_reg.h>
#include <ppc/machine_routines.h>

__END_DECLS

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include "MacRISC2.h"
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>

static unsigned long macRISC2Speed[] = { 0, 1 };

#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOPMSlotsMacRISC2.h"
#include "IOPMUSBMacRISC2.h"
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include <pexpert/pexpert.h>

#include "IOPlatformMonitor.h"
#include "Portable_PlatformMonitor.h"

extern char *gIOMacRISC2PMTree;

#ifndef kIOHibernateFeatureKey
#define kIOHibernateFeatureKey	"Hibernation"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super ApplePlatformExpert

OSDefineMetaClassAndStructors(MacRISC2PE, ApplePlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool MacRISC2PE::start(IOService *provider)
{
    long            		machineType;
    OSData          		*tmpData;
    IORegistryEntry 		*uniNRegEntry;
    IORegistryEntry 		*powerMgtEntry;
    UInt32			   		*primInfo;
	UInt32					stepType, bitsSet, bitsClear;
    UInt32					platformOptions = 0;
	bool					result;
	
    setChipSetType(kChipSetTypeCore2001);
	
    // Set the machine type.
    provider_name = provider->getName();
    

    determinePlatformNumber();

	machineType = kMacRISC2TypeUnknown;
	doPlatformPowerMonitor = false;
	if (provider_name != NULL) {
		if (0 == strncmp(provider_name, "PowerMac", strlen("PowerMac")))
			machineType = kMacRISC2TypePowerMac;
		else if (0 == strncmp(provider_name, "RackMac", strlen("RackMac")))
			machineType = kMacRISC2TypePowerMac;
		else if (0 == strncmp(provider_name, "PowerBook", strlen("PowerBook")))
			machineType = kMacRISC2TypePowerBook;
		else if (0 == strncmp(provider_name, "iBook", strlen("iBook")))
			machineType = kMacRISC2TypePowerBook;
		else	// kMacRISC2TypeUnknown
			IOLog ("AppleMacRISC2PE - warning: unknown machineType\n");
	}
    
    if ( strcmp( provider_name, "RackMac1,1" ) == 0 )   // P69 specifically
    {
        // the P69 BootROM(s) specify "MacRISC2" and "MacRISC" as compatible
        // property entries for P69.  This causes Disk Utility to enable the
        // check-box for the "Mac OS 9 Drivers Installed", even though P69
        // never allowed booting with Mac OS 9.
        // this code re-constructs the "compatible" property with the values
        // "RackMac1,1", "MacRISC3" and "Power Macintosh", leaving out
        // "MacRISC" and "MacRISC2".
        int         newCompatiblePropertySize;
        char        newCompatiblePropertyData[ 36 ];            // actual constructed property size is 36 bytes (see sizes below)

        strncpy( newCompatiblePropertyData, "RackMac1,1", sizeof("RackMac1,1") );      // sizeof( string ) always contains the '\0' byte as part of the size
        newCompatiblePropertySize = sizeof( "RackMac1,1" );     // == 11
        strncpy( &newCompatiblePropertyData[newCompatiblePropertySize], "MacRISC3", sizeof( "MacRISC3" ) );
        newCompatiblePropertySize += sizeof( "MacRISC3" );      // ==  9
        strncpy( &newCompatiblePropertyData[newCompatiblePropertySize], "Power Macintosh", sizeof( "Power Macintosh" ) );
        newCompatiblePropertySize += sizeof( "Power Macintosh" );//== 16; total = 11+9+16 = 36
        provider->setProperty( "compatible", (void *)newCompatiblePropertyData, newCompatiblePropertySize );
    }
	
	isPortable = (machineType == kMacRISC2TypePowerBook);
	
    setMachineType(machineType);
	
    // Get the bus speed from the Device Tree.
    tmpData = OSDynamicCast(OSData, provider->getProperty("clock-frequency"));
    if (tmpData == 0) return false;
    macRISC2Speed[0] = *(unsigned long *)tmpData->getBytesNoCopy();
    
    // Do we have our thermal information stored already configured in the bootROM ?
    
    tmpData = OSDynamicCast(OSData, provider->getProperty("platform-options"));
    
    if ( tmpData != NULL)
        platformOptions = *((UInt32 *)(tmpData->getBytesNoCopy()));
    
    hasEmbededThermals = ((platformOptions & 0x00000001) != 0);

    if ( (pMonPlatformNumber & kUsesIOPlatformPlugin) != 0 )
        hasPPlugin = true;
    else
        hasPMon = (pMonPlatformNumber != 0) || hasEmbededThermals;
    
    // Indicate may need CPU/I2 reset on wake    
    i2ResetOnWake = ((pMonPlatformNumber == kPB58MachineModel) || (pMonPlatformNumber == kPB59MachineModel));
    
	// get uni-N version for use by platformAdjustService
	uniNRegEntry = provider->childFromPath("uni-n", gIODTPlane);
	if (uniNRegEntry == 0) return false;
    tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("device-rev"));
    if (tmpData == 0) return false;
    uniNVersion = ((unsigned long *)tmpData->getBytesNoCopy())[0];
   	
    // Get PM features and private features
    powerMgtEntry = retrievePowerMgtEntry ();
    if (powerMgtEntry == 0)
    {
        kprintf ("didn't find power mgt node\n");
        return false;
    }

    tmpData  = OSDynamicCast(OSData, powerMgtEntry->getProperty ("prim-info"));
    if (tmpData != 0)
    {
        primInfo = (unsigned long *)tmpData->getBytesNoCopy();
        if (primInfo != 0)
        {
            _pePMFeatures            = primInfo[3];
            _pePrivPMFeatures        = primInfo[4];
            _peNumBatteriesSupported = ((primInfo[6]>>16) & 0x000000FF);
            kprintf ("Public PM Features: %0x.\n",_pePMFeatures);
            kprintf ("Privat PM Features: %0x.\n",_pePrivPMFeatures);
            kprintf ("Num Internal Batteries Supported: %0x.\n", _peNumBatteriesSupported);
        }
    }
  
    // This is to make sure that is PMRegisterDevice reentrant
    mutex = IOLockAlloc();
    if (mutex == NULL)
		return false;
    else
		IOLockInit( mutex );
	
    // Set up processorSpeedChangeFlags depending on platform
	processorSpeedChangeFlags = kNoSpeedChange;
	stepType = 0;
    if (machineType == kMacRISC2TypePowerBook)
    {
		OSIterator 		*childIterator;
                IORegistryEntry 		*cpuEntry, *powerPCEntry, *devicetreeRegEntry;
		OSData			*cpuSpeedData, *stepTypeData;

		// locate the first PowerPC,xx cpu node so we can get clock properties
		cpuEntry = provider->childFromPath("cpus", gIODTPlane);
		if ((childIterator = cpuEntry->getChildIterator (gIODTPlane)) != NULL)
        {
			while ((powerPCEntry = (IORegistryEntry *)(childIterator->getNextObject ())) != NULL)
            {
				if (!strncmp ("PowerPC", powerPCEntry->getName(gIODTPlane), strlen ("PowerPC")))
                {
					// Look for dynamic power step feature
					stepTypeData = OSDynamicCast( OSData, powerPCEntry->getProperty( "dynamic-power-step" ));
					if (stepTypeData)
                    {
						processorSpeedChangeFlags = kProcessorBasedSpeedChange | kProcessorFast | 
							kL3CacheEnabled | kL2CacheEnabled;
                                                        
                            stepTypeData = OSDynamicCast( OSData, powerPCEntry->getProperty( "has-bus-slewing" ));
                            if (stepTypeData)
                                processorSpeedChangeFlags |= kBusSlewBasedSpeedChange;
                    }
					else
                    {	// Look for forced-reduced-speed case
						stepTypeData = OSDynamicCast( OSData, powerPCEntry->getProperty( "force-reduced-speed" ));
						cpuSpeedData = OSDynamicCast( OSData, powerPCEntry->getProperty( "max-clock-frequency" ));
						if (stepTypeData && cpuSpeedData)
                        {
							// Platform requires environmentally forced speed changes possibly overriding user
							// choices.  These might include slowing down when charging with a weak charger or 
							// reducing speed when the lid is closed to avoid heat buildup.
							UInt32 newCPUSpeed, newNum;
							
							doPlatformPowerMonitor = true;
				
							// At minimum disable L3 cache
							// Note that caches are enabled at this point, but the processor may not be at full speed.
							processorSpeedChangeFlags = kDisableL3SpeedChange | kL3CacheEnabled | kL2CacheEnabled;

							if (stepTypeData->getLength() > 0)
								stepType = *(UInt32 *) stepTypeData->getBytesNoCopy();
				
							newCPUSpeed = *(UInt32 *) cpuSpeedData->getBytesNoCopy();
							if (newCPUSpeed != gPEClockFrequencyInfo.cpu_clock_rate_hz)
                            {
								// If max cpu speed is greater than what OF reported to us
								// then enable PMU speed change in addition to L3 speed change
								// and assuming platform supports that feature.
								if ((_pePrivPMFeatures & (1 << 17)) != 0)
									processorSpeedChangeFlags |= kPMUBasedSpeedChange;
								processorSpeedChangeFlags |= kEnvironmentalSpeedChange;
								// Also fix up internal clock rates
								newNum = newCPUSpeed / (gPEClockFrequencyInfo.cpu_clock_rate_hz /
														gPEClockFrequencyInfo.bus_to_cpu_rate_num);
								gPEClockFrequencyInfo.bus_to_cpu_rate_num = newNum;		// Set new numerator
								gPEClockFrequencyInfo.cpu_clock_rate_hz = newCPUSpeed;		// Set new speed (old, 32-bit)
								gPEClockFrequencyInfo.cpu_frequency_hz = newCPUSpeed;		// Set new speed (64-bit)
                                gPEClockFrequencyInfo.cpu_frequency_max_hz = newCPUSpeed;	// Max as well (64-bit)
							}
						}
                        else
                        { // All other notebooks
							// Enable PMU speed change, if platform supports it.  Note that there is also
							// an implicit assumption here that machine started up in fastest mode.
							if ((_pePrivPMFeatures & (1 << 17)) != 0)
                            {
								processorSpeedChangeFlags = kPMUBasedSpeedChange | kProcessorFast | 
									kL3CacheEnabled | kL2CacheEnabled;
								// Some platforms need to disable the L3 at slow speed.  Since we're
								// already assuming the machine started up fast, just set a flag
								// that will cause L3 to be toggled when the speed is changed.
								if ((_pePrivPMFeatures & (1<<21)) != 0)
									processorSpeedChangeFlags |= kDisableL3SpeedChange;
							}
						}
					}
					break;
				}
			}
			childIterator->release();
		}
		
		// check if this machine supports PowerPlay....
		devicetreeRegEntry = fromPath("/", gIODTPlane);
		tmpData = OSDynamicCast(OSData, devicetreeRegEntry->getProperty("graphics-setagressiveness"));
		if (tmpData) 
			// found property that says we support PowerPlay so set a bit to indicate this
			processorSpeedChangeFlags |= kSupportsPowerPlay;
	}
	
	// Create PlatformFunction nub
	OSDictionary *dict = OSDictionary::withCapacity(2);
	if (dict) {
		const OSSymbol *nameKey, *compatKey, *nameValueSymbol;
		const OSData *nameValueData, *compatValueData;
		char tmpName[32], tmpCompat[128];
		
		nameKey = OSSymbol::withCStringNoCopy("name");
		strncpy(tmpName, "IOPlatformFunction", sizeof( "IOPlatformFunction" ));
		nameValueSymbol = OSSymbol::withCString(tmpName);
		nameValueData = OSData::withBytes(tmpName, strlen(tmpName)+1);
		dict->setObject (nameKey, nameValueData);
		compatKey = OSSymbol::withCStringNoCopy("compatible");
		strncpy (tmpCompat, "IOPlatformFunctionNub", sizeof( "IOPlatformFunctionNub" ) );
		compatValueData = OSData::withBytes(tmpCompat, strlen(tmpCompat)+1);
		dict->setObject (compatKey, compatValueData);
		if (plFuncNub = IOPlatformExpert::createNub (dict)) {
			if (!plFuncNub->attach( this ))
				IOLog ("NUB ATTACH FAILED for IOPlatformFunctionNub\n");
			plFuncNub->setName (nameValueSymbol);

			plFuncNub->registerService();
		}
		dict->release();
		nameValueSymbol->release();
		nameKey->release();
		nameValueData->release();
		compatKey->release();
		compatValueData->release();
	}
	
	/*
	 * Call super::start *before* we create specialized child nubs.  Child drivers for these nubs
	 * want to call publishResource and publishResource needs the IOResources node to exist 
	 * if not we'll get a message like "not registry member at registerService()"
	 * super::start() takes care of creating IOResources
	 */
	result = super::start(provider);
	
	// Create PlatformMonitor or IOPlatformPlugin nub, as appropriate
	if (hasPMon || hasPPlugin) {
		OSDictionary *dict = OSDictionary::withCapacity(2);
		
		if (dict) {
			const OSSymbol *nameKey, *compatKey, *nameValueSymbol;
			const OSData *nameValueData, *compatValueData, *pHandle;
			char tmpName[32], tmpCompat[128];
			
            nameKey = OSSymbol::withCStringNoCopy("name");
            compatKey = OSSymbol::withCStringNoCopy("compatible");
            if ( hasPPlugin ) {
                // Create MacRISC4 style PlatformPlugin nub
                const OSString	*modelString;
                const OSSymbol	*modelKey;
                const OSSymbol	*pHandleKey;
                
                modelString = OSString::withCString( provider_name );
                modelKey = OSSymbol::withCStringNoCopy("model");
                dict->setObject (modelKey, modelString);
                modelString->release();
                modelKey->release();
                
                // Add AAPL,phandle for "platform-" function use
                pHandleKey = OSSymbol::withCStringNoCopy("AAPL,phandle");
                if (pHandle = OSDynamicCast (OSData, provider->getProperty (pHandleKey)))
                    dict->setObject (pHandleKey, pHandle);
                pHandleKey->release();

                strncpy (tmpName, "IOPlatformPlugin", sizeof( "IOPlatformPlugin" ) );
                if (( pMonPlatformNumber == kPB56MachineModel ) ||
                    ( pMonPlatformNumber == kPB57MachineModel ) ||
                    ( pMonPlatformNumber == kPB58MachineModel ) ||
                    ( pMonPlatformNumber == kPB59MachineModel )) {
                    strncpy (tmpCompat, "PBG4", sizeof( "PBG4" ) );
                } else
                    strncpy (tmpCompat, "MacRISC4", sizeof( "MacRISC4" ) );		// Generic plugin
                strncat (tmpCompat, "_PlatformPlugin", sizeof( "_PlatformPlugin" ) );
           } else {
                strncpy(tmpName, "IOPlatformMonitor", sizeof( "IOPlatformMonitor" ));
    
                if ( pMonPlatformNumber == kPB51MachineModel )
                {
                    strncpy (tmpCompat, "PB5_1", sizeof( "PB5_1" ) );
                }
                else if (( pMonPlatformNumber == kPB52MachineModel ) ||
                        ( pMonPlatformNumber == kPB53MachineModel ))
                {
                    strncpy (tmpCompat, "Portable2003", sizeof( "Portable2003" ) );
                }
                else if (( pMonPlatformNumber == kPB54MachineModel ) ||
                        ( pMonPlatformNumber == kPB55MachineModel ) ||
                        ( pMonPlatformNumber == kPB56MachineModel ) ||
                        ( pMonPlatformNumber == kPB57MachineModel ))
                {
                    strncpy (tmpCompat, "Portable2004", sizeof( "Portable2004" ) );
                }
                else 
                    strncpy (tmpCompat, "Portable", sizeof( "Portable" ) );
                
                strncat (tmpCompat, "_PlatformMonitor", sizeof( "_PlatformMonitor" ) );
            }
            
            nameValueSymbol = OSSymbol::withCString(tmpName);
            nameValueData = OSData::withBytes(tmpName, strlen(tmpName)+1);
            dict->setObject (nameKey, nameValueData);
                                               
			compatValueData = OSData::withBytes(tmpCompat, strlen(tmpCompat)+1);
			dict->setObject (compatKey, compatValueData);
			if (ioPMonNub = IOPlatformExpert::createNub (dict)) {
				if (!ioPMonNub->attach( this ))
					IOLog ("NUB ATTACH FAILED\n");
				ioPMonNub->setName (nameValueSymbol);

				ioPMonNub->registerService();
			}
			dict->release();
			nameValueSymbol->release();
			nameKey->release();
			nameValueData->release();
			compatKey->release();
			compatValueData->release();
            
            // Propagate platform- function properties to plugin nub
            // NOTE - this assumes *all* such properties need to be moved to the plugin nub
            //   as the properties in our nub will get deleted
            if (hasPPlugin) {
                OSDictionary			*propTable = NULL;
                OSCollectionIterator	*propIter = NULL;
                OSSymbol				*propKey;
                OSData					*propData;

                if ( ((propTable = provider->dictionaryWithProperties()) == 0) ||
                    ((propIter = OSCollectionIterator::withCollection(propTable)) == 0) ) {
                    if (propTable) propTable->release();
                    propTable = NULL;
                }

                if (propTable) {
                    while ((propKey = OSDynamicCast(OSSymbol, propIter->getNextObject())) != 0) {
                        if (strncmp(kFunctionRequiredPrefix,		// Check for "platform-"
                            propKey->getCStringNoCopy(),
                            strlen(kFunctionRequiredPrefix)) == 0) {
                            
                            if (strncmp(kFunctionProvidedPrefix,	// Check for "platform-do"
                                propKey->getCStringNoCopy(),
                                strlen(kFunctionProvidedPrefix)) == 0) continue; // Don't copy "platform-do"s
                                
                            propData = OSDynamicCast(OSData, propTable->getObject(propKey));
                            if (propData) {
                                if (ioPMonNub->setProperty (propKey, propData))
                                    // Successfully copied to plugin nub so remove our copy
                                    provider->removeProperty (propKey);
                            }
                        } else {
                            if (strcmp("cpu-vcore-control",		// Same for "cpu-vcore-control"
                                propKey->getCStringNoCopy()) == 0) {
                                                                    
                                propData = OSDynamicCast(OSData, propTable->getObject(propKey));
                                if (propData) {
                                    if (ioPMonNub->setProperty (propKey, propData))
                                        // Successfully copied to plugin nub so remove our copy
                                        provider->removeProperty (propKey);
                                }
                            }
                        }
                    }
                }
                if (propTable) propTable->release();
                if (propIter) propIter->release();
                
                // If the plugin needs to support PowerPlay tell it so.
                if ( processorSpeedChangeFlags & kSupportsPowerPlay )
                    ioPMonNub->setProperty ("UsePowerPlay", kOSBooleanTrue);
            }
		}
    }

    
	// Init power monitor states.  This should be driven by data in the device-tree
	if (doPlatformPowerMonitor)
    {
		bitsSet = kIOPMACInstalled | kIOPMACnoChargeCapability;
		bitsClear = 0;
		powerMonWeakCharger.bitsXor = bitsSet & ~bitsClear;
		powerMonWeakCharger.bitsMask = bitsSet | bitsClear;
		
		bitsSet = kIOPMRawLowBattery;
		bitsClear = 0;
		powerMonBatteryWarning.bitsXor = bitsSet & ~bitsClear;
		powerMonBatteryWarning.bitsMask = bitsSet | bitsClear;
		
		bitsSet = kIOPMBatteryDepleted;
		bitsClear = 0;
		powerMonBatteryDepleted.bitsXor = bitsSet & ~bitsClear;
		powerMonBatteryDepleted.bitsMask = bitsSet | bitsClear;
		
		bitsSet = 0;
		bitsClear = kIOPMBatteryInstalled;
		powerMonBatteryNotInstalled.bitsXor = bitsSet & ~bitsClear;
		powerMonBatteryNotInstalled.bitsMask = bitsSet | bitsClear;
		
		if ((stepType & 1) == 0) {
			bitsSet = kIOPMClosedClamshell;
			bitsClear = 0;
			powerMonClamshellClosed.bitsXor = bitsSet & ~bitsClear;
			powerMonClamshellClosed.bitsMask = bitsSet | bitsClear;
		} else {	// Don't do anything on clamshell closed
			powerMonClamshellClosed.bitsXor = 0;
			powerMonClamshellClosed.bitsMask = ~0L;
		}
	} else { // Assume no power monitoring
		powerMonWeakCharger.bitsXor = 0;
		powerMonWeakCharger.bitsMask = ~0L;
		powerMonBatteryWarning.bitsXor = 0;
		powerMonBatteryWarning.bitsMask = ~0L;
		powerMonBatteryDepleted.bitsXor = 0;
		powerMonBatteryDepleted.bitsMask = ~0L;
		powerMonBatteryNotInstalled.bitsXor = 0;
		powerMonBatteryNotInstalled.bitsMask = ~0L;
		powerMonClamshellClosed.bitsXor = 0;
		powerMonClamshellClosed.bitsMask = ~0L;
	}

    return result;
}

// **********************************************************************************
//
//   determinePlatformNumber
//
//   I realized there were at least three places in the Platform Monitor code where
//   the same string comparisons were being done to determine which machine we were
//	 running on.  Goal of this routine is to get it down to one.  It all revolves
//	 around a single instance variable named 'pMonPlatformNumber'
//
//	 pMonPlatformNumber == 0	 		- Means this code has not yet run or the machine
//								          is not one Platform Expert knows about.
//	 pMonPlatformNumber == other 		- Encoded UInt32 machine representation.
//
// **********************************************************************************
void MacRISC2PE::determinePlatformNumber( void )
{
	if	(!strcmp(provider_name, "PowerBook5,1")) pMonPlatformNumber = kPB51MachineModel;
    else if	(!strcmp(provider_name, "PowerBook5,2")) pMonPlatformNumber = kPB52MachineModel;
    else if	(!strcmp(provider_name, "PowerBook5,3")) pMonPlatformNumber = kPB53MachineModel;
    else if	(!strcmp(provider_name, "PowerBook5,4")) pMonPlatformNumber = kPB54MachineModel;
    else if	(!strcmp(provider_name, "PowerBook5,5")) pMonPlatformNumber = kPB55MachineModel;
    else if	(!strcmp(provider_name, "PowerBook5,6")) pMonPlatformNumber = kPB56MachineModel;
    else if	(!strcmp(provider_name, "PowerBook5,7")) pMonPlatformNumber = kPB57MachineModel;
    else if	(!strcmp(provider_name, "PowerBook5,8")) pMonPlatformNumber = kPB58MachineModel;
    else if	(!strcmp(provider_name, "PowerBook5,9")) pMonPlatformNumber = kPB59MachineModel;
    else if	(!strcmp(provider_name, "PowerBook6,1")) pMonPlatformNumber = kPB61MachineModel;
    else if	(!strcmp(provider_name, "PowerBook6,2")) pMonPlatformNumber = kPB62MachineModel;
    else if	(!strcmp(provider_name, "PowerBook6,3")) pMonPlatformNumber = kPB63MachineModel;
    else if (!strcmp(provider_name, "PowerBook6,4")) pMonPlatformNumber = kPB64MachineModel;
    else if	(!strcmp(provider_name, "PowerBook6,5")) pMonPlatformNumber = kPB65MachineModel;
    else if	(!strcmp(provider_name, "PowerBook6,6")) pMonPlatformNumber = kPB66MachineModel;
    else if	(!strcmp(provider_name, "PowerBook6,7")) pMonPlatformNumber = kPB67MachineModel;
    else if	(!strcmp(provider_name, "PowerBook6,8")) pMonPlatformNumber = kPB68MachineModel;
    else pMonPlatformNumber = 0;
}

IORegistryEntry * MacRISC2PE::retrievePowerMgtEntry (void)
{
    IORegistryEntry *     theEntry = 0;
    IORegistryEntry *     anObj = 0;
    IORegistryIterator *  iter;
    OSString *            powerMgtNodeName;

    iter = IORegistryIterator::iterateOver (IORegistryEntry::getPlane(kIODeviceTreePlane), kIORegistryIterateRecursively);
    if (iter)
    {
        powerMgtNodeName = OSString::withCString("power-mgt");
        anObj = iter->getNextObject ();
        while (anObj)
        {
            if (anObj->compareName(powerMgtNodeName))
            {
                theEntry = anObj;
                break;
            }
            anObj = iter->getNextObject();
        }
    
        powerMgtNodeName->release();
        iter->release ();
    }

    return theEntry;
}

bool MacRISC2PE::platformAdjustService(IOService *service)
{
    bool           		result;
    
    if (IODTMatchNubWithKeys(service, "kauai-ata"))
	{
        IORegistryEntry 	*devicetreeRegEntry;
        OSData				*tmpData;
            
        devicetreeRegEntry = fromPath("/", gIODTPlane);
        tmpData = OSDynamicCast(OSData, devicetreeRegEntry->getProperty("has-safe-sleep"));
        if (tmpData != 0)
            service->setProperty("has-safe-sleep", (void *) 0, (unsigned int) 0);
    }
    
	/*
		
	this is for 3290321 & 3383856 to patch up audio components of an improper device tree. 
	the following properties need to be removed from any platform that has the
	"has-anded-reset" property it's sound node.
	
	extint-gpio4's	'audio-gpio' property with value of 'line-input-detect'
					'audio-gpio-active-state' property

	gpio5's			'audio-gpio' property with value of 'headphone-mute'
					'audio-gpio-active-state' property

	gpio6's			'audio-gpio' property with value of 'amp-mute'
					'audio-gpio-active-state' property

	gpio11's		'audio-gpio' property with value of 'audio-hw-reset'
					'audio-gpio-active-state' property

	extint-gpio15's	'audio-gpio' property with value of 'headphone-detect'
					'audio-gpio-active-state' property
			
	*/
	
    if(!strcmp(service->getName(), "sound"))
	{
		OSObject			*hasAndedReset;
		const OSSymbol		*audioSymbol = OSSymbol::withCString("audio-gpio");
		const OSSymbol		*activeStateSymbol = OSSymbol::withCString("audio-gpio-active-state");

		IORegistryEntry		*gpioNode, *childNode;
		OSIterator			*childIterator;
		OSData				*tmpOSData;

		hasAndedReset = service->getProperty("has-anded-reset", gIODTPlane);
		
		if(hasAndedReset)
		{
			gpioNode = IORegistryEntry::fromPath("mac-io/gpio", gIODTPlane);
			if(gpioNode)
			{
				childIterator = gpioNode->getChildIterator(gIODTPlane);
				if(childIterator)
				{
					while((childNode = (IORegistryEntry *)(childIterator->getNextObject())) != NULL)
					{
						if(!strcmp(childNode->getName(), "extint-gpio4"))
						{
							tmpOSData = OSDynamicCast(OSData, childNode->getProperty(audioSymbol));
							if(tmpOSData)
							{
								if(!strcmp((const char *)tmpOSData->getBytesNoCopy(), "line-input-detect"))
								{
									childNode->removeProperty(audioSymbol);

									tmpOSData = OSDynamicCast(OSData, childNode->getProperty(activeStateSymbol));
									if(tmpOSData)
									{
										// we don't care what the returned value is, we just need to delete the property
										childNode->removeProperty(activeStateSymbol);
									}
								}
							}
						} 
						
						else if(!strcmp(childNode->getName(), "extint-gpio15"))
						{
							tmpOSData = OSDynamicCast(OSData, childNode->getProperty(audioSymbol));
							if(tmpOSData)
							{
								if(!strcmp((const char *)tmpOSData->getBytesNoCopy(), "headphone-detect"))
								{
									childNode->removeProperty(audioSymbol);

									tmpOSData = OSDynamicCast(OSData, childNode->getProperty(activeStateSymbol));
									if(tmpOSData)
									{
										// we don't care what the returned value is, we just need to delete the property
										childNode->removeProperty(activeStateSymbol);
									}
								}
							}
						}
					
						else if(!strcmp(childNode->getName(), "gpio5"))
						{
							tmpOSData = OSDynamicCast(OSData, childNode->getProperty(audioSymbol));
							if(tmpOSData)
							{
								if(!strcmp((const char *)tmpOSData->getBytesNoCopy(), "headphone-mute"))
								{
									childNode->removeProperty(audioSymbol);

									tmpOSData = OSDynamicCast(OSData, childNode->getProperty(activeStateSymbol));
									if(tmpOSData)
									{
										// we don't care what the returned value is, we just need to delete the property
										childNode->removeProperty(activeStateSymbol);
									}
								}
							}
						} 
						
						else if(!strcmp(childNode->getName(), "gpio6"))
						{
							tmpOSData = OSDynamicCast(OSData, childNode->getProperty(audioSymbol));
							if(tmpOSData)
							{
								if(!strcmp((const char *)tmpOSData->getBytesNoCopy(), "amp-mute"))
								{
									childNode->removeProperty(audioSymbol);

									tmpOSData = OSDynamicCast(OSData, childNode->getProperty(activeStateSymbol));
									if(tmpOSData)
									{
										// we don't care what the returned value is, we just need to delete the property
										childNode->removeProperty(activeStateSymbol);
									}
								}
							}
						} 
						
						else if(!strcmp(childNode->getName(), "gpio11"))
						{
							tmpOSData = OSDynamicCast(OSData, childNode->getProperty(audioSymbol));
							if(tmpOSData)
							{
								if(!strcmp((const char *)tmpOSData->getBytesNoCopy(), "audio-hw-reset"))
								{
									childNode->removeProperty(audioSymbol);

									tmpOSData = OSDynamicCast(OSData, childNode->getProperty(activeStateSymbol));
									if(tmpOSData)
									{
										// we don't care what the returned value is, we just need to delete the property
										childNode->removeProperty(activeStateSymbol);
									}
								}
							}
						}
					}

                    childIterator->release();
				
				} else
				{
					IOLog("MacRISC2PE::platformAdjustService ERROR - could not find childIterator\n");
					return false;
				}
			
			} else
			{
				IOLog("MacRISC2PE::platformAdjustService ERROR - could not find gpioNode\n");
				return false;
			}
		}
 
		audioSymbol->release();
		activeStateSymbol->release();
		return true;
	}

    if (IODTMatchNubWithKeys(service, "cpu"))
    {
		// Create a "cpu-device-type" property and populate it with "MacRISC2CPU".
        // This allows us to be more specific about which PE_*CPU object IOKit ends
        // up matching the "cpu" node(s) with.  Previously it was matching on an
        // IONameMatch of "cpu" which matches against every Mac we make.  This is
        // much more selective. There is a corresponding change in the project file
        // that does an IOPropertyMatch against this property with this value so
        // that only MacRISC2CPUs will match the MacRISC2CPU object/class.
		service->setProperty ("cpu-device-type", "MacRISC2CPU");
        
        if (i2ResetOnWake)
            service->setProperty ("reset-on-wake", kOSBooleanTrue);

        return true;
    }
    
    if (IODTMatchNubWithKeys(service, "open-pic"))
    {
	const OSSymbol	* keySymbol;
	OSSymbol 	* tmpSymbol;

        keySymbol = OSSymbol::withCStringNoCopy("InterruptControllerName");
        tmpSymbol = (OSSymbol *)IODTInterruptControllerName(service);
        result    = service->setProperty(keySymbol, tmpSymbol);
        return true;
    }

    if (!strcmp(service->getName(), "programmer-switch"))
    {
        // Set property to tell AppleNMI to mask/unmask NMI @ sleep/wake
        service->setProperty("mask_NMI", service); 
        return true;
    }
  
    if (!strcmp(service->getName(), "pmu"))
    {
        // Change the interrupt mapping for pmu source 4.
        OSArray              *tmpArray, *tmpArrayCopy;
        OSCollectionIterator *extIntList;
        IORegistryEntry      *extInt;
        OSObject             *extIntControllerName;
        OSObject             *extIntControllerData;
        OSString			 *platformModel;
    
        // Set the no-nvram property.
        service->setProperty("no-nvram", service);
    
        //  [4037930] Set the platform-model property for future updates
        platformModel = OSString::withCString (provider_name);
        if (platformModel) {
            service->setProperty("platform-model", platformModel);
            platformModel->release();
        }
        
        // Find the new interrupt information.
        extIntList = IODTFindMatchingEntries(getProvider(), kIODTRecursive, "'extint-gpio1'");
        extInt = (IORegistryEntry *)extIntList->getNextObject();
    
        tmpArray = (OSArray *)extInt->getProperty(gIOInterruptControllersKey);
        extIntControllerName = tmpArray->getObject(0);
        tmpArray = (OSArray *)extInt->getProperty(gIOInterruptSpecifiersKey);
        extIntControllerData = tmpArray->getObject(0);
    
        // Replace the interrupt infomation for pmu source 4.
        tmpArray = (OSArray *)service->getProperty(gIOInterruptControllersKey);
        tmpArrayCopy = (OSArray *)tmpArray->copyCollection();           // Make a copy so we can modify it outside the IORegistry
        tmpArrayCopy->replaceObject(4, extIntControllerName);
        service->setProperty(gIOInterruptControllersKey, tmpArrayCopy); // Put it back in registry
        tmpArrayCopy->release();
        
        tmpArray = (OSArray *)service->getProperty(gIOInterruptSpecifiersKey);
        tmpArrayCopy = (OSArray *)tmpArray->copyCollection();           // Make a copy so we can modify it outside the IORegistry
        tmpArrayCopy->replaceObject(4, extIntControllerData);
        service->setProperty(gIOInterruptSpecifiersKey, tmpArrayCopy);  // Put it back in registry
        tmpArrayCopy->release();
  
        extIntList->release();
        
        return true;
    }

    if (!strcmp(service->getName(), "via-pmu"))
    {
        service->setProperty("BusSpeedCorrect", this);
        return true;
    }
    
	/*
	 * For uni-n pci version 1.5 which services mac-io, tell pci driver to disable read data gating
	 * -- Note that previous versions of this driver defined version 1.5 as 0x0010.  This is not 
	 * correct and we now use 0x0011 for uni-n 1.5.
	 */
	if ((uniNVersion == kUniNVersion150) && IODTMatchNubWithKeys(service, "('pci', 'uni-north')") && 
		(service->childFromPath("mac-io", gIODTPlane) != NULL)) {
			service->setProperty ("DisableRDG", true);
			return true;
	}
    
	/* 
	 * For Firewire on uni-n Pangea & uni-n 1.5 through 2.0, adjust the cache line size and timer latency
	 */
	if (((uniNVersion >= kUniNVersion150) && (uniNVersion <= kUniNVersion200) || (uniNVersion == kUniNVersionPangea)) &&
		(!strcmp(service->getName(gIODTPlane), "firewire")) &&
		IODTMatchNubWithKeys(service->getParentEntry(gIODTPlane), "('pci', 'uni-north')")) {
			char data;
			
			data = 0x08;		// 32 byte cache line
			service->setProperty (kIOPCICacheLineSize, &data, 1);
			data = 0x40;		// latency timer to 40
			service->setProperty (kIOPCITimerLatency, &data, 1);
			return true;
	}
    
	/* 
	 * For Ethernet on uni-n 2.0, adjust the cache line size and timer latency
	 */
	if ((uniNVersion == kUniNVersion200) &&
		IODTMatchNubWithKeys(service, "('ethernet', 'gmac')") &&
		IODTMatchNubWithKeys(service->getParentEntry(gIODTPlane), "('pci', 'uni-north')")) {
			char 	data;
			long	cacheSize;
			OSData 	*cacheData;
			
			data = 0x08; 	// assume default of 32 byte cache line
			cacheData = OSDynamicCast( OSData, service->getProperty( "cache-line-size" ) );
			if (cacheData) {
				cacheSize = *(long *)cacheData->getBytesNoCopy();
				data = (cacheSize >> 2);
			}
			service->setProperty (kIOPCICacheLineSize, &data, 1);
			data = 0x20;		// latency timer to 20
			service->setProperty (kIOPCITimerLatency, &data, 1);
			return true;
	}
	
	// For usb@18 on PowerBook4,x machines add an "AAPL,SuspendablePorts" property if it doesn't exist
	if (!strcmp(service->getName(), "usb") && 
		(0 == strncmp(provider_name, "PowerBook4,", strlen("PowerBook4,"))) &&
		IODTMatchNubWithKeys(service->getParentEntry(gIODTPlane), "('pci', 'uni-north')")) {
		
		OSData				*regProp;
		IOPCIAddressSpace	*pciAddress;
		UInt32				ports;
	
		if( (regProp = (OSData *) service->getProperty("reg"))) {
			pciAddress = (IOPCIAddressSpace *) regProp->getBytesNoCopy();
			// Only for usb@18
			if (pciAddress->s.deviceNum == 0x18) {
				// If property doesn't exist, create it
				if(!((OSData *) service->getProperty(kAAPLSuspendablePorts))) {
					ports = 4;
					service->setProperty (kAAPLSuspendablePorts, &ports, sizeof(UInt32));
				}
				return true;
			}
		}
	}

	// for P69s (Xserves) with early BootROMs, the 'temp-monitor' node has an incorrect "reg" property
	// -- change the "reg" property to be correct.  ALL P69s, regardless of BootROM, have the
	//    'temp-monitor' I2C device at I2C address 0x92.  It is incorrectly set to 0x192 in early
	//    BootROMs, and causes the AppleCPUThermo driver to attempt to access an invalid device
	//    address.  The failure of that access causes hwmond to not return temperature information
	//    about the CPU to Server Monitor.
	if ( ( strcmp( "temp-monitor", service->getName() ) == 0 ) &&
	     ( strcmp( "RackMac1,1"  , provider_name      ) == 0 ) )    // ALL P69s, regardless of BootROM
	{
	OSData  * deviceType;
	UInt32  newRegPropertyValue;

		deviceType = OSDynamicCast( OSData, service->getProperty( "device_type" ) );
		if ( deviceType && ( strcmp( "ds1775", (char *)deviceType->getBytesNoCopy() ) == 0 ) )
		{
			// if the service is 'temp-monitor' and we are on a P69, and we have verified
			// that the 'temp-monitor' device is of type 'ds1775', go ahead and adjust the
			// "reg" property to be 0x00000092 (bus 0, device address 0x92).
			newRegPropertyValue = 0x92;
			service->setProperty( "reg", &newRegPropertyValue, sizeof( UInt32 ) );

			// return true;             // handled by the default 'return true' at the bottom of the routine
		}
	}

	// [3583001] P62 clock drift - add a special flag to eMac's i2c-hwclock nodes. The
	// AppleEMacClock driver loads early (OSBundleRequired = Root) and looks for this.
	// This is really only necessary on P62, but P86 has the same model property. It will
	// not have a detrimental effect on P86.
	if ( ( strcmp( "i2c-hwclock", service->getName() ) == 0 ) &&
	     ( strcmp( "PowerMac4,4"  , provider_name      ) == 0 ) )    // UniNorth/KeyLargo-based eMacs P62,P86
	{
		service->setProperty( "emac-clock", true );
	}

    if (i2ResetOnWake && !strcmp( "IOHWSensor", service->getName())) {
        OSData  *deviceType;
        
    	// Remember this driver for use by CPU driver
        deviceType = OSDynamicCast (OSData, service->getParentEntry(gIOServicePlane)->getProperty ("device_type"));
        if (deviceType && !strncmp ("gpu-sensor", (const char *)deviceType->getBytesNoCopy(), strlen ("gpu-sensor"))) {
            gpuSensor = service;    // Keep a reference for use by cpu driver
            return true;
        }
    }

    if (i2ResetOnWake && !strcmp( "ATY,JasperParent", service->getName())) {
    	// Remember this driver for use by CPU driver
        atiDriver = OSDynamicCast (IOAGPDevice, service);
        if (atiDriver) 
        	// Remember it's parent as well
        	agpBridgeDriver = OSDynamicCast (IOPCIBridge, atiDriver->getParentEntry (gIOServicePlane));
 
 		return true;
    }
    
	return true;
}

IOReturn MacRISC2PE::callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
					void *param1, void *param2,
					void *param3, void *param4)
{
    if (functionName == gGetDefaultBusSpeedsKey)
    {
        getDefaultBusSpeeds((long *)param1, (unsigned long **)param2);
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("EnableUniNEthernetClock"))
    {
        enableUniNEthernetClock((bool)param1, (IOService *)param2);
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo("EnableFireWireClock")) {
        enableUniNFireWireClock((bool)param1, (IOService *)param2);
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("EnableFireWireCablePower")) {
        enableUniNFireWireCablePower((bool)param1);
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("AccessUniN15PerformanceRegister"))
    {
        return accessUniN15PerformanceRegister((bool)param1, (long)param2, (unsigned long *)param3);
    }
  
    if (functionName->isEqualTo("PlatformIsPortable")) {
		*(bool *) param1 = isPortable;
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("PlatformPowerMonitor")) {
        if (hasPPlugin)
            return platformPluginPowerMonitor ((UInt32 *) param1);
        else
            return platformPowerMonitor ((UInt32 *) param1);
    }
	
    if (functionName->isEqualTo("PerformPMUSpeedChange")) {
		if (!macRISC2CPU)
			macRISC2CPU = OSDynamicCast (MacRISC2CPU, waitForService (serviceMatching("MacRISC2CPU")));
		
		if (macRISC2CPU) {
			macRISC2CPU->performPMUSpeedChange ((UInt32) param1);
			return kIOReturnSuccess;
		}
		
		return kIOReturnUnsupported;
    }

    if (functionName->isEqualTo("IOPMSetSleepSupported")) {
		return slotsMacRISC2->determineSleepSupport ();
    }
      
    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

void MacRISC2PE::getDefaultBusSpeeds(long *numSpeeds, unsigned long **speedList)
{
    if ((numSpeeds == 0) || (speedList == 0)) return;
  
    *numSpeeds = 1;
    *speedList = macRISC2Speed;
}

void MacRISC2PE::enableUniNEthernetClock(bool enable, IOService *nub)
{
	if (!uniN) 
		uniN = OSDynamicCast (AppleUniN, waitForService(serviceMatching("AppleUniN")));

	if (uniN)
		uniN->enableUniNEthernetClock (enable, nub);
	
	return;
}

void MacRISC2PE::enableUniNFireWireClock(bool enable, IOService *nub)
{
	if (!uniN) 
		uniN = OSDynamicCast (AppleUniN, waitForService(serviceMatching("AppleUniN")));

	if (uniN)
		uniN->enableUniNFireWireClock (enable, nub);
	
	return;
}

void MacRISC2PE::enableUniNFireWireCablePower(bool enable)
{
    // Turn off cable power supply on mid/merc/pismo(on pismo only, this kills the phy)

	const OSSymbol *tmpSymbol = OSSymbol::withCString("keyLargo_writeRegUInt8");

    if(getMachineType() == kMacRISC2TypePowerBook)
    {
        IOService *keyLargo;
        keyLargo = waitForService(serviceMatching("KeyLargo"));
        
        if(keyLargo)
        {
            UInt32 gpioOffset = 0x73;
            
            keyLargo->callPlatformFunction(tmpSymbol, true, (void *)&gpioOffset, (void *)(enable ? 0:4), 0, 0);
        }
    }
	
	tmpSymbol->release();
}

IOReturn MacRISC2PE::accessUniN15PerformanceRegister(bool write, long regNumber, unsigned long *data)
{
	if (!uniN) 
		uniN = OSDynamicCast (AppleUniN, waitForService(serviceMatching("AppleUniN")));

	if (uniN)
		return uniN->accessUniN15PerformanceRegister (write, regNumber, data);
	
	return kIOReturnUnsupported;
}


//*********************************************************************************
// platformPluginPowerMonitor
//
// A call platform function call called by the ApplePMU driver.  ApplePMU call us
// with a set of power flags.  We examine those flags and modify the state
// according to the characteristics of the platform. 
//
// If necessary, we force an immediate change in the power state
// This call is only used for MacRISC4 style plugins.  PlatformMonitor plugins.  
// use the platformPowerMonitor call
//*********************************************************************************
IOReturn MacRISC2PE::platformPluginPowerMonitor(UInt32 *powerFlags)
{
    static UInt32 			i = 0;
    static OSDictionary 	*dict;
    static OSNumber			*powerBits;
    static const OSSymbol	*gIOPPluginCurrentValueKey;
    
	if (!gIOPPluginCurrentValueKey)	gIOPPluginCurrentValueKey		= OSSymbol::withCString (kIOPMonCurrentValueKey);

    if (!dict)
        dict = OSDictionary::withCapacity(2);

    // the first time we're called is on the ApplePMU start thread and we don't
    // want to block it, so just skip the first few events
    if (i < 10) {
        i++;
        return kIOReturnSuccess;
    }

    if ((i == 10) && !ioPPlugin) {
        IOService *serv;
        
        i = 11;
        serv = waitForService(resourceMatching("IOPlatformPlugin"));
        ioPPlugin = OSDynamicCast (IOService, serv->getProperty("IOPlatformPlugin"));
    }

    if (ioPPlugin) {	// If there's an ioPPlugin, use it
        powerBits = OSNumber::withNumber ((long long)*powerFlags, 32);
        dict->setObject (gIOPPluginCurrentValueKey, powerBits);
        powerBits->release();

        // Send the current value to the Platform Plugin
        messageClient (kIOPMonMessagePowerMonitor, ioPPlugin, (void *)dict);

        /*
         * Here we differ from how PlatformMonitor style plugins work.  They
         * would retrieve the (possibly) modified value determined by the plugin
         * and return that to PMU, which would, in turn pass that to the Power
         * Manager.  If the forced reduced speed bit was set (kIOPMForceLowSpeed),
         * this would trigger a setAggressiveness call to reduce the processor speed.
         *
         * With the PlatformPlugin style plugins they will take immediate action,
         * if necessary, to reduce speed.  Nobody else has to be involved.
         *
         * Therefore, we return the value to PMU unmodified, except for clearing
         * the kIOPMForceLowSpeed.
         */
        *powerFlags &= ~kIOPMForceLowSpeed;  		// Clear low speed bit

    }
	i++;
	
	return kIOReturnSuccess;
}

//*********************************************************************************
// platformPowerMonitor
//
// A call platform function call called by the ApplePMU driver.  ApplePMU call us
// with a set of power flags.  We examine those flags and modify the state
// according to the characteristics of the platform. 
//
// If necessary, we force an immediate change in the power state
//
// This call is only used for PlatformMonitor plugins.  MacRISC4 style plugins
// use the platformPluginPowerMonitor call
//*********************************************************************************
IOReturn MacRISC2PE::platformPowerMonitor(UInt32 *powerFlags)
{
	IOReturn	result;

	/*
	 * If we created an ioPMonNub for this platform, then use it
	 */
	if (ioPMonNub) {
		static UInt32 			i = 0;
		static bool 			pmonInited = false;
		static OSDictionary 	*dict;
		static OSNumber			*powerBits;

		// the first time we're called is on the ApplePMU start thread and we don't
		// want to block it, so just skip the first few events
		if (i < 10) {
			i++;
			return kIOReturnSuccess;
		}

		if ((i == 10) && !ioPMon) {
			IOService *serv;
			
			i = 11;
			serv = waitForService(resourceMatching("IOPlatformMonitor"));
			ioPMon = OSDynamicCast (IOPlatformMonitor, serv->getProperty("IOPlatformMonitor"));
		}

		if (ioPMon) {	// If there's an ioPMon, use it
			if (!pmonInited) {
				dict = OSDictionary::withCapacity(2);
				if (!dict) {
					ioPMon = NULL;
				} else {
				
					powerBits = OSNumber::withNumber ((long long)*powerFlags, 32);
					
					dict->setObject (kIOPMonTypeKey, OSSymbol::withCString(kIOPMonTypeClamshellSens));
					dict->setObject (kIOPMonCurrentValueKey, powerBits);
		
					if (messageClient (kIOPMonMessageRegister, ioPMon, (void *)dict) != kIOReturnSuccess) {
						// IOPMon doesn't need to know about us, so don't bother with it
						IOLog ("MacRISC2PE::platformPowerMonitor - failed to register clamshell with IOPlatformMonitor\n");
						dict->release();
						ioPMon = NULL;
						return kIOReturnUnsupported;
					} 
					pmonInited = true;
					return kIOReturnSuccess;
				}
			}
			
			// save the current value into the dictionary
			powerBits->setValue((long long)*powerFlags);
			messageClient (kIOPMonMessagePowerMonitor, ioPMon, (void *)dict);
			// retrieve the value updated by the IOPMon and return to PMU
			*powerFlags = powerBits->unsigned32BitValue();
		}
		i++;
	
		return kIOReturnSuccess;
	}
	
	if (doPlatformPowerMonitor) {
		// First check primary power conditions
		if ((((*powerFlags ^ powerMonWeakCharger.bitsXor) & powerMonWeakCharger.bitsMask) == 0) ||
			(((*powerFlags ^ powerMonBatteryWarning.bitsXor) & powerMonBatteryWarning.bitsMask) == 0) ||
			(((*powerFlags ^ powerMonBatteryDepleted.bitsXor) & powerMonBatteryDepleted.bitsMask) == 0) ||
			(((*powerFlags ^ powerMonBatteryNotInstalled.bitsXor) & powerMonBatteryNotInstalled.bitsMask) == 0)) {
					/*
					 * For these primary power conditions we signal the power manager to force low power state
					 * This includes both reduced processor speed and disabled L3 cache.
					 */
					*powerFlags |= kIOPMForceLowSpeed;
					/*
					 * If we previously speed changed due to a closed clamshell and the L3 cache is still enabled
					 * we must call through to get the L3 cache disabled as well
					 */
					if (processorSpeedChangeFlags & kL3CacheEnabled) {
						if (!macRISC2CPU)
							macRISC2CPU = OSDynamicCast (MacRISC2CPU, waitForService (serviceMatching("MacRISC2CPU")));
						if (macRISC2CPU) {
							processorSpeedChangeFlags &= ~kClamshellClosedSpeedChange;
							macRISC2CPU->setAggressiveness (kPMSetProcessorSpeed, 1); // Force slow now so cache state is right
						}
					}
		} else if (((*powerFlags ^ powerMonClamshellClosed.bitsXor) & powerMonClamshellClosed.bitsMask) == 0) {
				/*
				 * clamShell closed with no other power conditions is a special case --
				 * leave L3 cache enabled
				 */
				*powerFlags |= kIOPMForceLowSpeed;
				
				if (!(processorSpeedChangeFlags & kL3CacheEnabled)) {
					if (!macRISC2CPU)
						macRISC2CPU = OSDynamicCast (MacRISC2CPU, waitForService (serviceMatching("MacRISC2CPU")));
					if (macRISC2CPU) {
						if (processorSpeedChangeFlags & kPMUBasedSpeedChange) {
							// Only want setAggressiveness to enable cache
							processorSpeedChangeFlags &= ~kPMUBasedSpeedChange;
							macRISC2CPU->setAggressiveness (kPMSetProcessorSpeed, 0); // Force fast now so cache state is right
							processorSpeedChangeFlags |= kPMUBasedSpeedChange;
						}
					}
				}
				processorSpeedChangeFlags |= kClamshellClosedSpeedChange;	// Show clamshell state
		} else {
			/*
			 * No low power conditions exist, clear all flags
			 */
			*powerFlags &= ~kIOPMForceLowSpeed;
			processorSpeedChangeFlags &= ~kClamshellClosedSpeedChange;
		}

		result = kIOReturnSuccess;
	} else
		result = kIOReturnUnsupported;		// Not supported on this platform
		
    return result;
}

//*********************************************************************************
// PMInstantiatePowerDomains
//
// This overrides the vanilla implementation in IOPlatformExpert.  It instantiates
// a root domain with two children, one for the USB bus (to handle the USB idle
// power budget), and one for the expansions slots on the PCI bus (to handle
// the idle PCI power budget)
//*********************************************************************************

void MacRISC2PE::PMInstantiatePowerDomains ( void )
{    
	const OSSymbol 			*desc = OSSymbol::withCString("powertreedesc");
    IOPMUSBMacRISC2 		*usbMacRISC2;
    IORegistryEntry 		*devicetreeRegEntry;
    OSData					*tmpData;
    OSArray                 *tmpArray;

	// Move our power tree description from our driver (where it's a property in the driver)
	// to our provider
	kprintf ("MacRISC2PE::PMInstantiatePowerDomains - getting pmtree property\n");
    tmpArray = OSDynamicCast(OSArray, getProperty(desc));
    
    if (tmpArray)
        thePowerTree = (OSArray *)tmpArray->copyCollection ();
    else
        thePowerTree = NULL;

    if( 0 == thePowerTree)
    {
        kprintf ("error retrieving power tree\n");
		return;
    }
	kprintf ("MacRISC2PE::PMInstantiatePowerDomains - got pmtree property\n");

    //getProvider()->setProperty (desc, thePowerTree);
	
	// No need to keep original around
	removeProperty(desc);
    		
    root = IOPMrootDomain::construct();

    if (NULL == root)
    {
        kprintf ("PMInstantiatePowerDomains - null ROOT\n");
        return;
    }

    root->attach(this);
    root->start(this);

    root->setSleepSupported(kRootDomainSleepSupported);
    
    devicetreeRegEntry = fromPath("/", gIODTPlane);
    tmpData = OSDynamicCast(OSData, devicetreeRegEntry->getProperty("has-safe-sleep"));
    if (tmpData != 0)
        root->publishFeature(kIOHibernateFeatureKey);

    PMRegisterDevice (NULL, root);

    usbMacRISC2 = new IOPMUSBMacRISC2;
    if (usbMacRISC2)
    {
        usbMacRISC2->init ();
        usbMacRISC2->attach (this);
        usbMacRISC2->start (this);
        PMRegisterDevice (root, usbMacRISC2);
    }

    slotsMacRISC2 = new IOPMSlotsMacRISC2;
    if (slotsMacRISC2)
    {
        slotsMacRISC2->init ();
        slotsMacRISC2->attach (this);
        slotsMacRISC2->start (this);
        PMRegisterDevice (root, slotsMacRISC2);
    }

    // MacRISC4 style plugins are responsible for publishing these features themselves
    if (!hasPPlugin) {
        if (processorSpeedChangeFlags != kNoSpeedChange) {
            // Any system that support Speed change supports Reduce Processor Speed.
            root->publishFeature("Reduce Processor Speed");
            
            // Enable Dynamic Power Step for low latency systems.
            if (processorSpeedChangeFlags & kProcessorBasedSpeedChange) {
                root->publishFeature("Dynamic Power Step");
            }
        }
    }
    
    return;
}


//*********************************************************************************
// PMRegisterDevice
//
// This overrides the vanilla implementation in IOPlatformExpert.  We try to 
// put a device into the right position within the power domain hierarchy.
//*********************************************************************************
extern const IORegistryPlane * gIOPowerPlane;

void MacRISC2PE::PMRegisterDevice(IOService * theNub, IOService * theDevice)
{
    bool            nodeFound  = false;
    IOReturn        err        = -1;
    OSData *	    propertyPtr = 0;
    const char *    theProperty;

    // Starts the protected area, we are trying to protect numInstancesRegistered
    if (mutex != NULL)
      IOLockLock(mutex);
     
    // reset our tracking variables before we check the XML-derived tree
    multipleParentKeyValue = NULL;
    numInstancesRegistered = 0;

    // try to find a home for this registrant in our XML-derived tree
    nodeFound = CheckSubTree (thePowerTree, theNub, theDevice, NULL);

    if (0 == numInstancesRegistered)
    {
        // make sure the provider is within the Power Plane...if not, 
        // back up the hierarchy until we find a grandfather or great
        // grandfather, etc., that is in the Power Plane.

        while( theNub && (!theNub->inPlane(gIOPowerPlane)))
            theNub = theNub->getProvider();
    }

    // Ends the protected area, we are trying to protect numInstancesRegistered
    if (mutex != NULL)
       IOLockUnlock(mutex);
     
    // try to register with the given (or reassigned in the case above) provider.
    if ( NULL != theNub )
        err = theNub->addPowerChild (theDevice);

    // failing that then register with root (but only if we didn't register in the 
    // XML-derived tree and only if the device we're registering is not the root).
    if ((err != IOPMNoErr) && (0 == numInstancesRegistered) && (theDevice != root)) {
        root->addPowerChild (theDevice);
    }

    // in addition, if it's in a PCI slot, give it to the Aux Power Supply driver
    
    propertyPtr = OSDynamicCast(OSData,theDevice->getProperty("AAPL,slot-name"));
    if ( propertyPtr ) {
        theProperty = (const char *) propertyPtr->getBytesNoCopy();
        	if ( strncmp("SLOT-",theProperty,5) == 0 ) {
            	slotsMacRISC2->addPowerChild (theDevice);
			}
    }
}

