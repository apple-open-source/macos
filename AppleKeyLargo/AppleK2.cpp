/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-2002 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: William Gulland
 *
 */

#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "IOPlatformFunction.h"
#include "AppleK2.h"
#define kIOPCICacheLineSize 	"IOPCICacheLineSize"
#define kIOPCITimerLatency		"IOPCITimerLatency"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super KeyLargo

OSDefineMetaClassAndStructors(AppleK2, KeyLargo);

AppleK2 *gHostK2 = NULL;

bool AppleK2::init(OSDictionary * properties)
{
	// Just to be sure we are not going to use the
	// backup structures by mistake let's invalidate
	// their contents.
	savedK2State.thisStateIsValid = false;
  
	return super::init(properties);
}

bool AppleK2::start(IOService *provider)
{
	OSData          *tmpData;
	const OSSymbol	*instantiate = OSSymbol::withCString("InstantiatePlatformFunctions");
	IOReturn		retval;
	UInt32			flags;
	IOPlatformFunction *func;
   	IOPCIDevice		*pciProvider;
    UInt32			i;

    fProvider = provider;
	tmpData = (OSData *) fProvider->getProperty( "AAPL,phandle" );
	if(tmpData)
		fPHandle = *((UInt32 *) tmpData->getBytesNoCopy());

    
	// if this is mac-io (as opposed to ext-mac-io) save a reference to it
	tmpData = OSDynamicCast(OSData, provider->getProperty("name"));
	if (tmpData == 0) return false;
  
	if (tmpData->isEqualTo ("mac-io", strlen ("mac-io")))
        gHostK2 = this;
	
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
	keyLargo_getHostKeyLargo = OSSymbol::withCString("keyLargo_getHostKeyLargo");
	keyLargo_powerI2S = OSSymbol::withCString("keyLargo_powerI2S");
	keyLargo_setPowerSupply = OSSymbol::withCString("setPowerSupply");
	keyLargo_EnableI2SModem = OSSymbol::withCString("EnableI2SModem");
	mac_io_publishChildren = OSSymbol::withCString("mac-io-publishChildren");
	mac_io_publishChild = OSSymbol::withCString("mac-io-publishChild");
 
    k2_enableFireWireClock = OSSymbol::withCString("EnableFireWireClock");
    k2_enableEthernetClock = OSSymbol::withCString("EnableUniNEthernetClock");
	
    k2_getHTLinkFrequency = OSSymbol::withCString("getHTLinkFrequency");
    k2_setHTLinkFrequency = OSSymbol::withCString("setHTLinkFrequency");
    k2_getHTLinkWidth = OSSymbol::withCString("getHTLinkWidth");
    k2_setHTLinkWidth = OSSymbol::withCString("setHTLinkWidth");
    
	// Call KeyLargo's start.
	if (!super::start(provider))
		return false;

	// Set clock reference counts
	setReferenceCounts ();
  
	enableCells();

	// Make nubs for the children.
	publishBelow(provider);

	// by default, this is false
	keepSCCenabledInSleep = false;
	
	registerService();

	// Locate Our HyperTransport parent's LDT capability block (if present)
	htLinkCapabilitiesBase = 0;
	if (pciProvider = OSDynamicCast (IOPCIDevice, keyLargoService->getParentEntry(gIODTPlane))) {
		UInt32		capID;
		
		htLinkCapabilitiesBase = pciProvider->configRead8 (kIOPCIConfigCapabilitiesPtr);
		while (htLinkCapabilitiesBase) {
			capID = pciProvider->configRead8 (htLinkCapabilitiesBase + kIOPCICapabilityIDOffset);
			if (capID == 8)		// LDT Capabilities ID is 8
				break;
			
			htLinkCapabilitiesBase = pciProvider->configRead8 (htLinkCapabilitiesBase + kIOPCINextCapabilityOffset);
		}
	}

	// initialize for Power Management
	initForPM(provider);
  
    if(keyLargoDeviceId != kShastaDeviceId4f) {
        // creates the USBPower handlers:
        for (i = 0; i < fNumUSB; i++) {
            usbBus[i] = new USBKeyLargo;
        
            if (usbBus[i] != NULL) {
                if ( usbBus[i]->init() && usbBus[i]->attach(this))
                    usbBus[i]->initForBus(fBaseUSBID+i, keyLargoDeviceId);                 
                else
                    usbBus[i]->release();
            }
        }
    }
    else {
        // Shasta machines have no PMU, set up watchdog here.
        // Presumably SMU supports watchdog timer.
        watchDogTimer = KeyLargoWatchDogTimer::withKeyLargo(this);
    }
    // Register the FireWire and Ethernet clock control routines
    publishResource(k2_enableFireWireClock, this);
    publishResource(k2_enableEthernetClock, this);
    
	// Scan for platform-do-xxx functions
	fPlatformFuncArray = NULL;

	retval = provider->getPlatform()->callPlatformFunction(instantiate, true,
			(void *)provider, (void *)&fPlatformFuncArray, (void *)0, (void *)0);

	if (retval == kIOReturnSuccess && (fPlatformFuncArray != NULL)) {
        UInt32 count = fPlatformFuncArray->getCount();
		for (i = 0; i < count; i++) {
			if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i))) {
				flags = func->getCommandFlags();

				//kprintf("AppleK2::start() - functionCheck - got function, flags 0x%lx, pHandle 0x%lx\n", 
				//	flags, func->getCommandPHandle());

				// If this function is flagged to be performed at initialization, do it
				if (flags & kIOPFFlagOnInit) {
					performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
				}

				if ((flags & kIOPFFlagOnDemand) || ((flags & kIOPFFlagIntGen))) {
					// On-Demand and IntGen functions need to have a resource published
					func->publishPlatformFunction(this);
				}

			}
			else {
				// This function won't be used -- generate a warning
				kprintf("AppleK2::start() - functionCheck - not an IOPlatformFunction object\n");
			}
		}
    }
    
    // Set Clock Stop Delay to minimum, which avoids a hardware bug when waking
    // the PCI busses from sleep.
    safeWriteRegUInt32(kK2FCR9, kK2FCR9ClkStopDelayMask, 0);
    
    // Dump current clock state
    logClockState();

	return true;
}

void AppleK2::stop(IOService *provider)
{
	// releases the USBPower handlers:
    if(keyLargoDeviceId != kShastaDeviceId4f) {
        UInt32 i;
        for (i = 0; i < fNumUSB; i++) {
            if (usbBus[i] != NULL)
                usbBus[i]->release();
        }
    }
	// release the fcr handles
	if (keyLargoService)
		keyLargoService->release(); 
  
	if (mutex != NULL)
		IOSimpleLockFree( mutex );
	
	return;
}

void AppleK2::publishBelow( IORegistryEntry * root )
{
	publishChildren(this);
}

void AppleK2::processNub( IOService * nub )
{
    nub->setProperty("preserveIODeviceTree", true);
}

