/*
 *  AudioScreamerAudio.cpp (definition)
 *  Project : Apple02Audio
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  Hardware independent (relatively) code for the Awacs Controller
 *  NEW-WORLD MACHINE ONLY!!!
 *
 *  As of the release of OS X version 1, the AppleScreamerAudio driver has gone
 *  under major changes. These changes adress only NewWorld machines (the translucent
 *  ones).
 *  
 *  This work was undertaken in order to use the information present in the Open Firmware
 *  "sound" node, and that represent the hardware wiring. We can then avoid the multiple
 *  cases for different machines, and have a driver that autoconfigures itself. Another
 *  goal is to isolate the Codec related function, from the rest of the driver. A superclass
 *  for all Apple Hardware driver can then be extracted and reused when new hardware
 *  is coming. 
 *    
 *  For commodity, all functions have been defined inside the driver class. This can 
 *  obviously done in a better way, by grouping the functionnality and creating 
 *  appropriate objects.
 *  
 *  The list of hardware access functions is not restrictive. It is only sufficient
 *  enough to answer to the behavior asked by the UI guidelines of OS X version 1.
 *  As long as the hardware support it, it is our intention to different 
 *  UI policies, and have a wider flexibility. 
 *
 *  PCMCIA card support is not supported for now 
 */
 
#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareDetect.h"
#include "AudioHardwareOutput.h"
#include "AudioHardwareInput.h"
#include "AudioHardwareMux.h"
#include "AudioHardwarePower.h"

#include "awacs_hw.h"
#include "AppleScreamerAudio.h"

#include "Apple02DBDMAAudioDMAEngine.h"

static void		ScreamerWaitUntilReady (volatile awacs_regmap_t *ioBaseAwacs);
static void 	Screamer_writeCodecControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value );
static void 	Screamer_writeSoundControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value );
static UInt32   Screamer_ReadStatusRegisters( volatile awacs_regmap_t *ioBaseAwacs );
static int		Screamer_readCodecControlReg(volatile awacs_regmap_t *ioBaseAwacs, int regNum);

#define	LEFTATTEN(x)				((x & kAWACsOutputLeftAtten) >> kAWACsOutputLeftShift)
#define RIGHTATTEN(x)				(x & kAWACsOutputRightAtten)

// Not used at the moment, turned off just to kill warning

static void ScreamerWaitUntilReady (volatile awacs_regmap_t *ioBaseAwacs) {
	UInt32					codecStatus;
	UInt32					partReady;

	// Wait for the part to become ready before we talk to it
	codecStatus = Screamer_ReadStatusRegisters (ioBaseAwacs);
	partReady = codecStatus & 1 << 22;
	while (!partReady) {
		IOSleep (1);
		codecStatus = Screamer_ReadStatusRegisters (ioBaseAwacs);
		partReady = codecStatus & 1 << 22;
	}
}

static int Screamer_readCodecControlReg(volatile awacs_regmap_t *ioBaseAwacs, int regNum)
{
    UInt32 reg, result;
    
    reg = (((regNum << kReadBackRegisterShift) & kReadBackRegisterMask) | kReadBackEnable) & kCodecCtlDataMask;
    reg |= kCodecCtlAddress7;
    reg |= kCodecCtlEMSelect0;
    
    OSWriteLittleInt32(&ioBaseAwacs->CodecControlRegister, 0, reg);
    eieio();
    
    do {
        result = OSReadLittleInt32(&ioBaseAwacs->CodecControlRegister, 0);
        eieio();
    } while (result & kCodecCtlBusy);
    
    // We're going to do this twice to make sure the results are back before reading the status register
    // What a pain - there must be a better way
    OSWriteLittleInt32(&ioBaseAwacs->CodecControlRegister, 0, reg);
    eieio();
    
    do {
        result = OSReadLittleInt32(&ioBaseAwacs->CodecControlRegister, 0);
        eieio();
    } while (result & kCodecCtlBusy);
    
    result = (OSReadLittleInt32(&ioBaseAwacs->CodecStatusRegister, 0) >> 4) & kCodecCtlDataMask;
    
    return result;
}


static void Screamer_writeCodecControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value )
{
	int          CodecControlReg;

	// debugIOLog (3,  "PPCSound(awacs): CodecControlReg @ %08x = %08x", (int)&ioBaseAwacs->CodecControlRegister, value);

	ScreamerWaitUntilReady (ioBaseAwacs);

	OSWriteLittleInt32(&ioBaseAwacs->CodecControlRegister, 0, value );
	eieio();

	do
	{
		CodecControlReg =  OSReadLittleInt32( &ioBaseAwacs->CodecControlRegister, 0 );
		eieio();
		IOSleep ( 1 );
	}
	while ( CodecControlReg & kCodecCtlBusy );
}


static void Screamer_writeSoundControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value )
{
	// debugIOLog (3,  "PPCSound(awacs): SoundControlReg = %08x", value);

	OSWriteLittleInt32( &ioBaseAwacs->SoundControlRegister, 0, value );
	eieio();
}

static UInt32 Screamer_ReadStatusRegisters( volatile awacs_regmap_t *ioBaseAwacs )
{	
    // we need to have something that check if the Screamer is busy or in readback mode

	return OSReadLittleInt32( &ioBaseAwacs->CodecStatusRegister, 0 );
}

#define super Apple02Audio

OSDefineMetaClassAndStructors(AppleScreamerAudio, Apple02Audio)

// Unix like prototypes
bool AppleScreamerAudio::init(OSDictionary *properties)
{
    debugIOLog (3, "+ AppleScreamerAudio::init");
    if (!super::init(properties))
        return false;        
    chipInformation.awacsVersion = kAWACsAwacsRevision;
    
//    mVolLeft = 0;
//    mVolRight = 0;
//    mIsMute = false;
    mVolMuteActive = false;

    debugIOLog (3, "- AppleScreamerAudio::init");
    return true;
}


void AppleScreamerAudio::free()
{
    debugIOLog (3, "+ AppleScreamerAudio::free");
	super::free();
    debugIOLog (3, "- AppleScreamerAudio::free");
}


IOService* AppleScreamerAudio::probe(IOService* provider, SInt32* score)
{
	// Finds the possible candidate for sound, to be used in
	// reading the caracteristics of this hardware:
    IORegistryEntry *sound = 0;
    debugIOLog (3, "+ AppleScreamerAudio::probe");
    
    super::probe(provider, score);
    *score = kIODefaultProbeScore;
    sound = provider->childFromPath("sound", gIODTPlane);
	// we are on a new world : the registry is assumed to be fixed
    if(sound) {
        OSData *tmpData;
        
        tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
        if(tmpData) {
            if(tmpData->isEqualTo(kAWACsModelName, sizeof(kAWACsModelName) -1) ||
               tmpData->isEqualTo(kScreamerModelName, sizeof(kScreamerModelName) -1) ) {
                *score = *score+1;
                debugIOLog (3, "- AppleScreamerAudio::probe");
                return(this);
            } 
        } 
    } 
    debugIOLog (3, "- AppleScreamerAudio::probe");
    return (0);
}

