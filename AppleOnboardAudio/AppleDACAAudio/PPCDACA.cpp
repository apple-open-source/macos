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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * Interface implementation for the DAC 3550A audio Controller
 *
 * HISTORY
 *
 *
 */

#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"

// Driver headers
#include "daca_hw.h"
#include "PPCDACA_inlined.h"
#include "PPCDACA.h"

#include "AppleDBDMAAudioDMAEngine.h"




#define super IOAudioDevice
OSDefineMetaClassAndStructors( PPCDACA, IOAudioDevice )

#define NUM_POWER_STATES	2

/* ==============
 * Public Methods
 * ============== */
bool
PPCDACA::init(OSDictionary * properties)
{
    CLOG("+ AppleDACAAudio : init \n");
    if (!super::init(properties))
            return false;
    
    gVolRight =0;
    gVolLeft = 0;
    gMute = false;
    layoutID = 0;
    
        // Initialize  the defualt registers to non zero values (so they will be revitten)
        //sampleRateReg = 0xFF;
    sampleRateReg = (UInt8) kLeftLRSelSR_REG | k1BitDelaySPSelSR_REG | kSRC_48SR_REG;
    analogVolumeReg = kPowerOnDefaultAVOL;
        //configurationReg = 0xFF;
    configurationReg = kInvertRightAmpGCFG | kDACOnGCFG | kSelect5VoltGCFG;

        // Mirror the analogVolumeReg for the speaker and for the headphones.
    mixerOutVolume = analogVolumeReg;

        // Last value of the status is 0
    lastStatus = 0;
 
    // Clears the interface
    interface = NULL;
    
    // Forget the provider:
    sound = NULL;
    headPhoneActive = false;
    // Initialize the sound format:
    dacaSerialFormat = kSndIOFormatUnknown;
    CLOG("- AppleDACAAudio : init\n");
    return true;
}

void
PPCDACA::free()
{
    CLOG("+ AppleDACAAudio : free \n");
    // Releases the sound:
    CLEAN_RELEASE(sound);
    super::free();
    CLOG("- AppleDACAAudio : free \n");
}

IOService*
PPCDACA::probe(IOService* provider, SInt32* score)
{
    
    CLOG("+ AppleDACAAudio : probe \n");

    super::probe(provider, score);
    *score = kIODefaultProbeScore;
    sound = provider->childFromPath("sound", gIODTPlane);
         //we are on a new world : the registry is assumed to be fixed
    if(sound) {
        OSData *tmpData;
        
        tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
        if(tmpData) {
            if(tmpData->isEqualTo(kDacaModelName, sizeof(kDacaModelName) -1)) {
                *score = *score+1;
                DEBUG_IOLOG("++ AppleDACAAudio::probe increasing score\n");
                return(this);
            } else {
                DEBUG_IOLOG ("++ AppleDACAAudio::probe, didn't find what we were looking for\n");
            }
        }
//        mySound->release ();
    }

    CLOG("- AppleDACAAudio : probe \n");
    return (0);
}

#define kNumDMAStreams 1
bool
PPCDACA::start(IOService* provider)
{
    // Gets the base for the DAC-3550 registers:
    IOMemoryMap *map;
    AppleDBDMAAudioDMAEngine	*driverDMAEngine;
    AbsoluteTime		timerInterval;
    OSObject *t = 0;
    IORegistryEntry			*theEntry, *tmpReg;
    IORegistryIterator *theIterator;
    
    CLOG("+ AppleDACAAudio : start \n");
    setManufacturerName("Apple");
    setDeviceName("Built-in audio controller");

        //get the video information
    theEntry = 0;
    theIterator  = IORegistryIterator::iterateOver(gIODTPlane, kIORegistryIterateRecursively);
    if(theIterator) {
        while (!theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, theIterator->getNextObject ())) != 0) {
                if(tmpReg->compareName(OSString::withCString("extint-gpio12"))) 
                    theEntry = tmpReg;
        }
        theIterator->release();
    } 
    
    if(theEntry) {
        t = theEntry->getProperty("video");
        if(t) 
            gAppleAudioVideoJackStateKey = OSSymbol::withCStringNoCopy ("AppleAudioVideoJackState");
        else 
            gAppleAudioVideoJackStateKey = 0;
    }

    
    if(!super::start(provider)) {
        return (false);
    }

    if (!findAndAttachI2C(provider))  {
        return (false);
    }

