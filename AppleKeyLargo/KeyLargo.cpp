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
 *  DRI: Josh de Cesare
 *
 */

#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/ppc/IODBDMA.h>

#include "USBKeyLargo.h"
#include "KeyLargo.h"

#define RevertEndianness32(X) ((X & 0x000000FF) << 24) | \
((X & 0x0000FF00) << 16) | \
((X & 0x00FF0000) >> 16) | \
((X & 0xFF000000) >> 24)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super AppleMacIO

OSDefineMetaClassAndStructors(KeyLargo, AppleMacIO);

bool KeyLargo::init(OSDictionary * properties)
{
  // Just to be sure we are not going to use the
  // backup structures by mistake let's invalidate
  // their contents.
  savedKeyLargState.thisStateIsValid = false;
  
  // And by default the wireless slot is powered on:
  cardStatus.cardPower = true;
  
  // sets the usb pointers to null:
  int i;
  for (i = 0; i < kNumUSB; i++)
    usbBus[i] = NULL;
  
  return super::init(properties);
}

bool KeyLargo::start(IOService *provider)
{
  IORegistryEntry *viaPMU;
  OSData          *tmpData;
  UInt32          pmuVersion;

    // Get the bus speed from the Device Tree.
  IORegistryEntry *entry;
  
  entry = fromPath("/", gIODTPlane);
  tmpData = OSDynamicCast(OSData, entry->getProperty("clock-frequency"));
  if (tmpData == 0) return false;
  busSpeed = *(unsigned long *)tmpData->getBytesNoCopy();

  // callPlatformFunction symbols
  keyLargo_resetUniNEthernetPhy = OSSymbol::withCString("keyLargo_resetUniNEthernetPhy");
  keyLargo_restoreRegisterState = OSSymbol::withCString("keyLargo_restoreRegisterState");
  keyLargo_syncTimeBase = OSSymbol::withCString("keyLargo_syncTimeBase");
  keyLargo_saveRegisterState = OSSymbol::withCString("keyLargo_saveRegisterState");
  keyLargo_turnOffIO = OSSymbol::withCString("keyLargo_turnOffIO");
  keyLargo_writeRegUInt8 = OSSymbol::withCString("keyLargo_writeRegUInt8");
  keyLargo_safeWriteRegUInt32 = OSSymbol::withCString("keyLargo_safeWriteRegUInt32");
  keyLargo_safeReadRegUInt32 = OSSymbol::withCString("keyLargo_safeReadRegUInt32");
  keyLargo_powerMediaBay = OSSymbol::withCString("powerMediaBay");

  // Call MacIO's start.
  if (!super::start(provider))
    return false;

  // sets up the mutex lock:
  mutex = IOSimpleLockAlloc();

  if (mutex != NULL)
     IOSimpleLockInit( mutex );

  // get the base address of KeyLargo.
  keyLargoBaseAddress = fMemory->getVirtualAddress();

  tmpData = OSDynamicCast(OSData, provider->getProperty("device-id"));
  if (tmpData == 0) return false;
  keyLargoDeviceId = *(long *)tmpData->getBytesNoCopy();
  
  tmpData = OSDynamicCast(OSData, provider->getProperty("revision-id"));
  if (tmpData == 0) return false;
  keyLargoVersion = *(long *)tmpData->getBytesNoCopy();
  
  enableCells();


  // Make nubs for the children.
  publishBelow(provider);



  // Get the PMU version to determine support for the WatchDog timer.
  pmuVersion = 0;
  viaPMU = provider->childFromPath("via-pmu", gIODTPlane);
  if (viaPMU != 0) {
    tmpData = OSDynamicCast(OSData, viaPMU->getProperty("pmu-version"));
    if (tmpData != 0) {
      pmuVersion = *(UInt32 *)tmpData->getBytesNoCopy();
      
      if (((pmuVersion & 0x000000FF) == 0x0C) &&
	  (((pmuVersion >> 8) & 0x0000FFFF) >= 0x0000D044)) {
	watchDogTimer = KeyLargoWatchDogTimer::withKeyLargo(this);
      }
    }
  }
  
  // at power on the media bay is on:
  mediaIsOn = true;
  
  registerService();
  
  
  // initialize for Power Management
  initForPM(provider);
  
  // creates the USBPower handlers:
  int i;
  for (i = 0; i < kNumUSB; i++) {
    usbBus[i] = new USBKeyLargo;
    
    if (usbBus[i] != NULL) {
      if ( usbBus[i]->init() && usbBus[i]->attach(this)) {
	usbBus[i]->initForBus(i);                 
      }
      else {
	usbBus[i]->release();
      }  
    }
  }

  
  return true;
}

void KeyLargo::stop(IOService *provider)
{
  // releases the USBPower handlers:
  int i;
  for (i = 0; i < kNumUSB; i++) {
    if (usbBus[i] != NULL) {
      usbBus[i]->release();
    }
  }

  if (mutex != NULL)
    IOSimpleLockFree( mutex );
}

