/*
 *  AudioHardwareOutput.h
 *  AppleOnboardAudio
 *
 *  Copyright (c) 2000 Apple Computer Inc. All rights reserved.
 *
 *  An AudioHardwareOutput represents an output port on the Codec. The number
 *  of AudioHardwareOutput present on a system is less or equal to the number
 *  of physical ports on the Codec.
 * 
 *  However an AudioHardwareOutput can, at the same time control different 
 *  audio output devices (internal speakers, external speakers, line out, 
 *  headphones). For example in the case of the iMac, one AudioHardwareOutput
 *  takes care about headphones and internal speaker. 
 *
 *  The list of AudioHardwareOutput present on a system  is read in the 
 *  "sound-objects" property of the "sound" node in the IOregistry (usually
 *  it comes right away from the Device Tree plane).
 *
 *  When a change occurs in the list of devices connected to the driver
 *  (the currentDevices of typope sndHWSpec field), this field is passed to the 
 *  output, which, consequently do a serie of actions on the state of he Codec.
 *
 *  For now an AudioHardwareOutput can do the following series of actions :
 *	- set volume
 *	- set mute state
 *	- receive the serie of connected device and be activated or not
 *  The AudioHardwareOutput then talk to his referenced hardware, in an uniform 
 *  way. This is the role of the driver to execute the given actions, if it can.
 *  We may add a serie. of action for effect modes. An AudioHardwareOutput is also
 *  responsible to set a serie of properties in the IORegistry, that can be used
 *  by other system pieces (aka Sound Control Panel). Thus can be passed : icon
 *  references, effect support, name, and others... This properties will eventually
 *  be accessible to a HAL properties Plugin. 
 *
 *  On must be conscious that the way the set of outputs is acting is dependant
 *  on UI guidelines. In the first version of OS X, these are based on the OS 9 
 *  ones. That is : 
 *     + when external speakers (or lineout) is connected, the internal
 *       speaker are turned off, 
 *     + when headphone are connected  other connected or internal speakers
 *       are turned off
 *     + there is no separate volume or mute for each port (when a volume 
 *       change is ask it affects all physical ports)
 *  In the future, we plan to implement the notion of "UI Policy" that will
 *  allow to use the chip either in a simple mode, either in an expert mode.
 * 
 */

#ifndef _AUDIOHARDWAREOUTPUT_H
#define _AUDIOHARDWAREOUTPUT_H

#include "AudioHardwareCommon.h"
#include "AudioHardwareUtilities.h"


    // Information passed to the "create" method. Ususally this information
    // is extracted by parsing the Open Firmware "sound-objects" properties
    // of the "sound" node

class AppleOnboardAudio;

typedef struct _s_AudioHardwareOutputInfo {
    UInt32 deviceMask;
    UInt32 deviceMatch;
    UInt32 iconID;
    UInt32 nameID;
    UInt32 portConnection;
    UInt32 portType;
    UInt32 param;
    short outputKind;
    
}AudioHardwareOutputInfo;

    // Different kind of AudioHardwareOutput. We could have created a parent class 
    // and subclass it for each kind, but as there are not a lot of AudioHardwareOutput
    // kind.

enum{
    kOutputPortTypeUnknown,
    kOutputPortTypeClassic,
    kOutputPortTypeProj5,
    kOutputPortTypeProj3,
    kOutputPortTypeEQ
};

    //Class declaration

class AudioHardwareOutput : public IOAudioPort {
    OSDeclareDefaultStructors(AudioHardwareOutput);

public:
        
    static AudioHardwareOutput *create(AudioHardwareOutputInfo theOutputInfo); 
    void attachAudioPluginRef(AppleOnboardAudio *theAudioPlugin);
            
        //receive a new set of devices
    void deviceIntService( UInt32 currentDevices );
    
        //setter/getter
    void setMute(bool muteState);
    bool getMute(void);
    void setVolume(UInt32 volLeft, UInt32 volRight);
    UInt32 getVolume(short Channel);
    
        //debug information
    void ioLog();
    
protected:
    bool init(AudioHardwareOutputInfo theOutputInfo);
    void free();

            //state
    UInt32 	volumeLeft; 		// left volume value (range linked the IOAudioLevel output volume control) 
    UInt32 	volumeRight;		// right volume value (range linked the IOAudioLevel output volume control)
    bool	mute;			// the output is muted
    bool	active;			// the output is active
    
            //internal information
    UInt32	oKind;		        // type of output port
    UInt32	sndHWPort;		// which physical port on the the codec output represents
    
    UInt32	deviceMask;		// mask value to compare with the sndHWSpec field of the driver
    UInt32	deviceMatch;		// match value use determine if this output is active
    short	nameResID;		// resource ID of STR containing name (UI stuff)
    short	iconResID;		// resource ID of ICON containing port icon (UI stuff)
    short 	outputKind;		// kind of output 
    			
    bool 	invertMute;		// use only for some iMacs for type Proj5 speaker
    
            // plugin to which it refers. As this example was brought up on the Screamer chip
            // the object is of type AppleScreamerAudio. THIS IS NOT HERE TO STAY!!!
            // We should assign a AppleHarwareAudio (or AppleOnboardAudio object).
            // This reference is passed at creation. However, it would be (in my opinion)
            // better to create by finding the right class in the IOKit object.
     
    AppleOnboardAudio *pluginRef;

};

#endif //_AUDIOHARDWAREOUTPUT_H