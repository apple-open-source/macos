/*
 *  AudioHardwareMux.cpp
 *  AppleOnboardAudio
 *
 *  Created by cerveau on Sat Feb 03 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AudioHardwareCommon.h"
#include "AudioHardwareMux.h"
#include "AudioHardwareConstants.h"

#define super OSObject

OSDefineMetaClassAndStructors(AudioHardwareMux, OSObject)

AudioHardwareMux *AudioHardwareMux::create(AudioHardwareMuxInfo theInfo){
 
    CLOG("+ AudioHardwareMux::create\n");
    AudioHardwareMux *myMux;
    myMux = new AudioHardwareMux;
    
    if(myMux) {
        if(!(myMux->init(theInfo))){
            myMux->release();
            myMux = 0;
        }            
    }
    CLOG("- AudioHardwareMux::create\n");
    return myMux;
}


bool AudioHardwareMux::init(AudioHardwareMuxInfo theInfo) {

    short idx;
    
    CLOG("+ AudioHardwareMux::init\n");
    if(!super::init())
        return(false);
    
    lastSource = 0;

    WSMuxhwSetting = 0;
    WSMuxscratch = 0;
    WSMuxchanCSetting = 0;
        
    Mux101chanCEnableAddr = 0;
    Mux101chanCSetting = 0;
    MuxPOnumSources = 0;
    
    MuxPortType = theInfo.MuxPortType ;
    
    if(kAudioHardwareMuxPO == MuxPortType) {
        MuxPOnumSources = 2;
        
        for(idx = 0; idx < 2; idx++) {
            MuxPOsources[idx].source = theInfo.MuxPOsources[idx].source;
            MuxPOsources[idx].bitMask = theInfo.MuxPOsources[idx].bitMask;
            MuxPOsources[idx].bitMatch = theInfo.MuxPOsources[idx].bitMatch;
        }
    }
    
    CLOG("- AudioHardwareMux::init\n");
    return(true);
}


void AudioHardwareMux::free(){
    //pluginRef->release();
    super::free();
}

void AudioHardwareMux::attachAudioPluginRef(AppleOnboardAudio *theAudioPlugin){
    pluginRef = theAudioPlugin;
    //pluginRef->retain();
}

UInt32 AudioHardwareMux::GetMuxSource( ){
    return(lastSource);
}

IOReturn AudioHardwareMux::SetMuxSource(UInt32 source ) {
    IOReturn result = kIOReturnSuccess;
    UInt32 index;
    UInt32 delayTime = 0;
    bool doTimingStuff;
    UInt32 progOuts;
    IOService *heathrow;
    
    switch(MuxPortType) {
        case kAudioHardwareMuxPO:
            //debugIOLog("We are in the right case\n");
            for (index = 0; index < MuxPOnumSources; index++) {
		if (MuxPOsources[index].source == source) {
                    progOuts = pluginRef->sndHWGetProgOutput();	
                    progOuts &= ~MuxPOsources[index].bitMask;	
                    progOuts |= MuxPOsources[index].bitMatch;
                    pluginRef->sndHWSetProgOutput(progOuts);
                    lastSource = source;
                }
            }
            break;
        case kAudioHardwareMux101:
            doTimingStuff = false;
            WSMuxchanCSetting = 0;
            UInt8  value;
            long MmuxOffset;
			
			value = 0;
            MmuxOffset = k101MuxChanCEnableOffset;           
            heathrow = IOService::waitForService(IOService::serviceMatching("Heathrow"));

            heathrow->callPlatformFunction(OSSymbol::withCString("heathrow_safeReadRegUInt8"), false, 
                                                                (void *)MmuxOffset, (void *)&value, 0, 0);
            value &= k101MuxClearChanCBitsMask;

            heathrow->callPlatformFunction(OSSymbol::withCString("heathrow_writeRegUInt8"), false, 
                                                                (void *)&MmuxOffset, (void *)value, 0, 0);
            switch(source) {
                case 'imic':
                    WSMuxchanCSetting |= k101MuxInternalMicInput;
                    doTimingStuff = true;
                    delayTime = k101AnalogMuxDelay;
                    lastSource = source;
                    break;
                case 'modm':
                    WSMuxchanCSetting |= k101MuxInternalModemInput;
                    doTimingStuff = true;
                    delayTime = k101AnalogMuxDelay;
                    lastSource = source;
                    break;
                default:
                    break;
            }                        
            
            heathrow->callPlatformFunction(OSSymbol::withCString("heathrow_safeReadRegUInt8"), false, 
                                                                (void *)MmuxOffset, (void *)&value, 0, 0); 
            
            value |= WSMuxchanCSetting; 
            
            if(doTimingStuff) {
                IOSleep(k101MuxMClkDelay);
                heathrow->callPlatformFunction(OSSymbol::withCString("heathrow_writeRegUInt8"), false, 
                                                         (void *)&MmuxOffset, (void *)value, 0, 0);
                IOSleep(delayTime);
            } else {
                heathrow->callPlatformFunction(OSSymbol::withCString("heathrow_writeRegUInt8"), false, 
                                                         (void *)&MmuxOffset, (void *)value, 0, 0);
            }
            break;
        default:
            result = kIOReturnError;
            goto BAIL;
    }

EXIT:    
    return(result);
BAIL:
    goto EXIT;
}


void AudioHardwareMux::ioLog() {
#ifdef DEBUGLOG
    short idx;
    debugIOLog( "   + Mux port information :\n");
     switch (MuxPortType) {
        case  kAudioHardwareMuxUnknown:debugIOLog("    -- Type is : Unknown \n");break;
        case kAudioHardwareMuxWS: debugIOLog("    -- Type is : WallStreet Mux \n");break;
        case kAudioHardwareMux101: debugIOLog("    -- Type is : 101 Mux \n");break;
        case kAudioHardwareMuxPO: debugIOLog("    -- Type is : PO Mux \n");break;
        default:debugIOLog("    -- Type is : unknown \n"); break;
    }
    debug2IOLog(" -- Last source is %ld\n", lastSource);
    
    if(MuxPortType == kAudioHardwareMuxPO) {
        for(idx = 0; idx <2; idx++) {
            debug2IOLog("   -- source %ld\n",MuxPOsources[idx].source);
            debug3IOLog("   -- bitMask, %ld, bitMatch %ld\n",MuxPOsources[idx].bitMask, MuxPOsources[idx].bitMatch);
        }        
    }
#endif
}