long long KeyLargo::syncTimeBase(void)
{
  unsigned long   cnt;
  unsigned long   gtLow, gtHigh, gtHigh2;
  unsigned long   tbLow, tbHigh, tbHigh2;
  long long       tmp, diffTicks, ratioLow, ratioHigh;
  
  ratioLow = (busSpeed << 32) / (kKeyLargoGTimerFreq * 4);
  ratioHigh = ratioLow >> 32;
  ratioLow &= 0xFFFFFFFFULL;
  
  // Save the old time base.
  do {
    tbHigh  = mftbu();
    tbLow   = mftb();
    tbHigh2 = mftbu();
  } while (tbHigh != tbHigh2);
  diffTicks = ((long long)tbHigh << 32) | tbLow;
  
  // Do the sync twice to make sure it is cached.
  for (cnt = 0; cnt < 2; cnt++) {
    // Read the Global Counter.
    do {
      gtHigh  = readRegUInt32(kKeyLargoCounterHiOffset);
      gtLow   = readRegUInt32(kKeyLargoCounterLoOffset);
      gtHigh2 = readRegUInt32(kKeyLargoCounterHiOffset);
    } while (gtHigh != gtHigh2);
    
    tmp = gtHigh * ratioLow + gtLow * ratioHigh +
      ((gtLow * ratioLow + 0x080000000ULL) >> 32);
    tbHigh = gtHigh * ratioHigh + (tmp >> 32);
    tbLow = tmp & 0xFFFFFFFFULL;
    
    mttb(0);
    mttbu(tbHigh);
    mttb(tbLow);
  }
  
  diffTicks = (((long long)tbHigh << 32) | tbLow) - diffTicks;
  
  return diffTicks;
}

void KeyLargo::turnOffKeyLargoIO(bool restart)
{
    UInt32 regTemp;

    if (!restart) {
        kprintf("KeyLargo::turnOffIO( --FALSE-- )\n");
    }

    if (!restart) {
        // turning off the USB clocks:
        regTemp = readRegUInt32(kKeyLargoFCR0);
        regTemp |= kKeyLargoFCR0SleepBitsSet;
        writeRegUInt32(kKeyLargoFCR0, regTemp);
        IODelay(1000);
    }

    regTemp = readRegUInt32(kKeyLargoFCR0);
    regTemp |= kKeyLargoFCR0SleepBitsSet;
    regTemp &= ~kKeyLargoFCR0SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR0, regTemp);

    // Set MediaBay Dev1 Enable before IDE Resets.
    regTemp = readRegUInt32(kKeyLargoMediaBay);
    regTemp |= kKeyLargoMB0DevEnable;
    writeRegUInt32(kKeyLargoMediaBay, regTemp);

    regTemp = readRegUInt32(kKeyLargoFCR1);
    regTemp |= kKeyLargoFCR1SleepBitsSet;
    regTemp &= ~kKeyLargoFCR1SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR1, regTemp);

    regTemp = readRegUInt32(kKeyLargoFCR2);
    regTemp |= kKeyLargoFCR2SleepBitsSet;
    regTemp &= ~kKeyLargoFCR2SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR2, regTemp);

    regTemp = readRegUInt32(kKeyLargoFCR3);
    if (keyLargoVersion >= kKeyLargoVersion2) {
        regTemp |= kKeyLargoFCR3ShutdownPLL2X;
        if (!restart) {
            regTemp |= kKeyLargoFCR3ShutdownPLLTotal;
        }
    }
    if (restart) {
        regTemp |= kKeyLargoFCR3RestartBitsSet;
        regTemp &= ~kKeyLargoFCR3RestartBitsClear;
    } else {
        regTemp |= kKeyLargoFCR3SleepBitsSet;
        regTemp &= ~kKeyLargoFCR3SleepBitsClear;
    }
    writeRegUInt32(kKeyLargoFCR3, regTemp);

    if (restart) {
        // turning on the USB clocks
        regTemp = readRegUInt32(kKeyLargoFCR0);
        regTemp &= ~kKeyLargoFCR0USBRefSuspend;
        writeRegUInt32(kKeyLargoFCR0, regTemp);
        IODelay(1000);

        // enables the keylargo cells we are going to need:
        enableCells();
    }
    
    if (restart) {
        kprintf("KeyLargo::turnOffIO( --TRUE-- )\n");
    }
}

void KeyLargo::turnOffPangeaIO(bool restart)
{
    UInt32 regTemp;

    // FCR0
    regTemp = readRegUInt32(kFCR0Offset);
    regTemp |= kPangeaFCR0SleepBitsSet;
    regTemp &= ~kPangeaFCR0SleepBitsClear;
    writeRegUInt32(kFCR0Offset, regTemp);

    // FCR1
    regTemp = readRegUInt32(kFCR1Offset);
    regTemp |= kPangeaFCR1SleepBitsSet;
//regTemp |= kFCR1_UltraIDE_Reset_l;   
    regTemp &= ~kPangeaFCR1SleepBitsClear;
    writeRegUInt32(kFCR1Offset, regTemp);

    // FCR2
    regTemp = readRegUInt32(kFCR2Offset);
    regTemp |= kPangeaFCR2SleepBitsSet;
    regTemp &= ~kPangeaFCR2SleepBitsClear;
    writeRegUInt32(kFCR2Offset, regTemp);

    // FCR3
    regTemp = readRegUInt32(kFCR3Offset);
    regTemp |= kPangeaFCR3SleepBitsSet;
    regTemp &= ~kPangeaFCR3SleepBitsClear;
    writeRegUInt32(kFCR3Offset, regTemp);

    // FCR4
    regTemp = readRegUInt32(kFCR4Offset);
    regTemp |= kPangeaFCR4SleepBitsSet;
    regTemp &= ~kPangeaFCR4SleepBitsClear;
    writeRegUInt32(kFCR4Offset, regTemp);
}

