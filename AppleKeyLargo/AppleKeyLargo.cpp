/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-2002 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/ppc/IODBDMA.h>

#include "KeyLargo.h"
#include "AppleKeyLargo.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super KeyLargo

OSDefineMetaClassAndStructors(AppleKeyLargo, KeyLargo);

AppleKeyLargo *gHostKeyLargo = NULL;

bool AppleKeyLargo::init(OSDictionary * properties)
{
	// Just to be sure we are not going to use the
	// backup structures by mistake let's invalidate
	// their contents.
	savedKeyLargoState.thisStateIsValid = false;
  
	// And by default the wireless slot is powered on:
	cardStatus.cardPower = true;
  
	return super::init(properties);
}

bool AppleKeyLargo::start(IOService *provider)
{
	OSData          *tmpData;
	UInt32			fcrValue;

	// if this is mac-io (as opposed to ext-mac-io) save a reference to it
	tmpData = OSDynamicCast(OSData, provider->getProperty("name"));
	if (tmpData == 0) return false;
  
	if (tmpData->isEqualTo ("mac-io", strlen ("mac-io")))
	gHostKeyLargo = this;
	
	// callPlatformFunction symbols
	keyLargo_resetUniNEthernetPhy = OSSymbol::withCString("keyLargo_resetUniNEthernetPhy");
	keyLargo_restoreRegisterState = OSSymbol::withCString("keyLargo_restoreRegisterState");
	keyLargo_syncTimeBase = OSSymbol::withCString("keyLargo_syncTimeBase");
	keyLargo_recalibrateBusSpeeds = OSSymbol::withCString("keyLargo_recalibrateBusSpeeds");
	keyLargo_saveRegisterState = OSSymbol::withCString("keyLargo_saveRegisterState");
	keyLargo_turnOffIO = OSSymbol::withCString("keyLargo_turnOffIO");
	keyLargo_writeRegUInt8 = OSSymbol::withCString("keyLargo_writeRegUInt8");
	keyLargo_safeWriteRegUInt8 = OSSymbol::withCString("keyLargo_safeWriteRegUInt8");
	keyLargo_safeReadRegUInt8 = OSSymbol::withCString("keyLargo_safeReadRegUInt8");
	keyLargo_safeWriteRegUInt32 = OSSymbol::withCString("keyLargo_safeWriteRegUInt32");
	keyLargo_safeReadRegUInt32 = OSSymbol::withCString("keyLargo_safeReadRegUInt32");
	keyLargo_powerMediaBay = OSSymbol::withCString("powerMediaBay");
	keyLargo_getHostKeyLargo = OSSymbol::withCString("keyLargo_getHostKeyLargo");
	keyLargo_powerI2S = OSSymbol::withCString("keyLargo_powerI2S");
	keyLargo_setPowerSupply = OSSymbol::withCString("setPowerSupply");
 
	// Call KeyLargo's start.
	if (!super::start(provider))
		return false;

	// ***Adding an FCR property in the IORegistry
	keyLargo_FCRNode = OSSymbol::withCString("fcr-values");

	fcrValue = readRegUInt32(kKeyLargoFCR0);
	fcrs[0] = OSNumber::withNumber(fcrValue, 32);
	fcrValue = readRegUInt32(kKeyLargoFCR1);
	fcrs[1] = OSNumber::withNumber(fcrValue, 32);
	fcrValue = readRegUInt32(kKeyLargoFCR2);
	fcrs[2] = OSNumber::withNumber(fcrValue, 32);
	fcrValue = readRegUInt32(kKeyLargoFCR3);
	fcrs[3] = OSNumber::withNumber(fcrValue, 32);
	fcrValue = readRegUInt32(kKeyLargoFCR4);
	fcrs[4] = OSNumber::withNumber(fcrValue, 32);
	
	if ((keyLargoDeviceId == kPangeaDeviceId25) || (keyLargoDeviceId == kIntrepidDeviceId3e)) 
		fcrValue = readRegUInt32(kKeyLargoFCR5);
	else
		fcrValue = 0;
	fcrs[5] = OSNumber::withNumber(fcrValue, 32);

	fcrArray = OSArray::withObjects(fcrs, 6, 0);
	if (fcrArray)
		keyLargoService->setProperty(keyLargo_FCRNode, (OSArray *)fcrArray);
 
	// Set clock reference counts
	setReferenceCounts ();
  
	enableCells();

	// Make nubs for the children.
	publishBelow(provider);

	// at power on the media bay is on:
	mediaIsOn = true;
  
	// by default, this is false
	keepSCCenabledInSleep = false;
	
	registerService();
  
  
	// initialize for Power Management
	initForPM(provider);
  
	// creates the USBPower handlers:
	UInt32 i;
	for (i = 0; i < fNumUSB; i++) {
		usbBus[i] = new USBKeyLargo;
    
		if (usbBus[i] != NULL) {
			if ( usbBus[i]->init() && usbBus[i]->attach(this))
				usbBus[i]->initForBus(fBaseUSBID+i, keyLargoDeviceId);                 
			else
				usbBus[i]->release();
		}
	}

	return true;
}

void AppleKeyLargo::stop(IOService *provider)
{
	// releases the USBPower handlers:
	UInt32 i;
	for (i = 0; i < fNumUSB; i++) {
		if (usbBus[i] != NULL)
			usbBus[i]->release();
	}

	// release the fcr handles
	if (keyLargoService)
		keyLargoService->release(); 
	if (fcrArray)
		fcrArray->release();
  
	if (mutex != NULL)
		IOSimpleLockFree( mutex );
	
	return;
}

void AppleKeyLargo::turnOffKeyLargoIO(bool restart)
{
	UInt32				regTemp;
	IOInterruptState	intState;

	// Take a lock around all the writes
	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

	if (!restart) {
        kprintf("AppleKeyLargo::turnOffIO( --FALSE-- )\n");
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

	if (keyLargoDeviceId == kKeyLargoDeviceId22) {	// Media bay on KeyLargo only
		// Set MediaBay Dev1 Enable before IDE Resets.
		regTemp = readRegUInt32(kKeyLargoMediaBay);
		regTemp |= kKeyLargoMB0DevEnable;
		writeRegUInt32(kKeyLargoMediaBay, regTemp);
	}

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
        kprintf("AppleKeyLargo::turnOffIO( --TRUE-- )\n");
    }
	
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

	return;
}

void AppleKeyLargo::turnOffPangeaIO(bool restart)
{
    UInt32 				regTemp;
	IOInterruptState	intState;
	bool				usingSCCA;

	// Take a lock around all the writes
	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

    // FCR0
    regTemp = readRegUInt32(kKeyLargoFCR0);
	usingSCCA = (regTemp & kKeyLargoFCR0SccAEnable);
	
    regTemp |= kPangeaFCR0SleepBitsSet;
    regTemp &= ~kPangeaFCR0SleepBitsClear;
    
	
	if(keepSCCenabledInSleep && usingSCCA)
	{
		//IOLog("AKL::turnOffPangeaIO: keeping scc enabled\n");
		regTemp |= (kKeyLargoFCR0SccAEnable | kKeyLargoFCR0SccCellEnable);
	}

    writeRegUInt32(kKeyLargoFCR0, regTemp);

    // FCR1
    regTemp = readRegUInt32(kKeyLargoFCR1);
    regTemp |= kPangeaFCR1SleepBitsSet;
	if (hostIsMobile)	// Don't reset IDE on desktops
		regTemp &= ~kKeyLargoFCR1UIDEReset;   
    regTemp &= ~kPangeaFCR1SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR1, regTemp);

    // FCR2
    regTemp = readRegUInt32(kKeyLargoFCR2);
    regTemp |= kPangeaFCR2SleepBitsSet;
    regTemp &= ~kPangeaFCR2SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR2, regTemp);

    // FCR3
    regTemp = readRegUInt32(kKeyLargoFCR3);
    regTemp |= kPangeaFCR3SleepBitsSet;
    regTemp &= ~kPangeaFCR3SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR3, regTemp);

    // FCR4
    regTemp = readRegUInt32(kKeyLargoFCR4);
    regTemp |= kPangeaFCR4SleepBitsSet;
    regTemp &= ~kPangeaFCR4SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR4, regTemp);
	
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

	return;
}