bool AppleScreamerAudio::initHardware(IOService *provider)
{
    bool myreturn = true;

    debugIOLog (3, "+ AppleScreamerAudio::initHardware");

    super::initHardware(provider);

// this code moved to sndHWPostThreadedInit () for [3284411]
#if 0
	// Common information
    codecStatus &= ~kAllSense;
    gCanPollStatus = true;    
    checkStatus(true);
   
	// create and flush ports, controls
    
    // Use the current hardware settings as our defaults
   /* if (chipInformation.outputAActive) {
        int regValue;
        
        regValue = Screamer_readCodecControlReg(ioBase, kAWACsOutputAAttenReg);
        
        mVolLeft = (15 - ((regValue & kAWACsOutputLeftAtten) << kAWACsOutputLeftShift)) * 4096;
        mVolRight = (15 - (regValue & kAWACsOutputRightAtten)) * 4096;
        
        regValue = Screamer_readCodecControlReg(ioBase, kAWACsMuteReg);
        
        deviceMuted = (regValue & kAWACsMuteOutputA) == kAWACsMuteOutputA;
    } else if (chipInformation.outputCActive) {
        int regValue;
        
        regValue = Screamer_readCodecControlReg(ioBase, kAWACsOutputCAttenReg);
        
        mVolLeft = (15 - ((regValue & kAWACsOutputLeftAtten) << kAWACsOutputLeftShift)) * 4096;
        mVolRight = (15 - (regValue & kAWACsOutputRightAtten)) * 4096;
        
        regValue = Screamer_readCodecControlReg(ioBase, kAWACsMuteReg);
        
        deviceMuted = (regValue & kAWACsMuteOutputC) == kAWACsMuteOutputC;
    } else {
        deviceMuted = true;
    }
    
    if (outVolLeft) {
        outVolLeft->setValue(mVolLeft);
    }
    
    if (outVolRight) {
        outVolRight->setValue(mVolRight);
    }
    
    if (outMute) {
        outMute->setValue(deviceMuted ? 1 : 0);
    } */
    
    
	// flushAudioControls();
    
	// Prepare the timer loop --> should go on the workloop
    nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
    addTimerEvent(this, &AppleScreamerAudio::timerCallback, timerInterval);
    powerState = kIOAudioDeviceActive;
 #endif
 
   debugIOLog (3, "- AppleScreamerAudio::initHardware");
    return myreturn;
}

void AppleScreamerAudio::sndHWPostDMAEngineInit (IOService *provider) {
	if (NULL != driverDMAEngine) {
		driverDMAEngine->setSampleLatencies (kScreamerSampleLatency, kScreamerSampleLatency);
	}

	mInternalMicDualMonoMode = e_Mode_CopyLeftToRight;	// [3306493]	

	//	rbm	30 Sept 2002					[3042658]	begin {
	if (NULL == outVolRight && NULL != outVolLeft) {
		// If they are running mono at boot time, set the right channel's last value to an illegal value
		// so it will come up in stereo and center balanced if they plug in speakers or headphones later.
		lastRightVol = kSCREAMER_OUT_OF_BOUNDS_HW_VOLUME;
		lastLeftVol = outVolLeft->getIntValue ();
	}
	//	rbm	30 Sept 2002					[3042658]	} end
}

void AppleScreamerAudio::setDeviceDetectionActive(){
    gCanPollStatus = true;
}
    
void AppleScreamerAudio::setDeviceDetectionInActive(){
    gCanPollStatus = false;
}

// This method handles everything after the initHW thread is done, keeping the 
// same sequence of code as before the thread was introduced. [3284411]
void AppleScreamerAudio::sndHWPostThreadedInit (IOService *provider) 
{
    AbsoluteTime		timerInterval;

	// Common information
    codecStatus &= ~kAllSense;
    gCanPollStatus = true;    
    checkStatus(true);
    
	// Prepare the timer loop --> should go on the workloop
    nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
    addTimerEvent(this, &AppleScreamerAudio::timerCallback, timerInterval);
    powerState = kIOAudioDeviceActive;
	return;
}

// IOAudio subclasses
void AppleScreamerAudio::sndHWInitialize(IOService *provider)
{
    IOMemoryMap *map;
    
    debugIOLog (3, "+ AppleScreamerAudio::sndHWInitialize");
	ourProvider = provider;
    map = provider->mapDeviceMemoryWithIndex(Apple02DBDMAAudioDMAEngine::kDBDMADeviceIndex);
    
    ioBase = (awacs_regmap_t *)map->getVirtualAddress();
    
	layoutID = GetDeviceID();							//	[3042658]	rbm	30 Sept 2002
	
    codecStatus = Screamer_ReadStatusRegisters( ioBase );
            
	minVolume = kSCREAMER_MINIMUM_HW_VOLUME;			//	[3042658]	minimum hardware volume setting
	maxVolume = kSCREAMER_MAXIMUM_HW_VOLUME;			//	[3042658]	maximum hardware volume setting

	// fill the chip info
    chipInformation.partType = sndHWGetType();
    chipInformation.awacsVersion = (codecStatus & kAWACsRevisionNumberMask) >> kAWACsRevisionShift;
    chipInformation.preRecalDelay = kPreRecalDelayCrystal;      // assume Crystal for recalibrate (safest)
    chipInformation.rampDelay = kRampDelayNational;		// assume National for ramping (safest)
    
    soundControlRegister = ( kInSubFrame0 | kOutSubFrame0 | kHWRate44100);
    Screamer_writeSoundControlReg( ioBase, soundControlRegister);

    codecControlRegister[0] = kCodecCtlAddress0 | kCodecCtlEMSelect0;
    codecControlRegister[1] = kCodecCtlAddress1 | kCodecCtlEMSelect0;
    codecControlRegister[2] = kCodecCtlAddress2 | kCodecCtlEMSelect0;
    codecControlRegister[4] = kCodecCtlAddress4 | kCodecCtlEMSelect0;
    
    Screamer_writeCodecControlReg( ioBase, codecControlRegister[0] );
    Screamer_writeCodecControlReg( ioBase, codecControlRegister[1] );
    Screamer_writeCodecControlReg( ioBase, codecControlRegister[2] );
    Screamer_writeCodecControlReg( ioBase, codecControlRegister[4] );
    
    if ( chipInformation.awacsVersion > kAWACsAwacsRevision ) {
        codecControlRegister[5] = kCodecCtlAddress5 | kCodecCtlEMSelect0;
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[5] );
        codecControlRegister[6] = kCodecCtlAddress6 | kCodecCtlEMSelect0;
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[6] );
    }

    switch (sndHWGetManufacturer()){
        case kSndHWManfCrystal :
            chipInformation.preRecalDelay = kPreRecalDelayCrystal;
            chipInformation.rampDelay = kRampDelayCrystal;
            break;
        case kSndHWManfNational :
            chipInformation.preRecalDelay = kPreRecalDelayNational;
            chipInformation.rampDelay = kRampDelayNational;
            break;
        default :
            break;
    }
    
	// do the IO init

	// These line should go into the IO part

	// this means we assume an mic input. We should assume through the 
	// input objects
    codecControlRegister[0] |= (kUnusedInput | kDefaultMicGain);

	// we should add the Screamer info
    debugIOLog (3, "- AppleScreamerAudio::sndHWInitialize");
}

