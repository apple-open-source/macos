/*
 *  AppleTAS3004Audio.cpp (definition)
 *  Project : Apple02Audio
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 * 
 *  The contents of this file constitute Original Code as defined in and
 *  are subject to the Apple Public Source License Version 1.1 (the
 *  "License").  You may not use this file except in compliance with the
 *  License.  Please obtain a copy of the License at
 *  http://www.apple.com/publicsource and read it before using this file.
 * 
 *  This Original Code and all software distributed under the License are
 *  distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *  License for the specific language governing rights and limitations
 *  under the License.
 * 
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  Hardware independent (relatively) code for the Texas Insruments TAS3004 Codec
 *  NEW-WORLD MACHINE ONLY!!!
 */

#include "AppleTAS3004Audio.h"

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>

#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AppleDBDMAAudio.h"

#define super IOService

OSDefineMetaClassAndStructors(AppleTAS3004Audio, AudioHardwareObjectInterface)

const UInt8 		AppleTAS3004Audio::kDEQAddress = 0x6A;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#pragma mark ---------------------
#pragma mark +UNIX LIKE FUNCTIONS
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ::init()
// call into superclass and initialize.
bool AppleTAS3004Audio::init(OSDictionary *properties)
{
	debugIOLog (3, "+ AppleTAS3004Audio::init");
	if (!super::init(properties))
		return false;

	mVolLeft = 0;
	mVolRight = 0;
	mTAS_WasDead = false;

	debugIOLog (3, "- AppleTAS3004Audio::init");
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTAS3004Audio::start (IOService * provider) {
	bool					result;

	debugIOLog (3, "+ AppleTAS3004Audio[%p]::start(%p)", this, provider);

	result = FALSE;
	FailIf (!provider, Exit);
	mAudioDeviceProvider = (AppleOnboardAudio *)provider;
	
	result = super::start (provider);

	result = provider->open (this, kFamilyOption_OpenMultiple);

Exit:
	debugIOLog (3, "- AppleTAS3004Audio[%p]::start(%p) returns %s", this, provider, result == true ? "true" : "false");
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTAS3004Audio::free()
{
	debugIOLog (3, "+ AppleTAS3004Audio::free");

	super::free();

	debugIOLog (3, "- AppleTAS3004Audio::free");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Post init init, but pre preDMAEngineInit init - sets platform object, possibly transport object
void AppleTAS3004Audio::initPlugin(PlatformInterface* inPlatformObject) 
{
	mPlatformInterface = inPlatformObject;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTAS3004Audio::willTerminate ( IOService * provider, IOOptionBits options )
{
	bool result = super::willTerminate( provider, options );
	debugIOLog (3, "AppleTAS3004Audio::willTerminate(%p) returns %d, mAudioDeviceProvider = %p", provider, result, mAudioDeviceProvider);

	if (provider == mAudioDeviceProvider) {
		debugIOLog (3, "closing our provider");
		provider->close (this);
	}

	debugIOLog (3, "- AppleTAS3004Audio[%p]::willTerminate(%p) returns %s", this, provider, result == true ? "true" : "false");
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTAS3004Audio::requestTerminate ( IOService * provider, IOOptionBits options )
{

	
	bool result = super::requestTerminate( provider, options );
	debugIOLog (3, "AppleTAS3004Audio::requestTerminate(%p) returns %d, mAudioDeviceProvider = %p", provider, result, mAudioDeviceProvider);
	return result;
}

/************************** Hardware Register Manipulation ********************/
// Hardware specific functions : These are all virtual functions and we have to 
// implement these in the driver class

// --------------------------------------------------------------------------
// ::sndHWInitialize
// hardware specific initialization needs to be in here, together with the code
// required to start audio on the device.
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
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ::initHardware
// Don't do a whole lot in here, but do call the inherited inithardware method.
// in turn this is going to call sndHWInitialize to perform initialization.	 All
// of the code to initialize and start the device needs to be in that routine, this 
// is kept as simple as possible.
bool AppleTAS3004Audio::preDMAEngineInit()
{
	IOReturn				err;
	IORegistryEntry			*sound;
	UInt32					loopCnt;
	UInt8					data[kTAS3004BIQwidth];						// space for biggest register size

	debugIOLog (3, "+ AppleTAS3004Audio::preDMAEngineInit");

	debugIOLog (3, "  ourProvider's name is %s", mAudioDeviceProvider->getName ());
	sound = mAudioDeviceProvider->getParentEntry (gIOServicePlane);
	FailIf (!sound, Exit);
	debugIOLog (3, "  sound's name is %s", sound->getName ());

	//	Initialize the TAS3004 as follows:
	//		Mode:					normal
	//		SCLK:					64 fs
	//		input serial mode:		i2s
	//		output serial mode:		i2s
	//		serial word length:		16 bits
	//		Dynamic range control:	disabled
	//		Volume (left & right):	muted
	//		Treble / Bass:			unity
	//		Biquad filters:			unity
	//	Initialize the TAS3004 registers the same as the TAS3004 with the following additions:
	//		AnalogPowerDown:		normal
	//		
	data[0] = ( kNormalLoad << kFL ) | ( k64fs << kSC ) | TAS_I2S_MODE | ( TAS_WORD_LENGTH << kW0 );
	err = CODEC_WriteRegister( kTAS3004MainCtrl1Reg, data, kUPDATE_SHADOW );	//	default to normal load mode, 16 bit I2S
	FailMessage ( kIOReturnSuccess != err );

	data[DRC_AboveThreshold]	= kDisableDRC;
	data[DRC_BelowThreshold]	= 0;
	data[DRC_Threshold]			= 0;
	data[DRC_Integration]		= 0;
	data[DRC_Attack]			= 0;
	data[DRC_Decay]				= 0;
	err = CODEC_WriteRegister( kTAS3004DynamicRangeCtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );

	for( loopCnt = 0; loopCnt < kTAS3004VOLwidth; loopCnt++ ) {				//	init to volume = muted
	    data[loopCnt] = 0;
	}
	
	err = CODEC_WriteRegister( kTAS3004VolumeCtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );

	data[0] = 0x72;									//	treble = bass = unity 0.0 dB
	err = CODEC_WriteRegister( kTAS3004TrebleCtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004BassCtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );

	data[0] = 0x10;								//	output mixer output channel to unity = 0.0 dB
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;								//	output mixer call progress channel to mute = -70.0 dB
	data[4] = 0x00;
	data[5] = 0x00;
	data[6] = 0x00;								//	output mixer analog playthrough channel to mute = -70.0 dB
	data[7] = 0x00;
	data[8] = 0x00;
	err = CODEC_WriteRegister( kTAS3004MixerLeftGainReg, data, kUPDATE_SHADOW );	//	initialize left channel
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004MixerRightGainReg, data, kUPDATE_SHADOW );	//	initialize right channel
	FailMessage ( kIOReturnSuccess != err );

	for( loopCnt = 1; loopCnt < kTAS3004BIQwidth; loopCnt++ )				//	all biquads to unity gain all pass mode
		data[loopCnt] = 0x00;
	data[0] = 0x10;

	err = CODEC_WriteRegister( kTAS3004LeftBiquad0CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad1CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad2CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad3CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad4CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad5CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad6CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftLoudnessBiquadReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );

	err = CODEC_WriteRegister( kTAS3004RightBiquad0CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad1CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad2CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad3CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad4CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad5CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightLoudnessBiquadReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	
	//	[3280002]	begin {
	data[0] = 0x00;				//	set the 7th right biquad to delay one sample where:	b0=0.0, b1=1.0, b2=0.0, a1=0.0, a2=0.0
	data[3] = 0x10;	
	err = CODEC_WriteRegister( kTAS3004RightBiquad6CtrlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	//	[3280002]	} end

	data[0] = 0x00;	//	loudness gain to mute
	err = CODEC_WriteRegister( kTAS3004LeftLoudnessBiquadGainReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightLoudnessBiquadGainReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	
	data[0] = ( kADMNormal << kADM ) | ( kDeEmphasisOFF << kADM ) | ( kPowerDownAnalog << kAPD );
	err = CODEC_WriteRegister( kTAS3004AnalogControlReg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );
	
	// [3173869], filters are used for phase correction, so can't use all pass mode on initialization (kNormalFilter mode was kAllPassFilter)
	data[0] = ( kNormalFilter << kAP ) | ( kNormalBassTreble << kDL );
	err = CODEC_WriteRegister( kTAS3004MainCtrl2Reg, data, kUPDATE_SHADOW );
	FailMessage ( kIOReturnSuccess != err );

	err = CODEC_Initialize();			//	flush the shadow contents to the HW
	FailMessage ( kIOReturnSuccess != err );
	
	IOSleep (1);
	
	err = SetAnalogPowerDownMode (kPowerDownAnalog);
	FailMessage ( kIOReturnSuccess != err );
	if (kIOReturnSuccess == err) {
		err = SetAnalogPowerDownMode (kPowerNormalAnalog);
		FailMessage ( kIOReturnSuccess != err );
	}

	// make sure that standby registers are init'd to something too
	memcpy (&standbyTAS3004Regs, &shadowTAS3004Regs, sizeof (TAS3004_ShadowReg));

	minVolume = kMinimumVolume;
	maxVolume = kMaximumVolume;

Exit:
	mPluginCurrentPowerState = kIOAudioDeviceActive;

	debugIOLog (3, "- AppleTAS3004Audio::preDMAEngineInit");
	return true;
}

// --------------------------------------------------------------------------
IOReturn	AppleTAS3004Audio::SetAnalogPowerDownMode( UInt8 mode )
{
	IOReturn	err;
	UInt8		dataBuffer[kTAS3004ANALOGCTRLREGwidth];
	
	err = kIOReturnSuccess;
	if ( kPowerDownAnalog == mode || kPowerNormalAnalog == mode )
	{
		err = CODEC_ReadRegister( kTAS3004AnalogControlReg, dataBuffer );
		if ( kIOReturnSuccess == err )
		{
			dataBuffer[0] &= ~( kAPD_MASK << kAPD );
			dataBuffer[0] |= ( mode << kAPD );
			err = CODEC_WriteRegister( kTAS3004AnalogControlReg, dataBuffer, kUPDATE_ALL );
		}
	}
	FailMessage ( kIOReturnSuccess != err );
	return err;
}


#pragma mark ---------------------
#pragma mark +HARDWARE IO ACTIVATION
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Set the hardware to select the desired input port after validating
//	that the target input port is available. 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	The TAS3004 supports mutually exclusive selection of one of four analog
//	input signals.  There is no provision for selection of a 'none' input.
//	Mapping of input selections is as follows:
//		kSndHWInput1:		TAS3004 analog input A, Stereo
//		kSndHWInput2:		TAS3004 analog input B, Stereo
//		kSndHWInput3:		TAS3004 analog input B, Mono sourced from left
//		kSndHWInput4:		TAS3004 analog input B, Mono sourced from right
//	Only a subset of the available inputs are implemented on any given CPU
//	due to the multiple modes that analog input B can operate in.  The
//	'sound-objects' describes the resident hardware implemenation regarding
//	available inputs and the type of input.  It is possible to achieve an
//	equivalent selection of none by collecting the connections to the analog
//	ports.  If the TAS3004 stereo input A is unused then kSndHWInput1 may
//	be aliased as kSndHWInputNone.  If neither of the TAS3004 input B
//	ports are used then kSndHWInput2 may be aliased as kSndHWInputNone.  If
//	one of the TAS3004 input B ports is used as a mono input port but the 
//	other input B mono input port remains unused then the unused mono input
//	port (i.e. kSndHWInput3 for the left channel or kSndHWInput4 for the
//	right channel) may be aliased as kSndHWInputNone.
IOReturn   AppleTAS3004Audio::setActiveInput (UInt32 input)
{
    UInt8		data[kTAS3004MaximumRegisterWidth];
    IOReturn	result = kIOReturnSuccess; 
    
	debugIOLog (3, "+ AppleTAS3004Audio::setActiveInput (%4s)", (char *)&input);

	//	Mask off the current input selection and then OR in the new selections
	CODEC_ReadRegister (kTAS3004AnalogControlReg, data);
	data[0] &= ~((1 << kADM) | (1 << kLRB) | (1 << kINP));
    switch (input) {
      case kIOAudioInputPortSubTypeLine:
			data[0] |= ((kADMNormal << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputA << kINP));
			result = CODEC_WriteRegister (kTAS3004AnalogControlReg, data, kUPDATE_ALL);
			break;
        case kIOAudioInputPortSubTypeInternalMicrophone:
			data[0] |= ((kADMBInputsMonaural << kADM) | (kRightInputForMonaural << kLRB) | (kAnalogInputB << kINP));
			result = CODEC_WriteRegister (kTAS3004AnalogControlReg, data, kUPDATE_ALL);
			break;
        default:			
			result = kIOReturnError;																					
			break;
		//case kSndHWInput2:	data[0] |= ((kADMNormal << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputB << kINP));			break;
        //case kSndHWInput3:	data[0] |= ((kADMBInputsMonaural << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputB << kINP));	break;
    }
	FailMessage ( kIOReturnSuccess != result );

    debugIOLog (3, "- AppleTAS3004Audio::setActiveInput");
    return(result);
}

#pragma mark ---------------------
#pragma mark +CONTROL FUNCTIONS
#pragma mark ---------------------

// control function

// --------------------------------------------------------------------------
IOReturn AppleTAS3004Audio::setCodecMute(bool mutestate)
{
	return setMute ( mutestate, kAnalogAudioSelector );			//	[3435307]	
}

// --------------------------------------------------------------------------
//	[3435307]	
IOReturn AppleTAS3004Audio::setCodecMute (bool muteState, UInt32 streamType) {
	IOReturn		result = kIOReturnSuccess;
	
	debugIOLog (3, "+ AppleTAS3004Audio::setCodecMute (%d, %4s)", muteState, (char*)&streamType);

	switch ( streamType ) {
		case kAnalogAudioSelector:
			if (true == muteState) {
				// mute the part
				result = SetVolumeCoefficients (0, 0);
			} else {
				// unmute the part
				result = SetVolumeCoefficients (volumeTable[(UInt32)mVolLeft], volumeTable[(UInt32)mVolRight]);
			}
			break;
		default:
			result = kIOReturnError;
			break;
	}
	debugIOLog (3, "- AppleTAS3004Audio::setCodecMute (%d, %4s) returns %X", muteState, (char*)&streamType, result);
	return result;
}

// --------------------------------------------------------------------------
//	[3435307]	
bool AppleTAS3004Audio::hasAnalogMute ()
{
	return true;
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getMinimumdBVolume () {
	return volumedBTable[minVolume];
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getMaximumdBVolume () {
	return volumedBTable[maxVolume];
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getMinimumVolume () {
	return minVolume;
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getMaximumVolume () {
	return maxVolume;
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getMaximumdBGain (void) {
	return kTAS3004_Plus_12dB;
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getMinimumdBGain (void) {
	return kTAS3004_Minus_12dB;
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getMaximumGain (void) {
	return kTAS3004_MaxGainSteps;
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getMinimumGain (void) {
	return 0;
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getDefaultInputGain (void) {
	return ( getMinimumGain() + (( getMaximumGain() - getMinimumGain() ) / 2 ) );
}

// --------------------------------------------------------------------------
UInt32 AppleTAS3004Audio::getDefaultOutputVolume (void) {
	return kInitialVolume;
}

// --------------------------------------------------------------------------
bool AppleTAS3004Audio::setCodecVolume(UInt32 leftVolume, UInt32 rightVolume)
{
	bool					result;

	result = false;

	debugIOLog (3, "+ AppleTAS3004Audio::sndHWSetSystemVolume (left: %ld, right %ld)", leftVolume, rightVolume);
	mVolLeft = leftVolume;
	mVolRight = rightVolume;
	result = SetVolumeCoefficients (volumeTable[(UInt32)mVolLeft], volumeTable[(UInt32)mVolRight]);

	debugIOLog (3, "- AppleTAS3004Audio::sndHWSetSystemVolume");
	return (result == kIOReturnSuccess);
}

// --------------------------------------------------------------------------
IOReturn AppleTAS3004Audio::setPlayThrough(bool playthroughstate)
{
	UInt8		leftMixerGain[kTAS3004MIXERGAINwidth];
	UInt8		rightMixerGain[kTAS3004MIXERGAINwidth];
	IOReturn	err;
	
	debugIOLog (3, "+ AppleTAS3004Audio::sndHWSetPlayThrough");

	err = CODEC_ReadRegister ( kTAS3004MixerLeftGainReg, leftMixerGain );
	FailIf ( kIOReturnSuccess != err, Exit );

	err = CODEC_ReadRegister ( kTAS3004MixerRightGainReg, rightMixerGain );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	//	[3281536 ]	begin {
	if ( playthroughstate ) {
		leftMixerGain[3] = rightMixerGain[3] = 0x10;
	} else {
		leftMixerGain[3] = rightMixerGain[3] = 0x00;
	}
	//	[3281536 ]	} end
	err = CODEC_WriteRegister ( kTAS3004MixerLeftGainReg, leftMixerGain, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister ( kTAS3004MixerRightGainReg, rightMixerGain, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );

Exit:
	debugIOLog (3, "- AppleTAS3004Audio::sndHWSetPlayThrough");
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTAS3004Audio::setSampleRate ( UInt32 sampleRate ) {
	IOReturn		result = kIOReturnBadArgument;
	
	debugIOLog (5,  "+ AppleTAS3004Audio:: setSampleRate ( %d )", (unsigned int) sampleRate );
	if ( ( kMinimumTAS3004SampleRate <= sampleRate ) && ( kMaximumTAS3004SampleRate >= sampleRate ) ) {
		CODEC_Initialize();
		result = kIOReturnSuccess;
	}
	debugIOLog (5,  "- AppleTAS3004Audio:: setSampleRate ( %d ) returns %lX", (unsigned int) sampleRate, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTAS3004Audio::setSampleDepth ( UInt32 sampleDepth ) {
	UInt8			data;
	IOReturn		result = kIOReturnBadArgument;
	
	debugIOLog (5,  "+ AppleTAS3004Audio:: setSampleDepth ( %d )", (unsigned int) sampleDepth );
	FailIf ( !( 16 == sampleDepth || 24 == sampleDepth ), Exit );
	
	CODEC_Initialize ();
	
	result = CODEC_ReadRegister ( kTAS3004MainCtrl1Reg, &data );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	data &= ~( kSerialWordLengthMASK << kW0 ); 
	
	if ( 16 == sampleDepth ) {
		data |= ( kSerialWordLength16 << kW0 );
	} else {
		data |= ( kSerialWordLength24 << kW0 );
	} 
	
	result = CODEC_WriteRegister ( kTAS3004MainCtrl1Reg, &data, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != result );
	
Exit:
	debugIOLog (5,  "+ AppleTAS3004Audio::setSampleWidth ( %d ) returns %d", (unsigned int)sampleDepth, (unsigned int)result );
	return result;
}

//	------------------------------------------------------------------------------------------------------------------------------------
//	Switching bewteen a system mastered clock and an external clock, such as a recovered clock from an S/PDIF AES3 stream, requires a 
//	"BREAK BEFORE MAKE" sequence to avoid having two hardware drivers connected together.  If selecting the master clock then the 
//	external MUX must be disconnected prior to enabling the system master clock.  If selecting an external MUX clock source then the 
//	internal system master clock must be disconnected first.  Sequences are:
//
//	TRANSITION						CYCLE					ACTION
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_MASTER_CLOCK to	|	1 Transport Break	|	Set MUX to alternate clock source, set I2S to SLAVE (BCLKMaster = SLAVE).
//	kTRANSPORT_SLAVE_CLOCK		|	2 Topaz Break		|	Stop CS84xx & mute TX.  Set all registers to act as a clock master.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 10, SRCD = 0
//								|						|	B.	Clock Source Control Register:	OUTC = 1, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 1
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	No Action.
//								|	5 Topaz Make		|	Start CS84xx.  Send request to restart transport hardware.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_SLAVE_CLOCK to	|	1 Transport Break	|	No Action.
//	kTRANSPORT_MASTER_CLOCK		|	2 Topaz Break		|	Stop CS84xx & disable TX.  Set all registers to act as a clock slave.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 00, SRCD = 1
//								|						|	B.	Clock Source Control Register:	OUTC = 0, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 0
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	Set MUX to default clock source, set I2S to SLAVE (BCLKMaster = MASTER).
//								|	5 Topaz Make		|	Start CS84xx & unmute TX.  Send request to restart transport hardware.
//								|						|	A.	Clear pending receiver errors.
//								|						|	B.	Enable receiver errors.
//								|						|	C.	Set CS8420 to RUN.
//								|						|	D.	Request a restart of the I2S transport.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
IOReturn	AppleTAS3004Audio::breakClockSelect ( UInt32 clockSource ) {
	IOReturn		result;
	
	debugIOLog (3,  "+ AppleTAS3004Audio::breakClockSelect ( %ld )", clockSource );
	result = kIOReturnError;
	FailIf ( !( kTRANSPORT_MASTER_CLOCK == clockSource || kTRANSPORT_SLAVE_CLOCK == clockSource ), Exit );
	mPlatformInterface->setCodecReset ( kCODEC_RESET_Analog, kGPIO_Reset );
	result = kIOReturnSuccess;
Exit:
	debugIOLog (3,  "- AppleTAS3004Audio::breakClockSelect ( %ld ) returns %X", clockSource, result );
	return result;
}

//	------------------------------------------------------------------------------------------------------------------------------------
//	Switching bewteen a system mastered clock and an external clock, such as a recovered clock from an S/PDIF AES3 stream, requires a 
//	"BREAK BEFORE MAKE" sequence to avoid having two hardware drivers connected together.  If selecting the master clock then the 
//	external MUX must be disconnected prior to enabling the system master clock.  If selecting an external MUX clock source then the 
//	internal system master clock must be disconnected first.  Sequences are:
//
//	TRANSITION						CYCLE					ACTION
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_MASTER_CLOCK to	|	1 Transport Break	|	Set MUX to alternate clock source, set I2S to SLAVE (BCLKMaster = SLAVE).
//	kTRANSPORT_SLAVE_CLOCK		|	2 Topaz Break		|	Stop CS84xx & mute TX.  Set all registers to act as a clock master.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 10, SRCD = 0
//								|						|	B.	Clock Source Control Register:	OUTC = 1, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 1
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	No Action.
//								|	5 Topaz Make		|	Start CS84xx.  Send request to restart transport hardware.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_SLAVE_CLOCK to	|	1 Transport Break	|	No Action.
//	kTRANSPORT_MASTER_CLOCK		|	2 Topaz Break		|	Stop CS84xx & disable TX.  Set all registers to act as a clock slave.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 00, SRCD = 1
//								|						|	B.	Clock Source Control Register:	OUTC = 0, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 0
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	Set MUX to default clock source, set I2S to SLAVE (BCLKMaster = MASTER).
//								|	5 Topaz Make		|	Start CS84xx & unmute TX.  Send request to restart transport hardware.
//								|						|	A.	Clear pending receiver errors.
//								|						|	B.	Enable receiver errors.
//								|						|	C.	Set CS8420 to RUN.
//								|						|	D.	Request a restart of the I2S transport.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
IOReturn	AppleTAS3004Audio::makeClockSelect ( UInt32 clockSource ) {
	IOReturn		result;
	
	debugIOLog (3,  "+ AppleTAS3004Audio::makeClockSelect ( %ld )", clockSource );
	result = kIOReturnError;
	FailIf ( !( kTRANSPORT_MASTER_CLOCK == clockSource || kTRANSPORT_SLAVE_CLOCK == clockSource ), Exit );
	CODEC_Initialize();
	result = kIOReturnSuccess;
Exit:
	debugIOLog (3,  "- AppleTAS3004Audio::makeClockSelect ( %ld ) returns %X", clockSource, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Fatal error recovery 
IOReturn AppleTAS3004Audio::recoverFromFatalError ( FatalRecoverySelector selector ) {

//	if (mSemaphores ) { debugIOLog (3,  "REDUNDANT RECOVERY FROM FATAL ERROR" ); }
	FailIf ( NULL == mPlatformInterface, Exit );
	
	switch ( selector ) {
		case kControlBusFatalErrorRecovery:
			CODEC_Initialize();
			break;
		case kClockSourceInterruptedRecovery:
			CODEC_Initialize();
			break;
		default:
			debugIOLog (3,  " *** Requested recovery from unknown condition!" );
			break;
	}
Exit:
	debugIOLog (3,  "± AppleTAS3004Audio::recoverFromFatalError ( %d )", (unsigned int)selector );
	return kIOReturnSuccess;
}

#pragma mark ---------------------
#pragma mark +POWER MANAGEMENT
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3933529]
IOReturn AppleTAS3004Audio::performSetPowerState ( UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnError;
	
	debugIOLog ( 6, "+ AppleTAS3004Audio::performSetPowerState ( %d, %d )", currentPowerState, pendingPowerState );
	switch ( pendingPowerState )
	{
		case kIOAudioDeviceSleep:
			result = performDeviceSleep ();
			FailMessage ( kIOReturnSuccess != result );
			break;
		case kIOAudioDeviceIdle:
			if ( kIOAudioDeviceActive == mPluginCurrentPowerState )
			{
				result = performDeviceSleep ();
				FailMessage ( kIOReturnSuccess != result );
			}
			else
			{
				result = kIOReturnSuccess;
			}
			break;
		case kIOAudioDeviceActive:
			result = performDeviceWake ();
			FailMessage ( kIOReturnSuccess != result );
			break;
	}
	mPluginCurrentPowerState = pendingPowerState;
	debugIOLog ( 6, "- AppleTAS3004Audio::performSetPowerState ( %d, %d ) returns 0x%lX", currentPowerState, pendingPowerState, result );
	return result;
}


// --------------------------------------------------------------------------
//	Set the audio hardware to sleep mode by placing the TAS3004 into
//	analog power down mode after muting the amplifiers.  The I2S clocks
//	must also be stopped after these two tasks in order to achieve
//	a fully low power state.  Some CPU implemenations implement a
//	Codec RESET by ANDing the mute states of the internal speaker
//	amplifier and the headphone amplifier.  The Codec must be put 
//	into analog power down state prior to asserting the both amplifier
//	mutes.  This is only invoked when the 'has-anded-reset' property
//	exists with a value of 'true'.  For all other cases, the original
//	signal manipulation order exists.
IOReturn AppleTAS3004Audio::performDeviceSleep () {
	IOReturn			result;
	
	debugIOLog (3, "+ AppleTAS3004Audio::performDeviceSleep");

	//	Mute all of the amplifiers
        
    result = SetAnalogPowerDownMode (kPowerDownAnalog);
	FailMessage ( kIOReturnSuccess != result );
 
//	mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
//	mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
//	mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
    
//	IOSleep (kAmpRecoveryMuteDuration);

	debugIOLog (3, "- AppleTAS3004Audio::performDeviceSleep");
	return result;
}
	
// --------------------------------------------------------------------------
//	The I2S clocks must have been started before performing this method.
//	This method sets the TAS3004 analog control register to normal operating
//	mode and unmutes the amplifers.  [2855519]  When waking the device it
//	is necessary to release at least one of the headphone or speaker amplifier
//	mute signals to release the Codec RESET on systems that implement the
//	Codec RESET by ANDing the headphone and speaker amplifier mute active states.
//	This must be done AFTER waking the I2S clocks!
IOReturn AppleTAS3004Audio::performDeviceWake () {
	IOReturn			err;
	IOReturn			tempErr;

	debugIOLog (3, "+ AppleTAS3004Audio::performDeviceWake");

	// ...then bring everything back up the way it should be.
	err = CODEC_Initialize ();			//	reset the TAS3001C and flush the shadow contents to the HW
	FailMessage ( kIOReturnSuccess != err );

	//	Set the TAS3004 analog control register to analog power up mode
	tempErr = SetAnalogPowerDownMode ( kPowerNormalAnalog );
	FailMessage ( kIOReturnSuccess != tempErr );
    
	if ( kIOReturnSuccess == err )
	{
		if ( kIOReturnSuccess != tempErr )
		{
			err = tempErr;
		}
	}

	debugIOLog (3,  "- AppleTAS3004Audio::performDeviceWake returns %lX", err );
	return err;
}

// --------------------------------------------------------------------------
//	[3787193]
IOReturn AppleTAS3004Audio::requestSleepTime ( UInt32 * microsecondsUntilComplete )
{
	IOReturn		result = kIOReturnBadArgument;
	
	debugIOLog ( 6, "+ AppleTAS3004Audio::requestSleepTime ( %p->%ld )", microsecondsUntilComplete, *microsecondsUntilComplete );
	if ( 0 != microsecondsUntilComplete )
	{
		*microsecondsUntilComplete = *microsecondsUntilComplete + kTAS3004_SLEEP_TIME_MICROSECONDS;
		result = kIOReturnSuccess;
	}
	debugIOLog ( 6, "- AppleTAS3004Audio::requestSleepTime ( %p->%ld ) returns 0x%lX", microsecondsUntilComplete, *microsecondsUntilComplete, result );
	return result;
}


#pragma mark ---------------------
#pragma mark + HARDWARE MANIPULATION
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Setup the pointer to the shadow register and the size of the shadow
//	register.
IOReturn 	AppleTAS3004Audio::GetShadowRegisterInfo( TAS3004_ShadowReg * shadowRegsPtr, UInt8 regAddr, UInt8 ** shadowPtr, UInt8* registerSize ) {
	IOReturn		err;
	
	err = kIOReturnSuccess;
	FailWithAction( NULL == shadowPtr, err = kIOReturnError, Exit );
	FailWithAction( NULL == registerSize, err = kIOReturnError, Exit );
	
	switch( regAddr )
	{
		case kTAS3004MainCtrl1Reg:					*shadowPtr = (UInt8*)&shadowRegsPtr->sMC1R;	*registerSize = kTAS3004MC1Rwidth;					break;
		case kTAS3004DynamicRangeCtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sDRC;	*registerSize = kTAS3004DRCwidth;					break;
		case kTAS3004VolumeCtrlReg:					*shadowPtr = (UInt8*)&shadowRegsPtr->sVOL;	*registerSize = kTAS3004VOLwidth;					break;
		case kTAS3004TrebleCtrlReg:					*shadowPtr = (UInt8*)&shadowRegsPtr->sTRE;	*registerSize = kTAS3004TREwidth;					break;
		case kTAS3004BassCtrlReg:					*shadowPtr = (UInt8*)&shadowRegsPtr->sBAS;	*registerSize = kTAS3004BASwidth;					break;
		case kTAS3004MixerLeftGainReg:				*shadowPtr = (UInt8*)&shadowRegsPtr->sMXL;	*registerSize = kTAS3004MIXERGAINwidth;				break;
		case kTAS3004MixerRightGainReg:				*shadowPtr = (UInt8*)&shadowRegsPtr->sMXR;	*registerSize = kTAS3004MIXERGAINwidth;				break;
		case kTAS3004LeftBiquad0CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sLB0;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004LeftBiquad1CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sLB1;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004LeftBiquad2CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sLB2;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004LeftBiquad3CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sLB3;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004LeftBiquad4CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sLB4;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004LeftBiquad5CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sLB5;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004LeftBiquad6CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sLB6;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004RightBiquad0CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sRB0;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004RightBiquad1CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sRB1;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004RightBiquad2CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sRB2;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004RightBiquad3CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sRB3;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004RightBiquad4CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sRB4;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004RightBiquad5CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sRB5;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004RightBiquad6CtrlReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sRB6;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004LeftLoudnessBiquadReg:			*shadowPtr = (UInt8*)&shadowRegsPtr->sLLB;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004RightLoudnessBiquadReg:		*shadowPtr = (UInt8*)&shadowRegsPtr->sRLB;	*registerSize = kTAS3004BIQwidth;					break;
		case kTAS3004LeftLoudnessBiquadGainReg:		*shadowPtr = (UInt8*)&shadowRegsPtr->sLLBG;	*registerSize = kTAS3004LOUDNESSBIQUADGAINwidth;	break;
		case kTAS3004RightLoudnessBiquadGainReg:	*shadowPtr = (UInt8*)&shadowRegsPtr->sRLBG;	*registerSize = kTAS3004LOUDNESSBIQUADGAINwidth;	break;
		case kTAS3004AnalogControlReg:				*shadowPtr = (UInt8*)&shadowRegsPtr->sACR;	*registerSize = kTAS3004ANALOGCTRLREGwidth;			break;
		case kTAS3004MainCtrl2Reg:					*shadowPtr = (UInt8*)&shadowRegsPtr->sMC2R;	*registerSize = kTAS3004MC2Rwidth;					break;
		default:									err = kIOReturnError; /* notEnoughHardware  */													break;
	}
	
Exit:
    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTAS3004Audio::InitEQSerialMode (UInt32 mode)
{
	IOReturn		err;
	UInt8			data;
	
	err = CODEC_ReadRegister (kTAS3004MainCtrl1Reg, &data);
	if ( kIOReturnSuccess == err ) {
		data &= ~( 1 << kFL );
		if ( kNormalLoad == mode ) {
			data |= (kNormalLoad << kFL);
		} else {
			data |= (kFastLoad << kFL);
		}
		err = CODEC_WriteRegister (kTAS3004MainCtrl1Reg, &data, kFORCE_UPDATE_ALL);
		FailMessage ( kIOReturnSuccess != err );
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTAS3004Audio::SetVolumeCoefficients( UInt32 left, UInt32 right )
{
	UInt8		volumeData[kTAS3004VOLwidth];
	IOReturn	err = kIOReturnSuccess;
	
	debugIOLog (3, "+ AppleTAS3004Audio::SetVolumeCoefficients ( L=%lX, R=%lX )", left, right);

	volumeData[2] = left;														
	volumeData[1] = left >> 8;												
	volumeData[0] = left >> 16;												
	
	volumeData[5] = right;														
	volumeData[4] = right >> 8;												
	volumeData[3] = right >> 16;
	
	err = CODEC_WriteRegister( kTAS3004VolumeCtrlReg, volumeData, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	debugIOLog (3, "- AppleTAS3004Audio::SetVolumeCoefficients ( L=%lX, R=%lX )", left, right);
	return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will perform a reset of the TAS3004C and then initialize
//	all registers within the TAS3004C to the values already held within 
//	the shadow registers.  The RESET sequence must not be performed until
//	the I2S clocks are running.	 The TAS3004C may hold the I2C bus signals
//	SDA and SCL low until the reset sequence (high->low->high) has been
//	completed.
IOReturn	AppleTAS3004Audio::CODEC_Initialize() {
	IOReturn	err;
	UInt32		retryCount;
	UInt32		initMode;
	UInt8		oldMode;
	UInt8		*shadowPtr;
	UInt8		registerSize;
	Boolean		done;
	
	debugIOLog ( 6, "+ AppleTAS3004Audio::CODEC_Initialize ()" );
	err = kIOReturnError;
	done = false;
	oldMode = 0;
	initMode = kUPDATE_HW;
	retryCount = 0;
	if (!mSemaphores)
	{
		mSemaphores = 1;
		do{
//			if ( 0 != retryCount ) { debugIOLog (3,  "[AppleTAS3004Audio] ... RETRYING, retryCount %ld", retryCount ); }
			debugIOLog (6,  "   ... Resetting TAS3004" );
            CODEC_Reset();

			if( 0 == oldMode )
				CODEC_ReadRegister( kTAS3004MainCtrl1Reg, &oldMode );					//	save previous load mode

			err = InitEQSerialMode( kSetFastLoadMode );						//	set fast load mode for biquad initialization
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftBiquad0CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftBiquad0CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftBiquad1CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftBiquad1CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftBiquad2CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftBiquad2CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftBiquad3CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftBiquad3CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftBiquad4CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftBiquad4CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftBiquad5CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftBiquad5CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftBiquad6CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftBiquad6CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftLoudnessBiquadReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftLoudnessBiquadReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightBiquad0CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightBiquad0CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightBiquad1CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightBiquad1CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightBiquad2CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightBiquad2CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightBiquad3CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightBiquad3CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightBiquad4CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightBiquad4CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightBiquad5CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightBiquad5CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightBiquad6CtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightBiquad6CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightLoudnessBiquadReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightLoudnessBiquadReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = InitEQSerialMode( kSetNormalLoadMode );								//	set normal load mode for most register initialization
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004DynamicRangeCtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004DynamicRangeCtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004VolumeCtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004VolumeCtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004TrebleCtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004TrebleCtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004BassCtrlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004BassCtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004MixerLeftGainReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004MixerLeftGainReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004MixerRightGainReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004MixerRightGainReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );

			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004LeftLoudnessBiquadGainReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004LeftLoudnessBiquadGainReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004RightLoudnessBiquadGainReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004RightLoudnessBiquadGainReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004AnalogControlReg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004AnalogControlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004MainCtrl2Reg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004MainCtrl2Reg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo ( &shadowTAS3004Regs, kTAS3004MainCtrl1Reg, &shadowPtr, &registerSize );
			err = CODEC_WriteRegister( kTAS3004MainCtrl1Reg, &oldMode, initMode );			//	restore previous load mode
			FailIf( kIOReturnSuccess != err, AttemptToRetry );

AttemptToRetry:				
			if( kIOReturnSuccess == err )		//	terminate when successful
			{
				done = true;
				mTAS_WasDead = false;
			}
			retryCount++;
		} while ( !done && ( kTAS3004_MAX_RETRY_COUNT != retryCount ) );
		mSemaphores = 0;
		if ( kTAS3004_MAX_RETRY_COUNT == retryCount )
		{
			mTAS_WasDead = true;
			debugIOLog (5,  "\n\n\n\n          TAS3004 IS DEAD: Check %s\n\n\n", "ChooseAudio in FCR1 bit 7 s/b '0'" );
		}
	}

	debugIOLog ( 6, "- AppleTAS3004Audio::CODEC_Initialize () returns 0x%lX", err );
    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[2855519]	Call down to lower level functions to implement the Codec
//	RESET assertion and negation where hardware dependencies exist...
void	AppleTAS3004Audio::CODEC_Reset ( void ) {

	mPlatformInterface->setCodecReset ( kCODEC_RESET_Analog, kGPIO_Run );
	IOSleep ( kCodec_RESET_SETUP_TIME );    //      I2S clocks must be running prerequisite to RESET
	mPlatformInterface->setCodecReset ( kCODEC_RESET_Analog, kGPIO_Reset );
	IOSleep ( kCodec_RESET_HOLD_TIME );
	mPlatformInterface->setCodecReset ( kCODEC_RESET_Analog, kGPIO_Run );
	IOSleep ( kCodec_RESET_RELEASE_TIME );	//	No I2C transactions for 

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Reading registers with the TAS3004 is not possible.  A shadow register
//	is maintained for each TAS3004 hardware register.  Whenever a write
//	operation is performed on a hardware register, the data is written to 
//	the shadow register.  Read operations copy data from the shadow register
//	to the client register buffer.
IOReturn 	AppleTAS3004Audio::CODEC_ReadRegister(UInt8 regAddr, UInt8* registerData) {
	UInt8			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		err;
	
	err = kIOReturnSuccess;
	
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	err = GetShadowRegisterInfo( &shadowTAS3004Regs, regAddr, &shadowPtr, &registerSize );
	if( kIOReturnSuccess == err )
	{
		for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
		{
			registerData[regByteIndex] = shadowPtr[regByteIndex];
		}
	}
	if( kIOReturnSuccess != err )
	{
		debugIOLog (3,  "  AppleTAS3004Audio::CODEC_ReadRegister %d notEnoughHardware = CODEC_ReadRegister( 0x%2.0X, 0x%8.0X )", err, regAddr, (unsigned int)registerData );
	}
	debugIOLog ( 6, "± AppleTAS3004Audio::CODEC_ReadRegister ( 0x%0.2X, %p ) returns 0x%0.8X", regAddr, registerData, err );
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	All TAS3004 write operations pass through this function so that a shadow
//	copy of the registers can be kept in global storage.  This function does enforce the data
//	size of the target register.  No partial register write operations are
//	supported.  IMPORTANT:  There is no enforcement regarding 'load' mode 
//	policy.  Client routines should properly maintain the 'load' mode by 
//	saving the contents of the master control register, set the appropriate 
//	load mode for the target register and then restore the previous 'load' 
//	mode.  All biquad registers should only be loaded while in 'fast load' 
//	mode.  All other registers should be loaded while in 'normal load' mode.
IOReturn 	AppleTAS3004Audio::CODEC_WriteRegister( UInt8 regAddr, UInt8* registerData, UInt8 mode )
{
	UInt8			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		err;
	Boolean			updateRequired;
	
	err = kIOReturnSuccess;
	updateRequired = false;
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	err = GetShadowRegisterInfo ( &shadowTAS3004Regs, regAddr, &shadowPtr, &registerSize );
	if( kIOReturnSuccess == err )
	{
		//	Write through to the shadow register as a 'write through' cache would and
		//	then write the data to the hardware;
		if( kUPDATE_SHADOW == mode || kUPDATE_ALL == mode || kFORCE_UPDATE_ALL == mode )
		{
			for ( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
			{
				if ( shadowPtr[regByteIndex] != registerData[regByteIndex] )
				{
					shadowPtr[regByteIndex] = registerData[regByteIndex];
					if ( kUPDATE_SHADOW != mode )									//  [3647247]   do not attempt to write to the hw if a cache only write is requested!
					{
						updateRequired = true;
					}
				}
			}
		}
		if ( kUPDATE_HW == mode || updateRequired || kFORCE_UPDATE_ALL == mode )
		{
			err = mPlatformInterface->writeCodecRegister ( kCodec_TAS3004, regAddr, registerData, registerSize );
			FailMessage ( kIOReturnSuccess != err );
			//	If there was a previous failure and this transaction was successful then
			//	reset and flush so that all registers are properly configured (i.e. the
			//	clocks were removed and recovery was not successful previously but can 
			//	be successful now).
			if ( kIOReturnSuccess != err )
			{
				if ( mTAS_WasDead )
				{
					mAudioDeviceProvider->interruptEventHandler ( kRequestCodecRecoveryStatus, (UInt32)kControlBusFatalErrorRecovery );
				}
				if ( !mSemaphores )			//	avoid redundant recovery
				{
					debugIOLog (3,  "  AppleTAS3004Audio::CODEC_WriteRegister mPlatformInterface->writeCodecRegister ( %d, 0x%x, %p, %d ) FATAL ERROR!", kCodec_TAS3004, regAddr, registerData, registerSize );
					mAudioDeviceProvider->interruptEventHandler ( kRequestCodecRecoveryStatus, (UInt32)kControlBusFatalErrorRecovery );
				}
			}
		}
	}

	if( kIOReturnSuccess != err )
	{
		debugIOLog (6, "± AppleTAS3004Audio::CODEC_WriteRegister returns 0x%X in AppleTAS3004Audio::CODEC_WriteRegister", err );
	}
    return err;
}


#pragma mark ---------------------
#pragma mark +UTILITY FUNCTIONS
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IORegistryEntry *AppleTAS3004Audio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSNumber				*tmpNumber;

	theEntry = NULL;
	iterator = NULL;
	FailIf (NULL == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		if (strcmp (tmpReg->getName (), name) == 0) {
			tmpNumber = OSDynamicCast (OSNumber, tmpReg->getProperty (key));
			if (NULL != tmpNumber && tmpNumber->unsigned32BitValue () == value) {
				theEntry = tmpReg;
				theEntry->retain();
			}
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IORegistryEntry *AppleTAS3004Audio::FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value) {
	OSIterator				*iterator = 0;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSData					*tmpData;

	theEntry = NULL;
	FailIf ( NULL == start, Exit );
	
	iterator = start->getChildIterator (gIODTPlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		tmpData = OSDynamicCast (OSData, tmpReg->getProperty (key));
		if (NULL != tmpData && tmpData->isEqualTo (value, strlen (value))) {
			theEntry = tmpReg;
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Boolean AppleTAS3004Audio::HasInput (void) {
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*numInputs = NULL;
	Boolean					hasInput;

	debugIOLog (3,  "+ AppleTAS3004Audio::HasInput" );
	hasInput = false;

	sound = mAudioDeviceProvider->getParentEntry (gIOServicePlane);
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kNumInputs));
	FailIf (!tmpData, Exit);
	numInputs = (UInt32*)tmpData->getBytesNoCopy ();
	if (*numInputs > 1) {
		hasInput = true;
	}
Exit:
	debugIOLog (3,  "- AppleTAS3004Audio::HasInput returns %d, numInputs %ld", hasInput, NULL == numInputs ? 0 : *numInputs );
	return hasInput;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTAS3004Audio::setDRCProcessing (void * inDRCStructure, Boolean inRealtime) {
	debugIOLog (3, "- AppleTAS3004Audio::setDRCProcessing (%p, %d)", inDRCStructure, inRealtime);
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTAS3004Audio::setEQProcessing (void * inEQStructure, Boolean inRealtime) {
	return;
}

IOReturn AppleTAS3004Audio::setBiquadCoefficients ( void * biquadCoefficients )
{
	IOReturn		err;
	IOReturn		totalErr = kIOReturnError;

	totalErr = kIOReturnSuccess;
	for ( UInt32 index = 0; index < ( kTAS3004NumBiquads * kTAS3004MaxStreamCnt ); index ++ ) {
		if ( kTAS3004NumBiquads > index ) {
			err = SndHWSetOutputBiquad ( kStreamFrontLeft, index, (FourDotTwenty*)biquadCoefficients );
		} else {
			err = SndHWSetOutputBiquad ( kStreamFrontRight, index - kTAS3004NumBiquads, (FourDotTwenty*)biquadCoefficients );
		}
		(( EQFilterCoefficients*)biquadCoefficients)++;
		if ( err ) { totalErr = err; }
	}
	return totalErr;
}

IOReturn AppleTAS3004Audio::BuildCustomEQCoefficients ( void * inEQStructure ) 
{
	return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will only restore unity gain all pass coefficients to the
//	biquad registers.  All other coefficients to be passed through exported
//	functions via the sound hardware plug-in manager libaray.  Radar 3280002
//	requires that the biquad filters always remain enabled as the 7th
//	biquad filter is dedicated for fixing the phase relationship between the
//	left and right channels where a hardware fault in the TAS3004 results in
//	a one sample delay of the left channel.  Disabling the EQ now flushes the
//	filter coefficients to a standby cache.  
void AppleTAS3004Audio::disableProcessing ( Boolean inRealtime ) {
	UInt32			index;
	UInt8			data[kTAS3004BIQwidth*2];
	IOReturn		err = kIOReturnSuccess;

	debugIOLog (3, "+ AppleTAS3004Audio::disableProcessing, already disabled = %s", mEQDisabled ? "true" : "false" );
	
	if (FALSE == mEQDisabled) {	// [3250195], don't allow multiple disables (sets standby to disabled coefficients!)
		//	[3280002]	begin {
		
		//	[3454015]	Removed all redundant transactions to set the parametric EQ filter
		//	mode to normal (not necessary since the filters are now always on to deal with
		//	fixing the phase shift problem between right and left channels).  Added code
		//	to mute the codec, delay for mute to take place prior to setting the filter 
		//	coefficients.  After setting the filter coefficients, the mute state is restored.  
		
		if (FALSE == inRealtime) {	
			SetVolumeCoefficients (0, 0);						//	mute the amplifier
			IODelay ( 70000 );									//	delay to allow mute to take place (2400 samples max)
			err = SetAnalogPowerDownMode (kPowerDownAnalog);			//	[3455140]
			FailMessage ( kIOReturnSuccess != err );
			IODelay ( 10000 );
			InitEQSerialMode ( kSetFastLoadMode );				//	and pause the DSP
		}
				
		for( index = 0; index < sizeof ( data ); index++ ) { data[index] = 0x00; }
		data[3] = 0x10;
		for ( index = 0; index < kTAS3004BIQwidth; index++ ) {
			standbyTAS3004Regs.sLB0[index] = shadowTAS3004Regs.sLB0[index];
			standbyTAS3004Regs.sLB1[index] = shadowTAS3004Regs.sLB1[index];
			standbyTAS3004Regs.sLB2[index] = shadowTAS3004Regs.sLB2[index];
			standbyTAS3004Regs.sLB3[index] = shadowTAS3004Regs.sLB3[index];
			standbyTAS3004Regs.sLB4[index] = shadowTAS3004Regs.sLB4[index];
			standbyTAS3004Regs.sLB5[index] = shadowTAS3004Regs.sLB5[index];
			standbyTAS3004Regs.sLB6[index] = shadowTAS3004Regs.sLB6[index];
			standbyTAS3004Regs.sRB0[index] = shadowTAS3004Regs.sRB0[index];
			standbyTAS3004Regs.sRB1[index] = shadowTAS3004Regs.sRB1[index];
			standbyTAS3004Regs.sRB2[index] = shadowTAS3004Regs.sRB2[index];
			standbyTAS3004Regs.sRB3[index] = shadowTAS3004Regs.sRB3[index];
			standbyTAS3004Regs.sRB4[index] = shadowTAS3004Regs.sRB4[index];
			standbyTAS3004Regs.sRB5[index] = shadowTAS3004Regs.sRB5[index];
			standbyTAS3004Regs.sRB6[index] = shadowTAS3004Regs.sRB6[index];
		}

		for( index = 0; index < kTAS3004DRCwidth; index++ ) {
			standbyTAS3004Regs.sDRC[index] = shadowTAS3004Regs.sDRC[index];
		}
		
		for ( index = kTAS3004LeftBiquad0CtrlReg; index < kTAS3004LeftBiquad6CtrlReg; index++ ) {
			err = CODEC_WriteRegister( index, &data[3], kUPDATE_ALL );
			FailMessage ( kIOReturnSuccess != err );
			err = CODEC_WriteRegister( index + ( kTAS3004RightBiquad0CtrlReg - kTAS3004LeftBiquad0CtrlReg ), &data[3], kUPDATE_ALL );
			FailMessage ( kIOReturnSuccess != err );
		}
	
		setDRCProcessing ( NULL, FALSE );
		
		err = CODEC_WriteRegister( kTAS3004LeftBiquad6CtrlReg, &data[3], kUPDATE_ALL );
		FailMessage ( kIOReturnSuccess != err );
		err = CODEC_WriteRegister( kTAS3004RightBiquad6CtrlReg, &data[0], kUPDATE_ALL );
		FailMessage ( kIOReturnSuccess != err );
	
		if (FALSE == inRealtime) {	
			//	[3454015]  If hardware was not muted then restore the volume setting.
			InitEQSerialMode ( kSetNormalLoadMode );			//	and resume running the DSP
			IODelay ( 10000 );
			err = SetAnalogPowerDownMode (kPowerNormalAnalog);
			FailMessage ( kIOReturnSuccess != err );
			IODelay ( 10000 );
			if ( !mAnalogMuteState ) {
				SetVolumeCoefficients (volumeTable[(UInt32)mVolLeft], volumeTable[(UInt32)mVolRight]);
			}
		}
		
		mEQDisabled = TRUE;
	}
	//	[3280002]	} end

	debugIOLog (3,  "- AppleTAS3004Audio::disableProcessing" );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTAS3004Audio::enableProcessing (void) {
	UInt8			mcr2Data[kTAS3004DRCwidth];
	UInt32			index;
	UInt8			data[kTAS3004BIQwidth*2];
	IOReturn		err = kIOReturnSuccess;

	debugIOLog (3,  "+ AppleTAS3004Audio::enableEQ" );
	
	//	[3454015]	Removed all redundant transactions to set the parametric EQ filter
	//	mode to normal (not necessary since the filters are now always on to deal with
	//	fixing the phase shift problem between right and left channels).  Added code
	//	to mute the codec, delay for mute to take place prior to setting the filter 
	//	coefficients.  After setting the filter coefficients, the mute state is restored.  
	
	SetVolumeCoefficients (0, 0);						//	mute the amplifier
	IODelay ( 70000 );									//	delay to allow mute to take place (2400 samples max)
	err = SetAnalogPowerDownMode (kPowerDownAnalog);			//	[3455140]
	FailMessage ( kIOReturnSuccess != err );
	IODelay ( 10000 );
	InitEQSerialMode ( kSetFastLoadMode );				//	and pause the DSP

	//	[3280002]	begin {
	
	for( index = 0; index < sizeof ( data ); index++ ) { data[index] = 0x00; }
	data[3] = 0x10;
	
	err = CODEC_WriteRegister( kTAS3004LeftBiquad0CtrlReg, standbyTAS3004Regs.sLB0, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad1CtrlReg, standbyTAS3004Regs.sLB1, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad2CtrlReg, standbyTAS3004Regs.sLB2, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad3CtrlReg, standbyTAS3004Regs.sLB3, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad4CtrlReg, standbyTAS3004Regs.sLB4, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad5CtrlReg, standbyTAS3004Regs.sLB5, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004LeftBiquad6CtrlReg, &data[3], kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	
	err = CODEC_WriteRegister( kTAS3004RightBiquad0CtrlReg, standbyTAS3004Regs.sRB0, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad1CtrlReg, standbyTAS3004Regs.sRB1, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad2CtrlReg, standbyTAS3004Regs.sRB2, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad3CtrlReg, standbyTAS3004Regs.sRB3, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad4CtrlReg, standbyTAS3004Regs.sRB4, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad5CtrlReg, standbyTAS3004Regs.sRB5, kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	err = CODEC_WriteRegister( kTAS3004RightBiquad6CtrlReg, &data[0], kUPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );
	
	//	[3280002]	} end

	// read out of the standby regs and turn on only if needed
	for( index = 0; index < kTAS3004DRCwidth; index++ ) {
		mcr2Data[index] = standbyTAS3004Regs.sDRC[index];
	}
	err = CODEC_WriteRegister( kTAS3004DynamicRangeCtrlReg, mcr2Data, kFORCE_UPDATE_ALL );
	FailMessage ( kIOReturnSuccess != err );

	//	[3454015]  If hardware was not muted then restore the volume setting.
	InitEQSerialMode ( kSetNormalLoadMode );			//	and resume running the DSP
	IODelay ( 10000 );
	err = SetAnalogPowerDownMode (kPowerNormalAnalog);		//	[3455140]
	FailMessage ( kIOReturnSuccess != err );
	IODelay ( 10000 );
	if ( !mAnalogMuteState ) {
		SetVolumeCoefficients (volumeTable[(UInt32)mVolLeft], volumeTable[(UInt32)mVolRight]);
	}

	mEQDisabled = FALSE;

	debugIOLog (3,  "- AppleTAS3004Audio::enableEQ" );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This function does not utilize fast mode loading as to do so would
//	revert all biquad coefficients not addressed by this execution instance to
//	unity all pass.	 Expect DSP processing delay if this function is used.	It
//	is recommended that SndHWSetOutputBiquadGroup be used instead.  THIS FUNCTION
//	WILL NOT ENABLE THE FILTERS.  DO NOT EXPORT THIS INTERFACE!!!
IOReturn AppleTAS3004Audio::SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients )
{
	IOReturn		err;
	UInt32			coefficientIndex;
	UInt32			TAS3004BiquadIndex;
	UInt32			biquadGroupIndex;
	UInt8			TAS3004Biquad[kTAS3004CoefficientsPerBiquad * kTAS3004NumBiquads];
	
	err = kIOReturnSuccess;
	FailWithAction( kTAS3004MaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = kIOReturnError, Exit );
	FailWithAction( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = kIOReturnError, Exit );
	
	TAS3004BiquadIndex = 0;
	biquadGroupIndex = biquadRefNum * kTAS3004CoefficientsPerBiquad;
	if( kStreamFrontRight == streamID )
		biquadGroupIndex += kNumberOfTAS3004BiquadCoefficientsPerChannel;

	for( coefficientIndex = 0; coefficientIndex < kTAS3004CoefficientsPerBiquad; coefficientIndex++ )
	{
		TAS3004Biquad[TAS3004BiquadIndex++] = biquadCoefficients[coefficientIndex].integerAndFraction1;
		TAS3004Biquad[TAS3004BiquadIndex++] = biquadCoefficients[coefficientIndex].fraction2;
		TAS3004Biquad[TAS3004BiquadIndex++] = biquadCoefficients[coefficientIndex].fraction3;
	}
	
	err = SetOutputBiquadCoefficients( streamID, biquadRefNum, TAS3004Biquad );

Exit:
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTAS3004Audio::SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients )
{
	UInt32			index;
	IOReturn		err;

	FailWithAction( 0 == biquadFilterCount || NULL == biquadCoefficients, err = kIOReturnError, Exit );
	err = kIOReturnSuccess;
	
	InitEQSerialMode( kSetFastLoadMode );							//	pause the DSP while loading coefficients
	
	index = 0;
	do {
		if( index >= ( biquadFilterCount / 2 ) ) {
			err = SndHWSetOutputBiquad( kStreamFrontRight, index - ( biquadFilterCount / 2 ), biquadCoefficients );
		} else {
			err = SndHWSetOutputBiquad( kStreamFrontLeft, index, biquadCoefficients );
		}
		index++;
		biquadCoefficients += kNumberOfCoefficientsPerBiquad;
	} while ( ( index < biquadFilterCount ) && ( kIOReturnSuccess == err ) );
	
	InitEQSerialMode( kSetNormalLoadMode );							//	enable the DSP

Exit:
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTAS3004Audio::SetOutputBiquadCoefficients( UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients )
{
	UInt32				index;
	UInt8				data[kTAS3004BIQwidth*2];
	IOReturn			err;
	
	err = kIOReturnSuccess;
		
	FailWithAction ( kTAS3004MaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = kIOReturnError, Exit );
	FailWithAction ( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = kIOReturnError, Exit );

	switch ( biquadRefNum )
	{
		case kBiquadRefNum_0:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = CODEC_WriteRegister( kTAS3004LeftBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = CODEC_WriteRegister( kTAS3004RightBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = CODEC_WriteRegister( kTAS3004LeftBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );
										FailMessage ( kIOReturnSuccess != err );
										err = CODEC_WriteRegister( kTAS3004RightBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_1:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = CODEC_WriteRegister( kTAS3004LeftBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = CODEC_WriteRegister( kTAS3004RightBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = CODEC_WriteRegister( kTAS3004LeftBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );
										FailMessage ( kIOReturnSuccess != err );
										err = CODEC_WriteRegister( kTAS3004RightBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_2:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = CODEC_WriteRegister( kTAS3004LeftBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = CODEC_WriteRegister( kTAS3004RightBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = CODEC_WriteRegister( kTAS3004LeftBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );
										FailMessage ( kIOReturnSuccess != err );
										err = CODEC_WriteRegister( kTAS3004RightBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_3:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = CODEC_WriteRegister( kTAS3004LeftBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = CODEC_WriteRegister( kTAS3004RightBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = CODEC_WriteRegister( kTAS3004LeftBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );
										FailMessage ( kIOReturnSuccess != err );
										err = CODEC_WriteRegister( kTAS3004RightBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_4:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = CODEC_WriteRegister( kTAS3004LeftBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = CODEC_WriteRegister( kTAS3004RightBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = CODEC_WriteRegister( kTAS3004LeftBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );
										FailMessage ( kIOReturnSuccess != err );
										err = CODEC_WriteRegister( kTAS3004RightBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_5:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = CODEC_WriteRegister( kTAS3004LeftBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = CODEC_WriteRegister( kTAS3004RightBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = CODEC_WriteRegister( kTAS3004LeftBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );
										FailMessage ( kIOReturnSuccess != err );
										err = CODEC_WriteRegister( kTAS3004RightBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		//	NOTE:	Biquad #6 is reserved for delaying the right channel by one sample to resolve issues
		//			associated with radar [3280002].  Passing the address of 'data[3]' when setting the
		//			filter coefficients will apply a unity all pass filter coefficient set.  Passing the
		//			address of 'data[0]' when setting the filter coefficients will apply a one sample
		//			delayed unity all pass filter coefficient set.  The right channel is always delayed
		//			by one sample while the left channel is not delayed.
		case kBiquadRefNum_6:
			// hardware phase correction
			for( index = 0; index < sizeof ( data ); index++ ) { data[index] = 0x00; }
			data[3] = 0x10;
			switch( streamID )
			{
				case kStreamFrontLeft:
				case kStreamFrontRight:
				case kStreamStereo:		err = CODEC_WriteRegister( kTAS3004LeftBiquad6CtrlReg, &data[3], kUPDATE_ALL );
										FailMessage ( kIOReturnSuccess != err );
										err = CODEC_WriteRegister( kTAS3004RightBiquad6CtrlReg, &data[0], kUPDATE_ALL );	break;
			}
			break;
	}
	FailMessage ( kIOReturnSuccess != err );
Exit:
	debugIOLog (3, "± AppleTAS3004Audio::SetOutputBiquadCoefficients (%4s, %ld,%p) returns %x", (char*)&streamID, biquadRefNum, biquadCoefficients, err);
	return err;
}

#pragma mark ---------------------
#pragma mark ¥ USER CLIENT SUPPORT
#pragma mark ---------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTAS3004Audio::getPluginState ( HardwarePluginDescriptorPtr outState ) {
	IOReturn		result = kIOReturnBadArgument;
	UInt8 *			shadowRegs;
	
	FailIf ( NULL == outState, Exit );
	outState->hardwarePluginType = getPluginType();
	outState->registerCacheSize = sizeof ( shadowTAS3004Regs );
	shadowRegs = (UInt8*)&shadowTAS3004Regs.sMC1R;
	for ( UInt32 registerAddress = 0; registerAddress < outState->registerCacheSize; registerAddress++ ) {
		outState->registerCache[registerAddress] = shadowRegs[registerAddress];
	}
	outState->recoveryRequest = 0;
	result = kIOReturnSuccess;
Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Compares the register cache being passed in via 'inState' to the register
//	cache maintained by the driver.  If a miscompare results then writes the
//	register cache passed in via 'inState' to the target register that has a
//	different value than the current driver maintained register cache.
IOReturn AppleTAS3004Audio::setPluginState ( HardwarePluginDescriptorPtr inState ) {
	TAS3004_ShadowReg *	newRegValues;
	UInt8 *				shadowPtr;
	UInt8 *				inStateShadowPtr;
	UInt8				registerSize;
	UInt8				outRegisterSize;
	IOReturn			result;
	UInt8				regAddrList[] = {	kTAS3004MainCtrl1Reg,
											kTAS3004DynamicRangeCtrlReg,
											kTAS3004VolumeCtrlReg,
											kTAS3004TrebleCtrlReg,
											kTAS3004BassCtrlReg,
											kTAS3004MixerLeftGainReg,
											kTAS3004MixerRightGainReg,
											kTAS3004LeftBiquad0CtrlReg,
											kTAS3004LeftBiquad1CtrlReg,
											kTAS3004LeftBiquad2CtrlReg,
											kTAS3004LeftBiquad3CtrlReg,
											kTAS3004LeftBiquad4CtrlReg,
											kTAS3004LeftBiquad5CtrlReg,
											kTAS3004LeftBiquad6CtrlReg,
											kTAS3004RightBiquad0CtrlReg,
											kTAS3004RightBiquad1CtrlReg,
											kTAS3004RightBiquad2CtrlReg,
											kTAS3004RightBiquad3CtrlReg,
											kTAS3004RightBiquad4CtrlReg,
											kTAS3004RightBiquad5CtrlReg,
											kTAS3004RightBiquad6CtrlReg,
											kTAS3004LeftLoudnessBiquadReg,
											kTAS3004RightLoudnessBiquadReg,
											kTAS3004LeftLoudnessBiquadGainReg,
											kTAS3004RightLoudnessBiquadGainReg,
											kTAS3004AnalogControlReg,
											kTAS3004MainCtrl2Reg
										};
	
	result = kIOReturnBadArgument;
	FailIf ( NULL == inState, Exit );
	FailIf ( sizeof ( shadowTAS3004Regs ) != inState->registerCacheSize, Exit );
	result = kIOReturnSuccess;
	newRegValues = (TAS3004_ShadowReg*)&inState->registerCache;
	
	for ( UInt32 index = 0; ( index < ( sizeof ( regAddrList ) / sizeof ( UInt8 ) ) ) && ( kIOReturnSuccess == result ); index++ ) {
		if ( kIOReturnSuccess == GetShadowRegisterInfo ( &shadowTAS3004Regs, regAddrList[index], &shadowPtr, &registerSize ) ) {
			if ( kIOReturnSuccess == GetShadowRegisterInfo ( newRegValues, regAddrList[index], &inStateShadowPtr, &outRegisterSize ) ) {
				for ( UInt8 registerByteAddress = 0; registerByteAddress < registerSize; registerByteAddress++ ) {
					if ( inStateShadowPtr[registerByteAddress] != shadowPtr[registerByteAddress] ) {
						result = CODEC_WriteRegister ( regAddrList[index], inStateShadowPtr, kUPDATE_ALL );
						FailMessage ( kIOReturnSuccess != result );
						break;
					}
				}
			}
		}
	}
Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
HardwarePluginType	AppleTAS3004Audio::getPluginType ( void ) {
	return kCodec_TAS3004;
}








