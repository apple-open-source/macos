/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
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
 * Copyright (c) 1999-2007 Apple Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */


#include <ppc/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOPlatformExpert.h>

#include <sys/kdebug.h>

#include "AppleMPIC.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
// #define APPLEMPIC_DEBUG 1

#ifdef APPLEMPIC_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef  super
#define super IOInterruptController

OSDefineMetaClassAndStructors(AppleMPICInterruptController, IOInterruptController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/************************************************************************************

	s t a r t

************************************************************************************/

bool AppleMPICInterruptController::start(IOService *provider)
{
	long				cnt, regTemp;
	OSObject			*tmpObject;
	IOInterruptAction	handler;

	// callPlatformFunction symbols
	mpic_getProvider = OSSymbol::withCString("mpic_getProvider");
	mpic_getIPIVector= OSSymbol::withCString("mpic_getIPIVector");
	mpic_setCurrentTaskPriority = OSSymbol::withCString("mpic_setCurrentTaskPriority");
	mpic_setUpForSleep = OSSymbol::withCString("mpic_setUpForSleep");
	mpic_dispatchIPI = OSSymbol::withCString("mpic_dispatchIPI");
  
	if (!super::start(provider))
		return false;
  
	// Get the interrupt controller name from the provider's properties.
	tmpObject = provider->getProperty("InterruptControllerName");
	interruptControllerName = OSDynamicCast(OSSymbol, tmpObject);
	if (interruptControllerName == 0) return false;
  
	// Set the interrupt controller name so it can be matched by others.
	setProperty("InterruptControllerName", interruptControllerName);
  
	// Host MPIC doesn't have "interrupts" property
	isHostMPIC = (provider->getProperty("interrupts") == NULL);
  
	// Check if MPIC requires big-endian access
	accessBigEndian = (provider->getProperty("big-endian") != NULL);
	
	// Check if we get reset across sleep wake
	resetOnWake = (provider->getProperty("reset-on-wake") != NULL);

	htIntLock = IOLockAlloc();
	if (htIntLock == NULL)
	{
		IOLog("AppleMPIC returning false with no htIntLock\n");
		return false;
	}

	// Map the MPIC's memory.
	mpicMemoryMap = provider->mapDeviceMemoryWithIndex(0);

	if (mpicMemoryMap == 0) return false;

	// get the base address of the MPIC.
	mpicBaseAddress = mpicMemoryMap->getVirtualAddress();

	// Read the Feature Reporting Register
	regTemp = LWBRX(mpicBaseAddress + kFeatureOffset);
	numCPUs =    ((regTemp & kFRRNumCPUMask) >> kFRRNumCPUShift) + 1;
	numVectors = ((regTemp & kFRRNumIRQsMask) >> kFRRNumIRQsShift) + 1;
  
	// Allocate the memory for the senses.
	senses = (UInt32 *)IOMalloc ((numVectors + numCPUs) * sizeof (long));
	if (senses == NULL) return false;
	for (cnt = 0; cnt < (numVectors + numCPUs); cnt++)
		senses[cnt] = 0;

	// Allocate the memory for the vectors.
	vectors = (IOInterruptVector *)IOMalloc((numVectors + numCPUs) *
					  sizeof(IOInterruptVector));
	if (vectors == NULL) return false;
	bzero(vectors, (numVectors + numCPUs) * sizeof(IOInterruptVector));

	// Allocate locks for the vectors.
	for (cnt = 0; cnt < (numVectors + numCPUs) ; cnt++)
	{
		vectors[cnt].interruptLock = IOLockAlloc();
		if (vectors[cnt].interruptLock == NULL)
		{
			for (cnt = 0; cnt < (numVectors + numCPUs); cnt++)
			{
			if (vectors[cnt].interruptLock != NULL)
				IOLockFree(vectors[cnt].interruptLock);
			}
			return false;
		}
	}
  
	// Reset the MPIC.
	STWBRX(kGCR0Reset, mpicBaseAddress + kGlobal0Offset);
	EIEIO();

	// Wait for the reset to complete.
	do
	{
		regTemp = LWBRX(mpicBaseAddress + kGlobal0Offset);
		EIEIO();
	} while (regTemp & kGCR0Reset);
  
	// Clear and mask all the interrupt vectors.
	for (cnt = 0; cnt < (numVectors + numCPUs); cnt++)
	{
		STWBRX(kIntnVPRMask, mpicBaseAddress+kIntnVecPriOffset+kIntnStride*cnt);
	}
	EIEIO();
  
	// Set the Spurious Vector Register.
	STWBRX(kSpuriousVectorNumber, mpicBaseAddress + kSpurVectOffset);

	// Set 8259 Cascade Mode.
	STWBRX(kGCR0Cascade, mpicBaseAddress + kGlobal0Offset);
	EIEIO();

	// [5376988] - Leave everything disabled at this point
#if 0
	// Set the all CPUs Current Task Priority to zero.
	for(cnt = 0; cnt < numCPUs; cnt++)
	{
		STWBRX(0, mpicBaseAddress + kPnCurrTskPriOffset + (kPnStride * cnt));
		EIEIO();
	}
#endif

	// allocates the arrays for the sleep varaibles:
	originalIpivecPriOffsets = (UInt32*)IOMalloc(sizeof(UInt32) * numCPUs);
	if (originalIpivecPriOffsets == NULL)
	{
		// we could also fre the memory allocated above. However without the
		// interrupt controller this system is not going to work so we will
		// stop soon anyway.
		return false;
	}

	originalCurrentTaskPris = (UInt32*)IOMalloc(sizeof(UInt32) * numCPUs);
	if (originalCurrentTaskPris == NULL)
	{
		// we could also fre the memory allocated above. However without the
		// interrupt controller this sytem is not going to work so we will
		// stop soon anyway.
		return false;
	}
	
	if (resetOnWake)
	{
		mpicSavedStatePtr = (MPICStatePtr)IOMalloc (sizeof (MPICState));
		if (mpicSavedStatePtr)
		{
			mpicSavedStatePtr->mpicInterruptSourceVectorPriority = (UInt32 *)IOMalloc (sizeof(UInt32) * numVectors);
			mpicSavedStatePtr->mpicInterruptSourceDestination	 = (UInt32 *)IOMalloc (sizeof(UInt32) * numVectors);
		}
		
	}

	registerService();  
  
	handler = getInterruptHandlerAddress();
	if (isHostMPIC)
	{
		 // host MPIC - set up CPU interrupts
		 // only set CPU interrupt properties for the host MPIC (which has no interrupts property)
		 getPlatform()->setCPUInterruptProperties(provider);

		// register the interrupt handler so it can receive interrupts.
		for (cnt = 0; cnt < numCPUs; cnt++)
		{
			provider->registerInterrupt(cnt, this, handler, 0);
			provider->enableInterrupt(cnt);
		}
	}
	else
	{
		// not host MPIC - just enable its interrupt
		provider->registerInterrupt(0, this, handler, 0);
		provider->enableInterrupt(0);
	}
  
	// Register this interrupt controller so clients can find it.
	getPlatform()->registerInterruptController(interruptControllerName, this);
  
	return true;
}



/************************************************************************************

	c a l l P l a t f o r m F u n c t i o n

************************************************************************************/

IOReturn AppleMPICInterruptController::callPlatformFunction(const OSSymbol *functionName,
                                                            bool waitForFunction,
                                                            void *param1, void *param2,
                                                            void *param3, void *param4)
{  
	if (functionName == mpic_getProvider)
	{
		IORegistryEntry **tmpIORegistryEntry = (IORegistryEntry**)param1;
		*tmpIORegistryEntry = getProvider();
		return kIOReturnSuccess;
	}

	if (functionName == mpic_getIPIVector)
	{
		OSData **tmpOSData = (OSData**)param2;
		*tmpOSData = getIPIVector(*(long *)param1);
		return kIOReturnSuccess;
	}
    
	if (functionName == mpic_setCurrentTaskPriority)
	{
		setCurrentTaskPriority(*(long *)param1);
		return kIOReturnSuccess;
	}
    
	if (functionName == mpic_setUpForSleep)
	{
		setUpForSleep((bool)param1, (int)param2);
		return kIOReturnSuccess;
	}
  
	if (functionName == mpic_dispatchIPI)
	{
		dispatchIPI(*(long *)param1, (long)param2);
		return kIOReturnSuccess;
	}

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}




/************************************************************************************

	g e t I n t e r r u p t T y p e

************************************************************************************/

IOReturn AppleMPICInterruptController::getInterruptType(IOService *nub,
							int source,
							int *interruptType)
{
	IOInterruptSource	*interruptSources;
	OSData				*vectorData;
	long				vectorNumber;

	if (interruptType == 0)
		return kIOReturnBadArgument;

	interruptSources = nub->_interruptSources;
	vectorData       = interruptSources[source].vectorData;
	vectorNumber      = *(long *)vectorData->getBytesNoCopy();
	*interruptType    = (((long *)vectorData->getBytesNoCopy())[1]) & kIntrTypeMask;

	return kIOReturnSuccess;
}




/************************************************************************************

	g e t I n t e r r u p t H a n d l e r A d d r e s s

************************************************************************************/

IOInterruptAction AppleMPICInterruptController::getInterruptHandlerAddress(void)
{
#ifdef OSMemberFunctionCast
	return OSMemberFunctionCast(IOInterruptAction, this, &AppleMPICInterruptController::handleInterrupt);
#else
	return (IOInterruptAction)&AppleMPICInterruptController::handleInterrupt;
#endif
}




/************************************************************************************

	h a n d l e I n t e r r u p t

************************************************************************************/

IOReturn AppleMPICInterruptController::handleInterrupt(	void *		/*refCon*/,
														IOService *	/*nub*/,
														int			source)
{
	long				vectorNumber, level, sense;
	IOInterruptVector	*vector;
    
	do
	{
		vectorNumber = LWBRX(mpicBaseAddress + source*kPnStride + kPnIntAckOffset);
		EIEIO();
    
		if (vectorNumber == kSpuriousVectorNumber)
			break;

		sense = senses[vectorNumber];    
		level = sense & kIntrTypeMask;
	    
		if ( ! level )
		{
			STWBRX(0, mpicBaseAddress + source * kPnStride + kPnEOIOffset);
			EIEIO();
		}
    
		vector = &vectors[vectorNumber];

		vector->interruptActive = 1;
		sync();
		isync();

		if ( ! vector->interruptDisabledSoft )
		{
			isync();

			// this seems hokey/dangerous, since vector->handler and vector->target may not be 32-bit values ...
			KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_IHDLR, 0), vectorNumber, (unsigned int)vector->handler, (unsigned int)vector->target, 0, 0);
        
			// Call the handler if it exists.
			if (vector->interruptRegistered)
			{
				vector->handler(vector->target, vector->refCon, vector->nub, vector->source);
	
				if (level && vector->interruptDisabledSoft)
				{
					// Hard disable the source.
					vector->interruptDisabledHard = 1;
					disableVectorHard(vectorNumber, vector);
				}
				else if ((sense & (kIntrHTMask | kIOInterruptTypeLevel)) == (kIntrHTMask | kIOInterruptTypeLevel))
				{
					/*
					 * If this is a HyperTransport interrupt and we didn't disable it above, go ahead
					 * and do a HyperTransport WaitEOI to allow interrupts to occur again.  If it was
					 * disabled the WaitEOI happens the next time enableVector is called
					 */
					htWaitEOI( sense >> 16 );
				}
			}
		}
		else
		{
		  // Hard disable the source.
		  vector->interruptDisabledHard = 1;
		  disableVectorHard(vectorNumber, vector);
		}
    
		if ( level )
		{
			STWBRX(0, mpicBaseAddress + source * kPnStride + kPnEOIOffset);
			EIEIO();
		}
    
		vector->interruptActive = 0;
    
	} while (1);
  
	return kIOReturnSuccess;
}




