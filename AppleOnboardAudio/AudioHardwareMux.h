/*
 *  AudioHardwareMux.h
 *  AppleOnboardAudio
 *
 *  Created by cerveau on Sat Feb 03 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _AUDIOHARDWAREMUXES_H
#define _AUDIOHARDWAREMUXES_H

#include "AudioHardwareCommon.h"
#include "AudioHardwareUtilities.h"
#include "AppleOnboardAudio.h"


        //Kind of muxes
enum{
    kAudioHardwareMuxUnknown = 0,
    kAudioHardwareMuxWS  = 1,
    kAudioHardwareMux101 = 2,
    kAudioHardwareMuxPO  = 3  //for Pismo and probably Titanium, if my I remember well
};

        //structure for the MuxProgOut
struct MuxSourceMap {
	UInt32		source;
	UInt32		bitMask;
	UInt32		bitMatch;
};
typedef struct MuxSourceMap MuxSourceMap;
   

typedef struct _s_AudioHardwareMuxInfo {
    UInt32	MuxPortType;
    
    UInt32		MuxPOnumSources;
    MuxSourceMap	MuxPOsources[2];
}AudioHardwareMuxInfo;

    
    //constant for the MuxWallStreet
enum  {
    kWSMuxMediaBayInput	=	0x00000000,	// media bay input enable
    kWSMuxCommsInput	=	0x00000010,	// comms input enable
    kWSMuxDockInput		=	0x00000020,	// dock input enable
    kWSMuxZoomVideoInput	=	0x00000030,	// zoom video input enable
    kWSMuxMuxFieldMask	=	0x00000030,	// mask of all possible mux settings
		
    kWSMuxChanCOn		=	0x02,		// turn on mux enable for channel C
    kWSMuxChanCOff		=	0x0A,		// turn off mux enable for channel C
    kWSMuxChanCIsOffMask	=	0x20,		// mask to test to see if channel C is off
    kWSMuxChanCEnableAddr	=	0xF3000037,	// address to hit to enable mux for channel C
    kWSMuxExternalAddress	=	0xF301B000,	// address to read to make mux settings take effect
		
    kWSMuxDelay		=	0x110,		// ms delay for mux setting to take effect
    kWSMuxMClkDelay		=	2,		// ms delay for leaving mclk off so that DAC can switch from internal to external mode
		
    kWSMuxOff		=	0,		// tell callback to turn mux off
    kWSMuxOn		=	1		// tell callback to turn mux on
};

    //constant for the Mux 101
enum  {
    k101MuxMediaBayInput		=	0x01,	// media bay input enable
    k101MuxZoomVideoInput		=	0x00,	// zoom video input enable
    k101MuxInternalMicInput		=	0x01,	// internal mic input enable
    k101MuxInternalModemInput	=	0x00,		// internal modem input enable
		
    k101MuxChanCOn			=	0x02, 	// turn on mux enable for channel C
    k101MuxChanCOff			=	0x00,	// turn off mux enable for channel C
    k101MuxClearChanCBitsMask	=	~0x0B,		// clear bit 3 (data) and low two bits (chancon and mux select)
    k101MuxChanCIsOffMask		=	0x20,	// mask to test to see if channel C is off
    k101MuxDefaultIOBaseAddress	=	0x80800000,	// base address of IO controller
    k101MuxChanCEnableOffset	=	0x00000037,	// offset from start of IO controller to mux control register
				
    k101AnalogMuxDelay		=	320,		// ms delay for analog mux setting to take effect
    k101DigitalMuxDelay		=	775,		// ms delay for digital mux setting to take effect (yes, the CS4334 takes a long time!)
    k101MuxMClkDelay		=	2,		// ms delay for leaving mclk off so that DAC can switch from internal to external mode
		
    k101MuxOff				=	0,	// tell callback to turn mux off
    k101MuxOn				=	1	// tell callback to turn mux on
};

    //As we don't have a lot to support we do know a class that takes care of these three muxes
    
class AudioHardwareMux : public OSObject {
    OSDeclareDefaultStructors(AudioHardwareMux);

public:
    static AudioHardwareMux *create(AudioHardwareMuxInfo theMuxInfo); 
    void attachAudioPluginRef(AppleOnboardAudio *theAudioPlugin);
    
    UInt32   GetMuxSource();
    IOReturn SetMuxSource(UInt32 source );
     void ioLog();

protected:
    bool init(AudioHardwareMuxInfo theOutputInfo);
    void free();

        
    UInt32	MuxPortType;
    UInt32	lastSource;		// last source set on mux

        //WSMux specific
    UInt32 	WSMuxhwSetting;	// only one of these shared for all mux instances;this should be a static
    UInt32	   	WSMuxscratch;	// scratch variable to cause read to mux control location
    UInt8		WSMuxchanCSetting;	// what to set the channel C
        
        //Mux101 specific (nothing should be passed to constructor)
    UInt8		*Mux101chanCEnableAddr;	// address to hit to set mux for input C
    UInt8		Mux101chanCSetting;	// what to set the mux control to

        //MuxProgOut specific (the Map should be pass to the constructor)
    UInt32		MuxPOnumSources;
    MuxSourceMap	MuxPOsources[2];   //we should make it a pointer but we know there is 2 to take care until now		
    AppleOnboardAudio *pluginRef;

};

#endif