#if 0
    // This code ran once to find the address of the therms
    openI2C();
    int i;
    for (i = 0; i <128 ; i++) {
        SInt8 dataSD[2] = {1, 0x01};

        if (interface->writeI2CBus((UInt8)i, 0, (UInt8*)dataSD, sizeof(dataSD)))
            IOLog("**::start interface->writeI2CBus(0x%02x, 0) succeeded!!\n",(UInt8)i);
    }
    closeI2C();
#endif
    
    map = provider->mapDeviceMemoryWithIndex(AppleDBDMAAudioDMAEngine::kDBDMADeviceIndex);
    if(!map)  {
        return (false);
    }
    soundConfigSpace = (UInt8 *)map->getPhysicalAddress();

    driverDMAEngine = new AppleDBDMAAudioDMAEngine;
    if (!driverDMAEngine->init(0, provider, false)) {
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

        // sets the clock base address figuring out which I2S cell we're on
    if ((((UInt32)soundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) {
        ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S0BaseOffset);
        i2SInterfaceNumber = 0;
    }
    else if ((((UInt32)soundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) {
        ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S1BaseOffset);
        i2SInterfaceNumber = 1;
    }
    else {
        DEBUG_IOLOG("PPCDACA::start failed to setup ioBaseAddress and ioClockBaseAddress\n");
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
        CLOG("PPCDACA::CreateAudioTopology DAC-3550 setup failed\n");

   // DEBUG_IOLOG("PPCDACA::start() - writing registers: 0x%x, 0x%x, 0x%x\n", sampleRateReg, kPowerOnDefaultAVOL, configurationReg);

    if (openI2C()) {
        // And sends the data we need:
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, &sampleRateReg, sizeof(sampleRateReg));
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8 *)&analogVolumeReg, sizeof(analogVolumeReg));
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, &configurationReg, sizeof(configurationReg));

        closeI2C();
    }
    
    checkStatusRegister(true);

    nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
    addTimerEvent(this, &PPCDACA::timerCallback, timerInterval);
    //publishResource("setModemSound", this);
    CLOG("- AppleDACAAudio : start \n");
    return true;
}

void
PPCDACA::stop(IOService *provider)
{
    CLOG("+ AppleDACAAudio : stop \n");
    super::stop(provider);
    CLOG("- AppleDACAAudio : stop \n");
}

        //Power Management functions
IOReturn PPCDACA::performPowerStateChange(IOAudioDevicePowerState oldPowerState,
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

IOReturn PPCDACA::performDeviceSleep()
{
    IOReturn result = kIOReturnSuccess;
    
    CLOG("+ AppleDACAAudio : performDeviceSleep \n");
    
    if (result == kIOReturnSuccess) {
        sampleRateRegCopy = sampleRateReg;
        analogVolumeRegCopy = analogVolumeReg;
        configurationRegCopy = configurationReg;
    
        writeRegisterBits(i2cBusSubaddrGCFG, kLowPowerGCFG, kNoChangeMask);
    }
    CLOG("- AppleDACAAudio : performDeviceSleep \n");
    return result;
}

IOReturn PPCDACA::performDeviceWake()
{
    IOReturn result = kIOReturnSuccess;
    
    CLOG("+ AppleDACAAudio : performDeviceWake \n");   
    
    if (kIOReturnSuccess == result ) {
        sampleRateReg = sampleRateRegCopy;
        analogVolumeReg = analogVolumeRegCopy;
        configurationReg = configurationRegCopy;

        // Open the interface and sets it in the wanted mode:
    if (openI2C()) {
            // And sends the data we need:
            interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, &sampleRateRegCopy, sizeof(sampleRateRegCopy));
            interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8 *)&analogVolumeRegCopy, sizeof(analogVolumeRegCopy));
            interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, &configurationRegCopy, sizeof(configurationRegCopy));

            // Closes the bus so other can access to it:
            closeI2C();
        }
    }
    CLOG("- AppleDACAAudio : performDeviceWake \n");
    return result;
}

