/*
 *  AudioHardwarePower.cpp
 *  Apple02Audio
 *
 *  Created by cerveau on Sun Jun 03 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AudioHardwarePower.h"
#include "Apple02Audio.h"

#pragma mark -Generic Power Object
OSDefineMetaClassAndStructors(AudioPowerObject, OSObject)
   
AudioPowerObject* AudioPowerObject::createAudioPowerObject(Apple02Audio *pluginRef){
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
    
bool AudioPowerObject::init(Apple02Audio *pluginRef){
    debugIOLog ( 3, "+ AudioPowerObject::init ( %p )", pluginRef );
 	
	mMicroSecondsRequired = 50000;
    
	if (!(OSObject::init())) 
        return false;
    
    if(pluginRef) {
        audioPluginRef = pluginRef;
        audioPluginRef->retain();
    }
    debugIOLog ( 3, "- AudioPowerObject::init ( %p ) returns %d", pluginRef, true );
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

UInt32 AudioPowerObject::GetTimeToChangePowerState (IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState) {
	if (kIOAudioDeviceActive == newPowerState && kIOAudioDeviceSleep == oldPowerState) {
		return mMicroSecondsRequired;
	} else {
		return 1;
	}
}

// For FW PB and PowerBook G4
#pragma mark -FW PowerBook and PowerBook G4

OSDefineMetaClassAndStructors(AudioProj10PowerObject, AudioPowerObject)

AudioProj10PowerObject* AudioProj10PowerObject::createAudioProj10PowerObject(Apple02Audio *pluginRef){
    AudioProj10PowerObject *myAudioproj10PowerObject = 0;
    debugIOLog (3, "+ AudioProj10PowerObject::createAudioProj10PowerObject");
    myAudioproj10PowerObject = new AudioProj10PowerObject;
    
    if(myAudioproj10PowerObject) {
        if(!(myAudioproj10PowerObject->init(pluginRef))){
            myAudioproj10PowerObject->release();
            myAudioproj10PowerObject = 0;
        }            
    }
    debugIOLog (3, "+ AudioProj10PowerObject::createAudioProj10PowerObject");
    return myAudioproj10PowerObject;
}

bool AudioProj10PowerObject::init(Apple02Audio *pluginRef){
    debugIOLog (3, "+ AudioProj10PowerObject::init");
 	mMicroSecondsRequired = 750000;
    return (AudioPowerObject::init(pluginRef));
    debugIOLog (3, "- AudioProj10PowerObject::init");
}

void AudioProj10PowerObject::setIdlePowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceIdle);
	}
}

void AudioProj10PowerObject::setFullPowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
	}
}

void AudioProj10PowerObject::setHardwarePowerIdleOn ( void ) {
    setHardwarePowerOn();
}

IOReturn AudioProj10PowerObject::setHardwarePowerOn(){
    debugIOLog (3, "+ AudioProj10PowerObject::setHardwarePowerOn");
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    IOService *keyLargo = 0;

    keyLargo = IOService::waitForService(IOService::serviceMatching("KeyLargo"));
    
    if(keyLargo){
        long gpioOffset = kPowerObjectOffset;
        UInt8  value = kPowerOn;
        keyLargo->callPlatformFunction("keyLargo_writeRegUInt8", false, (void *)&gpioOffset, (void *)(UInt32)value, 0, 0);
    }
    IOSleep(80);
    progOut = audioPluginRef->sndHWGetProgOutput();
    progOut &= ~kSndHWProgOutput0;
    audioPluginRef->sndHWSetProgOutput(progOut);
    IOSleep(200);

    if(audioPluginRef) {
        audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
        audioPluginRef->setDeviceDetectionActive();
    }
    debugIOLog (3, "- AudioProj10PowerObject::setHardwarePowerOn");
    return result;
}

IOReturn AudioProj10PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    IOService *keyLargo = 0;
    debugIOLog (3, "+ AudioProj10PowerObject::setHardwarePowerOff");
        
	// specific
    progOut = audioPluginRef->sndHWGetProgOutput();
    progOut |= kSndHWProgOutput0;  // this turns the boomer off
    audioPluginRef->sndHWSetProgOutput(progOut);
    IOSleep(200);
    
	// generic
    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceSleep);
    audioPluginRef->setDeviceDetectionInActive();

	// clock
    keyLargo = IOService::waitForService(IOService::serviceMatching("KeyLargo")); 
                
    if(keyLargo){
        long gpioOffset = kPowerObjectOffset;
        UInt8  value = kPowerOff;
        keyLargo->callPlatformFunction("keyLargo_writeRegUInt8", false, (void *)&gpioOffset, (void *)(UInt32)value, 0, 0);
    }

    debugIOLog (3, "- AudioProj10PowerObject::setHardwarePowerOff");
    return result;
}

    
// For PowerBook G3
#pragma mark -PowerBook G3

OSDefineMetaClassAndStructors(AudioProj6PowerObject, AudioPowerObject)

AudioProj6PowerObject* AudioProj6PowerObject::createAudioProj6PowerObject(Apple02Audio *pluginRef){
    AudioProj6PowerObject *myAudioProj6PowerObject =0;
    
    debugIOLog (3, "+ AudioProj6PowerObject::createAudioProj6PowerObject");
    myAudioProj6PowerObject = new AudioProj6PowerObject;
    
    if(myAudioProj6PowerObject) {
        if(!(myAudioProj6PowerObject->init(pluginRef))){
            myAudioProj6PowerObject->release();
            myAudioProj6PowerObject = 0;
        }            
    }
    debugIOLog (3, "+ AudioProj6PowerObject::createAudioProj6PowerObject");
    
    return myAudioProj6PowerObject;
}

bool AudioProj6PowerObject::init(Apple02Audio *pluginRef){
    debugIOLog (3, "+ AudioProj6PowerObject::init");
 	mMicroSecondsRequired = 750000;
    return (AudioPowerObject::init(pluginRef));
    debugIOLog (3, "- AudioProj6PowerObject::init");
}

void AudioProj6PowerObject::setIdlePowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceIdle);
	}
}

void AudioProj6PowerObject::setFullPowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
	}
}

void AudioProj6PowerObject::setHardwarePowerIdleOn ( void ) {
    setHardwarePowerOn();
}

IOReturn AudioProj6PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    IOService *HeathRow = 0;
    UInt32 mask, data;
    UInt32 powerRegAdrr;
     
    debugIOLog (3, "+ AudioProj6PowerObject::setHardwarePowerOff");

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

    debugIOLog (3, "- AudioProj6PowerObject::setHardwarePowerOff, %d", kIOReturnSuccess == result);
    return result;
}

IOReturn AudioProj6PowerObject::setHardwarePowerOn(){
    IOReturn result = kIOReturnSuccess;
    IOService *HeathRow = 0;
    UInt32 mask, data;
    UInt32 powerRegAdrr;

    debugIOLog (3, "+ AudioProj6PowerObject::setHardwarePowerOn");
        
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
    }

    debugIOLog (3, "- AudioProj6PowerObject::setHardwarePowerOn, %d", kIOReturnSuccess == result);
    return result;

}


// For B&W G3
#pragma mark -B&W G3

OSDefineMetaClassAndStructors(AudioProj4PowerObject, AudioPowerObject)

AudioProj4PowerObject* AudioProj4PowerObject::createAudioProj4PowerObject(Apple02Audio *pluginRef){
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

// for iBooks in Clamshell
#pragma mark -Original iBooks

OSDefineMetaClassAndStructors(AudioProj8PowerObject, AudioPowerObject)

AudioProj8PowerObject* AudioProj8PowerObject::createAudioProj8PowerObject(Apple02Audio *pluginRef)
{
    debugIOLog (3, "+ myAudioProj8PowerObject::createAudioProj10PowerObject");
    AudioProj8PowerObject* myAudioProj8PowerObject=0;
    myAudioProj8PowerObject = new AudioProj8PowerObject ;
    if(myAudioProj8PowerObject) 
    {
        if(!(myAudioProj8PowerObject->init(pluginRef)))
        {
            myAudioProj8PowerObject->release();
            myAudioProj8PowerObject = 0;
        }            
    }
    debugIOLog (3, "- myAudioProj8PowerObject::createAudioProj10PowerObject");
    return(myAudioProj8PowerObject);
}
								
bool AudioProj8PowerObject::init(Apple02Audio *pluginRef)
{
    debugIOLog (3, "+ AudioProj8PowerObject::init");
 	mMicroSecondsRequired = 1000000;
    return (AudioPowerObject::init(pluginRef));
    debugIOLog (3, "- AudioProj8PowerObject::init");
}

void AudioProj8PowerObject::setIdlePowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceIdle);
	}
}

void AudioProj8PowerObject::setFullPowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
	}
}

void AudioProj8PowerObject::setHardwarePowerIdleOn ( void ) {
    setHardwarePowerOn();
}

IOReturn AudioProj8PowerObject::setHardwarePowerOff()
{
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    debugIOLog (3, "+ AudioProj8PowerObject::setHardwarePowerOff");
    
    // note the difference between this and some of the other setHardwarePowerOff methods
    // it may be necessary to check for the existance and availability of i2c services before 
    // asking the driver to execute a state change.  So far this seems to work.
    
    progOut = audioPluginRef->sndHWGetProgOutput();
    progOut &= ~kSndHWProgOutput0;
    audioPluginRef->sndHWSetProgOutput(progOut);
    
    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceSleep);
    audioPluginRef->setDeviceDetectionInActive();

    debugIOLog (3, "- AudioProj8PowerObject::setHardwarePowerOff");
    
    return result;
}
	
IOReturn AudioProj8PowerObject::setHardwarePowerOn()
{
    debugIOLog (3, "+ AudioProj8PowerObject::setHardwarePowerOn");
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    
    if(audioPluginRef) {
        progOut = audioPluginRef->sndHWGetProgOutput();
        progOut |= kSndHWProgOutput0;
        audioPluginRef->sndHWSetProgOutput(progOut);
    }
    
    if(audioPluginRef) {
        audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
        audioPluginRef->setDeviceDetectionActive();
    }
    debugIOLog (3, "- AudioProj8PowerObject::setHardwarePowerOn");
    return result;
}

// for Screamer based G3 and iMac DVs
#pragma mark -Screamer based G3 and iMacs

OSDefineMetaClassAndStructors(AudioProj7PowerObject, AudioPowerObject)

AudioProj7PowerObject* AudioProj7PowerObject::createAudioProj7PowerObject(Apple02Audio *pluginRef){
    AudioProj7PowerObject *myAudioProj7PowerObject = 0;
    debugIOLog (3, "+ AudioProj7PowerObject::createAudioProj10PowerObject");
    myAudioProj7PowerObject = new AudioProj7PowerObject;
    
    if(myAudioProj7PowerObject) {
        if(!(myAudioProj7PowerObject->init(pluginRef))){
            myAudioProj7PowerObject->release();
            myAudioProj7PowerObject = 0;
        }            
    }
    debugIOLog (3, "+ AudioProj7PowerObject::createAudioProj10PowerObject");
    return myAudioProj7PowerObject;
}

bool AudioProj7PowerObject::init(Apple02Audio *pluginRef){
    debugIOLog (3, "+ AudioProj10PowerObject::init");
 	mMicroSecondsRequired = 750000;
   return (AudioPowerObject::init(pluginRef));
    debugIOLog (3, "- AudioProj10PowerObject::init");
}

IOReturn AudioProj7PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    IOService *keyLargo = 0;
    debugIOLog (3, "+ AudioProj7PowerObject::setHardwarePowerOff");
    
    progOut = audioPluginRef->sndHWGetProgOutput();
    progOut &= ~kSndHWProgOutput0;
    audioPluginRef->sndHWSetProgOutput(progOut);
    IOSleep(200);

    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceSleep);
    audioPluginRef->setDeviceDetectionInActive();

    keyLargo = IOService::waitForService(IOService::serviceMatching("KeyLargo")); 
                
    if(keyLargo){
        long gpioOffset = kPowerObjectOffset;
        UInt8  value = kPowerOff;
        keyLargo->callPlatformFunction("keyLargo_writeRegUInt8", false, (void *)&gpioOffset, (void *)(UInt32)value, 0, 0);
    }
    
    debugIOLog (3, "- AudioProj7PowerObject::setHardwarePowerOff");
    return result;
}

void AudioProj7PowerObject::setIdlePowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceIdle);
	}
}

void AudioProj7PowerObject::setFullPowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
	}
}

void AudioProj7PowerObject::setHardwarePowerIdleOn ( void ) {
    debugIOLog (3, "+ AudioProj7PowerObject::setHardwarePowerIdleOn");
    setHardwarePowerOn();
    debugIOLog (3, "- AudioProj7PowerObject::setHardwarePowerIdleOn");
}

IOReturn AudioProj7PowerObject::setHardwarePowerOn(){
    debugIOLog (3, "+ AudioProj7PowerObject::setHardwarePowerOn");
    IOReturn result = kIOReturnSuccess;
    UInt32 progOut;
    IOService *keyLargo = 0;

    keyLargo = IOService::waitForService(IOService::serviceMatching("KeyLargo"));
    
    if(keyLargo){
        long gpioOffset = kPowerObjectOffset;
        UInt8  value = kPowerOn;
        keyLargo->callPlatformFunction("keyLargo_writeRegUInt8", false, (void *)&gpioOffset, (void *)(UInt32)value, 0, 0);
    }
    
    if(audioPluginRef) {
        IOSleep(300);   		// we need to wait to be sure that the antipop has the time to do his stuff
        progOut = audioPluginRef->sndHWGetProgOutput();
        progOut |= kSndHWProgOutput0;
        audioPluginRef->sndHWSetProgOutput(progOut);
    }
    
    if(audioPluginRef) {
        audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
        audioPluginRef->setDeviceDetectionActive();
    }
    debugIOLog (3, "- AudioProj7PowerObject::setHardwarePowerOn");
    return result;
}

// for Texas3001 Tower
#pragma mark -Texas Desktop CPUs

OSDefineMetaClassAndStructors(AudioProj14PowerObject, AudioPowerObject)

AudioProj14PowerObject* AudioProj14PowerObject::createAudioProj14PowerObject(Apple02Audio *pluginRef){
    AudioProj14PowerObject* myAudioProj14PowerObject = NULL;

    debugIOLog (3, "+ AudioProj14PowerObject::createAudioProj14PowerObject");
    myAudioProj14PowerObject = new AudioProj14PowerObject;
    
    if(myAudioProj14PowerObject) {
        if(!(myAudioProj14PowerObject->init(pluginRef))){
            myAudioProj14PowerObject->release();
            myAudioProj14PowerObject = 0;
        }            
    }
    debugIOLog (3, "+ AudioProj14PowerObject::createAudioProj14PowerObject");

    return (myAudioProj14PowerObject);
}

bool AudioProj14PowerObject::init(Apple02Audio *pluginRef){
    debugIOLog (3, "+ AudioProj14PowerObject::init");
	mMicroSecondsRequired = 2000000;
    return (AudioPowerObject::init(pluginRef));
    debugIOLog (3, "- AudioProj14PowerObject::init");
}

void AudioProj14PowerObject::setIdlePowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceIdle);
	}
}

void AudioProj14PowerObject::setFullPowerState (void) {
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
	}
}

void AudioProj14PowerObject::setHardwarePowerIdleOn ( void ) {
    setHardwarePowerOn();
}

IOReturn AudioProj14PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;

    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceSleep);
	}

    return result;
}

IOReturn AudioProj14PowerObject::setHardwarePowerOn(){
    IOReturn result = kIOReturnSuccess;
    
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
	}

    return result;
}

// for iBook dual USB
#pragma mark -iBook dual USB

OSDefineMetaClassAndStructors(AudioProj16PowerObject, AudioPowerObject)

AudioProj16PowerObject* AudioProj16PowerObject::createAudioProj16PowerObject ( Apple02Audio *pluginRef ){
    AudioProj16PowerObject*		myAudioProj16PowerObject = NULL;

    debugIOLog ( 3, "+ AudioProj16PowerObject::createAudioProj16PowerObject ( %p )", pluginRef );
    myAudioProj16PowerObject = new AudioProj16PowerObject;
    
    if(myAudioProj16PowerObject) {
        if(!(myAudioProj16PowerObject->init(pluginRef))){
            myAudioProj16PowerObject->release();
            myAudioProj16PowerObject = 0;
        }            
    }
    debugIOLog ( 3, "- AudioProj16PowerObject::createAudioProj16PowerObject ( %p ) returns %p", pluginRef, myAudioProj16PowerObject );
    return (myAudioProj16PowerObject);
}

bool AudioProj16PowerObject::init(Apple02Audio *pluginRef){
	bool		result;
	
    debugIOLog ( 3, "+ AudioProj16PowerObject::init ( %p )", pluginRef );
	mMicroSecondsRequired = 2000000;
    result = (AudioPowerObject::init(pluginRef));
    debugIOLog ( 3, "- AudioProj16PowerObject::init ( %p ) returns %d", pluginRef, result );
	return result;
}

Boolean AudioProj16PowerObject::wantsIdleCalls (void) {
	debugIOLog ( 3, "± AudioProj16PowerObject::wantsIdleCalls() returns TRUE" );
	return TRUE;
}

void AudioProj16PowerObject::setIdlePowerState (void) {
	debugIOLog ( 3, "+ AudioProj16PowerObject::setIdlePowerState()" );
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceIdle);
	}
	debugIOLog ( 3, "- AudioProj16PowerObject::setIdlePowerState()" );
}

void AudioProj16PowerObject::setFullPowerState (void) {
	debugIOLog ( 3, "+ AudioProj16PowerObject::setFullPowerState()" );
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
	}
	debugIOLog ( 3, "- AudioProj16PowerObject::setFullPowerState()" );
}

void AudioProj16PowerObject::setHardwarePowerIdleOn ( void ) {
    setHardwarePowerOn();
}

IOReturn AudioProj16PowerObject::setHardwarePowerOff(){
    IOReturn result = kIOReturnSuccess;

	debugIOLog ( 3, "+ AudioProj16PowerObject::setHardwarePowerIdleOn()" );
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceSleep);
	}
	debugIOLog ( 3, "- AudioProj16PowerObject::setHardwarePowerIdleOn()" );
    return result;
}

IOReturn AudioProj16PowerObject::setHardwarePowerOn(){
    IOReturn result = kIOReturnSuccess;
    
	debugIOLog ( 3, "+ AudioProj16PowerObject::setHardwarePowerOn()" );
    if(audioPluginRef) {
	    audioPluginRef->sndHWSetPowerState(kIOAudioDeviceActive);
	}
	debugIOLog ( 3, "- AudioProj16PowerObject::setHardwarePowerOn()" );
    return result;
}