IOService * AppleK2::createNub( IORegistryEntry * from )
{
    IOService *	nub;

    nub = new AppleK2Device;

    if( nub && !nub->init( from, gIODTPlane )) {
	nub->free();
	nub = 0;
    }

    return( nub);
}


void AppleK2::turnOffK2IO(bool restart)
{
	UInt32				regTemp;
	IOInterruptState	intState;
	
	/*
	 * When we are called, PMU has already been signalled to initiate sleep and only
	 * 100 milliseconds are allowed for the system to finish quiescing the cpus.
	 *
	 * This is different from earlier systems and means we can't take any unnecessary
	 * delays, like kprintfs
	 */

	// Take a lock around all the writes
	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

    if (!restart) {
        // turning off the USB clocks:
        regTemp = readRegUInt32(kKeyLargoFCR0);
        regTemp |= kK2FCR0SleepBitsSet;
        writeRegUInt32(kKeyLargoFCR0, regTemp);
        IODelay(1000);
    }

    regTemp = readRegUInt32(kKeyLargoFCR0);
    regTemp |= kK2FCR0SleepBitsSet;
    regTemp &= ~kK2FCR0SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR0, regTemp);

    regTemp = readRegUInt32(kKeyLargoFCR1);
    regTemp |= kK2FCR1SleepBitsSet;
    regTemp &= ~kK2FCR1SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR1, regTemp);

    regTemp = readRegUInt32(kKeyLargoFCR2);
    regTemp |= kK2FCR2SleepBitsSet;
    regTemp &= ~kK2FCR2SleepBitsClear;
    writeRegUInt32(kKeyLargoFCR2, regTemp);

    regTemp = readRegUInt32(kKeyLargoFCR3);
    if (restart) {
        regTemp |= kK2FCR3RestartBitsSet;
        regTemp &= ~kK2FCR3RestartBitsClear;
    } else {
        regTemp |= kK2FCR3SleepBitsSet;
        regTemp &= ~kK2FCR3SleepBitsClear;
    }
    writeRegUInt32(kKeyLargoFCR3, regTemp);

    if (restart) {
        // enables the keylargo cells we are going to need:
        enableCells();
    }
    
    // Dump current clock state
	IODelay(100);	// let clocks settle
    logClockState();

	// Now turn off SCC - kprintf won't work beyond this point
    regTemp = readRegUInt32(kKeyLargoFCR0);
    regTemp |= kK2FCR0SCCSleepBitsSet;
    regTemp &= ~kK2FCR0SCCSleepBitsClear;
    writeRegUInt32(kKeyLargoFCR0, regTemp);


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
void AppleK2::setReferenceCounts (void)
{
	UInt32 fcr0, fcr1, fcr3, fcr5;
	bool chooseSCCA, chooseI2S1;

	clk45RefCount = 0;
	clk49RefCount = 0;
	fcr0 = readRegUInt32(kKeyLargoFCR0);
	fcr1 = readRegUInt32(kKeyLargoFCR1);
	fcr3 = readRegUInt32(kKeyLargoFCR3);
  
    fcr5 = readRegUInt32(kKeyLargoFCR5);
  
	chooseSCCA = (fcr0 & kKeyLargoFCR0ChooseSCCA);
	chooseI2S1 = !chooseSCCA;

	if (chooseSCCA && (fcr0 & kKeyLargoFCR0SccAEnable) && ((fcr0 & kKeyLargoFCR0SlowSccPClk) == 0)) {
		clk45RefCount++;
	}
  
	if ((fcr0 & kKeyLargoFCR0SccBEnable) && ((fcr0 & kKeyLargoFCR0SlowSccPClk) == 0)) {
		clk45RefCount++;
	}
  
	if (fcr1 & kKeyLargoFCR1I2S0Enable) {
        SInt32 i2sClock = readRegUInt32(kKeyLargoI2S0SerialFormat) & kKeylargoI2SClockSelect;
        if(i2sClock == kKeylargoI2SSelect45Mhz)
            clk49RefCount++;
        else if(i2sClock == kKeylargoI2SSelect49Mhz)
            clk45RefCount++;
	}

	if (chooseI2S1 && (fcr1 & kKeyLargoFCR1I2S1Enable)) {
        SInt32 i2sClock = readRegUInt32(kKeyLargoI2S1SerialFormat) & kKeylargoI2SClockSelect;
        if(i2sClock == kKeylargoI2SSelect45Mhz)
            clk49RefCount++;
        else if(i2sClock == kKeylargoI2SSelect49Mhz)
            clk45RefCount++;
	}

#define DEBUGREFCOUNTS 0
#if DEBUGREFCOUNTS
#define COUNTLOG kprintf
#define PRINTBOOL(b) (b) ? "true" : "false"
#define PRINTNOT(n) (n) ? "" : "NOT"
	COUNTLOG ("AppleK2::setReferenceCounts - chooseSCCA %s and %s enabled\n", 
		PRINTBOOL(chooseSCCA), PRINTNOT(fcr0 & kKeyLargoFCR0SccAEnable));
	COUNTLOG ("AppleK2::setReferenceCounts - chooseI2S0 %s and %s enabled\n", 
		PRINTBOOL(true), PRINTNOT(fcr1 & kKeyLargoFCR1I2S0Enable));
	COUNTLOG ("AppleK2::setReferenceCounts - chooseI2S1 %s and %s enabled\n", 
		PRINTBOOL(chooseI2S1), PRINTNOT(fcr1 & kKeyLargoFCR1I2S1Enable));
	COUNTLOG ("AppleK2::setReferenceCounts - VIA %s enabled\n", 
		PRINTNOT(fcr3 & kKeyLargoFCR3ViaClk16Enable));
	COUNTLOG ("AppleK2::setReferenceCounts - clk45RefCount = %d, clk49RefCount = %d\n",
		clk45RefCount, clk49RefCount);
#endif

	return;
}

void AppleK2::enableCells()
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
	//safeWriteRegUInt32( (unsigned long)kKeyLargoFCR2, kKeyLargoFCR2MPICEnable, kKeyLargoFCR2MPICEnable );

	return;
}

// NOTE: Marco changed the save and restore state to save all keylargo registers.
// this is a temporary fix, the real code should save and restore all registers
// in each specific driver (VIA, MPIC ...) However for now it is easier to follow
// the MacOS9 policy to do everything here.
void AppleK2::saveRegisterState(void)
{
    saveK2State();
    if(keyLargoDeviceId != kShastaDeviceId4f)
        saveVIAState(savedK2State.savedVIAState);
    savedK2State.thisStateIsValid = true;
	
	return;
}

void AppleK2::restoreRegisterState(void)
{
    if (savedK2State.thisStateIsValid) {
        restoreK2State();
        if(keyLargoDeviceId != kShastaDeviceId4f)
            restoreVIAState(savedK2State.savedVIAState);
    }

    savedK2State.thisStateIsValid = false;
	
	return;
}

