 /*
 *  AudioHardwareConstants.h
 *  Apple02Audio
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
 *  Contains a lot of constants used across the Apple02Audio project.
 *  There are three kind of constants : 
 *      - the string equivalent used mainly to parse the Open Firmware 
 *        device tree. Eventually they will move to the header file
 *        of a OF device tree parser objects.
 *      - A serie of enum representing the abstracted input, output numbers
 *        for any chip. This is enough to cover hardware Codec with 6 or 7
 *        input and outputs, which we will never have...
 *      - A enumeration for device kinds. We may extend it for Apple Speakers
 *        All devices are bit exclusives
 *      - An enumeration for all kind codec.
 */

#ifndef __AUDIOHARDWARECONSTANT__
#define __AUDIOHARDWARECONSTANT__

#define kSoundEntryName	"sound"

// Sound Entry
#define kSoundObjectsPropName		"sound-objects"

#define kAAPLAddress				"AAPL,address"
#define kGPIODTEntry				"gpio"

#define kInputObjEntryName			"input"
#define kOutputObjEntryName			"output"
#define kDetectObjEntryName			"detect"
#define kFeatureObjEntryName		"feature"
#define kInitObjEntryName			"init"
#define kMuxObjEntryName			"mux"
        
#define kNumDetectsPropName			"#-detects"
#define kNumFeaturesPropName		"#-features"
#define kNumOutputsPropName			"#-outputs"
#define kNumInputsPropName			"#-inputs"
#define kModelPropName				"model"

#define kAnyInDetectObjName			"AnyInSenseBitsDetect"
#define kInSenseBitsDetectObjName	"InSenseBitsDetect"	
#define kGPIODetectObjName			"GPIODetect"
#define kGPIOGenericDetectObjName	"GPIOGenericDetect"
#define kGPIOPrioritizedDetectObjName	"GPIOPrioritizedDetect"

#define kBitMaskPropName			"bit-mask"
#define kBitMatchPropName			"bit-match"
#define kDevicePropName				"device"
#define kDeviceIDPropName			"device-id"
#define kDeviceMaskPropName			"device-mask"
#define kDeviceMatchPropName		"device-match"
#define kDeviceTypePropName			"device_type"
#define	kHasANDedResetPropName		"has-anded-reset"
#define kIconIDPropName         	"icon-id"
#define kPortChannelsPropName   	"port-channels"
#define kPortConnectionPropName 	"port-connection"
#define kPortTypePropName       	"port-type"
#define kNameIDPropName         	"name-id"
#define kZeroGainPropName         	"zero-gain"	/* aml 4.26.02	*/

#define kOutputPortObjName			"OutputPort"
#define kOutputEQPortObjName		"OutputEQPort"
#define kOutputDallasEQPortObjName	"OutputDallasEQPort"
#define	kOutputMonoEQPortObjName	"OutputMonoEQPort"
#define kGazIntSpeakerObjName		"Proj1Speaker"
#define kGazSubwooferObjName		"Proj2Speaker"
#define kWSIntSpeakerObjName		"Proj3Speaker"
#define kGossSGSToneOutObjName		"Proj4Speaker"
#define kKiheiSpeakerObjName		"Proj5Speaker"

#define kAWACsModelName				"343S0140"
#define kScreamerModelName			"343S0184"
#define kBurgundyModelName			"343S0177"
#define kDacaModelName				"353S0228"
#define kTexasModelName				"355S0056"
#define	kTexas2ModelName			"353S0303"

#define kMuxPSurgeObjName 			"Proj1Mux"        
#define kMuxAlchemyObjName   		"Proj2Mux"    
#define kMuxHooperObjName    		"Proj3Mux"
#define kMuxPExpressObjName			"Proj4Mux"    
#define kMuxWSObjName         		"Proj5Mux"
#define kMuxGossWingsAObjName  		"Proj6Mux" 
#define kMuxGossWingsBObjName   	"Proj7Mux"
#define kMuxGossCanardObjName   	"Proj8Mux"
#define kMux101ObjName          	"Proj9Mux"
#define kMuxProgOutName         	"MuxProgOut" 

#define kSourceMapPropName      	"source-map"
#define kSourceMapCountPropName 	"source-map-count"

#ifndef	kDeviceFamilySpeaker
#define	kDeviceFamilySpeaker		1
#endif