// Uncomment the following define if you need to see the state of
// the media bay register:
//#define LOG_MEDIA_BAY_TRANSACTIONS

void KeyLargo::powerMediaBay(bool powerOn, UInt8 powerDevice)
{
    UInt32 regTemp;
    UInt32 whichDevice = powerDevice;
    
    IOLog("KeyLargo::powerMediaBay(%s) 0x%02x\n", (powerOn ? "TRUE" : "FALSE"), powerDevice);

    if (mediaIsOn == powerOn) {
#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog("KeyLargo::powerMediaBay mbreg = 0x%08lx\n",readRegUInt32(kKeyLargoMediaBay));
#endif
        return;
    }
    
    // Makes sure that the reset bit is off:
    regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 0 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

    regTemp &= (~(kKeyLargoMB0DevReset));

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 1 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

    writeRegUInt32(kKeyLargoMediaBay, regTemp);

    if (powerOn) {
        // we are powering on the bay and need a delay between turning on
        // media bay power and enabling the bus
        regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 2 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

        regTemp &= (~(kKeyLargoMB0DevPower));

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 3 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        writeRegUInt32(kKeyLargoMediaBay, regTemp);
	IODelay(500);
    }

    // to turn on the buses, we ensure all buses are off and then turn on the requested bus
    regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 4 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

    regTemp &= (~(kKeyLargoMB0DevEnable));

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 5 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
    writeRegUInt32(kKeyLargoMediaBay, regTemp);
    IODelay(500);

    // and turns on the right bus:
    regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 6 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
    regTemp |= (whichDevice << 11);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 7 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

    writeRegUInt32(kKeyLargoMediaBay, regTemp);
    IODelay(500);

    if (!powerOn) {
        // turn off media bay power:
        regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 8 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        regTemp |= kKeyLargoMB0DevPower;

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 9 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        writeRegUInt32(kKeyLargoMediaBay, regTemp);
    }
    else {
        // take us out of reset:
        regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 10 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        regTemp |= kKeyLargoMB0DevReset;

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 11 KeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        writeRegUInt32(kKeyLargoMediaBay, regTemp);

        IODelay(500);

        // And also takes the ATA bus out of reset:
        regTemp = readRegUInt32(kKeyLargoFCR1);
        regTemp |= kKeyLargoFCR1EIDE0Reset;
        writeRegUInt32(kKeyLargoFCR1, regTemp);
    }
    IODelay(500);
    
    mediaIsOn = powerOn;
}

void KeyLargo::powerWireless(bool powerOn)
{
    // if we are already in the wanted power
    // state just exit:
    if (cardStatus.cardPower == powerOn)
        return;

    if (powerOn) {
        // power the card on by setting the registers with their
        // back-up copy:
        writeRegUInt8(kKeyLargoExtIntGPIOBase + 10, cardStatus.wirelessCardReg[0]);
        writeRegUInt8(kKeyLargoExtIntGPIOBase + 13, cardStatus.wirelessCardReg[1]);
        writeRegUInt8(kKeyLargoGPIOBase + 13, cardStatus.wirelessCardReg[2]);
        writeRegUInt8(kKeyLargoGPIOBase + 14, cardStatus.wirelessCardReg[3]);
        writeRegUInt8(kKeyLargoGPIOBase + 15, cardStatus.wirelessCardReg[4]);
    }
    else {
        // makes a copy of all the wireless slot register and
        // clears the registers:
        cardStatus.wirelessCardReg[0] = readRegUInt8(kKeyLargoExtIntGPIOBase + 10);
        cardStatus.wirelessCardReg[1] = readRegUInt8(kKeyLargoExtIntGPIOBase + 13);
        cardStatus.wirelessCardReg[2] = readRegUInt8(kKeyLargoGPIOBase + 13);
        cardStatus.wirelessCardReg[3] = readRegUInt8(kKeyLargoGPIOBase + 14);
        cardStatus.wirelessCardReg[4] = readRegUInt8(kKeyLargoGPIOBase + 15);

        writeRegUInt8(kKeyLargoExtIntGPIOBase + 10, 0);
        writeRegUInt8(kKeyLargoExtIntGPIOBase + 13, 0);
        writeRegUInt8(kKeyLargoGPIOBase + 13, 0);
        writeRegUInt8(kKeyLargoGPIOBase + 14, 0);
        writeRegUInt8(kKeyLargoGPIOBase + 15, 0);
    }

    // and updates the status:
    cardStatus.cardPower = powerOn;
}

void KeyLargo::enableCells()
{
    UInt32 *fcr0Address = (UInt32*)(keyLargoBaseAddress + 0x38); // offset on the scc register KFCR00
    UInt32 *fcr2Address = (UInt32*)(keyLargoBaseAddress + 0x40); // offset on the modem register KFCR20
    unsigned int debugFlags;
    UInt32 bitMask;
    UInt32 oldValue;

    if (!PE_parse_boot_arg("debug", &debugFlags))
        debugFlags = 0;

    if( debugFlags & 0x18) {
        // Enables the SCC cell:
        bitMask = (1 << 6);     // Enables SCC
        //bitMask |= (1 << 5);  // Enables SCC B (which I do not belive is in use i any core99)
        bitMask |= (1 << 4);    // Enables SCC A
        bitMask |= (1 << 1);    // Enables SCC Interface A
        //bitMask |= (1 << 0);  // Enables SCC Interface B (which I do not belive is in use i any core99)

        oldValue = *fcr0Address;
        eieio();

        oldValue |= RevertEndianness32(bitMask);

        *fcr0Address = oldValue;
        eieio();
    }

    // Enables the mpic cell:
    bitMask = (1 << 17);     // Enables MPIC

    oldValue = *fcr2Address;
    eieio();

    oldValue |= RevertEndianness32(bitMask);

    *fcr2Address = oldValue;
    eieio();
}

