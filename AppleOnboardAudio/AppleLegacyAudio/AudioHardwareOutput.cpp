/*
 *  AudioHardwareOutput.cpp
 *  Apple02Audio
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

#include "AudioHardwareCommon.h"
#include "AudioHardwareOutput.h"
#include "AudioHardwareConstants.h"
#include "Apple02Audio.h"

#define super IOAudioPort

OSDefineMetaClassAndStructors(AudioHardwareOutput, IOAudioPort)

AudioHardwareOutput *AudioHardwareOutput::create(AudioHardwareOutputInfo theInfo){
    AudioHardwareOutput *myOutput;
    myOutput = new AudioHardwareOutput;
    
    if(myOutput) {
        if(!(myOutput->init(theInfo))){
            myOutput->release();
            myOutput = 0;
        }            
    }

    return myOutput;
}

bool AudioHardwareOutput::init(AudioHardwareOutputInfo theInfo) {

    if(!super::init())
        return(false);
        
    volumeLeft = 0;
    volumeRight = 0;
    mute = 0;
    active = true;   
            
    deviceMask = theInfo.deviceMask;
    oKind = theInfo.portType;		
    sndHWPort = theInfo.portConnection;
    deviceMatch = theInfo.deviceMatch;
    nameResID = theInfo.nameID;	
    iconResID = theInfo.iconID;
    pluginRef = 0;
    outputKind = theInfo.outputKind;
    
    if (kOutputPortTypeProj5==outputKind)
        invertMute = ((theInfo.param) !=0);
    
    return(true);
}


void AudioHardwareOutput::free(){
    // pluginRef->release();
    super::free();
}

void AudioHardwareOutput::attachAudioPluginRef(Apple02Audio *theAudioPlugin){
    pluginRef = theAudioPlugin;
    // pluginRef->retain();
}


void AudioHardwareOutput::deviceIntService( UInt32 currentDevices ) {
    UInt32						progOutputBits;
	OSNumber *					activeOutput;

    switch(outputKind) {
        case kOutputPortTypeClassic:
        case kOutputPortTypeProj3:
            active = (deviceMask & currentDevices) == deviceMatch;

            if(active) {
                pluginRef->sndHWSetActiveOutputExclusive(sndHWPort);
				if (NULL != pluginRef->outputSelector) {
					activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeHeadphones, 32);
					pluginRef->outputSelector->hardwareValueChanged (activeOutput);
				}
			} else {
				if (NULL != pluginRef->outputSelector) {
					activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeInternalSpeaker, 32);
					pluginRef->outputSelector->hardwareValueChanged (activeOutput);
				}
			}
            break;
        case kOutputPortTypeProj5:
            if ((deviceMask & currentDevices) != 0) {
				active = true;
				pluginRef->sndHWSetActiveOutputExclusive (sndHWPort);

				if ((kSndHWCPUHeadphone & currentDevices) == kSndHWCPUHeadphone) {
					progOutputBits = pluginRef->sndHWGetProgOutput ();
					if (invertMute) {
						progOutputBits &= ~kSndHWProgOutput1;
					} else {
						progOutputBits |= kSndHWProgOutput1;
					}
		
					pluginRef->sndHWSetProgOutput (progOutputBits);

					if (NULL != pluginRef->outputSelector) {
						activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeHeadphones, 32);
						pluginRef->outputSelector->hardwareValueChanged (activeOutput);
					}
				} else {
					progOutputBits = pluginRef->sndHWGetProgOutput ();
					if(invertMute) {
						progOutputBits |= kSndHWProgOutput1;
					} else {
						progOutputBits &= ~kSndHWProgOutput1;
					}
							
					pluginRef->sndHWSetProgOutput (progOutputBits);

					if (NULL != pluginRef->outputSelector) {
						activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeInternalSpeaker, 32);
						pluginRef->outputSelector->hardwareValueChanged (activeOutput);
					}
				}
            } else { // Must be External.
				active = false;
				if (NULL != pluginRef->outputSelector) {
					activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeExternalSpeaker, 32);
					pluginRef->outputSelector->hardwareValueChanged (activeOutput);
				}
            }
            break;
        default:
            break;
    }

	// For [2926907]
	if (NULL != pluginRef->headphoneConnection && (oKind == kIOAudioOutputPortSubTypeHeadphones || oKind == kIOAudioOutputPortSubTypeExternalSpeaker || oKind == kIOAudioOutputPortSubTypeLine)) {
		OSNumber *			headphoneState;
		headphoneState = OSNumber::withNumber ((long long unsigned int)active, 32);
		(void)pluginRef->headphoneConnection->hardwareValueChanged (headphoneState);
	}
	// end [2926907]

    ioLog();
}

void AudioHardwareOutput::ioLog(){
    switch (outputKind) {
        case kOutputPortTypeUnknown:
            debugIOLog (3, "+ Info for output port : unknown");
            break;
        case kOutputPortTypeProj5:
            debugIOLog (3, "+ Info for output port : Proj5");
            break;
        case kOutputPortTypeProj3:
            debugIOLog (3, "+ Info for output port : Proj3");
            break;
        case kOutputPortTypeEQ:
            debugIOLog (3, "+ Info for output port : EQ");
            break;        
    }
    
    if(active) {
        debugIOLog (3, "  The output is active of type %4.4s", (char*)&oKind);
    } else {
        debugIOLog (3, "  The output is inactive of type %4.4s", (char*)&oKind);
    }
}

void AudioHardwareOutput::setMute(bool muteState ){
    UInt32 progOutbits;
    
//    if(muteState != mute){
        if(kOutputPortTypeProj3 == outputKind) {
            progOutbits = pluginRef->sndHWGetProgOutput();
            
            if(muteState) 
                progOutbits &= ~kSndHWProgOutput0;
            else
                progOutbits |= kSndHWProgOutput0;
            
            pluginRef->sndHWSetProgOutput(progOutbits);        
            pluginRef->sndHWSetSystemMute(muteState);
        } else {
            pluginRef->sndHWSetSystemMute(muteState);
        }
        mute = muteState;
//    }
}

bool AudioHardwareOutput::getMute(void){
    return(mute);
}

void AudioHardwareOutput::setVolume(UInt32 volLeft, UInt32 volRight){
    pluginRef->sndHWSetSystemVolume(volLeft, volRight);
    
    volumeLeft = volLeft;
    volumeRight = volRight;
}

UInt32 AudioHardwareOutput::getVolume(short Channel){
    switch (Channel) {
        case 1:
            return(volumeLeft);
            break;
        case 2:
            return(volumeRight);
            break;
        default:
            return 0;
            break;
     } 
}

