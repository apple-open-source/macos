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
 * Hardware independent (relatively) code for the Awacs Controller
 *
 * HISTORY
 *
 * 14-Jan-1999
 *	Created.
 *
 */

#include "AudioHardwareCommon.h"
#include "AudioHardwareUtilities.h"
#include "AppleOWScreamerAudio.h"
#include "awacs_OWhw.h"
#include "AppleDBDMAAudioDMAEngine.h"
#include "AudioHardwareConstants.h"

/*
 * From Excelsior:Toolbox:SoundMgr:Hardware:Ports:OutPorts.cp & GossamerOut.h
 */
#define kSGS7433Addr 0x8A
#define kSGSNumRegs 7
unsigned char SGSShadowRegs[kSGSNumRegs] =
                                        { 0x09,		/* Adr:1, AutoIncr 				*/
                                          0x20, 	/* Reg 1: Vol  = 0dB				*/
                                          0xFF, 	/* Reg 2: Bass = 0dB, Treble = 0dB		*/
                                          0x00, 	/* Reg 3: Internal Spkr - Left  Atten = 0dB	*/
                                          0x0A,         /* Reg 4: Line Out	- Left  Atten = -10dB	*/
                                          0x00,         /* Reg 5: Internal Spkr - Right Atten = 0dB	*/
                                          0x0A };	/* Reg 6: Line Out	- Right Atten = -10dB   */
#define	sgsBalMuteBit		0x020	// When this bit is set in the output fader/ balance regs, the channel is muted
#define	sgsBalVolBits		0x1F	// mask for volume steps
#define kVolReg			0x01	// volume register
#define	kLFAttnReg		0x03	// Left Front (ie everything except the rear spkr jack)
#define	kRFAttnReg		0x05	// Right Front
#define	kLRAttnReg		0x04	// Left Rear (the rear speaker jack only)
#define	kRRAttnReg		0x06	// Right Rear

//#define DEBUGMODE 1

#define NUM_POWER_STATES	2

/*
 *
 */

static void 	writeCodecControlReg( volatile awacsOW_regmap_t *ioBaseAwacs, int value );
static void 	writeSoundControlReg( volatile awacsOW_regmap_t *ioBaseAwacs, int value );
static UInt32 readCodecStatusReg( volatile awacsOW_regmap_t *ioBaseAwacs );


static void writeCodecControlReg( volatile awacsOW_regmap_t *ioBaseAwacs, int value )
{
  int          CodecControlReg;

#ifdef DEBUGMODE
      DEBUG_IOLOG( "PPCSound(awacs): CodecControlReg @ %08x = %08x\n", (int)&ioBaseAwacs->CodecControlRegister, value);
#endif

  OSWriteLittleInt32(&ioBaseAwacs->CodecControlRegister, 0, value );
  eieio();

  do{
      CodecControlReg =  OSReadLittleInt32( &ioBaseAwacs->CodecControlRegister, 0 );
      eieio();
    }
  while ( CodecControlReg & kCodecCtlBusy );
}


static void writeSoundControlReg( volatile awacsOW_regmap_t *ioBaseAwacs, int value )
{

  DEBUG2_IOLOG( "PPCSound(awacs): SoundControlReg = %08x\n", value);
  OSWriteLittleInt32( &ioBaseAwacs->SoundControlRegister, 0, value );
  eieio();
}

static UInt32 readCodecStatusReg( volatile awacsOW_regmap_t *ioBaseAwacs )
{
  return OSReadLittleInt32( &ioBaseAwacs->CodecStatusRegister, 0 );
}

//static int readClippingCountReg( volatile awacsOW_regmap_t *ioBaseAwacs )
//{
//  return OSReadLittleInt32( &ioBaseAwacs->ClippingCountRegister, 0 );
//}

static void writeSGSRegs()
{
    int i;
    for (i = 0; i < kSGSNumRegs; i++) {
        (*PE_write_IIC)(kSGS7433Addr, i, SGSShadowRegs[i]);
    }
}

#define super IOAudioDevice
OSDefineMetaClassAndStructors(AppleOWScreamerAudio, IOAudioDevice)

bool AppleOWScreamerAudio::init(OSDictionary *properties)
{
    CLOG("+AppleOWScreamerAudio::init\n");
    if (!super::init(properties)) {
        return false;
    }

    awacsVersion = kAWACSMaxVersion;

    duringInitialization = true;
    curActiveSpkr = kCpuSpkr;
    gVolLeft = 255;
    gVolRight = 255;
    CLOG("-AppleOWScreamerAudio::init\n");
    return true;
}

