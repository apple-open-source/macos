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
//		$Log: U3.cpp,v $
//		Revision 1.11  2003/07/24 21:15:56  raddog
//		[3336924]Do not reset MPIC across sleep - fixes DVD playback after sleep
//		
//		Revision 1.10  2003/07/03 01:16:32  raddog
//		[3313953]U3 PwrMgmt register workaround
//		
//		Revision 1.9  2003/06/27 00:45:07  raddog
//		[3304596]: remove unnecessary access to U3 Pwr registers on wake, [3249029]: Disable unused second process on wake, [3301232]: remove unnecessary PCI code from PE
//		
//		Revision 1.8  2003/06/03 23:03:57  raddog
//		disable second cpu when unused - 3249029, 3273619
//		
//		Revision 1.7  2003/06/03 01:50:24  raddog
//		U3 sleep changes including calling SPU
//		
//		Revision 1.6  2003/05/07 00:14:55  raddog
//		[3125575] MacRISC4 initial sleep support
//		
//		Revision 1.5  2003/04/04 01:27:27  raddog
//		[3217587]: Q37: AppleU3 needs to enable U3 MPIC
//		
//		Revision 1.4  2003/03/04 17:53:20  raddog
//		[3187811] P76: U3.2.0 systems don't boot
//		[3187813] MacRISC4CPU bridge saving code can block on interrupt stack
//		[3138343] Q37 Feature: remove platform functions for U3
//		
//		Revision 1.3  2003/02/27 01:42:54  raddog
//		Better support for MP across sleep/wake [3146943]. This time we block in startCPU, rather than initCPU, which is safer.
//		
//		Revision 1.2  2003/02/18 00:02:01  eem
//		3146943: timebase enable for MP, bump version to 1.0.1d3.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

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
	IOInterruptState 		intState;
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
	// [3313953] reads to U3 Pwr Mgmt registers can sometimes return bogus results
	// The fix is to keep reading until a consistent result is obtained.
	if ((offset >= kU3PMClockControl) && (offset <= kU3PMSMax)) {
		UInt32 result1, result2;
		
		result2 = uniNBaseAddress[offset >> 2];			// Read it
		do {
			result1 = result2;							// Save latest result
			result2 = uniNBaseAddress[offset >> 2];		// Read it again
		} while (result1 != result2);					// Until result is consistent
		return result2;
	}
	
	// Normal case - no workaround necessary
    return uniNBaseAddress[offset >> 2];
}

// **********************************************************************************
// writeUniNReg
//
// **********************************************************************************
void AppleU3::writeUniNReg(UInt32 offset, UInt32 data)
{
    uniNBaseAddress[offset >> 2] = data;
	
	// [3313953] reads to U3 Pwr Mgmt registers can trigger a clock synchronization
	// boundary crossing problem.  To prevent this a read of the version register
	// is sufficient.
	if ((offset >= kU3PMClockControl) && (offset <= kU3PMSMax))
	    (void) readUniNReg(kUniNVersion);

    OSSynchronizeIO();
	
	return;
}

// **********************************************************************************
// safeReadRegUInt32
//
// **********************************************************************************
UInt32 AppleU3::safeReadRegUInt32(UInt32 offset)
{
	IOInterruptState intState;

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
	IOInterruptState	intState;
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
		spu->callPlatformFunction (symSetSPUSleep, false, (void *)false, (void *)0, (void *)0, (void *)0);
			
		// Set the sleeping state for HWInit.  This tells OF we were sleeping
		safeWriteRegUInt32(kUniNHWInitState, ~0UL, kUniNHWInitStateSleeping);
	
		// Clear MPIC interrupt output enable bit in U3 toggle register, but only if MPIC is present
		if (mpicRegEntry)
			safeWriteRegUInt32(kU3ToggleRegister, kU3MPICEnableOutputs, 0);
		
		// Set HyperTransport back to default state
		if (k2 && golem) {
			UInt32 data;

			setHTLinkFrequency (0);		// Set U3 end link frequency to 200 MHz

			// golem hack!
			data = golem->configRead32 (0xcc);
			data = (data & 0xFFFFF0FF);
			golem->configWrite32 (0xcc, data);
			data = golem->configRead32 (0xd0);
			data = (data & 0xFFFFF0FF);
			golem->configWrite32 (0xd0, data);
			
			// Set K2 end link frequency to 200MHz
			k2->callPlatformFunction (symSetHTLinkFrequency, false, (void *)0, (void *)0, (void *)0, (void *)0);
			
			setHTLinkWidth (0, 0);		// Set U3 end link width 8-bit
			// Set K2 end link width 8-bit - not required as K2 only runs 8 bit
			//k2->callPlatformFunction (symSetHTLinkWidth, false, (void *)0, (void *)0, (void *)0, (void *)0);

			// golem hack!
			data = golem->configRead32 (0xc4);
			data = (data & 0x88FFFFFF);
			golem->configWrite32 (0xc4, data);

		}
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
	UInt32 						offset, value, valueLen, mask, maskLen, data, writeLen, 
									cmd, cmdLen, result, pHandle, lastCmd,
									param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;

	IOPCIDevice					*nub;
	
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
	
	if (!k2)
		k2 = waitForService(serviceMatching("KeyLargo"));
	
	if (!spu) {
		service = waitForService(resourceMatching("AppleSPU"));
		if (service) 
			spu = OSDynamicCast (IOService, service->getProperty("AppleSPU"));
	}
	
	if (!pmu) 
		pmu = waitForService(serviceMatching("ApplePMU"));	
	
	if (!golem)
		golem = OSDynamicCast (IOPCIDevice, provider->fromPath("/ht@0,F2000000/pci@1", gIODTPlane));

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
bool AppleU3::getHTLinkFrequency (UInt32 *freqResult)
{
	UInt32			freq;

	freq = safeReadRegUInt32 (0x70100);
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
	/*
	 * In a two processor system where the second processor is unused, OF has left
	 * that processor in a spin loop executing out of ROM.  This steals bus cycles so
	 * we workaround it by taking the processor off the bus [3249029].  This also fixes
	 * a hang when boot-args cpus=1 is set [3273619]
	 */
	safeWriteRegUInt32 (kU3APIPhyConfigRegister1, ~0UL, 0);
	return;
}
