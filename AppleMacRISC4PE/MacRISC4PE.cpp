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
 *  DRI: Dave Radcliffe
 *
 */


#include <sys/cdefs.h>

__BEGIN_DECLS
#include <ppc/proc_reg.h>
#include <ppc/machine_routines.h>

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
#include <IOKit/pwr_mgt/IOPMPagingPlexus.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include <pexpert/pexpert.h>

extern char *gIOMacRISC4PMTree;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super ApplePlatformExpert

OSDefineMetaClassAndStructors(MacRISC4PE, ApplePlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool MacRISC4PE::start(IOService *provider)
{
    long            		machineType;
	char					tmpName[32];
    OSData          		*tmpData;
//    IORegistryEntry 		*uniNRegEntry;
    IORegistryEntry 		*powerMgtEntry;
    UInt32			   		*primInfo;
	const OSSymbol			*nameValueSymbol;
	const OSData			*nameValueData;
	OSDictionary			*pluginDict, *platFuncDict;
	bool					result;
	
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


// Ethan turned this ON for PowerMac 1.8ghz specific hack below
#if 1
    IORegistryEntry     *uniNRegEntry = NULL;
	// get uni-N version for use by platformAdjustService
	uniNRegEntry = provider->childFromPath("u3", gIODTPlane);
	if (uniNRegEntry == 0) {
		kprintf ("MacRISC4PE::start - no u3\n");
		return false;
	}
    tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("device-rev"));
    if (tmpData == 0) return false;
    uniNVersion = ((unsigned long *)tmpData->getBytesNoCopy())[0];

// Ethan B: PowerMac G5 UP 1.8ghz specific hack
// Disable sleep on these machines because of unstable sleep problems
    IORegistryEntry *cpu0_reg_entry = NULL;
    OSData *cpu0_freq_data = NULL;
    UInt32 cpu0_freq = 0;
    int second_cpu_exists = (int)fromPath ("/cpus/@1", gIODTPlane);
    const UInt32 one_eight_ghz_freq = 1800000000;
    
    cpu0_reg_entry = fromPath ("/cpus/@0", gIODTPlane);
    if(cpu0_reg_entry) 
    {
        cpu0_freq_data = OSDynamicCast (OSData, cpu0_reg_entry->getProperty ("clock-frequency"));
        if(cpu0_freq_data) 
        {
            cpu0_freq = *(UInt32 *)cpu0_freq_data->getBytesNoCopy();
			IOLog ("PE: cpu freq %ld\n", cpu0_freq);
        }
    }
    
    cannotSleep = ((0 == strncmp(provider_name, "PowerMac7,2", strlen("PowerMac7,2")))     // check model
        && ((kUniNRevision3_2_1 == uniNVersion)                      // check U3 version 2.1
        || (kUniNRevision3_2_3 == uniNVersion))                     // check U3 version 2.3
        && (!second_cpu_exists)                                     // check for existence of second CPU
        && (one_eight_ghz_freq == cpu0_freq) );                      // check CPU0 speed = 1.8ghz

	if (cannotSleep) setProperty ("PlatformCannotSleep", true);

#endif

    // Get PM features and private features
	// The power-mgt node is being deprecated in favor of specfic properties to describe
	// platform behaviors.  This is here mostly for backward compatibility
    powerMgtEntry = retrievePowerMgtEntry ();

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
	
	/*
	 * Call super::start *before* we create specialized child nubs.  Child drivers for these nubs
	 * want to call publishResource and publishResource needs the IOResources node to exist 
	 * if not we'll get a message like "not registry member at registerService()"
	 * super::start() takes care of creating IOResources
	 */
	result = super::start(provider);
	
	// Create PlatformFunction nub
	platFuncDict = OSDictionary::withCapacity(2);
	if (platFuncDict) {		
		strcpy(tmpName, "IOPlatformFunctionNub");
		nameValueSymbol = OSSymbol::withCString(tmpName);
		nameValueData = OSData::withBytes(tmpName, strlen(tmpName)+1);
		platFuncDict->setObject ("name", nameValueData);
		platFuncDict->setObject ("compatible", nameValueData);
		if (plFuncNub = IOPlatformExpert::createNub (platFuncDict)) {
			if (!plFuncNub->attach( this ))
				IOLog ("NUB ATTACH FAILED for IOPlatformFunctionNub\n");
			plFuncNub->setName (nameValueSymbol);

			plFuncNub->registerService();
		}
		platFuncDict->release();
		nameValueSymbol->release();
		nameValueData->release();
	} else return false;

	//
	// Create PlatformPlugin nub.  Use the "provider_name" as a key into the IOPlatformPluginTable
	// dictionary.  The name of the plugin to use is the value associated with the "provider_name" key.
	// If no entry is found in the dictionary, then the default "MacRISC4_PlatformPlugin" platform plugin
	// is used.
	//

	OSDictionary*					pluginLookupDict;

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

	if ( ( ioPPluginNub = IOPlatformExpert::createNub( pluginDict ) ) != NULL )
		{
		if ( !ioPPluginNub->attach( this ) )
			kprintf( "NUB ATTACH FAILED\n" );

		ioPPluginNub->setName( ioPlatformPluginString );
		ioPPluginNub->registerService();
		}

	pluginDict->release();

	modelString->release();

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
        parentICData = OSDynamicCast(OSData, service->getProperty(kMacRISC4ParentICKey));
        if (parentICData == 0)
        {
            parentIC = fromPath("mac-io/mpic", gIODTPlane);
            parentICData = OSDynamicCast(OSData, parentIC->getProperty("AAPL,phandle"));
            service->setProperty(kMacRISC4ParentICKey, parentICData);
        }
        
		// Identify this as a MacRISC4 type cpu so cpu matching works correctly
		service->setProperty ("cpu-device-type", "MacRISC4CPU");
        
        return true;
    }
    
    if (IODTMatchNubWithKeys(service, "open-pic"))
    {
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

    if (!strcmp(service->getName(), "programmer-switch"))
    {
        // Set property to tell AppleNMI to mask/unmask NMI @ sleep/wake
        service->setProperty("mask_NMI", service); 
        return true;
    }
  
    if (!strcmp(service->getName(), "pmu"))
    {
        // Change the interrupt mapping for pmu source 4.
        OSArray              *tmpArray;
        OSCollectionIterator *extIntList, *extIntListOldWay;
        IORegistryEntry      *extInt = NULL;
        OSObject             *extIntControllerName;
        OSObject             *extIntControllerData;
    
        // Set the no-nvram property.
        service->setProperty("no-nvram", service);

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
        tmpArray->replaceObject(4, extIntControllerName);
        tmpArray = (OSArray *)service->getProperty(gIOInterruptSpecifiersKey);
        tmpArray->replaceObject(4, extIntControllerData);
    
        if (extIntList) extIntList->release();
        if (extIntListOldWay) extIntListOldWay->release();
        
        return true;
    }

    if (!strcmp(service->getName(), "via-pmu"))
    {
        service->setProperty("BusSpeedCorrect", this);
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
    IOPMUSBMacRISC4 * usbMacRISC4;
	const OSSymbol *desc = OSSymbol::withCString("powertreedesc");

	// Move our power tree description from our driver (where it's a property in the driver)
	// to our provider
	kprintf ("MacRISC4PE::PMInstantiatePowerDomains - getting pmtree property\n");
    thePowerTree = OSDynamicCast(OSArray, getProperty(desc));

    if( 0 == thePowerTree)
    {
        kprintf ("error retrieving power tree\n");
		return;
    }
	kprintf ("MacRISC4PE::PMInstantiatePowerDomains - got pmtree property\n");

    getProvider()->setProperty (desc, thePowerTree);
	
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
// End of hack
   
    if (NULL == root)
    {
        kprintf ("PMInstantiatePowerDomains - null ROOT\n");
        return;
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
        if ( strncmp("SLOT-",theProperty,5) == 0 )
            slotsMacRISC4->addPowerChild (theDevice);
    }
	
	return;
}