void AppleOWScreamerAudio::free()
{
    CLOG("+AppleOWScreamerAudio::free\n");
    super::free();
    CLOG("-AppleOWScreamerAudio::free\n");
}

void AppleOWScreamerAudio::retain() const
{
    CLOG("+AppleOWScreamerAudio::retain\n");
    super::retain();
    CLOG("-AppleOWScreamerAudio::retain\n");
}

void AppleOWScreamerAudio::release() const
{
    CLOG("+AppleOWScreamerAudio::release\n");
    super::release();
    CLOG("-AppleOWScreamerAudio::release\n");
}

bool AppleOWScreamerAudio::start(IOService *provider)
{
    IOMemoryMap *		map;
    AppleDBDMAAudioDMAEngine 	*driverDMAEngine;
    IORegistryEntry *		perch = 0;
    IORegistryEntry *		sound = 0;
    AbsoluteTime		timerInterval;

    CLOG("+ AppleOWScreamerAudio::start\n");
    setManufacturerName("Apple");
    setDeviceName("Built-in audio controller");
    
    map = provider->mapDeviceMemoryWithIndex(AppleDBDMAAudioDMAEngine::kDBDMADeviceIndex);
    if (!map) {
        return false;
    }
    ioBase = (awacsOW_regmap_t *)map->getVirtualAddress();

    driverDMAEngine = new AppleDBDMAAudioDMAEngine;
    if (!driverDMAEngine->init(0, provider, true)) {
        driverDMAEngine->release();
        return false;
    }
    
	// Have to create the controls before calling activateAudioEngine
    if (!createPorts(driverDMAEngine)) {
        return false;
    }
    
    if( kIOReturnSuccess != activateAudioEngine(driverDMAEngine)){
        driverDMAEngine->release();
        return false;
    }
    
    driverDMAEngine->release();
    
    numDetects = 2;
    numOutputs = 2;

    sound = provider->childFromPath("sound", gIODTPlane);
    if (sound) {
        OSData *t;
        t = OSDynamicCast(OSData, sound->getProperty("#-detects"));
        if(t) {
            numDetects = *(UInt32*)(t->getBytesNoCopy());
        }
        t = OSDynamicCast(OSData, sound->getProperty("#-outputs"));
        if(t) {
            numOutputs = *(UInt32*)(t->getBytesNoCopy());
        }        
    }


    perch = IORegistryEntry::fromPath("/perch", gIODTPlane );

    iicAudioDevicePresent = (perch != NULL);
    if(iicAudioDevicePresent) {
        waitForService( resourceMatching( "IOiic0" ));
    }

    initHardware();
    
    /*
     * Mask out the input bits in the cached status register so that the AudioComponents
     * get updated to the correct values the first time we update the device status
     * (Everything is initialized on the assumption that nothing is plugged in)
     */
    codecStatus &= ~kAllSense;

    CLEAN_RELEASE(perch);
    CLEAN_RELEASE(sound);

    sndHWSetActiveInputExclusive(kSndHWInputNone);
    flushAudioControls();

    if (!super::start(provider)) {
        return false;
    }

    nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
    addTimerEvent(this, &AppleOWScreamerAudio::timerCallback, timerInterval);

    duringInitialization = false;
    CLOG("- AppleOWScreamerAudio::start\n");
    return true;
}

void AppleOWScreamerAudio::stop(IOService *provider)
{
    CLOG("- AppleOWScreamerAudio::stop\n");
    super::stop(provider);
    CLOG("- AppleOWScreamerAudio::stop\n");
}

