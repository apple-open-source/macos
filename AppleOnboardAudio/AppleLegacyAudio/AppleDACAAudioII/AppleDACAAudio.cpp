/*
 *  AppleDACAAudio.c
 *  Apple02Audio
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
 * The driver is derived from the Apple02Audio class.
 *
 */
 
#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioI2SHardwareConstants.h"

#include "AppleDACAAudio.h"
#include "daca_hw.h"

#include "Apple02DBDMAAudioDMAEngine.h"

#include "AudioI2SControl.h"

#define super Apple02Audio
OSDefineMetaClassAndStructors(AppleDACAAudio, Apple02Audio)

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
    debugIOLog (3, "+ AppleDACAAudio::init");

    if (!super::init(properties))
        return false;     

    debugIOLog (3, "- AppleDACAAudio::init");
    return true;
}

// ::free
// call inherited free
void AppleDACAAudio::free()
{
    debugIOLog (3, "+ AppleDACAAudio::free");
    
	CLEAN_RELEASE(ioBaseAddressMemory);		//	[3060321]
    // free myAudioI2SControl
    CLEAN_RELEASE(myAudioI2SControl) ;

	publishResource (fAppleAudioVideoJackStateKey, NULL);
    super::free();
    
    debugIOLog (3, "- AppleDACAAudio::free");
}

//====================================================================================================
// ::probe
// called at load time, to see if this driver really does match with a device.  In our
// case we check the registry to ensure we are loading on the appropriate hardware.
IOService* AppleDACAAudio::probe(IOService *provider, SInt32 *score)
{
	// Finds the possible candidate for sound, to be used in
	// reading the caracteristics of this hardware:
        
    debugIOLog (3, "+ AppleDACAAudio::probe");
    
    super::probe(provider, score);
    *score = kIODefaultProbeScore;
    sound = provider->childFromPath("sound", gIODTPlane);
    
	// we are on a new world : the registry is assumed to be fixed
         
    if(sound) 
    {
        OSData *tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
        if(tmpData) 
        {
            if(tmpData->isEqualTo(kDacaModelName, sizeof(kDacaModelName) -1) ) 
            {
                *score = *score+1;
                debugIOLog (3, "- AppleDACAAudio::probe");
                return(this);
            } 
        } 
    }
     
    debugIOLog (3, "- AppleDACAAudio::probe");
    return (0);
}

//====================================================================================================
// ::initHardware
// Don't do a whole lot in here, but do call the inherited inithardware method.
// in turn this is going to call sndHWInitialize to perform initialization.  All
// of the code to initialize and start the device needs to be in that routine, this 
// is kept as simple as possible.
bool AppleDACAAudio::initHardware(IOService *provider)
{
    bool myreturn = true;

    debugIOLog (3, "+ AppleDACAAudio::initHardware");
    
    // calling the superclasses initHarware will indirectly call our
    // sndHWInitialize() method.  So we don't need to do a whole lot 
    // in this function.
    super::initHardware(provider);
    
    debugIOLog (3, "- AppleDACAAudio::initHardware");
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
//    debugIOLog (3, "+ AppleDACAAudio::timerCallback");

    AppleDACAAudio *daca;
    daca = OSDynamicCast(AppleDACAAudio, target);
    if (daca) {
        daca->checkStatus(false);
    }
// probably don't want this on since we will get called a lot...
//    debugIOLog (3, "- AppleDACAAudio::timerCallback");
    return;
}



// --------------------------------------------------------------------------
// Method: checkStatus
//
// Purpose:
//       poll the detects, note this should probably be done with interrupts rather
//       than by polling.