typedef enum {
	kInternalSpeakerStatus			= 0,
	kHeadphoneStatus,
	kExtSpeakersStatus,
	kLineOutStatus,
	kDigitalInStatus,
	kDigitalOutStatus,
	kLineInStatus,
	kInputMicStatus,
	kExternalMicInStatus,
	kDigitalInInsertStatus,
	kDigitalInRemoveStatus,
	kRequestCodecRecoveryStatus,
	kClockInterruptedRecovery,
	kClockLockStatus,
	kClockUnLockStatus,
	kAES3StreamErrorStatus,
	kCodecErrorInterruptStatus,
	kCodecInterruptStatus,
	kBreakClockSelect,
	kMakeClockSelect,
	kSetSampleRate,
	kSetSampleBitDepth,
	kPowerStateChange,
	kPreDMAEngineInit,
	kPostDMAEngineInit,
	kRestartTransport,
	kRunPollTask,
	kSetSampleType,
	kSetMuteState,
	kSetAnalogMuteState,										//	[3435307]	
	kSetDigitalMuteState,										//	[3435307]	
	kNumberOfActionSelectors					//	ALWAYS MAKE THIS LAST!!!!
} ActionSelector;

//	================================================================================
//	Machine layout constants. All NewWorld machines have a device-id property
//	matching one of these layout constants for specifying the sound hardware layout.
//	Note that current allocation scheme is allocating an entry for both i2s-a and
//	i2s-c for each new CPU regardless whether both i2s cells are used.  Layout ID
//	label suffixes should include either 'a' or 'c' (not 'b' because i2s-b belongs
//	to the modem team).  The dual layout id allocation policy allows for marketing
//	to extend the feature set on a CPU.
enum {
	layoutC1					=	1,
	layout101					=	2,
	layoutG3					=	3,
	layoutYosemite				=	4,
	layoutSawtooth				=	5,
	layoutP1					=	6,
	layoutUSB					=	7,
	layoutKihei					= 	8,
	layoutDigitalCD				= 	9,
	layoutPismo					=	10,
	layoutPerigee				= 	11,
	layoutVirtual				=	12,
	layoutMercury				=	13,
	layoutTangent				=	14,
	layoutTessera				=	15,
	layoutP29					=	16,
	layoutWallStreet			=   17,
	layoutP25					=	18,
	layoutP53					=	19,
	layoutP54					=	20,
	layoutP57					=	21,
	layoutP58					=	22,
	layoutP62					=	23,
	layoutP72					=	24,
	layoutP92					=	25,
	layoutP59					=	26,
	layoutP57b					=	27,
	layoutP73					=	28,
	layoutP79					=	29,
	layoutP84					=	30,
	layoutP99					=	31,
	layoutQ25					=	32,
	layoutQ26					=	33,
	layoutP86					=	34,
	layoutQ16					=	35,
	layoutQ37					=	36,
	layoutQ27					=	37,
	layoutP72D					=	38,
	layoutQ41					=	39,
	layoutQ54					=	40,
	layoutQ59					=	41,
};
//	See process comment above when adding layout ID values to the above enumerations!!!
//	================================================================================

// Hardware type 
enum{
    kUnknownHardwareType,
    kGCAwacsHardwareType,
    kBurgundyHardwareType,
    kDACAHardwareType,
    kTexas3001HardwareType,
    kTexas3004HardwareType
};

//	[3435307]	rbm
enum {
	kAnalogAudioSelector			=	'aaud',
	kDigitalAudioSelector			=	'daud'
};

// Kind of devices
enum AudioPortTypes {
    kSndHWInternalSpeaker		=	0x00000001,		// internal speaker present on CPU
    kSndHWCPUHeadphone			=	0x00000002,		// headphones inserted into CPU headphone port
    kSndHWCPUExternalSpeaker	=	0x00000004,		// external speakers (or headphones) inserted in CPU output port
    kSndHWCPUSubwoofer			=	0x00000008,		// subwoofer (software controllable) present
    kSndHWCPUMicrophone			=	0x00000010,		// short jack microphone (mic level) inserted in CPU input port
    kSndHWCPUPlainTalk			=	0x00000020,		// PlainTalk microphone inserted into CPU input port
    kSndHWMonitorHeadphone		=	0x00000040,		// headphones inserted in monitor headphone port
    kSndHWMonitorPlainTalk		=	0x00000080,		// PlainTalk source input inserted in sound input port (even though it may physically be a short plug)
    kSndHWModemRingDetect		=	0x00000100,		// modem ring detect
    kSndHWModemLineCurrent		=	0x00000200,		// modem line current
    kSndHWModemESquared			=	0x00000400,		// modem E squared
	kSndHWLineInput				=	0x00000800,		// line input device present
	kSndHWLineOutput			=	0x00001000,		// line output device present
	kSndHWDigitalOutput			=	0x00002000,		// digital output device present
	kSndHWDigitalInput			=	0x00004000,		// digital in device present
    kSndHWInputDevices			=	0x000040B0,		// mask to get input devices (excluding modems)
    kSndHWAllDevices			=	0xFFFFFFFF		// all available devices
};

