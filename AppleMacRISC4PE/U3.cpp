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


#include <IOKit/platform/ApplePlatformExpert.h>

#include "U3.h"
#include "MacRISC4PE.h"

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Map memory map IO space */
#include <mach/mach_types.h>
__END_DECLS

static const OSSymbol *symsafeReadRegUInt32;
static const OSSymbol *symsafeWriteRegUInt32;
static const OSSymbol *symUniNSetPowerState;
static const OSSymbol *symUniNPrepareForSleep;

#define super IOService
OSDefineMetaClassAndStructors(AppleU3,ApplePlatformExpert)



// **********************************************************************************
// start
//
// **********************************************************************************
bool AppleU3::start ( IOService * nub )
{
    // UInt32			   		uniNArbCtrl, uniNMPCIMemTimeout;
	IOInterruptState 		intState = 0;
	IOPlatformFunction		*func;
	const OSSymbol			*functionSymbol = OSSymbol::withCString(kInstantiatePlatformFunctions);
	SInt32					retval;
	
		
	// If our PE isn't MacRISC4PE, we shouldn't be here
	if (!OSDynamicCast (MacRISC4PE, getPlatform())) return false;
	
	provider = nub;

	// Get our memory mapping
	uniNMemory = provider->mapDeviceMemoryWithIndex( 0 );
	if (!uniNMemory) {
		kprintf ("AppleU3::start - no memory\n");
		return false;
	}
	
	uniNBaseAddress = (UInt32 *)uniNMemory->getVirtualAddress();
	
	// sets up the mutex lock:
	mutex = IOSimpleLockAlloc();

	if (mutex != NULL)
		IOSimpleLockInit( mutex );

    // Set a lock for tuning UniN (currently nothing to tune)
	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);
		
				
    uniNVersion = readUniNReg(kUniNVersion);

    if (uniNVersion < kUniNVersion3) {
		kprintf ("AppleU3::start - UniN version 0x%lx not supported\n", uniNVersion);
		return false;
    }
	
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);
  
	// Figure out if we're on a notebook
	if (callPlatformFunction ("PlatformIsPortable", true, (void *) &hostIsMobile, (void *)0,
		(void *)0, (void *)0) != kIOReturnSuccess)
			hostIsMobile = false;

	// setup built-in platform functions
	symreadUniNReg = OSSymbol::withCString("readUniNReg");
        symsafeReadRegUInt32 = OSSymbol::withCString("safeReadRegUInt32");
	symsafeWriteRegUInt32 = OSSymbol::withCString("safeWriteRegUInt32");
	symUniNSetPowerState = OSSymbol::withCString("UniNSetPowerState");
    symUniNPrepareForSleep = OSSymbol::withCString("UniNPrepareForSleep");
    symGetHTLinkFrequency = OSSymbol::withCString("getHTLinkFrequency");
    symSetHTLinkFrequency = OSSymbol::withCString("setHTLinkFrequency");
    symGetHTLinkWidth = OSSymbol::withCString("getHTLinkWidth");
    symSetHTLinkWidth = OSSymbol::withCString("setHTLinkWidth");
    symSetSPUSleep = OSSymbol::withCString("setSPUsleep");
    symSetPMUSleep = OSSymbol::withCString("sleepNow");
	symU3APIPhyDisableProcessor1 = OSSymbol::withCString("u3APIPhyDisableProcessor1");

	// Identify any platform-do-functions
	retval = callPlatformFunction (functionSymbol, true, (void *)provider, 
		(void *)&platformFuncArray, (void *)0, (void *)0);
	if (retval == kIOReturnSuccess && (platformFuncArray != NULL)) {
		unsigned int i;
		
		// Examine the functions and for any that are demand, publish the function so callers can find us
		for (i = 0; i < platformFuncArray->getCount(); i++)
			if (func = OSDynamicCast (IOPlatformFunction, platformFuncArray->getObject(i)))
				if (func->getCommandFlags() & kIOPFFlagOnDemand)
					func->publishPlatformFunction (this);
	}

	// If we have our own MPIC, enable it
	if (mpicRegEntry = fromPath ("mpic", gIODTPlane, NULL, NULL, provider)) {
		// Set interrupt enable bits in U3 toggle register
		safeWriteRegUInt32(kU3ToggleRegister, kU3MPICEnableOutputs | kU3MPICReset, 
			kU3MPICEnableOutputs | kU3MPICReset);
	}

	// Create our friends
	createNubs(this, provider->getChildIterator( gIODTPlane ));
  	
	// Come and get it...
	registerService();
	
			
	// If we have a chip fault interrupt, install the handler and enable notifications.  It's normal for
	// this to fail on some platforms, since U3.1 and U3.2 don't have a chip fault pin.
	//
	// this must come after registerService() because it does a waitForService() ...
	if (kIOReturnSuccess == installChipFaultHandler( provider ))
	{
		// we have a chip fault notifier, initialize related functionality such as ECC
		setupDARTExcp();
		setupECC();
	}

	return super::start(provider);
}

void AppleU3::free ()
{

	if (platformFuncArray) {
		platformFuncArray->flushCollection();
		platformFuncArray->release();
	}

	if (mutex != NULL)
		IOSimpleLockFree( mutex );

	if (dimmErrors)
		IOFree( dimmErrors, sizeof(u3_parity_error_record_t) * dimmCount );

	if (dimmLock)
		IOSimpleLockFree( dimmLock );

	if (dimmErrorCountsTotal)
		IOFree( dimmErrorCountsTotal, dimmCount * sizeof(UInt32) );

	super::free();
	
	return;
}