void AppleOWScreamerAudio::initHardware()
{
    CLOG("+ AppleOWScreamerAudio::initHardware\n");
    codecStatus = readCodecStatusReg( ioBase );
    awacsVersion = (codecStatus & kRevisionNumberMask) >> kRevisionNumberShft;

    if ( awacsVersion > kAWACSMaxVersion ) {
        codecControlRegister[5] = kCodecCtlAddress5 | kCodecCtlEMSelect0;
        writeCodecControlReg(  ioBase, codecControlRegister[5] );
        codecControlRegister[6] = kCodecCtlAddress6 | kCodecCtlEMSelect0;
        writeCodecControlReg(  ioBase, codecControlRegister[6] );
    }

    if ( iicAudioDevicePresent ) {
        writeSGSRegs();
    }

    soundControlRegister = ( kInSubFrame0      |
                              kOutSubFrame0     |
                              kHWRate44100        );
    writeSoundControlReg( ioBase, soundControlRegister);

    codecControlRegister[0] = kCodecCtlAddress0 | kCodecCtlEMSelect0;
    codecControlRegister[1] = kCodecCtlAddress1 | kCodecCtlEMSelect0;
    codecControlRegister[2] = kCodecCtlAddress2 | kCodecCtlEMSelect0;
    codecControlRegister[4] = kCodecCtlAddress4 | kCodecCtlEMSelect0;

    codecControlRegister[0] |= (kMicInput | kDefaultMicGain);

    // Gossamer passes sound right through to be later controlled
    // by the SGS audio processor--turn on these pass-thru ports.
    if ( iicAudioDevicePresent ) {
        codecControlRegister[1] |= (kParallelOutputEnable);
    }

    writeCodecControlReg(  ioBase, codecControlRegister[0] );
    writeCodecControlReg(  ioBase, codecControlRegister[1] );
    writeCodecControlReg(  ioBase, codecControlRegister[2] );
    writeCodecControlReg(  ioBase, codecControlRegister[4] );
    
    CLOG("- AppleOWScreamerAudio::initHardware\n");
}

void AppleOWScreamerAudio::recalibrate()
{
    CLOG("+ AppleOWScreamerAudio::recalibrate\n");
    UInt32 tempCodecControlReg1;

    tempCodecControlReg1 = codecControlRegister[1];

    tempCodecControlReg1 |= (kMuteInternalSpeaker | kMuteHeadphone);
    //tempCodecControlReg1 |= kRecalibrate;

    IOSleep(10);
    writeCodecControlReg(ioBase, tempCodecControlReg1);
    IOSleep(10);
    writeCodecControlReg(ioBase, codecControlRegister[1]);
    CLOG("- AppleOWScreamerAudio::recalibrate\n");
}

IOService* AppleOWScreamerAudio::probe(IOService *provider, SInt32* score){
    IORegistryEntry *sound = 0;
    CLOG("+ AppleOWScreamerAudio::probe\n");
    
    super::probe(provider, score);
    *score = kIODefaultProbeScore;
    sound = provider->childFromPath("sound", gIODTPlane);
         //we are on a old world if there is no sound entry
    if(!sound) {
        *score = *score+1;
        return(this);
    } else {
        if (!(sound->getProperty(kModelPropName)) && !(sound->getProperty(kSoundObjectsPropName)))
            *score = *score+1;
            return(this);
    }

    return(0);

}

IOReturn AppleOWScreamerAudio::performPowerStateChange(IOAudioDevicePowerState oldPowerState,
                                                        IOAudioDevicePowerState newPowerState,
                                                        UInt32 *microsecondsUntilComplete)
{
    IOReturn result;
    
    result = super::performPowerStateChange(oldPowerState, newPowerState, microsecondsUntilComplete);
    
    if (result == kIOReturnSuccess) {
        if (oldPowerState == kIOAudioDeviceSleep) {
            result = performDeviceWake();
        } else if (newPowerState == kIOAudioDeviceSleep) {
            result = performDeviceSleep();
        }
    }
    
    return result;
}


IOReturn AppleOWScreamerAudio::performDeviceSleep(){
    IOReturn result = kIOReturnSuccess;
    
    return(result);
}

IOReturn AppleOWScreamerAudio::performDeviceWake()
{
    IOReturn result;

	writeCodecControlReg(  ioBase, codecControlRegister[0] );
	writeCodecControlReg(  ioBase, codecControlRegister[1] );
	writeCodecControlReg(  ioBase, codecControlRegister[2] );
	writeCodecControlReg(  ioBase, codecControlRegister[4] );
	if ( awacsVersion > kAWACSMaxVersion ) {
		writeCodecControlReg(  ioBase, codecControlRegister[5] );
		writeCodecControlReg(  ioBase, codecControlRegister[6] );
	}

	recalibrate();
    
	result = kIOReturnSuccess;

    return result;
}