void AppleK2::safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data)
{
	IOInterruptState intState;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

	UInt32 currentReg = readRegUInt32(offset);
	UInt32 newReg = (currentReg & ~mask) | (data & mask);
    
	
#if 0
    // Check for changing state of I2S clock enable, if so update
    // clock refs and potentially start or stop clk45 or clk49
    if(offset == kKeyLargoFCR1) {
        if((currentReg ^ newReg) & kKeyLargoFCR1I2S0ClkEnable) {
            SInt32 i2sClock = readRegUInt32(kKeyLargoI2S0SerialFormat) & kKeylargoI2SClockSelect;
            UInt32 fcr3Bits = readRegUInt32(kKeyLargoFCR3);
            if(newReg & kKeyLargoFCR1I2S0ClkEnable) {
                if (i2sClock == kKeylargoI2SSelect45Mhz && !(clk45RefCount++)) {
                    fcr3Bits |= kKeyLargoFCR3Clk45Enable;	// turn on clock
                }
                
                if (i2sClock == kKeylargoI2SSelect49Mhz && !(clk49RefCount++)) {
                    fcr3Bits |= kKeyLargoFCR3Clk49Enable;	// turn on clock
                }
            }
            else {
                if (i2sClock == kKeylargoI2SSelect45Mhz && clk45RefCount && !(--clk45RefCount)) {
                    fcr3Bits &= ~kKeyLargoFCR3Clk45Enable;	// turn off clock if refCount reaches zero
                }
                if (i2sClock == kKeylargoI2SSelect49Mhz && clk49RefCount && !(--clk49RefCount)) {
                    fcr3Bits &= ~kKeyLargoFCR3Clk49Enable;	// turn off clock if refCount reaches zero
                }
            }
            writeRegUInt32(kKeyLargoFCR3, fcr3Bits);
        }
    }
#endif
	writeRegUInt32(offset, newReg);
  
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);
	
	return;
}


// --------------------------------------------------------------------------
// Method: initForPM
//
// Purpose:
//   initialize the driver for power managment and register ourselves with
//   superclass policy-maker
void AppleK2::initForPM (IOService *provider)
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
IOReturn AppleK2::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
    // Do not do anything if the state is inavalid.
    if (powerStateOrdinal >= kNumberOfPowerStates)
        return IOPMAckImplied;

    if ( powerStateOrdinal == 0 ) {
        kprintf("K2 would be powered off here\n");
    }
    if ( powerStateOrdinal == 1 ) {
        kprintf("K2 would be powered on here\n");
    }
	if(watchDogTimer)
		watchDogTimer->setSleeping(powerStateOrdinal < 2);
    return IOPMAckImplied;
}

// Method: saveK2State
//
// Purpose:
//        saves the state of all the meaningful registers into a local buffer.
//    this method is almost a copy and paste of the orignal MacOS9 function
//    SaveKeyLargoState. The code does not care about the endianness of the
//    registers since the values are not meaningful, we just wish to save them
//    and restore them.
void AppleK2::saveK2State(void)
{
    K2MPICState*				savedK2MPICState;
    K2GPIOState*				savedK2GPIOState;
    K2ConfigRegistersState*	savedK2ConfigRegistersState;
    K2DBDMAState*				savedK2DBDMAState;
    K2I2SState*					savedK2I2SState;
    UInt32							channelOffset;
    int								i;

    // base of the K2 registers.
    UInt8* k2BaseAddr =	(UInt8*)keyLargoBaseAddress;
    UInt8* mpicBaseAddr =		(UInt8*)keyLargoBaseAddress + kKeyLargoMPICBaseOffset;

    // Save GPIO portion of K2.

    savedK2GPIOState = &savedK2State.savedGPIOState;

    savedK2GPIOState->gpioLevels[0] = *(UInt32 *)(k2BaseAddr + kKeyLargoGPIOLevels0);
    savedK2GPIOState->gpioLevels[1] = *(UInt32 *)(k2BaseAddr + kKeyLargoGPIOLevels1);

    for (i = 0; i < kK2GPIOCount; i++)
    {
        savedK2GPIOState->gpio[i] = *(UInt8 *)(k2BaseAddr + kK2GPIOBase + i);
    }

    // Save I2S registers - 10 registers per channel.

    savedK2I2SState = &savedK2State.savedI2SState;

    for (i = 0, channelOffset = 0; i < kKeyLargoI2SRegisterCount; i++, channelOffset += kKeyLargoI2SRegisterStride)
    {
        savedK2I2SState->i2s[i] = *(UInt32 *) (k2BaseAddr + kKeyLargoI2S0BaseOffset + channelOffset);
        savedK2I2SState->i2s[i + 1] = *(UInt32 *) (k2BaseAddr + kKeyLargoI2S1BaseOffset + channelOffset);
    }

    // Save DBDMA registers.  There are thirteen channels on KeyLargo.

    savedK2DBDMAState = &savedK2State.savedDBDMAState;

    for (i = 0, channelOffset = 0; i < kKeyLargoDBDMAChannelCount; i++, channelOffset += kKeyLargoDBDMAChannelStride)
    {
        volatile DBDMAChannelRegisters*				currentChannel;

        currentChannel = (volatile DBDMAChannelRegisters *) (k2BaseAddr + kKeyLargoDBDMABaseOffset + channelOffset);

        savedK2DBDMAState->dmaChannel[i].commandPtrLo = IOGetDBDMACommandPtr(currentChannel);
        savedK2DBDMAState->dmaChannel[i].interruptSelect = IOGetDBDMAInterruptSelect(currentChannel);
        savedK2DBDMAState->dmaChannel[i].branchSelect = IOGetDBDMABranchSelect(currentChannel);
        savedK2DBDMAState->dmaChannel[i].waitSelect = IOGetDBDMAWaitSelect(currentChannel);
    }

    // Save configuration registers in K2 (FCR 0-10)

    savedK2ConfigRegistersState = &savedK2State.savedConfigRegistersState;

    for (i = 0; i < kK2FCRCount; i++)
    {
        savedK2ConfigRegistersState->featureControl[i] = ((UInt32 *)(k2BaseAddr + kK2FCRBase))[i];
    }

    // Save MPIC portion of KeyLargo.

    savedK2MPICState = &savedK2State.savedMPICState;

    savedK2MPICState->mpicGlobal0 = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICGlobal0);
    savedK2MPICState->mpicIPI[0] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI0);
    savedK2MPICState->mpicIPI[1] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI1);
    savedK2MPICState->mpicIPI[2] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI2);
    savedK2MPICState->mpicIPI[3] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI3);

    savedK2MPICState->mpicSpuriousVector = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICSpuriousVector);
    savedK2MPICState->mpicTimerFrequencyReporting = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICTimeFreq);

    savedK2MPICState->mpicTimers[0] = *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase0);
    savedK2MPICState->mpicTimers[1] = *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase1);
    savedK2MPICState->mpicTimers[2] = *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase2);
    savedK2MPICState->mpicTimers[3] = *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase3);

    for (i = 0; i < kKeyLargoMPICVectorsCount; i++)
    {
        // Make sure that the "active" bit is cleared.
        savedK2MPICState->mpicInterruptSourceVectorPriority[i] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIntSrcVectPriBase + i * kKeyLargoMPICIntSrcSize) & (~0x00000040);
        savedK2MPICState->mpicInterruptSourceDestination[i] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIntSrcDestBase + i * kKeyLargoMPICIntSrcSize);
    }

    savedK2MPICState->mpicCurrentTaskPriorities[0] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP0CurrTaskPriority);
    savedK2MPICState->mpicCurrentTaskPriorities[1] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP1CurrTaskPriority);
    savedK2MPICState->mpicCurrentTaskPriorities[2] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP2CurrTaskPriority);
    savedK2MPICState->mpicCurrentTaskPriorities[3] = *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP3CurrTaskPriority);

	return;
}


