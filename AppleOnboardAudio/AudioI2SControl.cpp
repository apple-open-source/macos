/*
 *  AudioI2SControl.cpp
 *  AppleOnboardAudio
 *
 *  Created by nthompso on Fri Jul 13 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 *
 * This file contains a class for dealing with i2s for audio drivers.
 */

#include "AudioI2SControl.h"
#include "daca_hw.h"
#include "AudioI2SHardwareConstants.h"

#define super OSObject
OSDefineMetaClassAndStructors(AudioI2SControl, OSObject)

AudioI2SControl *AudioI2SControl::create(AudioI2SInfo *theInfo)
{
    DEBUG_IOLOG("+ AudioI2SControl::create\n");
    AudioI2SControl *myAudioI2SControl;
    myAudioI2SControl = new AudioI2SControl;
    
    if(myAudioI2SControl) {
        if(!(myAudioI2SControl->init(theInfo))){
            myAudioI2SControl->release();
            myAudioI2SControl = 0;
        }            
    }
    DEBUG_IOLOG("- AudioI2SControl::create\n");
    return myAudioI2SControl;
}

bool AudioI2SControl::init(AudioI2SInfo *theInfo) 
{    
    DEBUG_IOLOG("+ AudioI2SControl::init\n");
	
    IOMemoryMap 		*map = theInfo->map ;

    if(!super::init())
        return(false);

    // cache the config space
    soundConfigSpace = (UInt8 *)map->getPhysicalAddress();    

    // sets the clock base address figuring out which I2S cell we're on
    if ((((UInt32)soundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) 
    {
        ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S0BaseOffset);
        i2SInterfaceNumber = kUseI2SCell0;
    }
    else if ((((UInt32)soundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) 
    {
        ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S1BaseOffset);
        i2SInterfaceNumber = kUseI2SCell1;
    }
    else 
    {
        DEBUG_IOLOG("AudioI2SControl::init ERROR: unable to setup ioBaseAddress and i2SInterfaceNumber\n");
    }

    // This is the keylargo FC1 (Feature configuration 1)
    ioClockBaseAddress = (void *)((UInt32)ioBaseAddress + kI2SClockOffset);

    // Finds the position of the status register:
    ioStatusRegister_GPIO12 = (void *)((UInt32)ioBaseAddress + kGPio12);

    // Enables the I2S Interface:
    KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) | kI2S0InterfaceEnable);

    // Sets ups the DAC-3550 as required by this machine wiring
    // configuration:
    if (!dependentSetup())
    {
        DEBUG_IOLOG("AudioI2SControl::init ERROR: DAC-3550 setup failed\n");
    }

    DEBUG_IOLOG("- AudioI2SControl::init\n");
    return(true);
}


// --------------------------------------------------------------------------
void AudioI2SControl::free()
{
    super::free();
}

// --------------------------------------------------------------------------
// Method: setSampleRate :: Sets the sample rate on the I2S bus

bool AudioI2SControl::setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio) 
{
    UInt32	mclkRatio;
    UInt32	reqMClkRate;

    mclkRatio = mclkToFsRatio;		// remember the MClk ratio required

    if ( mclkRatio == 0 )		// or make one up if MClk not required
        mclkRatio = 64;			// use 64 x ratio since that will give us the best characteristics
    
    reqMClkRate = sampleRate * mclkRatio;	// this is the required MClk rate

	// look for a source clock that divides down evenly into the MClk
    if ((kClock18MHz % reqMClkRate) == 0) {  		// preferential source is 18 MHz
        dacaClockSource = kClock18MHz;
    } else if ((kClock45MHz % reqMClkRate) == 0) {	// next check 45 MHz clock (11.025, 22.050 & 44.100 KHz sample rates)
        dacaClockSource = kClock45MHz;
    } else if ((kClock49MHz % reqMClkRate) == 0) {	// last, try 49 Mhz clock (48.000 & 96.000 KHz sample rates)
        dacaClockSource = kClock49MHz;
    } else {
        CLOG("AppleDACAAudio::setSampleParameters Unable to find a suitable source clock (no globals changes take effect)\n");
        return false;
    }

    // get the MClk divisor
    dacaMclkDivisor = dacaClockSource / reqMClkRate;
    
    switch (dacaSerialFormat) {					// SClk depends on format
        case kSndIOFormatI2SSony:
        case kSndIOFormatI2S64x:
            dacaSclkDivisor = mclkRatio / k64TicksPerFrame;	// SClk divisor is MClk ratio/64
            break;
        case kSndIOFormatI2S32x:
            dacaSclkDivisor = mclkRatio / k32TicksPerFrame;	// SClk divisor is MClk ratio/32
            break;
        default:
            DEBUG_IOLOG("AppleDACAAudio::setSampleParameters Invalid serial format\n");
            return false;
            break;
    }

    return true;
 }

