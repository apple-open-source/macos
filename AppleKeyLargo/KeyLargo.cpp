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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super AppleMacIO

OSDefineMetaClass(KeyLargo, AppleMacIO)
OSDefineAbstractStructors(KeyLargo, AppleMacIO)

extern "C" {
	extern UInt32 TimeSystemBusKeyLargo ( IOLogicalAddress keyLargoBaseAddr );
}

bool KeyLargo::start(IOService *provider)
{
	IORegistryEntry *viaPMU;
	OSData          *tmpData;
	UInt32          pmuVersion;

	// Get the bus speed from the Device Tree.
	IORegistryEntry *entry;
  
	keyLargoService = provider;

	entry = fromPath("/", gIODTPlane);
	tmpData = OSDynamicCast(OSData, entry->getProperty("clock-frequency"));
	if (tmpData == 0) return false;

	busSpeed = *(unsigned long *)tmpData->getBytesNoCopy();
  
	// Figure out if we're on a notebook
	if (callPlatformFunction ("PlatformIsPortable", true, (void *) &hostIsMobile, (void *)0,
		(void *)0, (void *)0) != kIOReturnSuccess)
			hostIsMobile = false;
			
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
  
	// Re-tune the clocks
	AdjustBusSpeeds ( );
	syncTimeBase();
	
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

	// Figure out how many usb busses are in this chip.
	// They're peers of our provider, called 'usb' and have an 'AAPL,clock-id' property
	// Also get the lowest numbered USB bus (we assume the used busses are consecutive)
	// so the USBKeyLargo objects can be initialised with the right bus numbers.
	IOService *grandParent = provider->getProvider()->getProvider();
	OSIterator *iterator = grandParent->getChildIterator(gIODTPlane);
	IORegistryEntry *peer;
	OSData *clockID;
	fBaseUSBID = kMaxNumUSB;
	while (peer = (IORegistryEntry *)iterator->getNextObject()) {
		clockID = OSDynamicCast(OSData, peer->getProperty("AAPL,clock-id"));
		if(strcmp (peer->getName(), "usb") == 0 && clockID) {
			UInt8 id;
			id = ((UInt8 *)clockID->getBytesNoCopy())[3] - '0';
			if(id < fBaseUSBID)
				fBaseUSBID = id;
			fNumUSB++;
		}
	}
	iterator->release();

	// Figure out if we have a soft modem.
	fHasSoftModem = false;
	entry = fromPath("mac-io/i2s/i2s-b", gIODTPlane);
	if(entry) {
		// Check for compatible containing i2s-modem
		OSData *data = OSDynamicCast(OSData, entry->getProperty("compatible"));
		if(data) {
			fHasSoftModem = strncmp("i2s-modem", (const char *)data->getBytesNoCopy(), data->getLength()) == 0;
		}
	}

	return true;
}

// Following routine should do nothing in Merlot.
long long KeyLargo::syncTimeBase(void)
{
	UInt32			cnt;
	UInt32			gtLow, gtHigh, gtHigh2;
	UInt32			tbLow, tbHigh, tbHigh2;
	long long       tmp, diffTicks, ratioLow, ratioHigh;
  
	tmp = gPEClockFrequencyInfo.timebase_frequency_hz;
	ratioLow = (tmp << 32) / (kKeyLargoGTimerFreq);
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

void KeyLargo::recalibrateBusSpeeds(void)
{
	AdjustBusSpeeds();
}


UInt8 KeyLargo::readRegUInt8(unsigned long offset)
{
    return *(UInt8 *)(keyLargoBaseAddress + offset);
}

void KeyLargo::writeRegUInt8(unsigned long offset, UInt8 data)
{
    *(UInt8 *)(keyLargoBaseAddress + offset) = data;

    eieio();
	
	return;
}

void KeyLargo::safeWriteRegUInt8(unsigned long offset, UInt8 mask, UInt8 data)
{
	IOInterruptState intState;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

	UInt8 currentReg = readRegUInt8(offset);
	currentReg = (currentReg & ~mask) | (data & mask);
	writeRegUInt8(offset, currentReg);
  
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);
	
	return;
}

UInt8 KeyLargo::safeReadRegUInt8(unsigned long offset)
{
	IOInterruptState intState;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);
  
	UInt8 currentReg = readRegUInt8(offset);
  
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

	return (currentReg);  
}

UInt32 KeyLargo::readRegUInt32(unsigned long offset)
{
    return lwbrx(keyLargoBaseAddress + offset);
}

