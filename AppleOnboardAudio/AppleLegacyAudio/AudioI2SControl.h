/*
 *  AudioI2SControl.h
 *  Apple02Audio
 *
 *  Created by nthompso on Fri Jul 13 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 *
 * This file contains a class for dealing with i2s for audio drivers.
 */


#ifndef _AUDIOI2SCONTROL_H
#define _AUDIOI2SCONTROL_H

#include "AudioHardwareCommon.h"
#include "AudioHardwareUtilities.h"
#include "Apple02Audio.h"

// Sound Formats:
typedef enum SoundFormat 
{
    kSndIOFormatI2SSony,
    kSndIOFormatI2S64x,
    kSndIOFormatI2S32x,

    // This says "we never decided for a sound format before"
    kSndIOFormatUnknown
} SoundFormat;

// Characteristic constants:
typedef enum TicksPerFrame 
{
    k64TicksPerFrame		= 64,			// 64 ticks per frame
    k32TicksPerFrame		= 32 			// 32 ticks per frame
} TicksPerFrame;

typedef enum ClockSource 
{
	kClock49MHz				= 49152000,		// 49 MHz clock source
	kClock45MHz				= 45158400,		// 45 MHz clock source
	kClock18MHz				= 18432000		 // 18 MHz clock source
} ClockSource;

// this struct type is used as a param block for passing info about the i2s object
// being created into the create and init methods.
typedef struct _s_AudioI2SInfo 
{
    UInt32			i2sSerialFormat;
    IOMemoryMap		*map ;
} AudioI2SInfo;

#define kAudioFCR1Mask 0x001E3C00

// AudioI2SControl is essentially a class for setting the state of the I2S registers
class AudioI2SControl : public OSObject 
{
	OSDeclareDefaultStructors(AudioI2SControl);
private:
	// holds the current frame rate settings:
	ClockSource			clockSource;
	UInt32      		mclkDivisor;
	UInt32      		sclkDivisor;
	UInt32				serialFormat;
	UInt32				dataFormat;
	IODeviceMemory *	ioBaseAddressMemory;
	IODeviceMemory *	ioI2SBaseAddressMemory;

//	bool dependentSetup(void) ;

	UInt32 ReadWordLittleEndian(void *address,UInt32 offset);
	void WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value);

	void	*soundConfigSpace;					//	address of sound config space
    void	*ioBaseAddress;						// base address of our I/O controller
	void	*i2sBaseAddress;					//	base address of I2S I/O Module
//	void	*ioClockBaseAddress;				//	base address for the clock						[3060321]
//	void	*ioStatusRegister_GPIO12;			//	the register with the input detection			[3060321]

	// Recalls which i2s interface we are attached to:
	UInt8 i2SInterfaceNumber;

public:
	bool setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio, ClockSource *pClockSource, UInt32 *pMclkDivisor, UInt32 *pSclkDivisor, UInt32 newSerialFormat);
	void setSerialFormatRegister( ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat, UInt32 newDataFormat) ;

	// Access to all the I2S registers:
	// -------------------------------
	UInt32 GetIntCtlReg(void);
	void   SetIntCtlReg(UInt32 value);
	UInt32 GetSerialFormatReg(void);
	void   SetSerialFormatReg(UInt32 value);
	UInt32 GetCodecMsgOutReg(void);
	void   SetCodecMsgOutReg(UInt32 value);
	UInt32 GetCodecMsgInReg(void);
	void   SetCodecMsgInReg(UInt32 value);
	UInt32 GetFrameCountReg(void);
	void   SetFrameCountReg(UInt32 value);
	UInt32 GetFrameMatchReg(void);
	void   SetFrameMatchReg(UInt32 value);
	UInt32 GetDataWordSizesReg(void);
	void   SetDataWordSizesReg(UInt32 value);
	UInt32 GetPeakLevelSelReg(void);
	void   SetPeakLevelSelReg(UInt32 value);
	UInt32 GetPeakLevelIn0Reg(void);
	void   SetPeakLevelIn0Reg(UInt32 value);
	UInt32 GetPeakLevelIn1Reg(void);
	void   SetPeakLevelIn1Reg(UInt32 value);
	UInt32 GetCounterReg(void);

	UInt32	FCR1GetReg(void);
	void		Fcr1SetReg(UInt32 value);
	UInt32	FCR3GetReg(void);
	void		Fcr3SetReg(UInt32 value);

	// starts and stops the clock count:
	void   KLSetRegister(UInt32 klRegister, UInt32 value);
	UInt32   KLGetRegister(UInt32 klRegister);

	static AudioI2SControl *create(AudioI2SInfo *theInfo) ;
//	void *getIOStatusRegister_GPIO12(void) { return (ioStatusRegister_GPIO12); } ;			[3060321]

protected:
    bool init(AudioI2SInfo *theInfo) ;
    void free(void) ;
    bool clockRun(bool start) ;    
    UInt32 frameRate(UInt32 index) ;
};

#endif
