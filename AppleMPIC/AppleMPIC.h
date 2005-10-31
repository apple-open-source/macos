/*
 * Copyright (c) 1998-2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#ifndef _IOKIT_APPLEMPIC_H
#define _IOKIT_APPLEMPIC_H

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

#include <IOKit/pci/IOPCIDevice.h>

// Offsets and strides to MPIC registers.

#define kFeatureOffset         (0x01000)
#define kGlobal0Offset         (0x01020)
#define kVendorIDOffset        (0x01080)
#define kProcInitOffset        (0x01090)
#define kIPInVecPriOffset      (0x010A0)
#define kIPInVecPriStride      (0x00010)
#define kSpurVectOffset        (0x010E0)
#define kTmrFreqOffset         (0x010F0)
#define kTnCurrCntOffset       (0x01100)
#define kTnBaseCntOffset       (0x01110)
#define kTnVecPriOffset        (0x01120)
#define kTnDestOffset          (0x01130)
#define kTnStride              (0x00040)
#define kIntnVecPriOffset      (0x10000)
#define kIntnDestOffset        (0x10010)
#define kIntnStride            (0x00020)
#define kPnIPImDispOffset      (0x20040)
#define kPnIPImDispStride      (0x00010)
#define kPnCurrTskPriOffset    (0x20080)
#define kPnIntAckOffset        (0x200A0)
#define kPnEOIOffset           (0x200B0)
#define kPnStride              (0x01000)

// Feature Reporting Register
#define kFRRVersionMask        (0x000000FF)
#define kFRRVersionShift       (0)
#define kFRRNumCPUMask         (0x00001F00)
#define kFRRNumCPUShift        (8)
#define kFRRNumIRQsMask        (0x07FF0000)
#define kFRRNumIRQsShift       (16)

// Global Configuration Register 0
#define kGCR0Reset             (0x80000000)
#define kGCR0Cascade           (0x20000000)

// Vendor ID Register
#define kVIDRVendorIDMask      (0x000000FF)
#define kVIDRVendorIDShift     (0)
#define kVIDRDeviceIDMask      (0x0000FF00)
#define kVIDRDeviceIDShift     (8)

// Spurious Vector
#define kSpuriousVectorNumber  (0xFF)

// Interrupt Source n Vector/Priority Registers
#define kIntnVPRMask           (0x80000000)
#define kIntnVPRActive         (0x40000000)
#define kIntnVPRSense          (0x00400000)
#define kIntnVPRPriorityMask   (0x000F0000)
#define kIntnVPRPriorityShift  (16)
#define kIntnVPRVectorMask     (0x000000FF)
#define kIntnVPRVectorShift    (0)

// Interrupt type bits
#define kIntrTypeMask			(0x1)		// edge/level
#define kIntrHTMask				(0x2)		// 1 means interrupt comes over HyperTransport

// Hypertransport interrupt defines
enum {
	kHTIntCapID			= 0x08,			// HyperTransport LDT capability block ID
	kHTIntCapType		= 0x80,			// HyperTransport LDT capability block type

	kHTIntRequestEOI	= 0x20,			// Request EOI (aka Interrupt Info bit 5)
	kHTIntPolarity		= 0x02,			// Interrupt polarity
	kHTIntMask			= 0x01,			// Interrupt mask
	kHTIntDefRegMask	= (kHTIntRequestEOI | kHTIntPolarity | kHTIntMask),
	kHTWaitEOIBase		= 0x60,			// WaitEOI base register offset
	kHTIntIndexBase		= 0x10,			// Interrupt definition registers start at index 0x10
	kHTMaxInterrupt		= 85
};

enum {
	kMPICIPICount				= 4,
	kMPICTaskPriorityCount		= 4,
	kMPICTimerCount				= 4
};

struct MPICTimers {
	UInt32			currentCountRegister;
	UInt32			baseCountRegister;
	UInt32			vectorPriorityRegister;
	UInt32			destinationRegister;
};
typedef struct MPICTimers MPICTimers;
typedef volatile MPICTimers *MPICTimersPtr;

struct MPICState {
	UInt32			mpicGlobal0;
	UInt32 			mpicIPI[kMPICIPICount];
	UInt32 			mpicSpuriousVector;
	UInt32 			mpicTimerFrequencyReporting;
	MPICTimers 		mpicTimers[kMPICTimerCount];
	UInt32 			*mpicInterruptSourceVectorPriority;
	UInt32 			*mpicInterruptSourceDestination;
	UInt32			mpicCurrentTaskPriorities[kMPICTaskPriorityCount];
};
typedef struct MPICState MPICState;
typedef volatile MPICState *MPICStatePtr;

#if defined( __ppc__ )

#	define LWBRX(a)	(accessBigEndian ? *(UInt32 *)(a)       : lwbrx(a))
	//#define STWBRX(v,a) (accessBigEndian ? (*(UInt32 *)(a) = (v)) : (stwbrx((v),(a))))
	//The compiler doesn't like the ? : form of the macro for STWBRX.  Don't ask me why.
#	define STWBRX(v,a) if (accessBigEndian)  (*(UInt32 *)(a) = (v)); else stwbrx((v),(a))

#		define EIEIO()		eieio()
#else
	// make them NO-OPs
#	define LWBRX(a) (0)
#	define STWBRX(v,a)
#
	// sync & isync are ppc only so make them NOOPs as well
#	define sync()
#	define isync()
#	define EIEIO()
#endif	// defined( __ppc__ )

class AppleMPICInterruptController : public IOInterruptController
{
	OSDeclareDefaultStructors(AppleMPICInterruptController);
  
private:
	IOLogicalAddress		mpicBaseAddress;
	IOMemoryMap				*mpicMemoryMap;
	int						numCPUs;
	int						numVectors;
	OSSymbol				*interruptControllerName;
	IOService				*parentNub;
	UInt32					*senses;
	MPICStatePtr			mpicSavedStatePtr;
	bool					isHostMPIC, accessBigEndian, resetOnWake;
  
	// callPlatformFunction symbols
	const OSSymbol 	*mpic_dispatchIPI;
	const OSSymbol 	*mpic_getProvider;
	const OSSymbol 	*mpic_getIPIVector;
	const OSSymbol 	*mpic_setCurrentTaskPriority;
	const OSSymbol 	*mpic_setUpForSleep;

	// sleep variables:
	UInt32 *originalIpivecPriOffsets;
	UInt32 *originalCurrentTaskPris;
  
	// HyperTransport interrupt support variables	
	IOPCIDevice			*fHTInterruptProvider;
	UInt32				fHTIntCapabilities;			// Interrupt capabilities ID register offset
	UInt32				fHTIntDataPort;					// Offset to data port phantom register to read/write interrupt definition registers
  
	// HyperTransport interrupt support routines
	IOPCIDevice		*configureHTInterruptProvider ( UInt32 *htIntCapOffset, UInt32 *htIntDataPort );
	IOReturn		initHTVector (UInt32 vectorType, bool waking);
	void			htWaitEOI (UInt32 source);
	IOLock			*htIntLock;


public:
	virtual bool start(IOService *provider);
	virtual bool matchPropertyTable(OSDictionary * table);

	virtual IOReturn getInterruptType(IOService *nub, int source,
		int *interruptType);
  
	virtual IOInterruptAction getInterruptHandlerAddress(void);
	virtual IOReturn handleInterrupt(void *refCon,
		IOService *nub, int source);
  
	virtual bool vectorCanBeShared(long vectorNumber, IOInterruptVector *vector);
	virtual void initVector(long vectorNumber, IOInterruptVector *vector);
	virtual void disableVectorHard(long vectorNumber, IOInterruptVector *vector);
	virtual void enableVector(long vectorNumber, IOInterruptVector *vector);

	virtual OSData *getIPIVector(long physCPU);
	virtual void   dispatchIPI(long source, long targetMask);
	virtual void   setCurrentTaskPriority(long priority);
	virtual void   setUpForSleep(bool goingToSleep, int cpuNum);

	virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
		bool waitForFunction, void *param1, void *param2, void *param3, void *param4);

};


#endif /* ! _IOKIT_APPLEMPIC_H */
