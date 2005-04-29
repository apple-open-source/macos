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
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: Core99.cpp,v 1.23 2003/08/16 22:34:54 galcher Exp $
 *
 *  DRI: Bill Galcher
 *
 *      $Log: Core99.cpp,v $
 *      Revision 1.23  2003/08/16 22:34:54  galcher
 *      [3258963] Modify ::platformAdjustService() to add a property to 'cpu' nodes
 *      so that when passive matching happens, this won't match against the generic
 *      'name' == 'cpu', but instead will have a 'cpu-device-type' == 'Core99CPU',
 *      and will only match when one actually has one of those.
 *
 *      Revision 1.22  2003/03/12 03:11:21  galcher
 *      [3122869] Remove old C-style implementation of power tree XML and move it into IOKitPersonalities in the project, where it will get auto-created for us when matching occurs.  Then fetch it in ::PMInstantiatePowerDomains() and move to it its provider as was previously done.  This allows us to remove Core99PowerTree.cpp from the build entirely, fixing the original complaint of the bug.
 *
 */

#include <sys/cdefs.h>

__BEGIN_DECLS
#include <ppc/proc_reg.h>

/* Map memory map IO space */
#include <mach/mach_types.h>
extern vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
__END_DECLS


#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include "Core99.h"
#include <IOKit/pci/IOPCIDevice.h>

static unsigned long core99Speed[] = { 0, 1 };

#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOPMSlots99.h"
#include "IOPMUSB99.h"
#include <IOKit/pwr_mgt/IOPMPagingPlexus.h>

//#include "Core99PowerTree.cpp"
extern char * gIOCore99PMTree;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super ApplePlatformExpert

