/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 *   AWACs - Audio Waveform Amplifier and Converter
 *
 *   Notes:
 *
 *   1. All values written to (or read from) hardware must be endian reversed
 *      when using the constants from this file.
 *   2. The hardware uses a serial-bus time multiplexed scheme to communicate
 *      with up to four addressable devices (subframes). AWACs currently only uses
 *      'SubFrame 0'.  When a register field refers to a  subframe, then the SubFrame0
 *      value  should be used.
 *   3. The CodecControlRegister is used to as a pointer/data reg to access four 12-bit
 *      registers. The kCodecCtlAddress* values indicate the 12-bit register to be
 *      written and the kCodecCtlDataMask indicates the 12-bits to be written.
 *      kCodecCtlBusy is set/cleared by hardware when a pending value waiting to be
 *      written to one of the 12-bit registers. kCodecCtlEMSelect* indicates which
 *      SubFrame the data will go out on. (See Note 2)
 *  4.  The SoundControlRegister is used to set the data rate and indicates the input,
 *      output and status subframes. (See Note 2).
 *  5.  The CodecStatusRegister indicate when there was an error, or a change of input
 *      or output sources, i.e. a mic, line, or headphones were plugged in.
 *  6.  The ClippingCountRegister  is incremented on a left/right channel overflow. It
 *      is reset by reading its contents.
 *
 */

/*----------------------------- AWACs register mapping structure -------------------------*/

typedef UInt32                          SndIOSubFrame;

#define		kCodecCtlOffset			0x0010

enum AwacsCodecCtrl {
	kCtlDataMask			=	0x00000FFF,	// CODEC control data bits*/
	kCtlAddrMask			=	0x003FF000,	// CODEC control address bits*/

	kCtlEMSel0				=	0x00000000,	// Select subframe 0
	kCtlEMSel1				=	0x00400000,	// Select subframe 1
	kCtlEMSel2				=	0x00800000,	// Select subframe 2
	kCtlEMSel3				=	0x00C00000,	// Select subframe 4

	kCtlBusy				=	0x01000000	// AWACS busy bit
};

enum {
  kSndIOInputSubFrame0          = 0x0001, /* Four available input subframes on SndIO*/
  kSndIOInputSubFrame1          = 0x0002,
  kSndIOInputSubFrame2          = 0x0004,
  kSndIOInputSubFrame3          = 0x0008,
  kSndIOOutputSubFrame0         = 0x0100, /* Four available output subframes on SndIO*/
  kSndIOOutputSubFrame1         = 0x0200,
  kSndIOOutputSubFrame2         = 0x0400,
  kSndIOOutputSubFrame3         = 0x0800,
  kSndIOInputSubFrameMask       = 0x00FF,
  kSndIOOutputSubFrameMask      = 0xFF00,
  kSndIONumSubFrames            = 0x04
};

struct awacs_regmap_t
{
        volatile u_int32_t		       	SoundControlRegister;
        u_int32_t	       		       	pad0[3];
        volatile u_int32_t		       	CodecControlRegister;
        u_int32_t	       			pad1[3];
        volatile u_int32_t	       		CodecStatusRegister;
        u_int32_t	               		pad2[3];
        volatile u_int32_t	       		ClippingCountRegister;
        u_int32_t	       			pad3[3];
        volatile u_int32_t	       		ByteSwappingRegister;
};

/* Power state constants*/
enum {
  kSndHWRunState                = 0,    /* SndHW in the run state*/
  kSndHWDozeState               = 1,    /* SndHW in the doze state*/
  kSndHWIdleState               = 2,    /* SndHW in the idle state*/
  kSndHWSleepState              = 3,    /* SndHW in the sleep state*/
  kSndHWNotReadyState           = 4,    /* SndHW not ready (either in reset or wait state)*/
  kSndHWResetState              = 5,    /* SndHW reset (not a valid state to stay in)*/
  kSndHWPartDead                = 6,    /* SndHW is dead (can't get it out of reset)*/
  kSndHWInsomniaState           = 7,
  kSndHWWakeSubwoofer           = 8,
  kSndHWEnterSpeedSwitch        = 9,
  kSndHWExitSpeedSwitch         = 10
};

/*------------------------ AWACs CODEC Control register constants -----------------------*/

