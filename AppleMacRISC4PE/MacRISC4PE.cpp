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
 *  DRI: Dave Radcliffe
 *
 */
//		$Log: MacRISC4PE.cpp,v $
//		Revision 1.8  2003/07/22 01:25:53  raddog
//		[3332537]Fix Ethernet sleep hang by signalling its nub to not save/restore config space (causing a hang if the Ethernet clock is off)
//		
//		Revision 1.7  2003/06/27 00:45:07  raddog
//		[3304596]: remove unnecessary access to U3 Pwr registers on wake, [3249029]: Disable unused second process on wake, [3301232]: remove unnecessary PCI code from PE
//		
//		Revision 1.6  2003/06/03 01:53:01  raddog
//		3232168, 3259590 - pmu-interrupt map crash fix, cpu driver only load on MacRISC4
//		
//		Revision 1.5  2003/05/10 06:50:29  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.4.2.1  2003/05/01 09:28:32  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		Revision 1.4  2003/04/27 23:13:30  raddog
//		MacRISC4PE.cpp
//		
//		Revision 1.3  2003/04/14 20:05:27  raddog
//		[3224952]AppleMacRISC4CPU must specify which MPIC to use (improved fix over that previously submitted)
//		
//		Revision 1.2  2003/02/18 00:02:01  eem
//		3146943: timebase enable for MP, bump version to 1.0.1d3.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

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
	char					compatStr[128];
	char					tmpName[32];
    OSData          		*tmpData;
//    IORegistryEntry 		*uniNRegEntry;
    IORegistryEntry 		*powerMgtEntry;
    UInt32			   		*primInfo;
	const OSSymbol			*nameKey, *compatKey, *nameValueSymbol;
	const OSData			*nameValueData, *compatValueData;
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

#if 0
	// get uni-N version for use by platformAdjustService
	uniNRegEntry = provider->childFromPath("uni-n", gIODTPlane);
	if (uniNRegEntry == 0) {
		kprintf ("MacRISC4PE::start - no uni-n\n");
		return false;
	}
    tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("device-rev"));
    if (tmpData == 0) return false;
    uniNVersion = ((unsigned long *)tmpData->getBytesNoCopy())[0];
#endif

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
	
	nameKey = OSSymbol::withCStringNoCopy("name");
	compatKey = OSSymbol::withCStringNoCopy("compatible");
	// Create PlatformFunction nub
	platFuncDict = OSDictionary::withCapacity(2);
	if (platFuncDict) {		
		strcpy(tmpName, "IOPlatformFunctionNub");
		nameValueSymbol = OSSymbol::withCString(tmpName);
		nameValueData = OSData::withBytes(tmpName, strlen(tmpName)+1);
		platFuncDict->setObject (nameKey, nameValueData);
		platFuncDict->setObject (compatKey, nameValueData);
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
	
	/*
	 * Create PlatformPlugin nub - for specialized plugins, the compatible property is of the 
	 * form provider_name_PlatformPlugin, e.g., PowerMac7_1_PlatformPlugin.  For all others
	 * a generic compatible property of "MacRISC4_PlatformPlugin" is created.
	 */
	if (!strcmp (provider_name, "PowerMac7,2") /* add others here */) 
		strcpy (compatStr, "PowerMac7_2"); 
	else	// generic plugin
		strcpy (compatStr, "MacRISC4");

	pluginDict = OSDictionary::withCapacity(2);
	
	if (pluginDict) {
		strcpy(tmpName, "IOPlatformPlugin");
		nameValueSymbol = OSSymbol::withCString(tmpName);
		nameValueData = OSData::withBytes(tmpName, strlen(tmpName)+1);
		pluginDict->setObject (nameKey, nameValueData);
		strcat (compatStr, "_PlatformPlugin");

		compatValueData = OSData::withBytes(compatStr, strlen(compatStr)+1);
		pluginDict->setObject (compatKey, compatValueData);
		if (ioPPluginNub = IOPlatformExpert::createNub (pluginDict)) {
			if (!ioPPluginNub->attach( this ))
				kprintf ("NUB ATTACH FAILED\n");
			ioPPluginNub->setName (nameValueSymbol);

			ioPPluginNub->registerService();
		}
		pluginDict->release();
		nameValueSymbol->release();
		nameValueData->release();
		compatValueData->release();
    } else return false;
	
	nameKey->release();
	compatKey->release();
	
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

    if (IODTMatchNubWithKeys(service, "K2-GMAC"))
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
        IORegistryEntry      *extInt;
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

    root->setSleepSupported(kRootDomainSleepSupported);
   
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