bool PPCDACA::createPorts(IOAudioEngine *driverDMAEngine)
{
    
    IOAudioPort *outputPort = 0;
   
    CLOG("+ AppleDACAAudio : createPorts \n");
    if (!driverDMAEngine) {
        DEBUG_IOLOG("PPCDACA::CreateAudioTopology called without available DMA engines\n");
        return false;
    }

    /*
     * Create output port //there is nothing on the input
     */

     outputPort = IOAudioPort::withAttributes(kIOAudioPortTypeOutput, "Output port");
    if (!outputPort) {
        return false;
    }

    outVolLeft = IOAudioLevelControl::createVolumeControl(kInitialVolume,
                                          kMinimumVolume,
                                          kMaximumVolume,
                                          (-75 << 16) + (32768), /* -75 in fixed point 16.16 */
                                          3<< 16,
                                          kIOAudioControlChannelIDDefaultLeft,
                                          kIOAudioControlChannelNameLeft,
                                          kOutVolLeft, 
                                          kIOAudioControlUsageOutput);
    if (!outVolLeft) {
        return false;
    }

    driverDMAEngine->addDefaultAudioControl(outVolLeft);
    outVolLeft->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeLeftChangeHandler, this);

    outVolRight = IOAudioLevelControl::createVolumeControl(kInitialVolume,
                                          kMinimumVolume,
                                          kMaximumVolume,
                                          (-75 << 16) + (32768),
                                          3<<16,
                                          kIOAudioControlChannelIDDefaultRight,
                                          kIOAudioControlChannelNameRight,
                                          kOutVolRight, 
                                          kIOAudioControlUsageOutput);
    if (!outVolRight) {
        return false;
    }
   // outputPort->addAudioControl(outVolRight);    
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
    
    attachAudioPort(outputPort, driverDMAEngine, 0);
    
    CLOG("- AppleDACAAudio : createPorts \n");
    return true;
}


void PPCDACA::timerCallback(OSObject *target, IOAudioDevice *device)
{
    PPCDACA *daca;
    
    daca = OSDynamicCast(PPCDACA, target);
    if (daca) {
        daca->checkStatusRegister(false);
    }
}

/* ===============
 * Private Methods
 * =============== */

// --------------------------------------------------------------------------
// Method: findAndAttachI2C
//
// Purpose:
//   Attaches to the i2c interface:
bool
PPCDACA::findAndAttachI2C(IOService *provider)
{
    const OSSymbol *i2cDriverName;
    IOService *i2cCandidate;

    // Searches the i2c:
    i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-mac-io");
    i2cCandidate = waitForService(resourceMatching(i2cDriverName));
    //interface = OSDynamicCast(PPCI2CInterface, i2cCandidate->getProperty(i2cDriverName));
    interface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

    if (interface == NULL) {
        CLOG("Thermal::findAndAttachI2C can't find the i2c in the registry\n");
        return false;
    }

    // Make sure that we hold the interface:
    interface->retain();

    return true;
}

// --------------------------------------------------------------------------
// Method: detachFromI2C
//
// Purpose:
//   detaches from the I2C
bool PPCDACA::detachFromI2C(IOService* /*provider*/)
{
    if (interface) {
        //delete interface;
        interface->release();
        
        interface = 0;
    }
        
    return (true);
}