// **********************************************************************************
// callPlatformFunction
//
// **********************************************************************************
IOReturn AppleU3::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, 
		void *param1, void *param2, void *param3, void *param4)
{
    if (functionName == symsafeReadRegUInt32)
    {
        UInt32 *returnval = (UInt32 *)param2;
        *returnval = safeReadRegUInt32((UInt32)param1);
        return kIOReturnSuccess;
    }
	
    if (functionName == symsafeWriteRegUInt32)
    {
        safeWriteRegUInt32((UInt32)param1, (UInt32)param2, (UInt32)param3);
        return kIOReturnSuccess;
    }

    if (functionName == symUniNSetPowerState)
    {
        uniNSetPowerState((UInt32)param1);
        return kIOReturnSuccess;
    }
	
    if (functionName == symUniNPrepareForSleep)
    {
        prepareForSleep();
        return kIOReturnSuccess;
    }
	
    if (functionName == symGetHTLinkFrequency) {
		if (getHTLinkFrequency ((UInt32 *)param1))
			return kIOReturnSuccess;
		return kIOReturnError;
	}
	
    if (functionName == symSetHTLinkFrequency) {
		if (setHTLinkFrequency ((UInt32)param1))
			return kIOReturnSuccess;
		return kIOReturnError;
	}
	
    if (functionName == symGetHTLinkWidth) {
		if (getHTLinkWidth ((UInt32 *)param1, (UInt32 *)param2))
			return kIOReturnSuccess;
		return kIOReturnError;
	}
	
    if (functionName == symSetHTLinkWidth) {
		if (setHTLinkWidth ((UInt32)param1, (UInt32)param2))
			return kIOReturnSuccess;
		return kIOReturnError;
	}

    if (functionName == symU3APIPhyDisableProcessor1) {
		u3APIPhyDisableProcessor1 ();
		return kIOReturnSuccess;
	}
        
    if (functionName == symreadUniNReg) {
                UInt32 *returnval = (UInt32 *)param2;
                *returnval = readUniNReg((UInt32)param1);
                return kIOReturnSuccess;
    }
    
	if (platformFuncArray) {
		UInt32 i;
		IOPlatformFunction *pfFunc;
		
		for (i = 0; i < platformFuncArray->getCount(); i++)
			if (pfFunc = OSDynamicCast (IOPlatformFunction, platformFuncArray->getObject(i)))
				// Check for on-demand case
				if (pfFunc->platformFunctionMatch (functionName, kIOPFFlagOnDemand, NULL))
					return (performFunction (pfFunc, param1, param2, param3, param4)  ? kIOReturnSuccess : kIOReturnBadArgument);
	}

    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}


IOReturn AppleU3::callPlatformFunction(const char *functionName, bool waitForFunction, 
		void *param1, void *param2, void *param3, void *param4)
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

// **********************************************************************************
// readUniNReg
//
// **********************************************************************************
UInt32 AppleU3::readUniNReg(UInt32 offset)
{
    return uniNBaseAddress[offset >> 2];
}

// **********************************************************************************
// writeUniNReg
//
// **********************************************************************************
void AppleU3::writeUniNReg(UInt32 offset, UInt32 data)
{
    uniNBaseAddress[offset >> 2] = data;


    OSSynchronizeIO();

	return;
}

// **********************************************************************************
// safeReadRegUInt32
//
// **********************************************************************************
UInt32 AppleU3::safeReadRegUInt32(UInt32 offset)
{
	IOInterruptState intState = 0;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);
  
	UInt32 currentReg = readUniNReg(offset);
  
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);

	return (currentReg);  
}

// **********************************************************************************
// safeWriteRegUInt32
//
// **********************************************************************************
void AppleU3::safeWriteRegUInt32(UInt32 offset, UInt32 mask, UInt32 data)
{
	IOInterruptState	intState = 0;
	UInt32 				currentReg;

	if ( mutex  != NULL )
		intState = IOSimpleLockLockDisableInterrupt(mutex);

	if (mask == ~0UL)	// Just write out the data
		currentReg = data;
	else {
		// read, modify then write the data
		currentReg = readUniNReg(offset);
		currentReg = (currentReg & ~mask) | (data & mask);
	}
		
	writeUniNReg (offset, currentReg);
  
	if ( mutex  != NULL )
		IOSimpleLockUnlockEnableInterrupt(mutex, intState);
	
	return;
}

// **********************************************************************************
// uniNSetPowerState
//
// **********************************************************************************
void AppleU3::uniNSetPowerState (UInt32 state)
{
	UInt32 data;

	if (state == kUniNNormal)		// start and wake
	{		
		// Set MPIC interrupt enable bits in U3 toggle register, but only if MPIC is present
		if (mpicRegEntry)
			//safeWriteRegUInt32(kU3ToggleRegister, kU3MPICEnableOutputs | kU3MPICReset, 
				//kU3MPICEnableOutputs | kU3MPICReset);
			safeWriteRegUInt32(kU3ToggleRegister, kU3MPICEnableOutputs, 
				kU3MPICEnableOutputs);

		// Set the running state for HWInit.
		safeWriteRegUInt32(kUniNHWInitState, ~0UL, kUniNHWInitStateRunning);

		safeWriteRegUInt32(kU3DARTCntlRegister, ~0UL, saveDARTCntl);

		// ClockControl moved (and is saved by the SPU), and VSPSoftReset no longer exists, so
		// don't touch them on Kodiak machines.

		if (!IS_U4(uniNVersion))
		{
			safeWriteRegUInt32(kU3PMClockControl, ~0UL, saveClockCntl);
			safeWriteRegUInt32(kUniNVSPSoftReset, ~0UL, saveVSPSoftReset);
		}
	}
	else if (state == kUniNIdle2)
	{
		IOLog ("AppleU3: Idle 2 state not supported\n");
	}
	else if (state == kUniNSave)		// save state
	{
		saveDARTCntl = safeReadRegUInt32(kU3DARTCntlRegister);

		if (!IS_U4(uniNVersion))
		{
			saveClockCntl = safeReadRegUInt32(kU3PMClockControl);
			saveVSPSoftReset = safeReadRegUInt32(kUniNVSPSoftReset);
		}
	}
	else if (state == kUniNSleep)		// sleep
	{
		if (spu)	// Only call spu if present [3448210]
			spu->callPlatformFunction (symSetSPUSleep, false, (void *)false, (void *)0, (void *)0, (void *)0);
			
		// Set the sleeping state for HWInit.  This tells OF we were sleeping
		safeWriteRegUInt32(kUniNHWInitState, ~0UL, kUniNHWInitStateSleeping);
	
		// Clear MPIC interrupt output enable bit in U3 toggle register, but only if MPIC is present
		if (mpicRegEntry)
			safeWriteRegUInt32(kU3ToggleRegister, kU3MPICEnableOutputs, 0);
		
		// Set HyperTransport back to default state
		setHTLinkFrequency (0);		// Set U3 end link frequency to 200 MHz

			
			// Set K2 end link frequency to 200MHz
		if (k2)
			k2->callPlatformFunction (symSetHTLinkFrequency, false, (void *)0, (void *)0, (void *)0, (void *)0);
			
		setHTLinkWidth (0, 0);		// Set U3 end link width 8-bit
		
		// Set K2 end link width 8-bit - not required as K2 only runs 8 bit
		// if (k2) k2->callPlatformFunction (symSetHTLinkWidth, false, (void *)0, (void *)0, (void *)0, (void *)0);


	}
	
	return;
}