void AppleDACAAudio::checkStatus(bool force)
{
// probably don't want this on since we will get called a lot...
//    debugIOLog (3, "+ AppleDACAAudio::checkStatus");

    static UInt32		lastStatus = 0L;
    UInt32				extdevices;
    AudioHardwareDetect	*theDetect;
    OSArray 			*AudioDetects;
    UInt8 				currentStatusRegister = 0;
    UInt32 				i;
	UInt32				numDetects;
    bool				cachedHeadphonesInserted ;

    // get the value from the register

	currentStatusRegister = ioConfigurationBaseAddress[kEXTINT_GPIO12];		//	[3060321]
	
    // cache the value of fHeadphonesInserted
    cachedHeadphonesInserted = fHeadphonesInserted ;

    if(mCanPollStatus == false)
    {
        // probably don't want this on since we will get called a lot...
        // debugIOLog (3, "- AppleDACAAudio::checkStatus");
        return;
    }

    if (lastStatus != currentStatusRegister || force)  
    {
		// debugIOLog (3,  "AppleDACAAudio::checkStatus currentStatusRegister UPDATED to %X", currentStatusRegister );
        lastStatus = currentStatusRegister;

        AudioDetects = super::getDetectArray();
		extdevices = 0;
        if(AudioDetects) 
        {
			numDetects = AudioDetects->getCount();
            for(i = 0 ; i < numDetects ; i++ ) 
            {
                theDetect = OSDynamicCast(AudioHardwareDetect, AudioDetects->getObject(i));
                if (theDetect) 
                    extdevices |= theDetect->refreshDevices(currentStatusRegister);
                else
                    debugIOLog (3, "The detect was null.  That's bad.") ;
            }
            if(i==0) debugIOLog (3, "AudioDetects->getCount() returned zero") ;
            super::setCurrentDevices(extdevices);
			//	Opportunity to omit Balance controls if on the internal mono speaker.	[3046950]
			AdjustControls ();
        } 
        else 
        {
            debugIOLog (3, "AppleDACAAudio::checkStatus couldn't get detect array");
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
//    debugIOLog (3, "- AppleDACAAudio::checkStatus");
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
void 	AppleDACAAudio::sndHWInitialize(IOService *provider)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWInitialize");

    IOMemoryMap 			*map;
    OSObject 				*t = 0;
    IORegistryEntry			*theEntry;
    IORegistryEntry			*tmpReg;
    IORegistryIterator		*theIterator;					// used to iterate the registry to find video things
    
	//	[3060321]	begin {		Obtain the address of the headphone detect GPIO.  Then map it for later use.
	//	NOTE:	The original iBook does not have an 'AAPL,address' property.  It is
	//			necessary to derive the address of the memory mapped I/O register from
	//			the base address of the GPIO node which has an 'AAPL,address' property.
    // get the video jack information
	map = provider->mapDeviceMemoryWithIndex ( Apple02DBDMAAudioDMAEngine::kDBDMADeviceIndex );
	if ( map ) {
		soundConfigSpace = (UInt8*)map->getPhysicalAddress();
		if ((((UInt32)soundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) 
		{
			ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S0BaseOffset);
		}
		else if ((((UInt32)soundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) 
		{
			ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S1BaseOffset);
		}
		if ( ioBaseAddress ) {
			ioBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)ioBaseAddress), 256);
			ioConfigurationBaseAddress = (UInt8*)ioBaseAddressMemory->map()->getVirtualAddress();
		}
	}
	//	[3060321]	} end
	
	minVolume = kDACA_MINIMUM_HW_VOLUME;				// [3046950] minimum hardware volume setting
	maxVolume = kDACA_MAXIMUM_HW_VOLUME;				// [3046950] maximum hardware volume setting]

    // get the video jack information
    theEntry = 0;
    theIterator = IORegistryIterator::iterateOver(gIODTPlane, kIORegistryIterateRecursively);
    if(theIterator) 
    {
        while (!theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, theIterator->getNextObject ())) != 0) 
        {
			if( tmpReg->compareName(OSString::withCString("extint-gpio12")) ) 
				theEntry = tmpReg;
        }
        theIterator->release();
    } 

	if ( theEntry ) {
		t = theEntry->getProperty("video");
		if(t) 
			fAppleAudioVideoJackStateKey = OSSymbol::withCStringNoCopy ("AppleAudioVideoJackState");
		else 
			fAppleAudioVideoJackStateKey = 0;
	}
    
    map = provider->mapDeviceMemoryWithIndex(Apple02DBDMAAudioDMAEngine::kDBDMADeviceIndex);
    if(!map)  
    {
        debugIOLog (3, "AppleDACAAudio::sndHWInitialize ERROR: unable to mapDeviceMemoryWithIndex");
        goto error_exit ;
    }
    

    // the i2s stuff is in a separate class.  make an instance of the class and store
    AudioI2SInfo tempInfo;
    tempInfo.map = map;
    tempInfo.i2sSerialFormat = kSerialFormatSony;				//	[3060321]	rbm	2 Oct 2002
    
    // create an object, this will get inited.
    myAudioI2SControl = AudioI2SControl::create(&tempInfo) ;

    if(myAudioI2SControl == NULL)
    {
        debugIOLog (3, "AppleDACAAudio::sndHWInitialize ERROR: unable to i2s control object");
        goto error_exit ;
    }
    myAudioI2SControl->retain();
    
    // set the sample rate for the part
	dataFormat = ( ( 0 << kNumChannelsInShift ) | kDataIn16 | ( 2 << kNumChannelsOutShift ) | kDataOut16 );	//	[3060321]	rbm	2 Oct 2002

	myAudioI2SControl->setSampleParameters(kDACA_FRAME_RATE, 0, &clockSource, &mclkDivisor, &sclkDivisor, kSndIOFormatI2S32x);
	//	[3060321]	The data word format register and serial format register require that the I2S clocks be stopped and
	//				restarted before the register value is applied to operation of the I2S IOM.  We now pass the data
	//				word format to setSerialFormatRegister as that method stops the clocks when applying the value
	//				to the serial format register.  That method now also sets the data word format register while
	//				the clocks are stopped.		rbm	2 Oct 2002
	myAudioI2SControl->setSerialFormatRegister(clockSource, mclkDivisor, sclkDivisor, kSndIOFormatI2S32x, dataFormat);
    
	setDACASampleRate(kDACA_FRAME_RATE);
    
    // build a connection to the i2c bus
    if (!findAndAttachI2C(provider)) 
    {
        debugIOLog (3, "AppleDACAAudio::sndHWInitialize ERROR: unable to find and attach i2c");
        goto error_exit ;
    }
   
    
    // NOTE: for the internal speaker we assume that we are writing a stereo stream to a mono part.  For built in
    // audio we need to invert the right channel.  For external we need to ensure it is not inverted (doing so 
    // will lead to phase cancellation with a sub-woofer).
    
    // initialize the shadow regs to a known value, these get written out to set up the hardware.
    sampleRateReg	= (UInt8)kLeftLRSelSR_REG | k1BitDelaySPSelSR_REG | kSRC_48SR_REG ;
    analogVolumeReg	= kPowerOnDefaultAVOL ;
    configurationReg	= (UInt8)kInvertRightAmpGCFG | kDACOnGCFG | kSelect5VoltGCFG ;
    
   debugIOLog (3, "AppleDACAAudio::sndHWInitialize - writing registers: 0x%x, 0x%x, 0x%x", sampleRateReg, analogVolumeReg, configurationReg);

    if (openI2C()) 
    {
        // And sends the data we need:
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrSR_REG, &sampleRateReg, sizeof(sampleRateReg));
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8 *)&analogVolumeReg, sizeof(analogVolumeReg));
        interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, &configurationReg, sizeof(configurationReg));

        closeI2C();
    }
    