/************************************************************************************

	v e c t o r C a n B e S h a r e d

************************************************************************************/

bool AppleMPICInterruptController::vectorCanBeShared(long /*vectorNumber*/, IOInterruptVector */*vector*/)
{
	return true;
}




/************************************************************************************

	i n i t V e c t o r

************************************************************************************/

void AppleMPICInterruptController::initVector(long vectorNumber, IOInterruptVector *vector)
{
	IOInterruptSource *interruptSources;
	long              vectorType;
	OSData            *vectorData;
	long              regTemp, vectorBase;

	// Get the vector's type.
	interruptSources = vector->nub->_interruptSources;
	vectorData = interruptSources[vector->source].vectorData;

	// vectorType can now include information indicating it arrives over HyperTransport
	vectorType = ((long *)vectorData->getBytesNoCopy())[1];

	senses[vectorNumber] = vectorType;
	if (vectorType & kIntrHTMask)
	{
		if ( ! fHTInterruptProvider )
			fHTInterruptProvider = configureHTInterruptProvider (&fHTIntCapabilities, &fHTIntDataPort);
	}

	// Set the Vector/Priority and Destination Registers
	if (vectorNumber < numVectors)
	{
		vectorBase = mpicBaseAddress + kIntnStride * vectorNumber;

		regTemp = 0x1; // CPU 0 only.
		STWBRX(regTemp, vectorBase + kIntnDestOffset);
		EIEIO();

		// Vectors start masked with priority 8.
		regTemp  = kIntnVPRMask | (8 << kIntnVPRPriorityShift);
		/*
		 * HyperTransport interrupts into MPIC are always edge 
		 * so we set edge here at MPIC (bit == 0).  Meanwhile,
		 * initHTVector initializes the HyperTransport end to be edge/level as necessary
		 * and it also enables the interrupt.  Interrupts remain blocked at MPIC until
		 * the appropriate enable is done at MPIC.
		 */
		if (vectorType & kIntrHTMask)
			initHTVector (vectorType, false);		// Initialize and enable the interrupt at the HyperTransport end
		else
			// Normal MPIC interrupt - set edge/level accordingly
			regTemp |= ((vectorType & kIntrTypeMask) == kIOInterruptTypeLevel) ? kIntnVPRSense : 0;
			
		regTemp |= vectorNumber << kIntnVPRVectorShift;
		STWBRX(regTemp, vectorBase + kIntnVecPriOffset);
		EIEIO();
	}
	else
	{
		vectorBase = mpicBaseAddress + kIPInVecPriStride*(vectorNumber-numVectors);
    
		// IPI Vectors start masked with priority 14.
		regTemp  = kIntnVPRMask | (14 << kIntnVPRPriorityShift);
		regTemp |= vectorNumber << kIntnVPRVectorShift;
		STWBRX(regTemp, vectorBase + kIPInVecPriOffset);
		EIEIO();
	}
	
	return;
}




