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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Robert Zhang 
 *
 */


#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>

#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/platform/AppleNMI.h>

#include "Heathrow.h"
#include <IOKit/ppc/IODBDMA.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super AppleMacIO

OSDefineMetaClassAndStructors(Heathrow, AppleMacIO);

bool Heathrow::start(IOService *provider)
{
  bool		     ret;
  // Call MacIO's start.
  if (!super::start(provider))
    return false;

  // callPlatformFunction symbols
  heathrow_enableSCC = OSSymbol::withCString("EnableSCC");
  heathrow_powerModem = OSSymbol::withCString("PowerModem");
  heathrow_modemResetLow = OSSymbol::withCString("ModemResetLow");
  heathrow_modemResetHigh = OSSymbol::withCString("ModemResetHigh");
  heathrow_sleepState = OSSymbol::withCString("heathrow_sleepState");
  heathrow_powerMediaBay = OSSymbol::withCString("powerMediaBay");
  heathrow_set_light = OSSymbol::withCString("heathrow_set_light");
  heathrow_writeRegUInt8 = OSSymbol::withCString("heathrow_writeRegUInt8");
  heathrow_safeWriteRegUInt8 = OSSymbol::withCString("heathrow_safeWriteRegUInt8");
  heathrow_safeReadRegUInt8 = OSSymbol::withCString("heathrow_safeReadRegUInt8");
  heathrow_safeWriteRegUInt32 = OSSymbol::withCString("heathrow_safeWriteRegUInt32");
  heathrow_safeReadRegUInt32 = OSSymbol::withCString("heathrow_safeReadRegUInt32");

  // just initializes this:
  mediaIsOn = true;

  // sets up the mutex lock:
  mutex = IOSimpleLockAlloc();
      
  // Figure out which heathrow this is.
  if (IODTMatchNubWithKeys(provider, "heathrow"))
    heathrowNum = kPrimaryHeathrow;
  else if (IODTMatchNubWithKeys(provider, "gatwick"))
    heathrowNum = kSecondaryHeathrow;
  else return false; // This should not happen.
  
  if (heathrowNum == kPrimaryHeathrow) {
    if (getPlatform()->getChipSetType() != kChipSetTypePowerExpress)
      getPlatform()->setCPUInterruptProperties(provider);
  }
  
  // get the base address of the this heathrow.
  heathrowBaseAddress = fMemory->getVirtualAddress();
  
  // Make nubs for the children.
  publishBelow( provider );

  ret = installInterrupts(provider);

  // register iteself so we can find it:
  registerService();

  // mark the current state as invalid:
  savedState.thisStateIsValid = false;

  // attach to the power managment tree:
  initForPM(provider);
  
  //kprintf("Heathrow::start(%s) %d\n", provider->getName(), ret);

  return ret;
}

IOReturn Heathrow::callPlatformFunction(const OSSymbol *functionName,
                                                            bool waitForFunction,
                                                            void *param1, void *param2,
                                                            void *param3, void *param4)
{  
    if (functionName == heathrow_sleepState)
    {
        sleepState((bool)param1);
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_powerMediaBay) {
        bool powerOn = (param1 != NULL);
        powerMediaBay(powerOn, (UInt8)param2);
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_enableSCC)
    {
        EnableSCC((bool)param1);
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_powerModem)
    {
        PowerModem((bool)param1);
        return kIOReturnSuccess;
    }

    if (functionName ==  heathrow_modemResetLow)
    {
        ModemResetLow();
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_modemResetHigh)
    {
        ModemResetHigh();
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_set_light)
    {
        setChassisLightFullpower((bool)param1);
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_writeRegUInt8)
    {
        writeRegUInt8(*(unsigned long *)param1, (UInt8)param2);
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_safeWriteRegUInt8)
    {
        safeWriteRegUInt8((unsigned long)param1, (UInt8)param2, (UInt8)param3);
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_safeReadRegUInt8)
    {
        UInt8 *returnval = (UInt8 *)param2;
        *returnval = safeReadRegUInt8((unsigned long)param1);
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_safeWriteRegUInt32)
    {
        safeWriteRegUInt32((unsigned long)param1, (UInt32)param2, (UInt32)param3);
        return kIOReturnSuccess;
    }

    if (functionName == heathrow_safeReadRegUInt32)
    {
        UInt32 *returnval = param2;
        *returnval = safeReadRegUInt32((unsigned long)param1);
        return kIOReturnSuccess;
    }

    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

void Heathrow::EnableSCC(bool state)
{
	IOInterruptState intState;
    
    if ( mutex  != NULL )
    	intState = IOSimpleLockLockDisableInterrupt(mutex);
	
    if (state)
    {

        // Enables the SCC cell: 0x00420000 (this starts scc clock and enables scca)
        *(UInt32*)(heathrowBaseAddress + heathrowFCROffset) |= ( heathrowFCSCCCEn | heathrowFCSCCAEn );	    
        eieio();

       /* // Resets the SCC:
        *(UInt32*)(heathrowBaseAddress + heathrowFCROffset) |= heathrowFCResetSCC;
        eieio();

        IOSleep(15);
        *(UInt32*)(heathrowBaseAddress + heathrowFCROffset) &= ~heathrowFCResetSCC;
        eieio(); */
    }
    else
    {
        // disable
    }
    
    if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);
    

    return;
}

