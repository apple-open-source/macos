/*
 *  AppleDACAAudio.c
 *  AppleOnboardAudio
 *
 *  Created by nthompso on Tue Jul 03 2001.
 *
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * This file contains a new version of the Apple DACA audio driver for Mac OS X.
 * The driver is derived from the AppleOnboardAudio class.
 *
 */
 
#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"

#include "AppleDACAAudio.h"
#include "daca_hw.h"

#include "AppleDBDMAAudioDMAEngine.h"

#include "AudioI2SControl.h"

// In debug mode we may wish to step trough the INLINEd methods, so:
#ifdef DEBUGMODE
#define INLINE
#else
#define INLINE	inline
#endif


#define super AppleOnboardAudio
OSDefineMetaClassAndStructors(AppleDACAAudio, AppleOnboardAudio)


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#pragma mark +UNIX LIKE FUNCTIONS

// init, free, proble are the "Classical Unix driver functions" that you'll like as not find
// in other device drivers.  Note that there are no start and stop methods.  The code for start 
// effectively moves to the sndHWInitialize routine.  Also note that the initHardware method 
// effectively does very little other than calling the inherited method, which in turn calls
// sndHWInitialize, so all of the init code should be in the sndHWInitialize routine.

// ::init()
// call into superclass and initialize.
bool AppleDACAAudio::init(
    OSDictionary *properties)
{
    DEBUG_IOLOG("+ AppleDACAAudio::init\n");

    // init our class vars
    fIsMuted = false ;			// true if we are muted
    fCachedAnalogVolumeReg = 0x00 ;	// used to store the last volume reg before mute
    
    if (!super::init(properties))
        return false;     

    // make and initialize an AudioI2SControl
    
    DEBUG_IOLOG("- AppleDACAAudio::init\n");
    return true;
}

// ::free
// call inherited free
void AppleDACAAudio::free()
{
    DEBUG_IOLOG("+ AppleDACAAudio::free\n");
    
    // free myAudioI2SControl
    CLEAN_RELEASE(myAudioI2SControl) ;

	publishResource (fAppleAudioVideoJackStateKey, NULL);
    super::free();
    
    DEBUG_IOLOG("- AppleDACAAudio::free\n");
}

// ::probe
// called at load time, to see if this driver really does match with a device.  In our
// case we check the registry to ensure we are loading on the appropriate hardware.
IOService* AppleDACAAudio::probe(
    IOService *provider, 
    SInt32 *score)
{

// debugger is described in IOLib.h.  Uncomment this line 
// to drop into the debugger when this routine is called.
//Debugger("Entering Probe") ;

        // Finds the possible candidate for sound, to be used in
        // reading the caracteristics of this hardware:
        
    DEBUG_IOLOG("+ AppleDACAAudio::probe\n");
    
    super::probe(provider, score);
    *score = kIODefaultProbeScore;
    sound = provider->childFromPath("sound", gIODTPlane);
    
         //we are on a new world : the registry is assumed to be fixed
         
    if(sound) 
    {
        OSData *tmpData;
        
        tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
        if(tmpData) 
        {
            if(tmpData->isEqualTo(kDacaModelName, sizeof(kDacaModelName) -1) ) 
            {
                *score = *score+1;
                DEBUG_IOLOG("- AppleDACAAudio::probe\n");
                return(this);
            } 
        } 
    }
     
    DEBUG_IOLOG("- AppleDACAAudio::probe\n");
    return (0);
}

// ::initHardware
// Don't do a whole lot in here, but do call the inherited inithardware method.
// in turn this is going to call sndHWInitialize to perform initialization.  All
// of the code to initialize and start the device needs to be in that routine, this 
// is kept as simple as possible.
bool AppleDACAAudio::initHardware(IOService *provider)
{
    bool myreturn = true;

    DEBUG_IOLOG("+ AppleDACAAudio::initHardware\n");
    
    // calling the superclasses initHarware will indirectly call our
    // sndHWInitialize() method.  So we don't need to do a whole lot 
    // in this function.
    super::initHardware(provider);
    
    DEBUG_IOLOG("- AppleDACAAudio::initHardware\n");
    return myreturn;
}

