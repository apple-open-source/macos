/*
 *  AudioI2SControl.cpp
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

#include "AudioI2SControl.h"
#include "daca_hw.h"
#include "AudioI2SHardwareConstants.h"

#define super OSObject
OSDefineMetaClassAndStructors(AudioI2SControl, OSObject)

// --------------------------------------------------------------------------
AudioI2SControl *AudioI2SControl::create(AudioI2SInfo *theInfo)
{
    debugIOLog (3, "+ AudioI2SControl::create");
    AudioI2SControl *myAudioI2SControl;
    myAudioI2SControl = new AudioI2SControl;
    
    if(myAudioI2SControl) {
        if(!(myAudioI2SControl->init(theInfo))){
            myAudioI2SControl->release();
            myAudioI2SControl = 0;
        }            
    }
    debugIOLog (3, "- AudioI2SControl::create");
    return myAudioI2SControl;
}

// --------------------------------------------------------------------------
bool AudioI2SControl::init(AudioI2SInfo *theInfo) 
{    
	UInt32				tempFcr1;
	
    debugIOLog (3, "+ AudioI2SControl::init");
	
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
			debugIOLog (3,  "### WRONG I2S Serial Format" );
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
        debugIOLog (3, "AudioI2SControl::init ERROR: unable to setup ioBaseAddress and i2SInterfaceNumber");
    }

	//
	//	There are three sections of memory mapped I/O that are directly accessed by the Apple02Audio.  These
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
	//	The I2S DMA Channel is mapped in by the Apple02DBDMAAudioDMAEngine.  Only the I2S control registers are 
	//	mapped in by the AudioI2SControl.  The Apple I/O Configuration Space (i.e. FCRs, GPIOs and ExtIntGPIOs)
	//	are mapped in by the subclass of Apple02Audio.  The FCRs must also be mapped in by the AudioI2SControl
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
	tempFcr1 = KLGetRegister ( kFCR1Offset );
	if ( kUseI2SCell0 == i2SInterfaceNumber ) {
		tempFcr1 &= ~( 1 << kI2S0SwReset );
		KLSetRegister ( kFCR1Offset, tempFcr1 );
		KLSetRegister ( kFCR1Offset, tempFcr1 | kI2S0InterfaceEnable );
	} else {
		tempFcr1 &= ~( 1 << kI2S1SwReset );
		KLSetRegister ( kFCR1Offset, tempFcr1 );
		KLSetRegister ( kFCR1Offset, tempFcr1 | kI2S1InterfaceEnable );
	}

    debugIOLog (3, "- AudioI2SControl::init");
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
        debugIOLog (3, "AppleDACAAudio::setSampleParameters Unable to find a suitable source clock (no globals changes take effect)");
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
            debugIOLog (3, "AppleDACAAudio::setSampleParameters Invalid serial format");
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
    UInt32					regValue = 0;
    IOService *				keyLargo;
	IOReturn				err;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	err = kIOReturnError;

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

	keyLargo = NULL;
    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		// ...turn on the i2s clocks...
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:	err = keyLargo->callPlatformFunction (funcSymbolName, false, (void *)true, (void *)0, 0, 0);	break;	//	[3323977]
			case kUseI2SCell1:	err = keyLargo->callPlatformFunction (funcSymbolName, false, (void *)true, (void *)1, 0, 0);	break;	//	[3323977]
		}
		funcSymbolName->release ();		//	[3323977]
		if ( kIOReturnSuccess != err ) {
			debugIOLog (1,  "keyLargo->callPlatformFunction FAIL" );
		}
	}

	// 2] Setup the serial format register & data format register
	SetSerialFormatReg ( regValue );
	dataFormat = newDataFormat;				//	[3060321]	save this to verify value that was used to init!
	SetDataWordSizesReg ( dataFormat );		//	[3060321]	rbm	2 Oct 2002	MUST OCCUR WHILE CLOCK IS STOPPED
	// 3 restarts the clock:
    clockRun(true);
Exit:
	return;
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
	return KLGetRegister (kFCR1Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::Fcr1SetReg(UInt32 value)
{
	KLSetRegister (kFCR1Offset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AudioI2SControl::FCR3GetReg( void )
{
	return KLGetRegister (kFCR3Offset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AudioI2SControl::Fcr3SetReg(UInt32 value)
{
	KLSetRegister (kFCR3Offset, value);
}

// Access to Keylargo registers:
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3110829] register accesses need to translate endian format.
void AudioI2SControl::KLSetRegister(UInt32 klRegisterOffset, UInt32 value)
{
    IOService *				keyLargoService = NULL;
	UInt32					mask = kAudioFCR1Mask;
	const OSSymbol *		theSymbol;
	
	theSymbol = OSSymbol::withCString ("keyLargo_safeWriteRegUInt32");
	
    keyLargoService = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));

    if (keyLargoService && theSymbol) {
        keyLargoService->callPlatformFunction (theSymbol, false, (void *)klRegisterOffset, (void *)mask, (void *) value, 0);
		theSymbol->release ();
    } 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3110829] register accesses need to translate endian format.
UInt32 AudioI2SControl::KLGetRegister(UInt32 klRegisterOffset)
{
	UInt32					value = 0;
    IOService *				keyLargoService = NULL;
	const OSSymbol *		theSymbol;
	
	theSymbol = OSSymbol::withCString ("keyLargo_safeReadRegUInt32");

    keyLargoService = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));

    if (keyLargoService && theSymbol) {
        keyLargoService->callPlatformFunction (theSymbol, false, (void *)klRegisterOffset, &value, 0, 0);
		theSymbol->release ();
    } 

	return value;
}

// --------------------------------------------------------------------------
// Method: clockRun  ::starts and stops the clock count:
bool AudioI2SControl::clockRun(bool start) 
{
    bool success = true;

    if (start) {
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:
				KLSetRegister (kFCR1Offset, KLGetRegister (kFCR1Offset) | kI2S0ClockEnable);
				break;
			case kUseI2SCell1:
				KLSetRegister (kFCR1Offset, KLGetRegister (kFCR1Offset) | kI2S1ClockEnable);
				break;
			default:
				debugIOLog (1, "\n\n\n!!!!Wrong I2S interface number!!!!\n\n");
		}
    } else {
        UInt16 loop = 50;
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:
				KLSetRegister (kFCR1Offset, KLGetRegister (kFCR1Offset) | kI2S0InterfaceEnable);
				KLSetRegister (kFCR1Offset, KLGetRegister (kFCR1Offset) | kI2S0CellEnable);
				KLSetRegister (kFCR1Offset, KLGetRegister (kFCR1Offset) & ~kI2S0ClockEnable);
				break;
			case kUseI2SCell1:
				KLSetRegister (kFCR1Offset, KLGetRegister (kFCR1Offset) | kI2S1InterfaceEnable);
				KLSetRegister (kFCR1Offset, KLGetRegister (kFCR1Offset) | kI2S1CellEnable);
				KLSetRegister (kFCR1Offset, KLGetRegister (kFCR1Offset) & ~kI2S1ClockEnable);
				break;
			default:
				debugIOLog (1, "\n\n\n!!!!Wrong I2S interface number!!!!\n\n");
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
        debugIOLog (3, "PPCDACA::clockRun(%s) failed", (start ? "true" : "false"));

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