/************************************************************************************

	d i s a b l e V e c t o r H a r d

************************************************************************************/

void AppleMPICInterruptController::disableVectorHard(long vectorNumber, IOInterruptVector */*vector*/)
{
	long     regTemp, vectorBase;
  
	if (vectorNumber < numVectors)
	{
		vectorBase = mpicBaseAddress + kIntnVecPriOffset + kIntnStride * vectorNumber;
	}
	else
	{
		vectorBase = mpicBaseAddress + kIPInVecPriOffset + kIPInVecPriStride * (vectorNumber - numVectors);
	}

	regTemp = LWBRX(vectorBase);
	if ( ! (regTemp & kIntnVPRMask) )
	{
		regTemp |= kIntnVPRMask;
		STWBRX(regTemp, vectorBase);
	}

	return;
}




/************************************************************************************

	e n a b l e V e c t o r

************************************************************************************/

void AppleMPICInterruptController::enableVector(long vectorNumber, IOInterruptVector */*vector*/)
{
	long     regTemp, vectorBase, sense;

	sense = senses[vectorNumber];

	if (vectorNumber < numVectors)
	{
		vectorBase = mpicBaseAddress + kIntnVecPriOffset + kIntnStride * vectorNumber;
	}
	else
	{
		vectorBase = mpicBaseAddress + kIPInVecPriOffset + kIPInVecPriStride * (vectorNumber - numVectors);
	}
  
	regTemp = LWBRX(vectorBase);
	if (regTemp & kIntnVPRMask)
	{
		regTemp &= ~kIntnVPRMask;
		STWBRX(regTemp, vectorBase);
	}
	
	// Do a HyperTransport WaitEOI to allow interrupts to occur again
	if ((sense & (kIntrHTMask | kIOInterruptTypeLevel)) == (kIntrHTMask | kIOInterruptTypeLevel))
	{
		htWaitEOI(sense >> 16);
	}
	
	return;
}