error_exit:
    debugIOLog (3, "- AppleDACAAudio::sndHWInitialize");
    return ;
}

//====================================================================================================
void AppleDACAAudio::sndHWPostDMAEngineInit (IOService *provider) {
    AbsoluteTime		timerInterval;

	// this calls the callback timer routine the first time...
	mCanPollStatus = true;    
	checkStatus(true);

	nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
	addTimerEvent(this, &AppleDACAAudio::timerCallback, timerInterval);

	if (NULL == outVolRight && NULL != outVolLeft) {
		// If they are running mono at boot time, set the right channel's last value to an illegal value
		// so it will come up in stereo and center balanced if they plug in speakers or headphones later.
		lastRightVol = kDACA_OUT_OF_BOUNDS_HW_VOLUME;
		lastLeftVol = outVolLeft->getIntValue ();
	}

}

//====================================================================================================
UInt32 	AppleDACAAudio::sndHWGetInSenseBits(void)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetInSenseBits");
    debugIOLog (3, "- AppleDACAAudio::sndHWGetInSenseBits");
    return 0;     
}

//====================================================================================================
// we can't read the registers back, so return the value in the shadow reg.
UInt32 	AppleDACAAudio::sndHWGetRegister(UInt32 regNum)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetRegister");
    
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
            debugIOLog (3, "AppleDACAAudio::sndHWGetRegister 0x%x unknown subaddress", (UInt16)regNum);
            break;
    }
    
     return returnValue;
}