// --------------------------------------------------------------------------
// Method: timerCallback
//
// Purpose:
//        This is a static method that gets called from a timer task at regular intervals
//        Generally we do not want to do a lot of work here, we simply turn around and call
//        the appropriate method to perform out periodic tasks.

void AppleDACAAudio::timerCallback(OSObject *target, IOAudioDevice *device)
{
// probably don't want this on since we will get called a lot...
//    DEBUG_IOLOG("+ AppleDACAAudio::timerCallback\n");

    AppleDACAAudio *daca;
    daca = OSDynamicCast(AppleDACAAudio, target);
    if (daca) {
        daca->checkStatus(false);
    }
// probably don't want this on since we will get called a lot...
//    DEBUG_IOLOG("- AppleDACAAudio::timerCallback\n");
    return;
}



// --------------------------------------------------------------------------
// Method: checkStatus
//
// Purpose:
//       poll the detects, note this should prolly be done with interrupts rather
//       than by polling.

void AppleDACAAudio::checkStatus(
    bool force)
{
// probably don't want this on since we will get called a lot...
//    DEBUG_IOLOG("+ AppleDACAAudio::checkStatus\n");

    static UInt32 		lastStatus = 0L;
    UInt32				extdevices;
    AudioHardwareDetect 	*theDetect;
    OSArray 			*AudioDetects;
    void			*statusRegisterAddr ;
    UInt8 			currentStatusRegister ;
    UInt32 			i ;
    bool			cachedHeadphonesInserted ;

    // get the address of the current status register
    statusRegisterAddr = myAudioI2SControl->getIOStatusRegister_GPIO12() ;
    
    // get the value from the register
    currentStatusRegister = *(UInt8*)statusRegisterAddr ;
    
    // cache the value of fHeadphonesInserted
    cachedHeadphonesInserted = fHeadphonesInserted ;

    if(mCanPollStatus == false)
    {
        // probably don't want this on since we will get called a lot...
        // DEBUG_IOLOG("- AppleDACAAudio::checkStatus\n");
        return;
    }

    if (lastStatus != currentStatusRegister || force)  
    {
        lastStatus = currentStatusRegister;

        DEBUG2_IOLOG("AppleDACAAudio::checkStatus New Status = 0x%02x\n", currentStatusRegister);     

        AudioDetects = super::getDetectArray();
		extdevices = 0;
        if(AudioDetects) 
        {
            for(i = 0 ; i < AudioDetects->getCount() ; i++ ) 
            {
                theDetect = OSDynamicCast(AudioHardwareDetect, AudioDetects->getObject(i));
                if (theDetect) 
                    extdevices |= theDetect->refreshDevices(currentStatusRegister);
                else
                    DEBUG_IOLOG("The detect was null.  That's bad.\n") ;
            }
            if(i==0) DEBUG_IOLOG("AudioDetects->getCount() returned zero\n") ;
            super::setCurrentDevices(extdevices);
        } 
        else 
        {
            DEBUG_IOLOG("AppleDACAAudio::checkStatus couldn't get detect array\n");
        }
        
        // a side effect of the setCurrentDevices call is that our own sndHWSetActiveOutputExclusive will
        // get called.  This routine will set the fHeadphonesInserted field appropriately.  We can
        // then tell the video driver if something changed.  Note that we cache the value in 
        // fHeadphonesInserted at the top of this routine, and only call publishResource if the state changed

        // only call publishResource if we have a video out in the reg, and if the value of fHeadphonesInserted
        // changed while pollong the GPIO.
        if(fAppleAudioVideoJackStateKey != 0 && cachedHeadphonesInserted != fHeadphonesInserted) 
        {
            publishResource (fAppleAudioVideoJackStateKey, fHeadphonesInserted ? kOSBooleanTrue : kOSBooleanFalse);
        }
    }
// probably don't want this on since we will get called a lot...
//    DEBUG_IOLOG("- AppleDACAAudio::checkStatus\n");


}