// **********************************************************************************
// performFunction
//
// **********************************************************************************
bool AppleU3::performFunction(const IOPlatformFunction *func, void *cpfParam1,
			void *cpfParam2, void *cpfParam3, void *cpfParam4)
{
	static IOLock				*pfLock;
	bool						ret;
	IOPlatformFunctionIterator 	*iter;
	UInt32 						offset, value, valueLen, mask, maskLen, data = 0, writeLen, 
									cmd, cmdLen, result, pHandle, lastCmd = 0,
									param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;

	IOPCIDevice					*nub = NULL;
	
	if (func == 0) return(false);
	
	if (!pfLock)
		// Use a static lock here as there is only ever one instance of U3
		pfLock = IOLockAlloc();
	
	if (pfLock)
		IOLockLock (pfLock);

	if (!(iter = ((IOPlatformFunction *)func)->getCommandIterator())) {
		if (pfLock)
			IOLockUnlock (pfLock);

		return false;
	}

	pHandle = func->getCommandPHandle();
	
	ret = true;
	while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, &result)  && ret) {
		if (result != kIOPFNoError)
			ret = false;
		else
			// Examine the command - not all commands are supported
			switch (cmd) {
				case kCommandWriteReg32:
					offset = param1;
					value = param2;
					mask  = param3;
		// XXX This code is wrong  - it should just call safeWriteRegUInt32(offset, mask, value)
		// XXX see also kCommandRMWConfig
					// If mask isn't all ones, read data and mask it
					if (mask != 0xFFFFFFFF) {
						data = readUniNReg (offset);
						data &= mask;
						data |= value;
					} else	// just write the data
						data = value;
					
					// write the result to the Uni-N register
					writeUniNReg(offset, data);
					
					break;
		
				// Currently only handle config reads of 4 bytes or less
				case kCommandReadConfig:
					offset = param1;
					valueLen = param2;
					
					if (valueLen != 4) {
						IOLog ("AppleU3::performFunction config reads cannot handle anything other than 4 bytes, found length %ld\n", valueLen);
						ret = false;
					}
		
					if (!nub) {
						if (!pHandle) {
							IOLog ("AppleU3::performFunction config read requires pHandle to locate nub\n");
							ret = false;
						}
						nub = findNubForPHandle (pHandle);
						if (!nub) {
							IOLog ("AppleU3::performFunction config read cannot find nub for pHandle 0x%08lx\n", pHandle);
							ret = false;
						}
					}
					
					// NOTE - code below assumes read of 4 bytes, i.e., valueLen == 4!!
					data = nub->configRead32 (offset);
					if (cpfParam1)
						*(UInt32 *)cpfParam1 = data;
					
					lastCmd = kCommandReadConfig;
					break;
					
				// Currently only handle config reads/writes of 4 bytes
				case kCommandRMWConfig:
					// data must have been read above
					if (lastCmd != kCommandReadConfig) {
						IOLog ("AppleU3::performFunction - config modify/write requires prior read\n");
						ret = false;
					}
					
					offset = param1;
					maskLen = param2;
					valueLen = param3;
					writeLen = param4;
	
					if (writeLen != 4) {
						IOLog ("AppleU3::performFunction config read/modify/write cannot handle anything other than 4 bytes, found length %ld\n", writeLen);
						ret = false;
					}
					
					// NOTE - code below makes implicit assumption that mask and value are each 4 bytes!!
					mask = *(UInt32 *)param5;
					value = *(UInt32 *)param6;
		
					if (!nub) {
						if (!pHandle) {
							IOLog ("AppleU3::performFunction config read/modify/write requires pHandle to locate nub\n");
							ret = false;
						}
						nub = findNubForPHandle (pHandle);
						if (!nub) {
							IOLog ("AppleU3::performFunction config read/modify/write cannot find nub for pHandle 0x%08lx\n", pHandle);
							ret = false;
						}
					}
					
					// data must have been previously read (i.e., using kCommandReadConfig with result in data)
					data &= mask;
					data |= value;
					
					nub->configWrite32 (offset, data);
		
					break;
		
				default:
					IOLog("AppleU3::performFunction got unsupported command %08lx\n", cmd);
					ret = false;
					break;
		}
	}
	
	iter->release();

	if (pfLock)
		IOLockUnlock (pfLock);

	return(ret);
}

IOPCIDevice* AppleU3::findNubForPHandle( UInt32 pHandleValue )
{
	IORegistryIterator*								iterator;
	IORegistryEntry*								matchingEntry = NULL;

	iterator = IORegistryIterator::iterateOver( gIODTPlane, kIORegistryIterateRecursively );

	if ( iterator == NULL )
		return( NULL );

	while ( ( matchingEntry = iterator->getNextObject() ) != NULL ) {
		OSData*						property;

		if ( ( property = OSDynamicCast( OSData, matchingEntry->getProperty( "AAPL,phandle" ) ) ) != NULL )
			if ( pHandleValue == *( ( UInt32 * ) property->getBytesNoCopy() ) )
				break;
	}
	
	iterator->release();
	
	return( OSDynamicCast (IOPCIDevice, matchingEntry));
}