enum AWACsCODEC_ControlRegisterGeneralConstants
{
        kCodecCtlDataMask	=	0x00000FFF,	        /*CODEC control data bits*/

        kCodecCtlAddress0	=	0x00000000,		/*Select address 0*/
        kCodecCtlAddress1	=	0x00001000,		/*Select address 1*/
        kCodecCtlAddress2	=	0x00002000,		/*Select address 2*/
        kCodecCtlAddress4	=	0x00004000,		/*Select address 4*/
        kCodecCtlAddress5	=	0x00005000,		/*Select address 5 - Screamer only */
        kCodecCtlAddress6	=	0x00006000,		/*Select address 6 - Screamer only */
        kCodecCtlAddress7	=	0x00007000,		/*Select address 7 - Screamer only */

        kCodecCtlEMSelect0	=	0x00000000,		/*Select subframe 0*/
        kCodecCtlEMSelect1	=	0x00400000,		/*Select subframe 1*/
        kCodecCtlEMSelect2	=	0x00800000,		/*Select subframe 2*/
        kCodecCtlEMSelect4	=	0x00C00000,		/*Select subframe 4*/

        kCodecCtlBusy		=	0x01000000 		/*AWACS busy bit */
};

enum AWACsCODEC_ControlRegister0Constants
{
        kLeftInputGainMask		=	0x000000F0,	/*Bits used for left input gain*/
        kLeftInputGainShift		=	4,
        kRightInputGainMask		=	0x0000000F,	/*Bits used for right input gain*/
        kRightInputGainShift		=	0,

        kDefaultMicGain			=	0x000000CC,	/*Default right & left gain for a mic*/
        kDefaultCDGain			=	0x000000BB,	/*Default right & left gain for a CD*/

        kNotMicPreamp			=	0x00000100,	/*Select preamp on or off*/

        kInputMuxMask			=	0x00000E00,	/*Bits used to set which input to use */
        kCDInput		       	=	0x00000200,	/*Bit 9 = CD ROM audio input*/
        kMicInput		       	=	0x00000400,	/*Bit 10 = external mic input*/
        kUnusedInput			=	0x00000800,	/*Bit 11 = unused input*/
        kInitialAwacsR0Value    	=	0x00000000	/*Initial value set on boot*/
};


enum AWACsCODEC_ControlRegister1Constants
{
        kReservedR1Bit0			=	0x00000001,	/*Reserved - always set to zero*/
        kReservedR1Bit1			=	0x00000002,	/*Reserved - always set to zero*/
        kRecalibrate			=	0x00000004,	/*Recalibrate AWACs*/
        kExtraSampleRateMask    	=	0x00000038,	/*!!!Do these bits do anything???*/
        kLoopThruEnable			=	0x00000040,	/*Loop thru enable*/
        kMuteInternalSpeaker	        =	0x00000080,	/*Internal speaker mute*/
        kReservedR1Bit8			=	0x00000100,	/*Reserved - always set to zero*/
        kMuteHeadphone			=	0x00000200,	/*Headphone mute*/
        kOutputZero			=	0x00000400,
        kOutputOne			=	0x00000800,	/*Sometimes speaker enable*/
        kParallelOutputEnable		=	0x00000C00,	/*Parallel output port*/
        kInitialAwacsR1Value     	=	0x00000000	/*Initial value set on boot*/
};


enum AWACsCODEC_ControlRegister2Constants
{
        kRightHeadphoneAttenMask	=	0x0000000F,	/*Attenuation for right headphone speaker*/
        kRightHeadphoneAttenShift	=	0,		/*Bit shift from LSB position*/
        kReservedR2Bit4			=	0x00000010,	/*Reserved - always set to zero*/
        kReservedR2Bit5			=	0x00000020,	/*Reserved - always set to zero*/
        kLeftHeadphoneAttenMask 	=	0x000003C0,	/*Attenuation for left headphone speaker*/
        kLeftHeadphoneAttenShift	=	6,		/*Bit shift from LSB position*/
        kReservedR2Bit10		=	0x00000400,	/*Reserved - always set to zero*/
        kReservedR2Bit11		=	0x00000800,	/*Reserved - always set to zero*/
        kHeadphoneAttenMask		=	(kRightHeadphoneAttenMask | kLeftHeadphoneAttenMask),
        kInitialAwacsR2Value	       	=	0x00000000	/*Initial value set on boot*/
};