// Method: restoreK2State
//
// Purpose:
//        restores the state of all the meaningful registers from a local buffer.
//    this method is almost a copy and paste of the original MacOS9 function
//    RestoreK2State. The code does not care about the endiannes of the
//    registers since the values are not meaningful, we just wish to save them
//    and restore them.
void AppleK2::restoreK2State(void)
{
    K2MPICState*				savedK2MPICState;
    K2GPIOState*				savedK2GPIOState;
    K2ConfigRegistersState*		savedK2ConfigRegistersState;
    K2DBDMAState*				savedK2DBDMAState;
    K2I2SState*					savedK2I2SState;
    UInt32						channelOffset;
    int							i;

    // base of the keylargo registers.
    UInt8* keyLargoBaseAddr =	(UInt8*)keyLargoBaseAddress;
    UInt8* mpicBaseAddr =		(UInt8*)keyLargoBaseAddress + kKeyLargoMPICBaseOffset;

    // Restore MPIC portion of KeyLargo.

    savedK2MPICState = &savedK2State.savedMPICState;

    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICGlobal0) = savedK2MPICState->mpicGlobal0;
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI0) = savedK2MPICState->mpicIPI[0];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI1) = savedK2MPICState->mpicIPI[1];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI2) = savedK2MPICState->mpicIPI[2];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIPI3) = savedK2MPICState->mpicIPI[3];
    eieio();

    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICSpuriousVector) = savedK2MPICState->mpicSpuriousVector;
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICTimeFreq) = savedK2MPICState->mpicTimerFrequencyReporting;
    eieio();

    *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase0) = savedK2MPICState->mpicTimers[0];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase1) = savedK2MPICState->mpicTimers[1];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase2) = savedK2MPICState->mpicTimers[2];
    eieio();
    *(MPICTimers *)(mpicBaseAddr + kKeyLargoMPICTimerBase3) = savedK2MPICState->mpicTimers[3];
    eieio();

    for (i = 0; i < kKeyLargoMPICVectorsCount; i++)
    {
        *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIntSrcVectPriBase + i * kKeyLargoMPICIntSrcSize) = savedK2MPICState->mpicInterruptSourceVectorPriority[i];
        eieio();
        *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICIntSrcDestBase + i * kKeyLargoMPICIntSrcSize) = savedK2MPICState->mpicInterruptSourceDestination[i];
        eieio();
    }

    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP0CurrTaskPriority) = savedK2MPICState->mpicCurrentTaskPriorities[0];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP1CurrTaskPriority) = savedK2MPICState->mpicCurrentTaskPriorities[1];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP2CurrTaskPriority) = savedK2MPICState->mpicCurrentTaskPriorities[2];
    eieio();
    *(UInt32 *)(mpicBaseAddr + kKeyLargoMPICP3CurrTaskPriority) = savedK2MPICState->mpicCurrentTaskPriorities[3];
    eieio();


    // Restore configuration registers in K2

    savedK2ConfigRegistersState = &savedK2State.savedConfigRegistersState;

    for (i = 0; i < kK2FCRCount; i++)
    {
        ((UInt32 *)(keyLargoBaseAddr + kK2FCRBase))[i] = savedK2ConfigRegistersState->featureControl[i];
        eieio();
    }


    IODelay(250);

    // Restore DBDMA registers.  There are thirteen channels on KeyLargo.

    savedK2DBDMAState = &savedK2State.savedDBDMAState;

    for (i = 0, channelOffset = 0; i < kKeyLargoDBDMAChannelCount; i++, channelOffset += kKeyLargoDBDMAChannelStride)
    {
        volatile DBDMAChannelRegisters*				currentChannel;

        currentChannel = (volatile DBDMAChannelRegisters *) (keyLargoBaseAddr + kKeyLargoDBDMABaseOffset + channelOffset);

        IODBDMAReset((IODBDMAChannelRegisters*)currentChannel);
        IOSetDBDMACommandPtr(currentChannel, savedK2DBDMAState->dmaChannel[i].commandPtrLo);
        IOSetDBDMAInterruptSelect(currentChannel, savedK2DBDMAState->dmaChannel[i].interruptSelect);
        IOSetDBDMABranchSelect(currentChannel, savedK2DBDMAState->dmaChannel[i].branchSelect);
        IOSetDBDMAWaitSelect(currentChannel, savedK2DBDMAState->dmaChannel[i].waitSelect);
    }

    // Restore I2S registers - 10 registers per channel.

    savedK2I2SState = &savedK2State.savedI2SState;

    for (i = 0, channelOffset = 0; i < kKeyLargoI2SRegisterCount; i++, channelOffset += kKeyLargoI2SRegisterStride)
    {
        *(UInt32 *) (keyLargoBaseAddr + kKeyLargoI2S0BaseOffset + channelOffset) = savedK2I2SState->i2s[i];
        eieio();
        *(UInt32 *) (keyLargoBaseAddr + kKeyLargoI2S1BaseOffset + channelOffset) = savedK2I2SState->i2s[i + 1];
        eieio();
    }

    // Restore GPIO portion of KeyLargo.

    savedK2GPIOState = &savedK2State.savedGPIOState;

    *(UInt32 *)(keyLargoBaseAddr + kKeyLargoGPIOLevels0) = savedK2GPIOState->gpioLevels[0];
    eieio();
    *(UInt32 *)(keyLargoBaseAddr + kKeyLargoGPIOLevels1) = savedK2GPIOState->gpioLevels[1];
    eieio();

    for (i = 0; i < kK2GPIOCount; i++)
    {
        *(UInt8 *)(keyLargoBaseAddr + kK2GPIOBase + i) = savedK2GPIOState->gpio[i];
        eieio();
    }
	
	return;
}