void Heathrow::PowerModem(bool state)
{
    // On PowerMac 1,2 () the modem is always on and this bit
    // disables the nvram. So on Yikes we exit without doing
    // anything. Also setting this bit has as bas nvram side
    // effects.
  
  IOInterruptState intState;
  
  if (IODTMatchNubWithKeys(getPlatform()->getProvider(), "'PowerMac1,1'") ||
        IODTMatchNubWithKeys(getPlatform()->getProvider(), "'PowerMac1,2'"))
        return;

  if ( mutex  != NULL )
     intState = IOSimpleLockLockDisableInterrupt(mutex);
        
    if (state)
    {
        *(UInt32*)(heathrowBaseAddress + heathrowFCROffset) &= ~heathrowFCTrans;
        eieio();
    }
    else
    {
        *(UInt32*)(heathrowBaseAddress + heathrowFCROffset) |= heathrowFCTrans;
        eieio();
    }
  if ( mutex  != NULL )
     IOSimpleLockUnlockEnableInterrupt(mutex, intState);

    return;
}

void Heathrow::ModemResetLow()
{
		IOInterruptState intState;
  		if ( mutex  != NULL )
        	intState = IOSimpleLockLockDisableInterrupt(mutex);

        *(UInt32*)(heathrowBaseAddress + heathrowFCROffset) &= ~( heathrowFCSCCCEn | heathrowFCSCCAEn );
		
		if ( mutex  != NULL )
     		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

}

void Heathrow::ModemResetHigh()
{
		IOInterruptState intState;
		
		if ( mutex  != NULL )
        	intState = IOSimpleLockLockDisableInterrupt(mutex);

		
     	*(UInt32*)(heathrowBaseAddress + heathrowFCROffset) |= ( heathrowFCSCCCEn | heathrowFCSCCAEn );        
        
		if ( mutex  != NULL )
        	IOSimpleLockUnlockEnableInterrupt(mutex, intState);

        
}

UInt8 Heathrow::readRegUInt8(unsigned long offset)
{
    return *(UInt8 *)(heathrowBaseAddress + offset);
}

void Heathrow::writeRegUInt8(unsigned long offset, UInt8 data)
{
    *(UInt8 *)(heathrowBaseAddress + offset) = data;
    eieio();
}

void Heathrow::safeWriteRegUInt8(unsigned long offset, UInt8 mask, UInt8 data)
{
  IOInterruptState intState;

  if ( mutex  != NULL )
     intState = IOSimpleLockLockDisableInterrupt(mutex);

  UInt8 currentReg = readRegUInt8(offset);
  currentReg = (currentReg & ~mask) | (data & mask);
  writeRegUInt8(offset, currentReg);
  
  if ( mutex  != NULL )
     IOSimpleLockUnlockEnableInterrupt(mutex, intState);
}

UInt8 Heathrow::safeReadRegUInt8(unsigned long offset)
{
  IOInterruptState intState;
  if ( mutex  != NULL )
     intState = IOSimpleLockLockDisableInterrupt(mutex);
  
  UInt8 currentReg = readRegUInt8(offset);

  if ( mutex  != NULL )
     IOSimpleLockUnlockEnableInterrupt(mutex, intState);

  return (currentReg);  
}

UInt32 Heathrow::readRegUInt32(unsigned long offset)
{
    return lwbrx(heathrowBaseAddress + offset);
}

void Heathrow::writeRegUInt32(unsigned long offset, UInt32 data)
{
  stwbrx(data, heathrowBaseAddress + offset);
  eieio();
}

void Heathrow::safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data)
{
  IOInterruptState intState;

  if ( mutex  != NULL )
     intState = IOSimpleLockLockDisableInterrupt(mutex);

  UInt32 currentReg = readRegUInt32(offset);
  currentReg = (currentReg & ~mask) | (data & mask);
  writeRegUInt32(offset, currentReg);
  
  if ( mutex  != NULL )
     IOSimpleLockUnlockEnableInterrupt(mutex, intState);
}