// NOTE: Marco changed the save and restore state to save all keylargo registers.
// this is a temporary fix, the real code should save and restore all registers
// in each specific driver (VIA, MPIC ...) However for now it is easier to follow
// the MacOS9 policy to do everything here.
void KeyLargo::saveRegisterState(void)
{
    saveKeyLargoState();
    saveVIAState();
    powerWireless(false);
    savedKeyLargState.thisStateIsValid = true;
}

void KeyLargo::restoreRegisterState(void)
{
    if (savedKeyLargState.thisStateIsValid) {
        restoreKeyLargoState();
        restoreVIAState();
        powerWireless(true);
    }

    savedKeyLargState.thisStateIsValid = false;
}

UInt8 KeyLargo::readRegUInt8(unsigned long offest)
{
    return *(UInt8 *)(keyLargoBaseAddress + offest);
}

void KeyLargo::writeRegUInt8(unsigned long offest, UInt8 data)
{
    *(UInt8 *)(keyLargoBaseAddress + offest) = data;
    eieio();
}

UInt32 KeyLargo::readRegUInt32(unsigned long offest)
{
    return lwbrx(keyLargoBaseAddress + offest);
}

void KeyLargo::writeRegUInt32(unsigned long offest, UInt32 data)
{
  stwbrx(data, keyLargoBaseAddress + offest);
  eieio();
}

void KeyLargo::safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data)
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

UInt32 KeyLargo::safeReadRegUInt32(unsigned long offset)
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
void KeyLargo::initForPM (IOService *provider)
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
// Purpose:
IOReturn KeyLargo::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
    // Do not do anything if the state is inavalid.
    if (powerStateOrdinal >= 2)
        return IOPMNoSuchState;

    if ( powerStateOrdinal == 0 ) {
        kprintf("KeyLargo would be powered off here\n");
    }
    if ( powerStateOrdinal == 1 ) {
        kprintf("KeyLargo would be powered on here\n");
    }
    return IOPMAckImplied;
}

#define MPIC_OFFSET 0x40000
#define VIA_OFFSET 0x16000