void AppleKeyLargo::turnOffIntrepidIO(bool restart)
{
    UInt32 				regTemp;
	IOInterruptState	intState;
	bool				usingSCCA;

	// Take a lock around all the writes
	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

    // FCR0
    regTemp = readRegUInt32(kKeyLargoFCR0);
	usingSCCA = (regTemp & kKeyLargoFCR0SccAEnable);

    regTemp |= kIntrepidFCR0SleepBitsSet;
    regTemp &= ~kIntrepidFCR0SleepBitsClear;

	if(keepSCCenabledInSleep && usingSCCA)
	{
		//IOLog("AKL::turnOffIntrepidIO: keeping scc enabled\n");
		regTemp |= (kKeyLargoFCR0SccAEnable | kKeyLargoFCR0SccCellEnable);
	}

    writeRegUInt32(kKeyLargoFCR0, regTemp);

    // FCR1
    regTemp = readRegUInt32(kKeyLargoFCR1);
    regTemp |= kIntrepidFCR1SleepBitsSet;
    if (hostIsMobile)	// Don't reset EIDE on desktops
		regTemp &= ~kKeyLargoFCR1EIDE0Reset;   
	regTemp &= ~kIntrepidFCR1SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR1, regTemp);

    // FCR2
    regTemp = readRegUInt32(kKeyLargoFCR2);
    regTemp |= kIntrepidFCR2SleepBitsSet;
    regTemp &= ~kIntrepidFCR2SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR2, regTemp);

    // FCR3
    regTemp = readRegUInt32(kKeyLargoFCR3);
    regTemp |= kIntrepidFCR3SleepBitsSet;
    regTemp &= ~kIntrepidFCR3SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR3, regTemp);

    // FCR4
    regTemp = readRegUInt32(kKeyLargoFCR4);
    regTemp |= kIntrepidFCR4SleepBitsSet;
    regTemp &= ~kIntrepidFCR4SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR4, regTemp);
	
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

	return;
}


// Uncomment the following define if you need to see the state of
// the media bay register:
//#define LOG_MEDIA_BAY_TRANSACTIONS