UInt32 Heathrow::safeReadRegUInt32(unsigned long offset)
{
  IOInterruptState intState;
  if ( mutex  != NULL )
     intState = IOSimpleLockLockDisableInterrupt(mutex);

  UInt32 currentReg = readRegUInt32(offset);

  if ( mutex  != NULL )
     IOSimpleLockUnlockEnableInterrupt(mutex, intState);

  return (currentReg);  
}

// --------------------------------------------------------------------------
// Method: initForPM
//
// Purpose:
//   initialize the driver for power managment and register ourselves with
//   superclass policy-maker
void Heathrow::initForPM (IOService *provider)
{
    PMinit();                   // initialize superclass variables
    provider->joinPMtree(this); // attach into the power management hierarchy

    // KeyLargo has only 2 power states::
    // 0 OFF
    // 1 all ON
    // Pwer state fields:
    // unsigned long	version;		// version number of this struct
    // IOPMPowerFlags	capabilityFlags;	// bits that describe (to interested drivers) the capability of the device in this state
    // IOPMPowerFlags	outputPowerCharacter;	// description (to power domain children) of the power provided in this state
    // IOPMPowerFlags	inputPowerRequirement;	// description (to power domain parent) of input power required in this state
    // unsigned long	staticPower;		// average consumption in milliwatts
    // unsigned long	unbudgetedPower;	// additional consumption from separate power supply (mw)
    // unsigned long	powerToAttain;		// additional power to attain this state from next lower state (in mw)
    // unsigned long	timeToAttain;		// time required to enter this state from next lower state (in microseconds)
    // unsigned long	settleUpTime;		// settle time required after entering this state from next lower state (microseconds)
    // unsigned long	timeToLower;		// time required to enter next lower state from this one (in microseconds)
    // unsigned long	settleDownTime;		// settle time required after entering next lower state from this state (microseconds)
    // unsigned long	powerDomainBudget;	// power in mw a domain in this state can deliver to its children

    // NOTE: all these values are made up since now I do not have areal clue of what to put.
#define number_of_power_states 2

    static IOPMPowerState ourPowerStates[number_of_power_states] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
    };

    // register ourselves with ourself as policy-maker
    if (pm_vars != NULL)
        registerPowerDriver(this, ourPowerStates, number_of_power_states);
}

// Method: setPowerState
//
// VERY IMPORTANT NOTE:
// sleepState(bool) can be called from here or directly. This is NOT an oversight.
// What I am trying to resolve here is a problem with those powerbooks that have
// 2 Heathrow chips. In these machines the main Heathrow should be powered on
// in the CPU driver, and the second here. Since the HeathrowState holds a bit
// to remeber if the state is valid, and such a bit is cleared once the state is
// restored I am sure that I am not going to overwrite a valid state with an (older)
// invalid one.
IOReturn Heathrow::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
    if ( powerStateOrdinal == 0 ) {
        if (heathrowNum == kSecondaryHeathrow) {
            kprintf("Gatwick would be powered off here\n");
            sleepState(true);
        }
        else if (heathrowNum == kPrimaryHeathrow) {
            kprintf("Heathrow would be powered off here\n");
        *((unsigned long*)(heathrowBaseAddress + heathrowFCROffset)) &= ~heathrowFCIOBusEn;
        OSSynchronizeIO();
        *((unsigned long*)(heathrowBaseAddress + heathrowFCROffset)) &= ~heathrowFCATA0Reset;
        OSSynchronizeIO(); 
        }
    }
    if ( powerStateOrdinal == 1 ) {
        if (heathrowNum == kSecondaryHeathrow) {
            kprintf("Gatwick would be powered on here\n");
            sleepState(false);
        }
        else if (heathrowNum == kPrimaryHeathrow) {
            kprintf("Heathrow would be powered on here\n");            		
        *((unsigned long*)(heathrowBaseAddress + heathrowFCROffset)) |=  heathrowFCIOBusEn;
        OSSynchronizeIO();
        *((unsigned long*)(heathrowBaseAddress + heathrowFCROffset)) |= heathrowFCATA0Reset;            
        OSSynchronizeIO(); 
        }
    }
    return IOPMAckImplied;
}

