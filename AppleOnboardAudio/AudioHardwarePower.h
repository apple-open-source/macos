/*
 *  AudioHardwarePower.h
 *  AppleOnboardAudio
 *
 *  Created by lcerveau on Tue Feb 20 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _AUDIOHARDWAREPOWER_H_
#define _AUDIOHARDWAREPOWER_H_

#include "AudioHardwareCommon.h"
#include "AppleOnboardAudio.h"

enum{
    kBasePowerObject,     	//basic mute 
    kProj6PowerObject = 6,	// for WallStreet
    kProj7PowerObject,	  	// for Screamer based G4 tower and iMac DVs
    kProj8PowerObject,	  	// for iBook with Clamshell
    kProj10PowerObject  =10,    // for Pismo and Titanium
    kProj14PowerObject =14,	// for Texas 3001 based Tower
    kProj16PowerObject =16  	// for dual USB iBook
};

#pragma mark -
class AudioPowerObject : public OSObject {
    OSDeclareDefaultStructors(AudioPowerObject);

public:
    static  AudioPowerObject* createAudioPowerObject(AppleOnboardAudio *pluginRef);

    virtual IOReturn setHardwarePowerOn();
    virtual IOReturn setHardwarePowerOff();

protected:    
    virtual bool init(AppleOnboardAudio  *pluginRef);
    virtual void free();
    
    IODeviceMemory *powerReg;
    AppleOnboardAudio *audioPluginRef;
    short 	hardwareType;
    OSArray *OutputPortMuteStates;
    bool     singleMuteState;
}; 


    //For FW PB and Titanium
#pragma mark -
class AudioProj10PowerObject : public AudioPowerObject {
     OSDeclareDefaultStructors(AudioProj10PowerObject);

public:
     static AudioProj10PowerObject* createAudioProj10PowerObject(AppleOnboardAudio *pluginRef);

    virtual IOReturn setHardwarePowerOn();
    virtual IOReturn setHardwarePowerOff();

private:
    bool init(AppleOnboardAudio *pluginRef);
    enum {
        durationMillisecond = 1,
        kTime10ms = durationMillisecond * 10,	// a delay of 10 ms is required after 
                                                //starting clocks before doing a recalibrate
        kDefaultPowerObjectReg	= 0x80800000,	// default base address
        kPowerObjectOffset	= 0x0000006F,	// offset to GPIO5 register
        kPowerObjectMask	= 0x02,		// determine if PWD is high or low
        kPowerOn		= 0x05,		// sound power enable
        kPowerOff		= 0x04		// sound power disable
    };
};
    
    //For WallStreet
#pragma mark -
class AudioProj6PowerObject : public AudioPowerObject {
    OSDeclareDefaultStructors(AudioProj6PowerObject);

public:
    static AudioProj6PowerObject* createAudioProj6PowerObject(AppleOnboardAudio *pluginRef);                                                            
    virtual IOReturn setHardwarePowerOff();	
    virtual IOReturn setHardwarePowerOn();
    
    bool init(AppleOnboardAudio *pluginRef);

private:
    UInt32	*powerObjectReg;	// register Objectling power
	
    enum {
        durationMillisecond = 1,
        kTime10ms =	durationMillisecond * 10,   // a delay of 10 ms is required after 
                                                    //starting clocks before doing a recalibrate
        kDefaultPowerObjectReg	= 0xF3000000,	// default base address
        kPowerObjectOffset	= 0x00000038,	// offset to feature Object register
        kPowerObjectMask	= 0x00300000,	// enable sound clock and power, endian swapped
        kPowerOn		= 0x00200000,	// sound clock enable, sound power enable
        kPowerOff		= 0x00100000,	// sound clock disable, sound power disable
        kPowerPWDBit		= 0x00100000,	// sound PWD bit on/off (PWD asserted low)
        kPowerClkBit	        = 0x00200000	// sound SND_CLK_EN bit on/off (clocks on high)
    };
};

    //For B&W G3 (Yosemite)
#pragma mark -
class AudioProj4PowerObject : public AudioPowerObject {
    OSDeclareDefaultStructors(AudioProj4PowerObject);

public:
    static AudioProj4PowerObject* createAudioProj4PowerObject(AppleOnboardAudio *pluginRef);
								
    virtual IOReturn setHardwarePowerOff();	
    virtual IOReturn setHardwarePowerOn();

private:
    UInt32 *powerObjectReg;	// register Objectling power
	
    enum {
        durationMillisecond =1,
        kTime10ms	= durationMillisecond * 10,	// a delay of 10 ms is required after 
                                                    //starting clocks before doing a recalibrate
        kDefaultPowerObjectReg	= 0x80800000,	// default base address
        kPowerObjectOffset	= 0x00000038,	// offset to feature Object register
        kPowerObjectMask	= 0x00100000,	// enable sound clock and power, endian swapped
        kPowerOn		= 0x00100000,	// sound power enable
        kPowerOff		= 0x00100000,	// sound power disable
        kPowerYosemite		= 0x00100000	// sound power enable for Yosemite
    };
};

    //for iBooks in Clamshell
#pragma mark -
class AudioProj8PowerObject : public AudioPowerObject {
    OSDeclareDefaultStructors(AudioProj8PowerObject);
    
public:
    static AudioProj8PowerObject* createAudioProj8PowerObject(AppleOnboardAudio *pluginRef);
								
    virtual IOReturn setHardwarePowerOff();	
    virtual IOReturn setHardwarePowerOn();
};

    //for Screamer base G4 and iMac DVs
#pragma mark -
class AudioProj7PowerObject : public AudioPowerObject {
    OSDeclareDefaultStructors(AudioProj7PowerObject);
    
public:
    static AudioProj7PowerObject* createAudioProj7PowerObject(AppleOnboardAudio *pluginRef);
								
    virtual IOReturn setHardwarePowerOff();	
    virtual IOReturn setHardwarePowerOn();

private:
    bool init(AppleOnboardAudio *pluginRef);
    UInt8	*powerObjectReg; 	// register Objectling power
    UInt32	layoutID;	 	// layout id of built in hardware.
    Boolean	restoreProgOut;		// remember and restore ProgOut
    UInt32	oldProgOut;
	
    enum {
        durationMillisecond = 1,
        kTime10ms = durationMillisecond * 10,	// a delay of 10 ms is required after 
                                            //starting clocks before doing a recalibrate
        kTime100ms = durationMillisecond * 100,	// a delay of 100 ms is required to let the 
                                            //anti-pop circuit do its thing
        kTime500ms = durationMillisecond * 500,	// a delay of 500 ms is required to let the 
                                            //anti-pop circuit do its thing
        kDefaultPowerObjectReg	= 0x80800000,	// default base address
        kPowerObjectOffset	= 0x0000006F,	// offset to GPIO5 register
        kPowerObjectMask	= 0x02,		// determine if PWD is high or low
        kPowerOn		= 0x05,		// sound power enable
        kPowerOff		= 0x04		// sound power disable
    };
};
    
    //for Texas3001 Tower
#pragma mark -
class AudioProj14PowerObject : public AudioPowerObject {
    OSDeclareDefaultStructors(AudioProj14PowerObject);
    
public:
    static AudioProj14PowerObject* createAudioProj14PowerObject(AppleOnboardAudio *pluginRef);
								
    virtual IOReturn setHardwarePowerOff();	
    virtual IOReturn setHardwarePowerOn();
};
    
    //for iBook dual USB
#pragma mark -
class AudioProj16PowerObject : public AudioPowerObject {
    OSDeclareDefaultStructors(AudioProj16PowerObject);
    
public:
    static AudioProj16PowerObject* createAudioProj16PowerObject(AppleOnboardAudio *pluginRef);
								
    virtual IOReturn setHardwarePowerOff();	
    virtual IOReturn setHardwarePowerOn();
};
    
#endif