/************************************************************************************

	g e t I P I V e c t o r

************************************************************************************/

OSData *AppleMPICInterruptController::getIPIVector(long physCPU)
{
	long   tmpLongs[2];
	OSData *tmpData;

	if ((physCPU < 0) && (physCPU >= numCPUs))
		return 0;

	tmpLongs[0] = numVectors + physCPU;
	tmpLongs[1] = kIOInterruptTypeEdge;

	tmpData = OSData::withBytes(tmpLongs, 2 * sizeof(long));

	return tmpData;
}




/************************************************************************************

	d i s p a t c h I P I

************************************************************************************/

void AppleMPICInterruptController::dispatchIPI(long source, long targetMask)
{
	long ipiBase, cnt;

	ipiBase = mpicBaseAddress + kPnIPImDispOffset + kPnStride * source;

	for (cnt = 0; cnt < numCPUs; cnt++)
	{
		if (targetMask & (1 << cnt))
		{
			STWBRX((1 << cnt), ipiBase + kPnIPImDispStride * cnt);
			EIEIO();
		}
	}
	
	return;
}




/************************************************************************************

	s e t C u r r e n t T a s k P r i o r i t y

************************************************************************************/

void AppleMPICInterruptController::setCurrentTaskPriority(long priority)
{
	long cnt;
  
	// Set the all CPUs Current Task Priority.
	for(cnt = 0; cnt < numCPUs; cnt++)
	{
		STWBRX(priority, mpicBaseAddress + kPnCurrTskPriOffset + (kPnStride*cnt));
		EIEIO();
	}
	
	return;
}