// Method: saveKeyLargoState
//
// Purpose:
//        saves the state of all the meaningful registers into a local buffer.
//    this method is almost a copy and paste of the orignal MacOS9 function
//    SaveKeyLargoState. The code does not care bout the endiannes of the
//    registers since the values are not meaningful, we just wish to save them
//    and restore them.
void KeyLargo::saveKeyLargoState(void)
{
    KeyLargoMPICState*		savedKeyLargoMPICState;
    KeyLargoGPIOState*		savedKeyLargoGPIOState;
    KeyLargoConfigRegistersState*	savedKeyLargoConfigRegistersState;
    KeyLargoDBDMAState*		savedKeyLargoDBDMAState;
    KeyLargoAudioState*		savedKeyLargoAudioState;
    KeyLargoI2SState*		savedKeyLargoI2SState;
    UInt32				channelOffset;
    int				i;

    // base of the keylargo regiters.
    UInt8* keyLargoBaseAddr = (UInt8*)keyLargoBaseAddress;
    UInt8* mpicBaseAddr = (UInt8*)keyLargoBaseAddress + MPIC_OFFSET;

    // Save GPIO portion of KeyLargo.

    savedKeyLargoGPIOState = &savedKeyLargState.savedGPIOState;

    savedKeyLargoGPIOState->gpioLevels[0] = *(UInt32 *)(keyLargoBaseAddr + 0x50);
    savedKeyLargoGPIOState->gpioLevels[1] = *(UInt32 *)(keyLargoBaseAddr + 0x54);

    for (i = 0; i < 18; i++)
    {
        savedKeyLargoGPIOState->extIntGPIO[i] = *(UInt8 *)(keyLargoBaseAddr + 0x58 + i);
    }

    for (i = 0; i < 17; i++)
    {
        savedKeyLargoGPIOState->gpio[i] = *(UInt8 *)(keyLargoBaseAddr + 0x6A + i);
    }

    // Save Audio registers.

    savedKeyLargoAudioState = &savedKeyLargState.savedAudioState;

    for (i = 0, channelOffset = 0; i < 25; i++, channelOffset += 0x0010)
    {
        savedKeyLargoAudioState->audio[i] = *(UInt32 *) (keyLargoBaseAddr + 0x14000 + channelOffset);
    }

    // Save I2S registers.

    savedKeyLargoI2SState = &savedKeyLargState.savedI2SState;

    for (i = 0, channelOffset = 0; i < 10; i++, channelOffset += 0x0010)
    {
        savedKeyLargoI2SState->i2s[i] = *(UInt32 *) (keyLargoBaseAddr + 0x10000 + channelOffset);
        savedKeyLargoI2SState->i2s[i + 1] = *(UInt32 *) (keyLargoBaseAddr + 0x11000 + channelOffset);
    }

    // Save DBDMA registers.  There are thirteen channels on KeyLargo.

    savedKeyLargoDBDMAState = &savedKeyLargState.savedDBDMAState;

    for (i = 0, channelOffset = 0; i < 13; i++, channelOffset += 0x0100)
    {
        volatile DBDMAChannelRegisters*				currentChannel;

        currentChannel = (volatile DBDMAChannelRegisters *) (keyLargoBaseAddr + 0x8000 + channelOffset);

        savedKeyLargoDBDMAState->dmaChannel[i].commandPtrLo = IOGetDBDMACommandPtr(currentChannel);
        savedKeyLargoDBDMAState->dmaChannel[i].interruptSelect = IOGetDBDMAInterruptSelect(currentChannel);
        savedKeyLargoDBDMAState->dmaChannel[i].branchSelect = IOGetDBDMABranchSelect(currentChannel);
        savedKeyLargoDBDMAState->dmaChannel[i].waitSelect = IOGetDBDMAWaitSelect(currentChannel);
    }

    // Save configuration registers in KeyLargo (MediaBay Configuration Register, FCR 0-4)

    savedKeyLargoConfigRegistersState = &savedKeyLargState.savedConfigRegistersState;

    savedKeyLargoConfigRegistersState->mediaBay = *(UInt32 *)(keyLargoBaseAddr + kMediaBayRegOffset);

    for (i = 0; i < 5; i++)
    {
        savedKeyLargoConfigRegistersState->featureControl[i] = ((UInt32 *)(keyLargoBaseAddr + kFCR0Offset))[i];
    }

    // Save MPIC portion of KeyLargo.

    savedKeyLargoMPICState = &savedKeyLargState.savedMPICState;

    savedKeyLargoMPICState->mpicIPI[0] = *(UInt32 *)(mpicBaseAddr + MPICIPI0);
    savedKeyLargoMPICState->mpicIPI[1] = *(UInt32 *)(mpicBaseAddr + MPICIPI1);
    savedKeyLargoMPICState->mpicIPI[2] = *(UInt32 *)(mpicBaseAddr + MPICIPI2);
    savedKeyLargoMPICState->mpicIPI[3] = *(UInt32 *)(mpicBaseAddr + MPICIPI3);

    savedKeyLargoMPICState->mpicSpuriousVector = *(UInt32 *)(mpicBaseAddr + MPICSpuriousVector);
    savedKeyLargoMPICState->mpicTimerFrequencyReporting = *(UInt32 *)(mpicBaseAddr + MPICTimeFreq);

    savedKeyLargoMPICState->mpicTimers[0] = *(MPICTimers *)(mpicBaseAddr + MPICTimerBase0);
    savedKeyLargoMPICState->mpicTimers[1] = *(MPICTimers *)(mpicBaseAddr + MPICTimerBase1);
    savedKeyLargoMPICState->mpicTimers[2] = *(MPICTimers *)(mpicBaseAddr + MPICTimerBase2);
    savedKeyLargoMPICState->mpicTimers[3] = *(MPICTimers *)(mpicBaseAddr + MPICTimerBase3);

    for (i = 0; i < 64; i++)
    {
        // Make sure that the "active" bit is cleared.
        savedKeyLargoMPICState->mpicInterruptSourceVectorPriority[i] = *(UInt32 *)(mpicBaseAddr + MPICIntSrcVectPriBase + i * MPICIntSrcSize) & (~0x00000040);
        savedKeyLargoMPICState->mpicInterruptSourceDestination[i] = *(UInt32 *)(mpicBaseAddr + MPICIntSrcDestBase + i * MPICIntSrcSize);
    }

    savedKeyLargoMPICState->mpicCurrentTaskPriorities[0] = *(UInt32 *)(mpicBaseAddr + MPICP0CurrTaskPriority);
    savedKeyLargoMPICState->mpicCurrentTaskPriorities[1] = *(UInt32 *)(mpicBaseAddr + MPICP1CurrTaskPriority);
    savedKeyLargoMPICState->mpicCurrentTaskPriorities[2] = *(UInt32 *)(mpicBaseAddr + MPICP2CurrTaskPriority);
    savedKeyLargoMPICState->mpicCurrentTaskPriorities[3] = *(UInt32 *)(mpicBaseAddr + MPICP3CurrTaskPriority);
}