void AppleKeyLargo::powerMediaBay(bool powerOn, UInt8 powerDevice)
{
    UInt32 regTemp;
    UInt32 whichDevice = powerDevice;
	IOInterruptState	intState;

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog("AppleKeyLargo::powerMediaBay(%s) 0x%02x\n", (powerOn ? "TRUE" : "FALSE"), powerDevice);
#endif

    if (mediaIsOn == powerOn) {
#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog("AppleKeyLargo::powerMediaBay mbreg = 0x%08lx\n",readRegUInt32(kKeyLargoMediaBay));
#endif
        return;
    }
    
	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);
    
    // Makes sure that the reset bit is off:
    regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 0 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

    regTemp &= (~(kKeyLargoMB0DevReset));

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 1 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

    writeRegUInt32(kKeyLargoMediaBay, regTemp);

    if (powerOn) {
        // we are powering on the bay and need a delay between turning on
        // media bay power and enabling the bus
        regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 2 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

        regTemp &= (~(kKeyLargoMB0DevPower));

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 3 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        writeRegUInt32(kKeyLargoMediaBay, regTemp);
		IODelay(500);
    }

    // to turn on the buses, we ensure all buses are off and then turn on the requested bus
    regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 4 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

    regTemp &= (~(kKeyLargoMB0DevEnable));

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 5 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
    writeRegUInt32(kKeyLargoMediaBay, regTemp);
    IODelay(500);

    // and turns on the right bus:
    regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 6 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
    regTemp |= (whichDevice << 11);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
    IOLog(" 7 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif

    writeRegUInt32(kKeyLargoMediaBay, regTemp);
    IODelay(500);

    if (!powerOn) {
        // turn off media bay power:
        regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 8 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        regTemp |= kKeyLargoMB0DevPower;

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 9 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        writeRegUInt32(kKeyLargoMediaBay, regTemp);
    }
    else {
        // take us out of reset:
        regTemp = readRegUInt32(kKeyLargoMediaBay);

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 10 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
#endif
        regTemp |= kKeyLargoMB0DevReset;

#ifdef LOG_MEDIA_BAY_TRANSACTIONS
        IOLog(" 11 AppleKeyLargo::powerMediaBay = 0x%08lx\n",regTemp);
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
	
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

	return;
}

void AppleKeyLargo::powerWireless(bool powerOn)
{
	IOInterruptState	intState;

    // if we are already in the wanted power
    // state just exit:
    if (cardStatus.cardPower == powerOn)
        return;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

    if (powerOn) {
        // power the card on by setting the registers with their
        // back-up copy:
        writeRegUInt8(kKeyLargoExtIntGPIOBase + 10, cardStatus.wirelessCardReg[0]);
        writeRegUInt8(kKeyLargoExtIntGPIOBase + 13, cardStatus.wirelessCardReg[1]);
        writeRegUInt8(kKeyLargoGPIOBase + 13, cardStatus.wirelessCardReg[2]);
        writeRegUInt8(kKeyLargoGPIOBase + 14, cardStatus.wirelessCardReg[3]);
        writeRegUInt8(kKeyLargoGPIOBase + 15, cardStatus.wirelessCardReg[4]);
    } else {
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
	
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

	return;
}

/*
 * Set reference counts based on initial configuration
 *
 * The purpose of this function is to take the initial configuration as set
 * by Open Firmware, determine which cells are in use, based on the port mux
 * setup and which cells and clocks are enabled and keep a reference count
 * for each of the clocks that are used by multiple cells.  
 *
 * Elsewhere, as we turn things off and on, when a reference count for a clock
 * goes to zero, we can turn that clock off.
 */
void AppleKeyLargo::setReferenceCounts (void)
{
	UInt32 fcr0, fcr1, fcr3, fcr5;
	bool chooseSCCA, chooseSCCB, chooseI2S0, chooseI2S1, chooseAudio;

	clk31RefCount = 0;
	clk45RefCount = 0;
	clk49RefCount = 0;
	clk32RefCount = 0;
	fcr0 = readRegUInt32(kKeyLargoFCR0);
	fcr1 = readRegUInt32(kKeyLargoFCR1);
	fcr3 = readRegUInt32(kKeyLargoFCR3);
  
	if (keyLargoDeviceId == kKeyLargoDeviceId22) {			// KeyLargo
		chooseSCCB = (fcr0 & kKeyLargoFCR0ChooseSCCB);
		fcr5 = 0;
	} else if (keyLargoDeviceId == kPangeaDeviceId25 ||
		keyLargoDeviceId == kIntrepidDeviceId3e)			// Pangea or Intrepid
	{												
		chooseSCCB = true;
		fcr5 = readRegUInt32(kKeyLargoFCR5);
	}
  
	chooseSCCA = (fcr0 & kKeyLargoFCR0ChooseSCCA);
	chooseI2S1 = !chooseSCCA;

	chooseAudio = (fcr1 & kKeyLargoFCR1ChooseAudio);
	chooseI2S0 = !chooseAudio;

	if (chooseSCCA && (fcr0 & kKeyLargoFCR0SccAEnable)) {
		if (keyLargoDeviceId != kIntrepidDeviceId3e)
			clk31RefCount++;
		clk45RefCount++;
	}
  
	if (chooseSCCB && (fcr0 & kKeyLargoFCR0SccBEnable)) {
		if (keyLargoDeviceId != kIntrepidDeviceId3e)
			clk31RefCount++;
		clk45RefCount++;
	}
  
	if (chooseI2S0 && (fcr1 & kKeyLargoFCR1I2S0Enable)) {
		if (keyLargoDeviceId != kIntrepidDeviceId3e)
			clk49RefCount++;
		clk45RefCount++;
        fI2SState[0] = true;
	}

	if (chooseI2S1 && (fcr1 & kKeyLargoFCR1I2S1Enable)) {
		if (keyLargoDeviceId != kIntrepidDeviceId3e)
			clk49RefCount++;
		clk45RefCount++;
        fI2SState[1] = true;
	}

	if (chooseAudio && (fcr1 & kKeyLargoFCR1AudioCellEnable)) {
		if (keyLargoDeviceId != kIntrepidDeviceId3e)
			clk49RefCount++;
		clk45RefCount++;
	}

	/*
	 * If the VIA is enabled, count the 31MHz clock if we're on Key Largo.
	 * But on Pangea, kPangeaFCR5ViaUseClk31 determines if this clock is
	 * actually in use.  On Intrepid the 31MHz clock doesn't exist so we
	 * don't count it.
	 */
	
	if (fcr3 & kKeyLargoFCR3ViaClk16Enable)
		if (keyLargoDeviceId == kKeyLargoDeviceId22 || ((keyLargoDeviceId == kPangeaDeviceId25) && (fcr5 & kPangeaFCR5ViaUseClk31)))
			clk31RefCount++;

	if (fcr5 & kPangeaFCR5Clk32Enable) {
	// Pangea only
	if (keyLargoDeviceId == kPangeaDeviceId25) {
			if ((fcr3 & kKeyLargoFCR3ViaClk16Enable) && (!(fcr5 & kPangeaFCR5ViaUseClk31)))
				clk32RefCount++;
			if (!(fcr5 & kPangeaFCR5SCCUseClk31)) {
				if (chooseSCCA && (fcr0 & kKeyLargoFCR0SccAEnable))
					clk32RefCount++;
				if (chooseSCCB && (fcr0 & kKeyLargoFCR0SccBEnable))
					clk32RefCount++;
			}
		} else
			// Intrepid only - Intrepid always uses 32MHz clock
			if (keyLargoDeviceId == kIntrepidDeviceId3e) {
				if (fcr3 & kIntrepidFCR3ViaClk32Enable)
					clk32RefCount++;
				if (chooseSCCA && (fcr0 & kKeyLargoFCR0SccAEnable))
					clk32RefCount++;
				if (chooseSCCB && (fcr0 & kKeyLargoFCR0SccBEnable))
					clk32RefCount++;
			}
	}

#define DEBUGREFCOUNTS 0
#if DEBUGREFCOUNTS
#define DLOG IOLog
#define PRINTBOOL(b) (b) ? "true" : "false"
#define PRINTNOT(n) (n) ? "" : "NOT"
	DLOG ("AppleKeyLargo::setReferenceCounts - chooseSCCA %s and %s enabled\n", 
		PRINTBOOL(chooseSCCA), PRINTNOT(fcr0 & kKeyLargoFCR0SccAEnable));
	DLOG ("AppleKeyLargo::setReferenceCounts - chooseSCCB %s and %s enabled\n", 
		PRINTBOOL(chooseSCCB), PRINTNOT(fcr0 & kKeyLargoFCR0SccBEnable));
	DLOG ("AppleKeyLargo::setReferenceCounts - chooseI2S0 %s and %s enabled\n", 
		PRINTBOOL(chooseI2S0), PRINTNOT(fcr1 & kKeyLargoFCR1I2S0Enable));
	DLOG ("AppleKeyLargo::setReferenceCounts - chooseI2S1 %s and %s enabled\n", 
		PRINTBOOL(chooseI2S1), PRINTNOT(fcr1 & kKeyLargoFCR1I2S1Enable));
	DLOG ("AppleKeyLargo::setReferenceCounts - chooseAudio %s and %s enabled\n", 
		PRINTBOOL(chooseAudio), PRINTNOT(fcr1 & kKeyLargoFCR1AudioCellEnable));
	DLOG ("AppleKeyLargo::setReferenceCounts - VIA %s enabled\n", 
		PRINTNOT(fcr3 & kKeyLargoFCR3ViaClk16Enable));
	DLOG ("AppleKeyLargo::setReferenceCounts - clk31RefCount = %d, clk45RefCount = %d, clk49RefCount = %d\n",
		clk31RefCount, clk45RefCount, clk49RefCount);
#endif

	return;
}

void AppleKeyLargo::enableCells()
{
    unsigned int debugFlags;

    if (!PE_parse_boot_arg("debug", &debugFlags))
        debugFlags = 0;

    if( debugFlags & 0x18) {
		safeWriteRegUInt32( (unsigned long)kKeyLargoFCR0, kKeyLargoFCR0SccCellEnable |
			kKeyLargoFCR0SccAEnable |
			kKeyLargoFCR0ChooseSCCA, 
			kKeyLargoFCR0SccCellEnable |
			kKeyLargoFCR0SccAEnable |
			kKeyLargoFCR0ChooseSCCA );
    }

    // Enable the mpic cell:
	safeWriteRegUInt32( (unsigned long)kKeyLargoFCR2, kKeyLargoFCR2MPICEnable, kKeyLargoFCR2MPICEnable );

	return;
}

// NOTE: Marco changed the save and restore state to save all keylargo registers.
// this is a temporary fix, the real code should save and restore all registers
// in each specific driver (VIA, MPIC ...) However for now it is easier to follow
// the MacOS9 policy to do everything here.
void AppleKeyLargo::saveRegisterState(void)
{
    saveKeyLargoState();
    saveVIAState(savedKeyLargoState.savedVIAState);
    powerWireless(false);
    savedKeyLargoState.thisStateIsValid = true;
	
	return;
}

void AppleKeyLargo::restoreRegisterState(void)
{
    if (savedKeyLargoState.thisStateIsValid) {
        restoreKeyLargoState();
        restoreVIAState(savedKeyLargoState.savedVIAState);
        powerWireless(true);
    }

    savedKeyLargoState.thisStateIsValid = false;
	
	return;
}

void AppleKeyLargo::safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data)
{
	IOInterruptState intState;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

	UInt32 currentReg = readRegUInt32(offset);
	currentReg = (currentReg & ~mask) | (data & mask);
	writeRegUInt32(offset, currentReg);
  
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);
	
    // ***If we are writing a 32-bit reg, we want to see if it's an FCR register.
    // If it is, we want to update the fcr values in memory so we can update the
    // property later.
    
	if (offset >= kKeyLargoFCR0 && offset <= kKeyLargoFCR5 && fcrArray) {
		OSNumber* value = OSNumber::withNumber(currentReg, 32);
		
		((OSArray *)fcrArray)->replaceObject((offset - kKeyLargoFCRBase) >> 2, value);
		keyLargoService->setProperty(keyLargo_FCRNode, (OSArray *)fcrArray);
		value->release();
	}
	
	return;
}