/****************************** Workloop functions ***************************/
/*   There is work to do there !!!!  					     				 */
/*   In fact we should put that on the work loop, and have a family method   */
/*   to add whatever we want on it (polling, interrupt based stuff...)       */
/*   There is also to improve into the communication with any input 	     */
/*   and output. Instead of calling the arrays, we should send notification  */
/*   trough the IOAudioJackControl inheritance of the AudioHardwareDetect,   */
/*   and have all ports, listen to these notification (maybe it would have   */
/*   easier in Objective-C... or the IOKit does it for us)		     		 */
/*****************************************************************************/

void AppleScreamerAudio::checkStatus(bool force) {

    UInt32 newCodecStatus, inSense, extdevices;
    AudioHardwareDetect *theDetect;
    OSArray *AudioDetects;

	// debugIOLog (3, "+ AppleScreamerAudio::checkStatus");        
    
	if ( gCanPollStatus ) {
		newCodecStatus = Screamer_ReadStatusRegisters(ioBase);
	
		if (((codecStatus & kAWACsStatusInSenseMask) != (newCodecStatus &kAWACsStatusInSenseMask)) || force)   {
			UInt32 i;
	
			inSense = 0;
			codecStatus = newCodecStatus;
			extdevices = 0;
			inSense = sndHWGetInSenseBits();
	
			AudioDetects = getDetectArray();
			if(AudioDetects) {
				for(i = 0; i < AudioDetects->getCount(); i++) {
					theDetect = OSDynamicCast(AudioHardwareDetect, AudioDetects->getObject(i));
					if (theDetect) {
						extdevices |= theDetect->refreshDevices(inSense);
					}
				}
				setCurrentDevices(extdevices);
				
				//	Opportunity to omit Balance controls if on the internal mono speaker...	[3042658]	begin {
				useMasterVolumeControl = FALSE;
				if (NULL != driverDMAEngine) {
					if ( layoutSawtooth == layoutID  ) {
						//	Sawtooth's headphone detect is active high:
						//		"detect bit-mask 2 bit-match 2 device 2 index 0 model InSenseBitsDetect"
						//	The master control should be used when headphones are NOT present.
						if ( kSndHWInSense1 != ( inSense & kSndHWInSense1 ) ) {
							useMasterVolumeControl = TRUE;
						}
					}
				}
				debugIOLog (3,  "... useMasterVolumeControl %d", useMasterVolumeControl );
				AdjustControls ();					//	rbm	30 Sept 2002					[3042658]	} end
			} else {
				debugIOLog (3, "... didn't get the array");
			}
		} else {
			debugIOLog (3, "... newCodecStatus %d", (unsigned int)newCodecStatus );
		}
	}
	// debugIOLog (3, "- AppleScreamerAudio::checkStatus");
}


void AppleScreamerAudio::timerCallback(OSObject *target, IOAudioDevice *device) {
    AppleScreamerAudio *screamer;

	// debugIOLog (3, "+ AppleScreamerAudio::timerCallback");
    screamer = OSDynamicCast(AppleScreamerAudio, target);
    if (screamer) 
        screamer->checkStatus(false);
	// debugIOLog (3, "- AppleScreamerAudio::timerCallback");
}


UInt32 AppleScreamerAudio::sndHWGetCurrentSampleFrame (void) {
	return OSReadLittleInt32 (&ioBase->FrameCountRegister, 0);
}

void AppleScreamerAudio::sndHWSetCurrentSampleFrame (UInt32 value) {
	OSWriteLittleInt32 (&ioBase->FrameCountRegister, 0, value);
}



/*************************** sndHWXXXX functions******************************/
/*   These functions should be common to all Apple hardware drivers and be   */
/*   declared as virtual in the superclass. This is the only place where we  */
/*   should manipulate the hardware. Order is given by the UI policy system  */
/*   The set of function should be enought to implement the policy           */
/*****************************************************************************/

/************************** Hardware Register Manipulation ********************/

UInt32 AppleScreamerAudio::sndHWGetInSenseBits(){
    UInt32 newCodecStatus, status, inSense;
    
    inSense = 0;
    newCodecStatus = Screamer_ReadStatusRegisters(ioBase);
    newCodecStatus &= kAWACsStatusMask;
    status = newCodecStatus & kAWACsInSenseMask;
    
	// something is wacky with the order of the bytes
    if(status & kAWACsInSense0) 
        inSense |= kAWACsInSense3;
    if(status & kAWACsInSense1)
        inSense |= kAWACsInSense2;
    if(status & kAWACsInSense2)
        inSense |= kAWACsInSense1;
    if(status & kAWACsInSense3) 
        inSense |= kAWACsInSense0;

    return(inSense);
}

UInt32 AppleScreamerAudio::sndHWGetRegister(UInt32 regNum){
    return(codecControlRegister[regNum]);
}

IOReturn AppleScreamerAudio::sndHWSetRegister(UInt32 regNum, UInt32 value){
    IOReturn myReturn = kIOReturnSuccess;
    
    codecControlRegister[regNum] = value;
    Screamer_writeCodecControlReg(ioBase, codecControlRegister[regNum]);
    return(myReturn);
}

UInt32	AppleScreamerAudio::sndHWGetConnectedDevices(void) {
    return(currentDevices);
}

/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/

UInt32 AppleScreamerAudio::sndHWGetActiveOutputExclusive(void){
    UInt32 result;
    
    if(!(chipInformation.outputAActive) && !(chipInformation.outputCActive))
        result = kSndHWOutputNone;
    else if(chipInformation.outputAActive)
        result = kSndHWOutput1;
    else
        result = kSndHWOutput2;

    return(result);
}
    
IOReturn   AppleScreamerAudio::sndHWSetActiveOutputExclusive(UInt32 outputPort ){
    IOReturn myReturn = kIOReturnSuccess;
    
    switch (outputPort){
        case kSndHWOutputNone:
            chipInformation.outputAActive = false;
            chipInformation.outputCActive = false;
            break;
        case kSndHWOutput1:
            debugIOLog (3, "+ output A is active ");
            chipInformation.outputAActive = true;
            chipInformation.outputCActive = false;
            break;
        case kSndHWOutput2:
            debugIOLog (3, "+ output C is active ");
            chipInformation.outputAActive = false;
            chipInformation.outputCActive = true;
            break;
        default:
            myReturn = false;
            break;
    }		
    sndHWSetSystemMute(sndHWGetSystemMute());
    return (myReturn);
}

