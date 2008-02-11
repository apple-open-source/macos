/*
 * Copyright (c) 2002-2007 Apple Inc. All rights reserved.
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
 * Copyright (c) 2002-2007 Apple Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */


#include <sys/cdefs.h>

__BEGIN_DECLS
#include <ppc/proc_reg.h>

/* Map memory map IO space */
#include <mach/mach_types.h>
__END_DECLS

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include "MacRISC4PE.h"
//#include <IOKit/pci/IOPCIDevice.h>

static unsigned long macRISC4Speed[] = { 0, 1 };

#include <IOKit/pwr_mgt/RootDomain.h>
//XXX-#include "IOPMSlotsMacRISC4.h"
//XXX-#include "IOPMUSBMacRISC4.h"
//#include <IOKit/pwr_mgt/IOPMPagingPlexus.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include <pexpert/pexpert.h>

extern char *gIOMacRISC4PMTree;

#ifndef kIOHibernateFeatureKey
#define kIOHibernateFeatureKey	"Hibernation"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super ApplePlatformExpert

OSDefineMetaClassAndStructors(MacRISC4PE, ApplePlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool MacRISC4PE::start(IOService *provider)
{
    long            		machineType;
	char					tmpName[32];
    OSData          		*tmpData;
	IORegistryEntry 		*uniNRegEntry,
							*spuRegEntry,
							*powerMgtEntry,
							* aCPURegEntry;
    UInt32			   		*primInfo;
	const OSSymbol			*nameValueSymbol;
	const OSData			*nameValueData;
	OSDictionary			*pluginDict, *platFuncDict;
	bool					result, spuNeedsRamFix;
	
	kprintf ("MacRISC4PE::start - entered\n");
	
    setChipSetType(kChipSetTypeCore2001);
	
    // Set the machine type.
    provider_name = provider->getName();  

	machineType = kMacRISC4TypeUnknown;
	if (provider_name != NULL) {
		if (0 == strncmp(provider_name, "PowerMac", strlen("PowerMac")))
			machineType = kMacRISC4TypePowerMac;
		else if (0 == strncmp(provider_name, "RackMac", strlen("RackMac")))
			machineType = kMacRISC4TypePowerMac;
		else if (0 == strncmp(provider_name, "PowerBook", strlen("PowerBook")))
			machineType = kMacRISC4TypePowerBook;
		else if (0 == strncmp(provider_name, "iBook", strlen("iBook")))
			machineType = kMacRISC4TypePowerBook;
		else	// kMacRISC4TypeUnknown
			IOLog ("AppleMacRISC4PE - warning: unknown machineType\n");
	}
	
	isPortable = (machineType == kMacRISC4TypePowerBook);
	
    setMachineType(machineType);
	
    // Get the bus speed from the Device Tree.
    tmpData = OSDynamicCast(OSData, provider->getProperty("clock-frequency"));
    if (tmpData == 0) {
		kprintf ("MacRISC4PE::start - no clock-frequency property\n");
		return false;
	}
    macRISC4Speed[0] = *(unsigned long *)tmpData->getBytesNoCopy();

	spuNeedsRamFix = true;		// Assume we don't have the latest SPU
	
	spuRegEntry = provider->childFromPath("spu", gIODTPlane);
	if (spuRegEntry) {
		const UInt32 kProjectPhaseMask = 0xFFFFFF0F; // mask out the a,b,d,f phases since d<a<b<f. 
		const UInt32 kConvertDevelopmentMask = 0xFFFFFF9F; // Used to convert DX to 9X, EX to 8X. 
		const UInt32 kDevelopmentPhase = 0xD0; 
		const UInt32 kEngineeringBuild = 0xE0; 
		OSData *spuVersionData;
		UInt32	minSPUVersion, currentSPUVersion;
		UInt32 tempCurrent, tempMin; 
		
		minSPUVersion = 0x111f1;		// 1.1.1f1
		spuVersionData = OSDynamicCast (OSData, spuRegEntry->getProperty ("version"));
		if (spuVersionData) {
			currentSPUVersion = ((unsigned long *)spuVersionData->getBytesNoCopy())[0];
		
			// Convert versions into numerically compareable values
			if ((kDevelopmentPhase == (currentSPUVersion & ~kProjectPhaseMask)) ||  
				(kEngineeringBuild == (currentSPUVersion & ~kProjectPhaseMask))) 
					tempCurrent = currentSPUVersion & kConvertDevelopmentMask; 
			else tempCurrent = currentSPUVersion; 
			
			if ((kDevelopmentPhase == (minSPUVersion & ~kProjectPhaseMask)) || 
				(kEngineeringBuild == (minSPUVersion & ~kProjectPhaseMask))) 
					tempMin = minSPUVersion & kConvertDevelopmentMask; 
			else tempMin = minSPUVersion; 
			
			if (tempMin <= tempCurrent)
				spuNeedsRamFix = false;		// SPU is updated - don't need to do workaround
			
			spuRegEntry->release();
		}
	} else
		spuNeedsRamFix = false;		// No SPU, nothing to fix

	if (spuNeedsRamFix)
	{
	OSData			* cpu0FreqData = NULL;
	UInt32			cpu0Freq = 0;
	int				cpuCount;
	const UInt32	one_eight_ghz_freq = 1800000000;

		// get uni-N version
		uniNRegEntry = provider->childFromPath("u3", gIODTPlane);
		if ((uniNRegEntry) && (tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("device-rev"))))
		{
			uniNVersion = ((unsigned long *)tmpData->getBytesNoCopy())[0];

		// Count the CPUs
		aCPURegEntry = fromPath ("/cpus/@1", gIODTPlane);
		if (aCPURegEntry) {
			cpuCount = 2;		// do multiple CPUs exist (don't care about the actual number)
			aCPURegEntry->release();
		} else
			cpuCount = 1;
			
		aCPURegEntry = fromPath ("/cpus/@0", gIODTPlane);
		if( aCPURegEntry )
		{
			// get the cpu clock frequency
			cpu0FreqData = OSDynamicCast (OSData, aCPURegEntry->getProperty ("clock-frequency"));
			if(cpu0FreqData) 
				cpu0Freq = *(UInt32 *)cpu0FreqData->getBytesNoCopy();
			aCPURegEntry->release();
    	}
    
			cannotSleep = ((0 == strncmp(provider_name, "PowerMac7,2", strlen("PowerMac7,2")))     // check model
				&& (kUniNRevision3_2_1 == uniNVersion)		// check U3 version 2.1
				&& (cpuCount == 1)							// check uniprocessor
				&& (one_eight_ghz_freq == cpu0Freq) );		// check CPU0 speed = 1.8ghz
		
			if (cannotSleep) setProperty ("PlatformCannotSleep", true);

			uniNRegEntry->release();
		}
	}

    // Get PM features and private features
	// The power-mgt node is being deprecated in favor of specfic properties to describe
	// platform behaviors.  This is here mostly for backward compatibility
    powerMgtEntry = retrievePowerMgtEntry ();

	// If primInfo not defined init _pePMFeatures so we can at least power off PCI
	// Disable this for now as it triggered other, unintended consequences [3802979]
	//_pePMFeatures = kPMCanPowerOffPCIBusMask;
	_pePMFeatures = 0;

	if (powerMgtEntry) {
		tmpData  = OSDynamicCast(OSData, powerMgtEntry->getProperty ("prim-info"));
		if (tmpData != 0) {
			primInfo = (unsigned long *)tmpData->getBytesNoCopy();
			if (primInfo != 0) {
				_pePMFeatures            = primInfo[3];
				_pePrivPMFeatures        = primInfo[4];
				_peNumBatteriesSupported = ((primInfo[6]>>16) & 0x000000FF);
				kprintf ("MacRISC4PE: Public PM Features: %0x.\n",_pePMFeatures);
				kprintf ("MacRISC4PE: Privat PM Features: %0x.\n",_pePrivPMFeatures);
				kprintf ("MacRISC4PE: Num Internal Batteries Supported: %0x.\n", _peNumBatteriesSupported);
			}
		}
	} else kprintf ("MacRISC4PE: no power-mgt information available\n");
	
    // This is to make sure that  is PMRegisterDevice reentrant
    pmmutex = IOLockAlloc();
    if (pmmutex == NULL)
		return false;
    else
		IOLockInit( pmmutex );
	
	// [4043827] Platform Expert needs to advertise the existence of a mapper (DART)
	// requires [4043836] Remove DART hack from iokit kernel 
    IORegistryEntry * regEntry = IORegistryEntry::fromPath("/u3/dart", gIODTPlane);
    if (!regEntry)
		regEntry = IORegistryEntry::fromPath("/u4/dart", gIODTPlane);
    if (regEntry) {
	    setProperty(kIOPlatformMapperPresentKey, kOSBooleanTrue);
		regEntry->release();
    }
	
	//[5376988] AppleMPIC relies on an interrupts property to determine if it's the host MPIC
	// but on some systems neither has an interrupts property as OF creates everything for us
	// the result is that both driver instances think they are the host MPIC.  On systems where
	// both MPICs are actually used the u3/u4 MPIC always cascades into the K2 MPIC so K2 is the
	// host MPIC.  On other systems the u3/u4 MPIC handles all interrupts and the K2 one is 
	// superfluous and should not even load.
	// 
	// So here, we find the u3/u4 MPIC and determine if it is the host MPIC.  We then set a flag
	// so when we see the K2 MPIC at platformAdjustService time, we just kill it off.
	
    regEntry = IORegistryEntry::fromPath("/u3/mpic", gIODTPlane);
    if (!regEntry)
		regEntry = IORegistryEntry::fromPath("/u4/mpic", gIODTPlane);
    if (regEntry) {
		u3IsHostMPIC = (regEntry->getProperty ("interrupts") == NULL);
		regEntry->release();
    } else
    	u3IsHostMPIC = false;

	/*
	 * Call super::start *before* we create specialized child nubs.  Child drivers for these nubs
	 * want to call publishResource and publishResource needs the IOResources node to exist 
	 * if not we'll get a message like "not registry member at registerService()"
	 * super::start() takes care of creating IOResources
	 */
	result = super::start(provider);
	
	// Create PlatformFunction nub
	platFuncDict = OSDictionary::withCapacity(2);
	if (platFuncDict)
	{
		// yes, sizeof( "IOPlatformFunctionNub" ), not "strlen()", because we want the \0 at the end for the C string termination
		strncpy(tmpName, "IOPlatformFunctionNub", sizeof( "IOPlatformFunctionNub" ));
		nameValueSymbol = OSSymbol::withCString(tmpName);
		nameValueData = OSData::withBytes(tmpName, strlen(tmpName)+1);
		platFuncDict->setObject ("name", nameValueData);
		platFuncDict->setObject ("compatible", nameValueData);

		if (plFuncNub = IOPlatformExpert::createNub (platFuncDict))
		{
			if (!plFuncNub->attach( this ))
				IOLog ("NUB ATTACH FAILED for IOPlatformFunctionNub\n");
			plFuncNub->setName (nameValueSymbol);

			plFuncNub->registerService();
		}
		platFuncDict->release();
		nameValueSymbol->release();
		nameValueData->release();
	}
	else
		return false;

	//
	// Create PlatformPlugin nub.  Use the "provider_name" as a key into the IOPlatformPluginTable
	// dictionary.  The name of the plugin to use is the value associated with the "provider_name" key.
	// If no entry is found in the dictionary, then the default "MacRISC4_PlatformPlugin" platform plugin
	// is used.
	//

	OSDictionary					* pluginLookupDict;
	OSData							* pHandle;
	const OSSymbol					* pHandleKey;

	if ( ( pluginLookupDict = OSDynamicCast( OSDictionary, getProperty( "IOPlatformPluginTable" ) ) ) == NULL )
		{
		kprintf( "CANNOT LOAD PLATFORM-PLUGIN LOOKUP TABLE\n" );
		return( false );
		}

	removeProperty( "IOPlatformPluginTable" );

	OSString*						platformPluginNameString;
	OSData*							platformPluginNameData;
	const char*						platformPluginName = "MacRISC4_PlatformPlugin";
	OSString*						modelString;

	// The pluginDict requires the values associated with the "name" and "compatible" keys to be OSData types.  Convert the
	// OSStrings to OSData.

	if ( ( platformPluginNameString = OSDynamicCast( OSString, pluginLookupDict->getObject( provider_name ) ) ) != NULL )
		{
		platformPluginName = platformPluginNameString->getCStringNoCopy();
		}

	platformPluginNameData = OSData::withBytes( platformPluginName, strlen( platformPluginName ) + 1 );

	if ( ( pluginDict = OSDictionary::withCapacity( 3 ) ) == NULL )
		return( false );

	const char*						ioPlatformPluginString = "IOPlatformPlugin";

	nameValueData = OSData::withBytes( ioPlatformPluginString, strlen( ioPlatformPluginString ) + 1 );

	modelString = OSString::withCString( provider_name );

	pluginDict->setObject( "name", nameValueData );
	pluginDict->setObject( "compatible", platformPluginNameData );
	pluginDict->setObject( "model", modelString );

	nameValueData->release();
	platformPluginNameData->release();

	// Add AAPL,phandle for "platform-" function use
	pHandleKey = OSSymbol::withCStringNoCopy("AAPL,phandle");
	if (pHandle = OSDynamicCast (OSData, provider->getProperty (pHandleKey)))
		pluginDict->setObject (pHandleKey, pHandle);
	pHandleKey->release();

	if ( ( ioPPluginNub = IOPlatformExpert::createNub( pluginDict ) ) != NULL )
	{
		if ( !ioPPluginNub->attach( this ) )
			kprintf( "NUB ATTACH FAILED\n" );

		ioPPluginNub->setName( ioPlatformPluginString );
		ioPPluginNub->registerService();
	}

	pluginDict->release();

	modelString->release();


	// Propagate platform- function properties to plugin nub
	// NOTE - this assumes *all* such properties need to be moved to the plugin nub
	//   as the properties in our nub will get deleted
	{
	OSDictionary			* propTable;
	OSCollectionIterator	* propIter;
	OSSymbol				* propKey;
	OSData					* propData;

		propTable = NULL;
		propIter = NULL;
		if ( ((propTable = provider->dictionaryWithProperties()) == 0) ||
			 ((propIter  = OSCollectionIterator::withCollection( propTable )) == 0) )
		{
			if ( propTable )
				propTable->release();
			propTable = NULL;
		}

		if ( propTable )
		{
			while ( (propKey = OSDynamicCast(OSSymbol, propIter->getNextObject())) != 0 )
			{
				// look for all properties starting with "platform-"
				if ( strncmp( kFunctionRequiredPrefix, propKey->getCStringNoCopy(), strlen(kFunctionRequiredPrefix)) == 0)
				{
					// Check specifically for "platform-do" properties and don't copy them
					if (strncmp(kFunctionProvidedPrefix, propKey->getCStringNoCopy(), strlen(kFunctionProvidedPrefix)) == 0)
						continue; // Don't copy "platform-do"s
						
					propData = OSDynamicCast(OSData, propTable->getObject(propKey));
					if (propData)
					{
						if ( ioPPluginNub->setProperty (propKey, propData) )
							// Successfully copied to plugin nub so remove our copy
							provider->removeProperty (propKey);
					}
				}
			}
		}
		if (propTable) propTable->release();
		if (propIter)  propIter->release();
	}


	kprintf ("MacRISC4PE::start - done\n");
    return result;
}