enum AWACsCODEC_ControlRegister4Constants
{
        kRightSpeakerAttenMask		=	0x0000000F,	/*Attenuation for right internal speaker*/
        kRightSpeakerAttenShift		=	0,	       	/*Bit shift from LSB position*/
        kReservedR4Bit4			=	0x00000010,	/*Reserved - always set to zero*/
        kReservedR4Bit5			=	0x00000020,	/*Reserved - always set to zero*/
        kLeftSpeakerAttenMask		=	0x000003C0,	/*Attenuation for left internal speaker*/
        kLeftSpeakerAttenShift		=	6,	       	/*Bit shift from LSB position*/
        kReservedR4Bit10		=	0x00000400,	/*Reserved - always set to zero*/
        kReservedR4Bit11		=	0x00000800,	/*Reserved - always set to zero*/
        kSpeakerAttenMask		=	(kRightSpeakerAttenMask | kLeftSpeakerAttenMask),
        kInitialAwacsR4Value		=	0x00000000	/*Initial value set on boot*/
};

/*---------------------- Screamer Extensions ----------------------------------------*/

enum AWACsVersions
{
        kAWACSMaxVersion		=	2
};

enum AWACsCODEC_ControlRegister5Constants
{
        kRightLoopThruAttenMask		=	0x0000000F,
        kRightLoopThruAttenShift	=	0,
        kLeftLoopThruAttenMask		=	0x000003C0,
        kLeftLoopThruAttenShift         =       6
};

enum AWACsCODEC_ControlRegister6Constants
{
        kPowerModeDoze			=	0x00000001,
        kPowerModeIdle			= 	0x00000002,
        kMicPreampBoostEnable		=	0x00000004,
        kPCMCIASpeakerAttenMask		=       0x00000038,
        kPowerModeAnalogShutdown	=       0x00000040,
        kLittleDischarge		=       0x00000080,
        kBigDischarge  			=       0x00000100
};

enum AWACsCODEC_ControlRegister7Constants
{
        kReadBackEnable			= 	0x00000001,
        kReadBackRegisterMask		= 	0x0000000E,
        kReadBackRegisterShift		=	1
};

/*----------------- AWACs CODEC Status Register constants -------------------------------*/

enum AWACsCODEC_StatusRegisterConstants
{
        kNotMicSense	        	=	0x00000001,	/*This bit is 0 when a mic(input) is plugged in*/
        kNotMicShift			=	0,
        kLineInSense	          	=	0x00000002,	/*This bit is 1, when a line(input) is pluggen in*/
        kLineInShft			=	1,
        kAux1Sense			=       0x00000004,     /*This bit is 1, when something is plugged into the Whisper line-out */
        kAux1Shft			=	2,
        kHeadphoneSense	        	=	0x00000008,	/*This bit is 1 when headphone is plugged in*/
        kHeadphoneShft			=	3,
        kAllSense			= 	0x0000000F,
        kManufacturerIDMask	        =	0x00000F00,	/*AWACS chip manufacturer ID bits*/
        kManufacturerIDShft	        =	8,	       	/*Bits to shift right to get ID in LSB position*/
        kRevisionNumberMask	        =	0x0000F000,	/*AWACS chip revision bits*/
        kRevisionNumberShft     	=	12,	       	/*Bits to shift right to get rev in LSB position*/
        kAwacsErrorStatus	      	=	0x000F0000,	/*AWACS error status bits*/
        kOverflowRight                  =       0x00100000,
        kOverflowLeft                   =       0x00200000,
        kValidData                      =       0x00400000,
        kExtend                         =       0x00800000
};

/*--------------------- AWACs Sound Control register ------------------------------------*/

enum AWACsSoundControlRegisterConstants
{
        kInSubFrameMask			=	0x0000000F,	/*All of the input subframe bits*/
        kInSubFrame0			=	0x00000001,	/*Input subframe 0*/
        kInSubFrame1			=	0x00000002,	/*Input subframe 1*/
        kInSubFrame2			=	0x00000004,	/*Input subframe 2*/
        kInSubFrame3			=	0x00000008,	/*Input subframe 3*/