// --------------------------------------------------------------------------
// Method: frameRate
//
// Purpose:
//        Should look in the registry : for now return a default value

#define kCommonFrameRate 44100

UInt32 PPCDACA::frameRate(UInt32 index) {
    return (UInt32)kCommonFrameRate;  
}


// --------------------------------------------------------------------------
// Method: getI2CPort
//
// Purpose:
//        returns the i2c port to use for the audio chip.
UInt32 PPCDACA::getI2CPort()
{
    if(sound) {
        OSData *t;

        t = OSDynamicCast(OSData, sound->getProperty("AAPL,i2c-port-select"));
        if (t != NULL) {
            UInt32 myPort = *((UInt32*)t->getBytesNoCopy());
            return myPort;
        } else
            CLOG( "PPCDACA::getI2CPort missing property port\n");
    }
    
    return 0;
}

// --------------------------------------------------------------------------
// Method: openI2C
//
// Purpose:
//        opens and sets up the i2c bus
bool PPCDACA::openI2C() {
    if (interface != NULL) {
        // Open the interface and sets it in the wanted mode:
        interface->openI2CBus(getI2CPort());
        interface->setStandardSubMode();

        // let's use the driver in a more intelligent way than the dafult one:
        interface->setPollingMode(false);
        
        return true;
    }
    return false;
}


// --------------------------------------------------------------------------
// Method: closeI2C
//

void
PPCDACA::closeI2C() {
    // Closes the bus so other can access to it:
    interface->closeI2CBus();
}

// --------------------------------------------------------------------------
// Method: dependentSetup
//
// Purpose:
//        this handles the setup of the DAC-3550 chip for each kind of
//        hosting hardware.
bool PPCDACA::dependentSetup()
{
    // Sets the frame rate:
    UInt32 myFrameRate = frameRate(0);

    //dacaSerialFormat = kSndIOFormatI2SSony;			// start out in Sony format
    dacaSerialFormat = kSndIOFormatI2S32x;
    
    // Reads the initial status of the DAC3550A registers:
    // The following functions return "false" on the first generation of iBooks. However I wish to
    // keep them, since:
    // 1] it is bad to make assumptions on what the hardware can do and can not do here (eventually these
    //    assumptions should be made in the i2c driver).
    // 2] the next generation of iBook may supprot reading the DACA registers, and we will have more precise
    //    values in the "mirror" registers.
    if (openI2C()) {
        // And reads the data we need:
        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, & sampleRateReg, sizeof(sampleRateReg));
        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8*)& analogVolumeReg, sizeof(analogVolumeReg));
        interface->readI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, & configurationReg, sizeof(configurationReg));

        closeI2C();
    }

    // If nobody set a format before choose one:
    if (dacaSerialFormat ==  kSndIOFormatUnknown)
        dacaSerialFormat = kSndIOFormatI2SSony;

    // This call will set the next of the drame parametes
    // (dacaClockSource, dacaMclkDivisor,  dacaSclkDivisor)
    if (!setSampleParameters(myFrameRate, 0)) {
        CLOG("PPCDACA::dependentSetup can not set i2s sample rate\n");
        return false;
    } else if (!setDACASampleRate(myFrameRate)) {
        CLOG("PPCDACA::dependentSetup can not set DACA sample rate\n");
        return false;
    } else {
        setSerialFormatRegister(dacaClockSource, dacaMclkDivisor, dacaSclkDivisor, dacaSerialFormat);
    }
    return true;
}

// --------------------------------------------------------------------------
// Method: checkStatusRegister
//
// Purpose:
//        if the argument is true mutes the internal speaker, otherwise
//        it "unmutes" it.
void
PPCDACA::checkStatusRegister(bool force)
{
    if (statusChanged() || force)  {
        OSNumber * num = OSNumber::withNumber(lastStatus, 8);
        setProperty("Status", num);
        num->release();
        debug2IOLog("New Status = 0x%02x\n", lastStatus);

		// We only want to tell the video driver that the headphone jack state changed if it really has.
		if ( force || headPhoneActive != headphonesInserted() ) {
			headPhoneActive = headphonesInserted();
        
			if(gAppleAudioVideoJackStateKey) {
				publishResource (gAppleAudioVideoJackStateKey, headPhoneActive ? kOSBooleanTrue : kOSBooleanFalse);
			}
		}
    }
}


