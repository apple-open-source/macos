/*
 *  AppleHardwareInput.cpp
 *  Apple02Audio
 *
 *  Created by lcerveau on Wed Jan 03 2001.
 *  Copyright (c) 2000 __CompanyName__. All rights reserved.
 *
 */

#include "AudioHardwareCommon.h"
#include "AudioHardwareInput.h"
#include "AudioHardwareConstants.h"

#define super IOAudioPort

OSDefineMetaClassAndStructors(AudioHardwareInput, IOAudioPort)

AudioHardwareInput *AudioHardwareInput::create(AudioHardwareInputInfo theInfo){
    AudioHardwareInput *myIntput;
    myIntput = new AudioHardwareInput;
    
    if(myIntput) {
        if(!(myIntput->init(theInfo))){
            myIntput->release();
            myIntput = 0;
        }            
    }

    return myIntput;
}

bool AudioHardwareInput::init(AudioHardwareInputInfo theInfo) {
    if(!super::init())
        return(false);
    
    sndHWPort = theInfo.sndHWPort; 		
    inputPortType = theInfo.inputPortType;
    channels = theInfo.channels;
    isOnMuX = theInfo.isOnMuX;
    
    if(isOnMuX)
        theMuxRef = theInfo.theMuxRef;
    else 
        theMuxRef = 0;
        
    
    active = 0;   
    return(true);
}

void AudioHardwareInput::free(){
    // pluginRef->release();
    super::free();
}

void AudioHardwareInput::attachAudioPluginRef(Apple02Audio *theAudioPlugin){
    pluginRef = theAudioPlugin;
    // pluginRef->retain();
}


bool AudioHardwareInput::deviceSetActive( UInt32 currentDevices ){
    UInt32 devices;
    bool oldactive;
    
	// we make the following assumption until we have input selection
	// Internal microphone is supposed to be active until we have 
	// input selection. When an external mic is connected it is activated
	// automatically
    // debugIOLog (3, "Current devices are %x ", currentDevices);
    devices = pluginRef->sndHWGetConnectedDevices();    
    devices = devices & kSndHWInputDevices;   		// we have only the connected device
    oldactive = active;

	// mask the device to get only input devices

    switch(inputPortType) {
        case kIntMicSource:
        case kExtMicSource: 
        case kSoundInSource:
		case kModemSource:
            if (0 != devices)
                active = true;
            else 
                active = false;
            
            if(active != oldactive) {
                if(active) {
                    pluginRef->sndHWSetActiveInputExclusive(sndHWPort);
                    if(isOnMuX)
                        theMuxRef->SetMuxSource(inputPortType);
                    // debugIOLog (3, " --> Switching to port %d", sndHWPort);
                }
			}
            ioLog();
            break;
        default:
            active = false;
            break;
    }

    return(true);

}

void AudioHardwareInput::ioLog() {
#ifdef DEBUGLOG
    debugIOLog (3,  "  # Input port information :");
     switch (inputPortType) {
        case kNoSource:debugIOLog (3, "  -- Type is : 'none' ");													break;
        case kCDSource :debugIOLog (3, "  -- Type is : 'cd  '");													break;
        case kExtMicSource:debugIOLog (3, "  -- Type is : 'emic' or %d or 0x%lX", kExtMicSource, kExtMicSource);	break;
        case kSoundInSource:debugIOLog (3, "  -- Type is : 'sinj'");												break;
        case kRCAInSource:debugIOLog (3, "  -- Type is :'irca' (RCA jack) ");										break;
        case kTVFMTunerSource:debugIOLog (3, "  -- Type is : 'tvfm' (TVFM Tuner) ");								break;
        case kDAVInSource:debugIOLog (3, "  -- Type is :'idav' (DAV analog)");										break;
        case kIntMicSource:debugIOLog (3, "  -- Type is :'imic' or %d or 0x%lX", kIntMicSource, kIntMicSource);		break;
        case kMediaBaySource:debugIOLog (3, "  -- Type is :'mbay'");												break;
        case kModemSource :debugIOLog (3, "  -- Type is :'modm' or %d or 0x%lX", kModemSource, kModemSource);		break;
        case kPCCardSource:debugIOLog (3, "  -- Type is :'pcm'");													break;
        case kZoomVideoSource:debugIOLog (3, "  -- Type is :'zvpc'");												break;
        case kDVDSource:debugIOLog (3, "  -- Type is :'dvda'");														break;
        case kMicrophoneArray:debugIOLog (3, "  -- Type is : 'mica' (microphone array) ");							break;
        default:debugIOLog (3, "  -- Type is : unknown ");															break;
    }
    
    debugIOLog (3, "  -- Physical port is %ld", sndHWPort);
    debugIOLog (3, "  -- Affected channels are %ld", channels);
    debugIOLog (3, "  -- Is a Mux input ? : %d", isOnMuX);
    if(isOnMuX)
        theMuxRef->ioLog();
    debugIOLog (3, "  -- Active State ? : %d", active);
#endif
}

void AudioHardwareInput::forceActivation(UInt32 selector) {
	debugIOLog (3, "forceActivation(%4s), inputPortType = %4s, active = %d", (char*)&selector, (char*)&inputPortType, active);

    if(selector == inputPortType) {
//        if(!active) {
            pluginRef->sndHWSetActiveInputExclusive(sndHWPort);
            if(isOnMuX)
				theMuxRef->SetMuxSource(inputPortType);

           // debugIOLog (3, " --> Switching to port %d", sndHWPort);
            active = true;
//        }
    } else {
        active = false;
    }
    ioLog();
}

UInt32  AudioHardwareInput::getInputPortType(void) {
     return(inputPortType);
}

void AudioHardwareInput::setInputGain(UInt32 leftGain, UInt32 rightGain){
    
    pluginRef->sndHWSetSystemInputGain(leftGain, rightGain);
    
    gainLeft = leftGain;
    gainRight = rightGain;
}