bool AppleOWScreamerAudio::createPorts(IOAudioEngine *driverDMAEngine)
{
    IOAudioPort *outputPort = 0;
    IOAudioPort *inputPort = 0;
    IOAudioPort *passThru = 0;

    CLOG("+ AppleOWScreamerAudio::createPorts\n");
    if (!driverDMAEngine) {
        return false;
    }

    /*
     * Create out part port : 2 level (obne for each side and one mute)
     */

    outputPort = IOAudioPort::withAttributes(kIOAudioPortTypeOutput, "Output port");
    if (!outputPort) {
        return false;
    }

    outVolLeft = IOAudioLevelControl::createVolumeControl(32,0,32,
                                          (-22 << 16) + (32768), /* -22.5 in fixed point 16.16 */
                                          0,
                                          kIOAudioControlChannelIDDefaultLeft,
                                          kIOAudioControlChannelNameLeft,
                                          kOutVolLeft, 
                                          kIOAudioControlUsageOutput);
    if (!outVolLeft) {
        return false;
    }

    driverDMAEngine->addDefaultAudioControl(outVolLeft);
    outVolLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeLeftChangeHandler, this);

    outVolRight = IOAudioLevelControl::createVolumeControl(32,0,32,
                                          (-22 << 16) + (32768),
                                          0,
                                          kIOAudioControlChannelIDDefaultRight,
                                          kIOAudioControlChannelNameRight,
                                          kOutVolRight, 
                                          kIOAudioControlUsageOutput);
    if (!outVolRight) {
        return false;
    }


    driverDMAEngine->addDefaultAudioControl(outVolRight);
    outVolRight->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeRightChangeHandler, this);

    
    outMute = IOAudioToggleControl::createMuteControl(false,
                                      kIOAudioControlChannelIDAll,
                                      kIOAudioControlChannelNameAll,
                                      kOutMute, 
                                      kIOAudioControlUsageOutput);
    if (!outMute) {
        return false;
    }

    driverDMAEngine->addDefaultAudioControl(outMute);
    outMute->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputMuteChangeHandler, this);

    /*
     * Create  input port : 2 level controls associated to it
     */

    inputPort = IOAudioPort::withAttributes(kIOAudioPortTypeInput, "Input Port");
    if (!inputPort) {
        return false;
    }

    inGainLeft = IOAudioLevelControl::createVolumeControl(65535,
                                          0,
                                          65535,
                                          0,
                                          (22 << 16) + (32768),	/* 22.5 in fixed point - 16.16 */
                                          kIOAudioControlChannelIDDefaultLeft,
                                          kIOAudioControlChannelNameLeft,
                                          kInGainLeft, 
                                          kIOAudioControlUsageInput);
    if (!inGainLeft) {
        return false;
    }

    driverDMAEngine->addDefaultAudioControl(inGainLeft);
    inGainLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainRightChangeHandler, this);
    
    inGainRight = IOAudioLevelControl::createVolumeControl(65535,
                                          0,
                                          65535,
                                          0,
                                          (22 << 16) + (32768),	/* 22.5 in fixed point - 16.16 */
                                          kIOAudioControlChannelIDDefaultRight,
                                          kIOAudioControlChannelNameRight,
                                          kInGainRight, 
                                          kIOAudioControlUsageInput);
    if (!inGainRight) {
        return false;
    }
    driverDMAEngine->addDefaultAudioControl(inGainRight);
    inGainRight->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainRightChangeHandler, this);
     
    /*
     * Build passthru port
     */

    passThru = IOAudioPort::withAttributes(kIOAudioPortTypePassThru, "PassThru");
    if (!passThru) {
        return false;
    }

        
    playthruToggle = IOAudioToggleControl::createMuteControl(true,
                                         kIOAudioControlChannelIDAll,
                                         kIOAudioControlChannelNameAll,
                                         kPassThruToggle, 
                                         kIOAudioControlUsagePassThru);
    if (!playthruToggle) {
        return false;
    }
   // passThru->addAudioControl(playthruToggle);
      
    attachAudioPort(outputPort, driverDMAEngine, 0);
    attachAudioPort(inputPort, 0, driverDMAEngine);
    attachAudioPort(passThru, inputPort, outputPort);


    checkStatus(true);

    inputPort->release();
    outputPort->release();
    passThru->release();
    
    CLOG("- AppleOWScreamerAudio::createPorts\n");
    return true;
}