// Note that this is an overload of performFunction.  The other, extant performFunction
// should go away once this version is fully supported
bool AppleK2::performFunction(IOPlatformFunction *func, void *pfParam1,
			void *pfParam2, void *pfParam3, void *pfParam4)
{
	IOPlatformFunctionIterator 	*iter;
	UInt32 						cmd, cmdLen, result, param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;
	
	if (!func)
		return false;
	
	if (!(iter = func->getCommandIterator()))
		return false;
	
	while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, &result)) {
		if (result != kIOPFNoError) {
			iter->release();
			return false;
		}

		switch (cmd) {
            case kCommandWriteReg32:
                safeWriteRegUInt32(param1, param3, param2);
                break;
                
            case kCommandReadReg32:
                *(UInt32 *)pfParam1 = safeReadRegUInt32(param1);
                break;
                
            case kCommandWriteReg8:
                safeWriteRegUInt8(param1, param3, param2);
                break;
                
            case kCommandReadReg8:
                *(UInt8 *)pfParam1 = safeReadRegUInt8(param1);
                break;
                
            case kCommandReadReg32MaskShRtXOR:
                *(UInt32 *)pfParam1 = ((safeReadRegUInt32(param1) & param2) >> param3) ^ param4;
                break;
                
            case kCommandReadReg8MaskShRtXOR:
                 *(UInt8 *)pfParam1 = ((safeReadRegUInt8(param1) & param2) >> param3) ^ param4;
                 break;
                 
            case kCommandWriteReg32ShLtMask:
                safeWriteRegUInt32(param1, param3, ((UInt32)pfParam1)<<param2);
                break;
                
            case kCommandWriteReg8ShLtMask:
                safeWriteRegUInt8(param1, param3, ((UInt32)pfParam1)<<param2);
                break;
                
			default:
				kprintf ("AppleK2::performFunction - bad command %ld\n", cmd);
				return false;   		        	    
		}
	}
    iter->release();
	return true;
}

IOReturn AppleK2::callPlatformFunction(const OSSymbol *functionName,
                                        bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4)
{  
	if (fPlatformFuncArray) {
		UInt32 i;
		IOPlatformFunction *pfFunc;
		UInt32 count = fPlatformFuncArray->getCount();
		for (i = 0; i < count; i++) {
			if (pfFunc = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i))) {
				// Check for on-demand case
				if (pfFunc->platformFunctionMatch (functionName, kIOPFFlagOnDemand, NULL)) {
					return (performFunction (pfFunc, param1, param2, param3, param4) ? kIOReturnSuccess : kIOReturnBadArgument);
				}
			}
		}
    }
    
    if (functionName == keyLargo_resetUniNEthernetPhy)
    {
        resetUniNEthernetPhy();
        return kIOReturnSuccess;
    }

    if (functionName == k2_enableFireWireClock) {
        enablePCIDeviceClock(kK2FCR1FWClkEnable, (bool)param1, (IOService *)param2);
        return kIOReturnSuccess;
    }
    
    if (functionName == k2_enableEthernetClock) {
        enablePCIDeviceClock(kK2FCR1GBClkEnable, (bool)param1, (IOService *)param2);
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
        turnOffK2IO((bool)param1);
            
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
//kprintf("keyLargo_safeWriteRegUInt32(%p, %p, %p)\n",
//        param1, param2, param3);
        
        safeWriteRegUInt32((unsigned long)param1, (UInt32)param2, (UInt32)param3);
        return kIOReturnSuccess;
    }

    if (functionName == keyLargo_safeReadRegUInt32)
    {
        UInt32 *returnval = (UInt32 *)param2;
//kprintf("keyLargo_safeReadRegUInt32(%p)\n", param1);
        *returnval = safeReadRegUInt32((unsigned long)param1);
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
		*returnVal = (UInt32) gHostK2;
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

    if (functionName == mac_io_publishChildren)
    {
        if (publishChildren((IOService *)param1,(IOService *(*)(IORegistryEntry *))param2))
			return kIOReturnSuccess;
		return kIOReturnError;
	}

    if (functionName == mac_io_publishChild)
    {
        if (publishChild((IOService *)param1, (IORegistryEntry *)param2,
                                    (IOService *(*)(IORegistryEntry *))param3))
			return kIOReturnSuccess;
		return kIOReturnError;
	}

    if (functionName == k2_getHTLinkFrequency) {
		if (getHTLinkFrequency ((UInt32 *)param1))
			return kIOReturnSuccess;
		return kIOReturnError;
	}
	
    if (functionName == k2_setHTLinkFrequency) {
		if (setHTLinkFrequency ((UInt32)param1))
			return kIOReturnSuccess;
		return kIOReturnError;
	}
	
    if (functionName == k2_getHTLinkWidth) {
		if (getHTLinkWidth ((UInt32 *)param1, (UInt32 *)param2))
			return kIOReturnSuccess;
		return kIOReturnError;
	}
	
    if (functionName == k2_setHTLinkWidth) {
		if (setHTLinkWidth ((UInt32)param1, (UInt32)param2))
			return kIOReturnSuccess;
		return kIOReturnError;
	}
	
    if (functionName == keyLargo_EnableI2SModem)
    {
        EnableI2SModem((bool)param1);
        return kIOReturnSuccess;
    }
    
    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

/*
 * EnableSCC - power on or off the SCC cell
 *
 * state - true to power on, false to power off
 * device - 0 for SCCA, 1 for SCCB
 * type - true to use I2S1 under SCCA, 0 for not
 */
void AppleK2::EnableSCC(bool state, UInt8 device, bool type)
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
        } else if(device == 1) {			// SCCB
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
            if((currentReg & kKeyLargoFCR0SlowSccPClk) == 0) {
                if (!(clk45RefCount++)) {
                    currentReg3 |= kKeyLargoFCR3Clk45Enable;	// turn on clock
                }
            }
			writeRegUInt32( (UInt32)kKeyLargoFCR3, (UInt32)currentReg3 );

		}
		
		// Modify current value of register...
        currentReg |= bitsToSet;
        currentReg &= ~bitsToClear;
        
		// ...and write it back
        writeRegUInt32( (UInt32)kKeyLargoFCR0, (UInt32)currentReg );

        if ( mutex  != NULL )			// Release Lock
            IOSimpleLockUnlockEnableInterrupt(mutex, intState);

#if DEBUGREFCOUNTS
		COUNTLOG ("AppleK2::EnableSCC (enable) - clk45RefCount = %d, clk49RefCount = %d\n",
			clk45RefCount, clk49RefCount);