UInt32 	 AppleScreamerAudio::sndHWGetActiveInputExclusive(void){
    UInt32		input;
    UInt32		inputReg;
    UInt32		pcmciaReg;
		
    input = kSndHWInputNone;
	
    inputReg = sndHWGetRegister(kAWACsInputReg) & kAWACsInputField;
    pcmciaReg = sndHWGetRegister(kAWACsPCMCIAAttenReg) & kAWACsPCMCIAAttenField;

    switch (inputReg){
        case kAWACsInputA:
            input = kSndHWInput1;
            break;
        case kAWACsInputB:
            input = kSndHWInput2;
            break;
        case kAWACsInputC:
            input = kSndHWInput3;
            break;
        default:
            if (pcmciaReg == kAWACsPCMCIAOn)
                input = kSndHWInput4;
            else if (inputReg != 0)
                debugIOLog (3, "Invalid input setting");
            break;
    }
	
    return (input);
}

IOReturn AppleScreamerAudio::sndHWSetActiveInputExclusive(UInt32 input ){
    IOReturn result = kIOReturnSuccess; 
    
    UInt32		inputReg;
    UInt32		pcmciaReg;
    Boolean		needsRecalibrate;
	
	// needsRecalibrate = (input != sndHWGetActiveInputExclusive());
	needsRecalibrate = FALSE;		// no need to recalibrate when switching to the modem input, this is the modem input on PowerBook G4
	// start with all inputs off
    inputReg = sndHWGetRegister(kAWACsInputReg) & ~kAWACsInputField;
    pcmciaReg = sndHWGetRegister(kAWACsPCMCIAAttenReg) & ~kAWACsPCMCIAAttenField;
    	
    switch (input){
        case kSndHWInputNone:
			needsRecalibrate = TRUE;		// Force a recalibration
            break;
        case kSndHWInput1:
            inputReg |= kAWACsInputA;	
            break;
        case kSndHWInput2:
            inputReg |= kAWACsInputB;
            break;
        case kSndHWInput3:
            inputReg |= kAWACsInputC;
            break;
        case kSndHWInput4:
            pcmciaReg |= kAWACsPCMCIAOn;
            break;
        default:
            result = kIOReturnError;
            goto EXIT;
            break;
    }
	
	// this should disappear. We put the gain input to the max value
    // gainReg = sndHWGetRegister(kAWACsGainReg) & ~kAWACsGainField;		// get and clear current gain setting
	// gainReg |= ((kAWACsMaxHardwareGain << kAWACsGainLeftShift) & kAWACsGainLeft);
	// gainReg |= (kAWACsMaxHardwareGain & kAWACsGainRight);
	// sndHWSetRegister(kAWACsGainReg, gainReg);

    sndHWSetRegister(kAWACsInputReg, inputReg);
	
	if (needsRecalibrate) 
        GC_Recalibrate();

EXIT:
    return(result);
}

// --------------------------------------------------------------------------
// You either have only a master volume control, or you have both volume controls.
IOReturn AppleScreamerAudio::AdjustControls (void) {
	IOFixed							mindBVol;
	IOFixed							maxdBVol;
	Boolean							mustUpdate;

	debugIOLog (3, "+ AdjustControls()");
	FailIf (NULL == driverDMAEngine, Exit);
	mustUpdate = FALSE;

	mindBVol = kSCREAMER_MIN_VOLUME;
	maxdBVol = kSCREAMER_MAX_VOLUME;

	//	Must update if any of the following conditions exist:
	//	1.	No master volume control exists AND the master volume is the target
	//	2.	The master volume control exists AND the master volume is not the target
	//	3.	the minimum or maximum dB volume setting for the left volume control changes
	//	4.	the minimum or maximum dB volume setting for the right volume control changes
	
	if ((NULL == outVolMaster && TRUE == useMasterVolumeControl) ||
		(NULL != outVolMaster && FALSE == useMasterVolumeControl) ||
		(NULL != outVolLeft && outVolLeft->getMinValue () != minVolume) ||
		(NULL != outVolLeft && outVolLeft->getMaxValue () != maxVolume) ||
		(NULL != outVolRight && outVolRight->getMinValue () != minVolume) ||
		(NULL != outVolRight && outVolRight->getMaxValue () != maxVolume)) {
		mustUpdate = TRUE;
	}

	if (TRUE == mustUpdate) {
		debugIOLog (3, "AdjustControls: mindBVol = %d.0x%x, maxdBVol = %d.0x%x", 
			(unsigned int)( 0 != mindBVol & 0x80000000 ? ( mindBVol >> 16 ) | 0xFFFF0000 : mindBVol >> 16 ), 
			(unsigned int)( mindBVol << 16 ), 
			(unsigned int)( 0 != maxdBVol & 0x80000000 ? ( maxdBVol >> 16 ) | 0xFFFF0000 : maxdBVol >> 16 ), 
			(unsigned int)( maxdBVol << 16 ) );
	
		driverDMAEngine->pauseAudioEngine ();
		driverDMAEngine->beginConfigurationChange ();
	
		if (TRUE == useMasterVolumeControl) {
			// We have only the master volume control (possibly not created yet) and have to remove the other volume controls (possibly don't exist)
			if (NULL == outVolMaster) {
				debugIOLog (3, "AdjustControls: deleteing descrete channel controls and creating master control");
				// remove the existing left and right volume controls
				if (NULL != outVolLeft) {
					lastLeftVol = outVolLeft->getIntValue ();
					driverDMAEngine->removeDefaultAudioControl (outVolLeft);
					outVolLeft = NULL;
				} 
		
				if (NULL != outVolRight) {
					lastRightVol = outVolRight->getIntValue ();
					driverDMAEngine->removeDefaultAudioControl (outVolRight);
					outVolRight = NULL;
				}
	
				// Create the master control
				outVolMaster = IOAudioLevelControl::createVolumeControl((lastLeftVol + lastRightVol) / 2, minVolume, maxVolume, mindBVol, maxdBVol,
													kIOAudioControlChannelIDAll,
													kIOAudioControlChannelNameAll,
													kOutVolMaster, 
													kIOAudioControlUsageOutput);
	
				if (NULL != outVolMaster) {
					driverDMAEngine->addDefaultAudioControl(outVolMaster);
					outVolMaster->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
					outVolMaster->flushValue ();
				}
			}
		} else {
			// or we have both controls (possibly not created yet) and we have to remove the master volume control (possibly doesn't exist)
			if (NULL == outVolLeft) {
				debugIOLog (3, "AdjustControls: deleteing master control and creating descrete channel controls");
				// Have to create the control again...
				if (lastLeftVol > kSCREAMER_MAXIMUM_HW_VOLUME && NULL != outVolMaster) {
					lastLeftVol = outVolMaster->getIntValue ();
				}
				outVolLeft = IOAudioLevelControl::createVolumeControl (lastLeftVol, kSCREAMER_MINIMUM_HW_VOLUME, kSCREAMER_MAXIMUM_HW_VOLUME, mindBVol, maxdBVol,
													kIOAudioControlChannelIDDefaultLeft,
													kIOAudioControlChannelNameLeft,
													kOutVolLeft,
													kIOAudioControlUsageOutput);
				if (NULL != outVolLeft) {
					driverDMAEngine->addDefaultAudioControl (outVolLeft);
					outVolLeft->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				}
			}
			
			if (NULL == outVolRight) {
				// Have to create the control again...
				if (lastRightVol > kSCREAMER_MAXIMUM_HW_VOLUME && NULL != outVolMaster) {
					lastRightVol = outVolMaster->getIntValue ();
				}
				outVolRight = IOAudioLevelControl::createVolumeControl (lastRightVol, kSCREAMER_MINIMUM_HW_VOLUME, kSCREAMER_MAXIMUM_HW_VOLUME, mindBVol, maxdBVol,
													kIOAudioControlChannelIDDefaultRight,
													kIOAudioControlChannelNameRight,
													kOutVolRight,
													kIOAudioControlUsageOutput);
				if (NULL != outVolRight) {
					driverDMAEngine->addDefaultAudioControl (outVolRight);
					outVolRight->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				}
			}
	
			if (NULL != outVolMaster) {
				driverDMAEngine->removeDefaultAudioControl (outVolMaster);
				outVolMaster = NULL;
			}
		}
	
		if (NULL != outVolMaster) {
			outVolMaster->setMinValue (minVolume);
			outVolMaster->setMinDB (mindBVol);
			outVolMaster->setMaxValue (maxVolume);
			outVolMaster->setMaxDB (maxdBVol);
			if (outVolMaster->getIntValue () > maxVolume) {
				outVolMaster->setValue (maxVolume);
			}
			outVolMaster->flushValue ();
		}
	
		if (NULL != outVolLeft) {
			outVolLeft->setMinValue (minVolume);
			outVolLeft->setMinDB (mindBVol);
			outVolLeft->setMaxValue (maxVolume);
			outVolLeft->setMaxDB (maxdBVol);
			if (outVolLeft->getIntValue () > maxVolume) {
				outVolLeft->setValue (maxVolume);
			}
			outVolLeft->flushValue ();
		}
	
		if (NULL != outVolRight) {
			outVolRight->setMinValue (minVolume);
			outVolRight->setMinDB (mindBVol);
			outVolRight->setMaxValue (maxVolume);
			outVolRight->setMaxDB (maxdBVol);
			if (outVolRight->getIntValue () > maxVolume) {
				outVolRight->setValue (maxVolume);
			}
			outVolRight->flushValue ();
		}
	
		driverDMAEngine->completeConfigurationChange ();
		driverDMAEngine->resumeAudioEngine ();
	}

Exit:
	debugIOLog (3, "- AdjustControls()");
	return kIOReturnSuccess;
}