//====================================================================================================
// set the reg over i2c and make sure the value is cached in the shadow reg so we can "get it back"
IOReturn  AppleDACAAudio::sndHWSetRegister(UInt32 regNum, UInt32 val)
{
    UInt8	value8	;
    UInt16	value16	;
    bool	success	;
    UInt8 	subAddress ;
    IOReturn 	myReturn ;

    debugIOLog (3, "+ AppleDACAAudio::sndHWSetRegister");
    
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
                debugIOLog (3, "writing status register %02x", (UInt16)value8);

                if (success)
                    sampleRateReg = value8;
                
                break;
    
            case i2cBusSubAddrAVOL:                
                // set the register via i2c...
                success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubAddrAVOL, (UInt8*)&value16, sizeof(value16));
                debugIOLog (3, "writing analog vol register %04x", value16);
                
                if (success)
                    analogVolumeReg = value16;
                    
                break;

            case i2cBusSubaddrGCFG:

                // And send the data we need:
                success = interface->writeI2CBus((UInt8)i2cBusAddrDAC3550A, i2cBusSubaddrGCFG, &value8, sizeof(value8));
                debugIOLog (3, "writing config reg %02x, success = %d", (UInt16)value8, success);
                
                if (success)
                    configurationReg = value8;
                    
                break;

            default:
                debugIOLog (3, "AppleDACAAudio::sndHWSetRegister 0x%x unknown register", (UInt16)subAddress);
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
        debugIOLog (3, "sndHWSetRegister: something went wrong") ;
    }
    
    debugIOLog (3, "- AppleDACAAudio::sndHWSetRegister");
    return(myReturn);
}

UInt32 AppleDACAAudio::sndHWGetCurrentSampleFrame (void) {
	return myAudioI2SControl->GetFrameCountReg ();
}

void AppleDACAAudio::sndHWSetCurrentSampleFrame (UInt32 value) {
	myAudioI2SControl->SetFrameCountReg (value);
}

#pragma mark +HARDWARE IO ACTIVATION
/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/

UInt32	AppleDACAAudio::sndHWGetActiveOutputExclusive(void)
{
    UInt32	returnVal = 0l;
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetActiveOutputExclusive");
    if( fHeadphonesInserted == false )
        returnVal = kSndHWOutput1 ;
    else
        returnVal = kSndHWOutput2 ;
    
    debugIOLog (3, "- AppleDACAAudio::sndHWGetActiveOutputExclusive");
    return 0;
}

//====================================================================================================
IOReturn   AppleDACAAudio::sndHWSetActiveOutputExclusive(UInt32 outputPort )
{
    IOReturn			myReturn;
    UInt8				tmpConfigReg;

    debugIOLog (3, "+ AppleDACAAudio::sndHWSetActiveOutputExclusive");

    myReturn = kIOReturnBadArgument;		// assume that the arg is not recognized in the switch
                                                // if it is then set the value of myReturn in there.

    switch( outputPort ) 
    {

        case kSndHWOutput1:		// mono internal speaker
            fHeadphonesInserted = false ;   
            tmpConfigReg = setBitsGCFGShadowReg(kInvertRightAmpGCFG, kNoChangeMask) ;
            myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
			if (NULL != driverDMAEngine) {
				useMasterVolumeControl = TRUE;
			}
            break;
        case kSndHWOutput2:		// stereo headphones
            fHeadphonesInserted = true ;                        
            tmpConfigReg = setBitsGCFGShadowReg(kNoChangeMask, kInvertRightAmpGCFG) ;
            myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
			if (NULL != driverDMAEngine) {
				useMasterVolumeControl = FALSE;
			}
            break;
        default:
            debugIOLog (3, "Invalid set output active request");
            break;
    }

    debugIOLog (3, "- AppleDACAAudio::sndHWSetActiveOutputExclusive");
    return(myReturn);
}

//====================================================================================================
UInt32 	AppleDACAAudio::sndHWGetActiveInputExclusive(void)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetActiveInputExclusive");
    debugIOLog (3, "- AppleDACAAudio::sndHWGetActiveInputExclusive");
    return fActiveInput;
}

//====================================================================================================
IOReturn   AppleDACAAudio::sndHWSetActiveInputExclusive(UInt32 input )
{
    IOReturn 	myReturn = kIOReturnBadArgument;
    UInt8	tmpConfigReg = 0;
    debugIOLog (3, "+ AppleDACAAudio::sndHWSetActiveInputExclusive");

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
			debugIOLog (3, "Invalid set output active request");
			break;
    }
            
    fActiveInput = input;	
    
    debugIOLog (3, "- AppleDACAAudio::sndHWSetActiveInputExclusive");
    return(myReturn);
}