#endif
    } else {	// Powering down
        if(device == 0)
            bitsToClear |= kKeyLargoFCR0SccAEnable;				// Disables SCC A

         else if(device == 1) {
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
            if((currentReg & kKeyLargoFCR0SlowSccPClk) == 0) {
                if (clk45RefCount && !(--clk45RefCount)) {
                    currentReg3 &= ~kKeyLargoFCR3Clk45Enable;	// turn off clock if refCount reaches zero
                }
            }
			writeRegUInt32( (UInt32)kKeyLargoFCR3, (UInt32)currentReg3 );
		}

		// Modify current value of register...
        currentReg |= bitsToSet;
        currentReg &= ~bitsToClear;
        
		// ...and write it back
        writeRegUInt32( (UInt32)kKeyLargoFCR0, (UInt32)currentReg );

        if ( mutex  != NULL )			// Release Lock
            IOSimpleLockUnlockEnableInterrupt(mutex, intState);
			
#if DEBUGREFCOUNTS
		COUNTLOG ("AppleK2::EnableSCC (disable) - clk45RefCount = %d, clk49RefCount = %d\n",
			clk45RefCount, clk49RefCount);
#endif
    }

    return;
}

void AppleK2::PowerModem(bool state)
{
    OSData *prop;
    
    // Make sure ChooseSCCA is 0.
    safeWriteRegUInt32 (kKeyLargoFCR0, kKeyLargoFCR0ChooseSCCA, 0);
    
    if(fHasSoftModem) {
        // Turn the I2S1 clock on or off.
        if(state) {
            safeWriteRegUInt32 (kKeyLargoFCR1, kKeyLargoFCR1I2S1ClkEnable,
                                                kKeyLargoFCR1I2S1ClkEnable);
        }
        else {
            // reset I2S1 before disabling clock
            safeWriteRegUInt32 (kKeyLargoFCR1, kK2FCR1I2S1SWReset, kK2FCR1I2S1SWReset);
            IOSleep(50);
            safeWriteRegUInt32 (kKeyLargoFCR1, kK2FCR1I2S1SWReset, 0);
            safeWriteRegUInt32 (kKeyLargoFCR1, kKeyLargoFCR1I2S1ClkEnable, 0);
        }
    }
    prop = (OSData *) fProvider->getProperty( "platform-modem-power" );
    if(prop && fPHandle) {
        char callName[255];
		IOReturn res;
        sprintf(callName,"%s-%8lx", "platform-modem-power", fPHandle);
        res = IOService::callPlatformFunction(callName, false, (void*) state, 0, 0, 0  );
    }
    else {
        if (state) {
            writeRegUInt8(kKeyLargoGPIOBase + 0x4, 0x4); // power modem on
            eieio();
        } else {
            writeRegUInt8(kKeyLargoGPIOBase + 0x4, 0x5); // power modem off
            eieio();
        }
    }
    return;
}

void AppleK2::ModemResetLow()
{
    OSData *prop;
    prop = (OSData *) fProvider->getProperty( "platform-modem-reset" );
    if(prop && fPHandle) {
        char callName[255];
		IOReturn res;
        sprintf(callName,"%s-%8lx", "platform-modem-reset", fPHandle);
        res = IOService::callPlatformFunction(callName, false, (void*) false, 0, 0, 0  );
    }
    else {
        *(UInt8*)(keyLargoBaseAddress + kKeyLargoGPIOBase + 0x3) |= 0x04;	// Set GPIO3_DDIR to output
        eieio();
        *(UInt8*)(keyLargoBaseAddress + kKeyLargoGPIOBase + 0x3) &= ~0x01;	// Set GPIO3_DataOut output to zero
        eieio();
	}
	return;
}

void AppleK2::ModemResetHigh()
{
    OSData *prop;
    prop = (OSData *) fProvider->getProperty( "platform-modem-reset" );
    if(prop && fPHandle) {
        char callName[255];
		IOReturn res;
        sprintf(callName,"%s-%8lx", "platform-modem-reset", fPHandle);
        res = IOService::callPlatformFunction(callName, false, (void*)true, 0, 0, 0  );
    }
    else {
        *(UInt8*)(keyLargoBaseAddress + kKeyLargoGPIOBase + 0x3) |= 0x04;	// Set GPIO3_DDIR to output
        eieio();
        *(UInt8*)(keyLargoBaseAddress + kKeyLargoGPIOBase + 0x3) |= 0x01;	// Set GPIO3_DataOut output to 1
        eieio();
	}
	return;
}

void AppleK2::PowerI2S (bool powerOn, UInt32 cellNum)
{
	UInt32 fcr0Bits,fcr1Bits, fcr3Bits;
    
    fcr0Bits=0;
	if (cellNum == 0) {
		fcr1Bits = kKeyLargoFCR1I2S0CellEnable |
						kKeyLargoFCR1I2S0ClkEnable |
						kKeyLargoFCR1I2S0Enable;
		fcr3Bits = kKeyLargoFCR3I2S0Clk18Enable;
	} else if (cellNum == 1) {
        fcr0Bits = kKeyLargoFCR0ChooseSCCA;
		fcr1Bits = kKeyLargoFCR1I2S1CellEnable |
						kKeyLargoFCR1I2S1ClkEnable |
						kKeyLargoFCR1I2S1Enable;
		fcr3Bits = kKeyLargoFCR3I2S1Clk18Enable;
    } else if (cellNum == 2 && keyLargoDeviceId == kShastaDeviceId4f) {
        fcr0Bits = kKeyLargoFCR0ChooseSCCB;
        fcr1Bits = kShastaFCR1I2S2CellEnable |
						kShastaFCR1I2S2ClkEnable |
						kShastaFCR1I2S2Enable;
		fcr3Bits = kShastaFCR3I2S2Clk18Enable;
	} else
		return; 		// bad cellNum ignored


	if (powerOn) {
		// turn on all I2S bits (note ChooseSCCA/B bits are inverted)
		safeWriteRegUInt32 (kKeyLargoFCR0, fcr0Bits, 0);
		safeWriteRegUInt32 (kKeyLargoFCR1, fcr1Bits, fcr1Bits);
		safeWriteRegUInt32 (kKeyLargoFCR3, fcr3Bits, fcr3Bits);
	} else {
		// turn off all I2S bits (note ChooseSCCA/B bits are inverted)
		safeWriteRegUInt32 (kKeyLargoFCR0, fcr0Bits, fcr0Bits);
		safeWriteRegUInt32 (kKeyLargoFCR1, fcr1Bits, 0);
		safeWriteRegUInt32 (kKeyLargoFCR3, fcr3Bits, 0);
	}
	return;
}

/*
 * set the power supply to hi or low power state.  This is used for processor
 * speed cycling
 */