UInt32  AppleScreamerAudio::sndHWGetProgOutput(void ){
    return (sndHWGetRegister(kAWACsProgOutputReg) & kAWACsProgOutputField) >> kAWACsProgOutputShift;
}

IOReturn   AppleScreamerAudio::sndHWSetProgOutput(UInt32 outputBits){
    UInt32	progOutputReg;
    IOReturn result; 
	
    result = kIOReturnError;
    FailIf((outputBits & (kSndHWProgOutput0 | kSndHWProgOutput1)) != outputBits, EXIT);
    
    result = kIOReturnSuccess;
    progOutputReg = sndHWGetRegister(kAWACsProgOutputReg) & ~kAWACsProgOutputField;
    progOutputReg |= ((outputBits << kAWACsProgOutputShift) & kAWACsProgOutputField);
    sndHWSetRegister(kAWACsProgOutputReg, progOutputReg);
	
EXIT:
    return result;
}

/************************** Global (i.e all outputs) manipulation of mute and volume ***************/

bool   AppleScreamerAudio::sndHWGetSystemMute(void){
    return (gIsMute);
}
 
IOReturn   AppleScreamerAudio::sndHWSetSystemMute(bool mutestate){
    UInt32	muteReg;
    
//	debugIOLog (1, "mutestate = %d, gVolRight = %ld, gVolLeft = %ld", mutestate, gVolRight, gVolLeft);
    muteReg = sndHWGetRegister(kAWACsMuteReg) & ~kAWACsMuteField;
	if (mutestate || (gVolRight == gVolLeft && 0 == gVolRight)) {	
        muteReg |= (kAWACsMuteOutputA | kAWACsMuteOutputC);
		if (gVolRight == gVolLeft && 0 == gVolRight) {
			mVolMuteActive = TRUE;
		}
    } else {
		// Set the volume to the correct volume as it might have been changed while we were muted
		sndHWSetSystemVolume (gVolLeft, gVolRight);

        if (chipInformation.outputAActive) {
            muteReg &= ~kAWACsMuteOutputA;
        } else {
            muteReg |= kAWACsMuteOutputA;
        }
        if (chipInformation.outputCActive) {
            muteReg &= ~kAWACsMuteOutputC;
        } else {
            muteReg |= kAWACsMuteOutputC;
        }
    }
    sndHWSetRegister(kAWACsMuteReg, muteReg);
    return(kIOReturnSuccess);
}

IOReturn   AppleScreamerAudio::sndHWSetSystemVolume(UInt32 value){
    sndHWSetSystemVolume( value,  value);
    return kIOReturnSuccess;
}

