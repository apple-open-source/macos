/*
 *  AppleDACAAudio.h
 *  AppleOnboardAudio
 *
 *  Created by nthompso on Thu Jul 05 2001.
 *  Copyright (c) 2001 Apple. All rights reserved.
 *
 */

/*
 * Contains the interface definition for the DAC 3550A audio Controller
 * This is the audio controller used in the original iBook.
 */
 
 
#ifndef _APPLEDACAAUDIO_H
#define _APPLEDACAAUDIO_H

#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/i2c/PPCI2CInterface.h>

#include "AppleOnboardAudio.h"
#include "AudioHardwareDetect.h"
#include "AudioI2SControl.h"

// In debug mode we may wish to step trough the INLINEd methods, so:
#ifdef DEBUGMODE
#define INLINE
#else
#define INLINE	inline
#endif

// declare a class for our driver.  This is based from AppleOnboardAudio

class AppleDACAAudio : public AppleOnboardAudio
{
    OSDeclareDefaultStructors(AppleDACAAudio);

    friend class AudioHardwareOutput;
    friend class AudioHardwareInput;
    friend class AudioHardwareMux;

protected:
    bool	mCanPollStatus;			// set if we can look at the detects

    // Hardware register manipulation
    virtual void 	sndHWInitialize(IOService *provider) ;
	virtual void	sndHWPostDMAEngineInit (IOService *provider);

    virtual UInt32 	sndHWGetInSenseBits(void) ;
    virtual UInt32 	sndHWGetRegister(UInt32 regNum) ;
    virtual IOReturn   	sndHWSetRegister(UInt32 regNum, UInt32 value) ;

    // IO activation functions
    virtual  UInt32	sndHWGetActiveOutputExclusive(void);
    virtual  IOReturn   sndHWSetActiveOutputExclusive(UInt32 outputPort );
    virtual  UInt32 	sndHWGetActiveInputExclusive(void);
    virtual  IOReturn   sndHWSetActiveInputExclusive(UInt32 input );
    
    // control functions
    virtual  bool   	sndHWGetSystemMute(void);
    virtual  IOReturn  	sndHWSetSystemMute(bool mutestate);
    virtual  bool   	sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume);
    virtual  IOReturn   sndHWSetSystemVolume(UInt32 value);
    virtual  IOReturn	sndHWSetPlayThrough(bool playthroughstate);
    virtual  IOReturn   sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) ;
   

    //Identification
    virtual UInt32 	sndHWGetType( void );
    virtual UInt32	sndHWGetManufacturer( void );

			// User Client calls
	virtual UInt8	readGPIO (UInt32 selector) {return 0;}
	virtual void	writeGPIO (UInt32 selector, UInt8 data) {return;}
	virtual Boolean	getGPIOActiveState (UInt32 gpioSelector) {return 0;}

public:
    // Classic Unix driver functions
    virtual bool init(OSDictionary *properties);
    virtual void free();

    virtual IOService* probe(IOService *provider, SInt32*);

    //IOAudioDevice subclass
    virtual bool initHardware(IOService *provider);
            
    // Turn detects on and off
    virtual void setDeviceDetectionActive();
    virtual void setDeviceDetectionInActive();

    //Power Management
    virtual  IOReturn   sndHWSetPowerState(IOAudioDevicePowerState theState);
    
    // 
    virtual  UInt32	sndHWGetConnectedDevices(void);
    virtual  UInt32 	sndHWGetProgOutput();
    virtual  IOReturn   sndHWSetProgOutput(UInt32 outputBits);