// --------------------------------------------------------------------------
// You either have only a master volume control, or you have both volume controls.
IOReturn AppleDACAAudio::AdjustControls (void) {
	IOFixed							mindBVol;
	IOFixed							maxdBVol;
	Boolean							mustUpdate;

	debugIOLog (3, "+ AdjustControls()");
	FailIf (NULL == driverDMAEngine, Exit);
	mustUpdate = FALSE;

	mindBVol = kDACA_MIN_VOLUME;
	maxdBVol = kDACA_MAX_VOLUME;

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
			0 != mindBVol & 0x80000000 ? (unsigned int)(( mindBVol >> 16 ) | 0xFFFF0000) : (unsigned int)(mindBVol >> 16), (unsigned int)(mindBVol << 16), 
			0 != maxdBVol & 0x80000000 ? (unsigned int)(( maxdBVol >> 16 ) | 0xFFFF0000) : (unsigned int)(maxdBVol >> 16), (unsigned int)(maxdBVol << 16) );
	
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
				if (lastLeftVol > kDACA_MAXIMUM_HW_VOLUME && NULL != outVolMaster) {
					lastLeftVol = outVolMaster->getIntValue ();
				}
				outVolLeft = IOAudioLevelControl::createVolumeControl (lastLeftVol, kDACA_MINIMUM_HW_VOLUME, kDACA_MAXIMUM_HW_VOLUME, mindBVol, maxdBVol,
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
				if (lastRightVol > kDACA_MAXIMUM_HW_VOLUME && NULL != outVolMaster) {
					lastRightVol = outVolMaster->getIntValue ();
				}
				outVolRight = IOAudioLevelControl::createVolumeControl (lastRightVol, kDACA_MINIMUM_HW_VOLUME, kDACA_MAXIMUM_HW_VOLUME, mindBVol, maxdBVol,
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

#pragma mark +CONTROL FUNCTIONS
// control function
bool AppleDACAAudio::sndHWGetSystemMute(void)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetSystemMute");
    debugIOLog (3, "- AppleDACAAudio::sndHWGetSystemMute");
	return gIsMute;
}

//====================================================================================================
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
    IOReturn	retval = kIOReturnSuccess;
    UInt16		tmpAnalogVolumeReg;

    debugIOLog (3, "+ AppleDACAAudio::sndHWSetSystemMute");

    if (true == mutestate) {
		// set the shadow reg to zero and write it to the part
		tmpAnalogVolumeReg = setBitsAVOLShadowReg(0x00, kLeftAVOLMask | kRightAVOLMask) ;
		retval = sndHWSetRegister(i2cBusSubAddrAVOL, tmpAnalogVolumeReg);
	} else {
		// set the shadow reg to the current volume values and write it to the part
		tmpAnalogVolumeReg = setBitsAVOLShadowReg(((gVolLeft << kLeftAVOLShift) | (gVolRight << kRightAVOLShift)), (kLeftAVOLMask | kRightAVOLMask));
		retval = sndHWSetRegister(i2cBusSubAddrAVOL, tmpAnalogVolumeReg);
	} 

    debugIOLog (3, "- AppleDACAAudio::sndHWSetSystemMute");
    return(retval);
}

//====================================================================================================
bool AppleDACAAudio::sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume)
{

    bool 	retval = false ;
    UInt16	tmpAnalogVolumeReg ;
    
    debugIOLog (3, "+ AppleDACAAudio::sndHWSetSystemVolume (left: %ld, right %ld)", leftVolume, rightVolume);
    
    // we want to manipulate the daca registers for the volume
    
    // Sets the volume on the left channel
    tmpAnalogVolumeReg = setBitsAVOLShadowReg(
                            ((leftVolume << kLeftAVOLShift) | (rightVolume << kRightAVOLShift)), 
                            (kLeftAVOLMask | kRightAVOLMask) ) ;
                            
    retval = sndHWSetRegister(i2cBusSubAddrAVOL, tmpAnalogVolumeReg);
    
    debugIOLog (3, "- AppleDACAAudio::sndHWSetSystemVolume");
    return (retval==kIOReturnSuccess);
}

//====================================================================================================
IOReturn AppleDACAAudio::sndHWSetSystemVolume(UInt32 value)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWSetSystemVolume (vol: %ld)", value);
    
    IOReturn myReturn = kIOReturnError ;
        
    // just call the default function in this class with the same val for left and right.
    if( true == sndHWSetSystemVolume( value, value ))
    {
        myReturn = kIOReturnSuccess;
    }

    debugIOLog (3, "- AppleDACAAudio::sndHWSetSystemVolume");
    return(myReturn);
}