// Method: restoreKeyLargoState
//
// Purpose:
//        restores the state of all the meaningful registers from a local buffer.
//    this method is almost a copy and paste of the orignal MacOS9 function
//    RestoreKeyLargoState. The code does not care bout the endiannes of the
//    registers since the values are not meaningful, we just wish to save them
//    and restore them.
void KeyLargo::restoreKeyLargoState(void)
{
    KeyLargoMPICState*				savedKeyLargoMPICState;
    KeyLargoGPIOState*				savedKeyLargoGPIOState;
    KeyLargoConfigRegistersState*	savedKeyLargoConfigRegistersState;
    KeyLargoDBDMAState*				savedKeyLargoDBDMAState;
    KeyLargoAudioState*				savedKeyLargoAudioState;
    KeyLargoI2SState*				savedKeyLargoI2SState;
    UInt32						channelOffset;
    int							i;

    // base of the keylargo regiters.
    UInt8* keyLargoBaseAddr = (UInt8*)keyLargoBaseAddress;
    UInt8* mpicBaseAddr = (UInt8*)keyLargoBaseAddress + MPIC_OFFSET;

    // Restore MPIC portion of KeyLargo.


    savedKeyLargoMPICState = &savedKeyLargState.savedMPICState;

    *(UInt32 *)(mpicBaseAddr + MPICIPI0) = savedKeyLargoMPICState->mpicIPI[0];
    eieio();
    *(UInt32 *)(mpicBaseAddr + MPICIPI1) = savedKeyLargoMPICState->mpicIPI[1];
    eieio();
    *(UInt32 *)(mpicBaseAddr + MPICIPI2) = savedKeyLargoMPICState->mpicIPI[2];
    eieio();
    *(UInt32 *)(mpicBaseAddr + MPICIPI3) = savedKeyLargoMPICState->mpicIPI[3];
    eieio();

    *(UInt32 *)(mpicBaseAddr + MPICSpuriousVector) = savedKeyLargoMPICState->mpicSpuriousVector;
    eieio();
    *(UInt32 *)(mpicBaseAddr + MPICTimeFreq) = savedKeyLargoMPICState->mpicTimerFrequencyReporting;
    eieio();

    *(MPICTimers *)(mpicBaseAddr + MPICTimerBase0) = savedKeyLargoMPICState->mpicTimers[0];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + MPICTimerBase1) = savedKeyLargoMPICState->mpicTimers[1];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + MPICTimerBase2) = savedKeyLargoMPICState->mpicTimers[2];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + MPICTimerBase3) = savedKeyLargoMPICState->mpicTimers[3];
    eieio();

    for (i = 0; i < 64; i++)
    {
        *(UInt32 *)(mpicBaseAddr + MPICIntSrcVectPriBase + i * MPICIntSrcSize) = savedKeyLargoMPICState->mpicInterruptSourceVectorPriority[i];
        eieio();
        *(UInt32 *)(mpicBaseAddr + MPICIntSrcDestBase + i * MPICIntSrcSize) = savedKeyLargoMPICState->mpicInterruptSourceDestination[i];
        eieio();
    }

    *(UInt32 *)(mpicBaseAddr + MPICP0CurrTaskPriority) = savedKeyLargoMPICState->mpicCurrentTaskPriorities[0];
    eieio();
    *(UInt32 *)(mpicBaseAddr + MPICP1CurrTaskPriority) = savedKeyLargoMPICState->mpicCurrentTaskPriorities[1];
    eieio();
    *(UInt32 *)(mpicBaseAddr + MPICP2CurrTaskPriority) = savedKeyLargoMPICState->mpicCurrentTaskPriorities[2];
    eieio();
    *(UInt32 *)(mpicBaseAddr + MPICP3CurrTaskPriority) = savedKeyLargoMPICState->mpicCurrentTaskPriorities[3];
    eieio();


    // Restore configuration registers in KeyLargo (MediaBay Configuration Register, FCR 0-4)

    savedKeyLargoConfigRegistersState = &savedKeyLargState.savedConfigRegistersState;

    *(UInt32 *)(keyLargoBaseAddr + kMediaBayRegOffset) = savedKeyLargoConfigRegistersState->mediaBay;
    eieio();


    for (i = 0; i < 5; i++)
    {
        ((UInt32 *)(keyLargoBaseAddr + kFCR0Offset))[i] = savedKeyLargoConfigRegistersState->featureControl[i];
        eieio();
    }

    IODelay(250);

    // Restore DBDMA registers.  There are thirteen channels on KeyLargo.

    savedKeyLargoDBDMAState = &savedKeyLargState.savedDBDMAState;

    for (i = 0, channelOffset = 0; i < 13; i++, channelOffset += 0x0100)
    {
        volatile DBDMAChannelRegisters*				currentChannel;

        currentChannel = (volatile DBDMAChannelRegisters *) (keyLargoBaseAddr + 0x8000 + channelOffset);

        IODBDMAReset((IODBDMAChannelRegisters*)currentChannel);
        IOSetDBDMACommandPtr(currentChannel, savedKeyLargoDBDMAState->dmaChannel[i].commandPtrLo);
        IOSetDBDMAInterruptSelect(currentChannel, savedKeyLargoDBDMAState->dmaChannel[i].interruptSelect);
        IOSetDBDMABranchSelect(currentChannel, savedKeyLargoDBDMAState->dmaChannel[i].branchSelect);
        IOSetDBDMAWaitSelect(currentChannel, savedKeyLargoDBDMAState->dmaChannel[i].waitSelect);
    }

    // Restore Audio registers.

    savedKeyLargoAudioState = &savedKeyLargState.savedAudioState;

    for (i = 0, channelOffset = 0; i < 25; i++, channelOffset += 0x0010)
    {
        *(UInt32 *) (keyLargoBaseAddr + 0x14000 + channelOffset) = savedKeyLargoAudioState->audio[i];
        eieio();
    }

    // Restore I2S registers.

    savedKeyLargoI2SState = &savedKeyLargState.savedI2SState;

    for (i = 0, channelOffset = 0; i < 10; i++, channelOffset += 0x0010)
    {
        *(UInt32 *) (keyLargoBaseAddr + 0x10000 + channelOffset) = savedKeyLargoI2SState->i2s[i];
        eieio();
        *(UInt32 *) (keyLargoBaseAddr + 0x11000 + channelOffset) = savedKeyLargoI2SState->i2s[i + 1];
        eieio();
    }

    // Restore GPIO portion of KeyLargo.

    savedKeyLargoGPIOState = &savedKeyLargState.savedGPIOState;

    *(UInt32 *)(keyLargoBaseAddr + 0x50) = savedKeyLargoGPIOState->gpioLevels[0];
    eieio();
    *(UInt32 *)(keyLargoBaseAddr + 0x54) = savedKeyLargoGPIOState->gpioLevels[1];
    eieio();

    for (i = 0; i < 18; i++)
    {
        *(UInt8 *)(keyLargoBaseAddr + 0x58 + i) = savedKeyLargoGPIOState->extIntGPIO[i];
        eieio();
    }

    for (i = 0; i < 17; i++)
    {
        *(UInt8 *)(keyLargoBaseAddr + 0x6A + i) = savedKeyLargoGPIOState->gpio[i];
        eieio();
    }
}