bool AppleScreamerAudio::sndHWSetSystemVolume(UInt32 leftvalue, UInt32 rightvalue) {
	// This function is not very flexible. It sets the volume for 
	// each output port to the same level. This is obvioulsy not
	// very flexible, but we keep that for the UI Policy implementation

//    bool			hasChanged = false;
    UInt32			leftAttn;
	UInt32			rightAttn;

//	debugIOLog (1, "leftvalue = %ld, rightvalue = %ld, gVolRight = %ld, gVolLeft = %ld", leftvalue, rightvalue, gVolRight, gVolLeft);

//    if (((SInt32)leftvalue != gVolLeft)) {
        if (0 == leftvalue) {
			leftvalue = 1;
			leftMute = TRUE;
		} else {
			leftMute = FALSE;
		}
        leftvalue -= 1;
        leftAttn = 15 - leftvalue;
//		debugIOLog (1, "leftAttn = %ld, leftvalue = %ld ", leftAttn, leftvalue);
		// we change the left value for each register
        codecControlRegister[kAWACsOutputAAttenReg] 
                = (codecControlRegister[kAWACsOutputAAttenReg] & ~kAWACsOutputLeftAtten) |
                            (leftAttn << kAWACsOutputLeftShift);
        codecControlRegister[kAWACsOutputCAttenReg] 
                = (codecControlRegister[kAWACsOutputCAttenReg] & ~kAWACsOutputLeftAtten) |
                            (leftAttn << kAWACsOutputLeftShift);
//        hasChanged = true;
//    }
    
//    if (((SInt32)rightvalue != gVolRight)) {
        if (0 == rightvalue) {
			rightvalue = 1;
			rightMute = TRUE;
		} else {
			rightMute = FALSE;
		}
        rightvalue -= 1;
        rightAttn = 15 - rightvalue;
//		debugIOLog (1, "rightAttn = %ld, rightvalue = %ld ", rightAttn, rightvalue);
		// we change the right value for each register
        codecControlRegister[kAWACsOutputAAttenReg] 
                = (codecControlRegister[kAWACsOutputAAttenReg] & ~kAWACsOutputRightAtten) |
                            (rightAttn);
        codecControlRegister[kAWACsOutputCAttenReg] 
                = (codecControlRegister[kAWACsOutputCAttenReg] & ~kAWACsOutputRightAtten) |
                            (rightAttn);
//        hasChanged = true;
//    }
    
//    if (hasChanged) {
        Screamer_writeCodecControlReg ( ioBase, codecControlRegister[kAWACsOutputAAttenReg] );
        Screamer_writeCodecControlReg ( ioBase, codecControlRegister[kAWACsOutputCAttenReg] );
    
	//	if((rightvalue == leftvalue) && (0 == rightvalue)) {
		if (TRUE == leftMute && TRUE == rightMute) {
			mVolMuteActive = true;
//			debugIOLog (1, "setting system mute from volume handler");
			sndHWSetSystemMute(true);
		} else {
			if(mVolMuteActive) {
				mVolMuteActive = false;
//				debugIOLog (1, "turning off system mute from volume handler");
				sndHWSetSystemMute(false);
			}
		}
//    }

    return(true);
}


IOReturn AppleScreamerAudio::sndHWSetPlayThrough( bool playthroughState )
{
	UInt32	playthruReg;
	IOReturn result = kIOReturnSuccess; 

	playthruReg = sndHWGetRegister(kAWACsLoopThruReg) & ~kAWACsLoopThruEnable;
	if (playthroughState) {
		playthruReg |= kAWACsLoopThruEnable;
	}
	sndHWSetRegister(kAWACsLoopThruReg, playthruReg);

	return result;
}


/************************** Identification of the codec ************************/

UInt32 AppleScreamerAudio::sndHWGetType( void ) {
	UInt32		revision, info;
	UInt32		codecStatus;
	
        codecStatus = Screamer_ReadStatusRegisters( ioBase );
        revision = (codecStatus & kAWACsRevisionNumberMask) >> kAWACsRevisionShift;
        
	switch (revision) {
            case kAWACsAwacsRevision :
                info = kSndHWTypeAWACs;
                break;
            case kAWACsScreamerRevision :
                info = kSndHWTypeScreamer;
                break;
            default :
                info = kSndHWTypeUnknown;
                break;
	}		
        return info;
}

// --------------------------------------------------------------------------
IOReturn   AppleScreamerAudio::sndHWSetPowerState ( IOAudioDevicePowerState theState ) {
    IOReturn		myReturn;
    
    debugIOLog (3, "+ AppleScreamerAudio::sndHWSetPowerState");
    myReturn = kIOReturnSuccess;
    switch ( theState ) {
        case kIOAudioDeviceSleep:	myReturn = performDeviceSleep();		break;		//	When sleeping
        case kIOAudioDeviceIdle:	myReturn = performDeviceIdleSleep();	break;		//	When no audio engines running
        case kIOAudioDeviceActive:	myReturn = performDeviceWake();			break;		//	audio engines running
        default:					myReturn = kIOReturnBadArgument;		break;
    }
    debugIOLog (3, "- AppleScreamerAudio::sndHWSetPowerState");
    return ( myReturn );
}

// --------------------------------------------------------------------------
IOReturn	AppleScreamerAudio::performDeviceWake () {
	IOReturn	myReturn;
	
  	debugIOLog (3, "+ AppleScreamerAudio::performDeviceWake");
	myReturn = setCodecPowerState ( kIOAudioDeviceActive );	
	gPowerState = kIOAudioDeviceActive;
    debugIOLog (3, "- AppleScreamerAudio::performDeviceWake");
    return ( myReturn );
}

// --------------------------------------------------------------------------
IOReturn	AppleScreamerAudio::performDeviceSleep () {
	IOReturn	myReturn;
	
    debugIOLog (3, "+ AppleScreamerAudio::performDeviceSleep");
	myReturn = kIOReturnError;
	if ( kIOAudioDeviceSleep != gPowerState ) {
		myReturn = setCodecPowerState ( kIOAudioDeviceSleep );	
		if ( kIOReturnSuccess == myReturn ) {
			gPowerState = kIOAudioDeviceSleep;
		}
	}
    debugIOLog (3, "- AppleScreamerAudio::performDeviceSleep");
    return ( myReturn );
}

// --------------------------------------------------------------------------
IOReturn	AppleScreamerAudio::performDeviceIdleSleep () {
	IOReturn	myReturn;
	
    debugIOLog (3, "+ AppleScreamerAudio::performDeviceIdleSleep");
	myReturn = setCodecPowerState ( kIOAudioDeviceActive );	
	gPowerState = kIOAudioDeviceActive;
    debugIOLog (3, "- AppleScreamerAudio::performDeviceIdleSleep");
    return ( myReturn );
}


// --------------------------------------------------------------------------
IOReturn	AppleScreamerAudio::setCodecPowerState ( IOAudioDevicePowerState theState ) {
	IOReturn	myReturn;
	
    debugIOLog (3, "+ AppleScreamerAudio::setCodecPowerState");
	myReturn = kIOReturnSuccess;
    if ( chipInformation.partType == kSndHWTypeAWACs )
        setAWACsPowerState ( theState );
    else
        setScreamerPowerState ( theState );	
    debugIOLog (3, "- AppleScreamerAudio::setCodecPowerState");
    return ( myReturn );
}

