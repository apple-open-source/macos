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

// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
bool AudioI2SControl::init(AudioI2SInfo *theInfo) 
{    
    debugIOLog("+ AudioI2SControl::init\n");
	
    if(!super::init())
        return(false);
		
	if ( NULL == theInfo ) { return ( false ); }		//	[3060321]	rbm	1 Oct 2002
	
    IOMemoryMap 		*map = theInfo->map ;

	switch ( theInfo->i2sSerialFormat ) {				//	[3060321]	rbm	2 Oct 2002	begin {
		case kSerialFormatSony:							//	fall through to kSerialFormatSiliLabs
		case kSerialFormat64x:							//	fall through to kSerialFormatSiliLabs
		case kSerialFormat32x:							//	fall through to kSerialFormatSiliLabs
		case kSerialFormatDAV:							//	fall through to kSerialFormatSiliLabs
		case kSerialFormatSiliLabs:
			serialFormat = theInfo->i2sSerialFormat;	//	set the format as requested
			break;
		default:
			debugIOLog ( "### WRONG I2S Serial Format\n" );
			serialFormat = kSerialFormatSony;			//	force a legal value here...
			break;
	}													//	[3060321]	rbm	1 Oct 2002	} end
	
    // cache the config space
	soundConfigSpace = (UInt8 *)map->getPhysicalAddress();

    // sets the clock base address figuring out which I2S cell we're on
    if ((((UInt32)soundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) 
    {
		//	[3060321]	ioBaseAddress is required by this object in order to enable the target
		//				I2S I/O Module for which this object is to service.  The I2S I/O Module
		//				enable occurs through the configuration registers which reside in the
		//				first block of ioBase.		rbm		2 Oct 2002
		ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S0BaseOffset);
		ioBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)soundConfigSpace - kI2S0BaseOffset), 256);
        i2SInterfaceNumber = kUseI2SCell0;
    }
    else if ((((UInt32)soundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) 
    {
		//	[3060321]	ioBaseAddress is required by this object in order to enable the target
		//				I2S I/O Module for which this object is to service.  The I2S I/O Module
		//				enable occurs through the configuration registers which reside in the
		//				first block of ioBase.		rbm		2 Oct 2002
		ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S1BaseOffset);
		ioBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)soundConfigSpace - kI2S1BaseOffset), 256);
        i2SInterfaceNumber = kUseI2SCell1;
    }
    else 
    {
        DEBUG_IOLOG("AudioI2SControl::init ERROR: unable to setup ioBaseAddress and i2SInterfaceNumber\n");
    }

	//	[3060321]	ioConfigurationBaseAddress is required by this object in order to enable the target
	//				I2S I/O Module for which this object is to service.  The I2S I/O Module
	//				enable occurs through the configuration registers which reside in the
	//				first block of ioBase.		rbm		2 Oct 2002
	if (NULL != ioBaseAddressMemory) {
		ioConfigurationBaseAddress = (void *)ioBaseAddressMemory->map()->getVirtualAddress();
	} else {
		return false;
	}

	//
	//	There are three sections of memory mapped I/O that are directly accessed by the AppleOnboardAudio.  These
	//	include the GPIOs, I2S DMA Channel Registers and I2S control registers.  They fall within the memory map 
	//	as follows:
	//	~                              ~
	//	|______________________________|
	//	|                              |
	//	|         I2S Control          |
	//	|______________________________|	<-	soundConfigSpace = ioBase + i2s0BaseOffset ...OR... ioBase + i2s1BaseOffset
	//	|                              |
	//	~                              ~
	//	~                              ~
	//	|______________________________|
	//	|                              |
	//	|       I2S DMA Channel        |
	//	|______________________________|	<-	i2sDMA = ioBase + i2s0_DMA ...OR... ioBase + i2s1_DMA
	//	|                              |
	//	~                              ~
	//	~                              ~
	//	|______________________________|
	//	|            FCRs              |
	//	|            GPIO              |	<-	gpio = ioBase + gpioOffsetAddress
	//	|         ExtIntGPIO           |	<-	fcr = ioBase + fcrOffsetAddress
	//	|______________________________|	<-	ioConfigurationBaseAddress
	//	|                              |
	//	~                              ~
	//
	//	The I2S DMA Channel is mapped in by the AppleDBDMAAudioDMAEngine.  Only the I2S control registers are 
	//	mapped in by the AudioI2SControl.  The Apple I/O Configuration Space (i.e. FCRs, GPIOs and ExtIntGPIOs)
	//	are mapped in by the subclass of AppleOnboardAudio.  The FCRs must also be mapped in by the AudioI2SControl
	//	object as the init method must enable the I2S I/O Module for which the AudioI2SControl object is
	//	being instantiated for.
	//
	
	//	Map the I2S configuration registers
	if (kUseI2SCell0 == i2SInterfaceNumber) {
		ioI2SBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)soundConfigSpace), kI2S_IO_CONFIGURATION_SIZE);
	} else {
		ioI2SBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)soundConfigSpace), kI2S_IO_CONFIGURATION_SIZE);
	}
	if (NULL != ioI2SBaseAddressMemory) {
		i2sBaseAddress = (void *)ioI2SBaseAddressMemory->map()->getVirtualAddress();
	} else {
		return false;
	}

	//	Enable the I2S interface by setting the enable bit in the feature 
	//	control register.  This one action requires knowledge of the address 
	//	of I/O configuration address space.		[3060321]	rbm		2 Oct 2002
	if ( kUseI2SCell0 == i2SInterfaceNumber ) {
		KLSetRegister ( ((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset, KLGetRegister ( ((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset ) | kI2S0InterfaceEnable );
	} else {
		KLSetRegister ( ((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset, KLGetRegister ( ((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset ) | kI2S1InterfaceEnable );
	}

    DEBUG_IOLOG("- AudioI2SControl::init\n");
    return(true);
}


// --------------------------------------------------------------------------
void AudioI2SControl::free()
{
	if (NULL != ioBaseAddressMemory) {
		ioBaseAddressMemory->release();
	}
	
	if (NULL != ioI2SBaseAddressMemory) {
		ioI2SBaseAddressMemory->release();
	}

    super::free();
}

// --------------------------------------------------------------------------
// Method: setSampleRate :: Sets the sample rate on the I2S bus

bool AudioI2SControl::setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio, ClockSource *pClockSource, UInt32 *pMclkDivisor, UInt32 *pSclkDivisor, UInt32 newSerialFormat) 
{
    UInt32	mclkRatio;
    UInt32	reqMClkRate;
	
    mclkRatio = mclkToFsRatio;		// remember the MClk ratio required

    if ( mclkRatio == 0 )		// or make one up if MClk not required
        mclkRatio = 64;			// use 64 x ratio since that will give us the best characteristics
    
    reqMClkRate = sampleRate * mclkRatio;	// this is the required MClk rate

	// look for a source clock that divides down evenly into the MClk
    if ((kClock18MHz % reqMClkRate) == 0) {  		// preferential source is 18 MHz
        clockSource = kClock18MHz;
    } else if ((kClock45MHz % reqMClkRate) == 0) {	// next check 45 MHz clock (11.025, 22.050 & 44.100 KHz sample rates)
        clockSource = kClock45MHz;
    } else if ((kClock49MHz % reqMClkRate) == 0) {	// last, try 49 Mhz clock (48.000 & 96.000 KHz sample rates)
        clockSource = kClock49MHz;
    } else {
        CLOG("AppleDACAAudio::setSampleParameters Unable to find a suitable source clock (no globals changes take effect)\n");
        return false;
    }
	*pClockSource = clockSource;

    // get the MClk divisor
    mclkDivisor = clockSource / reqMClkRate;
	*pMclkDivisor = mclkDivisor;
    
    switch (newSerialFormat) {					// SClk depends on format	[3060320]	rbm		2 Oct 2002
        case kSndIOFormatI2SSony:
        case kSndIOFormatI2S64x:
            sclkDivisor = mclkRatio / k64TicksPerFrame;	// SClk divisor is MClk ratio/64
			serialFormat = newSerialFormat;
            break;
        case kSndIOFormatI2S32x:
            sclkDivisor = mclkRatio / k32TicksPerFrame;	// SClk divisor is MClk ratio/32
			serialFormat = newSerialFormat;
            break;
        default:
            DEBUG_IOLOG("AppleDACAAudio::setSampleParameters Invalid serial format\n");
            return false;
            break;
    }
	*pSclkDivisor = sclkDivisor;

    return true;
 }

// --------------------------------------------------------------------------
// Method: setSerialFormatRegister ::Set global values to the serial format register
void AudioI2SControl::setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat, UInt32 newDataFormat)
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
	// 2] Setup the serial format register & data format register
	SetSerialFormatReg ( regValue );
	dataFormat = newDataFormat;				//	[3060321]	save this to verify value that was used to init!
	SetDataWordSizesReg ( dataFormat );		//	[3060321]	rbm	2 Oct 2002	MUST OCCUR WHILE CLOCK IS STOPPED
	// 3 restarts the clock:
    clockRun(true);    
}


#pragma mark + GENERIC REGISTER ACCESS ROUTINES

// Generic INLINEd methods to access to registers:
// ===============================================
UInt32 AudioI2SControl::ReadWordLittleEndian(void *address, UInt32 offset )
{
    return OSReadLittleInt32(address, offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value)
{
    OSWriteLittleInt32(address, offset, value);
}

// INLINEd methods to access to all the I2S registers:
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetIntCtlReg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SIntCtlOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetIntCtlReg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SIntCtlOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetSerialFormatReg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SSerialFormatOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetSerialFormatReg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SSerialFormatOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetCodecMsgOutReg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SCodecMsgOutOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetCodecMsgOutReg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SCodecMsgOutOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetCodecMsgInReg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SCodecMsgInOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetCodecMsgInReg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SCodecMsgInOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetFrameCountReg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SFrameCountOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetFrameCountReg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SFrameCountOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetFrameMatchReg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SFrameMatchOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetFrameMatchReg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SFrameMatchOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetDataWordSizesReg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SDataWordSizesOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetDataWordSizesReg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SDataWordSizesOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetPeakLevelSelReg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SPeakLevelSelOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetPeakLevelSelReg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SPeakLevelSelOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetPeakLevelIn0Reg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SPeakLevelIn0Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetPeakLevelIn0Reg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SPeakLevelIn0Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetPeakLevelIn1Reg(void)
{
	return ReadWordLittleEndian(i2sBaseAddress, kI2SPeakLevelIn1Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::SetPeakLevelIn1Reg(UInt32 value)
{
	WriteWordLittleEndian(i2sBaseAddress, kI2SPeakLevelIn1Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::GetCounterReg(void )
{
	return ((UInt32)(i2sBaseAddress) + kI2SFrameCountOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::FCR1GetReg( void )
{
	return ReadWordLittleEndian ( ioConfigurationBaseAddress, kFCR1Offset );		//	[3060321]	FCR accessor to use ioConfigurationBaseAddress
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::Fcr1SetReg(UInt32 value)
{
	WriteWordLittleEndian( ioConfigurationBaseAddress, kFCR1Offset, value );		//	[3060321]	FCR accessor to use ioConfigurationBaseAddress
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::FCR3GetReg( void )
{
	return ReadWordLittleEndian ( ioConfigurationBaseAddress, kFCR3Offset );		//	[3060321]	FCR accessor to use ioConfigurationBaseAddress
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::Fcr3SetReg(UInt32 value)
{
	WriteWordLittleEndian( ioConfigurationBaseAddress, kFCR3Offset, value );		//	[3060321]	FCR accessor to use ioConfigurationBaseAddress
}

// Access to Keylargo registers:
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3110829] register accesses need to translate endian format.
void AudioI2SControl::KLSetRegister(void *klRegister, UInt32 value)
{
	OSWriteLittleInt32(klRegister, 0, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3110829] register accesses need to translate endian format.
UInt32 AudioI2SControl::KLGetRegister(void *klRegister)
{
    return (OSReadLittleInt32(klRegister, 0));
}

// --------------------------------------------------------------------------
// Method: clockRun  ::starts and stops the clock count:
bool AudioI2SControl::clockRun(bool start) 
{
    bool success = true;

    if (start) {
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:
				KLSetRegister(((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset, KLGetRegister( ((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset ) | kI2S0ClockEnable);
				break;
			case kUseI2SCell1:
				KLSetRegister(((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset, KLGetRegister( ((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset ) | kI2S1ClockEnable);
				break;
		}
    } else {
        UInt16 loop = 50;
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:
				KLSetRegister(((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset, KLGetRegister( ((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset ) & (~kI2S0ClockEnable));
				break;
			case kUseI2SCell1:
				KLSetRegister(((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset, KLGetRegister( ((UInt8*)ioConfigurationBaseAddress) + kFCR1Offset ) & (~kI2S1ClockEnable));
				break;
		}
        
		while (((GetIntCtlReg() & kClocksStoppedPending) == 0) && (loop--)) {
			// it does not do anything, jut waites for the clock
			// to stop
			IOSleep(10);
		}
		// we are successful if the clock actually stopped.
		success = ((GetIntCtlReg() & kClocksStoppedPending) != 0);
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