        kOutSubFrameMask		= 	0x000000F0, 	/*All of the output subframe bits*/
        kOutSubFrame0			=	0x00000010,	/*Output subframe 0*/
        kOutSubFrame1			=	0x00000020,	/*Output subframe 1*/
        kOutSubFrame2			=	0x00000040,	/*Output subframe 2*/
        kOutSubFrame3			=	0x00000080, 	/*Output subframe 3*/

        kHWRateMask	       		=	0x00000700,	/*All of the hardware sampling rate bits*/
        kHWRate44100			=	0x00000000,	/*Hardware sampling bits for 44100 Hz*/
        kHWRate29400			=	0x00000100,	/*Hardware sampling bits for 29400 Hz*/
        kHWRate22050			=	0x00000200,	/*Hardware sampling bits for 22050 Hz*/
        kHWRate17640			=	0x00000300,	/*Hardware sampling bits for 17640 Hz*/
        kHWRate14700			=	0x00000400,	/*Hardware sampling bits for 14700 Hz*/
        kHWRate11025			=	0x00000500,	/*Hardware sampling bits for 11025 Hz*/
        kHWRate08820			=	0x00000600,	/*Hardware sampling bits for  8820 Hz*/
        kHWRate07350			=	0x00000700,	/*Hardware sampling bits for  7350 Hz*/

        kHWRateShift                    =       8,

        kAWACSError	       	 	=	0x00000800,	/*AWACs error indicator*/
        kPortChange	       	 	=	0x00001000,	/*Port change indicator*/
        kEnableErrInt		 	=	0x00002000,	/*Interrupt on error enable*/
        kEnablePortChangeInt 	        =	0x00004000,	/*Interrupt on port change enable*/

        kStatusSubFrmSel0	 	=	0x00000000,	/*Select subframe zero  status*/
        kStatusSubFrmSel1 	 	=	0x00008000,	/*Select subframe one   status*/
        kStatusSubFrmSel2 	 	=	0x00010000,	/*Select subframe twoo  status*/
        kStatusSubFrmSel3 	 	=	0x00018000	/*Select subframe three status*/
};

/*--------------------- AWACs Clipping Count register ------------------------------------*/

enum AWACsClippingCountRegisterConstants
{
       kRightClippingCount              =       0x000000FF,
       kLeftClippingCount               =       0x0000FF00
};


/*-------------------- Awacs definition from the OS 9 version ---------------------------*/
enum {
	kAWACsInputReg			=	0x00000000,		// register for input port selection
	kAWACsInputA			= 	0x00000800,		// input A enable and mask
	kAWACsInputB			=	0x00000400,		// input B enable and mask
	kAWACsInputC			=	0x00000200,		// input C enable and mask
	kAWACsInputField		=	kAWACsInputA | kAWACsInputB | kAWACsInputC,	// mask to get the entire input mux field
	
	kAWACsPreampBReg		=	0x00000000,		// register for input B preamp
	kAWACsPreampB			=	0x00000100,		// input B preamp enable and mask

	kAWACsGainReg			=	0x00000000,		// register for gain settings
	kAWACsGainLeft			=	0x000000F0, 		// mask to get left gain
	kAWACsGainLeftShift		=	0x00000004,		// shift for left gain
	kAWACsGainRight			=	0x0000000F,		// mask to get right gain
	kAWACsGainField			=	kAWACsGainLeft | kAWACsGainRight,	// mask to get the entire gain field

	kAWACsProgOutputReg		=	0x00000001,		// register for programmable output settings
	kAWACsOutputZero		=	0x00000400,		// hit output zero
	kAWACsOutputOne			=	0x00000800,		// hit output one
	kAWACsProgOutputField		=	kAWACsOutputZero | kAWACsOutputOne,	// mask to get the entire programmable output field
	kAWACsProgOutputShift		=	10,			// shift for programmable output bits
	
	kAWACsMuteReg			=	0x00000001,		// register for muting outputs
	kAWACsMuteOutputA		=	0x00000200,		// mute output A
	kAWACsMuteOutputC		=	0x00000080,		// mute output C
	kAWACsMuteField			=	kAWACsMuteOutputA | kAWACsMuteOutputC,	// mask to get the entire mute field
	
	kAWACsLoopThruReg		=	0x00000001,		// register for setting loop-through
	kAWACsLoopThruEnable		=	0x00000040,		// enable loop-through mode (playthrough)
	