OSDefineMetaClassAndStructors(Core99PE, ApplePlatformExpert);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool Core99PE::start(IOService *provider)
{
  long            machineType;
  long            allInOne;
  OSData          *tmpData;
  IORegistryEntry *uniNRegEntry;
  IORegistryEntry *powerMgtEntry;
  unsigned long   *primInfo;
  unsigned long   uniNArbCtrl, uniNBaseAddressTemp;
  const char      *provider_name;
  
  setChipSetType(kChipSetTypeCore99);
  

  // Set the machine type based on first entry found.
  provider_name = provider->getName();
  
  if      (!strcmp(provider_name, "PowerMac2,1"))
    machineType = kCore99TypePowerMac2_1;
  else if (!strcmp(provider_name, "PowerMac2,2"))
    machineType = kCore99TypePowerMac2_2;
  else if (!strcmp(provider_name, "PowerMac3,1"))
    machineType = kCore99TypePowerMac3_1;
  else if (!strcmp(provider_name, "PowerMac3,2"))
    machineType = kCore99TypePowerMac3_2;
  else if (!strcmp(provider_name, "PowerMac3,3"))
    machineType = kCore99TypePowerMac3_3;
  else if (!strcmp(provider_name, "PowerMac5,1"))
    machineType = kCore99TypePowerMac5_1;
  else if (!strcmp(provider_name, "PowerBook2,1"))
    machineType = kCore99TypePowerBook2_1;
  else if (!strcmp(provider_name, "PowerBook2,2"))
    machineType = kCore99TypePowerBook2_2;
  else if (!strcmp(provider_name, "PowerBook3,1"))
    machineType = kCore99TypePowerBook3_1;
  else return false;
  
  isPortable = (0 == strncmp(provider_name, "PowerBook", strlen("PowerBook"))) ||
	(0 == strncmp(provider_name, "iBook", strlen("iBook")));
  
  setMachineType(machineType);
  
  // Find out if this an all in one.
  allInOne = 0;
  switch (getMachineType()) {
  case kCore99TypePowerMac2_1 :
  case kCore99TypePowerMac2_2 :
    allInOne = 1;
    break;
  }
  if (allInOne) setProperty("AllInOne", this);
  
  // Get the bus speed from the Device Tree.
  tmpData = OSDynamicCast(OSData, provider->getProperty("clock-frequency"));
  if (tmpData == 0) return false;
  core99Speed[0] = *(unsigned long *)tmpData->getBytesNoCopy();
   
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
  if (uniNVersion < kUniNVersion150) {
    uniNArbCtrl = readUniNReg(kUniNArbCtrl);
    uniNArbCtrl &= ~kUniNArbCtrlQAckDelayMask;
    if (uniNVersion < kUniNVersion107) {
      uniNArbCtrl |= kUniNArbCtrlQAckDelay105 << kUniNArbCtrlQAckDelayShift;
    } else {
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
  if (powerMgtEntry == 0) {
    kprintf ("didn't find power mgt node\n");
    return false;
  }
  tmpData  = OSDynamicCast(OSData, powerMgtEntry->getProperty ("prim-info"));
  if (tmpData != 0) {
    primInfo = (unsigned long *)tmpData->getBytesNoCopy();
    if (primInfo != 0) {
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

IORegistryEntry * Core99PE::retrievePowerMgtEntry (void)
{
  IORegistryEntry *     theEntry = 0;
  IORegistryEntry *     anObj = 0;
  IORegistryIterator *  iter;
  OSString *            powerMgtNodeName;

  iter = IORegistryIterator::iterateOver (IORegistryEntry::getPlane(kIODeviceTreePlane), kIORegistryIterateRecursively);
  if (iter) {
    powerMgtNodeName = OSString::withCString("power-mgt");
    anObj = iter->getNextObject ();
    while (anObj) {
      if (anObj->compareName(powerMgtNodeName)) {
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

bool Core99PE::platformAdjustService(IOService *service)
{
  const OSSymbol *tmpSymbol, *keySymbol;
  bool           result;
  
  if (IODTMatchNubWithKeys(service, "cpu"))
  {
	// previously, the Core99CPU has been matching against
	// a IONameMatch property of "cpu", which is not very specific.
	// create a more specific name to match against.
	service->setProperty ("cpu-device-type", "Core99CPU");
	return true;

  }

  if (IODTMatchNubWithKeys(service, "open-pic"))
  {
        keySymbol = OSSymbol::withCStringNoCopy("InterruptControllerName");
        tmpSymbol = IODTInterruptControllerName(service);
        result = service->setProperty(keySymbol, (OSSymbol *)tmpSymbol);
        return true;
  }
  
  if (!strcmp(service->getName(), "programmer-switch")) {
    service->setProperty("mask_NMI", service);               // Set property to tell AppleNMI to mask/unmask NMI @ sleep/wake
    return true;
  }
  
  if (!strcmp(service->getName(), "pmu")) {
    // Change the interrupt mapping for pmu source 4.
    OSArray              *tmpArray;
    OSCollectionIterator *extIntList;
    IORegistryEntry      *extInt;
    OSObject             *extIntControllerName;
    OSObject             *extIntControllerData;
    
    // Set the no-nvram property.
    service->setProperty("no-nvram", service);
    
    // Find the new interrupt information.
    extIntList = IODTFindMatchingEntries(getProvider(), kIODTRecursive,
					 "'extint-gpio1'");
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

  if (!strcmp(service->getName(), "via-pmu")) {
    service->setProperty("BusSpeedCorrect", this);
    return true;
  }

  return true;
}

IOReturn Core99PE::callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
					void *param1, void *param2,
					void *param3, void *param4)
{
  if (functionName == gGetDefaultBusSpeedsKey) {
    getDefaultBusSpeeds((long *)param1, (unsigned long **)param2);
    return kIOReturnSuccess;
  }
  
  if (functionName->isEqualTo("EnableUniNEthernetClock")) {
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
  
  if (functionName->isEqualTo("AccessUniN15PerformanceRegister")) {
    return accessUniN15PerformanceRegister((bool)param1, (long)param2,
					   (unsigned long *)param3);
  }
  
  if (functionName->isEqualTo("PlatformIsPortable")) {
    *(bool *) param1 = isPortable;
    return kIOReturnSuccess;
  }
  
  return super::callPlatformFunction(functionName, waitForFunction,
				     param1, param2, param3, param4);
}

unsigned long Core99PE::readUniNReg(unsigned long offest)
{
  return uniNBaseAddress[offest / 4];
}

void Core99PE::writeUniNReg(unsigned long offest, unsigned long data)
{
  uniNBaseAddress[offest / 4] = data;
  eieio();
}

void Core99PE::getDefaultBusSpeeds(long *numSpeeds,
				   unsigned long **speedList)
{
  if ((numSpeeds == 0) || (speedList == 0)) return;
  
  *numSpeeds = 1;
  *speedList = core99Speed;
}

void Core99PE::enableUniNEthernetClock(bool enable)
{
  unsigned long regTemp;
  
  if (mutex != NULL)
     IOLockLock(mutex);

  regTemp = readUniNReg(kUniNClockControl);
  
  if (enable) {
    regTemp |= kUniNEthernetClockEnable;
  } else {
    regTemp &= ~kUniNEthernetClockEnable;
  }
  
  writeUniNReg(kUniNClockControl, regTemp);

  if (mutex != NULL)
     IOLockUnlock(mutex);
}

void Core99PE::enableUniNFireWireClock(bool enable)
{
  unsigned long regTemp;

  //IOLog("FWClock, enable = %d kFW = %x\n", enable, kUniNFirewireClockEnable);

  if (mutex != NULL)
     IOLockLock(mutex);
  
  regTemp = readUniNReg(kUniNClockControl);

  if (enable) {
    regTemp |= kUniNFirewireClockEnable;
  } else {
    regTemp &= ~kUniNFirewireClockEnable;
  }
  
  writeUniNReg(kUniNClockControl, regTemp);

  if (mutex != NULL)
     IOLockUnlock(mutex);
}

void Core99PE::enableUniNFireWireCablePower(bool enable)
{
    // Turn off cable power supply on mid/merc/pismo(on pismo only, this kills the phy)
    long x = getMachineType();
    
    if(x == kCore99TypePowerBook2_2 ||
        x == kCore99TypePowerBook3_1 ) {
        IOService *keyLargo;
        keyLargo = waitForService(serviceMatching("KeyLargo"));
        if(keyLargo) {
            UInt32 gpioOffset = 0x73;
            
            keyLargo->callPlatformFunction(OSSymbol::withCString("keyLargo_writeRegUInt8"),
                    true, (void *)&gpioOffset, (void *)(enable ? 0:4), 0, 0);
        }
    }
}

enum {
  kMCMonitorModeControl = 0,
  kMCCommand,
  kMCPerformanceMonitor0,
  kMCPerformanceMonitor1,
  kMCPerformanceMonitor2,
  kMCPerformanceMonitor3
};

IOReturn Core99PE::accessUniN15PerformanceRegister(bool write, long regNumber,
						   unsigned long *data)
{
  unsigned long offset;
  
  if (uniNVersion < kUniNVersion150) return kIOReturnUnsupported;
  
  switch (regNumber) {
  case kMCMonitorModeControl  : offset = kUniNMMCR; break;
  case kMCCommand             : offset = kUniNMCMDR; break;
  case kMCPerformanceMonitor0 : offset = kUniNMPMC1; break;
  case kMCPerformanceMonitor1 : offset = kUniNMPMC2; break;
  case kMCPerformanceMonitor2 : offset = kUniNMPMC3; break;
  case kMCPerformanceMonitor3 : offset = kUniNMPMC4; break;
  default                     : return kIOReturnBadArgument;
  }
  
  if (data == 0) return kIOReturnBadArgument;
  
  if (write) {
    writeUniNReg(offset, *data);
  } else {
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

#define kPOWER_TREE_DESCRIPTION_PROPERTY_NAME "powertreedesc"

void Core99PE::PMInstantiatePowerDomains ( void )
{    
IOPMUSB99		* usb99;
const OSSymbol	* PTDescription = OSSymbol::withCString( kPOWER_TREE_DESCRIPTION_PROPERTY_NAME );

    // NOTE - the power tree XML is now included in AppleCore99PE.pbproj/project.pbxproj
    //        as a Personality-based property.  ALL properties of this type are
    //        automatically created in the appropriate node when the KEXT is matched.
    //        Therefore, all we have to do is go fetch this property and move it up
    //        to its provider.

    thePowerTree = OSDynamicCast(OSArray, getProperty( PTDescription ));
    if ( thePowerTree )
        getProvider()->setProperty( kPOWER_TREE_DESCRIPTION_PROPERTY_NAME, thePowerTree );
    else
        kprintf( "%s::PMInstantiatePowerDomains - unable to retrieve '%s' property\n",
                    this->getName(), kPOWER_TREE_DESCRIPTION_PROPERTY_NAME );

#if CREATE_PLEXUS
	plexus = new IOPMPagingPlexus;
	if ( plexus )
    {
        plexus->init();
        plexus->attach(this);
        plexus->start(this);
    }
#endif
         
	root = IOPMrootDomain::construct();
	root->attach(this);
	root->start(this);

    if ( plexus ) {
        root->addPowerChild(plexus);
    }

    root->setSleepSupported(kRootDomainSleepSupported);
   
   if (NULL == root) {
     kprintf ("PMInstantiatePowerDomains - null ROOT\n");
     return;
   }

   PMRegisterDevice (NULL, root);

   usb99 = new IOPMUSB99;
   if (usb99) {
     usb99->init ();
     usb99->attach (this);
     usb99->start (this);
     PMRegisterDevice (root, usb99);
     if ( plexus ) {
        plexus->addPowerChild (usb99);
     }
   }

   slots99 = new IOPMSlots99;
   if (slots99) {
     slots99->init ();
     slots99->attach (this);
     slots99->start (this);
     PMRegisterDevice (root, slots99);
     if ( plexus ) {
        plexus->addPowerChild (slots99);
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

void Core99PE::PMRegisterDevice(IOService * theNub, IOService * theDevice)
{
  bool            nodeFound  = false;
  IOReturn        err        = -1;
  OSData *	  propertyPtr = 0;
  const char *	  theProperty;

  // Starts the protected area, we are trying to protect numInstancesRegistered
  if (mutex != NULL)
     IOLockLock(mutex);

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
        slots99->addPowerChild (theDevice);
     }
  }
}