// Codec kind
enum {
    kSndHWTypeUnknown			=	0x00000000,		// unknown part
    kSndHWTypeAWACs				=	0x00000001,		// AWACs part
    kSndHWTypeScreamer			=	0x00000002,		// Screamer part
    kSndHWTypeBurgundy			=	0x00000003,		// Burgundy
    kSndHWTypeUSB				=	0x00000004,		// USB codec on a wire...
    kSndHWTypeDaca				= 	0x00000005,		// DAC3550A
    kSndHWTypeDigitalSnd		=	0x00000006,		// DigitalSnd virtual HW
    kSndHWTypeTumbler			=	0x00000007,		// Texas I2S with Equalizer & Dallas ID thing...
	kSndHWTypeTexas2			=	0x00000008,		// Texas2 I2s with Equalizer & Dallas ID thing...
    kSndHWManfUnknown			=	0x00000000,		// unknown manufacturer (error during read)
    kSndHWManfCrystal			=	0x00000001,		// manufactured by crystal
    kSndHWManfNational			=	0x00000002,		// manufactured by national
    kSndHWManfTI				=	0x00000003,		// manufactured by texas instruments
    kSndHWManfMicronas			=	0x00000004		// manufactured by Micronas Intermetall
};


// Output port constants
enum {
	kSndHWOutput1				=	1,				// output 1
	kSndHWOutput2				=	2,				// output 2
	kSndHWOutput3				=	3,				// output 3
	kSndHWOutput4				=	4,				// output 4
	kSndHWOutput5				=	5,				// output 5
	kSndHWOutputNone			=	0				// no output
};


enum {
	kSndHWProgOutput0			=	0x00000001,		// programmable output zero
	kSndHWProgOutput1			=	0x00000002,		// programmable output one
	kSndHWProgOutput2			=	0x00000004,		// programmable output two
	kSndHWProgOutput3			=	0x00000008,		// programmable output three
	kSndHWProgOutput4			=	0x00000010		// programmable output four
};

// Input Port constants
enum {
	kSndHWInSenseNone			=	0x00000000,		// no input sense bits
	kSndHWInSense0				=	0x00000001,		// input sense bit zero
	kSndHWInSense1				=	0x00000002,		// input sense bit one
	kSndHWInSense2				=	0x00000004,		// input sense bit two
	kSndHWInSense3				=	0x00000008,		// input sense bit three
	kSndHWInSense4				=	0x00000010,		// input sense bit four
	kSndHWInSense5				=	0x00000020,		// input sense bit five
	kSndHWInSense6				=	0x00000040		// input sense bit six
};


enum {
  kSndHWInput1                  = 1,    /* input 1*/
  kSndHWInput2                  = 2,    /* input 2*/
  kSndHWInput3                  = 3,    /* input 3*/
  kSndHWInput4                  = 4,    /* input 4*/
  kSndHWInput5                  = 5,    /* input 5*/
  kSndHWInput6                  = 6,    /* input 6*/
  kSndHWInput7                  = 7,    /* input 7*/
  kSndHWInput8                  = 8,    /* input 8*/
  kSndHWInput9                  = 9,    /* input 9*/
  kSndHWInputNone               = 0     /* no input*/
};

//	WARNING:	Do not change the values of the following enumerations.  Changes to
//				the enumerations will cause the 'Audio Hardware Utility' to fail.
enum GpioAddressSelector {
	kHeadphoneMuteSel			=	'hmut',
	kHeadphoneDetecteSel		=	'hcon',
	kAmplifierMuteSel			=	'amut',
	kSpeakerIDSel				=	'dlas',
	kCodecResetSel				=	'rset',
	kLineInDetectSel			=	'ldet',
	kLineOutMuteSel				=	'lmut',
	kLineOutDetectSel			=	'lcon',
	kDigitalOutDetectSel		=	'digO',
	kComboOutJackTypeSel		=	'dgOT',
	kDigitalInDetectSel			=	'digI',
	kComboInJackTypeSel			=	'dgIT',
	kMasterMuteSel				=	'mstr',
	kInternalSpeakerIDSel		=	'ispi',
	kSpeakerDetectSel			=	'sdet'
};
//	END WARNING:

