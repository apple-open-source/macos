/*
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
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
#ifndef _APPLEI2S_H
#define _APPLEI2S_H

#include <IOKit/IOService.h>

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
//#define APPLEI2S_DEBUG 1

#ifdef APPLEI2S_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

class AppleI2S : public IOService
{
	OSDeclareDefaultStructors(AppleI2S)

	private:
		// key largo gives us register access
    	IOService	*keyLargoDrv;			

        // i2s register space offset relative to key largo base address
        UInt32 i2sBaseAddress;
		
	public:
        virtual bool init(OSDictionary *dict);
        virtual void free(void);
        virtual IOService *probe(IOService *provider, SInt32 *score);
        virtual bool start(IOService *provider);
        virtual void stop(IOService *provider);
        
		// AppleI2S reads and writes are services through the callPlatformFunction
		virtual IOReturn callPlatformFunction( const char *functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

		virtual IOReturn callPlatformFunction( const OSSymbol *functionSymbol,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

};

// callPlatformFunction symbols to access key largo or K2 registers
#define kSafeWriteRegUInt32 	"keyLargo_safeWriteRegUInt32"
#define kSafeReadRegUInt32		"keyLargo_safeReadRegUInt32"

// K2 register write is always passed  noMask for mask value
#define 	noMask 					0xFFFFFFFF

// callPlatformFunction symbols for AppleI2S
#define kI2SGetIntCtlReg		"I2SGetIntCtlReg"
#define kI2SSetIntCtlReg		"I2SSetIntCtlReg"
#define kI2SGetSerialFormatReg	"I2SGetSerialFormatReg"
#define kI2SSetSerialFormatReg	"I2SSetSerialFormatReg"
#define kI2SGetCodecMsgOutReg	"I2SGetCodecMsgOutReg"
#define kI2SSetCodecMsgOutReg	"I2SSetCodecMsgOutReg"
#define kI2SGetCodecMsgInReg	"I2SGetCodecMsgInReg"
#define kI2SSetCodecMsgInReg	"I2SSetCodecMsgInReg"
#define kI2SGetFrameCountReg	"I2SGetFrameCountReg"
#define kI2SSetFrameCountReg	"I2SSetFrameCountReg"
#define kI2SGetFrameMatchReg	"I2SGetFrameMatchReg"
#define kI2SSetFrameMatchReg	"I2SSetFrameMatchReg"
#define kI2SGetDataWordSizesReg	"I2SGetDataWordSizesReg"
#define kI2SSetDataWordSizesReg	"I2SSetDataWordSizesReg"
#define kI2SGetPeakLevelSelReg	"I2SGetPeakLevelSelReg"
#define kI2SSetPeakLevelSelReg	"I2SSetPeakLevelSelReg"
#define kI2SGetPeakLevelIn0Reg		"I2SGetPeakLevelIn0Reg"
#define kI2SSetPeakLevelIn0Reg		"I2SSetPeakLevelIn0Reg"
#define kI2SGetPeakLevelIn1Reg		"I2SGetPeakLevelIn1Reg"
#define kI2SSetPeakLevelIn1Reg		"I2SSetPeakLevelIn1Reg"

// I2S Register offsets within keyLargo or K2
#define		kI2SIntCtlOffset		0x0000
#define		kI2SSerialFormatOffset	0x0010
#define		kI2SCodecMsgOutOffset	0x0020
#define		kI2SCodecMsgInOffset	0x0030
#define		kI2SFrameCountOffset	0x0040
#define		kI2SFrameMatchOffset	0x0050
#define		kI2SDataWordSizesOffset	0x0060
#define		kI2SPeakLevelSelOffset	0x0070
#define		kI2SPeakLevelIn0Offset	0x0080
#define		kI2SPeakLevelIn1Offset	0x0090

// The following is actually read from the "reg" property in the tree.
#define		kI2SBaseOffset			0x10000

#endif /* ! _APPLEI2S_H */
