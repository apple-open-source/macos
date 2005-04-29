/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * SccTypes.h - This file contains the class definition for the
 *		     AppleSCCSerial driver, which supports IBM Asynchronous
 *		     Communications Adapters, and compatible hardware.
 *
 * Writers:
 * Elias Keshishoglou
 *
 * Copyright ©: 	1999 Apple Computer, Inc.  all rights reserved.
 */

#ifndef SccTypes_h
#define SccTypes_h

static inline void SynchronizeIO(void)
{
    eieio();
}

typedef unsigned char 		UInt8;
typedef signed short 		SInt16;
typedef unsigned short 		UInt16;
typedef signed long 		SInt32;
typedef unsigned long 		UInt32;
#define kOTNoError		0
#define kInternalCheckError	0
#define noErr			0

/* Added from Copeland Sources.
*/

//typedef	unsigned short		OSStatus;
typedef unsigned char		*Ptr;

typedef UInt8			InterruptState;	// ejk check this	
typedef UInt8			ISTProperty;	// ejk check this	


// Interrupts
//
typedef enum InterruptTypes {
    kSerialInterrupts   	= 0,          	// IST Member from motherboard
    kTxDMAInterrupts  		= 1,     	// IST Member from motherboard
    kRxDMAInterrupts  		= 2,      	// IST Member from motherboard
    kNoInterrupts		= 3,
    kTxInterrupts		= 4,  		//SCC chip level
    kRxInterrupts		= 5,
    kSccInterrupts		= 6,		// sets/clears chip interrupts
    kAllInterrupts		= 7		// invokes OS enabler/disbaler
} InterruptTypes;

// SCC interrupt sources
typedef enum SCCInterruptSource {
    kSccTransmitInterrupt	= 0,		// transmit buffer empty interrupt
    kSccExtStatusInterrupt	= 1,		// external/status interrupt
    kSccReceiveInterrupt	= 2,		// receiver interrupt
    kSccReceiveErrorInterrupt	= 3,		// receiver error interrupt
    kSccInterruptSources	= 4		// total number of SCC interrupt sources
} SCCInterruptSource;


#if 0
// IST Information
//
struct ISTInfo
{
    ISTProperty			*data;
    InterruptHandler		origISRFunction		[ kISTPropertyMemberCount ];
    InterruptEnabler		origEnablerFunction	[ kISTPropertyMemberCount ];
    InterruptDisabler		origDisablerFunction	[ kISTPropertyMemberCount ];
    void			*origRefCon		[ kISTPropertyMemberCount ];
};
typedef struct ISTInfo ISTInfo;
typedef ISTInfo *ISTInfoPtr;
#endif	

// Machine Types
//
typedef enum Machine_Type	{
    kUnknownMachine 		= 0,
    k5300Machine,				// PowerBook Class
    k6100Machine,				// PDM Class
    k7100Machine,
    k8100Machine,
    k7500Machine,				// PowerSurge Class
    k8500Machine,	
    k9500Machine,
    ke407Machine                		//Alchemy  (more types to come  NAGA 7/8/96
} Machine_Type;

// These are temporary until the Motherboard expert provides them - …MB
enum SerialOffsets
{
    channelADataOffset		= 6,		// channel A data in or out
    channelAControlOffset	= 2,		// channel A control
    channelBDataOffset		= 4,		// channel B data in or out
    channelBControlOffset	= 0,		// channel B control

    channelADataOffsetRISC	= 0x30,		// channel A data in or out
    channelAControlOffsetRISC	= 0x20,		// channel A control
    channelBDataOffsetRISC	= 0x10,		// channel B data in or out
    channelBControlOffsetRISC	= 0		// channel B control
};
//	typedef UInt8 SerialOffsets;	****
	
#define channelDataOffsetRISC	0x010
#define channelControlOffsetRISC 0x000
#define channelDataOffset	4
#define channelControlOffset	0

enum InterruptAssignments {
    kIntChipSet			= 0,
    kIntTxDMA,
    kIntRxDMA,
    MaxInterrupts
};
	
#define DMABufferSize		4096

enum ParityType {
    NoParity 			= 0,
    OddParity,
    EvenParity,
    MaxParity
};
//	typedef UInt8 ParityType;		***

enum SerialPortSelector
{
    serialPortA			= 0,
    serialPortB			= 1,
    MaxPortsPerChip		= 2
};
//	typedef UInt8 SerialPortSelector;	***
#define ChannelAName		"ch-a"
#define ChannelBName		"ch-b"


#define kDefaultBaudRate	9600
//#define kMaxBaudRate		57600
#define kMaxBaudRate		230400		// experimenting with higher speeds hul
#define kMaxCirBufferSize	4096		

#endif SccTypes.h_h