void AppleU3::prepareForSleep ( void )
{
	IOService *service;
	static bool noGolem, noSPU;
	
	// Find the K2 driver
	if (!k2)
		k2 = waitForService(serviceMatching("KeyLargo"));
	
	// Find the SPU driver, if present
	if (!(spu || noSPU)) {
		if (fromPath("/spu", gIODTPlane))		// Does /spu exist [3448210]?
			spu = waitForService(serviceMatching("AppleSPU"));	// Yes, wait for associated driver

		noSPU = (spu == NULL);				// If node or driver not found, set noSPU true so we don't keep looking for it
	}
	
	// Find the PMU device
	if (!pmu) {
		service = waitForService(resourceMatching("IOPMU"));
		if (service) 
			pmu = OSDynamicCast (IOService, service->getProperty("IOPMU"));
	}
		
	// Find golem, if present
	if (!(golem || noGolem)) {
		golem = OSDynamicCast (IOPCIDevice, provider->fromPath("/ht@0,F2000000/pci@1", gIODTPlane));
		if (golem) {
			// Check that it's pci-x compatible
			if (!(IODTMatchNubWithKeys(golem, "pci-x"))) {
				noGolem = true;		// Set noGolem true so we don't keep looking for it
				golem = NULL;		// Zero out the reference to it so we won't use it
			}

		} else
			noGolem = true;			// Set noGolem true so we don't keep looking for it
	}

	return;
}


bool AppleU3::getHTLinkFrequency (UInt32 *freqResult)
{
	UInt32			freq;

	freq = safeReadRegUInt32 (kU3HTLinkFreqRegister);
	freq = (freq >> 8) & 0xF;
	*freqResult = freq;
	
	return true;
}

// See getHTLinkFrequency for interpretation of newFreq
bool AppleU3::setHTLinkFrequency (UInt32 newFreq)
{
	UInt32			freq;

	// Read current 32-bit value
	freq = safeReadRegUInt32 (kU3HTLinkFreqRegister);
	freq = (freq & 0xFFFFF0FF) | (newFreq << 8);
	safeWriteRegUInt32 (kU3HTLinkFreqRegister, ~0UL, freq);
	
	return true;
}


bool AppleU3::getHTLinkWidth (UInt32 *linkOutWidthResult, UInt32 *linkInWidthResult)
{
	UInt32			width;

	width = safeReadRegUInt32 (kU3HTLinkConfigRegister);
	*linkOutWidthResult = (width >> 28) & 0x7;
	*linkInWidthResult = (width >> 24) & 0x7;
	
	return true;
}

// See getHTLinkWidth for interpretation of newFreq
bool AppleU3::setHTLinkWidth (UInt32 newLinkOutWidth, UInt32 newLinkInWidth)
{
	UInt32			width;

	width = safeReadRegUInt32 (kU3HTLinkConfigRegister);
	width = (width & 0x88FFFFFF) | (newLinkOutWidth << 28) | (newLinkInWidth << 24);
	safeWriteRegUInt32 (kU3HTLinkConfigRegister, ~0UL, width);
	
	return true;
}	

// **********************************************************************************
// u3APIPhyDisableProcessor1
//
// **********************************************************************************
void AppleU3::u3APIPhyDisableProcessor1 ( void ) 
{


	safeWriteRegUInt32 (kU3APIPhyConfigRegister1, ~0UL, 0);		// еее not correct for dual-core (but not fatal)
	return;
}

// **********************************************************************************
// installChipFaultHandler
//
// The U3 twins (lite/heavy) have a chip fault signal (CHP_FAULT_N) that the system
// implementors can attach to a GPIO.  Chip fault is asserted in response to an
// ApplePI exception and can be masked with the Chip Fault Mask Register.
//
// This routine will be called from AppleU3::start() if there is a
// "platform-chip-fault" property in the u3 device tree node.  It will create the
// appropriate platform function symbols and install a callback function to handle
// chip fault events.
//
// ECC memory errors are delivered to the system using this mechanism.
//
// **********************************************************************************
IOReturn AppleU3::installChipFaultHandler ( IOService * provider )
{
	const OSData	*provider_phandle;
	char			stringBuf[32];
	UInt32			pHandle;

	symChipFaultFunc = symPFIntRegister = symPFIntEnable = symPFIntDisable = NULL;

	// We should have a chip fault signal on U3-Lite, U3-Heavy and U4
	if (provider->getProperty(kChipFaultFuncName) == NULL)
	{
		if ( IS_U3_HEAVY(uniNVersion) || IS_U4(uniNVersion) )
			kprintf("AppleU3: WARNING: %s property expected, but not found\n", kChipFaultFuncName );
	
		return kIOReturnUnsupported;
	}

	if ((provider_phandle = OSDynamicCast( OSData, provider->getProperty("AAPL,phandle") )) == NULL)
		return kIOReturnNotFound;

	symPFIntRegister	= OSSymbol::withCString(kIOPFInterruptRegister);
	symPFIntEnable		= OSSymbol::withCString(kIOPFInterruptEnable);
	symPFIntDisable		= OSSymbol::withCString(kIOPFInterruptDisable);

	// construct a function symbol of the form "platform-chip-fault-ff001122"
	pHandle = *((UInt32 *)provider_phandle->getBytesNoCopy());
	if ( pHandle == 0 )
		IOLog( "AppleU3::installChipFaultHandler - pHandle should not be zero!\n" );
	// kChipFaultFuncName = "platform-chip-fault" == 19 chars; "-%08lx" == 9 chars; 1 char for EOS; 29 bytes total
	snprintf(stringBuf, sizeof(stringBuf)-1, "%s-%08lx", kChipFaultFuncName, pHandle);
	symChipFaultFunc = OSSymbol::withCString(stringBuf); 

	// Mask all chip fault sources.  Bits that we want unmasked will be handled separately.
	if ( IS_U4(uniNVersion) )
		safeWriteRegUInt32 ( kU4APIMask1Register, ~0UL, 0 );
	else
		safeWriteRegUInt32 ( kU3ChipFaultMaskRegister, ~0UL, 0 );

	// register for notifications
	return callPlatformFunction(symChipFaultFunc, TRUE,
			(void *) AppleU3::sHandleChipFault, this, NULL, (void *) symPFIntRegister);
}