UInt32 AppleScreamerAudio::sndHWGetManufacturer(void) {
    UInt32		codecStatus, info = 0;
    
    codecStatus = Screamer_ReadStatusRegisters( ioBase );
	
    switch (codecStatus & kAWACsManufacturerIDMask){
        case kAWACsManfCrystal:
            debugIOLog (3, "Crystal Manufacturer");
            info = kSndHWManfCrystal;
            break;
        case kAWACsManfNational:
            debugIOLog (3, "National Manufacturer");
            info = kSndHWManfNational;
            break;
        case kAWACsManfTI:
            debugIOLog (3, "TI Manufacturer");
            info = kSndHWManfTI;
            break;
        default:
            debugIOLog (3, "Unknown Manufacturer");
            info = kSndHWManfUnknown;
            break;
    }
    return info;
}

IOReturn AppleScreamerAudio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain){
    IOReturn 	myReturn = kIOReturnSuccess; 
    UInt32 		gainReg;
    UInt8 		galeft, garight;

	UInt32		preAmpReg, preAmpRegNum;
	UInt32		preAmpBit;
	Boolean		doPreAmp;

    debugIOLog (3, "+ AppleScreamerAudio::sndHWSetSystemInputGain (%ld, %ld)", leftGain, rightGain);
    galeft = (UInt8) leftGain;
    garight = (UInt8) rightGain;
	
    gainReg = sndHWGetRegister(kAWACsGainReg) & ~kAWACsGainField;		// get and clear current gain setting

    gainReg |= ((galeft << kAWACsGainLeftShift) & kAWACsGainLeft);
    gainReg |= (garight & kAWACsGainRight);
    sndHWSetRegister(kAWACsGainReg, gainReg);

	// [3099044]
	switch (sndHWGetActiveInputExclusive ())
	{
		case kSndHWInput1:
			preAmpRegNum = kAWACsPreampAReg;
			preAmpBit = kAWACsPreampA;
			doPreAmp = true;
			break;
		case kSndHWInput2:
			preAmpRegNum = kAWACsPreampBReg;
			preAmpBit = kAWACsPreampB;
			doPreAmp = true;
			break;
		default:
			doPreAmp = false;
			break;
	}
	
	if (doPreAmp)
	{
		preAmpReg = sndHWGetRegister(preAmpRegNum) & ~preAmpBit;		// get and clear pre amp setting
		if ((galeft > (UInt8)kAWACsMaxHardwareGain) || (garight > (UInt8)kAWACsMaxHardwareGain)) {	// If either are greater, turn on preamp.
			preAmpReg |= preAmpBit;
		}	
		sndHWSetRegister(preAmpRegNum, preAmpReg);
	}
	// end [3099044]
	      
    debugIOLog (3, "- AppleScreamerAudio::sndHWSetSystemInputGain");
    return(myReturn);
}

#pragma mark ++++++++ UTILITIES

/************************** Utilities for AWACS/Screamer only ************************/
/*
void AppleScreamerAudio::GC_Recalibrate()
{
    UInt32 awacsReg, saveReg;
    
    debugIOLog (3, "+ AppleScreamerAudio::GC_Recalibrate");

    IOSleep( kPreRecalDelayCrystal); //This recalibrate delay is a hack for some
                                     //broken Crystal parts
        
    awacsReg = sndHWGetRegister(kAWACsRecalibrateReg);
    saveReg = awacsReg;
    
    awacsReg |= (kMuteInternalSpeaker | kMuteHeadphone); //mute the outputs
    awacsReg |= kAWACsRecalibrate;			 //set the recalibrate bits
    
    sndHWSetRegister(kAWACsRecalibrateReg, awacsReg);
    IOSleep(1000);//kRecalibrateDelay*2);//There seems to be some confusion on the time we have to wait
                                         //No doc indicates it clearly. This value was indicated in 
                                         //the OS 9 code
    sndHWSetRegister(kAWACsRecalibrateReg, saveReg);
    debugIOLog (3, "- AppleScreamerAudio::GC_Recalibrate");
}
*/

void AppleScreamerAudio::GC_Recalibrate () {
	UInt32					awacsReg;
	UInt32					saveReg;

	debugIOLog (3, "+ AppleScreamerAudio::GC_Recalibrate");

	IOSleep (kPreRecalDelayCrystal); 		// This recalibrate delay is a hack for some broken Crystal parts

	awacsReg = sndHWGetRegister (kAWACsRecalibrateReg);
	saveReg = awacsReg;

	awacsReg |= (kMuteInternalSpeaker | kMuteHeadphone); // mute the outputs
	awacsReg |= kAWACsRecalibrate;			 // set the recalibrate bits

	sndHWSetRegister (kAWACsRecalibrateReg, awacsReg);

	ScreamerWaitUntilReady (ioBase);

	sndHWSetRegister (kAWACsRecalibrateReg, saveReg);
	debugIOLog (3, "- AppleScreamerAudio::GC_Recalibrate");
}

void AppleScreamerAudio::restoreSndHWRegisters( void )
{
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[0] );
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[1] );
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[2] );
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[4] );
        if ( chipInformation.awacsVersion > kAWACsAwacsRevision ) {
            Screamer_writeCodecControlReg(  ioBase, codecControlRegister[5] );
            Screamer_writeCodecControlReg(  ioBase, codecControlRegister[6] );
        }
}

void AppleScreamerAudio::InitializeShadowRegisters(void){
    UInt32 regNumber;
    
    switch(chipInformation.partType) {
        case kSndHWTypeScreamer:
            codecControlRegister[kMaxSndHWRegisters] = 0;
            for (regNumber = 0; regNumber < kMaxSndHWRegisters-1; regNumber++)
                codecControlRegister[regNumber] = Screamer_readCodecControlReg(ioBase,regNumber);
            break;
        case kSndHWTypeAWACs:
            for (regNumber = 0; regNumber < kMaxSndHWRegisters; regNumber++)
                    codecControlRegister[regNumber] = 0;
            break;
    }

}

void AppleScreamerAudio::setAWACsPowerState ( IOAudioDevicePowerState state )
{
    switch (state){
        case kIOAudioDeviceActive : 
            restoreSndHWRegisters();
            GC_Recalibrate();
            break;
        case kIOAudioDeviceIdle :
            break;
        case kIOAudioDeviceSleep :
            break;
        default:
            break;
    }
}

IOAudioDevicePowerState AppleScreamerAudio::SndHWGetPowerState( void )
{
	return powerState;							// return the cached copy, since part might not have power
}