bool Heathrow::installInterrupts(IOService *provider)
{
  IORegistryEntry    *regEntry;
  OSSymbol           *interruptControllerName;
  //IOInterruptAction is typedefed as a function ptr in xnu/iokit/IOKit/IOService.h
  IOInterruptAction  handler;
  AppleNMI           *appleNMI;
  long               nmiSource;
  OSData             *nmiData;
  IOReturn           error;

  // Everything below here is for interrupts, return true if
  // interrupts are not needed.
  if (getPlatform()->getChipSetType() == kChipSetTypePowerExpress) return true;
  
  // get the name of the interrupt controller
  if( (regEntry = provider->childFromPath("interrupt-controller",
	gIODTPlane))) {
    interruptControllerName = (OSSymbol *)IODTInterruptControllerName(regEntry);
    regEntry->release();
  } else
    interruptControllerName = getInterruptControllerName();
  
  // Allocate the interruptController instance.
  interruptController = new HeathrowInterruptController;
  if (interruptController == NULL) return false;
  
  // call the interruptController's init method.
  error = interruptController->initInterruptController(provider, heathrowBaseAddress);
  if (error != kIOReturnSuccess) return false;

  // and clears the interrupt state:
  interruptController->clearAllInterrupts();
  
  handler = interruptController->getInterruptHandlerAddress();
  provider->registerInterrupt(0, interruptController, handler, 0);
  
  provider->enableInterrupt(0);
  
  // Register the interrupt controller so clients can find it.
  getPlatform()->registerInterruptController(interruptControllerName,
					     interruptController);
  
  if (heathrowNum != kPrimaryHeathrow) return true;
  
  // Create the NMI Driver.
  nmiSource = 20;
  nmiData = OSData::withBytes(&nmiSource, sizeof(long));
  appleNMI = new AppleNMI;
  if ((nmiData != 0) && (appleNMI != 0)) {
    appleNMI->initNMI(interruptController, nmiData);
  } 
  
  return true;
}

OSSymbol *Heathrow::getInterruptControllerName(void)
{
  OSSymbol *interruptControllerName;
  
  switch (heathrowNum) {
  case kPrimaryHeathrow :
    interruptControllerName = (OSSymbol *)gIODTDefaultInterruptController;
    break;
    
  case kSecondaryHeathrow :
    interruptControllerName = (OSSymbol *)OSSymbol::withCStringNoCopy("SecondaryInterruptController");
    break;
    
  default:
    interruptControllerName = (OSSymbol *)OSSymbol::withCStringNoCopy("UnknownInterruptController");
    break;
  }
  
  return interruptControllerName;
}

void Heathrow::enableMBATA()
{
    unsigned long heathrowIDs, heathrowFCR;

    heathrowIDs = lwbrx(heathrowBaseAddress + heathrowIDOffset);
    if ((heathrowIDs & 0x0000FF00) == 0x00003000) {
        heathrowFCR = lwbrx(heathrowBaseAddress + heathrowFCROffset);
		//this corresponds to heathrowFCATA1Reset in big Endian(bit 23 in little E)
        heathrowFCR |= 0x00800000;

        stwbrx(heathrowFCR, heathrowBaseAddress + heathrowFCROffset);
        IODelay(100);
    }
}

void Heathrow::powerMediaBay(bool powerOn, UInt8 deviceOn)
{
    unsigned long heathrowIDs;
    unsigned long powerDevice = deviceOn;
    
    //kprintf("Heathrow::powerMediaBay(%s) 0x%02x\n", (powerOn ? "TRUE" : "FALSE"), powerDevice);
    ///kprintf(" 0 Heathrow::powerMediaBay = 0x%08lx\n", lwbrx(heathrowBaseAddress + heathrowFCROffset));

    if (mediaIsOn == powerOn)
        return;
    
    // Align the bits of the power device:
    powerDevice = powerDevice << 26;
    powerDevice &= heathrowFCMediaBaybits;
    
    heathrowIDs = lwbrx(heathrowBaseAddress + heathrowIDOffset);
    if ((heathrowIDs & 0x0000F000) != 0x00007000) {
        unsigned long *heathrowFCR = (unsigned long*)(heathrowBaseAddress + heathrowFCROffset);

        kprintf(" 1 Heathrow::powerMediaBay = 0x%08lx\n", *heathrowFCR);

        // make sure media bay is in reset (MB reset bit is low)
        *heathrowFCR &= ~heathrowFCMBReset;
        eieio();

        if (powerOn) {
            // we are powering on the bay and need a delay between turning on
            // media bay power and enabling the bus
            *heathrowFCR &= ~heathrowFCMBPwr;
            eieio();

            IODelay(50000);
        }

        // to turn on the buses, we ensure all buses are off and then turn on the ata bus
        *heathrowFCR &= ~heathrowFCMediaBaybits;
        eieio();
        *heathrowFCR |= powerDevice;
        eieio();
        
        if (!powerOn) {
            // turn off media bay power
            *heathrowFCR |= heathrowFCMBPwr;
            eieio();
        }
        else {
            // take us out of reset
            *heathrowFCR |= heathrowFCMBReset;
            eieio();
            
            enableMBATA();
        }

        IODelay(50000);
        //kprintf(" 2 Heathrow::powerMediaBay = 0x%08lx\n", *heathrowFCR);
        //kprintf(" 3 Heathrow::powerMediaBay = 0x%08lx\n", lwbrx(heathrowBaseAddress + heathrowFCROffset));
    }

    mediaIsOn = powerOn;
}