void AppleOWScreamerAudio::checkStatus(bool force)
{
    UInt32 newCodecStatus;
    UInt32 InsenseStatus;
    bool OmicPlugged, OheadPlugged, micPlugged, headPlugged;
    newCodecStatus = readCodecStatusReg(ioBase);

        //get the old status In fact this stuff is pretty rydimentary
    InsenseStatus = codecStatus & 0x0000000F;
    if(duringInitialization) {
        OmicPlugged = false;
        OheadPlugged = false;
    } else {
        OmicPlugged = ((InsenseStatus & kSndHWInSense1) == kSndHWInSense1);
        OheadPlugged = ((InsenseStatus & kSndHWInSense2) == kSndHWInSense2);
    }

    if ((codecStatus != newCodecStatus) || force) {
        InsenseStatus = newCodecStatus & 0x0000000F;
        micPlugged = ((InsenseStatus & kSndHWInSense1) == kSndHWInSense1);
        headPlugged = ((InsenseStatus & kSndHWInSense2) == kSndHWInSense2);

                //change  in the headphone
        if(headPlugged != OheadPlugged) {
            if(headPlugged) {
                curActiveSpkr = kHeadPhoneSpkr;  
                setToneHardwareMuteRear(false);		// Enable ext/rear speakers
                setToneHardwareMuteFront(true);		// Mute front
                setToneHardwareMuteBoomer(true);
            } else {
                curActiveSpkr = kCpuSpkr;
                setToneHardwareMuteBoomer(false);
                setToneHardwareMuteFront(false);	// Enable tone front
                setToneHardwareMuteRear(true);
            }
        }
        
        if(micPlugged != OmicPlugged) {
            if(micPlugged) 
                sndHWSetActiveInputExclusive(kSndHWInput2);
            else 
                sndHWSetActiveInputExclusive(kSndHWInputNone);
        }

        codecStatus = newCodecStatus;
    }
}

void AppleOWScreamerAudio::timerCallback(OSObject *target, IOAudioDevice *device)
{
    AppleOWScreamerAudio *screamer;

    screamer = OSDynamicCast(AppleOWScreamerAudio, target);
    if (screamer) {
        screamer->checkStatus(false);
    }
}


IOReturn AppleOWScreamerAudio::setModemSound(bool state){
    return(kIOReturnError);
}


IOReturn AppleOWScreamerAudio::callPlatformFunction( const OSSymbol * functionName,bool
            waitForFunction,void *param1, void *param2, void *param3, void *param4 ){
            
    if(functionName->isEqualTo("setModemSound")) {
        return(setModemSound((bool)param1));
    }        
    
    return(super::callPlatformFunction(functionName,
            waitForFunction,param1, param2, param3, param4));
       
}


IOReturn AppleOWScreamerAudio::setToneHardwareMuteRear(bool mute){
    IOReturn result = kIOReturnSuccess;
      
    unsigned char chipLeftReg, chipRightReg;

	// READ
    readsgs7433 ( kLRAttnReg, &chipLeftReg);
    readsgs7433 ( kRRAttnReg, &chipRightReg);
	
	// MODIFY
    if( mute ) {
        chipLeftReg |= sgsBalMuteBit;		// set bit 5
        chipRightReg |= sgsBalMuteBit;		// set bit 5
    } else {
        chipLeftReg &= ~sgsBalMuteBit;		// clear bit 5 only
        chipRightReg &= ~sgsBalMuteBit;
    }	
	// WRITE
    writesgs7433(  kLRAttnReg, chipLeftReg);
    writesgs7433(  kRRAttnReg, chipRightReg);
	
    return(result);
}

IOReturn AppleOWScreamerAudio::setToneHardwareMuteFront(bool mute){
    IOReturn result = kIOReturnSuccess;
        
    return(result);
}
    
IOReturn AppleOWScreamerAudio::setToneHardwareMuteBoomer(bool mute){
    IOReturn result = kIOReturnSuccess;
     
    UInt32	progOutputBits;

    progOutputBits = sndHWGetProgOutput();
	
    if ( mute )
        progOutputBits &= ~kSndHWProgOutput1;	// clear bit to mute, like power up
    else
        progOutputBits |=  kSndHWProgOutput1;	// set bit to enable, ie clear mute bit

    result=  sndHWSetProgOutput(progOutputBits);	// And write it out	
    return(result);
}
    