// --------------------------------------------------------------------------
// Method: setSerialFormatRegister ::Set global values to the serial format register
void AudioI2SControl::setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat)
{
    UInt32	regValue = 0;

    switch ((int)clockSource) {
        case kClock18MHz:			regValue = kClockSource18MHz;														break;
        case kClock45MHz:			regValue = kClockSource45MHz;														break;
        case kClock49MHz:			regValue = kClockSource49MHz;														break;
        default:																										break;
    }

    switch (mclkDivisor) {
        case 1:						regValue |= kMClkDivisor1;															break;
        case 3:						regValue |= kMClkDivisor3;															break;
        case 5:						regValue |= kMClkDivisor5;															break;
        default:					regValue |= (((mclkDivisor / 2) - 1) << kMClkDivisorShift) & kMClkDivisorMask;		break;
    }

    switch ((int)sclkDivisor) {		//	sclk is equivalent to Bclk
        case 1:						regValue |= kSClkDivisor1;															break;
        case 3:						regValue |= kSClkDivisor3;															break;
        default:					regValue |= (((sclkDivisor / 2) - 1) << kSClkDivisorShift) & kSClkDivisorMask;		break;
    }
    regValue |= kSClkMaster;		// force master mode

    switch (serialFormat) {
        case kSndIOFormatI2SSony:	regValue |= kSerialFormatSony;														break;
        case kSndIOFormatI2S64x:	regValue |= kSerialFormat64x;														break;
        case kSndIOFormatI2S32x:	regValue |= kSerialFormat32x;														break;
        default:																										break;
    }

	// This is a 3 step process:

	// 1] Stop the clock:
    clockRun(false);
	// 2] Setup the serial format register
	switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:    I2S0_SetSerialFormatReg(regValue);		break;
			case kUseI2SCell1:    I2S1_SetSerialFormatReg(regValue);		break;
	}
	// 3 restarts the clock:
    clockRun(true);    
}

// --------------------------------------------------------------------------
// Method: dependentSetup
//
// Purpose:
//        this handles the setup of the DAC-3550 chip for each kind of
//        hosting hardware.
bool AudioI2SControl::dependentSetup(void)
{
    DEBUG_IOLOG( "+ AppleDACAAudio::dependentSetup\n");

    // Sets the frame rate:
    UInt32 myFrameRate = frameRate(0);

    // dacaSerialFormat = kSndIOFormatI2SSony;			// start out in Sony format
    dacaSerialFormat = kSndIOFormatI2S32x;
    
    // Reads the initial status of the DAC3550A registers:
    // The following functions return "false" on the first generation of iBooks. However I wish to
    // keep them, since:
    // 1] it is bad to make assumptions on what the hardware can do and can not do here (eventually these
    //    assumptions should be made in the i2c driver).
    // 2] the next generation of iBook may supprot reading the DACA registers, and we will have more precise
    //    values in the "mirror" registers.
//    if (openI2C()) 
//    {
//        // And reads the data we need:
//        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, & sampleRateReg, sizeof(sampleRateReg));
//        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8*)& analogVolumeReg, sizeof(analogVolumeReg));
//        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, & configurationReg, sizeof(configurationReg));
//
//        closeI2C();
//    }

    // If nobody set a format before choose one:
    if (dacaSerialFormat ==  kSndIOFormatUnknown)
        dacaSerialFormat = kSndIOFormatI2SSony;

    // This call will set the next of the drame parametes
    // (dacaClockSource, dacaMclkDivisor,  dacaSclkDivisor)
    if (!setSampleParameters(myFrameRate, 0)) 
    {
        DEBUG_IOLOG("AppleDACAAudio::dependentSetup can not set i2s sample rate\n");
        return false;
//    } 
// this is moved into the AppleDACAAudio class
//    else if (!setDACASampleRate(myFrameRate)) 
//    {
//        DEBUG_IOLOG("AppleDACAAudio::dependentSetup can not set DACA sample rate\n");
//        return false;
    } 
	else 
    {
        setSerialFormatRegister(dacaClockSource, dacaMclkDivisor, dacaSclkDivisor, dacaSerialFormat);
    }
    
    DEBUG_IOLOG( "+ AppleDACAAudio::dependentSetup\n");
    return true;
}

#pragma mark + GENERIC REGISTER ACCESS ROUTINES

// Generic INLINEd methods to access to registers:
// ===============================================
INLINE UInt32 AudioI2SControl::ReadWordLittleEndian(void *address, UInt32 offset )
{
#if 0
    UInt32 *realAddress = (UInt32*)(address) + offset;
    UInt32 value = *realAddress;
    UInt32 newValue =
        ((value & 0x000000FF) << 16) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0xFF000000) >> 16);

    return (newValue);
#else
    return OSReadLittleInt32(address, offset);
#endif
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value)
{
#if 0
    UInt32 *realAddress = (UInt32*)(address) + offset;
    UInt32 newValue =
        ((value & 0x000000FF) << 16) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0xFF000000) >> 16);

    *realAddress = newValue;
#else
    OSWriteLittleInt32(address, offset, value);