/*************************** sndHWXXXX functions******************************/
/*   These functions should be common to all Apple hardware drivers and be   */
/*   declared as virtual in the superclass. This is the only place where we  */
/*   should manipulate the hardware. Order is given by the UI policy system  */
/*   The set of functions should be enough to implement the policy           */
/*****************************************************************************/


#pragma mark +HARDWARE REGISTER MANIPULATION
/************************** Hardware Register Manipulation ********************/
// Hardware specific functions : These are all virtual functions and we have to 
// implement these in the driver class

// ::sndHWInitialize
// hardware specific initialization needs to be in here, together with the code
// required to start audio on the device.
void 	AppleDACAAudio::sndHWInitialize(
    IOService *provider)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWInitialize\n");

    IOMemoryMap 		*map;
    OSObject 			*t = 0;
    IORegistryEntry		*theEntry ;
    IORegistryEntry		*tmpReg ;
    IORegistryIterator 		*theIterator;	// used to iterate the registry to find video things
    UInt32 			myFrameRate  = 0 ;
    
    // get the video jack information
    theEntry = 0;
    theIterator = IORegistryIterator::iterateOver(gIODTPlane, kIORegistryIterateRecursively);
    if(theIterator) 
    {
        while (!theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, theIterator->getNextObject ())) != 0) 
        {
                if(tmpReg->compareName(OSString::withCString("extint-gpio12"))) 
                    theEntry = tmpReg;
        }
        theIterator->release();
    } 
    
    if(theEntry) {
        t = theEntry->getProperty("video");
        if(t) 
            fAppleAudioVideoJackStateKey = OSSymbol::withCStringNoCopy ("AppleAudioVideoJackState");
        else 
            fAppleAudioVideoJackStateKey = 0;
    }

    map = provider->mapDeviceMemoryWithIndex(AppleDBDMAAudioDMAEngine::kDBDMADeviceIndex);
    if(!map)  
    {
        DEBUG_IOLOG("AppleDACAAudio::sndHWInitialize ERROR: unable to mapDeviceMemoryWithIndex\n");
        goto error_exit ;
    }
    

    // the i2s stuff is in a separate class.  make an instance of the class and store
    AudioI2SInfo tempInfo ;
    tempInfo.map = map ;
    tempInfo.i2sSerialFormat = kSndIOFormatI2SSony ;
    
    // create an object, this will get inited.
    myAudioI2SControl = AudioI2SControl::create(&tempInfo) ;

    if(myAudioI2SControl == NULL)
    {
        DEBUG_IOLOG("AppleDACAAudio::sndHWInitialize ERROR: unable to i2s control object\n");
        goto error_exit ;
    }
    myAudioI2SControl->retain() ;
    
    // set the sample rate for the part
    myFrameRate = frameRate(0) ;
    setDACASampleRate(myFrameRate) ;
    
    // build a connection to the i2c bus
    if (!findAndAttachI2C(provider)) 
    {
        DEBUG_IOLOG("AppleDACAAudio::sndHWInitialize ERROR: unable to find and attach i2c\n");
        goto error_exit ;
    }
   
    
    // NOTE: for the internal speaker we assume that we are writing a stereo stream to a mono part.  For built in
    // audio we need to invert the right channel.  For external we need to ensure it is not inverted (doing so 
    // will lead to phase cancellation with a sub-woofer).
    
    // initialize the shadow regs to a known value, these get written out to set up the hardware.
    sampleRateReg	= (UInt8)kLeftLRSelSR_REG | k1BitDelaySPSelSR_REG | kSRC_48SR_REG ;
    analogVolumeReg	= kPowerOnDefaultAVOL ;
    configurationReg	= (UInt8)kInvertRightAmpGCFG | kDACOnGCFG | kSelect5VoltGCFG ;
    
   debug4IOLog("AppleDACAAudio::sndHWInitialize - writing registers: 0x%x, 0x%x, 0x%x\n", sampleRateReg, analogVolumeReg, configurationReg);

    if (openI2C()) 
    {
        // And sends the data we need:
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, &sampleRateReg, sizeof(sampleRateReg));
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8 *)&analogVolumeReg, sizeof(analogVolumeReg));
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, &configurationReg, sizeof(configurationReg));

        closeI2C();
    }
    