IOReturn AppleOWScreamerAudio::writesgs7433(UInt8 RegIndex, unsigned char RegValue )
{
    IOReturn result = kIOReturnSuccess;
    
    SGSShadowRegs[RegIndex]= RegValue;
    writeSGSRegs();
    
    return(result);
} 


IOReturn AppleOWScreamerAudio::readsgs7433( UInt8 RegIndex, unsigned char *RegValue)
{
     IOReturn result = kIOReturnSuccess;
     
    *RegValue= SGSShadowRegs[RegIndex];	// return the shadow reg
    return(result);
}


// A few sndHWLib function

UInt32  AppleOWScreamerAudio::sndHWGetProgOutput(void ){
    return (sndHWGetRegister(kAWACsProgOutputReg) & kAWACsProgOutputField) >> kAWACsProgOutputShift;
}

IOReturn   AppleOWScreamerAudio::sndHWSetProgOutput(UInt32 outputBits){
    UInt32	progOutputReg;
    IOReturn result; 
	
    result = kIOReturnError;
    FAIL_IF((outputBits & (kSndHWProgOutput0 | kSndHWProgOutput1)) != outputBits, EXIT);
    
    result = kIOReturnSuccess;
    progOutputReg = sndHWGetRegister(kAWACsProgOutputReg) & ~kAWACsProgOutputField;
    progOutputReg |= ((outputBits << kAWACsProgOutputShift) & kAWACsProgOutputField);
    sndHWSetRegister(kAWACsProgOutputReg, progOutputReg);
	
EXIT:
    return result;
}


UInt32 AppleOWScreamerAudio::sndHWGetRegister(UInt32 regNum){
    return(codecControlRegister[regNum]);
}


IOReturn AppleOWScreamerAudio::sndHWSetRegister(UInt32 regNum, UInt32 value){
    IOReturn myReturn = kIOReturnSuccess;
    
    codecControlRegister[regNum] = value;
    writeCodecControlReg(ioBase, codecControlRegister[regNum]);
    return(myReturn);
}


IOReturn AppleOWScreamerAudio::sndHWSetActiveInputExclusive(UInt32 input ){
    
    IOReturn result = kIOReturnSuccess; 
    
    UInt32		inputReg, gainReg;
    UInt32		pcmciaReg;
    Boolean		needsRecalibrate;
	
    needsRecalibrate = (input != sndHWGetActiveInputExclusive());
	
        // start with all inputs off
    inputReg = sndHWGetRegister(kAWACsInputReg) & ~kAWACsInputField;
    pcmciaReg = sndHWGetRegister(kAWACsPCMCIAAttenReg) & ~kAWACsPCMCIAAttenField;
    	
    switch (input){
        case kSndHWInputNone:
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
            needsRecalibrate = false;
            break;
        default:
            result = kIOReturnError;
            goto EXIT;
            break;
    }
	
        //this should disappear. We put the gain input to the max value
    
    gainReg = sndHWGetRegister(kAWACsGainReg) & ~kAWACsGainField;		// get and clear current gain setting

    gainReg |= ((kAWACsMaxHardwareGain << kAWACsGainLeftShift) & kAWACsGainLeft);
    gainReg |= (kAWACsMaxHardwareGain & kAWACsGainRight);
    sndHWSetRegister(kAWACsGainReg, gainReg);

    sndHWSetRegister(kAWACsInputReg, inputReg);
	
    //if (needsRecalibrate) 
     //   GC_Recalibrate();
      
EXIT:
    return(result);
}