//====================================================================================================
IOReturn AppleDACAAudio::sndHWSetPlayThrough(bool playthroughState)
{
	IOReturn myReturn;
    UInt8	tmpConfigReg = 0;

    debugIOLog (3, "+ AppleDACAAudio::sndHWSetPlayThrough");

	if (playthroughState) {			// reenable any active aux input
		switch (fActiveInput) {
			case kSndHWInputNone:
				myReturn = kIOReturnSuccess;
				break;
			case kSndHWInput1:		// CD
				tmpConfigReg = setBitsGCFGShadowReg(kAuxOneGCFG, kNoChangeMask) ;
				myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
				break;
			case kSndHWInput2:		// modem call progress
				tmpConfigReg = setBitsGCFGShadowReg(kAuxTwoGCFG, kNoChangeMask) ;
				myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
				break;
			default:
				myReturn = kIOReturnError;
				break;
		}
	} else {							// turn off all aux inputs
		tmpConfigReg = setBitsGCFGShadowReg(kNoChangeMask, kAuxOneGCFG | kAuxTwoGCFG) ;
		myReturn = sndHWSetRegister(i2cBusSubaddrGCFG, tmpConfigReg);
	}

	gIsPlayThroughActive = playthroughState;

    debugIOLog (3, "- AppleDACAAudio::sndHWSetPlayThrough");
    return(myReturn);
}

//====================================================================================================
IOReturn AppleDACAAudio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) 
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWSetSystemInputGain");

    IOReturn myReturn = kIOReturnSuccess;

    debugIOLog (3, "- AppleDACAAudio::sndHWSetSystemInputGain");
    return(myReturn);
}


#pragma mark +INDENTIFICATION

//====================================================================================================
// ::sndHWGetType
//Identification - the only thing this driver supports is the DACA3550 part, return that.
UInt32 AppleDACAAudio::sndHWGetType( void )
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetType");
    
    UInt32	returnValue = kSndHWTypeDaca ; 	// I think this is the only one supported by this driver

    debugIOLog (3, "- AppleDACAAudio::sndHWGetType");
    
    return returnValue ;
}

//====================================================================================================
// ::sndHWGetManufactuer
// return the detected part's manufacturer.  I think Daca is a single sourced part
// from Micronas Intermetall.  Always return just that.
UInt32 AppleDACAAudio::sndHWGetManufacturer( void )
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetManufacturer");
    
    UInt32	returnValue = kSndHWManfMicronas ;
    
    debugIOLog (3, "- AppleDACAAudio::sndHWGetManufacturer");
    return returnValue ;
}


#pragma mark +DETECT ACTIVATION & DEACTIVATION
//====================================================================================================
// ::setDeviceDetectionActive
// turn on detection, TODO move to superclass?? 
void AppleDACAAudio::setDeviceDetectionActive(void)
{
    debugIOLog (3, "+ AppleDACAAudio::setDeviceDetectionActive");
    mCanPollStatus = true ;
    
    debugIOLog (3, "- AppleDACAAudio::setDeviceDetectionActive");
    return ;
}

//====================================================================================================
// ::setDeviceDetectionInActive
// turn off detection, TODO move to superclass?? 
void AppleDACAAudio::setDeviceDetectionInActive(void)
{
    debugIOLog (3, "+ AppleDACAAudio::setDeviceDetectionInActive");
    mCanPollStatus = false ;
    
    debugIOLog (3, "- AppleDACAAudio::setDeviceDetectionInActive");
    return ;
}

#pragma mark +POWER MANAGEMENT
//====================================================================================================
// Power Management
IOReturn AppleDACAAudio::sndHWSetPowerState(IOAudioDevicePowerState theState)
{
    IOReturn		myReturn;
    
    debugIOLog (3, "+ AppleDACAAudio::sndHWSetPowerState");
    
    myReturn = kIOReturnSuccess;
    switch(theState)
    {
        case kIOAudioDeviceSleep:	myReturn = performDeviceIdleSleep();	break;		//	When sleeping
        case kIOAudioDeviceIdle:	myReturn = performDeviceSleep();		break;		//	When no audio engines running
        case kIOAudioDeviceActive:	myReturn = performDeviceWake();			break;		//	audio engines running
        default:
            debugIOLog (3, "AppleDACAAudio::sndHWSetPowerState unknown power state");
            myReturn = kIOReturnBadArgument ;
            break ;
    }

    debugIOLog (3, "- AppleDACAAudio::sndHWSetPowerState");
    return(myReturn);
}
    