error_exit:
    DEBUG_IOLOG("- AppleDACAAudio::sndHWInitialize\n");
    return ;
}

void AppleDACAAudio::sndHWPostDMAEngineInit (IOService *provider) {
    AbsoluteTime		timerInterval;

	// this calls the callback timer routine the first time...
	mCanPollStatus = true;    
	checkStatus(true);

	nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
	addTimerEvent(this, &AppleDACAAudio::timerCallback, timerInterval);
//	registerService();
//	publishResource("setModemSound", this);
}

UInt32 	AppleDACAAudio::sndHWGetInSenseBits(
    void)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetInSenseBits\n");
    DEBUG_IOLOG("- AppleDACAAudio::sndHWGetInSenseBits\n");
    return 0;     
}

// we can't read the registers back, so return the value in the shadow reg.
UInt32 	AppleDACAAudio::sndHWGetRegister(
    UInt32 regNum)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetRegister\n");
    
    UInt32	returnValue = 0;
    
    switch (regNum) 
    {
        case i2cBusSubAddrSR_REG:
            returnValue = sampleRateReg ;
            break;

        case i2cBusSubAddrAVOL:
            returnValue = analogVolumeReg ;
            break;

        case i2cBusSubaddrGCFG:
            returnValue = configurationReg ;
            break;

        default:
            DEBUG2_IOLOG("AppleDACAAudio::sndHWGetRegister 0x%x unknown subaddress\n", (UInt16)regNum);
            break;
    }
    
     return returnValue;
}

// set the reg over i2c and make sure the value is cached in the shadow reg so we can "get it back"
IOReturn  AppleDACAAudio::sndHWSetRegister(
    UInt32 regNum, 
    UInt32 val)
{
    UInt8	value8	;
    UInt16	value16	;
    bool	success	;
    UInt8 	subAddress ;
    IOReturn 	myReturn ;

    DEBUG_IOLOG("+ AppleDACAAudio::sndHWSetRegister\n");
    
    value8	= (val & 0x000000FF) ;	// just ensure we didn't pass any junk in, mask off the bottom 8 bits
    value16	= (val & 0x0000FFFF) ;	// just ensure we didn't pass any junk in, mask off the bottom 16 bits
    success	= false ;			// indication of the success of the 12c write
    subAddress	= (regNum & 0x000000FF) ;	// just ensure we didn't pass any junk in, mask off the bottom 8 bits
    myReturn 	= kIOReturnSuccess ;	// try to return something meaningful in the event of a problem

    if (openI2C()) 
    {
        switch (subAddress) 
        {
            case i2cBusSubAddrSR_REG:
                // send the data to the device
                success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, &value8, sizeof(value8));
                DEBUG2_IOLOG("writing status register %02x\n", (UInt16)value8);

                if (success)
                    sampleRateReg = value8;
                
                break;
    
            case i2cBusSubAddrAVOL:                
                // set the register via i2c...
                success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8*)&value16, sizeof(value16));
                DEBUG2_IOLOG("writing analog vol register %04x\n", value16);
                
                if (success)
                    analogVolumeReg = value16;
                    
                break;

            case i2cBusSubaddrGCFG:

                // And send the data we need:
                success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, &value8, sizeof(value8));
                DEBUG2_IOLOG("writing config reg %02x\n", (UInt16)value8);
                
                if (success)
                    configurationReg = value8;
                    
                break;

            default:
                DEBUG2_IOLOG("AppleDACAAudio::sndHWSetRegister 0x%x unknown register\n", (UInt16)subAddress);
                success = true ;
                myReturn = kIOReturnBadArgument ;
                break;
        }

        // We do not need i2c anymore, free up so others can use
        closeI2C();
    }
    
    if(success != true)
    {
        myReturn = kIOReturnError ;
        DEBUG_IOLOG("sndHWSetRegister: something went wrong\n") ;
    }
    
        
    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetRegister\n");
    return(myReturn);
}

#pragma mark +HARDWARE IO ACTIVATION
/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/