enum Hardware32RegisterSelectors {
    kI2sSerialFormatRegisterSelector	=	0,			/*	I2S0 bus implemenations				*/
    kI2sDataWordFormatRegisterSelector	=	1,			/*	I2S0 bus implemenations				*/
    kFeatureControlRegister1Selector	=	2,			/*	DAV bus & I2S bus implemenations	*/
    kFeatureControlRegister3Selector	=	3,			/*	DAV bus & I2S bus implemenations	*/
    kCodecControlRegisterSelector		=	4,			/*	DAV bus implemenations				*/
    kCodecStatusRegisterSelector		=	5,			/*	DAV bus implemenations				*/
    kI2s1SerialFormatRegisterSelector	=	6,			/*	I2S1 bus implemenations				*/
    kI2s1DataWordFormatRegisterSelector	=	7			/*	I2S1 bus implemenations				*/
};

enum CodecRegisterUserClientWidth {
	kMaxCodecStructureSize				=	512,		/*	Codec READ transactions copy the entire register cache in a single transaction!	*/
	kMaxCodecRegisterWidth				=	 16,		/*	Codec WRITE transactions address only a single register	*/
	kMaxBiquadWidth						=	512,
	kMaxBiquadInfoSize					=	256,
	kMaxProcessingParamSize				=	512			/*	used with getProcessingParams or setProcessingParams	*/
};

enum speakerIDBitAddresses {
	kHeadphone_Connected	=	25,
	kSpeakerID_Connected	=	24,
	kSpeakerID_Family		=	16,
	kSpeakerID_Type			=	 8,
	kSpeakerID_SubType		=	 0
};

// Shift value to identify the control IDs
#define DETECTSHIFT 1000
#define OUTPUTSHIFT 1100
#define INPUTSHIFT  1200


// PRAM read write values
enum{
    kMaximumPRAMVolume 	= 7,
    kMinimumPRAMVolume	= 0,
    KNumPramVolumeSteps	= (kMaximumPRAMVolume- kMinimumPRAMVolume+1),
    kPRamVolumeAddr		= 8,
    
    kDefaultVolume		= 0x006E006E,
    kInvalidVolumeMask	= 0xFE00FE00
    
};

typedef UInt32 sndHWDeviceSpec;

typedef struct {
	UInt32			numBiquad;
	UInt32			numCoefficientsPerBiquad;
	UInt32			biquadCoefficientBitWidth;
	UInt32			coefficientIntegerBitWidth;
	UInt32			coefficientFractionBitWidth;
	UInt32			coefficientOrder[1];			//	this array is of size 'numCoefficientsPerBiquad' and holds OSTypes describing the coefficient
} BiquadInfoList;
typedef BiquadInfoList * BiquadInfoListPtr;

enum {
	// Regular, actually addressable streams (i.e. data can be streamed to and/or from these).
	// As such, these streams can be used with get and set current stream map;
	// get and set relative volume min, max, and current; and 
	// get hardware stream map
	kStreamFrontLeft			= 'fntl',	// front left speaker
	kStreamFrontRight			= 'fntr',	// front right speaker
	kStreamSurroundLeft			= 'surl',	// surround left speaker
	kStreamSurroundRight		= 'surr',	// surround right speaker
	kStreamCenter				= 'cntr',	// center channel speaker
	kStreamLFE					= 'lfe ',	// low frequency effects speaker
	kStreamHeadphoneLeft		= 'hplf',	// left headphone
	kStreamHeadphoneRight		= 'hprt',	// right headphone
	kStreamLeftOfCenter			= 'loc ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[in front]
	kStreamRightOfCenter		= 'roc ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[in front]
	kStreamSurround				= 'sur ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[rear]
	kStreamSideLeft				= 'sidl',	//	see usb audio class spec. v1.0, section 3.7.2.3	[left wall]
	kStreamSideRight			= 'sidr',	//	see usb audio class spec. v1.0, section 3.7.2.3	[right wall]
	kStreamTop					= 'top ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[overhead]
	kStreamMono					= 'mono'	//	for usb devices with a spatial configuration of %0000000000000000
};

#endif