IOReturn AppleK2::SetPowerSupply (bool powerHi)
{
	char			value;
	UInt32			delay;
    OSIterator 		*childIterator;
    IORegistryEntry *childEntry;
	OSData			*regData;
	
	if (!k2CPUVCoreSelectGPIO) {
		// locate the gpio node associated with cpu-vcore-select
		if ((childIterator = getChildIterator (gIOServicePlane)) != NULL) {
			while ((childEntry = (IORegistryEntry *)(childIterator->getNextObject ())) != NULL) {
				if (!strcmp ("cpu-vcore-select", childEntry->getName(gIOServicePlane))) {
					regData = OSDynamicCast( OSData, childEntry->getProperty( "reg" ));
					if (regData) {
						// get the GPIO offset from the reg property
						k2CPUVCoreSelectGPIO = *(UInt32 *) regData->getBytesNoCopy();
						break;
					}
				}
			}
			childIterator->release();
		}
	
		if (!k2CPUVCoreSelectGPIO)		// If still unknown return unsupported
			return (kIOReturnUnsupported);
	}

	// Set gpio for 1 for high voltage, 0 for low voltage
	value = kKeyLargoGPIOOutputEnable | (powerHi ? kKeyLargoGPIOData : 0);
	writeRegUInt8 (k2CPUVCoreSelectGPIO, value);
	
	// Wait for power supply to ramp up.
	delay = 200;
	assert_wait(&delay, THREAD_UNINT);
	thread_set_timer(delay, NSEC_PER_USEC);
	thread_block(0);

	return (kIOReturnSuccess);
}

void AppleK2::resetUniNEthernetPhy(void)
{
	void *hasVesta = fProvider->getProperty("hasVesta");
	
	// This should be determined from the device tree.
    kprintf("resetUniNEthernetPhy!\n");

	// WARNING:  GPIO 29 may be routed to a discrete PHY or to a combo PHY
	// like Vesta, which incorporates both Ethernet and FireWire components.
	// If using Vesta, assertion of GPIO 29 will reset FireWire as well as
	// Ethernet.  This is generally not the desired behavior.
	
	// DOUBLE WARNING: This is a temporary fix.  A proper fix involves changing
	// the device tree to better describe the proper behavior.  Expect this code
	// to change - see 3475890
	
	if (!hasVesta) /* using a discrete Ethernet PHY */ {
		// Q45 Ethernet PHY (BCM5221) reset is controlled by GPIO 29.
		// This should be determined from the device tree.
	
		// Assert GPIO29 for 10ms (> 1ms) (active high)
		// to hard reset the Phy, and bring it out of low-power mode.
		writeRegUInt8(kK2GPIOBase + 29, kKeyLargoGPIOOutputEnable | kKeyLargoGPIOData);
		IOSleep(10);
		// Make direction an input so we don't continue to drive the data line
		writeRegUInt8(kK2GPIOBase + 29, 0);
		IOSleep(10);
	}

	return;
}

// **********************************************************************************
// enablePCIDeviceClock
//
// **********************************************************************************
void AppleK2::enablePCIDeviceClock(UInt32 mask, bool enable, IOService *nub)
{
	if (enable && nub) {
        OSData			*cacheData, *latencyData;
        IOPCIDevice		*provider;
    
        provider = OSDynamicCast(IOPCIDevice, nub);
        
        if (provider) {
            cacheData = (OSData *)provider->getProperty (kIOPCICacheLineSize);
            latencyData = (OSData *)provider->getProperty (kIOPCITimerLatency);
        
            if (cacheData || latencyData) {
                UInt32				configData;	
    
                configData = provider->configRead32 (kIOPCIConfigCacheLineSize);
                    
                if (cacheData) 
                    configData = (configData & 0xFFFFFF00) | *(char *) cacheData->getBytesNoCopy();
                
                if (latencyData) 
                    configData = (configData & 0xFFFF00FF) | ((*(char *) latencyData->getBytesNoCopy()) << 8);
                                
                provider->configWrite32 (kIOPCIConfigCacheLineSize, configData);
            }
        }
    }
	safeWriteRegUInt32 (kKeyLargoFCR1, mask, 
		enable ? mask : 0);

    // Give clock sequencer at least 100 microseconds to update clocks
    IOSleep(1);		
    //logClockState();

	return;
}

/*
 * getHTLinkFrequency - return the current HyperTransport link frequency.
 * The result is in bits defined for the link frequency, not in absolute
 * frequency.  The result may be interpret as follows:
 *
 *		0000b			 200MHz
 *		0001b			 300MHz
 *		0010b			 400MHz
 *		0011b			 500MHz
 *		0100b			 600MHz
 *		0101b			 800MHz
 *		0110b			1000MHz
 *		0111b - 1110b	Reserved
 *		1111b			Vendor Specific
 */
bool AppleK2::getHTLinkFrequency (UInt32 *freqResult)
{
	bool			result;
	UInt32			freq;
	IOPCIDevice		*pciProvider;
	
	if (!(pciProvider = OSDynamicCast (IOPCIDevice, keyLargoService->getParentEntry(gIODTPlane))))
		return false;
	
	if (result = (htLinkCapabilitiesBase != 0)) {
		freq = pciProvider->configRead32 (htLinkCapabilitiesBase + kHTLinkCapLDTOffset);
		freq = (freq >> 8) & 0xF;
		*freqResult = freq;
	}
	
	return result;
}

// See getHTLinkFrequency for interpretation of newFreq
bool AppleK2::setHTLinkFrequency (UInt32 newFreq)
{
	bool			result;
	UInt32			freq;
	IOPCIDevice		*pciProvider;
	
	if (!(pciProvider = OSDynamicCast (IOPCIDevice, keyLargoService->getParentEntry(gIODTPlane))))
		return false;
		
	if (result = ((htLinkCapabilitiesBase != 0) && (newFreq <= 0xF))) {
		// Read current 32-bit value
		freq = pciProvider->configRead32 (htLinkCapabilitiesBase + kHTLinkCapLDTOffset);
		freq = (freq & 0xFFFFF0FF) | (newFreq << 8);
		pciProvider->configWrite32 (htLinkCapabilitiesBase + kHTLinkCapLDTOffset, freq);
	}
	
	return result;
}

/*
 * getHTLinkWidth - return the current HyperTransport link in/out width.
 * The results (in and out) may be interpret as follows:
 *
 *		000b	 8-bit
 *		001b	 16-bit
 *		011b	 32-bit
 *		100b	 2-bit
 *		101b	 4-bit
 *		111b	 disconnected
 */
bool AppleK2::getHTLinkWidth (UInt32 *linkOutWidthResult, UInt32 *linkInWidthResult)
{
	bool			result;
	UInt32			width;
	IOPCIDevice		*pciProvider;
	
	if (!(pciProvider = OSDynamicCast (IOPCIDevice, keyLargoService->getParentEntry(gIODTPlane))))
		return false;
	
	if (result = (htLinkCapabilitiesBase != 0)) {
		width = pciProvider->configRead32 (htLinkCapabilitiesBase + kHTLinkCapLinkCtrlOffset);
		*linkOutWidthResult = (width >> 28) & 0x7;
		*linkInWidthResult = (width >> 24) & 0x7;
	}
	
	return result;
}