UInt32	AppleDACAAudio::sndHWGetActiveOutputExclusive(
    void)
{
    UInt32	returnVal = 0l;
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetActiveOutputExclusive\n");
    if( fHeadphonesInserted == false )
        returnVal = kSndHWOutput1 ;
    else
        returnVal = kSndHWOutput2 ;
    
    DEBUG_IOLOG("- AppleDACAAudio::sndHWGetActiveOutputExclusive\n");
    return 0;
}

IOReturn   AppleDACAAudio::sndHWSetActiveOutputExclusive(
    UInt32 outputPort )
{
    IOReturn 	myReturn ;
    UInt8 	tmpConfigReg ;
                                                        
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWSetActiveOutputExclusive\n");
    
    myReturn = kIOReturnBadArgument;		// assume that the arg is not recognized in the switch
                                                // if it is then set the value of myReturn in there.
                                                
    switch( outputPort ) 
    {

        case kSndHWOutput1:		// mono internal speaker
            fHeadphonesInserted = false ;   
            // set the                      
            tmpConfigReg = setBitsGCFGShadowReg(kInvertRightAmpGCFG, kInvertRightAmpGCFG) ;
            myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
            break;
                
        case kSndHWOutput2:		// stereo headphones
            fHeadphonesInserted = true ;                        
            tmpConfigReg = setBitsGCFGShadowReg(!kInvertRightAmpGCFG, kInvertRightAmpGCFG) ;
            myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
            break;
                
        default:
            DEBUG_IOLOG("Invalid set output active request\n");
            break;
    }
    

    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetActiveOutputExclusive\n");
    return(myReturn);
}

UInt32 	AppleDACAAudio::sndHWGetActiveInputExclusive(
    void)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetActiveInputExclusive\n");
    DEBUG_IOLOG("- AppleDACAAudio::sndHWGetActiveInputExclusive\n");
    return fActiveInput;
}

IOReturn   AppleDACAAudio::sndHWSetActiveInputExclusive(
    UInt32 input )
{
    IOReturn 	myReturn = kIOReturnBadArgument;
    UInt8	tmpConfigReg = 0;
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWSetActiveInputExclusive\n");

    switch (input)
    {
            case kSndHWInputNone:

                    tmpConfigReg = setBitsGCFGShadowReg(kNoChangeMask, kAuxOneGCFG | kAuxTwoGCFG) ;
                    myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
                    break;
                    
            case kSndHWInput1:		// CD
                    tmpConfigReg = setBitsGCFGShadowReg(kAuxOneGCFG, kAuxTwoGCFG) ;
                    myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
                    break;
                    
            case kSndHWInput2:		// modem call progress
                    tmpConfigReg = setBitsGCFGShadowReg(kAuxTwoGCFG, kAuxOneGCFG) ;
                    myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
                    break;
                    
            default:
                    DEBUG_IOLOG("Invalid set output active request\n");
                    break;
    }
            
    fActiveInput = input;	
    
    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetActiveInputExclusive\n");
    return(myReturn);
}

#pragma mark +CONTROL FUNCTIONS
// control function
bool AppleDACAAudio::sndHWGetSystemMute(void)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetSystemMute\n");
    DEBUG_IOLOG("- AppleDACAAudio::sndHWGetSystemMute\n");
    return fIsMuted;
}