// --------------------------------------------------------------------------
// Method: muteInternalSpeaker
//
// Purpose:
//        if the argument is true mutes the internal speaker, otherwise
//        it "unmutes" it.
void
PPCDACA::muteInternalSpeaker(bool mute)
{
    if (mute){
        if (!headphonesInserted())
            writeRegisterBits(i2cBusSubAddrAVOL, 0, kRightAVOLMask | kLeftAVOLMask);
        internalSpeakerMuted = mute;
    }
    else {
        internalSpeakerMuted = mute;

        if (!headphonesInserted())
            writeRegisterBits(i2cBusSubAddrAVOL, analogVolumeReg, kRightAVOLMask | kLeftAVOLMask);
    }
}

// --------------------------------------------------------------------------
// Method: muteHeadphones
//
// Purpose:
//        if the argument is true mutes the rear panel mini jack, otherwise
//        it "unmutes" it.
void
PPCDACA::muteHeadphones(bool mute)
{
    if (mute){
        if (headphonesInserted())
            writeRegisterBits(i2cBusSubAddrAVOL, 0, kRightAVOLMask | kLeftAVOLMask);
        internalHeadphonesMuted = mute;       
    }
    else {
        internalHeadphonesMuted = mute;        

        if (headphonesInserted())
            writeRegisterBits(i2cBusSubAddrAVOL, analogVolumeReg, kRightAVOLMask | kLeftAVOLMask);
    }
}

void
PPCDACA::muteMixerOut(bool mute)
{
    if (mute){
        writeRegisterBits(i2cBusSubAddrAVOL, 0, kRightAVOLMask | kLeftAVOLMask);
        mixerOutMuted = mute;
    } else {
        mixerOutMuted = mute;
        writeRegisterBits(i2cBusSubAddrAVOL, mixerOutVolume, kRightAVOLMask | kLeftAVOLMask);
    }
}


// --------------------------------------------------------------------------
// Method: statusChanged()  returns true if something changed in the status register:

bool PPCDACA::statusChanged() {
    UInt8 currentStatusRegister = *(UInt8*)ioStatusRegister_GPIO12;

    if (lastStatus == currentStatusRegister)
        return (false);

    lastStatus = currentStatusRegister;
    return (true);
}

// --------------------------------------------------------------------------
// Method: headphonesInserted ::returns true if the headphones are inserted.
bool PPCDACA::headphonesInserted() {
    return((lastStatus & kHeadphoneBit) == 0);
}



/* =============================================================
 * VERY Private Methods used to access to the DAC-3550 registers
 * ============================================================= */

// --------------------------------------------------------------------------
// Method: enableInterrupts   ::enable the interrupts for the I2S interface:

bool PPCDACA::enableInterrupts() {
    I2SSetIntCtlReg(kFrameCountEnable | kClocksStoppedEnable);
    return true;
}

// Method: disableInterrupts
//         disable the interrupts for the I2S interface:

bool PPCDACA::disableInterrupts() {
    I2SSetIntCtlReg(I2SGetIntCtlReg() & (~(kFrameCountEnable | kClocksStoppedEnable)));
    return true;
}

// --------------------------------------------------------------------------
// Method: clockRun  ::starts and stops the clock count:

bool PPCDACA::clockRun(bool start) {
    bool success = true;

    if (start) {
        KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) | kI2S0ClockEnable);
    } else {
        UInt16 loop = 50;
        KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) & (~kI2S0ClockEnable));
        
        while (((I2SGetIntCtlReg() & kClocksStoppedPending) == 0) && (loop--)) {
            // it does not do anything, jut waites for the clock
            // to stop
            IOSleep(10);
        }

        // we are successful if the clock actually stopped.
        success =  ((I2SGetIntCtlReg() & kClocksStoppedPending) != 0);
    }

    if (!success)
        debug2IOLog("PPCDACA::clockRun(%s) failed\n", (start ? "true" : "false"));

    return success;
}

// --------------------------------------------------------------------------
// Method: setSampleRate :: Sets the sample rate on the I2S bus

bool PPCDACA::setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio) {
    UInt32	mclkRatio;
    UInt32	reqMClkRate;

    mclkRatio = mclkToFsRatio;		// remember the MClk ratio required

    if ( mclkRatio == 0 )		// or make one up if MClk not required
        mclkRatio = 64;			// use 64 x ratio since that will give us the best characteristics
    
    reqMClkRate = sampleRate * mclkRatio;	// this is the required MClk rate

        // look for a source clock that divides down evenly into the MClk
    if ((kClock18MHz % reqMClkRate) == 0) {  // preferential source is 18 MHz
        dacaClockSource = kClock18MHz;
    } else if ((kClock45MHz % reqMClkRate) == 0) { // next check 45 MHz clock
        dacaClockSource = kClock45MHz;
    } else if ((kClock49MHz % reqMClkRate) == 0) { // last, try 49 Mhz clock
        dacaClockSource = kClock49MHz;
    } else {
        CLOG("PPCDACA::setSampleParameters Unable to find a suitable source clock (no globals changes take effect)\n");
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
            DEBUG_IOLOG("PPCDACA::setSampleParameters Invalid serial format\n");
            return false;
            break;
    }

    return true;
 }


// --------------------------------------------------------------------------
// Method: setSerialFormatRegister ::Set global values to the serial format register

void PPCDACA::setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat)
{
    UInt32	regValue = 0;

    switch ((int)clockSource) {
        case kClock18MHz:
            regValue = kClockSource18MHz;
            break;
        case kClock45MHz:
            regValue = kClockSource45MHz;
            break;
        case kClock49MHz:
            regValue = kClockSource49MHz;
            break;
        default:
            break;
    }

    switch (mclkDivisor) {
        case 1:
            regValue |= kMClkDivisor1;
            break;
        case 3:
            regValue |= kMClkDivisor3;
            break;
        case 5:
            regValue |= kMClkDivisor5;
            break;
        default:
            regValue |= (((mclkDivisor / 2) - 1) << kMClkDivisorShift) & kMClkDivisorMask;
            break;
    }

    switch ((int)sclkDivisor) {
        case 1:
            regValue |= kSClkDivisor1;
            break;
        case 3:
            regValue |= kSClkDivisor3;
            break;
        default:
            regValue |= (((sclkDivisor / 2) - 1) << kSClkDivisorShift) & kSClkDivisorMask;
            break;
    }
    regValue |= kSClkMaster;										// force master mode

    switch (serialFormat) {
        case kSndIOFormatI2SSony:
            regValue |= kSerialFormatSony;
            break;
        case kSndIOFormatI2S64x:
            regValue |= kSerialFormat64x;
            break;
        case kSndIOFormatI2S32x:
            regValue |= kSerialFormat32x;
            break;
        default:
            break;
    }

        // This is a 3 step process:

        // 1] Stop the clock:
    clockRun(false);
        // 2] Setup the serial format register
    I2SSetSerialFormatReg(regValue);
        // 3 restarts the clock:
    clockRun(true);    
}

// --------------------------------------------------------------------------
// Method: setDACASampleRate
//
// Purpose:
//        Gets the sample rate and makes it in a format that is compatible
//        with the adac register. The funtion returs false if it fails.
bool
PPCDACA::setDACASampleRate(UInt rate)
{
    UInt32 dacRate = 0;
    
    switch (rate) {
        case 44100: 				// 32 kHz - 48 kHz
            dacRate = kSRC_48SR_REG;
            break;
        default:
            break;
    }
    return(writeRegisterBits(i2cBusSubAddrSR_REG, dacRate, kSampleRateControlMask));
}