// **********************************************************************************
// sHandleChipFault
//
// C-style static callback for chip fault interrupt.
//
// **********************************************************************************

// Use this Syndrome Table to match against the MESR to determine exactly
// which bit was corrected [0:127].  Note: this only works for correctable 1-bit
// errors -- with uncorrectable errors, we can only know the rank.
#define kECCAllBits 144
UInt32 SyndromeTable[kECCAllBits] =
{
        0x0849 , 0x0428 , 0x0214 , 0x01C2 , 0x9084 , 0x8042 , 0x4021 , 0x201C , 0x4908 ,
        0x2804 , 0x1402 , 0xC201 , 0x8490 , 0x4280 , 0x2140 , 0x1C20 , 0x0894 , 0x0482 ,
        0x0241 , 0x012C , 0x4089 , 0x2048 , 0x1024 , 0xC012 , 0x9408 , 0x8204 , 0x4102 ,
        0x2C01 , 0x8940 , 0x4820 , 0x2410 , 0x12C0 , 0x0881 , 0x044C , 0x0226 , 0x0113 ,
        0x1088 , 0xC044 , 0x6022 , 0x3011 , 0x8108 , 0x4C04 , 0x2602 , 0x1301 , 0x8810 ,
        0x44C0 , 0x2260 , 0x1130 , 0x0288 , 0x0144 , 0x0C22 , 0x0611 , 0x8028 , 0x4014 ,
        0x20C2 , 0x1061 , 0x8802 , 0x4401 , 0x220C , 0x1106 , 0x2880 , 0x1440 , 0xC220 ,
        0x6110 , 0x0B48 , 0x0924 , 0x0812 , 0x04C1 , 0x80B4 , 0x4092 , 0x2081 , 0x104C ,
        0x480B , 0x2409 , 0x1208 , 0xC104 , 0xB480 , 0x9240 , 0x8120 , 0x4C10 , 0x0198 ,
        0x0C84 , 0x0642 , 0x0321 , 0x8019 , 0x40C8 , 0x2064 , 0x1032 , 0x9801 , 0x840C ,
        0x4206 , 0x2103 , 0x1980 , 0xC840 , 0x6420 , 0x3210 , 0x0868 , 0x0434 , 0x02D2 ,
        0x01A1 , 0x8086 , 0x4043 , 0x202D , 0x101A , 0x6808 , 0x3404 , 0xD202 , 0xA101 ,
        0x8680 , 0x4340 , 0x2D20 , 0x1A10 , 0x8884 , 0x4442 , 0x2221 , 0x111C , 0x4888 ,
        0x2444 , 0x1222 , 0xC111 , 0x8488 , 0x4244 , 0x2122 , 0x1C11 , 0x8848 , 0x4424 ,
        0x2212 , 0x11C1 , 0x8000 , 0x4000 , 0x2000 , 0x1000 , 0x0800 , 0x0400 , 0x0200 ,
        0x0100 , 0x0080 , 0x0040 , 0x0020 , 0x0010 , 0x0008 , 0x0004 , 0x0002 , 0x0001
};