// Method: saveVIAState
//
// Purpose:
//        saves the state of all the meaningful registers into a local buffer.
//    this method is almost a copy and paste of the orignal MacOS9 function
//    SaveVIAState.
void KeyLargo::saveVIAState(void)
{
    UInt8* viaBase = (UInt8*)keyLargoBaseAddress + VIA_OFFSET;
    UInt8* savedKeyLargoViaState = savedKeyLargState.savedVIAState;

    // Save VIA state.  These registers don't seem to get restored to any known state.
    savedKeyLargoViaState[0] = *(UInt8*)(viaBase + vBufA);
    savedKeyLargoViaState[1] = *(UInt8*)(viaBase + vDIRA);
    savedKeyLargoViaState[2] = *(UInt8*)(viaBase + vBufB);
    savedKeyLargoViaState[3] = *(UInt8*)(viaBase + vDIRB);
    savedKeyLargoViaState[4] = *(UInt8*)(viaBase + vPCR);
    savedKeyLargoViaState[5] = *(UInt8*)(viaBase + vACR);
    savedKeyLargoViaState[6] = *(UInt8*)(viaBase + vIER);
    savedKeyLargoViaState[7] = *(UInt8*)(viaBase + vT1C);
    savedKeyLargoViaState[8] = *(UInt8*)(viaBase + vT1CH);
}


// Method: restoreVIAState
//
// Purpose:
//        restores the state of all the meaningful registers from a local buffer.
//    this method is almost a copy and paste of the orignal MacOS9 function
//    RestoreVIAState.
void KeyLargo::restoreVIAState(void)
{
    UInt8* viaBase = (UInt8*)keyLargoBaseAddress + VIA_OFFSET;
    UInt8* savedKeyLargoViaState = savedKeyLargState.savedVIAState;

    // Restore VIA state.  These registers don't seem to get restored to any known state.
    *(UInt8*)(viaBase + vBufA) = savedKeyLargoViaState[0];
    eieio();
    *(UInt8*)(viaBase + vDIRA) = savedKeyLargoViaState[1];
    eieio();
    *(UInt8*)(viaBase + vBufB) = savedKeyLargoViaState[2];
    eieio();
    *(UInt8*)(viaBase + vDIRB) = savedKeyLargoViaState[3];
    eieio();
    *(UInt8*)(viaBase + vPCR) = savedKeyLargoViaState[4];
    eieio();
    *(UInt8*)(viaBase + vACR) = savedKeyLargoViaState[5];
    eieio();
    *(UInt8*)(viaBase + vIER) = savedKeyLargoViaState[6];
    eieio();
    *(UInt8*)(viaBase + vT1C) = savedKeyLargoViaState[7];
    eieio();
    *(UInt8*)(viaBase + vT1CH) = savedKeyLargoViaState[8];
    eieio();
}