UInt32 	 AppleOWScreamerAudio::sndHWGetActiveInputExclusive(void){
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
                CLOG("Invalid input setting\n");
            break;
    }
	
    return (input);
}


    
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Calculates the PRAM volume value for stereo volume.
 UInt8 AppleOWScreamerAudio::VolumeToPRAMValue( UInt32 leftVol, UInt32 rightVol )
{ 
	UInt32			pramVolume;						// Volume level to store in PRAM
	UInt32 			averageVolume;					// summed volume
    const UInt32 	volumeRange = (kMaximumVolume - kMinimumVolume+1); // for now. Change to the uber class later
    UInt32 			volumeSteps;
    
	averageVolume = (leftVol + rightVol) >> 1;		// sum the channel volumes and get an average
    volumeSteps = volumeRange / kMaximumPRAMVolume;	// divide the range by the range of the pramVolume
    pramVolume = averageVolume / volumeSteps;
    
	// Since the volume level in PRAM is only worth three bits,
	// we round small values up to 1. This avoids SysBeep from
	// flashing the menu bar when it thinks sound is off and
	// should really just be very quiet.

	if ((pramVolume == 0) && (leftVol != 0 || rightVol !=0 ))
		pramVolume = 1;
		
	return (pramVolume & 0x07);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Writes the given unsignedfixed volume out to PRAM.
void AppleOWScreamerAudio::WritePRAMVol(  UInt32 leftVol, UInt32 rightVol  )
{
	UInt8				pramVolume;
	UInt8 				curPRAMVol;
	IODTPlatformExpert * 		platform = NULL;
		
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());
        	



        if (platform)
	{
		pramVolume = VolumeToPRAMValue(leftVol,rightVol);
		
		// get the old value to compare it with
		platform->readXPRAM((IOByteCount)kPRamVolumeAddr,&curPRAMVol, (IOByteCount)1);
		
		
                    // Update only if there is a change
		if (pramVolume != (curPRAMVol & 0x07))
		{
			// clear bottom 3 bits of volume control byte from PRAM low memory image
			curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;
			
			
			// write out the volume control byte to PRAM
			platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
		}
	}
}


/****************************** Control Handler***********************************/
/*   These functions are needed to deal with the change of DMA attached control  */
/*   									         */
/*********************************************************************************/

IOReturn AppleOWScreamerAudio::volumeLeftChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOWScreamerAudio *audioDevice;
    
    
    audioDevice = (AppleOWScreamerAudio *)target;
    if (audioDevice) { result = audioDevice->volumeLeftChanged(volumeControl, oldValue, newValue);}
    return result;
}

IOReturn AppleOWScreamerAudio::volumeLeftChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    SInt32 value;
        
    value = newValue;
    
    if ( iicAudioDevicePresent ) {
        setToneHardwareVolume(newValue, gVolRight);
    } else {
        value = 15 - ((value * 16 / 65536) & 15);
        codecControlRegister[4] = (codecControlRegister[4] & ~kLeftSpeakerAttenMask) |
                            (value << kLeftSpeakerAttenShift);
        writeCodecControlReg( ioBase, codecControlRegister[4] );
    }
    WritePRAMVol(gVolLeft,gVolRight);
    return(result);
}

    
IOReturn AppleOWScreamerAudio::volumeRightChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOWScreamerAudio *audioDevice;
    
    audioDevice = (AppleOWScreamerAudio *)target;
    if (audioDevice) { result = audioDevice->volumeRightChanged(volumeControl, oldValue, newValue);}
    return result;
}

IOReturn AppleOWScreamerAudio::volumeRightChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    SInt32 value;
    value = newValue;
    
    if ( iicAudioDevicePresent ) {              
        setToneHardwareVolume( gVolLeft, newValue);
    } else {
        value = 15 - ((value * 16 / 65536) & 15);
        codecControlRegister[4] = (codecControlRegister[4] & ~kRightSpeakerAttenMask) |
                            (value << kRightSpeakerAttenShift);
        writeCodecControlReg( ioBase, codecControlRegister[4] );
    }
    WritePRAMVol(gVolLeft,gVolRight);
  
    return result;

}

    
IOReturn AppleOWScreamerAudio::outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOWScreamerAudio *audioDevice;
    
    audioDevice = (AppleOWScreamerAudio *)target;
    if (audioDevice) { result = audioDevice->outputMuteChanged(muteControl, oldValue, newValue);}
    return result;
}

IOReturn AppleOWScreamerAudio::outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    SInt32 value;
    value = newValue;
                //pass it to the AudioHardwareOutputObjects
    if (iicAudioDevicePresent ) {
        unsigned char chipInReg;
                    // READ
        readsgs7433 ( kInFuncReg, &chipInReg);
	
            // MODIFY
        if( value )					// accept any non 0 as audioMuted
            chipInReg |= SGSChipMuteBit;		// Mute sound - set mute bit		
        else
            chipInReg &= ~SGSChipMuteBit;		// Unmute it - clear mute bit

                    // WRITE
        result= writesgs7433 ( kInFuncReg, chipInReg);
           
    } else {
        if (value) {
            codecControlRegister[1] |= kMuteInternalSpeaker;
        } else {
            codecControlRegister[1] &= ~kMuteInternalSpeaker;
        }
    }
    writeCodecControlReg( ioBase, codecControlRegister[1] );
                
            //write the PRAM stuff
    if (value)
        WritePRAMVol(0,0);
    else
        WritePRAMVol(gVolLeft,gVolRight);
        
    return result;
}