/* static */	/* executing on system workloop */
void AppleU3::sHandleChipFault( void * vSelf, void * vRefCon, void * /* NULL */, void * /* unused */ )
{
UInt32	apiexcp, mear, mear1, mesr, rank, dimmloc, savedMaskRegister;
char	errstr[128];

	AppleU3 * me = OSDynamicCast( AppleU3, (OSMetaClassBase *) vSelf );

	// don't use 'me' before we check to make sure it's okay
	if (!me) return;

	// Mask all chip fault sources.
	if ( IS_U4(me->uniNVersion) )
	{
		savedMaskRegister = me->safeReadRegUInt32( kU4APIMask1Register );
		me->safeWriteRegUInt32 ( kU4APIMask1Register, ~0UL, 0 );
	}
	else
	{
		savedMaskRegister = me->safeReadRegUInt32( kU3ChipFaultMaskRegister );
		me->safeWriteRegUInt32 ( kU3ChipFaultMaskRegister, ~0UL, 0 );
	}

	// read the APIEXCP register to find out the source of this event. 
	// **************************i*********************************
	// NOTE - the read operation causes the faults to be cleared.
	// **************************i*********************************
	if ( IS_U4(me->uniNVersion) )
		apiexcp = me->safeReadRegUInt32( kU4APIExceptionRegister );
	else
		apiexcp = me->safeReadRegUInt32( kU3APIExceptionRegister );

	// Catch DART exceptions
	if ( (apiexcp & kU3API_DARTExcp) || (apiexcp & kU4API_DARTExcp) )
	{
		UInt32 dartexcp = 0;

		if( IS_U4(me->uniNVersion) )
		{
			dartexcp = me->safeReadRegUInt32( kU4DARTExceptionRegister );
			if ( dartexcp & kU4DARTExcpRQOPMask ) {
				char	xcdString[40];
				switch((dartexcp & kU4DARTExcpXCDMask))			 //          1         2         3         4
				{												 // 1234567890123456789012345678901234567890
					case 0:
						snprintf(xcdString, sizeof( xcdString )-1, "XBE DART out of bounds Exception: ");
						break;
					case 1:
						snprintf(xcdString, sizeof( xcdString)-1,  "XBE DART Entry Exception: ");
						break;
					case 2:
						snprintf(xcdString, sizeof( xcdString)-1, "XBE DART Read Protection Exception: ");
						break;
					case 3:
						snprintf(xcdString, sizeof( xcdString)-1, "XBE DART Write Protection Exception: ");
						break;
					case 4:
						snprintf(xcdString, sizeof( xcdString)-1, "XBE DART Addressing Exception: ");
						break;
					case 5:
						snprintf(xcdString, sizeof( xcdString)-1, "XBE DART TLB Parity Error: ");
						break;
				}
#if 0	// no more panics on DART exceptions -- this wouldn't happen on a non-DART system.  Just log it.
				snprintf( errstr, sizeof( errstr)-1, "DART %s%s %s logical page 0x%05lX\n",
					xcdString,
					( dartexcp & kU4DARTExcpRQSRCMask )? "HyperTransport" :
														 "PCI0",
					( dartexcp & kU4DARTExcpRQOPMask ) ? "write" :
														 "read",
					( dartexcp & kU4DARTExcpLogAdrsMask ) >> kU4DARTExcpLogAdrsShift );
				// kaboom!
				panic( "%s", errstr );
#endif
				snprintf( errstr, sizeof( errstr)-1, "DMA %s%s %s logical page 0x%05lX\n",
					xcdString,
					( dartexcp & kU4DARTExcpRQSRCMask )? "HyperTransport" :
														 "PCI0",
					( dartexcp & kU4DARTExcpRQOPMask ) ? "write" :
														 "read",
					( dartexcp & kU4DARTExcpLogAdrsMask ) >> kU4DARTExcpLogAdrsShift );
				kprintf( "%s", errstr );
				IOLog( "%s", errstr );
			}

		}
		else		// U3H
		{
			dartexcp = me->safeReadRegUInt32( kU3DARTExceptionRegister );
			// rdar://4137750 -- ONLY panic on DART WRITE errors.  Ignore DART READs.
			// they are often caused by PCI speculative reads, which are typically bogus.
			if ( dartexcp & kU3DARTExcpRQOPMask )	// writes ONLY
			{
#if 0	// no more panics on DART exceptions -- this wouldn't happen on a non-DART system.  Just log it.
				snprintf( errstr, sizeof( errstr )-1,    "DART %s%s%s %s logical page 0x%05lX\n",
					( dartexcp & kU3DARTExcpXBEMask )?   "out-of-bounds exception: " : "",
					( dartexcp & kU3DARTExcpXEEMask )?   "entry exception: " : "",
					( dartexcp & kU3DARTExcpRQSRCMask )? "HyperTransport" : "PCI0",
					( dartexcp & kU3DARTExcpRQOPMask )?  "write" : "read",
					( dartexcp & kU3DARTExcpLogAdrsMask ) >> kU3DARTExcpLogAdrsShift );

				// kaboom!
				panic( "%s", errstr );
#endif
				snprintf( errstr, sizeof( errstr )-1,    "DMA %s%s%s write logical page 0x%05lX\n",
					( dartexcp & kU3DARTExcpXBEMask )?   "out-of-bounds exception: " : "",
					( dartexcp & kU3DARTExcpXEEMask )?   "entry exception: " : "",
					( dartexcp & kU3DARTExcpRQSRCMask )? "HyperTransport" : "PCI0",
					( dartexcp & kU3DARTExcpLogAdrsMask ) >> kU3DARTExcpLogAdrsShift );
				kprintf( "%s", errstr );
				IOLog( "%s", errstr );
			}
			
		}

	}

	// if this is U3 Heavy, Check for ECC errors
	if ( IS_U3_HEAVY(me->uniNVersion) && (apiexcp & (kU3API_ECC_UE_H | kU3API_ECC_UE_L | kU3API_ECC_CE_H | kU3API_ECC_CE_L)) )
	{
	UInt32	activeUEbits, activeCEbits, CEbitsToCheck = 0;

		// interrogate U3 ECC registers
		//
		//	*** NOTE ***	reading the MESR causes the ECC state to be cleared
		//

		mear = me->safeReadRegUInt32( kU3MemErrorAddressRegister );
		mesr = me->safeReadRegUInt32( kU3MemErrorSyndromeRegister );
		
		// get the dimm slot index
		rank = (mear & kU3MEAR_RNK_A_mask) >> kU3MEAR_RNK_A_shift;

		// grab the uncorrectable and correctable ECC bit settings
		activeUEbits  = apiexcp & (kU3API_ECC_UE_H | kU3API_ECC_UE_L);
		activeCEbits  = apiexcp & (kU3API_ECC_CE_H | kU3API_ECC_CE_L);
		// retrieve the upper/lower syndrome values
		syndromes     = mesr &  kU3MESR_ECC_SYNDROMES_mask;
		upperSyndrome = ( syndromes >> 8 ) & kU3MESR_ECC_SYNDROME_mask;
		lowerSyndrome =   syndromes        & kU3MESR_ECC_SYNDROME_mask;

		// Check for uncorrectable errors
		if ( activeUEbits & ( kU3API_ECC_UE_H | kU3API_ECC_UE_L ) )		// if EITHER the upper or lower uncorrectable error is indicated
		{


			{
				// according to Sally F, if the ECC_xE_H bit(s) are set, you need to choose the HIGHER of the dimm-pair
				// and if BOTH UE_H and UE_L are set, we _should_ be reporting BOTH DIMMs as having errors.
				dimmloc = rank - (rank % 2);	// gives dimm number of dimm where UE_L occurred.  add 1 for UE_H.
				if ( activeUEbits == ( kU3API_ECC_UE_H | kU3API_ECC_UE_L ) )
				{
					snprintf( errstr, sizeof( errstr )-1, "DIMMs %s & %s",
								me->dimmErrors[dimmloc].slotName, me->dimmErrors[dimmloc+1].slotName );
				}
				else
				{
					if (apiexcp & kU3API_ECC_UE_H)	// check if error was in upper or lower DIMM
						dimmloc ++;					// if upper, add 1 to *dimmloc*
					snprintf( errstr, sizeof( errstr )-1, "%s", me->dimmErrors[dimmloc].slotName );
				}

				/*  ***** DEATH BY UNCORRECTABLE ERROR HAPPENS HERE *****  */

				panic("Uncorrectable parity error detected in %s (APIEXCP=0x%08lX, MEAR=0x%08lX MESR=0x%08lX)\n",
					/* slot name(s) */ errstr, apiexcp, mear, mesr);
			}
		}


		if ( activeCEbits )	// from APIEXCP
		{
			// according to Sally F, if the ECC_xE_H bit(s) are set, you need to choose the HIGHER of the dimm-pair
			// and if BOTH ECC_CE_H and CE_L are set, we ought to be setting updates for both DIMMs, not just one.

			dimmloc = rank - (rank % 2 );	// gives location of lower DIMM of the DIMM-pair
			IOSimpleLockLock( me->dimmLock );

			// if BOTH bits are set, update the counts for both lower and upper DIMM location
			if ( activeCEbits == ( kU3API_ECC_CE_H | kU3API_ECC_CE_L ) )
			{
				//kprintf("AppleU3 got correctable error in dimms %u and %u\n", dimmloc, dimmloc+1);
				me->dimmErrors[dimmloc].count++;
				me->dimmErrors[dimmloc+1].count++;
			}
			else	// either one or the other - figure out if we we need to further refine *dimmloc*
			{
				if ( activeCEbits & kU3API_ECC_CE_H )	// if CE_H is set, CE_L isn't, and we need to increment *dimmloc*
					dimmloc ++;
				//kprintf("AppleU3 got correctable error dimm %u\n", dimmloc);
				me->dimmErrors[dimmloc].count++;
			}

			IOSimpleLockUnlock( me->dimmLock );

			// schedule a notification thread callout (if not already scheduled)
			if (thread_call_is_delayed( me->eccErrorCallout, NULL ) == FALSE)
			{
			AbsoluteTime deadline;
			
				clock_interval_to_deadline( kU3ECCNotificationIntervalMS, kMillisecondScale, &deadline );
				thread_call_enter1_delayed( me->eccErrorCallout, vRefCon, deadline);
			}
		}
	}

	// if this is U4, Check for ECC error
	if ( IS_U4(me->uniNVersion) && (apiexcp & (kU4API_ECC_UEExcp | kU4API_ECC_CEExcp)) )
	{
		// interrogate U4 ECC registers
		mear = me->safeReadRegUInt32( kU4MemErrorAddressRegister1 );
		mear1 = me->safeReadRegUInt32( kU4MemErrorAddressRegister2 );
		mesr = me->safeReadRegUInt32( kU4MemErrorSyndromeRegister );

		// get the dimm slot index
		dimmloc = rank = (mear & kU4MEAR_RK_mask) >> kU4MEAR_RK_shift;

		// Check for an uncorrectable error
		if (apiexcp & kU4API_ECC_UEExcp)
		{
			panic("Uncorrectable parity error detected in rank %ld [%s, %s] (MEAR0=0x%08lX MEAR1=0x%08lX MESR=0x%08lX)\n",
				rank, me->dimmErrors[dimmloc].slotName, me->dimmErrors[dimmloc+1].slotName, mear, mear1, mesr);
		}
		else
		{
			int i; 

			// Search Syndrome Table to find exact bit which caused CE.  Use bit to determine if the error occurred
			// on the lower/upper DIMM in the rank.  In general, using the rank and syndrome we can determine which
			// DIMM caused the correctable error.  For uncorrectable errors, we can only know the rank (pair of DIMMs).
			for ( i = 0; i <= 127; i++ )
			{
				if ( SyndromeTable[i] == (mesr & 0xFFFF) )
				{
					if( i > 63 ) // if low bit, then pick then next dimm in the rank.
						dimmloc++;

					break;

				}
			}
		}

		IOSimpleLockLock( me->dimmLock );
		me->dimmErrors[dimmloc].count++;
		IOSimpleLockUnlock( me->dimmLock );

		// schedule a notification thread callout (if not already scheduled)
		if (thread_call_is_delayed( me->eccErrorCallout, NULL ) == FALSE)
		{
			AbsoluteTime deadline;
			clock_interval_to_deadline( kU3ECCNotificationIntervalMS, kMillisecondScale, &deadline );
			thread_call_enter1_delayed( me->eccErrorCallout, vRefCon, deadline);
		}
	}

DART_CASCADE_SKIP:

	// Restore mask register.
	if ( IS_U4(me->uniNVersion) )
		me->safeWriteRegUInt32 ( kU4APIMask1Register, savedMaskRegister, savedMaskRegister );
	else
		me->safeWriteRegUInt32 ( kU3ChipFaultMaskRegister, savedMaskRegister, savedMaskRegister );
}