IOReturn KeyLargo::callPlatformFunction(const OSSymbol *functionName,
                                        bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4)
{  
    if (functionName == keyLargo_resetUniNEthernetPhy)
    {
        resetUniNEthernetPhy();
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_restoreRegisterState)
    {
        restoreRegisterState();
        return kIOReturnSuccess;
    }
    
    if (functionName == keyLargo_syncTimeBase)
    {
        syncTimeBase();
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_saveRegisterState)
    {
        saveRegisterState();
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_turnOffIO)
    {
        if (keyLargoDeviceId == kKeyLargoDeviceId25)
            turnOffPangeaIO((bool)param1);
        else
            turnOffKeyLargoIO((bool)param1);
            
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_writeRegUInt8)
    {
        writeRegUInt8(*(unsigned long *)param1, (UInt8)param2);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_safeWriteRegUInt32)
    {
        safeWriteRegUInt32((unsigned long)param1, (UInt32)param2, (UInt32)param3);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_safeReadRegUInt32)
    {
        UInt32 *returnval = param2;
        *returnval = safeReadRegUInt32((unsigned long)param1);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_powerMediaBay)
    {
        bool powerOn = (param1 != NULL);
        powerMediaBay(powerOn, (UInt8)param2);
        return kIOReturnSuccess;
    }
    
    if (functionName->isEqualTo("EnableSCC"))
    {
        EnableSCC((bool)param1);
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo("PowerModem"))
    {
        PowerModem((bool)param1);
        return kIOReturnSuccess;
    }

    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

void KeyLargo::EnableSCC(bool state)
{
    if (state)
    {
        UInt32 bitMask;

        // Enables the SCC cell:
        bitMask = (1 << 6);	// Enables SCC
        //bitMask |= (1 << 5);	// Enables SCC B (which I do not belive is in use i any core99)
        bitMask |= (1 << 4);	// Enables SCC A
        bitMask |= (1 << 1);	// Enables SCC Interface A
        //bitMask |= (1 << 0);	// Enables SCC Interface B (which I do not belive is in use i any core99)
        safeWriteRegUInt32( (unsigned long)kKeyLargoFCR0, (UInt32)0x52, (UInt32)bitMask );

        // Resets the SCC:
        bitMask = (1 << 3);	// Resets the SCC
        safeWriteRegUInt32( (unsigned long)kKeyLargoFCR0, (UInt32)0x8, (UInt32)bitMask );

        IOSleep(15);

        bitMask = (0 << 3);
        safeWriteRegUInt32( (unsigned long)kKeyLargoFCR0, (UInt32)0x8, (UInt32)bitMask );
    }
    else
    {
        // disable code
    }
    
    return;
}

void KeyLargo::PowerModem(bool state)
{
    if (keyLargoDeviceId == kKeyLargoDeviceId25)
    {
        if (state)
        {
            writeRegUInt8(kKeyLargoGPIOBase + 0x3, 0x4); // set reset
            eieio();
            writeRegUInt8(kKeyLargoGPIOBase + 0x2, 0x4); // power modem on
            eieio();
            writeRegUInt8(kKeyLargoGPIOBase + 0x3, 0x5); // unset reset
            eieio();
        }
        else
        {
            writeRegUInt8(kKeyLargoGPIOBase + 0x2, 0x5); // power modem off
            eieio();
        }
    }
    else
    {
        if (state)
            safeWriteRegUInt32( (unsigned long)kKeyLargoFCR2, (UInt32)(1<<25), (UInt32)(0) );
        else
            safeWriteRegUInt32( (unsigned long)kKeyLargoFCR2, (UInt32)(1<<25), (UInt32)kKeyLargoFCR2AltDataOut );
    }

    return;
}

void KeyLargo::resetUniNEthernetPhy(void)
{
  // Uni-N Ethernet's Phy reset is controlled by GPIO16.
  // This should be determined from the device tree.
  
  // Pull down GPIO16 for 10ms (> 1ms) to hard reset the Phy,
  // and bring it out of low-power mode.
  writeRegUInt8(kKeyLargoGPIOBase + 16, kKeyLargoGPIOOutputEnable);
  IOSleep(10);
  writeRegUInt8(kKeyLargoGPIOBase + 16,
		kKeyLargoGPIOOutputEnable | kKeyLargoGPIOData);
  IOSleep(10);
}


#undef super
#define super IOWatchDogTimer

OSDefineMetaClassAndStructors(KeyLargoWatchDogTimer, IOWatchDogTimer);


KeyLargoWatchDogTimer *KeyLargoWatchDogTimer::withKeyLargo(KeyLargo *keyLargo)
{
  KeyLargoWatchDogTimer *watchDogTimer = new KeyLargoWatchDogTimer;
  
  if (watchDogTimer == 0) return 0;
  
  while (1) {
    if (!watchDogTimer->init()) break;
    
    watchDogTimer->attach(keyLargo);
    
    if (watchDogTimer->start(keyLargo)) return watchDogTimer;
    
    watchDogTimer->detach(keyLargo);
    break;
  }
  
  return 0;
}

bool KeyLargoWatchDogTimer::start(IOService *provider)
{
  keyLargo = OSDynamicCast(KeyLargo, provider);
  if (keyLargo == 0) return false;
  
  return super::start(provider);
}

enum {
  kKeyLargoWatchDogLow    = 0x15030,
  kKeyLargoWatchDogHigh   = 0x15034,
  kKeyLargoCounterLow     = 0x15038,
  kKeyLargoCounterHigh    = 0x1503C,
  kKeyLargoWatchDogEnable = 0x15048
};

void KeyLargoWatchDogTimer::setWatchDogTimer(UInt32 timeOut)
{
  UInt32 timeLow, timeHigh, timeHigh2, watchDogLow, watchDogHigh;
  UInt64 offset, time;
  
  if (timeOut != 0) {
    offset = (UInt64)timeOut * kKeyLargoGTimerFreq;
    
    do {
      timeHigh = keyLargo->readRegUInt32(kKeyLargoCounterHigh);
      timeLow = keyLargo->readRegUInt32(kKeyLargoCounterLow);
      timeHigh2 = keyLargo->readRegUInt32(kKeyLargoCounterHigh);
      
    } while (timeHigh != timeHigh2);
    time = (((UInt64)timeHigh) << 32) + timeLow;
    
    time += offset;
    
    watchDogLow = time & 0x0FFFFFFFFULL;
    watchDogHigh = time >> 32;
    
    keyLargo->writeRegUInt32(kKeyLargoWatchDogLow, watchDogLow);
    keyLargo->writeRegUInt32(kKeyLargoWatchDogHigh, watchDogHigh);
  }
  
  keyLargo->writeRegUInt32(kKeyLargoWatchDogEnable,
			   (timeOut != 0) ? 1 : 0);
}
