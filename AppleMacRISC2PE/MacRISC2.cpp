/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2000 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Tom Sherman
 *
 */

#include <ppc/proc_reg.h>
#include <ppc/machine_routines.h>

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include "MacRISC2.h"
#include <IOKit/pci/IOPCIDevice.h>

static unsigned long macRISC2Speed[] = { 0, 1 };

#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOPMSlotsMacRISC2.h"
#include "IOPMUSBMacRISC2.h"
#include <IOKit/pwr_mgt/IOPMPagingPlexus.h>

extern char *gIOMacRISC2PMTree;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super ApplePlatformExpert

OSDefineMetaClassAndStructors(MacRISC2PE, ApplePlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool MacRISC2PE::start(IOService *provider)
{
    long            	machineType;
    OSData          	*tmpData;
    IORegistryEntry 	*uniNRegEntry;
    IORegistryEntry 	*powerMgtEntry;
    unsigned long   	*primInfo;
    unsigned long   	uniNArbCtrl, uniNBaseAddressTemp;
    const char 		*provider_name;
    setChipSetType(kChipSetTypeCore2001);
        
    // Set the machine type.
    provider_name = provider->getName();  
   
    if ( (provider_name != NULL) && (0 == strncmp(provider_name, "PowerMac", strlen("PowerMac"))) )
    {
        machineType = kMacRISC2TypePowerMac;
    }
    
    if ( (provider_name != NULL) && (0 == strncmp(provider_name, "PowerBook", strlen("PowerBook"))) )
    {
        machineType = kMacRISC2TypePowerBook;
    }

    setMachineType(machineType);

    // Get the bus speed from the Device Tree.
    tmpData = OSDynamicCast(OSData, provider->getProperty("clock-frequency"));
    if (tmpData == 0) return false;
    macRISC2Speed[0] = *(unsigned long *)tmpData->getBytesNoCopy();
   
    // Get a memory mapping for Uni-N's registers.
    uniNRegEntry = provider->childFromPath("uni-n", gIODTPlane);
    if (uniNRegEntry == 0) return false;
    tmpData = OSDynamicCast(OSData, uniNRegEntry->getProperty("reg"));
    if (tmpData == 0) return false;
    uniNBaseAddressTemp = ((unsigned long *)tmpData->getBytesNoCopy())[0];
    uniNBaseAddress = (unsigned long *)ml_io_map(uniNBaseAddressTemp, 0x1000);
    if (uniNBaseAddress == 0) return false;
  
    // Set QAckDelay depending on the version of Uni-N.
    uniNVersion = readUniNReg(kUniNVersion);

    if (uniNVersion < kUniNVersion150)
    {
        uniNArbCtrl = readUniNReg(kUniNArbCtrl);
        uniNArbCtrl &= ~kUniNArbCtrlQAckDelayMask;

        if (uniNVersion < kUniNVersion107)
        {
            uniNArbCtrl |= kUniNArbCtrlQAckDelay105 << kUniNArbCtrlQAckDelayShift;
        } 
        else
        {
            uniNArbCtrl |= kUniNArbCtrlQAckDelay << kUniNArbCtrlQAckDelayShift;
        }
        writeUniNReg(kUniNArbCtrl, uniNArbCtrl);
    }

    // Creates the nubs for the children of uni-n
    IOService *uniNServiceEntry = OSDynamicCast(IOService, uniNRegEntry);
    if (uniNServiceEntry != NULL)
        createNubs(this, uniNRegEntry->getChildIterator( gIODTPlane ));
  
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
    mutex = IOLockAlloc();
    if (mutex == NULL) {
      return false;
    }
    else
      IOLockInit( mutex );
      
    return super::start(provider);
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
    const OSSymbol *tmpSymbol, *keySymbol;
    bool           result;
  
    if (IODTMatchNubWithKeys(service, "open-pic"))
    {
        keySymbol = OSSymbol::withCStringNoCopy("InterruptControllerName");
        tmpSymbol = IODTInterruptControllerName(service);
        result = service->setProperty(keySymbol, tmpSymbol);
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
        OSCollectionIterator *extIntList;
        IORegistryEntry      *extInt;
        OSObject             *extIntControllerName;
        OSObject             *extIntControllerData;
    
        // Set the no-nvram property.
        service->setProperty("no-nvram", service);
    
        // Find the new interrupt information.
        extIntList = IODTFindMatchingEntries(getProvider(), kIODTRecursive, "'extint-gpio1'");
        extInt = (IORegistryEntry *)extIntList->getNextObject();
    
        tmpArray = (OSArray *)extInt->getProperty(gIOInterruptControllersKey);
        extIntControllerName = tmpArray->getObject(0);
        tmpArray = (OSArray *)extInt->getProperty(gIOInterruptSpecifiersKey);
        extIntControllerData = tmpArray->getObject(0);
    
        // Replace the interrupt infomation for pmu source 4.
        tmpArray = (OSArray *)service->getProperty(gIOInterruptControllersKey);
        tmpArray->replaceObject(4, extIntControllerName);
        tmpArray = (OSArray *)service->getProperty(gIOInterruptSpecifiersKey);
        tmpArray->replaceObject(4, extIntControllerData);
    
        extIntList->release();
        
        return true;
    }

    if (!strcmp(service->getName(), "via-pmu"))
    {
        service->setProperty("BusSpeedCorrect", this);
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
        enableUniNEthernetClock((bool)param1);
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo("EnableFireWireClock")) {
        enableUniNFireWireClock((bool)param1);
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
  
    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

unsigned long MacRISC2PE::readUniNReg(unsigned long offest)
{
    return uniNBaseAddress[offest / 4];
}

void MacRISC2PE::writeUniNReg(unsigned long offest, unsigned long data)
{
    uniNBaseAddress[offest / 4] = data;
    eieio();
}

void MacRISC2PE::getDefaultBusSpeeds(long *numSpeeds, unsigned long **speedList)
{
    if ((numSpeeds == 0) || (speedList == 0)) return;
  
    *numSpeeds = 1;
    *speedList = macRISC2Speed;
}

void MacRISC2PE::enableUniNEthernetClock(bool enable)
{
    unsigned long regTemp;

    if (mutex != NULL)
        IOLockLock(mutex);
  
    regTemp = readUniNReg(kUniNClockControl);
  
    if (enable)
    {
        regTemp |= kUniNEthernetClockEnable;
    } 
    else
    {
        regTemp &= ~kUniNEthernetClockEnable;
    }
  
    writeUniNReg(kUniNClockControl, regTemp);

    if (mutex != NULL)
        IOLockUnlock(mutex);
}

void MacRISC2PE::enableUniNFireWireClock(bool enable)
{
    unsigned long regTemp;

    //IOLog("FWClock, enable = %d kFW = %x\n", enable, kUniNFirewireClockEnable);
  
    if (mutex != NULL)
        IOLockLock(mutex);

    regTemp = readUniNReg(kUniNClockControl);

    if (enable)
    {
        regTemp |= kUniNFirewireClockEnable;
    } 
    else
    {
        regTemp &= ~kUniNFirewireClockEnable;
    }
  
    writeUniNReg(kUniNClockControl, regTemp);

    if (mutex != NULL)
        IOLockUnlock(mutex);
}

void MacRISC2PE::enableUniNFireWireCablePower(bool enable)
{
    // Turn off cable power supply on mid/merc/pismo(on pismo only, this kills the phy)

    if(getMachineType() == kMacRISC2TypePowerBook)
    {
        IOService *keyLargo;
        keyLargo = waitForService(serviceMatching("KeyLargo"));
        
        if(keyLargo)
        {
            UInt32 gpioOffset = 0x73;
            
            keyLargo->callPlatformFunction(OSSymbol::withCString("keyLargo_writeRegUInt8"),
                    true, (void *)&gpioOffset, (void *)(enable ? 0:4), 0, 0);
        }
    }
}


enum
{
  kMCMonitorModeControl = 0,
  kMCCommand,
  kMCPerformanceMonitor0,
  kMCPerformanceMonitor1,
  kMCPerformanceMonitor2,
  kMCPerformanceMonitor3
};

IOReturn MacRISC2PE::accessUniN15PerformanceRegister(bool write, long regNumber, unsigned long *data)
{
    unsigned long offset;
  
    if (uniNVersion < kUniNVersion150) return kIOReturnUnsupported;
  
    switch (regNumber)
    {
    case kMCMonitorModeControl  : offset = kUniNMMCR; break;
    case kMCCommand             : offset = kUniNMCMDR; break;
    case kMCPerformanceMonitor0 : offset = kUniNMPMC1; break;
    case kMCPerformanceMonitor1 : offset = kUniNMPMC2; break;
    case kMCPerformanceMonitor2 : offset = kUniNMPMC3; break;
    case kMCPerformanceMonitor3 : offset = kUniNMPMC4; break;
    default                     : return kIOReturnBadArgument;
    }
  
    if (data == 0) return kIOReturnBadArgument;
  
    if (write)
    {
        writeUniNReg(offset, *data);
    } 
    else 
    {
        *data = readUniNReg(offset);
    }
  
    return kIOReturnSuccess;
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
    OSString * errorStr = new OSString;
    OSObject * obj;
    IOPMUSBMacRISC2 * usbMacRISC2;

    obj = OSUnserializeXML (gIOMacRISC2PMTree, &errorStr);

    if( 0 == (thePowerTree = ( OSArray * ) obj) )
    {
        kprintf ("error parsing power tree: %s", errorStr->getCStringNoCopy());
    }

    getProvider()->setProperty ("powertreedesc", thePowerTree);

#if CREATE_PLEXUS
   plexus = new IOPMPagingPlexus;
   if ( plexus ) {
        plexus->init();
        plexus->attach(this);
        plexus->start(this);
    }
#endif
         
    root = new IOPMrootDomain;
    root->init();
    root->attach(this);
    root->start(this);

    if ( plexus ) {
        root->addPowerChild(plexus);
    }

    root->setSleepSupported(kRootDomainSleepSupported);
   
    if (NULL == root)
    {
        kprintf ("PMInstantiatePowerDomains - null ROOT\n");
        return;
    }

    PMRegisterDevice (NULL, root);

    usbMacRISC2 = new IOPMUSBMacRISC2;
    if (usbMacRISC2)
    {
        usbMacRISC2->init ();
        usbMacRISC2->attach (this);
        usbMacRISC2->start (this);
        PMRegisterDevice (root, usbMacRISC2);
        if ( plexus ) {
            plexus->addPowerChild (usbMacRISC2);
        }
    }

    slotsMacRISC2 = new IOPMSlotsMacRISC2;
    if (slotsMacRISC2)
    {
        slotsMacRISC2->init ();
        slotsMacRISC2->attach (this);
        slotsMacRISC2->start (this);
        PMRegisterDevice (root, slotsMacRISC2);
        if ( plexus ) {
            plexus->addPowerChild (slotsMacRISC2);
        }
    }

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
        if ( plexus ) {
            plexus->addPowerChild (theDevice);
        }
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