void Heathrow::processNub(IOService *nub)
{
  int           cnt, numSources;
  OSArray       *controllerNames, *controllerSources;
  OSSymbol      *interruptControllerName;
  char          *nubName;
  
  nubName = (char *)nub->getName();
  
  if (!strcmp(nubName, "media-bay")) {
      enableMBATA();
  }
  
  // change the interrupt controller name for this nub
  // if it is on the secondary heathrow.
  if (heathrowNum == kPrimaryHeathrow) return;
  
  interruptControllerName = getInterruptControllerName();

  if (!strcmp(nubName, "media-bay")) {
    controllerSources = OSDynamicCast(OSArray, getProperty("vectors-media-bay"));
  } else if (!strcmp(nubName, "ch-a")) {
    controllerSources = OSDynamicCast(OSArray, getProperty("vectors-escc-ch-a"));
  } else if (!strcmp(nubName, "floppy")) {
      controllerSources = OSDynamicCast(OSArray, getProperty("vectors-floppy"));
  } else if (!strcmp(nubName, "ata4")) {
      controllerSources = OSDynamicCast(OSArray, getProperty("vectors-ata4"));
  } else return;
  
  numSources = controllerSources->getCount();
  
  controllerNames = OSArray::withCapacity(numSources);
  for (cnt = 0; cnt < numSources; cnt++) {
    controllerNames->setObject(interruptControllerName);
  }

  nub->setProperty(gIOInterruptControllersKey, controllerNames);
  nub->setProperty(gIOInterruptSpecifiersKey, controllerSources);
}

// Set the color of the front panel light on desktop Gossamers.  
void Heathrow::setChassisLightFullpower(bool fullpwr)
{
    if (fullpwr)
    {
	*(UInt32*)(heathrowBaseAddress + kChassisLightColor) = 0x00000000;
	eieio();
    }
    else
    {
	*(UInt32*)(heathrowBaseAddress + kChassisLightColor) = 0xffffffff;
	eieio();
    }
}


// If sleepMe is true places heatrow to sleep,
// Otherwise wakes it up.
void Heathrow::sleepState(bool sleepMe)
{
    if (sleepMe) {
        // Saves the state and creates the conditions for sleep:

        // Disables and saves all the interrupts:
        //kprintf("Heathrow::sleepState saveInterruptState\n");
        saveInterruptState();

        // Saves all the DMA registers:
        //kprintf("Heathrow::sleepState saveDMAState\n");
        saveDMAState();

        // Saves the VIA registers:
        //kprintf("Heathrow::sleepState saveVIAState\n");
        saveVIAState();

        // Saves the GP registers:
        //kprintf("Heathrow::sleepState saveGPState\n");
        saveGPState();

        // Defines the state as valid:
        savedState.thisStateIsValid = true;
    }
    else if (savedState.thisStateIsValid) {
        // Restores the GP registers:
        //kprintf("Heathrow::sleepState restoreGPState\n");
        restoreGPState();

        // Wakes up and restores the state:
        //kprintf("Heathrow::sleepState restoreVIAState\n");
        restoreVIAState();

        // Restores the DMA registers:
        //kprintf("Heathrow::sleepState restoreDMAState\n");
        restoreDMAState();

        // Restores and enables the interrupts:
        //kprintf("Heathrow::sleepState restoreInterruptState\n");
        restoreInterruptState();

        // This state is no more valid:
        savedState.thisStateIsValid = false;

        // Turn on the media bay if necessary.
        enableMBATA();
    }
}

void Heathrow::saveInterruptState()
{
    // Save the interrupt state
    savedState.interruptMask1 = *(UInt32*)(heathrowBaseAddress + kMask1Offset);
    eieio();
    savedState.interruptMask2 = *(UInt32*)(heathrowBaseAddress + kMask2Offset);
    eieio();
}

void Heathrow::restoreInterruptState()
{
    // Clears all the possible pending interrupts
    *(UInt32*)(heathrowBaseAddress + kClear1Offset) = 0xFFFFFFFF;
    eieio();
    *(UInt32*)(heathrowBaseAddress + kClear2Offset) = 0xFFFFFFFF;
    eieio();

    // Restores the interrupts
    *(UInt32*)(heathrowBaseAddress + kMask1Offset) = savedState.interruptMask1;
    eieio();
    *(UInt32*)(heathrowBaseAddress + kMask2Offset) = savedState.interruptMask2;
    eieio();

    // Clears all the possible pending interrupts (again)
    *(UInt32*)(heathrowBaseAddress + kClear1Offset) = 0xFFFFFFFF;
    eieio();
    *(UInt32*)(heathrowBaseAddress + kClear2Offset) = 0xFFFFFFFF;
    eieio();
}