// mute the part.  Something here is odd.  We seem to get called twice to mute the part.
// Just having a chached value for the avol reg won't work: you get called once, so you
// cache the avol reg in the cached value.  You call through to set the avol reg to zero,
// which mutes the part.  Then we get called again.  Avol has already been set to zero,
// so we now cache that.  We then get called to unmute, so we restore the (wrong) zero'd
// value from the cached avol reg.  Whic is an error, since we actually just want to restore
// the non zero value that was in the avol register before we began with this charade.
// This is why we need to make the value in fCachedAnalogVolumeReg sticky.  We init
// it to zero when the class is constructed.  From then on, if we've ever been muted with a 
// volume other than zero that value should stick.
IOReturn AppleDACAAudio::sndHWSetSystemMute(bool mutestate)
{
    IOReturn	retval = kIOReturnSuccess; ;
    UInt16	tmpAnalogVolumeReg ;
    
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWSetSystemMute\n");
        
    
    if( true == mutestate )
    {
        if( false == fIsMuted )
        {
        
            fIsMuted = mutestate ;
            // mute the part
            
            // cache the value in the shadow analog volume reg
            fCachedAnalogVolumeReg = analogVolumeReg;	// used to store the last volume reg before mute
                            
            // set the shadow reg to zero and write it to the part
            tmpAnalogVolumeReg = setBitsAVOLShadowReg(0x00, kLeftAVOLMask | kRightAVOLMask) ;
            retval = sndHWSetRegister(i2cBusSubAddrAVOL, tmpAnalogVolumeReg);
        }
    }
    else
    {
        // unmute the part
        fIsMuted = mutestate ;
        
        // set the shadow reg to the cached value and write it to the part
        tmpAnalogVolumeReg = setBitsAVOLShadowReg(fCachedAnalogVolumeReg, kLeftAVOLMask | kRightAVOLMask) ;
        retval = sndHWSetRegister(i2cBusSubAddrAVOL, tmpAnalogVolumeReg);
   
    } 

    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetSystemMute\n");
    return(retval);
}

bool AppleDACAAudio::sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume)
{

    bool 	retval = false ;
    UInt16	tmpAnalogVolumeReg ;
    
    DEBUG3_IOLOG("+ AppleDACAAudio::sndHWSetSystemVolume (left: %ld, right %ld)\n", leftVolume, rightVolume);
    
    // we want to manipulate the daca registers for the volume
    
    // Sets the volume on the left channel
    tmpAnalogVolumeReg = setBitsAVOLShadowReg(
                            ((leftVolume << kLeftAVOLShift) | (rightVolume << kRightAVOLShift)), 
                            (kLeftAVOLMask | kRightAVOLMask) ) ;
                            
    retval = sndHWSetRegister(i2cBusSubAddrAVOL, tmpAnalogVolumeReg);
    
    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetSystemVolume\n");
    return (retval==kIOReturnSuccess);
}

IOReturn AppleDACAAudio::sndHWSetSystemVolume(UInt32 value)
{
    DEBUG2_IOLOG("+ AppleDACAAudio::sndHWSetSystemVolume (vol: %ld)\n", value);
    
    IOReturn myReturn = kIOReturnError ;
        
    // just call the default function in this class with the same val for left and right.
    if( true == sndHWSetSystemVolume( value, value ))
    {
        myReturn = kIOReturnSuccess;
    }

    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetSystemVolume\n");
    return(myReturn);
}

IOReturn AppleDACAAudio::sndHWSetPlayThrough(bool playthroughstate)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWSetPlayThrough\n");

    IOReturn myReturn = kIOReturnSuccess;

    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetPlayThrough\n");
    return(myReturn);
}

IOReturn AppleDACAAudio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) 
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWSetPlayThrough\n");

    IOReturn myReturn = kIOReturnSuccess;

    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetPlayThrough\n");
    return(myReturn);
}


#pragma mark +INDENTIFICATION

// ::sndHWGetType
//Identification - the only thing this driver supports is the DACA3550 part, return that.
UInt32 AppleDACAAudio::sndHWGetType( 
    void )
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetType\n");
    
    UInt32	returnValue = kSndHWTypeDaca ; 	// I think this is the only one supported by this driver

    DEBUG_IOLOG("- AppleDACAAudio::sndHWGetType\n");
    
    return returnValue ;
}

// ::sndHWGetManufactuer
// return the detected part's manufacturer.  I think Daca is a single sourced part
// from Micronas Intermetall.  Always return just that.
UInt32 AppleDACAAudio::sndHWGetManufacturer( 
    void )
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetManufacturer\n");
    
    UInt32	returnValue = kSndHWManfMicronas ;
    
    DEBUG_IOLOG("- AppleDACAAudio::sndHWGetManufacturer\n");
    return returnValue ;
}


#pragma mark +DETECT ACTIVATION & DEACTIVATION
// ::setDeviceDetectionActive
// turn on detection, TODO move to superclass?? 
void AppleDACAAudio::setDeviceDetectionActive(
    void)
{
    DEBUG_IOLOG("+ AppleDACAAudio::setDeviceDetectionActive\n");
    mCanPollStatus = true ;
    
    DEBUG_IOLOG("- AppleDACAAudio::setDeviceDetectionActive\n");
    return ;
}