// **********************************************************************************
// sDispatchECCNotifier
//
// C-style callback method for periodic ECC notification.
//
// **********************************************************************************

/* static */
void AppleU3::sDispatchECCNotifier( void *self, void *refcon )
{
	AppleU3 * me = OSDynamicCast( AppleU3, (OSMetaClassBase *) self );

	//kprintf("AppleU3::sDispatchECCNotifier\n");

	if (me) me->eccNotifier( refcon );
}

// **********************************************************************************
// eccNotifier
//
// C++-style callback method for periodic ECC notification.
//
// **********************************************************************************

void AppleU3::eccNotifier( void * refcon )
{
	UInt32 slot;
	UInt32 * dimmErrorCounts;
	u3_parity_error_msg_t msg;
	IORegistryEntry * memoryNode;
	OSData *eccCeCounts = NULL;
	
	// kprintf("AppleU3::eccNotifier\n");

	// allocate memory for a copy of the dimm error counts
	dimmErrorCounts = (UInt32 *) IOMalloc( dimmCount * sizeof(UInt32) );

	if (!dimmErrorCounts)
	{
		IOLog("ECC Notifier -- failed to allocate memory!\n");
		return;
	}

	// get a copy of the dimm error counts and zero out the live array
	IOSimpleLockLock( dimmLock );

	for (slot=0; slot<dimmCount; slot++)
	{
		dimmErrorCountsTotal[slot] += dimmErrors[slot].count;
		dimmErrorCounts[slot] = dimmErrors[slot].count;
		dimmErrors[slot].count = 0;
	}

	IOSimpleLockUnlock( dimmLock );

	if ((memoryNode = fromPath("/memory", gIODTPlane, 0, 0, 0)) != NULL)
	{
		eccCeCounts = OSData::withBytes(dimmErrorCountsTotal, 4 * dimmCount);
		memoryNode->setProperty("ecc-ce-counts", eccCeCounts);

		eccCeCounts->release();
		memoryNode->release();
	}

	// post a notification for each slot that's encountered errors
	for (slot=0; slot<dimmCount; slot++)
	{
		if (dimmErrorCounts[slot] > 0)
		{
			// write to system log
			IOLog("WARNING: %lu parity errors corrected in %s\n",
					dimmErrorCounts[slot], dimmErrors[slot].slotName);

			// send a notification to interested clients
			msg.version = 0x1;
			msg.slotIndex = slot;
			strncpy(msg.slotName, dimmErrors[slot].slotName, 32);
			msg.count = dimmErrorCounts[slot];
		
			messageClients( kIOPlatformMessageParityError,
					(void *) &msg, sizeof(u3_parity_error_msg_t) );
		}
	}

	IOFree( dimmErrorCounts, dimmCount * sizeof(UInt32) );
}