// --------------------------------------------------------------------------
// Method: initForPM
//
// Purpose:
//   initialize the driver for power managment and register ourselves with
//   superclass policy-maker
void AppleKeyLargo::initForPM (IOService *provider)
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
#define kNumberOfPowerStates 3

    static IOPMPowerState ourPowerStates[kNumberOfPowerStates] = {
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0, IOPMSoftSleep, IOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};


    // register ourselves with ourself as policy-maker
    if (pm_vars != NULL)
        registerPowerDriver(this, ourPowerStates, kNumberOfPowerStates);
	
	return;
}

// Method: setPowerState
//
// Purpose:
IOReturn AppleKeyLargo::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
    // Do not do anything if the state is inavalid.
    if (powerStateOrdinal >= kNumberOfPowerStates)
        return IOPMAckImplied;

    if ( powerStateOrdinal == 0 ) {
        kprintf("KeyLargo would be powered off here\n");
    }
    if ( powerStateOrdinal == 1 ) {
        kprintf("KeyLargo would be powered on here\n");
    }
	if(watchDogTimer)
		watchDogTimer->setSleeping(powerStateOrdinal < 2);
    return IOPMAckImplied;
}

// Method: saveKeyLargoState
//
// Purpose:
//        saves the state of all the meaningful registers into a local buffer.
//    this method is almost a copy and paste of the orignal MacOS9 function
//    SaveKeyLargoState. The code does not care about the endianness of the
//    registers since the values are not meaningful, we just wish to save them
//    and restore them.
void AppleKeyLargo::saveKeyLargoState(void)
{
    KeyLargoMPICState*				savedKeyLargoMPICState;
    KeyLargoGPIOState*				savedKeyLargoGPIOState;
    KeyLargoConfigRegistersState*	savedKeyLargoConfigRegistersState;
    KeyLargoDBDMAState*				savedKeyLargoDBDMAState;
    KeyLargoAudioState*				savedKeyLargoAudioState;
    KeyLargoI2SState*				savedKeyLargoI2SState;
    UInt32							channelOffset;
    int								i;

    // base of the keylargo registers.
    UInt8* keyLargoBaseAddr =	(UInt8*)keyLargoBaseAddress;
    UInt8* mpicBaseAddr =		(UInt8*)keyLargoBaseAddress + kKeyLargoMPICBaseOffset;

    // Save GPIO portion of KeyLargo.

    savedKeyLargoGPIOState = &savedKeyLargoState.savedGPIOState;

    savedKeyLargoGPIOState->gpioLevels[0] = *(UInt32 *)(keyLargoBaseAddr + kKeyLargoGPIOLevels0);
    savedKeyLargoGPIOState->gpioLevels[1] = *(UInt32 *)(keyLargoBaseAddr + kKeyLargoGPIOLevels1);

    for (i = 0; i < kKeyLargoExtIntGPIOCount; i++)
    {
        savedKeyLargoGPIOState->extIntGPIO[i] = *(UInt8 *)(keyLargoBaseAddr + kKeyLargoExtIntGPIORegBase + i);
    }

    for (i = 0; i < kKeyLargoGPIOCount; i++)
    {
        savedKeyLargoGPIOState->gpio[i] = *(UInt8 *)(keyLargoBaseAddr + kKeyLargoGPIOBase + i);
    }

    // Save Audio registers.

    savedKeyLargoAudioState = &savedKeyLargoState.savedAudioState;

    for (i = 0, channelOffset = 0; i < kKeyLargoAudioRegisterCount; i++, channelOffset += kKeyLargoAudioRegisterStride)
    {
        savedKeyLargoAudioState->audio[i] = *(UInt32 *) (keyLargoBaseAddr + kKeyLargoAudioBaseOffset + channelOffset);
    }

    // Save I2S registers - 10 registers per channel.

    savedKeyLargoI2SState = &savedKeyLargoState.savedI2SState;

    for (i = 0, channelOffset = 0; i < kKeyLargoI2SRegisterCount; i++, channelOffset += kKeyLargoI2SRegisterStride)
    {
        savedKeyLargoI2SState->i2s[i] = *(UInt32 *) (keyLargoBaseAddr + kKeyLargoI2S0BaseOffset + channelOffset);
        savedKeyLargoI2SState->i2s[i + 1] = *(UInt32 *) (keyLargoBaseAddr + kKeyLargoI2S1BaseOffset + channelOffset);
    }

    // Save DBDMA registers.  There are thirteen channels on KeyLargo.

    savedKeyLargoDBDMAState = &savedKeyLargoState.savedDBDMAState;

    for (i = 0, channelOffset = 0; i < kKeyLargoDBDMAChannelCount; i++, channelOffset += kKeyLargoDBDMAChannelStride)
    {
        volatile DBDMAChannelRegisters*				currentChannel;

        currentChannel = (volatile DBDMAChannelRegisters *) (keyLargoBaseAddr + kKeyLargoDBDMABaseOffset + channelOffset);

        savedKeyLargoDBDMAState->dmaChannel[i].commandPtrLo = IOGetDBDMACommandPtr(currentChannel);
        savedKeyLargoDBDMAState->dmaChannel[i].interruptSelect = IOGetDBDMAInterruptSelect(currentChannel);
        savedKeyLargoDBDMAState->dmaChannel[i].branchSelect = IOGetDBDMABranchSelect(currentChannel);
        savedKeyLargoDBDMAState->dmaChannel[i].waitSelect = IOGetDBDMAWaitSelect(currentChannel);
    }

    // Save configuration registers in KeyLargo (MediaBay Configuration Register, FCR 0-4 
	// for KeyLargo, and additionally FCR5 for Intrepid and Pangea)

    savedKeyLargoConfigRegistersState = &savedKeyLargoState.savedConfigRegistersState;

	if(keyLargoDeviceId == kKeyLargoDeviceId22) 	
	{
		// Media bay on KeyLargo only
		savedKeyLargoConfigRegistersState->mediaBay = *(UInt32 *)(keyLargoBaseAddr + kKeyLargoMediaBay);

		for (i = 0; i < kKeyLargoFCRCount; i++)
		{
			savedKeyLargoConfigRegistersState->featureControl[i] = ((UInt32 *)(keyLargoBaseAddr + kKeyLargoFCR0))[i];
		}
	} else if(keyLargoDeviceId == kPangeaDeviceId25) 	
	{
		for (i = 0; i < kPangeaFCRCount; i++)
		{
			savedKeyLargoConfigRegistersState->featureControl[i] = ((UInt32 *)(keyLargoBaseAddr + kKeyLargoFCR0))[i];
		}
	} else /* if(keyLargoDeviceId == kIntrepidDeviceId3e) */
	{
		for (i = 0; i < kIntrepidFCRCount; i++)
		{
			savedKeyLargoConfigRegistersState->featureControl[i] = ((UInt32 *)(keyLargoBaseAddr + kKeyLargoFCR0))[i];
		}
	}




    // Save MPIC portion of KeyLargo.

    savedKeyLargoMPICState = &savedKeyLargoState.savedMPICState;

    savedKeyLargoMPICState->mpicIPI[0] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI0);
    savedKeyLargoMPICState->mpicIPI[1] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI1);
    savedKeyLargoMPICState->mpicIPI[2] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI2);
    savedKeyLargoMPICState->mpicIPI[3] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI3);

    savedKeyLargoMPICState->mpicSpuriousVector = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICSpuriousVector);
    savedKeyLargoMPICState->mpicTimerFrequencyReporting = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICTimeFreq);

    savedKeyLargoMPICState->mpicTimers[0] = *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase0);
    savedKeyLargoMPICState->mpicTimers[1] = *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase1);
    savedKeyLargoMPICState->mpicTimers[2] = *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase2);
    savedKeyLargoMPICState->mpicTimers[3] = *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase3);

    for (i = 0; i < kKeyLargoMPICVectorsCount; i++)
    {
        // Make sure that the "active" bit is cleared.
        savedKeyLargoMPICState->mpicInterruptSourceVectorPriority[i] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIntSrcVectPriBase + i * kKeyLargoMPICIntSrcSize) & (~0x00000040);
        savedKeyLargoMPICState->mpicInterruptSourceDestination[i] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIntSrcDestBase + i * kKeyLargoMPICIntSrcSize);
    }

    savedKeyLargoMPICState->mpicCurrentTaskPriorities[0] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP0CurrTaskPriority);
    savedKeyLargoMPICState->mpicCurrentTaskPriorities[1] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP1CurrTaskPriority);
    savedKeyLargoMPICState->mpicCurrentTaskPriorities[2] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP2CurrTaskPriority);
    savedKeyLargoMPICState->mpicCurrentTaskPriorities[3] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP3CurrTaskPriority);

	return;
}


