/*
 *  AudioHardwarePower.cpp
 *  AppleOnboardAudio
 *
 *  Created by cerveau on Sun Jun 03 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AudioHardwarePower.h"
#include "AppleOnboardAudio.h"


#pragma mark - 
OSDefineMetaClassAndStructors(AudioPowerObject, OSObject)
   
AudioPowerObject* AudioPowerObject::createAudioPowerObject(AppleOnboardAudio *pluginRef){
    AudioPowerObject *myAudioPowerObject = 0;
    
    myAudioPowerObject = new AudioPowerObject;
    if(myAudioPowerObject) {
        if(!(myAudioPowerObject->init(pluginRef))){
            myAudioPowerObject->release();
            myAudioPowerObject = 0;
        }            
    }
    return myAudioPowerObject;
}
    
bool AudioPowerObject::init(AppleOnboardAudio *pluginRef){
    DEBUG_IOLOG("+ AudioPowerObject::init\n");
    if (!(OSObject::init())) 
        return false;
    
    if(pluginRef) {
        audioPluginRef = pluginRef;
        audioPluginRef->retain();
    }
    DEBUG_IOLOG("- AudioPowerObject::init\n");
    return true;
}
    
void AudioPowerObject::free(){
    audioPluginRef->release();
    OSObject::free();
}
    
IOReturn AudioPowerObject::setHardwarePowerOn(){

    IOReturn result = kIOReturnSuccess;
    
    return result;
}

IOReturn AudioPowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}
 


    //For FW PB and Titanium
#pragma mark -

OSDefineMetaClassAndStructors(AudioProj10PowerObject, AudioPowerObject)

AudioProj10PowerObject* AudioProj10PowerObject::createAudioProj10PowerObject(AppleOnboardAudio *pluginRef){
    AudioProj10PowerObject *myAudioproj10PowerObject = 0;
    DEBUG_IOLOG("+ AudioProj10PowerObject::createAudioProj10PowerObject\n");
    myAudioproj10PowerObject = new AudioProj10PowerObject;
    
    if(myAudioproj10PowerObject) {
        if(!(myAudioproj10PowerObject->init(pluginRef))){
            myAudioproj10PowerObject->release();
            myAudioproj10PowerObject = 0;
        }            
    }
    DEBUG_IOLOG("+ AudioProj10PowerObject::createAudioProj10PowerObject\n");
    return myAudioproj10PowerObject;
}

bool AudioProj10PowerObject::init(AppleOnboardAudio *pluginRef){
    DEBUG_IOLOG("+ AudioProj10PowerObject::init\n");
    return (AudioPowerObject::init(pluginRef));
    DEBUG_IOLOG("- AudioProj10PowerObject::init\n");
}



IOReturn AudioProj10PowerObject::setHardwarePowerOn(){
    DEBUG_IOLOG("+ AudioProj10PowerObject::setHardwarePowerOn\n");
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    IOService *keyLargo = 0;

    keyLargo = IOService::waitForService(IOService::serviceMatching("KeyLargo"));
    
    if(keyLargo){
        long gpioOffset = kPowerObjectOffset;
        UInt8  value = kPowerOn;  //KPowerOn
        keyLargo->callPlatformFunction("keyLargo_writeRegUInt8", false, (void *)&gpioOffset, (void *)value, 0, 0);
    }
    IOSleep(80);
    progOut = audioPluginRef->sndHWGetProgOutput();
    progOut &= ~kSndHWProgOutput0;
    audioPluginRef->sndHWSetProgOutput(progOut);
    IOSleep(200);

    if(audioPluginRef) {
        audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
        audioPluginRef->setDeviceDetectionActive();
        audioPluginRef->setMuteState(singleMuteState);
    }
    DEBUG_IOLOG("- AudioProj10PowerObject::setHardwarePowerOn\n");
    return result;
}

IOReturn AudioProj10PowerObject::setHardwarePowerOff(){
    
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    IOService *keyLargo = 0;
    DEBUG_IOLOG("+ AudioProj10PowerObject::setHardwarePowerOff\n");
    
        //generic code
    if(audioPluginRef) {
         singleMuteState = audioPluginRef->getMuteState();
         audioPluginRef->setMuteState(false);
    }
    
        //clock
    keyLargo = IOService::waitForService(IOService::serviceMatching("KeyLargo")); 
                
    if(keyLargo){
        long gpioOffset = kPowerObjectOffset;
        UInt8  value = kPowerOff;  //KPowerOff
        keyLargo->callPlatformFunction("keyLargo_writeRegUInt8", false, (void *)&gpioOffset, (void *)value, 0, 0);
    }
        //specific
    progOut = audioPluginRef->sndHWGetProgOutput();
    progOut |= kSndHWProgOutput0;  //this turns the boomer off
    audioPluginRef->sndHWSetProgOutput(progOut);
    IOSleep(200);
    
        //generic
    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceSleep);
    audioPluginRef->setDeviceDetectionInActive();

    DEBUG_IOLOG("- AudioProj10PowerObject::setHardwarePowerOff");
    return result;
}

    
    //For WallStreet
#pragma mark -

OSDefineMetaClassAndStructors(AudioProj6PowerObject, AudioPowerObject)

AudioProj6PowerObject* AudioProj6PowerObject::createAudioProj6PowerObject(AppleOnboardAudio *pluginRef){
    AudioProj6PowerObject *myAudioProj6PowerObject =0;
    
    DEBUG_IOLOG("+ AudioProj6PowerObject::createAudioProj6PowerObject\n");
    myAudioProj6PowerObject = new AudioProj6PowerObject;
    
    if(myAudioProj6PowerObject) {
        if(!(myAudioProj6PowerObject->init(pluginRef))){
            myAudioProj6PowerObject->release();
            myAudioProj6PowerObject = 0;
        }            
    }
    DEBUG_IOLOG("+ AudioProj6PowerObject::createAudioProj6PowerObject\n");
    
    return myAudioProj6PowerObject;
}

bool AudioProj6PowerObject::init(AppleOnboardAudio *pluginRef){
    DEBUG_IOLOG("+ AudioProj6PowerObject::init\n");
    return (AudioPowerObject::init(pluginRef));
    DEBUG_IOLOG("- AudioProj6PowerObject::init\n");
}

								
IOReturn AudioProj6PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    IOService *HeathRow = 0;
    UInt32 mask, data;
    UInt32 powerRegAdrr;
     
    DEBUG_IOLOG("+ AudioProj6PowerObject::setHardwarePowerOff\n");

        //generic
    if(audioPluginRef) {
         singleMuteState = audioPluginRef->getMuteState();
         audioPluginRef->setMuteState(false);
    }
    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceSleep);
    audioPluginRef->setDeviceDetectionInActive();
    
    HeathRow = IOService::waitForService(IOService::serviceMatching("Heathrow"));
    
    powerRegAdrr = kPowerObjectOffset;
    if(HeathRow) {
        mask = kPowerObjectMask;
        data = kPowerOff;
        HeathRow->callPlatformFunction(OSSymbol::withCString("heathrow_safeWriteRegUInt32"), false, 
                                                                (void *)powerRegAdrr, (void *)mask, (void *) data, 0);
    } 
        
    
    DEBUG2_IOLOG("- AudioProj6PowerObject::setHardwarePowerOff, %d\n", kIOReturnSuccess == result);
    return result;
}

IOReturn AudioProj6PowerObject::setHardwarePowerOn(){
    IOReturn result = kIOReturnSuccess;
    IOService *HeathRow = 0;
    UInt32 mask, data;
    UInt32 powerRegAdrr;

    DEBUG_IOLOG("+ AudioProj6PowerObject::setHardwarePowerOn\n");
        
    HeathRow = IOService::waitForService(IOService::serviceMatching("Heathrow"));

    powerRegAdrr = kPowerObjectOffset;
    if(HeathRow) {
        mask = kPowerObjectMask;
        data = kPowerOn;        
        HeathRow->callPlatformFunction(OSSymbol::withCString("heathrow_safeWriteRegUInt32"), false, 
                                                                (void *)powerRegAdrr, (void *)mask, (void *) data, 0);
        IOSleep(10);
    }
    
    if(audioPluginRef) {
        audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
        audioPluginRef->setDeviceDetectionActive();
        audioPluginRef->setMuteState(singleMuteState);
    }

    DEBUG2_IOLOG("- AudioProj6PowerObject::setHardwarePowerOn, %d\n", kIOReturnSuccess == result);
    return result;

}


    //For B&W G3 (Yosemite)
#pragma mark -

OSDefineMetaClassAndStructors(AudioProj4PowerObject, AudioPowerObject)

AudioProj4PowerObject* AudioProj4PowerObject::createAudioProj4PowerObject(AppleOnboardAudio *pluginRef){
    AudioProj4PowerObject* myAudioProj4PowerObject=0;
    
    return(myAudioProj4PowerObject);
}
								
IOReturn AudioProj4PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}

IOReturn AudioProj4PowerObject::setHardwarePowerOn(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}

    //for iBooks in Clamshell
#pragma mark -

OSDefineMetaClassAndStructors(AudioProj8PowerObject, AudioPowerObject)

AudioProj8PowerObject* AudioProj8PowerObject::createAudioProj8PowerObject(AppleOnboardAudio *pluginRef){
    AudioProj8PowerObject* myAudioProj8PowerObject=0;
    
    return(myAudioProj8PowerObject);
}
								

IOReturn AudioProj8PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}
	
IOReturn AudioProj8PowerObject::setHardwarePowerOn(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}

    //for Screamer base G4 and iMac DVs
#pragma mark -

OSDefineMetaClassAndStructors(AudioProj7PowerObject, AudioPowerObject)

AudioProj7PowerObject* AudioProj7PowerObject::createAudioProj7PowerObject(AppleOnboardAudio *pluginRef){
    AudioProj7PowerObject *myAudioProj7PowerObject = 0;
    DEBUG_IOLOG("+ AudioProj7PowerObject::createAudioProj10PowerObject\n");
    myAudioProj7PowerObject = new AudioProj7PowerObject;
    
    if(myAudioProj7PowerObject) {
        if(!(myAudioProj7PowerObject->init(pluginRef))){
            myAudioProj7PowerObject->release();
            myAudioProj7PowerObject = 0;
        }            
    }
    DEBUG_IOLOG("+ AudioProj7PowerObject::createAudioProj10PowerObject\n");
    return myAudioProj7PowerObject;
}

bool AudioProj7PowerObject::init(AppleOnboardAudio *pluginRef){
    DEBUG_IOLOG("+ AudioProj10PowerObject::init\n");
    return (AudioPowerObject::init(pluginRef));
    DEBUG_IOLOG("- AudioProj10PowerObject::init\n");
}

								
IOReturn AudioProj7PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    IOService *keyLargo = 0;
    DEBUG_IOLOG("+ AudioProj7PowerObject::setHardwarePowerOff\n");
    
    if(audioPluginRef) {
         singleMuteState = audioPluginRef->getMuteState();
         audioPluginRef->setMuteState(false);
    }
    
    progOut = audioPluginRef->sndHWGetProgOutput();
    progOut &= ~kSndHWProgOutput0;
    audioPluginRef->sndHWSetProgOutput(progOut);
    IOSleep(200);

    keyLargo = IOService::waitForService(IOService::serviceMatching("KeyLargo")); 
                
    if(keyLargo){
        long gpioOffset = kPowerObjectOffset;
        UInt8  value = kPowerOff;  //KPowerOff
        keyLargo->callPlatformFunction("keyLargo_writeRegUInt8", false, (void *)&gpioOffset, (void *)value, 0, 0);
    }
    
    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceSleep);
    audioPluginRef->setDeviceDetectionInActive();

    DEBUG_IOLOG("- AudioProj7PowerObject::setHardwarePowerOff");
    return result;
}
	
IOReturn AudioProj7PowerObject::setHardwarePowerOn(){
    DEBUG_IOLOG("+ AudioProj7PowerObject::setHardwarePowerOn\n");
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    IOService *keyLargo = 0;

    keyLargo = IOService::waitForService(IOService::serviceMatching("KeyLargo"));
    
    if(keyLargo){
        long gpioOffset = kPowerObjectOffset;
        UInt8  value = kPowerOn;  //KPowerOn
        keyLargo->callPlatformFunction("keyLargo_writeRegUInt8", false, (void *)&gpioOffset, (void *)value, 0, 0);
    }
    
    if(audioPluginRef) {
        IOSleep(300);   //we need to wait to be sure that the antipop has the time to do his stuff
        progOut = audioPluginRef->sndHWGetProgOutput();
        progOut |= kSndHWProgOutput0;
        audioPluginRef->sndHWSetProgOutput(progOut);
    }
    
    if(audioPluginRef) {
        audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
        audioPluginRef->setDeviceDetectionActive();
        audioPluginRef->setMuteState(singleMuteState);
    }
    DEBUG_IOLOG("- AudioProj7PowerObject::setHardwarePowerOn\n");
    return result;
}

    //for Texas3001 Tower
#pragma mark -

OSDefineMetaClassAndStructors(AudioProj14PowerObject, AudioPowerObject)

AudioProj14PowerObject* AudioProj14PowerObject::createAudioProj14PowerObject(AppleOnboardAudio *pluginRef){
    AudioProj14PowerObject* myAudioProj14PowerObject =0;
    
    return(myAudioProj14PowerObject);
}
								
IOReturn AudioProj14PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}

IOReturn AudioProj14PowerObject::setHardwarePowerOn(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}


    

    //for iBook dual USB
#pragma mark -

OSDefineMetaClassAndStructors(AudioProj16PowerObject, AudioPowerObject)

AudioProj16PowerObject* AudioProj16PowerObject::createAudioProj16PowerObject(AppleOnboardAudio *pluginRef){
    AudioProj16PowerObject* myAudioProj16PowerObject=0;
    
    return(myAudioProj16PowerObject);
}
								
IOReturn AudioProj16PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}

IOReturn AudioProj16PowerObject::setHardwarePowerOn(){
    IOReturn result = kIOReturnSuccess;
    
    return result;
}