IOReturn AppleOWScreamerAudio::gainLeftChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOWScreamerAudio *audioDevice;
    
    audioDevice = (AppleOWScreamerAudio *)target;
    if (audioDevice) { result = audioDevice->gainLeftChanged(gainControl, oldValue, newValue);}
    return result;

}

IOReturn AppleOWScreamerAudio::gainLeftChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    SInt32 value;
    
    value = (newValue * 16 / 65536) & 15;
    codecControlRegister[0] = (codecControlRegister[0] & ~kLeftInputGainMask) |
                        (value << kLeftInputGainShift);
    writeCodecControlReg( ioBase, codecControlRegister[0] );
            
    return result;
}


IOReturn AppleOWScreamerAudio::gainRightChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOWScreamerAudio *audioDevice;
    
    audioDevice = (AppleOWScreamerAudio *)target;
    if (audioDevice) { result = audioDevice->gainRightChanged(gainControl, oldValue, newValue);}
    return result;

}

IOReturn AppleOWScreamerAudio::gainRightChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue){
   IOReturn result = kIOReturnSuccess;
   SInt32 value;
   
   value = (newValue * 16 / 65536) & 15;
    codecControlRegister[0] = (codecControlRegister[0] & ~kRightInputGainMask) |
                        (value << kRightInputGainShift);
    writeCodecControlReg( ioBase, codecControlRegister[0] );
    return result;

}

IOReturn AppleOWScreamerAudio::passThruChangeHandler(IOService *target, IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    AppleOWScreamerAudio *audioDevice;
    
    audioDevice = (AppleOWScreamerAudio *)target;
    if (audioDevice) { result = audioDevice->passThruChanged(passThruControl, oldValue, newValue);}
    return result;

}

IOReturn AppleOWScreamerAudio::passThruChanged(IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue){
    
    IOReturn result = kIOReturnSuccess;
    
    if(newValue)
        codecControlRegister[1] &= ~kLoopThruEnable;
    else
        codecControlRegister[1] |= kLoopThruEnable;
    writeCodecControlReg( ioBase, codecControlRegister[1] );
    return result;
}


IOReturn AppleOWScreamerAudio::setToneHardwareVolume(UInt32 volLeft, UInt32 volRight){
        //for now we are doing manipulation on each bit
    setToneHardwareBalance(volLeft, volRight);
    gVolLeft = volLeft;
    gVolRight = volRight;
    return(kIOReturnSuccess);
}

IOReturn AppleOWScreamerAudio::setToneHardwareBalance(UInt32 volLeft, UInt32 volRight){
    unsigned char attnl, attnr;
    unsigned char LFvalue, RFValue;

               //first deal with the left side
    if(volLeft != gVolLeft) {
        readsgs7433 ( kLFAttnReg, &LFvalue);
        if( 0 == volLeft) {
            LFvalue |= sgsBalMuteBit; //0 we mute
        } else {
            if(0 == gVolLeft) { //we are leaving an unmute state
                LFvalue &= ~sgsBalMuteBit;
            }
            attnl = (unsigned char) (31-volLeft+1);
            LFvalue &=sgsBalMuteBit;
            LFvalue |= attnl;
        }
        writesgs7433( kLFAttnReg, LFvalue);
        writesgs7433( kLRAttnReg, LFvalue);
    }
        //now we deal with the right side
    if(volRight != gVolRight) {
        readsgs7433 ( kRFAttnReg, &RFValue);
        if( 0 == volRight) {
            RFValue|= sgsBalMuteBit; //0 we mute
        } else {
            if(0 == gVolRight)  {//we are laeving an unmute state
                RFValue&= ~sgsBalMuteBit; //0 we mute
            }
            attnr = (unsigned char) (31-volRight+1);
            RFValue &=sgsBalMuteBit;
            RFValue |= attnr;
        }
        writesgs7433( kRFAttnReg, RFValue);
        writesgs7433( kRRAttnReg, RFValue);
    }
    return(kIOReturnSuccess);
}

