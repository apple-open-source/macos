/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
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
		kprintf ("AppleU3::start - UniN version 0x%x not supported\n", uniNVersion);
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

	if (state == kUniNNormal) {		// start and wake
		// Set MPIC interrupt enable bits in U3 toggle register, but only if MPIC is present
		if (mpicRegEntry)
			//safeWriteRegUInt32(kU3ToggleRegister, kU3MPICEnableOutputs | kU3MPICReset, 
				//kU3MPICEnableOutputs | kU3MPICReset);
			safeWriteRegUInt32(kU3ToggleRegister, kU3MPICEnableOutputs, 
				kU3MPICEnableOutputs);

		// Set the running state for HWInit.
		safeWriteRegUInt32(kUniNHWInitState, ~0UL, kUniNHWInitStateRunning);
	} else if (state == kUniNIdle2) {
		IOLog ("AppleU3: Idle 2 state not supported\n");
	} else if (state == kUniNSleep) {		// sleep
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
	bool						ret;
	IOPlatformFunctionIterator 	*iter;
	UInt32 						offset, value, valueLen, mask, maskLen, data = 0, writeLen, 
									cmd, cmdLen, result, pHandle = 0, lastCmd = 0,
									param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;

	IOPCIDevice					*nub = NULL;
	
	if (func == 0) return(false);

	if (!(iter = ((IOPlatformFunction *)func)->getCommandIterator()))
		return false;
	
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
		// XXX This code is wrong  - it just just call safeWriteRegUInt32(offset, mask, value)
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


	safeWriteRegUInt32 (kU3APIPhyConfigRegister1, ~0UL, 0);
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
	char			stringBuf[256];
	UInt32			pHandle;

	symChipFaultFunc = symPFIntRegister = symPFIntEnable = symPFIntDisable = NULL;

	// We should have a chip fault signal on U3 lite and heavy
	if (provider->getProperty(kChipFaultFuncName) == NULL)
	{
		if (IS_U3_HEAVY(uniNVersion))
			IOLog("AppleU3: WARNING: platform-chip-fault expected, but not found\n");
	
		return kIOReturnUnsupported;
	}

	if ((provider_phandle = OSDynamicCast( OSData, provider->getProperty("AAPL,phandle") )) == NULL)
		return kIOReturnNotFound;

	symPFIntRegister	= OSSymbol::withCString(kIOPFInterruptRegister);
	symPFIntEnable		= OSSymbol::withCString(kIOPFInterruptEnable);
	symPFIntDisable		= OSSymbol::withCString(kIOPFInterruptDisable);

	// construct a function symbol of the form "platform-chip-fault-ff001122"
	pHandle = *((UInt32 *)provider_phandle->getBytesNoCopy());
	sprintf(stringBuf, "%s-%08lx", kChipFaultFuncName, pHandle);
	symChipFaultFunc = OSSymbol::withCString(stringBuf); 

	// Mask all chip fault sources.  Bits that we want unmasked will be handled separately.
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

/* static */	/* executing on system workloop */
void AppleU3::sHandleChipFault( void * vSelf, void * vRefCon, void * /* NULL */, void * /* unused */ )
{
	UInt32 apiexcp;

	AppleU3 * me = OSDynamicCast( AppleU3, (OSMetaClassBase *) vSelf );

	// kprintf( "AppleU3::sHandleChipFault\n" );

	if (!me) return;

	// read the APIEXCP register to find out the source of this event.  the read operation
	// causes the faults to be cleared.
	apiexcp = me->safeReadRegUInt32( kU3APIExceptionRegister );

	// kprintf( "AppleU3 Got Chip Fault! APIEXCP = %08lX\n", apiexcp );

	// Catch DART exceptions
	if (apiexcp & kU3API_DARTExcp)
	{
		char errstr[128];
		UInt32 dartexcp = me->safeReadRegUInt32( kU3DARTExceptionRegister );

		sprintf( errstr, "DART %s%s%s %s logical page 0x%05lX\n",
			( dartexcp & kU3DARTExcpXBEMask ) ? "out-of-bounds exception: " : "",
			( dartexcp & kU3DARTExcpXEEMask ) ? "entry exception: " : "",
			( dartexcp & kU3DARTExcpRQSRCMask ) ? "HyperTransport" : "PCI0",
			( dartexcp & kU3DARTExcpRQOPMask ) ? "write" : "read",
			( dartexcp & kU3DARTExcpLogAdrsMask ) >> kU3DARTExcpLogAdrsShift );

		// kaboom!
		panic( errstr );
	}

	// if this is U3 Heavy, Check for ECC error
	if (IS_U3_HEAVY(me->uniNVersion) &&
	    (apiexcp & (kU3API_ECC_UE_H | kU3API_ECC_UE_L | kU3API_ECC_CE_H | kU3API_ECC_CE_L)))
	{
		UInt32 mear, mesr, rank, dimmloc;

		// interrogate U3 ECC registers
		mear = me->safeReadRegUInt32( kU3MemErrorAddressRegister );
		mesr = me->safeReadRegUInt32( kU3MemErrorSyndromeRegister );

		// get the dimm slot index
		rank = (mear & kU3MEAR_RNK_A_mask) >> kU3MEAR_RNK_A_shift;

		// Check for an uncorrectable error
		if (apiexcp & (kU3API_ECC_UE_H | kU3API_ECC_UE_L))
		{
			dimmloc = rank - (rank%2) + (apiexcp & kU3API_ECC_UE_H ? 0 : 1);

			panic("Uncorrectable parity error detected in %s (MEAR=0x%08lX MESR=0x%08lX)\n",
					me->dimmErrors[dimmloc].slotName, mear, mesr);
		}

		// must be a correctable error.  increment the count for this DIMM
		else // if (apiexcp & (kU3API_ECC_CE_H | kU3API_ECC_CE_L))
		{
			dimmloc = rank - (rank%2) + (apiexcp & kU3API_ECC_CE_H ? 0 : 1);

			//kprintf("AppleU3 got correctable error dimm %u\n", dimmloc);

			IOSimpleLockLock( me->dimmLock );
			me->dimmErrors[dimmloc].count++;
			IOSimpleLockUnlock( me->dimmLock );

			// schedule a notification thread callout (if not already scheduled)
			if (thread_call_is_delayed( me->eccErrorCallout, NULL ) == FALSE)
			{
				// kprintf("AppleU3 scheduling notifier thread\n");

				AbsoluteTime deadline;
				clock_interval_to_deadline( kU3ECCNotificationIntervalMS, kMillisecondScale, &deadline );
				thread_call_enter1_delayed( me->eccErrorCallout, vRefCon, deadline);
			}
		}
	}
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

	// kprintf("AppleU3::eccNotifier\n");

	// allocate memory for a copy of the dimm error counts
	dimmErrorCounts = (UInt32 *) IOMalloc( dimmCount * sizeof(UInt32) );

	if (!dimmErrorCounts)
	{
		IOLog("ECC Notifier -- failed to allocate memory!\n");
		return;
	}

	// get a copy of the dimm error counts, and zero out the live array
	IOSimpleLockLock( dimmLock );

	for (slot=0; slot<dimmCount; slot++)
	{
		dimmErrorCounts[slot] = dimmErrors[slot].count;
		dimmErrors[slot].count = 0;
	}

	IOSimpleLockUnlock( dimmLock );

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

	// If it's not U3 Heavy, we don't have ECC
	if (!IS_U3_HEAVY(uniNVersion)) return;

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

		slotNames += strlen(slotNames) + 1;	// advance to the next dimm name
	}

	IOLog( "Enabling ECC Error Notifications\n" );

	// flag that this is an ecc supported memory controller
	setProperty("ecc-supported", "true" );

	// Clear the mask bits in the MCCR to enable error propogation
	bits = kU3MCCR_ECC_UE_MASK_H | kU3MCCR_ECC_CE_MASK_H | kU3MCCR_ECC_UE_MASK_L | kU3MCCR_ECC_CE_MASK_L;
	safeWriteRegUInt32( kU3MemCheckCtrlRegister, bits, ~bits );

	// Set the mask bits in the CFMR to enable chip fault generation
	bits = kU3API_ECC_UE_H | kU3API_ECC_CE_H | kU3API_ECC_UE_L | kU3API_ECC_CE_L;
	safeWriteRegUInt32( kU3ChipFaultMaskRegister, bits, bits );
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
	safeWriteRegUInt32( kU3ChipFaultMaskRegister, kU3API_DARTExcp, kU3API_DARTExcp );
}