	kAWACsRecalibrateReg		=	0x00000001,		// register to force a recalibration
	kAWACsRecalibrate		=	0x00000004,		// recalibrate
	
	kAWACsOutputAAttenReg		=	0x00000002,		// register for output A attenuation
	kAWACsOutputCAttenReg		=	0x00000004,		// register for output A attenuation	
	kAWACsOutputLeftAtten		=	0x000003C0,		// output left attenuation
	kAWACsOutputLeftShift		=	0x00000006,		// shift for left attenuation
	kAWACsOutputRightAtten		=	0x0000000F,		// output right attenuation
	kAWACsOutputAttenField		=	kAWACsOutputLeftAtten | kAWACsOutputRightAtten,	// mask to get entire output attenuation field
	
	kAWACsPlayThruAttenReg		=	0x00000005,		// playthrough attenuation register (not used in meaningful way)
	
	kAWACsPowerReg			=	0x00000006,		// register for power handling
	kAWACsSettlingTimeField		=	0x00000180,		// settling time bits
	kAWACsSettlingTimeLong		=	0x00000100,		// big discharge bit
	kAWACsSettlingTimeShort		=	0x00000080,		// little discharge bit
	kAWACsAnalogShutdown		=	0x00000040,		// analog shutdown bit
	
	kAWACsPCMCIAAttenReg		=	0x00000006,		// register for PCMCIA volume
	kAWACsPCMCIAAttenField		=	0x00000038,		// the entire field
	kAWACsPCMCIAOn			=	0x00000008,		// PCMCIA on
	kAWACsPCMCIAOff			=	0x00000000,		// PCMCIA off
	
	kAWACsPreampAReg		=	0x00000006,		// register for input A preamp
	kAWACsPreampA			= 	0x00000004,		// input A preamp enable and mask
	
	kAWACsPowerStateField		=	0x00000003,		// state bits on Screamer
	kAWACsPowerStateIdle		=	0x00000002,		// Screamer idle bit
	kAWACsPowerStateDoze		=	0x00000001,		// Screamer doze bit
	kAWACsPowerStateRun		=	0x00000000,		// Screamer run setting
	
	kAWACsMinHardwareGain		=	0,
	kAWACsMaxHardwareGain		=	15,
	kAWACsMaxHardwareGainPreAmp	=	31,
	kAWACsHWdBStepSize		=	0x00018000,		// Fixed 1.5
	
	kAWACsMinVolume			=	15,
	kAWACsInvalidVolumeMask		=	0xFE00FE00,
	kAWACsMaxVolumeA		=	2,
	kAWACsMaxVolumeC		=	0
};

// AWACS CODEC control register general equates
enum AwacsSndHWCtrl {
	kAWACsCtlAddressMask		=		0x003FF000,	// Mask off the address field of the control register
	kAWACsCtlAddressShift		=		12,		// Shift to place address in address field

	kAWACsReadbackRegister		=		7,		// register to do hardware readback
	kAWACsReadbackRegAddrMask	=		0x0000000E,	// mask to isolate the readback register address
	kAWACsReadbackRegAddrShift	=		1,		// shift to place address in proper field
	kAWACsReadbackEnable		=		0x00000001,	// set/clear readback enable
	kAWACsReadbackMask		=		0x0000000F,	// mask to isolate the readback register address and enable bits
	kAWACsReadbackDataShift		=		4,		// shift to place return in the right place
	kAWACsReadbackDataMask		=		0x00000FFF	// mask to isolate the nibbles of the returned data
};

// AWACS CODEC status register equates
enum AwacsSndHWStatus {
	kAWACsManufacturerIDMask	=	0x00000F00,		// AWACS chip manufacturer ID bits
	kAWACsManfCrystal		=	0x00000100,		// crystal part
	kAWACsManfNational		=	0x00000200,		// national part
	kAWACsManfTI			=	0x00000300,		// texas instruments part
	
	kAWACsRevisionNumberMask	=	0x0000F000,			// AWACS chip revision bits
	kAWACsRevisionShift		=	12,			// Shift to get the version number
	kAWACsValidData			=	0x00400000,		// AWACS has valid data
	kAWACsRecalTimeOut		=	1000,			// Number of milliseconds to wait before timing out a recalibrate