#endif    
}

// INLINEd methods to access to all the I2S registers:
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetIntCtlReg(void)
{
	return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SIntCtlOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetIntCtlReg(void)
{
	return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SIntCtlOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetIntCtlReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SIntCtlOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetIntCtlReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SIntCtlOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetSerialFormatReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SSerialFormatOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetSerialFormatReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SSerialFormatOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetSerialFormatReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SSerialFormatOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetSerialFormatReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SSerialFormatOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetCodecMsgOutReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SCodecMsgOutOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetCodecMsgOutReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SCodecMsgOutOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetCodecMsgOutReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SCodecMsgOutOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetCodecMsgOutReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SCodecMsgOutOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetCodecMsgInReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SCodecMsgInOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetCodecMsgInReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SCodecMsgInOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetCodecMsgInReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SCodecMsgInOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetCodecMsgInReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SCodecMsgInOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetFrameCountReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SFrameCountOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetFrameCountReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SFrameCountOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetFrameCountReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SFrameCountOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetFrameCountReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SFrameCountOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetFrameMatchReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SFrameMatchOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetFrameMatchReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SFrameMatchOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetFrameMatchReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SFrameMatchOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetFrameMatchReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SFrameMatchOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetDataWordSizesReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SDataWordSizesOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetDataWordSizesReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SDataWordSizesOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetDataWordSizesReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SDataWordSizesOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetDataWordSizesReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SDataWordSizesOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetPeakLevelSelReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SPeakLevelSelOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetPeakLevelSelReg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SPeakLevelSelOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetPeakLevelSelReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SPeakLevelSelOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetPeakLevelSelReg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SPeakLevelSelOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetPeakLevelIn0Reg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SPeakLevelIn0Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetPeakLevelIn0Reg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SPeakLevelIn0Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetPeakLevelIn0Reg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SPeakLevelIn0Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetPeakLevelIn0Reg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SPeakLevelIn0Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetPeakLevelIn1Reg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SPeakLevelIn1Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetPeakLevelIn1Reg(void)
{
    return ReadWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SPeakLevelIn1Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S0_SetPeakLevelIn1Reg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S0BaseOffset + kI2SPeakLevelIn1Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::I2S1_SetPeakLevelIn1Reg(UInt32 value)
{
    WriteWordLittleEndian(ioBaseAddress, kI2S1BaseOffset + kI2SPeakLevelIn1Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S0_GetCounterReg(void )
{
    return ((UInt32)(ioBaseAddress) + kI2S0BaseOffset + kI2SFrameCountOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::I2S1_GetCounterReg(void )
{
    return ((UInt32)(ioBaseAddress) + kI2S1BaseOffset + kI2SFrameCountOffset);
}

// Access to Keylargo registers:
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE void AudioI2SControl::KLSetRegister(void *klRegister, UInt32 value)
{
    UInt32 *reg = (UInt32*)klRegister;
    *reg = value;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
INLINE UInt32 AudioI2SControl::KLGetRegister(void *klRegister)
{
    UInt32 *reg = (UInt32*)klRegister;
    return (*reg);
}

// --------------------------------------------------------------------------
// Method: clockRun  ::starts and stops the clock count:
bool AudioI2SControl::clockRun(bool start) 
{
    bool success = true;

    if (start) {
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:
				KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) | kI2S0ClockEnable);
				break;
			case kUseI2SCell1:
				KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) | kI2S1ClockEnable);
				break;
		}
    } else {
        UInt16 loop = 50;
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:
				KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) & (~kI2S0ClockEnable));
				break;
			case kUseI2SCell1:
				KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) & (~kI2S1ClockEnable));
				break;
		}
        
        switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:
				while (((I2S0_GetIntCtlReg() & kClocksStoppedPending) == 0) && (loop--)) {
					// it does not do anything, jut waites for the clock
					// to stop
					IOSleep(10);
				}
				// we are successful if the clock actually stopped.
				success =  ((I2S0_GetIntCtlReg() & kClocksStoppedPending) != 0);
				break;
			case kUseI2SCell1:
				while (((I2S1_GetIntCtlReg() & kClocksStoppedPending) == 0) && (loop--)) {
					// it does not do anything, jut waites for the clock
					// to stop
					IOSleep(10);
				}
				// we are successful if the clock actually stopped.
				success =  ((I2S1_GetIntCtlReg() & kClocksStoppedPending) != 0);
				break;
			default:
				success = false;
				break;
        }
    }

    if (!success)
        debug2IOLog("PPCDACA::clockRun(%s) failed\n", (start ? "true" : "false"));

    return success;
}


// --------------------------------------------------------------------------
// Method: frameRate
//
// Purpose:
//        Should look in the registry : for now return a default value

#define kCommonFrameRate 44100

UInt32 AudioI2SControl::frameRate(UInt32 index) 
{
    return (UInt32)kCommonFrameRate;  
}