// Method: restoreKeyLargoState
//
// Purpose:
//        restores the state of all the meaningful registers from a local buffer.
//    this method is almost a copy and paste of the original MacOS9 function
//    RestoreKeyLargoState. The code does not care about the endiannes of the
//    registers since the values are not meaningful, we just wish to save them
//    and restore them.
void AppleKeyLargo::restoreKeyLargoState(void)
{
    KeyLargoMPICState*				savedKeyLargoMPICState;
    KeyLargoGPIOState*				savedKeyLargoGPIOState;
    KeyLargoConfigRegistersState*	savedKeyLargoConfigRegistersState;
    KeyLargoDBDMAState*				savedKeyLargoDBDMAState;
    KeyLargoAudioState*				savedKeyLargoAudioState;
    KeyLargoI2SState*				savedKeyLargoI2SState;
    UInt32							channelOffset;
    int								i;

    // base of the keylargo registers.
    UInt8* keyLargoBaseAddr =	(UInt8*)keyLargoBaseAddress;
    UInt8* mpicBaseAddr =		(UInt8*)keyLargoBaseAddress + kKeyLargoMPICBaseOffset;

    // Restore MPIC portion of KeyLargo.

    savedKeyLargoMPICState = &savedKeyLargoState.savedMPICState;

    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI0) = savedKeyLargoMPICState->mpicIPI[0];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI1) = savedKeyLargoMPICState->mpicIPI[1];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI2) = savedKeyLargoMPICState->mpicIPI[2];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI3) = savedKeyLargoMPICState->mpicIPI[3];
    eieio();

    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICSpuriousVector) = savedKeyLargoMPICState->mpicSpuriousVector;
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICTimeFreq) = savedKeyLargoMPICState->mpicTimerFrequencyReporting;
    eieio();

    *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase0) = savedKeyLargoMPICState->mpicTimers[0];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase1) = savedKeyLargoMPICState->mpicTimers[1];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase2) = savedKeyLargoMPICState->mpicTimers[2];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase3) = savedKeyLargoMPICState->mpicTimers[3];
    eieio();

    for (i = 0; i < kKeyLargoMPICVectorsCount; i++)
    {
        *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIntSrcVectPriBase + i * kKeyLargoMPICIntSrcSize) = savedKeyLargoMPICState->mpicInterruptSourceVectorPriority[i];
        eieio();
        *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIntSrcDestBase + i * kKeyLargoMPICIntSrcSize) = savedKeyLargoMPICState->mpicInterruptSourceDestination[i];
        eieio();
    }

    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP0CurrTaskPriority) = savedKeyLargoMPICState->mpicCurrentTaskPriorities[0];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP1CurrTaskPriority) = savedKeyLargoMPICState->mpicCurrentTaskPriorities[1];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP2CurrTaskPriority) = savedKeyLargoMPICState->mpicCurrentTaskPriorities[2];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP3CurrTaskPriority) = savedKeyLargoMPICState->mpicCurrentTaskPriorities[3];
    eieio();


    // Restore configuration registers in KeyLargo (MediaBay Configuration Register, FCR 0-4 
	// for KeyLargo, and additionally FCR5 for Intrepid and Pangea)

    savedKeyLargoConfigRegistersState = &savedKeyLargoState.savedConfigRegistersState;

	if (keyLargoDeviceId == kKeyLargoDeviceId22) 
	{	
		// Media bay on KeyLargo only
		*(UInt32 *)(keyLargoBaseAddr + kKeyLargoMediaBay) = savedKeyLargoConfigRegistersState->mediaBay;
		eieio();

		for (i = 0; i < kKeyLargoFCRCount; i++)
		{
			((UInt32 *)(keyLargoBaseAddr + kKeyLargoFCR0))[i] = savedKeyLargoConfigRegistersState->featureControl[i];
			eieio();
		}
	} else if(keyLargoDeviceId == kPangeaDeviceId25) 	
	{
		for (i = 0; i < kPangeaFCRCount; i++)
		{
			((UInt32 *)(keyLargoBaseAddr + kKeyLargoFCR0))[i] = savedKeyLargoConfigRegistersState->featureControl[i];
			eieio();
		}
	} else /* if(keyLargoDeviceId == kIntrepidDeviceId3e) */
	{
		for (i = 0; i < kIntrepidFCRCount; i++)
		{
			((UInt32 *)(keyLargoBaseAddr + kKeyLargoFCR0))[i] = savedKeyLargoConfigRegistersState->featureControl[i];
			eieio();
		}
	}




    IODelay(250);

    // Restore DBDMA registers.  There are thirteen channels on KeyLargo.

    savedKeyLargoDBDMAState = &savedKeyLargoState.savedDBDMAState;

    for (i = 0, channelOffset = 0; i < kKeyLargoDBDMAChannelCount; i++, channelOffset += kKeyLargoDBDMAChannelStride)
    {
        volatile DBDMAChannelRegisters*				currentChannel;

        currentChannel = (volatile DBDMAChannelRegisters *) (keyLargoBaseAddr + kKeyLargoDBDMABaseOffset + channelOffset);

        IODBDMAReset((IODBDMAChannelRegisters*)currentChannel);
        IOSetDBDMACommandPtr(currentChannel, savedKeyLargoDBDMAState->dmaChannel[i].commandPtrLo);
        IOSetDBDMAInterruptSelect(currentChannel, savedKeyLargoDBDMAState->dmaChannel[i].interruptSelect);
        IOSetDBDMABranchSelect(currentChannel, savedKeyLargoDBDMAState->dmaChannel[i].branchSelect);
        IOSetDBDMAWaitSelect(currentChannel, savedKeyLargoDBDMAState->dmaChannel[i].waitSelect);
    }

    // Restore Audio registers.

    savedKeyLargoAudioState = &savedKeyLargoState.savedAudioState;

    for (i = 0, channelOffset = 0; i < kKeyLargoAudioRegisterCount; i++, channelOffset += kKeyLargoAudioRegisterStride)
    {
        *(UInt32 *) (keyLargoBaseAddr + kKeyLargoAudioBaseOffset + channelOffset) = savedKeyLargoAudioState->audio[i];
        eieio();
    }

    // Restore I2S registers - 10 registers per channel.

    savedKeyLargoI2SState = &savedKeyLargoState.savedI2SState;

    for (i = 0, channelOffset = 0; i < kKeyLargoI2SRegisterCount; i++, channelOffset += kKeyLargoI2SRegisterStride)
    {
        *(UInt32 *) (keyLargoBaseAddr + kKeyLargoI2S0BaseOffset + channelOffset) = savedKeyLargoI2SState->i2s[i];
        eieio();
        *(UInt32 *) (keyLargoBaseAddr + kKeyLargoI2S1BaseOffset + channelOffset) = savedKeyLargoI2SState->i2s[i + 1];
        eieio();
    }

    // Restore GPIO portion of KeyLargo.

    savedKeyLargoGPIOState = &savedKeyLargoState.savedGPIOState;

    *(UInt32 *)(keyLargoBaseAddr + kKeyLargoGPIOLevels0) = savedKeyLargoGPIOState->gpioLevels[0];
    eieio();
    *(UInt32 *)(keyLargoBaseAddr + kKeyLargoGPIOLevels1) = savedKeyLargoGPIOState->gpioLevels[1];
    eieio();

    for (i = 0; i < kKeyLargoExtIntGPIOCount; i++)
    {
        *(UInt8 *)(keyLargoBaseAddr + kKeyLargoExtIntGPIORegBase + i) = savedKeyLargoGPIOState->extIntGPIO[i];
        eieio();
    }

    for (i = 0; i < kKeyLargoGPIOCount; i++)
    {
        *(UInt8 *)(keyLargoBaseAddr + kKeyLargoGPIOBase + i) = savedKeyLargoGPIOState->gpio[i];
        eieio();
    }
	
	return;
}