IORegistryEntry * MacRISC4PE::retrievePowerMgtEntry (void)
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

bool MacRISC4PE::platformAdjustService(IOService *service)
{
    const OSSymbol		*keySymbol;
	OSSymbol			*tmpSymbol;
    bool				result;
    IORegistryEntry		*parentIC;
    OSData				*parentICData;


#if 1			// VESTA HACK - remove this code once a better solution for resetting the phy!!
	IORegistryEntry		*phy;
	OSData				*compat;
	static				IOService *macio;
	static				bool hasVesta;

	// the vesta hack
    if (IODTMatchNubWithKeys(service, "mac-io")) {
		macio = service;		// Remember mac-io node
		if (hasVesta)
			macio->setProperty ("hasVesta", hasVesta);

		return true;
	}
	
    if (IODTMatchNubWithKeys(service, "K2-GMAC") || IODTMatchNubWithKeys(service, "gmac")) {
		// Same as code below. just copied here so all this code can be deleted when the time comes
		// [3332537] Mark ethernet PCI space as non-volatile (not needing save/restore)
		service->setProperty("IOPMPCIConfigSpaceVolatile", kOSBooleanFalse);
		
		// The vesta hack.
		// Look for the phy.  If its the vesta phy, remember that and if macio has been
		// discovered, record that info there.
		/*
		 * 12/19/03 - ARRGH!  This is a hack upon a hack [3505453].  The original hack was to look
		 * for the Vesta PHY and set a flag indicating its presence.  The AppleK2 driver then looked
		 * at that to see if it was necessary to reset the PHY [3475890],  But the problem is that the
		 * original G5s don't have Vesta but also don't need the reset.  So the original change broke
		 * G5 desktops.  A better fix is to check explicitly for the B5221 PHY (which does need a reset)
		 * rather then checking for Vesta.
		 *
		 * Rather than also making a change to AppleK2 I'm changing the meaning of hasVesta from
		 * "has B5461 PHY" to "!(has B5221 PHY).  The correct thing will be done by AppleK2 but
		 * the meaning for AppleK2 becomes hasVesta means don't need to reset the PHY.
		 */
		phy = service->getChildEntry( gIODTPlane );
		if (phy) {
			// Match to compatible directly.  IODTMatchNubWithKeys does not work because phy
			// only exists in the device tree plane and is not an IOService
			compat = OSDynamicCast (OSData, phy->getProperty ("compatible"));
			if (compat && (strncmp ( ( const char * ) compat->getBytesNoCopy(), "B5221", strlen ("B5221")))) {
				hasVesta = true;
				if (macio)
					macio->setProperty ("hasVesta", hasVesta);
			} 
		}
		
        return true;
    }


#endif
  
    if (IODTMatchNubWithKeys(service, "cpu"))
    {
		if (!(service->getProperty(gIOInterruptControllersKey)))
		{
			/*
			 * Ideally the interrupts properties are created by the BootROM and we can just use them
			 * But historically that wasn't true so if the interrupt controller properties don't exist 
			 * we provide a hook so they can be created in the CPU driver.
			 */
			parentICData = OSDynamicCast(OSData, service->getProperty(kMacRISC4ParentICKey));
			if (parentICData == 0)
			{
				parentIC = fromPath("mac-io/mpic", gIODTPlane);
				if (parentIC)
				{
					parentICData = OSDynamicCast(OSData, parentIC->getProperty("AAPL,phandle"));
					service->setProperty(kMacRISC4ParentICKey, parentICData);
					parentIC->release();
				}
			}
        }
        
		// Identify this as a MacRISC4 type cpu so cpu matching works correctly
		service->setProperty ("cpu-device-type", "MacRISC4CPU");
        
        return true;
    }
    
    if (IODTMatchNubWithKeys(service, "open-pic"))
    {
		// [5376988] - Kill off K2 mpic if not needed (because the u3/u4 mpic is the only one needed
		// If the "big-endian" property doesn't exist, this must be the K2 mpic
		if (u3IsHostMPIC && (service->getProperty ("big-endian") == NULL))
			return false;
		
        keySymbol = OSSymbol::withCStringNoCopy("InterruptControllerName");
        tmpSymbol = (OSSymbol *)IODTInterruptControllerName(service);
        result = service->setProperty(keySymbol, tmpSymbol);
        return true;
    }

    if (IODTMatchNubWithKeys(service, "K2-GMAC") || IODTMatchNubWithKeys(service, "gmac"))
    {
		// [3332537] Mark ethernet PCI space as non-volatile (not needing save/restore)
		service->setProperty("IOPMPCIConfigSpaceVolatile", kOSBooleanFalse);
        return true;
    }

    if ( strncmp( service->getName(), "pci80211", sizeof( "pci80211" ) ) == 0 )
    {
    	//
    	// [3843806] For original iMac G5, we need to tell the AirPort driver to handle channel 1 special.
		// If this property already exists, then do not do anything.
		//
		if ( strncmp( provider_name, "PowerMac8,1", sizeof( "PowerMac8,1" ) ) == 0 )
		{
			if ( service->getProperty( "NCWI" ) == NULL )
			{
				//
				// The first byte is the count, and the remaining data indicates which channel.
				//

				static const unsigned char				NcwiPropertyData[ 2 ] = { 0x01, 0x01 };

				service->setProperty( "NCWI", ( void * ) NcwiPropertyData, sizeof( NcwiPropertyData ) );

				return( true );
			}
		}
    }  

    if ( strncmp( service->getName(), "smu", sizeof( "smu" ) ) == 0 )
    {
        OSString			 *platformModel;

        // Set the platform-model property [4003701] so smu updates work
        platformModel = OSString::withCString (provider_name);
        if (platformModel) {
            service->setProperty("platform-model", platformModel);
            platformModel->release();
        }

    	//
    	// [3942950] For original iMac G5, we need to indicate to the AppleSMU driver that the
    	// new LED code can be used.  If this property already exists, then do not do anything.
		//
		if ( strncmp( provider_name, "PowerMac8,1", sizeof( "PowerMac8,1" ) ) == 0 )
		{
			if ( service->getProperty( "sleep-led-limits" ) == NULL )
			{
				service->setProperty( "sleep-led-limits", ( void * ) NULL, 0 );

			}
		}
		return( true );
    }  
  
    if ( strncmp(service->getName(), "pmu", sizeof( "pmu" ) ) == 0 )
    {
        // Change the interrupt mapping for pmu source 4.
        OSArray              *tmpArray, *tmpArrayCopy;
        OSCollectionIterator *extIntList, *extIntListOldWay;
        IORegistryEntry      *extInt = NULL;
        OSObject             *extIntControllerName;
        OSObject             *extIntControllerData;
        OSString			 *platformModel;
    
        // Set the no-nvram property.
        service->setProperty("no-nvram", service);

        // Set the platform-model property
        platformModel = OSString::withCString (provider_name);
        if (platformModel) {
            service->setProperty("platform-model", platformModel);
            platformModel->release();
        }
    
		extIntList = extIntListOldWay = NULL;
		
        // Find the new interrupt information.
        extIntList = IODTFindMatchingEntries(getProvider(), kIODTRecursive, "'pmu-interrupt'");
		if (extIntList) {
			extInt = (IORegistryEntry *)extIntList->getNextObject();
			if (extInt) 
				kprintf ("pmu - got pmu-interrupt node new way - name '%s'\n", extInt->getName());
			else {
				extIntListOldWay = IODTFindMatchingEntries(getProvider(), kIODTRecursive, "'extint-gpio1'");
				extInt = (IORegistryEntry *)extIntListOldWay->getNextObject();
				if (extInt)
					kprintf ("pmu - got pmu-interrupt node old way - name '%s'\n", extInt->getName());
				else
					panic ("MacRISC4PE::platformAdjustService - no interrupt information for pmu");
			}
		}

        tmpArray = (OSArray *)extInt->getProperty(gIOInterruptControllersKey);
        extIntControllerName = tmpArray->getObject(0);
        tmpArray = (OSArray *)extInt->getProperty(gIOInterruptSpecifiersKey);
        extIntControllerData = tmpArray->getObject(0);
   
        // Replace the interrupt infomation for pmu source 4.
        tmpArray = (OSArray *)service->getProperty(gIOInterruptControllersKey);
//        tmpArray->replaceObject(4, extIntControllerName);	// rdar://5083196
		tmpArrayCopy = (OSArray *) tmpArray->copyCollection(); // make a copy so we can modify it outside of the IORegistry
		tmpArrayCopy->replaceObject(4, extIntControllerName);
		service->setProperty(gIOInterruptControllersKey, tmpArrayCopy); // put the modified copy back in the registry
		tmpArrayCopy->release();
        tmpArray = (OSArray *)service->getProperty(gIOInterruptSpecifiersKey);
//		tmpArray->replaceObject(4, extIntControllerData); // rdar://5083196
        tmpArrayCopy = (OSArray *) tmpArray->copyCollection(); // make a copy so we can modify it outside of the IORegistry
        tmpArrayCopy->replaceObject(4, extIntControllerData);
        service->setProperty(gIOInterruptSpecifiersKey, tmpArrayCopy); // put it back in the registry
        tmpArrayCopy->release();
    
        if (extIntList) extIntList->release();
        if (extIntListOldWay) extIntListOldWay->release();
        
        return true;
    }

    if ( strncmp(service->getName(), "via-pmu", sizeof( "via-pmu" )) == 0 )
    {
        service->setProperty("BusSpeedCorrect", this);
        return true;
    }
	
    if ( ( strncmp(service->getName(), "pci", sizeof( "pci" ) ) == 0) && service->getProperty ("shasta-interrupt-sequencer"))
    {
		publishResource ("ht-interrupt-sequencer", service);
        return true;
    }
	
    return true;
}