// --------------------------------------------------------------------------
IOReturn	AppleDACAAudio::performDeviceWake () {
    IOReturn				myReturn;
	IOService				*keyLargo;
	UInt32					temp;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]
    
    debugIOLog (3, "+ AppleDACAAudio::performDeviceWake");

    myReturn = kIOReturnSuccess;

	//	[power support]	rbm	10 Oct 2002		added I2S clock management across sleep / wake via KeyLargo
	keyLargo = NULL;
	keyLargo = IOService::waitForService ( IOService::serviceMatching ( "KeyLargo" ) );

	temp = myAudioI2SControl->GetSerialFormatReg();
	temp = myAudioI2SControl->GetDataWordSizesReg();
	if ( NULL != keyLargo ) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		//	Turn ON the I2S clocks
		keyLargo->callPlatformFunction ( funcSymbolName, false, (void*)true, (void*)0, 0, 0 );
		IODelay ( 100 );
		funcSymbolName->release ();															//	[3323977]
	}
	//	Restore I2S registers	(rbm 11 Oct 2002)
	myAudioI2SControl->setSampleParameters(kDACA_FRAME_RATE, 0, &clockSource, &mclkDivisor, &sclkDivisor, kSndIOFormatI2S32x);
	myAudioI2SControl->setSerialFormatRegister(clockSource, mclkDivisor, sclkDivisor, kSndIOFormatI2S32x, dataFormat);
	setDACASampleRate(kDACA_FRAME_RATE);
	
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
	temp = myAudioI2SControl->GetSerialFormatReg();
	temp = myAudioI2SControl->GetDataWordSizesReg();
Exit:
    debugIOLog (3, "- AppleDACAAudio::performDeviceWake");
    return(myReturn);
}

// --------------------------------------------------------------------------
IOReturn	AppleDACAAudio::performDeviceSleep () {
    IOReturn		myReturn;
    UInt16			tmpAnalogVolReg;
    UInt8			tmpConfigReg;
	IOService		*keyLargo;
	UInt32			temp;
    
    debugIOLog (3, "+ AppleDACAAudio::performDeviceSleep");
	
    myReturn = kIOReturnSuccess;

	//	[power support]	rbm	10 Oct 2002		added I2S clock management across sleep / wake via KeyLargo
	keyLargo = NULL;
	keyLargo = IOService::waitForService ( IOService::serviceMatching ( "KeyLargo" ) );

	temp = myAudioI2SControl->GetSerialFormatReg();
	temp = myAudioI2SControl->GetDataWordSizesReg();
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
	if ( NULL != keyLargo ) {
		//	Turn OFF the I2S clocks
		IODelay ( 100 );
		keyLargo->callPlatformFunction ( OSSymbol::withCString ( "keyLargo_powerI2S" ), false, (void*)false, (void*)0, 0, 0 );
	}
	temp = myAudioI2SControl->GetSerialFormatReg();
	temp = myAudioI2SControl->GetDataWordSizesReg();

    debugIOLog (3, "- AppleDACAAudio::performDeviceSleep");
    return(myReturn);
}

// --------------------------------------------------------------------------
IOReturn	AppleDACAAudio::performDeviceIdleSleep () {
	IOReturn	myReturn;
	
    debugIOLog (3, "+ AppleDACAAudio::performDeviceIdleSleep");
	myReturn = performDeviceSleep();
    debugIOLog (3, "- AppleDACAAudio::performDeviceIdleSleep");
    return(myReturn);
}


//====================================================================================================
// ::sndHWGetConnectedDevices
// TODO: Move to superclass
UInt32 AppleDACAAudio::sndHWGetConnectedDevices(void)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetConnectedDevices");
   UInt32	returnValue = currentDevices;

    debugIOLog (3, "- AppleDACAAudio::sndHWGetConnectedDevices");
    return returnValue ;
}

//====================================================================================================
UInt32 AppleDACAAudio::sndHWGetProgOutput(void)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWGetProgOutput");
    debugIOLog (3, "- AppleDACAAudio::sndHWGetProgOutput");
    return 0;
}