void Heathrow::saveGPState()
{
    savedState.featureControlReg = *(UInt32*)(heathrowBaseAddress + heathrowFCROffset);
    savedState.auxControlReg     = *(UInt32*)(heathrowBaseAddress + heathrowAUXFCROffset);
}

void Heathrow::restoreGPState()
{
    *(UInt32*)(heathrowBaseAddress + heathrowFCROffset) = savedState.featureControlReg;
    eieio();
    IODelay(1000);
    *(UInt32*)(heathrowBaseAddress + heathrowAUXFCROffset) = savedState.auxControlReg;
    eieio();
    IODelay(1000);
}

void Heathrow::saveDMAState()
{
    int i;
    UInt32 channelOffset;
    
    for (i = 0, channelOffset = 0; i <= 12; i++, channelOffset += 0x0100)
    {
        volatile DBDMAChannelRegisters*	currentChannel;

        currentChannel = (volatile DBDMAChannelRegisters *) (heathrowBaseAddress + 0x8000 + channelOffset);

        savedState.savedDBDMAState[i].commandPtrLo = IOGetDBDMACommandPtr(currentChannel);
        savedState.savedDBDMAState[i].interruptSelect = IOGetDBDMAInterruptSelect(currentChannel);
        savedState.savedDBDMAState[i].branchSelect = IOGetDBDMABranchSelect(currentChannel);
        savedState.savedDBDMAState[i].waitSelect = IOGetDBDMAWaitSelect(currentChannel);
    }
}

void Heathrow::restoreDMAState()
{
    int i;
    UInt32 channelOffset;

    for (i = 0, channelOffset = 0; i <=12; i++, channelOffset += 0x0100)
    {
        volatile DBDMAChannelRegisters* currentChannel;

        currentChannel = (volatile DBDMAChannelRegisters *) (heathrowBaseAddress + 0x8000 + channelOffset);

        IODBDMAReset((IODBDMAChannelRegisters*)currentChannel);
        IOSetDBDMACommandPtr(currentChannel, savedState.savedDBDMAState[i].commandPtrLo);
        IOSetDBDMAInterruptSelect(currentChannel, savedState.savedDBDMAState[i].interruptSelect);
        IOSetDBDMABranchSelect(currentChannel, savedState.savedDBDMAState[i].branchSelect);
        IOSetDBDMAWaitSelect(currentChannel, savedState.savedDBDMAState[i].waitSelect);
    }
}

void Heathrow::saveVIAState(void)
{
    UInt8* viaBase = (UInt8*)heathrowBaseAddress + heathrowVIAOffset;
    UInt8* savedViaState = savedState.savedVIAState;

    // Save VIA state.  These registers don't seem to get restored to any known state.
    savedViaState[0] = *(UInt8*)(viaBase + vBufA);
    savedViaState[1] = *(UInt8*)(viaBase + vDIRA);
    savedViaState[2] = *(UInt8*)(viaBase + vBufB);
    savedViaState[3] = *(UInt8*)(viaBase + vDIRB);
    savedViaState[4] = *(UInt8*)(viaBase + vPCR);
    savedViaState[5] = *(UInt8*)(viaBase + vACR);
    savedViaState[6] = *(UInt8*)(viaBase + vIER);
    savedViaState[7] = *(UInt8*)(viaBase + vT1C);
    savedViaState[8] = *(UInt8*)(viaBase + vT1CH);
}