IOReturn MacRISC4PE::callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
					void *param1, void *param2,
					void *param3, void *param4)
{
    if (functionName == gGetDefaultBusSpeedsKey)
    {
        getDefaultBusSpeeds((long *)param1, (unsigned long **)param2);
        return kIOReturnSuccess;
    }
  
    if (functionName->isEqualTo("PlatformIsPortable")) {
		*(bool *) param1 = isPortable;
        return kIOReturnSuccess;
    }
    
    if (functionName->isEqualTo("IOPMSetSleepSupported")) {
		return slotsMacRISC4->determineSleepSupport ();
    }
    
    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

void MacRISC4PE::getDefaultBusSpeeds(long *numSpeeds, unsigned long **speedList)
{
    if ((numSpeeds == 0) || (speedList == 0)) return;
  
    *numSpeeds = 1;
    *speedList = macRISC4Speed;
}

//*********************************************************************************
// PMInstantiatePowerDomains
//
// This overrides the vanilla implementation in IOPlatformExpert.  It instantiates
// a root domain with two children, one for the USB bus (to handle the USB idle
// power budget), and one for the expansions slots on the PCI bus (to handle
// the idle PCI power budget)
//*********************************************************************************

void MacRISC4PE::PMInstantiatePowerDomains ( void )
{    
    IOPMUSBMacRISC4		*usbMacRISC4;
	UInt32				hibEnable;
	OSArray				*tmpArray;

	const OSSymbol *desc = OSSymbol::withCString("powertreedesc");

	// Move our power tree description from our driver (where it's a property in the driver)
	// to our provider
	kprintf ("MacRISC4PE::PMInstantiatePowerDomains - getting pmtree property\n");
    tmpArray = OSDynamicCast(OSArray, getProperty(desc));
    if ( tmpArray )
    	thePowerTree = (OSArray *)tmpArray->copyCollection();
    else
    	thePowerTree = NULL;

    if( 0 == thePowerTree)
    {
        kprintf ("error retrieving power tree\n");
		return;
    }
	kprintf ("MacRISC4PE::PMInstantiatePowerDomains - got pmtree property\n");

    // getProvider()->setProperty (desc, thePowerTree);
	
	// No need to keep original around
	removeProperty(desc);

    root = IOPMrootDomain::construct();
    root->attach(this);
    root->start(this);

    if (cannotSleep) {
        // Attempt to spot these machines by UniN version, number of CPU's, speed of CPU0, and model name
        // This forces the machines to doze at all times rather than sleep.
        root->setSleepSupported(kPCICantSleep);
    } else {
   		root->setSleepSupported(kRootDomainSleepSupported);
    }
   
    if (NULL == root)
    {
        kprintf ("PMInstantiatePowerDomains - null ROOT\n");
        return;
    }

    if (PE_parse_boot_arg("hib", &hibEnable) && hibEnable)
    {
        root->publishFeature(kIOHibernateFeatureKey);
    }

    PMRegisterDevice (NULL, root);

    usbMacRISC4 = new IOPMUSBMacRISC4;
    if (usbMacRISC4)
    {
        usbMacRISC4->init ();
        usbMacRISC4->attach (this);
        usbMacRISC4->start (this);
        PMRegisterDevice (root, usbMacRISC4);
    }

    slotsMacRISC4 = new IOPMSlotsMacRISC4;
    if (slotsMacRISC4)
    {
        slotsMacRISC4->init ();
        slotsMacRISC4->attach (this);
        slotsMacRISC4->start (this);
        PMRegisterDevice (root, slotsMacRISC4);
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

void MacRISC4PE::PMRegisterDevice(IOService * theNub, IOService * theDevice)
{
    bool            nodeFound  = false;
    IOReturn        err        = -1;
    OSData *	    propertyPtr = 0;
    const char *    theProperty;

    // Starts the protected area, we are trying to protect numInstancesRegistered
    if (pmmutex != NULL)
      IOLockLock(pmmutex);
     
    // reset our tracking variables before we check the XML-derived tree
    multipleParentKeyValue = NULL;
    numInstancesRegistered = 0;

    // try to find a home for this registrant in our XML-derived tree
    nodeFound = CheckSubTree (thePowerTree, theNub, theDevice, NULL);

    if (0 == numInstancesRegistered) {
        // make sure the provider is within the Power Plane...if not, 
        // back up the hierarchy until we find a grandfather or great
        // grandfather, etc., that is in the Power Plane.

        while( theNub && (!theNub->inPlane(gIOPowerPlane)))
            theNub = theNub->getProvider();
    }

    // Ends the protected area, we are trying to protect numInstancesRegistered
    if (pmmutex != NULL)
       IOLockUnlock(pmmutex);
     
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
        if ( strncmp( theProperty, "SLOT-", strlen("SLOT-")) == 0 )
            slotsMacRISC4->addPowerChild (theDevice);
    }
	
	return;
}