/************************************************************************************

	s e t U p F o r S l e e p

************************************************************************************/

void AppleMPICInterruptController::setUpForSleep(bool goingToSleep, int cpu)
{
	int i;
	volatile UInt32 *ipivecPriOffset = (UInt32*)(mpicBaseAddress + kIPInVecPriOffset + (cpu << 4));
	volatile UInt32 *currentTaskPri = (UInt32*)(mpicBaseAddress + kPnCurrTskPriOffset + (kPnStride * cpu));

	DLOG("\nAppleMPICInterruptController::setUpForSleep(%s, %d)\n",(goingToSleep ? "true" : "false"), cpu);
     
	if ( goingToSleep )
	{
		DLOG("AppleMPICInterruptController::setUpForSleep ipivecPriOffset(0x%08lx) = 0x%08lx\n",(UInt32)ipivecPriOffset, 0x80000000);
		DLOG("AppleMPICInterruptController::setUpForSleep currentTaskPri(0x%08lx) = 0x%08lx\n",(UInt32)currentTaskPri, 0x0000000F);

		if ( resetOnWake && (cpu == 0) )
		{
			mpicSavedStatePtr->mpicGlobal0 = *(UInt32 *)(mpicBaseAddress + kGlobal0Offset);
			DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kGlobal0Offset, mpicSavedStatePtr->mpicGlobal0 );
			for (i = 0; i < kMPICIPICount; i++)
			{
				mpicSavedStatePtr->mpicIPI[i] = *(UInt32 *)(mpicBaseAddress + kIPInVecPriOffset + (i * kIPInVecPriStride));
				DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kIPInVecPriOffset + (i * kIPInVecPriStride), mpicSavedStatePtr->mpicIPI[i] );
			}
		
			mpicSavedStatePtr->mpicSpuriousVector = *(UInt32 *)(mpicBaseAddress + kSpurVectOffset);
			DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kSpurVectOffset, mpicSavedStatePtr->mpicSpuriousVector);
			mpicSavedStatePtr->mpicTimerFrequencyReporting = *(UInt32 *)(mpicBaseAddress + kTmrFreqOffset);
			DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kTmrFreqOffset, mpicSavedStatePtr->mpicTimerFrequencyReporting );
		
			for (i = 0; i < kMPICTimerCount; i++)
			{
				mpicSavedStatePtr->mpicTimers[i].currentCountRegister 	= *(UInt32 *)(mpicBaseAddress + kTnBaseCntOffset + (i * kTnStride));
				DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kTnBaseCntOffset + (i * kTnStride), mpicSavedStatePtr->mpicTimers[i].currentCountRegister );
				mpicSavedStatePtr->mpicTimers[i].baseCountRegister 		= *(UInt32 *)(mpicBaseAddress + kTnBaseCntOffset + (i * kTnStride) + 0x10);
				DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kTnBaseCntOffset + (i * kTnStride + 0x10), mpicSavedStatePtr->mpicTimers[i].baseCountRegister );
				mpicSavedStatePtr->mpicTimers[i].vectorPriorityRegister = *(UInt32 *)(mpicBaseAddress + kTnBaseCntOffset + (i * kTnStride) + 0x20);
				DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kTnBaseCntOffset + (i * kTnStride + 0x20), mpicSavedStatePtr->mpicTimers[i].vectorPriorityRegister );
				mpicSavedStatePtr->mpicTimers[i].destinationRegister 	= *(UInt32 *)(mpicBaseAddress + kTnBaseCntOffset + (i * kTnStride) + 0x30);
				DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kTnBaseCntOffset + (i * kTnStride + 0x30), mpicSavedStatePtr->mpicTimers[i].destinationRegister );
			}
			
			for (i = 0; i < numVectors; i++)
			{
				// Make sure that the "active" bit is cleared.
				mpicSavedStatePtr->mpicInterruptSourceVectorPriority[i] = *(UInt32 *)(mpicBaseAddress + kIntnVecPriOffset + (i * kIntnStride)) & 
					(accessBigEndian ? (~0x40000000) : (~0x00000040));
				DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kIntnVecPriOffset + (i * kIntnStride), mpicSavedStatePtr->mpicInterruptSourceVectorPriority[i] );
				mpicSavedStatePtr->mpicInterruptSourceDestination[i] 	= *(UInt32 *)(mpicBaseAddress + kIntnDestOffset + (i * kIntnStride));
				DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kIntnDestOffset + (i * kIntnStride), mpicSavedStatePtr->mpicInterruptSourceDestination[i] );
			}
		
			for (i = 0; i < kMPICTaskPriorityCount; i++)
			{
				mpicSavedStatePtr->mpicCurrentTaskPriorities[i] = *(UInt32 *)(mpicBaseAddress + kPnCurrTskPriOffset  + (kPnStride * cpu));
				DLOG( "MPIC: saved(0x%08x): 0x%08x\n", kPnCurrTskPriOffset  + (kPnStride * cpu), mpicSavedStatePtr->mpicCurrentTaskPriorities[i] );
			}
		}
		
		// We are going to sleep
		originalIpivecPriOffsets[cpu] = *ipivecPriOffset;
		EIEIO();
		originalCurrentTaskPris[cpu] = *currentTaskPri;
		EIEIO();

		if (numCPUs > 1)
		{
			STWBRX (0x80000000, (unsigned int)ipivecPriOffset);
			//*ipivecPriOffset = 0x00000080;
			EIEIO();
		}
        
		STWBRX (0xF, (unsigned int)currentTaskPri);
		//*currentTaskPri =  (0x0000000F << 24);
		EIEIO();

	}
	else
	{
		// We are waking up
		//DLOG( "AppleMPICInterruptController::setUpForSleep ipivecPriOffset(0x%08lx) = 0x%08lx\n",(UInt32)ipivecPriOffset, originalIpivecPriOffsets[cpu] );
		//DLOG( "AppleMPICInterruptController::setUpForSleep currentTaskPri(0x%08lx)  = 0x%08lx\n",(UInt32)currentTaskPri, originalCurrentTaskPris[cpu] );

		DLOG( "AppleMPICInterruptController::setUpForSleep on wake ipivecPriOffset(0x%08lx) = 0x%08lx - LWBRX(0x%lx)\n",(UInt32)ipivecPriOffset, *ipivecPriOffset, LWBRX ((unsigned int)ipivecPriOffset) );
		DLOG( "AppleMPICInterruptController::setUpForSleep on wake currentTaskPri(0x%08lx)  = 0x%08lx - LWBRX(0x%lx)\n",(UInt32)currentTaskPri, *currentTaskPri, LWBRX ((unsigned int)currentTaskPri) );

		if (resetOnWake && (cpu == 0))
		{
			*(UInt32 *)(mpicBaseAddress + kGlobal0Offset) = mpicSavedStatePtr->mpicGlobal0;
			DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kGlobal0Offset, mpicSavedStatePtr->mpicGlobal0);
			for (i = 0; i < kMPICIPICount; i++)
			{
				*(UInt32 *)(mpicBaseAddress + kIPInVecPriOffset + (i * kIPInVecPriStride)) = mpicSavedStatePtr->mpicIPI[i];
				DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kIPInVecPriOffset + (i * kIPInVecPriStride), mpicSavedStatePtr->mpicIPI[i] );
			}
			
			*(UInt32 *)(mpicBaseAddress + kSpurVectOffset) = mpicSavedStatePtr->mpicSpuriousVector;
			DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kSpurVectOffset, mpicSavedStatePtr->mpicSpuriousVector );
			*(UInt32 *)(mpicBaseAddress + kTmrFreqOffset) = mpicSavedStatePtr->mpicTimerFrequencyReporting;
			DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kTmrFreqOffset, mpicSavedStatePtr->mpicTimerFrequencyReporting );
		
			for (i = 0; i < kMPICTimerCount; i++)
			{
				*(UInt32 *)(mpicBaseAddress + kTnBaseCntOffset + (i * kTnStride)) 		 = mpicSavedStatePtr->mpicTimers[i].currentCountRegister;
				DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kTnBaseCntOffset + (i * kTnStride), mpicSavedStatePtr->mpicTimers[i].currentCountRegister );
				*(UInt32 *)(mpicBaseAddress + kTnBaseCntOffset + (i * kTnStride) + 0x10) = mpicSavedStatePtr->mpicTimers[i].baseCountRegister;
				DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kTnBaseCntOffset + (i * kTnStride + 0x10), mpicSavedStatePtr->mpicTimers[i].baseCountRegister );
				*(UInt32 *)(mpicBaseAddress + kTnBaseCntOffset + (i * kTnStride) + 0x20) = mpicSavedStatePtr->mpicTimers[i].vectorPriorityRegister;
				DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kTnBaseCntOffset + (i * kTnStride + 0x20), mpicSavedStatePtr->mpicTimers[i].vectorPriorityRegister );
				*(UInt32 *)(mpicBaseAddress + kTnBaseCntOffset + (i * kTnStride) + 0x30) = mpicSavedStatePtr->mpicTimers[i].destinationRegister;
				DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kTnBaseCntOffset + (i * kTnStride + 0x30), mpicSavedStatePtr->mpicTimers[i].destinationRegister );
			}
		
			for (i = 0; i < numVectors; i++)
			{
				*(UInt32 *)(mpicBaseAddress + kIntnVecPriOffset + (i * kIntnStride)) = mpicSavedStatePtr->mpicInterruptSourceVectorPriority[i];
				DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kIntnVecPriOffset + (i * kIntnStride), mpicSavedStatePtr->mpicInterruptSourceVectorPriority[i] );
				*(UInt32 *)(mpicBaseAddress + kIntnDestOffset + i * kIntnStride) 	 = mpicSavedStatePtr->mpicInterruptSourceDestination[i];
				DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kIntnDestOffset + (i * kIntnStride), mpicSavedStatePtr->mpicInterruptSourceDestination[i] );
			}
		
			for (i = 0; i < kMPICTaskPriorityCount; i++)
			{
				*(UInt32 *)(mpicBaseAddress + kPnCurrTskPriOffset  + (kPnStride * i)) = mpicSavedStatePtr->mpicCurrentTaskPriorities[i];
				DLOG( "MPIC: restored(0x%08x): 0x%08x\n", kPnCurrTskPriOffset  + (kPnStride * i), mpicSavedStatePtr->mpicCurrentTaskPriorities[i] );
			}
			
			// Re-init any HyperTransport vectors
			for (i = 0; i < (numVectors + numCPUs); i++)
			{
				if (senses[i] & kIntrHTMask) 
					initHTVector (senses[i], true);		// Re-initialize and enable the interrupt at the HyperTransport end
			}
		}

		if (numCPUs > 1)
		{
			*ipivecPriOffset = originalIpivecPriOffsets[cpu];
			EIEIO();
		}

		STWBRX (0, (unsigned int)currentTaskPri);
		//*currentTaskPri =  (0x00000000 << 24); // originalCurrentTaskPris[cpu];
		EIEIO();

		// Set 8259 Cascade Mode - this is needed on K2 machines.
		STWBRX(kGCR0Cascade, mpicBaseAddress + kGlobal0Offset);
		EIEIO();
	}
	
	return;
}