// --------------------------------------------------------------------------
// Method: setDACAVolume
//
//        sets the volume for the left or right channel. The first argument is
//        the wnated volume (as a range from 0 to 255), while the second argument
//        is a boolean (true for the right channel, false for the left channel).
bool PPCDACA::setDACAVolume(UInt16 volume, bool isLeft)
{
//    UInt8 newVolume = ((UInt32)((UInt32)volume * (UInt32)kVolumeRangeLevel_VOL) / (UInt32)65535 + (UInt32)kMinVolumeLevel_VOL);
    UInt8 newVolume = (UInt8) volume;
     
    
    bool success = true;
    DEBUG2_IOLOG("Writing new volume %d\n", newVolume);
    if (isLeft) // Sets the volume on the right channel
        success = writeRegisterBits(i2cBusSubAddrAVOL, (newVolume << kLeftAVOLShift), kLeftAVOLMask);
    else	// Sets the volume on the left channel
        success = writeRegisterBits(i2cBusSubAddrAVOL, (newVolume << kRightAVOLShift), kRightAVOLMask);

    return success;
}

// ---------------------------------------------------------------------------------------
// Method: writeRegisterBits
//
// Purpose:
//      This function sets or clears bits in the Daca registers.  The first argument is the
//      register sub-address, the second argument is a mask for turning bits on, while the third
//      argument is a mask for turning bits off.
bool
PPCDACA::writeRegisterBits(UInt8 subAddress, UInt32 bitMaskOn, UInt32 bitMaskOff)
{
    UInt8 bitsOn = 0, bitsOff = 0, value;
    UInt16 shortBitsOn = 0, shortBitsOff = 0, value16;
    bool success = false;

        // mask off irrelevant bytes
    if (subAddress == i2cBusSubAddrAVOL) {
            // 16-bit register
        shortBitsOn = bitMaskOn & 0x0000FFFF;
        shortBitsOff = bitMaskOff & 0x0000FFFF;
    }
    else {
            // 8-bit registers
        bitsOn = bitMaskOn & 0x000000FF;
        bitsOff = bitMaskOff & 0x000000FF;
    }

    if (openI2C()) {
        switch (subAddress) {
            case i2cBusSubAddrSR_REG:
                value = sampleRateReg | bitsOn;
                value &= ~bitsOff;
                // continue only if on bits are not already on and off bits are not already off
                if (value != sampleRateReg) {
                    // And sends the data we need:
                    success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, subAddress, &value, sizeof(value));

                    if (success)
                        sampleRateReg = value;
                } else {
                    success = true;
                }
                break;

            case i2cBusSubAddrAVOL:
                value16 = analogVolumeReg;
                value16 &= ~shortBitsOff;
                value16 |= shortBitsOn;
                // continue only if on bits are not already on and off bits are not already off
                if (value16 !=analogVolumeReg)
                {
                    // It changes the volume for real only if the current output is not muted.
                    if ((((!headphonesInserted()) && (!internalSpeakerMuted)) ||
                         (headphonesInserted() && (!internalHeadphonesMuted))) && !mixerOutMuted) {
                        // And sends the data we need:
                        success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, subAddress, (UInt8*)&value16, sizeof(value16));
                    }
                    else
                        success = true;

                    if (success)
                        analogVolumeReg = value16;
                } else {
                    // If the volume is already set at the wanted value
                    // we do not do anything, but we return success since from the
                    // caller point of view it is as the volume was correctly
                    // set.
                    success = true;
                }
                break;

            case i2cBusSubaddrGCFG:
                value = configurationReg | bitsOn;
                value &= ~bitsOff;
                // continue only if on bits are not already on and off bits are not already off
                if (value != configurationReg)
                {
                    // And sends the data we need:
                    success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, subAddress, &value, sizeof(value));

                    if (success)
                        configurationReg = value;
                } else {
                    // If the config register is already set at the wanted value
                    // we do not do anything, but we return success since from the
                    // caller point of view it is as the configuration was correctly
                    // set.
                    success = true;
                }
                break;

            default:
                DEBUG2_IOLOG("PPCDACA::writeRegisterBits 0x%x unknown subaddress\n", (UInt16)subAddress);
                break;
        }

        // We do not need this anymore:
        closeI2C();
    }
    
    return success;
}

