/*
 *  AppleHardwareInput.cpp
 *  AppleOnboardAudio
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
    //pluginRef->release();
    super::free();
}

void AudioHardwareInput::attachAudioPluginRef(AppleOnboardAudio *theAudioPlugin){
    pluginRef = theAudioPlugin;
    //pluginRef->retain();
}


bool AudioHardwareInput::deviceSetActive( UInt32 currentDevices ){
    UInt32 devices;
    bool oldactive;
    
        //we make the following assumption until we have input selection
        //Internal microphone is supposed to be active until we have 
        //input selection. When an external mic is connected it is activated
        //automatically
    //debug2IOLog("Current devices are %x \n", currentDevices);
    devices = pluginRef->sndHWGetConnectedDevices();    
    devices = devices & kSndHWInputDevices;   //we have only the connected device
    oldactive = active;

        //mask the device to get only input devices

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
                    //CLOG(" --> Switching to port %d\n", sndHWPort);
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
    debugIOLog( "+ Input port information :\n");
     switch (inputPortType) {
        case kNoSource:debugIOLog(" -- Type is : none \n");break;
        case kCDSource :debugIOLog(" -- Type is : cd  \n");break;
        case kExtMicSource:debug2IOLog(" -- Type is : emic or %d\n", kExtMicSource);break;
        case kSoundInSource:debugIOLog(" -- Type is : sinj\n");break;
        case kRCAInSource:debugIOLog(" -- Type is :irca (RCA jack) \n");break;
        case kTVFMTunerSource:debugIOLog(" -- Type is : tvfm (TVFM Tuner) \n");break;
        case kDAVInSource:debugIOLog(" -- Type is :idav (DAV analog)\n");break;
        case kIntMicSource:debug2IOLog(" -- Type is :imic or %d\n", kIntMicSource);break;
        case kMediaBaySource:debugIOLog(" -- Type is :mbay\n");break;
        case kModemSource :debug2IOLog(" -- Type is :modm or %d\n", kModemSource);break;
        case kPCCardSource:debugIOLog(" -- Type is :pcm\n");break;
        case kZoomVideoSource:debugIOLog(" -- Type is :zvpc\n");break;
        case kDVDSource:debugIOLog(" -- Type is :dvda\n");break;
        case kMicrophoneArray:debugIOLog(" -- Type is : mica (microphone array) \n");break;
        default:debugIOLog(" -- Type is : unknown \n"); break;
    }
    
    debug2IOLog(" -- Physical port is %ld\n", sndHWPort);
    debug2IOLog(" -- Affected channels are %ld\n", channels);
    debug2IOLog(" -- Is a Mux input ? : %d\n", isOnMuX);
    if(isOnMuX)
        theMuxRef->ioLog();
    debug2IOLog(" -- Active State ? : %d\n", active);
#endif
}

void AudioHardwareInput::forceActivation(UInt32 selector) {
    
    if(selector == inputPortType) {
        if(!active) {
            pluginRef->sndHWSetActiveInputExclusive(sndHWPort);
            if(isOnMuX)
                    theMuxRef->SetMuxSource(inputPortType);

           // CLOG(" --> Switching to port %d\n", sndHWPort);
            active = true;
        }
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