/************************************************************************************

	m a t c h P r o p e r t y T a b l e

************************************************************************************/

bool AppleMPICInterruptController::matchPropertyTable(OSDictionary * table)
{
	// We return success if the following expression is true -- individual
	// comparisions evaluate to truth if the named property is not present
	// in the supplied matching dictionary.

	return compareProperty(table, "InterruptControllerName");
}




/************************************************************************************

	c o n f i g u r e H T I n t e r r u p t P r o v i d e r

************************************************************************************/
// Hypertransport interrupt support
IOPCIDevice *AppleMPICInterruptController::configureHTInterruptProvider ( UInt32 *htIntCapOffset, UInt32 *htIntDataPort )
{
	IOByteCount		offset;
	UInt32			configData;
	IOService		*service;
	IOPCIDevice		*ht;
	
	ht = NULL;
	service = waitForService( resourceMatching("ht-interrupt-sequencer") );
	if (service) 
		if (ht = OSDynamicCast (IOPCIDevice, service->getProperty("ht-interrupt-sequencer")))
		{
			offset = 0;
			do
			{
				// Look for the HyperTransport interrupt capability block
				configData = ht->extendedFindPCICapability (kHTIntCapID, &offset);
				if (configData && offset)
				{
					if (((configData >> 24) & 0xFF) == kHTIntCapType)
					{
						// This is the one we're looking for
						*htIntCapOffset = offset;
						*htIntDataPort = offset + sizeof (UInt32);
						break;
					}
					// keep looking
				}
				else
					break;		// not found
			} while (1);
		}
	
	return ht;
}