// See getHTLinkWidth for interpretation of newFreq
bool AppleK2::setHTLinkWidth (UInt32 newLinkOutWidth, UInt32 newLinkInWidth)
{
	bool			result;
	UInt32			width;
	IOPCIDevice		*pciProvider;
	
	if (!(pciProvider = OSDynamicCast (IOPCIDevice, keyLargoService->getParentEntry(gIODTPlane))))
		return false;
	
	if (result = (htLinkCapabilitiesBase != 0) && (newLinkOutWidth <= 0x7) && (newLinkInWidth <= 0x7)) {
		width = pciProvider->configRead32 (htLinkCapabilitiesBase + kHTLinkCapLinkCtrlOffset);
		width = (width & 0x88FFFFFF) | (newLinkOutWidth << 28) | (newLinkInWidth << 24);
		pciProvider->configWrite32 (htLinkCapabilitiesBase + kHTLinkCapLinkCtrlOffset, width);
	}
	
	return result;
}	

void AppleK2::EnableI2SModem(bool enable)
{
    UInt32 fcr0ToClear = kKeyLargoFCR0ChooseSCCA | kKeyLargoFCR0SccAEnable;
    UInt32 fcr1ToSet = kKeyLargoFCR1I2S1CellEnable | kKeyLargoFCR1I2S1Enable;
    UInt32 fcr3ToSet = kKeyLargoFCR3I2S1Clk18Enable;
    
    if(enable) {
        safeWriteRegUInt32 (kKeyLargoFCR0, fcr0ToClear, 0);
        safeWriteRegUInt32 (kKeyLargoFCR3, fcr3ToSet, fcr3ToSet);
        safeWriteRegUInt32 (kKeyLargoFCR1, fcr1ToSet, fcr1ToSet);
        
        // reset I2S1 bus
        safeWriteRegUInt32 (kKeyLargoFCR1, kKeyLargoFCR1I2S1ClkEnable | kK2FCR1I2S1SWReset,
                                            kKeyLargoFCR1I2S1ClkEnable | kK2FCR1I2S1SWReset);
        IOSleep(50);
        safeWriteRegUInt32 (kKeyLargoFCR1, kKeyLargoFCR1I2S1ClkEnable | kK2FCR1I2S1SWReset,
                                            0);
        
    }
    else {
        safeWriteRegUInt32 (kKeyLargoFCR0, fcr0ToClear, fcr0ToClear);
        safeWriteRegUInt32 (kKeyLargoFCR3, fcr3ToSet, 0);
        safeWriteRegUInt32 (kKeyLargoFCR1, fcr1ToSet, 0);
    }
}


IOReturn AppleK2::setAggressiveness( unsigned long selector, unsigned long newLevel )
	{
	if ( selector == kPMEthernetWakeOnLANSettings )
		{
		UInt32						value = 0;

		// If wake-on-lan is enabled, clear EnableGBpadPwrdown in Shasta/K2 FCR3.
		// If wake-on-lan is disabled, set EnableGBpadPwrdown in Shasta/K2 FCR3.

		if ( newLevel == 0 )
			{
			value = kK2FCR3EnableGBpadPwrdown;
			}

		safeWriteRegUInt32( kKeyLargoFCR3, kK2FCR3EnableGBpadPwrdown, value );
		}

	return( super::setAggressiveness( selector, newLevel ) );
	}


void AppleK2::logClockState()
{
    UInt32 clockState;
#define CLOCKLOG kprintf
    
    clockState = readRegUInt32(kK2FCR9);
    
    if(clockState & kK2FCR9PCI1Clk66isStopped)
        CLOCKLOG("PCI1 clock stopped\n");
    else
        CLOCKLOG("PCI1 clock running\n");
        
    if(clockState & kK2FCR9PCI2Clk66isStopped)
        CLOCKLOG("PCI2 clock stopped\n");
    else
        CLOCKLOG("PCI2 clock running\n");
        
    if(clockState & kK2FCR9FWClk66isStopped)
        CLOCKLOG("FireWire clock stopped\n");
    else
        CLOCKLOG("FireWire clock running\n");
        
    if(clockState & kK2FCR9UATAClk66isStopped)
        CLOCKLOG("UATA66 clock stopped\n");
    else
        CLOCKLOG("UATA66 clock running\n");
        
    if(clockState & kK2FCR9UATAClk100isStopped)
        CLOCKLOG("UATA100 clock stopped\n");
    else
        CLOCKLOG("UATA100 clock running\n");
        
    if(clockState & kK2FCR9PCI3Clk66isStopped)
        CLOCKLOG("PCI3 clock stopped\n");
    else
        CLOCKLOG("PCI3 clock running\n");
        
    if(clockState & kK2FCR9GBClk66isStopped)
        CLOCKLOG("Ethernet clock stopped\n");
    else
        CLOCKLOG("Ethernet clock running\n");
        
    if(clockState & kK2FCR9PCI4Clk66isStopped)
        CLOCKLOG("PCI4 clock stopped\n");
    else
        CLOCKLOG("PCI4 clock running\n");
        
    if(clockState & kK2FCR9SATAClk66isStopped)
        CLOCKLOG("SerialATA clock stopped\n");
    else
        CLOCKLOG("SerialATA clock running\n");
        
    if(clockState & kK2FCR9USB0Clk48isStopped)
        CLOCKLOG("USB0 clock stopped\n");
    else
        CLOCKLOG("USB0 clock running\n");
        
    if(clockState & kK2FCR9USB1Clk48isStopped)
        CLOCKLOG("USB1 clock stopped\n");
    else
        CLOCKLOG("USB1 clock running\n");
        
    if(clockState & kK2FCR9Clk45isStopped)
        CLOCKLOG("Clock45 stopped\n");
    else
        CLOCKLOG("Clock45 running\n");
        
    if(clockState & kK2FCR9Clk49isStopped)
        CLOCKLOG("Clock49 stopped\n");
    else
        CLOCKLOG("Clock49 running\n");
        
    if(clockState & kK2FCR9Osc25Shutdown)
        CLOCKLOG("Osc25 stopped\n");
    else
        CLOCKLOG("Osc25 running\n");
        
        
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(AppleK2Device, AppleMacIODevice);
bool AppleK2Device::compareName( OSString * name,
					OSString ** matched ) const
{
    return( IODTCompareNubName( this, name, matched )
         ||  IORegistryEntry::compareName( name, matched ) );
}

IOReturn AppleK2Device::getResources( void )
{
	IOService *mac_io = this;
	if( getDeviceMemory())
        return( kIOReturnSuccess );
 
    while (mac_io && ((mac_io = mac_io->getProvider()) != 0))
        if (strcmp("mac-io", mac_io->getName()) == 0)
            break;
    if (mac_io == 0)
        return kIOReturnError;
    IODTResolveAddressing( this, "reg", mac_io->getDeviceMemoryWithIndex(0) );

    return( kIOReturnSuccess);
}