	kAWACsStatusMask		=	0x00FFFFFF,		// AWACS chip status bits
        kAWACsStatusInSenseMask		=	0x0000000F,
	kAWACsAwacsRevision		=	2,			// AWACs revision
	kAWACsScreamerRevision		=	3,			// Screamer revision
	
	kAWACsInSenseMask		=	0x0000000F,		// mask for status bits
	kAWACsInSense0			=	0x00000008,		// in sense bit 0
	kAWACsInSense1			=	0x00000004,		// in sense bit 1
	kAWACsInSense2			=	0x00000002,		// in sense bit 2
	kAWACsInSense3			=	0x00000001,		// in sense bit 3

	// screamer specific hardware bit states
	kScreamerRunState1		=	0,			// idle and doze clear, run state one
	kScreamerRunState2		=	3,			// idle and doze set, run state two
	kScreamerDozeState		=	1,			// hardware doze bit set
	kScreamerIdleState		=	2,			// hardware idle bit set
	kScreamerStateField		=	3			// field containing state bits
};


enum GeneralHardware {
	kSampleRatesCount		=	3,

	kMaxPCMCIAVolume		=	7,
	kMinPCMCIAVolume		=	0,

	kInvalidGainMask		= 	0xFFFE0000,	// mask to check gain settings against
	kMinSoftwareGain		=	0x8000,		// 0.5 setting for the software API
	kMaxSoftwareGain		=	0x18000,	// 1.5 setting for the software API
	kUnitySoftwareGain		=	0x10000,	// 1.0 setting for the software API
	kUnitySoftwareBass		=	0x10000,	// 1.0 setting for the software API
	kUnitySoftwareTreble	=	0x10000,		// 1.0 setting for the software API

	kMaxSndHWRegisters		= 	8,		// Number of registers in sndHW (8 in Screamer)
	kInvalidRegNumber		=	3,		// register 3 is not a valid register to read or write
	
	kMaxAsyncEvents			=	384,		// most async events that we allow
	kAsyncEventDelay		=	1,		// delay event, data2 = delay time
	kAsyncEventSetReg		=	2,		// set register contents, data1 = async, data2 = contents to write
	kAsyncEventWaitReady		=	3,		// wait for sound hardware ready, data2 = number of milliseconds to try
	kAsyncEventWaitIdle		=	4,		// wait for idle hardware, data2 = number of milliseconds to try
	kAsyncEventWakeDMA		=	5,		// call SndHWDMAWakeUp, no params
	kAsyncEventCallback		=	6,		// perform a callback, data2 = refCon1, data3 = refCon2, data4 = SndHWAsyncCallbackUPP
	
	kByteMask			=	0x00FF,		// mask to get a byte
	kAsyncWaitTime			=	1,		// number of milliseconds allowed for async
	kMicroPerMilli			=	1000,		// number of micro whatevers in one milli whatever
		
	kMaxHWAtten			=	0xF,		// maximum attenuation on the hardware
	kNumVolumeSteps			=	16,		// 16 volume steps on the hardware
	kMaxHWAttenLR			=	0x3CF,		// maximum hardware attenuation for both channels
	kMinHWGain			=	0,		// minimum hardware gain
	kPreRecalDelayCrystal		=	750,		// number of milliseconds to delay before setting recalibrate with a Crystal part 'cause it's broken
	kPreRecalDelayNational		=	0,		// number of milliseconds to delay before setting recalibrate with a National part 'cause it's not broken
	kRecalibrateDelay		=	2,		// number of milliseconds to delay after setting recalibrate
	kRampDelayCrystal		=	10,		// number of milliseconds to delay after each step of the attenuation ramp
	kRampDelayNational		=	0,		// number of milliseconds to delay after each step of the attenuation ramp

	kIdleRunDelay			=	5,	// idle <-> run transition delay milliseconds i
	kIdleDozeDelay			=	5,	// idle <-> doze transition delay milliseconds
	kDozeRunDelay			=	0		// doze <-> run transition delay milliseconds (the spec says 5ms, but that's audible - keep it at zero and it works OK but is no longer audible)
};


//some constants

#define	kScreamerAttenStep			1.5
#define kScreamerMaxVolume			0.0
#define kScreamerMinVolume			-22.5
#define kScreamerVolumeRange		(kScreamerMaxVolume - kScreamerMinVolume)
#define	kMAX_SW_VOL					256.0