// ::setDeviceDetectionInActive
// turn off detection, TODO move to superclass?? 
void AppleDACAAudio::setDeviceDetectionInActive(
    void)
{
    DEBUG_IOLOG("+ AppleDACAAudio::setDeviceDetectionInActive\n");
    mCanPollStatus = false ;
    
    DEBUG_IOLOG("- AppleDACAAudio::setDeviceDetectionInActive\n");
    return ;
}

#pragma mark +POWER MANAGEMENT
    //Power Management
IOReturn AppleDACAAudio::sndHWSetPowerState(
    IOAudioDevicePowerState theState)
{
    IOReturn 	myReturn  ;
    UInt16	tmpAnalogVolReg ;
    UInt8	tmpConfigReg ;
    
    
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWSetPowerState\n");
    
    myReturn = kIOReturnSuccess;

    switch(theState)
    {
        case kIOAudioDeviceSleep :	// When sleeping
        case kIOAudioDeviceIdle	:	// When no audio engines running
            // mute the part
            tmpAnalogVolReg = 0x00 ;
            
            // set the part to low power mode
            tmpConfigReg = 0x0 ;
            
             if (openI2C()) 
             {
                // And sends the data we need:
                interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8 *)&tmpAnalogVolReg, sizeof(tmpAnalogVolReg));
                interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, &tmpConfigReg, sizeof(tmpConfigReg));
            
                // Closes the bus so others can access to it:
                closeI2C();
            }
            break ;
       
        case kIOAudioDeviceActive :	// audio engines running

            // Open the interface and reset all values
            if (openI2C()) 
            {
                // And sends the data we need:
                interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, &configurationReg, sizeof(configurationReg));
                interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, &sampleRateReg, sizeof(sampleRateReg));
                interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8 *)&analogVolumeReg, sizeof(analogVolumeReg));
            
                // Closes the bus so others can access to it:
                closeI2C();
            }
            break ;
        default:
            DEBUG_IOLOG("AppleDACAAudio::sndHWSetPowerState unknown power state\n");
            myReturn = kIOReturnBadArgument ;
            break ;
        
    }

    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetPowerState\n");
    return(myReturn);
}
    
// ::sndHWGetConnectedDevices
// TODO: Move to superclass
UInt32 AppleDACAAudio::sndHWGetConnectedDevices(
    void)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetConnectedDevices\n");
   UInt32	returnValue = currentDevices;

    DEBUG_IOLOG("- AppleDACAAudio::sndHWGetConnectedDevices\n");
    return returnValue ;
}

UInt32 AppleDACAAudio::sndHWGetProgOutput(
    void)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWGetProgOutput\n");
    DEBUG_IOLOG("- AppleDACAAudio::sndHWGetProgOutput\n");
    return 0;
}

IOReturn AppleDACAAudio::sndHWSetProgOutput(
    UInt32 outputBits)
{
    DEBUG_IOLOG("+ AppleDACAAudio::sndHWSetProgOutput\n");

    IOReturn myReturn = kIOReturnSuccess;

    DEBUG_IOLOG("- AppleDACAAudio::sndHWSetProgOutput\n");
    return(myReturn);
}

#pragma mark +PRIVATE ROUTINES FOR ACCESSING I2C

/* ===============
 * Private Methods
 * =============== */

// --------------------------------------------------------------------------
// Method: findAndAttachI2C
//
// Purpose:
//   Attaches to the i2c interface:
bool AppleDACAAudio::findAndAttachI2C(
    IOService *provider)
{
    DEBUG_IOLOG("+ AppleDACAAudio::findAndAttachI2C\n");
    const OSSymbol 	*i2cDriverName;
    IOService 		*i2cCandidate;

    // Searches the i2c:
    i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-mac-io");
    i2cCandidate = waitForService(resourceMatching(i2cDriverName));
    //interface = OSDynamicCast(PPCI2CInterface, i2cCandidate->getProperty(i2cDriverName));
    interface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

    if (interface == NULL) {
        DEBUG_IOLOG("- AppleDACAAudio::findAndAttachI2C ERROR: can't find the i2c in the registry\n");
        return false;
    }

    // Make sure that we hold the interface:
    interface->retain();

    DEBUG_IOLOG("- AppleDACAAudio::findAndAttachI2C\n");
    return true;
}