/************************************************************************************

	i n i t H T V e c t o r

************************************************************************************/

IOReturn AppleMPICInterruptController::initHTVector (UInt32 vectorType, bool waking)
{
	UInt32				index, intDefLow;
	IOReturn			status;
	
	if ( ! fHTInterruptProvider )
	{
		DLOG( "AppleMPIC::initHTVector - no HyperTransport interrupt provider\n" );
		return kIOReturnError;
	}
	
	
	/*
	 * High 16 bits of the vector type are the HyperTransport interrupt source number (NOT
	 * the MPIC interrupt source number).  Bottom bit (bit 0) defines edge/level as for other
	 * interrupts 
	 */	
	index = vectorType >> 16;
	if (index <= kHTMaxInterrupt)
	{
		/*
		 * Interrupt definition registers are accessed indirectly through the Interrupt Register
		 * Dataport.  An index indicating the register to be accessed is written to bits 16:23 of
		 * the interrupt capabilities register.  Other bits in this register are read-only so we
		 * ignore them.
		 *
		 * Interrupt definition registers are 64 bits so two indexes (even index for bits 31:0 and
		 * odd index for bits 63:32) are used.  The first interrupt definition register starts at
		 * index 0x10.
		 */
		index = (index << 1) + kHTIntIndexBase;		// Index for low (even) interrupt definition register
		
		/*
		 * During boot many drivers on different threads are trying to init their interrupts so
		 * we need to use a lock to protect our data port accesses
		 *
		 * The situation is different during wake where we are called to re-init the vectors.  Here
		 * we are called on a thread that cannot block but re-initing vectors is sequential so we can
		 * skip the lock
		 */

		if ( ! waking )
			IOLockLock (htIntLock);
	
		// set dataport index
		fHTInterruptProvider->configWrite32 (fHTIntCapabilities, index << 16);
		
		// read current value
		intDefLow = fHTInterruptProvider->configRead32 (fHTIntDataPort) & ~kHTIntDefRegMask;
		
		//Set request EOI for level interrupts and polarity (level/low, edge/hi)
		intDefLow |= (vectorType & kIOInterruptTypeLevel) ? (kHTIntRequestEOI | kHTIntPolarity) : 0;
		
		DLOG( "AppleMPIC::initHTVector - source 0x%x, index 0x%x, writing 0x%x\n", vectorType >> 16, index, intDefLow );

		// write through the dataport
		fHTInterruptProvider->configWrite32 (fHTIntDataPort, intDefLow);
		
		// DLOG( "AppleMPIC::initHTVector - after write, read back 0x%x\n", fHTInterruptProvider->configRead32 (fHTIntDataPort) );

		if ( ! waking )
			IOLockUnlock (htIntLock);
		
		status = kIOReturnSuccess;
	}
	else
	{
		DLOG( "AppleMPIC::initHTVector - illegal interrupt source 0x%x\n", index );
		status = kIOReturnBadArgument;
	}
		
	return status;
}




/************************************************************************************

	h t W a i t E O I

************************************************************************************/
/*
 * Clear the HyperTransport WaitEOI for the interrupt
 *
 * Only required for level interrupts
 */
void AppleMPICInterruptController::htWaitEOI(UInt32 source)
{
	UInt32		offset;
		
	offset = ((source >> 3) & ~3) + kHTWaitEOIBase;	// Offset to correct WaitEOI register
//			DLOG( "k2_HTEOI - source 0x%x,  offset 0x%x, writing 0x%x, read 0x%x\n", (UInt32) param1, offset,
//				1 << (source & 0x1F), htProvider->configRead32 (offset));
	fHTInterruptProvider->configWrite32 (offset, 1 << (source & 0x1F));

	return;
}