//====================================================================================================
IOReturn AppleDACAAudio::sndHWSetProgOutput(UInt32 outputBits)
{
    debugIOLog (3, "+ AppleDACAAudio::sndHWSetProgOutput");

    IOReturn myReturn = kIOReturnSuccess;

    debugIOLog (3, "- AppleDACAAudio::sndHWSetProgOutput");
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
bool AppleDACAAudio::findAndAttachI2C(IOService *provider)
{
    debugIOLog (3, "+ AppleDACAAudio::findAndAttachI2C");
    const OSSymbol 	*i2cDriverName;
    IOService 		*i2cCandidate;

    // Searches the i2c:
    i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-mac-io");
    i2cCandidate = waitForService(resourceMatching(i2cDriverName));
    // interface = OSDynamicCast(PPCI2CInterface, i2cCandidate->getProperty(i2cDriverName));
    interface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

    if (interface == NULL) {
        debugIOLog (3, "- AppleDACAAudio::findAndAttachI2C ERROR: can't find the i2c in the registry");
        return false;
    }

    // Make sure that we hold the interface:
    interface->retain();

    debugIOLog (3, "- AppleDACAAudio::findAndAttachI2C");
    return true;
}

// --------------------------------------------------------------------------
// Method: detachFromI2C
//
// Purpose:
//   detaches from the I2C
bool AppleDACAAudio::detachFromI2C(IOService* /*provider*/)
{
    debugIOLog (3, "+ AppleDACAAudio::detachFromI2C");
    
    if (interface) 
    {
        //delete interface;
        interface->release();
        interface = 0;
    }
        
    debugIOLog (3, "- AppleDACAAudio::detachFromI2C");
    return (true);
}


// --------------------------------------------------------------------------
// Method: getI2CPort
//
// Purpose:
//	returns the i2c port to use for the audio chip.  if there is a problem
//	ascertaining the port from the regstry, then return nil.

UInt32 AppleDACAAudio::getI2CPort(void)
{
    UInt32 	myPort = 0L;	// the i2c port we will return, null if not found
    
    debugIOLog (3, "+ AppleDACAAudio::getI2CPort");

    if(sound) {
        OSData 		*t ;

        t = OSDynamicCast(OSData, sound->getProperty("AAPL,i2c-port-select")) ;
        if (t != NULL) 
        {
            myPort = *((UInt32*)t->getBytesNoCopy());
        } 
        else
        {
            debugIOLog (3,  "AppleDACAAudio::getI2CPort ERROR missing property port");
            myPort = 0L ; 
        }
    }
    
    debugIOLog (3,  "- AppleDACAAudio::getI2CPort");
    return myPort;
}

// --------------------------------------------------------------------------
// Method: openI2C
//
// Purpose:
//        opens and sets up the i2c bus
bool AppleDACAAudio::openI2C(void) 
{
    debugIOLog (3,  "+ AppleDACAAudio::openI2C");

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
    
    debugIOLog (3,  "- AppleDACAAudio::openI2C");
    return canOpenI2C ;
}


// --------------------------------------------------------------------------
// Method: closeI2C
//

void AppleDACAAudio::closeI2C(void) 
{
    debugIOLog (3,  "+ AppleDACAAudio::closeI2C");
    
    // Closes the bus so others can access to it:
    interface->closeI2CBus();
    
    debugIOLog (3,  "- AppleDACAAudio::closeI2C");
}

#pragma mark + GENERAL PURPOSE UTILITY ROUTINES


// --------------------------------------------------------------------------
// Method: frameRate
//
// Purpose:
//        Should look in the registry : for now return a default value

#define kCommonFrameRate 44100

UInt32 AppleDACAAudio::frameRate(UInt32 index) 
{
    return (UInt32)kCommonFrameRate;  
}


// --------------------------------------------------------------------------
// Method: setDACASampleRate
//
// Purpose:
//        Gets the sample rate and makes it in a format that is compatible
//        with the adac register. The function returns false if it fails.
bool AppleDACAAudio::setDACASampleRate(UInt rate)
{
    UInt32 dacRate;
    
    dacRate = 44100 == rate ? kSRC_48SR_REG : 0;
    return ( sndHWSetRegister ( i2cBusSubAddrSR_REG, setBitsSR_REGShadowReg(dacRate, kSampleRateControlMask ) ) );
}

// --------------------------------------------------------------------------
IORegistryEntry * AppleDACAAudio::FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSData					*tmpData;

	theEntry = NULL;
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