// --------------------------------------------------------------------------
// Method: detachFromI2C
//
// Purpose:
//   detaches from the I2C
bool AppleDACAAudio::detachFromI2C(
    IOService* /*provider*/)
{
    DEBUG_IOLOG("+ AppleDACAAudio::detachFromI2C\n");
    
    if (interface) 
    {
        //delete interface;
        interface->release();
        interface = 0;
    }
        
    DEBUG_IOLOG("- AppleDACAAudio::detachFromI2C\n");
    return (true);
}


// --------------------------------------------------------------------------
// Method: getI2CPort
//
// Purpose:
//	returns the i2c port to use for the audio chip.  if there is a problem
//	ascertaining the port from the regstry, then return nil.

UInt32 AppleDACAAudio::getI2CPort(
    void)
{
    UInt32 	myPort = 0L;	// the i2c port we will return, null if not found
    
    DEBUG_IOLOG("+ AppleDACAAudio::getI2CPort\n");

    if(sound) {
        OSData 		*t ;

        t = OSDynamicCast(OSData, sound->getProperty("AAPL,i2c-port-select")) ;
        if (t != NULL) 
        {
            myPort = *((UInt32*)t->getBytesNoCopy());
        } 
        else
        {
            DEBUG_IOLOG( "AppleDACAAudio::getI2CPort ERROR missing property port\n");
            myPort = 0L ; 
        }
    }
    
    DEBUG_IOLOG( "- AppleDACAAudio::getI2CPort\n");
    return myPort;
}

// --------------------------------------------------------------------------
// Method: openI2C
//
// Purpose:
//        opens and sets up the i2c bus
bool AppleDACAAudio::openI2C(
    void) 
{
    DEBUG_IOLOG( "+ AppleDACAAudio::openI2C\n");

    bool	canOpenI2C = false ;	// our return value, indicates whether we were able to open i2c bus
    
    if (interface != NULL) 
    {
        // Open the interface and sets it in the wanted mode:
        if(interface->openI2CBus(getI2CPort()) == true)
        {
            interface->setStandardSubMode();
    
            // let's use the driver in a more intelligent way than the dafult one:
            interface->setPollingMode(false);
            
            canOpenI2C = true ;
        }
    }
    
    DEBUG_IOLOG( "- AppleDACAAudio::openI2C\n");
    return canOpenI2C ;
}


// --------------------------------------------------------------------------
// Method: closeI2C
//

void AppleDACAAudio::closeI2C(
    void) 
{
    DEBUG_IOLOG( "+ AppleDACAAudio::closeI2C\n");
    
    // Closes the bus so others can access to it:
    interface->closeI2CBus();
    
    DEBUG_IOLOG( "- AppleDACAAudio::closeI2C\n");
}

#pragma mark + GENERAL PURPOSE UTILITY ROUTINES


// --------------------------------------------------------------------------
// Method: frameRate
//
// Purpose:
//        Should look in the registry : for now return a default value

#define kCommonFrameRate 44100

UInt32 AppleDACAAudio::frameRate(
    UInt32 index) 
{
    return (UInt32)kCommonFrameRate;  
}


// --------------------------------------------------------------------------
// Method: setDACASampleRate
//
// Purpose:
//        Gets the sample rate and makes it in a format that is compatible
//        with the adac register. The function returns false if it fails.
bool AppleDACAAudio::setDACASampleRate(
    UInt rate)
{
    UInt32 dacRate = 0;
    
    switch (rate) 
    {
        case 44100: 				// 32 kHz - 48 kHz
            dacRate = kSRC_48SR_REG;
            break;
            
        default:
            break;
    }
    return(sndHWSetRegister(i2cBusSubAddrSR_REG, setBitsSR_REGShadowReg(dacRate, kSampleRateControlMask)));
}