private:
    // private data members
    
    // it's not possible to read the daca registers back so it is important to maintain
    // a copy of the resisters here, it effect a set of shadow registers for the device.
    
    UInt8  	sampleRateReg;
    UInt16 	analogVolumeReg;
    UInt8  	configurationReg;

    const OSSymbol		*fAppleAudioVideoJackStateKey;
    
    bool			fIsMuted ;			// true if we are muted
    bool			fHeadphonesInserted ;		// true if headphones (or speakers) are inserted in the jack
    UInt16			fCachedAnalogVolumeReg ;	// used to store the last volume reg before mute
    UInt8			fActiveInput ;			// used to store the currently selected input
    AudioI2SControl 		*myAudioI2SControl ;    	// this class is an abstraction for i2s services

      
    // Remember the provider
    IORegistryEntry *sound;

    // *********************************
    // * I 2 C  DATA & Member Function *
    // *********************************


    // This provides access to the DACA registers:
    PPCI2CInterface *interface;

    // private routines for accessing i2c
    bool findAndAttachI2C( IOService *provider ) ;
    bool detachFromI2C(  IOService* /*provider*/) ;
    UInt32 getI2CPort( void ) ;
    bool openI2C( void ) ;
    void closeI2C( void ) ;


    // private utility methods
    bool dependentSetup(void) ;		// final daca specific setup
    UInt32 frameRate(UInt32 index) ;
    bool setDACASampleRate( UInt rate ) ;
    bool writeRegisterBits( UInt8 subAddress,  UInt32 bitMaskOn,  UInt32 bitMaskOff) ;
    
    //These will probably change when we have a general method
    //to verify the Detects.  Wait til we figure out how to do 
    // this with interrupts and then make that generic.
    virtual void checkStatus(bool force);
    static void timerCallback(OSObject *target, IOAudioDevice *device);
    
    // Routines for setting registers.  If you look at the old driver you'll see that the write register
    // routine had 3 params: register, bits on, bits off.  The register is is the register to write to.
    // bits on and bits off are less clear.  If you check out the description of the micronas part,
    // DAC3550A Stereo Audio DAC, section 3.6 "control registers" you'll see that the registers are 
    // multi purpose.  The "bits on" mask says which bits to set for the value requested.  The "bits off"
    // mask forces certain bits off.  
    //
    // This seems a little counter intuitive.  As an example consider one of the 16 bit registers,
    // the "analog volume" register.  Bits 5:0 define the right channel volume, bits 7:6 are not used and 
    // are always set to zero, bits 13:8 define the left channel value, bit 14 sets de-emphasis on/off
    // and bit 15 is unused.
    //
    // As an example, to set the left channel (only) volume, you would pass in the desired volume in
    // bits 5:0, and mask against 0x3F00 (this is a mask with 0011111100000000).  This will correctly 
    // set the left channel volume without inadvertenly setting other values in this register.
    //
    // The AppleOnboardAudio class supports a set register method, these macros allow us to specify
    // the value param, whilst still allowing us to mask correctly and compatibly.


    inline UInt16 setBitsAVOLShadowReg(UInt16 value, UInt16 mask) 
    {
        // NOTE: this does not write the shadow reg to the part, it just
        // updates the shadow reg to the requested value.  You need to 
        // write this to the DACA part when you are done.
        
        // turns on the bits specified by value only if they are in the mask,
        // leaving the rest of the contents of the register unaffected.
        UInt16	newValue ;
        
        // place contents of reg into temp value
        newValue = analogVolumeReg ;
        
        // zero values specified by the mask, leaving the rest of the register intact
        newValue &= ~mask ;
        
        // or in the bits passed in newvalue
        newValue |= (value & mask);

        // set the value of the shadow register
        analogVolumeReg = newValue ;
        
        return newValue ;
    } 
    
    inline UInt8 setBitsSR_REGShadowReg(UInt8 value, UInt8 mask) 
    {
        // NOTE: this does not write the shadow reg to the part, it just
        // updates the shadow reg to the requested value.  You need to 
        // write this to the DACA part when you are done.
        
        // turns on the bits specified by value only if they are in the mask,
        // leaving the rest of the contents of the register unaffected.
        UInt8	newValue ;
        
        // place contents of reg into temp value
        newValue = sampleRateReg;
        
        // zero values specified by the mask, leaving the rest of the register intact
        newValue &= ~mask;
        
        // or in the bits passed in newvalue
        newValue |= (value & mask);

        // set the value of the shadow register
        sampleRateReg = newValue ;
        
        return newValue ;
    } 
    
    inline UInt8 setBitsGCFGShadowReg(UInt8 value, UInt8 mask) 
    {
        // NOTE: this does not write the shadow reg to the part, it just
        // updates the shadow reg to the requested value.  You need to 
        // write this to the DACA part when you are done.
        
        // turns on the bits specified by value only if they are in the mask,
        // leaving the rest of the contents of the register unaffected.
        UInt8	newValue ;
        
        // place contents of reg into temp value
        newValue = configurationReg;
        
        // zero values specified by the mask, leaving the rest of the register intact
        newValue &= ~mask;
        
        // or in the bits passed in newvalue
        newValue |= (value & mask);

        // set the value of the shadow register
        configurationReg = newValue ;

        return newValue ;
    } 
    
    
   
} ;


#endif