void AppleScreamerAudio::setScreamerPowerState(IOAudioDevicePowerState state) {
	IOAudioDevicePowerState			curState;

	curState = SndHWGetPowerState ();

	debugIOLog (3, "[AppleScreamerAudio] going to power state %d from %d", state, curState);
	switch (state) {
		case kIOAudioDeviceActive:
			GoRunState(curState);
			break;
		case kIOAudioDeviceIdle:
			break;
		case kIOAudioDeviceSleep:
			GoSleepState(curState);
			break;
		default:
			FailMessage("Invalid set power state");
			break;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Take the part from the curState into the run state.
// 	
// 	From SLEEP to RUN:
//		Indicate that recalibrate should actually happen next time
// 		Wait up to 583 ms for sndHW ready
// 		Restore complete contents of registers 0, 2, 4, and 5
// 		Restore register 1 except for mute bits
// 		Restore register 6 except for state bits
// 		Mute both outputs
// 		Set state bits to RUN
// 		Wait for 5 ms
// 		Wait up to 583 ms for sndHW ready
//		Recalibrate if necessary
// 		Restore DMA to previous state
// 		Enable port change interrupts
// 		Restore muting on both outputs
// 		Remember state is RUN
// 	
// 	From IDLE to RUN:
// 		Mute both outputs
// 		Set state bits to RUN
// 		Wait for 5 ms
// 		Wait up to 583 ms for sndHW ready
//		Recalibrate if necessary
// 		Enable port change interrupts
// 		Restore muting on both outputs
// 		Remember state is RUN
// 	
// 	From DOZE to RUN:
// 		Mute both outputs
// 		Set state bits to RUN
// 		Wait for 5 ms
// 		Wait up to 583 ms for sndHW ready
// 		Restore muting on both outputs
// 		Remember state is RUN
void AppleScreamerAudio::GoRunState( IOAudioDevicePowerState curState )
{
	Boolean						detectsActive;

	switch (curState) {
		case kIOAudioDeviceSleep :							// Sleep -> Run
			recalibrateNecessary = true;					// after powering-up, need to recalibrate
		case kIOAudioDeviceIdle : 							// Idle -> Run
			restoreSndHWRegisters ();
			SetStateBits (kScreamerRunState1, kIdleRunDelay);// Clear both idle and doze bits
			GC_Recalibrate ();								// recalibrate if necessary
			detectsActive = gCanPollStatus;
			gCanPollStatus = TRUE;
			checkStatus (TRUE);
			sndHWSetSystemMute (sndHWGetSystemMute());					// restore muting from run state
			gCanPollStatus = detectsActive;
			break;
		case kIOAudioDeviceActive :							// Run -> Run
			break;
		default :
			FailMessage("Invalid curState in GoRunState");
			break;
	}
	powerState = kIOAudioDeviceActive;						// Set state global to run
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Take the part from the curState into the idle state.
// 	
// 	From SLEEP to IDLE:
//		Indicate that recalibrate should actually happen next time
// 		Wait up to 583 ms for sndHW ready
// 		Restore complete contents of registers 0, 2, 4, and 5
// 		Restore register 1 except for mute bits
// 		Restore register 6 except for state bits
// 		Remember state is IDLE
// 	
// 	From DOZE to IDLE:
// 		Disable port change interrupts
// 		Mute both outputs
// 		Set state bits to IDLE
// 		Wait for 5 ms
// 		Wait up to 583 ms for sndHW ready
// 		Remember state is IDLE
// 	
// 	From RUN to IDLE:
// 		Disable port change interrupts
// 		Mute both outputs
// 		Set state bits to IDLE
// 		Wait for 5 ms
// 		Wait up to 583 ms for sndHW ready
// 		Remember state is IDLE
void AppleScreamerAudio::GoIdleState( IOAudioDevicePowerState curState )
{
	switch (curState) {
		case kIOAudioDeviceSleep :							// Sleep -> Idle
			recalibrateNecessary = true;					// after powering-up, need to recalibrate
		case kIOAudioDeviceIdle :							// Idle -> Idle
			break;
		case kIOAudioDeviceActive :							// Run -> Idle
			SetStateBits(kScreamerIdleState, kIdleRunDelay);// Set idle, clear doze
			break;
		default :
			FailMessage("Invalid curState in GoIdleState");
			break;
	}
	powerState = kIOAudioDeviceIdle;						// Set state global to idle
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Take the part from the curState into the sleep state.
//	I know this can be "optimized" by utilizing the case statements falling
//	through. It is more clear and readable the way it is, please leave it.
//
// 	From IDLE to SLEEP:
// 		Disable port change interrupts
//		Do necessary steps to power down part quietly (currently nothing)
// 		Remember state is SLEEP
// 	
// 	From DOZE to SLEEP:
// 		Mute both outputs
// 		Set state bits to IDLE
// 		Wait for 5 ms
// 		Wait up to 583 ms for sndHW ready
// 		Disable port change interrupts
//		Do necessary steps to power down part quietly (currently nothing)
// 		Remember state is SLEEP
// 	
// 	From RUN to SLEEP:
// 		Save current DMA state for restore on wake
// 		Mute both outputs
// 		Set state bits to IDLE
// 		Wait for 5 ms
// 		Wait up to 583 ms for sndHW ready
// 		Disable port change interrupts
//		Do necessary steps to power down part quietly (currently nothing)
// 		Remember state is SLEEP
void AppleScreamerAudio::GoSleepState( IOAudioDevicePowerState curState )
{
	switch (curState) {
		case kIOAudioDeviceSleep :							// Sleep -> Sleep
			break;
		case kIOAudioDeviceIdle : 							// Idle -> Sleep
			break;
		case kIOAudioDeviceActive :							// Run -> Sleep
			SetStateBits(kScreamerIdleState, kIdleRunDelay);// Set idle, clear doze
			setCurrentDevices(0);
			break;
		default :
			FailMessage("Invalid curState in GoSleepState");
			break;
	}
	powerState = kIOAudioDeviceSleep;						// Set state global to sleep
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Mute all outputs and set the state bits. At the end, restore the global mute
//	state to the proper user-selected state.
void AppleScreamerAudio::SetStateBits( UInt32 stateBits, UInt32 delay )
{
	UInt32		tempReg;
//	Boolean		localMuted;

	FailMessage((stateBits & kScreamerStateField) != stateBits);

//	localMuted = sndHWGetSystemMute();							// we'll want to restore this after muting
	sndHWSetSystemMute(true);									// Mute all outputs

	tempReg = sndHWGetRegister(kAWACsPowerReg);					// Get current state
	tempReg &= ~kScreamerStateField;							// Clear Idle and Doze bits
	tempReg |= (stateBits & kScreamerStateField);				// Or in the state bits

	IOSleep (delay);
	sndHWSetRegister (kAWACsPowerReg, tempReg);

//	gIsMute = localMuted;										// restore actual mute state

	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleScreamerAudio::GetDeviceID (void) {
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*deviceID;
	UInt32					theDeviceID;

	theDeviceID = 0;

	sound = ourProvider->childFromPath (kSoundEntryName, gIODTPlane);
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kDeviceIDPropName));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32*)tmpData->getBytesNoCopy ();
	if (NULL != deviceID) {
		debugIOLog (3, "deviceID = %ld", *deviceID);
		theDeviceID = *deviceID;
	} else {
		debugIOLog (3, "deviceID = NULL!");
	}

Exit:
	return theDeviceID;
}