IOReturn PPCDACA::setModemSound(bool state){
    return(kIOReturnError);
}


IOReturn PPCDACA::callPlatformFunction( const OSSymbol * functionName,bool
            waitForFunction,void *param1, void *param2, void *param3, void *param4 ){
            
    if(functionName->isEqualTo("setModemSound")) {
        return(setModemSound((bool)param1));
    }  
    return(super::callPlatformFunction(functionName,
            waitForFunction,param1, param2, param3, param4));
              
}


    
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Calculates the PRAM volume value for stereo volume.
 UInt8 PPCDACA::VolumeToPRAMValue( UInt32 leftVol, UInt32 rightVol )
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
void PPCDACA::WritePRAMVol(  UInt32 leftVol, UInt32 rightVol  )
{
	UInt8				pramVolume;
	UInt8 				curPRAMVol;
	IODTPlatformExpert * 		platform = NULL;
		
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());
        if (platform) {
		pramVolume = VolumeToPRAMValue(leftVol,rightVol);
        
		debug2IOLog("pramVolume = %d\n",pramVolume);
                    // get the old value to compare it with
		platform->readXPRAM((IOByteCount)kPRamVolumeAddr,&curPRAMVol, (IOByteCount)1);
		
		
                    // Update only if there is a change
		if (pramVolume != (curPRAMVol & 0x07)){
			// clear bottom 3 bits of volume control byte from PRAM low memory image
			curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;			
            debug2IOLog("curPRAMVol = 0x%x\n",curPRAMVol);
			// write out the volume control byte to PRAM
			platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
		}
	}
}

//Control Handlers
IOReturn PPCDACA::volumeLeftChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    PPCDACA *audioDevice;
    
    
    audioDevice = (PPCDACA *)target;
    if (audioDevice) { result = audioDevice->volumeLeftChanged(volumeControl, oldValue, newValue);}
    return result;
}

IOReturn PPCDACA::volumeLeftChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    
    setDACAVolume(newValue, false);
    gVolLeft = newValue;
    
    WritePRAMVol(gVolLeft,gVolRight);
   
    return(result);
}

    
IOReturn PPCDACA::volumeRightChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    PPCDACA *audioDevice;
    
    audioDevice = (PPCDACA *)target;
    if (audioDevice) { result = audioDevice->volumeRightChanged(volumeControl, oldValue, newValue);}
    return result;
}


IOReturn PPCDACA::volumeRightChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;    
    
    setDACAVolume(newValue, true);
    gVolRight = newValue;
    WritePRAMVol(gVolLeft,gVolRight);
    
    return result;
}

    
IOReturn PPCDACA::outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;
    PPCDACA *audioDevice;
    
    audioDevice = (PPCDACA *)target;
    if (audioDevice) { result = audioDevice->outputMuteChanged(muteControl, oldValue, newValue);}
    return result;
}

IOReturn PPCDACA::outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue){
    IOReturn result = kIOReturnSuccess;    
    
    if(! headPhoneActive) {
        
        muteInternalSpeaker(newValue != 0);
        muteHeadphones(newValue == 0);
    } else {
        muteHeadphones(newValue != 0);
        muteInternalSpeaker(newValue == 0);
    }
    return result;
}