// **********************************************************************************
// setupECC
//
// If we are on U3 heavy and ECC has been enabled by HWInit, unmask the chip fault
// sources for ECC errors.
//
// **********************************************************************************

void AppleU3::setupECC( void )
{
	IORegistryEntry * memoryNode;
	const OSData * slotNamesData;
	const char * slotNames;
	UInt32 bits, i, slotBitField;

	// If it's not U3 Heavy or U4, we don't have ECC
	if ( !IS_U3_HEAVY(uniNVersion) && !IS_U4(uniNVersion) ) return;

	// ECC is initialized and enabled by HWInit if possible.  Check if it is turned on.
	if ( ! ( kU3MCCR_ECC_EN & safeReadRegUInt32( kU3MemCheckCtrlRegister ) )) return;

	// grab the DIMM slot names from the device tree, they are in the /memory node under the
	// slot-names property.  The first word of the property is a bit field with one bit set
	// for each available slot.  A null-terminated c-style ascsii string follows, one for each
	// slot.
	if ((memoryNode = fromPath("/memory", gIODTPlane, 0, 0, 0)) == NULL) return;
	slotNamesData = OSDynamicCast(OSData, memoryNode->getProperty("slot-names"));
	memoryNode->release();
	if (!slotNamesData) return;

	slotNames = (const char *) slotNamesData->getBytesNoCopy();
	slotBitField = *(const UInt32 *) slotNames;	// grab the bitfield word
	slotNames += sizeof(UInt32);	// advance the pointer to the first ascii name

	// count the number of slots we have
	dimmCount = 0;
	for (i=0; i<32; i++)
	{
		if (!(slotBitField & (1 << i))) break;
		dimmCount++;
	}

	if (dimmCount > kU3MaxDIMMSlots) return;

	// initialize 
	dimmErrorCountsTotal = (UInt32 *) IOMalloc( dimmCount * sizeof(UInt32) );

	// kprintf("AppleU3::setupECC dimmCount is %u\n", dimmCount);

	// allocate a thread callout so we can service chip faults without worrying about
	// blocking someone's workloop
	eccErrorCallout = thread_call_allocate((thread_call_func_t) AppleU3::sDispatchECCNotifier,
													(thread_call_param_t) this);
	if (!eccErrorCallout) return;

	// allocate and initialize the lock that protects the error counts
	if ((dimmLock = IOSimpleLockAlloc()) == NULL) return;
	IOSimpleLockInit( dimmLock );

	// allocate an array to hold all the dimm error counts
	dimmErrors = (u3_parity_error_record_t *) IOMalloc( sizeof(u3_parity_error_record_t) * dimmCount );
	if (!dimmErrors)
	{
		IOSimpleLockFree( dimmLock );
		dimmLock = NULL;
		return;
	}

	// copy slot names into the array -- no need to lock this yet, since other threads won't
	// try to modify it until we've unmasked the error bits below
	for (i=0; i<dimmCount; i++)
	{
		strncpy( dimmErrors[i].slotName, slotNames, 31 );	// copy the slot name
		dimmErrors[i].slotName[31] = '\0';	// guarantee a terminating null
		dimmErrors[i].count = 0;	// zero the error count
		dimmErrorCountsTotal[i] = 0;	// zero the total error count
		slotNames += strlen(slotNames) + 1;	// advance to the next dimm name
	}

	IOLog( "Enabling ECC Error Notifications\n" );

	// flag that this is an ecc supported memory controller
	setProperty("ecc-supported", "true" );

	if ( IS_U4(uniNVersion) )
	{
		// Clear the mask bits in the MCCR to enable error propogation
		bits = kU4MCCR_ECC_UE_MASK | kU4MCCR_ECC_CE_MASK;
		safeWriteRegUInt32( kU4MemCheckCtrlRegister, bits, ~bits );

		// Set the mask bits in the CFMR to enable chip fault generation
		bits = kU4API_ECC_UEExcp | kU4API_ECC_CEExcp;
		safeWriteRegUInt32( kU4APIMask1Register, bits, bits );
	}
	else
	{
		// Clear the mask bits in the MCCR to enable error propogation
		bits = kU3MCCR_ECC_UE_MASK_H | kU3MCCR_ECC_CE_MASK_H | kU3MCCR_ECC_UE_MASK_L | kU3MCCR_ECC_CE_MASK_L;
		safeWriteRegUInt32( kU3MemCheckCtrlRegister, bits, ~bits );

		// Set the mask bits in the CFMR to enable chip fault generation
		bits = kU3API_ECC_UE_H | kU3API_ECC_CE_H | kU3API_ECC_UE_L | kU3API_ECC_CE_L;
		safeWriteRegUInt32( kU3ChipFaultMaskRegister, bits, bits );
	}
}

// **********************************************************************************
// setupDARTExcp
//
// This if called to enable DART exception notification via U3Twins' chip fault.
//
// **********************************************************************************

void AppleU3::setupDARTExcp( void )
{
	// Set the mask bit in the CFMR to enable chip fault generation
	if ( IS_U4(uniNVersion) )
		safeWriteRegUInt32( kU4APIMask1Register, kU3API_DARTExcp, kU3API_DARTExcp );
	else
		safeWriteRegUInt32( kU3ChipFaultMaskRegister, kU3API_DARTExcp, kU3API_DARTExcp );
}