IOReturn AppleKeyLargo::callPlatformFunction(const OSSymbol *functionName,
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

    if (functionName == keyLargo_recalibrateBusSpeeds)
    {
        recalibrateBusSpeeds();
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_saveRegisterState)
    {
        saveRegisterState();
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_turnOffIO)
    {
		if (keyLargoDeviceId == kKeyLargoDeviceId22)
			turnOffKeyLargoIO((bool)param1);
		else if (keyLargoDeviceId == kPangeaDeviceId25)
            turnOffPangeaIO((bool)param1);
        else
            turnOffIntrepidIO((bool)param1);
            
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_writeRegUInt8)
    {
        writeRegUInt8(*(unsigned long *)param1, (UInt32)param2);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_safeWriteRegUInt8)
    {
        safeWriteRegUInt8((unsigned long)param1, (UInt32)param2, (UInt32)param3);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_safeReadRegUInt8)
    {
        UInt8 *returnval = (UInt8 *)param2;
        *returnval = safeReadRegUInt8((unsigned long)param1);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_safeWriteRegUInt32)
    {
        safeWriteRegUInt32((unsigned long)param1, (UInt32)param2, (UInt32)param3);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_safeReadRegUInt32)
    {
        UInt32 *returnval = (UInt32 *)param2;
        *returnval = safeReadRegUInt32((unsigned long)param1);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_powerMediaBay)
    {
        bool powerOn = (param1 != NULL);
 
		// No media bay on Pangea or Intrepid
		if (keyLargoDeviceId == kPangeaDeviceId25 || keyLargoDeviceId == kIntrepidDeviceId3e) 
			return kIOReturnUnsupported;
	
		powerMediaBay(powerOn, (UInt32)param2);
        return kIOReturnSuccess;
    }
    
    if (functionName->isEqualTo("EnableSCC"))
    {
        EnableSCC((bool)param1, (UInt32)param2, (bool)param3);
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo("PowerModem"))
    {
        PowerModem((bool)param1);
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo("ModemResetLow"))
    {
        ModemResetLow();
        return kIOReturnSuccess;
    }

    if (functionName->isEqualTo("ModemResetHigh"))
    {
        ModemResetHigh();
        return kIOReturnSuccess;
    }
	
    if (functionName == keyLargo_getHostKeyLargo)
    {
        UInt32 *returnVal = (UInt32 *)param1;
		*returnVal = (UInt32) gHostKeyLargo;
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_powerI2S)
    {
        PowerI2S((bool)param1, (UInt32)param2);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_setPowerSupply)
    {
        return SetPowerSupply((bool)param1);
    }

    if (functionName->isEqualTo("keepSCCEnabledInSleep"))
    {
		keepSCCenabledInSleep = (bool)param1;
        return kIOReturnSuccess;
    }


    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

IOReturn AppleKeyLargo::callPlatformFunction( const char * functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 )
{
	IOReturn result = kIOReturnNoMemory;
	const OSSymbol *functionSymbol = OSSymbol::withCString(functionName);
  
	if (functionSymbol != 0) {
		result = callPlatformFunction(functionSymbol, waitForFunction,
				  param1, param2, param3, param4);
		functionSymbol->release();
	}
  
  return result;
}

/*
 * EnableSCC - power on or off the SCC cell
 *
 * state - true to power on, false to power off
 * device - 0 for SCCA, 1 for SCCB
 * type - true to use I2S1 under SCCA or IrDA under SCCB, 0 for not
 */
void AppleKeyLargo::EnableSCC(bool state, UInt8 device, bool type)
{
    UInt32 bitsToSet, bitsToClear, currentReg, currentReg3, currentReg5;
    IOInterruptState intState;
		
	bitsToSet = bitsToClear = currentReg = currentReg3 = currentReg5 = 0;
	
	if (state) {												// Powering on        
        if(device == 0) {					// SCCA
			bitsToSet = kKeyLargoFCR0SccCellEnable;				// Enables SCC Cell
			bitsToSet |= kKeyLargoFCR0SccAEnable;				// Enables SCC Interface A

            if(type) 						// I2S1
                bitsToClear |= kKeyLargoFCR0ChooseSCCA;			// Enables SCC Interface A to support I2S1
            else // SCC
                bitsToSet |= kKeyLargoFCR0ChooseSCCA;			// Enables SCC Interface A to support SCCA
        } else if(device == 1) {					// SCCB
			if (keyLargoDeviceId == kKeyLargoDeviceId22) {		// irda only on KeyLargo, not Pangea or Intrepid
				bitsToSet = kKeyLargoFCR0SccCellEnable;				// Enables SCC Cell
				bitsToSet |= kKeyLargoFCR0SccBEnable;				// Enables SCC Interface B
				if(type) {						// IrDA
					bitsToSet |= kKeyLargoFCR0IRDAClk19Enable;		// irda 19.584 MHz clock
					bitsToSet |= kKeyLargoFCR0IRDAClk32Enable;		// irda 32 mhz clock on
					bitsToSet |= kKeyLargoFCR0IRDAEnable;			// IrDA Enable
					bitsToClear |= kKeyLargoFCR0IRDAFastCon;		// fast connect
					bitsToClear |= kKeyLargoFCR0IRDADefault0;		// default0
					bitsToClear |= kKeyLargoFCR0IRDADefault1;		// default1	
					bitsToSet |= kKeyLargoFCR0UseIRSource1;			// use ir source 1
					bitsToClear |= kKeyLargoFCR0UseIRSource2;		// do not use ir source 2
					bitsToClear |= kKeyLargoFCR0HighBandFor1MB;		// high band for 1mbit.  0 for low speed?
					//bitsToClear |= (1 << 2);	// SlowPCLK.    0 for SCCPCLK at 24.576 MHZ, 1 for 15.6672 MHz
					bitsToClear |= kKeyLargoFCR0ChooseSCCB;			// Enables SCC Interface B to support IrDA
				} else // SCC
					bitsToSet |= kKeyLargoFCR0ChooseSCCB;			// Enables SCC Interface B to support SCCB
			} else
				return;		// Nothing to set
        } else
			return;		// Bad device
                
        if ( mutex  != NULL ) 			// Take Lock
            intState = IOSimpleLockLockDisableInterrupt(mutex);
  
		// Read current value of register
        currentReg = readRegUInt32( (UInt32)kKeyLargoFCR0);
		
		// Increment reference count, but only if we're not currently enabled
		if (!(currentReg & bitsToSet & (kKeyLargoFCR0SccAEnable | kKeyLargoFCR0SccBEnable))) {
			currentReg3 = readRegUInt32( (UInt32)kKeyLargoFCR3);
			if ((keyLargoDeviceId != kIntrepidDeviceId3e) && (!(clk31RefCount++))) {
				currentReg3 |= kKeyLargoFCR3Clk31Enable;	// turn on clock
			}
			if (!(clk45RefCount++)) {
				currentReg3 |= kKeyLargoFCR3Clk45Enable;	// turn on clock
			}
			writeRegUInt32( (UInt32)kKeyLargoFCR3, (UInt32)currentReg3 );

			if (keyLargoDeviceId == kPangeaDeviceId25) {
				currentReg5 = readRegUInt32( (UInt32)kKeyLargoFCR5);
				if (!(currentReg5 & kPangeaFCR5SCCUseClk31) &&  !(clk32RefCount++)) {
					currentReg5 |= kPangeaFCR5Clk32Enable;	// turn on clock
					writeRegUInt32( (UInt32)kKeyLargoFCR5, (UInt32)currentReg5 );
				}
			}
			
			if (keyLargoDeviceId == kIntrepidDeviceId3e) {
				currentReg5 = readRegUInt32( (UInt32)kKeyLargoFCR5);
				if (!(clk32RefCount++)) {
					currentReg5 |= kPangeaFCR5Clk32Enable;	// turn on clock
					writeRegUInt32( (UInt32)kKeyLargoFCR5, (UInt32)currentReg5 );
				}
			}
		}
		
		// Modify current value of register...
        currentReg |= bitsToSet;
        currentReg &= ~bitsToClear;
        
		// ...and write it back
        writeRegUInt32( (UInt32)kKeyLargoFCR0, (UInt32)currentReg );

        if(device == 1 && (type) && keyLargoDeviceId == kKeyLargoDeviceId22) {		// Reset the IrDA:
            currentReg |= kKeyLargoFCR0IRDASWReset;
            writeRegUInt32( (UInt32)kKeyLargoFCR0, (UInt32)currentReg );

            IODelay(15000);

            currentReg &= ~kKeyLargoFCR0IRDASWReset;
            writeRegUInt32( (UInt32)kKeyLargoFCR0, (UInt32)currentReg );
        }
        
        if ( mutex  != NULL )			// Release Lock
            IOSimpleLockUnlockEnableInterrupt(mutex, intState);

#if DEBUGREFCOUNTS
		DLOG ("AppleKeyLargo::EnableSCC (enable) - clk31RefCount = %d, clk45RefCount = %d, clk49RefCount = %d\n",
			clk31RefCount, clk45RefCount, clk49RefCount);
#endif
    } else {	// Powering down
        if(device == 0)
            bitsToClear |= kKeyLargoFCR0SccAEnable;				// Disables SCC A

         else if(device == 1) {
			if (keyLargoDeviceId == kKeyLargoDeviceId22) {		// irda only on KeyLargo, not Pangea or Intrepid
				bitsToClear |= kKeyLargoFCR0SccBEnable;				// Disables SCC B
				if (type) {
					bitsToClear |= kKeyLargoFCR0IRDAClk19Enable;	// irda 19.584 MHz clock
					bitsToClear |= kKeyLargoFCR0IRDAClk32Enable;	// irda 32 mhz clock on
					bitsToClear |= kKeyLargoFCR0IRDAEnable;			// IrDA Enable
					bitsToClear |= kKeyLargoFCR0IRDAFastCon;		// fast connect
					bitsToClear |= kKeyLargoFCR0IRDADefault0;		// default0
					bitsToClear |= kKeyLargoFCR0IRDADefault1;		// default1	
					bitsToClear |= kKeyLargoFCR0UseIRSource1;		// use ir source 1
					bitsToClear |= kKeyLargoFCR0UseIRSource2;		// do not use ir source 2
					bitsToClear |= kKeyLargoFCR0HighBandFor1MB;		// high band for 1mbit.  0 for low speed?
					//bitsToClear |= (1 << 2);	// SlowPCLK.    0 for SCCPCLK at 24.576 MHZ, 1 for 15.6672 MHz
				}
			} else
				return; 	// Nothing to clear
        } else
			return;		// Bad device

        if ( mutex  != NULL ) 			// Take Lock
            intState = IOSimpleLockLockDisableInterrupt(mutex);

		// Read current value of register
        currentReg = readRegUInt32( (UInt32)kKeyLargoFCR0);

		// If both SCCA and SCCB will be disabled when we're done, also disable the SCC cell
		if (!(currentReg & ~bitsToClear & (kKeyLargoFCR0SccAEnable | kKeyLargoFCR0SccBEnable))) {
			bitsToClear |= kKeyLargoFCR0SccCellEnable;
		}

		// Turn off clocks we no longer need - but only if we're really disabling the cell
		if (currentReg & bitsToClear & (kKeyLargoFCR0SccAEnable | kKeyLargoFCR0SccBEnable)) {
			currentReg3 = readRegUInt32( (UInt32)kKeyLargoFCR3);
			if ((keyLargoDeviceId != kIntrepidDeviceId3e) && clk31RefCount && !(--clk31RefCount)) {
				currentReg3 &= ~kKeyLargoFCR3Clk31Enable;	// turn off clock if refCount reaches zero
			}
			if (clk45RefCount && !(--clk45RefCount)) {
				currentReg3 &= ~kKeyLargoFCR3Clk45Enable;	// turn off clock if refCount reaches zero
			}
			writeRegUInt32( (UInt32)kKeyLargoFCR3, (UInt32)currentReg3 );

			if (keyLargoDeviceId == kPangeaDeviceId25) {
				currentReg5 = readRegUInt32( (UInt32)kKeyLargoFCR5);
				if (!(currentReg5 & kPangeaFCR5SCCUseClk31) &&  clk32RefCount && !(--clk32RefCount)) {
					currentReg5 &= ~kPangeaFCR5Clk32Enable ;	// turn off clock
					writeRegUInt32( (UInt32)kKeyLargoFCR5, (UInt32)currentReg5 );
				}
			}
			
			if (keyLargoDeviceId == kIntrepidDeviceId3e) {
				currentReg5 = readRegUInt32( (UInt32)kKeyLargoFCR5);
				if ( clk32RefCount && !(--clk32RefCount)) {
					currentReg5 &= ~kPangeaFCR5Clk32Enable;	// turn off clock
					writeRegUInt32( (UInt32)kKeyLargoFCR5, (UInt32)currentReg5 );
				}
			}
		}

		// Modify current value of register...
        currentReg |= bitsToSet;
        currentReg &= ~bitsToClear;
        
		// ...and write it back
        writeRegUInt32( (UInt32)kKeyLargoFCR0, (UInt32)currentReg );

        if ( mutex  != NULL )			// Release Lock
            IOSimpleLockUnlockEnableInterrupt(mutex, intState);
			
#if DEBUGREFCOUNTS
		DLOG ("AppleKeyLargo::EnableSCC (disable) - clk31RefCount = %d, clk45RefCount = %d, clk49RefCount = %d\n",
			clk31RefCount, clk45RefCount, clk49RefCount);
#endif
    }

    return;
}

void AppleKeyLargo::PowerModem(bool state)
{
    if (keyLargoDeviceId == kPangeaDeviceId25 || keyLargoDeviceId == kIntrepidDeviceId3e) {	// Pangea or Intrepid
        if (state) {
            writeRegUInt8(kKeyLargoGPIOBase + 0x2, 0x4); // power modem on
            eieio();
        } else {
            writeRegUInt8(kKeyLargoGPIOBase + 0x2, 0x5); // power modem off
            eieio();
        }
    } else if (keyLargoDeviceId == kKeyLargoDeviceId22) {		// KeyLargo
        if (state)
            safeWriteRegUInt32( (unsigned long)kKeyLargoFCR2, (UInt32)kKeyLargoFCR2AltDataOut, (UInt32)(0) );
        else
            safeWriteRegUInt32( (unsigned long)kKeyLargoFCR2, (UInt32)kKeyLargoFCR2AltDataOut, (UInt32)kKeyLargoFCR2AltDataOut );
    }

    return;
}

void AppleKeyLargo::ModemResetLow()
{
	*(UInt8*)(keyLargoBaseAddress + kKeyLargoGPIOBase + 0x3) |= 0x04;	// Set GPIO3_DDIR to output
	eieio();
	*(UInt8*)(keyLargoBaseAddress + kKeyLargoGPIOBase + 0x3) &= ~0x01;	// Set GPIO3_DataOut output to zero
	eieio();
	
	return;
}

void AppleKeyLargo::ModemResetHigh()
{
	*(UInt8*)(keyLargoBaseAddress + kKeyLargoGPIOBase + 0x3) |= 0x04;	// Set GPIO3_DDIR to output
	eieio();
	*(UInt8*)(keyLargoBaseAddress + kKeyLargoGPIOBase + 0x3) |= 0x01;	// Set GPIO3_DataOut output to 1
	eieio();
	
	return;
}

void AppleKeyLargo::PowerI2S (bool powerOn, UInt32 cellNum)
{
	UInt32 fcr1Bits, fcr3Bits;
	
	if (cellNum == 0) {
		fcr1Bits = kKeyLargoFCR1I2S0CellEnable |
						kKeyLargoFCR1I2S0ClkEnable |
						kKeyLargoFCR1I2S0Enable;
		fcr3Bits = kKeyLargoFCR3I2S0Clk18Enable;
	} else if (cellNum == 1) {
		fcr1Bits = kKeyLargoFCR1I2S1CellEnable |
						kKeyLargoFCR1I2S1ClkEnable |
						kKeyLargoFCR1I2S1Enable;
		fcr3Bits = kKeyLargoFCR3I2S1Clk18Enable;
	} else
		return; 		// bad cellNum ignored

    if(fI2SState[cellNum] == powerOn)
        return;     // Already in right state.
            
	if (powerOn) {
		if (!(clk45RefCount++)) {
			fcr3Bits |= kKeyLargoFCR3Clk45Enable;	// turn on clock
		}
		
		if(keyLargoDeviceId != kIntrepidDeviceId3e)
		{
			if (!(clk49RefCount++)) 
			{
				fcr3Bits |= kKeyLargoFCR3Clk49Enable;	// turn on clock
			}
		}
		// turn on all I2S bits
		safeWriteRegUInt32 (kKeyLargoFCR1, fcr1Bits, fcr1Bits);
		safeWriteRegUInt32 (kKeyLargoFCR3, fcr3Bits, fcr3Bits);
	} else {
		if (clk45RefCount && !(--clk45RefCount)) {
			fcr3Bits |= kKeyLargoFCR3Clk45Enable;	// turn off clock if refCount reaches zero
		}
		if(keyLargoDeviceId != kIntrepidDeviceId3e)
		{
			if (clk49RefCount && !(--clk49RefCount)) 
			{
				fcr3Bits |= kKeyLargoFCR3Clk49Enable;	// turn off clock if refCount reaches zero
			}
		}	
		// turn off all I2S bits
		safeWriteRegUInt32 (kKeyLargoFCR1, fcr1Bits, 0);
		safeWriteRegUInt32 (kKeyLargoFCR3, fcr3Bits, 0);

	}
    fI2SState[cellNum] = powerOn;
	return;
}

/*
 * set the power supply to hi or low power state.  This is used for processor
 * speed cycling
 */
IOReturn AppleKeyLargo::SetPowerSupply (bool powerHi)
{
	char			value;
	UInt32			delay;
    OSIterator 		*childIterator;
    IORegistryEntry *childEntry;
	OSData			*regData;
	
	if (!keyLargoCPUVCoreSelectGPIO) {
		// locate the gpio node associated with cpu-vcore-select
		if ((childIterator = getChildIterator (gIOServicePlane)) != NULL) {
			while ((childEntry = (IORegistryEntry *)(childIterator->getNextObject ())) != NULL) {
				if (!strcmp ("cpu-vcore-select", childEntry->getName(gIOServicePlane))) {
					regData = OSDynamicCast( OSData, childEntry->getProperty( "reg" ));
					if (regData) {
						// get the GPIO offset from the reg property
						keyLargoCPUVCoreSelectGPIO = *(UInt32 *) regData->getBytesNoCopy();
						break;
					}
				}
			}
			childIterator->release();
		}
	
		if (!keyLargoCPUVCoreSelectGPIO)		// If still unknown return unsupported
			return (kIOReturnUnsupported);
	}

	// Set gpio for 1 for high voltage, 0 for low voltage
	value = kKeyLargoGPIOOutputEnable | (powerHi ? kKeyLargoGPIOData : 0);
	writeRegUInt8 (keyLargoCPUVCoreSelectGPIO, value);
	
	// Wait for power supply to ramp up.
	delay = 200;
	assert_wait(&delay, THREAD_UNINT);
	thread_set_timer(delay, NSEC_PER_USEC);
	thread_block(0);

	return (kIOReturnSuccess);
}

void AppleKeyLargo::resetUniNEthernetPhy(void)
{
	// Uni-N Ethernet's Phy reset is controlled by GPIO16.
	// This should be determined from the device tree.
  
	// Pull down GPIO16 for 10ms (> 1ms) to hard reset the Phy,
	// and bring it out of low-power mode.
	writeRegUInt8(kKeyLargoGPIOBase + 16, kKeyLargoGPIOOutputEnable);
	IOSleep(10);
	// Make direction an input so we don't continue to drive the data line
	writeRegUInt8(kKeyLargoGPIOBase + 16, kKeyLargoGPIOData);
	IOSleep(10);
	
	return;
}

void AppleKeyLargo::processNub(IOService * nub)
{
    super::processNub(nub);

    // On Intrepid systems update the pmu-info property of the via-pmu node and
    // delete clock-spreading-info from the power-mgt node
    if(keyLargoDeviceId == kIntrepidDeviceId3e) {
        const char * name = nub->getName();
        if( strcmp("via-pmu", name) == 0) {
            static UInt8 data[] = {0x06, 0x0B, 0x01, 0x41, 0x4e, 0x44, 0x59};
            OSData *existing = OSDynamicCast(OSData, nub->getProperty("pmu-info"));
            if(existing)
                existing->appendBytes(data, sizeof(data));
            else
                nub->setProperty("pmu-info", data, sizeof(data));
        }
        else if(strcmp("power-mgt", name) == 0)
            nub->removeProperty("clock-spreading-info");
    }
}