void Heathrow::restoreVIAState(void)
{
    UInt8* viaBase = (UInt8*)heathrowBaseAddress + heathrowVIAOffset;
    UInt8* savedViaState = savedState.savedVIAState;

    // Restore VIA state.  These registers don't seem to get restored to any known state.
    *(UInt8*)(viaBase + vBufA) = savedViaState[0];
    eieio();
    *(UInt8*)(viaBase + vDIRA) = savedViaState[1];
    eieio();
    *(UInt8*)(viaBase + vBufB) = savedViaState[2];
    eieio();
    *(UInt8*)(viaBase + vDIRB) = savedViaState[3];
    eieio();
    *(UInt8*)(viaBase + vPCR) = savedViaState[4];
    eieio();
    *(UInt8*)(viaBase + vACR) = savedViaState[5];
    eieio();
    *(UInt8*)(viaBase + vIER) = savedViaState[6];
    eieio();
    *(UInt8*)(viaBase + vT1C) = savedViaState[7];
    eieio();
    *(UInt8*)(viaBase + vT1CH) = savedViaState[8];
    eieio();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors(HeathrowInterruptController, IOInterruptController);

IOReturn HeathrowInterruptController::initInterruptController(IOService *provider, IOLogicalAddress iBase)
{
  int cnt;

  interruptControllerBase = iBase;
  parentNub = provider;
  
  // Allocate the task lock.
  taskLock = IOLockAlloc();
  if (taskLock == 0) return kIOReturnNoResources;
  
  // Allocate the memory for the vectors
  vectors = (IOInterruptVector *)IOMalloc(kNumVectors * sizeof(IOInterruptVector));
  if (vectors == NULL) {
    IOLockFree(taskLock);
    return kIOReturnNoMemory;
  }
  bzero(vectors, kNumVectors * sizeof(IOInterruptVector));
  
  // Allocate locks 
  for (cnt = 0; cnt < kNumVectors; cnt++) {
    vectors[cnt].interruptLock = IOLockAlloc();
    if (vectors[cnt].interruptLock == NULL) {
      IOLockFree(taskLock);
      for (cnt = 0; cnt < kNumVectors; cnt++) {
	if (vectors[cnt].interruptLock != NULL)
	  IOLockFree(vectors[cnt].interruptLock);
      }
      return kIOReturnNoResources;
    }
  }

  // Setup the registers accessors
  events1Reg = (unsigned long)(interruptControllerBase + kEvents1Offset);
  events2Reg = (unsigned long)(interruptControllerBase + kEvents2Offset);
  mask1Reg   = (unsigned long)(interruptControllerBase + kMask1Offset);
  mask2Reg   = (unsigned long)(interruptControllerBase + kMask2Offset);
  clear1Reg  = (unsigned long)(interruptControllerBase + kClear1Offset);
  clear2Reg  = (unsigned long)(interruptControllerBase + kClear2Offset);
  levels1Reg = (unsigned long)(interruptControllerBase + kLevels1Offset);
  levels2Reg = (unsigned long)(interruptControllerBase + kLevels2Offset);
   
  return kIOReturnSuccess;
}

void HeathrowInterruptController::clearAllInterrupts(void)
{
  // Initialize the registers.
  
  // Disable all interrupts.
  stwbrx(0x00000000, mask1Reg);
  stwbrx(0x00000000, mask2Reg);
  eieio();
  
  // Clear all pending interrupts.
  stwbrx(0xFFFFFFFF, clear1Reg);
  stwbrx(0xFFFFFFFF, clear2Reg);
  eieio();
  
  // Disable all interrupts. (again?)
  stwbrx(0x00000000, mask1Reg);
  stwbrx(0x00000000, mask2Reg);
  eieio();
}

//returns the address of the handler function and casts it into type  IOInterruptAction
//IOInterruptAction is typedeffed in iokit/IOKit/IOService.h as
//ypedef void (*IOInterruptAction)( OSObject * target, void * refCon,
//				   IOService * nub, int source );
IOInterruptAction HeathrowInterruptController::getInterruptHandlerAddress(void)
{
  return (IOInterruptAction)&HeathrowInterruptController::handleInterrupt;
}

IOReturn HeathrowInterruptController::handleInterrupt(void * /*refCon*/,
						      IOService * /*nub*/,
						      int /*source*/)
{
  int               done;
  long              events, vectorNumber;
  //Defined as a struct in iokit/IOKit/IOInterruptController.h, one of its elements is handler which is
  //of type IOInterruptHandler
  IOInterruptVector *vector;
  unsigned long     maskTmp;
  
  do {
    done = 1;
    
    // Do all the sources for events1, plus any pending interrupts.
    // Also add in the "level" sensitive sources
    maskTmp = lwbrx(mask1Reg);
    //clear external interrupts bits InterruptEvents register, which is read-only
    events = lwbrx(events1Reg) & ~kTypeLevelMask;
    //now takes care of external interrupts bits with consideration of InterruptLevels register
    events |= lwbrx(levels1Reg) & maskTmp & kTypeLevelMask;
	//now takes care of pending bits
    events |= pendingEvents1 & maskTmp;
    pendingEvents1 = 0;
    eieio();
    
    // Since we have to clear the level'd one clear the current edge's too.
    //set external interrupts bits to 1 and as a result clears all the those bits in InterruptEvents register
    stwbrx(kTypeLevelMask | events, clear1Reg);
    eieio();
    
    if (events) done = 0;
    
    while (events) {
      vectorNumber = 31 - cntlzw(events);
      events ^= (1 << vectorNumber);
      vector = &vectors[vectorNumber];
      
      vector->interruptActive = 1;
      sync();
      isync();
      if (!vector->interruptDisabledSoft) {
	isync();
	
	// Call the handler if it exists.
	if (vector->interruptRegistered) {
	//handler is of type IOInterruptHandler which is typedefed in iokit/IOKit/IOInterrupts.h as a func ptr
	  vector->handler(vector->target, vector->refCon,
			  vector->nub, vector->source);
	}
      } else {
	// Hard disable the source.
	vector->interruptDisabledHard = 1;
	disableVectorHard(vectorNumber, vector);
      }
      
      vector->interruptActive = 0;
    }
    
    // Do all the sources for events2, plus any pending interrupts.
    maskTmp = lwbrx(mask2Reg);
    events = lwbrx(events2Reg);
    events |= pendingEvents1 & maskTmp;
    pendingEvents2 = 0;
    eieio();
    
    if (events) {
      done = 0;
      stwbrx(events, clear2Reg);
      eieio();
    }
    
    while (events) {
      vectorNumber = 31 - cntlzw(events);
      events ^= (1 << vectorNumber);
      vector = &vectors[vectorNumber + kVectorsPerReg];
      
      vector->interruptActive = 1;
      sync();
      isync();
      if (!vector->interruptDisabledSoft) {
	isync();
	
	// Call the handler if it exists.
	if (vector->interruptRegistered) {
	  vector->handler(vector->target, vector->refCon,
			  vector->nub, vector->source);
	}
      } else {
	// Hard disable the source.
	vector->interruptDisabledHard = 1;
	disableVectorHard(vectorNumber + kVectorsPerReg, vector);
      }
      
      vector->interruptActive = 0;
    }
  } while (!done);
  
  return kIOReturnSuccess;
}

bool HeathrowInterruptController::vectorCanBeShared(long /*vectorNumber*/, IOInterruptVector */*vector*/)
{
  return true;
}

int HeathrowInterruptController::getVectorType(long vectorNumber, IOInterruptVector */*vector*/)
{
  int interruptType;
  
  if (kTypeLevelMask & (1 << vectorNumber)) {
    interruptType = kIOInterruptTypeLevel;
  } else {
    interruptType = kIOInterruptTypeEdge;
  }
  
  return interruptType;
}

void HeathrowInterruptController::disableVectorHard(long vectorNumber, IOInterruptVector */*vector*/)
{
  unsigned long     maskTmp;
  boolean_t         interruptState = ml_set_interrupts_enabled(FALSE);

  // Turn the source off at hardware.
  if (vectorNumber < kVectorsPerReg) {
    maskTmp = lwbrx(mask1Reg);
    maskTmp &= ~(1 << vectorNumber);
    stwbrx(maskTmp, mask1Reg);
    eieio();
  } else {
    vectorNumber -= kVectorsPerReg;
    maskTmp = lwbrx(mask2Reg);
    maskTmp &= ~(1 << vectorNumber);
    stwbrx(maskTmp, mask2Reg);
    eieio();
  }
  
  ml_set_interrupts_enabled(interruptState);
}

void HeathrowInterruptController::enableVector(long vectorNumber,
					       IOInterruptVector *vector)
{
  unsigned long     maskTmp;
  boolean_t         interruptState;

  if (vectorNumber < kVectorsPerReg) {

    interruptState = ml_set_interrupts_enabled(FALSE);

    maskTmp = lwbrx(mask1Reg);
    maskTmp |= (1 << vectorNumber);
    stwbrx(maskTmp, mask1Reg);
    eieio();
    
    ml_set_interrupts_enabled(interruptState);

    if ((lwbrx(levels1Reg) & (1 << vectorNumber)) && (vector != NULL)) {
      // lost the interrupt
      causeVector(vectorNumber, vector);
    }
  } else {
    vectorNumber -= kVectorsPerReg;

    interruptState = ml_set_interrupts_enabled(FALSE);

    maskTmp = lwbrx(mask2Reg);
    maskTmp |= (1 << vectorNumber);
    stwbrx(maskTmp, mask2Reg);
    eieio();

    ml_set_interrupts_enabled(interruptState);

    if ((lwbrx(levels2Reg) & (1 << vectorNumber)) && (vector != NULL)) {
      // lost the interrupt
      causeVector(vectorNumber + kVectorsPerReg, vector);
    }
  }
}

void HeathrowInterruptController::causeVector(long vectorNumber,
					      IOInterruptVector */*vector*/)
{
  boolean_t  interruptState = ml_set_interrupts_enabled(FALSE);
  
  if (vectorNumber < kVectorsPerReg) {
    pendingEvents1 |= 1 << vectorNumber;
  } else {
    vectorNumber -= kVectorsPerReg;
    pendingEvents2 |= 1 << vectorNumber;
  }
  
  ml_set_interrupts_enabled(interruptState);
    
  parentNub->causeInterrupt(0);
}