void KeyLargo::writeRegUInt32(unsigned long offset, UInt32 data)
{
	stwbrx(data, keyLargoBaseAddress + offset);
	eieio();
	
	return;
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

// Method: saveVIAState
//
// Purpose:
//        saves the state of all the meaningful registers into a local buffer.
//    this method is almost a copy and paste of the orignal MacOS9 function
//    SaveVIAState.
void KeyLargo::saveVIAState(UInt8* savedK2ViaState)
{
    UInt8* viaBase = (UInt8*)keyLargoBaseAddress + kKeyLargoVIABaseOffset;

    // Save VIA state.  These registers don't seem to get restored to any known state.
    savedK2ViaState[0] = *(UInt8*)(viaBase + kKeyLargovBufA);
    savedK2ViaState[1] = *(UInt8*)(viaBase + kKeyLargovDIRA);
    savedK2ViaState[2] = *(UInt8*)(viaBase + kKeyLargovBufB);
    savedK2ViaState[3] = *(UInt8*)(viaBase + kKeyLargovDIRB);
    savedK2ViaState[4] = *(UInt8*)(viaBase + kKeyLargovPCR);
    savedK2ViaState[5] = *(UInt8*)(viaBase + kKeyLargovACR);
    savedK2ViaState[6] = *(UInt8*)(viaBase + kKeyLargovIER);
    savedK2ViaState[7] = *(UInt8*)(viaBase + kKeyLargovT1C);
    savedK2ViaState[8] = *(UInt8*)(viaBase + kKeyLargovT1CH);
	
	return;
}


// Method: restoreVIAState
//
// Purpose:
//        restores the state of all the meaningful registers from a local buffer.
//    this method is almost a copy and paste of the orignal MacOS9 function
//    RestoreVIAState.
void KeyLargo::restoreVIAState(UInt8* savedK2ViaState)
{
    UInt8* viaBase = (UInt8*)keyLargoBaseAddress + kKeyLargoVIABaseOffset;

    // Restore VIA state.  These registers don't seem to get restored to any known state.
    *(UInt8*)(viaBase + kKeyLargovBufA) = savedK2ViaState[0];
    eieio();
    *(UInt8*)(viaBase + kKeyLargovDIRA) = savedK2ViaState[1];
    eieio();
    *(UInt8*)(viaBase + kKeyLargovBufB) = savedK2ViaState[2];
    eieio();
    *(UInt8*)(viaBase + kKeyLargovDIRB) = savedK2ViaState[3];
    eieio();
    *(UInt8*)(viaBase + kKeyLargovPCR) = savedK2ViaState[4];
    eieio();
    *(UInt8*)(viaBase + kKeyLargovACR) = savedK2ViaState[5];
    eieio();
    *(UInt8*)(viaBase + kKeyLargovIER) = savedK2ViaState[6];
    eieio();
    *(UInt8*)(viaBase + kKeyLargovT1C) = savedK2ViaState[7];
    eieio();
    *(UInt8*)(viaBase + kKeyLargovT1CH) = savedK2ViaState[8];
    eieio();
	
	return;
}

/*
 * The bus speed values reported by Open Firmware may not be accurate enough so we calculate
 * our own values and make them know to mach.
 */
void KeyLargo::AdjustBusSpeeds ( void )
  {
	IOInterruptState 	is;
	IOSimpleLock		*intLock;
	UInt32				ticks;
	UInt64				systemBusHz;
#if 0
	UInt64				tmp64;
#endif
	
	intLock = IOSimpleLockAlloc();
	if (intLock) {
		IOSimpleLockInit (intLock);
		is = IOSimpleLockLockDisableInterrupt(intLock);	// There shall be no interruptions
	}
	
	ticks = TimeSystemBusKeyLargo (keyLargoBaseAddress);
	if (intLock) {
		IOSimpleLockUnlockEnableInterrupt(intLock, is);	// As you were
		IOSimpleLockFree (intLock);
	}
	
	systemBusHz = 4194300;
	systemBusHz *= 18432000;
	systemBusHz /= ticks;
	
#if 0
	kprintf ("KeyLargo::AdjustBusSpeeds - ticks = %ld, new systemBusHz = %ld, old systemBusHz = %ld\n", ticks,
		(UInt32)(systemBusHz & 0xFFFFFFFF), (UInt32)(gPEClockFrequencyInfo.bus_clock_rate_hz & 0xFFFFFFFF));

	kprintf ("old clock frequency values:\n");
	kprintf ("    bus_clock_rate_hz:   %ld\n", gPEClockFrequencyInfo.bus_clock_rate_hz);
	kprintf ("    cpu_clock_rate_hz:   %ld\n", gPEClockFrequencyInfo.cpu_clock_rate_hz);
	kprintf ("    dec_clock_rate_hz:   %ld\n", gPEClockFrequencyInfo.dec_clock_rate_hz);
	kprintf ("    bus_clock_rate_num:  %ld\n", gPEClockFrequencyInfo.bus_clock_rate_num);
	kprintf ("    bus_clock_rate_den:  %ld\n", gPEClockFrequencyInfo.bus_clock_rate_den);
	kprintf ("    bus_to_cpu_rate_num: %ld\n", gPEClockFrequencyInfo.bus_to_cpu_rate_num);
	kprintf ("    bus_to_cpu_rate_den: %ld\n", gPEClockFrequencyInfo.bus_to_cpu_rate_den);
	kprintf ("    bus_to_dec_rate_num: %ld\n", gPEClockFrequencyInfo.bus_to_dec_rate_num);
	kprintf ("    bus_to_dec_rate_den: %ld\n", gPEClockFrequencyInfo.bus_to_dec_rate_den);
	kprintf ("    timebase_frequency_hz:   %ld\n", gPEClockFrequencyInfo.timebase_frequency_hz);
	kprintf ("    timebase_frequency_num:  %ld\n", gPEClockFrequencyInfo.timebase_frequency_num);
	kprintf ("    timebase_frequency_den:  %ld\n", gPEClockFrequencyInfo.timebase_frequency_den);
#endif
	
	// Don't adjust the bus and dec clock frequency, the kernel only wants accurate timebase
	// and with TBEN active there is no longer a simple conversion from decrementer rate to bus
	// frequency.
#if 0
	tmp64 = systemBusHz;
	tmp64 /= gPEClockFrequencyInfo.bus_clock_rate_den;
	gPEClockFrequencyInfo.bus_clock_rate_num = (UInt32) (tmp64 & 0xFFFFFFFF);
	// Set the truncated numbers in gPEClockFrequencyInfo.
	gPEClockFrequencyInfo.bus_clock_rate_hz = (UInt32) (systemBusHz & 0xFFFFFFFF);
	//gPEClockFrequencyInfo.cpu_clock_rate_hz = tmp_cpu_speed;
	gPEClockFrequencyInfo.dec_clock_rate_hz = (UInt32) ((systemBusHz & 0xFFFFFFFF) / 4);
#endif

	gPEClockFrequencyInfo.timebase_frequency_hz = (UInt32) ((systemBusHz & 0xFFFFFFFF) / 4);
	gPEClockFrequencyInfo.timebase_frequency_num = gPEClockFrequencyInfo.timebase_frequency_hz;
	gPEClockFrequencyInfo.timebase_frequency_den = 1;
	
#if 0
	kprintf ("new clock frequency values:\n");
	kprintf ("    bus_clock_rate_hz:   %ld\n", gPEClockFrequencyInfo.bus_clock_rate_hz);
	kprintf ("    cpu_clock_rate_hz:   %ld\n", gPEClockFrequencyInfo.cpu_clock_rate_hz);
	kprintf ("    dec_clock_rate_hz:   %ld\n", gPEClockFrequencyInfo.dec_clock_rate_hz);
	kprintf ("    bus_clock_rate_num:  %ld\n", gPEClockFrequencyInfo.bus_clock_rate_num);
	kprintf ("    bus_clock_rate_den:  %ld\n", gPEClockFrequencyInfo.bus_clock_rate_den);
	kprintf ("    bus_to_cpu_rate_num: %ld\n", gPEClockFrequencyInfo.bus_to_cpu_rate_num);
	kprintf ("    bus_to_cpu_rate_den: %ld\n", gPEClockFrequencyInfo.bus_to_cpu_rate_den);
	kprintf ("    bus_to_dec_rate_num: %ld\n", gPEClockFrequencyInfo.bus_to_dec_rate_num);
	kprintf ("    bus_to_dec_rate_den: %ld\n", gPEClockFrequencyInfo.bus_to_dec_rate_den);
	kprintf ("    timebase_frequency_hz:   %ld\n", gPEClockFrequencyInfo.timebase_frequency_hz);
	kprintf ("    timebase_frequency_num:  %ld\n", gPEClockFrequencyInfo.timebase_frequency_num);
	kprintf ("    timebase_frequency_den:  %ld\n", gPEClockFrequencyInfo.timebase_frequency_den);
#endif

	// notify the kernel of the change
	PE_call_timebase_callback ();

	return;
}

bool KeyLargo::publishChildren(IOService * driver, IOService *(*createChildNub)(IORegistryEntry *))
{
    OSIterator			* kids;
    IORegistryEntry		* next;
	IOService			* provider;

	provider = OSDynamicCast(IOService, driver->getParentEntry(gIOServicePlane));
	if (!provider) {
		return false;
	}

    kids = IODTFindMatchingEntries(provider, kIODTExclusive, deleteList() );
//	kids = provider->getChildIterator (gIODTPlane);
	if (kids) {
		while (next = (IORegistryEntry *)kids->getNextObject())
		{
			publishChild(driver, next, createChildNub);
		}
		kids->release();
	}
	return true;
}

bool KeyLargo::publishChild(IOService * driver, IORegistryEntry *child,
							IOService *(*createChildNub)(IORegistryEntry *))
{
    IOService			* nub;

	if (createChildNub)
		nub = createChildNub(child);
	else
		nub = createNub(child);
	if (0 == nub)
		return false;

	nub->attach( driver );
	processNub(nub);
	nub->registerService();
	return true;
}

